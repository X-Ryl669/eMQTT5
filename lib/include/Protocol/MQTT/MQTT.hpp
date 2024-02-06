#ifndef hpp_CPP_MQTT_CPP_hpp
#define hpp_CPP_MQTT_CPP_hpp

// We need basic types
#include <Types.hpp>
// We need Platform code for allocations too
#include <Platform/Platform.hpp>

#if (MQTTDumpCommunication == 1)
    // Because all projects are different, it's hard to give a generic method for dumping elements.
    // So we end up with only limited dependencies:
    // - a string class that concatenate with operator +=
    // - the string class has MQTTStringGetData(MQTTString instance) method returning a pointer to the string array
    // - the string class has MQTTStringGetLength(MQTTString instance) method returning the array size of the string
    // - a hexadecimal dumping method
    // - a printf-like formatting function

    // Feel free to define the string class and method to use beforehand if you want to use your own.
    // If none provided, we use's std::string class (or ClassPath's FastString depending on the environment)
    #ifndef MQTTStringPrintf
      static MQTTString MQTTStringPrintf(const char * format, ...)
      {
          va_list argp;
          va_start(argp, format);
          char buf[512];
          // We use vasprintf extension to avoid dual parsing of the format string to find out the required length
          int err = vsnprintf(buf, sizeof(buf), format, argp);
          va_end(argp);
          if (err <= 0) return MQTTString();
          if (err >= (int)sizeof(buf)) err = (int)(sizeof(buf) - 1);
          buf[err] = 0;
          return MQTTString(buf, (size_t)err);
      }
    #endif
    #ifndef MQTTHexDump
      static void MQTTHexDump(MQTTString & out, const uint8* bytes, const uint32 length)
      {
          for (uint32 i = 0; i < length; i++)
              out += MQTTStringPrintf("%02X", bytes[i]);
      }
    #endif
#endif

/** All network protocol specific structure or enumerations are declared here */
namespace Protocol
{
    /** The MQTT specific enumeration or structures */
    namespace MQTT
    {
        /** The types declared in this namespace are shared between the different versions */
        namespace Common
        {
            /** This is the standard error code while reading an invalid value from hostile source */
            enum LocalError
            {
                BadData         = 0xFFFFFFFF,   //!< Malformed data
                NotEnoughData   = 0xFFFFFFFE,   //!< Not enough data
                Shortcut        = 0xFFFFFFFD,   //!< Serialization shortcut used (not necessarly an error)

                MinErrorCode    = 0xFFFFFFFD,
            };

            /** Quickly check if the given code is an error */
            static inline bool isError(uint32 value) { return value >= MinErrorCode; }
            /** Check if serialization shortcut was used */
            static inline bool isShortcut(uint32 value) { return value == Shortcut; }

            /** A cross platform bitfield class that should be used in union like this:
                @code
                union
                {
                    T whatever;
                    BitField<T, 0, 1> firstBit;
                    BitField<T, 7, 1> lastBit;
                    BitField<T, 2, 2> someBits;
                };
                @endcode */
            template <typename T, int Offset, int Bits>
            struct BitField
            {
                /** This is public to avoid undefined behavior while used in union */
                T value;

                static_assert(Offset + Bits <= (int) sizeof(T) * 8, "Member exceeds bitfield boundaries");
                static_assert(Bits < (int) sizeof(T) * 8, "Can't fill entire bitfield with one member");

                /** Base constants are typed to T so we skip type conversion everywhere */
                static const T Maximum = (T(1) << Bits) - 1;
                static const T Mask = Maximum << Offset;

                /** Main access operator, use like any other member */
                inline operator T() const { return (value >> Offset) & Maximum; }
                /** Assign operator */
                inline BitField & operator = (T v) { value = (value & ~Mask) | (v << Offset); return *this; }
            };


            /** The base interface all MQTT serializable structure must implement */
            struct Serializable
            {
                /** We have a getSize() method that gives the number of bytes requires to serialize this object */
                virtual uint32 getSize() const = 0;

                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's at least getSize() bytes long
                    @return The number of bytes used in the buffer */
                virtual uint32 copyInto(uint8 * buffer) const = 0;
                /** Read the value from a buffer.
                    @param buffer       A pointer to an allocated buffer
                    @param bufLength    The length of the buffer in bytes
                    @return The number of bytes read from the buffer, or a LocalError upon error (use isError() to test for it) */
                virtual uint32 readFrom(const uint8 * buffer, uint32 bufLength)
#if MQTTClientOnlyImplementation == 1
                {
                    return BadData;
                }
#else
                = 0;
#endif

#if MQTTDumpCommunication == 1
                /** Dump the serializable to the given string */
                virtual void dump(MQTTString & out, const int indent = 0) = 0;
#endif

#if MQTTAvoidValidation != 1
                /** Check if this object is correct after deserialization */
                virtual bool check() const { return true; }
#endif
                /** Required destructor - Not virtual here, it's never deleted virtually */
                ~Serializable() {}
            };

            /** Empty serializable used for generic code to avoid useless specific case in packet serialization */
            struct EmptySerializable : public Serializable
            {
                uint32 getSize() const { return 0; }
                uint32 copyInto(uint8 *) const { return 0; }
                uint32 readFrom(const uint8 *, uint32) { return 0; }
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0) { out += MQTTStringPrintf("%*s%s\n", indent, "", "<none>"); }
#endif
            };

            /** Invalid serialization used as an escape path */
            struct Hidden InvalidData : public Serializable
            {
                uint32 getSize() const { return 0; }
                uint32 copyInto(uint8 *) const { return 0; }
                uint32 readFrom(const uint8 *, uint32) { return BadData; }
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0) { out += MQTTStringPrintf("%*s%s\n", indent, "", "<invalid>"); }
#endif
#if MQTTAvoidValidation != 1
                bool check() const { return false; }
#endif
            };


            /** The visitor that'll be called with the relevant value */
            struct MemMappedVisitor
            {
                /** Accept the given buffer */
                virtual uint32 acceptBuffer(const uint8 * buffer, const uint32 bufLength) = 0;
                // All visitor will have a getValue() method, but the returned type depends on the visitor and thus,
                // can not be declared polymorphically
#if MQTTDumpCommunication == 1
                virtual void dump(MQTTString & out, const int indent = 0)
                {
                    out += MQTTStringPrintf("%*s", (int)indent, "");
                    // Voluntary incomplete
                }
#endif

                /** Default destructor */
                virtual ~MemMappedVisitor() {}
            };

            /** Plumbing code for simple visitor pattern to avoid repetitive code in this file */
            template <typename T>
            struct SerializableVisitor : public MemMappedVisitor
            {
                T & getValue() { return *static_cast<T*>(this); }
                operator T& () { return getValue(); }
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    static_cast<T*>(this)->dump(out, indent);
                }
#endif

                uint32 acceptBuffer(const uint8 * buffer, const uint32 bufLength) { return static_cast<T*>(this)->readFrom(buffer, bufLength); }
            };

            /** Plumbing code for simple visitor pattern to avoid repetitive code in this file */
            template <typename T>
            struct PODVisitor : public MemMappedVisitor
            {
                T value;
                T & getValue() { return value; }

                operator T& () { return getValue(); }
                PODVisitor(const T value = 0) : value(value) {}

                uint32 acceptBuffer(const uint8 * buffer, const uint32 bufLength)
                {
                    if (bufLength < sizeof(value)) return NotEnoughData;
                    memcpy(&value, buffer, sizeof(value));
                    return sizeof(value);
                }
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    MemMappedVisitor::dump(out, indent);
                    out += getValue();
                    out += "\n";
                }
#endif

            };

            /** Plumbing code for simple visitor pattern to avoid repetitive code in this file */
            template <typename T>
            struct LittleEndianPODVisitor : public MemMappedVisitor
            {
                T value;
                T & getValue() { return value; }

                operator T& () { return getValue(); }
                LittleEndianPODVisitor(const T value = 0) : value(value) {}

                uint32 acceptBuffer(const uint8 * buffer, const uint32 bufLength)
                {
                    if (bufLength < sizeof(value)) return NotEnoughData;
                    memcpy(&value, buffer, sizeof(value));
                    value = BigEndian(value);
                    return sizeof(value);
                }
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    MemMappedVisitor::dump(out, indent);
                    out += getValue();
                    out += "\n";
                }
#endif

            };


#pragma pack(push, 1)
            /** A MQTT string is basically a string with a BigEndian size going first (section 1.5.4) */
            struct String
            {
                /** The string length in bytes */
                uint16 length;
                char   data[];

                /** Call this method to read the structure when it's casted from the network buffer */
                void swapNetwork() { length = BigEndian(length); }
            };

            /** A string that's memory managed itself */
            struct DynamicString Final : public Serializable
            {
                /** The string length in bytes */
                uint16      length;
                /** The data itself */
                char   *    data;

                /** For consistancy with the other structures, we have a getSize() method that gives the number of bytes requires to serialize this object */
                uint32 getSize() const { return (uint32)length + 2; }

                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 4 bytes long
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const { uint16 size = BigEndian(length); memcpy(buffer, &size, 2); memcpy(buffer+2, data, length); return (uint32)length + 2; }
                /** Read the value from a buffer.
                    @param buffer       A pointer to an allocated buffer that's at least 4 bytes long
                    @param bufLength    The length of the buffer in bytes
                    @return The number of bytes read from the buffer, or BadData upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if (bufLength < 2) return NotEnoughData;
                    uint16 size = 0; memcpy(&size, buffer, 2); length = BigEndian(size);
                    if ((uint32)(length+2) > bufLength) return NotEnoughData;
                    data = (char*)Platform::safeRealloc(data, length);
                    memcpy(data, buffer+2, length);
                    return (uint32)length+2;
                }
#if MQTTAvoidValidation != 1
                /** Check if the value is correct */
                bool check() const { return data ? length : length == 0; }
#endif
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0) { out += MQTTStringPrintf("%*sStr (%d bytes): %.*s\n", (int)indent, "", (int)length, length, data); }
#endif

                /** Default constructor */
                DynamicString() : length(0), data(0) {}
                /** Construct from a text */
                DynamicString(const char * text) : length(text ? strlen(text) : 0), data((char*)Platform::malloc(length)) { memcpy(data, text, length); }
                /** Construct from a FastString */
                DynamicString(const MQTTString & text) : length(MQTTStringGetLength(text)), data((char*)Platform::malloc(length)) { memcpy(data, MQTTStringGetData(text), length); }
                /** Construct from a FastString */
                DynamicString(const MQTTROString & text) : length(MQTTStringGetLength(text)), data((char*)Platform::malloc(length)) { memcpy(data, MQTTStringGetData(text), length); }
                /** Copy constructor */
                DynamicString(const DynamicString & other) : length(other.length), data((char*)Platform::malloc(length)) { memcpy(data, other.data, length); }
#if HasCPlusPlus11 == 1
                /** Move constructor */
                DynamicString(DynamicString && other) : length(std::move(other.length)), data(std::move(other.data)) { }
#endif
                /** Destructor */
                ~DynamicString() { Platform::free(data); length = 0; }
                /** Convert to a ReadOnlyString */
                operator MQTTROString() const { return MQTTROString(data, length); }
                /** Comparison operator */
                bool operator != (const MQTTROString & other) const { return length != MQTTStringGetLength(other) || memcmp(data, MQTTStringGetData(other), length); }
                /** Comparison operator */
                bool operator == (const MQTTROString & other) const { return length == MQTTStringGetLength(other) && memcmp(data, MQTTStringGetData(other), length) == 0; }
                /** Copy operator */
                DynamicString & operator = (const DynamicString & other) { if (this != &other) { this->~DynamicString(); length = other.length; data = (char*)Platform::malloc(length); memcpy(data, other.data, length); } return *this; }
                /** Copy operator */
                void from(const char * str, const size_t len = 0) { this->~DynamicString(); length = len ? len : (strlen(str)+1); data = (char*)Platform::malloc(length); memcpy(data, str, length); data[length - 1] = 0; }

            };

            /** A dynamic string pair */
            struct DynamicStringPair Final : public Serializable
            {
                /** The key used for the pair */
                DynamicString key;
                /** The value used for the pair */
                DynamicString value;

                /** For consistancy with the other structures, we have a getSize() method that gives the number of bytes requires to serialize this object */
                uint32 getSize() const { return key.getSize() + value.getSize(); }

                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's at least getSize() bytes long
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const { uint32 o = key.copyInto(buffer); o += value.copyInto(buffer+o); return o; }
                /** Read the value from a buffer.
                    @param buffer       A pointer to an allocated buffer that's at least 4 bytes long
                    @param bufLength    The length of the buffer in bytes
                    @return The number of bytes read from the buffer, or BadData upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    uint32 o = key.readFrom(buffer, bufLength);
                    if (isError(o)) return o;
                    uint32 s = value.readFrom(buffer + o, bufLength - o);
                    if (isError(s)) return s;
                    return s+o;
                }
#if MQTTAvoidValidation != 1
                /** Check if the value is correct */
                bool check() const { return key.check() && value.check(); }
#endif
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0) { out += MQTTStringPrintf("%*sKV:\n", (int)indent, ""); key.dump(out, indent + 2); value.dump(out, indent + 2); }
#endif

                /** Default constructor */
                DynamicStringPair(const DynamicString & k = "", const DynamicString & v = "") : key(k), value(v) {}
                /** Copy constructor */
                DynamicStringPair(const DynamicStringPair & other) : key(other.key), value(other.value) {}
#if HasCPlusPlus11 == 1
                /** Move constructor */
                DynamicStringPair(DynamicStringPair && other) : key(std::move(other.key)), value(std::move(other.value)) { }
#endif
            };

            /** A MQTT binary data with a BigEndian size going first (section 1.5.6) */
            struct BinaryData
            {
                /** The data length in bytes */
                uint16 length;
                uint8  data[];

                /** Call this method to read the structure when it's casted from the network buffer */
                void swapNetwork() { length = BigEndian(length); }
            };

            /** A dynamic binary data, with self managed memory */
            struct DynamicBinaryData Final : public Serializable
            {
                /** The string length in bytes */
                uint16      length;
                /** The data itself */
                uint8   *    data;

                /** For consistancy with the other structures, we have a getSize() method that gives the number of bytes requires to serialize this object */
                uint32 getSize() const { return (uint32)length + 2; }

                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 4 bytes long
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const { uint16 size = BigEndian(length); memcpy(buffer, &size, 2); memcpy(buffer+2, data, length); return (uint32)length + 2; }
                /** Read the value from a buffer.
                    @param buffer       A pointer to an allocated buffer that's at least 4 bytes long
                    @param bufLength    The length of the buffer in bytes
                    @return The number of bytes read from the buffer, or BadData upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if (bufLength < 2) return NotEnoughData;
                    uint16 size = 0; memcpy(&size, buffer, 2); length = BigEndian(size);
                    if ((uint32)(length+2) > bufLength) return NotEnoughData;
                    data = (uint8*)Platform::safeRealloc(data, length);
                    memcpy(data, buffer+2, length);
                    return (uint32)length + 2;
                }
#if MQTTAvoidValidation != 1
                /** Check if the value is correct */
                bool check() const { return data ? length : length == 0; }
#endif
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0) { out += MQTTStringPrintf("%*sBin (%d bytes):", (int)indent, "", (int)length); MQTTHexDump(out, data, length); out += "\n"; }
#endif
                /** Copy operator */
                DynamicBinaryData & operator = (const DynamicBinaryData & other) { if (this != &other) this->~DynamicBinaryData(); new(this) DynamicBinaryData(other); return *this; }
                /** Construct from a memory block */
                DynamicBinaryData(const uint16 length = 0, const uint8 * block = 0) : length(length), data(length ? (uint8*)Platform::malloc(length) : (uint8*)0) { memcpy(data, block, length); }
                /** Copy constructor */
                DynamicBinaryData(const DynamicBinaryData & other) : length(other.length), data(length ? (uint8*)Platform::malloc(length) : (uint8*)0) { memcpy(data, other.data, length); }
#if HasCPlusPlus11 == 1
                /** Move constructor */
                DynamicBinaryData(DynamicBinaryData && other) : length(std::move(other.length)), data(std::move(other.data)) { }
#endif
                /** Destructor */
                ~DynamicBinaryData() { Platform::free(data); length = 0; }
            };



            /** A read only dynamic string view.
                This is used to avoid copying a string buffer when only a pointer is required.
                This string can be mutated to many buffer but no modification is done to the underlying array of chars */
            struct DynamicStringView Final : public Serializable, public SerializableVisitor<DynamicStringView>
            {
                /** The string length in bytes */
                uint16          length;
                /** The data itself */
                const char *    data;

                /** For consistancy with the other structures, we have a getSize() method that gives the number of bytes requires to serialize this object */
                uint32 getSize() const { return (uint32)length + 2; }

                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 4 bytes long
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const { uint16 size = BigEndian(length); memcpy(buffer, &size, 2); memcpy(buffer+2, data, length); return (uint32)length + 2; }
                /** Read the value from a buffer.
                    @param buffer       A pointer to an allocated buffer that's at least 4 bytes long
                    @param bufLength    The length of the buffer in bytes
                    @return The number of bytes read from the buffer, or BadData upon error
                    @warning This method capture a pointer on the given buffer so it must outlive this object when being called.
                             Don't use this method if buffer is a temporary data. */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if (bufLength < 2) return NotEnoughData;
                    uint16 size = 0; memcpy(&size, buffer, 2); length = BigEndian(size);
                    if ((uint32)(length+2) > bufLength) return NotEnoughData;
                    data = (const char*)&buffer[2];
                    return (uint32)length + 2;
                }

#if MQTTAvoidValidation != 1
                /** Check if the value is correct */
                bool check() const { return data ? length : length == 0; }
#endif
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0) { out += MQTTStringPrintf("%*sStr (%d bytes): %.*s\n", (int)indent, "", (int)length, length, data); }
#endif


                /** Capture from a dynamic string here.
                    Beware of this method as the source must outlive this instance */
                DynamicStringView & operator = (const DynamicString & source) { length = source.length; data = source.data; return *this; }
                /** Capture from a dynamic string here.
                    Beware of this method as the source must outlive this instance */
                DynamicStringView & operator = (const char * string) { length = strlen(string); data = string; return *this; }

                /** Comparison operator */
                bool operator != (const DynamicStringView & other) const { return length != other.length || memcmp(data, other.data, length); }
                /** Comparison operator */
                bool operator == (const DynamicStringView & other) const { return length == other.length && memcmp(data, other.data, length) == 0; }
                /** Comparison operator */
                bool operator == (const char * other) const { return length == strlen(other) && memcmp(data, other, length) == 0; }

                /** From a usual dynamic string */
                DynamicStringView(const DynamicString & other) : length(other.length), data(other.data) {}
                /** From a given C style buffer */
                DynamicStringView(const char * string) : length(strlen(string)), data(string) {}
                /** From a given C style buffer with length */
                DynamicStringView(const char * string, const size_t len) : length(len), data(string) {}
                /** From a given C style buffer with length */
                DynamicStringView(const MQTTROString & text) : length(MQTTStringGetLength(text)), data(MQTTStringGetData(text)) { }
                /** A null version */
                DynamicStringView() : length(0), data(0) {}

            };

            /** A dynamic string pair view.
                This is used to avoid copying a string buffer when only a pointer is required.
                This string can be mutated to many buffer but no modification is done to the underlying array of chars */
            struct DynamicStringPairView Final : public Serializable, public SerializableVisitor<DynamicStringPairView>
            {
                /** The key used for the pair */
                DynamicStringView key;
                /** The value used for the pair */
                DynamicStringView value;

                /** For consistancy with the other structures, we have a getSize() method that gives the number of bytes requires to serialize this object */
                uint32 getSize() const { return key.getSize() + value.getSize(); }

                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's at least getSize() bytes long
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const { uint32 o = key.copyInto(buffer); o += value.copyInto(buffer+o); return o; }
                /** Read the value from a buffer.
                    @param buffer       A pointer to an allocated buffer that's at least 4 bytes long
                    @param bufLength    The length of the buffer in bytes
                    @return The number of bytes read from the buffer, or BadData upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    uint32 o = key.readFrom(buffer, bufLength);
                    if (isError(o)) return o;
                    uint32 s = value.readFrom(buffer + o, bufLength - o);
                    if (isError(s)) return s;
                    return s+o;
                }
#if MQTTAvoidValidation != 1
                /** Check if the value is correct */
                bool check() const { return key.check() && value.check(); }
#endif
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0) { out += MQTTStringPrintf("%*sKV:\n", (int)indent, ""); key.dump(out, indent + 2); value.dump(out, indent + 2); }
#endif

                /** Default constructor */
                DynamicStringPairView(const DynamicStringView & k = "", const DynamicStringView & v = "") : key(k), value(v) {}
#if HasCPlusPlus11 == 1
                /** Move constructor */
                DynamicStringPairView(DynamicStringPair && other) : key(std::move(other.key)), value(std::move(other.value)) { }
#endif
            };

            /** A read only dynamic dynamic binary data, without self managed memory.
                This is used to avoid copying a binary data buffer when only a pointer is required. */
            struct DynamicBinDataView Final : public Serializable, public SerializableVisitor<DynamicBinDataView>
            {
                /** The string length in bytes */
                uint16             length;
                /** The data itself */
                const uint8   *    data;

                /** For consistancy with the other structures, we have a getSize() method that gives the number of bytes requires to serialize this object */
                uint32 getSize() const { return (uint32)length + 2; }

                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 4 bytes long
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const { uint16 size = BigEndian(length); memcpy(buffer, &size, 2); memcpy(buffer+2, data, length); return (uint32)length + 2; }
                /** Read the value from a buffer.
                    @param buffer       A pointer to an allocated buffer that's at least 4 bytes long
                    @param bufLength    The length of the buffer in bytes
                    @return The number of bytes read from the buffer, or BadData upon error
                    @warning This method capture a pointer on the given buffer so it must outlive this object when being called.
                             Don't use this method if buffer is a temporary data. */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if (bufLength < 2) return NotEnoughData;
                    uint16 size = 0; memcpy(&size, buffer, 2); length = BigEndian(size);
                    if ((uint32)(length+2) > bufLength) return NotEnoughData;
                    data = &buffer[2];
                    return (uint32)length + 2;
                }
#if MQTTAvoidValidation != 1
                /** Check if the value is correct */
                bool check() const { return data ? length : length == 0; }
#endif
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0) { out += MQTTStringPrintf("%*sBin (%d bytes):", (int)indent, "", (int)length); MQTTHexDump(out, data, length); out += "\n"; }
#endif

                /** Construct from a memory block */
                DynamicBinDataView(const uint16 length = 0, const uint8 * block = 0) : length(length), data(block) { }
                /** Copy constructor */
                DynamicBinDataView(const DynamicBinaryData & other) : length(other.length), data(other.data) { }


                /** Capture from a dynamic string here.
                    Beware of this method as the source must outlive this instance */
                DynamicBinDataView & operator = (const DynamicBinaryData & source) { length = source.length; data = source.data; return *this; }
            };

#pragma pack(pop)

            /** The variable byte integer encoding (section 1.5.5).
                It's always stored encoded as a network version */
            struct VBInt Final : public Serializable
            {
                enum
                {
                    MaxSizeOn1Byte  = 127,
                    MaxSizeOn2Bytes = 16383,
                    MaxSizeOn3Bytes = 2097151,
                    MaxPossibleSize = 268435455, //!< The maximum possible size
                };

                union
                {
                    /** In the worst case, it's using 32 bits */
                    uint8   value[4];
                    /** The quick accessible word */
                    uint32  word;
                };
                /** The actual used size for transmitting the value, in bytes */
                uint16  size;


                /** Copy operator */
                VBInt & operator =(const VBInt & other) 
                {
                    word = other.word; 
                    size = other.size;
                    return *this;
                } 

                /** Set the value. This algorithm is 26% faster compared to the basic method shown in the standard */
                VBInt & operator = (uint32 other)
                {
                    uint8 carry = 0;
                    uint8 pseudoLog = (other > 127) + (other > 16383) + (other > 2097151) + (other > 268435455);
                    size = pseudoLog+1;
                    switch (pseudoLog)
                    {
                    case 3: value[pseudoLog--] = (other >> 21); other &= 0x1FFFFF; carry = 0x80; // Intentionally no break here
                    // fall through
                    case 2: value[pseudoLog--] = (other >> 14) | carry; other &= 0x3FFF; carry = 0x80; // Same
                    // fall through
                    case 1: value[pseudoLog--] = (other >>  7) | carry; other &= 0x7F; carry = 0x80; // Ditto
                    // fall through
                    case 0: value[pseudoLog--] = other | carry; return *this;
                    default:
                    case 4: value[0] = value[1] = value[2] = value[3] = 0xFF; size = 0; return *this;  // This is an error anyway
                    }
                }
                /** Get the value as an unsigned integer (decode)
                    @warning No check is made here to assert the encoding is good. Use check() to assert the encoding. */
                operator uint32 () const
                {
                    uint32 o = 0;
                    switch(size)
                    {
                    case 0: return 0; // This is an error anyway
                    case 4: o = value[3] << 21;            // Intentionally no break here
                    // fall through
                    case 3: o |= (value[2] & 0x7F) << 14;  // Same
                    // fall through
                    case 2: o |= (value[1] & 0x7F) << 7;   // Ditto
                    // fall through
                    case 1: o |= value[0] & 0x7F; // Break is useless here too
                    }
                    return o;
                }

                /** Check if the value is correct */
                inline bool checkImpl() const
                {
                    return size > 0 && size < 5 && (value[size-1] & 0x80) == 0;
                }
#if MQTTAvoidValidation != 1
                bool check() const { return checkImpl(); }
#endif
                /** For consistancy with the other structures, we have a getSize() method that gives the number of bytes requires to serialize this object */
                uint32 getSize() const { return size; }

                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 4 bytes long
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const { memcpy(buffer, value, size); return size; }
                /** Read the value from a buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 4 bytes long
                    @return The number of bytes read from the buffer, or BadData upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    for (size = 0;;)
                    {
                        if ((uint32)(size+1) > bufLength) return NotEnoughData;
                        value[size] = buffer[size];
                        if (value[size++] < 0x80) break;
                        if (size == 4) return BadData;
                    }
                    return size;
                }
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0) { out += MQTTStringPrintf("%*sVBInt: %u\n", (int)indent, "", (uint32)*this); }
#endif

                /** Default constructor */
                VBInt(uint32 value = 0) { this->operator=(value); }
                /** Copy constructor */
                VBInt(const VBInt & other) : word(other.word), size(other.size) { }
            };

            /** The variable byte integer encoding (section 1.5.5).
                It's always stored encoded as a network version */
            struct MappedVBInt Final : public PODVisitor<uint32>
            {
                /** Get the value from this mapped variable byte integer
                    @param buffer       A pointer to the buffer to read from
                    @param bufLength    The length of the buffer to read from
                    @return the number of bytes used in the buffer */
                uint32 acceptBuffer(const uint8 * buffer, const uint32 bufLength)
                {
                    uint32 size = 0; uint32 & o = getValue(); o = 0;
                    for (size = 0; size < 4 && size < bufLength; )
                    {
                        if (size+1 > bufLength) return NotEnoughData;
                        if (buffer[size++] < 0x80) break;
                    }

                    switch(size)
                    {
                    default:
                    case 0: return BadData; // This is an error anyway
                    case 4: o = buffer[3] << 21;            // Intentionally no break here
                    // fall through
                    case 3: o |= (buffer[2] & 0x7F) << 14;  // Same
                    // fall through
                    case 2: o |= (buffer[1] & 0x7F) << 7;   // Ditto
                    // fall through
                    case 1: o |= buffer[0] & 0x7F; // Break is useless here too
                    }
                    return size;
                }
            };




            /** The control packet type.
                Src means the expected direction, C is for client to server, S for server to client and B for both direction. */
            enum ControlPacketType
            {
                RESERVED        = 0,    //!< Src:Forbidden, it's reserved
                CONNECT         = 1,    //!< Src:C Connection requested
                CONNACK         = 2,    //!< Src:S Connection acknowledged
                PUBLISH         = 3,    //!< Src:B Publish message
                PUBACK          = 4,    //!< Src:B Publish acknowledged (QoS 1)
                PUBREC          = 5,    //!< Src:B Publish received (QoS 2 delivery part 1)
                PUBREL          = 6,    //!< Src:B Publish released (QoS 2 delivery part 2)
                PUBCOMP         = 7,    //!< Src:B Publish completed (QoS 2 delivery part 3)
                SUBSCRIBE       = 8,    //!< Src:C Subscribe requested
                SUBACK          = 9,    //!< Src:S Subscribe acknowledged
                UNSUBSCRIBE     = 10,   //!< Src:C Unsubscribe requested
                UNSUBACK        = 11,   //!< Src:S Unsubscribe acknowledged
                PINGREQ         = 12,   //!< Src:C Ping requested
                PINGRESP        = 13,   //!< Src:S Ping answered
                DISCONNECT      = 14,   //!< Src:B Disconnect notification
                AUTH            = 15,   //!< Src:B Authentication exchanged
            };

            /** This is a useless struct used to define a static function in the header */
            struct Helper
            {
                /** Used for debug only, this convert the control packet type to a name */
                static const char * getControlPacketName(const ControlPacketType type)
                {
                    static const char * names[16] = { "RESERVED", "CONNECT", "CONNACK", "PUBLISH", "PUBACK", "PUBREC", "PUBREL", "PUBCOMP", "SUBSCRIBE", "SUBACK",
                                                      "UNSUBSCRIBE", "UNSUBACK", "PINGREQ", "PINGRESP", "DISCONNECT", "AUTH" };
                    return names[(int)type];
                }
                /** Get the next packet for each ACK of publishing */
                static ControlPacketType getNextPacketType(const ControlPacketType type)
                {
                    static uint8 nexts[16] = { 0, 0, 0, PUBACK, 0, PUBREL, PUBCOMP, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
                    return (ControlPacketType)nexts[type];
                }
            };
        }

        /** The version 5 for this protocol (OASIS MQTTv5 http://docs.oasis-open.org/mqtt/mqtt/v5.0/mqtt-v5.0.html ) */
        namespace V5
        {
            // Bring shared types here
            using namespace Protocol::MQTT::Common;

            /** A generic type erasure class to minimize different code dealing with types */
            struct GenericTypeBase
            {
                virtual uint32 typeSize() const = 0;
                /** From network and To network are supposed to be called in succession
                    leading to the same state so they are both const even if individually, they modify the value */
                virtual void swapNetwork() const = 0;
                virtual void * raw() = 0;
#if MQTTAvoidValidation != 1
                bool check() const { return true; }
#endif
            };
            /** A globally used GenericType that's there to minimize the number of generic code
                that needs to be specialized by the compiler */
            template <typename T>
            struct GenericType : public GenericTypeBase
            {
                T value;
                uint32 typeSize() const { return sizeof(T); }
                void swapNetwork() const { const_cast<T&>(value) = BigEndian(value); }
                void * raw() { return &value; }
                operator T & () { return value; }
                GenericType & operator = (const T v) { value = v; return *this; }
                GenericType(T v = 0) : value(v) {}
            };



            /** The reason codes */
            enum ReasonCodes
            {
                Success                             = 0x00, //!< Success
                NormalDisconnection                 = 0x00, //!< Normal disconnection
                GrantedQoS0                         = 0x00, //!< Granted QoS 0
                GrantedQoS1                         = 0x01, //!< Granted QoS 1
                GrantedQoS2                         = 0x02, //!< Granted QoS 2
                DisconnectWithWillMessage           = 0x04, //!< Disconnect with Will Message
                NoMatchingSubscribers               = 0x10, //!< No matching subscribers
                NoSubscriptionExisted               = 0x11, //!< No subscription existed
                ContinueAuthentication              = 0x18, //!< Continue authentication
                ReAuthenticate                      = 0x19, //!< Re-authenticate
                UnspecifiedError                    = 0x80, //!< Unspecified error
                MalformedPacket                     = 0x81, //!< Malformed Packet
                ProtocolError                       = 0x82, //!< Protocol Error
                ImplementationSpecificError         = 0x83, //!< Implementation specific error
                UnsupportedProtocolVersion          = 0x84, //!< Unsupported Protocol Version
                ClientIdentifierNotValid            = 0x85, //!< Client Identifier not valid
                BadUserNameOrPassword               = 0x86, //!< Bad User Name or Password
                NotAuthorized                       = 0x87, //!< Not authorized
                ServerUnavailable                   = 0x88, //!< Server unavailable
                ServerBusy                          = 0x89, //!< Server busy
                Banned                              = 0x8A, //!< Banned
                ServerShuttingDown                  = 0x8B, //!< Server shutting down
                BadAuthenticationMethod             = 0x8C, //!< Bad authentication method
                KeepAliveTimeout                    = 0x8D, //!< Keep Alive timeout
                SessionTakenOver                    = 0x8E, //!< Session taken over
                TopicFilterInvalid                  = 0x8F, //!< Topic Filter invalid
                TopicNameInvalid                    = 0x90, //!< Topic Name invalid
                PacketIdentifierInUse               = 0x91, //!< Packet Identifier in use
                PacketIdentifierNotFound            = 0x92, //!< Packet Identifier not found
                ReceiveMaximumExceeded              = 0x93, //!< Receive Maximum exceeded
                TopicAliasInvalid                   = 0x94, //!< Topic Alias invalid
                PacketTooLarge                      = 0x95, //!< Packet too large
                MessageRateTooHigh                  = 0x96, //!< Message rate too high
                QuotaExceeded                       = 0x97, //!< Quota exceeded
                AdministrativeAction                = 0x98, //!< Administrative action
                PayloadFormatInvalid                = 0x99, //!< Payload format invalid
                RetainNotSupported                  = 0x9A, //!< Retain not supported
                QoSNotSupported                     = 0x9B, //!< QoS not supported
                UseAnotherServer                    = 0x9C, //!< Use another server
                ServerMoved                         = 0x9D, //!< Server moved
                SharedSubscriptionsNotSupported     = 0x9E, //!< Shared Subscriptions not supported
                ConnectionRateExceeded              = 0x9F, //!< Connection rate exceeded
                MaximumConnectTime                  = 0xA0, //!< Maximum connect time
                SubscriptionIdentifiersNotSupported = 0xA1, //!< Subscription Identifiers not supported
                WildcardSubscriptionsNotSupported   = 0xA2, //!< Wildcard Subscriptions not supported
            };

            /** The dynamic string class we prefer using depends on whether we are using client or server code */
#if MQTTClientOnlyImplementation == 1
            typedef DynamicStringView   DynString;
            typedef DynamicBinDataView  DynBinData;
#else
            typedef DynamicString       DynString;
            typedef DynamicBinaryData   DynBinData;
#endif




#pragma pack(push, 1)
            /** A MQTT fixed header (section 2.1.1).
                This is not used directly, but only to remember the expected format. Instead, each packet type is declared underneath, since it's faster to parse them directly */
            struct FixedHeader
            {
                union
                {
                    uint8 raw;
                    BitField<uint8, 4, 4> type;
                    BitField<uint8, 3, 1> dup;
                    BitField<uint8, 1, 2> QoS;
                    BitField<uint8, 0, 1> retain;

                };
            };

            struct FixedHeaderBase
            {
                uint8 typeAndFlags;
                virtual ControlPacketType   getType() const { return (ControlPacketType)(typeAndFlags >> 4); }
                virtual uint8               getFlags() const { return typeAndFlags & 0xF; }
#if MQTTAvoidValidation != 1
                virtual bool check() const { return true; }
#endif
#if MQTTDumpCommunication == 1
                virtual void dump(MQTTString & out, const int indent = 0) { out += MQTTStringPrintf("%*sHeader: (type %s, no flags)\n", (int)indent, "", Helper::getControlPacketName(getType())); }
#endif

                FixedHeaderBase(const ControlPacketType type, const uint8 flags) : typeAndFlags(((uint8)type) << 4 | (flags & 0xF)) {}
                ~FixedHeaderBase() {} // Not virtual here to avoid generating
            };

            /** The common format for the fixed header type */
            template <ControlPacketType type, uint8 flags>
            struct FixedHeaderType Final : public FixedHeaderBase
            {
                bool                check() const { return getFlags() == flags; }
                static bool         checkFlag(const uint8 flag) { return flag == flags; }

                FixedHeaderType() : FixedHeaderBase(type, flags) {}
            };

            /** The only header where flags have a meaning is for Publish operation */
            template <>
            struct FixedHeaderType<PUBLISH, 0> Final : public FixedHeaderBase
            {
                bool isDup()     const { return typeAndFlags & 0x8; }
                bool isRetain()  const { return typeAndFlags & 0x1; }
                uint8 getQoS()   const { return (typeAndFlags & 0x6) >> 1; }

                void setDup(const bool e)       { typeAndFlags = (typeAndFlags & ~0x8) | (e ? 8 : 0); }
                void setRetain(const bool e)    { typeAndFlags = (typeAndFlags & ~0x1) | (e ? 1 : 0); }
                void setQoS(const uint8 e)      { typeAndFlags = (typeAndFlags & ~0x6) | (e < 3 ? (e << 1) : 0); }

                static bool         checkFlag(const uint8 flag) { return true; }
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0) { out += MQTTStringPrintf("%*sHeader: (type PUBLISH, retain %d, QoS %d, dup %d)\n", (int)indent, "", isRetain(), getQoS(), isDup()); }
#endif


                FixedHeaderType(const uint8 flags = 0) : FixedHeaderBase(PUBLISH, flags) {}
                FixedHeaderType(const bool dup, const uint8 QoS, const bool retain) : FixedHeaderBase(PUBLISH, (dup ? 8 : 0) | (retain ? 1 : 0) | (QoS < 3 ? (QoS << 1) : 0)) {}
            };

            /** The possible header types */
            typedef FixedHeaderType<CONNECT,    0> ConnectHeader;
            typedef FixedHeaderType<CONNACK,    0> ConnectACKHeader;
            typedef FixedHeaderType<PUBLISH,    0> PublishHeader;
            typedef FixedHeaderType<PUBACK,     0> PublishACKHeader;
            typedef FixedHeaderType<PUBREC,     0> PublishReceivedHeader;
            typedef FixedHeaderType<PUBREL,     2> PublishReleasedHeader;
            typedef FixedHeaderType<PUBCOMP,    0> PublishCompletedHeader;
            typedef FixedHeaderType<SUBSCRIBE,  2> SubscribeHeader;
            typedef FixedHeaderType<SUBACK,     0> SubscribeACKHeader;
            typedef FixedHeaderType<UNSUBSCRIBE,2> UnsubscribeHeader;
            typedef FixedHeaderType<UNSUBACK,   0> UnsubscribeACKHeader;
            typedef FixedHeaderType<PINGREQ,    0> PingRequestHeader;
            typedef FixedHeaderType<PINGRESP,   0> PingACKHeader;
            typedef FixedHeaderType<DISCONNECT, 0> DisconnectHeader;
            typedef FixedHeaderType<AUTH,       0> AuthenticationHeader;
#pragma pack(pop)

            /** Simple check header code and packet size.
                @return error that will be detected with isError() or the number of bytes required for this packet */
            static inline uint32 checkHeader(const uint8 * buffer, const uint32 size, ControlPacketType * type = 0)
            {
                if (size < 2) return NotEnoughData;
                uint8 expectedFlags[] = { 0xF, 0, 0xF, 0, 0, 2, 0, 2, 0, 2, 0, 0, 0, 0, 0, 0};
                if ((*buffer >> 4) != PUBLISH && ((*buffer & 0xF) ^ expectedFlags[(*buffer>>4)])) return BadData;
                if (type) *type = (ControlPacketType)(*buffer >> 4);
                // Then read the VB header
                VBInt len;
                uint32 s = len.readFrom(buffer + 1, size - 1);
                if (isError(s)) return s;
                return (uint32)len + s + 1;
            }

            /** The known property types (section 2.2.2.2) */
            enum PropertyType
            {
                BadProperty             = 0,    //!< Does not exist in the standard, but useful to store a bad property type


                PayloadFormat           = 0x01, //!< Payload Format Indicator
                MessageExpiryInterval   = 0x02, //!< Message Expiry Interval
                ContentType             = 0x03, //!< Content Type
                ResponseTopic           = 0x08, //!< Response Topic
                CorrelationData         = 0x09, //!< Correlation Data
                SubscriptionID          = 0x0B, //!< Subscription Identifier
                SessionExpiryInterval   = 0x11, //!< Session Expiry Interval
                AssignedClientID        = 0x12, //!< Assigned Client Identifier
                ServerKeepAlive         = 0x13, //!< Server Keep Alive
                AuthenticationMethod    = 0x15, //!< Authentication Method
                AuthenticationData      = 0x16, //!< Authentication Data
                RequestProblemInfo      = 0x17, //!< Request Problem Information
                WillDelayInterval       = 0x18, //!< Will Delay Interval
                RequestResponseInfo     = 0x19, //!< Request Response Information
                ResponseInfo            = 0x1A, //!< Response Information
                ServerReference         = 0x1C, //!< Server Reference
                ReasonString            = 0x1F, //!< Reason String
                ReceiveMax              = 0x21, //!< Receive Maximum
                TopicAliasMax           = 0x22, //!< Topic Alias Maximum
                TopicAlias              = 0x23, //!< Topic Alias
                QoSMax                  = 0x24, //!< Maximum QoS
                RetainAvailable         = 0x25, //!< Retain Available
                UserProperty            = 0x26, //!< User Property
                PacketSizeMax           = 0x27, //!< Maximum Packet Size
                WildcardSubAvailable    = 0x28, //!< Wildcard Subscription Available
                SubIDAvailable          = 0x29, //!< Subscription Identifier Available
                SharedSubAvailable      = 0x2A, //!< Shared Subscription Available




                MaxUsedPropertyType,            //!< Used as a gatekeeper for the knowing the maximum value for the properties
            };

            namespace PrivateRegistry
            {
                template <typename T> struct SizeOf { enum { Size = sizeof(T) }; };

                // Not using variadic template here since this can be built without C++11
                template <typename T, typename U>
                struct MaxSize { enum { Size = sizeof(T) > (size_t)U::Size ? sizeof(T) : (size_t)U::Size }; };

                struct MaxVisitorsSize
                {
                    enum { Size = MaxSize<PODVisitor<uint8>,
                        MaxSize<LittleEndianPODVisitor<uint16>,
                        MaxSize<LittleEndianPODVisitor<uint32>,
                        MaxSize<MappedVBInt,
                        MaxSize<DynamicBinDataView,
                        MaxSize<DynamicStringView,
                        SizeOf<DynamicStringPairView> > > > > > >::Size };
                };

                // Copy of std::is_same that should work even without C++11
                struct false_type { enum { Value = false}; };
                struct true_type { enum { Value = true}; };
                template<typename, typename> struct is_same : public false_type { };
                template<typename T>         struct is_same<T, T> : public true_type { };

                // By default, all types are invalid except for those below
                template <typename T> struct isValidType {};

                template<> struct isValidType< PODVisitor<uint8>              > { enum { Value = 0 }; };
                template<> struct isValidType< LittleEndianPODVisitor<uint16> > { enum { Value = 1 }; };
                template<> struct isValidType< LittleEndianPODVisitor<uint32> > { enum { Value = 2 }; };
                template<> struct isValidType< MappedVBInt                    > { enum { Value = 3 }; };
                template<> struct isValidType< DynamicBinDataView             > { enum { Value = 4 }; };
                template<> struct isValidType< DynamicStringView              > { enum { Value = 5 }; };
                template<> struct isValidType< DynamicStringPairView          > { enum { Value = 6 }; };


                enum { PropertiesCount = 27 };
                static const uint8 invPropertyMap[MaxUsedPropertyType] =
                {
                    PropertiesCount, // BadProperty,
                     0, // PayloadFormat           ,
                     1, // MessageExpiryInterval   ,
                     2, // ContentType             ,
                    PropertiesCount, // BadProperty,
                    PropertiesCount, // BadProperty,
                    PropertiesCount, // BadProperty,
                    PropertiesCount, // BadProperty,
                     3, // ResponseTopic           ,
                     4, // CorrelationData         ,
                    PropertiesCount, // BadProperty,
                     5, // SubscriptionID          ,
                    PropertiesCount, // BadProperty,
                    PropertiesCount, // BadProperty,
                    PropertiesCount, // BadProperty,
                    PropertiesCount, // BadProperty,
                    PropertiesCount, // BadProperty,
                     6, // SessionExpiryInterval   ,
                     7, // AssignedClientID        ,
                     8, // ServerKeepAlive         ,
                    PropertiesCount, // BadProperty,
                     9, // AuthenticationMethod    ,
                    10, // AuthenticationData      ,
                    11, // RequestProblemInfo      ,
                    12, // WillDelayInterval       ,
                    13, // RequestResponseInfo     ,
                    14, // ResponseInfo            ,
                    PropertiesCount, // BadProperty,
                    15, // ServerReference         ,
                    PropertiesCount, // BadProperty,
                    PropertiesCount, // BadProperty,
                    16, // ReasonString            ,
                    PropertiesCount, // BadProperty,
                    17, // ReceiveMax              ,
                    18, // TopicAliasMax           ,
                    19, // TopicAlias              ,
                    20, // QoSMax                  ,
                    21, // RetainAvailable         ,
                    22, // UserProperty            ,
                    23, // PacketSizeMax           ,
                    24, // WildcardSubAvailable    ,
                    25, // SubIDAvailable          ,
                    26, // SharedSubAvailable      ,
                };

                /** Get the property name for a given property type */
                static const char * getPropertyName(const uint8 propertyType)
                {
                    static const char* propertyMap[PrivateRegistry::PropertiesCount] =
                    {
                        "PayloadFormat", "MessageExpiryInterval", "ContentType", "ResponseTopic", "CorrelationData",
                        "SubscriptionID", "SessionExpiryInterval", "AssignedClientID", "ServerKeepAlive",
                        "AuthenticationMethod", "AuthenticationData", "RequestProblemInfo", "WillDelayInterval",
                        "RequestResponseInfo", "ResponseInfo", "ServerReference", "ReasonString", "ReceiveMax",
                        "TopicAliasMax", "TopicAlias", "QoSMax", "RetainAvailable", "UserProperty", "PacketSizeMax",
                        "WildcardSubAvailable", "SubIDAvailable", "SharedSubAvailable",
                    };
                    if (propertyType >= MaxUsedPropertyType) return 0;
                    uint8 index = PrivateRegistry::invPropertyMap[propertyType];
                    if (index == PrivateRegistry::PropertiesCount) return 0;
                    return propertyMap[index];
                }
            }

            /** Avoid storing many instance of static visitors in BSS and memory.
                Instead, use a variant and only store the index to the visitor type.
                Currently, the possible property types are:
                    PODVisitor<uint8>
                    LittleEndianPODVisitor<uint16>
                    LittleEndianPODVisitor<uint32>
                    MappedVBInt
                    DynamicBinDataView
                    DynamicStringView
                    DynamicStringPairView
            */
            struct VisitorVariant
            {
            private:
                /** Where the buffer for the visitor is being build */
                uint8 buffer[PrivateRegistry::MaxVisitorsSize::Size];
                /** The visitor type */
                uint8 type;

                /** Used to avoid declaring an external variable when iterating properties */
                PropertyType propType;
                /** Used to avoid declaring an external variable when iterating properties */
                uint32       offset;


                MemMappedVisitor * getBase()
                {
                    switch(type)
                    {
                    case 0: return static_cast<MemMappedVisitor*>(reinterpret_cast< PODVisitor<uint8>* >             (buffer));
                    case 1: return static_cast<MemMappedVisitor*>(reinterpret_cast< LittleEndianPODVisitor<uint16>* >(buffer));
                    case 2: return static_cast<MemMappedVisitor*>(reinterpret_cast< LittleEndianPODVisitor<uint32>* >(buffer));
                    case 3: return static_cast<MemMappedVisitor*>(reinterpret_cast< MappedVBInt *>                   (buffer));
                    case 4: return static_cast<MemMappedVisitor*>(reinterpret_cast< DynamicBinDataView *>            (buffer));
                    case 5: return static_cast<MemMappedVisitor*>(reinterpret_cast< DynamicStringView *>             (buffer));
                    case 6: return static_cast<MemMappedVisitor*>(reinterpret_cast< DynamicStringPairView *>         (buffer));
                    default: return 0;
                    }
                }
            public:
                uint32 acceptBuffer(const uint8 * buf, const uint32 bufLength)
                {
                    if (MemMappedVisitor * v = getBase()) return v->acceptBuffer(buf, bufLength);
                    return BadData;
                }
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    if (MemMappedVisitor * v = getBase()) v->dump(out, indent);
                }
#endif



                template <typename T>
                T * as()
                {
                    switch (type)
                    {
                    case 0: return PrivateRegistry::is_same<T, PODVisitor<uint8> >             ::Value ? reinterpret_cast<T*>(buffer) : 0;
                    case 1: return PrivateRegistry::is_same<T, LittleEndianPODVisitor<uint16> >::Value ? reinterpret_cast<T*>(buffer) : 0;
                    case 2: return PrivateRegistry::is_same<T, LittleEndianPODVisitor<uint32> >::Value ? reinterpret_cast<T*>(buffer) : 0;
                    case 3: return PrivateRegistry::is_same<T, MappedVBInt >                   ::Value ? reinterpret_cast<T*>(buffer) : 0;
                    case 4: return PrivateRegistry::is_same<T, DynamicBinDataView >            ::Value ? reinterpret_cast<T*>(buffer) : 0;
                    case 5: return PrivateRegistry::is_same<T, DynamicStringView >             ::Value ? reinterpret_cast<T*>(buffer) : 0;
                    case 6: return PrivateRegistry::is_same<T, DynamicStringPairView >         ::Value ? reinterpret_cast<T*>(buffer) : 0;
                    default: return 0;
                    }
                }
//                template <typename T>
//                inline T* as(T* const &) { return as<T>(); }

                /** Mutate with the given type */
                bool mutate(const uint8 type, const PropertyType prop)
                {
                    switch(type)
                    {
                    case 0: mutate< PODVisitor<uint8> >             (prop); return true;
                    case 1: mutate< LittleEndianPODVisitor<uint16> >(prop); return true;
                    case 2: mutate< LittleEndianPODVisitor<uint32> >(prop); return true;
                    case 3: mutate< MappedVBInt >                   (prop); return true;
                    case 4: mutate< DynamicBinDataView >            (prop); return true;
                    case 5: mutate< DynamicStringView >             (prop); return true;
                    case 6: mutate< DynamicStringPairView >         (prop); return true;
                    default: this->type = type; return false;
                    }
                }

                template <typename T>
                void mutate(const PropertyType prop)
                {
                    // If the compiler stops here, it means you're trying to get a visitor for an unsupported type
                    // Only supported types are written above
                    type = PrivateRegistry::isValidType<T>::Value;
                    propType = prop;
                    new (buffer) T();
                }

                /** Get the visitor property type */
                PropertyType propertyType() const { return propType; }
                /** Get the visitor property type */
                void propertyType(const PropertyType prop) { propType = prop; }
                /** Get the current iterator offset */
                uint32 getOffset() const { return offset; }
                /** Set the offset */
                void setOffset(const uint32 offset) { this->offset = offset; }

                /** By default, it's an invalid variant */
                VisitorVariant() : type(7), propType(BadProperty), offset(0) {}

                template <typename T>
                VisitorVariant() : type(PrivateRegistry::isValidType<T>::Value), propType(BadProperty), offset(0) { new (buffer) T(); }

                ~VisitorVariant() {} // No destruction required here
            };


            /** A registry used to store the mapping between properties and their visitor */
            class MemMappedPropertyRegistry
            {
                uint8 propertiesType[PrivateRegistry::PropertiesCount];

            public:
                /** Singleton pattern */
                static MemMappedPropertyRegistry & getInstance()
                {
                    static MemMappedPropertyRegistry registry;
                    return registry;
                }

                /** Get the property name (used for dumping code mainly) */
                static const char * getPropertyName(const uint8 propertyType) { return PrivateRegistry::getPropertyName(propertyType); }

                /** Get a visitor for the given property.
                    You provide a visitor variant as input and it'll be mutated for the given property type.
                    You can then use VisitorVariant::acceptBuffer or VisitorVariant::as to get back the
                    real visited type.

                    Example code:
                    @code
                        VisitorVariant visitor;
                        if (MemMappedPropertyRegistry::getInstance().getVisitorForProperty(visitor, MaxPacketSize)
                            && visitor.acceptBuffer(buffer, bufLength))
                        {
                            auto maxSize = visitor.as< LittleEndianPODVisitor<uint32> >();
                            printf("Max packet size: %u\n", maxSize);
                        }
                    @endcode
                    */
                bool getVisitorForProperty(VisitorVariant & visitor, const uint8 propertyType)
                {
                    if (propertyType >= MaxUsedPropertyType) return false;

                    uint8 index = PrivateRegistry::invPropertyMap[propertyType];
                    if (index == PrivateRegistry::PropertiesCount) return false;
                    return visitor.mutate(propertiesType[index], (PropertyType)propertyType);
                }

            private:
                MemMappedPropertyRegistry()
                {
                    // Register all properties now
                    propertiesType[ 0] = 0; /* PODVisitor<uint8>              */ // PayloadFormat
                    propertiesType[ 1] = 2; /* LittleEndianPODVisitor<uint32> */ // MessageExpiryInterval
                    propertiesType[ 2] = 5; /* DynamicStringView              */ // ContentType
                    propertiesType[ 3] = 5; /* DynamicStringView              */ // ResponseTopic
                    propertiesType[ 4] = 4; /* DynamicBinDataView             */ // CorrelationData
                    propertiesType[ 5] = 3; /* MappedVBInt                    */ // SubscriptionID
                    propertiesType[ 6] = 2; /* LittleEndianPODVisitor<uint32> */ // SessionExpiryInterval
                    propertiesType[ 7] = 5; /* DynamicStringView              */ // AssignedClientID
                    propertiesType[ 8] = 1; /* LittleEndianPODVisitor<uint16> */ // ServerKeepAlive
                    propertiesType[ 9] = 5; /* DynamicStringView              */ // AuthenticationMethod
                    propertiesType[10] = 4; /* DynamicBinDataView             */ // AuthenticationData
                    propertiesType[11] = 0; /* PODVisitor<uint8>              */ // RequestProblemInfo
                    propertiesType[12] = 2; /* LittleEndianPODVisitor<uint32> */ // WillDelayInterval
                    propertiesType[13] = 0; /* PODVisitor<uint8>              */ // RequestResponseInfo
                    propertiesType[14] = 5; /* DynamicStringView              */ // ResponseInfo
                    propertiesType[15] = 5; /* DynamicStringView              */ // ServerReference
                    propertiesType[16] = 5; /* DynamicStringView              */ // ReasonString
                    propertiesType[17] = 1; /* LittleEndianPODVisitor<uint16> */ // ReceiveMax
                    propertiesType[18] = 1; /* LittleEndianPODVisitor<uint16> */ // TopicAliasMax
                    propertiesType[19] = 1; /* LittleEndianPODVisitor<uint16> */ // TopicAlias
                    propertiesType[20] = 0; /* PODVisitor<uint8>              */ // QoSMax
                    propertiesType[21] = 0; /* PODVisitor<uint8>              */ // RetainAvailable
                    propertiesType[22] = 6; /* DynamicStringPairView          */ // UserProperty
                    propertiesType[23] = 2; /* LittleEndianPODVisitor<uint32> */ // PacketSizeMax
                    propertiesType[24] = 0; /* PODVisitor<uint8>              */ // WildcardSubAvailable
                    propertiesType[25] = 0; /* PODVisitor<uint8>              */ // SubIDAvailable
                    propertiesType[26] = 0; /* PODVisitor<uint8>              */ // SharedSubAvailable
                }
            };


            /** This is a simple property header that's common to all properties */
            struct PropertyBase : public Serializable
            {
                /** While we should support a variable length property type, there is no property type allowed above 127 for now, so let's resume to a single uint8 */
                const uint8 type;
                /** The next property in the list */
                PropertyBase * next;
                /** Whether we need to delete the object (default to false) */
                bool heapAllocated;


                /** The property type */
                PropertyBase(const PropertyType type, const bool heap = false) : type((uint8)type), next(0), heapAllocated(heap) {}
                /** Clone the property */
                virtual PropertyBase * clone() const = 0;
                /** Suicide (walking the list is done in Properties::suicide) */
                void suicide() { if (heapAllocated) delete this; }
                /** Accept a visitor for this property */
                virtual bool acceptVisitor(VisitorVariant & visitor) const
                {
                    if (!MemMappedPropertyRegistry::getInstance().getVisitorForProperty(visitor, type)) return false;
                    return false;
                }
                /** Virtual destructor is required since we destruct virtually the chained list */
                virtual ~PropertyBase() {}
            };

            /** The base of all PropertyView. They are mapped on an existing buffer and are not allocating anything. */
            struct PropertyBaseView : public MemMappedVisitor
            {
                /** While we should support a variable length property type, there is no property type allowed above 127 for now, so let's resume to a single uint8 */
                const uint8 type;
                /** The property type */
                PropertyBaseView(const PropertyType type) : type((uint8)type) {}
                /** Required destructor */
                virtual ~PropertyBaseView() {}
            };


            struct PropertyBaseImpl : public PropertyBase
            {
                GenericTypeBase & value;
                /** This give the size required for serializing this property header in bytes */
                uint32 getSize() const { return sizeof(type) + value.typeSize(); }
                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long, and at worst very large (use getSize to figure out the required size).
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const { buffer[0] = type; value.swapNetwork(); memcpy(buffer+1, value.raw(), value.typeSize()); value.swapNetwork(); return value.typeSize() + 1; }
                /** Read the value from a buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long
                    @return The number of bytes read from the buffer, or BadData upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if ((buffer[0] & 0x80) || buffer[0] != type) return BadData;
                    if (bufLength < value.typeSize()+1) return NotEnoughData;
                    memcpy(value.raw(), buffer+1, value.typeSize());
                    value.swapNetwork();
                    return value.typeSize() + 1;
                }
#if MQTTAvoidValidation != 1
                /** Check if this property is valid */
                bool check() const { return type < 0x80; }
#endif
                /** The default constructor */
                PropertyBaseImpl(const PropertyType type, GenericTypeBase & v, const bool heap = false) : PropertyBase(type, heap), value(v) {}
            };

            /** The link between the property type and its possible value follow section 2.2.2.2 */
            template <typename T>
            struct Property Final : public PropertyBaseImpl
            {
                /** The property value, depends on the type */
                GenericType<T>           value;

#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    out += MQTTStringPrintf("%*sType %s\n", indent, "", PrivateRegistry::getPropertyName(type));
                    out += MQTTStringPrintf("%*s", indent+2, ""); out += (T)value; out += "\n";
                }
#endif

                /** Clone this property */
                PropertyBase * clone() const { return new Property((PropertyType)type, value.value, true); }
                /** Accept a visitor for this type */
                bool acceptVisitor(VisitorVariant & visitor) const
                {
                    if (!PropertyBase::acceptVisitor(visitor)) return false;
                    LittleEndianPODVisitor<T> * view = visitor.as< LittleEndianPODVisitor<T> >();
                    if (!view) return false;
                    return view->acceptBuffer((const uint8*)const_cast< GenericType<T>& >(value).raw(), value.typeSize()) == value.typeSize();
                }

                /** The default constructor */
                Property(const PropertyType type, T v = 0, const bool heap = false) : PropertyBaseImpl(type, value, heap), value(v) {}
            };

            template <>
            struct Property<uint8> Final : public PropertyBaseImpl
            {
                /** The property value, depends on the type */
                GenericType<uint8>           value;

#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    out += MQTTStringPrintf("%*sType %s\n", indent, "", PrivateRegistry::getPropertyName(type));
                    out += MQTTStringPrintf("%*s", indent+2, ""); out += (uint8)value; out += "\n";
                }
#endif

                /** Clone this property */
                PropertyBase * clone() const { return new Property((PropertyType)type, value.value, true); }
                /** Accept a visitor for this type */
                bool acceptVisitor(VisitorVariant & visitor) const
                {
                    if (!PropertyBase::acceptVisitor(visitor)) return false;
                    PODVisitor<uint8> * view = visitor.as< PODVisitor<uint8> >();
                    if (!view) return false;
                    return view->acceptBuffer((const uint8*)const_cast< GenericType<uint8>& >(value).raw(), value.typeSize()) == value.typeSize();
                }

                /** The default constructor */
                Property(const PropertyType type, uint8 v = 0, const bool heap = false) : PropertyBaseImpl(type, value, heap), value(v) {}
            };


            template<>
            struct Property<DynamicString> Final : public PropertyBase
            {
                /** The property value, depends on the type */
                DynamicString       value;

                /** This give the size required for serializing this property header in bytes */
                uint32 getSize() const { return sizeof(type) + value.getSize(); }
                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long, and at worst very large (use getSize to figure out the required size).
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const { buffer[0] = type; uint32 o = value.copyInto(buffer+1); return o + 1; }
                /** Read the value from a buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long
                    @return The number of bytes read from the buffer, or BadData upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if ((buffer[0] & 0x80) || buffer[0] != type) return BadData;
                    if (bufLength < 3) return NotEnoughData;
                    uint32 o = value.readFrom(buffer+1, bufLength - 1);
                    if (isError(o)) return o;
                    return o+1;
                }
#if MQTTAvoidValidation != 1
                /** Check if this property is valid */
                bool check() const { return type < 0x80 && value.check(); }
#endif
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    out += MQTTStringPrintf("%*sType %s\n", indent, "", PrivateRegistry::getPropertyName(type));
                    value.dump(out, indent + 2);
                }
#endif
                /** Clone this property */
                PropertyBase * clone() const { return new Property((PropertyType)type, value, true); }
                /** Accept a visitor for this type */
                bool acceptVisitor(VisitorVariant & visitor) const
                {
                    if (!PropertyBase::acceptVisitor(visitor)) return false;
                    DynamicStringView * view = visitor.as<DynamicStringView>();
                    if (!view) return false;
                    *view = value;
                    return true;
                }

                /** The default constructor */
                Property(const PropertyType type, const DynamicString value = "", const bool heap = false) : PropertyBase(type, heap), value(value) {}
            };

            template<>
            struct Property<DynamicBinaryData> Final : public PropertyBase
            {
                /** The property value, depends on the type */
                DynamicBinaryData   value;

                /** This give the size required for serializing this property header in bytes */
                uint32 getSize() const { return sizeof(type) + value.getSize(); }
                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long, and at worst very large (use getSize to figure out the required size).
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const { buffer[0] = type; uint32 o = value.copyInto(buffer+1); return o + 1; }
                /** Read the value from a buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long
                    @return The number of bytes read from the buffer, or BadData upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if ((buffer[0] & 0x80) || buffer[0] != type) return BadData;
                    if (bufLength < 3) return NotEnoughData;
                    uint32 o = value.readFrom(buffer+1, bufLength - 1);
                    if (isError(o)) return o;
                    return o+1;
                }
#if MQTTAvoidValidation != 1
                /** Check if this property is valid */
                bool check() const { return type < 0x80 && value.check(); }
#endif
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    out += MQTTStringPrintf("%*sType %s\n", indent, "", PrivateRegistry::getPropertyName(type));
                    value.dump(out, indent + 2);
                }
#endif

                /** Clone this property */
                PropertyBase * clone() const { return new Property((PropertyType)type, value, true); }
                /** Accept a visitor for this type */
                bool acceptVisitor(VisitorVariant & visitor) const
                {
                    if (!PropertyBase::acceptVisitor(visitor)) return false;
                    DynamicBinDataView * view = visitor.as<DynamicBinDataView>();
                    if (!view) return false;
                    *view = value;
                    return true;
                }

                /** The default constructor */
                Property(const PropertyType type, const DynamicBinaryData value = 0, const bool heap = false) : PropertyBase(type, heap), value(value) {}
            };

            template<>
            struct Property<DynamicStringPair> Final : public PropertyBase
            {
                /** The property value, depends on the type */
                DynamicStringPair   value;

                /** This give the size required for serializing this property header in bytes */
                uint32 getSize() const { return sizeof(type) + value.getSize(); }
                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long, and at worst very large (use getSize to figure out the required size).
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const { buffer[0] = type; uint32 o = value.copyInto(buffer+1); return o + 1; }
                /** Read the value from a buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long
                    @return The number of bytes read from the buffer, or BadData upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if ((buffer[0] & 0x80) || buffer[0] != type) return BadData;
                    if (bufLength < 5) return NotEnoughData;
                    uint32 o = value.readFrom(buffer+1, bufLength - 1);
                    if (isError(o)) return o;
                    return o+1;
                }
#if MQTTAvoidValidation != 1
                /** Check if this property is valid */
                bool check() const { return type < 0x80 && value.check(); }
#endif
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    out += MQTTStringPrintf("%*sType %s\n", indent, "", PrivateRegistry::getPropertyName(type));
                    value.dump(out, indent + 2);
                }
#endif
                /** Clone this property */
                PropertyBase * clone() const { return new Property((PropertyType)type, value, true); }
                /** Accept a visitor for this type */
                bool acceptVisitor(VisitorVariant & visitor) const
                {
                    if (!PropertyBase::acceptVisitor(visitor)) return false;
                    DynamicStringPairView * view = visitor.as<DynamicStringPairView>();
                    if (!view) return false;
                    view->key = value.key;
                    view->value = value.value;
                    return true;
                }

                /** The default constructor */
                Property(const PropertyType type, const DynamicStringPair value, const bool heap = false) : PropertyBase(type, heap), value(value) {}
            };


            template<>
            struct Property<DynamicStringView> Final : public PropertyBase
            {
                /** The property value, depends on the type */
                DynamicStringView       value;

                /** This give the size required for serializing this property header in bytes */
                uint32 getSize() const { return sizeof(type) + value.getSize(); }
                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long, and at worst very large (use getSize to figure out the required size).
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const { buffer[0] = type; uint32 o = value.copyInto(buffer+1); return o + 1; }
                /** Read the value from a buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long
                    @return The number of bytes read from the buffer, or BadData upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if ((buffer[0] & 0x80) || buffer[0] != type) return BadData;
                    if (bufLength < 3) return NotEnoughData;
                    uint32 o = value.readFrom(buffer+1, bufLength - 1);
                    if (isError(o)) return o;
                    return o+1;
                }
#if MQTTAvoidValidation != 1
                /** Check if this property is valid */
                bool check() const { return type < 0x80 && value.check(); }
#endif
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    out += MQTTStringPrintf("%*sType %s\n", indent, "", PrivateRegistry::getPropertyName(type));
                    value.dump(out, indent + 2);
                }
#endif
                /** Clone this property */
                PropertyBase * clone() const { return new Property((PropertyType)type, value, true); }
                /** Accept a visitor for this type */
                bool acceptVisitor(VisitorVariant & visitor) const
                {
                    if (!PropertyBase::acceptVisitor(visitor)) return false;
                    DynamicStringView * view = visitor.as<DynamicStringView>();
                    if (!view) return false;
                    *view = value;
                    return true;
                }

                /** The default constructor */
                Property(const PropertyType type, const DynamicStringView value = "", const bool heap = false) : PropertyBase(type, heap), value(value) {}
            };

            template<>
            struct Property<DynamicBinDataView> Final : public PropertyBase
            {
                /** The property value, depends on the type */
                DynamicBinDataView   value;

                /** This give the size required for serializing this property header in bytes */
                uint32 getSize() const { return sizeof(type) + value.getSize(); }
                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long, and at worst very large (use getSize to figure out the required size).
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const { buffer[0] = type; uint32 o = value.copyInto(buffer+1); return o + 1; }
                /** Read the value from a buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long
                    @return The number of bytes read from the buffer, or BadData upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if ((buffer[0] & 0x80) || buffer[0] != type) return BadData;
                    if (bufLength < 3) return NotEnoughData;
                    uint32 o = value.readFrom(buffer+1, bufLength - 1);
                    if (isError(o)) return o;
                    return o+1;
                }
#if MQTTAvoidValidation != 1
                /** Check if this property is valid */
                bool check() const { return type < 0x80 && value.check(); }
#endif
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    out += MQTTStringPrintf("%*sType %s\n", indent, "", PrivateRegistry::getPropertyName(type));
                    value.dump(out, indent + 2);
                }
#endif
                /** Clone this property */
                PropertyBase * clone() const { return new Property((PropertyType)type, value, true); }
                /** Accept a visitor for this type */
                bool acceptVisitor(VisitorVariant & visitor) const
                {
                    if (!PropertyBase::acceptVisitor(visitor)) return false;
                    DynamicBinDataView * view = visitor.as<DynamicBinDataView>();
                    if (!view) return false;
                    *view = value;
                    return true;
                }
                /** The default constructor */
                Property(const PropertyType type, const DynamicBinDataView value = 0, const bool heap = false) : PropertyBase(type, heap), value(value) {}
            };

            template<>
            struct Property<DynamicStringPairView> Final : public PropertyBase
            {
                /** The property value, depends on the type */
                DynamicStringPairView   value;

                /** This give the size required for serializing this property header in bytes */
                uint32 getSize() const { return sizeof(type) + value.getSize(); }
                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long, and at worst very large (use getSize to figure out the required size).
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const { buffer[0] = type; uint32 o = value.copyInto(buffer+1); return o + 1; }
                /** Read the value from a buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long
                    @return The number of bytes read from the buffer, or BadData upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if ((buffer[0] & 0x80) || buffer[0] != type) return BadData;
                    if (bufLength < 5) return NotEnoughData;
                    uint32 o = value.readFrom(buffer+1, bufLength - 1);
                    if (isError(o)) return o;
                    return o+1;
                }
#if MQTTAvoidValidation != 1
                /** Check if this property is valid */
                bool check() const { return type < 0x80 && value.check(); }
#endif
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    out += MQTTStringPrintf("%*sType %s\n", indent, "", PrivateRegistry::getPropertyName(type));
                    value.dump(out, indent + 2);
                }
#endif
                /** Clone this property */
                PropertyBase * clone() const { return new Property((PropertyType)type, value, true); }
                /** Accept a visitor for this type */
                bool acceptVisitor(VisitorVariant & visitor) const
                {
                    if (!PropertyBase::acceptVisitor(visitor)) return false;
                    DynamicStringPairView * view = visitor.as<DynamicStringPairView>();
                    if (!view) return false;
                    *view = value;
                    return true;
                }
                /** The default constructor */
                Property(const PropertyType type, const DynamicStringPairView value, const bool heap = false) : PropertyBase(type, heap), value(value) {}
            };


            template<>
            struct Property<VBInt> Final : public PropertyBase
            {
                /** The property value, depends on the type */
                VBInt               value;

                /** This give the size required for serializing this property header in bytes */
                uint32 getSize() const { return sizeof(type) + value.getSize(); }
                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long, and at worst very large (use getSize to figure out the required size).
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const { buffer[0] = type; uint32 o = value.copyInto(buffer+1); return o + 1; }
                /** Read the value from a buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long
                    @return The number of bytes read from the buffer, or BadData upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if ((buffer[0] & 0x80) || buffer[0] != type) return BadData;
                    if (bufLength < 2) return NotEnoughData;
                    uint32 o = value.readFrom(buffer+1, bufLength - 1);
                    if (isError(o)) return o;
                    return o+1;
                }
#if MQTTAvoidValidation != 1
                /** Check if this property is valid */
                bool check() const { return type < 0x80 && value.check(); }
#endif
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    out += MQTTStringPrintf("%*sType %s\n", indent, "", PrivateRegistry::getPropertyName(type));
                    value.dump(out, indent + 2);
                }
#endif

                /** Clone this property */
                PropertyBase * clone() const { return new Property((PropertyType)type, value, true); }
                /** Accept a visitor for this type */
                bool acceptVisitor(VisitorVariant & visitor) const
                {
                    if (!PropertyBase::acceptVisitor(visitor)) return false;
                    MappedVBInt* view = visitor.as<MappedVBInt>();
                    if (!view) return false;
                    return view->acceptBuffer(value.value, value.size) == value.size;
                }

                /** The default constructor */
                Property(const PropertyType type, const uint32 value = 0, const bool heap = false) : PropertyBase(type, heap), value(value) {}
            };

            /** The deserialization registry for properties */
            struct PropertyRegistry
            {
                /** The function used to create a new instance of a property */
                typedef PropertyBase * (*InstantiateFunc)();

                /** The creator method array */
                InstantiateFunc unserializeFunc[MaxUsedPropertyType];
                /** Register a property to this registry */
                void registerProperty(PropertyType type, InstantiateFunc func) { unserializeFunc[type] = func; }
                /** Unserialize the given buffer, the buffer must point to the property to unserialize.
                    @param buffer       A pointer to a buffer that's bufLength bytes long
                    @param bufLength    Length of the buffer in bytes
                    @param output       On output, will be allocated to a Serializable (the expected property type)
                    @return the used number of bytes in the buffer, or a LocalError upon error */
                uint32 unserialize(const uint8 * buffer, uint32 bufLength, PropertyBase *& output)
                {
                    if (bufLength < 1 || !buffer) return NotEnoughData;
                    uint8 type = buffer[0];
                    if (type >= MaxUsedPropertyType) return BadData;
                    InstantiateFunc f = unserializeFunc[type];
                    if (!f) return BadData;
                    // Instantiate the right property
                    output = (*f)();
                    if (!output) return BadData;
                    return output->readFrom(buffer, bufLength);
                }
                /** Get an instance of this object */
                static PropertyRegistry & getInstance() { static PropertyRegistry reg; return reg; }
            };


            template <PropertyType type, typename T>
            struct TypedProperty
            {
                enum { Type = type };
                static PropertyBase * allocateProp() { return new Property<T>(type, T(), true) ; }
                TypedProperty(const T & value) : Property<T>(type, value) {}
            };

            /** Usual autoregister the property creation function */
            template <typename T>
            struct AutoRegisterProperty
            {
                AutoRegisterProperty() { PropertyRegistry::getInstance().registerProperty((PropertyType)T::Type, &T::allocateProp); }
            };


            /** Then declare all properties now, based on Table 2-4 */
            typedef TypedProperty<PayloadFormat, uint8>                     PayloadFormatProp;
            typedef TypedProperty<MessageExpiryInterval, uint32>            MessageExpiryIntervalProp;
            typedef TypedProperty<ContentType, DynamicString>               ContentTypeProp;
            typedef TypedProperty<ResponseTopic, DynamicString>             ResponseTopicProp;
            typedef TypedProperty<CorrelationData, DynamicBinaryData>       CorrelationDataProp;
            typedef TypedProperty<SubscriptionID, VBInt>                    SubscriptionIDProp;
            typedef TypedProperty<SessionExpiryInterval, uint32>            SessionExpiryIntervalProp;
            typedef TypedProperty<AssignedClientID, DynamicString>          AssignedClientIDProp;
            typedef TypedProperty<ServerKeepAlive, uint16>                  ServerKeepAliveProp;
            typedef TypedProperty<AuthenticationMethod, DynamicString>      AuthenticationMethodProp;
            typedef TypedProperty<AuthenticationData, DynamicBinaryData>    AuthenticationDataProp;
            typedef TypedProperty<RequestProblemInfo, uint8>                RequestProblemInfoProp;
            typedef TypedProperty<WillDelayInterval, uint32>                WillDelayIntervalProp;
            typedef TypedProperty<RequestResponseInfo, uint8>               RequestResponseInfoProp;
            typedef TypedProperty<ResponseInfo, DynamicString>              ResponseInfoProp;
            typedef TypedProperty<ServerReference, DynamicString>           ServerReferenceProp;
            typedef TypedProperty<ReasonString, DynamicString>              ReasonStringProp;
            typedef TypedProperty<ReceiveMax, uint16>                       ReceiveMaxProp;
            typedef TypedProperty<TopicAliasMax, uint16>                    TopicAliasMaxProp;
            typedef TypedProperty<TopicAlias, uint16>                       TopicAliasProp;
            typedef TypedProperty<QoSMax, uint8>                            QoSMaxProp;
            typedef TypedProperty<RetainAvailable, uint8>                   RetainAvailableProp;
            typedef TypedProperty<UserProperty, DynamicStringPair>          UserPropertyProp;
            typedef TypedProperty<PacketSizeMax, uint32>                    PacketSizeMaxProp;
            typedef TypedProperty<WildcardSubAvailable, uint8>              WildcardSubAvailableProp;
            typedef TypedProperty<SubIDAvailable, uint8>                    SubIDAvailableProp;
            typedef TypedProperty<SharedSubAvailable, uint8>                SharedSubAvailableProp;

            /** This needs to be done once, at least */
            static inline void registerAllProperties()
            {
                static bool registryDoneAlready = false;
                if (!registryDoneAlready)
                {
                    AutoRegisterProperty<PayloadFormatProp>         PayloadFormatProp_reg;
                    AutoRegisterProperty<MessageExpiryIntervalProp> MessageExpiryIntervalProp_reg;
                    AutoRegisterProperty<ContentTypeProp>           ContentTypeProp_reg;
                    AutoRegisterProperty<ResponseTopicProp>         ResponseTopicProp_reg;
                    AutoRegisterProperty<CorrelationDataProp>       CorrelationDataProp_reg;
                    AutoRegisterProperty<SubscriptionIDProp>        SubscriptionIDProp_reg;
                    AutoRegisterProperty<SessionExpiryIntervalProp> SessionExpiryIntervalProp_reg;
                    AutoRegisterProperty<AssignedClientIDProp>      AssignedClientIDProp_reg;
                    AutoRegisterProperty<ServerKeepAliveProp>       ServerKeepAliveProp_reg;
                    AutoRegisterProperty<AuthenticationMethodProp>  AuthenticationMethodProp_reg;
                    AutoRegisterProperty<AuthenticationDataProp>    AuthenticationDataProp_reg;
                    AutoRegisterProperty<RequestProblemInfoProp>    RequestProblemInfoProp_reg;
                    AutoRegisterProperty<WillDelayIntervalProp>     WillDelayIntervalProp_reg;
                    AutoRegisterProperty<RequestResponseInfoProp>   RequestResponseInfoProp_reg;
                    AutoRegisterProperty<ResponseInfoProp>          ResponseInfoProp_reg;
                    AutoRegisterProperty<ServerReferenceProp>       ServerReferenceProp_reg;
                    AutoRegisterProperty<ReasonStringProp>          ReasonStringProp_reg;
                    AutoRegisterProperty<ReceiveMaxProp>            ReceiveMaxProp_reg;
                    AutoRegisterProperty<TopicAliasMaxProp>         TopicAliasMaxProp_reg;
                    AutoRegisterProperty<TopicAliasProp>            TopicAliasProp_reg;
                    AutoRegisterProperty<QoSMaxProp>                QoSMaxProp_reg;
                    AutoRegisterProperty<RetainAvailableProp>       RetainAvailableProp_reg;
                    AutoRegisterProperty<UserPropertyProp>          UserPropertyProp_reg;
                    AutoRegisterProperty<PacketSizeMaxProp>         PacketSizeMaxProp_reg;
                    AutoRegisterProperty<WildcardSubAvailableProp>  WildcardSubAvailableProp_reg;
                    AutoRegisterProperty<SubIDAvailableProp>        SubIDAvailableProp_reg;
                    AutoRegisterProperty<SharedSubAvailableProp>    SharedSubAvailableProp_reg;
                    registryDoneAlready = true;
                }
            }


            /** The allowed properties for each control packet type.
                This is used externally to allow generic code to be written */
            template <PropertyType type> struct ExpectedProperty { enum { AllowedMask = 0 }; };
            template <> struct ExpectedProperty<PayloadFormat>           { enum { AllowedMask = (1<<(uint8)PUBLISH) | 1 }; };
            template <> struct ExpectedProperty<MessageExpiryInterval>   { enum { AllowedMask = (1<<(uint8)PUBLISH) | 1 }; };
            template <> struct ExpectedProperty<ContentType>             { enum { AllowedMask = (1<<(uint8)PUBLISH) | 1 }; };
            template <> struct ExpectedProperty<ResponseTopic>           { enum { AllowedMask = (1<<(uint8)PUBLISH) | 1 }; };
            template <> struct ExpectedProperty<CorrelationData>         { enum { AllowedMask = (1<<(uint8)PUBLISH) | 1 }; };
            template <> struct ExpectedProperty<TopicAlias>              { enum { AllowedMask = (1<<(uint8)PUBLISH) }; };
            template <> struct ExpectedProperty<WillDelayInterval>       { enum { AllowedMask = 1 }; };
            template <> struct ExpectedProperty<SubscriptionID>          { enum { AllowedMask = (1<<(uint8)PUBLISH) | (1<<(uint8)SUBSCRIBE) }; };
            template <> struct ExpectedProperty<SessionExpiryInterval>   { enum { AllowedMask = (1<<(uint8)CONNECT) | (1<<(uint8)CONNACK) | (1<<(uint8)DISCONNECT) }; };
            template <> struct ExpectedProperty<AuthenticationMethod>    { enum { AllowedMask = (1<<(uint8)CONNECT) | (1<<(uint8)CONNACK) | (1<<(uint8)AUTH) }; };
            template <> struct ExpectedProperty<AuthenticationData>      { enum { AllowedMask = (1<<(uint8)CONNECT) | (1<<(uint8)CONNACK) | (1<<(uint8)AUTH) }; };
            template <> struct ExpectedProperty<ReceiveMax>              { enum { AllowedMask = (1<<(uint8)CONNECT) | (1<<(uint8)CONNACK) }; };
            template <> struct ExpectedProperty<TopicAliasMax>           { enum { AllowedMask = (1<<(uint8)CONNECT) | (1<<(uint8)CONNACK) }; };
            template <> struct ExpectedProperty<PacketSizeMax>           { enum { AllowedMask = (1<<(uint8)CONNECT) | (1<<(uint8)CONNACK) }; };
            template <> struct ExpectedProperty<RequestProblemInfo>      { enum { AllowedMask = (1<<(uint8)CONNECT) }; };
            template <> struct ExpectedProperty<RequestResponseInfo>     { enum { AllowedMask = (1<<(uint8)CONNECT) }; };
            template <> struct ExpectedProperty<AssignedClientID>        { enum { AllowedMask = (1<<(uint8)CONNACK) }; };
            template <> struct ExpectedProperty<ServerKeepAlive>         { enum { AllowedMask = (1<<(uint8)CONNACK) }; };
            template <> struct ExpectedProperty<QoSMax>                  { enum { AllowedMask = (1<<(uint8)CONNACK) }; };
            template <> struct ExpectedProperty<RetainAvailable>         { enum { AllowedMask = (1<<(uint8)CONNACK) }; };
            template <> struct ExpectedProperty<WildcardSubAvailable>    { enum { AllowedMask = (1<<(uint8)CONNACK) }; };
            template <> struct ExpectedProperty<SubIDAvailable>          { enum { AllowedMask = (1<<(uint8)CONNACK) }; };
            template <> struct ExpectedProperty<SharedSubAvailable>      { enum { AllowedMask = (1<<(uint8)CONNACK) }; };
            template <> struct ExpectedProperty<ResponseInfo>            { enum { AllowedMask = (1<<(uint8)CONNACK) }; };
            template <> struct ExpectedProperty<ServerReference>         { enum { AllowedMask = (1<<(uint8)CONNACK) | (1<<(uint8)DISCONNECT) }; };
            template <> struct ExpectedProperty<ReasonString>            { enum { AllowedMask = (1<<(uint8)CONNACK) | (1<<(uint8)PUBACK) | (1<<(uint8)PUBREC) | (1<<(uint8)PUBREL) | (1<<(uint8)PUBCOMP) | (1<<(uint8)SUBACK) | (1<<(uint8)UNSUBACK) | (1<<(uint8)DISCONNECT) | (1<<(uint8)AUTH) }; };
            template <> struct ExpectedProperty<UserProperty>            { enum { AllowedMask = 0xFFFF }; };
            /** Check if the given property is allowed in the given control packet type in O(1) */
            static inline bool isAllowedProperty(const PropertyType type, const ControlPacketType ctype)
            {   // This takes 82 bytes of program memory by allowing O(1) in property validity, compared to O(N) search and duplicated code everywhere.
                static uint16 allowedProperties[MaxUsedPropertyType] =
                {
                    ExpectedProperty<(PropertyType) 1>::AllowedMask, ExpectedProperty<(PropertyType) 2>::AllowedMask, ExpectedProperty<(PropertyType) 3>::AllowedMask, ExpectedProperty<(PropertyType) 4>::AllowedMask, ExpectedProperty<(PropertyType) 5>::AllowedMask, ExpectedProperty<(PropertyType) 6>::AllowedMask,
                    ExpectedProperty<(PropertyType) 7>::AllowedMask, ExpectedProperty<(PropertyType) 8>::AllowedMask, ExpectedProperty<(PropertyType) 9>::AllowedMask, ExpectedProperty<(PropertyType)10>::AllowedMask, ExpectedProperty<(PropertyType)11>::AllowedMask, ExpectedProperty<(PropertyType)12>::AllowedMask,
                    ExpectedProperty<(PropertyType)13>::AllowedMask, ExpectedProperty<(PropertyType)14>::AllowedMask, ExpectedProperty<(PropertyType)15>::AllowedMask, ExpectedProperty<(PropertyType)16>::AllowedMask, ExpectedProperty<(PropertyType)17>::AllowedMask, ExpectedProperty<(PropertyType)18>::AllowedMask,
                    ExpectedProperty<(PropertyType)19>::AllowedMask, ExpectedProperty<(PropertyType)20>::AllowedMask, ExpectedProperty<(PropertyType)21>::AllowedMask, ExpectedProperty<(PropertyType)22>::AllowedMask, ExpectedProperty<(PropertyType)23>::AllowedMask, ExpectedProperty<(PropertyType)24>::AllowedMask,
                    ExpectedProperty<(PropertyType)25>::AllowedMask, ExpectedProperty<(PropertyType)26>::AllowedMask, ExpectedProperty<(PropertyType)27>::AllowedMask, ExpectedProperty<(PropertyType)28>::AllowedMask, ExpectedProperty<(PropertyType)29>::AllowedMask, ExpectedProperty<(PropertyType)30>::AllowedMask,
                    ExpectedProperty<(PropertyType)31>::AllowedMask, ExpectedProperty<(PropertyType)32>::AllowedMask, ExpectedProperty<(PropertyType)33>::AllowedMask, ExpectedProperty<(PropertyType)34>::AllowedMask, ExpectedProperty<(PropertyType)35>::AllowedMask, ExpectedProperty<(PropertyType)36>::AllowedMask,
                    ExpectedProperty<(PropertyType)37>::AllowedMask, ExpectedProperty<(PropertyType)38>::AllowedMask, ExpectedProperty<(PropertyType)39>::AllowedMask, ExpectedProperty<(PropertyType)40>::AllowedMask, ExpectedProperty<(PropertyType)41>::AllowedMask, ExpectedProperty<(PropertyType)42>::AllowedMask,
                };
                if (!type || type >= MaxUsedPropertyType) return 0;
                return (allowedProperties[(int)type - 1] & (1<<(uint8)ctype)) > 0;
            }

            /** Additional method required for properties */
            struct SerializableProperties : public Serializable
            {
#if MQTTAvoidValidation != 1
                virtual bool checkPropertiesFor(const ControlPacketType type) const = 0;
#endif
            };

            /** The property structure (section 2.2.2).
                This object is trying to avoid memory allocation to limit its impact.
                Such feature is implemented by a "reference mechanism".
                Whenever an instance is build by "copying" or "capture", a reference to the original object
                is captured and no copy is really made. If you need a copy, use the clone method.

                The reference is done with specific double-head tracking for it.
                When a reference is taken, both head and reference are set to the reference source.
                If modifications are done on this instance, only the head is modified.
                When destructed, destruction happens until the reference */
            struct Properties Final : public SerializableProperties
            {
                /** The properties length (can be 0) (this only counts the following members) */
                VBInt length;
                /** The properties set */
                PropertyBase * head;
                /** Whether it's just a reference to another property (so skip destruction of the chained list) */
                PropertyBase * reference;

                /** Destroy correctly this instance */
                void suicide()
                {
                    while (head && head != reference)
                    {
                        PropertyBase * n = head->next;
                        head->suicide();
                        head = n;
                    }
                    reference = 0;
                }

                /** Get the i-th property */
                const PropertyBase * getProperty(size_t index) const { const PropertyBase * u = head; while (u && index--) u = u->next; return !index ? u : 0; }
                /** Get the i-th property of the given type */
                const PropertyBase * getProperty(const PropertyType type, size_t index = 0) const
                {
                    PropertyBase * u = head;
                    while (u)
                    {
                        if (u->type == type)
                        {
                           if (index-- == 0) return u;
                        }
                        u = u->next;
                    }
                    return 0;
                }
                /** Fetch the i-th property with the given visitor.
                    @note This is inefficient compared to other getProperty methods, since it's performing a O(N) search each call.
                    @param visitor  The visitor will be mutated on the next property to view
                    @return true if there's another property to visit */
                bool getProperty(VisitorVariant & visitor) const
                {
                    visitor.propertyType(BadProperty);
                    const uint32 offset = visitor.getOffset();
                    const PropertyBase * u = getProperty((size_t)offset);
                    if (!u) return false;
                    // Then copy or take a view on the property value
                    if (!u->acceptVisitor(visitor)) return false;
                    visitor.setOffset(offset + 1);
                    return true;
                }

                /** This give the size required for serializing this property header in bytes */
                uint32 getSize() const { return length.getSize() + (uint32)length; }
                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's getSize() long.
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const
                {
                    uint32 o = length.copyInto(buffer);
                    PropertyBase * c = head;
                    while (c) { o += c->copyInto(buffer + o); c = c->next; }
                    return o;
                }
#if MQTTClientOnlyImplementation != 1
                /** Read the value from a buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long
                    @return The number of bytes read from the buffer, or 0xFF upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    uint32 o = length.readFrom(buffer, bufLength);
                    if (isError(o)) return o;
                    if ((uint32)length > bufLength - length.getSize()) return NotEnoughData;
                    suicide();
                    buffer += o; bufLength -= o;
                    PropertyBase * property = 0;
                    uint32 cumSize = (uint32)length;
                    while (cumSize)
                    {
                        uint32 s = PropertyRegistry::getInstance().unserialize(buffer, cumSize, property);
                        if (isError(s)) return s;
                        if (head) property->next = head;
                        head = property;
                        buffer += s; cumSize -= s;
                        o += s;
                    }
                    return o;
                }
#endif
#if MQTTAvoidValidation != 1
                /** Check if this property is valid */
                bool check() const { return length.check() && head ? head->check() : true; }
#endif
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    out += MQTTStringPrintf("%*sProperties with length ", (int)indent, ""); length.dump(out, 0);
                    if (!(uint32)length) return;
                    PropertyBase * c = head;
                    while (c) {
                        c->dump(out, indent + 2);
                        c = c->next;
                    }
                }
#endif
#if MQTTAvoidValidation != 1
                /** Check if the properties are compatible for the given packet type */
                bool checkPropertiesFor(const ControlPacketType type) const
                {
                    if (!check()) return false;
                    PropertyBase * u = head;
                    while (u) { if (!isAllowedProperty((PropertyType)u->type, type)) return false; u = u->next; }
                    return true;
                }
#endif
                /** Append a property to this list
                    @param property     A pointer to a new allocated property to append to this list that's owned
                    @return true upon successful append, false upon error (the pointer is not owned in that case) */
                bool append(PropertyBase * property)
                {
                    // This does not update an existing property
                    if (property->type != UserProperty && getProperty(property->type))
                        return false;

                    VBInt l((uint32)length + property->getSize());
                    if (!l.checkImpl()) return false;
                    length = l;
                    property->next = head;
                    head = property;
                    return true;
                }
                /** Capture a given property. This only refers to the given property */
                void capture(Properties * other)
                {
                    if (!other) return;
                    head = other->head;
                    length = other->length;
                    reference = head;
                }

                /** Make a deep copy. This actually create a version that's heap allocated for each value */
                Properties * clone()
                {
                    Properties * ret = new Properties();
                    ret->length = length;
                    const PropertyBase * n = head;
                    PropertyBase * & m = ret->head;
                    while (n)
                    {
                        m = n->clone();
                        m = m->next;
                        n = n->next;
                    }
                    return ret;
                }

                /** Build an empty property list */
                Properties() : head(0), reference(0) {}
                /** Copy construction does not really create a copy, but take a reference on the existing object */
                Properties(const Properties & other) : length(other.length), head(other.head), reference(other.head) {}
#if HasCPlusPlus11 == 1
                /** Move constructor (to be preferred) */
                Properties(Properties && other) : length(std::move(other.length)), head(std::move(other.head)), reference(std::move(other.reference)) {}
#endif
                /** Build a property list starting with the given property that's owned.
                    @param firstProperty    A pointer on a new allocated Property that's owned by this list */
                Properties(PropertyBase * firstProperty)
                    : length(firstProperty->getSize()), head(firstProperty), reference(0) {}
                ~Properties() { suicide(); }
            };


            /** A read-only view off property extracted from a packet (section 2.2.2).
                Unlike the Properties class above that's able to add and parse properties, this
                one only parse properties but never allocate anything on the heap.

                The idea here is to parse properties on the fly, one by one and let the client code
                perform fetching the information it wants from them.

                Typically, you'll use this class like this:
                @code
                    PropertiesView v;
                    uint32 r = v.readFrom(buffer, bufLength);
                    if (isError(r)) return BadData;

                    VisitorVariant visitor;
                    while (v.getProperty(visitor))
                    {
                        if (visitor.propertyType() == DesiredProperty)
                        {
                            DynamicStringView * view = visitor.as< DynamicStringView >();
                            // Do something with view
                        }
                        else if (visitor.propertyType() == SomeOtherProperty)
                        {
                            auto pod = visitor.as< PODVisitor<uint8> >();
                            uint8 value = pod->getValue(); // Do something with value
                        }
                    }
                @endcode */
            struct PropertiesView Final : public SerializableProperties
            {
                /** The properties length (can be 0) (this only counts the following members) */
                VBInt length;
                /** The given input buffer */
                const uint8 * buffer;

                /** Fetch the i-th property with the given visitor
                    @param visitor  The visitor will be mutated on the next property to view
                    @return true if there's another property to visit */
                bool getProperty(VisitorVariant & visitor) const
                {
                    visitor.propertyType(BadProperty);
                    const uint32 offset = visitor.getOffset();
                    if (offset >= (uint32)length || !buffer) return false;
                    // Deduce property type from the given byte
                    uint8 t = buffer[offset];
                    if (!MemMappedPropertyRegistry::getInstance().getVisitorForProperty(visitor, t)) return 0;
                    // Then visit the property now
                    uint32 r = visitor.acceptBuffer(&buffer[offset + 1], (uint32)length - offset - 1);
                    if (isError(r)) return false;
                    visitor.setOffset(offset + r + 1);
                    return true;
                }
                /** This give the size required for serializing this property header in bytes */
                uint32 getSize() const { return length.getSize() + (uint32)length; }
                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's getSize() long.
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * _buffer) const
                {
                    uint32 o = length.copyInto(_buffer);
                    memcpy(_buffer + o, buffer, (uint32)length);
                    return o + (uint32)length;
                }
                /** Read the value from a buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long
                    @return The number of bytes read from the buffer, or 0xFF upon error */
                uint32 readFrom(const uint8 * _buffer, uint32 bufLength)
                {
                    uint32 o = length.readFrom(_buffer, bufLength);
                    if (isError(o)) return o;
                    if ((uint32)length > bufLength - length.getSize()) return NotEnoughData;
                    buffer = _buffer + o;
                    return o + (uint32)length;
                }
#if MQTTAvoidValidation != 1
                /** Check if this property is valid */
                bool check() const { return length.check(); }
#endif
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    out += MQTTStringPrintf("%*sProperties with length ", (int)indent, ""); length.dump(out, 0);
                    if (!(uint32)length) return;
                    VisitorVariant visitor;
                    while (getProperty(visitor))
                    {
                        out += MQTTStringPrintf("%*sType %s\n", indent+2, "", PrivateRegistry::getPropertyName(visitor.propertyType()));
                        visitor.dump(out, indent + 4);
                    }
                }
#endif
#if MQTTAvoidValidation != 1
                /** Check if the properties are compatible for the given packet type */
                bool checkPropertiesFor(const ControlPacketType type) const
                {
                    if (!check()) return false;
                    VisitorVariant v;
                    while (getProperty(v))
                    {
                        if (!isAllowedProperty(v.propertyType(), type)) return false;
                    }
                    return true;
                }
#endif

                /** Build an empty property list */
                PropertiesView() : buffer(0) {}
                /** Copy construction */
                PropertiesView(const PropertiesView & other) : length(other.length), buffer(other.buffer)
                {}
#if HasCPlusPlus11 == 1
                /** Move constructor (to be preferred) */
                PropertiesView(PropertiesView && other) : length(std::move(other.length)), buffer(std::move(other.buffer)) {}
#endif
            };

            /** The possible value for retain handling in subscribe packet */
            enum RetainHandling
            {
                GetRetainedMessageAtSubscriptionTime        = 0,    //!< Get the retained message at subscription time
                GetRetainedMessageForNewSubscriptionOnly    = 1,    //!< Get the retained message only for new subscription
                NoRetainedMessage                           = 2,    //!< Don't get retained message at all
            };

            /** The possible Quality Of Service values */
            enum QualityOfServiceDelivery
            {
                AtMostOne                           = 0,    //!< At most one delivery (unsecure sending)
                AtLeastOne                          = 1,    //!< At least one delivery (could have retransmission)
                ExactlyOne                          = 2,    //!< Exactly one delivery (longer to send)
            };

            struct ScribeTopicBase : public Serializable
            {
            protected:
                /** The scribe topic */
                DynString           topic;
                /** The next pointer in the list */
                ScribeTopicBase *   next;
                /** Check if it's stack allocated or not */
                bool                stackBased;

            public:
#if MQTTAvoidValidation != 1
                /** Check if this property is valid */
                bool check() const { return topic.check() && (next ? next->check() : true); }
#endif
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    topic.dump(out, indent);
                    if (next) next->dump(out, indent);
                }
#endif


            public:
                /** Main suicide function */
                void suicide() { if (next) next->suicide(); if (!stackBased) delete this; }
                /** Append a subscribe topic to the end of this list */
                void append(ScribeTopicBase * newTopic) { ScribeTopicBase ** end = &next; while(*end) { end = &(*end)->next; } *end = newTopic; }
                /** Count the number of topic */
                uint32 count() const { uint32 c = 1; const ScribeTopicBase * p = next; while (p) { c++; p = p->next; } return c; }


            public:
                ScribeTopicBase(const bool stackBased) : next(0), stackBased(stackBased) {}
                ScribeTopicBase(const DynString & topic, const bool stackBased) : topic(topic), next(0), stackBased(stackBased) {}
                virtual ~ScribeTopicBase() { next = 0; }
            };

#pragma pack(push, 1)
            /** The subscribe topic list */
            struct SubscribeTopic Final : public ScribeTopicBase
            {
                union
                {
                    /** The subscribe option */
                    uint8 option;
                    /** Reserved bits, should be 0 */
                    BitField<uint8, 6, 2> reserved;
                    /** The retain policy flag */
                    BitField<uint8, 4, 2> retainHandling;
                    /** The retain as published flag */
                    BitField<uint8, 3, 1> retainAsPublished;
                    /** The non local flag */
                    BitField<uint8, 2, 1> nonLocal;
                    /** The QoS flag */
                    BitField<uint8, 0, 2> QoS;
                };
                /** This give the size required for serializing this property header in bytes */
                uint32 getSize() const { return topic.getSize() + 1 + (next ? next->getSize() : 0); }
                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's getSize() long.
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const
                {
                    uint32 o = topic.copyInto(buffer);
                    buffer += o;
                    buffer[0] = option; o++;
                    if (next) o += next->copyInto(buffer+1);
                    return o;
                }
#if MQTTClientOnlyImplementation != 1
                /** Read the value from a buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long
                    @return The number of bytes read from the buffer, or 0xFF upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if (next) next->suicide();
                    next = 0;
                    uint32 o = 0, s = topic.readFrom(buffer, bufLength);
                    if (isError(s)) return s;
                    buffer += s; bufLength -= s; o += s;
                    if (!bufLength) return NotEnoughData;
                    option = buffer[0]; buffer++; bufLength--; o++;
                    if (bufLength)
                    {
                        next = new SubscribeTopic();
                        s = next->readFrom(buffer, bufLength);
                        if (isError(s)) return s;
                        o += s;
                    }
                    return o;
                }
#endif
#if MQTTAvoidValidation != 1
                /** Check if this property is valid */
                bool check() const { return reserved == 0 && retainAsPublished != 3 && QoS != 3 && ScribeTopicBase::check(); }
#endif
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    out += MQTTStringPrintf("%*sSubscribe (QoS %d, nonLocal %d, retainAsPublished %d, retainHandling %d): ", (int)indent, "", (uint8)QoS, (uint8)nonLocal, (uint8)retainAsPublished, (uint8)retainHandling);
                    ScribeTopicBase::dump(out, indent);
                }
#endif

                /** Append a subscribe topic to the end of this list */
                inline void append(SubscribeTopic * newTopic) { ScribeTopicBase::append(newTopic); }

                /** Default constructor */
                SubscribeTopic(const bool stackBased = false) : ScribeTopicBase(stackBased), option(0) {}
                /** Full constructor */
                SubscribeTopic(const DynString & topic, const uint8 retainHandling, const bool retainAsPublished, const bool nonLocal, const uint8 QoS, const bool stackBased = false)
                    : ScribeTopicBase(topic, stackBased), option(0) { this->retainHandling = retainHandling; this->retainAsPublished = retainAsPublished ? 1 : 0; this->nonLocal = nonLocal ? 1:0; this->QoS = QoS; }
            };
#pragma pack(pop)
            /** The unsubscribe topic list */
            struct UnsubscribeTopic : public ScribeTopicBase
            {
                /** This give the size required for serializing this property header in bytes */
                uint32 getSize() const { return topic.getSize() + (next ? next->getSize() : 0); }
                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's getSize() long.
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const
                {
                    uint32 o = topic.copyInto(buffer);
                    buffer += o;
                    if (next) o += next->copyInto(buffer);
                    return o;
                }
#if MQTTClientOnlyImplementation != 1
                /** Read the value from a buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long
                    @return The number of bytes read from the buffer, or 0xFF upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if (next) next->suicide();
                    next = 0;
                    uint32 o = 0, s = topic.readFrom(buffer, bufLength);
                    if (isError(s)) return s;
                    buffer += s; bufLength -= s; o += s;
                    if (bufLength)
                    {
                        next = new UnsubscribeTopic();
                        s = next->readFrom(buffer, bufLength);
                        if (isError(s)) return s;
                        o += s;
                    }
                    return o;
                }
#endif
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    out += MQTTStringPrintf("%*sUnsubscribe: ", (int)indent, ""); topic.dump(out, indent);
                    ScribeTopicBase::dump(out, indent);
                }
#endif

                /** Append a subscribe topic to the end of this list */
                inline void append(UnsubscribeTopic * newTopic) { ScribeTopicBase::append(newTopic); }

                /** Default constructor */
                UnsubscribeTopic(const bool stackBased = false) : ScribeTopicBase(stackBased) {}
                /** Full constructor */
                UnsubscribeTopic(const DynString & topic, const bool stackBased = false)
                    : ScribeTopicBase(topic, stackBased) {}
            };

            /** The variable header presence for each possible packet type, the payload presence in each packet type */
            template <ControlPacketType type>
            struct ControlPacketMeta
            {
                /** The fixed header to expect */
                typedef FixedHeaderType<type, 0> FixedHeader;
                /** The variable header to use */
                typedef Properties               VariableHeader;
                /** The payload data if any expected */
                static const bool hasPayload = false;
            };

            /** The default fixed field before the variable header's properties */
            template <ControlPacketType type>
            struct FixedField : public Serializable
            {
                /** For action from the packet header to the behaviour */
                inline void setFlags(const uint8 &) {}
                /** Some packets are using shortcut length, so we need to know about this */
                inline void setRemainingLength(const uint32) {}
            };

            struct FixedFieldGeneric;
            /** The payload for some packet types. By default, it's empty. */
            struct SerializablePayload : public EmptySerializable
            {
                /** Set the flags marked in the fixed field header */
                virtual void setFlags(const FixedFieldGeneric &) {}
                /** Set the expected packet size (this is useful for packet whose payload is application defined) */
                virtual void setExpectedPacketSize(uint32) {}

                ~SerializablePayload() {}
            };

            template <ControlPacketType type>
            struct Payload : public SerializablePayload
            {
            };

            /** Declare all the expected control packet type and format */
            template <> struct ControlPacketMeta<CONNECT>       { typedef ConnectHeader          FixedHeader; typedef Properties        VariableHeader; static const bool hasPayload = true;  };
            template <> struct ControlPacketMeta<CONNACK>       { typedef ConnectACKHeader       FixedHeader; typedef Properties        VariableHeader; static const bool hasPayload = false; };
            template <> struct ControlPacketMeta<PUBLISH>       { typedef PublishHeader          FixedHeader; typedef Properties        VariableHeader; static const bool hasPayload = true;  }; // The packet identifier being dependend upon flags, it's supported in the fixed header type
            template <> struct ControlPacketMeta<PUBACK>        { typedef PublishACKHeader       FixedHeader; typedef Properties        VariableHeader; static const bool hasPayload = false; }; // The packet ID is in the header
            template <> struct ControlPacketMeta<PUBREC>        { typedef PublishReceivedHeader  FixedHeader; typedef Properties        VariableHeader; static const bool hasPayload = false; }; // Same
            template <> struct ControlPacketMeta<PUBREL>        { typedef PublishReleasedHeader  FixedHeader; typedef Properties        VariableHeader; static const bool hasPayload = false; }; // Same
            template <> struct ControlPacketMeta<PUBCOMP>       { typedef PublishCompletedHeader FixedHeader; typedef Properties        VariableHeader; static const bool hasPayload = false; }; // Same
            template <> struct ControlPacketMeta<SUBSCRIBE>     { typedef SubscribeHeader        FixedHeader; typedef Properties        VariableHeader; static const bool hasPayload = true;  };
            template <> struct ControlPacketMeta<SUBACK>        { typedef SubscribeACKHeader     FixedHeader; typedef Properties        VariableHeader; static const bool hasPayload = true;  };
            template <> struct ControlPacketMeta<UNSUBSCRIBE>   { typedef UnsubscribeHeader      FixedHeader; typedef Properties        VariableHeader; static const bool hasPayload = true;  };
            template <> struct ControlPacketMeta<UNSUBACK>      { typedef UnsubscribeACKHeader   FixedHeader; typedef Properties        VariableHeader; static const bool hasPayload = true;  };
            template <> struct ControlPacketMeta<PINGREQ>       { typedef PingRequestHeader      FixedHeader; typedef EmptySerializable VariableHeader; static const bool hasPayload = false; };
            template <> struct ControlPacketMeta<PINGRESP>      { typedef PingACKHeader          FixedHeader; typedef EmptySerializable VariableHeader; static const bool hasPayload = false; };
            template <> struct ControlPacketMeta<DISCONNECT>    { typedef DisconnectHeader       FixedHeader; typedef Properties        VariableHeader; static const bool hasPayload = false; };
            template <> struct ControlPacketMeta<AUTH>          { typedef AuthenticationHeader   FixedHeader; typedef Properties        VariableHeader; static const bool hasPayload = false; };


#pragma pack(push, 1)
            struct ConnectHeaderImpl
            {
                /** The protocol name: "\0\4MQTT" */
                uint8 protocolName[6];
                /** The protocol version */
                uint8 protocolVersion;
                // The connect flags
                union
                {
                    /** This is used to avoid setting all flags by hand */
                    uint8 flags;

                    /** The user name flag */
                    BitField<uint8, 7, 1> usernameFlag;
                    /** The password flag */
                    BitField<uint8, 6, 1> passwordFlag;
                    /** The will retain flag */
                    BitField<uint8, 5, 1> willRetain;
                    /** The will QoS flag */
                    BitField<uint8, 3, 2> willQoS;
                    /** The will flag */
                    BitField<uint8, 2, 1> willFlag;
                    /** The clean start session */
                    BitField<uint8, 1, 1> cleanStart;
                    /** Reserved bit, should be 0 */
                    BitField<uint8, 0, 1> reserved0;
                };
                /** The keep alive time in seconds */
                mutable uint16 keepAlive;
#if MQTTAvoidValidation != 1
                bool check() const { return reserved0 == 0 && willQoS < 3 && memcmp(protocolName, expectedProtocolName(), sizeof(protocolName)) == 0; }
#endif
                /** Get the expected protocol name */
                static const uint8 * expectedProtocolName() { static uint8 protocolName[6] = { 0, 4, 'M', 'Q', 'T', 'T' }; return protocolName; }
                /** The default constructor */
                ConnectHeaderImpl() : protocolVersion(5), flags(0), keepAlive(0) { memcpy(protocolName, expectedProtocolName(), sizeof(protocolName));  }
            };
#pragma pack(pop)


            struct FixedFieldGeneric : public Serializable
            {
                GenericTypeBase & value;

                uint32 getSize() const { return value.typeSize(); }
                uint32 copyInto(uint8 * buffer) const
                {
                    value.swapNetwork();
                    memcpy(buffer, value.raw(), getSize());
                    value.swapNetwork();
                    return value.typeSize();
                }
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if (bufLength < value.typeSize()) return NotEnoughData;
                    memcpy(value.raw(), buffer, value.typeSize());
                    value.swapNetwork();
                    return value.typeSize();
                }
#if MQTTAvoidValidation != 1
                /** Check if this header is correct */
                bool check() const { return value.check(); }
#endif
                /** No action from the packet header to the behaviour here */
                virtual void setFlags(const uint8 &) {}
                /** Some packets are using shortcut length, so we need to know about this */
                virtual void setRemainingLength(const uint32 length) {}

                FixedFieldGeneric(GenericTypeBase & v) : value(v) {}
            };

            template<>
            struct GenericType<ConnectHeaderImpl> Final : public GenericTypeBase
            {
                ConnectHeaderImpl & value;
                uint32 typeSize() const { return sizeof(value); }
                void swapNetwork() const { const_cast<ConnectHeaderImpl&>(value).keepAlive = BigEndian(value.keepAlive); }
                void * raw() { return &value; }
                operator ConnectHeaderImpl& () { return value; }
                GenericType<ConnectHeaderImpl> & operator = (const ConnectHeaderImpl & o) { value = o; return *this; }
#if MQTTAvoidValidation != 1
                bool check() const { return value.check(); }
#endif
                GenericType(ConnectHeaderImpl & v) : value(v) {}
            };

            /** The fixed field for CONNECT packet */
            template <> class FixedField<CONNECT> Final : public ConnectHeaderImpl, public FixedFieldGeneric
            {
                GenericType<ConnectHeaderImpl> _v;
            public:
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    out += MQTTStringPrintf("%*sCONNECT packet (clean %d, will %d, willQoS %d, willRetain %d, password %d, username %d, keepAlive: %d)\n", (int)indent, "", (uint8)cleanStart, (uint8)willFlag, (uint8)willQoS, (uint8)willRetain, (uint8)passwordFlag, (uint8)usernameFlag, keepAlive);
                }
#endif
                /** The default constructor */
                FixedField() : FixedFieldGeneric(_v), _v(*this) {}
            };

#pragma pack(push, 1)
            struct ConnACKHeaderImpl
            {
                /** The acknowledge flag */
                uint8 acknowledgeFlag;
                /** The connect reason */
                uint8 reasonCode;

#if MQTTAvoidValidation != 1
                bool check() const { return (acknowledgeFlag & 0xFE) == 0; }
#endif

                ConnACKHeaderImpl() : acknowledgeFlag(0), reasonCode(0) {}
            };
#pragma pack(pop)

            template<> struct GenericType<ConnACKHeaderImpl> Final : public GenericTypeBase
            {
                ConnACKHeaderImpl & value;
                uint32 typeSize() const { return sizeof(value); }
                void swapNetwork() const {  }
                void * raw() { return &value; }
                GenericType<ConnACKHeaderImpl> & operator = (const ConnACKHeaderImpl & o) { value = o; return *this; }
                operator ConnACKHeaderImpl& () { return value; }
                GenericType(ConnACKHeaderImpl & v) : value(v) {}
            };



            /** The fixed field for the CONNACK packet */
            template <> class FixedField<CONNACK> Final : public ConnACKHeaderImpl, public FixedFieldGeneric
            {
                GenericType<ConnACKHeaderImpl> _v;
            public:
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0) { out += MQTTStringPrintf("%*sCONNACK packet (ack %u, reason %u)\n", (int)indent, "", acknowledgeFlag, reasonCode); }
#endif
                /** The default constructor */
                FixedField() : FixedFieldGeneric(_v), _v(*this) {}
            };

            /** Some packets, like DISCONNECT, CONNACK, ... can be shorter and in that case, return a Shortcut return value */
            struct FixedFieldWithRemainingLength : public FixedFieldGeneric
            {
                /** Some packets are using shortcut length, so we need to know about this */
                void setRemainingLength(const uint32 length) { remLength = length; }
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    uint32 r = FixedFieldGeneric::readFrom(buffer, bufLength);
                    if (!isError(r) && remLength == value.typeSize()) return Shortcut;
                    return r;
                }

                FixedFieldWithRemainingLength(GenericTypeBase & v, uint32 remLength) : FixedFieldGeneric(v), remLength(remLength) {}
            protected:
                uint32 remLength;
            };

            /** The fixed field for the packet with a packet ID */
            struct FixedFieldWithID : public FixedFieldGeneric
            {
                /** The packet identifier */
                GenericType<uint16> packetID;

#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0) { out += MQTTStringPrintf("%*sControl packet (id 0x%04X)\n", (int)indent, "", (uint16)packetID); }
#endif
                /** The default constructor */
                FixedFieldWithID() : FixedFieldGeneric(packetID) { }
            };

            struct FixedFieldWithReason : public FixedFieldWithRemainingLength
            {
                /** The connect reason */
                GenericType<uint8> reasonCode;

                /** Allow conversion to reason code here directly */
                ReasonCodes reason() const { return (ReasonCodes)reasonCode.value; }

#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0) { out += MQTTStringPrintf("%*Control packet (reason %u)\n", (int)indent, "", (uint8)reasonCode); }
#endif
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if (remLength == 0) { reasonCode.value = 0; return Shortcut; }
                    return FixedFieldGeneric::readFrom(buffer, bufLength);
                }
                /** The default constructor */
                FixedFieldWithReason() : FixedFieldWithRemainingLength(reasonCode, 1) { }
            };

#pragma pack(push, 1)
            struct IDAndReason
            {
                /** The packet identifier */
                uint16 packetID;
                /** The connect reason */
                uint8 reasonCode;

                IDAndReason() : packetID(0), reasonCode(0) {}
            };
#pragma pack(pop)

            template<> struct GenericType<IDAndReason> Final : public GenericTypeBase
            {
                IDAndReason & value;
                uint32 typeSize() const { return sizeof(value); }
                void swapNetwork() const { const_cast<uint16&>(value.packetID) = BigEndian(value.packetID); }
                void * raw() { return &value; }
                GenericType<IDAndReason> & operator = (const IDAndReason & o) { value = o; return *this; }
                operator IDAndReason& () { return value; }

                GenericType(IDAndReason & v) : value(v) {}
            };


            /** The fixed field for the publish acknowledges packets */
            class FixedFieldWithIDAndReason : public IDAndReason, public FixedFieldWithRemainingLength
            {
                GenericType<IDAndReason> _v;
            public:

                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if (bufLength < 2) return NotEnoughData;
                    memcpy(&packetID, buffer, sizeof(packetID)); packetID = BigEndian(packetID);
                    if (remLength == 2) return Shortcut;
                    _v.value.reasonCode = buffer[2];
                    if (remLength == 3) return Shortcut; // No need to read properties here
                    return 3;
                }

#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0) { out += MQTTStringPrintf("%*sControl packet (id 0x%04X, reason %u)\n", (int)indent, "", packetID, reasonCode); }
#endif

                /** The default constructor */
                FixedFieldWithIDAndReason() : FixedFieldWithRemainingLength(_v, 3), _v(*this) { }
            };

            /** The SUBSCRIBE header is a generic FixedField header with a packet id */
            template <> struct FixedField<SUBSCRIBE>    Final: public FixedFieldWithID {};
            /** The SUBACK header is a generic FixedField header with a packet id */
            template <> struct FixedField<SUBACK>       Final: public FixedFieldWithID {};
            /** The UNSUBSCRIBE header is a generic FixedField header with a packet id */
            template <> struct FixedField<UNSUBSCRIBE>  Final: public FixedFieldWithID {};
            /** The UNSUBACK header is a generic FixedField header with a packet id */
            template <> struct FixedField<UNSUBACK>     Final: public FixedFieldWithID {};

            /** The PUBACK header is a generic FixedField header with a reason code */
            template <> struct FixedField<PUBACK>       Final: public FixedFieldWithIDAndReason {};
            /** The PUBREC header is a generic FixedField header with a reason code */
            template <> struct FixedField<PUBREC>       Final: public FixedFieldWithIDAndReason {};
            /** The PUBREL header is a generic FixedField header with a reason code */
            template <> struct FixedField<PUBREL>       Final: public FixedFieldWithIDAndReason {};
            /** The PUBCOMP header is a generic FixedField header with a reason code */
            template <> struct FixedField<PUBCOMP>      Final: public FixedFieldWithIDAndReason {};

            /** The fixed field for the DISCONNECT packet which supports shortcut */
            template <> struct FixedField<DISCONNECT>   Final: public FixedFieldWithReason
            {
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    uint32 r = FixedFieldWithReason::readFrom(buffer, bufLength);
                    if (!isError(r) && remLength == 1) return Shortcut;
                    return r;
                }
            };
            /** The fixed field for the AUTH packet which supports shortcut */
            template <> struct FixedField<AUTH>         Final: public FixedFieldWithReason {};

#pragma pack(push, 1)
            struct TopicAndID
            {
                /** The topic name */
                DynString topicName;
                /** The packet identifier */
                uint16 packetID;
            };
#pragma pack(pop)

            template<> struct GenericType<TopicAndID> Final : public GenericTypeBase
            {
                TopicAndID & value;
                uint32 typeSize() const { return value.topicName.getSize(); }
                void swapNetwork() const { const_cast<uint16&>(value.packetID) = BigEndian(value.packetID); }
#if MQTTAvoidValidation != 1
                bool check() const { return value.topicName.check(); }
#endif
                void * raw() { return &value; }
                GenericType<TopicAndID> & operator = (const TopicAndID & o) { value = o; return *this; }
                operator TopicAndID& () { return value; }

                GenericType(TopicAndID & v) : value(v) {}
            };

            /** The fixed field for the PUBLISH packet.
                In fact, it's not fixed at all, so we simply implement a serializable interface here */
            template <> class FixedField<PUBLISH> Final: public TopicAndID, public FixedFieldGeneric
            {
                GenericType<TopicAndID> _v;
            public:
                /** This give the size required for serializing this property header in bytes */
                uint32 getSize() const { return topicName.getSize() + (hasPacketID() ? 2 : 0); }
                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's getSize() long.
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const
                {
                    uint32 o = topicName.copyInto(buffer);
                    if (hasPacketID())
                    {
                        uint16 p = BigEndian(packetID);
                        memcpy(buffer + o, &p, sizeof(p)); o += sizeof(p);
                    }
                    return o;
                }
                /** Read the value from a buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long
                    @return The number of bytes read from the buffer, or 0xFF upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    uint32 o = topicName.readFrom(buffer, bufLength);
                    if (isError(o)) return o;
                    buffer += o; bufLength -= o;
                    if (hasPacketID())
                    {
                        if (bufLength < sizeof(packetID)) return NotEnoughData;
                        memcpy(&packetID, buffer, sizeof(packetID)); packetID = BigEndian(packetID);
                        o += sizeof(packetID);
                    }
                    return o;
                }
                /** No action from the packet header to the behaviour here */
                inline void setFlags(const uint8 & u) { flags = &u; }
                /** Check if the flags says there is a packet identifier */
                inline bool hasPacketID() const { return flags && (*flags & 6) > 0; }
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0) { out += MQTTStringPrintf("%*sPUBLISH packet (id 0x%04X): ", (int)indent, "", packetID); topicName.dump(out, 0); }
#endif

                /** The default constructor */
                FixedField() : FixedFieldGeneric(_v), _v(*this), flags(0) { }

            private:
                /** The main header flags */
                const uint8 * flags;
            };



            // Payloads
            ///////////////////////////////////////////////////////////////////////////////////////

            /** Helper structure used to store a will message.
                Please notice that only a client code will use this, so the properties are the full blown properties.
                They allocate on heap to store the list of properties */
            struct WillMessage Final : public Serializable
            {
                /** That's the will properties to attachs to the will message if required */
                Properties          willProperties;
                /** The will topic */
                DynString           willTopic;
                /** The last will application message payload */
                DynBinData          willPayload;

                /** We have a getSize() method that gives the number of bytes requires to serialize this object */
                virtual uint32 getSize() const { return willProperties.getSize() + willTopic.getSize() + willPayload.getSize(); }

                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's at least getSize() bytes long
                    @return The number of bytes used in the buffer */
                virtual uint32 copyInto(uint8 * buffer) const
                {
                    uint32 o = willProperties.copyInto(buffer);
                    o += willTopic.copyInto(buffer+o);
                    o += willPayload.copyInto(buffer+o);
                    return o;
                }
#if MQTTClientOnlyImplementation != 1
                /** Read the value from a buffer.
                    @param buffer       A pointer to an allocated buffer
                    @param bufLength    The length of the buffer in bytes
                    @return The number of bytes read from the buffer, or a LocalError upon error (use isError() to test for it) */
                virtual uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    uint32 s = willProperties.readFrom(buffer, bufLength), o = 0;
                    if (isError(s)) return s;
                    o += s; buffer += s; bufLength -= s;
                    s = willTopic.readFrom(buffer, bufLength);
                    if (isError(s)) return s;
                    o += s; buffer += s; bufLength -= s;
                    s = willPayload.readFrom(buffer, bufLength);
                    if (isError(s)) return s;
                    o += s;
                    return o;
                }
#endif

#if MQTTAvoidValidation != 1
                /** Check the will properties validity */
                bool check() const
                {
                    if (!willProperties.check()) return false;
                    PropertyBase * u = willProperties.head;
                    while (u)
                    {   // Will properties are noted as if their control packet type was 0 (since it's reserved, let's use it)
                        if (!isAllowedProperty((PropertyType)u->type, (ControlPacketType)0)) return false;
                        u = u->next;
                    }
                    return willTopic.check() && willPayload.check();
                }
#endif

#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0) { out += MQTTStringPrintf("%*sWill message\n", (int)indent, ""); willProperties.dump(out, indent + 2); willTopic.dump(out, indent + 2); willPayload.dump(out, indent + 2); }
#endif


                /** Default construction */
                WillMessage() {}
#if HasCPlusPlus11 == 1
                /** Construction for this message */
                WillMessage(DynString && topic, DynBinData && payload, Properties && properties) :
                    willProperties(properties), willTopic(topic), willPayload(payload) {}
#endif

                /** Copy construction */
                WillMessage(const DynString & topic, const DynBinData & payload, const Properties & properties = Properties()) :
                    willProperties(properties), willTopic(topic), willPayload(payload) {}
            };

            /** The expected payload for connect packet.
                If the MQTTClientOnlyImplementation macro is defined, then this structure is memory mapped (no heap allocation are made).
                This should be safe since, in that case, you'd never receive any such packet from a server */
            template<>
            struct Payload<CONNECT> Final : public SerializablePayload
            {
                /** This is mandatory to have */
                DynString           clientID;
                /** The Will message, if any provided */
                WillMessage *       willMessage;
                /** The user name that's used for authentication */
                DynString           username;
                /** The password used for authentication */
                DynBinData          password;

                /** Set the fixed header */
                void setFlags(const FixedFieldGeneric & field) { fixedHeader = (FixedField<CONNECT>*)&field; }

#if MQTTAvoidValidation != 1
                /** Check if the client ID is valid */
                bool checkClientID() const
                {
                    if (!clientID.length) return true; // A zero length id is allowed
                    // The two lines below are over zealous. We can remove them.
                    static char allowedChars[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
                    for (int i = 0; i < clientID.length; i++) if (!isInArray(clientID.data[i], allowedChars)) return false;
                    return true;
                }
                /** Check the will properties validity */
                bool checkWillProperties() const
                {
                    if (fixedHeader && !fixedHeader->willFlag) return true;
                    return willMessage->check();
                }
#endif

                /** This give the size required for serializing this property header in bytes */
                uint32 getSize() const { return clientID.getSize() + getFilteredSize(); }
                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's getSize() long.
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const
                {
                    uint32 o = clientID.copyInto(buffer);
                    if (!fixedHeader) return o;
                    if (fixedHeader->willFlag)       o += willMessage->copyInto(buffer+o);
                    if (fixedHeader->usernameFlag)   o += username.copyInto(buffer+o);
                    if (fixedHeader->passwordFlag)   o += password.copyInto(buffer+o);
                    return o;
                }
#if MQTTClientOnlyImplementation != 1
                /** Read the value from a buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long
                    @return The number of bytes read from the buffer, or 0xFF upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    uint32 s = clientID.readFrom(buffer, bufLength), o = 0;
                    if (isError(s) || !fixedHeader) return s;
                    o += s; buffer += s; bufLength -= s;
                    if (fixedHeader->willFlag)
                    {
                        if (!willMessage) willMessage = new WillMessage;
                        s = willMessage->readFrom(buffer, bufLength);
                        if (isError(s)) return s;
                        o += s; buffer += s; bufLength -= s;
                    }
                    if (fixedHeader->usernameFlag)
                    {
                        s = username.readFrom(buffer, bufLength);
                        if (isError(s)) return s;
                        o += s; buffer += s; bufLength -= s;
                    }
                    if (fixedHeader->passwordFlag)
                    {
                        s = password.readFrom(buffer, bufLength);
                        if (isError(s)) return s;
                        o += s; buffer += s; bufLength -= s;
                    }
                    return o;
                }
#endif
#if MQTTAvoidValidation != 1
                /** Check if this property is valid */
                bool check() const
                {
                    if (!clientID.check()) return false;
                    if (!fixedHeader) return true;
                    if (fixedHeader->willFlag && !checkWillProperties()) return false;
                    if (fixedHeader->usernameFlag && !username.check()) return false;
                    if (fixedHeader->passwordFlag && !password.check()) return false;
                    return true;
                }
#endif
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    out += MQTTStringPrintf("%*sCONNECT payload\n", (int)indent, "");
                    out += MQTTStringPrintf("%*sClientID: ", (int)indent + 2, ""); clientID.dump(out, 0);
                    if (fixedHeader->willFlag) willMessage->dump(out, indent + 2);  // Not testing pointer here, since it's a correctly constructed object is expected
                    out += MQTTStringPrintf("%*sUsername: ", (int)indent + 2, ""); username.dump(out, 0);
                    out += MQTTStringPrintf("%*sPassword: ", (int)indent + 2, ""); password.dump(out, 0);
                }
#endif

                Payload() : willMessage(0), fixedHeader(0) {}
#if MQTTClientOnlyImplementation != 1
                ~Payload() { delete0(willMessage); }
#endif

            private:
                /** This is the flags set in the connect header. This is used to ensure good serialization, this is not serialized */
                const FixedField<CONNECT> *  fixedHeader;
                uint32 getFilteredSize() const
                {
                    uint32 s = 0;
                    if (!fixedHeader) return s;
                    if (fixedHeader->willFlag)      s += willMessage->getSize();
                    if (fixedHeader->usernameFlag)  s += username.getSize();
                    if (fixedHeader->passwordFlag)  s += password.getSize();
                    return s;
                }
            };

            /** Generic code for payload with plain data */
            template <bool withAllocation>
            struct PayloadWithData : public SerializablePayload
            {
                /** The payload data */
                uint8 * data;
                /** The payload size */
                uint32  size;

                /** Set the expected packet size (this is useful for packet whose payload is application defined) */
                inline void setExpectedPacketSize(uint32 sizeInBytes)
                {
                    data = (uint8*)Platform::safeRealloc(data, sizeInBytes);
                    size = data ? sizeInBytes : 0;
                }
                /** This give the size required for serializing this property header in bytes */
                uint32 getSize() const { return size; }
                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's getSize() long.
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const
                {
                    memcpy(buffer, data, size);
                    return size;
                }
                /** Read the value from a buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long
                    @return The number of bytes read from the buffer, or 0xFF upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if (bufLength < size) return NotEnoughData;
                    memcpy(data, buffer, size);
                    return size;
                }

#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    out += MQTTStringPrintf("%*sPayload (length: %u)", (int)indent, "", size);
                    for (uint32 i = 0; i < size; i++)
                    {
                        if (!(i%16)) out+= MQTTStringPrintf("\n%*s", (int)indent + 2, "");
                        out += MQTTStringPrintf("%02X ", data[i]);
                    }
                    out += "\n";
                }
#endif
                PayloadWithData() : data(0), size(0) {}
                ~PayloadWithData() { free0(data); size = 0; }
            };

            template <>
            struct PayloadWithData<false> : public SerializablePayload
            {
                /** The payload data */
                const uint8 * data;
                /** The payload size */
                uint32  size;

                /** Set the expected packet size (this is useful for packet whose payload is application defined) */
                inline void setExpectedPacketSize(uint32 sizeInBytes)
                {
                    size = sizeInBytes;
                }
                /** This give the size required for serializing this property header in bytes */
                uint32 getSize() const { return size; }
                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's getSize() long.
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const
                {
                    memcpy(buffer, data, size);
                    return size;
                }
                /** Read the value from a buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long
                    @return The number of bytes read from the buffer, or 0xFF upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if (bufLength < size) return NotEnoughData;
                    data = buffer;
                    return size;
                }

#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    out += MQTTStringPrintf("%*sPayload (length: %u)", (int)indent, "", size);
                    for (uint32 i = 0; i < size; i++)
                    {
                        if (!(i%16)) out+= MQTTStringPrintf("\n%*s", (int)indent + 2, "");
                        out += MQTTStringPrintf("%02X ", data[i]);
                    }
                    out += "\n";
                 }
#endif

                PayloadWithData() : data(0), size(0) {}
                ~PayloadWithData() { data = 0; size = 0; }
            };

            /** The expected payload for subscribe packet */
            template<>
            struct Payload<SUBSCRIBE> Final: public SerializablePayload
            {
                /** The subscribe topics */
                SubscribeTopic * topics;

                /** Set the expected packet size (this is useful for packet whose payload is application defined) */
                inline void setExpectedPacketSize(uint32 sizeInBytes) { expSize = sizeInBytes; }

                /** This give the size required for serializing this property header in bytes */
                uint32 getSize() const { return topics ? topics->getSize() : 0; }
                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's getSize() long.
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const { return topics ? topics->copyInto(buffer) : 0; }
#if MQTTClientOnlyImplementation != 1
                /** Read the value from a buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long
                    @return The number of bytes read from the buffer, or 0xFF upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if (bufLength < expSize) return NotEnoughData;
                    if (topics) topics->suicide();
                    topics = new SubscribeTopic();
                    return topics->readFrom(buffer, expSize);
                }
#endif

#if MQTTAvoidValidation != 1
                /** Check if this property is valid */
                bool check() const { return topics ? topics->check() : true; }
#endif
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    out += MQTTStringPrintf("%*sSUBSCRIBE Payload\n", (int)indent, "");
                    if (topics) topics->dump(out, indent + 2);
                }
#endif


                Payload() : topics(0), expSize(0) {}
                ~Payload() { if (topics) topics->suicide(); topics = 0; }
            private:
                uint32 expSize;
            };

            /** The expected payload for unsubscribe packet */
            template<>
            struct Payload<UNSUBSCRIBE> Final: public SerializablePayload
            {
                /** The subscribe topics */
                UnsubscribeTopic * topics;

                /** Set the expected packet size (this is useful for packet whose payload is application defined) */
                inline void setExpectedPacketSize(uint32 sizeInBytes) { expSize = sizeInBytes; }

                /** This give the size required for serializing this property header in bytes */
                uint32 getSize() const { return topics ? topics->getSize() : 0; }
                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's getSize() long.
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const { return topics ? topics->copyInto(buffer) : 0; }
#if MQTTClientOnlyImplementation != 1
                /** Read the value from a buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long
                    @return The number of bytes read from the buffer, or 0xFF upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if (bufLength < expSize) return NotEnoughData;
                    if (topics) topics->suicide();
                    topics = new UnsubscribeTopic();
                    return topics->readFrom(buffer, expSize);
                }
#endif
#if MQTTAvoidValidation != 1
                /** Check if this property is valid */
                bool check() const { return topics ? topics->check() : true; }
#endif
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    out += MQTTStringPrintf("%*sUNSUBSCRIBE Payload\n", (int)indent, "");
                    if (topics) topics->dump(out, indent + 2);
                }
#endif


                Payload() : topics(0), expSize(0) {}
                ~Payload() { if (topics) topics->suicide(); topics = 0; }
            private:
                uint32 expSize;
            };


            /** The specialization for PUBLISH payload.
                As per the standard, it's opaque data that are application defined */
            template<> struct Payload<PUBLISH>  Final: public PayloadWithData<true> {};

            /** The expected payload for a subscribe acknowledge.
                The data is a array of reason code, and it should contains as many reasons as found in subscribe packet received */
            template<> struct Payload<SUBACK>   Final: public PayloadWithData<true> {};

            /** The expected payload for a unsubscribe acknowledge.
                The data is a array of reason code, and it should contains as many reasons as found in unsubscribe packet received */
            template<> struct Payload<UNSUBACK> Final: public PayloadWithData<true> {};


            template <ControlPacketType type, bool>
            struct PayloadSelector { typedef Payload<type> PayloadType; };

            template <> struct PayloadSelector<PUBLISH, true>   { typedef PayloadWithData<false> PayloadType; };
            template <> struct PayloadSelector<SUBACK, true>    { typedef PayloadWithData<false> PayloadType; };
            template <> struct PayloadSelector<UNSUBACK, true>  { typedef PayloadWithData<false> PayloadType; };

            // Those don't have any payload, let's simplify them
            template <bool a> struct PayloadSelector<CONNACK, a>    { typedef SerializablePayload PayloadType; };
            template <bool a> struct PayloadSelector<PUBACK, a>     { typedef SerializablePayload PayloadType; };
            template <bool a> struct PayloadSelector<PUBREL, a>     { typedef SerializablePayload PayloadType; };
            template <bool a> struct PayloadSelector<PUBREC, a>     { typedef SerializablePayload PayloadType; };
            template <bool a> struct PayloadSelector<PUBCOMP, a>    { typedef SerializablePayload PayloadType; };
            template <bool a> struct PayloadSelector<DISCONNECT, a> { typedef SerializablePayload PayloadType; };
            template <bool a> struct PayloadSelector<AUTH, a>       { typedef SerializablePayload PayloadType; };


            /** Generic variable header with heap allocated properties with or without an identifier */
            template <ControlPacketType type, bool propertyMapped>
            struct VHPropertyChooser
            {
                typedef typename ControlPacketMeta<type>::VariableHeader VHProperty;
                typedef Payload<type> PayloadType;
            };

            /** This is only valid for properties without an identifier */
            template <ControlPacketType type>
            struct VHPropertyChooser<type, true>
            {
                typedef PropertiesView VHProperty;
                typedef typename PayloadSelector<type, false>::PayloadType PayloadType;
            };

            /** The base for all control packet */
            struct ControlPacketSerializable : public Serializable
            {
                virtual uint32 computePacketSize(const bool includePayload = true) = 0;
            };
            /** The base for all control packet */
            struct ControlPacketSerializableImpl : public ControlPacketSerializable
            {
                /** The fixed header */
                FixedHeaderBase &                                               header;
                /** The remaining length in bytes, not including the header and itself */
                VBInt                                                           remLength;
                /** The fixed variable header */
                FixedFieldGeneric &                                             fixedVariableHeader;
                /** The variable header containing properties */
                SerializableProperties &                                        props;
                /** The payload (if any required) */
                SerializablePayload &                                           payload;


#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    out += MQTTStringPrintf("%*s%s control packet (rlength: %u)\n", (int)indent, "", Helper::getControlPacketName(header.getType()), (uint32)remLength);
                    header.dump(out, indent + 2);
                    fixedVariableHeader.dump(out, indent + 2);
                    props.dump(out, indent + 2);
                    payload.dump(out, indent + 2);
                }
#endif


                /** An helper function to actually compute the current packet size, instead of returning the computed value.
                    @param includePayload   If set (default), it compute the payload size and set the remaining length accordingly
                    @return the packet size in bytes if includePayload is true, or the expected size of the payload if not */
                uint32 computePacketSize(const bool includePayload = true)
                {
                    if (includePayload)
                    {
                        uint32 o = fixedVariableHeader.getSize() + props.getSize() + payload.getSize();
                        remLength = o;
                        return o + 1 + remLength.getSize();
                    }
                    return (uint32)remLength - fixedVariableHeader.getSize() - props.getSize();
                }
                /** This give the size required for serializing this property header in bytes */
                uint32 getSize() const { return 1 + remLength.getSize() + (uint32)remLength; }
                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's getSize() long.
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const
                {
                    uint32 o = 1; buffer[0] = header.typeAndFlags;
                    o += remLength.copyInto(buffer+o);
                    o += fixedVariableHeader.copyInto(buffer+o);
                    o += props.copyInto(buffer+o);
                    o += payload.copyInto(buffer+o);
                    return o;
                }
                /** Read the value from a buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long
                    @return The number of bytes read from the buffer, or 0xFF upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if (bufLength < 2) return NotEnoughData;
                    uint32 o = 1; const_cast<uint8&>(header.typeAndFlags) = buffer[0];

                    buffer += o; bufLength -= o;

                    uint32 s = remLength.readFrom(buffer, bufLength);
                    if (isError(s)) return s;
                    o += s; buffer += s; bufLength -= s;
                    uint32 expLength = (uint32)remLength;
                    if (bufLength < expLength) return NotEnoughData;

                    fixedVariableHeader.setRemainingLength(expLength);
                    s = fixedVariableHeader.readFrom(buffer, bufLength);
                    if (isError(s)) return isShortcut(s) ? o + expLength : s;
                    o += s; buffer += s; bufLength -= s;

                    s = props.readFrom(buffer, bufLength);
                    if (isError(s)) return s;
                    o += s; buffer += s; bufLength -= s;

                    payload.setExpectedPacketSize(computePacketSize(false));
                    s = payload.readFrom(buffer, bufLength);
                    if (isError(s)) return s;
                    return o + s;
                }
#if MQTTAvoidValidation != 1
                /** Check if this property is valid */
                bool check() const
                {
                    return header.check() && remLength.check() && fixedVariableHeader.check() && props.checkPropertiesFor(header.getType()) && payload.check();
                }
#endif
                ControlPacketSerializableImpl(FixedHeaderBase & _header, FixedFieldGeneric & _fixedVariableHeader, SerializableProperties & _props, SerializablePayload & _payload)
                    : header(_header), fixedVariableHeader(_fixedVariableHeader), props(_props), payload(_payload) {}
            };

            template <ControlPacketType type, bool propertyMapped = false>
            struct ControlPacket Final : public ControlPacketSerializableImpl
            {
                /** The fixed header */
                typename ControlPacketMeta<type>::FixedHeader                   header;
                /** The fixed variable header */
                FixedField<type>                                                fixedVariableHeader;
                /** The variable header containing properties */
                typename VHPropertyChooser<type, propertyMapped>::VHProperty    props;
                /** The payload (if any required) */
#if MQTTClientOnlyImplementation == 1
                // Client implementation never need to allocate anything here, either it's client provided or server's buffer provided
                typename PayloadSelector<type, true>::PayloadType               payload;
#else
                typename PayloadSelector<type, propertyMapped>::PayloadType     payload;
#endif
                ControlPacket() : ControlPacketSerializableImpl(header, fixedVariableHeader, props, payload)
                {
                    payload.setFlags(fixedVariableHeader); fixedVariableHeader.setFlags(header.typeAndFlags);
                }
            };

            /** Publish reply packets are too similar to avoid making a single version out of them to avoid code bloat. */
            struct PublishReplyPacket Final : public ControlPacketSerializableImpl
            {
                /** The fixed header */
                FixedHeaderBase                                                 header;
                /** The fixed variable header */
                FixedField<PUBACK>                                              fixedVariableHeader;
                /** The variable header containing properties */
                typename VHPropertyChooser<PUBACK, true>::VHProperty            props;
                /** The payload (if any required) */
                SerializablePayload                                             payload;

                PublishReplyPacket(const ControlPacketType type) : ControlPacketSerializableImpl(header, fixedVariableHeader, props, payload), header(type, type == PUBREL ? 2 : 0)
                {}
            };


            /** Ping control packet are so empty that it makes sense to further optimize their parsing to strict minimum */
            template <ControlPacketType type>
            struct PingTemplate : public ControlPacketSerializable
            {
                /** The fixed header */
                typename ControlPacketMeta<type>::FixedHeader       header;

                /** This give the size required for serializing this property header in bytes */
                uint32 getSize() const { return 2; }
                /** The packet size is always 2 */
                uint32 computePacketSize(const bool includePayload = true) { return 2; }
                /** Copy the value into the given buffer.
                    @param buffer   A pointer to an allocated buffer that's getSize() long.
                    @return The number of bytes used in the buffer */
                uint32 copyInto(uint8 * buffer) const
                {
                    buffer[0] = header.typeAndFlags; buffer[1] = 0; return 2;
                }
                /** Read the value from a buffer.
                    @param buffer   A pointer to an allocated buffer that's at least 1 byte long
                    @return The number of bytes read from the buffer, or 0xFF upon error */
                uint32 readFrom(const uint8 * buffer, uint32 bufLength)
                {
                    if (bufLength < 2) return NotEnoughData;
                    const_cast<uint8&>(header.typeAndFlags) = buffer[0];
                    if (buffer[1]) return BadData;
                    return 2;
                }
#if MQTTAvoidValidation != 1
                /** Check if this property is valid */
                bool check() const { return header.check(); }
#endif
#if MQTTDumpCommunication == 1
                void dump(MQTTString & out, const int indent = 0)
                {
                    out += MQTTStringPrintf("%*s%s control packet\n", (int)indent, "", Helper::getControlPacketName(type));
                    header.dump(out, indent + 2);
                }
#endif

            };
            /** Declare the ping request */
            template<> struct ControlPacket<PINGREQ>    Final: public PingTemplate<PINGREQ> {};
            /** Declare the ping response */
            template<> struct ControlPacket<PINGRESP>   Final: public PingTemplate<PINGRESP> {};

            /** Some useful type definition to avoid understanding the garbage above */
            typedef ControlPacket<PUBLISH>          PublishPacket;
            typedef ControlPacket<PUBLISH, true>    ROPublishPacket;
            typedef ControlPacket<SUBACK>           SubACKPacket;
            typedef ControlPacket<SUBACK, true>     ROSubACKPacket;
            typedef ControlPacket<UNSUBACK>         UnsubACKPacket;
            typedef ControlPacket<UNSUBACK, true>   ROUnsubACKPacket;
            typedef ControlPacket<CONNECT>          ConnectPacket;
            typedef ControlPacket<CONNACK>          ConnACKPacket;
            typedef ControlPacket<CONNACK, true>    ROConnACKPacket;
            typedef ControlPacket<AUTH>             AuthPacket;
            typedef ControlPacket<AUTH, true>       ROAuthPacket;
            typedef ControlPacket<PUBACK>           PubACKPacket;
            typedef ControlPacket<PUBACK, true>     ROPubACKPacket;
            typedef ControlPacket<PUBREC>           PubRecPacket;
            typedef ControlPacket<PUBREC, true>     ROPubRecPacket;
            typedef ControlPacket<PUBREL>           PubRelPacket;
            typedef ControlPacket<PUBREL, true>     ROPubRelPacket;
            typedef ControlPacket<PUBCOMP>          PubCompPacket;
            typedef ControlPacket<PUBCOMP, true>    ROPubCompPacket;
            typedef ControlPacket<DISCONNECT>       DisconnectPacket;
            typedef ControlPacket<DISCONNECT, true> RODisconnectPacket;
            typedef ControlPacket<PINGREQ>          PingReqPacket;
            typedef ControlPacket<PINGRESP>         PingRespPacket;

        }
    }
}



#endif

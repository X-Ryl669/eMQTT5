// We need our implementation
#include <Network/Clients/MQTT.hpp>

#if MQTTUseAuth == 1
  /** Used to track reentrancy in the AUTH recursive scheme */
  enum AuthReentrancy
  {
     FromConnect = 0x80000000,
     AuthMask    = 0x7FFFFFFF,
  };
#endif


#if MQTTOnlyBSDSocket != 1
#pragma message("This configuration is not supported and depends on external code in tests folder that is not exported upon install")
// We need socket declaration
#include "Network/Socket.hpp"
// We need SSL socket declaration too
//#include "../../../include/Network/SSLSocket.hpp"
// We need FastLock too
#include "Threading/Lock.hpp"
  #if MQTTDumpCommunication == 1
    // We need hexDump
    #include "Utils/Dump.hpp"
  #endif
#else
// We need BSD socket here
#include <sys/socket.h>
// We need gethostbyname or getaddrinfo
#include <netdb.h>
// We need TCP_NODELAY
#include <netinet/tcp.h>
// We need std::atomic too
#include <atomic>
  #if MQTTUseTLS == 1
    // We need MBedTLS code
    #include <mbedtls/certs.h>
    #include <mbedtls/ctr_drbg.h>
    #include <mbedtls/entropy.h>
    #include <mbedtls/error.h>
    #include <mbedtls/net_sockets.h>
    #include <mbedtls/platform.h>
    #include <mbedtls/ssl.h>
  #endif
#endif
// We need StackHeapBuffer to avoid stressing the heap allocator when it's not required
#include <Platform/StackHeapBuffer.hpp>

// This is the maximum allocation that'll be performed on the stack before it's being replaced by heap allocation
// This also means that the stack size for the thread using such function must be larger than this value
#define StackSizeAllocationLimit    512


namespace Network { namespace Client {

    #pragma pack(push, 1)
    /** A packet ID buffer that's allocated on an existing buffer (using variable length array).
        This should allow to use a single allocation for the whole lifetime of the client

        PacketID are 16 bits but we use 32 bits here because:
          1. We use bit 16 for storing the communication direction (1 is for broker to client, 0 for client to broker), since packet ID allocation is independent of direction
          2. We use bit 31 for storing the QoS level (0 is for QoS1, 1 for QoS2)
          3. We use bit 30 for storing the publish cycle step (0 for non ACKed QoS2, 1 for PUBREC or PUBREL depending on direction)
        */
    struct Buffers
    {
        uint8 * recvBuffer() { return end() * sizeof(uint32) + buffer; }
        const uint8 * recvBuffer() const { return end() * sizeof(uint32) + buffer; }
        uint8 findID(uint32 ID)
        {
            for (uint8 i = 0; i < end(); i++)
                if ((packetsID()[i] & 0x1FFFF) == ID)
                    return i;
            return maxID;
        }
        bool clearSetID(uint32 set, uint32 clear = 0)
        {
            uint8 i = findID(clear);
            if (i == end()) return false;
            packetsID()[i] = set;
            return true;
        }
        static inline bool isSending(uint32 ID)     { return (ID & 0x10000) == 0 && ID; }
        static inline bool isQoS1(uint32 ID)        { return (ID & 0x80000000) == 0; }
        static inline bool isQoS2Step2(uint32 ID)   { return (ID & 0x40000000) != 0; }
        inline bool storeQoS1ID(uint32 ID)  { return clearSetID((uint32)ID, 0); }
        inline bool storeQoS2ID(uint32 ID)  { return clearSetID((uint32)ID | 0x80000000, 0); }
        inline bool avanceQoS2(uint32 ID)   { uint8 p = findID(ID); if (p == maxID) return false; packetsID()[p] |= 0x40000000; return true; }
        inline bool releaseID(uint32 ID)    { return clearSetID(0, (uint32)ID); }
        inline uint8 end() const            { return maxID; }
        inline uint8 packetsCount() const   { return maxID / 3; }
        inline void reset()                 { memset(packetsID(), 0, maxID * 3 * sizeof(uint32)); }
        inline uint32 packetID(uint8 i) const { return packetsID()[i]; }
        inline uint8 countSentID() const    {
            uint8 count = 0;
            for (uint8 i = 0; i < end(); i++)
                if ((packetsID()[i] & 0x10000) == 0 && packetsID()[i])
                    count++;
            return count;
        }

        Buffers(uint32 size, uint32 maxID) : buffer((uint8*)::calloc(size + maxID * 3 * sizeof(uint32), 1)), size(size), maxID((uint8)(maxID * 3)) {}
        ~Buffers() { ::free(buffer); buffer = 0; size = 0; maxID = 0; }

        uint32  size;

    private:
        uint32 * packetsID() { return (uint32*)buffer; }
        const uint32 * packetsID() const { return (const uint32*)buffer; }

        uint8 * buffer;
        uint8   maxID;
    };
    #pragma pack(pop)

    template <typename T, typename U> constexpr inline U bitU(T value) { return 1 << (U)value; }
    template <typename T> constexpr inline uint16 bit(T value) { return 1 << (uint16)value; }
    namespace State
    {
        /** The current connection state */
        enum MQTT
        {
            Unknown         = 0,
            Connecting      = 1,
            Authenticating  = 2,
            Running         = 3,
            Subscribing     = 4,
            Unsubscribing   = 5,
            Pinging         = 6,
            Disconnecting   = 7,
            Disconnected    = 8,

            Count           = 9,
        };

        static constexpr uint16 publishMask = bit(Protocol::MQTT::V5::ControlPacketType::PUBLISH) | bit(Protocol::MQTT::V5::ControlPacketType::PUBACK) | bit(Protocol::MQTT::V5::ControlPacketType::PUBREC) | bit(Protocol::MQTT::V5::ControlPacketType::PUBREL) | bit(Protocol::MQTT::V5::ControlPacketType::PUBCOMP);
        static constexpr uint16 releaseIDMask = bit(Protocol::MQTT::V5::ControlPacketType::PUBACK) | bit(Protocol::MQTT::V5::ControlPacketType::PUBCOMP);
        static constexpr uint16 releaseBufferMask = bit(Protocol::MQTT::V5::ControlPacketType::PUBACK) | bit(Protocol::MQTT::V5::ControlPacketType::PUBREC);
        static constexpr uint16 expectedPacketMask[] =
        {
            0,
            bit(Protocol::MQTT::V5::ControlPacketType::CONNACK) | bit(Protocol::MQTT::V5::ControlPacketType::AUTH),
            bit(Protocol::MQTT::V5::ControlPacketType::CONNACK) | bit(Protocol::MQTT::V5::ControlPacketType::AUTH),
            publishMask,
            bit(Protocol::MQTT::V5::ControlPacketType::SUBACK) | publishMask,
            bit(Protocol::MQTT::V5::ControlPacketType::UNSUBACK) | publishMask,
            bit(Protocol::MQTT::V5::ControlPacketType::PINGRESP) | publishMask,
            bit(Protocol::MQTT::V5::ControlPacketType::DISCONNECT),
            0
        };
    }

    static void dumpBufferAsPacket(const char * prompt, const uint8* buffer, uint32 length);

#if MQTTQoSSupportLevel == 1
    struct PacketBookmark
    {
        uint16 ID;
        uint32 size;
        uint32 pos;

        inline void set(uint16 ID, uint32 size, uint32 pos) { this->ID = ID; this->size = size; this->pos = pos; }
        PacketBookmark(uint16 ID = 0, uint32 size = 0, uint32 pos = 0) : ID(ID), size(size), pos(pos) {}
    };
    struct RingBufferStorage::Impl
    {
        /** Read and write pointer in the ring buffer */
        uint32           r, w;
        /** Buffer size minus 1 in bytes */
        const uint32     sm1;
        /** The buffer to write packets into */
        uint8 *          buffer;
        /** The metadata about the packets */
        PacketBookmark * packets;
        /** Maximum number of packets in the metadata array */
        uint8            packetsCount;

        /** Find the packet with the given ID */
        uint8 findID(uint32 ID)
        {
            for (uint8 i = 0; i < packetsCount; i++)
                if (packets[i].ID == ID)
                    return i;
            return packetsCount;
        }

        /** Get the consumed size in the buffer */
        inline uint32 getSize() const { return w >= r ? w - r : (sm1 - r + w + 1); }
        /** Get the available size in the buffer */
        inline uint32 freeSize() const { return sm1 - getSize(); }

        /** Add a packet to this buffer (no allocation is done at this time) */
        bool save(const uint16 packetID, const uint8 * packet, uint32 size)
        {
            // Check we can fit the packet
            if (size > sm1 || freeSize() < size) return false;
            // Check if we have a free space for storing the packet's information
            uint8 i = findID(0);
            if (i == packetsCount) return false;

            const uint32 part1 = min(size, sm1 - w + 1);
            const uint32 part2 = size - part1;

            memcpy((buffer + w), packet, part1);
            memcpy((buffer), packet + part1, part2);

            packets[i].set(packetID, size, w);
            w = (w + size) & sm1;
            return true;
        }
        /** Get a packet from the buffer.
            Since this is used to resend packet over a TCP (thus streaming) socket,
            we can skip one copy to rebuilt a contiguous packet by simply returning
            the two part in the ring buffer and let the application send them successively.
            From the receiving side, they'll be received as one contiguous packet. */
        bool load(const uint16 packetID, const uint8 *& packetHead, uint32 & sizeHead, const uint8 *& packetTail,  uint32 & sizeTail)
        {
            // Look for the packet
            uint8 i = findID(packetID);
            if (i == packetsCount) return false;

            // Check if the packet is split
            packetHead = buffer + packets[i].pos;
            sizeHead = min(packets[i].size, (sm1 - packets[i].pos + 1));
            sizeTail = packets[i].size - sizeHead;
            packetTail = buffer;
            return true;
        }

        /** Remove a packet from the buffer */
        bool release(const uint16 packetID)
        {
            uint8 i = findID(packetID);
            if (i == packetsCount) return false;

            PacketBookmark & packet = packets[i];

            // Here, we have 2 cases. Either the packet is on the read position of the ring buffer
            // and in that case, we just need to advance the read position.
            // Either it's in the middle of the ring buffer and we need to move all the data around to remove it
            // Let's deal with the former case first
            uint32 pos = packet.pos, size = packet.size, end = (packet.pos + packet.size) & sm1;
            packet.set(0, 0, 0);
            if (pos == r)
            {
                r = (r + size) & sm1;
                return true;
            }

            // Another optimization step is when the write position is at the end of this packet
            // We can just revert the storage of the packet directly
            if (end == w)
            {
                w = pos;
                return true;
            }

            // Ok, now we have to move the data around here
            // First let's move memory to remove that packet.
            // We'll fix the packet's position later on
            // We are in this case here:
            //  bbbbccw  r p   eaaaaaaaaa      with p/e (pos/end) and a / b / c the next packet
            //        |  | |   |
            // [-------------------------]
            // After move, it should look like:
            //  ccw       r aaaaaaaaabbbb      with p/e (pos/end) and a / b / c the next packet
            //    |       | |   |
            // [-------------------------]
            // We see they are 3 sections: a is the data between e and the buffer end
            // b is the data whose size is equal to buffer end - p - sizeof(a) (the part that was moved from
            // the beginning of the buffer to the end of the buffer)
            // and c is the part that was move from the end of the packet to the beginning of the buffer
            // We have to perform move a to p, move b to buffer.end - a.size and move c to buffer begin

            // It's hard to think without unwrapping the buffer, so let's imagine we are doing so (we'll rewrap after discussion)
            // Let's set w' = w+sm1+1, e' = e + (sm1 - 1)
            // It'll lead to this diagram:
            //  bbbbccw  r p   eaaaaaaaaa BBBBCCW       with p/e (pos/end) and a / b / c the next packet
            //        |  | |   |                |
            // [-------------------------:-------------------------]
            // Or
            //     eaacccccw  r        p     EAACCCCCW
            //     |       |  |        |     |       |
            // [-------------------------:-------------------------]
            // Or
            //      r  p    eaaaaa w
            //      |  |    |    | |
            // [-------------------------:-------------------------]
            // In that case, we are doing a single memory move operation here, but we simply wrap the position
            uint32 s = sm1 + 1, W = w < pos ? w + s : w, E = end < pos ? end + s : end;

            for (uint32 u = 0; u < W - E; u++)
                buffer[(u + pos) & sm1] = buffer[(u + pos + size) & sm1];

            // Adjust the new write position
            w = (W - size) & sm1;

            // We can split the packets in 2 cases: before or after the packet to remove.
            // We'll iterate each packet and decide if we need to move it (it's after the packet to remove)
            // This isn't the most efficient algorithm, but since the number of packets to store is small
            // there's no point in optimizing it further

            bool continueSearching = true;
            while (continueSearching)
            {
                continueSearching = false;
                for (uint8 j = 0; j < packetsCount; j++)
                {
                    PacketBookmark & iter = packets[j];
                    if (iter.pos == end)
                    {
                        end = (iter.pos + iter.size) & sm1;
                        iter.pos = pos;
                        pos = (iter.pos + iter.size) & sm1;
                        continueSearching = true;
                        break;
                    }
                }
            }
            return true;
        }

  #if 0
        /** This is only used in the test code to ensure it's working as expected */
        bool selfCheck() const
        {
            for (uint8 j = 0; j < packetsCount; j++)
            {
                const PacketBookmark & i = packets[j];
                if (!i.ID) continue;
                const uint32 end = (i.pos + i.size) & sm1;
                if (end == w) continue;
                // Find if any packet starts with the next slot
                bool found = false;
                for (uint8 k = 0; k < packetsCount; k++)
                {
                    if (packets[k].pos == end)
                    {
                        // Found one
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    fprintf(stderr, "Error while checking packet %u (%u,s:%u), no next packet found and not tail position\n", i.ID, i.pos, i.size);
                    return false;
                }
            }
            fprintf(stdout, "RB: r(%u) w(%u)\n", r, w);
            return true;
        }
  #endif

        Impl(size_t size, uint8 * buffer, uint8 packetsCount, PacketBookmark * packets) : r(0), w(0), sm1(size - 1), buffer(buffer), packets(packets), packetsCount(packetsCount) {}
    };

    /** Helper function to perform a single allocation for all data so it avoids stressing the allocator */
    static RingBufferStorage::Impl * allocImpl(const uint32 bufferSize, const uint32 maxPacketCount)
    {
        uint8 * p = (uint8*)::calloc(1, sizeof(RingBufferStorage::Impl) + bufferSize + maxPacketCount * sizeof(PacketBookmark));
        return new (p) RingBufferStorage::Impl(bufferSize, p + sizeof(RingBufferStorage::Impl), maxPacketCount, (PacketBookmark*)(p + sizeof(RingBufferStorage::Impl) + bufferSize));
    }

    bool RingBufferStorage::savePacketBuffer(const uint16 packetID, const uint8 * buffer, const uint32 size) { return impl->save(packetID, buffer, size); }
    bool RingBufferStorage::releasePacketBuffer(const uint16 packetID) { return impl->release(packetID); }
    bool RingBufferStorage::loadPacketBuffer(const uint16 packetID, const uint8 *& bufferHead, uint32 & sizeHead, const uint8 *& bufferTail, uint32 & sizeTail)
    {
        return impl->load(packetID, bufferHead, sizeHead, bufferTail, sizeTail);
    }

    /** The ring buffer storage size. Must be a power of 2 */
    RingBufferStorage::RingBufferStorage(const size_t bufferSize, const size_t maxPacketCount) : impl(allocImpl(bufferSize, maxPacketCount)) {}
    RingBufferStorage::~RingBufferStorage() { ::free0(impl); }
#endif

    /** Common base interface that's common to all implementation using CRTP to avoid code duplication */
    template <typename Child>
    struct ImplBase
    {
        typedef MQTTv5::ErrorType ErrorType;

        /** The DER encoded certificate (if provided) */
        const Protocol::MQTT::Common::DynamicBinDataView *  brokerCert;
        /** This client unique identifier */
        Protocol::MQTT::Common::DynamicString               clientID;
        /** The message received callback to use */
        MessageReceived *                                   cb;

        /** The last communication time in second */
        uint32                      lastCommunication;
        /** The publish current default identifier allocator */
        uint16                      publishCurrentId;
        /** The keep alive delay in seconds */
        uint16                      keepAlive;
#if MQTTUseUnsubscribe == 1
        /** The last unsubscribe id */
        uint16                      unsubscribeId;
        /** The last unsubscribe error code */
        MQTTv5::ErrorType::Type     lastUnsubscribeError;
#endif
#if MQTTQoSSupportLevel == 1
        /** The storage interface */
        PacketStorage *     storage;
#endif
        /** The reading state. Because data on a TCP stream is
            a stream, we have to remember what state we are currently following while parsing data */
        enum RecvState
        {
            Ready = 0,
            GotType,
            GotLength,
            GotCompletePacket,
        }                   recvState;
        /** The maximum packet size the server is willing to accept */
        uint32              maxPacketSize;
        /** The available data in the buffer */
        uint32              available;
        /** The receiving buffer */
        Buffers             buffers;
        /** The receiving VBInt size for the packet header */
        uint8               packetExpectedVBSize;
        /** The current MQTT state in the state machine */
        State::MQTT         state;

        uint16 allocatePacketID()
        {
            return ++publishCurrentId;
        }

        ImplBase(const char * clientID, MessageReceived * callback, const Protocol::MQTT::Common::DynamicBinDataView * brokerCert, PacketStorage * storage)
             : brokerCert(brokerCert), clientID(clientID), cb(callback), lastCommunication(0), publishCurrentId(0), keepAlive(300),
#if MQTTUseUnsubscribe == 1
               unsubscribeId(0), lastUnsubscribeError(ErrorType::WaitingForResult),
#endif
#if MQTTQoSSupportLevel == 1
               storage(storage),
#endif
               recvState(Ready), buffers(max(callback->maxPacketSize(), 8U), min(callback->maxUnACKedPackets(), 127U)), maxPacketSize(65535), available(0), packetExpectedVBSize(Protocol::MQTT::Common::VBInt(max(callback->maxPacketSize(), 8U)).getSize()), state(State::Unknown)
        {
#if MQTTQoSSupportLevel == 1
            if (!storage) this->storage = new RingBufferStorage(buffers.size, buffers.packetsCount() * 2);
#else
            (void)storage; // Prevent variable unused warning
#endif
        }
#if MQTTQoSSupportLevel == 1
        ~ImplBase() { delete0(storage); }
#endif

        bool shouldPing()
        {
            return (((uint32)time(NULL) - lastCommunication) >= keepAlive);
        }

        void setConnectionState(State::MQTT connState) { state = connState; }

        bool hasValidLength() const
        {
            Protocol::MQTT::Common::VBInt l;
            return l.readFrom(buffers.recvBuffer() + 1, available - 1) != Protocol::MQTT::Common::BadData;
        }

        /** Receive a control packet from the socket in the given time.
            @retval positive    The number of bytes received
            @retval 0           Protocol error, you should close the socket
            @retval -1          Socket error
            @retval -2          Timeout */
        int receiveControlPacket(const bool lowLatency = false)
        {
            if (!that()->socket) return -1;
            // Depending on the current state, we need to fetch as many bytes as possible within the given timeoutMs
            // This is a complex problem here because we want both to optimize for
            //  - latency (returns as fast as possible when we've received a complete packet)
            //  - blocking time (don't return immediately if the data is currently in transfer, need to wait for it to arrive)
            //  - minimal syscalls (don't call recv byte per byte as the overhead will be significant)
            //  - network queue (don't fetch more byte than necessary for getting a single control packet)
            //  - streaming usage (this can be called while a control packet was being received and we timed out)

            // So the algorithm used here depends on the current receiving state
            // If we haven't received packet length yet, we have to fetch the header very carefully
            // Else, we can enter a more general receiving loop until we have all bytes from the control packet
            int ret = 0;
            Protocol::MQTT::Common::VBInt len;

#if MQTTLowLatency == 1
            // In low latency mode, return as early as possible
            if (lowLatency && !this->socket->select(true, false, 0)) return -2;
#endif

            // We want to keep track of complete timeout time over multiple operations
            auto timeout = that()->getTimeout();
            switch (recvState)
            {
            case Ready:
            case GotType:
            {   // Here, make sure we only fetch the length first
                // The minimal size is 2 bytes for PINGRESP, DISCONNECT and AUTH.
                // Because of this, we can't really outsmart the system everytime
                ret = that()->recv((char*)&buffers.recvBuffer()[available], 2 - available, timeout);
                if (ret > 0) available += ret;
                // Deal with timeout first
                if (timeout == 0) return -2;
                // Deal with socket errors here
                if (ret < 0 || available < 2) return -1;
                // Depending on the packet type, let's wait for more data
                if (buffers.recvBuffer()[0] < 0xD0 || buffers.recvBuffer()[1]) // Below ping response or packet size larger than 2 bytes
                {
                    int querySize = (packetExpectedVBSize + 1) - available;
                    ret = that()->recv((char*)&buffers.recvBuffer()[available], querySize, timeout);
                    if (ret > 0) available += ret;
                    // Deal with timeout first
                    if (timeout == 0) return -2;
                    // Deal with socket errors here
                    if (ret < 0) return ret;
                }

                recvState = GotLength;
                break;
            }
            default: break;
            }
            // Here we should either have a valid control packet header
            uint32 r = len.readFrom(&buffers.recvBuffer()[1], available - 1);
            if (r == Protocol::MQTT::Common::BadData)
                return 0; // Close the socket here, the given data are wrong or not the right protocol
            if (r == Protocol::MQTT::Common::NotEnoughData)
            {
                if (available >= (packetExpectedVBSize+1))
                {   // The server sends us a packet that's larger than the expected maximum size,
                    // In MQTTv5 it's a protocol error, so let's disconnect
                    return 0;
                }
                // We haven't received enough data in the given timeout to make progress, let's report a timeout
                recvState = GotType;
                return -2;
            }
            uint32 remainingLength = len;
            uint32 totalPacketSize = remainingLength + 1 + len.getSize();
            ret = totalPacketSize == available ? 0 : that()->recv((char*)&buffers.recvBuffer()[available], (totalPacketSize - available), timeout);
            if (ret > 0) available += ret;
            if (timeout == 0) return -2;
            if (ret < 0) return ret;

            // Ok, let's check if we have received the complete packet
            if (available == totalPacketSize)
            {
                recvState = GotCompletePacket;
#if MQTTDumpCommunication == 1
                dumpBufferAsPacket("< Received packet", buffers.recvBuffer(), available);
#endif
                lastCommunication = (uint32)time(NULL);
                return (int)available;
            }
            // No yet, but we probably timed-out.
            return -2;
        }

        /** Get the last received packet type */
        Protocol::MQTT::V5::ControlPacketType getLastPacketType() const
        {
            if (recvState != GotCompletePacket) return Protocol::MQTT::V5::RESERVED;
            Protocol::MQTT::V5::FixedHeader header;
            header.raw = buffers.recvBuffer()[0];
            return (Protocol::MQTT::V5::ControlPacketType)(uint8)header.type;
        }

        /** Extract a control packet of the given type */
        int extractControlPacket(const Protocol::MQTT::V5::ControlPacketType type, Protocol::MQTT::Common::Serializable & packet)
        {
            if (recvState != GotCompletePacket)
            {
                int ret = receiveControlPacket();
                if (ret <= 0) return ret;

                if (recvState != GotCompletePacket)
                    return -2;
            }

            // Check the packet is the last expected type
            if (getLastPacketType() != type) return -3;

            // Seems to be the expected type, let's unserialize it
            uint32 r = packet.readFrom(buffers.recvBuffer(), buffers.size);
            if (Protocol::MQTT::Common::isError(r)) return -4; // Parsing error

            // Done with receiving the packet let's remember it
            resetPacketReceivingState();

            return (int)r;
        }

        void resetPacketReceivingState() { recvState = Ready; available = 0; }
        inline Child * that() { return static_cast<Child*>(this); }
        inline const Child * that() const { return static_cast<const Child*>(this); }


        void close(const Protocol::MQTT::V5::ReasonCodes code = Protocol::MQTT::V5::ReasonCodes::UnspecifiedError)
        {
            delete0(that()->socket);
            cb->connectionLost(code);
            state = State::Unknown;
        }

        bool isOpen()
        {
            return that()->socket != nullptr;
        }

        int send(const char * buffer, const uint32 length)
        {
            if (!that()->socket) return -1;
            // Prevent mixing sending packet on the wire here, only one thread can send a complete packet at once.
#if MQTTDumpCommunication == 1
            dumpBufferAsPacket("> Sending packet", (const uint8*)buffer, length);
#endif
            return that()->sendImpl(buffer, length);
        }

        ErrorType sendAndReceive(const void * buffer, const uint32 packetSize, bool withAnswer)
        {
            // Make sure we are on a clean receiving state
            resetPacketReceivingState();

            if (send((const char*)buffer, packetSize) != packetSize)
                return ErrorType::NetworkError;

            if (!withAnswer) return ErrorType::Success;

            // Next, we'll wait for server's CONNACK or AUTH coming here (or error)
            int receivedPacketSize = receiveControlPacket();
            if (receivedPacketSize <= 0)
            {   // This will also comes here
                if (receivedPacketSize == 0) close();
                return receivedPacketSize == -2 ? ErrorType::TimedOut : ErrorType::NetworkError;
            }
            return ErrorType::Success;
        }

        ErrorType prepareSAR(Protocol::MQTT::V5::ControlPacketSerializable & packet, bool withAnswer = true, bool isPublish = false)
        {
            // Ok, setting are done, let's build this packet now
            uint32 packetSize = packet.computePacketSize();
            DeclareStackHeapBuffer(buffer, packetSize, StackSizeAllocationLimit);
            if (packet.copyInto(buffer) != packetSize)
                return ErrorType::UnknownError;

#if MQTTQoSSupportLevel == -1
#else
            // Check for saving publish packet if required
            if (isPublish)
            {
                Protocol::MQTT::V5::PublishPacket publish = (Protocol::MQTT::V5::PublishPacket&)packet;
                uint8 QoS = publish.header.getQoS();
                if (QoS > 0) {
                    uint16 packetID = publish.fixedVariableHeader.packetID;
  #if MQTTQoSSupportLevel == 1
                    // Save packet
                    if (!storage->savePacketBuffer(packetID, buffer, packetSize))
                        return ErrorType::StorageError;
  #endif
                    // Save packet ID too
                    if ((QoS == 1 && !buffers.storeQoS1ID(packetID)) || (QoS == 2 && !buffers.storeQoS2ID(packetID)))
                        return ErrorType::StorageError;
                }
            }
#endif

    #if MQTTDumpCommunication == 1
    //      String out;
    //      packet.dump(out, 2);
    //      printf("Prepared:\n%s\n", (const char*)out);
    #endif
            return sendAndReceive(buffer, packetSize, withAnswer);
        }

        ErrorType requestOneLoop(Protocol::MQTT::V5::ControlPacketSerializable & packet)
        {
            ErrorType ret = prepareSAR(packet, true);
            if (ret) return ret;

            while(true)
            {
                ret = dealWithNoise();
                if (ret != ErrorType::TranscientPacket) break;

                // Receive a new packet to continue the loop
                int receivedPacketSize = receiveControlPacket();
                if (receivedPacketSize <= 0)
                {   // This will also comes here
                    if (receivedPacketSize == 0) close();
                    return receivedPacketSize == -2 ? ErrorType::TimedOut : ErrorType::NetworkError;
                }
            }
            // Exit the special state if any the reply packet is the one expected
            if (ret == ErrorType::Success &&
                state > State::Running && state < State::Disconnecting &&
                (bit(getLastPacketType()) & State::publishMask) == 0)
                state = State::Running;

            return ret;
        }

        /** Deal with answer packet noise here.
            This is called after receiving a control packet.
            The mask is used to filter the allowed packet types we expect to see.
            Typically, depending on the client state, the mask will be either:
             * At CONNECT stage: AUTH / CONNACK / DISCONNECT
             * At AUTH stage: AUTH / CONNACK / DISCONNECT
             * After connected stage: PUBLISH / PUBACK / PUBREC / PUBREL / PUBCOMP / DISCONNECT
             * At PINGREQ state: PINGRESP / PUBLISH / PUBACK / PUBREC / PUBREL / PUBCOMP / DISCONNECT
             * At DISCONNECT state: DISCONNECT
             * At SUBSCRIBE state: SUBACK / PUBLISH / PUBACK / PUBREC / PUBREL / PUBCOMP / DISCONNECT
             * At UNSUBSCRIBE state: UNSUBACK / PUBLISH / PUBACK / PUBREC / PUBREL / PUBCOMP / DISCONNECT

            If a packet insn't in the mask, it's a protocol error.
            Notice that for running states (not the connect/auth/disconnect), you have to deal with
            spurious PUBLISH packets.

            Dealing with those packet can be as simple as storing the work to perform later on, and that's what this
            method does.
            @return ErrorType::Success upon success (a packet for the current state is received)
                    ErrorType::TranscientPacket upon receiving a OOB publish-like packet (or QoS stuff). In that case, it's a good idea to restart the receive loop and recall the method
                    any other error upon this error
            */
        ErrorType dealWithNoise()
        {
            Protocol::MQTT::V5::ControlPacketType type = getLastPacketType();
            uint16 typeMask = bit(type);

            if (type == Protocol::MQTT::V5::DISCONNECT)
            {   // Disconnect is a special packet that can happens at any state
                Protocol::MQTT::V5::RODisconnectPacket packet;
                Protocol::MQTT::V5::ReasonCodes reason = Protocol::MQTT::V5::NormalDisconnection;
                int ret = extractControlPacket(type, packet);
                if (ret > 0) reason = packet.fixedVariableHeader.reason();
                close(reason);
                return ErrorType::NotConnected; // No work to perform upon server sending disconnect
            }

            // Check for unexpected packet
            if (State::expectedPacketMask[state] & typeMask == 0)
                return ErrorType::NetworkError;

            // Handle publish packets if needed
            if (typeMask & State::publishMask)
            {
                uint16 packetID = 0;
                Protocol::MQTT::V5::ControlPacketType next = Protocol::MQTT::V5::RESERVED;
                if (type == Protocol::MQTT::V5::PUBLISH)
                {
                    Protocol::MQTT::V5::ROPublishPacket packet;
                    int ret = extractControlPacket(type, packet);
                    if (ret == 0) { close(); return ErrorType::NotConnected; }
                    if (ret < 0) return ErrorType::NetworkError;
                    // Call the user as soon as possible to limit latency
                    // Notice that the user might be PUBLISH'ing here
                    cb->messageReceived(packet.fixedVariableHeader.topicName, Protocol::MQTT::Common::DynamicBinDataView(packet.payload.size, packet.payload.data), packet.fixedVariableHeader.packetID, packet.props);
                    // Save the ID if QoS
                    uint8 QoS = packet.header.getQoS();
                    if (QoS == 0)
                    {
                        // Done with this packet
                        resetPacketReceivingState();
                        return ErrorType::TranscientPacket;
                    }
#if MQTTQoSSupportLevel == -1
                    return ErrorType::NetworkError;
#else

                    packetID = packet.fixedVariableHeader.packetID;

                    bool store = (QoS == 1) ? buffers.storeQoS1ID(packetID | 0x10000) : buffers.storeQoS2ID(packetID | 0x10000);
                    if (!store) return ErrorType::StorageError;
                    next = (QoS == 1) ? Protocol::MQTT::V5::ControlPacketType::PUBACK : Protocol::MQTT::V5::ControlPacketType::PUBREC;
#endif
                } else
                {
#if MQTTQoSSupportLevel == -1
                    return ErrorType::NetworkError;
#else
                    Protocol::MQTT::V5::PublishReplyPacket reply(type);
                    int ret = extractControlPacket(type, reply);
                    if (ret <= 0) return ErrorType::NetworkError;

                    packetID = reply.fixedVariableHeader.packetID;
#if MQTTQoSSupportLevel == 1
                    if (typeMask & State::releaseBufferMask)
                    {
                        if (!storage->releasePacketBuffer(packetID)) // They always come from us
                            return ErrorType::StorageError;
                    }
#endif
                    if (typeMask & State::releaseIDMask)
                    {
                        if (!buffers.releaseID(packetID)) // They always come from us
                            return ErrorType::StorageError;
                    } else
                    {   // We need to reply to the broker (for example: PUBREL or PUBREC)
                        next = Protocol::MQTT::Common::Helper::getNextPacketType(type);
                    }
#endif
                }

#if MQTTQoSSupportLevel == -1
#else
                if (next != Protocol::MQTT::V5::RESERVED)
                {   // We need to reply to the broker
                    // Send the answer
                    Protocol::MQTT::V5::PublishReplyPacket answer(next);
                    answer.fixedVariableHeader.packetID = packetID;
                    next = Protocol::MQTT::Common::Helper::getNextPacketType(next);
                    if (ErrorType err = prepareSAR(answer, false))
                        return err;

                    // Check we need to advance the QoS2 processing now
                    if (type == Protocol::MQTT::V5::PUBREC && !buffers.avanceQoS2(packetID))
                        return ErrorType::StorageError;
                    // Or remove the ID for if there's no next packet to send (ACK or REL)
                    else if (next == Protocol::MQTT::V5::RESERVED && !buffers.releaseID(packetID | 0x10000))
                        return ErrorType::StorageError;
                }

                resetPacketReceivingState();
                return ErrorType::TranscientPacket;
#endif
            }
            // Ok, done
            return ErrorType::Success;
        }

#if MQTTUseAuth == 1
        ErrorType handleAuth()
        {
            Protocol::MQTT::V5::ROAuthPacket packet;
            int ret = extractControlPacket(Protocol::MQTT::V5::AUTH, packet);
            if (ret > 0)
            {
                // Parse the Auth packet and call the user method
                // Try to find the auth method, and the auth data
                DynamicStringView authMethod;
                DynamicBinDataView authData;
                Protocol::MQTT::V5::VisitorVariant visitor;
                while (packet.props.getProperty(visitor) && (authMethod.length == 0 || authData.length == 0))
                {
                    if (visitor.propertyType() == Protocol::MQTT::V5::AuthenticationMethod)
                    {
                        auto view = visitor.as< DynamicStringView >();
                        authMethod = *view;
                    }
                    else if (visitor.propertyType() == Protocol::MQTT::V5::AuthenticationData)
                    {
                        auto data = visitor.as< DynamicBinDataView >();
                        authData = *data;
                    }
                }
                return cb->authReceived(packet.fixedVariableHeader.reason(), authMethod, authData, packet.props) ? MQTTv5::ErrorType::Success : MQTTv5::ErrorType::NetworkError;
            }
            return ErrorType::NetworkError;
        }
#endif
        ErrorType handleConnACK()
        {
            // Parse the ConnACK packet;
            Protocol::MQTT::V5::ROConnACKPacket packet;
            int ret = extractControlPacket(Protocol::MQTT::V5::CONNACK, packet);
            if (ret > 0)
            {
                // We are only interested in the result of the connection
                if (packet.fixedVariableHeader.acknowledgeFlag & 1)
                {   // Session is present on the server. For now, we don't care, do we ?

                }
                if (packet.fixedVariableHeader.reasonCode != 0
#if MQTTUseAuth == 1
                    && packet.fixedVariableHeader.reasonCode != Protocol::MQTT::V5::NotAuthorized
                    && packet.fixedVariableHeader.reasonCode != Protocol::MQTT::V5::BadAuthenticationMethod
#endif
                )
                {
                    // We have failed connection with the following reason:
                    return (MQTTv5::ReasonCodes)packet.fixedVariableHeader.reasonCode;
                }
                // Now, we are going to parse the other properties
#if MQTTUseAuth == 1
                DynamicStringView authMethod;
                DynamicBinDataView authData;
#endif
                Protocol::MQTT::V5::VisitorVariant visitor;
                while (packet.props.getProperty(visitor))
                {
                    switch (visitor.propertyType())
                    {
                    case Protocol::MQTT::V5::PacketSizeMax:
                    {
                        auto pod = visitor.as< Protocol::MQTT::V5::LittleEndianPODVisitor<uint32> >();
                        maxPacketSize = pod->getValue();
                        break;
                    }
                    case Protocol::MQTT::V5::AssignedClientID:
                    {
                        auto view = visitor.as< Protocol::MQTT::V5::DynamicStringView >();
                        clientID.from(view->data, view->length); // This allocates memory for holding the copy
                        break;
                    }
                    case Protocol::MQTT::V5::ServerKeepAlive:
                    {
                        auto pod = visitor.as< Protocol::MQTT::V5::LittleEndianPODVisitor<uint16> >();
                        keepAlive = (pod->getValue() + (pod->getValue()>>1)) >> 1; // Use 0.75 of the server's told value
                        break;
                    }
#if MQTTUseAuth == 1
                    case Protocol::MQTT::V5::AuthenticationMethod:
                    {
                        auto view = visitor.as<DynamicStringView>();
                        authMethod = *view;
                    } break;
                    case Protocol::MQTT::V5::AuthenticationData:
                    {
                        auto data = visitor.as<DynamicBinDataView>();
                        authData = *data;
                    } break;
#endif
                    // Actually, we don't care about other properties. Maybe we should ?
                    default: break;
                    }
                }
#if MQTTUseAuth == 1
                if (packet.fixedVariableHeader.reasonCode == Protocol::MQTT::V5::NotAuthorized
                 || packet.fixedVariableHeader.reasonCode == Protocol::MQTT::V5::BadAuthenticationMethod)
                {   // Let the user be aware of the required authentication properties so next connect will/can contains them
                    cb->authReceived((ReasonCodes)packet.fixedVariableHeader.reasonCode, authMethod, authData, packet.props);
                    return ErrorType::NetworkError; // Force close the connection as per 4.12.0-1
                }
#endif
                // Ok, the connection was accepted (and authentication cleared).
                state = State::Running;
#if MQTTQoSSupportLevel == 1
                // Check if we need to resend some unACK'ed packets
                uint8 resend = buffers.countSentID();
                if (resend)
                {
                    // Loop over the buffers and resend them
                    uint8 i = 0;
                    while (true)
                    {
                        uint32 packetID = buffers.packetID(i);
                        if (buffers.isSending(packetID))
                        {
                            if (buffers.isQoS2Step2(packetID))
                            {
                                // We've already received the PUBREC packet so ownership is on the broker.
                                // We need to resend the PUBREL packet here
                                Protocol::MQTT::V5::PublishReplyPacket answer(Protocol::MQTT::V5::PUBREL);
                                answer.fixedVariableHeader.packetID = (uint16)(packetID & 0xFFFF);
                                if (ErrorType err = prepareSAR(answer, true))
                                    return err;
                            } else
                            {
                                // No PUBACK or no PUBREC received, we need to resend the packet
                                uint16 id = packetID & 0xFFFF;
                                const uint8 * packetH = 0, * packetT = 0; uint32 sizeH = 0, sizeT = 0;
                                if (!storage->loadPacketBuffer(id, packetH, sizeH, packetT, sizeT))
                                    return ErrorType::StorageError;

                                // Send the packet and run the event loop to purge the acknowledgement.
                                ErrorType ret = ErrorType::Success;
                                if ((ret = sendAndReceive(packetH, sizeH, sizeT == 0)))
                                    return ret;

                                if (sizeT && (ret = sendAndReceive(packetT, sizeT, true)))
                                    return ret;
                            }
                            // As per 4.9 flow control, we can't send all other packet without processing the
                            // QoS dance. At this step of communication, we can't receive any PUBLISH packet since
                            // we haven't subscribed yet. We can only receive DISCONNECT & QoS packets here
                            if (ErrorType ret = dealWithNoise())
                                return ret;
                        }
                        if (i == buffers.end()) break;
                        i++; // This works because the buffers is a ring buffer, so when an ID is released in the dealWithNoise() above, the
                             // position of the next ID don't move. At this step, any new ID will be processed later on.
                    }
                }
#else
                buffers.reset();
#endif
                return ErrorType::Success;
            }
            return Protocol::MQTT::V5::ProtocolError;
        }
    };

#if MQTTOnlyBSDSocket != 1
    /*  The socket class we are using for socket operations.
        There's a default implementation for Berkeley socket and (Open)SSL socket in the ClassPath, but
        you can implement any library you want, like, for example, lwIP, so change this if you do */
    typedef Network::Socket::BerkeleySocket Socket;
  #if MQTTUseTLS == 1
    /*  The SSL socket we are using (when using SSL/TLS connection).
        There's a default implementation for (Open/Libre)SSL socket in ClassPath, but you can implement
        one class with, for example, MBEDTLS here if you want. Change this if you do */
    typedef Network::Socket::SSL_TLS        SSLSocket;
    /** The SSL context to (re)use. If you need to skip negotiating, you'll need to modify this context */
    typedef SSLSocket::SSLContext           SSLContext;
  #endif

  #if MQTTDumpCommunication == 1
    static void dumpBufferAsPacket(const char * prompt, const uint8* buffer, uint32 length)
    {
        // Dump the packet received
        String packetDump;
        Utils::hexDump(packetDump, buffer, length, 16, true, true);
        Protocol::MQTT::V5::FixedHeader header;
        header.raw = buffer[0];
        Logger::log(Logger::Dump, "%s: %s(R:%d,Q:%d,D:%d)%s", prompt, Protocol::MQTT::V5::Helper::getControlPacketName((Protocol::MQTT::Common::ControlPacketType)(uint8)header.type), (uint8)header.retain, (uint8)header.QoS, (uint8)header.dup, (const char*)packetDump);
    }
  #endif

    /** The scoped lock class we are using */
    typedef Threading::ScopedLock   ScopedLock;

    struct MQTTv5::Impl : public ImplBase<MQTTv5::Impl>
    {
        /** The multithread protection for this object */
        Threading::Lock         sendLock;
        /** This client socket */
        Socket *                socket;
        /** The default timeout in milliseconds */
        uint32                  timeoutMs;

        Impl(const char * clientID, MessageReceived * callback, const DynamicBinDataView * brokerCert, PacketStorage * storage)
             : ImplBase(clientID, callback, brokerCert, storage), socket(0), timeoutMs(3000) {}
        ~Impl() { delete0(socket); }

        Time::TimeOut getTimeout() const { return timeoutMs; }
        inline void setTimeout(uint32 timeout) { timeoutMs = timeout; }

        int recv(char* buf, int len, const Time::TimeOut & timeout)
        {
            return socket->receiveReliably(buf, len, timeout);
        }

        int sendImpl(const char * buffer, const uint32 length)
        {
            ScopedLock scope(sendLock);
            return socket->sendReliably(buffer, (int)length, timeoutMs);
        }

        int connectWith(const char * host, const uint16 port, const bool withTLS)
        {
            if (this->isOpen()) return -1;
            this->state = State::Connecting;
            if (withTLS)
            {
  #if MQTTUseTLS == 1
                if (!sslContext)
                {   // If one certificate is given let's use it instead of the default CA bundle
                    sslContext = brokerCert ? new SSLContext(NULL, Crypto::SSLContext::Any) : new SSLContext();
                }
                if (!sslContext) return -2;
                // Insert here any session specific configuration or certificate validator
                if (brokerCert)
                {
                    if (const char * error = sslContext->loadCertificateFromDER(brokerCert->data, brokerCert->length))
                    {
                        Logger::log(Logger::Error, "Could not load the given certificate: %s", error);
                        return -2;
                    }
                }
                socket = new Network::Socket::SSL_TLS(*sslContext, Network::Socket::BaseSocket::Stream);
  #else
                return -1;
  #endif
            } else socket = new Socket(Network::Socket::BaseSocket::Stream);

            if (!socket) return -2;
            // Let the socket be asynchronous and without Nagle's algorithm
            if (!socket->setOption(Network::Socket::BaseSocket::Blocking, 0)) return -3;
            if (!socket->setOption(Network::Socket::BaseSocket::NoDelay, 1)) return -4;
            if (!socket->setOption(Network::Socket::BaseSocket::NoSigPipe, 1)) return -5;
            // Then connect the socket to the server
            int ret = socket->connect(Network::Address::URL("", String(host) + ":" + port, ""));
            if (ret < 0) return -6;
            if (ret == 0) return 0;

            // Here, we need to wait until connection happens or times out
            if (socket->select(false, true, timeoutMs)) return 0;
            return -7;
        }
    };
#else
#ifndef MQTTLock
    /* If you have a true lock object in your system (for example, in FreeRTOS, use a mutex),
       you should provide one instead of this one as this one just burns CPU while waiting */
    class SpinLock
    {
        mutable std::atomic<bool> state;
    public:
        /** Construction */
        SpinLock() : state(false) {}
        /** Acquire the lock */
        inline void acquire() volatile
        {
            while (state.exchange(true, std::memory_order_acq_rel))
            {
                // Put a sleep method here (using select here since it's cross platform in BSD socket API)
                struct timeval tv;
                tv.tv_sec = 0; tv.tv_usec = 500; // Wait 0.5ms per loop
                select(0, NULL, NULL, NULL, &tv);
            }
        }
        /** Try to acquire the lock */
        inline bool tryAcquire() volatile { return state.exchange(true, std::memory_order_acq_rel) == false; }
        /** Check if the lock is taken. For debugging purpose only */
        inline bool isLocked() const volatile { return state.load(std::memory_order_consume); }
        /** Release the lock */
        inline void release() volatile { state.store(false, std::memory_order_release);; }
    };

    typedef SpinLock Lock;
    struct ScopedLock
    {
        Lock & a;
        ScopedLock(Lock & a) : a(a) { a.acquire(); }
        ~ScopedLock() { a.release(); }
    };
#else
    #define Lock        MQTTLock
    #define ScopedLock  MQTTScopedLock
#endif
#ifndef closesocket
    #define closesocket close
#endif

#ifndef MSG_NOSIGNAL
    #define MSG_NOSIGNAL 0
#endif

#if MQTTDumpCommunication == 1
    static void hexdump(const void * buf, int len)
    {
        for (int i = 0; i < len; i++)
            printf("%02X ", ((unsigned char*)buf)[i]);
        printf("\n"); // Flush output
    }
    static void dumpBufferAsPacket(const char * prompt, const uint8* buffer, uint32 length)
    {
        // Dump the packet to send
        Protocol::MQTT::V5::FixedHeader header;
        header.raw = buffer[0];
        printf("%s: %s(R:%d,Q:%d,D:%d)\n", prompt, Protocol::MQTT::V5::Helper::getControlPacketName((Protocol::MQTT::Common::ControlPacketType)(uint8)header.type), (uint8)header.retain, (uint8)header.QoS, (uint8)header.dup);
        hexdump(buffer, length);
    }
#endif

#if MQTTUseTLS == 1
    // Small optimization to remove useless virtual table in the final binary if not used
    #define MQTTVirtual virtual
#else
    #define MQTTVirtual
#endif

    struct BaseSocket
    {
        int     socket;
        struct timeval &         timeoutMs;

        MQTTVirtual int connect(const char * host, uint16 port, const MQTTv5::DynamicBinDataView *)
        {
            socket = ::socket(AF_INET, SOCK_STREAM, 0);
            if (socket == -1) return -2;

            // Please notice that under linux, it's not required to set the socket
            // as non blocking if you define SO_SNDTIMEO, for connect timeout.
            // so the code below could be optimized away. Yet, lwIP does show the
            // same behavior so when a timeout for connection is actually required
            // you must issue a select call here.

            // Set non blocking here
            int socketFlags = ::fcntl(socket, F_GETFL, 0);
            if (socketFlags == -1) return -3;
            if (::fcntl(socket, F_SETFL, (socketFlags | O_NONBLOCK)) != 0) return -3;

            // Let the socket be without Nagle's algorithm
            int flag = 1;
            if (::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) return -4;
            // Then connect the socket to the server
            struct addrinfo hints = {};
            hints.ai_family = AF_INET; // IPv4 only for now
            hints.ai_flags = AI_ADDRCONFIG;
            hints.ai_socktype = SOCK_STREAM;

            // Resolve address
            struct addrinfo *result = NULL;
            if (getaddrinfo(host, NULL, &hints, &result) < 0 || result == NULL) return -5;

            // Then connect to it
            struct sockaddr_in address;
            address.sin_port = htons(port);
            address.sin_family = AF_INET;
            address.sin_addr = ((struct sockaddr_in *)(result->ai_addr))->sin_addr;

            // free result
            freeaddrinfo(result);
            int ret = ::connect(socket, (const sockaddr*)&address, sizeof(address));
            if (ret < 0 && errno != EINPROGRESS) return -6;
            if (ret == 0) return 0;

            // Here, we need to wait until connection happens or times out
            if (select(false, true))
            {
                // Restore blocking behavior here
                if (::fcntl(socket, F_SETFL, socketFlags) != 0) return -3;
                // And set timeouts for both recv and send
                if (::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeoutMs, sizeof(timeoutMs)) < 0) return -4;
                if (::setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeoutMs, sizeof(timeoutMs)) < 0) return -4;
                // Ok, done!
                return 0;
            }

            return -7;
        }
        MQTTVirtual int recv(char * buffer, const uint32 minLength, const uint32 maxLength = 0)
        {
            int ret = ::recv(socket, buffer, minLength, MSG_WAITALL);
            if (ret <= 0) return ret;
            if (maxLength <= minLength) return ret;

            int nret = ::recv(socket, &buffer[ret], maxLength - ret, 0);
            return nret <= 0 ? nret : nret + ret;
        }

        MQTTVirtual int send(const char * buffer, const uint32 length)
        {
#if MQTTDumpCommunication == 1
            dumpBufferAsPacket("> Sending packet", (const uint8*)buffer, length);
#endif
            return ::send(socket, buffer, (int)length, 0);
        }

        // Useful socket helpers functions here
        MQTTVirtual int select(bool reading, bool writing, const uint32 timeoutMillis = (uint32)-1)
        {
            // Linux modifies the timeout when calling select
            struct timeval v;
            if (timeoutMillis == (uint32)-1) v = timeoutMs;
            else { v.tv_sec = timeoutMillis / 1000; v.tv_usec = (timeoutMillis % 1000) * 1000; }

            fd_set set;
            FD_ZERO(&set);
            FD_SET(socket, &set);
            // Then select
            return ::select(socket + 1, reading ? &set : NULL, writing ? &set : NULL, NULL, &v);
        }

        BaseSocket(struct timeval & timeoutMs) : socket(-1), timeoutMs(timeoutMs) {}
        MQTTVirtual ~BaseSocket() { ::closesocket(socket); socket = -1; }
    };


#if MQTTUseTLS == 1
    class MBTLSSocket : public BaseSocket
    {
        mbedtls_entropy_context entropy;
        mbedtls_ctr_drbg_context entropySource;
        mbedtls_ssl_context ssl;
        mbedtls_ssl_config conf;
        mbedtls_x509_crt cacert;
        mbedtls_net_context net;

    private:
        bool buildConf(const MQTTv5::DynamicBinDataView * brokerCert)
        {
            if (brokerCert)
            {   // Use given root certificate (if you have a recent version of mbedtls, you could use mbedtls_x509_crt_parse_der_nocopy instead to skip a useless copy here)
                if (::mbedtls_x509_crt_parse_der(&cacert, brokerCert->data, brokerCert->length))
                    return false;
            }

            // Now create configuration from default
            if (::mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT))
                return false;

            ::mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
            ::mbedtls_ssl_conf_authmode(&conf, brokerCert ? MBEDTLS_SSL_VERIFY_REQUIRED : MBEDTLS_SSL_VERIFY_NONE);

            uint32_t ms = timeoutMs.tv_usec / 1000;
            ::mbedtls_ssl_conf_read_timeout(&conf, ms < 50 ? 3000 : ms);

            // Random number generator
            ::mbedtls_ssl_conf_rng(&conf, ::mbedtls_ctr_drbg_random, &entropySource);
            if (::mbedtls_ctr_drbg_seed(&entropySource, ::mbedtls_entropy_func, &entropy, NULL, 0))
                return false;

            if (::mbedtls_ssl_setup(&ssl, &conf))
                return false;

            return true;
        }

    public:
        MBTLSSocket(struct timeval & timeoutMs) : BaseSocket(timeoutMs)
        {
            mbedtls_ssl_init(&ssl);
            mbedtls_ssl_config_init(&conf);
            mbedtls_x509_crt_init(&cacert);
            mbedtls_ctr_drbg_init(&entropySource);
            mbedtls_entropy_init(&entropy);
        }

        int connect(const char * host, uint16 port, const MQTTv5::DynamicBinDataView * brokerCert)
        {
            int ret = BaseSocket::connect(host, port, 0);
            if (ret) return ret;

            // MBedTLS doesn't deal with natural socket timeout correctly, so let's fix that
            struct timeval zeroTO = {};
            if (::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &zeroTO, sizeof(zeroTO)) < 0) return -4;
            if (::setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &zeroTO, sizeof(zeroTO)) < 0) return -4;

            net.fd = socket;

            if (!buildConf(brokerCert))                                             return -8;
            if (::mbedtls_ssl_set_hostname(&ssl, host))                             return -9;

            // Set the method the SSL engine is using to fetch/send data to the other side
            ::mbedtls_ssl_set_bio(&ssl, &net, ::mbedtls_net_send, NULL, ::mbedtls_net_recv_timeout);

            ret = ::mbedtls_ssl_handshake(&ssl);
            if (ret != 0 && ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
                return -10;

            // Check certificate if one provided
            if (brokerCert)
            {
                uint32_t flags = mbedtls_ssl_get_verify_result(&ssl);
                if (flags != 0)
                {
#if MQTTDumpCommunication == 1
                    char verify_buf[100] = {0};
                    mbedtls_x509_crt_verify_info(verify_buf, sizeof(verify_buf), "  ! ", flags);
                    printf("mbedtls_ssl_get_verify_result: %s flag: 0x%x\n", verify_buf, (unsigned int)flags);
#endif
                    return -11;
                }
            }
            return 0;
        }

        int send(const char * buffer, const uint32 length)
        {
#if MQTTDumpCommunication == 1
            dumpBufferAsPacket("> Sending packet", (const uint8*)buffer, length);
#endif
            return ::mbedtls_ssl_write(&ssl, (const uint8*)buffer, length);
        }

        int recv(char * buffer, const uint32 minLength, const uint32 maxLength = 0)
        {
            uint32 ret = 0;
            while (ret < minLength)
            {
                int r = ::mbedtls_ssl_read(&ssl, (uint8*)&buffer[ret], minLength - ret);
                if (r <= 0)
                {
                    // Those means that we need to call again the read method
                    if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE)
                        continue;
                    if (r == MBEDTLS_ERR_SSL_TIMEOUT) {
                        errno = EWOULDBLOCK; // Remember it's a timeout
                        return -1;
                    }
                    return ret ? (int)ret : r; // Silent error here
                }
                ret += (uint32)r;
            }
            if (maxLength <= minLength) return ret;

            // This one is a non blocking call
            int nret = ::mbedtls_ssl_read(&ssl, (uint8*)&buffer[ret], maxLength - ret);
            if (nret == MBEDTLS_ERR_SSL_TIMEOUT) return ret;
            return nret <= 0 ? nret : nret + ret;
        }

        ~MBTLSSocket()
        {
            mbedtls_ssl_close_notify(&ssl);
            mbedtls_x509_crt_free(&cacert);
            mbedtls_entropy_free(&entropy);
            mbedtls_ssl_config_free(&conf);
            mbedtls_ctr_drbg_free(&entropySource);
            mbedtls_ssl_free(&ssl);
        }
    };
#endif

    struct MQTTv5::Impl : public ImplBase<MQTTv5::Impl>
    {
        /** The multithread protection for this object */
        Lock                        sendLock;
        /** This client socket */
        BaseSocket *                socket;
                /** The default timeout in milliseconds */
        struct timeval              timeoutMs;

        Impl(const char * clientID, MessageReceived * callback, const DynamicBinDataView * brokerCert, PacketStorage * storage)
             : ImplBase(clientID, callback, brokerCert, storage), socket(0), timeoutMs({3, 0}) {}
        ~Impl() { delete0(socket); }

        uint32 getTimeout() const { return 0; }
        inline void setTimeout(uint32 timeout)
        {
            timeoutMs.tv_sec = (uint32)timeout / 1024; // Avoid division here (compiler should shift the value here), the value is approximative anyway
            timeoutMs.tv_usec = ((uint32)timeout & 1023) * 977;  // Avoid modulo here and make sure it doesn't overflow (since 1023 * 977 < 1000000)
        }

        int recv(char* buf, int len, uint32 & timeout)
        {
            int ret = socket->recv(buf, len);
            // Deal with timeout first
            if (ret < 0 &&errno == EWOULDBLOCK) timeout = 0;
            return ret;
        }
        int connectWith(const char * host, const uint16 port, const bool withTLS)
        {
            if (this->isOpen()) return -1;
            this->state = State::Connecting;
            socket =
#if MQTTUseTLS == 1
                withTLS ? new MBTLSSocket(timeoutMs) :
#endif
                new BaseSocket(timeoutMs);
            return socket ? socket->connect(host, port, brokerCert) : -1;
        }

        int sendImpl(const char * buffer, const int size)
        {
            ScopedLock scope(sendLock);
            return socket ? socket->send(buffer, size) : -1;
        }
    };
#endif

    MQTTv5::MQTTv5(const char * clientID, MessageReceived * callback, const DynamicBinDataView * brokerCert, PacketStorage * storage) : impl(new Impl(clientID, callback, brokerCert, storage)) {}
    MQTTv5::~MQTTv5() { delete0(impl); }



    // Connect to the given server URL.
    MQTTv5::ErrorType MQTTv5::connectTo(const char * serverHost, const uint16 port, bool useTLS, const uint16 keepAliveTimeInSec,
        const bool cleanStart, const char * userName, const DynamicBinDataView * password, WillMessage * willMessage, const QoSDelivery willQoS, const bool willRetain,
        Properties * properties)
    {
        if (serverHost == nullptr || !port)
            return ErrorType::BadParameter;

        // Please do not move the line below as it must outlive the packet
        Protocol::MQTT::V5::Property<uint32> maxProp(Protocol::MQTT::V5::PacketSizeMax, impl->buffers.size);
        Protocol::MQTT::V5::Property<uint16> maxRecv(Protocol::MQTT::V5::ReceiveMax, impl->buffers.packetsCount());
        Protocol::MQTT::V5::ControlPacket<Protocol::MQTT::V5::CONNECT> packet;

        if (impl->isOpen()) return ErrorType::AlreadyConnected;

        // Capture properties (to avoid copying them)
        packet.props.capture(properties);

        // Check if we have a max packet size property and if not, append one to let the server know our limitation (if any)
        if (impl->buffers.size < Protocol::MQTT::Common::VBInt::MaxPossibleSize)
            packet.props.append(&maxProp); // It'll fail silently if it already exists
        if (impl->buffers.packetsCount())
            packet.props.append(&maxRecv); // It'll fail silently if it already exists


#if MQTTAvoidValidation != 1
        if (!packet.props.checkPropertiesFor(Protocol::MQTT::V5::CONNECT))
            return ErrorType::BadProperties;
#endif

        // Check we can contact the server and connect to it (not initial write to the server is required here)
        if (int ret = impl->connectWith(serverHost, port, useTLS))
        {
            impl->close();
            return ret == -7 ? ErrorType::TimedOut : ErrorType::NetworkError;
        }

        // Create the header object now
        impl->keepAlive = (keepAliveTimeInSec + (keepAliveTimeInSec / 2)) / 2; // Make it 75% of what's given so we always wake up before doom's clock
        packet.fixedVariableHeader.keepAlive = keepAliveTimeInSec;
        packet.fixedVariableHeader.cleanStart = cleanStart ? 1 : 0;
        packet.fixedVariableHeader.willFlag = willMessage != nullptr ? 1 : 0;
        packet.fixedVariableHeader.willQoS = (uint8)willQoS;
        packet.fixedVariableHeader.willRetain = willRetain ? 1 : 0;
        packet.fixedVariableHeader.passwordFlag = password != nullptr ? 1 : 0;
        packet.fixedVariableHeader.usernameFlag = userName != nullptr ? 1 : 0;

        // And the payload too now
        packet.payload.clientID = impl->clientID;
        if (willMessage != nullptr) packet.payload.willMessage = willMessage;
        if (userName != nullptr)    packet.payload.username = userName;
        if (password != nullptr)    packet.payload.password = *password;

        // Ok, setting are done, let's build this packet now
        if (ErrorType ret = impl->prepareSAR(packet))
        {
            impl->close();
            return ret;
        }

        // Then extract the packet type
        Protocol::MQTT::V5::ControlPacketType type = impl->getLastPacketType();
        if (type == Protocol::MQTT::V5::CONNACK)
        {
            ErrorType ret = impl->handleConnACK();

            if (ret != ErrorType::Success)
                impl->close();
            return ret;
        }
#if MQTTUseAuth == 1
        else if (type == Protocol::MQTT::V5::AUTH)
        {
            // Authentication need to know if we are in a CONNECT/CONNACK process since it behaves differently in that case
            impl->setConnectionState(State::Authenticating);

            if (impl->handleAuth() == ErrorType::Success)
            { // We need to receive either a CONNACK or a AUTH packet now, so let's do that until we're done
                while (true)
                {
                    // Ok, now we have a packet read it
                    type = impl->getLastPacketType();
                    if (type == Protocol::MQTT::V5::CONNACK)
                    {
                        ErrorType ret = impl->handleConnACK();
                        if (ret != ErrorType::Success)
                            impl->close();

                        return ret;
                    }
                    else if (type == Protocol::MQTT::V5::AUTH)
                    {
                        if (ErrorType ret = impl->handleAuth())
                        {   // In case of authentication error, let's report back up
                            impl->close();
                            return ret;
                        }   // Else, let's continue the Authentication dance
                    }
                    else
                    {
                        impl->close();
                        return Protocol::MQTT::V5::ProtocolError;
                    }
                }
            }
        }
#endif
        // All other cases are error, let's close the connection now.
        impl->close();
        return Protocol::MQTT::V5::ProtocolError;
    }

#if MQTTUseAuth == 1
    // Authenticate with the given server.
    MQTTv5::ErrorType MQTTv5::auth(const ReasonCodes reasonCode, const DynamicStringView & authMethod, const DynamicBinDataView & authData, Properties * properties)
    {
        if (reasonCode != Protocol::MQTT::V5::Success && reasonCode != Protocol::MQTT::V5::ContinueAuthentication && reasonCode != Protocol::MQTT::V5::ReAuthenticate)
            return ErrorType::BadParameter;
        if (!properties && (authMethod.length == 0 || authData.length == 0))
            return ErrorType::BadParameter; // A auth method is required

        // Don't move this around, it must appear on stack until it's no more used
        Protocol::MQTT::V5::Property<DynamicStringView> method(Protocol::MQTT::V5::AuthenticationMethod, authMethod);
        Protocol::MQTT::V5::Property<DynamicBinDataView> data(Protocol::MQTT::V5::AuthenticationData, authData);

        Protocol::MQTT::V5::AuthPacket packet;

        if (!impl->isOpen()) return ErrorType::NotConnected;
        if (impl->getLastPacketType() != Protocol::MQTT::V5::RESERVED)
            return ErrorType::TranscientPacket;

        // Capture properties (to avoid copying them)
        packet.props.capture(properties);

        // Check if we have a auth method
        packet.props.append(&method); // That'll fail silently if it already exists
        packet.props.append(&data); // That'll fail silently if it already exists

        packet.fixedVariableHeader.reasonCode = reasonCode;

        // Then send the packet
        if (ErrorType ret = impl->prepareSAR(packet))
            return ret;

        Protocol::MQTT::V5::ControlPacketType type = impl->getLastPacketType();
        if (type == Protocol::MQTT::V5::AUTH)
        {
            ErrorType ret = impl->handleAuth();
            return ret;
        } else if (type == Protocol::MQTT::V5::CONNACK && impl->state == State::Authenticating)
        {   // We don't signal any error here, it's up to the parent's connectTo to check this packet
            // since this method will be called from the authReceived callback
            return ErrorType::Success;
        }
        return Protocol::MQTT::V5::ProtocolError;
    }
#endif

    // Subscribe to a topic.
    MQTTv5::ErrorType MQTTv5::subscribe(const char * _topic, const RetainHandling retainHandling, const bool withAutoFeedBack, const QoSDelivery maxAcceptedQoS, const bool retainAsPublished, Properties * properties)
    {
        if (_topic == nullptr)
            return ErrorType::BadParameter;

        // Create the subscribe topic here
#if MQTTQoSSupportLevel == -1
        // Don't support anything above QoS 0
        Protocol::MQTT::V5::SubscribeTopic topic(_topic, retainHandling, retainAsPublished, !withAutoFeedBack, QoSDelivery::AtMostOne, true);
#else
        Protocol::MQTT::V5::SubscribeTopic topic(_topic, retainHandling, retainAsPublished, !withAutoFeedBack, maxAcceptedQoS, true);
#endif
        // Then proceed to subscribing
        return subscribe(topic, properties);
    }

    MQTTv5::ErrorType MQTTv5::subscribe(SubscribeTopic & topics, Properties * properties)
    {
        if (!impl->isOpen()) return ErrorType::NotConnected;
        // If we are interrupting while receiving a packet, let's stop before make any more damage
        if (impl->state != State::Running)
            return ErrorType::TranscientPacket;

        Protocol::MQTT::V5::ControlPacket<Protocol::MQTT::V5::SUBSCRIBE> packet;
        // Capture properties (to avoid copying them)
        packet.props.capture(properties);

#if MQTTAvoidValidation != 1
        if (!packet.props.checkPropertiesFor(Protocol::MQTT::V5::SUBSCRIBE))
            return ErrorType::BadProperties;
#endif


        packet.fixedVariableHeader.packetID = impl->allocatePacketID();
        packet.payload.topics = &topics;
        impl->setConnectionState(State::Subscribing);

        // Then send the packet
        if (ErrorType ret = impl->requestOneLoop(packet))
            return ret;

        // Then extract the packet type
        Protocol::MQTT::V5::ControlPacketType type = impl->getLastPacketType();
        if (type == Protocol::MQTT::V5::SUBACK)
        {
            Protocol::MQTT::V5::ROSubACKPacket rpacket;
            int ret = impl->extractControlPacket(type, rpacket);
            if (ret <= 0) return ErrorType::NetworkError;

            if (rpacket.fixedVariableHeader.packetID != packet.fixedVariableHeader.packetID)
                return ErrorType::NetworkError;

            // Then check reason codes
            SubscribeTopic * topic = packet.payload.topics;
            uint32 count = topic->count();
            if (!rpacket.payload.data || rpacket.payload.size < count)
                return ReasonCodes::ProtocolError;
            for (uint32 i = 0; i < count; i++)
                if (rpacket.payload.data[i] >= ReasonCodes::UnspecifiedError)
                    return (ReasonCodes)rpacket.payload.data[i];

            return ErrorType::Success;
        }

        return MQTTv5::ErrorType::NetworkError;
    }

#if MQTTUseUnsubscribe == 1
    MQTTv5::ErrorType MQTTv5::unsubscribe(UnsubscribeTopic & topics, Properties * properties)
    {
        if (!impl->isOpen()) return ErrorType::NotConnected;
        // If we are interrupting while receiving a packet, let's stop before make any more damage
        if (impl->state != State::Running)
            return ErrorType::TranscientPacket;

        // If we are currently unsubscribing and still waiting for ACK, don't accept any other unsubscription
        if (impl->unsubscribeId)
            return ErrorType::TranscientPacket;

        Protocol::MQTT::V5::ControlPacket<Protocol::MQTT::V5::UNSUBSCRIBE> packet;
        // Capture properties (to avoid copying them)
        packet.props.capture(properties);

#if MQTTAvoidValidation != 1
        if (!packet.props.checkPropertiesFor(Protocol::MQTT::V5::UNSUBSCRIBE))
            return ErrorType::BadProperties;
#endif

        packet.fixedVariableHeader.packetID = impl->allocatePacketID();
//        impl->unsubscribeId = packet.fixedVariableHeader.packetID;
        packet.payload.topics = &topics;

        impl->setConnectionState(State::Unsubscribing);

        // Then send the packet
        if (ErrorType ret = impl->requestOneLoop(packet))
            return ret;

        // Then extract the packet type
        Protocol::MQTT::V5::ControlPacketType type = impl->getLastPacketType();
        if (type == Protocol::MQTT::V5::UNSUBACK)
        {
            Protocol::MQTT::V5::ROUnsubACKPacket rpacket;
            int ret = impl->extractControlPacket(type, rpacket);
            if (ret <= 0) return ErrorType::NetworkError;

            if (rpacket.fixedVariableHeader.packetID != packet.fixedVariableHeader.packetID)
                return ErrorType::NetworkError;

            // Then check reason codes
            uint32 count = rpacket.payload.size;
            if (!rpacket.payload.data)
                return ReasonCodes::ProtocolError;
            for (uint32 i = 0; i < count; i++)
                if (rpacket.payload.data[i] >= ReasonCodes::UnspecifiedError)
                    return (ErrorType::Type)rpacket.payload.data[i];

            return ErrorType::Success;
        }

        return MQTTv5::ErrorType::NetworkError;
    }
#endif

    // Publish to a topic.
    MQTTv5::ErrorType MQTTv5::publish(const char * topic, const uint8 * payload, const uint32 payloadLength, const bool retain, const QoSDelivery QoS, const uint16 packetIdentifier, Properties * properties)
    {
        if (topic == nullptr)
            return ErrorType::BadParameter;

        if (!impl->isOpen()) return ErrorType::NotConnected;
        if (impl->state != State::Running) return ErrorType::TranscientPacket;

        Protocol::MQTT::V5::PublishPacket packet;
        // Capture properties (to avoid copying them)
        packet.props.capture(properties);

#if MQTTAvoidValidation != 1
        if (!packet.props.checkPropertiesFor(Protocol::MQTT::V5::PUBLISH))
            return ErrorType::BadProperties;
#endif

        // Create header now
        packet.header.setRetain(retain);
#if MQTTQoSSupportLevel == -1
        const bool withAnswer = false;
        packet.header.setQoS((uint8)QoSDelivery::AtMostOne);
#else
        bool withAnswer = QoS != QoSDelivery::AtMostOne;
        packet.header.setQoS((uint8)QoS);
#endif
        packet.header.setDup(false); // At first, it's not a duplicate message
        packet.fixedVariableHeader.packetID = withAnswer ? impl->allocatePacketID() : 0; // Only if QoS is not 0
        packet.fixedVariableHeader.topicName = topic;
        packet.payload.setExpectedPacketSize(payloadLength);
        packet.payload.readFrom(payload, payloadLength);

        // The publish cycle isn't run until the next event loop. This allow true asynchronous publishing
        return impl->prepareSAR(packet, false, true);
    }

    // The client event loop you must call regularly.
    MQTTv5::ErrorType MQTTv5::eventLoop()
    {
        if (!impl->isOpen()) return ErrorType::NotConnected;

        // Check if we have a packet ready for reading now
        Protocol::MQTT::Common::ControlPacketType type = impl->getLastPacketType();
        if (type == Protocol::MQTT::V5::RESERVED)
        {
            // Check if we need to ping the server
            if (impl->shouldPing())
            {
                // Create a Ping request packet and send it
                impl->setConnectionState(State::Pinging);
                Protocol::MQTT::V5::PingReqPacket packet;
                if (ErrorType ret = impl->requestOneLoop(packet))
                    return ret;

                type = impl->getLastPacketType();
                if (type == Protocol::MQTT::V5::PINGRESP)
                {
                    Protocol::MQTT::V5::PingRespPacket rpacket;
                    int ret = impl->extractControlPacket(type, rpacket);
                    if (ret <= 0) return ErrorType::NetworkError;
                }
                // Ok, done for now
                return ErrorType::Success;
            }
            // Check the server for any packet...
            int ret = impl->receiveControlPacket(true);
            if (ret == 0)
            {
                impl->close();
                return ErrorType::NotConnected;
            }
            // No answer in time, it's not an error here
            if (ret == -2) return ErrorType::Success;
            if (ret < 0)   return ErrorType::NetworkError;
            // Ok, now we have a packet read it
            type = impl->getLastPacketType();
        }

        ErrorType ret = impl->dealWithNoise();
        if (ret == ErrorType::TranscientPacket)
            return ErrorType::Success;

        return ret;
    }

    // Disconnect from the server
    MQTTv5::ErrorType MQTTv5::disconnect(const ReasonCodes code, Properties * properties)
    {
        if (code != ReasonCodes::NormalDisconnection && code != ReasonCodes::DisconnectWithWillMessage && code < ReasonCodes::UnspecifiedError)
            return ErrorType::BadParameter;


        if (!impl->isOpen()) return ErrorType::Success;

        Protocol::MQTT::V5::ControlPacket<Protocol::MQTT::V5::DISCONNECT> packet;
        packet.fixedVariableHeader.reasonCode = code;
        // Capture properties (to avoid copying them)
        packet.props.capture(properties);

#if MQTTAvoidValidation != 1
        if (!packet.props.checkPropertiesFor(Protocol::MQTT::V5::DISCONNECT))
            return ErrorType::BadProperties;
#endif

        impl->setConnectionState(State::Disconnecting);
        if (ErrorType ret = impl->prepareSAR(packet, false))
            return ret;

        // There is no need to wait for ACK for a disconnect
        impl->close();
        return ErrorType::Success;
    }

    void MQTTv5::setDefaultTimeout(const uint32 timeoutMs)
    {
        impl->setTimeout(timeoutMs);
    }

}}

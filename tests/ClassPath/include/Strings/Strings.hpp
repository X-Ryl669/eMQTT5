#ifndef hpp_Strings_hpp
#define hpp_Strings_hpp

// We need types
#include "Types.hpp"
// We need Platform
#include "Platform/Platform.hpp"
// We need BString
#include "bstring.hpp"
// We need very simple read only strings too
#include "VerySimpleReadOnlyString.hpp"

/** UTF-8 strings and UCS-2 strings.
    For UTF-8, use FastString (from Bstrlib::String)
    For UCS-2, use Strings::ReadOnlyUnicodeString

    You can convert from on type to the other by calling Strings::convert function.
*/
namespace Strings
{
    // Forward declare the findLength function
    inline const unsigned int findLength(const uint8 * txt, const size_t limit = 0) { return findLength((tCharPtr)txt, limit); }

    /** Convert VerySimpleReadOnlyString to a fast string */
    inline FastString   convert(const VerySimpleReadOnlyString & string) { return FastString((const void*)string.getData(), string.getLength()); }

    /** Well, the name says it all, this is a very simple read only Unicode string.
        String data is heap allocated, and the copy constructor is a move-able constructor

        @warning The copy constructor doesn't copy the heap, but move the allocated heap space to the copied object.
        @warning If you need to actually copy the heap space, use the Clone() method. */
    class ReadOnlyUnicodeString
    {
    public:
        /** The char type */
        typedef wchar_t CharType;
        /** The char pointer type */
        typedef const CharType * const   CharPointer;

    private:
        /** The current string length */
        const int       length;
        /** The data pointer */
        CharPointer     data;

        // Interface
    public:
        /** Get a pointer on the data */
        inline CharPointer  getData() const     { return data; }
        /** Get the string length */
        inline const int    getLength() const   { return length; }
        /** Limit the string length to the given value
            @param newLength the new length
            @return true on success */
        inline bool limitTo(const int newLength)   { if (newLength > length) return false; const_cast<int&>(length) = newLength; return true; }
        /** Save this data in the given buffer */
        inline void saveIn(wchar_t * buffer, const int size) const { const int phys = size < length ? size : length; memcpy(buffer, data, phys * sizeof(data[0])); buffer[phys] = 0; }
        /** Clone the string data (this actually allocate a new space in the heap, and copy the data to it)  */
        inline ReadOnlyUnicodeString * Clone() const { return new ReadOnlyUnicodeString(data, length); }

        // Construction and operators
    public:
        /** Default constructor
            @param _data    A pointer to the converted data (can be 0)
            @param _length  If _length == 0, then the string length is computed only if _data isn't 0 (else the length is 0). */
        ReadOnlyUnicodeString(CharPointer _data = 0, const int _length = 0) : length(_length == 0 ? findLengthWide((const wchar_t*)_data) : _length), data((_length == 0 ? findLengthWide((const wchar_t*)_data) : _length) ?
            new CharType[(_length == 0 ? findLengthWide((const wchar_t*)_data) : _length)] : 0) { if (_data) memcpy(const_cast<CharType *>(data), _data, length * sizeof(*data)); }
        /** The destructor */
        ~ReadOnlyUnicodeString() { CharType * & _data = const_cast<CharType *&>(data); delete[] _data; _data = 0;  *const_cast<int*>(&length) = 0; }
        /** The copy constructor doesn't copy the data, but actually move them */
        ReadOnlyUnicodeString(const ReadOnlyUnicodeString & copy) : length(copy.length), data(copy.data) { CharType * & _data = const_cast<CharType *&>(const_cast<ReadOnlyUnicodeString &>(copy).data); _data = 0;  *const_cast<int *>(&const_cast<ReadOnlyUnicodeString &>(copy).length) = 0; }
        /** Compare operator */
        inline const bool operator == (const ReadOnlyUnicodeString & copy) const { return length == copy.length && memcmp(data, copy.data, length * sizeof(data[0])) == 0; }
        /** Inverted compare operator */
        inline const bool operator != (const ReadOnlyUnicodeString & copy) const { return !operator ==(copy); }
    };

    /** Convert UTF8 FastString to a wide char string */
    extern ReadOnlyUnicodeString convert(const FastString & string);

    /** Convert a wide char string to a UTF-8 FastString */
    extern FastString convert(const ReadOnlyUnicodeString & string);

    /** Check for a valid UTF-8 sequence. This ensures no overlong (shortest form is mandatory) nor surrogate nor invalid code points are using in encoding.
        @param string   The UTF-8 string to check
        @return 0 on correctly encoded UTF-8 string, or a pointer to the first byte that fails encoding on error */
    const uint8 * checkUTF8(const uint8 * string);


    /** Copy the source to the destination, padding the destination with 0 if required.
        On output, the destination ends with a 0 in all case */
    void copyAndZero(void * dest, int destSize, const void * src, int srcSize);

    /** Quote the given string to RFC 2045 compatible format (in use in MIME messages) */
    FastString quotedPrintable(const FastString & textToQuote);
    /** Unquote the given string from RFC 2045 compatible format (used in MIME messages) */
    FastString unquotedPrintable(const FastString & textToUnquote);

    /** A class containing an array of strings, and allowing some juicy features
        like joining the array into a single string or splitting a string in an array. */
    template <class T>
    class StringArrayT
    {
        // Type definition and enumeration
    public:
        /** The pointer to the string array */
        typedef T *     TPtr;

        // Members
    private:
        /** The array */
        TPtr *      array;
        /** The actual usage */
        size_t      currentSize;
        /** The array allocation size */
        size_t      allocatedSize;

        // Helpers
    private:
        /** Reset the vector to its initial state (doesn't perform destruction of data) */
        inline void Reset() { currentSize = 0; allocatedSize = 0; array = 0; }
        /** Enlarge the array */
        inline void Enlarge(size_t amount = 0)
        {
            if (!amount) amount = allocatedSize >= 2 ? allocatedSize + (allocatedSize >> 1) : 2;
            size_t newAllocatedSize = allocatedSize + amount;
            TPtr * newArray = (TPtr*)Platform::safeRealloc(array, newAllocatedSize * sizeof(array[0]));
            if (!newArray) { Clear(); return; }
            memset(&newArray[currentSize], 0, (newAllocatedSize - currentSize) * sizeof(newArray[0]));
            array = newArray;
            allocatedSize = newAllocatedSize;
        }
        /** Default element */
        static T & getDefaultElement() { static T elem; return elem; }


        // Array interface
    public:
        /** Clear the array, and destruct any remaining objects */
        inline void Clear() { for(size_t i = 0; i < currentSize; i++) delete array[i]; free(array); Reset(); }

        /** Append an element to the end of the array */
        inline void Append(const T & ref) { if (currentSize+1 >= allocatedSize) Enlarge(); array[currentSize++] = new T(ref); }
        /** Append an element to the end of the array */
        inline void Append(const StringArrayT & ref) { if (currentSize + ref.currentSize >= allocatedSize) Enlarge(ref.currentSize); for (size_t i = 0; i < ref.getSize(); i++) array[currentSize++] = new T(*ref.array[i]); }
        /** Append an element only if not present
            @param ref The string to insert in the array
            @return getSize() - 1 if appended correctly, else the position of the element if already present in the array */
        inline size_t appendIfNotPresent(const T & ref) { size_t pos = indexOf(ref); if (pos == getSize()) Append(ref); return pos; }
        /** Grow this array by (at least) the given number of elements.
            This set up the allocation size to, at least, currentSize + count.
            The elements are owned.
            @param elements    A pointer to the elements to append (they are copied, can be 0)
            @param count       How many elements to copy from the given array */
        inline void Grow(const size_t count, T * const elements) throw()
        {
            Enlarge(count);
            if (elements) memmove(&array[currentSize], elements, count * sizeof(T));
            else for (size_t i = currentSize; i < currentSize + count; i++) array[i] = new T();
            currentSize += count;
        }
        /** Insert an element just before the given index
            @param index    Zero based index of the element once inserted
            @param ref      The element */
        inline void insertBefore(size_t index, const T & ref)
        {
            if (index >= currentSize - 1) { Append(ref); return; }
            if (currentSize + 1 >= allocatedSize) Enlarge();
            memmove(&array[index+1], &array[index], (currentSize - index) * sizeof(array[0]));
            array[index] = new T(ref);
            currentSize++;
        }
        /** Remove an object from the array
            @param index Zero based index of the object to remove */
        inline void Remove(size_t index)
        {
            if (index >= currentSize) return;
            delete array[index];
            memmove(&array[index], &array[index+1], (currentSize - index - 1) * sizeof(array[0]));
            array[currentSize--] = 0;
            // Reduce the size only if we have enough space left later on
            if (allocatedSize > 2 && currentSize < allocatedSize / 2)
            {
                TPtr * newArray = (TPtr*)Platform::safeRealloc(array, allocatedSize / 2 * sizeof(array[0]));
                if (newArray) { array = newArray; allocatedSize /= 2; }
            }
        }
        /** Remove an object by first searching into the array.
            This is just a wrapper around indexOf and Remove.
            @param objectToSearch   The object to look for in the array
            @return true if the object was found and removed */
        inline bool removeItem(const T & objectToSearch) throw() { size_t pos = indexOf(objectToSearch); if (pos < currentSize) { Remove(pos); return true; } return false; }

        /** Forget an string from the array.
            The string is not deleted, and as such, will cause a memory leak unless you've taken the reference beforehand.
            @param index Zero based index of the object to remove
            @return A pointer to a "new" allocated object you must delete */
        inline T * Forget(size_t index) throw() { if (index < currentSize) { Swap(index, currentSize - 1); TPtr old = array[currentSize--]; array[currentSize] = 0; return old; } return 0; }
        /** Swap operator
            @param index1   The index to the first object to swap
            @param index2   The index to the second object to swap
            @warning Nothing is done if any index is out of range */
        inline void Swap(const size_t index1, const size_t index2) { if (index1 < currentSize && index2 < currentSize) { TPtr tmp = array[index1]; array[index1] = array[index2]; array[index2] = tmp; } }


        /** Classic copy operator */
        inline StringArrayT & operator = (const StringArrayT & other)
        {
            if (&other == this) return *this;
            Clear(); array = (TPtr*)Platform::realloc(array, other.allocatedSize * sizeof(array[0]));
            if (!array) return *this;
            memset(array, 0, other.allocatedSize * sizeof(array[0]));
            for(size_t i = 0; i < other.currentSize; i++) array[i] = new T(*other.array[i]);
            allocatedSize = other.allocatedSize;
            currentSize = other.currentSize;
            return *this;
        }

        /** Access size member */
        inline size_t getSize() const { return currentSize; }
        /** Access operator
            @param index The position in the array
            @return the index-th element if index is in bound, or an empty string in the other case */
        inline T & operator [] (size_t index) { return index < currentSize ? *array[index] : getDefaultElement(); }
        /** Access operator
            @param index The position in the array
            @return the index-th element if index is in bound, or an empty string in the other case */
        inline const T & operator [] (size_t index) const { return index < currentSize ? *array[index] : getDefaultElement(); }
        /** Get element at position
            @param index The position in the array
            @return the index-th element if index is in bound, or an empty string in the other case */
        inline const T & getElementAtPosition (size_t index) const { return index < currentSize ? *array[index] : getDefaultElement(); }

        /** Search operator
            @param objectToSearch   The object to look for in the array
            @param fromPos          The position to start from
            @return the element index of the given element or getSize() if not found. */
        inline size_t indexOf(const T & objectToSearch, const size_t fromPos = 0) const { for (size_t i = fromPos; i < currentSize; i++) if (*array[i] == objectToSearch) return i; return currentSize; }
        /** Search operator, tells if it contains the given content or not
            @param objectToSearch   The object to look for in the array
            @param fromPos          The position to start from
            @return true if the element is found in the array, false if not */
        inline bool Contains(const T & objectToSearch, const size_t fromPos = 0) const { return indexOf(objectToSearch, fromPos) != currentSize; }
        /** Reverse search operator
            @param objectToSearch The object to look for in the array
            @param startPos         The position to start from
            @return the element index of the given element or getSize() if not found. */
        inline size_t lastIndexOf(const T & objectToSearch, const size_t fromPos = (size_t)(1<<30UL)) const { for (size_t i = min(currentSize, fromPos); i > 0; i--) if (*array[i - 1] == objectToSearch) return i - 1; return currentSize; }
        /** Fast access operator, but doesn't check the index given in */
        inline T & getElementAtUncheckedPosition(size_t index) { return *array[index]; }
        /** Compare operator */
        inline bool operator == (const StringArrayT & other) const { if (other.currentSize != currentSize) return false; for(size_t i = 0; i < currentSize; i++) if (*array[i] != *other.array[i]) return false; return true; }

        // Our specific interface
    public:
        /** Join the array in a big string, using the given separator */
        inline T Join(const T & separator = "\n") const { T ret; for (size_t i = 0; i < currentSize; i++) ret += *array[i] + ((i < currentSize-1) ? separator : T("")); return ret; }
        /** Split the given string into an array, using the given separator.
            The separator is not included in the resulting strings */
        inline void appendLines(const T & text, const T & separator = "\n")
        {
            int lastPos = 0;
            int pos = text.Find(separator);
            while (pos != -1)
            {
                Append(text.midString(lastPos, pos - lastPos));
                lastPos = pos + separator.getLength();
                pos = text.Find(separator, lastPos);
            }
            Append(text.midString(lastPos, text.getLength()));
        }
        /** Inline string search.
            Contrary to the indexOf method that's searching if the array contains a given string, this one search
            if any string in the array contains the given string.
            For example, for a array containing { "abc", "def", "xyz" }, indexOf("e") will not find anything, while
            lookUp("e") will return 1.
            @param objectToSearch   The object to look for in the array
            @param fromPos          The position in the array to start from
            @param internalPos      If provided, on input contains the position in the string to start searching from,
                                    and on output contains the position found in the substring
            @return the element index of the given element or getSize() if not found. */
        inline size_t lookUp(const T & objectToSearch, const size_t fromPos = 0, int * internalPos = 0) const
        {
            int startPos = internalPos ? *internalPos : 0;
            for (size_t i = fromPos; i < currentSize; i++)
            {
                int foundPos = array[i]->Find(objectToSearch, startPos);
                if (foundPos != -1) { if (internalPos) *internalPos = foundPos; return i; }
            }
            return currentSize;
        }
        /** Extract only a subpart of the array into another array.
            @param start The part to start extracting from
            @param end   The position to stop extracting from (not included)
            @return a StringArray that's containing a copy of the specified part of the array */
        inline StringArrayT Extract(const size_t start, size_t end) const
        {
            if (start > getSize()) return StringArrayT();
            if (end > getSize()) end = getSize();
            StringArrayT ret(0, end - start);
            for(size_t i = start; i < end; i++) ret.array[i - start] = new T(*array[i]);
            return ret;
        }

        struct Internal
        {
            TPtr *      array;
            size_t      currentSize;
            size_t      allocatedSize;
        };
        /** Move strategy is explicit with this intermediate object */
        Internal getMovable() { Internal intern = { array, currentSize, allocatedSize }; Reset(); return intern; }
        static Internal emptyInternal() { Internal intern = { 0, 0, 0 }; return intern; }

        // Construction and destruction
    public:
        /** Default Constructor */
        inline StringArrayT() : array(0), currentSize(0), allocatedSize(0)  { }
        /** Copy constructor */
        inline StringArrayT(const StringArrayT & other) : array(0), currentSize(0), allocatedSize(0) { *this = other; }
        /** Move constructor. Use getMovable() to move the array */
        inline StringArrayT(const Internal & intern) : array(intern.array), currentSize(intern.currentSize), allocatedSize(intern.allocatedSize) {}
        /** Construct an array by splitting the string by the given separator.
            The separator is not included in the resulting strings */
        inline StringArrayT(const T & text, const T & separator = "\n", const T & trimArgs = "") : array(0), currentSize(0), allocatedSize(0)
        {
            // Need to count the number of times the separator is found in the string
            allocatedSize = text.Count(separator);
            if (text.midString(-separator.getLength(), separator.getLength()) != separator)
                allocatedSize++;

            array = (TPtr*)Platform::safeRealloc(array, allocatedSize * sizeof(array[0]));
            if (!array) { allocatedSize = 0; return; }

            int lastPos = 0;
            int pos = - separator.getLength();
            while ((pos = text.Find(separator, pos + separator.getLength())) != -1)
            {
                array[currentSize++] = new T(text.midString(lastPos, pos - lastPos).Trimmed(trimArgs));
                lastPos = pos + separator.getLength();
            }
            if (lastPos < text.getLength() &&
                   !(lastPos == (text.getLength() - separator.getLength()) && text.midString(lastPos, separator.getLength()) == separator))
                array[currentSize++] = new T(text.midString(lastPos, text.getLength()).Trimmed(trimArgs));
        }
        /** Build the string array from a given const char * array[] like this:
            @code
            const char * lines[] = { "bob", "bar" };
            StringArray array(lines);
            String text = array.Join(); // text == "bob\nbar";
            @endcode */
        template <const size_t referenceSize>
        inline StringArrayT(const char * (&innerArray)[referenceSize])
            : array((TPtr*)realloc(NULL, referenceSize * sizeof(array[0]))), currentSize(referenceSize), allocatedSize(referenceSize)
        {
            if (!array) currentSize = allocatedSize = 0;
            else for (size_t i = 0; i < referenceSize; i++) array[i] = new T(innerArray[i]);
        }
        /** Build the string array from a given char ** array like this:
            @code
            char ** lines;
            StringArray array(lines, linesCount);
            @endcode */
        inline StringArrayT(char ** innerArray, const size_t referenceSize)
            : array((TPtr*)realloc(NULL, referenceSize * sizeof(array[0]))), currentSize(referenceSize), allocatedSize(referenceSize)
        {
            if (!array) currentSize = allocatedSize = 0;
            else if (innerArray) for (size_t i = 0; i < referenceSize; i++) array[i] = new T(innerArray[i]);
            else memset(array, 0, referenceSize * sizeof(*array));
        }
        /** Build the string array from a given const char ** array like this:
            @code
            int main(int argc, const char** argv)
            {
                StringArray arguments(argc, argv);
            @endcode */
        inline StringArrayT(const int referenceSize, const char ** innerArray)
            : array((TPtr*)realloc(NULL, referenceSize * sizeof(array[0]))), currentSize(referenceSize), allocatedSize(referenceSize)
        {
            if (!array) currentSize = allocatedSize = 0;
            else if (innerArray) for (size_t i = 0; i < referenceSize; i++) array[i] = new T(innerArray[i]);
            else memset(array, 0, referenceSize * sizeof(*array));
        }
        /** Destructor */
        ~StringArrayT()        { Clear(); }
    };

    /** The default version is using UTF-8 and R/W Strings */
    typedef StringArrayT<FastString> StringArray;
    /** But there's also a R/O version */
    typedef StringArrayT<VerySimpleReadOnlyString> StringArrayRO;

    /** This is used in Container's algorithms to sort and search the array */
    template <typename T>
    struct CompareStringT
    {
        static inline int compareData(const T & first, const T & second)
        {
            int ret = memcmp(first.getData(), second.getData(), min(first.getLength(), second.getLength()));
            if (ret) return ret;
            return first.getLength() < second.getLength() ? -1 : 1;
        }
    };
    /** The default version is using UTF-8 and R/W Strings */
    typedef CompareStringT<FastString> CompareString;
    /** But there's also a R/O version */
    typedef CompareStringT<VerySimpleReadOnlyString> CompareStringRO;

    template <typename T>
    struct TypeToNameT
    {
        static FastString getTypeFromName() {
            Strings::FastString templateType = Strings::FastString(
#ifdef _MSC_VER
                __FUNCTION__);
#else
                __PRETTY_FUNCTION__);
#endif
                // Try GCC type first, since it's the most specific : "LocalVariableImpl<T>::getName() [ with T = yourTypeHere ]"
                Strings::FastString finalType = templateType.fromFirst("=").upToFirst(";").upToFirst("]").Trimmed();
                // Else, try to use the templated argument itself: "LocalVariableImpl<yourTypeHere>::getName()"
                if (!finalType) finalType = templateType.fromFirst("<").upToFirst(">").Trimmed();
                return finalType;
        }
    };
    /** Get the templated type name. This does not depends on RTTI, but on the preprocessor, so it should be quite safe to use even on old compilers */
    template <typename T>
    FastString getTypeName(const T *) { return TypeToNameT<T>::getTypeFromName(); }


}


#endif

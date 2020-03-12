#ifndef hpp_CPP_VerySimpleReadOnlyString_CPP_hpp
#define hpp_CPP_VerySimpleReadOnlyString_CPP_hpp

// We need types
#include "Types.hpp"

// Forward declare the FastString
namespace Bstrlib { struct String; }
namespace Strings
{
    /** Define the FastString type */
    typedef Bstrlib::String FastString;


    typedef char const * const tCharPtr;
    const char usualTrimSequence[] = " \t\v\f\r\n";

    // Forward declare the findLength function
    const unsigned int findLength(tCharPtr, const size_t limit = 0);
    // Forward declare the findLengthWide function
    const unsigned int findLengthWide(const wchar_t *);

    /** Well, the name says it all, this is a very simple read only string.
        The main advantage of this class is that it doesn't allocate any
        memory at all, and works on fixed size buffer correctly.
        So you can/should use it on embedded system wherever applicable or for any parsing work.
        Please notice that it supports mutating its heads (start of string and length of string)
        but not the string content itself.
        There is no (const char*) operator since it might not be zero terminated (obviously). */
    class VerySimpleReadOnlyString
    {
    private:
        /** The data pointer */
        tCharPtr    data;
        /** The current string length */
        const int   length;

        // Interface
    public:
        /** Get a pointer on the data */
        inline tCharPtr     getData() const     { return data; }
        /** Get the string length */
        inline const int    getLength() const   { return length; }
        /** Limit the string length to the given value
            @param newLength the new length
            @return true on success */
        inline bool limitTo(const int newLength)   { if (newLength > length) return false; (void)Mutate(data, newLength); return true; }
        /** Get the substring from this string */
        VerySimpleReadOnlyString midString(int left, int len) const { return VerySimpleReadOnlyString(left < length ? &data[left] : "", max(0, min(len, length - left))); }
        /** Split at the given position.
            For example, the following code gives:
            @code
                String text = "abcdefdef";
                String ret = text.splitAt(3); // ret = "abc", text = "defdef"
                ret = text.splitAt(3, 1);     // ret = "de", text = "def"
                ret = text.splitAt(9);        // ret = "def", text = ""
            @endcode
            @param pos    The position to split this string.
                          If the position is larger than the string's length, the complete string is returned,
                          and this string is modified to be empty.
                          If the position is negative, an empty string is returned, and this string is left
                          unmodified.
            @param stripFromRet   This is the amount of characters to strip from the right of the returned string.
                                  This is equivalent to .limitTo(getLength() - stripFromRet)
            @return The part from the left to the given position. */
        VerySimpleReadOnlyString splitAt(int pos, int stripFromRet = 0);
        /** Trim the string from the given char (and direction) */
        VerySimpleReadOnlyString trimRight(const char ch) const { int len = length; while(len > 1 && data && data[len - 1] == ch) len--; return VerySimpleReadOnlyString(data, len); }
        /** Trim the string from the given char (and direction) */
        VerySimpleReadOnlyString trimLeft(const char ch) const { int len = length; while(len > 1 && data && data[length - len] == ch) len--; return VerySimpleReadOnlyString(data + (length - len), len); }
        /** Trim the beginning of string from any char in the given array */
        VerySimpleReadOnlyString trimmedLeft(const char* chars, int nlen = 0) const { int len = length; if (!nlen && chars) nlen = (int)strlen(chars); while(len > 1 && data && memchr(chars, data[length - len], nlen) != NULL) len--; return VerySimpleReadOnlyString(data + (length - len), len); }
        /** Trim the beginning of string from any char in the given array */
        VerySimpleReadOnlyString trimmedLeft() const { return trimmedLeft(usualTrimSequence, sizeof(usualTrimSequence)); }
        /** Trim the end of string from any char in the given array */
        VerySimpleReadOnlyString trimmedRight(const char* chars, int nlen = 0) const { int len = length; if (!nlen && chars) nlen = (int)strlen(chars); while(len > 1 && data && memchr(chars, data[len - 1], nlen) != NULL) len--; return VerySimpleReadOnlyString(data, len); }
        /** Trim the end of string from any char in the given array */
        VerySimpleReadOnlyString trimmedRight() const { return trimmedRight(usualTrimSequence, sizeof(usualTrimSequence)); }
        /** Trim the beginning of string from any char in the given array.
            This is using fluent interface and modifies the internal object. */
        VerySimpleReadOnlyString & leftTrim(const char* chars, int nlen = 0) { int len = length; if (!nlen && chars) nlen = (int)strlen(chars); while(len > 1 && data && memchr(chars, data[length - len], nlen) != NULL) len--; return Mutate(data + (length - len), len); }
        /** Trim the beginning of string from any char in the given array.
            This is using fluent interface and modifies the internal object. */
        template <size_t nlen> VerySimpleReadOnlyString & leftTrim(const char (&chars)[nlen]) { int len = length; while(len > 1 && data && memchr(chars, data[length - len], nlen-1) != NULL) len--; return Mutate(data + (length - len), len); }
        /** Trim the beginning of string from any char in the given array
            This is using fluent interface and modifies the internal object. */
        VerySimpleReadOnlyString & leftTrim() { return leftTrim(usualTrimSequence, sizeof(usualTrimSequence)); }
        /** Trim the end of string from any char in the given array
            This is using fluent interface and modifies the internal object. */
        VerySimpleReadOnlyString & rightTrim(const char* chars, int nlen = 0) { int len = length; if (!nlen && chars) nlen = (int)strlen(chars); while(len > 1 && data && memchr(chars, data[len - 1], nlen) != NULL) len--; return Mutate(data, len); }
        /** Trim the end of string from any char in the given array
            This is using fluent interface and modifies the internal object. */
        template <size_t nlen> VerySimpleReadOnlyString & rightTrim(const char (&chars)[nlen]) { int len = length; while(len > 1 && data && memchr(chars, data[len - 1], nlen - 1) != NULL) len--; return Mutate(data, len); }
        /** Trim the end of string from any char in the given array
            This is using fluent interface and modifies the internal object. */
        VerySimpleReadOnlyString & rightTrim() { return rightTrim(usualTrimSequence, sizeof(usualTrimSequence)); }
        /** Trim the string from any char in the given array */
        VerySimpleReadOnlyString Trimmed(const char* chars, int nlen = 0) const
        {
            int llen = length, rlen = length;
            if (!nlen && chars) nlen = (int)strlen(chars);
            while(nlen && llen > 1 && data && memchr(chars, data[length - llen], nlen) != NULL) llen--;
            while(nlen && rlen > 1 && data && memchr(chars, data[rlen - 1], nlen) != NULL) rlen--;
            return VerySimpleReadOnlyString(data + (length - llen), rlen - (length  - llen));
        }
        /** Trim the string from any char in the given array */
        VerySimpleReadOnlyString Trimmed() const { return Trimmed(usualTrimSequence, sizeof(usualTrimSequence)); }
        /** Trim the string from any char in the given array */
        VerySimpleReadOnlyString Trimmed(const VerySimpleReadOnlyString & t) const
        {
            int llen = length, rlen = length;
            while(t.length && llen > 1 && data && memchr(t.data, data[length - llen], t.length) != NULL) llen--;
            while(t.length && rlen > 1 && data && memchr(t.data, data[rlen - 1], t.length) != NULL) rlen--;
            return VerySimpleReadOnlyString(data + (length - llen), rlen - (length  - llen));
        }
        /** Trim the string from any char in the given array
            This is using fluent interface and modifies the internal object. */
        VerySimpleReadOnlyString & Trim(const char* chars, int nlen = 0)
        {
            int llen = length, rlen = length;
            if (!nlen && chars) nlen = (int)strlen(chars);
            while(nlen && llen > 1 && data && memchr(chars, data[length - llen], nlen) != NULL) llen--;
            while(nlen && rlen > 1 && data && memchr(chars, data[rlen - 1], nlen) != NULL) rlen--;
            return Mutate(data + (length - llen), rlen - (length  - llen));
        }
        /** Trim the string from any char in the given array
            This is using fluent interface and modifies the internal object. */
        VerySimpleReadOnlyString & Trim() { return Trim(usualTrimSequence, sizeof(usualTrimSequence)); }
        /** Trim the string from any char in the given array
            This is using fluent interface and modifies the internal object. */
        VerySimpleReadOnlyString & Trim(const VerySimpleReadOnlyString & t)
        {
            int llen = length, rlen = length;
            while(t.length && llen > 1 && data && memchr(t.data, data[length - llen], t.length) != NULL) llen--;
            while(t.length && rlen > 1 && data && memchr(t.data, data[rlen - 1], t.length) != NULL) rlen--;
            return Mutate(data + (length - llen), rlen - (length  - llen));
        }

        /** Find the specific needle in the string.
            This is a very simple O(n*m) search.
            @return the position of the needle, or getLength() if not found.
            @warning For historical reasons, Strings::FastString::Find returns -1 if not found */
        const unsigned int Find(const VerySimpleReadOnlyString & needle, unsigned int pos = 0) const;
        /** Find any of the given set of chars
            @return the position of the needle, or getLength() if not found.
            @warning For historical reasons, Strings::FastString::findAnyChar returns -1 if not found */
        const unsigned int findAnyChar(const char * chars, unsigned int pos = 0, int nlen = 0) const { int len = pos; if (!nlen && chars) nlen = (int)strlen(chars); while(len < length && data && memchr(chars, data[len], nlen) == NULL) len++; return len; }
        /** Find first char that's not in the given set of chars
            @return the position of the needle, or getLength() if not found.
            @warning For historical reasons, Strings::FastString::invFindAnyChar returns -1 if not found */
        const unsigned int invFindAnyChar(const char * chars, unsigned int pos = 0, int nlen = 0) const { int len = pos; if (!nlen && chars) nlen = (int)strlen(chars); while(len < length && data && memchr(chars, data[len], nlen) != NULL) len++; return len; }
        /** Find the specific needle in the string, starting from the end of the string.
            This is a very simple O(n*m) search.
            @return the position of the needle, or getLength() if not found.
            @warning For historical reasons, Strings::FastString::reverseFind returns -1 if not found */
        const unsigned int reverseFind(const VerySimpleReadOnlyString & needle, unsigned int pos = (unsigned int)-1) const;
        /** Count the number of times the given substring appears in the string */
        const unsigned int Count(const VerySimpleReadOnlyString & needle) const;

        /** Split a string when the needle is found first, returning the part before the needle, and
            updating the string to start on or after the needle.
            If the needle is not found, it returns an empty string if includeFind is false, or the whole string if true.
            For example this code returns:
            @code
                String text = "abcdefdef";
                String ret = text.splitFrom("d"); // ret = "abc", text = "efdef"
                ret = text.splitFrom("f", true);  // ret = "e", text = "fdef"
            @endcode
            @param find         The string to look for
            @param includeFind  If true the string is updated to start on the find text. */
        const VerySimpleReadOnlyString splitFrom(const VerySimpleReadOnlyString & find, const bool includeFind = false);

        /** Get the substring from the given needle up to the given needle.
            For example, this code returns:
            @code
                String text = "abcdefdef";
                String ret = text.fromTo("d", "f"); // ret = "e"
                ret = text.fromTo("d", "f", true);  // ret = "def"
                ret = text.fromTo("d", "g"); // ret = ""
                ret = text.fromTo("d", "g", true); // ret = "defdef"
                ret = text.fromTo("g", "f", [true or false]); // ret = ""
            @endcode

            @param from         The first needle to look for
            @param to           The second needle to look for
            @param includeFind  If true, the text searched for is included in the result
            @return If "from" needle is not found, it returns an empty string, else if "to" needle is not found,
                    it returns an empty string upon includeFind being false, or the string starting from "from" if true. */
        const VerySimpleReadOnlyString fromTo(const VerySimpleReadOnlyString & from, const VerySimpleReadOnlyString & to, const bool includeFind = false) const;

        /** Get the string up to the first occurrence of the given string
            If not found, it returns the whole string unless includeFind is true (empty string in that case).
            For example, this code returns:
            @code
                String ret = String("abcdefdef").upToFirst("d"); // ret == "abc"
                String ret = String("abcdefdef").upToFirst("g"); // ret == "abcdefdef"
            @endcode
            @param find         The text to look for
            @param includeFind  If set, the needle is included in the result */
        const VerySimpleReadOnlyString upToFirst(const VerySimpleReadOnlyString & find, const bool includeFind = false) const;
        /** Get the string up to the last occurrence of the given string
            If not found, it returns the whole string unless includeFind is true (empty string in that case).
            For example, this code returns:
            @code
                String ret = String("abcdefdef").upToLast("d"); // ret == "abcdef"
                String ret = String("abcdefdef").upToLast("g"); // ret == "abcdefdef"
            @endcode
            @param find         The text to look for
            @param includeFind  If set, the needle is included in the result */
        const VerySimpleReadOnlyString upToLast(const VerySimpleReadOnlyString & find, const bool includeFind = false) const;
        /** Get the string from the last occurrence of the given string.
            If not found, it returns an empty string if includeFind is false, or the whole string if true
            For example, this code returns:
            @code
                String ret = String("abcdefdef").fromLast("d"); // ret == "ef"
                String ret = String("abcdefdef").fromLast("d", true); // ret == "def"
                String ret = String("abcdefdef").fromLast("g"); // ret == ""
            @endcode
            @param find         The text to look for
            @param includeFind  If set, the needle is included in the result */
        const VerySimpleReadOnlyString fromLast(const VerySimpleReadOnlyString & find, const bool includeFind = false) const;
        /** Get the string from the first occurrence of the given string
            If not found, it returns an empty string if includeFind is false, or the whole string if true
            For example, this code returns:
            @code
                String ret = String("abcdefdef").fromFirst("d"); // ret == "efdef"
                String ret = String("abcdefdef").fromFirst("d", true); // ret == "defdef"
                String ret = String("abcdefdef").fromFirst("g"); // ret == ""
            @endcode
            @param find         The text to look for
            @param includeFind  If set, the needle is included in the result */
        const VerySimpleReadOnlyString fromFirst(const VerySimpleReadOnlyString & find, const bool includeFind = false) const;
        /** Get the substring from the given needle if found, or the whole string if not.
            For example, this code returns:
            @code
                String text = "abcdefdef";
                String ret = text.dropUpTo("d"); // ret = "efdef"
                ret = text.dropUpTo("d", true); // ret = "defdef"
                ret = text.dropUpTo("g", [true or false]); // ret = "abcdefdef"
            @endcode
            @param find         The string to look for
            @param includeFind  If true the string is updated to start on the find text. */
        const VerySimpleReadOnlyString dropUpTo(const VerySimpleReadOnlyString & find, const bool includeFind = false) const;
        /** Get the substring up to the given needle if found, or the whole string if not, and split from here.
            For example, this code returns:
            @code
                String text = "abcdefdef";
                String ret = text.splitUpTo("d"); // ret = "abc", text = "efdef"
                ret = text.splitUpTo("g", [true or false]); // ret = "efdef", text = ""
                text = "abcdefdef";
                ret = text.splitUpTo("d", true); // ret = "abcd", text = "efdef"
            @endcode
            @param find         The string to look for
            @param includeFind  If true the string is updated to start on the find text. */
        const VerySimpleReadOnlyString splitUpTo(const VerySimpleReadOnlyString & find, const bool includeFind = false);

        /** The basic conversion operators */
        operator int() const;
        /** The basic conversion operators */
        operator unsigned int() const;
        /** The basic conversion operators */
        operator int64() const;
        /** The basic conversion operators */
        operator double() const;

        /** Get the integer out of this string.
            This method support any usual encoding of the integer, and detect the integer format automatically.
            This method is optimized for speed, and does no memory allocation on heap
            Supported formats examples: "0x1234, 0700, -1234, 0b00010101"
            @param base     If provided, only the given base is supported (default to 0 for auto-detection).
            @param consumed If provided, will be filled with the number of consumed characters.
            @return The largest possible integer that's parseable. */
        int64 parseInt(const int base = 0, int * consumed = 0) const { char * end = 0; int64 out = strtoll((const char*)data, &end, base); if (consumed) *consumed = (int)(end - (char*)data); return out; }
        /** Get the double stored in this string
            @param consumed If provided, will be filled with the number of consumed characters.
            @return The largest possible double number that's parseable. */
        double parseDouble(int * consumed = 0) const { char * end = 0; double out = strtod((const char*)data, &end); if (consumed) *consumed = (int)(end - (char*)data); return out; }

        /** So you can check the string directly for emptiness */
        inline bool operator !() const { return length == 0; }
        /** So you can check the string directly for emptiness */
        inline operator bool() const { return length > 0; }
        /** Operator [] to access a single char */
        char operator[] (int index) const { return index < length ? data[index] : 0; }

        // Construction and operators
    public:
        /** Default constructor */
        VerySimpleReadOnlyString(tCharPtr _data = 0, const int _length = -1) : data(_data), length(_length == -1 ? findLength(data) : _length) { }
        /** Constant string build */
        template <size_t N>
        VerySimpleReadOnlyString(const char (&_data)[N]) : data(_data), length(N-1) { }
        /** Convertion constructor, the given object must live after around this lifetime */
        VerySimpleReadOnlyString(const FastString & other);
        /** The destructor */
        ~VerySimpleReadOnlyString() {}
        /** Copy constructor */
        VerySimpleReadOnlyString(const VerySimpleReadOnlyString & copy) : data(copy.data), length(copy.length) {}
        /** Equal operator */
        inline VerySimpleReadOnlyString & operator = (const VerySimpleReadOnlyString & copy) { if (&copy != this) return Mutate(copy.data, copy.length); return *this; }
        /** Compare operator */
        inline const bool operator == (const VerySimpleReadOnlyString & copy) const { return length == copy.length && memcmp(data, copy.data, length) == 0; }
        /** Compare operator */
        inline const bool operator == (tCharPtr copy) const { return length == (int)findLength(copy) && memcmp(data, copy, length) == 0; }
        /** Compare operator */
        template <size_t N> inline const bool operator == (const char (&copy)[N]) const { return length == N-1 && memcmp(data, copy, length) == 0; }
        /** Compare operator */
        const bool operator == (const FastString & copy) const;
        /** Inverted compare operator */
        inline const bool operator != (const VerySimpleReadOnlyString & copy) const { return !operator ==(copy); }
        /** Inverted compare operator */
        inline const bool operator != (tCharPtr copy) const { return !operator ==(copy); }
        /** Inverted compare operator */
        template <size_t N> inline const bool operator != (const char (&copy)[N]) const { return !operator ==(copy); }
        /** Inverted compare operator */
        inline const bool operator != (const FastString & copy) const { return !operator ==(copy); }

    private:
        /** Mutate this string head positions (not the content) */
    //    inline const VerySimpleReadOnlyString & Mutate(tCharPtr d, const int len) const { const_cast<const char * & >(data) = d; const_cast<int&>(length) = len; return *this; }
        /** Mutate this string head positions (not the content) */
        inline VerySimpleReadOnlyString & Mutate(tCharPtr d, const int len) { const_cast<const char * & >(data) = d; const_cast<int&>(length) = len; return *this; }
#if (DEBUG==1)
        // Prevent unwanted conversion
        /** Prevent unwanted conversion */
        template <typename T> bool operator == (const T & t) const
        {   // If compiler stops here, it's because you're trying to compare a string with a type it doesn't understand.
            // Try to cast this type to (const char*) or (String), but don't rely on default, the compiler can't read your mind.
            CompileTimeAssertFalse(T); return false;
        }
        /** Prevent unwanted conversion */
        template <typename T> bool operator < (const T & t) const
        {   // If compiler stops here, it's because you're trying to compare a string with a type it doesn't understand.
            // Try to cast this type to (const char*) or (String), but don't rely on default, the compiler can't read your mind.
            CompileTimeAssertFalse(T); return false;
        }
        /** Prevent unwanted conversion */
        template <typename T> bool operator > (const T & t) const
        {   // If compiler stops here, it's because you're trying to compare a string with a type it doesn't understand.
            // Try to cast this type to (const char*) or (String), but don't rely on default, the compiler can't read your mind.
            CompileTimeAssertFalse(T); return false;
        }
        /** Prevent unwanted conversion */
        template <typename T> bool operator <= (const T & t) const
        {   // If compiler stops here, it's because you're trying to compare a string with a type it doesn't understand.
            // Try to cast this type to (const char*) or (String), but don't rely on default, the compiler can't read your mind.
            CompileTimeAssertFalse(T); return false;
        }
        /** Prevent unwanted conversion */
        template <typename T> bool operator >= (const T & t) const
        {   // If compiler stops here, it's because you're trying to compare a string with a type it doesn't understand.
            // Try to cast this type to (const char*) or (String), but don't rely on default, the compiler can't read your mind.
            CompileTimeAssertFalse(T); return false;
        }
        /** Prevent unwanted conversion */
        template <typename T> bool operator != (const T & t) const
        {   // If compiler stops here, it's because you're trying to compare a string with a type it doesn't understand.
            // Try to cast this type to (const char*) or (String), but don't rely on default, the compiler can't read your mind.
            CompileTimeAssertFalse(T); return false;
        }
#endif
    };
}

#endif

#include "../include/Strings/Strings.hpp"
#include "Platform/Platform.hpp"

namespace Strings
{

    // A faster method to compute log10 of a number
    inline uint32 countDigits(uint64 n)
    {
        uint32 count = 1;
        while (true)
        {
            // Avoid integer division for each power of ten, branch instead, it's faster
            if (n < 10)         return count;
            if (n < 100)        return count + 1;
            if (n < 1000)       return count + 2;
            if (n < 10000)      return count + 3;
            n /= 10000u;
            count += 4;
        }
    }

    // Fast long to int code
    char* ulltoa(uint64 value, char* result, int base)
    {
        if (base < 2 || base > 36) { *result = '\0'; return result; }
        char* ptr = result, *ptr1 = result, tmp_char;
        uint64 tmp_value;
        do {
            tmp_value = value;
            value /= base;
            *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
        } while ( value );
        *ptr-- = '\0';
        // Then swap and reorder first to last
        while(ptr1 < ptr) {
            tmp_char = *ptr;
            *ptr--= *ptr1;
            *ptr1++ = tmp_char;
        }
        return result;
    }


    template <int base>
    struct ToStr
    {
        enum { MinBufferSize = 65 };
        inline static char * convert(uint64 value, char * result) { return ulltoa(value, result, base); }
    };

    template <>
    struct ToStr<10>
    {
        // Max value: 18446744073709551615
        enum { MinBufferSize = 21 };

        static char * convert(uint64 value, char * result)
        {
            static char Digits[]    = "0001020304050607080910111213141516171819202122232425262728293031323334353637383940414243444546474849"
                                      "5051525354555657585960616263646566676869707172737475767778798081828384858687888990919293949596979899";
            result += MinBufferSize - 1; *result = 0;
            while (value >= 100)
            {
               // Avoid integer division for each power of ten, use lookup table
               uint64 q = (value / 100); uint32 r = (uint32)(value % 100);
               value = q;
               *--result = Digits[r * 2 + 1];
               *--result = Digits[r * 2];
             }
             if (value < 10) { *--result = '0' + value; return result; }
             *--result = Digits[value * 2 + 1];
             *--result = Digits[value * 2];
             return result;
        }
        static char * convert(int64 value, char * result)
        {
            if (value < 0)
            {
                char * r = convert((uint64)-value, result);
                *--r = '-'; return r;
            }
            return convert((uint64)value, result);
        }
    };


    const unsigned int VerySimpleReadOnlyString::Find(const VerySimpleReadOnlyString & needle, unsigned int pos) const
    {
	    for (unsigned int j = 0; pos + j < (unsigned int)length;)
	    {
		    if (needle.data[j] == data[pos + j])
		    {
			    j++;
			    if (j == (unsigned int)needle.length)	return pos;
			    continue;
		    }
		    pos++;
		    j = 0;
	    }
        return (unsigned int)length;
    }
    const unsigned int VerySimpleReadOnlyString::reverseFind(const VerySimpleReadOnlyString & needle, unsigned int pos) const
    {
        if (needle.length > length) return length;
        unsigned int i = min(pos, (unsigned int)(length - needle.length)); // If there is no space to find out the needle at the end, simply snap back
	    for (unsigned int j = 0;;)
	    {
		    if (needle.data[j] == data[i + j])
		    {
			    j ++;
			    if (j >= (unsigned int)needle.length) return i;
		    } else
		    {
			    if (i-- == 0) break;
			    j = 0;
		    }
	    }
        return length;
    }

    const unsigned int VerySimpleReadOnlyString::Count(const VerySimpleReadOnlyString & needle) const
    {
        int pos = -1; unsigned int count = 0;
        while ((pos = Find(needle, pos+1)) != -1) count++;
        return count;
    }

	const VerySimpleReadOnlyString VerySimpleReadOnlyString::splitFrom(const VerySimpleReadOnlyString & find, const bool includeFind)
	{
	    const unsigned int pos = Find(find);
	    if (pos == (unsigned int)length)
	    {
	        if (includeFind)
	        {
	            VerySimpleReadOnlyString ret(*this);
                (void)Mutate(data + length, 0);
	            return ret;
	        }
	        return VerySimpleReadOnlyString("", 0);
	    }
	    const int size = pos + find.length;
	    VerySimpleReadOnlyString ret(data, includeFind ? size : pos);
        (void)Mutate(data + size, length - size);
	    return ret;
	}

    VerySimpleReadOnlyString VerySimpleReadOnlyString::splitAt(int pos, int stripFromRet)
    {
        if (stripFromRet > pos) stripFromRet = pos;
        if (pos < 0) return VerySimpleReadOnlyString();
        VerySimpleReadOnlyString ret(data, min(pos - stripFromRet, length));
        if (pos > length) (void)Mutate(data, 0);
        else (void)Mutate(data + pos, length - pos);
        return ret;
    }


    const VerySimpleReadOnlyString VerySimpleReadOnlyString::fromTo(const VerySimpleReadOnlyString & from, const VerySimpleReadOnlyString & to, const bool includeFind) const
	{
	    const unsigned int fromPos = Find(from);
	    const unsigned int toPos = Find(to, fromPos + from.length);
	    return VerySimpleReadOnlyString(fromPos >= (unsigned int)length ? "" : &data[includeFind ? fromPos : fromPos + (unsigned int)from.length],
	                                    toPos < (unsigned int)length ? (includeFind ? toPos + (unsigned int)to.length - fromPos : toPos - fromPos - (unsigned int)from.length)
	                                       // If the "to" needle was not found, either we return the whole string (includeFind) or an empty string
	                                       : (includeFind ? (unsigned int)length - fromPos : 0));
	}

	// Get the string up to the first occurrence of the given string
	const VerySimpleReadOnlyString VerySimpleReadOnlyString::upToFirst(const VerySimpleReadOnlyString & find, const bool includeFind) const
	{
	    const unsigned int pos = Find(find);
	    return VerySimpleReadOnlyString(pos == (unsigned int)length && includeFind ? "" : data, includeFind ? (pos == (unsigned int)length ? 0 : pos + (unsigned int)find.length) : pos);
	}
	// Get the string up to the last occurrence of the given string
	const VerySimpleReadOnlyString VerySimpleReadOnlyString::upToLast(const VerySimpleReadOnlyString & find, const bool includeFind) const
	{
	    const unsigned int pos = reverseFind(find);
	    return VerySimpleReadOnlyString(pos == (unsigned int)length && includeFind ? "" : data, includeFind ? (pos == (unsigned int)length ? 0 : pos + (unsigned int)find.length) : pos);
	}
	// Get the string from the last occurrence of the given string.
	const VerySimpleReadOnlyString VerySimpleReadOnlyString::fromLast(const VerySimpleReadOnlyString & find, const bool includeFind) const
	{
	    const unsigned int pos = reverseFind(find);
	    return VerySimpleReadOnlyString(pos == (unsigned int)length ? (includeFind ? data : "") : &data[includeFind ? pos : pos + (unsigned int)find.length],
	                                    pos == (unsigned int)length ? (includeFind ? (unsigned int)length : 0) : (includeFind ? (unsigned int)length - pos : (unsigned int)length - pos - (unsigned int)find.length));
	}
	// Get the string from the first occurrence of the given string
	const VerySimpleReadOnlyString VerySimpleReadOnlyString::fromFirst(const VerySimpleReadOnlyString & find, const bool includeFind) const
	{
	    const unsigned int pos = Find(find);
	    return VerySimpleReadOnlyString(pos == (unsigned int)length ? (includeFind ? data : "") : &data[includeFind ? pos : pos + (unsigned int)find.length],
	                                    pos == (unsigned int)length ? (includeFind ? (unsigned int)length : 0)
	                                                      : (includeFind ? (unsigned int)length - pos
	                                                                  : (unsigned int)length - pos - (unsigned int)find.length));
	}
	// Get the string from the first occurrence of the given string
	const VerySimpleReadOnlyString VerySimpleReadOnlyString::dropUpTo(const VerySimpleReadOnlyString & find, const bool includeFind) const
	{
	    const unsigned int pos = Find(find);
	    return VerySimpleReadOnlyString(pos == (unsigned int)length ? data : &data[includeFind ? pos : pos + (unsigned int)find.length],
	                                    pos == (unsigned int)length ? (unsigned int)length : (includeFind ? (unsigned int)length - pos
	                                                                  : (unsigned int)length - pos - (unsigned int)find.length));
	}
	// Get the substring up to the given needle if found, or the whole string if not, and split from here.
    const VerySimpleReadOnlyString VerySimpleReadOnlyString::splitUpTo(const VerySimpleReadOnlyString & find, const bool includeFind)
    {
	    const unsigned int pos = Find(find);
	    if (pos == (unsigned int)length)
	    {
            VerySimpleReadOnlyString ret(*this);
            (void)Mutate(data + length, 0);
            return ret;
	    }
	    const int size = pos + find.length;
	    VerySimpleReadOnlyString ret(data, includeFind ? size : pos);
        (void)Mutate(data+size, length - size);
	    return ret;
    }


	/** The basic conversion operators */
	VerySimpleReadOnlyString::operator int64() const
	{
#ifdef _MSC_VER
        return (int64)_strtoi64(data, NULL, 10);
#else
	    return (int64)strtoll(data, NULL, 10);
#endif
	}
	/** The basic conversion operators */
	VerySimpleReadOnlyString::operator int() const
	{
	    return atoi(data);
	}
	/** The basic conversion operators */
	VerySimpleReadOnlyString::operator unsigned int() const
	{
	    return atol(data);
	}
	/** The basic conversion operators */
	VerySimpleReadOnlyString::operator double() const
	{
	    return atof(data);
	}
    /** Compare operator */
    const bool VerySimpleReadOnlyString::operator == (const FastString & copy) const
    {
        return length == copy.getLength() && memcmp(data, (const char*)copy, length) == 0;
    }
    // Conversion
    VerySimpleReadOnlyString::VerySimpleReadOnlyString(const FastString & other)
        : data((tCharPtr)(const char*)other), length(other.getLength())
    { }


    // Convert UTF8 FastString to a wide char string
    ReadOnlyUnicodeString convert(const FastString & string)
    {
        // Create some space for the UCS-2 string
        ReadOnlyUnicodeString ret(0, string.getLength() + 1);
        ReadOnlyUnicodeString::CharType * data = const_cast<ReadOnlyUnicodeString::CharType *>(ret.getData());
        if (!data) return ret;

        ReadOnlyUnicodeString::CharType * dest = data;

        // Direct access to the buffer (don't bound check for every access)
        const uint8 * buffer = (const uint8*)string;
        for (int i = 0; i < string.getLength();)
        {
            // Get the current char
            const uint8 c = buffer [i++];

            // Check for UTF8 code
            if ((c & 0x80) != 0)
            {
                // The data is in the 7 low bits
                uint32 dataMask = 0x7f;
                // The count bit mask
                uint32 bitCountMask = 0x40;
                // The consumption count
                int charCount = 0;

                while ((c & bitCountMask) != 0 && bitCountMask)
                {
                    ++charCount;
                    dataMask >>= 1; bitCountMask >>= 1;
                }

                // Get the few bits remaining here
                int n = (c & dataMask);

                // Then extract the remaining bits
                while (--charCount >= 0 && i < string.getLength())
                {
                    const uint8 extra = buffer[i];
                    // Make sure it's a valid UTF8 encoding
                    if ((extra & 0xc0) != 0x80) break;

                    // Store the new bits too
                    n <<= 6; n |= (extra & 0x3f);
                    ++i;
                }

                *dest++ = (ReadOnlyUnicodeString::CharType)n;
            }
            else // Append the char as-is
                *dest++ = (ReadOnlyUnicodeString::CharType)c;
        }

        // Store the last zero byte
        *dest = 0;
        // Create the returned object
        ret.limitTo((int)(dest - data));
        return ret;
    }

    /** Convert a wide char string to a UTF-8 FastString */
    // +----------+----------+----------+----------+
    // | 33222222 | 22221111 | 111111   |          |
    // | 10987654 | 32109876 | 54321098 | 76543210 | bit
    // +----------+----------+----------+----------+
    // |          |          |          | 0xxxxxxx | 1 byte 0x00000000..0x0000007F
    // |          |          | 110yyyyy | 10xxxxxx | 2 byte 0x00000080..0x000007FF
    // |          | 1110zzzz | 10yyyyyy | 10xxxxxx | 3 byte 0x00000800..0x0000FFFF
    // | 11110www | 10wwzzzz | 10yyyyyy | 10xxxxxx | 4 byte 0x00010000..0x0010FFFF
    // +----------+----------+----------+----------+
    // | 00000000 | 00011111 | 11111111 | 11111111 | Theoretical upper limit of legal scalars: 2097151 (0x001FFFFF)
    // | 00000000 | 00010000 | 11111111 | 11111111 | Defined upper limit of legal scalar codes
    // +----------+----------+----------+----------+

    FastString convert(const ReadOnlyUnicodeString & string)
    {
        FastString ret;
        for (int i = 0; i < string.getLength(); i++)
        {
            const uint32 c = string.getData()[i];

            // From http://www1.tip.nl/~t876506/utf8tbl.html#algo
            if (c < 0x80)
            {   // Single byte char
                ret += (uint8)c;
            } // Multi byte char
            else if (c < 0x800)
            {   // 2 bytes encoding
                ret += (uint8)(0xc0 | (c >> 6));
                ret += (uint8)(0x80 | (c & 0x3f));
            }
            else if (c < 0x10000)
            {   // 3 bytes encoding
                ret += (uint8)(0xe0 | (c >> 12));
                ret += (uint8)(0x80 | ( (c>>6) & 0x3f));
                ret += (uint8)(0x80 | (c & 0x3f));
            }
            else if (c < 0x200000)
            {   // 4 bytes encoding
                ret += (uint8)(0xf0 | (c >> 18));
                ret += (uint8)(0x80 | ( (c>>12) & 0x3f));
                ret += (uint8)(0x80 | ( (c>>6) & 0x3f));
                ret += (uint8)(0x80 | (c & 0x3f));
            }
            else if (c < 0x4000000)
            {   // 5 bytes encoding
                ret += (uint8)(0xf8 | (c >> 24));
                ret += (uint8)(0x80 | ( (c>>18) & 0x3f));
                ret += (uint8)(0x80 | ( (c>>12) & 0x3f));
                ret += (uint8)(0x80 | ( (c>>6) & 0x3f));
                ret += (uint8)(0x80 | (c & 0x3f));
            }
            else if (c < 0x80000000)
            {   // 6 bytes encoding
                ret += (uint8)(0xfc | (c >> 30));
                ret += (uint8)(0x80 | ( (c>>24) & 0x3f));
                ret += (uint8)(0x80 | ( (c>>18) & 0x3f));
                ret += (uint8)(0x80 | ( (c>>12) & 0x3f));
                ret += (uint8)(0x80 | ( (c>>6) & 0x3f));
                ret += (uint8)(0x80 | (c & 0x3f));
            } // else invalid Unicode char, so ignored
        }
        return ret;
    }

    const uint8 * checkUTF8(const uint8 * s)
    {
        while (*s)
        {
            if (*s < 0x80) s++; // Basic ASCII
            else if ((s[0] & 0xE0) == 0xC0)
            { // 110XXXXx 10xxxxxx
                // Could be overlong
                if ((s[1] & 0xC0) != 0x80 || (s[0] & 0xFE) == 0xC0) return s;
                s += 2;
            } else if ((s[0] & 0xF0) == 0xE0)
            { // 1110XXXX 10Xxxxxx 10xxxxxx
                if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[0] == 0xE0 && (s[1] & 0xE0) == 0x80) // Is overlong?
                ||  (s[0] == 0xED && (s[1] & 0xE0) == 0xA0) // Has surrogate ?
                ||  (s[0] == 0xEF && s[1] == 0xBF && (s[2] & 0xFE) == 0xBE)) // Or invalid code point like FFFE or FFFF
                    return s;
                s += 3;
            } else if ((s[0] & 0xF8) == 0xF0)
            { // 11110XXX 10XXxxxx 10xxxxxx 10xxxxxx
                if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80 || (s[0] == 0xF0 && (s[1] & 0xF0) == 0x80) // Is overlong ?
                ||  (s[0] == 0xF4 && s[1] > 0x8F) || s[0] > 0xF4) // Invalid code point like > 10FFFF
                    return s;
                s += 4;
            }
            else return s; // Invalid sequence anyway for UTF-8
        }
        return 0;
    }

    void copyAndZero(void * dest, int destSize, const void * src, int srcSize)
    {
        if (!dest || !destSize) return;
        ((char*)dest)[destSize - 1] = 0;
        if (src != 0)
            memcpy(dest, src, min(destSize - 1, srcSize));

        if (srcSize < destSize)
            memset((char*)dest + srcSize, 0, destSize - srcSize);
    }

    const unsigned int findLengthWide(const wchar_t * txt)
    {
        return txt ? (unsigned int)wcslen(txt) : 0;
    }

    /** @internal This is a hack to allow optimization based on pointer/int conversion */
    union Dual
    {
        const unsigned char * ch;
        const unsigned int * c;
        unsigned long int chVal; // This is wrong for 64 bit code in general, but ok in our usage
        const char * inp;
    } dual;


    // Here is the implementation for findLength of an ASCII based string
    const unsigned int findLength(tCharPtr _input, size_t limit)
    {
#ifdef OwnStringLength

        unsigned int hasZeroByte = 0;
        const char * input = const_cast<const char *>(_input);
        if (!input) return 0;

        if (limit)
        {
            while (limit && *input) { input++; limit--; }
            return (unsigned int)(input - _input);
        }


        Dual dual;
        dual.inp = input;

        // First part, check in byte order as long as the address is not aligned
        for (; (dual.chVal & (sizeof (dual.chVal) - 1)) != 0; ++dual.ch)
            if (!*dual.ch) return (unsigned long int)(dual.inp - input);


        for (;;)
        {
            // We dereference the pointer to 32 bits data access.
            // This is not allowed in current standard, as the data might not exist at this location (however no 32 bit processor can handle non-dword aligned access)
            unsigned int v = *dual.c++;
            // This find a zero byte in a 32 bit word in only 4 operations (this can be extended to 64 bits easily)
            hasZeroByte = (v - 0x01010101UL) & ~v & 0x80808080UL;
            if (hasZeroByte)
            {
                if (!(*(dual.c - 1) & 0x000000ff)) return (unsigned int)(dual.inp - input) - 4;
                if (!(*(dual.c - 1) & 0x0000ff00)) return (unsigned int)(dual.inp - input) - 3;
                if (!(*(dual.c - 1) & 0x00ff0000)) return (unsigned int)(dual.inp - input) - 2;
                if (!(*(dual.c - 1) & 0xff000000)) return (unsigned int)(dual.inp - input) - 1;
            }
        }
#else
        return _input ? (unsigned int)strlen((const char*)_input) : 0;
#endif
        // We should never reach this point
        Platform::breakUnderDebugger();
        return 0;
    }

    struct QuotedEncoding
    {
        FastString & out;
        int lineLength;
        int lastOne;

        void writeHex(const char c)
        {
            // Check for line cut
            if (lineLength >= 73) { out += "=\r\n"; lineLength = 3; }
	        else lineLength += 3;
	        out += FastString::Print("=%2x", c);
        }
        inline void writeASCII(const char c)
        {
            // Write out the char checking for line cut
            if (c == '\r' || c == '\n')     { out += c; lineLength = 0; }
            else if (lineLength < 75)       { out += c; ++lineLength; }
            else                            { out += "=\r\n"; out += c; lineLength = 1; }
        }

        inline void quoteChar(const char c)
        {
            if (lastOne != -1)
            {
                // CRLF is allowed on end of line
                if (lastOne == '\r' && c == '\n')
			        writeASCII((char)lastOne);
	            if (c == '\r' || c == '\n')
		            writeHex((char)lastOne);
	            else writeASCII((char)lastOne);

	            lastOne = -1;
            }
            if (c == '\t' || c == ' ') lastOne = (int)c;
            else if (c == '\r' || c == '\n' || (c > 32 && c < 127 && c != '=')) writeASCII(c);
            else writeHex(c);
        }

        QuotedEncoding(FastString & out) : out(out), lineLength(0), lastOne(-1) {}
    };
    struct QuotedDecoding
    {
        FastString & out;

        inline bool unquoteChar(const char * & input, char & out)
        {
	        int ch = *input++;
	        while (ch == '=')
	        {
		        ch = *input++;
		        // Filter CRLF
		        if (ch == '\r') ch = *input++;
#define IsHex(ch)	(ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f')
		        else if (IsHex(ch))
		        {
		            int hexValue = ch <= '9' ? (ch - '0') : (ch <= 'F' ? ch - 'A' + 10 : ch - 'a' + 10);
			        ch = *input++;
			        if (IsHex(ch))
#undef IsHex
			        {
				        hexValue <<= 4;
				        hexValue += ch <= '9' ? (ch - '0') : (ch <= 'F' ? ch - 'A' + 10 : ch - 'a' + 10);
				        out = (char)hexValue;
				        return true;
			        }
                    return false;
		        }
		        else if (ch != '\n') return false;
		        ch = *input++;
	        }
	        out = (char)ch;
	        return true;
        }

        QuotedDecoding(FastString & out) : out(out) {}
    };

    // Quote the given string to RFC 2045 compatible format (in use in MIME messages)
    FastString quotedPrintable(const FastString & textToQuote)
    {
        FastString out; QuotedEncoding enc(out);
        const char * text = (const char *)textToQuote;
        for (int i = 0; i < textToQuote.getLength(); i++)
            enc.quoteChar(text[i]);
        return out;
    }
    // Unquote the given string from RFC 2045 compatible format (used in MIME messages)
    FastString unquotedPrintable(const FastString & textToUnquote)
    {
        FastString out; QuotedDecoding dec(out);
        const char * text = (const char *)textToUnquote, *end = text + textToUnquote.getLength();
        while (text < end)
        {
            char ch = 0;
            if (!dec.unquoteChar(text, ch)) return "";
            out += ch;
        }
        return out;
    }

}

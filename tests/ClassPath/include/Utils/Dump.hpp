#ifndef hpp_DumpToString_hpp
#define hpp_DumpToString_hpp


// We need FastStrings
#include "../Strings/Strings.hpp"

namespace Utils
{
    /** Dump the given byte array to string as an hexadecimal string */
    inline void dumpToHexString(Strings::FastString & out, const uint8 * const array, const uint32 length)
    {
        for (uint32 i = 0; i < length; i++)
            out += Strings::FastString::Print("%02X", (unsigned int)array[i]);
    }
    
    /** Dump the given byte array to string as an hexadecimal string */
    inline Strings::FastString dumpToHexString(const void * array, const uint32 length)
    {
        Strings::FastString ret; dumpToHexString(ret, (const uint8 * const)array, length); return ret;
    }

    /** Produce a nice hexdump of the given byte array.
        @param out          The hexdump is appended to this string
        @param array        The input buffer of length bytes
        @param length       The input buffer length in bytes
        @param colSize      The number of bytes dumped per line
        @param withAddress  If set, the relative address is display in the beginning of each line
        @param withCharVal  If set, a character map is displayed after each dumped line, impossible chars are replaced by '.' */
    void hexDump(Strings::FastString & out, const uint8 * const array, const uint32 length, const uint32 colSize = 16, const bool withAddress = false, const bool withCharVal = false);

    /** Dump the given byte array to string as an hexadecimal string with ASCII display on right */
    inline Strings::FastString hexDump(const void * array, const uint32 length)
    {
        Strings::FastString ret; hexDump(ret, (const uint8 * const)array, length, 128, false, true); return ret;
    }
}


#endif

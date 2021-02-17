// We need our declaration
#include "../include/Utils/Dump.hpp"

namespace Utils
{
    // Produce a nice hexdump of the given byte array
    void hexDump(Strings::FastString & out, const uint8 * const array, const uint32 length, const uint32 colSize, const bool withAddress, const bool withCharVal)
    {
        uint32 i = 0;
        for (; i < length; i++)
        {
            if ((i % colSize) == 0)
            {
                // Output the charVal if any present
                if (withCharVal && i)
                {
                    out += " ";
                    for (uint32 j = i - colSize; j < i; j++)
                    {
                        out += Strings::FastString::Print("%c", array[j] < 32 ? '.' : array[j]);
                    }
                }

                out += "\n";
                if (withAddress)
                    out += Strings::FastString::Print("%08X ", i);

            }
            out += Strings::FastString::Print("%02X ", (unsigned int)array[i]);
        }
        // Output the charVal if any present
        if (withCharVal && i)
        {
            for (uint32 k = 0; k < colSize - (i % colSize); k++)
                out += "   ";

            out += " ";
            for (uint32 j = i - (i % colSize); j < i; j++)
            {
                out += Strings::FastString::Print("%c", array[j] < 32 ? '.' : array[j]);
            }
        }
    }
}


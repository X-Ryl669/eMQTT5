#ifndef hpp_StringHash_hpp
#define hpp_StringHash_hpp

// We need types
#include "Types.hpp"

namespace Hash
{
    #undef get16bits
    #if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
      || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
    #define get16bits(d) (*((const uint16 *) (d)))
    #endif

    #if !defined (get16bits)
    #define get16bits(d) ((((uint32)(((const uint8 *)(d))[1])) << 8)\
                           +(uint32)(((const uint8 *)(d))[0]) )
    #endif


    static uint32 SuperFastHash (const char * data, int len, uint32 hash)
    {
        uint32  tmp;
        int     rem;

        if (len <= 0 || data == NULL) return 0;

        rem = len & 3;
        len >>= 2;

        /* Main loop */
        for (;len > 0; len--)
        {
            hash  += get16bits (data);
            tmp    = (get16bits (data+2) << 11) ^ hash;
            hash   = (hash << 16) ^ tmp;
            data  += 2*sizeof (uint16);
            hash  += hash >> 11;
        }

        /* Handle end cases */
        switch (rem)
        {
            case 3: hash += get16bits (data);
                    hash ^= hash << 16;
                    hash ^= data[sizeof (uint16)] << 18;
                    hash += hash >> 11;
                    break;
            case 2: hash += get16bits (data);
                    hash ^= hash << 11;
                    hash += hash >> 17;
                    break;
            case 1: hash += *data;
                    hash ^= hash << 10;
                    hash += hash >> 1;
        }

        /* Force "avalanching" of final 127 bits */
        hash ^= hash << 3;
        hash += hash >> 5;
        hash ^= hash << 4;
        hash += hash >> 17;
        hash ^= hash << 25;
        hash += hash >> 6;

        return hash;
    }
    /** Hash a byte array (usually a string as UTF8) to a 32 bits value */
    inline uint32 superFastHash (const char * data, int len) { return SuperFastHash(data, len, len); }
}

#endif

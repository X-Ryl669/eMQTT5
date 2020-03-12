#ifndef hpp_CPP_Hasher_CPP_hpp
#define hpp_CPP_Hasher_CPP_hpp

// We need deleters
#include "Container/Deleters.hpp"

namespace Container
{
    /** Hash the given key.
        The hasher interface transform a key to make searching faster.
        The identity transformer interface gives the same output as the input.
        @warning The HashKeyT must be a plain old data, or it will break, and it must support % operator
    */
    template <class KeyType>
    struct NoHashKey
    {
        /** The final hash type */
        typedef KeyType HashKeyT;
        /** The key hashing process */
        static inline HashKeyT hashKey(const KeyType & initialKey) { return initialKey; }
        /** Compare function */
        static inline bool     compareKeys(const KeyType & first, const KeyType & second) { return first == second; }
    };

    /** The hasher interface transform a key to make searching faster. */
    template <class KeyType>
    struct HashKey
    {
        /** The final hash type */
        typedef uint32 HashKeyT;

        /** The key hashing process */
        static HashKeyT hashKey(const KeyType & initialKey);
        /** Compare function */
        static inline bool compareKeys(const KeyType & first, const KeyType & second) { return first == second; }
    };

    /** Default function for uint32 */
    inline uint32 hashIntegerKey(uint32 x) { x = ((x >> 16) ^ x) * 0x45d9f3b; x = ((x >> 16) ^ x) * 0x45d9f3b; x = ((x >> 16) ^ x); return x ? (uint32)x : (uint32)1; } // Avoid 0 as it's reserved
    /** Default function for uint32 */
    inline uint32 hashIntegerKey(uint64 x) { x = ((x >> 32) ^ x) * 0x45d9f3b; x = ((x >> 16) ^ x) * 0x45d9f3b; x = ((x >> 16) ^ x) * 0x45d9f3b; x = ((x >> 16) ^ x); return x ? (uint32)x : (uint32)1; }
    /** Default function for uint32 */
    inline uint32 hashIntegerKey(uint16 x) { uint32 r = x * 0x45d9f3b; r = ((r >> 16) ^ r); return r ? r : 1; }
    /** Default function for any other integer type is to avoid hashing */
    template <typename T> inline uint32 hashIntegerKey(T x) { return x; }


    /** Implementation of the hashing policies for integers.
        @param T    must be an integer type
        You must provide a uint32 hashIntegerKey(T) overload for this to work */
    template <typename T>
    struct IntegerHashingPolicy
    {
        /** The type for the hashed key */
        typedef uint32 HashKeyT;
        /** Allow compiletime testing of the default value */
        enum { DefaultAreZero = 1 };

        /** Check if the initial keys are equal */
        static bool isEqual(const T key1, const T key2) { return key1 == key2; }
        /** Compute the hash value for the given input */
        static inline HashKeyT Hash(T x) { return hashIntegerKey(x); }
        /** Get the default hash value (this is stored by default in the buckets), it's the value that's means that the hash is not computed yet. */
        static inline HashKeyT defaultHash() { return 0; }
        /** The invalid key */
        static inline T invalidKey() { return T(0); }
        /** Reset the key value to default */
        static inline void resetKey(T & key) { key = 0; }
    };

    template <typename Str>
    struct StringHashingPolicy
    {
        /** The type for the hashed key */
        typedef uint32 HashKeyT;
        /** Allow compiletime testing of the default value */
        enum { DefaultAreZero = 1 };

        /** Check if the initial keys are equal */
        static bool isEqual(const Str & key1, const Str & key2) { return key1 == key2; }
        /** Compute the hash value for the given input */
        static inline HashKeyT Hash(const Str & x) { return HashKey<Str>::hashKey(x); }
        /** Get the default hash value (this is stored by default in the buckets), it's the value that's means that the hash is not computed yet. */
        static inline HashKeyT defaultHash() { return 0; }
        /** The invalid key */
        static inline const Str & invalidKey() { static Str empty; return empty; }
        /** Reset the key value to default */
        static inline void resetKey(Str & key) { key = ""; }
    };

    /** This is used to have const reference when applicable */
    template <typename U, bool>
    struct DirectAccess
    {
        typedef U Type;
    };

    template <typename U>
    struct DirectAccess<U, false>
    {
        typedef const U & Type;
    };
}

#endif

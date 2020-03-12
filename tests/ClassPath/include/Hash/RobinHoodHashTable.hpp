#ifndef hpp_RobinHoodHashTable_hpp
#define hpp_RobinHoodHashTable_hpp

// We need hashers declaration too
#include "Hasher.hpp"

namespace Container
{

    /** The internal table is made of these. We use the same optimization as described in RobinHood's paper */
    template <typename T, typename Key, typename HashPolicy = IntegerHashingPolicy<Key>, bool isPODKey = IsPOD<Key>::result != 0 >
    struct Bucket
    {
        /** The hask key type we need */
        typedef typename HashPolicy::HashKeyT HashKeyT;
        /** The key format we are using for our method, to avoid useless copy */
        typedef typename DirectAccess<Key, isPODKey >::Type KeyT;

        /** This bucket data */
        T           data;

        /** Get the hash for this bucket */
        inline HashKeyT getHash(void *) const { return hash; }
        /** Get the key for this bucket */
        inline KeyT getKey(void *) const { return key; }
        /** Set the hask for this bucket */
        inline void setHash(const HashKeyT h, void *) { hash = h; }
        /** Set the key for this bucket */
        inline void setKey(KeyT k, void *) { key = k; }
        /** Reset the key to the default value */
        inline void resetKey(void *) { HashPolicy::resetKey(key); }

        /** Swap the content of this bucket with the given parameters.
            @return On output, this key is set to k, this hash is set to h and data is set to value, and k is set to the previous key, h to the previous hash, value to the previous data */
        inline void swapBucket(Key & k, HashKeyT & h, T & value)
        {
            HashKeyT tmpH = hash; hash = h; h = tmpH;
            Key  tmpK = key; key = k; k = tmpK;
            T tmpD = data; data = value; value = tmpD;
        }
        /** Swap the bucket with another one */
        inline void swapBucket(Bucket & o) { swapBucket(o.key, o.hash, o.data); }


        Bucket() : hash(HashPolicy::defaultHash()) { HashPolicy::resetKey(key); }

    private:
        /** The hashed key */
        HashKeyT    hash;
        /** The key itself */
        Key         key;
    };


    /** This class implements a RobinHood hash table.
        Unlike the HashTable, this table uses open addressing, that is, it does not allocate more memory in case of collision to store a chained list in the bucket array.
        Instead, it relocates the existing entries to ensure a small proximity from the hashed position to the actual entry.
        It's called RobinHood because the basic idea is to get from the rich (entries located close to their hashed positions) to give to the poor (entries located far from their
        hashed positions). It does so by sequentially moving the items with hash collisions so their are (on average) relatively close to their expected position.

        For usage, @sa Tests::RobinHoodHashTableTests

        @warning The type must be a Plain Old Data (with no destructor), since no destructor are called, and data is moved
        @param Type         The data type. Must be very small and POD (if you need to store non POD type, use Proxy or ProxyArray type)
        @param Key          The key type.
        @param HashPolicy   Defaults to integer based key (should be small and POD too)
        @param Bucket       The bucket type to use (default to a very simple bucket type, but can be made more compact by template specialization)
        @param isPODType    True will make the table consider the type as POD and handle it by value, while false will make it handle by const reference */
    template <typename Type, typename Key = uint32, typename HashPolicy = IntegerHashingPolicy<Key>, typename Bucket = Bucket<Type, Key, HashPolicy, IsPOD<Key>::result != 0>, bool isPODType = IsPOD<Type>::result != 0 >
    class RobinHoodHashTable
    {
        // Type definitions and enumerations
    public:
        /** The hash key type we are using */
        typedef typename HashPolicy::HashKeyT HashKeyT;
        /** The type format we are using for our method, to avoid useless copy */
        typedef typename DirectAccess<Type, isPODType >::Type T;
        /** The key format we are using for our method, to avoid useless copy */
        typedef typename Bucket::KeyT KeyT;

        /** Constants used in this table */
        enum
        {
            GrowthRate = 2, //!< The default exponential grow rate
        };
        /** The generator callback function used in resize */
        typedef bool (*ResizeGenFunc)(size_t index, Type & t, Key & k, void * token);

        /** The iterator type */
        struct IterT
        {
            const RobinHoodHashTable & table;
            mutable size_t       currentPos;

            bool isValid() const { return currentPos < table.allocSize; }
            inline const IterT & operator ++() const
            {
                ++currentPos;
                while (currentPos < table.allocSize && table.table[currentPos].getHash(table.opaque) == HashPolicy::defaultHash()) ++currentPos;
                return *this;
            }
            Type * operator *() const { return currentPos < table.allocSize ? &table.table[currentPos].data : 0; }
            KeyT getKey() const { return currentPos < table.allocSize ? table.table[currentPos].getKey(table.opaque) : HashPolicy::invalidKey(); }
            void Reset() const { currentPos = (size_t)-1; operator++(); }

            IterT(const RobinHoodHashTable & table) : table(table), currentPos((size_t)-1) { operator++(); }
        };

        // Members
    private:
        /** The hash table storage */
        Bucket *        table;
        /** The load factor */
        const float     loadFactor;
        /** The current count of items in the table */
        size_t          count;
        /** The current table allocation size */
        size_t          allocSize;
        /** The maximum probing size */
        size_t          probingMaxSize;

        /** The opaque cross-bucket object */
        void *          opaque;


        // Helpers
    private:
        /** Compute the distance to the initial value */
        size_t computeDistToInit(size_t index) const
        {
            const HashKeyT hash = table[index].getHash(opaque);
            if (hash == HashPolicy::defaultHash()) return allocSize; // Impossible distance
            size_t init = hash % allocSize;
            return init <= index ? index - init : index + allocSize - init; // Warp the index
        }

        // Interface
    public:
        /** Clear the table */
        void Clear(size_t newSize = 0, void * newOpaque = 0)
        {
            opaque = newOpaque;
            count = 0;
            // Don't allow completely empty table, as it takes a lot of code to test for this case everywhere
            // It slows down everything for a degenerated case, so simply avoid this case.
            if (!newSize) newSize = 2;
            // Except for destruction (that's an hidden flag)
            if (newSize == (size_t)-1) newSize = 0;
            if (newSize == allocSize)
            {
                for (size_t i = 0; i < allocSize; i++) { table[i].~Bucket(); new (&table[i]) Bucket(); }
                return;
            }
            // Destruct
            if (table) for (size_t i = 0; i < allocSize; i++) table[i].~Bucket();
            allocSize = newSize;
            probingMaxSize = allocSize;

            free(table);
            if (newSize)
            {   // And reconstruct
                table = (Bucket*)malloc(allocSize * sizeof(Bucket));
                for (size_t i = 0; i < allocSize; i++) new (&table[i]) Bucket();
            }
        }

        /** Check if this table contains the given key */
        bool containsKey(KeyT key) { return getValue(key) != 0; }

        /** Get the value for the given key */
        Type * getValue(KeyT key) const
        {
            const HashKeyT hash = HashPolicy::Hash(key);
            size_t initPos = hash % allocSize;

            for (size_t i = 0; i < probingMaxSize; i++)
            {
                size_t current = (initPos + i) % allocSize;
                size_t probeDist = computeDistToInit(current);
                if (probeDist == allocSize || i > probeDist) break; // Not found within the given distance

                if (HashPolicy::isEqual(key, table[current].getKey(opaque))) return &table[current].data;
            }
            return 0;
        }

        /** Store a value in the table
            @param key      The key to map the data with
            @param data     The data to store in the table
            @param update   If true, the value is updated on collision */
        bool storeValue(KeyT key, T data, const bool update = false)
        {
            if ((count+1) >= allocSize * loadFactor && (update && getValue(key) == 0)) return false; // Not enough space to worth appending it

            Bucket bucket;
            HashKeyT hash = HashPolicy::Hash(key);
            bucket.setHash(hash, opaque);
            bool bucketUsed = false;
            size_t initPos = hash % allocSize;
            size_t current = 0, probeCurrent = 0;

            // Search the insertion position
            for (size_t i = 0; i < probingMaxSize; i++)
            {
                current = (initPos + i) % allocSize;
                HashKeyT currentHash = table[current].getHash(opaque);
                if (update && currentHash == hash && !bucketUsed)
                {   // Seems to exist already. Make sure it's still valid when checking keys
                    if (HashPolicy::isEqual(key, table[current].getKey(opaque)))
                    {   // Update now
                        table[current].data = data;
                        return true;
                    }
                }
                if (currentHash == HashPolicy::defaultHash())
                {   // Empty area
                    if (bucketUsed) table[current].swapBucket(bucket);
                    else
                    {
                        table[current].data = data;
                        table[current].setKey(key, opaque);
                        table[current].setHash(hash, opaque);
                    }
                    break;
                }
                size_t probeDist = computeDistToInit(current); // If the bucket is empty, the previous test used it already, so this can not happen here
                if (probeCurrent > probeDist)
                {   // Swap the current bucket with the one to insert
                    if (!bucketUsed)
                    {   // Copy is required before swapping, let's copy it now
                        bucket.data = data;
                        bucket.setKey(key, opaque);
                        bucket.setHash(hash, opaque);
                        bucketUsed = true;
                    }
                    table[current].swapBucket(bucket);
                    probeCurrent = probeDist;
                }
                probeCurrent++;
            }
            count++;
            return true;
        }

        /** Extract a value from the table. The value is forgotten from the table (done by swapping 0 with the value) */
        Type extractValue (KeyT key)
        {
            const HashKeyT hash = HashPolicy::Hash(key);
            size_t initPos = hash % allocSize;
            size_t current = 0;
            Type ret = 0;

            for(size_t i = 0; i < probingMaxSize; i++)
            {
                current = (initPos + i) % allocSize;
                size_t probeDist = computeDistToInit(current);
                if (probeDist == allocSize || i > probeDist) return ret; // Not found within the given distance

                if (HashPolicy::isEqual(key, table[current].getKey(opaque)))
                {
                    // Delete entry now
                    table[current].resetKey(opaque);
                    table[current].setHash(HashPolicy::defaultHash(), opaque);
                    ret = table[current].data;
                    table[current].data = 0;

                    // Backshift happens here
                    for (size_t j = 1; j < allocSize; j++)
                    {
                        size_t prev = (current + j - 1) % allocSize;
                        size_t swap = (current + j) % allocSize;
                        if (table[swap].getHash(opaque) == HashPolicy::defaultHash())
                        {
                            table[prev].data = 0;
                            break;
                        }
                        size_t probeDist = computeDistToInit(swap);
                        if (probeDist == allocSize) return 0; // Error in computing the probing distance
                        if (probeDist == 0)
                        {
                            table[prev].data = 0;
                            break;
                        }
                        // Swap buckets now
                        table[prev].swapBucket(table[swap]);
                    }
                    count--;
                    return ret;
                }
            }
            // Not found
            return ret;
        }

        /** This is used only in test to ensure coherency of the table */
        size_t computeSize()
        {
            size_t counter = 0;
            for(uint32 i = 0; i < allocSize; ++i)
                counter += table[i].getHash(opaque) != HashPolicy::defaultHash();
            return counter;
        }
        /** Get the current table's items count */
        size_t getSize() const { return count; }
        /** Get the current memory usage for this table (in bytes) */
        size_t getMemUsage() const { return sizeof(*this) + allocSize * sizeof(*table); }

        /** Get an iterator to this table.
            The iterator is only valid while the table is not modified. */
        IterT getFirstIterator() { return IterT(*this); }
        /** Get an iterator to this table.
            The iterator is only valid while the table is not modified. */
        const IterT getFirstIterator() const { return IterT(*this); }

        /** Ensure there is enough space for appending an element to this table and append
            This is a shortcut doing
            @code
            if (shouldResize() && !resize()) return false;
            return storeValue(key, data, update);
            @endcode
            @param key      The key to map the data with
            @param data     The data to store in the table
            @param update   If true, the value is updated on collision
            @return false on failed storage, likely because of resize issue, true on success */
        inline bool reliableStoreValue(KeyT key, T data, const bool update = false)
        {
            if (shouldResize())
            {   // Optimize the case we are simply updating
                if (update) { Type * v = getValue(key); if (v) { *v = data; return true; } }
                if (!resize()) return false;
            }
            return storeValue(key, data, update);
        }

        /** Check if the table need resizing */
        bool shouldResize() const { return (count + 1) >= allocSize * loadFactor; }


        /** Resize the table.
            This will realloc the table and call the generator to populate the table.
            Resizing is done by using the growth rate defined */
        bool resize(ResizeGenFunc generator, void * token = 0 )
        {
            // To avoid allocating twice the size here, we can add 100% to the current allocation with realloc.
            // However, no other thread should be processing the table while doing so, and we don't support this yet (RW lock will be added later on)
            // The given generator will be called to refill the table once reallocated.
            // At least on linux, realloc for big area does not make any copy so it's ok to call realloc here
            size_t oldCount = count;
            size_t newAllocSize = allocSize * GrowthRate;
            if (!newAllocSize) newAllocSize = 2;
            Bucket * prev = (Bucket*)realloc(table, newAllocSize * sizeof(Bucket));
            if (!prev) return false; // Failed to realloc larger
            table = prev;
            allocSize = newAllocSize;
            probingMaxSize = newAllocSize;
            count = 0;
            // Call constructors now
            for (size_t i = 0; i < allocSize; i++) new(&table[i]) Bucket();
            // Then fill the table
            T item; Key k;
            for (size_t i = 0; i < oldCount; i++)
            {
                if (!generator(i, item, k, token)) return false;
                if (!storeValue(k, item)) return false;
            }
            return true;
        }

        /** Resize the table.
            There are two possibilities here, here it's done automatically by running other the current set.
            The implementation here is local (so not optimal because it can use 50% more memory). To use the second method, use the other resize() method
            Resizing is done by using the growth rate defined */
        bool resize()
        {
            // We create a new table that's twice the size and insert all items in there, then swap all values with it.
            RobinHoodHashTable other(allocSize * GrowthRate, opaque);
            for(size_t i = 0; i < allocSize; ++i)
                if (table[i].getHash(opaque) != HashPolicy::defaultHash())
                {
                    if (!other.storeValue(table[i].getKey(opaque), table[i].data)) return false;
                }


            // Then we need to move the data from other to use
            free(table); table = other.table; other.table = 0;
            allocSize = other.allocSize;
            probingMaxSize = other.probingMaxSize;

            return true;
        }

        // Construction and destruction
    public:
        /** Construct a RobinHood hash table */
        RobinHoodHashTable(const size_t allocSize, void * opaque = 0)
            :  table((Bucket*)malloc(allocSize * sizeof(*table))), loadFactor(0.80), count(0),
                allocSize(allocSize), probingMaxSize(allocSize), opaque(opaque)
        {
            // We want to use realloc here, so new[] and delete[] are inaccessible here
            for (size_t i = 0; i < allocSize; i++) new(&table[i]) Bucket();
        }

        /** Default destructor */
        ~RobinHoodHashTable()
        {
            // We have used realloc, so new[] and delete[] are inaccessible here
            Clear((size_t)-1);
        }

        friend struct IterT;
    };

}


#endif

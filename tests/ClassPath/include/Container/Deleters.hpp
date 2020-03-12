#ifndef hpp_Deleters_hpp
#define hpp_Deleters_hpp

#include "Types.hpp"

namespace Container
{
    /** Default deletion for node's objects doing nothing (no deletion) */
    template <typename T>
    struct NoDeletion
    {   typedef T Type;
        inline static void deleter(T *) {}
        inline static void deleterNoRef(T *) {} };

    /** Default true deletion for node's objects  */
    template <typename T>
    struct DeletionWithDelete
    {   typedef T Type;
        inline static void deleter(T *& t) { delete t; t = 0; }
        inline static void deleterNoRef(T * t) { delete t; } };

    /** Default true deletion for node's objects with free */
    template <typename T>
    struct DeletionWithFree
    {   typedef T Type;
        inline static void deleter(T *& t) { free((void*)t); t = 0; }
        inline static void deleterNoRef(T * t) { delete t; }};

    /** Default true deletion for node's objects constructed with an array */
    template <typename T>
    struct DeletionWithDeleteArray
    {   typedef T Type;
        inline static void deleter(T *& t) { delete[] t; t = 0; }
        inline static void deleterNoRef(T * t) { delete[] t; } };

    /** Default true deletion for node's objects with delete the inner object table */
    template <typename T>
    struct DeletionWithInnerDelete
    {   typedef T* Type;
        inline static void deleter(T ** t) { delete *t; *t = 0; }
        inline static void deleterNoRef(T ** t) { delete *t; *t = 0; }};

    /** Default true deletion for node's objects with delete the inner object array */
    template <typename T>
    struct DeletionWithInnerDeleteArray
    {   typedef T Type;
        inline static void deleter(T ** t) { delete[] *t; *t = 0; }
        inline static void deleterNoRef(T ** t) { delete[] *t; *t = 0; }};

    /** The auto deleter capture the type while the storage itself is with void * */
    template <typename T>
    struct AutoDeleter
    {   typedef T Type;
        inline static void deleter(void* & t) { delete (T*)t; t = 0; }
        inline static void deleterNoRef(void* t) { delete (T*)t; }};

    /** The auto deleter capture the type while the storage itself is with void * */
    template <typename T>
    struct AutoDeleterArray
    {   typedef T Type;
        inline static void deleter(void* & t) { delete[] (T*)t; t = 0; }
        inline static void deleterNoRef(void* t) { delete[] (T*)t; }};

    /** Simple helper class that's keeping scope ordered and only delete if told so */
    template <class T> class ConditionalDelete
    {
        T * ptr;
        bool deleteOnExit;
    public:
        void deleteLater(const bool b = true) { deleteOnExit = b; }
        ConditionalDelete(T * ptr) : ptr(ptr), deleteOnExit(false) {}
        ~ConditionalDelete() { if (deleteOnExit) delete ptr; }
    };

    /** This is used to store a pointer to a data type.
        The ProxyArray class is using delete[] operator to delete the object.
        It's using move semantic (no two ProxyArray can point to the same object, and while copying
        them, the other is mutated to avoid deleting the proxied data).
        Typically, this is transparent to use in POD only Container.
        So, for example, instead of this code:
        @code
        RobinHoodHashTable<NonPOD*, int> hash;
        hash.storeValue(0, new NonPOD[2]);
        [...]
        hash.getValue(0)->someNonPODMethod();

        // Then later for destruction
        while (hash.getFirstIterator().isValid()) { delete[] hash.extractValue(hash.getFirstIterator().getKey()); }
        @endcode

        It's simplified to
        @code
        RobinHoodHashTable<ProxyArray, int> hash;
        hash.storeValue(0, new NonPOD[2]);
        // If using C++11, you can also do this
        hash.storeValue(1, new NonPOD[3] { whatever, NonPODConstructor, accept});
        // Optionally if you want to use the size() member of the ProxyArray (for serialization purpose only), you can do:
        hash.storeValue(2, ProxyArray(new NonPOD[3], 3)); // Second 3 should be the number of elements in the array same as in []
        [...]
        hash.getValue(0)->someNonPODMethod();

        // No need to destruct anything, it's done magically by the proxy array
        @endcode


        @warning There are some pattern that the ProxyArray does not like, like being manipulated from outside.
                 Typically, you should not deal with this object directly, but instead with the proxied type */
    class ProxyArray
    {
        typedef void* TPtr;

        TPtr    data;
        size_t  dataSize;
        void (*deleteFunc)(void *&);

        void del() { if (deleteFunc) (*deleteFunc)(data); }

    public:
        operator TPtr & ()  { return data; }
        operator TPtr * ()  { return &data; }
        size_t size() const { return dataSize; }

                                ProxyArray & operator = (const ProxyArray & p) { del(); deleteFunc = p.deleteFunc; data = p.data; dataSize = p.dataSize; const_cast<ProxyArray&>(p).deleteFunc = 0; return *this; }
                                ProxyArray & operator = (const TPtr t) { del(); data = t; dataSize = 0; deleteFunc = 0; return *this; }
        template <typename U>   ProxyArray & operator = (const U * u) { del(); data = u; dataSize = sizeof(U); deleteFunc = &AutoDeleterArray<U>::deleter; return *this; }

                                ProxyArray(const TPtr t = 0) : data(t), dataSize(0), deleteFunc(0) {}
                                // Move constructor, p is modified to avoid cleaning the object
                                ProxyArray(const ProxyArray & p) : data(p.data), dataSize(p.dataSize), deleteFunc(p.deleteFunc) { const_cast<ProxyArray&>(p).deleteFunc = 0; }
        template <typename U>   ProxyArray(const U * u) : data((TPtr)u), dataSize(sizeof(*u)), deleteFunc(&AutoDeleterArray<U>::deleter) {}
        template <typename U>   ProxyArray(const U * u, const int count) : data((TPtr)u), dataSize(count * sizeof(*u)), deleteFunc(&AutoDeleterArray<U>::deleter) {}
        // Because we should look like POD, it's correct to have all our member that are zero mapped
        ~ProxyArray() { del(); data = 0; dataSize = 0; deleteFunc = 0; }
    };

    /** This is used to store a pointer to a data type.
        The Proxy class is using delete operator to delete the object.
        It's using move semantic (no two ProxyArray can point to the same object, and while copying
        them, the other is mutated to avoid deleting the proxied data).
        Typically, this is transparent to use in POD only Container.
        So, for example, instead of this code:
        @code
        RobinHoodHashTable<NonPOD*, int> hash;
        hash.storeValue(0, new NonPOD);
        [...]
        hash.getValue(0)->someNonPODMethod();

        // Then later for destruction
        while (hash.getFirstIterator().isValid()) { delete hash.extractValue(hash.getFirstIterator().getKey()); }
        @endcode

        It's simplified to
        @code
        RobinHoodHashTable<Proxy<NonPOD>, int> hash;
        hash.storeValue(0, new NonPOD);
        [...]
        hash.getValue(0)->someNonPODMethod();

        // No need to destruct anything, it's done magically by the proxy
        @endcode


        @warning There are some pattern that the Proxy does not like, like being manipulated from outside.
                 Typically, you should not deal with this object directly, but instead with the proxied type */
    class Proxy
    {
        typedef void* TPtr;

        TPtr    data;
        size_t  dataSize;
        void (*deleteFunc)(void *&);

        void del() { if (deleteFunc) (*deleteFunc)(data); }

    public:
        operator TPtr & ()  { return data; }
        operator TPtr * ()  { return &data; }
        size_t size() const { return dataSize; }
        template <typename U> U * as()            { return static_cast<U*>(data); }

                                Proxy & operator = (const Proxy & p) { del(); deleteFunc = p.deleteFunc; data = p.data; dataSize = p.dataSize; const_cast<Proxy&>(p).deleteFunc = 0; return *this; }
                                Proxy & operator = (const TPtr t) { del(); data = t; dataSize = 0; deleteFunc = 0; return *this; }
        template <typename U>   Proxy & operator = (const U * u) { del(); data = u; dataSize = sizeof(U); deleteFunc = &AutoDeleter<U>::deleter; return *this; }

                                Proxy(const TPtr t = 0) : data(t), dataSize(0), deleteFunc(0) {}
                                // Move constructor, p is modified to avoid cleaning the object
                                Proxy(const Proxy & p) : data(p.data), dataSize(p.dataSize), deleteFunc(p.deleteFunc) { const_cast<Proxy&>(p).deleteFunc = 0; }
        template <typename U>   Proxy(const U * u) : data((TPtr)u), dataSize(sizeof(*u)), deleteFunc(&AutoDeleter<U>::deleter) {}
        // Because we should look like POD, it's correct to have all our member that are zero mapped
        ~Proxy() { del(); data = 0; dataSize = 0; deleteFunc = 0; }
    };
}

#endif

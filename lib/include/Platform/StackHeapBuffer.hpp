#ifndef hpp_StackHeapBuffer_hpp
#define hpp_StackHeapBuffer_hpp

// We need types like ssize_t here
#include <Types.hpp>

namespace Platform
{
    /** An object that's storing a pointer to a buffer and also retains its size in bytes.
        If the pointer is allocated on the stack, it's not freed. */
    class StackHeapBuffer
    {
        void *      ptr;
        ssize_t     size;
    
        /** Removed copy constructor */
        StackHeapBuffer(const StackHeapBuffer & other); // : ptr(other.ptr), size(other.size) { const_cast<ssize_t&>(other.size) = -1; }
    public:
        /** Construction */
        StackHeapBuffer(void * ptr, const ssize_t size) :  ptr(ptr), size(size) {}
        /** Destruction */
        ~StackHeapBuffer() { if (size > 0) { ::free(ptr); ptr = 0; } }
        /** Main operator */
        operator void * () const { return ptr; }
        /** A conversion operator */
        template <typename U>
        operator U * () const { return static_cast<U* const>(ptr); }
        /** Get the buffer size */
        size_t getSize() const { return size > 0 ? (size_t)size : (size_t)-size; }
        /** Check if the allocation is on the stack or on the heap */
        bool onStack() const { return size < 0; }
    };


    /** Convenient macro used to create a stack or heap allocated buffer.
        Unlike stack based allocator, this does not consume any stack size upfront (before) allocation, only for each allocation if it can fit in the heap (or not).
        The decision is based on a given threshold you can set.
        
        Default stack based allocator (more or less) does this:
        @verbatim
        byte array[N];
        size_t n;
        if (n + queriedAllocationSize < N) return &array[n += queriedAllocationSize];
        else return malloc(queriedAllocationSize)
        @endif
        As you see, this code consumes N bytes on your stack even if you only allocate 1 byte.

        This stack allocator does not pre-allocate and instead deal with allocation upon each call.
        It's being called like this:
        @code
            {
                DeclareStackHeapBuffer(myBuffer, n, 256); // Equivalent to: Platform::StackHeapBuffer myBuffer(n);

                // Use myBuffer like this:
                strcpy((char*)myBuffer, "something");
                // No need to free or delete the buffer (it's done for you) at end of **function** (or scope if allocated on heap).
            }
        @endcode

        @warning You must understand the limitation of these buffers:
                 When allocated on the stack, the available space is limited (=> stack overflow) so don't use these in a loop
                 Also, the space is reclaimed only when the function you are declaring it within is returning, not at the end of the current scope.
                 When allocated on the heap, the memory is reclaimed at end of the scope.
                 In all cases, you can't access the buffer after the end of the scope so it should not matter much, but it's important to know this
                 when you're computing your actual stack size usage.
        @warning No destructor are either called by these buffers. So don't store any non-POD in such buffer.

        Typically, this code is (probably) wrong:
        @code
        void myFunc()
        {
            for (int i = 0; i < 10; i++)
            {
                DeclareStackHeapBuffer(myBuffer, 256, 1024);
                ((char* const)myBuffer) [3] = 2;
            }
            // Here, the memory allocated by myBuffer is still allocated, so you ends up with 256 * 10 live allocations on your stack here
        }
        // It's only deallocated here, when the function returns
        @endcode */
    #define DeclareStackHeapBuffer(name, size, threshold) \
        Platform::StackHeapBuffer name(size <= threshold ? ::alloca(size) : ::malloc(size), size <= threshold ? -(ssize_t)size : (ssize_t)size)

}

#endif

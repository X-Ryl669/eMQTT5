#ifndef hpp_eMQTTTypes_hpp
#define hpp_eMQTTTypes_hpp



// Configure the typical macros
#if __linux == 1
    #define _LINUX 1
#endif
#if __APPLE__ == 1
    #define _MAC 1
#endif


// Check whether we can include <atomic>
#if defined(__clang__)
    #if __has_include( <atomic> )
        #define HAS_STD_ATOMIC 1
    #else
        #define HAS_STD_ATOMIC 0
    #endif
#endif

#if __cplusplus >= 201103L
    #define HAS_STD_ATOMIC 1
    #define HasCPlusPlus11 1
#endif
#if __cplusplus >= 201500L
    #define HasCPlusPlus17 1
#endif

// Safety checks for this configuration
#include <atomic> 


// Try to find out the endianness of the target system (this might fail)
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    #define IsBigEndian 1
#elif defined(__BIG_ENDIAN__) || defined(__BIG_ENDIAN) || defined(_BIG_ENDIAN)
    #define IsBigEndian 1
#else
    #define IsBigEndian 0
#endif

// Don't test against Linux and Mac each time we need a Posix system
#if (_LINUX == 1) || (_MAC == 1)
    #define _POSIX 1
#endif

#ifndef _WIN32
    #define InlinePlatformCode
#endif


#ifdef _WIN32
    #ifndef WINVER
        #define WINVER  0x600
    #endif
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x600
    #endif
    // We don't care about the unreferenced formal parameter warning
    #pragma warning(disable: 4100)
    // Don't want Windows to include its own min/max macro
    #define NOMINMAX
    #include <winsock2.h>
    // Remove the mess Microsoft left after it in rpcndr.h
    #undef small
    #undef hyper
    #include <windows.h>
    #include <Mmsystem.h>
    #include <Nb30.h>
    #include <ws2tcpip.h>
    #include <tchar.h>
    #include <iphlpapi.h>
    // Remove conflicting winnt.h definition
    #undef DELETE

    #if defined(_MSC_VER)
        #include <intrin.h>

        // This is required, because depending on the current RUNNING kernel (Vista vs XP),
        // either the function is present in kernel32.dll or not. So we provide our own when they are not.
        extern unsigned __int64 (*PTRInterlockedCompareExchange64)(volatile unsigned __int64 *, unsigned __int64, unsigned __int64);
        extern unsigned __int64 (*PTRInterlockedExchange64)(volatile unsigned __int64 *, unsigned __int64);
        extern unsigned __int64 (*PTRInterlockedExchangeAdd64)(volatile unsigned __int64 *, unsigned __int64);
        extern unsigned __int64 (*PTRInterlockedIncrement64)(volatile unsigned __int64 *);
        extern unsigned __int64 (*PTRInterlockedDecrement64)(volatile unsigned __int64 *);
    #endif
#elif defined(_POSIX)
    #include <arpa/inet.h>
    #include <dirent.h>
    #include <errno.h>
    #include <fcntl.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <pthread.h>
    #include <signal.h>
    #include <sys/poll.h>
    #include <sys/socket.h>
    #include <sys/time.h>
    #include <sys/types.h>
    #include <unistd.h>
    #include <sys/ptrace.h>
    #include <aio.h>
    #include <ifaddrs.h>
  #if defined(_LINUX)
    #include <semaphore.h>
    typedef sem_t sig_sem;
    #define SemInit(s, c) sem_init((sem_t*)&s, 0, c)
    #define SemPost(s)    sem_post(&s)
    #define SemWait(s)    sem_wait(&s)
    #define SemDestroy(s) sem_destroy((sem_t*)&s);
  #elif defined(_MAC)
    #include <sys/sysctl.h>
    #include <sys/stat.h>
    #include <sys/uio.h>
    #include <net/route.h>
    #include <net/if_dl.h>
    #include <mach/semaphore.h>

    typedef semaphore_t sig_sem;
    #define SemInit(s, c) semaphore_create(mach_task_self(), (semaphore_t*)&s, SYNC_POLICY_FIFO, c)
    #define SemPost(s)    semaphore_signal(s)
    #define SemWait(s)    semaphore_wait(s)
    #define SemDestroy(s) semaphore_destroy(mach_task_self(), s)
  #endif
#endif


#if defined(_WIN32) || defined(_POSIX)
    #include <limits.h>
    #include <stdlib.h>
    #include <memory.h>
    #include <new>
    #include <stdarg.h>
    #include <stdio.h>
    #include <string.h>
    #include <wchar.h>
#endif

#ifdef _WIN32
    #ifndef DontWantUINT8
        typedef unsigned char uint8;
    #endif
    #ifndef DontWantUINT32
        typedef unsigned int uint32;
    #endif
    #ifndef DontWantUINT16
        typedef unsigned short uint16;
    #endif
    #ifndef DontWantUINT64
        typedef unsigned __int64 uint64;
    #endif
    #ifndef DontWantINT8
        typedef signed char int8;
    #endif
    #ifndef DontWantINT32
        typedef signed int int32;
    #endif
    #ifndef DontWantINT16
        typedef signed short int16;
    #endif
    #ifndef DontWantINT64
        typedef signed __int64 int64;
    #endif
    #ifndef DontWantNativeInt
        typedef intptr_t nativeint;
    #endif
    #define PF_LLD  "%I64d"
    #define PF_LLU  "%I64u"

    /** Unless you run on embedded OS, you'll not need those functions
        @return int value you need to pass to leaveAtomicSection */
    #define enterAtomicSection() 1
    /** Unless you run on embedded OS, you'll not need those functions */
    #define leaveAtomicSection(X) do {} while(0)

#elif defined(_POSIX)
    #include <sys/types.h>
    #ifndef DontWantUINT8
        typedef u_int8_t uint8;
    #endif
    #ifndef DontWantUINT32
        typedef u_int32_t uint32;
    #endif
    #ifndef DontWantUINT16
        typedef u_int16_t uint16;
    #endif
    #ifndef DontWantUINT64
        typedef u_int64_t uint64;
    #endif
    #ifndef DontWantINT8
        typedef int8_t int8;
    #endif
    #ifndef DontWantINT32
        typedef int32_t int32;
    #endif
    #ifndef DontWantINT16
        typedef int16_t int16;
    #endif
    #ifndef DontWantINT64
        typedef int64_t int64;
    #endif
    #ifndef DontWantNativeInt
        typedef intptr_t nativeint;
    #endif

    #define PF_LLD  "%lld"
    #define PF_LLU  "%llu"

    /** Unless you run on embedded OS, you'll not need those functions
        @return int value you need to pass to leaveAtomicSection */
    #define enterAtomicSection() 1
    /** Unless you run on embedded OS, you'll not need those functions */
    #define leaveAtomicSection(X) do {} while(0)

#elif defined(ESP_PLATFORM)
    #include <stdlib.h>
    #include <string.h>
    // We need BSD socket here and TCP_NODELAY
    #include <sys/socket.h>
    // We need gethostbyname or getaddrinfo
    #include <netdb.h>
    #include <stdarg.h>

    #ifndef DontWantUINT8
        typedef unsigned char uint8;
    #endif
    #ifndef DontWantUINT32
        typedef unsigned int uint32;
    #endif
    #ifndef DontWantUINT16
        typedef unsigned short uint16;
    #endif

    #ifndef DontWantUINT64
        typedef unsigned long long uint64;
    #endif

    #ifndef DontWantINT8
        typedef signed char int8;
    #endif
    #ifndef DontWantINT32
        typedef signed int int32;
    #endif
    #ifndef DontWantINT16
        typedef signed short int16;
    #endif

    #ifndef DontWantINT64
        typedef long long int64;
    #endif
    #ifndef DontWantNativeInt
        typedef intptr_t nativeint;
    #endif

    #define PF_LLD  "%lld"
    #define PF_LLU  "%llu"

    /** Unless you run on embedded OS, you'll not need those functions
        @return int value you need to pass to leaveAtomicSection */
    extern "C" int enterAtomicSection();
    /** Unless you run on embedded OS, you'll not need those functions */
    extern "C" void leaveAtomicSection(int);

#else
    // Prevent the types.h file to define the fd_set type (stupid definition by the way)
    #define __USE_W32_SOCKETS
    #include <stdio.h>
    #include <time.h>
    #include <stdlib.h>
    #include <string.h>
    #include <wchar.h>
    #include "pthreadRTOS.h"
    #include "lwip/sockets.h"

    /* lwIP includes */
    #include "lwip/tcpip.h"
    #include "lwip/netif.h"
    #include "lwip/ip_addr.h"
    #include "lwip/debug.h"
    #include "lwip/debug.h"

    #include "socket_map.h"
    #include "bsp/inc/hw_types.h"
    #include "driverlib/rom_map.h"
    #include "driverlib/interrupt.h"


    #ifndef DontWantUINT8
        typedef unsigned char uint8;
    #endif
    #ifndef DontWantUINT32
        typedef unsigned int uint32;
    #endif
    #ifndef DontWantUINT16
        typedef unsigned short uint16;
    #endif

    #ifndef DontWantUINT64
        typedef unsigned long long uint64;
    #endif

    #ifndef DontWantINT8
        typedef signed char int8;
    #endif
    #ifndef DontWantINT32
        typedef signed int int32;
    #endif
    #ifndef DontWantINT16
        typedef signed short int16;
    #endif

    #ifndef DontWantINT64
        typedef long long int64;
    #endif
        #ifndef DontWantNativeInt
        typedef intptr_t nativeint;
    #endif

    #define PF_LLD  "%lld"
    #define PF_LLU  "%llu"

    /** Unless you run on embedded OS, you'll not need those functions
        @return int value you need to pass to leaveAtomicSection */
    extern "C" int enterAtomicSection();
    /** Unless you run on embedded OS, you'll not need those functions */
    extern "C" void leaveAtomicSection(int);
#endif

#ifndef MQTTString
  #include <string>
  #define MQTTString    std::string
#endif
#ifndef MQTTROString
  #if HasCPlusPlus17 == 1
    #include <string_view>
    #define MQTTROString    std::string_view
  #else
    class MQTTROString
    {
        const char * d;
        const size_t l;
    public:
        const char * data() const { return d; }
        size_t length() const { return l; }
        MQTTROString(const char * d = 0, const size_t l = 0) : d(d), l(l) {}
        MQTTROString(const MQTTROString & o) : d(o.d), l(o.l) {}
    };
  #endif
#endif
#ifndef MQTTStringGetData
  #define MQTTStringGetData(X) X.data()
  #define MQTTStringGetLength(X) X.length()
#endif

#ifdef _WIN32
    typedef HANDLE              HTHREAD;
    typedef HANDLE              HMUTEX;
    typedef HANDLE              OEVENT;
#else
    typedef pthread_t           HTHREAD;
    typedef pthread_mutex_t     HMUTEX;
    typedef pthread_mutex_t     OEVENT;
#endif

#if MinimalFootPrint == 0
    /** Thread local storage modifier.
        @warning Apple until XCode 8 does not support C++11 thread_local specifier, so it falls back to the C's __thread which in turn does not call destructors.
                 So, in general, you should not mark a non POD object with this flag */
    #if HasCPlusPlus11 == 1 && __apple_build_version__ > 7030029
        #define TLSDecl  thread_local
    #elif defined(_MSC_VER)
        #define TLSDecl  __declspec(thread)
    #elif defined(_POSIX)
        #define TLSDecl __thread
    #else
        // It's not per thread, but since it's neither any case above, well...
        #define TLSDecl SORRY_THIS_PLATFORM_DOES_NOT_SUPPORT_THREADS
    #endif
#else
    #define TLSDecl 
#endif

#ifndef min
    template <typename T>
        inline T min(T a, T b) { return a < b ? a : b; }
    template <typename T>
        inline T max(T a, T b) { return a > b ? a : b; }
    template <typename T>
        inline T clamp(T a, T low, T high) { return a < low ? low : (a > high ? high : a); }
    template <typename T, size_t N >
        inline bool isInArray(const T & a, T (&arr)[N]) { for(size_t i = 0; i < N; i++) if (arr[i] == a) return true; return false; }
    template <typename T>
        inline void Swap(T & a, T & b) { T tmp = a; a = b; b = tmp; }
    #define minDefined
#endif

// Helper function that should never be omitted
inline uint8 BigEndian(uint8 a) { return a; }

// Easy method to get a int as big endian in all case
inline uint32 BigEndian(uint32 a)
{
    static unsigned long signature= 0x01020304UL;
    if (4 == (unsigned char&)signature) // little endian
        return ((a & 0xFF000000) >> 24) | ((a & 0x00FF0000) >> 8) | ((a & 0xFF00) << 8) | ((a & 0xFF) << 24);
    if (1 == (unsigned char&)signature) // big endian
        return a;
    // Error here
    return 0;
}
// Easy method to get a int as big endian in all case
inline uint16 BigEndian(uint16 a)
{
    static unsigned long signature= 0x01020304UL;
    if (4 == (unsigned char&)signature) // little endian
        return (a >> 8) | ((a & 0xFF) << 8);
    if (1 == (unsigned char&)signature) // big endian
        return a;
    return 0;
}
// Easy method to get a int as big endian in all case
inline uint64 BigEndian(uint64 a)
{
    static unsigned long signature= 0x01020304UL;
    if (4 == (unsigned char&)signature) // little endian
    {
        return ((a & 0x00000000000000ffULL) << 56) | ((a & 0x000000000000ff00ULL) << 40)
             | ((a & 0x0000000000ff0000ULL) << 24) | ((a & 0x00000000ff000000ULL) << 8)
             | ((a & 0x000000ff00000000ULL) >> 8)  | ((a & 0x0000ff0000000000ULL) >> 24)
             | ((a & 0x00ff000000000000ULL) >> 40) | ((a & 0xff00000000000000ULL) >> 56);
    }
    if (1 == (unsigned char&)signature) // big endian
        return a;
    return 0;
}
/** Round up the given number to the given word size
    @param x        The number to round up
    @param wordSize The word size in bytes to round up to */
inline size_t Monsanto(const size_t x, const size_t wordSize = 4) { return (x + wordSize - 1) & ~(wordSize - 1); }




#ifdef _MSC_VER
    #define Deprecated(X) __declspec(deprecated) X
    #define Aligned(X) __declspec(align (X))
    #define ForcedInline(X) __forceinline X
    #define Unused(X) UNREFERENCED_PARAMETER(X)
    #define Hidden
#elif defined(__GNUC__)
    #define Deprecated(X) X __attribute__ ((deprecated))
    #define Aligned(X) __attribute__ ((aligned (X)))
    #define ForcedInline(X) X __attribute__ ((always_inline))
    #define Unused(X) X __attribute__ ((unused))
    #define Hidden    __attribute__ ((visibility ("hidden")))
#else
    #define Deprecated(X) X
    #define Aligned(X)
    #define ForcedInline(X) X
    #define Unused(X) X
    #define Hidden 
#endif
#define ForceUndefinedSymbol(x) void* __ ## x ## _fp =(void*)&x;

#if HasCPlusPlus11 == 1
    #define Final final
#else
    #define Final
#endif

#ifndef DontWantFreeHelpers
    /** Free a pointer and zero it */
    template <typename T> inline void free0(T*& t) { free(t); t = 0; }
    /** Delete a pointer and zero it */
    template <typename T> inline void delete0(T*& t) { delete t; t = 0; }
    /** Delete a pointer to an array and zero it */
    template <typename T> inline void deleteA0(T*& t) { delete[] t; t = 0; }
    /** Delete a pointer to an array, zero it, and zero the elements count too */
    template <typename T, typename U> inline void deleteA0(T*& t, U & size) { delete[] t; t = 0; size = 0; }
    /** Delete all items of an array, delete the array, zero it, and zero the elements count too */
    template <typename T, typename U> inline void deleteArray0(T*& t, U & size) { for (U i = 0; i < size; i++) delete t[i]; delete[] t; t = 0; size = 0; }
    /** Delete all array items of an array, delete the array, zero it, and zero the elements count too */
    template <typename T, typename U> inline void deleteArrayA0(T*& t, U & size) { for (U i = 0; i < size; i++) delete[] t[i]; delete[] t; t = 0; size = 0; }

namespace Private
{
    // If the compiler stop here, you're actually trying to figure out the size of an pointer and not a compile-time array.
    template< typename T, size_t N >
    char (&ArraySize_REQUIRES_ARRAY_ARGUMENT(T (&)[N]))[N];

    // You can have default derivation from this structure
    struct Empty {};

    template <typename T>
    struct Alignment
    {
        // C++ standard requires struct's member to be aligned to the largest member's alignment requirement
        struct In { char p; T q; };
        enum { value = sizeof(In) - sizeof(T) };
    };
}

#define ArrSz(X) sizeof(Private::ArraySize_REQUIRES_ARRAY_ARGUMENT(X))
#ifndef ArraySize
   #define ArraySize ArrSz
#endif

#define AlignOf(X) Private::Alignment<X>::value
#endif


#ifndef TypeDetection_Impl
#define TypeDetection_Impl
template <typename T> struct IsPOD { enum { result = 0 }; };
template <typename T> struct IsPOD<T*> { enum { result = 1 }; };
template <typename T> struct IsNumber { enum { result = 0 }; };
template <typename T> class ZeroInit { T value; public: operator T&() { return value; } operator const T&() const { return value; } ZeroInit(T v = 0) : value(v) {} };

#define MakePOD(X) template <> struct IsPOD<X > { enum { result = 1 }; }; \
                   template <> struct IsPOD<const X > { enum { result = 1 }; }; \
                   template <> struct IsNumber<X > { enum { result = 1 }; }; \
                   template <> struct IsNumber<const X > { enum { result = 1 }; }

#define MakeIntPOD(X) template <> struct IsPOD<signed X > { enum { result = 1 }; }; \
                      template <> struct IsPOD<unsigned X > { enum { result = 1 }; }; \
                      template <> struct IsPOD<const signed X > { enum { result = 1 }; }; \
                      template <> struct IsPOD<const unsigned X > { enum { result = 1 }; }; \
                      template <> struct IsNumber<signed X > { enum { result = 1 }; }; \
                      template <> struct IsNumber<unsigned X > { enum { result = 1 }; }; \
                      template <> struct IsNumber<const signed X > { enum { result = 1 }; }; \
                      template <> struct IsNumber<const unsigned X > { enum { result = 1 }; }


MakePOD(bool);
MakeIntPOD(int);
MakeIntPOD(long);
MakeIntPOD(char);
MakeIntPOD(long long);
MakeIntPOD(short);
MakePOD(double);
MakePOD(long double);
MakePOD(float);

#undef MakePOD
#undef MakeIntPOD
#endif

#ifndef DontWantSafeBool
/** Useful Safe bool idiom.
    @param Derived      The current class that should be "bool" convertible
    @param Base         If provided, it makes the complete stuff derives from this class, thus avoiding multiple inheritance
    @code
       // Before
       struct A { bool operator !() const; ... };
       struct B : public C { bool operator !() const; ... };

       // Use like this :
       struct A : public SafeBool<A> { bool operator !() const; ... };
       struct B : public SafeBool<B, C> { bool operator !() const; ... };

       A a;
       if (a) printf("It does!\n");
    @endcode */
template <class Derived, class Base = Private::Empty>
class SafeBool : public Base
{
    void badBoolType() {}; typedef void (SafeBool::*badBoolPtr)();
public:
    /** When used like in a bool context, this is called */
    inline operator badBoolPtr() const { return !static_cast<Derived const&>( *this ) ? 0 : &SafeBool::badBoolType; }
};
#endif


#define WantFloatParsing 1


#endif

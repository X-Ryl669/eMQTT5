// We need our declaration
#include <Platform/Platform.hpp>

#ifdef _WIN32

namespace Platform
{
    struct LargePageSupport
    {
    private:
        static SIZE_T & getLargePageSize() { static SIZE_T largePageSize = 0; return largePageSize; }
    public:
        static SIZE_T getSize()
        {
            static bool initied = false;
            SIZE_T & largePageSize = getLargePageSize();
            if (!initied)
            {
                HANDLE hToken = 0;
                if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
                {
                    LUID luid;
                    if (LookupPrivilegeValue(NULL, TEXT("SeLockMemoryPrivilege"), &luid))
                    {
                        TOKEN_PRIVILEGES tp;

                        tp.PrivilegeCount = 1;
                        tp.Privileges[0].Luid = luid;
                        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

                        AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), 0, 0);
                    }

                    CloseHandle(hToken);
                }

                {
                    if (HMODULE hKernel = GetModuleHandle(TEXT("kernel32.dll")))
                    {
                        typedef SIZE_T (WINAPI * GetLargePageMinimumProcT)();

                        GetLargePageMinimumProcT largePageMinimumProc = (GetLargePageMinimumProcT)GetProcAddress(hKernel, "GetLargePageMinimum");
                        if (largePageMinimumProc != NULL)
                        {
                            largePageSize = largePageMinimumProc();
                            if ((largePageSize & (largePageSize - 1)) != 0) largePageSize = 0;
                        }
                    }
                }
                initied = true;
            }
            return largePageSize;
        }
        
        static inline SIZE_T roundSize(size_t size)
        {
            return (size + getLargePageSize() - 1) & (~(getLargePageSize() - 1));
        }
    };


    void * malloc(size_t size, const bool largeAccess)
    {
        if (largeAccess)
        {
            if ((LargePageSupport::getSize() != 0) && (size >= 256 * 1024))
            {
                void * address = ::VirtualAlloc(0, LargePageSupport::roundSize(size), MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
                if (address != NULL) return address;
            }
            return ::VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);
        }
        return ::malloc(size);
    }

    void * calloc(size_t elementNumber, size_t size, const bool largeAccess)
    {
        if (largeAccess)
        {
            if ((LargePageSupport::getSize() != 0) && (size >= 256 * 1024))
            {
                void * address = ::VirtualAlloc(0, LargePageSupport::roundSize(size), MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
                if (address != NULL) return address;
            }
            return ::VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);
        }
        return ::calloc(elementNumber, size);
    }

    void free(void * ptr, const bool largeAccess)
    {
        if (largeAccess) ::VirtualFree(ptr, 0, MEM_RELEASE);
        else ::free(ptr);
    }
    void * realloc(void * p, size_t size)
    {
        return ::realloc(p, size);
    }
        
    const char * getProcessName()
    {
        static char * processName;
        if (!processName)
        {
            char outBuffer[1024];
            DWORD procSize = GetModuleFileNameA(NULL, outBuffer, 1024);
            if (procSize)
                processName = strdup(outBuffer); // It's ugly, it leaks, but it's small.
        }
        return processName;
    }
}

extern "C"
{
    #pragma warning(push)
    #pragma warning(disable : 4035) // disable no-return warning
    inline unsigned __int64 _OurInterlockedCompareExchange64(volatile unsigned __int64 *dest, unsigned __int64 exchange, unsigned __int64 comperand)
    {
        //value returned in eax::edx
        __asm
        {
            lea esi, comperand;
            lea edi, exchange;

            mov eax,  [esi];
            mov edx, 4[esi];
            mov ebx,  [edi];
            mov ecx, 4[edi];
            mov esi, dest;
            //lock CMPXCHG8B [esi] is equivalent to the following except
            //that it's atomic:
            //ZeroFlag = (edx:eax == *esi);
            //if (ZeroFlag) *esi = ecx:ebx;
            //else edx:eax = *esi;
            lock CMPXCHG8B [esi];
        }
    }
    #pragma warning(pop)

    inline unsigned __int64 _OurInterlockedExchange64(volatile unsigned __int64 * pu64, unsigned __int64 u64)
    {
        __asm
        {
            mov     ebx, dword ptr [u64]
            mov     ecx, dword ptr [u64 + 4]
            mov     edi, pu64
            mov     eax, dword ptr [edi]
            mov     edx, dword ptr [edi + 4]
        retry:
            lock cmpxchg8b [edi]
            jnz retry
            mov     dword ptr [u64], eax
            mov     dword ptr [u64 + 4], edx
        }
        return u64;
    }

    inline unsigned __int64 _OurInterlockedExchangeAdd64(volatile unsigned __int64 * pu64, unsigned __int64 u64)
    {
        uint64 oldVal, expectedVal;
        do
        {
            oldVal = *pu64;
            expectedVal = oldVal + u64;
        } while (_OurInterlockedCompareExchange64(pu64, expectedVal, oldVal) != expectedVal);
        return expectedVal;
    }

    inline unsigned __int64 _OurInterlockedIncrement64(volatile unsigned __int64 * pu64)
    {
        return _OurInterlockedExchangeAdd64(pu64, 1);
    }

    inline unsigned __int64 _OurInterlockedDecrement64(volatile unsigned __int64 * pu64)
    {
        return _OurInterlockedExchangeAdd64(pu64, (uint64)-1);
    }
}
unsigned __int64 (*PTRInterlockedCompareExchange64)(volatile unsigned __int64 *, unsigned __int64, unsigned __int64);
unsigned __int64 (*PTRInterlockedExchange64)(volatile unsigned __int64 *, unsigned __int64);
unsigned __int64 (*PTRInterlockedExchangeAdd64)(volatile unsigned __int64 *, unsigned __int64);
unsigned __int64 (*PTRInterlockedIncrement64)(volatile unsigned __int64 *);
unsigned __int64 (*PTRInterlockedDecrement64)(volatile unsigned __int64 *);

#pragma warning(push)
#pragma warning(disable:4074)
#pragma init_seg(compiler)

struct InitFunc
{
    InitFunc()
    {
        HMODULE hMod = GetModuleHandleW(L"KERNEL32.DLL");
        if (LOBYTE(LOWORD(GetVersion())) < 6 || hMod == NULL)
        {
            PTRInterlockedCompareExchange64 = _OurInterlockedCompareExchange64;
            PTRInterlockedExchange64 = _OurInterlockedExchange64;
            PTRInterlockedExchangeAdd64 = _OurInterlockedExchangeAdd64;
            PTRInterlockedIncrement64 = _OurInterlockedIncrement64;
            PTRInterlockedDecrement64 = _OurInterlockedDecrement64;
        } else
        {
            PTRInterlockedCompareExchange64 = (unsigned __int64 (*)(volatile unsigned __int64 *, unsigned __int64, unsigned __int64))GetProcAddress(hMod, "InterlockedCompareExchange64");
            PTRInterlockedExchange64 = (unsigned __int64 (*)(volatile unsigned __int64 *, unsigned __int64))GetProcAddress(hMod, "InterlockedExchange64");
            PTRInterlockedExchangeAdd64 = (unsigned __int64 (*)(volatile unsigned __int64 *, unsigned __int64))GetProcAddress(hMod, "InterlockedExchangeAdd64");
            PTRInterlockedIncrement64 = (unsigned __int64 (*)(volatile unsigned __int64 *))GetProcAddress(hMod, "InterlockedIncrement64");
            PTRInterlockedDecrement64 = (unsigned __int64 (*)(volatile unsigned __int64 *))GetProcAddress(hMod, "InterlockedDecrement64");
        }

    }
} atomicFuncMaker;

#pragma warning(pop)
    /*
   LONG  __cdecl _InterlockedIncrement(LONG volatile *Addend);
   LONG  __cdecl _InterlockedDecrement(LONG volatile *Addend);
   LONG  __cdecl _InterlockedCompareExchange(LPLONG volatile Dest, LONG Exchange, LONG Comp);
   LONG  __cdecl _InterlockedExchange(LPLONG volatile Target, LONG Value);
   LONG  __cdecl _InterlockedExchangeAdd(LPLONG volatile Addend, LONG Value);

   LONGLONG  __cdecl _InterlockedIncrement64(LONGLONG volatile *Addend);
   LONGLONG  __cdecl _InterlockedDecrement64(LONGLONG volatile *Addend);
   LONGLONG  __cdecl _InterlockedCompareExchange64(LONGLONG volatile *Dest, LONGLONG Exchange, LONGLONG Comp);
   LONGLONG  __cdecl _InterlockedExchange64(LONGLONG volatile *Target, LONGLONG Value);
   LONGLONG  __cdecl _InterlockedExchangeAdd64(LONGLONG volatile *Addend, LONGLONG Value);
}
//    #pragma intrinsic(_InterlockedCompareExchange64, _InterlockedExchange64, _InterlockedExchangeAdd64, _InterlockedIncrement64, _InterlockedDecrement64)
*/

#endif

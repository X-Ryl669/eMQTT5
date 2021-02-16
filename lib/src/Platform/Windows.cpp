// We need our declaration
#include <Platform/Platform.hpp>
// We need string code too
#include "../../include/Strings/Strings.hpp"
// We need locks too
#include "../../include/Threading/Lock.hpp"

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
    
    bool queryHiddenInput(const char * prompt, char * buffer, size_t & size)
    {
        DWORD amount, mode = 0;
        // Don't allow multiple thread from running here
        static Threading::Lock lock;
        Threading::ScopedLock scope(lock);
        
        HANDLE hstdin = GetStdHandle(STD_INPUT_HANDLE), hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hstdin == INVALID_HANDLE_VALUE || (hstdout == INVALID_HANDLE_VALUE && prompt && prompt[0] != 0))
        {
            ::MessageBoxA(NULL, "This application requires a console.\r\nPlease restart the application from a command line\r\n(type WindowsKey + R, then 'cmd.exe')", "Error", MB_OK | MB_ICONERROR);
            return false;
        }
        
        if (GetConsoleMode(hstdin, &mode) == FALSE) return false;
        if (SetConsoleMode(hstdin, ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT) == FALSE)
            return false;
        // Write prompt
        Strings::ReadOnlyUnicodeString promptUni = Strings::convert(Strings::FastString(prompt));
        if (WriteConsoleW(hstdout, promptUni.getData(), wcslen(promptUni.getData()), &amount, NULL) == FALSE)
            return false;
        
        wchar_t * inBuffer = (wchar_t*)malloc(size * sizeof(*inBuffer));
        if (ReadConsoleW(hstdin, inBuffer, size, &amount, NULL) == FALSE)
        {   free(inBuffer); return false; }
        
        Strings::FastString input = Strings::convert(Strings::ReadOnlyUnicodeString(inBuffer, amount));
        free(inBuffer);
        
        memset(buffer, 0, size);
        memcpy(buffer, (const char*)input, ((size_t)input.getLength() < size - 1 ? (size_t)input.getLength() : size - 1));
        size = (size_t)input.getLength() < size ? (size_t)input.getLength() : size;
        
        return SetConsoleMode(hstdin, mode) != FALSE;
    }
    
    const char * getProcessName()
    {
        static Strings::FastString processName;
        if (!processName)
        {
            wchar_t outBuffer[1024];
            DWORD procSize = GetModuleFileNameW(NULL, outBuffer, 1024);
            if (procSize)
            {
                processName = Strings::convert(Strings::ReadOnlyUnicodeString(outBuffer, procSize)).fromLast("\\");
            }
        }
        return processName;
    }
    
    DynamicLibrary::DynamicLibrary(const char * pathToLibrary)
        : handle(0)
    {
        Strings::ReadOnlyUnicodeString path = Strings::convert(Strings::FastString(pathToLibrary));
        handle = (void*)LoadLibraryW(path.getData());
    }
    DynamicLibrary::~DynamicLibrary()
    {
        if (handle) FreeLibrary((HMODULE)handle); handle = 0;
    }
    
    // Load the given symbol out of this library
    void * DynamicLibrary::loadSymbol(const char * nameInUTF8) const
    {
        if (handle && nameInUTF8)
        {
            return (void*)GetProcAddress((HMODULE)handle, nameInUTF8);
        }
        return 0;
    }
    // Get the platform expected file name for the given library name
    void DynamicLibrary::getPlatformName(const char * libraryName, char * outputName)
    {
        if (!libraryName || !outputName) return;
        strcpy(outputName, libraryName);
        strcat(outputName, ".dll"); // On windows, it's the most common name
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

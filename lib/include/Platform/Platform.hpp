#ifndef hpp_CPP_Platform_CPP_hpp
#define hpp_CPP_Platform_CPP_hpp

// Types like size_t or NULL
#include <Types.hpp>

/** The platform specific declarations */
namespace Platform
{
    /** The end of line marker */
    enum EndOfLine
    {
        LF   =   1,     //!< The end of line is a line feed (usually 10 or "\n")
        CR   =   2,     //!< The end of line is a carriage return (usually 13 or "\r")
        CRLF =   4,     //!< The end of line is both CR and LF ("\r\n")

#ifdef _WIN32
        Default = CRLF,
#else
        Default = LF,
#endif

        Any         =   0x7,   //!< Any end of line is accepted
        AutoDetect  =   0x8,   //!< Auto detect end of line, stop on either "\r" or "\n", if stopping on "\r", eat the next "\n" if found.
    };

#ifdef _WIN32
#define PathSeparator  "\\"
#else
#define PathSeparator  "/"
#endif
    
    /** File separator char */
    enum 
    {
#ifdef _WIN32
        Separator = '\\'
#else
        Separator = '/'
#endif
    };

#ifdef InlinePlatformCode
    // When being build in embedded code, don't pay for a wrapper here, instead forward to the system's functions
    inline void * malloc(size_t size)                   { return ::malloc(size); }
    inline void * calloc(size_t a, size_t b)            { return ::calloc(a,b); }
    inline void free(void * p)                          { return ::free(p); }
    inline void * realloc(void * p, size_t size)        { return ::realloc(p, size); }

    /** Get the current process name.
        This does not rely on remembering the argv[0] since this does not exists on Windows.
        This returns the name of executable used to run the process */
    inline const char * getProcessName()
    {
        static char * processName = NULL;
        if (!processName)
        {
  #ifdef _LINUX
            FILE * f = fopen("/proc/self/cmdline", "r");
            if (f) {
                char buffer[256];
                processName = strdup(fgets(buffer, 256, f));
                fclose(f);
            }
  #elif defined(_MAC)
            processName = strdup(getprogname());
  #else
            return "program";
  #endif
        }
        return processName;
    }
#else
    /** The simple malloc overload.
        If you need to use another allocator, you should define this method 
        @param size         Element size in bytes
        @param largeAccess  If set, then optimized functions are used for large page access.
                            Allocation for large access should call free with large access. */
    void * malloc(size_t size, const bool largeAccess = false);
    
    /** The simple calloc overload.
        If you need to use another allocator, you should define this method 
        @param elementCount  How many element to allocate
        @param size          One element size in bytes
        @param largeAccess  If set, then optimized functions are used for large page access.
                            Allocation for large access should call free with large access. */
    void * calloc(size_t elementCount, size_t size, const bool largeAccess = false);
    /** A simpler version of calloc, with only one size specified */
    inline void * zalloc(size_t size, const bool largeAccess = false) { return calloc(1, size, largeAccess); }
    /** The simple free overload.
        If you need to use another allocator, you should define this method 
        @param p     A pointer to an area to return to the heap 
        @param largeAccess  If set, then optimized functions are used for large page access.
                            Allocation for large access should call free with large access. */
    void free(void * p, const bool largeAccess = false);
    /** The simple realloc overload. 
        If you need to use another allocator, you should define this method 
        @param p    A pointer to the allocated area to reallocate
        @param size The required size of the new area in bytes
        @warning Realloc is intrinsically unsafe to use, since it can leak memory in most case, use safeRealloc instead */
    void * realloc(void * p, size_t size);
    /** Get the current process name.
        This does not rely on remembering the argv[0] since this does not exists on Windows.
        This returns the name of executable used to run the process */
    const char * getProcessName();
#endif

    /** The safe realloc method.
        This method avoid allocating a zero sized byte array (like realloc(0, 0) does).
        It also avoid leaking memory as a code like (ptr = realloc(ptr, newSize) 
        (in case of error) does). */
    inline void * safeRealloc(void * p, size_t size) 
    {
        if (p == 0 && size == 0) return 0;
        if (size == 0)
        {
            free(p);
            return 0; // On FreeBSD realloc(ptr, 0) frees ptr BUT allocates a 0 sized buffer.
        }
        void * other = realloc(p, size);
        if (size && other == NULL)
            free(p); // Reallocation fails, let's free the previous pointer

        return other;
    }
    
    inline bool isUnderDebugger()
    {
#if (DEBUG==1)
    #ifdef _WIN32
        return (IsDebuggerPresent() == TRUE);
    #elif defined(_LINUX)
        static signed char testResult = 0;
        if (testResult == 0)
        {
            testResult = (char) ptrace (PT_TRACE_ME, 0, 0, 0);
            if (testResult >= 0)
            {
                ptrace (PT_DETACH, 0, (caddr_t) 1, 0);
                testResult = 1;
            }
        }
        return (testResult < 0);
    #elif defined (_MAC)
        static signed char testResult = 0;
        if (testResult == 0)
        {
            struct kinfo_proc info;
            int m[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
            size_t sz = sizeof (info);
            sysctl (m, 4, &info, &sz, 0, 0);
            testResult = ((info.kp_proc.p_flag & P_TRACED) != 0) ? 1 : -1;
        }

        return testResult > 0;
    #elif defined (NEXIO)
        return true;
    #endif
#endif
        return false;
    }
    
    /** This is used to trigger the debugger when called */
    inline void breakUnderDebugger()
    {
      
#if (DEBUG==1)
        if(isUnderDebugger())
    #ifdef _WIN32
            DebugBreak();
    #elif defined(_LINUX)
            raise(SIGTRAP);
    #elif defined (_MAC)
            __asm__("int $3\n" : : );
    #elif defined (NEXIO)
            __asm("bkpt");
    #else
            #error Put your break into debugger code here
    #endif
#endif
    }
    
#if defined(_POSIX) || defined(_DOXYGEN)
    /** Useful RAII class for Posix file index */
    class FileIndexWrapper
    {
        int fd;

    public:
        /** So it can be used in place of usual int */
        inline operator int() const { return fd; }
        /** Mutate the file descriptor with a new descriptor. It closes the previous descriptor. */
        inline void Mutate(int newfd) { if (fd >= 0) close(fd); fd = newfd; }
        /** Forget the file descriptor */
        inline int Forget() { int a = fd; fd = 0; return a; }
        /** Check if reading is possible on the file descriptor without blocking */
        inline bool isReadPossible(const int timeoutMs)
        {
            fd_set fds; FD_ZERO(&fds); FD_SET(fd, &fds); int ret = 0;
            struct timeval tv = { timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
            while ((ret = ::select(fd+1, &fds, NULL, NULL, &tv)) == -1 && errno == EINTR) { tv.tv_sec = timeoutMs / 1000; tv.tv_usec = (timeoutMs % 1000) * 1000; }
            return ret == 1;
        }

        FileIndexWrapper(int fd) : fd(fd) {}
        ~FileIndexWrapper() { if (fd >= 0) close(fd); fd = -1; }
    };
    
#endif

}

#endif

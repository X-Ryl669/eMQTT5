#ifndef hpp_CPP_Platform_CPP_hpp
#define hpp_CPP_Platform_CPP_hpp
// Types like size-t or NULL
#include "../Types.hpp"

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
    /** Ask for a hidden input that'll be stored in the UTF-8 buffer.
        This requires a console. 
        Under Windows, this requires the process to be run from a command line.
        This is typically required for asking a password. 
        New line are not retained in the output, if present.
        
        @param prompt   The prompt that's displayed on user console 
        @param buffer   A pointer to a buffer that's at least (size) byte large 
                        that'll be filled by the function
        @param size     On input, the buffer size, on output, it's set to the used buffer size 
        @return false if it can not hide the input, or if it can't get any char in it  */
    bool queryHiddenInput(const char * prompt, char * buffer, size_t & size);
    /** Get the current process name.
        This does not rely on remembering the argv[0] since this does not exists on Windows.
        This returns the name of executable used to run the process */
    const char * getProcessName();
	
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
    
    /** Turn the current process into a daemon. 
        Log will be redirected to syslog service, 
        Input and output file descriptor will be closed, and we'll detach from the 
        running terminal.
        @warning If you intend to run a server or anything that does file-manipulation, please remember
                 that this is forking and the parent must call _exit() or std::quick_exit() and not exit() or return from main.
                 In the later case, the destructors will likely modify the file descriptors of the shared resources (with the child 
                 daemon) and lead to hard to debug issues.
                 
        @param pathToPIDFile    The path to the file containing the daemon PID (useful for system script typically)
        @param syslogName       The name of the syslog reported daemon
        @param parent           On parent process will be set to true, and false in child process
        @return false for forking error   */
    bool daemonize(const char * pathToPIDFile, const char * syslogName, bool & parent);

    /** Drop super user privileges.
        After calling this function, the real id are the effective id and saved id.
        Ancillary groups are also dropped.
        @warning You must check the return for this function else if your program is compromised, you might leave escalation issues.
        @param dropUserID       If true, the real user ID will be used to overrride all other user ID
        @param dropGroupID      If true, the read group ID will be used to override all other group ID
        @return true on success. */
    bool dropPrivileges(const bool dropUserID = true, const bool dropGroupID = true);
#endif

    /** This structure is used to load some code dynamically from a file on the filesystem */
    class DynamicLibrary
    {
        // Members
    private:
        /** The library internal handle */
        void * handle;
        
        // Interface
    public:
        /** Load the given symbol out of this library
            @param nameInUTF8   The name of the symbol. It's up to the caller to ensure cross platform name are used 
            @return A pointer on the loaded symbol, or 0 if not found */
        void * loadSymbol(const char * nameInUTF8) const;
        /** Load a symbol and cast it to the given format.
            @param nameInUTF8   The name of the symbol. It's up to the caller to ensure cross platform name are used 
            @sa loadSymbol */
        template <class T>
        inline T loadSymbolAs(const char * nameInUTF8) const { return reinterpret_cast<T>(loadSymbol(nameInUTF8)); }
        /** Get the platform expected file name for the given library name 
            @param libraryName   The name of the library, excluding suffix (like .DLL, or .so).
            @param outputName    A pointer to a buffer that at least 10 bytes larger than the libraryName buffer. */
        static void getPlatformName(const char * libraryName, char * outputName);
        /** Check if the library has loaded correctly */
        inline bool isLoaded() const { return handle != 0; }
    
        // Construction and destruction
    public:
        /** The constructor */
        DynamicLibrary(const char * pathToLibrary);
        /** The destructor */
        ~DynamicLibrary();
    };
}

#endif

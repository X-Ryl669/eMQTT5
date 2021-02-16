// We need our declaration
#include "../include/Platform/Platform.hpp"
// We need locks too
#include "../include/Threading/Lock.hpp"
// We need Logger too
#include "../include/Logger/Logger.hpp"

#ifdef _POSIX
#include <termios.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef _LINUX
#include <grp.h>
#endif


namespace Platform
{
    void * malloc(size_t size, const bool)
    {
        return ::malloc(size);
    }
    
    void * calloc(size_t elementNumber, size_t size, const bool)
    {
        return ::calloc(elementNumber, size);
    }
   
    void free(void * p, const bool)
    {
        ::free(p);
    }
    void * realloc(void * p, size_t size)
    {
        return ::realloc(p, size);
    }
    
    bool queryHiddenInput(const char * prompt, char * buffer, size_t & size)
    {
        struct termios oflags, nflags;
        
        // Don't allow multiple thread from running here
        static Threading::Lock lock;
        Threading::ScopedLock scope(lock);

        // Disabling echo
        FILE *in = fopen("/dev/tty", "r+"), *out = in;
        if (in == NULL)
        {
            in = stdin;
            out = stderr;
        }

        tcgetattr(fileno(in), &oflags);
        nflags = oflags;
        nflags.c_lflag &= ~ECHO;
        nflags.c_lflag |= ECHONL;

        if (tcsetattr(fileno(in), TCSANOW, &nflags) != 0
            || fputs(prompt, out) < 0
            || fflush(out) != 0
            || fgets(buffer, size, stdin) == NULL
            || tcsetattr(fileno(stdin), TCSANOW, &oflags) != 0)
        {
            if (in != stdin) fclose(in);
            return false;
        }
 
        if (in != stdin) fclose(in);
        
        size = strlen(buffer);
        if (size && buffer[size - 1] == '\n')
            buffer[--size] = 0;
            
        return true;
    }
    
    const char * getProcessName()
    {
        static Strings::FastString processName;
        if (!processName)
        {
#ifdef _LINUX
            FILE * f = fopen("/proc/self/cmdline", "r");
            if (f) {
                char buffer[256];
                processName = fgets(buffer, 256, f);
                fclose(f);
            }
#else
            processName = getprogname();
#endif
        }
        return processName;
    }
    
    DynamicLibrary::DynamicLibrary(const char * pathToLibrary)
        : handle(dlopen(pathToLibrary, RTLD_LAZY))
    {
        
    }
    
    DynamicLibrary::~DynamicLibrary()
    {
        if (handle) dlclose(handle); handle = 0;
    }
    
    
    // Load the given symbol out of this library
    void * DynamicLibrary::loadSymbol(const char * nameInUTF8) const
    {
        if (handle && nameInUTF8) return dlsym(handle, nameInUTF8);
        return 0;
    }
    // Get the platform expected file name for the given library name
    void DynamicLibrary::getPlatformName(const char * libraryName, char * outputName)
    {
        if (!libraryName || !outputName) return;
        strcpy(outputName, libraryName);
        strcat(outputName, ".so"); // On Mac OSX both .bundle and .so are valid, so let's use .so
    }
    
    
    /** The output sink to the error console */
    struct SyslogSink : public Logger::OutputSink
    {
        // Members
    private:
        Threading::Lock lock;
    public:
        virtual void gotMessage(const char * message, const unsigned int flags)
        {
            if (logMask & flags)
            {
                Threading::ScopedLock scope(lock);
                int level = LOG_INFO;
                if ((flags & Logger::Dump) > 0)       level = LOG_DEBUG;
                if ((flags & Logger::Warning) > 0)    level = LOG_WARNING;
                if ((flags & Logger::Error) > 0)      level = LOG_ERR;
                syslog(level, "%s", (const char*)message);
            }
        }
        SyslogSink(unsigned int logMask, const char * daemonName) : OutputSink(logMask)
        {
            openlog(daemonName, LOG_PID, LOG_DAEMON);
        }
        ~SyslogSink() { closelog(); }
    };
    

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#ifdef DEBUG
#define OUTLOG(X, Y, ...)  printf(Y "\n", ##__VA_ARGS__) // , __VA_ARGS)
#else
#define OUTLOG(X, Y, ...)  syslog(X, Y, ##__VA_ARGS__)
#endif
    static volatile bool childReady = false;

    // The child signal handler
    static void childSigHandler(int signum)
    {
        switch(signum)
        {
        case SIGALRM: childReady = false; break;
        case SIGUSR1: childReady = true;  break;
        case SIGCHLD: childReady = false; break;
        default: break;
        }
    }

    struct RemovePIDFile
    {
        const char * pidFile;
        RemovePIDFile(const char * pidFile) : pidFile(pidFile) {}
        ~RemovePIDFile() { unlink(pidFile); }
    };
    static RemovePIDFile & autoRemovePIDFile(const char * pidFile) { static RemovePIDFile i(pidFile); return i; }
    static bool isDaemonInstalled = false;

    bool daemonize(const char * pidFile, const char * syslogName, bool & isParent)
    {
        // Set up the logger to use
        Logger::setDefaultSink(new SyslogSink(Logger::getDefaultSink().logMask, syslogName));
        Logger::log(Logger::Content, "Starting %s", syslogName);
    
        isParent = true;

        // Check if already a daemon
        if (isDaemonInstalled && getppid() == 1)
        {
            const char * ttyName = ctermid(NULL);
            int devtty = open(ttyName, O_RDWR);
            if (devtty < 0)
            {
                Logger::log(Logger::Content, "Seems like %s is already a daemon, the tty name is %s", syslogName, ttyName);
                return true;
            }
            close(devtty);
        }

        // Trap signals that we expect to recieve
        signal(SIGCHLD, childSigHandler); signal(SIGUSR1, childSigHandler); signal(SIGALRM, childSigHandler);

        // Fork off the parent process
        childReady = false;
        pid_t pid = fork();
        if (pid < 0)
        {
            Logger::log(Logger::Error, "Unable to fork daemon, code=%d (%s)", errno, strerror(errno));
            return false;
        }
        // If we got a good PID, then we can exit the parent process.
        if (pid > 0)
        {
            // Wait for confirmation from the child via SIGUSR1 or SIGCHLD
            uint32 sleepTime = 0;
            while (sleepTime < 2000 && !childReady)
            {
                usleep(100000);
                sleepTime += 100;
            }
            return childReady;
        }

        // At this point we are executing as the child process
        isParent = false;
        if (pidFile && pidFile[0])
        {
            FILE * f = fopen(pidFile, "w");
            fprintf(f, "%d", getpid());
            fclose(f);
            (void)autoRemovePIDFile(pidFile);
        }

        // Cancel certain signals
        signal(SIGCHLD, SIG_DFL); // A child process dies
        // Various TTY signals
        signal(SIGTSTP, SIG_IGN); signal(SIGTTOU, SIG_IGN); signal(SIGTTIN, SIG_IGN);
        // Ignore hangup
        signal(SIGHUP,  SIG_IGN);

        // Change the file mode mask, create new session for child, and change to root folder to avoid holding references
        umask(0);
        if (setsid() < 0)
        {
            Logger::log(Logger::Error, "Unable to create new session, code=%d (%s)", errno, strerror(errno));
            return false;
        }

        if ((chdir("/")) < 0)
        {
            Logger::log(Logger::Error, "Unable to reset current directory, code=%d (%s)", errno, strerror(errno));
            return false;
        }

        // Redirect standard files to /dev/null
        freopen( "/dev/null", "r", stdin); freopen( "/dev/null", "w", stdout); freopen( "/dev/null", "w", stderr);

        // Tell the parent process that we have started
        kill(getppid(), SIGUSR1);
        isDaemonInstalled = true;
        return true;
    }

    bool dropPrivileges(const bool userID, const bool groupID)
    {
        gid_t newgid = getgid(), oldgid = getegid();
        uid_t newuid = getuid(), olduid = geteuid();

        // We need to drop ancillary groups while we are root as they can be used back to regain root
        if (!olduid && groupID) setgroups(1, &newgid);

        if (groupID && newgid != oldgid)
        {
            if (setgid(newgid) == -1) return false;
        }

        if (userID && newuid != olduid)
        {
            if (setuid(newuid) == -1) return false;
        }
        // Verify we can get back to full privileges
        if (groupID && newgid != oldgid && (setegid(oldgid) != -1 || getegid() != newgid)) return false;
        if (userID && newuid != olduid && (seteuid(olduid) != -1 || geteuid() != newuid)) return false;
        return true;
    }

}

#endif

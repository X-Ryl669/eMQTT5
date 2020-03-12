#ifndef hpp_Logger_hpp_
#define hpp_Logger_hpp_

#include <stdio.h>
#include <time.h>
#ifdef _WIN32
  #include <share.h>
#endif

// We need the minimal declaration
#include "LoggerMinimal.hpp"
// We need locks to allow multithreading logging to file
#include "Threading/Lock.hpp"
// We need strings too
#include "Strings/Strings.hpp"


namespace Logger
{
    /** The logger output sink interface */
    struct OutputSink
    {
        /** The allowed mask to log */
        const unsigned int logMask;
        /** Check if a given message should go through the current mask */
        virtual bool checkFlags(const unsigned int flags) const
        {
            return getVerbosity(flags) <= VerbosityLevel && (logMask & flags);
        }
        /** Get an UTF-8 message, without end-of-line, to sink to output */
        virtual void gotMessage(const char * message, const unsigned int flags) = 0;
        /** Required virtual destructor */
        virtual ~OutputSink() {};

        /** Define the log mask while creating */
        OutputSink(const unsigned int logMask) : logMask(logMask) {}
    };

    /** The output sink to the console */
    struct ConsoleSink : public OutputSink
    {
        // Members
    private:
        Threading::Lock lock;
    public:
        virtual void gotMessage(const char * message, const unsigned int flags)
        {
            if (checkFlags(flags))
            {
                Threading::ScopedLock scope(lock);
                #if defined(NEXIO)
                    dbg_printf("%s\n", (const char*)message);
                #elif defined(_WIN32)
                    OutputDebugStringA((const char*)message);
                    OutputDebugStringA("\n");
                #else
                    fprintf(stdout, "%s\n", (const char*)message);
                #endif
            }
        }
        ConsoleSink(const unsigned int logMask) : OutputSink(logMask) {}
    };

#if defined(_WIN32) || defined(_POSIX)
    /** The Tee sink.
        This redirect the logs to up to two different sinks.
        Because each sink can have its own log flags, you can have a console and a file sink only capturing whatever you want. */
    struct TeeSink : public OutputSink
    {
        OutputSink * sinks[2];
        bool         ownSinks;

        virtual void gotMessage(const char * message, const unsigned int flags)
        {
            if (sinks[0]) sinks[0]->gotMessage(message, flags);
            if (sinks[1]) sinks[1]->gotMessage(message, flags);
        }
        TeeSink(OutputSink * first, OutputSink * second) : OutputSink(0), ownSinks(true) { sinks[0] = first; sinks[1] = second; }
        TeeSink(OutputSink & first, OutputSink & second) : OutputSink(0), ownSinks(false) { sinks[0] = &first; sinks[1] = &second; }
        ~TeeSink() { if (ownSinks) { delete0(sinks[0]); delete0(sinks[1]); } }
    };

  #ifdef _WIN32
    /** The output sink to the debug console.
        To see this error sink, you need either to run the software under Visual Studio's debugger, or have DebugView installed and running. */
    struct DebugConsoleSink : public OutputSink
    {
        // Members
    private:
        Threading::Lock lock;
    public:
        virtual void gotMessage(const char * message, const unsigned int flags)
        {
            if (checkFlags(flags))
            {
                Threading::ScopedLock scope(lock);
                OutputDebugStringA((const char*)message);
                OutputDebugStringA("\r\n");
            }
        }
        DebugConsoleSink(const unsigned int logMask) : OutputSink(logMask) {}
    };
  #else
    typedef ConsoleSink DebugConsoleSink;
  #endif

    /** The output sink to the error console.
        Under POSIX and Win32 system, this writes the log to the error's file descriptor (that is, what's under stderr FILE pointer). */
    struct ErrorConsoleSink : public OutputSink
    {
        // Members
    private:
        Threading::Lock lock;
    public:
        virtual void gotMessage(const char * message, const unsigned int flags)
        {
            if (checkFlags(flags))
            {
                Threading::ScopedLock scope(lock);
                fprintf(stderr, "%s\n", (const char*)message);
            }
        }
        ErrorConsoleSink(unsigned int logMask) : OutputSink(logMask) {}
    };

  #if defined(_WIN32) || defined(_POSIX)
    /** The output sink to a file.
        Output the logs to a file for either appending or replacing.
        No limit whatsoever is used, so you can fill your entire hard drive with such logger. */
    struct FileOutputSink : public OutputSink
    {
        // Members
    private:
        FILE *          file;
        Threading::Lock lock;

        // OutputSink interface
    public:
        virtual void gotMessage(const char * message, const unsigned int flags);
        FileOutputSink(unsigned int logMask, const Strings::FastString & fileName, const bool appendToFile = true) : OutputSink(logMask), file(fopen(fileName, appendToFile ? "ab" : "wb")) {}
        ~FileOutputSink() { Threading::ScopedLock scope(lock); if (file) fclose(file); file = 0; }
    };

    /** Structured output sink.
        This sink captures time and flag for each log output.
        It detects repetitions and avoid writing them (adding a "repeated x times" marker in the log).
        It also alternates automatically between two output files so that each file are never bigger than the given break size.
        This means that depending on your log amount, only a small history is kept whose size is based on bytes.
        This is a basic "log rotation" feature, without any bells and whistles. */
    struct StructuredFileOutputSink : public OutputSink
    {
        // Members
    private:
        Threading::Lock lock;

        // Used to rotate the log
        Strings::FastString baseFileName;
        int  breakSize;
        int  currentSize;
        bool flipFlop;
        int                 lastMessageCount;
        Strings::FastString lastMessage;
        uint32              lastTime;
        unsigned int        lastFlags;
        FILE *  file;

        // Helpers
    private:
        /** Flush the last message */
        void flushLastMessage();

        // OutputSink interface
    public:
        /** Get a message to log */
        virtual void gotMessage(const char * message, const unsigned int flags);
        /** Build a structured output sink for file.
            @param logMask      The log mask to filter logs
            @param fileName     The file to log into. The name is used as a base name which is rotated
                                every breakSize bytes are output.
                                Typically, for a log called "test.log", the rotated files will be called
                                "test.0.log" then "test.1.log" then "test.0.log" (and not "test.log").
                                The initial log file ("test.log"), if already above the breakSize is erased.
            @param appendToFile If true, the specified log file is open for appending, else it's erased.
                                If the log file is already bigger than the breakSize, it's erased whatever the state of this flag
            @param breakSize    The number of byte allowed to fit in the log file. */
        StructuredFileOutputSink(unsigned int logMask, const Strings::FastString & fileName, const bool appendToFile = true, const int breakSize = 2*1024*1024);
        ~StructuredFileOutputSink() { if (file) { flushLastMessage(); fclose(file); } }
    };
  #endif // Files
#endif // Win32 & Linux

    /** Set the sink to use */
    extern void setDefaultSink(OutputSink * newSink);
    /** Get a reference on the currently selected default sink */
    extern OutputSink & getDefaultSink();

/*
    static void dlog(const char * file, const int line, const unsigned int flags, const char * format, ...)
    {
        char buffer[2048];
        va_list argp;
        va_start(argp, format);
        vsprintf(buffer, format, argp);
        va_end(argp);
        if (defaultSink)
            defaultSink->gotMessage(buffer, flags);
#ifdef _WIN32 // Send output to debug anyway
        OutputDebugStringA((const char*)buffer);
        OutputDebugStringA("\r\n");
#endif
    }

#if _MSC_VER <= 1200
    #define log Logger::rlog
#else
    #if (defined DEBUG)
        #define log(flag, format, ...) dlog(__FILE__, __LINE__, flag, format, __VA_ARGS__)
    #else
        #define log(flag, format, ...) rlog(flag, format, __VA_ARGS__)
    #endif

#endif
    */

}

#endif

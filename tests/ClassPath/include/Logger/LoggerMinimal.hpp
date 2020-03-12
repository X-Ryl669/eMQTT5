#ifndef hpp_LoggerMinimal_hpp_
#define hpp_LoggerMinimal_hpp_

/** If you intend to log something to a console, or a file, or both, this is where to look for */
namespace Logger
{
    /** The allowed flags to filter upon.
        When using Logger::log, you need to "tag" your message with some flags which are checked against the current application's mask.
        If they fit the mask, the log message will go through.  */
    enum Flags
    {
        Error       =   0x00000001,   //!< A typical error
        Warning     =   0x00000002,   //!< A typical warning
        File        =   0x00000004,   //!< The log is related to file operations
        Network     =   0x00000008,   //!< The log is related to network operations
        Directory   =   0x00000010,   //!< The log is related to directory operations
        Cache       =   0x00000020,   //!< The log is related to cache operations
        Content     =   0x00000040,   //!< The log is related to content operations
        Function    =   0x00000080,   //!< Typically used to show user-specified function
        Dump        =   0x00000100,   //!< Probably the most verbose log
        Creation    =   0x00000200,   //!< Log related to creating objects
        Deletion    =   0x00000400,   //!< Log related to deleting objects
        Timeout     =   0x00000800,   //!< Log related to time outs
        Connection  =   0x00001000,   //!< Log related to connections (network / local)
        Tests       =   0x00002000,   //!< Special Tests class for some logs
        Database    =   0x00004000,   //!< Logs database queries (warning, this can be a security risk)
        Config      =   0x00008000,   //!< Config related logs
        Crypto      =   0x00010000,   //!< Crypto based logs (warning, this can be a security risk)
        Packet      =   0x00020000,   //!< Log Packet related operations

        // Verbosity
        VerboseLow  =   0x00000000,   //!< Low verbosity
        VerboseMed  =   0x10000000,   //!< Medium verbosity
        VerboseHigh =   0x20000000,   //!< High verbosity
        VerboseHype =   0x30000000,   //!< Super high verbosity

        // Compound

        AllFlags    =   0x0FFFFFFF,
    };

    /** Check a given verbosity level */
    inline unsigned int getVerbosity(const unsigned int flags) { return (unsigned int)((flags & 0x30000000) >> 28); }

#ifndef VerbosityLevel
  /** If not defined yet, need to set the minimum verbosity we are accepting */
  #define VerbosityLevel 0
#endif


    /** If using these macros, then the logs might be silented in non-debug build.
        Check is done with the verbosity level, if it does not match, the complete code is removed */
#if DEBUG == 1
  #if (VerbosityLevel >= 1)
    #define VerbLogMed(X, F, ...)  Logger::log(X | Logger::VerboseMed, F, __VA_ARGS__)
  #else
    #define VerbLogMed(X, F, ...)  do {} while(0)
  #endif
  #if (VerbosityLevel >= 2)
    #define VerbLogHigh(X, F, ...) Logger::log(X | Logger::VerboseHigh, F, __VA_ARGS__)
  #else
    #define VerbLogHigh(X, F, ...) do {} while(0)
  #endif
  #if (VerbosityLevel >= 3)
    #define VerbLogHype(X, F, ...) Logger::log(X | Logger::VerboseHype, F, __VA_ARGS__)
  #else
    #define VerbLogHype(X, F, ...) do {} while(0)
  #endif
#else
  #define VerbLogMed(X, F, ...)  do {} while(0)
  #define VerbLogHigh(X, F, ...) do {} while(0)
  #define VerbLogHype(X, F, ...) do {} while(0)
#endif

    /** This is the main function for logging any information to the selected sink
        You'll use it like any other printf like function.
        @param flags    Any combination of the Logger::Flags value (the sink will check its own mask against these flags to allow logging or not)
        @param format   The printf like format */
    void log(const unsigned int flags, const char * format, ...);
}

#endif

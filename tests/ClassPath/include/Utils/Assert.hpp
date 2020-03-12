#ifndef hpp_Assert_hpp
#define hpp_Assert_hpp

// We need types
#include "Types.hpp"
// We need logger too
#include "Logger/LoggerMinimal.hpp"

#if (DEBUG==1)
    namespace Platform { void breakUnderDebugger(); }
    
    #define Assert(X)     do { if (!(X)) { \
                                 Logger::log(Logger::Error, "%s(%d) : Failed assertion %s", __FILE__, __LINE__, #X); \
                                 Platform::breakUnderDebugger(); \
                               }} while(0)

#else 
    #define Assert(X)     do { (void)sizeof((X)); } while(0)
#endif

#include "StaticAssert.hpp"

#endif

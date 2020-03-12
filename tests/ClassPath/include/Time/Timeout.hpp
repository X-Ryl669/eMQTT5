#ifndef hpp_Timeout_hpp
#define hpp_Timeout_hpp

// We need getTimeWithBase declaration
#include "Time.hpp"

namespace Time
{
    enum
    {
        DefaultTimeOut = 3000, //!< The default timeout in millisecond
        Infinite = 2147483647, //!< The maximum time maps to around 24 days. If you need more time, you should redesign your code to handle shorter timeout anyway
    };

    /** A General purpose timeout object.
        This can be used to track the amount of time the different operations have used.
        It can be used to limit operations time also on the interfaces that support it.
        The interface does not respect usual const standard to ease development.
        This allows such code:
        @code
            void funcWithTimeout(arg a1, arg a2, const TimeOut & timeout = DefaultTimeout);
            // This is correct (but you ignore the result of the timeout tracking and use default time out)
            funcWithTimeout(a1, a2);
            // If you need to set the timeout
            funcWithTimeout(a1, a2, 200); // 200ms of timeout
     
            // If you care about underlying errors or timeout event
            TimeOut out(200); // 200ms of timeout
            funcWithTimeout(a1, a2, out);
            if (out.timedOut()) { log(Error, "Timed out or error"); }
            else { log(Dump, "Func took %dms to complete", 200 - out); }
        @endcode 
        
        When writing a function supporting timeout, you'll write it this way:
        @code
            void myFunc(const TimeOut & timeout = DefaultTimeout)
            {
                // Perform some lengthy operation that returns 0 on timeout or < 0 on error (as most standard function does)
                int ret = lengthyCode(..., (int)timeout);
     
                timeout.filterError(ret); // Use the internal method to filter error
                if (timeout <= 0) return; // Use int operator to access the remaining time
     
                // Another example, if the lengthy operation is not standard reporting
                // Please notice that the timeout object here accumulated the time spent in the former operation
                // so the complete myFunc execution time will likely be close to timeout.
                while (!done && !timeout.timedOut()) { progress(); }     
            }
        @endcode
        */
    class TimeOut
    {
        // Members
    private:
        /** The remaining time. Negative values are used to indicate error */
        mutable int remainingTime;
        /** The last checked time */
        mutable uint32 lastCheckedTime;
        /** Check if we need to autorefil the timeout on successful operation */
        const bool autoRefill;

        // Helpers
    private:
        /** Get the number of millisecond elapsed */
        int elapsedMs() const { uint32 now = getTimeWithBase(1000); int ret = now - lastCheckedTime; lastCheckedTime = now; return ret; }

        // Interface
    public:
        /** Convert this timeout to a number in milliseconds */
        operator int() const { return remainingTime; }
        /** Called when we succeeded our operation */
        void success() const { if (!autoRefill && remainingTime > 0) { remainingTime -= elapsedMs(); if (remainingTime < 0) remainingTime = 0; } }
        /** Called when failed. This is a convenience function to use this object as a error tracker, errorCode should be negative */
        void failed(int errorCode) const { remainingTime = errorCode; }
        /** Use the given error code to figure out if we need to store an error or not */
        void filterError(int error) const { if (error <= 0) remainingTime = error; else success(); }
        /** One of the internal operation timed out. This is mainly used to track the total timeout expired status */
        bool timedOut() const { success(); return remainingTime <= 0; }
        /** Check if this timeout is auto refilled */
        bool isAutoRefilled() const { return autoRefill; }

        // Construction and destruction
    public:
        /** Construct a timeout with the given millisecond count */
        TimeOut(const int millisecond, const bool autoRefill = false)
            : remainingTime(millisecond), lastCheckedTime(getTimeWithBase(1000)), autoRefill(autoRefill)
        {}
    };
}

#endif

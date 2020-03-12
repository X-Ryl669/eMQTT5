#ifndef hpp_Time_hpp
#define hpp_Time_hpp

// We need base types
#include "Types.hpp"
// We need strings too
#include "Strings/Strings.hpp"

namespace Time
{
    /** Get the current time as number of second since Epoch */
    double getPreciseTime();

#ifdef _WIN32
    /** Convert the given time from FILETIME */
    double convert(const FILETIME & ft);
    /** Convert the given time to FILETIME */
    void convert(const double time, FILETIME & ft);
#endif
}    

#ifdef _WIN32
struct timeval_std
{
    time_t tv_sec;
    long   tv_usec;
};

extern "C" int gettimeofday(struct timeval_std *tv, struct timezone *tz);

#endif

/** Time related functionality is declared here */
namespace Time
{
#ifdef _WIN32
    /** Windows declare an erroneous timeval structure, where the tv_sec member is 32 bits and not the same size as time_t */
    typedef struct ::timeval_std TimeVal;
#else
    /** Linux use the right version */
    typedef struct ::timeval TimeVal;
#endif

    /** Convert the given time from timespec */
    double convert(const TimeVal & ft);
    /** Convert the given time to timespec */
    void convert(const double time, TimeVal & tv);

	// Need this to declare the right class for friend in the right namespace Time:: Needed specially when a 
	// lib is generated with adding an englobing namespace
	struct Time;
	
    /** The usual duration handling.
        Duration are offset-less, by design. 
        Substracting two Time gives a Duration. */
    struct Duration
    {
		/** Allow the time to directly manipulate us */
        friend struct Time;
        // Members
    private:
        /** The duration holder */
        TimeVal duration;
        
        // Interface
    public:    
        /** Check if this time is before another time */
        bool operator < (const Duration & time) const { return (duration.tv_sec < time.duration.tv_sec) ? true : (duration.tv_sec == time.duration.tv_sec ? (duration.tv_usec < time.duration.tv_usec) : false); }
        /** Check if this time is after another time */
        bool operator > (const Duration & time) const { return (duration.tv_sec > time.duration.tv_sec) ? true : (duration.tv_sec == time.duration.tv_sec ? (duration.tv_usec > time.duration.tv_usec) : false); }
        /** Check if this time exactly like another time */
        bool operator == (const Duration & time) const { return (duration.tv_sec == time.duration.tv_sec && duration.tv_usec == time.duration.tv_usec); }

        /** Get the difference between two duration */
        const Duration & operator -= (const Duration & time) { duration.tv_sec -= time.duration.tv_sec - (time.duration.tv_usec > duration.tv_usec);
                                                                duration.tv_usec -= time.duration.tv_usec - (time.duration.tv_usec > duration.tv_usec) * 1000000; return *this; }
        /** Add two durations */
        const Duration & operator += (const Duration & time) { duration.tv_sec += time.duration.tv_sec + (time.duration.tv_usec + duration.tv_usec > 1000000);
                                                                duration.tv_usec += duration.tv_usec - ((time.duration.tv_usec + duration.tv_usec > 1000000) * 1000000); return *this; }

        /** Add two durations */
        Duration operator + (const Duration & time) const { Duration ret(*this); return ret += time; }
        /** Get the difference between two duration */
        Duration operator - (const Duration & time) const { Duration ret(*this); return ret -= time; }

        /** Check if this duration is not zero */
        bool isSet() const { return duration.tv_sec || duration.tv_usec; }
        
        /** Get the number of second for this duration */
        long Second() const { return (long)duration.tv_sec; }
        /** Get the number of microsecond for this duration */
        long microSecond() const { return duration.tv_usec; }
        /** Get the duration in seconds with all precision */
        double preciseDuration() const { return static_cast<double>(duration.tv_sec) + static_cast<double>(duration.tv_usec) * 0.000001; }
        
        /** Get the duration in the specified unit.
            @code
                // Get this duration as days
                double days = duration.getDurationIn(Day);
                // Get this duration as minutes
                double minutes = duration.getDurationIn(Minute);
                // Get this duration as a multiple of the given duration
                double durationAs = duration.getDurationIn(duration); // Should be 1.0, of course
            @endcode */
        inline double getDurationIn(const Duration & other) const { return preciseDuration() / other.preciseDuration(); }

        /** Construct the object from second and microsecond */
        Duration(const uint32 second = 0, const uint32 usecond = 0)
        {
            duration.tv_sec = second;
            duration.tv_usec = (unsigned int)usecond;
        }
        /** Construct the object from second and microsecond */
        Duration(const time_t second, const long usecond)
        {
            duration.tv_sec = second;
            duration.tv_usec = (unsigned int)usecond;
        }
        /** Construct the object from a precise duration */
        Duration(const double _duration)
        {
            duration.tv_sec = (unsigned int)_duration;
            duration.tv_usec = (unsigned int)(((_duration) - duration.tv_sec) * 1000000 + 0.5);
        }
    };

    /** A duration of one second */
    static Duration Second(1.0);
    /** A duration of one minute */
    static Duration Minute(60.0);
    /** A duration of one hour */
    static Duration Hour(3600.0);
    /** A duration of one day */
    static Duration Day(86400.0);
    /** A duration of one week */
    static Duration Week(604800.0);
    
    
    /** Manipulate time. */
    struct Time
    {
    protected:
        /** The time since Epoch */
        TimeVal timeSinceEpoch;

    public:
        /** Get the difference between two times as a time duration */
        Duration operator - (const Time & time) const { return Duration(timeSinceEpoch.tv_sec - time.timeSinceEpoch.tv_sec - (time.timeSinceEpoch.tv_usec > timeSinceEpoch.tv_usec), (time.timeSinceEpoch.tv_usec > timeSinceEpoch.tv_usec) * 1000000 + timeSinceEpoch.tv_usec - time.timeSinceEpoch.tv_usec); }
        /** Get the difference between two times as a time duration */
        Time operator + (const Duration & duration) const { return Time(timeSinceEpoch.tv_sec + duration.duration.tv_sec + (duration.duration.tv_usec + timeSinceEpoch.tv_usec > 1000000), timeSinceEpoch.tv_usec + duration.duration.tv_usec - ((duration.duration.tv_usec + timeSinceEpoch.tv_usec > 1000000) * 1000000)); }
        /** Get the difference between a time and a duration */
        Time operator - (const Duration & duration) const { return Time(timeSinceEpoch.tv_sec - duration.duration.tv_sec - (duration.duration.tv_usec > timeSinceEpoch.tv_usec), (duration.duration.tv_usec > timeSinceEpoch.tv_usec) * 1000000 + timeSinceEpoch.tv_usec - duration.duration.tv_usec); }

        /** Check if this time is before another time */
        bool operator < (const Time & time) const { return (timeSinceEpoch.tv_sec < time.timeSinceEpoch.tv_sec) ? true : (timeSinceEpoch.tv_sec == time.timeSinceEpoch.tv_sec ? (timeSinceEpoch.tv_usec < time.timeSinceEpoch.tv_usec) : false); }
        /** Check if this time is after another time */
        bool operator > (const Time & time) const { return (timeSinceEpoch.tv_sec > time.timeSinceEpoch.tv_sec) ? true : (timeSinceEpoch.tv_sec == time.timeSinceEpoch.tv_sec ? (timeSinceEpoch.tv_usec > time.timeSinceEpoch.tv_usec) : false); }
        /** Check if this time exactly like another time */
        bool operator == (const Time & time) const { return (timeSinceEpoch.tv_sec == time.timeSinceEpoch.tv_sec && timeSinceEpoch.tv_usec == time.timeSinceEpoch.tv_usec); }

        /** Get the current time */
        static Time Now() { Time ret; gettimeofday(&ret.timeSinceEpoch, NULL); return ret; }
        
        /** Parse the current time from a representation following RFC1036 or RFC 1123 (or RFC 850) or asctime or RFC 8601.
            @warning This ignores the timezone or the DST field if set. 
            @return false if the time couldn't be parsed correctly */
        bool fromDate(const char * timeAsUTF8);
        /** Convert this time to RFC1123 format (that is, for example "Sun, 01 Jan 1970 00:00:00 GMT").
            The text is appended to the given buffer
            @param buffer       On output, contains the date in RFC 1123 format. The buffer can be 0 to get the required size.
            @param useISO8601   If true, the output format is using ISO8601 formatting rules, and not RFC1123. You better ignore this parameter
            @return the number of bytes required and/or written too  */
        int toDate(char * buffer, const bool useISO8601 = false) const;
        /** Convert this time to a date in RFC1123 format.
            @sa toDate 
            @param useISO8601   If true, the output format is using ISO8601 formatting rules, and not RFC1123. You better ignore this parameter
            @return a string instance that's containing the date */
        Strings::FastString toDate(const bool useISO8601 = false) const;
        /** Get the date information.
            @param year         The number of years since 1900 (so it's 90 for 1990 and 110 for 2010)
            @param month        The number of month since January (so it's 0 for January and 11 for December) 
            @param dayOfMonth   The day index in the month set (starts by 1)
            @param hour         The number of hours since midnight (so it's in [0  23] range)
            @param min          The number of minutes (so it's in [0 60] range)
            @param sec          The number of seconds (so it's in [0 60] range, 60 is used for leap seconds) 
            @param dayOfWeek    If provided, returns the current day of week (starting from Sunday = 0)
            @sa Time constructor */
        virtual void getAsDate(int & year, int & month, int & dayOfMonth, int & hour, int & min, int & sec, int * dayOfWeek = 0) const;


        // Interface
    public:    
        /** Get the number of second for this time */
        long Second() const { return (long)timeSinceEpoch.tv_sec; }
        /** Get the number of microsecond for this time */
        long microSecond() const { return timeSinceEpoch.tv_usec; }
        /** Get this time as the number of second since Epoch */
        double preciseTime() const { return static_cast<double>(timeSinceEpoch.tv_sec) + static_cast<double>(timeSinceEpoch.tv_usec) * 0.000001; }
        
        /** Get this time as a native time_t value (only the number of second is returned) */
        virtual time_t asNative() const { return timeSinceEpoch.tv_sec; }


        /** Construct the object from second and microsecond */
        Time(const uint32 second = 0, const uint32 usecond = 0)
        {
            timeSinceEpoch.tv_sec = second;
            timeSinceEpoch.tv_usec = usecond;
        }
        /** Construct the object from second and microsecond */
        Time(const time_t second, const long usecond)
        {
            timeSinceEpoch.tv_sec = second;
            timeSinceEpoch.tv_usec = (unsigned int)usecond;
        }
        /** Construct the object from a precise time */
        Time(const double _duration)
        {
            timeSinceEpoch.tv_sec = (time_t)_duration;
            timeSinceEpoch.tv_usec = (unsigned int)(((_duration) - timeSinceEpoch.tv_sec) * 1000000 + 0.5);
        }
        /** Construct the object from a specific date in UTC 
            @param year         The number of years since 1900 (so it's 90 for 1990 and 110 for 2010)
            @param month        The number of month since January (so it's 0 for January and 11 for December) 
            @param dayOfMonth   The day index in the month set (starts by 1)
            @param hour         The number of hours since midnight (so it's in [0  23] range)
            @param min          The number of minutes (so it's in [0 60] range)
            @param sec          The number of seconds (so it's in [0 60] range, 60 is used for leap seconds) */
        Time(const int year, const int month, const int dayOfMonth, const int hour, const int min, const int sec);
        
        /** Default destructor */
        virtual ~Time() {}
    };

    /** Obviously, this one is the origin */
    extern const Time Epoch;
    /** This is the end of time (no time value can be bigger than this) */
    extern const Time MaxTime;
    /** This is only used for a very simple remainder for NTP/Unix time conversion */
    const unsigned long NTPOffsetInSeconds = 2208988800UL;
    
    /** Convert the given time from a Time */
    static inline double convert(const Time & time) { return time.preciseTime(); }
    /** Convert the given time to Time */
    static inline void convert(const double time, Time & tim) { tim = Time(time); }

    /** A overload for the LocalTime that's in the current system timezone.
        @sa Time */
    struct LocalTime : public Time
    {
        /** Construct the object from second and microsecond */
        LocalTime(const time_t second = 0, const long usecond = 0)
            : Time(second, usecond) {}

        /** Construct the object from a specific date in local time.
            The input tries to figure out the daylight saving time offset for the given time.
            @warning Due to the way DST is working, it's not always possible to figure out the UTC time out of this local time.
                     When the DST is going backward in time (there is 2 times the same hour in a day), there is two different points
                     in local time that would be possible. The system tries to prefer non-DST time, but this is platform specific 
                     and not guaranted.

            @param year         The number of years since 1900 (so it's 90 for 1990 and 110 for 2010)
            @param month        The number of month since January (so it's 0 for January and 11 for December)
            @param dayOfMonth   The day index in the month set (starts by 1)
            @param hour         The number of hours since midnight (so it's in [0  23] range)
            @param min          The number of minutes (so it's in [0 60] range)
            @param sec          The number of seconds (so it's in [0 60] range, 60 is used for leap seconds) */
        LocalTime(const int year, const int month, const int dayOfMonth, const int hour, const int min, const int sec);
        /** Get the date information.
            @param year         The number of years since 1900 (so it's 90 for 1990 and 110 for 2010)
            @param month        The number of month since January (so it's 0 for January and 11 for December) 
            @param dayOfMonth   The day index in the month set (starts by 1)
            @param hour         The number of hours since midnight (so it's in [0  23] range)
            @param min          The number of minutes (so it's in [0 60] range)
            @param sec          The number of seconds (so it's in [0 60] range, 60 is used for leap seconds) 
            @param dayOfWeek    If provided, returns the current day of week (starting from Sunday = 0)
            @sa Time constructor */
        virtual void getAsDate(int & year, int & month, int & dayOfMonth, int & hour, int & min, int & sec, int * dayOfWeek = 0) const;
        /** Get this time as a native time_t value (only the number of second is returned) */
        virtual time_t asNative() const;
        /** Get the local time now */
        static LocalTime Now();
    };
    /** Convert a LocalTime to a UTC Time */
    Time fromLocal(const LocalTime & time);
    /** Convert a UTC Time to a LocalTime */
    LocalTime toLocal(const Time & time);


    
    /** Get a timestamp for the given base (tries to be as precise as possible on the current platform)
        Returned value are monotonic (always increase), except for platform with no high precision clock, 
        where the system wall clock is returned (and can be rewind when the user change the system time)

        On Linux, if the user changes the time, the value isn't affected (it doesn't jump back) and the
        time is increased monotonic. If you need the time/date, don't use this function, but use
        Time::Now instead.

        You should use this function when you need a time for measuring delays for example.
        @param base  The base to return the time in 
        @return A value in range [0; base[  */
    uint32 getTimeWithBase(const uint32 base);
    /** Get a timestamp for the given base (tries to be as precise as possible on the current platform)
        Returned value are monotonic (always increase), except for platform with no high precision clock, 
        where the system wall clock is returned (and can be rewind when the user change the system time)

        On Linux, if the user changes the time, the value isn't affected (it doesn't jump back) and the
        time is increased monotonic. If you need the time/date, don't use this function, but use
        Time::Now instead.

        You should use this function when you need a time for measuring delays for example.
        @param base  The base to return the time in 
        @return A value in range [0; base[  */
    uint64 getTimeWithBaseHiRes(const uint64 base);    
}

#endif

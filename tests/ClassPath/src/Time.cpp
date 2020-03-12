#include "../include/Time/Time.hpp"

#if defined(_MAC)
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif

#ifdef _WIN32

#define EPOCH_DIFF 0x019DB1DED53E8000I64 /* 116444736000000000 nsecs */
#define RATE_DIFF 10000000 /* 100 nsecs */

namespace Time
{

    double convert(const FILETIME & fileTime)
    {
        LARGE_INTEGER ul;
        ul.LowPart = fileTime.dwLowDateTime;
        ul.HighPart = fileTime.dwHighDateTime;
    
        double currentTime = (double)((ul.QuadPart - EPOCH_DIFF) / (double)RATE_DIFF);
        return currentTime;
    }

    void convert(const double time, FILETIME & ft)
    {
        LARGE_INTEGER ul;
        ul.QuadPart = (LONGLONG)((time * (double)RATE_DIFF) + EPOCH_DIFF);
        ft.dwLowDateTime = ul.LowPart;
        ft.dwHighDateTime = ul.HighPart;
    }

    double getPreciseTime()
    {
        FILETIME fileTime = {0};
        ::GetSystemTimeAsFileTime(&fileTime);
        return convert(fileTime);
    }

}

#include <winsock2.h>
#include <time.h>

#if defined(_MSC_VER) || defined(__BORLANDC__)
#define EPOCHFILETIME (116444736000000000i64)
#else
#define EPOCHFILETIME (116444736000000000LL)
#endif

struct timezone 
{
    int tz_minuteswest; /* minutes W of Greenwich */
    int tz_dsttime;     /* type of dst correction */
};

extern "C" int gettimeofday(struct timeval_std *tv, struct timezone *tz)
{
    FILETIME        ft;
    LARGE_INTEGER   li;
    __int64         t;
    static int      tzflag;

    if (tv)
    {
        GetSystemTimeAsFileTime(&ft);
        li.LowPart  = ft.dwLowDateTime;
        li.HighPart = ft.dwHighDateTime;
        t  = li.QuadPart;       /* In 100-nanosecond intervals */
        t -= EPOCHFILETIME;     /* Offset to the Epoch time */
        t /= 10;                /* In microseconds */
        tv->tv_sec  = (time_t)(t / 1000000);
        tv->tv_usec = (long)(t % 1000000);
    }

    if (tz)
    {
        if (!tzflag)
        {
            _tzset();
            tzflag++;
        }
        tz->tz_minuteswest = _timezone / 60;
        tz->tz_dsttime = _daylight;
    }

    return 0;
}
#else
namespace Time
{
    double getPreciseTime()
    {
        struct timeval fileTime = {0};
        gettimeofday(&fileTime, NULL);
        return convert(fileTime);
    }
}
#endif

namespace Time
{
    double convert(const TimeVal & fileTime)
    {
        double currentTime = (double)(fileTime.tv_sec) + (double)(fileTime.tv_usec) / 1000000.;
        return currentTime;
    }

    TimeVal convert(const double time)
    {
        TimeVal ul = {0};
        ul.tv_sec = (time_t)time;
        ul.tv_usec = (long)(time * 1000000.0 - (double)ul.tv_sec * 1000000);
        return ul;
    }
    
    // The maximum time no one can go further
#define MaxValStep(t)   ( ((t)1) << (CHAR_BIT * sizeof(t) - 1 - ((t)-1 < 1)) )
    const Time MaxTime( MaxValStep(time_t) - 1 + MaxValStep(time_t), 999999);
#undef MaxValStep
    const Time Epoch((time_t)0, 0);

    static inline time_t makeTime(struct tm * tmt)
    {
        return mktime(tmt);
    }
    
    
    static time_t timeGM(struct tm * tm)
    {
        // Common cummulative year days before the given month
        static const unsigned short moff[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

        if ((unsigned)tm->tm_mon >= 12) return (time_t)-1;
        
        if (tm->tm_year < 70)
            return (time_t) -1;

        int y = tm->tm_year + 1900 - (tm->tm_mon < 2);
        
        int nleapdays = y / 4 - y / 100 + y / 400 - (1969 / 4 - 1969 / 100 + 1969 / 400);
        tm->tm_yday = moff[tm->tm_mon] + tm->tm_mday - 1; // Fix up tm_yday
        
        time_t t = ((((time_t) (tm->tm_year - 70) * 365 + tm->tm_yday + nleapdays) * 24 +
            tm->tm_hour) * 60 + tm->tm_min) * 60 + tm->tm_sec;

        return (t < 0 ? (time_t)-1 : t);
    }
    /*
    static time_t timeGM(struct tm *a)
    {   // This comes from Posix standard
        return (time_t)a->tm_sec + a->tm_min*60 + a->tm_hour*3600 + a->tm_yday*86400 + (a->tm_year-70)*31536000 + ((a->tm_year-69)/4)*86400 - ((a->tm_year-1)/100)*86400 + ((a->tm_year+299)/400)*86400;
    }*/

    
    static inline void makeLocalTime(const time_t tim, struct tm & tmt)
    {
#ifdef _WIN32
        localtime_s(&tmt, &tim);
#else
        localtime_r(&tim, &tmt);
#endif
    }

    static inline void makeUTCTime(const time_t tim, struct tm & tmt)
    {
#ifdef _WIN32
        gmtime_s(&tmt, &tim);
#else
        gmtime_r(&tim, &tmt);
#endif
    }    
    
    
    Time fromLocal(const LocalTime & time)
    {
/*
        struct tm tmt;
        // The LocalTime class store its time as number of second since Epoch + (unknown) local offset.
        makeUTCTime((time_t)time.Second(), tmt);
        tmt.tm_isdst = -1; // Force detection of the DST rule in place at that time
        return Time(makeTime(&tmt), time.microSecond());
        */
        return Time(time.Second(), time.microSecond());
    }
    LocalTime toLocal(const Time & time)
    {
/*        struct tm tmt;
        makeLocalTime((time_t)time.Second(), tmt);
        return LocalTime(timeGM(&tmt), time.microSecond());*/
        return LocalTime(time.Second(), time.microSecond());
    }
    
    LocalTime LocalTime::Now()
    {
        return toLocal(Time::Now());
    }
    
    LocalTime::LocalTime(const int year, const int month, const int dayOfMonth, const int hour, const int min, const int sec)
        : Time(time(NULL), 0)
    {
        struct tm tmt;
        tmt.tm_year = year;
        tmt.tm_mon = month;
        tmt.tm_mday = dayOfMonth;
        tmt.tm_hour = hour;
        tmt.tm_min = min;
        tmt.tm_sec = sec;
        tmt.tm_isdst = -1;
        timeSinceEpoch.tv_sec = makeTime(&tmt);
    }

    // Get the date information.
    void LocalTime::getAsDate(int & year, int & month, int & dayOfMonth, int & hour, int & min, int & sec, int * dayOfWeek) const
    {
        struct tm ekT = {0};
        makeLocalTime(timeSinceEpoch.tv_sec, ekT);
        year = ekT.tm_year;
        month = ekT.tm_mon;
        dayOfMonth = ekT.tm_mday;
        hour = ekT.tm_hour;
        min = ekT.tm_min;
        sec = ekT.tm_sec;
        if (dayOfWeek) *dayOfWeek = ekT.tm_wday;
    }

    time_t LocalTime::asNative() const
    {
        struct tm ekT = {0};
        makeLocalTime(timeSinceEpoch.tv_sec, ekT);
        struct tm utcT = {0};
        time_t start = time(NULL);
        makeUTCTime(start, utcT);
        utcT.tm_isdst = ekT.tm_isdst;

        // Compute offset from GMT and localtime, including DST
        time_t offset = makeTime(&utcT) - start;
        return timeSinceEpoch.tv_sec - offset;
    }
    
    bool Time::fromDate(const char * _date)
    {
#if defined(_WIN32) || defined(_POSIX)
        typedef Strings::FastString String;
        String date(_date);
        struct tm ekT = {0};

        const char * monthName[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
        
        // Check for ISO8601 format first
        if (date.Find("T") >= 8 && date.fromFirst("T").getLength() > 6)
        {
            // Both format YYYY-MM-DDTHH:MM:SS and YYYYMMDDTHHMMSS exists and are valid
            date.findAndReplace("-", ""); date.findAndReplace("-", ""); date.findAndReplace("-", "");
            date.findAndReplace(":", ""); date.findAndReplace(":", ""); date.findAndReplace(":", "");
            
            ekT.tm_year = (int)date.midString(0, 4) - 1900;
            ekT.tm_mon = (int)date.midString(4, 2) - 1; // Month are in [0 11] range
            ekT.tm_mday = (int)date.midString(6, 2);
            ekT.tm_hour = (int)date.midString(9, 2);
            ekT.tm_min = (int)date.midString(11, 2);
            ekT.tm_sec = (int)date.midString(13, 2);
        } 
        // Try to figure out the format used (RFC1036 or RFC1123 or asctime)
        else if (date.Find(',') == -1)
        {
            // Probably asctime, so let's figure out the format now
            String remain = date.fromFirst(" ");
            String monthTxt = remain.midString(0, 3);

            for (ekT.tm_mon = 0; ekT.tm_mon < 12 && monthTxt != monthName[ekT.tm_mon]; ekT.tm_mon++);
            if (ekT.tm_mon == 12) return false;

            ekT.tm_mday = (int)remain.midString(4, 2);
            ekT.tm_hour = (int)remain.midString(7, 2);
            ekT.tm_min  = (int)remain.midString(10, 2);
            ekT.tm_sec  = (int)remain.midString(13, 2);
            ekT.tm_year = (int)remain.midString(16, 4) - 1900;
        }
        else
        {   // Either RFC 1123 or RFC 1036
            String remain = date.fromFirst(" ");
            int yearLength = remain.Find('-') != -1 ? 
                                    remain.fromFirst("-").fromFirst("-").upToFirst(" ").getLength()
                                 :  remain.fromFirst(" ").fromFirst(" ").upToFirst(" ").getLength();
            ekT.tm_mday = (int)remain.midString(0, 2);
            String monthTxt = remain.midString(3, 3);

            for (ekT.tm_mon = 0; ekT.tm_mon < 12 && monthTxt != monthName[ekT.tm_mon]; ekT.tm_mon++);
            if (ekT.tm_mon == 12) return false;

            ekT.tm_year = (int)remain.midString(7, yearLength); 
            if (ekT.tm_year > 1900) ekT.tm_year -= 1900; else if (ekT.tm_year < 70) ekT.tm_year += 100;
            ekT.tm_hour = (int)remain.midString(8 + yearLength, 2);
            ekT.tm_min  = (int)remain.midString(11 + yearLength, 2);
            ekT.tm_sec  = (int)remain.midString(14 + yearLength, 2);
        }

        timeSinceEpoch.tv_sec = timeGM(&ekT);
        timeSinceEpoch.tv_usec = 0;
        return true;
#else        
        return false;
#endif
    }
    
    Time::Time(const int year, const int month, const int dayOfMonth, const int hour, const int min, const int sec)
    {
#if defined(_WIN32) || defined(_POSIX)
        struct tm ekT = {0};
        ekT.tm_year = year;
        ekT.tm_mon = month;
        ekT.tm_mday = dayOfMonth;
        ekT.tm_hour = hour;
        ekT.tm_min = min;
        ekT.tm_sec = sec;
        timeSinceEpoch.tv_sec = timeGM(&ekT);
        timeSinceEpoch.tv_usec = 0;
#else
        timeSinceEpoch.tv_sec = 0;
        timeSinceEpoch.tv_usec = 0;
#endif
    }
    
    // Export a date to RFC 1123 format
    int Time::toDate(char * buffer, const bool iso8601) const
    {
#if defined(_WIN32) || defined(_POSIX)
        if (!buffer) return 30;
        const char * monthName[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
        const char * dayName[] = { "Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat" };
        
        struct tm ekT = {0};
        time_t t = (time_t)preciseTime();
        makeUTCTime(t, ekT);
        if (iso8601)
            return sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02dZ", ekT.tm_year + 1900, ekT.tm_mon + 1, ekT.tm_mday, ekT.tm_hour, ekT.tm_min, ekT.tm_sec);
        return sprintf(buffer, "%s, %02d %s %04d %02d:%02d:%02d %s", dayName[ekT.tm_wday], ekT.tm_mday, monthName[ekT.tm_mon], ekT.tm_year + 1900, ekT.tm_hour, ekT.tm_min, ekT.tm_sec, "GMT") + 1;
#else
        if (iso8601)
            return sprintf(buffer, "1970-01-01T00:00:00Z");
        strcpy(buffer, "Sun, 01 Jan 1970 00:00:00 GMT");
        return strlen(buffer);
#endif        
    }
    
	Strings::FastString Time::toDate(const bool iso8601) const
	{
		char buffer[30];
		toDate(buffer, iso8601);
		return Strings::FastString(buffer);
	}
    
    // Get the date information.
    void Time::getAsDate(int & year, int & month, int & dayOfMonth, int & hour, int & min, int & sec, int * dayOfWeek) const
    {
        struct tm ekT = {0};
        time_t t = timeSinceEpoch.tv_sec;
        makeUTCTime(t, ekT);
        year = ekT.tm_year;
        month = ekT.tm_mon;
        dayOfMonth = ekT.tm_mday;
        hour = ekT.tm_hour;
        min = ekT.tm_min;
        sec = ekT.tm_sec;
        if (dayOfWeek) *dayOfWeek = ekT.tm_wday;

    }
    
#if defined(_WIN32)
    static void (WINAPI* pGetSystemTimePreciseAsFileTime)(LPFILETIME) = 0;
    
    struct Calib
    {
        bool done;
        uint64 freq;
        uint64 systemTime100ns;
        uint64 perfCounter;
        
        CRITICAL_SECTION lock;
        Calib() : done(false), freq(0), systemTime100ns(0), perfCounter(0)
        {
            LARGE_INTEGER li;
            QueryPerformanceFrequency(&li);
            freq = li.QuadPart;
            
            InitializeCriticalSectionAndSpinCount(&lock, 4096);
        }
        ~Calib() { DeleteCriticalSection(&lock); }
    };
    static Calib calib;
    void CalibrateClocks()
    {
#pragma comment(lib, "winmm.lib")    
        timeBeginPeriod(1);
        
        // Wait until the scheduler as triggered us
        FILETIME cur, start;
        GetSystemTimeAsFileTime(&start);
        do {
            GetSystemTimeAsFileTime(&cur);
        } while (memcmp(&start, &cur, sizeof(cur)) == 0);
        
        // No need to maintain high resolution anymore
        timeEndPeriod(1);

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        
        calib.perfCounter = now.QuadPart;
        calib.systemTime100ns = ((uint64)cur.dwHighDateTime << 32ULL) | (uint64)cur.dwLowDateTime;
        calib.done = true;
    }
    
    static void WINAPI fallbackPreciseTime(LPFILETIME lpFT)
    {
        // Under Windows API, kernel developer decided not to give any good way (with a good offset and monotonic values) to get time
        // so we must trick, cheat, spit to try to fix their laziness.
        // To build a high resolution value we have to combine a low resolution (20ms ?) system time, and a microsecond clock.
        // It would be easy except that both clock derives in time, and we "should" re-calibrated when the error between both clock
        // reach the low clock resolution
        // But since when we query the low clock, we don't get updated value (unless we wait for being re-scheduled leading to up to date low clock)
        // this means blocking the calling thread for a complete scheduler's time slice
        
        // Add to this that this method can be called by multiple thread, and you get the idea how complex the Win dev left us.
        bool doneCalib = false;
        FILETIME ft;
        do
        {
            // Should we calibrate now ?
            if (!calib.done)
            {   // Only one thread should do that
                EnterCriticalSection(&calib.lock);
                
                if (!calib.done)
                {   // If we are here, then no other thread started calibration yet
                    // However, don't let other thread spin uselessly on the lock, since we have to block
                    // until the low res clock increase, and this takes a while.
                    SetCriticalSectionSpinCount(&calib.lock, 0);
                    CalibrateClocks();
                    doneCalib = true;
                    SetCriticalSectionSpinCount(&calib.lock, 4096);
                }
                LeaveCriticalSection(&calib.lock);
            }
            
            // Get both clock time now
            GetSystemTimeAsFileTime(&ft);
            LARGE_INTEGER ul, qpc;
            ul.LowPart = ft.dwLowDateTime;
            ul.HighPart = ft.dwHighDateTime;
            
            QueryPerformanceCounter(&qpc);
            
            // Ok, compute the final time
            EnterCriticalSection(&calib.lock);
            uint64 clk = calib.systemTime100ns + ((qpc.QuadPart - calib.perfCounter) * RATE_DIFF) / calib.freq;
            LeaveCriticalSection(&calib.lock);
            
            // Check if the distance between clock need recalibration
            // If it's larger than 40ms (more than 2 scheduler unit time), we need to recalibrate
            // because we should have spotted the change in the low res clock and we did not
            if (AbsDiff(clk, (uint64)ul.QuadPart) < 400000)
            {
                lpFT->dwLowDateTime = clk & 0xFFFFFFFFULL;
                lpFT->dwHighDateTime = clk >> 32ULL;
                return;
            }
            
            // Ok, recalibration is required
            calib.done = false;
        } while (!doneCalib);
        // Here we did calibration, yet, the time if still off by a wide margin, we'll never get a better measure than the low res system clock
        memcpy(lpFT, &ft, sizeof(ft));
    }

    static void initHighFreqFunc()
    {
        // Check if we can use Windows 8's new GetSystemTimePreciseAsFileTime API, else we'll need to roll our own
        if (!pGetSystemTimePreciseAsFileTime)
        {
            if (HMODULE h = GetModuleHandleA("kernel32.dll"))
                InterlockedExchangePointer((PVOID*)&pGetSystemTimePreciseAsFileTime, (PVOID)(void (WINAPI*)(LPFILETIME))GetProcAddress(h, "GetSystemTimePreciseAsFileTime"));
            if (!pGetSystemTimePreciseAsFileTime)
                InterlockedExchangePointer((PVOID*)&pGetSystemTimePreciseAsFileTime, (PVOID)&fallbackPreciseTime);
        }
    }
    
#elif defined(_LINUX)
    // Return the offset from clock realtime and clock monotonic
    static struct timespec & getInitialTimespec()
    {
        static struct timespec ts;
        struct timespec mt;
        static bool initialized = false;
        if (!initialized)
        {
            if (clock_gettime(CLOCK_REALTIME, &ts) == EINVAL)
            {
                TimeVal now;
                gettimeofday(&now, NULL);
                ts.tv_sec = now.tv_sec;
                ts.tv_nsec = now.tv_usec * 1000;
            }
            if (clock_gettime(CLOCK_MONOTONIC, &mt) == EINVAL)
            {
                TimeVal now;
                gettimeofday(&now, NULL);
                mt.tv_sec = now.tv_sec;
                mt.tv_nsec = now.tv_usec * 1000;
            }
            ts.tv_sec -= mt.tv_sec;
            if (mt.tv_nsec > ts.tv_nsec) { ts.tv_sec --; ts.tv_nsec = ts.tv_nsec + 1000000000 - mt.tv_nsec; }
            else ts.tv_nsec -= mt.tv_nsec;
            initialized = true;
        }
        return ts;
    }
#elif defined(_MAC)
    // Return the offset from clock realtime and clock monotonic
    static uint64 getInitialTimespec()
    {
        static uint64 clock;
        static bool initialized = false;
        if (!initialized)
        {
            TimeVal now;
            // Try to get the time since Epoch and then, the front clock time.
            gettimeofday(&now, NULL);
            uint64 ts = mach_absolute_time();
            
            mach_timebase_info_data_t    sTimebaseInfo;
            mach_timebase_info(&sTimebaseInfo);
            
            // Convert the clock to nanoseconds
            clock = (uint64)now.tv_sec * 1000000000 + (uint64)now.tv_usec * 1000;
            // Then back to the mach time format
            clock = (clock * sTimebaseInfo.numer) / sTimebaseInfo.denom;

            // Clock is now in mach's native timebase so we can remove the absolute offset
            clock -= ts;
            initialized = true;
        }
        return clock;
    }
#endif

    
    uint64 multDiv(uint64 a, uint64 b, uint64 c)
    {
        uint64 const base = 1ULL<<32;
        uint64 const maxdiv = (base-1) * base + (base-1);

        // First get the easy thing
        uint64 res = (a/c) * b + (a%c) * (b/c);
        a %= c;
        b %= c;
        // Are we done?
        if (a == 0 || b == 0)
            return res;
        // Is it easy to compute what remain to be added?
        if (c < base)
            return res + (a*b/c);
        // Now 0 < a < c, 0 < b < c, c >= 1ULL
        // Normalize
        uint64 norm = maxdiv/c;
        c *= norm;
        a *= norm;
        // split into 2 digits
        uint64 ah = a / base, al = a % base;
        uint64 bh = b / base, bl = b % base;
        uint64 ch = c / base, cl = c % base;
        // compute the product
        uint64 p0 = al*bl;
        uint64 p1 = p0 / base + al*bh;
        p0 %= base;
        uint64 p2 = p1 / base + ah*bh;
        p1 = (p1 % base) + ah * bl;
        p2 += p1 / base;
        p1 %= base;
        // p2 holds 2 digits, p1 and p0 one
        
        // first digit is easy, not null only in case of overflow
        // uint64 q2 = p2 / c;
        p2 = p2 % c;
        
        // second digit, estimate
        uint64 q1 = p2 / ch;
        // and now adjust
        uint64 rhat = p2 % ch;
        // the loop can be unrolled, it will be executed at most twice for
        // even bases -- three times for odd one -- due to the normalisation above
        while (q1 >= base || (rhat < base && q1*cl > rhat*base+p1)) {
            q1--;
            rhat += ch;
        }
        // subtract
        p1 = ((p2 % base) * base + p1) - q1 * cl;
        p2 = (p2 / base * base + p1 / base) - q1 * ch;
        p1 = p1 % base + (p2 % base) * base;
        
        // now p1 hold 2 digits, p0 one and p2 is to be ignored
        uint64 q0 = p1 / ch;
        rhat = p1 % ch;
        while (q0 >= base || (rhat < base && q0*cl > rhat*base+p0)) {
            q0--;
            rhat += ch;
        }
        // we don't need to do the subtraction (needed only to get the remainder,
        // in which case we have to divide it by norm)
        return res + q0 + q1 * base; // + q2 *base*base
    }
    
    uint32 getTimeWithBase(const uint32 base)
    {
#ifdef _WIN32
        return (uint32)getTimeWithBaseHiRes(base);
#elif defined(_LINUX)
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC, &ts) == EINVAL)
        {
            TimeVal now;
            gettimeofday(&now, NULL);
            ts.tv_sec = now.tv_sec;
            ts.tv_nsec = now.tv_usec * 1000;
        }
        struct timespec init = getInitialTimespec();
        ts.tv_nsec += init.tv_nsec; ts.tv_sec += init.tv_sec;
        if (ts.tv_nsec > 1000000000) { ts.tv_sec ++; ts.tv_nsec -= 1000000000; }
        uint64 fracPart = (uint64)ts.tv_nsec * base;
        uint64 longTimeInBase = ((uint64)ts.tv_sec * base) + (uint64)(fracPart / 1000000000);
        return (uint32)longTimeInBase;
#else
        TimeVal now;
        gettimeofday(&now, NULL);

        uint64 fracPart = (uint64)now.tv_usec * base;
        uint64 longTimeInBase = ((uint64)now.tv_sec * base) + (uint64)(fracPart / 1000000); 
        return (uint32)longTimeInBase;
#endif
    }



    uint64 getTimeWithBaseHiRes(const uint64 base)
    {
#ifdef _WIN32
        FILETIME ft;
        initHighFreqFunc();
        pGetSystemTimePreciseAsFileTime(&ft);
        LARGE_INTEGER ul;
        ul.LowPart = ft.dwLowDateTime;
        ul.HighPart = ft.dwHighDateTime;
    
        return multDiv(ul.QuadPart - EPOCH_DIFF, base, RATE_DIFF);
#elif defined(_LINUX)
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        {
            TimeVal now;
            gettimeofday(&now, NULL);
            ts.tv_sec = now.tv_sec;
            ts.tv_nsec = now.tv_usec * 1000;
        }
        struct timespec init = getInitialTimespec();
        ts.tv_nsec += init.tv_nsec; ts.tv_sec += init.tv_sec;
        if (ts.tv_nsec > 1000000000) { ts.tv_sec ++; ts.tv_nsec -= 1000000000; }

        uint64 fracPart = (uint64)ts.tv_nsec * base;
        uint64 longTimeInBase = ((uint64)ts.tv_sec * base) + (uint64)(fracPart / 1000000000);
        return longTimeInBase;
#elif defined(_MAC)
        uint64 now = mach_absolute_time() + getInitialTimespec();

        static mach_timebase_info_data_t    sTimebaseInfo;
        
        if ( sTimebaseInfo.denom == 0 )
            (void) mach_timebase_info(&sTimebaseInfo);
        
        return multDiv (now * sTimebaseInfo.numer, base, sTimebaseInfo.denom * 1000000000);
#else
        TimeVal now;
        gettimeofday(&now, NULL);

        uint64 fracPart = (uint64)now.tv_usec * base;
        uint64 longTimeInBase = ((uint64)now.tv_sec * base) + (uint64)(fracPart / 1000000);
        return longTimeInBase;
#endif
    }
    
}



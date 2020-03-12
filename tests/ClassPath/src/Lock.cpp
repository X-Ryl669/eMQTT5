#include "../include/Threading/Lock.hpp"

#ifndef _WIN32
typedef void (*PThreadVoid)(void*);
#endif

static void Sleep(const uint32 lMilliseconds, const bool hard)
{
#ifdef _WIN32
    ::Sleep((DWORD)lMilliseconds);
#else
    if (lMilliseconds == 0) sched_yield();
    else
    {
#ifdef _POSIX
        struct timespec req = { (time_t)(lMilliseconds / 1000), (long)((lMilliseconds % 1000) * 1000000) };
        while (nanosleep(&req, &req) < 0 && hard);
#else
        portTickType xDelay = lMilliseconds / portTICK_RATE_MS;
        portTickType xLastWakeTime = xTaskGetTickCount();
        vTaskDelayUntil( &xLastWakeTime, xDelay );
#endif
    }
#endif
}

#if defined(HasExtendedLock)
// We need asserts
#include "../../include/Utils/Assert.hpp"
  #if HasAtomicClass == 1
// We need fast clock too
#include "../../include/Time/Time.hpp"

bool Threading::ReadWriteLock::acquireReader(const TimeOut timeout) volatile
{
    for (int retry = 0;;)
    {
        // Highest priority to writer is done here
        if (waitingWriter.read() && !writerDone.Wait(timeout)) return false;
        if (waitingWriter.read()) continue; // Some writers are still pending

        uint32 pokeReader = readers.read();
        if (pokeReader != WriterFlag)
        {   // Using CAS here instead of atomic increment to avoid transforming a reader to a writer
            uint32 expectedReader = pokeReader + 1;
            if (readers.compareAndSet(pokeReader, expectedReader, true))
            return true; // Got it
        }

        if (++retry > RetryCount)
        {   // Either on high contention, or a writer's got it without releasing, need to wait anyway
            retry = 0;
            if (!writerDone.Wait(timeout)) return false;
        }
    }
}
void Threading::ReadWriteLock::releaseReader() volatile
{
    for (int retry = 0;;)
    {
        uint32 pokeReader = readers.read();
        Assert(pokeReader > 0 && pokeReader != WriterFlag && "You can't release a reader you don't have!");
        if (pokeReader != WriterFlag && pokeReader)
        {
            uint32 expectedReader = pokeReader - 1;
            if (readers.compareAndSet(pokeReader, expectedReader, true))
            return; // Done!
        }

        if (++retry > RetryCount)
        {   // High contention, let's ask to the thread to yield
            retry = 0;
            Sleep(0);
        }
    }
}
bool Threading::ReadWriteLock::acquireWriter(const TimeOut timeout) volatile
{
    // Tell all user that at least one writer wants to write
    uint32 pokeWriter = waitingWriter.read();
    while (!waitingWriter.compareAndSet(pokeWriter, pokeWriter+1, true)) {}

    uint32 start = Time::getTimeWithBase(1000);

    for (int retry = 0;;)
    {
        uint32 pokeReader = readers.read();
        if (!pokeReader && readers.compareAndSet(pokeReader, WriterFlag, true))
        {
            writerDone.Reset();
            return true; // Got it
        }

        if (++retry > RetryCount)
        {   // Either on high contention, or a writer's got it without releasing, need to wait anyway
            retry = 0;
            if (pokeWriter != 0)
            {
                if (!writerDone.Wait(timeout))
                {
                    Assert(timeout != Infinite);
                    --waitingWriter;
                    return false;
                }
            }
            else
            {
                if (timeout == Infinite || (Time::getTimeWithBase(1000) - start) < (int)timeout)
                    // We are the first writer, so let's wait until all readers are done (there's no event for this anyway)
                    Sleep(0);
                else
                {
                    --waitingWriter;
                    return false;
                }
            }
        }
    }
}
void Threading::ReadWriteLock::releaseWriter() volatile
{
    for (int retry = 0;;)
    {
        uint32 pokeReader = readers.read();
        Assert(pokeReader == WriterFlag && "You can't release a writer lock you don't have!");
        if (pokeReader == WriterFlag && readers.compareAndSet(pokeReader, 0, true))
        {
            --waitingWriter;
            writerDone.Set();
            return; // Got it
        }

        if (++retry > RetryCount)
        {   // Either on high contention
            retry = 0;
            Sleep(0);
        }
    }
}
void Threading::ReadWriteLock::downgradeFromWriter() volatile
{
    for (int retry = 0;;)
    {
        uint32 pokeReader = readers.read();
        Assert(pokeReader == WriterFlag && "You can't release a writer lock you don't have!");
        if (pokeReader == WriterFlag && readers.compareAndSet(pokeReader, 1, true))
        {
            --waitingWriter;
            writerDone.Set();
            return; // Got it
        }

        if (++retry > RetryCount)
        {   // Either on high contention
            retry = 0;
            Sleep(0);
        }
    }
}
bool Threading::ReadWriteLock::upgradeToWriter(const TimeOut timeout) volatile
{
    uint32 start = Time::getTimeWithBase(1000);
    // Tell all user that at least one writer wants to write
    ++waitingWriter;
    for (int retry = 0;;)
    {
        uint32 pokeReader = readers.read();
        Assert(pokeReader && pokeReader != WriterFlag && "You can't upgrade to a writer if you're not already a reader!");
        if (pokeReader == 1 && readers.compareAndSet(pokeReader, WriterFlag, true))
        {
            writerDone.Reset();
            return true; // Got it
        }

        if (++retry > RetryCount)
        {   // Either on high contention, or a writer's got it without releasing, need to wait anyway
            retry = 0;
            if (timeout == Infinite || (Time::getTimeWithBase(1000) - start) < (int)timeout)
                Sleep(0);    // Release our time slice so other thread can release their readers
            else
            {
                --waitingWriter;
                return false;
            }
        }
    }
}

  #else
bool Threading::ReadWriteLock::acquireReader(const Threading::TimeOut timeout) volatile
{
    ScopedLock scope(lock);
    int prevReadCount = currentReaderCount;
    if(!writerCount)
    {   // Enter successful without wait
        ++currentReaderCount;
        Assert(currentReaderCount > prevReadCount);
        return true;
    }
    else
    {
        if (timeout)
        {
            if (!readerWait(timeout))
            {
                Assert(false);
                return false;
            }
            Assert(currentReaderCount > 0);
		    return true;
        } else
        {
            Assert(false);
            return false;
        }

    }
}

bool Threading::ReadWriteLock::acquireWriter(const Threading::TimeOut timeout) volatile
{
	bool canWrite;

	lock.Acquire();
	if(!(writerCount | currentReaderCount))
	{
		++writerCount;
		canWrite = true;
	}
	else if(!timeout) canWrite = false;
	else
	{
		canWrite = writerWaitAndLeaveCSIfSuccess(timeout);
		if(canWrite)
			return true;
	}

	lock.Release();
	return canWrite;
}

// Internal/Real implementation
bool Threading::ReadWriteLock::readerWait(const Threading::TimeOut timeout) volatile
{
	bool canRead = false;
	++waitingReaderCount;
	if (!read) read = new Event(NULL, Event::ManualReset, Event::InitiallyFree);

	if(timeout == Infinite)
	{
		do
		{
			lock.Release();
			read->Wait();
			// There might be one or more Writers entered, that's
			// why we need DO-WHILE loop here
			lock.Acquire();
		} while(0 != writerCount);

		++currentReaderCount;
		canRead = true;
	}
	else
	{
		lock.Release();
		clock_t beginTime = clock();
		int     consumedTime = 0;

		while(1)
		{
			canRead = read->Wait((TimeOut::Type)(timeout - consumedTime));
            lock.Acquire();
			if(!writerCount)
			{
				// Regardless timeout or not, there is no Writer
				// So it's safe to be Reader right now
				++currentReaderCount;
				canRead = true;
				break;
			}

			// Timeout after waiting
			if(!canRead) break;

			// There are some Writers have just entered
			// So leave CS and prepare to try again
			lock.Release();

			consumedTime = ((clock() - beginTime) * 1000) / CLOCKS_PER_SEC;
			if(consumedTime > (int)timeout)
			{
				// Don't worry why the code here looks stupid
				// Because this case rarely happens, it's better
				//  to optimize code for the usual case
				canRead = false;
				lock.Acquire();
				break;
			}
		}
	}

	if (--waitingReaderCount == 0)
	{
	    delete read; read = 0;
	}
	return canRead;
}
void Threading::ReadWriteLock::readerRelease() volatile
{
	int readerCount = --currentReaderCount;
	Assert(0 <= readerCount);
	if(!readerCount && write) write->Set();
}
bool Threading::ReadWriteLock::writerWaitAndLeaveCSIfSuccess(const Threading::TimeOut timeout) volatile
{
	Assert(0 != timeout);

	// Increase Writer-counter & reset Reader-event if necessary
	int prevWriterCount = ++writerCount;
	if(	prevWriterCount == 1 && read) read->Reset();

    if (!write) write = new Event(NULL, Event::AutoReset, Event::InitiallyFree);
	lock.Release();

	bool canWrite = write->Wait(timeout);
	if(!canWrite)
	{
		// Undo what we changed after timeout
		lock.Acquire();
		if(--writerCount == 0)
		{
		    delete write; write = 0;

			if(currentReaderCount == 0)
			{
				// Although it was timeout, it's still safe to be writer now
				++writerCount;
				lock.Release();
				canWrite = true;
			}
			else if (read)
			{
			    read->Set();
			}
		}
	}
	return canWrite;
}
bool Threading::ReadWriteLock::upgradeToWriterLockAndLeaveCS(const Threading::TimeOut timeout) volatile
{
	Assert(currentReaderCount > 0);

	if(!timeout)
	{
		lock.Release();
		return false;
	}

	--currentReaderCount;
	bool canWrite = writerWaitAndLeaveCSIfSuccess(timeout);
	if(!canWrite)
	{
		// Now analyze why it was failed to have suitable action
		if(!writerCount)
		{
			Assert(0 < currentReaderCount);
			// There are some readers still owning the lock
			// It's safe to be a reader again after failure
			++currentReaderCount;
		}
		else
		{
			// Reach to here, it's NOT safe to be a reader immediately
			readerWait(Infinite);
			if(currentReaderCount == 1)
			{
				// After wait, now it's safe to be writer
				Assert(0 == writerCount);
				currentReaderCount = 0;
				writerCount = 1;
				canWrite = true;
			}
		}
		lock.Release();
	}

	return canWrite;
}
void Threading::ReadWriteLock::writerRelease(const bool downgrade) volatile
{
	Assert(0 == currentReaderCount);

	if(downgrade)
		++currentReaderCount;

	if(--writerCount == 0) { delete write; write = 0; if (read) read->Set(); }
	else
	{
		//////////////////////////////////////////////////////////////////////////
		// Some WRITERs are queued
		Assert(0 < writerCount && write);
		if(!downgrade) write->Set();
	}
}
#endif

Threading::ScopedPP::ScopedPP(Lock & lock, const WithStartMarker * ts, PingPong & work)
            : lock(lock), work(work)
{
    if(ts->start.Wait(InstantCheck)) { work.wantToDo(Infinite); }
    lock.Acquire();
}
#endif
#undef BreakUnless



#ifndef _WIN32
bool Threading::stillBefore(struct timeval * tOut)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return (now.tv_sec < tOut->tv_sec ? true : (now.tv_usec < tOut->tv_usec));
}

bool Threading::Event::_Wait(const TimeOut & rcxTO) volatile
{
  #ifndef _POSIX
    bool isOkay = false;
    // compute time delay
    portTickType  tickDelay = rcxTO == Infinite ? portMAX_DELAY : (rcxTO == InstantCheck ? 0 : rcxTO / portTICK_RATE_MS);
    portBASE_TYPE  result;
    unsigned portLONG ulVar = 0;

    // The value in the queue must be removed
    if (!manualReset)
    {
        // The queue is automatically freed
        return xQueueReceive(xQueue, &ulVar, tickDelay) == pdTRUE;
    }
    // The queue is not automatically freed
    return xQueuePeek(xQueue, &ulVar, tickDelay) == pdTRUE;
  #else
    bool isOkay = false;
    if (rcxTO == Infinite)
    {
        bool bState = false;
        // Install clean up handlers
        pthread_cleanup_push((PThreadVoid)pthread_mutex_unlock, (void *) &event);
        int mutexLockSuccess = pthread_mutex_lock((HMUTEX*)&event), retValue = 0;
        if (mutexLockSuccess == 0)
        {
            bState = state;
            if (bState)
            {
                // Is it in auto reset mode ?
                if (!manualReset) state = false;
                isOkay = true;
            } else
            {
                // The variable is not set, so wait state changing
                while (!bState)
                {
                    // pthread_cond_wait release the mxEvent mutex and
                    // lock the condition atomically
                    // Because if any other thread wants to modify the state
                    // it has to lock the mutex, the change will be protected then
                    if ((retValue = pthread_cond_wait((pthread_cond_t*)&condition, (HMUTEX*)&event)) == EINTR) continue;
                    if (retValue != 0)
                    {   // An error occurred
                        isOkay = false; break;
                    }
                    bState = state;
                }

                if (!retValue)
                {
                    if (!manualReset) state = false;
                    isOkay = true;
                }
            }

        } // else return false; // No more needed with cleanup handlers
        // Unlock the mutex here
        pthread_cleanup_pop(mutexLockSuccess == 0);
        return isOkay;
    }
    else if (rcxTO == InstantCheck)
    {
        // Install clean up handlers
        pthread_cleanup_push((PThreadVoid)pthread_mutex_unlock, (void *) &event);
        int retValue = pthread_mutex_trylock((HMUTEX*)&event);
        if (retValue == 0)
        {
            isOkay = state;
            // Is it in auto reset mode ?
            if (state && !manualReset) state = false;

        } //else return false; // No more needed with cleanup handlers
        // Unlock the mutex here
        pthread_cleanup_pop(retValue == 0);
        return isOkay;
    }
    else // Wait with time out
    {
        // Process the absolute time for timeout
        struct timespec abstime;
        struct timeval tOut;
        gettimeofday(&tOut, NULL);
        tOut.tv_usec += (uint32)(rcxTO % 1000) * 1000;
        tOut.tv_sec += (uint32)rcxTO / 1000;
        if (tOut.tv_usec > 1000000)
        { tOut.tv_sec ++; tOut.tv_usec -= 1000000;  }
        abstime.tv_sec = tOut.tv_sec;
        abstime.tv_nsec = tOut.tv_usec * 1000;

        // First try to lock the mutex
        // Sadly, we must poll here

        // Install clean up handlers
        pthread_cleanup_push((PThreadVoid)pthread_mutex_unlock, (void *) &event);
        int mutexLockSuccess = -1, retval = 0;
        // Poll to get the mutex
        while (stillBefore(&tOut))
        {
            mutexLockSuccess = pthread_mutex_trylock((HMUTEX*)&event);
            if (mutexLockSuccess == 0) break;
            // We didn't get the mutex, so sleep a little (to avoid polling)
            sched_yield();
        }
        if (mutexLockSuccess == 0)
        {   // We now have the mutex, so check the state
            bool bState = state;
            if (bState)
            {
                // Is it in auto reset mode ?
                if (!manualReset) state = false;
                isOkay = true;
            } else
            {   // Unset here, so wait for the condition
                // The variable is not set, so wait state changing
                while (!bState)
                {
                    retval = EINTR;
                    while (retval == EINTR) retval = pthread_cond_timedwait((pthread_cond_t*)&condition, (HMUTEX*)&event, &abstime);

                    if (retval != 0) //== ETIMEDOUT)
                    {   // No change in the specified time, so release the mutex and return
                        break;
                    }

                    // A thread could have reset a unset state, thus causing condition
                    // broadcasting. We have to make sure the state is good now
                    bState = state;
                }

                if (!retval)
                {
                    if (!manualReset) state = false;
                    isOkay = true;
                }
            }
        }
        // Unlock the mutex here
        pthread_cleanup_pop(mutexLockSuccess == 0);
        return isOkay;
    }
  #endif
}

bool Threading::Event::_Reset() volatile
{
  #ifndef _POSIX
    unsigned portLONG ulVar = 0;
    // Instant wait of the state but only one value in the queue
    portBASE_TYPE  result = xQueueReceive(xQueue, &ulVar, 0);
    // Don't care of the state of the queue
    return true;
  #else
    bool isOkay = false;
    // Install clean up handlers
    pthread_cleanup_push((PThreadVoid)pthread_mutex_unlock, (void *) &event);
    int retValue = pthread_mutex_lock((HMUTEX*)&event);
    if (retValue == 0)
    {
        state = false;
        pthread_cond_broadcast((pthread_cond_t*)&condition);
        isOkay = true;
    }
    // Unlock the mutex here
    pthread_cleanup_pop(retValue == 0);
    return isOkay;
  #endif
}

bool Threading::Event::_Set(void * arg) volatile
{
  #ifndef _POSIX
    unsigned portLONG ulVar = 1;
    portBASE_TYPE result = arg ? xQueueSendFromISR(xQueue, ( void * ) &ulVar, (signed portBASE_TYPE *)arg) : xQueueSend( xQueue, ( void * ) &ulVar, 0);
    // alreday set but not received . the flag is set . No need a new message
    return result == errQUEUE_FULL || result == pdTRUE;
  #else
    bool isOkay = false;
    // Install clean up handlers
    pthread_cleanup_push((PThreadVoid)pthread_mutex_unlock, (void *) &event);
    int retValue = pthread_mutex_lock((HMUTEX*)&event);
    if (retValue == 0)
    {
        state = true;
        pthread_cond_broadcast((pthread_cond_t*)&condition);
        isOkay = true;
    }
    // Unlock the mutex here
    pthread_cleanup_pop(retValue == 0);
    return isOkay;
  #endif
}


bool Threading::MutexLock::_Lock(const TimeOut & rcxTO) volatile
{
    if (rcxTO == Infinite)
    {
        // Either pthread_mutex_lock return EINVAL (not initialized), and then it is not going to change
        // as we are the initializer (simply returns false here)
        return pthread_mutex_lock((HMUTEX*)&mutex) == 0;
    }
    else if (rcxTO == InstantCheck)
    {
        return pthread_mutex_trylock((HMUTEX*)&mutex) == 0;
    }
    else
    {   // We need to pool here, sorry
        // Process the absolute time for timeout
        struct timeval tOut;
        gettimeofday(&tOut, NULL);
        tOut.tv_usec += (uint32)rcxTO * 1000;
        if (tOut.tv_usec > 1000000)
        { tOut.tv_sec ++; tOut.tv_usec -= 1000000;  }

        // First try to lock the mutex
        // Sadly, we must poll here
        int retval = -1;
        while (stillBefore(&tOut))
        {
            retval = pthread_mutex_trylock((HMUTEX*)&mutex);
            if (retval == 0) break;
            // We didn't get the mutex, so sleep a little (to avoid CPU intensive wait)
            sched_yield();
        }
        if (retval == 0)
        {   // We now have the mutex, so check the state
            return true;
        }
        return false;
    }
}
void Threading::MutexLock::_Unlock(void * arg) volatile
{
  #ifndef _POSIX
    if (arg) pthread_mutex_unlock_isr((HMUTEX*)&mutex, arg);
    else
  #endif
    pthread_mutex_unlock((HMUTEX*)&mutex);
}


bool Threading::FastLock::_Lock(const TimeOut & rcxTO) volatile
{
    if (rcxTO == Infinite)
    {
        // Either pthread_mutex_lock return EINVAL (not initialized), and then it is not going to change
        // as we are the initializer (simply returns false here)
        return pthread_mutex_lock((HMUTEX*)&mutex) == 0;
    }
    else if (rcxTO == InstantCheck)
    {
        return pthread_mutex_trylock((HMUTEX*)&mutex) == 0;
    }
    else
    {   // We need to pool here, sorry
        // Process the absolute time for timeout
        struct timeval tOut;
        gettimeofday(&tOut, NULL);
        tOut.tv_usec += (uint32)rcxTO * 1000;
        if (tOut.tv_usec > 1000000)
        { tOut.tv_sec ++; tOut.tv_usec -= 1000000;  }

        // First try to lock the mutex
        // Sadly, we must poll here
        int retval = -1;
        while (stillBefore(&tOut))
        {
            retval = pthread_mutex_trylock((HMUTEX*)&mutex);
            if (retval == 0) break;
            // We didn't get the mutex, so sleep a little (to avoid CPU intensive wait)
            sched_yield();
        }
        if (retval == 0)
        {   // We now have the mutex, so check the state
            return true;
        }
        return false;
    }
}

void Threading::FastLock::_Unlock(void * arg) volatile
{
  #ifndef _POSIX
    if (arg) pthread_mutex_unlock_isr((HMUTEX*)&mutex, arg);
    else
  #endif
    pthread_mutex_unlock((HMUTEX*)&mutex);
}


  #if defined(_POSIX) && !defined(HAS_ATOMIC_BUILTIN)
HMUTEX Threading::SharedData<uint32>::sxMutex = PTHREAD_MUTEX_INITIALIZER;
  #endif

  #if defined(NO_ATOMIC_BUILTIN64) && (HAS_STD_ATOMIC != 1)
    #if defined(_POSIX)
HMUTEX Threading::sxMutex = PTHREAD_MUTEX_INITIALIZER;
    #endif
  #endif

#else // _WIN32
CRITICAL_SECTION Threading::sxMutex;
struct AutoRegisterAtomicCS
{
    AutoRegisterAtomicCS() { InitializeCriticalSectionAndSpinCount(&Threading::sxMutex, 4000); }
    ~AutoRegisterAtomicCS() { DeleteCriticalSection(&Threading::sxMutex); }
};
static AutoRegisterAtomicCS __aracs;
#endif

#if (WantAtomicClass == 1)
bool Threading::backoffSpin(int & counter, Threading::BackoffFunc f, void * arg)
{
   if (counter < 10) SpinPause();
   else if (counter < 20) for (size_t i = 0; i < 50; i++) { SpinPause(); }
   else if (counter < 22) Sleep(0); // This yield the thread
   else if (f) f(arg); // If a backoff function is provided, call that function instead of waiting 
   else if (counter < 24) Sleep(1);
   else Sleep(10);
   ++counter;
   return true;
}
#endif

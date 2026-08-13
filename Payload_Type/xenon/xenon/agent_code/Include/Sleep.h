#pragma once
#ifndef SLEEP_H
#define SLEEP_H

#include <windows.h>

VOID     SleepWithJitter(INT baseSleepTime, INT maxJitter);
DWORD    SleepIdle(DWORD dwMilliseconds);
VOID     SleepInit(void);
VOID     SleepWake(void);
HANDLE   SleepThreadHandle(void);
PAPCFUNC SleepWakeApc(void);

#endif

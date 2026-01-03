#pragma once

#ifndef STRATEGY_H
#define STRATEGY_H

#include <windows.h>

#if defined(HTTPX_TRANSPORT)

VOID StrategyRotate(_In_ BOOL isConnectionSuccess, _Inout_ int* attempts);

#endif  //HTTPX_TRANSPORT

#endif
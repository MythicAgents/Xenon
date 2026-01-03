#pragma once

#ifndef CHECKIN_H
#define CHECKIN_H

#include <windows.h>
#include <lm.h>
#include <lmwksta.h>
#include "Package.h"
// #include "Transport.h"
#include "Parser.h"

#pragma comment(lib, "netapi32.lib")


BOOL CheckinSend();
#endif
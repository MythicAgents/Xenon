/*
 * Xenon BOF Test Harness
 *
 * Loads a COFF/BOF object file from disk, packs typed arguments, executes it
 * via RunCOFF(), and prints BeaconPrintf/BeaconOutput to stdout — no Mythic
 * server or agent deployment required.
 *
 * Build (from agent_code/):
 *   make -f tools/Makefile
 *
 * Usage:
 *   harness.exe <bof.o> [entrypoint] [args...]
 *
 *   entrypoint  defaults to "go". Omit if using typed args immediately.
 *
 * Argument types (TYPE:VALUE):
 *   z:<str>     NUL-terminated string          → BeaconDataExtract
 *   Z:<str>     NUL-terminated wide string     → BeaconDataExtract
 *   i:<int>     32-bit signed integer          → BeaconDataInt  (hex OK: 0x1a2b)
 *   s:<int>     16-bit signed short            → BeaconDataShort
 *   b:<file>    Binary blob read from <file>   → BeaconDataExtract
 *
 * Examples:
 *   harness.exe whoami.x64.o
 *   harness.exe dir.x64.o go z:C:\Temp
 *   harness.exe netuser.x64.o go z:Administrator z:DOMAIN
 *   harness.exe inject.x64.o go i:1234 b:payload.bin
 */

/* --- Build-time flags ---------------------------------------------------- */
#define _DEBUG           /* enable _dbg/_err printf logging                  */
#define _MANUAL          /* Config.h: enable all INCLUDE_CMD_* macros         */
#define HARNESS_BUILD    /* exclude InlineExecute() Mythic dispatcher         */

/* --- Standard headers ---------------------------------------------------- */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* --- Project headers (include path set to agent_code/Include in Makefile) - */
#include "Xenon.h"
#include "BeaconCompatibility.h"
#include "Tasks/InlineExecute.h"

/* =========================================================================
 * Global stubs — satisfy extern declarations pulled in by Xenon.h /
 * BeaconCompatibility.c without linking the full agent.
 * ======================================================================= */

static CONFIG_XENON harness_cfg;         /* zero-initialised; set spawnto in main() */
PCONFIG_XENON xenonConfig = &harness_cfg;

/* Identity globals (declared extern in Identity.h) */
HANDLE gIdentityToken     = NULL;
BOOL   gIdentityIsLoggedIn = FALSE;
WCHAR* gIdentityDomain    = NULL;

/* Identity function stubs */
BOOL IdentityIsAdmin(void)                             { return FALSE; }
VOID IdentityImpersonateToken(void)                    {}
VOID IdentityAgentRevertToken(void)                    {}
BOOL IdentityGetUserInfo(HANDLE h, char* b, int s)     { return FALSE; }

/* =========================================================================
 * Argument packing
 *
 * Produces the binary buffer that BeaconDataParse() consumes:
 *
 *   [4-byte big-endian payload_size][packed args...]
 *
 * BeaconDataParse skips the first 4 bytes (treats them as the total length
 * field), then subsequent reads pull typed values from the remaining bytes:
 *
 *   BeaconDataInt    — 4-byte big-endian int32
 *   BeaconDataShort  — 2-byte big-endian int16
 *   BeaconDataExtract — [4-byte BE length][raw bytes]  (strings, blobs)
 * ======================================================================= */

typedef struct { uint8_t* data; size_t len; size_t cap; } Buf;

static void buf_init(Buf* b) {
    b->cap  = 256;
    b->data = (uint8_t*)malloc(b->cap);
    b->len  = 0;
}

static void buf_append(Buf* b, const void* src, size_t n) {
    while (b->len + n > b->cap) { b->cap *= 2; b->data = (uint8_t*)realloc(b->data, b->cap); }
    memcpy(b->data + b->len, src, n);
    b->len += n;
}

static void buf_be32(Buf* b, uint32_t v) {
    uint8_t x[4] = { (uint8_t)(v>>24), (uint8_t)(v>>16), (uint8_t)(v>>8), (uint8_t)v };
    buf_append(b, x, 4);
}

static void buf_be16(Buf* b, uint16_t v) {
    uint8_t x[2] = { (uint8_t)(v>>8), (uint8_t)v };
    buf_append(b, x, 2);
}

/*
 * pack_args — convert TYPE:VALUE command-line tokens to a BOF argument buffer.
 * Returns a malloc'd buffer; caller frees. Sets *out_len to total buffer size.
 */
static uint8_t* pack_args(int n, char** tokens, size_t* out_len) {
    Buf payload;
    buf_init(&payload);

    for (int i = 0; i < n; i++) {
        if (strlen(tokens[i]) < 2 || tokens[i][1] != ':') {
            fprintf(stderr, "[!] Argument %d must be TYPE:VALUE (e.g. z:hello, i:42)\n", i + 1);
            exit(1);
        }
        char  type = tokens[i][0];
        char* val  = tokens[i] + 2;

        switch (type) {
        case 'z': {
            uint32_t slen = (uint32_t)(strlen(val) + 1);
            buf_be32(&payload, slen);
            buf_append(&payload, val, slen);
            break;
        }
        case 'Z': {
            int wchars = MultiByteToWideChar(CP_ACP, 0, val, -1, NULL, 0);
            WCHAR* ws = (WCHAR*)malloc(wchars * sizeof(WCHAR));
            MultiByteToWideChar(CP_ACP, 0, val, -1, ws, wchars);
            uint32_t blen = (uint32_t)(wchars * sizeof(WCHAR));
            buf_be32(&payload, blen);
            buf_append(&payload, ws, blen);
            free(ws);
            break;
        }
        case 'i':
            buf_be32(&payload, (uint32_t)(int32_t)strtol(val, NULL, 0));
            break;
        case 's':
            buf_be16(&payload, (uint16_t)(int16_t)strtol(val, NULL, 0));
            break;
        case 'b': {
            FILE* f = fopen(val, "rb");
            if (!f) { fprintf(stderr, "[!] Cannot open blob file: %s\n", val); exit(1); }
            fseek(f, 0, SEEK_END); long fsz = ftell(f); rewind(f);
            uint8_t* blob = (uint8_t*)malloc((size_t)fsz);
            fread(blob, 1, (size_t)fsz, f); fclose(f);
            buf_be32(&payload, (uint32_t)fsz);
            buf_append(&payload, blob, (size_t)fsz);
            free(blob);
            break;
        }
        default:
            fprintf(stderr, "[!] Unknown argument type '%c'. Valid types: z Z i s b\n", type);
            exit(1);
        }
    }

    /* Prepend 4-byte header (BeaconDataParse skips these) */
    Buf final;
    buf_init(&final);
    buf_be32(&final, (uint32_t)payload.len);
    buf_append(&final, payload.data, payload.len);
    free(payload.data);

    *out_len = final.len;
    return final.data;
}

/* =========================================================================
 * main
 * ======================================================================= */

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr,
            "Xenon BOF Test Harness\n"
            "\n"
            "Usage: harness.exe <bof.o> [entrypoint] [args...]\n"
            "\n"
            "  entrypoint  BOF entry function (default: go)\n"
            "              Omit if the next argument is a typed arg.\n"
            "\n"
            "Argument types:\n"
            "  z:<value>   NUL-terminated string  (BeaconDataExtract)\n"
            "  Z:<value>   NUL-terminated wide string\n"
            "  i:<value>   32-bit int, dec or 0x hex  (BeaconDataInt)\n"
            "  s:<value>   16-bit short  (BeaconDataShort)\n"
            "  b:<file>    Binary blob read from <file>  (BeaconDataExtract)\n"
            "\n"
            "Examples:\n"
            "  harness.exe whoami.x64.o\n"
            "  harness.exe dir.x64.o go z:C:\\Temp\n"
            "  harness.exe netuser.x64.o go z:Administrator z:DOMAIN\n"
        );
        return 1;
    }

    /* Provide a valid spawnto path for any BOF that calls BeaconGetSpawnTo */
    harness_cfg.spawnto = "svchost.exe";

    const char* bof_path = argv[1];

    /* Determine entrypoint: if argv[2] contains ':' it is a typed arg, not a name */
    const char* entry  = "go";
    int args_start     = 2;
    if (argc >= 3 && strchr(argv[2], ':') == NULL) {
        entry      = argv[2];
        args_start = 3;
    }

    /* ---- Load BOF from disk ---- */
    FILE* f = fopen(bof_path, "rb");
    if (!f) { fprintf(stderr, "[!] Cannot open BOF: %s\n", bof_path); return 1; }
    fseek(f, 0, SEEK_END);
    long bof_size = ftell(f);
    rewind(f);
    char* bof_data = (char*)malloc((size_t)bof_size);
    if (!bof_data) { fclose(f); fprintf(stderr, "[!] OOM\n"); return 1; }
    fread(bof_data, 1, (size_t)bof_size, f);
    fclose(f);

    printf("[*] BOF:   %s (%ld bytes)\n", bof_path, bof_size);
    printf("[*] Entry: %s\n", entry);

    /* ---- Pack arguments ---- */
    size_t   arg_len = 0;
    uint8_t* arg_buf = NULL;
    int      n_args  = argc - args_start;

    if (n_args > 0) {
        printf("[*] Args:  %d argument(s)\n", n_args);
        arg_buf = pack_args(n_args, argv + args_start, &arg_len);
    } else {
        /* BeaconDataParse always expects the 4-byte header */
        arg_buf = (uint8_t*)calloc(1, 4);
        arg_len = 4;
    }

    /* ---- Execute ---- */
    printf("[*] Running...\n\n");
    DWORD sz = (DWORD)bof_size;
    BOOL  ok = RunCOFF(bof_data, &sz, (char*)entry, (char*)arg_buf, (unsigned long)arg_len);

    /* ---- Print output ---- */
    int   out_size = 0;
    char* out_data = BeaconGetOutputData(&out_size);

    printf("\n");
    if (out_data && out_size > 0) {
        printf("======== BeaconOutput (%d bytes) ========\n", out_size);
        fwrite(out_data, 1, (size_t)out_size, stdout);
        if (out_data[out_size - 1] != '\n') printf("\n");
        printf("=========================================\n");
        free(out_data);
    } else {
        printf("[*] No BeaconOutput captured.\n");
    }

    if (!ok) fprintf(stderr, "[!] RunCOFF returned FALSE\n");

    free(bof_data);
    free(arg_buf);
    return ok ? 0 : 1;
}

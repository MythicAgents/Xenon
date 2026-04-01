# Xenon BOF Test Harness
#
# Run from agent_code/ directory:
#   make -f tools/Makefile
#
# Or from this directory:
#   make
#
# Output: tools/harness.exe  (copy to Windows target for testing)

CC     = x86_64-w64-mingw32-gcc
ROOT   = ..
INC    = $(ROOT)/Include
SRC    = $(ROOT)/Src

# _DEBUG   — enables _dbg/_err printf output
# _MANUAL  — Config.h enables all INCLUDE_CMD_* macros (incl. INLINE_EXECUTE)
# HARNESS_BUILD — excludes InlineExecute() Mythic dispatcher (no Package/Parser deps)
CFLAGS = -Wall -w -g -I$(INC) -D_DEBUG -D_MANUAL -DHARNESS_BUILD

SRCS = tools/harness.c \
       $(SRC)/Utils.c \
       $(SRC)/BeaconCompatibility.c \
       $(SRC)/Tasks/InlineExecute.c

LIBS = -liphlpapi -lnetapi32 -lwininet -lws2_32

.PHONY: all clean

all: tools/harness.exe

tools/harness.exe: $(SRCS)
	$(CC) $(SRCS) -o tools/harness.exe $(CFLAGS) $(LIBS)

clean:
	rm -f tools/harness.exe

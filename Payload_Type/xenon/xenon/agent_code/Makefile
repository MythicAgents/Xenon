# Compiler
CC = x86_64-w64-mingw32-gcc
CFLAGS = -Wall -w -s -IInclude
LFLAGS = -liphlpapi -lnetapi32 -lwininet -lws2_32 
DFLAGS = -D_DEBUG

# Directories
SRC_FILES := Src/*.c Src/Tasks/*.c

# Build Targets
all: exe dll

exe: artifact_x64.exe

dll: artifact_x64.dll

debug: debug_exe debug_dll

debug_exe: artifact_x64-debug.exe

debug_dll: artifact_x64-debug.dll


# Executable Target
artifact_x64.exe: $(SRC_FILES)
	$(CC) $^ -o $@ $(CFLAGS) -D_EXE $(LFLAGS)

artifact_x64-debug.exe: $(SRC_FILES)
	$(CC) $^ -o $@ $(CFLAGS) -D_EXE $(LFLAGS) $(DFLAGS)

# DLL Target
artifact_x64.dll: $(SRC_FILES)
	$(CC) $^ -o $@ -shared -D_DLL $(CFLAGS) $(LFLAGS)

artifact_x64-debug.dll: $(SRC_FILES)
	$(CC) $^ -o $@ -shared -D_DLL $(CFLAGS) $(LFLAGS) $(DFLAGS)


# Clean up
clean:
	rm -f *.dll *.exe *.bin

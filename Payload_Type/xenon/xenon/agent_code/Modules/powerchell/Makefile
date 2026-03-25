# Compiler
CXX = x86_64-w64-mingw32-g++
DLLTOOL = x86_64-w64-mingw32-dlltool
# Use execute-assembly's metahost.h and mscorlib.h (MinGW doesn't ship these)
EA_SRC = ../execute-assembly/src
CFLAGS = -Wall -w -s -std=c++17 -O2 -I. -IPowerChellLib -I$(EA_SRC) -DNDEBUG -DUNICODE -D_UNICODE
# MinGW on Linux often lacks mscoree/Propsys; we build libmscoree.a from .def and don't need Propsys (compat layer used)
MSCOREE_IMPLIB = libmscoree.a
LFLAGS = -L. -lmscoree -lshell32 -lole32 -loleaut32 -luser32

# Source files (explicit paths; wildcards do not expand in Make variable assignment)
SRC_FILES = PowerChell/PowerChell/main.cpp PowerChellLib/clr.cpp PowerChellLib/patch.cpp PowerChellLib/powershell.cpp PowerChellLib/clr_guids.c

# Build import lib for mscoree.dll so linker can resolve CLRCreateInstance (needed when cross-compiling from Linux)
$(MSCOREE_IMPLIB): mscoree.def
	$(DLLTOOL) -k -d mscoree.def -l $(MSCOREE_IMPLIB) -D mscoree.dll

# Build Targets
all: dll exe

exe: PowerChell.x64.exe

dll: $(MSCOREE_IMPLIB) PowerChell.x64.dll

# Executable Target
PowerChell.x64.exe: $(SRC_FILES)
	$(CXX) $^ -o $@ $(CFLAGS) $(LFLAGS) -mwindows

# DLL Target
PowerChell.x64.dll: $(SRC_FILES)
	$(CXX) $^ -o $@ -shared -D_WINDLL $(CFLAGS) $(LFLAGS) -mwindows
	cp $@ ../bin/powerchell.x64.dll

# Clean up
clean:
	rm -f *.dll *.exe $(MSCOREE_IMPLIB)

BOFNAME := remote-exec-scshell
CC_x64 := x86_64-w64-mingw32-gcc
CFLAGS := -w -Wno-int-conversion -Wno-incompatible-pointer-types -Os -s -c

all:
	$(CC_x64) -o bin/$(BOFNAME).x64.o $(CFLAGS) src/main.c -I src
	x86_64-w64-mingw32-strip --strip-unneeded bin/$(BOFNAME).x64.o
	cp bin/$(BOFNAME).x64.o ../../bin
BOFNAME := remote-exec-winrm
CC_x64 := x86_64-w64-mingw32-g++
CFLAGS := -w -Os -s -c -masm=intel -nostdlib -Wno-nonnull -fno-asynchronous-unwind-tables -fno-exceptions -fno-rtti

all:
	$(CC_x64) -o bin/$(BOFNAME).x64.o $(CFLAGS) src/main.cpp -I src
	x86_64-w64-mingw32-strip --strip-unneeded bin/$(BOFNAME).x64.o
	cp bin/$(BOFNAME).x64.o ../../bin
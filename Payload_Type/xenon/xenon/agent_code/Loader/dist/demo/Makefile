CC=i686-w64-mingw32-gcc
CC_64=x86_64-w64-mingw32-gcc

all: run.x86.exe run.x64.exe test.x86.dll test.x64.dll test.x86.o test.x64.o

#
# x86 targets
#
run.x86.exe:
	$(CC) -DWIN_X86 -masm=intel -Wall -Wno-pointer-arith src/run.c -o run.x86.exe -lws2_32

test.x86.dll:
	$(CC) -shared src/testdll.c -o test.x86.dll

test.x86.o:
	$(CC) -c src/testobj.c -o test.x86.o

#
# x64 targets
#
run.x64.exe:
	$(CC_64) -DWIN_X64 -masm=intel -Wall -Wno-pointer-arith src/run.c -o run.x64.exe -lws2_32

test.x64.dll:
	$(CC_64) -shared src/testdll.c -o test.x64.dll

test.x64.o:
	$(CC_64) -c src/testobj.c -o test.x64.o

#
# Other targets
#
clean:
	rm -f run.x86.exe run.x64.exe test.x86.dll test.x64.dll test.x86.o test.x64.o

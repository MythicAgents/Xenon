CC_64=x86_64-w64-mingw32-gcc

all: bin/execute_assembly.x64.o bin/loader.x64.o

bin:
	mkdir bin

bin/loader.x64.o: bin
	$(CC_64) -DWIN_X64 -shared -masm=intel -Wall -Wno-pointer-arith -fno-toplevel-reorder -c src/loader.c -o bin/loader.x64.o

bin/execute_assembly.x64.o: bin
	$(CC_64) -DWIN_X64 -shared -masm=intel -Wall -Wno-pointer-arith -fno-toplevel-reorder -c src/execute_assembly.c -o bin/execute_assembly.x64.o

clean:
	rm -f bin/*
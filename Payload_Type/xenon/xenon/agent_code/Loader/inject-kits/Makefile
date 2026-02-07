CC_64=x86_64-w64-mingw32-gcc

all: bin/inject_spawn.x64.o bin/inject_explicit.x64.o

bin:
	mkdir bin

bin/inject_spawn.x64.o: bin
	$(CC_64) -DWIN_X64 -Wall -c inject_spawn.c -o bin/inject_spawn.x64.o

bin/inject_explicit.x64.o: bin
	$(CC_64) -DWIN_X64 -Wall -c inject_explicit.c -o bin/inject_explicit.x64.o

clean:
	rm -f bin/*

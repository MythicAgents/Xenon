CC_64 = x86_64-w64-mingw32-gcc
DLLTOOL = x86_64-w64-mingw32-dlltool

# CLR (mscoree) import lib for cross-compile
MSCOREE_DEF = mscoree.def
MSCOREE_IMPLIB = bin/libmscoree.a

CFLAGS = -DWIN_X64 -Os -s -Isrc -Wall -Wno-pointer-arith -DUNICODE -D_UNICODE
LDFLAGS = -Lbin -lmscoree -lole32 -loleaut32 -lpsapi -lshell32

all: bin/execute_assembly.x64.dll

bin:
	mkdir -p bin

$(MSCOREE_IMPLIB): $(MSCOREE_DEF) | bin
	$(DLLTOOL) -k -d $(MSCOREE_DEF) -l $(MSCOREE_IMPLIB) -D mscoree.dll

bin/execute_assembly.x64.dll: $(MSCOREE_IMPLIB) src/main.c | bin
	$(CC_64) src/main.c -o $@ -shared $(CFLAGS) $(LDFLAGS) -mwindows
	cp $@ ../bin/execute_assembly.x64.dll

clean:
	rm -f bin/*
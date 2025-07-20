CC = x86_64-w64-mingw32-gcc-win32
CFLAGS = -nostdlib  -ffunction-sections -fno-ident -fno-asynchronous-unwind-tables -w -Os
LDFLAGS = -Wl,-e,AlignRSP

# Default pipename (can be overridden on command line)
PIPENAME ?= xenon
VARS = -DPIPENAME="\"$(PIPENAME)\""

set_pipe.exe: set_pipe.c
	@echo "[*] Compiling stub..."
	$(CC) $(CFLAGS) $(LDFLAGS) $(VARS) -o $@ $^
	
	@echo "[*] Stripping symbols from PE..."
	strip $@
	
	@echo "[*] Extracting .text section to stub.bin..."
	objcopy -O binary --only-section=.text --only-section=.rdata $@ stub.bin

	@echo "[*] Patching size in stub.bin..."
	python3 patch_size.py stub.bin

	@echo "[*] Cleaning up stub.exe..."
	rm -f set_pipe.exe
	@echo "[+] stub.bin ready and patched."

.PHONY: clean
clean:
	rm -f *.bin *.exe

BOFNAME := keylogger
SRC_DIR := src
BIN_DIR := bin
CC_x64 := x86_64-w64-mingw32-gcc
CFLAGS := -Os -Wall -fno-stack-protector -fno-stack-check -mno-stack-arg-probe

all:
	mkdir -p $(BIN_DIR)
	$(CC_x64) -o $(BIN_DIR)/$(BOFNAME).x64.o -c $(SRC_DIR)/main.c -I $(SRC_DIR) $(CFLAGS)
	mkdir -p ../bin
	cp $(BIN_DIR)/$(BOFNAME).x64.o ../bin/

clean:
	rm -f $(BIN_DIR)/*.o

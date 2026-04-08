BOFNAME := remote-exec-wmi
CC_x64 := x86_64-w64-mingw32-g++

all:
	$(CC_x64) -o bin/$(BOFNAME).x64.o -c src/main.cpp -I src -l comsuppw -l comsuppwd -w
	cp bin/$(BOFNAME).x64.o ../../bin
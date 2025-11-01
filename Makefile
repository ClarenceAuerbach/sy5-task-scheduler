.PHONY: erraid tadmor

CC      := gcc
CFLAGS  := -g -Wall -std=c17 -D_DEFAULT_SOURCE

all : erraid tadmor

src/bin:
	mkdir -p src/bin

erraid : src/main/erraid.c | src/bin 
	$(CC) $(CFLAGS) src/main/erraid.c -o src/bin/erraid

tadmor : src/main/tadmor.c | src/bin 
	$(CC) $(CFLAGS) src/main/tadmor.c -o src/bin/tadmor

distclean :
	rm -f src/bin/*

run : 
	mkdir -p /tmp/$(USER)/erraid/tasks && ./src/bin/erraid $(USER)

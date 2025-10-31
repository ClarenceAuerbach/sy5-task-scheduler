.PHONY: erraid tadmor

CC      := gcc
CFLAGS  := -g -Wall -std=c17 -D_DEFAULT_SOURCE

all : erraid tadmor

bin:
	mkdir -p bin

erraid : src/erraid.c | bin
	$(CC) $(CFLAGS) src/erraid.c -o bin/erraid

tadmor : src/tadmor.c | bin
	$(CC) $(CFLAGS) src/tadmor.c -o bin/tadmor

distclean:
	rm -f bin/*

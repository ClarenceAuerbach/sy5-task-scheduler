.PHONY: erraid tadmor

CC      := gcc
CFLAGS  := -g -Wall -std=c17 -D_DEFAULT_SOURCE

all : erraid tadmor

erraid : src/erraid.c 
	$(CC) $(CFLAGS) src/erraid.c -o bin/erraid

tadmor : src/tadmor.c 
	$(CC) $(CFLAGS) src/tadmor.c -o bin/tadmor

distclean:
	rm -f bin/*

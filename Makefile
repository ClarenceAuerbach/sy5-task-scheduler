CC      ?= gcc
CFLAGS  ?= -g -Wall -std=c17 -D_DEFAULT_SOURCE

all : erraid tadmor

erraid : erraid.c 

tadmor  : tadmor.c 

distclean:
	rm -f erraid tadmor *.o
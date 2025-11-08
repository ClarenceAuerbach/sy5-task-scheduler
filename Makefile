.PHONY: run kill clean test

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c17 -Iinclude

SRCS_MAIN = $(wildcard src/main/*.c)
SRCS_TEST = $(wildcard src/test/*.c)
OBJS_MAIN = $(SRCS_MAIN:src/main/%.c=obj/main/%.c)
OBJS_TEST = $(OBJS_MAIN:src/test/%.c=obj/test/%.c)

all: bin/erraid test

test: bin/tests
	./bin/tests

bin/erraid: $(OBJS_MAIN)
	mkdir -p bin
	$(CC) $^ -o $@

bin/tests: $(OBJS_TEST)
	mkdir -p bin
	$(CC) $^ -o $@

obj/main/%.o: src/main/%.c
	mkdir -p obj/main
	$(CC) $(CFLAGS) -c $< -o $@

obj/test/%.o: src/test/%.c
	mkdir -p obj/test
	$(CC) $(CFLAGS) -c $< -o $@

distclean:
	rm -rf bin obj

# Won't be permanent, removed once we have tadmor
run : 
	mkdir -p /tmp/$(USER)/erraid/tasks && ./bin/erraid $(USER)

kill : 
	kill $$(cat /tmp/$(USER)/erraid/tasks/erraid_pid.pid) 2>/dev/null || true
	rm -f /tmp/$(USER)/erraid/tasks/erraid_pid.pid

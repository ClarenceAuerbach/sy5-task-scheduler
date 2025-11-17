.PHONY: run kill clean test prepare_tests

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c17 -Iinclude

MAIN_SRCS = $(wildcard src/main/*.c)
TEST_SRCS = $(wildcard src/test/*.c)
MAIN_OBJS = $(MAIN_SRCS:src/main/%.c=obj/main/%.o)
TEST_BINS = $(TEST_SRCS:src/test/%.c=bin/test/%)

all: bin/erraid test

test: prepare_tests
	@echo "\033[1mRunning Tests\033[0m"
	@passed=0; failed=0; \
		for t in $(TEST_BINS); do \
			echo "\033[35mRunning $$t...\033[0m"; \
			if $$t; then \
				echo "\033[32m[ OK ]\033[0m $$t"; \
				passed=$$((passed+1)); \
			else \
				echo "\033[31m[FAIL]\033[0m $$t"; \
				failed=$$((failed+1)); \
			fi; \
		done; \
		if [ $$((failed)) -eq 0 ]; then \
			echo "\033[32;1mAll tests passed!\033[0m"; \
		else \
			echo "\033[32m$$passed passed\n\033[31m$$failed failed\033[0m"; \
		fi;

prepare_tests: $(TEST_BINS)

bin/erraid: $(MAIN_OBJS) | bin
	$(CC) $^ -o $@

# Linking all main object files to avoid dependency issues
bin/test/test_%: obj/test/test_%.o $(filter-out obj/main/erraid.o, $(MAIN_OBJS)) | bin/test
	$(CC) $^ -o $@

obj/main/%.o: src/main/%.c | obj/main
	$(CC) $(CFLAGS) -c $< -o $@

obj/test/%.o: src/test/%.c | obj/test
	$(CC) $(CFLAGS) -c $< -o $@

obj/test obj/main bin/test bin:
	mkdir -p $@

distclean:
	rm -rf bin obj

# Won't be permanent, removed once we have tadmor
run : bin/erraid
	mkdir -p /tmp/$(USER)/erraid/tasks && ./bin/erraid ./src/test/data/exemple-arborescence-1/tmp-username-erraid

kill : 
	@PID=$$(ps aux | grep '[e]rraid' | awk '{print $$2}'); \
	if [ -n "$$PID" ]; then \
		kill $$PID ; \
	fi

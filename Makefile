.PHONY: run kill clean

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c17 -Iinclude
SRC_DIR = src/main
OBJ_DIR = obj
BIN_DIR = bin

SRCS = $(wildcard $(SRC_DIR)/*.c)
# Generates obj/xxx.o files from src/xxx.c files
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET = $(BIN_DIR)/erraid


all: $(TARGET)

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

# Rule for generating obj files from source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compilation using obj files
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(OBJS) -o $@

distclean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
# Won't be permanent, removed once we have tadmor
run : 
	mkdir -p /tmp/$(USER)/erraid/tasks && ./bin/erraid $(USER)

kill : 
	kill $$(cat /tmp/$(USER)/erraid/tasks/erraid_pid.pid) 2>/dev/null || true
	rm -f /tmp/$(USER)/erraid/tasks/erraid_pid.pid

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2

# Target executable
TARGET = ecmsocket

# Source file
SRC = ecmsocket.c

# Build directories
BUILD_DIR = ../build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin

# Object file
OBJ = $(OBJ_DIR)/$(SRC:.c=.o)

# Final binary
BIN = $(BIN_DIR)/$(TARGET)

# Default Screen parameter argument
# To get values, Run fbset -i
SCREEN_XRES ?= 1920
SCREEN_YRES ?= 1080
SCREEN_PIXEL_BITS ?= 16
SCREEN_LINE_LEN ?= 3840

# Ensure build directories exist
$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $(OBJ_DIR) $(BIN_DIR)

# Build rules
all: $(OBJ_DIR) $(BIN_DIR) $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -DSCREEN_XRES=$(SCREEN_XRES) -DSCREEN_YRES=$(SCREEN_YRES) -DSCREEN_PIXEL_BITS=$(SCREEN_PIXEL_BITS) -DSCREEN_LINE_LEN=$(SCREEN_LINE_LEN) -o $@ $^

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -DSCREEN_XRES=$(SCREEN_XRES) -DSCREEN_YRES=$(SCREEN_YRES) -DSCREEN_PIXEL_BITS=$(SCREEN_PIXEL_BITS) -DSCREEN_LINE_LEN=$(SCREEN_LINE_LEN) -c $< -o $@

# Clean rule
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean

# Compiler
CC = gcc

# Compiler Flags
CFLAGS = -Wall -Wextra -O2

# Linker Flags
LDFLAGS = -lgpiod

# Target executable
TARGET = ecmio

# Source files
SRC = ecmio.c

# Build directories
BUILD_DIR = ../build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin

# Object file
OBJ = $(OBJ_DIR)/$(SRC:.c=.o)

# Final binary
BIN = $(BIN_DIR)/$(TARGET)

# Create build directories if they don't exist
$(OBJ_DIR) $(BIN_DIR):
        mkdir -p $(OBJ_DIR) $(BIN_DIR)

# Build the executable
all: $(OBJ_DIR) $(BIN_DIR) $(BIN)

$(BIN): $(OBJ)
        $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ): ecmio.c | $(OBJ_DIR)
        $(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
        rm -rf $(BUILD_DIR)
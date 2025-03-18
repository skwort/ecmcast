# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2

# Target executable
TARGET = ecmpipe #framebuffer_pixel

# Source files
SRCS = ecmpipe.c #$(wildcard *.c)

# Object files
OBJS = $(SRCS:.c=.o)

# Build rules
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean

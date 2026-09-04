# Compiler and flags
CC = gcc
CFLAGS = -Wall -g

# Directories
SRC_DIR = src
BUILD_DIR = build

# Target executable
TARGET = client

# Source and object files
SRCS = $(SRC_DIR)/client.c
OBJS = $(BUILD_DIR)/client.o

# Default target
all: $(TARGET)

# Link executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Compile C source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Run the client
run: $(TARGET)
	./$(TARGET)

# Clean object files and binary
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

.PHONY: all run clean

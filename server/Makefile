# Compiler and flags
CC = gcc
CFLAGS = -Wall -g -pthread

# Target executable
TARGET = server

# All source files
SRCS = 1_socket.c 2_handle_client.c 3_auth.c 4_1_transaction.c 4_client_menu.c 5_admin_menu.c 6_employee_menu.c 7_manager_menu.c

# Object files
OBJS = $(SRCS:.c=.o)

# Default target
all: $(TARGET)

# Link all object files
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Run the server
run: $(TARGET)
	./$(TARGET)

# Clean object files and binary
clean:
	rm -f $(OBJS) $(TARGET)

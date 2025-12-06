# Compiler and Flags
CC = gcc
# Added -g for debug symbols as required by PDF page 19
CFLAGS = -std=c99 -Wall -Werror -pedantic-errors -DNDEBUG -pthread -g

# Source and Object files
# We explicitly list the object file provided by the staff
PROVIDED_OBJ = my_system_call_c.o

# Helper to find all .c files (smash.c, commands.c, signals.c)
SRCS = $(wildcard *.c)
# Convert .c filenames to .o filenames
OBJS = $(SRCS:.c=.o)

# Target executable name
TARGET = smash

# Default rule
all: $(TARGET)

# Linking rule: Requires your objects AND the provided system call object
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(PROVIDED_OBJ) -o $(TARGET)

# Compilation rule: Turns .c into .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -rf $(TARGET) $(OBJS)
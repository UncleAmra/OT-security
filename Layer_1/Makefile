# Makefile for compiling the Modular OT Security Physics Engine (Layer 1)
# Works on both Linux and Windows (using GCC/MinGW)

TARGET = physics_engine
CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude

# Source files
SRCS = src/main.c \
       src/boiler.c \
       src/turbine.c \
       src/generator.c \
       src/condenser.c \
       src/modbus_client.c

# Object files
OBJS = $(SRCS:.c=.o)

# Detect OS
ifeq ($(OS),Windows_NT)
    LIBS = -lws2_32
    RM = del /q /f
    EXEC = $(TARGET).exe
    CLEAN_CMD = $(RM) $(TARGET).exe src\*.o 2>nul || exit 0
else
    LIBS = -lm
    RM = rm -f
    EXEC = $(TARGET)
    CLEAN_CMD = $(RM) $(EXEC) src/*.o
endif

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJS) $(LIBS)

# Compilation rule for object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(CLEAN_CMD)

.PHONY: all clean

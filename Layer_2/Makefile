# ==============================================================================
# Makefile for Purdue Layer 2: Industrial C++ PLC Runtime Engine
# Supports both Linux and Windows (MinGW / GCC)
# ==============================================================================

TARGET = plc_controller
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude -I../common

SRCS = src/main.cpp \
       src/PlcDataBank.cpp \
       src/PlcSafetyInterlocks.cpp \
       src/PlcScanEngine.cpp \
       src/ModbusTcpServer.cpp

OBJS = $(SRCS:.cpp=.o)

ifeq ($(OS),Windows_NT)
    LIBS = -lws2_32
    RM = del /q /f
    EXEC = $(TARGET).exe
    CLEAN_CMD = $(RM) $(TARGET).exe src\*.o 2>nul || exit 0
else
    LIBS = -lpthread
    RM = rm -f
    EXEC = $(TARGET)
    CLEAN_CMD = $(RM) $(EXEC) src/*.o
endif

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(EXEC) $(OBJS) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(CLEAN_CMD)

.PHONY: all clean

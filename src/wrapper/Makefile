CC = g++
CFLAGS = -fPIC -std=c++20 -o3 -Wall -Wextra
LDFLAGS = -shared -ldl -pthread -DDEBUG2 -DSOCKET_DEBUG2
SOURCES = debug_printf.cpp general_hooks.cpp openg_intercepts.cpp sdl2_intercepts.cpp plthook_elf.c daemon_comm.cpp ../daemon/daemon_utils.cpp
TARGET = libhook.so

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) $(LDFLAGS) -o $(TARGET)

preprocess: $(SOURCES)
	$(CC) $(CFLAGS) -E $(SOURCES) $(LDFLAGS)

clean:
	rm -f $(TARGET)
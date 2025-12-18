CC = g++
CFLAGS = -std=c++20 -o3 -Wall -Wextra
LDFLAGS = -lrt -lnvidia-ml -DDEBUG2
SOURCES = ../wrapper/debug_printf.cpp daemon.cpp daemon_utils.cpp
TARGET = daemon.out

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) $(LDFLAGS) -o $(TARGET)

preprocess: $(SOURCES)
	$(CC) $(CFLAGS) -E $(SOURCES) $(LDFLAGS)

clean:
	rm -f $(TARGET)


MAKEFLAGS += Rr

TARGET := a.exe

.PHONY: all clean

all: $(TARGET)

clean:
	rm -f $(TARGET)


$(TARGET): main.cpp
	g++  $< -o $@ -Wall -Wextra -Wpedantic -Wshadow -std=c++17 -Ofast -march=native

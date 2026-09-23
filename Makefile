CXX := g++
CC := gcc
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic
CFLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS := -Iinclude

TARGET := shattered_depths
OBJ := build/main.o build/rng.o

all: $(TARGET)

build:
	mkdir -p build

build/main.o: src/main.cpp | build
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

build/rng.o: src/rng.c | build
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $@

clean:
	rm -rf build $(TARGET)

.PHONY: all clean

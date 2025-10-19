BUILD_DIR         := build
COMPILER_SOURCES  := $(shell find . -name '*.cpp')
COMPILER_OBJECTS  := $(patsubst $(COMPILER_SOURCES)/%.cpp,$(BUILD_DIR)/%_cpp.o,$(COMPILER_SOURCES))

CC     := g++
CFLAGS := -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wdouble-promotion \
		-Wnull-dereference -Wformat=2 -Wmissing-include-dirs -Wswitch-enum -Wuninitialized -Werror \
		-g -std=c++20

.PHONY: all

all: csompiler

csompiler: $(COMPILER_OBJECTS)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(COMPILER_OBJECTS) -o $(BUILD_DIR)/csompiler

$(BUILD_DIR)/%_cpp.o: $(COMPILER_SOURCES)/%.cpp
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)/*

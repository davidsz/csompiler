BUILD_DIR         := build
COMPILER_SOURCES  := $(shell find . -name '*.cpp')
COMPILER_OBJECTS  := $(patsubst $(COMPILER_SOURCES)/%.cpp,$(BUILD_DIR)/%_cpp.o,$(COMPILER_SOURCES))

CC     := g++
CFLAGS := -g

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

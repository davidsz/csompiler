BUILD_DIR         := build
SRC_DIR           := .
COMPILER_SOURCES  := $(shell find . -name '*.cpp')
COMPILER_OBJECTS  := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%_cpp.o,$(COMPILER_SOURCES))

CC     := g++
CFLAGS := -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wdouble-promotion \
		-Wnull-dereference -Wformat=2 -Wmissing-include-dirs -Wswitch-enum -Wuninitialized -Werror \
		-g -std=c++20 -MMD -MP \
		-ferror-limit=5 -ftemplate-backtrace-limit=3 -fcolor-diagnostics -fno-elide-type \
		-fno-show-column -fno-caret-diagnostics -fdiagnostics-format=clang \
		-Isrc \
#		-DNDEBUG=1

.PHONY: all clean

all: $(BUILD_DIR)/csompiler

$(BUILD_DIR)/csompiler: $(COMPILER_OBJECTS)
	$(CC) $(CFLAGS) $(COMPILER_OBJECTS) -o $@

$(BUILD_DIR)/%_cpp.o: $(SRC_DIR)/%.cpp
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)/*

# Load dependency files
-include $(COMPILER_OBJECTS:.o=.d)

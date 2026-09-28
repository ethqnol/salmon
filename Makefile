CXX      := g++
LLVM_CXXFLAGS := $(shell llvm-config --cxxflags | sed 's/-fno-exceptions//; s/-std=c++[0-9]*//')
LLVM_LDFLAGS  := $(shell llvm-config --ldflags --system-libs --libs core)
CXXFLAGS := -std=c++20 -Wall -Wextra -Wno-unused-parameter -Wno-maybe-uninitialized -Iinclude -MMD -MP $(LLVM_CXXFLAGS)

BUILD_DIR := build
SRC_DIR   := src

SRCS     := $(shell find $(SRC_DIR) -name '*.cpp')
OBJS     := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRCS))
DEPS     := $(OBJS:.o=.d)
TARGET   := salmon

.PHONY: all run clean

# Default target
all: $(TARGET)

# Link object files into final executable
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LLVM_LDFLAGS) -o $@

# Compile source files into objects inside build/
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

-include $(DEPS)
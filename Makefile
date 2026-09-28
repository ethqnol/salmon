CXX      := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Iinclude -MMD -MP

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
	$(CXX) $(OBJS) -o $@

# Compile source files into objects inside build/
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

-include $(DEPS)
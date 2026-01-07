# Compiler and flags
CXX := g++
CXXFLAGS := -O3 -std=c++23 -Wall -Wextra -Iinclude

# Directories
SRC_DIR := src
TEST_DIR := tests
OBJ_DIR := obj
BIN_DIR := bin

# Source files
SRCS := $(SRC_DIR)/mesh.cpp $(SRC_DIR)/spmc_queue/spmc_queue.cpp
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))

# Test files
TEST_SRCS := $(TEST_DIR)/tiny_obj_loader/tiny_obj_loader_harness.cpp \
             $(TEST_DIR)/rapidobj/rapidobj_harness.cpp \
             $(TEST_DIR)/fast_obj/fast_obj_harness.cpp
TEST_BINS := $(patsubst $(TEST_DIR)/%_harness.cpp,$(BIN_DIR)/%_harness,$(TEST_SRCS))

# Default target
.PHONY: all clean
all: $(BIN_DIR)/mesh_lib_harness $(TEST_BINS)

# Build object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Build the main test harness
$(BIN_DIR)/mesh_lib_harness: $(OBJS) $(TEST_DIR)/fast_obj/fast_obj_harness.cpp
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Build other test harnesses
$(BIN_DIR)/%_harness: $(TEST_DIR)/%_harness.cpp
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

# Clean build files
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

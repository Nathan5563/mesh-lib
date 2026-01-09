# Compiler and flags
CXX := g++
CXXFLAGS := -O3 -std=c++23 -Wall -Wextra -Iinclude

# Directories
SRC_DIR := src
TEST_DIR := tests
BIN_DIR := bin

# Default target
.PHONY: all clean
all: $(BIN_DIR)/mesh_lib_harness $(BIN_DIR)/tiny_obj_loader_harness $(BIN_DIR)/rapidobj_harness $(BIN_DIR)/fast_obj_harness

# Build the test harnesses
$(BIN_DIR)/mesh_lib_harness: $(SRC_DIR)/mesh.cpp $(TEST_DIR)/mesh_lib/mesh_lib_harness.cpp
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@
$(BIN_DIR)/tiny_obj_loader_harness: $(TEST_DIR)/tiny_obj_loader/tiny_obj_loader_harness.cpp
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@
$(BIN_DIR)/rapidobj_harness: $(TEST_DIR)/rapidobj/rapidobj_harness.cpp
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@
$(BIN_DIR)/fast_obj_harness: $(TEST_DIR)/fast_obj/fast_obj_harness.cpp
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Clean build files
clean:
	rm -rf $(BIN_DIR)

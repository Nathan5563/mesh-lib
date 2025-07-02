CXX := g++
CXXFLAGS := -g3 -std=c++17 -Wall -Wextra -O3

# Sources for the library part
SRCS := src/mesh.cpp src/obj-parser.cpp

# Object files from sources (placed in obj/)
OBJS := $(patsubst src/%.cpp,obj/%.o,$(SRCS))

# Test harness source and executable (binary in bin/)
TEST_SRC := tests/harness.cpp
TEST_EXE := bin/harness

.PHONY: all clean run

all: $(TEST_EXE)

# Compile library sources
obj/%.o: src/%.cpp
	mkdir -p obj
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile and link test executable
$(TEST_EXE): $(OBJS) $(TEST_SRC)
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -rf obj/* bin/*

CXX := g++
CXXFLAGS := -O3 -std=c++17 -Wall -Wextra

SRCS := src/mesh.cpp src/obj-parser.cpp

OBJS := $(patsubst src/%.cpp,obj/%.o,$(SRCS))

TEST_SRC := tests/harness.cpp
TEST_EXE := bin/harness

TINYOBJLOADER_SRC := tests/tinyobjloader/*
TINYOBJLOADER_EXE := bin/tinyobjloader

.PHONY: all clean

all: $(TEST_EXE) $(TINYOBJLOADER_EXE)

obj/%.o: src/%.cpp
	mkdir -p obj
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TEST_EXE): $(OBJS) $(TEST_SRC)
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $^ -o $@ -lfmt

$(TINYOBJLOADER_EXE): $(TINYOBJLOADER_SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -rf obj/* bin/*

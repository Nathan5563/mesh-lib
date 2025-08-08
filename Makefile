CXX := g++
CXXFLAGS := -O3 -std=c++17 -Wall -Wextra

SRCS := src/mesh.cpp src/obj-parser.cpp

OBJS := $(patsubst src/%.cpp,obj/%.o,$(SRCS))

TEST_SRC := tests/mesh-lib/*
TEST_EXE := bin/mesh-lib-harness

TINYOBJLOADER_SRC := tests/tinyobjloader/*
TINYOBJLOADER_EXE := bin/tinyobjloader-harness

RAPIDOBJ_SRC := tests/rapidobj/*
RAPIDOBJ_EXE := bin/rapidobj-harness

FAST_OBJ_SRC := tests/fast_obj/*
FAST_OBJ_EXE := bin/fast_obj-harness

.PHONY: all clean

all: $(TEST_EXE) $(TINYOBJLOADER_EXE) $(RAPIDOBJ_EXE) $(FAST_OBJ_EXE)

obj/%.o: src/%.cpp
	mkdir -p obj
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TEST_EXE): $(OBJS) $(TEST_SRC)
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $^ -o $@ -lfmt

$(TINYOBJLOADER_EXE): $(TINYOBJLOADER_SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(RAPIDOBJ_EXE): $(RAPIDOBJ_SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@ -pthread

$(FAST_OBJ_EXE): $(FAST_OBJ_SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -rf obj/* bin/*

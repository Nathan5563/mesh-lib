#include <iostream>
#include <string>
#include <chrono>

#include "../include/mesh.hpp"

int main(int argc, char* argv[])
{
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <path-to-obj-input> <path-to-obj-output>" << std::endl;
        return 1;
    }

    // WARNING: no error checking here
    std::string input = argv[1];
    std::string output = argv[2];

    Mesh mesh;

    auto start_load = std::chrono::high_resolution_clock::now();    
    mesh.fromObj(input);
    auto end_load = std::chrono::high_resolution_clock::now();
    auto load_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_load - start_load).count();

    auto start_write = std::chrono::high_resolution_clock::now();
    mesh.toObj(output);
    auto end_write = std::chrono::high_resolution_clock::now();
    auto write_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_write - start_write).count();

    std::cerr << "Load time: " << load_duration << " us" << std::endl;
    std::cerr << "Write time: " << write_duration << " us" << std::endl;

    return 0;
}

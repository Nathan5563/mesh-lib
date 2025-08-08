#include <iostream>
#include <string>
#include <chrono>

#include "../../include/mesh.hpp"

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <path-to-obj-input>" << std::endl;
        return 1;
    }

    // WARNING: no error checking here
    std::string input = argv[1];
    // std::string output = argv[2];

    Mesh mesh;

    mesh.fromObj(input);

    // mesh.toObj(output);

    return 0;
}

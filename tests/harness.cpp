#include <iostream>
#include <string>

#include "../include/mesh.hpp"

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_obj_file>" << std::endl;
        return 1;
    }

    // WARNING: no error checking here
    std::string path = argv[1];

    Mesh mesh;

    mesh.fromObj(path);
    
    mesh.toObj();

    return 0;
}

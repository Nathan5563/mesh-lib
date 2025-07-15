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

    // Try "mesh.fromObj" or "mesh.fromObjParallel"
    mesh.fromObjParallel(path);
    
    // Uncomment to export the mesh to stdout in obj format
    // mesh.toObj();

    return 0;
}

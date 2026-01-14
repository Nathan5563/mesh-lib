#include <iostream>
#include <string>

#include "../../include/mesh.hpp"

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <path_to_mesh_file>" << std::endl;
        return 1;
    }

    // WARNING: no error checking here
    std::string path = argv[1];

    Mesh mesh;
    if (!mesh.importObj(path.c_str()))
    {
        std::cerr << "Error loading mesh from file: " << path << std::endl;
        return 1;
    }

    // std::string filename = std::string(argv[1]).substr(std::string(argv[1]).find_last_of("/\\") + 1);
    // mesh.exportObj(("/home/nathan/projects/mesh-lib/data/output/" + filename).c_str());

    return 0;
}

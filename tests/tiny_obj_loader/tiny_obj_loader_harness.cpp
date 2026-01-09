#include <iostream>
#include <string>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <path_to_obj_file>" << std::endl;
        return 1;
    }

    // WARNING: no error checking here
    std::string path = argv[1];

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn;
    std::string err;
    tinyobj::LoadObj(
        &attrib,
        &shapes,
        &materials,
        &warn,
        path.c_str(),
        nullptr,
        false
    );

    if (err.empty() == false)
    {
        std::cerr << "Error parsing file: " << err << std::endl;
        return 1;
    }

    return 0;
}

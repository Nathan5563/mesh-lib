#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#include "../include/mesh.hpp"
#include "obj-parser.hpp"

// ---------------------------------------------------------------------------
//   Core API
// ---------------------------------------------------------------------------

void Mesh::clear()
{
    this->vertices.clear();
    this->faces.clear();
}

void Mesh::fromObj(const std::string &path)
{
    std::ifstream obj_file(path);
    if (!obj_file.is_open())
    {
        std::cerr << "Failed to open file" << std::endl;
        return;
    }

    this->clear();

    importMeshFromObj(*this, obj_file);
}

void Mesh::toObj() const
{
    std::cout << "# Vertices:" << std::endl;
    for (const auto& vertex : this->vertices) {
        std::cout << "v " << vertex.x << " " << vertex.y << " " << vertex.z << std::endl;
    }

    std::cout << "# Faces:" << std::endl;
    for (const auto& face : this->faces) {
        std::cout << "f " << face.a << " " << face.b << " " << face.c << std::endl;
    }
}

#include <iostream>
#include <string>

#include "../include/mesh.hpp"

int main()
{
    Mesh mesh;

    std::string path = /* insert .obj file path here */;

    mesh.fromObj(path);
    
    mesh.toObj();
}

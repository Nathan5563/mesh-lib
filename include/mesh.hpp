#ifndef MESH_HPP
#define MESH_HPP

// Mesh data structure

#include <memory>

class _MeshImpl;

class Mesh
{
public:
    Mesh();

    // Imports an OBJ file into this Mesh.
    // Returns false on failure.
    bool importObj(const char* path);

    // Exports this Mesh as an OBJ file.
    // Returns false on failure.
    bool exportObj(const char* path) const;

private:
    std::unique_ptr<_MeshImpl> _impl;
};

#endif // mesh.hpp

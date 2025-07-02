#ifndef __MESH_HPP__
#define __MESH_HPP__

#include <vector>
#include <string>

// Represents a vertex in the 3D coordinate plane.
struct Vertex
{
    // Coordinates of the vertex in the plane
    float x, y, z;
};

// Represents a face in the 3D coordinate plane.
//
// WARNING: All faces are currently expected to be triangles.
struct Face
{
    // Indices into the list of vertices
    size_t a, b, c;
};

struct Mesh
{
    // 0-indexed list of all vertices in the mesh
    std::vector<Vertex> vertices;

    // 0-indexed list of all faces in the mesh
    std::vector<Face> faces;

    // Clear the contents of the calling Mesh object
    void clear();

    // Replace the calling Mesh object's data with data from a .obj file
    void fromObj(const std::string &path);

    // Export the calling Mesh object in .obj format to stdout
    void toObj() const;
};

#endif // __MESH_HPP__

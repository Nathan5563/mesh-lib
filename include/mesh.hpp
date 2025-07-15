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

struct Texture
{
    float u, v;
};

struct Normal
{
    float x, y, z;
};

// Represents a face in the 3D coordinate plane.
//
// WARNING: All faces are currently expected to be triangles.
struct Face
{
    // Indices into the list of vertices
    int64_t v1, v2, v3;

    // Indices into the list of textures
    int64_t vt1, vt2, vt3;

    // Indices into the list of normals
    int64_t vn1, vn2, vn3;
};

struct Mesh
{
    // 0-indexed list of all vertices in the mesh
    std::vector<Vertex> vertices;

    // 1-indexed list of all faces in the mesh as appears in the obj file
    std::vector<Face> faces;

    std::vector<Texture> textures;

    std::vector<Normal> normals;

    // Clear the contents of the calling Mesh object
    void clear();

    // Replace the calling Mesh object's data with data from a .obj file
    void fromObj(const std::string &path);

    // Replace the calling Mesh object's data with data from a .obj file
    // Uses threads to parallelize parsing
    void fromObjParallel(const std::string &path);

    // Export the calling Mesh object in .obj format to stdout
    void toObj() const;
};

#endif // __MESH_HPP__

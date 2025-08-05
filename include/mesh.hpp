#ifndef MESH_HPP
#define MESH_HPP

#include <vector>
#include <string>

struct Vertex
{
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

// WARNING: All faces are currently expected to be triangles.
struct Face
{
    // Indices into mesh.vertices
    int64_t v1, v2, v3;

    // Indices into mesh.textures
    int64_t vt1, vt2, vt3;

    // Indices into mesh.normals
    int64_t vn1, vn2, vn3;
};

struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<Face> faces;
    std::vector<Texture> textures;
    std::vector<Normal> normals;

    // Clear the contents of the calling Mesh object
    void clear();

    // Replace the calling Mesh object's data with data from a .obj file
    void fromObj(const std::string &path);

    // Export the calling Mesh object in .obj format to stdout
    void toObj(const std::string &path) const;
};

#endif // MESH_HPP

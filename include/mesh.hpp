#ifndef MESH_HPP
#define MESH_HPP

#include <cstdint>
#include <vector>
#include <string>

struct Vertex
{
    float x, y, z; // x, y, z coordinates
};

struct Texture
{
    float u, v; // u, v coordinates
};

struct Normal
{
    float x, y, z; // x, y, z coordinates
};

struct Indices
{
    size_t start; // first index in associated index vector
    size_t len; // number of indices in associated index vector
};

struct Face
{
    Indices v; // vertex indices
    Indices vt; // texture indices
    Indices vn; // normal indices
};

struct Mesh
{
    std::vector<Vertex> vertices; // vertices
    std::vector<Texture> textures; // texture coordinates
    std::vector<Normal> normals; // normals

    std::vector<int64_t> vertex_indices; // vertex indices as referenced by faces
    std::vector<int64_t> texture_indices; // texture indices as referenced by faces
    std::vector<int64_t> normal_indices; // normal indices as referenced by faces
    
    std::vector<Face> faces; // faces

    // Clear the contents of the calling Mesh object
    void clear();

    // Replace the calling Mesh object's data with data from a .obj file
    void fromObj(const std::string &path);

    // Export the calling Mesh object in .obj format to stdout
    void toObj(const std::string &path) const;
};

#endif // MESH_HPP

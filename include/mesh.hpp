#ifndef MESH_HPP
#define MESH_HPP

#include <cstdint>
#include <vector>
#include <string>

struct Vec2
{
    float u, v;
};

struct Vec3
{
    float x, y, z;
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
    size_t mtl; // material index
};

struct MtlVariant
{
    enum class Type { Rgb, Spectral, Xyz } type;
    union {
        Vec3 color;
        struct { std::string file; float multiplier; } spectral;
    };
    MtlVariant() : type(Type::Rgb), color{} {}
};

struct Material
{
    std::string name;
    MtlVariant ka;
    MtlVariant kd;
    MtlVariant ks;
    MtlVariant tf;
    float d;
    bool d_halo;
    uint8_t illum;
    float ns;
    float sharpness;
    float ni;
};

struct Mesh
{
    std::vector<Vec3> vertices; // vertices
    std::vector<Vec2> textures; // texture coordinates
    std::vector<Vec3> normals; // normals

    std::vector<int64_t> vertex_indices; // vertex indices as referenced by faces
    std::vector<int64_t> texture_indices; // texture indices as referenced by faces
    std::vector<int64_t> normal_indices; // normal indices as referenced by faces
    std::vector<Face> faces; // faces

    std::vector<Material> materials; // materials

    // Clear the contents of the calling Mesh object
    void clear();

    // Replace the calling Mesh object's data with data from a .obj file
    void fromObj(const std::string &path);

    // Export the calling Mesh object in .obj format to stdout
    void toObj(const std::string &path) const;
};

#endif // MESH_HPP

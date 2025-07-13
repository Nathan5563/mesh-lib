#include <iostream>
#include "../lib/fast_float/fast_float.h"

#include "../include/obj-parser.hpp"
#include "../include/mesh.hpp"

// ---------------------------------------------------------------------------
//   Forward Declarations and Inline Functions
// ---------------------------------------------------------------------------

static void parseLine(Mesh &mesh, std::string_view line);
static void parseVertex(Mesh &mesh, std::string_view line);
static void parseTexture(Mesh &mesh, std::string_view line);
static void parseNormal(Mesh &mesh, std::string_view line);
static void parseFace(Mesh &mesh, std::string_view line);
static int parseIndex(const char *start, const char *end, size_t size);
static void updateFace(Face &f, size_t state, int v_idx, int vt_idx, int vn_idx);
inline bool is_space(char c) 
{
    return c == ' ' || c == '\t';
}

// ---------------------------------------------------------------------------
//   Core API
// ---------------------------------------------------------------------------

void importMeshFromObj(Mesh &mesh, const char *obj_file, off_t file_size)
{
    size_t pos = 0;
    size_t line_end = 0;
    std::string_view line;
    while (pos < static_cast<size_t>(file_size)) {
        const char *start = obj_file + pos;
        const char *end = obj_file + file_size;
        const char *newline = static_cast<const char *>(memchr(start, '\n', end - start));
        
        // Get a view of the current line
        line_end = newline ? (newline - obj_file) : file_size;
        line = std::string_view(obj_file + pos, line_end - pos);

        // Setup the next line before parsing to avoid infinite loops on `continue`
        pos = newline ? line_end + 1 : file_size;

        // Pass leading whitespace
        size_t i = 0;
        for (; i < line.size() && is_space(line[i]); ++i);

        // Skip comments and empty lines
        if (i >= line.size() || line[i] == '#') continue;

        parseLine(mesh, line.substr(i));
    }
}

void exportMeshToObj(const Mesh &mesh) {
    for (auto &v : mesh.vertices)
    {
        std::cout << "v " << v.x << ' ' << v.y << ' ' << v.z << '\n';
    }
    for (auto &t : mesh.textures)
    {
        std::cout << "vt " << t.u << ' ' << t.v << '\n';
    }
    for (auto &n : mesh.normals)
    {
        std::cout << "vn " << n.x << ' ' << n.y << ' ' << n.z << '\n';
    }
    
    auto printIndex = [](int idx) {
        if (idx == -1) std::cout << "";
        else std::cout << (idx + 1);
    };

    for (auto &f : mesh.faces) {
        std::cout << "f ";
        // vertex 1
        std::cout << (f.v1 + 1) << '/';
        printIndex(f.vt1);
        std::cout << '/';
        printIndex(f.vn1);
        std::cout << ' ';
        // vertex 2
        std::cout << (f.v2 + 1) << '/';
        printIndex(f.vt2);
        std::cout << '/';
        printIndex(f.vn2);
        std::cout << ' ';
        // vertex 3
        std::cout << (f.v3 + 1) << '/';
        printIndex(f.vt3);
        std::cout << '/';
        printIndex(f.vn3);
        std::cout << '\n';
    }
}

// ---------------------------------------------------------------------------
//   Templates
// ---------------------------------------------------------------------------

template<typename T>
void customPushBack(std::vector<T>& vec, const T& value, float growth_factor = 4.0f) {
    if (vec.size() == vec.capacity()) {
        size_t new_capacity = static_cast<size_t>(vec.capacity() * growth_factor);
        if (new_capacity <= vec.capacity()) {
            // Handle small capacities like 0 or 1:
            new_capacity = vec.capacity() + 1;
        }
        vec.reserve(new_capacity);
    }
    vec.push_back(value);
}

template <typename Callback>
static void forEachComponent(std::string_view line, size_t max_components, Callback cb)
{
    const char *ptr = line.data();
    const char *end = ptr + line.size();
    const char *start;
    size_t num_components = 0;
    while (ptr < end && num_components < max_components)
    {
        // Pass whitespace
        while (ptr < end && is_space(*ptr)) ++ptr;

        // Get a pointer and a size for the current component
        start = ptr;
        while (ptr < end && !is_space(*ptr)) ++ptr;

        // If there was a component found, call the callback
        if (ptr > start) cb(start, ptr, num_components);

        // Increment the number of components
        ++num_components;
    }
}

template <typename T>
static inline bool toNumber(const char *start, const char *end, T &out)
{
    auto result = fast_float::from_chars(start, end, out);
    return result.ec == std::errc();
}

// ---------------------------------------------------------------------------
//   Parsers
// ---------------------------------------------------------------------------

static void parseLine(Mesh &mesh, std::string_view line)
{
    if (line.size() < 5)
    {
        return;
    }

    if (line[0] == 'v')
    {
        if (is_space(line[1]))
        {
            parseVertex(mesh, line.substr(2));
        }
        else if (line[1] == 't' && is_space(line[2]))
        {
            parseTexture(mesh, line.substr(3));
        }
        else if (line[1] == 'n' && is_space(line[2]))
        {
            parseNormal(mesh, line.substr(3));
        }
    }
    else if (line[0] == 'f' && is_space(line[1]))
    {
        parseFace(mesh, line.substr(2));
    }
}

static void parseVertex(Mesh &mesh, std::string_view line)
{
    Vertex v = {NAN, NAN, NAN};

    forEachComponent(
        line,
        3,
        [&](const char *start, const char *end, size_t state) 
        {
            float val;
            if (toNumber(start, end, val))
            {
                if (state == 0) v.x = val;
                else if (state == 1) v.y = val;
                else if (state == 2) v.z = val;
            }
            else
            {
                // ERROR
            }
        }
    );

    customPushBack(mesh.vertices, v);
}

static void parseTexture(Mesh &mesh, std::string_view line)
{
    Texture t = {NAN, NAN};

    forEachComponent(
        line,
        2,
        [&](const char *start, const char *end, size_t state)
        {
            float val;
            if (toNumber(start, end, val))
            {
                if (state == 0) t.u = val;
                else if (state == 1) t.v = val;
            }
            else
            {
                // ERROR
            }
        }
    );

    customPushBack(mesh.textures, t);
}

static void parseNormal(Mesh &mesh, std::string_view line)
{
    Normal n = {NAN, NAN, NAN};

    forEachComponent(
        line,
        3,
        [&](const char *start, const char *end, size_t state)
        {
            float val;
            if (toNumber(start, end, val))
            {
                if (state == 0) n.x = val;
                else if (state == 1) n.y = val;
                else if (state == 2) n.z = val;
            }
            else
            {
                // ERROR
            }
        }
    );

    customPushBack(mesh.normals, n);
}

static void parseFace(Mesh &mesh, std::string_view line)
{
    Face f = {-1, -1, -1, -1, -1, -1, -1, -1, -1};

    const char *ptr = line.data();
    const char *end = ptr + line.size();

    const char *start;
    size_t state = 0;
    while (ptr < end && state < 3)
    {
        // Pass whitespace
        while (ptr < end && is_space(*ptr)) ++ptr;

        // Get a pointer and a size for the current component
        start = ptr;
        while (ptr < end && !is_space(*ptr)) ++ptr;

        // Parse the face, keeping track of '/' separators to get textures and normals
        const char *slash1 = static_cast<const char *>(memchr(start, '/', ptr - start));
        const char *slash2 = nullptr;
        int v_idx = parseIndex(start, slash1 ? slash1 : ptr, mesh.vertices.size());
        int vt_idx = -1;
        int vn_idx = -1;
        if (slash1 && slash1 + 1 < ptr)
        {
            slash2 = static_cast<const char *>(memchr(slash1 + 1, '/', ptr - (slash1 + 1)));
            vt_idx = parseIndex(slash1 + 1, slash2 ? slash2 : ptr, mesh.textures.size());
            if (slash2 && slash2 + 1 < ptr)
            {
                vn_idx = parseIndex(slash2 + 1, ptr, mesh.normals.size());
            }
        }

        // Save the indices in the Face
        updateFace(f, state, v_idx, vt_idx, vn_idx);

        ++state;
    }

    customPushBack(mesh.faces, f);
}

static int parseIndex(const char *start, const char *end, size_t size)
{
    int idx = 0;
    if (toNumber(start, end, idx)) 
    {
        if (idx < 0) idx = static_cast<int>(size) + idx + 1;
        if (idx > 0) return idx - 1;
    }
    // Represent invalid indices with -1
    return -1;
}

// ---------------------------------------------------------------------------
//   Helpers
// ---------------------------------------------------------------------------

static void updateFace(Face &f, size_t state, int v_idx, int vt_idx, int vn_idx)
{
    if (state == 0)
    {
        f.v1 = v_idx;
        f.vt1 = vt_idx;
        f.vn1 = vn_idx;
    }
    else if (state == 1)
    {
        f.v2 = v_idx;
        f.vt2 = vt_idx;
        f.vn2 = vn_idx;
    }
    else if (state == 2)
    {
        f.v3 = v_idx;
        f.vt3 = vt_idx;
        f.vn3 = vn_idx;
    }
}

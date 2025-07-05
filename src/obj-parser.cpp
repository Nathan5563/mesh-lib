#include <iostream>
#include <cctype>
#include <cmath>
#include "../lib/fast_float/fast_float.h"

#include "obj-parser.hpp"
#include "../include/mesh.hpp"

static void countAndPrealloc(const char *obj_file, off_t file_size, Mesh &mesh);
static void parseVertex(Mesh &mesh, std::string_view line);
static bool updateVertex(Vertex &v, const char *start, size_t len);
static void parseFace(Mesh &mesh, std::string_view line);
static bool updateFace(Mesh &mesh, Face &f, const char *start, size_t len);

inline bool is_space(char c)
{
    return c == ' ' || c == '\t';
}

// ---------------------------------------------------------------------------
//   Core API
// ---------------------------------------------------------------------------

void importMeshFromObj(Mesh &mesh, const char *obj_file, off_t file_size)
{
    // First pass: count vertices and faces to preallocate memory
    countAndPrealloc(obj_file, file_size, mesh);

    // Second pass: previous parsing logic
    size_t pos = 0;
    size_t line_end = 0;
    while (pos < static_cast<size_t>(file_size))
    {
        const char *start = obj_file + pos;
        const char *end = obj_file + file_size;
        const char *newline = static_cast<const char *>(memchr(start, '\n', end - start));
        if (newline == nullptr)
        {
            line_end = file_size;
        }
        else
        {
            line_end = newline - obj_file;
        }

        std::string_view line(obj_file + pos, line_end - pos);
        pos = line_end;
        if (pos < static_cast<size_t>(file_size) &&
            obj_file[pos] == '\r')
        {
            ++pos;
        }
        if (pos < static_cast<size_t>(file_size) &&
            obj_file[pos] == '\n')
        {
            ++pos;
        }

        if (line.empty() ||
            line[0] == '#' ||
            is_space(line[0]))
        {
            continue;
        }
        else if ((line.size() < 2) ||
                 (line[1] != ' ') ||
                 ((line[0] != 'v') && (line[0] != 'f')))
        {
            // std::cerr << "Unsupported features detected" << std::endl;
            continue;
        }
        else
        {
            if (line[0] == 'v')
            {
                parseVertex(mesh, line);
            }
            else
            {
                parseFace(mesh, line);
            }
        }
    }
}

void exportMeshToObj(const Mesh &mesh)
{
    std::cout << "# Vertices:" << std::endl;
    for (const auto &vertex : mesh.vertices)
    {
        std::cout << "v " << vertex.x << " " << vertex.y << " " << vertex.z << std::endl;
    }

    std::cout << "# Faces:" << std::endl;
    for (const auto &face : mesh.faces)
    {
        std::cout << "f " << face.a + 1 << " " << face.b + 1 << " " << face.c + 1 << std::endl;
    }
}

// ---------------------------------------------------------------------------
//   Helpers
// ---------------------------------------------------------------------------

static void countAndPrealloc(const char *obj_file, off_t file_size, Mesh &mesh)
{
    size_t vertex_count = 0;
    size_t face_count = 0;
    size_t pos = 0;
    size_t line_end = 0;
    while (pos < static_cast<size_t>(file_size))
    {
        const char *start = obj_file + pos;
        const char *end = obj_file + file_size;
        const char *newline = static_cast<const char *>(memchr(start, '\n', end - start));
        if (newline == nullptr)
        {
            line_end = file_size;
        }
        else
        {
            line_end = newline - obj_file;
        }

        if ((line_end - pos) >= 2 && obj_file[pos + 1] == ' ')
        {
            if (obj_file[pos] == 'v')
            {
                ++vertex_count;
            }
            else if (obj_file[pos] == 'f')
            {
                ++face_count;
            }
        }

        pos = line_end;
        if (pos < static_cast<size_t>(file_size) && obj_file[pos] == '\r')
        {
            ++pos;
        }
        if (pos < static_cast<size_t>(file_size) && obj_file[pos] == '\n')
        {
            ++pos;
        }
    }
    mesh.vertices.reserve(vertex_count);
    mesh.faces.reserve(face_count);
}

static void parseVertex(Mesh &mesh, std::string_view line)
{
    size_t line_length = line.size();
    Vertex v = {NAN, NAN, NAN};

    const char *ptr = line.data() + 2;
    const char *end = line.data() + line_length;

    while (ptr < end)
    {
        while (ptr < end && is_space(*ptr))
        {
            ++ptr;
        }

        const char *token_start = ptr;
        while (ptr < end && !is_space(*ptr))
        {
            ++ptr;
        }

        size_t len = ptr - token_start;
        if (len > 0)
        {
            bool done = updateVertex(v, token_start, len);
            if (done)
            {
                break;
            }
        }
    }

    mesh.vertices.push_back(v);
}

static bool updateVertex(Vertex &v, const char *start, size_t len)
{
    float coord;
    auto r = fast_float::from_chars(start, start + len, coord);
    if (r.ec != std::errc())
    {
        // WARNING: handle parse error
    }

    bool res = false;
    if (std::isnan(v.x))
    {
        v.x = coord;
    }
    else if (std::isnan(v.y))
    {
        v.y = coord;
    }
    else if (std::isnan(v.z))
    {
        v.z = coord;
        res = true;
    }

    return res;
}

static void parseFace(Mesh &mesh, std::string_view line)
{
    size_t line_length = line.size();
    Face f = {0, 0, 0};

    const char *ptr = line.data() + 2;
    const char *end = line.data() + line_length;

    while (ptr < end)
    {
        while (ptr < end && is_space(*ptr))
        {
            ++ptr;
        }

        const char *token_start = ptr;
        while (ptr < end && !is_space(*ptr) && *ptr != '/')
        {
            ++ptr;
        }

        size_t len = ptr - token_start;
        if (len > 0)
        {
            bool done = updateFace(mesh, f, token_start, len);
            if (done)
            {
                break;
            }
        }

        while (ptr < end && !is_space(*ptr))
        {
            ++ptr;
        }
    }

    mesh.faces.push_back(f);
}

// WARNING: face indices use int width (32 bits)
static bool updateFace(Mesh &mesh, Face &f, const char *start, size_t len)
{
    int index = 0;
    auto r = fast_float::from_chars(start, start + len, index);
    if (r.ec != std::errc())
    {
        // WARNING: handle parse error
    }
    if (index < 0)
    {
        index = static_cast<int>(mesh.vertices.size()) + index + 1;
    }

    bool res = false;
    if (f.a == 0)
    {
        f.a = static_cast<size_t>(index) - 1;
    }
    else if (f.b == 0)
    {
        f.b = static_cast<size_t>(index) - 1;
    }
    else if (f.c == 0)
    {
        f.c = static_cast<size_t>(index) - 1;
        res = true;
    }

    return res;
}

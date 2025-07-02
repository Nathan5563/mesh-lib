#include <iostream>
#include <cctype>
#include <cmath>
#include <charconv>

#include "obj-parser.hpp"
#include "../include/mesh.hpp"

static void parseVertex(Mesh &mesh, std::string &buf, std::string_view line);
static bool updateVertex(Vertex &v, std::string &buf);
static void parseFace(Mesh &mesh, std::string &buf, std::string_view line);
static bool updateFace(Mesh &mesh, Face &f, std::string &buf);

// ---------------------------------------------------------------------------
//   Core API
// ---------------------------------------------------------------------------

void importMeshFromObj(Mesh &mesh, const char *obj_file, off_t file_size)
{
    size_t pos = 0;
    size_t line_end = 0;
    std::string buf;
    while (pos < static_cast<size_t>(file_size))
    {
        line_end = pos;
        while (line_end < static_cast<size_t>(file_size) &&
               obj_file[line_end] != '\n' &&
               obj_file[line_end] != '\r')
        {
            ++line_end;
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
            std::isspace(static_cast<unsigned char>(line[0])))
        {
            continue;
        }
        else if ((line.size() < 2) ||
                 (line[1] != ' ') ||
                 ((line[0] != 'v') && (line[0] != 'f')))
        {
            std::cerr << "Unsupported features detected" << std::endl;
            continue;
        }
        else
        {
            if (line[0] == 'v')
            {
                parseVertex(mesh, buf, line);
            }
            else
            {
                parseFace(mesh, buf, line);
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
        std::cout << "f " << face.a << " " << face.b << " " << face.c << std::endl;
    }
}

// ---------------------------------------------------------------------------
//   Helpers
// ---------------------------------------------------------------------------

static void parseVertex(Mesh &mesh, std::string &buf, std::string_view line)
{
    buf.clear();
    size_t line_length = line.size();

    Vertex v = {NAN, NAN, NAN};
    for (size_t idx = 2; idx < line_length; ++idx)
    {
        if (std::isspace(static_cast<unsigned char>(line[idx])))
        {
            if (!buf.empty())
            {
                bool done = updateVertex(v, buf);
                buf.clear();
                if (done)
                {
                    break;
                }
            }
        }
        else
        {
            buf += line[idx];
        }
    }
    if (!buf.empty())
    {
        updateVertex(v, buf);
    }

    mesh.vertices.push_back(v);
}

static bool updateVertex(Vertex &v, std::string &buf)
{
    float coord;
    auto r = std::from_chars(buf.data(), buf.data() + buf.size(), coord);
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

static void parseFace(Mesh &mesh, std::string &buf, std::string_view line)
{
    buf.clear();
    size_t line_length = line.size();

    Face f = {0, 0, 0};
    for (size_t idx = 2; idx < line_length; ++idx)
    {
        if (std::isspace(static_cast<unsigned char>(line[idx])))
        {
            if (!buf.empty())
            {
                bool done = updateFace(mesh, f, buf);
                buf.clear();
                if (done)
                {
                    break;
                }
            }
        }
        else if (line[idx] == '/')
        {
            while (idx < line_length &&
                   !std::isspace(static_cast<unsigned char>(line[idx])))
            {
                ++idx;
            }
        }
        else
        {
            buf += line[idx];
        }
    }
    if (!buf.empty())
    {
        updateFace(mesh, f, buf);
    }

    mesh.faces.push_back(f);
}

// WARNING: face indices use int width (32 bits)
static bool updateFace(Mesh &mesh, Face &f, std::string &buf)
{
    int index = 0;
    auto r = std::from_chars(buf.data(), buf.data() + buf.size(), index);
    if (r.ec != std::errc())
    {
        // WARNING: handle parse error (e.g., skip or report)
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

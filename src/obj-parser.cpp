#include <iostream>
#include <fstream>
#include <cctype>
#include <cmath>

#include "obj-parser.hpp"
#include "../include/mesh.hpp"

static void parseVertex(Mesh &mesh, std::string &line);
static bool updateVertex(Vertex &v, std::string &buf);
static void parseFace(Mesh &mesh, std::string &line);
static bool updateFace(Mesh &mesh, Face &f, std::string &buf);

// ---------------------------------------------------------------------------
//   Core API
// ---------------------------------------------------------------------------

void importMeshFromObj(Mesh &mesh, std::ifstream &obj_file)
{
    std::string line;
    while (std::getline(obj_file, line))
    {
        // Ignore blank lines, comments, and lines with leading whitespace
        if (line.empty() ||
            line[0] == '#' ||
            std::isspace(static_cast<unsigned char>(line[0])))
        {
            continue;
        }
        // Ignore all features except vertices and faces
        else if ((line[1] != ' ') ||
                 ((line[0] != 'v') && (line[0] != 'f')))
        {
            std::cerr << "Unsupported features detected" << std::endl;
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
    for (const auto& vertex : mesh.vertices) {
        std::cout << "v " << vertex.x << " " << vertex.y << " " << vertex.z << std::endl;
    }

    std::cout << "# Faces:" << std::endl;
    for (const auto& face : mesh.faces) {
        std::cout << "f " << face.a << " " << face.b << " " << face.c << std::endl;
    }
}

// ---------------------------------------------------------------------------
//   Helpers
// ---------------------------------------------------------------------------

static void parseVertex(Mesh &mesh, std::string &line)
{
    size_t line_length = line.size();

    std::string buf;
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
    bool res = false;
    float coord = std::stof(buf);
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

static void parseFace(Mesh &mesh, std::string &line)
{
    size_t line_length = line.size();

    std::string buf;
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
    int index = std::stoi(buf);
    if (index < 0)
    {
        index = static_cast<int>(mesh.vertices.size()) + index + 1;
    }

    bool res = false;
    if (f.a == 0)
    {
        f.a = static_cast<size_t>(index);
    }
    else if (f.b == 0)
    {
        f.b = static_cast<size_t>(index);
    }
    else if (f.c == 0)
    {
        f.c = static_cast<size_t>(index);
        res = true;
    }

    return res;
}

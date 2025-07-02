#include <iostream>
#include <fstream>
#include <cctype>
#include <cmath>

#include "obj-parser.hpp"
#include "../include/mesh.hpp"

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

void exportMeshToObj(const Mesh &mesh, const std::ofstream &obj_file)
{
}

// ---------------------------------------------------------------------------
//   Helpers
// ---------------------------------------------------------------------------

static void parseVertex(Mesh &mesh, std::string line)
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
                if (std::isnan(v.x))
                {
                    v.x = std::stof(buf);
                }
                else if (std::isnan(v.y))
                {
                    v.y = std::stof(buf);
                }
                else if (std::isnan(v.z))
                {
                    v.z = std::stof(buf);
                }
                else
                {
                    break;
                }
                buf.clear();
            }
        }
        else
        {
            buf += line[idx];
        }
    }
    if (!buf.empty() && std::isnan(v.z))
    {
        v.z = std::stof(buf);
    }

    mesh.vertices.push_back(v);
}

// TODO
static void parseFace(Mesh &mesh, std::string line)
{
}
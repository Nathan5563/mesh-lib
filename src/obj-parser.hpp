#ifndef __OBJ_PARSER_HPP__
#define __OBJ_PARSER_HPP__

#include "../include/mesh.hpp"

// Import data from the given .obj file into the given mesh object
//
// WARNING: .obj specifications are not closely adhered to.
//
// This parser supports only vertices (v) and faces (f).
// Each line must be one of the following:
//   - A comment starting with '#'
//   - A vertex starting with 'v'
//   - A face starting with 'f'
// Leading whitespace is not allowed.
// Only position information is taken from the faces, i.e.,
//   f 0/2/1 2/0/0 3/1/0
// will only use the vertex indices
//   f 0 2 3
//
// WARNING: this parser assumes the given .obj file is syntactically correct.
void importMeshFromObj(Mesh &mesh, std::ifstream &obj_file);

// Export data from the given mesh object into the given .obj file
void exportMeshToObj(const Mesh &mesh, std::ofstream &obj_file);

#endif // __OBJ_PARSER_HPP__
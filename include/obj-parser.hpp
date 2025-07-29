#ifndef OBJ_PARSER_HPP
#define OBJ_PARSER_HPP

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
//   f 5/2/1 -2/0/0 3/1/0
// will only use the vertex indices
//   f 5 -2 3
//
// WARNING: this parser assumes the given .obj file is syntactically correct.
void importMeshFromObj(Mesh &mesh, const char *obj_file, off_t file_size);

// Uses threads to parallelize parsing, but otherwise identical to the above
void importMeshFromObjParallel(Mesh &mesh, const char *obj_file, off_t file_size);

// Export data from the given mesh object in .obj format to stdout
void exportMeshToObj(const Mesh &mesh, int fd);

#endif // OBJ_PARSER_HPP
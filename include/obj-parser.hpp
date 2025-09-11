#ifndef OBJ_PARSER_HPP
#define OBJ_PARSER_HPP

#include "mesh.hpp"

constexpr size_t MIN_SIZE_FOR_PARALLEL = 1 * 1024 * 1024;

// Import data from the given .obj data into the given mesh object
//
// WARNING: .obj specifications are not perfectly adhered to. This
// parser assumes the given file is syntactically correct.
//
// This parser supports vertices (v), vertex textures (vt), vertex 
// normals (vn), faces (f), and materials (mtllib + usemtl).
void importMeshFromObj(Mesh &mesh, const char *obj_file, size_t file_size);

// Uses threads to parallelize parsing, but otherwise identical to importMeshFromObj
void importMeshFromObjParallel(Mesh &mesh, const char *obj_file, size_t file_size);

// Export data from the given mesh object in .obj format to the file specified
// by the given file descriptor
void exportMeshToObj(const Mesh &mesh, int fd);

#endif // OBJ_PARSER_HPP

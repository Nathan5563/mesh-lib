#include <iostream>
#include <vector>
#include <string>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../include/mesh.hpp"
#include "../include/obj-parser.hpp"

// ---------------------------------------------------------------------------
//   Core API
// ---------------------------------------------------------------------------

void Mesh::clear()
{
    this->vertices.clear();
    this->textures.clear();
    this->normals.clear();

    this->vertex_indices.clear();
    this->texture_indices.clear();
    this->normal_indices.clear();
    this->faces.clear();

    this->materials.clear();
}

void Mesh::fromObj(const std::string &path)
{
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1)
    {
        perror("open");
        return;
    }

    struct stat st;
    if (fstat(fd, &st) == -1)
    {
        perror("fstat");
        close(fd);
        return;
    }

    size_t file_size = static_cast<size_t>(st.st_size);
    if (file_size == 0)
    {
        std::cerr << "File size is zero" << std::endl;
        close(fd);
        return;
    }

    char *obj_file = static_cast<char *>(
        mmap(nullptr, file_size, PROT_READ, MAP_POPULATE | MAP_PRIVATE, fd, 0)
    );
    if (obj_file == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        return;
    }

    close(fd);

    this->clear();

    if (file_size < MIN_SIZE_FOR_PARALLEL)
    {
        std::cout << "File size less than 100M, using sequential parser..." << std::endl;
        importMeshFromObj(*this, obj_file, file_size);
    }
    else
    {
        std::cout << "File size greater than 100M, using parallel parser..." << std::endl;
        importMeshFromObjParallel(*this, obj_file, file_size);
    }    

    munmap(obj_file, file_size);
}

void Mesh::toObj(const std::string &path) const
{
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1)
    {
        perror("open");
        return;
    }

    exportMeshToObj(*this, fd);

    close(fd);
}

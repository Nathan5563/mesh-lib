#include <iostream>
#include <vector>
#include <string>

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../include/mesh.hpp"
#include "obj-parser.hpp"

// ---------------------------------------------------------------------------
//   Core API
// ---------------------------------------------------------------------------

void Mesh::clear()
{
    this->vertices.clear();
    this->faces.clear();
}

void Mesh::fromObj(const std::string &path)
{
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) {
        perror("open");
        return;
    }
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        close(fd);
        return;
    }
    off_t file_size = st.st_size;
    if (file_size == 0) {
        fprintf(stderr, "File size is zero\n");
        close(fd);
        return;
    }
    char* data = static_cast<char*>(mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (data == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return;
    }
    close(fd);

    this->clear();

    importMeshFromObj(*this, data, file_size);

    munmap(data, file_size);
}

void Mesh::toObj() const
{
    exportMeshToObj(*this);
}

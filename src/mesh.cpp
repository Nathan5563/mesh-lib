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
    this->faces.clear();
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
    off_t file_size = st.st_size;
    if (file_size == 0)
    {
        std::cerr << "File size is zero" << std::endl;
        close(fd);
        return;
    }
    char *data = static_cast<char *>(mmap(nullptr, file_size, PROT_READ, MAP_POPULATE | MAP_PRIVATE, fd, 0));
    if (data == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        return;
    }
    close(fd);
    const char *__restrict obj_file = static_cast<const char *>(__builtin_assume_aligned(data, 64));

    this->clear();

    importMeshFromObj(*this, obj_file, file_size);

    munmap(data, file_size);
}

void Mesh::fromObjParallel(const std::string &path)
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
    off_t file_size = st.st_size;
    if (file_size == 0)
    {
        std::cerr << "File size is zero" << std::endl;
        close(fd);
        return;
    }
    char *data = static_cast<char *>(mmap(nullptr, file_size, PROT_READ, MAP_POPULATE | MAP_PRIVATE, fd, 0));
    if (data == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        return;
    }
    close(fd);
    const char *__restrict obj_file = static_cast<const char *>(__builtin_assume_aligned(data, 64));

    this->clear();

    importMeshFromObjParallel(*this, obj_file, file_size);

    munmap(data, file_size);
}

void Mesh::toObj() const
{
    exportMeshToObj(*this);
}

#include "../include/mesh.hpp"

#include <vector>
#include <thread>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "spmc_queue/spmc_queue.hpp"

typedef struct
{
    float u, v;
} vec2f;

typedef struct
{
    float x, y, z;
} vec3f;

typedef struct
{
    std::size_t i, j, k;
} vec3i;

typedef struct
{
    std::vector<vec3f> vertices;
    std::vector<vec2f> textures;
    std::vector<vec3f> normals;
    std::vector<vec3i> face_tape;
    std::vector<std::size_t> face_bounds;
} consumer_store;

typedef struct
{
    std::size_t begin, end;
} range;

typedef struct
{
    std::size_t batch_id;
    std::size_t consumer_id;
    range v, t, n, ft, fb;
} batch_artifact;

typedef struct
{
    std::size_t batch_size;
    std::size_t num_consumers;
    std::size_t queue_capacity;
} mesh_config;

class _MeshImpl
{
public:
    std::vector<consumer_store> mConsumerStores;
    std::vector<batch_artifact> mBatchArtifacts;
    SPMCQueue mQueue;
    mesh_config mConfig;

    _MeshImpl(mesh_config config) : 
        mConfig(config), 
        mQueue(config.queue_capacity) 
    {
        mConsumerStores.resize(config.num_consumers);
    }

    bool importObj(void* obj)
    {
        // create one producer thread and mConfig.num_consumers consumer threads
        return true;
    }

    bool exportObj(void* obj)
    {
        return true;
    }
};

Mesh::Mesh()
{
    mesh_config config;
    config.batch_size = 256 * 1024;
    config.num_consumers = std::thread::hardware_concurrency() - 4;
    config.queue_capacity = 2 * config.num_consumers;

    _impl = std::make_unique<_MeshImpl>(config);
}

bool Mesh::importObj(const char* path)
{
    int fd = open(path, O_RDONLY);
    if (fd == -1)
    {
        return false;
    }

    struct stat st;
    if (fstat(fd, &st) == -1)
    {
        close(fd);
        return false;
    }
    std::size_t file_size = static_cast<std::size_t>(st.st_size);
    if (file_size == 0)
    {
        close(fd);
        return false;
    }

    std::size_t num_batches =
        (file_size + _impl->mConfig.batch_size - 1) / _impl->mConfig.batch_size;
    _impl->mBatchArtifacts.resize(num_batches);

    void* obj = mmap(
        nullptr, 
        file_size, 
        PROT_READ, 
        MAP_PRIVATE, 
        fd, 
        0
    );
    close(fd);
    if (obj == MAP_FAILED)
    {
        return false;
    }

    if (!_impl->importObj(obj))
    {
        return false;
    }

    int r = munmap(obj, file_size);
    if (r != 0)
    {
        return false;
    }

    return true;
}

bool Mesh::exportObj(const char* path) const
{
    return true;
}

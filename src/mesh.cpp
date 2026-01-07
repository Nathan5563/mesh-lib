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

void producerWork(
    SPMCQueue& queue,
    const void* obj,
    std::size_t file_size,
    const mesh_config& config
) {
    // while (/* more_work */)
    // {
    //     while (!queue.try_push(/* batch */))
    //     {
    //         std::this_thread::yield();
    //     }
    // }
    // queue.close();
}

void consumerWork(
    SPMCQueue& queue,
    consumer_store& store,
    std::size_t consumer_id
) {
    // for (;;)
    // {
    //     void* batch;
    //     if (queue.try_pop(batch))
    //     {
    //         // process batch
    //         continue;
    //     }

    //     if (queue.is_closed() && queue.is_empty())
    //     {
    //         break;
    //     }

    //     std::this_thread::yield();
    // }
}


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

    bool importObj(void* obj, std::size_t file_size)
    {
        const std::size_t num_consumers = mConfig.num_consumers;
        std::vector<std::thread> consumers;
        consumers.reserve(num_consumers);
        for (std::size_t i = 0; i < num_consumers; ++i)
        {
            consumers.emplace_back(
                [this, i]()
                {
                    consumerWork(
                        mQueue,
                        mConsumerStores[i],
                        i
                    );
                }
            );
        }

        std::thread producer;
        producer = std::thread(
            [this, obj, file_size]()
            {
                // NOTE: file_size and batching logic belong here
                producerWork(
                    mQueue,
                    obj,
                    file_size,
                    mConfig
                );
            }
        );

        producer.join();
        for (auto& t : consumers)
        {
            t.join();
        }

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
    config.num_consumers = std::thread::hardware_concurrency() > 4 ? 
        std::thread::hardware_concurrency() - 4 : 2;
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

    if (!_impl->importObj(obj, file_size))
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

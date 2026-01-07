#ifndef SPMC_QUEUE_HPP
#define SPMC_QUEUE_HPP

// Single-producer, multiple-consumer queue data structure

#include <cstddef>
#include <memory>

class _spmcQueueImpl;

class SPMCQueue
{
public:
    explicit SPMCQueue(std::size_t capacity);

    // Producer side (single thread)
    // Blocks if full. Returns false if closed.
    bool push(void* item);

    // Consumer side (multiple threads)
    // Blocks if empty. Returns false if closed and empty.
    bool pop(void*& item);

    // Signal no more items will be pushed.
    void close();

    // Non-copyable
    SPMCQueue(const SPMCQueue&) = delete;
    SPMCQueue& operator=(const SPMCQueue&) = delete;

private:
    std::unique_ptr<_spmcQueueImpl> _impl;
};

#endif // spmc_queue.hpp

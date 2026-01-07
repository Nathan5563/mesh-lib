#ifndef SPMC_QUEUE_HPP
#define SPMC_QUEUE_HPP

#include <cstddef>
#include <memory>

class _SPMCQueueImpl;

class SPMCQueue {
public:
    explicit SPMCQueue(std::size_t capacity);

    // Non-blocking. Returns false if full or closed.
    bool try_push(void* item);

    // Non-blocking. Returns false if empty.
    bool try_pop(void*& item);

    // Signals no more data will be pushed.
    void close();

    // Checks if no more data will be pushed.
    bool is_closed() const;

    // Checks if the queue is empty.
    bool is_empty() const;

    SPMCQueue(const SPMCQueue&) = delete;
    SPMCQueue& operator=(const SPMCQueue&) = delete;

private:
    std::unique_ptr<_SPMCQueueImpl> _impl;
};

#endif

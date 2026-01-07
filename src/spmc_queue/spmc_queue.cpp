#include "spmc_queue.hpp"

class _SPMCQueueImpl
{
public:
    explicit _SPMCQueueImpl(std::size_t capacity)
        : capacity(capacity), closed(false) {}

private:
    std::size_t capacity;
    bool closed;
};

SPMCQueue::SPMCQueue(std::size_t capacity) : 
    _impl(std::make_unique<_SPMCQueueImpl>(capacity)) 
{}

#include "spmc_queue.hpp"

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

#include <atomic>
#include <new>
#include <stdexcept>

static inline std::size_t next_pow2(std::size_t x)
{
    if (x == 0)
    {
        throw std::invalid_argument("capacity must be > 0");
    }
    if (x < 2)
    {
        return 2;
    }
    --x;
    for (std::size_t bit = 1; bit < sizeof(std::size_t) * 8; bit <<= 1)
    {
        x |= x >> bit;
    }
    return x + 1;
}

class _SPMCQueueImpl
{
public:
    struct Slot
    {
        std::atomic<std::size_t> sequence;
        void* data;
    };

    const std::size_t mCapacity;
    const std::size_t mIndexMask;

    // Consumers claim from head
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> mReadIndex{0};

    // Producer publishes to tail
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> mWriteIndex{0};

    std::atomic<bool> mClosed{false};

    Slot* mSlots;

    explicit _SPMCQueueImpl(std::size_t capacity)
        : mCapacity(next_pow2(capacity)),
          mIndexMask(mCapacity - 1),
          mSlots(static_cast<Slot*>(::operator new(sizeof(Slot) * mCapacity)))
    {
        for (std::size_t slot_index = 0; slot_index < mCapacity; ++slot_index)
        {
            new (&mSlots[slot_index]) Slot{
                std::atomic<std::size_t>(slot_index),
                nullptr
            };
        }
    }

    ~_SPMCQueueImpl() {
        for (std::size_t slot_index = 0; slot_index < mCapacity; ++slot_index)
        {
            mSlots[slot_index].~Slot();
        }
        ::operator delete(mSlots);
    }
};

SPMCQueue::SPMCQueue(std::size_t capacity) :
    _impl(std::make_unique<_SPMCQueueImpl>(capacity))
{}

bool SPMCQueue::try_push(void* item)
{
    auto& queue = *_impl;

    if (queue.mClosed.load(std::memory_order_acquire))
    {
        return false;
    }

    const std::size_t producer_index =
        queue.mWriteIndex.load(std::memory_order_relaxed);

    auto& slot = queue.mSlots[producer_index & queue.mIndexMask];

    const std::size_t slot_sequence =
        slot.sequence.load(std::memory_order_acquire);

    if (slot_sequence != producer_index)
    {
        return false; // queue is full
    }

    slot.data = item;
    slot.sequence.store(producer_index + 1, std::memory_order_release);
    queue.mWriteIndex.store(producer_index + 1, std::memory_order_relaxed);
    return true;
}

bool SPMCQueue::try_pop(void*& item)
{
    auto& queue = *_impl;

    std::size_t consumer_index =
        queue.mReadIndex.load(std::memory_order_relaxed);

    for (;;)
    {
        auto& slot = queue.mSlots[consumer_index & queue.mIndexMask];

        const std::size_t slot_sequence =
            slot.sequence.load(std::memory_order_acquire);

        if (slot_sequence == consumer_index + 1)
        {
            if (queue.mReadIndex.compare_exchange_weak(
                    consumer_index,
                    consumer_index + 1,
                    std::memory_order_relaxed,
                    std::memory_order_relaxed))
            {
                item = slot.data;
                slot.sequence.store(
                    consumer_index + queue.mCapacity,
                    std::memory_order_release);
                return true;
            }
            continue;
        }

        if (slot_sequence < consumer_index + 1)
        {
            return false; // queue is empty
        }

        consumer_index =
            queue.mReadIndex.load(std::memory_order_relaxed);
    }
}

void SPMCQueue::close()
{
    _impl->mClosed.store(true, std::memory_order_release);
}

bool SPMCQueue::is_closed() const
{
    return _impl->mClosed.load(std::memory_order_acquire);
}

bool SPMCQueue::is_empty() const
{
    return _impl->mReadIndex.load(std::memory_order_acquire) ==
           _impl->mWriteIndex.load(std::memory_order_acquire);
}

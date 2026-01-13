#ifndef SPMC_QUEUE_HPP
#define SPMC_QUEUE_HPP

#include <atomic>
#include <stdexcept>

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

static inline std::size_t spmc_next_pow2(std::size_t x)
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

template <class T>
class alignas(CACHE_LINE_SIZE) SPMCQueue
{
    static_assert(std::is_trivially_copyable_v<T>,
        "SPMCQueue<T> requires T to be trivially copyable");

public:
    explicit SPMCQueue(std::size_t capacity)
        : mCapacity(spmc_next_pow2(capacity)),
          mIndexMask(mCapacity - 1),
          mSlots(static_cast<Slot*>(::operator new(sizeof(Slot) * mCapacity)))
    {
        for (std::size_t i = 0; i < mCapacity; ++i)
        {
            new (&mSlots[i]) Slot{};
            mSlots[i].sequence.store(i, std::memory_order_relaxed);
        }
        mReadIndex.store(0, std::memory_order_relaxed);
        mWriteIndex.store(0, std::memory_order_relaxed);
        mClosed.store(false, std::memory_order_relaxed);
    }

    ~SPMCQueue()
    {
        for (std::size_t i = 0; i < mCapacity; ++i)
        {
            mSlots[i].~Slot();
        }
        ::operator delete(mSlots);
    }

    SPMCQueue(const SPMCQueue&) = delete;
    SPMCQueue& operator=(const SPMCQueue&) = delete;

    // Non-blocking. Returns false if full or closed.
    bool try_push(const T& item)
    {
        if (mClosed.load(std::memory_order_acquire))
        {
            return false;
        }

        const std::size_t producer_index = mWriteIndex.load(std::memory_order_relaxed);
        Slot& slot = mSlots[producer_index & mIndexMask];

        const std::size_t seq = slot.sequence.load(std::memory_order_acquire);
        if (seq != producer_index)
        {
            return false; // full
        }

        slot.data = item;
        slot.sequence.store(producer_index + 1, std::memory_order_release);
        mWriteIndex.store(producer_index + 1, std::memory_order_release);
        return true;
    }

    // Non-blocking. Returns false if empty.
    bool try_pop(T& out)
    {
        std::size_t consumer_index = mReadIndex.load(std::memory_order_relaxed);

        for (;;)
        {
            Slot& slot = mSlots[consumer_index & mIndexMask];
            const std::size_t seq = slot.sequence.load(std::memory_order_acquire);

            if (seq == consumer_index + 1)
            {
                if (mReadIndex.compare_exchange_weak(
                        consumer_index,
                        consumer_index + 1,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed))
                {
                    out = slot.data;
                    slot.sequence.store(consumer_index + mCapacity, std::memory_order_release);
                    return true;
                }
                continue;
            }

            if (seq < consumer_index + 1)
            {
                return false; // empty
            }

            consumer_index = mReadIndex.load(std::memory_order_relaxed);
        }
    }

    std::size_t capacity() const
    {
        return mCapacity;
    }

private:
    struct alignas(CACHE_LINE_SIZE) Slot
    {
        std::atomic<std::size_t> sequence;
        T data;
    };

    const std::size_t mCapacity;
    const std::size_t mIndexMask;

    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> mReadIndex{0};
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> mWriteIndex{0};

    std::atomic<bool> mClosed{false};

    alignas(CACHE_LINE_SIZE) Slot* mSlots;
};

#endif // SPMC_QUEUE_HPP

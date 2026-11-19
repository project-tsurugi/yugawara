#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <shared_mutex>
#include <thread>

namespace yugawara::util {

/**
 * @brief a reader/writer mutex which splits its shared locks into multiple shards.
 * @details Each thread acquires shared locks only on its own shard, so that readers on different shards
 *      never write to the same cache line.
 *      Exclusive locks must acquire all shards, so that they become slower than ones of std::shared_mutex.
 *      This is designed for data which are rarely modified but frequently read from many threads.
 *
 *      Each thread is assigned a sequential slot number on its first use, and uses the shard
 *      of (slot number) mod (the number of shards).
 *      The number of shards is a prime number by default, to avoid that periodic patterns of thread
 *      creation are concentrated to the some shards.
 * @note This satisfies *Lockable* and *SharedLockable* as well as std::shared_mutex,
 *      but shared locks must be released on the thread which acquired them.
 * @note Re-entering shared locks on the same thread has the same semantics as std::shared_mutex,
 *      because the thread always uses the same shard.
 */
class sharded_shared_mutex {
public:
    /// @brief the size of individual shards, which is the destructive interference size on major CPUs.
    static constexpr std::size_t shard_alignment = 128;

    /**
     * @brief creates a new instance with the default number of shards.
     * @details The default number of shards is the smallest prime number which is
     *      greater than or equal to the number of hardware threads.
     */
    sharded_shared_mutex() :
        sharded_shared_mutex(default_shard_count())
    {}

    /**
     * @brief creates a new instance.
     * @param shard_count the number of shards, must be greater than 0
     */
    explicit sharded_shared_mutex(std::size_t shard_count) :
        shard_count_ { shard_count == 0 ? 1 : shard_count },
        shards_ { std::make_unique<shard[]>(shard_count_) } // NOLINT(*-avoid-c-arrays)
    {}

    ~sharded_shared_mutex() = default;

    sharded_shared_mutex(sharded_shared_mutex const&) = delete;
    sharded_shared_mutex& operator=(sharded_shared_mutex const&) = delete;
    sharded_shared_mutex(sharded_shared_mutex&&) = delete;
    sharded_shared_mutex& operator=(sharded_shared_mutex&&) = delete;

    /**
     * @brief acquires the exclusive lock.
     * @details This acquires all shards in order.
     */
    void lock() {
        for (std::size_t i = 0; i < shard_count_; ++i) {
            shards_[i].mutex.lock();
        }
    }

    /**
     * @brief tries to acquire the exclusive lock.
     * @return true if successfully acquired
     * @return false otherwise
     */
    bool try_lock() {
        for (std::size_t i = 0; i < shard_count_; ++i) {
            if (!shards_[i].mutex.try_lock()) {
                for (std::size_t j = i; j > 0; --j) {
                    shards_[j - 1].mutex.unlock();
                }
                return false;
            }
        }
        return true;
    }

    /**
     * @brief releases the exclusive lock.
     */
    void unlock() {
        for (std::size_t i = shard_count_; i > 0; --i) {
            shards_[i - 1].mutex.unlock();
        }
    }

    /**
     * @brief acquires a shared lock on the shard of the current thread.
     */
    void lock_shared() {
        current_shard().lock_shared();
    }

    /**
     * @brief tries to acquire a shared lock on the shard of the current thread.
     * @return true if successfully acquired
     * @return false otherwise
     */
    bool try_lock_shared() {
        return current_shard().try_lock_shared();
    }

    /**
     * @brief releases the shared lock on the shard of the current thread.
     */
    void unlock_shared() {
        current_shard().unlock_shared();
    }

    /**
     * @brief returns the number of shards.
     * @return the number of shards
     */
    [[nodiscard]] std::size_t shard_count() const noexcept {
        return shard_count_;
    }

    /**
     * @brief returns the shard index of the current thread.
     * @return the shard index
     */
    [[nodiscard]] std::size_t current_shard_index() const noexcept {
        return current_slot() % shard_count_;
    }

    /**
     * @brief returns the default number of shards.
     * @return the smallest prime number which is greater than or equal to the number of hardware threads
     */
    [[nodiscard]] static std::size_t default_shard_count() noexcept {
        std::size_t n = std::thread::hardware_concurrency();
        n = n < 2 ? 2 : n;
        while (!is_prime(n)) {
            ++n;
        }
        return n;
    }

private:
    struct alignas(shard_alignment) shard {
        std::shared_mutex mutex {};
    };

    std::size_t shard_count_;
    std::unique_ptr<shard[]> shards_; // NOLINT(*-avoid-c-arrays)

    [[nodiscard]] std::shared_mutex& current_shard() noexcept {
        return shards_[current_shard_index()].mutex;
    }

    [[nodiscard]] static std::size_t current_slot() noexcept {
        static std::atomic<std::size_t> next_slot { 0 };
        thread_local std::size_t const slot = next_slot.fetch_add(1, std::memory_order_relaxed);
        return slot;
    }

    [[nodiscard]] static constexpr bool is_prime(std::size_t n) noexcept {
        if (n < 2) {
            return false;
        }
        for (std::size_t i = 2; i * i <= n; ++i) {
            if (n % i == 0) {
                return false;
            }
        }
        return true;
    }
};

} // namespace yugawara::util

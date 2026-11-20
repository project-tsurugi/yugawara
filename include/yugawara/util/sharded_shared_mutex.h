#pragma once

#include <cstddef>
#include <memory>
#include <shared_mutex>

namespace yugawara::util {

/**
 * @brief a reader/writer mutex which splits its shared locks into multiple shards.
 * @details Each thread acquires shared locks only on its assigned shard, so that readers on different shards
 *      never write to the same cache line.
 *      Exclusive locks must acquire all shards, so that they become slower than ones of std::shared_mutex.
 *      This is designed for data which are rarely modified but frequently read from many threads.
 *
 *      Each thread is assigned a sequential slot number on its first use of any sharded_shared_mutex,
 *      and uses the shard of (slot number) mod (the number of shards).
 * @note This satisfies *Lockable* and *SharedLockable* as well as std::shared_mutex,
 *      but shared locks must be released on the thread which acquired them.
 * @note As well as std::shared_mutex, the standard does not allow re-entering shared locks on the same thread.
 *      This class does not make it worse, because the thread always uses the same shard.
 * @attention While a thread is acquiring the exclusive lock, it holds a part of the shards,
 *      and then the threads on those shards cannot acquire shared locks.
 *      Therefore, a thread holding a shared lock must not wait for other threads,
 *      which may directly or indirectly acquire a shared lock of the same mutex.
 * @attention This does not guarantee fairness for writers.
 *      If readers continuously hold shared locks, a thread acquiring the exclusive lock may wait for a long time,
 *      and also the readers on the shards already acquired by the thread are blocked in the meantime.
 */
class sharded_shared_mutex {
public:
    /// @brief the default number of shards.
    static constexpr std::size_t default_shard_count = 16;

    /**
     * @brief the alignment of individual shards.
     * @details This is a conservative value of the destructive interference size,
     *      which also covers the adjacent cache line prefetch on major CPUs.
     */
    static constexpr std::size_t shard_alignment = 128;

    /**
     * @brief creates a new instance with the default number of shards.
     * @throws std::bad_alloc if failed to allocate the shards
     * @see default_shard_count
     */
    sharded_shared_mutex();

    /**
     * @brief creates a new instance.
     * @param shard_count the number of shards, must be greater than 0
     * @throws std::invalid_argument if the number of shards is 0
     * @throws std::bad_alloc if failed to allocate the shards
     */
    explicit sharded_shared_mutex(std::size_t shard_count);

    ~sharded_shared_mutex() = default;

    sharded_shared_mutex(sharded_shared_mutex const&) = delete;
    sharded_shared_mutex& operator=(sharded_shared_mutex const&) = delete;
    sharded_shared_mutex(sharded_shared_mutex&&) = delete;
    sharded_shared_mutex& operator=(sharded_shared_mutex&&) = delete;

    /**
     * @brief acquires the exclusive lock.
     * @details This acquires all shards in order.
     *      If this fails, this releases the shards acquired so far.
     * @throws std::system_error if failed to acquire the lock
     */
    void lock();

    /**
     * @brief tries to acquire the exclusive lock.
     * @details If this fails, this releases the shards acquired so far.
     * @return true if successfully acquired
     * @return false otherwise
     */
    bool try_lock();

    /**
     * @brief releases the exclusive lock.
     */
    void unlock();

    /**
     * @brief acquires a shared lock on the shard of the current thread.
     * @throws std::system_error if failed to acquire the lock
     */
    void lock_shared();

    /**
     * @brief tries to acquire a shared lock on the shard of the current thread.
     * @return true if successfully acquired
     * @return false otherwise
     */
    bool try_lock_shared();

    /**
     * @brief releases the shared lock on the shard of the current thread.
     */
    void unlock_shared();

    /**
     * @brief returns the number of shards.
     * @return the number of shards
     */
    [[nodiscard]] std::size_t shard_count() const noexcept;

    /**
     * @brief returns the shard index of the current thread.
     * @return the shard index, which is less than shard_count()
     * @note This is mainly for testing and diagnostics.
     */
    [[nodiscard]] std::size_t current_shard_index() const noexcept;

private:
    struct alignas(shard_alignment) shard {
        std::shared_mutex mutex {};
    };

    std::size_t shard_count_;
    std::unique_ptr<shard[]> shards_; // NOLINT(*-avoid-c-arrays)

    [[nodiscard]] std::shared_mutex& current_shard() noexcept;
};

} // namespace yugawara::util

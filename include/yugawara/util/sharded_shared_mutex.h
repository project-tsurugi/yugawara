#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <shared_mutex>

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
 * @attention PoC: this class is a proof-of-concept for measuring the effect of sharding the reader locks
 *      of storage::configurable_provider. The production code will be re-designed from scratch.
 */
class sharded_shared_mutex {
public:
    /// @brief the size of individual shards, which is the destructive interference size on major CPUs.
    static constexpr std::size_t shard_alignment = 128;

    /**
     * @brief the environment variable name to override the default number of shards.
     * @attention PoC: this environment variable is only for measuring the proof-of-concept code,
     *      and will be removed (or re-designed) in the production code. Do not depend on it.
     */
    static constexpr char const* shard_count_environment_variable = "YUGAWARA_SHARD_COUNT";

    /// @brief the maximum number of shards which can be specified via the environment variable (PoC).
    static constexpr std::size_t max_shard_count_from_environment = 65'536;

    /**
     * @brief creates a new instance with the default number of shards.
     * @see default_shard_count()
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
     * @details This is the smallest prime number which is greater than or equal to the number of
     *      hardware threads.
     *      PoC: If the environment variable YUGAWARA_SHARD_COUNT is a valid number
     *      (from 1 to max_shard_count_from_environment), this returns it instead.
     *      The value is computed only once in the process, and is reported to the log.
     * @return the default number of shards
     */
    [[nodiscard]] static std::size_t default_shard_count() noexcept;

    /**
     * @brief computes the default number of shards (PoC).
     * @param environment_value the value of the environment variable YUGAWARA_SHARD_COUNT, or nullptr if it is not set
     * @param hardware_threads the number of hardware threads
     * @return the specified number if environment_value is valid
     * @return the smallest prime number which is greater than or equal to hardware_threads (at least 2) otherwise
     */
    [[nodiscard]] static std::size_t compute_default_shard_count(
            char const* environment_value,
            std::size_t hardware_threads) noexcept;

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
};

} // namespace yugawara::util

#include <yugawara/util/sharded_shared_mutex.h>

#include <atomic>
#include <stdexcept>

#include <takatori/util/exception.h>

namespace yugawara::util {

namespace {

// NOTE: this is defined only in libyugawara, so that all threads share the same sequence of slot numbers
std::size_t current_slot() noexcept {
    static std::atomic<std::size_t> next_slot { 0 };
    thread_local std::size_t const slot = next_slot.fetch_add(1, std::memory_order_relaxed);
    return slot;
}

std::size_t validate_shard_count(std::size_t shard_count) {
    if (shard_count == 0) {
        using ::takatori::util::throw_exception;
        throw_exception(std::invalid_argument("shard count must be greater than 0"));
    }
    return shard_count;
}

} // namespace

sharded_shared_mutex::sharded_shared_mutex() :
    sharded_shared_mutex { default_shard_count }
{}

sharded_shared_mutex::sharded_shared_mutex(std::size_t shard_count) :
    shard_count_ { validate_shard_count(shard_count) },
    shards_ { std::make_unique<shard[]>(shard_count_) } // NOLINT(*-avoid-c-arrays)
{}

void sharded_shared_mutex::lock() {
    std::size_t i = 0;
    try {
        for (; i < shard_count_; ++i) {
            shards_[i].mutex.lock();
        }
    } catch (...) {
        // release the shards acquired so far
        for (std::size_t j = i; j > 0; --j) {
            shards_[j - 1].mutex.unlock();
        }
        throw;
    }
}

bool sharded_shared_mutex::try_lock() {
    for (std::size_t i = 0; i < shard_count_; ++i) {
        if (!shards_[i].mutex.try_lock()) {
            // release the shards acquired so far
            for (std::size_t j = i; j > 0; --j) {
                shards_[j - 1].mutex.unlock();
            }
            return false;
        }
    }
    return true;
}

void sharded_shared_mutex::unlock() {
    for (std::size_t i = shard_count_; i > 0; --i) {
        shards_[i - 1].mutex.unlock();
    }
}

void sharded_shared_mutex::lock_shared() {
    current_shard().lock_shared();
}

bool sharded_shared_mutex::try_lock_shared() {
    return current_shard().try_lock_shared();
}

void sharded_shared_mutex::unlock_shared() {
    current_shard().unlock_shared();
}

std::size_t sharded_shared_mutex::shard_count() const noexcept {
    return shard_count_;
}

std::size_t sharded_shared_mutex::current_shard_index() const noexcept {
    return current_slot() % shard_count_;
}

std::shared_mutex& sharded_shared_mutex::current_shard() noexcept {
    return shards_[current_shard_index()].mutex;
}

} // namespace yugawara::util

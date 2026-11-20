#include <yugawara/util/sharded_shared_mutex.h>

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include <yugawara/util/maybe_shared_lock.h>

namespace yugawara::util {

class sharded_shared_mutex_test : public ::testing::Test {};

namespace {

template<class F>
auto on_other_thread(F&& f) {
    decltype(f()) result {};
    std::thread th { [&] { result = std::forward<F>(f)(); } };
    th.join();
    return result;
}

bool try_lock_shared_once(sharded_shared_mutex& m) {
    bool locked = m.try_lock_shared();
    if (locked) {
        m.unlock_shared();
    }
    return locked;
}

bool try_lock_once(sharded_shared_mutex& m) {
    bool locked = m.try_lock();
    if (locked) {
        m.unlock();
    }
    return locked;
}

// the number of threads to cover all shards, because each new thread takes the next slot
constexpr std::size_t shard_count = 7;
constexpr std::size_t thread_count = shard_count * 2;

} // namespace

TEST_F(sharded_shared_mutex_test, maybe_shared_lock) {
    static_assert(std::is_same_v<maybe_shared_lock<sharded_shared_mutex>, std::shared_lock<sharded_shared_mutex>>);
}

TEST_F(sharded_shared_mutex_test, default_shard_count) {
    sharded_shared_mutex m {};
    EXPECT_EQ(m.shard_count(), sharded_shared_mutex::default_shard_count);
}

TEST_F(sharded_shared_mutex_test, shard_count) {
    sharded_shared_mutex m { shard_count };
    EXPECT_EQ(m.shard_count(), shard_count);
    EXPECT_LT(m.current_shard_index(), shard_count);
}

TEST_F(sharded_shared_mutex_test, shard_count_zero) {
    EXPECT_THROW(sharded_shared_mutex { 0 }, std::invalid_argument);
}

TEST_F(sharded_shared_mutex_test, same_thread_same_shard) {
    sharded_shared_mutex m { shard_count };
    auto index = m.current_shard_index();
    EXPECT_EQ(m.current_shard_index(), index);

    // also the same for other mutexes with the same number of shards
    sharded_shared_mutex other { shard_count };
    EXPECT_EQ(other.current_shard_index(), index);
}

TEST_F(sharded_shared_mutex_test, exclusive_excludes_shared) {
    sharded_shared_mutex m { shard_count };
    m.lock();
    for (std::size_t i = 0; i < thread_count; ++i) {
        EXPECT_FALSE(on_other_thread([&] { return try_lock_shared_once(m); })) << i;
    }
    EXPECT_FALSE(on_other_thread([&] { return try_lock_once(m); }));
    m.unlock();

    for (std::size_t i = 0; i < thread_count; ++i) {
        EXPECT_TRUE(on_other_thread([&] { return try_lock_shared_once(m); })) << i;
    }
}

TEST_F(sharded_shared_mutex_test, shared_excludes_exclusive) {
    sharded_shared_mutex m { shard_count };
    m.lock_shared();
    for (std::size_t i = 0; i < thread_count; ++i) {
        EXPECT_TRUE(on_other_thread([&] { return try_lock_shared_once(m); })) << i;
    }
    EXPECT_FALSE(on_other_thread([&] { return try_lock_once(m); }));
    m.unlock_shared();

    EXPECT_TRUE(on_other_thread([&] { return try_lock_once(m); }));
}

TEST_F(sharded_shared_mutex_test, try_lock_releases_acquired_shards) {
    sharded_shared_mutex m { shard_count };

    // advance the slots, so that the next new thread uses the last shard
    for (std::size_t i = 0; i < thread_count; ++i) {
        if (on_other_thread([&] { return m.current_shard_index(); }) == shard_count - 2) {
            break;
        }
    }

    // hold a shared lock on the last shard, so that try_lock() fails after acquiring the other shards
    std::mutex holder_mutex {};
    std::condition_variable holder_cv {};
    bool held = false;
    bool release = false;
    std::size_t held_index {};
    std::thread holder { [&] {
        m.lock_shared();
        {
            std::unique_lock lock { holder_mutex };
            held_index = m.current_shard_index();
            held = true;
            holder_cv.notify_all();
            holder_cv.wait(lock, [&] { return release; });
        }
        m.unlock_shared();
    } };
    auto release_holder = [&] {
        {
            std::unique_lock lock { holder_mutex };
            release = true;
            holder_cv.notify_all();
        }
        holder.join();
    };
    {
        std::unique_lock lock { holder_mutex };
        holder_cv.wait(lock, [&] { return held; });
    }
    if (held_index != shard_count - 1) {
        // the precondition is broken: the following checks are meaningless
        release_holder();
        FAIL() << "the holder must use the last shard: " << held_index;
    }

    EXPECT_FALSE(on_other_thread([&] { return try_lock_once(m); }));

    // the failed try_lock() must not keep any shards
    for (std::size_t i = 0; i < thread_count; ++i) {
        EXPECT_TRUE(on_other_thread([&] { return try_lock_shared_once(m); })) << i;
    }

    release_holder();
    EXPECT_TRUE(on_other_thread([&] { return try_lock_once(m); }));
}

} // namespace yugawara::util

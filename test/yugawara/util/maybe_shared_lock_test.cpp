#include <yugawara/util/maybe_shared_lock.h>

#include <mutex>
#include <shared_mutex>
#include <type_traits>

#include <gtest/gtest.h>

namespace yugawara::util {

class maybe_shared_lock_test : public ::testing::Test {};

namespace {

// a mutex type which supports shared locking, but is not std::shared_mutex
class custom_shared_mutex {
public:
    void lock() { mutex_.lock(); }
    bool try_lock() { return mutex_.try_lock(); }
    void unlock() { mutex_.unlock(); }
    void lock_shared() { mutex_.lock_shared(); }
    bool try_lock_shared() { return mutex_.try_lock_shared(); }
    void unlock_shared() { mutex_.unlock_shared(); }

private:
    std::shared_mutex mutex_ {};
};

} // namespace

TEST_F(maybe_shared_lock_test, shared_mutex) {
    static_assert(std::is_same_v<maybe_shared_lock<std::shared_mutex>, std::shared_lock<std::shared_mutex>>);
}

TEST_F(maybe_shared_lock_test, exclusive_mutex) {
    static_assert(std::is_same_v<maybe_shared_lock<std::mutex>, std::unique_lock<std::mutex>>);
}

TEST_F(maybe_shared_lock_test, custom_shared_mutex) {
    static_assert(std::is_same_v<maybe_shared_lock<custom_shared_mutex>, std::shared_lock<custom_shared_mutex>>);

    custom_shared_mutex mutex {};
    {
        maybe_shared_lock<custom_shared_mutex> first { mutex };
        maybe_shared_lock<custom_shared_mutex> second { mutex };
        EXPECT_TRUE(first.owns_lock());
        EXPECT_TRUE(second.owns_lock());
        EXPECT_FALSE(mutex.try_lock());
    }
    ASSERT_TRUE(mutex.try_lock());
    mutex.unlock();
}

} // namespace yugawara::util

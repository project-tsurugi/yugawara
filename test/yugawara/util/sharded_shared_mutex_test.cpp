#include <yugawara/util/sharded_shared_mutex.h>

#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/util/clonable.h>

#include <yugawara/util/maybe_shared_lock.h>
#include <yugawara/storage/basic_configurable_provider.h>

namespace yugawara::util {

class sharded_shared_mutex_test : public ::testing::Test {};

static_assert(std::is_same_v<maybe_shared_lock<std::shared_mutex>, std::shared_lock<std::shared_mutex>>);
static_assert(std::is_same_v<maybe_shared_lock<std::mutex>, std::unique_lock<std::mutex>>);
static_assert(std::is_same_v<maybe_shared_lock<sharded_shared_mutex>, std::shared_lock<sharded_shared_mutex>>);

namespace {

template<class F>
auto on_other_thread(F&& f) {
    decltype(f()) result {};
    std::thread th { [&] { result = f(); } };
    th.join();
    return result;
}

} // namespace

TEST_F(sharded_shared_mutex_test, default_shard_count) {
    sharded_shared_mutex m {};
    auto n = m.shard_count();
    EXPECT_GE(n, std::thread::hardware_concurrency());
    EXPECT_GE(n, 2);
    for (std::size_t i = 2; i * i <= n; ++i) {
        EXPECT_NE(n % i, 0) << i;
    }
}

TEST_F(sharded_shared_mutex_test, shard_count) {
    sharded_shared_mutex m { 7 };
    EXPECT_EQ(m.shard_count(), 7);
    EXPECT_LT(m.current_shard_index(), 7);

    sharded_shared_mutex z { 0 };
    EXPECT_EQ(z.shard_count(), 1);
}

TEST_F(sharded_shared_mutex_test, shards_differ_between_threads) {
    sharded_shared_mutex m { 7 };
    auto a = on_other_thread([&] { return m.current_shard_index(); });
    auto b = on_other_thread([&] { return m.current_shard_index(); });
    // sequential threads take sequential slots
    EXPECT_NE(a, b);
    // the same thread always uses the same shard
    EXPECT_EQ(m.current_shard_index(), m.current_shard_index());
}

TEST_F(sharded_shared_mutex_test, exclusive_excludes_shared) {
    sharded_shared_mutex m { 7 };
    m.lock();
    for (int i = 0; i < 10; ++i) {
        EXPECT_FALSE(on_other_thread([&] {
            bool locked = m.try_lock_shared();
            if (locked) {
                m.unlock_shared();
            }
            return locked;
        }));
    }
    EXPECT_FALSE(on_other_thread([&] { return m.try_lock(); }));
    m.unlock();

    EXPECT_TRUE(on_other_thread([&] {
        bool locked = m.try_lock_shared();
        if (locked) {
            m.unlock_shared();
        }
        return locked;
    }));
}

TEST_F(sharded_shared_mutex_test, shared_excludes_exclusive) {
    sharded_shared_mutex m { 7 };
    m.lock_shared();
    EXPECT_TRUE(on_other_thread([&] {
        bool locked = m.try_lock_shared();
        if (locked) {
            m.unlock_shared();
        }
        return locked;
    }));
    EXPECT_FALSE(on_other_thread([&] { return m.try_lock(); }));
    m.unlock_shared();

    EXPECT_TRUE(on_other_thread([&] {
        bool locked = m.try_lock();
        if (locked) {
            m.unlock();
        }
        return locked;
    }));
}

TEST_F(sharded_shared_mutex_test, configurable_provider) {
    namespace s = ::yugawara::storage;
    namespace t = ::takatori::type;
    using provider_type = s::basic_configurable_provider<sharded_shared_mutex>;

    auto p = std::make_shared<provider_type>();
    auto tbl = p->add_table(s::table { "T", { { "C1", t::int4() } } });
    std::shared_ptr<s::table const> ctbl = tbl;
    auto primary = p->add_index(s::index { ctbl, "T", { tbl->columns()[0] }, {}, { s::index_feature::primary } });

    constexpr std::size_t nthreads = 8;
    constexpr std::size_t nindices = 50;
    std::atomic<bool> stop { false };
    std::atomic<std::size_t> errors { 0 };
    std::vector<std::thread> readers {};
    for (std::size_t i = 0; i < nthreads; ++i) {
        readers.emplace_back([&] {
            while (!stop.load()) {
                if (p->find_primary_index(*tbl) != primary) {
                    ++errors;
                }
                std::size_t count = 0;
                p->each_index([&](std::string_view, std::shared_ptr<s::index const> const&) {
                    ++count;
                });
                if (count < 1 || count > nindices + 1) {
                    ++errors;
                }
            }
        });
    }
    for (std::size_t i = 0; i < nindices; ++i) {
        auto name = "I" + std::to_string(i);
        p->add_index(s::index { ctbl, std::string_view { name }, { tbl->columns()[0] } });
    }
    stop = true;
    for (auto&& th : readers) {
        th.join();
    }
    EXPECT_EQ(errors.load(), 0);

    std::size_t count = 0;
    p->each_index([&](std::string_view, std::shared_ptr<s::index const> const&) {
        ++count;
    });
    EXPECT_EQ(count, nindices + 1);
}

} // namespace yugawara::util

#include <yugawara/storage/basic_configurable_provider.h>

#include <cstddef>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>

#include <yugawara/storage/configurable_provider.h>
#include <yugawara/util/sharded_shared_mutex.h>

namespace yugawara::storage {

namespace t = ::takatori::type;

namespace {

/*
 * Tests of basic_configurable_provider which depend on the mutex type:
 * the constructor arguments for the mutex, and the locking contract.
 * The tests of the locking contract use mutex types which record their lock operations.
 * They run on a single thread, and only check which locks are acquired and when.
 * The behavior of the provider independent of the mutex type is tested with configurable_provider
 * (see storage_configurable_provider_test).
 */

enum class lock_kind {
    exclusive,
    shared,
};

struct lock_event {
    std::string mutex_name;
    lock_kind kind;
    bool acquire;
};

class lock_log {
public:
    void record(std::string_view mutex_name, lock_kind kind, bool acquire) {
        events_.push_back(lock_event { std::string { mutex_name }, kind, acquire });
    }

    void clear() noexcept {
        events_.clear();
    }

    [[nodiscard]] std::vector<lock_event> const& events() const noexcept {
        return events_;
    }

    // the kinds of locks acquired on the given mutex
    [[nodiscard]] std::set<lock_kind> acquired_kinds(std::string_view mutex_name) const {
        std::set<lock_kind> results {};
        for (auto&& event : events_) {
            if (event.mutex_name == mutex_name && event.acquire) {
                results.emplace(event.kind);
            }
        }
        return results;
    }

    // whether the given mutex is held now
    [[nodiscard]] bool held(std::string_view mutex_name) const {
        std::ptrdiff_t count = 0;
        for (auto&& event : events_) {
            if (event.mutex_name == mutex_name) {
                count += event.acquire ? 1 : -1;
            }
        }
        return count > 0;
    }

    // whether all locks acquired on the given mutex are released
    [[nodiscard]] bool balanced(std::string_view mutex_name) const {
        std::ptrdiff_t count = 0;
        for (auto&& event : events_) {
            if (event.mutex_name == mutex_name) {
                count += event.acquire ? 1 : -1;
                if (count < 0) {
                    return false;
                }
            }
        }
        return count == 0;
    }

    // whether the inner mutex is acquired at least once, and always while the outer mutex is held
    [[nodiscard]] bool acquired_only_inside(std::string_view inner, std::string_view outer) const {
        std::ptrdiff_t outer_count = 0;
        bool found = false;
        for (auto&& event : events_) {
            if (event.mutex_name == outer) {
                outer_count += event.acquire ? 1 : -1;
            } else if (event.mutex_name == inner && event.acquire) {
                if (outer_count <= 0) {
                    return false;
                }
                found = true;
            }
        }
        return found;
    }

private:
    std::vector<lock_event> events_ {};
};

// satisfies BasicLockable, but does not provide shared locks
class recording_mutex {
public:
    recording_mutex(lock_log& log, std::string_view name) :
        log_ { std::addressof(log) },
        name_ { name }
    {}

    void lock() {
        log_->record(name_, lock_kind::exclusive, true);
    }

    void unlock() {
        log_->record(name_, lock_kind::exclusive, false);
    }

protected:
    void lock_shared_impl() {
        log_->record(name_, lock_kind::shared, true);
    }

    void unlock_shared_impl() {
        log_->record(name_, lock_kind::shared, false);
    }

private:
    lock_log* log_;
    std::string name_;
};

// satisfies Lockable and SharedLockable
class recording_shared_mutex : public recording_mutex {
public:
    using recording_mutex::recording_mutex;

    bool try_lock() {
        lock();
        return true;
    }

    void lock_shared() {
        lock_shared_impl();
    }

    bool try_lock_shared() {
        lock_shared();
        return true;
    }

    void unlock_shared() {
        unlock_shared_impl();
    }
};

using exclusive_provider = basic_configurable_provider<recording_mutex>;
using shared_provider = basic_configurable_provider<recording_shared_mutex>;

} // namespace

class storage_basic_configurable_provider_test : public ::testing::Test {
public:
    lock_log log {};

    std::shared_ptr<table const> origin = takatori::util::clone_shared(table {
            "T1",
            {
                    { "C1", t::int4() },
            },
    });

    column const& c1 = origin->columns()[0];

    template<class Provider>
    void fill(Provider& p) {
        p.add_relation(table { "R1", { { "C1", t::int4() } } });
        p.add_index(index { origin, "I1", { origin->columns()[0] }, {}, { index_feature::primary } });
        p.add_sequence(sequence { "S1" });
    }

    // invokes all read operations
    static void read_all(provider const& p, table const& tbl) {
        auto nop = [](std::string_view, auto const&) {};
        (void) p.find_relation("R1");
        (void) p.find_table("R1");
        (void) p.find_index("I1");
        (void) p.find_primary_index(tbl);
        (void) p.find_sequence("S1");
        p.each_relation(nop);
        p.each_index(nop);
        p.each_table_index(tbl, nop);
        p.each_sequence(nop);
    }
};

TEST_F(storage_basic_configurable_provider_test, read_operations_acquire_shared_locks) {
    shared_provider p { {}, log, "P" };
    fill(p);
    log.clear();

    read_all(p, *origin);

    EXPECT_EQ(log.acquired_kinds("P"), (std::set<lock_kind> { lock_kind::shared }));
    EXPECT_TRUE(log.balanced("P"));
}

TEST_F(storage_basic_configurable_provider_test, read_operations_acquire_exclusive_locks_without_shared_locks) {
    exclusive_provider p { {}, log, "P" };
    fill(p);
    log.clear();

    read_all(p, *origin);

    EXPECT_EQ(log.acquired_kinds("P"), (std::set<lock_kind> { lock_kind::exclusive }));
    EXPECT_TRUE(log.balanced("P"));
}

TEST_F(storage_basic_configurable_provider_test, write_operations_acquire_exclusive_locks) {
    shared_provider p { {}, log, "P" };

    p.add_relation(table { "R1", { { "C1", t::int4() } } });
    p.add_relation(takatori::util::clone_shared(table { "R2", { { "C1", t::int4() } } }));
    p.add_table(table { "T2", { { "C1", t::int4() } } });
    p.add_table(takatori::util::clone_shared(table { "T3", { { "C1", t::int4() } } }));
    p.add_index(index { origin, "I1", { origin->columns()[0] } });
    p.add_index(std::make_shared<index>(index { origin, "I2", { origin->columns()[0] } }));
    p.add_sequence(sequence { "S1" });
    p.add_sequence(std::make_shared<sequence>("S2"));
    p.add_relation(table { "R1", { { "C1", t::int4() } } }, true);
    p.remove_relation("R1");
    p.remove_index("I1");
    p.remove_sequence("S1");

    EXPECT_EQ(log.acquired_kinds("P"), (std::set<lock_kind> { lock_kind::exclusive }));
    EXPECT_TRUE(log.balanced("P"));
}

TEST_F(storage_basic_configurable_provider_test, consumers_are_called_while_holding_lock) {
    shared_provider p { {}, log, "P" };
    fill(p);

    std::size_t count = 0;
    auto check = [&](std::string_view, auto const&) {
        EXPECT_TRUE(log.held("P"));
        ++count;
    };
    p.each_relation(check);
    p.each_index(check);
    p.each_table_index(*origin, check);
    p.each_sequence(check);

    EXPECT_EQ(count, 4);
    EXPECT_FALSE(log.held("P"));
}

TEST_F(storage_basic_configurable_provider_test, consumers_are_called_while_holding_lock_for_parent_entries) {
    auto parent = std::make_shared<shared_provider>(nullptr, log, "PARENT");
    fill(*parent);
    shared_provider child { parent, log, "CHILD" };

    std::size_t count = 0;
    auto check = [&](std::string_view, auto const&) {
        EXPECT_TRUE(log.held("CHILD"));
        ++count;
    };
    child.each_relation(check);
    child.each_index(check);
    child.each_table_index(*origin, check);
    child.each_sequence(check);

    EXPECT_EQ(count, 4);
    EXPECT_FALSE(log.held("CHILD"));
}

TEST_F(storage_basic_configurable_provider_test, read_operations_lock_parent_inside_lock) {
    auto parent = std::make_shared<shared_provider>(nullptr, log, "PARENT");
    fill(*parent);
    shared_provider child { parent, log, "CHILD" };

    auto check = [&](auto&& operation) {
        log.clear();
        operation();
        EXPECT_TRUE(log.acquired_only_inside("PARENT", "CHILD"));
        EXPECT_TRUE(log.balanced("PARENT"));
        EXPECT_TRUE(log.balanced("CHILD"));
    };
    auto nop = [](std::string_view, auto const&) {};

    // the entries are only in the parent, so that the child must look up the parent
    check([&] { EXPECT_TRUE(child.find_relation("R1")); });
    check([&] { EXPECT_TRUE(child.find_index("I1")); });
    check([&] { EXPECT_TRUE(child.find_primary_index(*origin)); });
    check([&] { EXPECT_TRUE(child.find_sequence("S1")); });
    check([&] { child.each_relation(nop); });
    check([&] { child.each_index(nop); });
    check([&] { child.each_table_index(*origin, nop); });
    check([&] { child.each_sequence(nop); });
}

TEST_F(storage_basic_configurable_provider_test, write_operations_lock_parent_inside_lock) {
    auto parent = std::make_shared<shared_provider>(nullptr, log, "PARENT");
    fill(*parent);
    shared_provider child { parent, log, "CHILD" };
    log.clear();

    // the parent has the entries of the same names, so that they must be rejected after looking up the parent
    EXPECT_THROW(child.add_relation(table { "R1", { { "C1", t::int4() } } }), std::invalid_argument);
    EXPECT_THROW(child.add_index(index { origin, "I1", { origin->columns()[0] } }), std::invalid_argument);
    EXPECT_THROW(child.add_sequence(sequence { "S1" }), std::invalid_argument);

    EXPECT_TRUE(log.acquired_only_inside("PARENT", "CHILD"));
    EXPECT_EQ(log.acquired_kinds("CHILD"), (std::set<lock_kind> { lock_kind::exclusive }));
    EXPECT_EQ(log.acquired_kinds("PARENT"), (std::set<lock_kind> { lock_kind::shared }));
    EXPECT_TRUE(log.balanced("PARENT"));
    EXPECT_TRUE(log.balanced("CHILD"));
}

TEST_F(storage_basic_configurable_provider_test, find_primary_index_in_parent_with_exclusive_mutex) {
    using std_mutex_provider = basic_configurable_provider<std::mutex>;
    auto parent = std::make_shared<std_mutex_provider>();
    auto primary = parent->add_index({ origin, "PRIMARY_PARENT", { c1 }, {}, { index_feature::primary } });
    std_mutex_provider child { parent };

    EXPECT_EQ(child.find_primary_index(*origin), primary);
}

TEST_F(storage_basic_configurable_provider_test, mutex_arguments) {
    using sharded_mutex_provider = basic_configurable_provider<util::sharded_shared_mutex>;
    auto parent = std::make_shared<sharded_mutex_provider>(nullptr, 4);
    auto primary = parent->add_index({ origin, "PRIMARY_PARENT", { c1 }, {}, { index_feature::primary } });
    sharded_mutex_provider child { parent, 2 };

    EXPECT_EQ(child.find_primary_index(*origin), primary);
    EXPECT_THROW((sharded_mutex_provider { {}, 0 }), std::invalid_argument);
}

TEST_F(storage_basic_configurable_provider_test, mutex_arguments_unavailable) {
    using std_mutex_provider = basic_configurable_provider<std::mutex>;
    static_assert(std::is_constructible_v<std_mutex_provider>);
    static_assert(std::is_constructible_v<std_mutex_provider, std::shared_ptr<configurable_provider>>);
    static_assert(!std::is_constructible_v<std_mutex_provider, std::nullptr_t, std::size_t>);
}

} // namespace yugawara::storage

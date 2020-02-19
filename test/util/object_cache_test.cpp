#include <yugawara/util/object_cache.h>

#include <gtest/gtest.h>

namespace yugawara::util {

class object_cache_test : public ::testing::Test {};

class Obj {
public:
    Obj(int value) noexcept : value_(value) {} // NOLINT

    Obj* clone(takatori::util::object_creator creator) const {
        return creator.create_object<Obj>(value_);
    }

    int value() const noexcept { return value_; }

private:
    int value_ {};
};

struct eq {
    bool operator()(Obj a, Obj b) const noexcept { return a.value() == b.value(); }
};

struct hash {
    std::size_t operator()(Obj obj) const noexcept { return static_cast<std::size_t>(obj.value()); }
};

using obj_cache = object_cache<Obj, hash, eq>;

static_assert(std::is_default_constructible_v<obj_cache>);
static_assert(std::is_move_constructible_v<obj_cache>);

TEST_F(object_cache_test, simple) {
    obj_cache cache;
    EXPECT_FALSE(cache.find(100).lock());

    auto p1 = cache.add(100).lock();
    EXPECT_EQ(p1->value(), 100);

    auto p2 = cache.find(100).lock();
    EXPECT_EQ(p2, p1);

    auto p3 = cache.find(101).lock();
    EXPECT_FALSE(p3);
}

TEST_F(object_cache_test, add_entry) {
    obj_cache cache;

    auto p1 = std::make_shared<Obj>(100);
    EXPECT_FALSE(cache.find(100).lock());

    EXPECT_EQ(cache.add(p1).lock(), p1);
    EXPECT_EQ(cache.find(100).lock(), p1);
    EXPECT_EQ(cache.add(std::make_shared<Obj>(100)).lock(), p1);
}

TEST_F(object_cache_test, clear) {
    obj_cache cache;
    auto p = cache.add(1);
    EXPECT_TRUE(cache.find(1).lock());
    EXPECT_FALSE(p.expired());

    cache.clear();
    EXPECT_FALSE(cache.find(1).lock());
    EXPECT_TRUE(p.expired());
}

} // namespace yugawara::util

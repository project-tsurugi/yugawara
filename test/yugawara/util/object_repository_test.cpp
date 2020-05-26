#include <yugawara/util/object_repository.h>

#include <gtest/gtest.h>

namespace yugawara::util {

class object_repository_test : public ::testing::Test {};

namespace {

class Obj {
public:
    explicit Obj(int value) noexcept : value_(value) {}
    virtual ~Obj() = default;

    virtual Obj* clone(takatori::util::object_creator creator) const {
        return creator.create_object<Obj>(value_);
    }

    int value() const noexcept { return value_; }

private:
    int value_ {};
};

class Sub : public Obj {
public:
    explicit Sub(int value) noexcept : Obj(value) {}
    ~Sub() override = default;

    Sub* clone(takatori::util::object_creator creator) const override {
        return creator.create_object<Sub>(value());
    }
};

struct eq {
    bool operator()(Obj const& a, Obj const& b) const noexcept { return a.value() == b.value(); }
};

struct hash {
    std::size_t operator()(Obj const& obj) const noexcept { return static_cast<std::size_t>(obj.value()); }
};

} // namespace

using obj_cache = object_cache<Obj, hash, eq>;
using obj_repo = object_repository<Obj, hash, eq>;

static_assert(std::is_default_constructible_v<obj_repo>);
static_assert(std::is_move_constructible_v<obj_repo>);

TEST_F(object_repository_test, simple) {
    obj_repo repo;

    auto p1 = repo.get(Obj(100));
    EXPECT_EQ(p1->value(), 100);

    auto p2 = repo.get(Obj(100));
    EXPECT_NE(p2, p1);
    EXPECT_EQ(p2->value(), 100);

    auto p3 = repo.get(Obj(101));
    EXPECT_EQ(p3->value(), 101);
}

TEST_F(object_repository_test, cached) {
    object_repository repo { obj_cache {} };

    auto p1 = repo.get(Obj(100));
    EXPECT_EQ(p1->value(), 100);

    auto p2 = repo.get(Obj(100));
    EXPECT_EQ(p2, p1);

    auto p3 = repo.get(Obj(101));
    EXPECT_EQ(p3->value(), 101);

    repo.clear();
    auto p4 = repo.get(Obj(100));
    EXPECT_NE(p4, p1);
    EXPECT_EQ(p4->value(), 100);
}

TEST_F(object_repository_test, subtype) {
    object_repository repo { obj_cache {} };

    std::shared_ptr<Sub> p1 = repo.get(Sub(100));
    EXPECT_EQ(p1->value(), 100);
}

} // namespace yugawara::util

#include <yugawara/schema/catalog.h>

#include <gtest/gtest.h>

#include <takatori/util/downcast.h>

#include <yugawara/schema/configurable_provider.h>

namespace yugawara::schema {

using ::takatori::util::downcast;

class catalog_test : public ::testing::Test {};

static_assert(!std::is_copy_constructible_v<catalog>);

TEST_F(catalog_test, default) {
    catalog d { 1, "s" };

    EXPECT_TRUE(downcast<schema::configurable_provider>(&d.schema_provider()));
    EXPECT_FALSE(d.shared_schema_provider());
}

TEST_F(catalog_test, simple) {
    auto p = std::make_shared<schema::configurable_provider>();

    catalog d { 1, "s", p};

    EXPECT_EQ(&d.schema_provider(), p.get());
}

TEST_F(catalog_test, output) {
    auto p = std::make_shared<schema::configurable_provider>();

    catalog d { 1, "s", p};

    std::cout << d << std::endl;
}

} // namespace yugawara::schema

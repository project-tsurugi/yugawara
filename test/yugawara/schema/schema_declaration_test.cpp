#include <yugawara/schema/declaration.h>

#include <gtest/gtest.h>

#include <takatori/util/downcast.h>

#include <yugawara/storage/configurable_provider.h>
#include <yugawara/variable/configurable_provider.h>
#include <yugawara/function/configurable_provider.h>
#include <yugawara/aggregate/configurable_provider.h>
#include <yugawara/type/configurable_provider.h>

namespace yugawara::schema {

using ::takatori::util::downcast;

class schema_declaration_test : public ::testing::Test {};

TEST_F(schema_declaration_test, default) {
    declaration d { 1, "s" };

    EXPECT_TRUE(downcast<storage::configurable_provider>(&d.storage_provider()));
    EXPECT_TRUE(downcast<variable::configurable_provider>(&d.variable_provider()));
    EXPECT_TRUE(downcast<function::configurable_provider>(&d.function_provider()));
    EXPECT_TRUE(downcast<aggregate::configurable_provider>(&d.set_function_provider()));
    EXPECT_TRUE(downcast<type::configurable_provider>(&d.type_provider()));

    EXPECT_FALSE(d.shared_storage_provider());
    EXPECT_FALSE(d.shared_variable_provider());
    EXPECT_FALSE(d.shared_function_provider());
    EXPECT_FALSE(d.shared_set_function_provider());
    EXPECT_FALSE(d.shared_type_provider());
}

TEST_F(schema_declaration_test, simple) {
    auto st = std::make_shared<storage::configurable_provider>();
    auto va = std::make_shared<variable::configurable_provider>();
    auto fn = std::make_shared<function::configurable_provider>();
    auto sf = std::make_shared<aggregate::configurable_provider>();
    auto ty = std::make_shared<type::configurable_provider>();

    declaration d { 1, "s", st, va, fn, sf, ty };

    EXPECT_EQ(&d.storage_provider(), st.get());
    EXPECT_EQ(&d.variable_provider(), va.get());
    EXPECT_EQ(&d.function_provider(), fn.get());
    EXPECT_EQ(&d.set_function_provider(), sf.get());
    EXPECT_EQ(&d.type_provider(), ty.get());
}

TEST_F(schema_declaration_test, output) {
    auto st = std::make_shared<storage::configurable_provider>();
    auto va = std::make_shared<variable::configurable_provider>();
    auto fn = std::make_shared<function::configurable_provider>();
    auto sf = std::make_shared<aggregate::configurable_provider>();
    auto ty = std::make_shared<type::configurable_provider>();

    declaration d { 1, "s", st, va, fn, sf, ty };

    std::cout << d << std::endl;
}

} // namespace yugawara::schema

#include <yugawara/binding/external_relation_info.h>

#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/plan/forward.h>

#include <yugawara/binding/factory.h>
#include <yugawara/binding/extract.h>
#include <yugawara/binding/relation_info_kind.h>
#include <yugawara/binding/exchange_info.h>

#include <yugawara/storage/index.h>

namespace yugawara::binding {

namespace descriptor = ::takatori::descriptor;
namespace plan = ::takatori::plan;
namespace t = ::takatori::type;

static_assert(external_relation_info::tag == relation_info_kind::external);
static_assert(descriptor_has_entity_v<descriptor::relation, external_relation_info>);
static_assert(std::is_same_v<
        descriptor_shared_type<descriptor::relation, external_relation_info>,
        std::shared_ptr<external_relation_info const>>);

class external_relation_info_test : public ::testing::Test {
public:
    factory bindings {};

    descriptor::variable c1 = bindings.exchange_column("c1");
    descriptor::variable c2 = bindings.exchange_column("c2");

    static std::size_t hash(descriptor::relation const& desc) {
        return std::hash<descriptor::relation> {}(desc);
    }

    template<class T>
    static std::string print(T const& info) {
        std::ostringstream out {};
        out << info;
        return out.str();
    }
};

TEST_F(external_relation_info_test, simple) {
    external_relation_info info { 1, { c1, c2 } };

    EXPECT_EQ(info.kind(), relation_info_kind::external);
    EXPECT_EQ(info.relation_id(), 1);

    std::vector<descriptor::variable> expected { c1, c2 };
    EXPECT_EQ(info.columns(), expected);

    auto inputs = info.input_columns();
    ASSERT_EQ(inputs.size(), 2);
    EXPECT_EQ(inputs[0], c1);
    EXPECT_EQ(inputs[1], c2);

    auto outputs = info.output_columns();
    ASSERT_EQ(outputs.size(), 2);
    EXPECT_EQ(outputs[0], c1);
    EXPECT_EQ(outputs[1], c2);
}

TEST_F(external_relation_info_test, class_id) {
    external_relation_info info { 1, {} };
    plan::forward exchange {};
    exchange_info other { exchange };

    EXPECT_NE(info.class_id(), other.class_id());
}

TEST_F(external_relation_info_test, columns_modify) {
    external_relation_info info { 1, { c1 } };
    info.columns().emplace_back(c2);

    std::vector<descriptor::variable> expected { c1, c2 };
    EXPECT_EQ(info.columns(), expected);
    EXPECT_EQ(info.input_columns().size(), 2);
}

TEST_F(external_relation_info_test, find_type_absent) {
    external_relation_info info { 1, { c1, c2 } };

    EXPECT_FALSE(info.find_type(c1));
    EXPECT_FALSE(info.find_type_shared(c1));
}

TEST_F(external_relation_info_test, add_type) {
    external_relation_info info { 1, { c1, c2 } };
    auto type = std::make_shared<t::int4>();
    info.add_type(c1, type);

    auto found = info.find_type(c1);
    ASSERT_TRUE(found);
    EXPECT_EQ(*found, t::int4 {});

    auto shared = info.find_type_shared(c1);
    EXPECT_EQ(shared, type);
    EXPECT_EQ(found.get(), shared.get());

    EXPECT_FALSE(info.find_type(c2));
}

TEST_F(external_relation_info_test, add_type_value) {
    external_relation_info info { 1, { c1, c2 } };
    info.add_type(c1, t::int4 {});

    auto found = info.find_type(c1);
    ASSERT_TRUE(found);
    EXPECT_EQ(*found, t::int4 {});
    EXPECT_TRUE(info.find_type_shared(c1));

    // overwrite
    info.add_type(c1, t::int8 {});
    auto overwritten = info.find_type(c1);
    ASSERT_TRUE(overwritten);
    EXPECT_EQ(*overwritten, t::int8 {});
}

TEST_F(external_relation_info_test, add_type_overwrite) {
    external_relation_info info { 1, { c1 } };
    info.add_type(c1, std::make_shared<t::int4>());
    info.add_type(c1, std::make_shared<t::int8>());

    auto found = info.find_type(c1);
    ASSERT_TRUE(found);
    EXPECT_EQ(*found, t::int8 {});
}

TEST_F(external_relation_info_test, add_type_remove) {
    external_relation_info info { 1, { c1 } };
    info.add_type(c1, std::make_shared<t::int4>());
    info.add_type(c1, nullptr);

    EXPECT_FALSE(info.find_type(c1));
    EXPECT_FALSE(info.find_type_shared(c1));
}

TEST_F(external_relation_info_test, add_type_not_in_columns) {
    external_relation_info info { 1, { c1 } };

    auto not_in_columns = bindings.exchange_column("__not_in_columns__");
    info.add_type(not_in_columns, std::make_shared<t::int4>());

    auto found = info.find_type(not_in_columns);
    ASSERT_TRUE(found);
    EXPECT_EQ(*found, t::int4 {});
}

TEST_F(external_relation_info_test, equivalent) {
    external_relation_info a { 1, { c1, c2 } };
    external_relation_info b { 1, { c1, c2 } };
    EXPECT_EQ(a, b);
}

TEST_F(external_relation_info_test, equivalent_ignore_hints) {
    external_relation_info a { 1, { c1, c2 } };
    a.add_type(c1, t::int4 {});
    external_relation_info b { 1, { c1, c2 } };
    EXPECT_EQ(a, b);

    auto da = bindings.external_relation(std::move(a));
    auto db = bindings.external_relation(std::move(b));
    EXPECT_EQ(da, db);
    EXPECT_EQ(hash(da), hash(db));
}

TEST_F(external_relation_info_test, different_relation_id) {
    external_relation_info a { 1, { c1, c2 } };
    external_relation_info b { 2, { c1, c2 } };
    EXPECT_NE(a, b);
}

TEST_F(external_relation_info_test, different_columns_order) {
    external_relation_info a { 1, { c1, c2 } };
    external_relation_info b { 1, { c2, c1 } };
    EXPECT_NE(a, b);
}

TEST_F(external_relation_info_test, different_columns) {
    external_relation_info a { 1, { c1 } };
    external_relation_info b { 1, { c2 } };
    EXPECT_NE(a, b);
}

TEST_F(external_relation_info_test, clone) {
    external_relation_info info { 1, { c1, c2 } };
    info.add_type(c1, t::int4 {});

    std::unique_ptr<external_relation_info> copy { info.clone() };
    EXPECT_EQ(*copy, info);
    EXPECT_EQ(copy->relation_id(), 1);
    auto found = copy->find_type(c1);
    ASSERT_TRUE(found);
    EXPECT_EQ(*found, t::int4 {});
}

TEST_F(external_relation_info_test, clone_move) {
    external_relation_info info { 1, { c1, c2 } };
    info.add_type(c1, t::int4 {});

    std::unique_ptr<external_relation_info> copy { std::move(info).clone() };
    EXPECT_EQ(copy->relation_id(), 1);
    std::vector<descriptor::variable> expected { c1, c2 };
    EXPECT_EQ(copy->columns(), expected);
    auto found = copy->find_type(c1);
    ASSERT_TRUE(found);
    EXPECT_EQ(*found, t::int4 {});
}

TEST_F(external_relation_info_test, descriptor_shared) {
    auto info = std::make_shared<external_relation_info>(1, std::vector { c1, c2 });
    auto desc = bindings.external_relation(info);

    EXPECT_EQ(kind_of(desc), relation_info_kind::external);
    EXPECT_EQ(std::addressof(extract<external_relation_info>(desc)), info.get());

    auto found = extract_if<external_relation_info>(desc);
    ASSERT_TRUE(found);
    EXPECT_EQ(found.get(), info.get());

    auto shared = extract_shared<external_relation_info>(desc);
    EXPECT_EQ(shared, info);
}

TEST_F(external_relation_info_test, descriptor_move) {
    external_relation_info info { 1, { c1, c2 } };
    info.add_type(c1, t::int4 {});
    auto desc = bindings.external_relation(std::move(info));

    auto&& extracted = extract<external_relation_info>(desc);
    EXPECT_EQ(extracted.relation_id(), 1);
    std::vector<descriptor::variable> expected { c1, c2 };
    EXPECT_EQ(extracted.columns(), expected);
    EXPECT_TRUE(extracted.find_type(c1));
}

TEST_F(external_relation_info_test, descriptor_operator) {
    auto shared = bindings(std::make_shared<external_relation_info>(1, std::vector { c1 }));
    auto moved = bindings(external_relation_info { 1, { c1 } });

    EXPECT_EQ(kind_of(shared), relation_info_kind::external);
    EXPECT_EQ(kind_of(moved), relation_info_kind::external);
    EXPECT_EQ(shared, moved);
}

TEST_F(external_relation_info_test, descriptor_mismatch) {
    auto desc = bindings.external_relation(external_relation_info { 1, { c1 } });

    EXPECT_FALSE(extract_if<plan::exchange>(desc));
    EXPECT_FALSE(extract_if<storage::index>(desc));
    EXPECT_FALSE(extract_shared<storage::index>(desc));
    EXPECT_THROW((void) extract<plan::exchange>(desc), std::invalid_argument);
    EXPECT_THROW((void) extract<storage::index>(desc), std::invalid_argument);

    plan::forward exchange {};
    auto other = bindings.exchange(exchange);
    EXPECT_FALSE(extract_if<external_relation_info>(other));
    EXPECT_FALSE(extract_shared<external_relation_info>(other));
    EXPECT_THROW((void) extract<external_relation_info>(other), std::invalid_argument);
}

TEST_F(external_relation_info_test, kind_to_string) {
    EXPECT_EQ(to_string_view(relation_info_kind::external), "external");
}

TEST_F(external_relation_info_test, print_hints) {
    external_relation_info info { 1, { c1, c2 } };
    info.add_type(c1, t::int4 {});

    auto str = print(info);
    EXPECT_NE(str.find("c1"), std::string::npos) << str;
    EXPECT_NE(str.find(print(t::int4 {})), std::string::npos) << str;
}

TEST_F(external_relation_info_test, print_hints_not_in_columns) {
    external_relation_info info { 1, { c1, c2 } };

    auto not_in_columns = bindings.exchange_column("__not_in_columns__");
    info.add_type(not_in_columns, t::int8 {});

    external_relation_info plain { 1, { c1, c2 } };

    // hints of variables outside of columns() are not shown
    auto str = print(info);
    EXPECT_EQ(str, print(plain));
    EXPECT_EQ(str.find("__not_in_columns__"), std::string::npos) << str;
}

} // namespace yugawara::binding

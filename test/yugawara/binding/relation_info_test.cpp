#include <yugawara/binding/relation_info.h>

#include <sstream>
#include <stdexcept>
#include <type_traits>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/plan/forward.h>
#include <takatori/util/clonable.h>
#include <takatori/util/maybe_shared_ptr.h>

#include <yugawara/binding/factory.h>
#include <yugawara/binding/extract.h>
#include <yugawara/binding/relation_info_kind.h>
#include <yugawara/binding/index_info.h>
#include <yugawara/binding/exchange_info.h>
#include <yugawara/binding/external_relation_info.h>

#include <yugawara/storage/table.h>
#include <yugawara/storage/index.h>

namespace yugawara::binding {

namespace descriptor = ::takatori::descriptor;
namespace plan = ::takatori::plan;
namespace t = ::takatori::type;

using ::takatori::util::maybe_shared_ptr;

// which entities can be extracted from relation descriptors
static_assert(descriptor_has_entity_v<descriptor::relation, storage::index>);
static_assert(descriptor_has_entity_v<descriptor::relation, plan::exchange>);
static_assert(!descriptor_has_entity_v<descriptor::relation, storage::table>);
static_assert(std::is_same_v<
        descriptor_shared_type<descriptor::relation, storage::index>,
        maybe_shared_ptr<storage::index const>>);

static_assert(index_info::tag == relation_info_kind::index);
static_assert(exchange_info::tag == relation_info_kind::exchange);

class relation_info_test : public ::testing::Test {
public:
    factory bindings {};

    std::shared_ptr<storage::table const> table = ::takatori::util::clone_shared(storage::table {
            "T1",
            {
                    { "C1", t::int4() },
            },
    });
    std::shared_ptr<storage::index const> index1 = std::make_shared<storage::index>(table, "I1");
    std::shared_ptr<storage::index const> index2 = std::make_shared<storage::index>(table, "I2");

    plan::forward exchange1 {};
    plan::forward exchange2 {};

    static std::size_t hash(descriptor::relation const& desc) {
        return std::hash<descriptor::relation> {}(desc);
    }
};

TEST_F(relation_info_test, kind_of_index) {
    auto desc = bindings.index(*index1);
    EXPECT_EQ(kind_of(desc), relation_info_kind::index);
}

TEST_F(relation_info_test, kind_of_exchange) {
    auto desc = bindings.exchange(exchange1);
    EXPECT_EQ(kind_of(desc), relation_info_kind::exchange);
}

TEST_F(relation_info_test, extract_index) {
    auto desc = bindings.index(*index1);
    EXPECT_EQ(std::addressof(extract<storage::index>(desc)), index1.get());
}

TEST_F(relation_info_test, extract_exchange) {
    auto desc = bindings.exchange(exchange1);
    EXPECT_EQ(std::addressof(extract<plan::exchange>(desc)), std::addressof(exchange1));
}

TEST_F(relation_info_test, extract_mismatch) {
    auto index_desc = bindings.index(*index1);
    auto exchange_desc = bindings.exchange(exchange1);
    EXPECT_THROW((void) extract<plan::exchange>(index_desc), std::invalid_argument);
    EXPECT_THROW((void) extract<storage::index>(exchange_desc), std::invalid_argument);
}

TEST_F(relation_info_test, extract_if_index) {
    auto desc = bindings.index(*index1);

    auto found = extract_if<storage::index>(desc);
    ASSERT_TRUE(found);
    EXPECT_EQ(found.get(), index1.get());

    EXPECT_FALSE(extract_if<plan::exchange>(desc));
}

TEST_F(relation_info_test, extract_if_exchange) {
    auto desc = bindings.exchange(exchange1);

    auto found = extract_if<plan::exchange>(desc);
    ASSERT_TRUE(found);
    EXPECT_EQ(found.get(), std::addressof(exchange1));

    EXPECT_FALSE(extract_if<storage::index>(desc));
}

TEST_F(relation_info_test, extract_shared_index_shared) {
    auto desc = bindings.index(maybe_shared_ptr<storage::index const> { index1 });

    auto found = extract_shared<storage::index>(desc);
    ASSERT_TRUE(found);
    EXPECT_EQ(found.get(), index1.get());
    EXPECT_EQ(found.ownership(), index1);
}

TEST_F(relation_info_test, extract_shared_index_reference) {
    auto desc = bindings.index(*index1);

    auto found = extract_shared<storage::index>(desc);
    ASSERT_TRUE(found);
    EXPECT_EQ(found.get(), index1.get());
    EXPECT_FALSE(found.ownership());
}

TEST_F(relation_info_test, extract_shared_mismatch) {
    auto desc = bindings.exchange(exchange1);
    EXPECT_FALSE(extract_shared<storage::index>(desc));
}

TEST_F(relation_info_test, equivalent_index) {
    auto a = bindings.index(*index1);
    auto b = bindings.index(*index1);
    EXPECT_EQ(a, b);
    EXPECT_EQ(hash(a), hash(b));
}

TEST_F(relation_info_test, equivalent_index_reference_and_shared) {
    auto a = bindings.index(*index1);
    auto b = bindings.index(maybe_shared_ptr<storage::index const> { index1 });
    EXPECT_EQ(a, b);
    EXPECT_EQ(hash(a), hash(b));
}

TEST_F(relation_info_test, different_index) {
    auto a = bindings.index(*index1);
    auto b = bindings.index(*index2);
    EXPECT_NE(a, b);
}

TEST_F(relation_info_test, equivalent_exchange) {
    auto a = bindings.exchange(exchange1);
    auto b = bindings.exchange(exchange1);
    EXPECT_EQ(a, b);
    EXPECT_EQ(hash(a), hash(b));
}

TEST_F(relation_info_test, different_exchange) {
    auto a = bindings.exchange(exchange1);
    auto b = bindings.exchange(exchange2);
    EXPECT_NE(a, b);
}

TEST_F(relation_info_test, different_kind) {
    auto a = bindings.index(*index1);
    auto b = bindings.exchange(exchange1);
    EXPECT_NE(a, b);
    EXPECT_NE(b, a);
}

TEST_F(relation_info_test, different_kind_external) {
    auto external = bindings.external_relation(external_relation_info { 1, {} });
    auto index = bindings.index(*index1);
    auto exchange = bindings.exchange(exchange1);
    EXPECT_NE(external, index);
    EXPECT_NE(index, external);
    EXPECT_NE(external, exchange);
    EXPECT_NE(exchange, external);
}

TEST_F(relation_info_test, kind_of_external) {
    auto desc = bindings.external_relation(external_relation_info { 1, {} });
    EXPECT_EQ(kind_of(desc), relation_info_kind::external);
}

TEST_F(relation_info_test, extract_external) {
    auto info = std::make_shared<external_relation_info>(1, std::vector<descriptor::variable> {});
    auto desc = bindings.external_relation(info);
    EXPECT_EQ(std::addressof(extract<external_relation_info>(desc)), info.get());
}

TEST_F(relation_info_test, extract_external_mismatch) {
    auto external_desc = bindings.external_relation(external_relation_info { 1, {} });
    auto index_desc = bindings.index(*index1);
    auto exchange_desc = bindings.exchange(exchange1);
    EXPECT_THROW((void) extract<storage::index>(external_desc), std::invalid_argument);
    EXPECT_THROW((void) extract<plan::exchange>(external_desc), std::invalid_argument);
    EXPECT_THROW((void) extract<external_relation_info>(index_desc), std::invalid_argument);
    EXPECT_THROW((void) extract<external_relation_info>(exchange_desc), std::invalid_argument);
}

TEST_F(relation_info_test, extract_if_external) {
    auto info = std::make_shared<external_relation_info>(1, std::vector<descriptor::variable> {});
    auto desc = bindings.external_relation(info);

    auto found = extract_if<external_relation_info>(desc);
    ASSERT_TRUE(found);
    EXPECT_EQ(found.get(), info.get());

    EXPECT_FALSE(extract_if<storage::index>(desc));
    EXPECT_FALSE(extract_if<plan::exchange>(desc));
}

TEST_F(relation_info_test, extract_if_external_mismatch) {
    auto index_desc = bindings.index(*index1);
    auto exchange_desc = bindings.exchange(exchange1);
    EXPECT_FALSE(extract_if<external_relation_info>(index_desc));
    EXPECT_FALSE(extract_if<external_relation_info>(exchange_desc));
}

TEST_F(relation_info_test, kind_to_string) {
    EXPECT_EQ(to_string_view(relation_info_kind::index), "index");
    EXPECT_EQ(to_string_view(relation_info_kind::exchange), "exchange");

    std::ostringstream out {};
    out << relation_info_kind::index;
    EXPECT_EQ(out.str(), "index");
}

} // namespace yugawara::binding

#include "yugawara/analyzer/intermediate_plan_normalizer.h"

#include <gtest/gtest.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/find.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/join_scan.h>
#include <takatori/relation/project.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/configurable_provider.h>
#include <yugawara/extension/relation/subquery.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer {

// import test utils
using namespace ::yugawara::testing;

using ::takatori::scalar::comparison_operator;

using ::takatori::relation::endpoint_kind;

class intermediate_plan_normalizer_test: public ::testing::Test {
protected:
    binding::factory bindings {};

    std::shared_ptr<storage::configurable_provider> storages = std::make_shared<storage::configurable_provider>();

    std::shared_ptr<storage::table> t0 = storages->add_table({
            "T0",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    std::shared_ptr<storage::table> t1 = storages->add_table({
            "T1",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    storage::column const& t0c0 = t0->columns()[0];
    storage::column const& t0c1 = t0->columns()[1];
    storage::column const& t0c2 = t0->columns()[2];
    storage::column const& t1c0 = t1->columns()[0];
    storage::column const& t1c1 = t1->columns()[1];
    storage::column const& t1c2 = t1->columns()[2];

    std::shared_ptr<storage::index> i0 = storages->add_index({ t0, "I0", });
    std::shared_ptr<storage::index> i1 = storages->add_index({ t1, "I1", });
};

TEST_F(intermediate_plan_normalizer_test, relation_subquery) {
    relation::graph_type inner;
    auto c0 = bindings.stream_variable("c0");
    auto&& in = inner.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
            },
    });

    relation::graph_type r;
    auto o0 = bindings.stream_variable("o0");
    auto&& subquery = r.insert(extension::relation::subquery {
            std::move(inner),
            {
                    { c0, o0 },
            },
            true,
    });
    auto&& out = r.insert(relation::emit { o0 });
    subquery.output() >> out.input();

    intermediate_plan_normalizer normalizer {};
    normalizer(r);

    ASSERT_EQ(r.size(), 3);
    ASSERT_TRUE(r.contains(in));
    ASSERT_TRUE(r.contains(out));
    auto&& project = next<relation::project>(in.output());

    ASSERT_EQ(in.columns().size(), 1);
    EXPECT_EQ(in.columns()[0].source(), bindings(t0c0));
    auto&& c0m = in.columns()[0].destination();
    EXPECT_NE(c0m, c0);

    ASSERT_EQ(project.columns().size(), 1);
    EXPECT_EQ(project.columns()[0].value(), scalar::variable_reference { c0m });
    auto&& q0 = project.columns()[0].variable();

    ASSERT_EQ(out.columns().size(), 1);
    EXPECT_EQ(out.columns()[0].source(), q0);
}

} // namespace yugawara::analyzer

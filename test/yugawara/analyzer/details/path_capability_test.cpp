#include <yugawara/analyzer/details/path_capability.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>

#include <takatori/scalar/binary.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/project.h>
#include <takatori/relation/filter.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/configurable_provider.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

class path_capability_test : public ::testing::Test {
protected:
    binding::factory bindings;

    storage::configurable_provider storages;

    std::shared_ptr<storage::table> t0 = storages.add_table({
            "T0",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
            },
    });
    descriptor::variable t0c0 = bindings(t0->columns()[0]);
    descriptor::variable t0c1 = bindings(t0->columns()[1]);

    std::shared_ptr<storage::index> i0 = storages.add_index({ t0, "I0", });

    descriptor::variable c0 = bindings.stream_variable("c0");
    descriptor::variable c1 = bindings.stream_variable("c1");

    relation::scan& add_scan(relation::graph_type& r) {
        return r.insert(relation::scan {
                bindings(*i0),
                {
                        { t0c0, c0 },
                        { t0c1, c1 },
                },
        });
    }
};

TEST_F(path_capability_test, project) {
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::project {
            relation::project::column {
                    varref { c0 },
                    bindings.stream_variable("x"),
            },
    });
    r0.output() >> r1.input();

    auto result = find_path_capabilities(r1.input(), r1.output());
    EXPECT_EQ(result, (path_capability_set {
            path_capability::preserves_order,
            path_capability::at_least_one,
            path_capability::exactly_one,
    }));
}

TEST_F(path_capability_test, filter) {
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::filter {
            compare(c0, c1),
    });
    r0.output() >> r1.input();

    auto result = find_path_capabilities(r1.input(), r1.output());
    EXPECT_EQ(result, path_capability_set {});
}

TEST_F(path_capability_test, different_operators) {
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::project {
            relation::project::column {
                    varref { c0 },
                    bindings.stream_variable("x"),
            },
    });
    auto&& r2 = r.insert(relation::project {
            relation::project::column {
                    varref { c1 },
                    bindings.stream_variable("y"),
            },
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    auto result = find_path_capabilities(r1.input(), r2.output());
    EXPECT_EQ(result, path_capability_set {});
}

} // namespace yugawara::analyzer::details

#include <yugawara/compiler.h>

#include <gtest/gtest.h>

#include <takatori/relation/scan.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/write.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/project.h>
#include <takatori/relation/find.h>

#include <takatori/statement/execute.h>
#include <takatori/statement/write.h>

#include <takatori/serializer/json_printer.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/configurable_provider.h>

#include <yugawara/testing/utils.h>

namespace yugawara {

// import test utils
using namespace ::yugawara::testing;

namespace statement = ::takatori::statement;

using ::takatori::scalar::comparison_operator;

using ::takatori::relation::endpoint_kind;

class compiler_test: public ::testing::Test {
protected:
    ::takatori::util::object_creator creator;
    binding::factory bindings { creator };

    runtime_feature_set runtime_features { compiler_options::default_runtime_features };
    std::shared_ptr<storage::configurable_provider> storages = std::make_shared<storage::configurable_provider>();
    std::shared_ptr<analyzer::index_estimator> indices {};

    std::shared_ptr<storage::table> t0 = storages->add_table("T0", {
            "T0",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    storage::column const& t0c0 = t0->columns()[0];
    storage::column const& t0c1 = t0->columns()[1];
    storage::column const& t0c2 = t0->columns()[2];

    std::shared_ptr<storage::index> i0 = storages->add_index("I0", { t0, "I0", });

    compiler_options options() {
        return {
                storages,
                indices,
                runtime_features,
                creator,
        };
    }

    static ::takatori::util::optional_ptr<compiler_result::diagnostic_type const> find_diagnostic(
            compiler_result::code_type code,
            compiler_result const& result) {
        for (auto&& d : result.diagnostics()) {
            if (d.code() == code) {
                return d;
            }
        }
        return {};
    }

    static void dump(compiler_result const& r) {
        ::takatori::serializer::json_printer printer { std::cout };
        r.object_scanner()(
                r.statement(),
                ::takatori::serializer::json_printer { std::cout });
    }
};

TEST_F(compiler_test, graph_emit) {
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto&& in = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
            },
    });
    auto&& out = r.insert(relation::emit { c0 });
    in.output() >> out.input();

    auto result = compiler()(options(), std::move(r));
    ASSERT_TRUE(result);

    auto&& c = downcast<statement::execute>(result.statement());

    ASSERT_EQ(c.execution_plan().size(), 1);
    auto&& p0 = find(c.execution_plan(), in);

    ASSERT_EQ(p0.operators().size(), 2);
    ASSERT_TRUE(p0.operators().contains(in));
    ASSERT_TRUE(p0.operators().contains(out));

    ASSERT_EQ(in.columns().size(), 1);
    EXPECT_EQ(in.columns()[0].source(), bindings(t0c0));
    auto&& c0p0 = in.columns()[0].destination();

    ASSERT_EQ(out.columns().size(), 1);
    EXPECT_EQ(out.columns()[0].source(), c0p0);

    EXPECT_EQ(result.type_of(bindings(t0c0)), t::int4());
    EXPECT_EQ(result.type_of(c0p0), t::int4());

    dump(result);
}

TEST_F(compiler_test, graph_update) {
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& in = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
                    { bindings(t0c2), c2 },
            },
    });
    auto&& filter = r.insert(relation::filter {
            compare(varref { c1 }, constant(0)),
    });
    auto x2 = bindings.stream_variable("x2");
    auto&& project = r.insert(relation::project {
            relation::project::column {
                    scalar::binary {
                            scalar::binary_operator::add,
                            varref { c2 },
                            constant(1)
                    },
                    x2,
            },
    });
    auto&& out = r.insert(relation::write {
            relation::write_kind::update,
            bindings(*i0),
            {
                    { c0, bindings(t0c0) },
            },
            {
                    { x2, bindings(t0c2) },
            },
    });
    in.output() >> filter.input();
    filter.output() >> project.input();
    project.output() >> out.input();

    storages->add_index("I0C1", {
            t0,
            "I0C1",
            {
                    t0c1,
            },
            {},
            {
                    storage::index_feature::find,
            },
    });
    auto result = compiler()(options(), std::move(r));
    ASSERT_TRUE(result);

    auto&& c = downcast<statement::execute>(result.statement());

    ASSERT_EQ(c.execution_plan().size(), 1);
    auto&& p0 = find(c.execution_plan(), out);

    ASSERT_EQ(p0.operators().size(), 3);
    auto&& find = head<relation::find>(p0.operators());
    auto&& prj = next<relation::project>(find.output());
    auto&& wrt = next<relation::write>(prj.output());

    ASSERT_EQ(find.columns().size(), 2);
    EXPECT_EQ(find.columns()[0].source(), bindings(t0c0));
    EXPECT_EQ(find.columns()[1].source(), bindings(t0c2));
    auto&& c0p0 = find.columns()[0].destination();
    auto&& c2p0 = find.columns()[1].destination();
    ASSERT_EQ(find.keys().size(), 1);
    EXPECT_EQ(find.keys()[0].variable(), bindings(t0c1));
    EXPECT_EQ(find.keys()[0].value(), constant(0));

    ASSERT_EQ(prj.columns().size(), 1);
    EXPECT_EQ(prj.columns()[0].value(), (scalar::binary {
            scalar::binary_operator::add,
            varref { c2p0 },
            constant(1)
    }));
    auto&& x2p0 = prj.columns()[0].variable();

    ASSERT_EQ(wrt.keys().size(), 1);
    EXPECT_EQ(wrt.keys()[0].source(), c0p0);
    EXPECT_EQ(wrt.keys()[0].destination(), bindings(t0c0));

    ASSERT_EQ(wrt.columns().size(), 1);
    EXPECT_EQ(wrt.columns()[0].source(), x2p0);

    EXPECT_EQ(result.type_of(c0p0), t::int4());
    EXPECT_EQ(result.type_of(c2p0), t::int4());
    EXPECT_EQ(result.type_of(x2p0), t::int4());

    EXPECT_EQ(result.type_of(prj.columns()[0].value()), t::int4());
    {
        auto&& b = downcast<scalar::binary>(prj.columns()[0].value());
        EXPECT_EQ(result.type_of(b.left()), t::int4());
        EXPECT_EQ(result.type_of(b.right()), t::int4());
    }

    dump(result);
}

TEST_F(compiler_test, graph_error) {
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto&& in = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
            },
    });
    // WHERE C0 + 1
    auto&& filter = r.insert(relation::filter {
            scalar::binary {
                    scalar::binary_operator::add,
                    varref { c0 },
                    constant(1),
            },
    });
    auto&& out = r.insert(relation::emit { c0 });
    in.output() >> filter.input();
    filter.output() >> out.input();

    auto result = compiler()(options(), std::move(r));
    EXPECT_FALSE(result);

    ASSERT_EQ(result.diagnostics().size(), 1);
    EXPECT_TRUE(find_diagnostic(compiler_code::inconsistent_type, result));
}

TEST_F(compiler_test, write) {
    auto result = compiler()(options(), statement::write {
            statement::write_kind::insert,
            bindings(*i0),
            {
                    bindings(t0c0),
                    bindings(t0c1),
                    bindings(t0c2),
            },
            {
                    {
                            constant(0),
                            constant(1),
                            constant(2),
                    },
            },
    });
    ASSERT_TRUE(result);

    auto&& c = downcast<statement::write>(result.statement());

    EXPECT_EQ(c.operator_kind(), statement::write_kind::insert);
    EXPECT_EQ(c.destination(), bindings(*i0));
    ASSERT_EQ(c.columns().size(), 3);
    EXPECT_EQ(c.columns()[0], bindings(t0c0));
    EXPECT_EQ(c.columns()[1], bindings(t0c1));
    EXPECT_EQ(c.columns()[2], bindings(t0c2));
    ASSERT_EQ(c.tuples().size(), 1);
    EXPECT_EQ(result.type_of(bindings(t0c0)), t::int4());
    EXPECT_EQ(result.type_of(bindings(t0c1)), t::int4());
    EXPECT_EQ(result.type_of(bindings(t0c2)), t::int4());
    {
        auto&& vs = c.tuples()[0].elements();
        ASSERT_EQ(vs.size(), 3);
        EXPECT_EQ(vs[0], constant(0));
        EXPECT_EQ(vs[1], constant(1));
        EXPECT_EQ(vs[2], constant(2));

        EXPECT_EQ(result.type_of(vs[0]), t::int4());
        EXPECT_EQ(result.type_of(vs[1]), t::int4());
        EXPECT_EQ(result.type_of(vs[2]), t::int4());
    }
}

TEST_F(compiler_test, write_error) {
    auto result = compiler()(options(), statement::write {
            statement::write_kind::insert,
            bindings(*i0),
            {
                    bindings(t0c0),
                    bindings(t0c1),
                    bindings(t0c2),
            },
            {
                    {
                            constant(0),
                            boolean(false),
                            constant(2),
                    },
            },
    });
    EXPECT_FALSE(result);
    EXPECT_TRUE(find_diagnostic(compiler_code::inconsistent_type, result));
}

} // namespace yugawara

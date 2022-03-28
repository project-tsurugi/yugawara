#include <yugawara/compiler.h>

#include <gtest/gtest.h>

#include <takatori/value/character.h>

#include <takatori/relation/scan.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/write.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/project.h>
#include <takatori/relation/find.h>
#include <takatori/relation/intermediate/join.h>

#include <takatori/statement/execute.h>
#include <takatori/statement/write.h>
#include <takatori/statement/create_table.h>
#include <takatori/statement/drop_table.h>
#include <takatori/statement/create_index.h>
#include <takatori/statement/drop_index.h>
#include <takatori/statement/empty.h>

#include <takatori/serializer/json_printer.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/configurable_provider.h>

#include <yugawara/testing/utils.h>

namespace yugawara {

// import test utils
using namespace ::yugawara::testing;

namespace statement = ::takatori::statement;

using ::takatori::scalar::comparison_operator;

class compiler_test: public ::testing::Test {
protected:
    binding::factory bindings {};

    runtime_feature_set runtime_features { compiler_options::default_runtime_features };
    std::shared_ptr<storage::configurable_provider> storages = std::make_shared<storage::configurable_provider>();
    std::shared_ptr<analyzer::index_estimator> indices {};

    std::shared_ptr<storage::table> t0 = storages->add_table({
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

    std::shared_ptr<storage::index> i0 = storages->add_index({ t0, "I0", });

    compiler_options options() {
        return {
                runtime_features,
                {},
                indices,
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

    // inspection
    {
        auto inspection = compiler().inspect(c);
        ASSERT_TRUE(inspection);

        EXPECT_EQ(inspection->type_of(bindings(t0c0)), t::int4());
        EXPECT_EQ(inspection->type_of(c0p0), t::int4());
    }
    {
        auto inspection = compiler().inspect(c.execution_plan());
        ASSERT_TRUE(inspection);

        EXPECT_EQ(inspection->type_of(bindings(t0c0)), t::int4());
        EXPECT_EQ(inspection->type_of(c0p0), t::int4());
    }
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

    storages->add_index({
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

TEST_F(compiler_test, create_table) {
    auto sch = std::make_shared<schema::declaration>("SC");
    auto tbl = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    auto idx = std::make_shared<storage::index>(storage::index {
            tbl,
            "I",
            {
                    { tbl->columns()[0] }
            }
    });
    auto result = compiler()(options(), statement::create_table {
            bindings(sch),
            bindings(tbl),
            bindings(idx),
            {},
    });
    ASSERT_TRUE(result);

    auto&& c = downcast<statement::create_table>(result.statement());
    auto&& rt = binding::extract(c.definition());
    EXPECT_NE(std::addressof(rt), tbl.get());
    EXPECT_EQ(rt.simple_name(), tbl->simple_name());

    auto&& rk = binding::extract<storage::index>(c.primary_key());
    EXPECT_NE(std::addressof(rk), idx.get());
    EXPECT_EQ(rk.simple_name(), idx->simple_name());
    EXPECT_EQ(std::addressof(rk.table()), std::addressof(rt));
}

TEST_F(compiler_test, create_table_default_value_immediate_inconsistent) {
    auto sch = std::make_shared<schema::declaration>("SC");
    auto tbl = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4(), variable::nullable, v::character { "X" } },
                    { "C2", t::int4() },
            },
    });
    auto idx = std::make_shared<storage::index>(storage::index {
            tbl,
            "I",
            {
                    { tbl->columns()[0] }
            }
    });
    auto result = compiler()(options(), statement::create_table {
            bindings(sch),
            bindings(tbl),
            bindings(idx),
            {},
    });
    EXPECT_FALSE(result);
    EXPECT_TRUE(find_diagnostic(compiler_code::inconsistent_type, result));
}

TEST_F(compiler_test, drop_table) {
    auto sch = std::make_shared<schema::declaration>("SC");
    auto result = compiler()(options(), statement::drop_table {
            bindings(sch),
            bindings(t0),
    });
    ASSERT_TRUE(result);

    auto&& c = downcast<statement::drop_table>(result.statement());
    auto&& r = binding::extract(c.target());
    EXPECT_EQ(std::addressof(r), t0.get());
}

TEST_F(compiler_test, create_index) {
    auto sch = std::make_shared<schema::declaration>("SC");
    auto result = compiler()(options(), statement::create_index {
            bindings(sch),
            bindings(i0),
    });
    ASSERT_TRUE(result);

    auto&& c = downcast<statement::create_index>(result.statement());
    auto&& r = binding::extract<storage::index>(c.definition());
    EXPECT_NE(std::addressof(r), i0.get());
    EXPECT_EQ(r.simple_name(), i0->simple_name());
    EXPECT_EQ(r.shared_table(), t0);
}

TEST_F(compiler_test, drop_index) {
    auto sch = std::make_shared<schema::declaration>("SC");
    auto result = compiler()(options(), statement::drop_index {
            bindings(sch),
            bindings(i0),
    });
    ASSERT_TRUE(result);

    auto&& c = downcast<statement::drop_index>(result.statement());
    auto&& r = binding::extract<storage::index>(c.target());
    EXPECT_EQ(std::addressof(r), i0.get());
}

TEST_F(compiler_test, empty) {
    auto result = compiler()(options(), statement::empty {});
    ASSERT_TRUE(result);
}

TEST_F(compiler_test, inspect_scalar) {
    scalar::immediate expr {
            v::int4 { 100 },
            t::int4 {},
    };

    auto result = compiler().inspect(expr);
    ASSERT_TRUE(result);

    EXPECT_EQ(result->type_of(expr), t::int4 {});
}

TEST_F(compiler_test, inspect_relation) {
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

    auto inspection = compiler().inspect(r);
    ASSERT_TRUE(inspection);

    EXPECT_EQ(inspection->type_of(bindings(t0c0)), t::int4());
    EXPECT_EQ(inspection->type_of(c0), t::int4());
}

TEST_F(compiler_test, fix_heap_reuse) {
    auto t_c = storages->add_table({
            "customer",
            {
                    { "c_id", t::int4()} ,
                    { "c_w_id", t::int4() },
                    { "c_d_id", t::int4() },
                    { "c_x", t::int4() },
            },
    });
    auto t_w = storages->add_table({
            "warehouse",
            {
                    { "w_id", t::int4()} ,
                    { "w_x", t::int4()} ,
            },
    });
    auto i_c = storages->add_index({
            t_c,
            "customer_primary",
            {
                    t_c->columns()[0],
                    t_c->columns()[1],
                    t_c->columns()[2],
            },
            {},
            {
                    ::yugawara::storage::index_feature::find,
                    ::yugawara::storage::index_feature::scan,
                    ::yugawara::storage::index_feature::unique,
                    ::yugawara::storage::index_feature::primary,
            },
    });
    auto i_w = storages->add_index({
            t_w,
            "warehouse_primary",
            {
                    t_w->columns()[0],
            },
            {},
            {
                    ::yugawara::storage::index_feature::find,
                    ::yugawara::storage::index_feature::scan,
                    ::yugawara::storage::index_feature::unique,
                    ::yugawara::storage::index_feature::primary,
            },
    });

    /*
    "SELECT w_x, c_x FROM warehouse, customer "
    "WHERE w_id = :w_id "
    "AND c_w_id = w_id AND "
    "c_d_id = :c_d_id AND "
    "c_id = :c_id "
    */
    relation::graph_type r;

    auto w_id = bindings.stream_variable("w_id");
    auto w_x = bindings.stream_variable("w_x");
    auto&& inl = r.insert(relation::scan {
            bindings(*i_w),
            {
                    { bindings(t_w->columns()[0]), w_id },
                    { bindings(t_w->columns()[1]), w_x },
            },
    });

    auto c_id = bindings.stream_variable("c_id");
    auto c_w_id = bindings.stream_variable("c_w_id");
    auto c_d_id = bindings.stream_variable("c_d_id");
    auto c_x = bindings.stream_variable("c_x");
    auto&& inr = r.insert(relation::scan {
            bindings(*i_c),
            {
                    { bindings(t_c->columns()[0]), c_id },
                    { bindings(t_c->columns()[1]), c_w_id },
                    { bindings(t_c->columns()[2]), c_d_id },
                    { bindings(t_c->columns()[3]), c_x },
            },
    });

    /*
    "SELECT w_x, c_x FROM warehouse, customer "
    "WHERE w_id = :w_id "
    "AND c_w_id = w_id AND "
    "c_d_id = :c_d_id AND "
    "c_id = :c_id "
    */

    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });

    auto&& filter = r.insert(relation::filter {
            land(
                    compare(varref(w_id), constant(1)),
                    land(
                            compare(varref(c_w_id), varref(w_id)),
                            land(
                                    compare(varref(c_d_id), constant(2)),
                                    compare(varref(c_id), constant(3))))),
    });

    auto&& out = r.insert(relation::emit { w_x, c_x });

    inl.output() >> join.left();
    inr.output() >> join.right();
    join.output() >> filter.input();
    filter.output() >> out.input();

    auto result = compiler()(options(), std::move(r));
    ASSERT_TRUE(result);
}

} // namespace yugawara

#include <yugawara/compiler.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/type/character.h>
#include <takatori/type/table.h>

#include <takatori/value/character.h>

#include <takatori/relation/scan.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/write.h>
#include <takatori/relation/apply.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/project.h>
#include <takatori/relation/find.h>
#include <takatori/relation/values.h>
#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/intermediate/aggregate.h>
#include <takatori/relation/step/join.h>

#include <takatori/plan/group.h>

#include <takatori/statement/execute.h>
#include <takatori/statement/write.h>
#include <takatori/statement/create_table.h>
#include <takatori/statement/drop_table.h>
#include <takatori/statement/create_index.h>
#include <takatori/statement/drop_index.h>
#include <takatori/statement/grant_table.h>
#include <takatori/statement/revoke_table.h>
#include <takatori/statement/empty.h>

#include <takatori/serializer/json_printer.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/configurable_provider.h>
#include <yugawara/variable/declaration.h>

#include <yugawara/extension/scalar/subquery.h>
#include <yugawara/extension/scalar/exists.h>
#include <yugawara/extension/scalar/quantified_compare.h>
#include <yugawara/extension/relation/subquery.h>

#include <yugawara/testing/utils.h>

namespace yugawara {

// import test utils
using namespace ::yugawara::testing;

namespace statement = ::takatori::statement;

using ::takatori::util::optional_ptr;
using ::takatori::util::string_builder;

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

    descriptor::variable find_destination(
            descriptor::variable const& v,
            std::vector<relation::details::mapping_element> const& mappings) {
        auto iter = std::find_if(
                mappings.begin(), mappings.end(),
                [&](relation::details::mapping_element const& e) {
                    return e.source() == v;
                });
        if (iter == mappings.end()) {
            throw std::domain_error(string_builder {}
                    << "missing mapping for: "
                    << v
                    << string_builder::to_string);
        }
        return iter->destination();
    }

    static std::string diagnostics(compiler_result const& result) {
        std::ostringstream oss;
        for (auto&& d : result.diagnostics()) {
            oss << d << std::endl;
        }
        return oss.str();
    }

    static optional_ptr<compiler_result::diagnostic_type const> find_diagnostic(
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
        std::cout << ::testing::UnitTest::GetInstance()->current_test_info()->name() << ": ";
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
    dump(result);

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
    dump(result);

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
    dump(result);

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
    dump(result);

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
    dump(result);

    auto&& c = downcast<statement::drop_index>(result.statement());
    auto&& r = binding::extract<storage::index>(c.target());
    EXPECT_EQ(std::addressof(r), i0.get());
}

TEST_F(compiler_test, grant_table) {
    auto result = compiler()(options(), statement::grant_table {
            {
                    bindings(t0),
                    {
                            statement::table_action_kind::select,
                    },
                    {
                            {
                                    statement::authorization_user_kind::specified,
                                    "admin",
                                    {
                                        statement::table_action_kind::control,
                                    },
                            },
                    },
            },
    });
    ASSERT_TRUE(result);
    dump(result);
}

TEST_F(compiler_test, revoke_table) {
    auto result = compiler()(options(), statement::revoke_table {
            {
                    bindings(t0),
                    {
                            statement::table_action_kind::select,
                    },
                    {
                            {
                                    statement::authorization_user_kind::specified,
                                    "admin",
                                    {
                                        statement::table_action_kind::control,
                                    },
                            },
                    },
            },
    });
    ASSERT_TRUE(result);
    dump(result);
}

TEST_F(compiler_test, empty) {
    auto result = compiler()(options(), statement::empty {});
    ASSERT_TRUE(result);
    dump(result);
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

TEST_F(compiler_test, fix_left_join_cond) {
    auto t_tb1 = storages->add_table({
            "tb1",
            {
                    { "pk", t::int4()} ,
                    { "c1", t::int4()} ,
            },
    });
    auto t_tb2 = storages->add_table({
            "tb2",
            {
                    { "pk", t::int4()} ,
                    { "c2", t::int4()} ,
            },
    });
    auto i_tb1 = storages->add_index({
            t_tb1,
            "i_tb1",
            {
                    t_tb1->columns()[0],
            },
            {},
            {
                    ::yugawara::storage::index_feature::find,
                    ::yugawara::storage::index_feature::scan,
                    ::yugawara::storage::index_feature::unique,
                    ::yugawara::storage::index_feature::primary,
            },
    });
    auto i_tb2 = storages->add_index({
            t_tb2,
            "i_tb2",
            {
                    t_tb2->columns()[0],
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
    "SELECT pk FROM tb1 "
    "LEFT OUTER JOIN tb2 ON tb1.pk = tb2.pk"
    "WHERE tb1.pk = 1"
    */
    relation::graph_type r;

    auto tb1_pk = bindings.stream_variable("tb1_pk");
    auto tb1_c1 = bindings.stream_variable("tb1_c1");
    auto tb2_pk = bindings.stream_variable("tb2_pk");
    auto tb2_c2 = bindings.stream_variable("tb2_c2");
    auto&& in_tb1 = r.insert(relation::scan {
            bindings(*i_tb1),
            {
                    { bindings(t_tb1->columns()[0]), tb1_pk },
                    { bindings(t_tb1->columns()[1]), tb1_c1 },
            },
    });
    auto&& in_tb2 = r.insert(relation::scan {
            bindings(*i_tb2),
            {
                    { bindings(t_tb2->columns()[0]), tb2_pk },
                    { bindings(t_tb2->columns()[1]), tb2_c2 },
            },
    });

    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
            compare(varref(tb1_pk), varref(tb2_pk))
    });

    auto&& filter = r.insert(relation::filter {
            compare(varref(tb2_pk), constant(1))
    });

    auto&& out = r.insert(relation::emit { tb1_pk, tb2_pk });

    in_tb1.output() >> join.left();
    in_tb2.output() >> join.right();
    join.output() >> filter.input();
    filter.output() >> out.input();

    auto opts = options();
    opts.runtime_features().erase(runtime_feature::index_join);
    opts.runtime_features().erase(runtime_feature::broadcast_exchange);
    opts.runtime_features().erase(runtime_feature::broadcast_join_scan);
    auto result = compiler()(opts, std::move(r));
    ASSERT_TRUE(result);

    dump(result);
}

TEST_F(compiler_test, fix_join_duplicate_cond) {
    auto t_tb1 = storages->add_table({
            "tb1",
            {
                    { "pk", t::int4() },
                    { "c1", t::int4() },
                    { "c2", t::int4() },
            },
    });
    auto t_tb2 = storages->add_table({
            "tb2",
            {
                    { "pk", t::int4() },
                    { "c1", t::int4() },
                    { "c2", t::int4() },
            },
    });
    auto i_tb1 = storages->add_index({
            t_tb1,
            "i_tb1",
            {
                    t_tb1->columns()[0],
            },
            {},
            {
                    ::yugawara::storage::index_feature::find,
                    ::yugawara::storage::index_feature::scan,
                    ::yugawara::storage::index_feature::unique,
                    ::yugawara::storage::index_feature::primary,
            },
    });
    auto i_tb2 = storages->add_index({
            t_tb2,
            "i_tb2",
            {
                    t_tb2->columns()[0],
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
    "SELECT pk FROM tb1 "
    "LEFT OUTER JOIN tb2 ON tb1.c1 = tb2.c1 AND tb1.c1 = tb2.c2"
    "WHERE tb1.pk = 1"
    */
    relation::graph_type r;

    auto tb1_pk = bindings.stream_variable("tb1_pk");
    auto tb1_c1 = bindings.stream_variable("tb1_c1");
    auto tb1_c2 = bindings.stream_variable("tb1_c2");
    auto tb2_pk = bindings.stream_variable("tb2_pk");
    auto tb2_c1 = bindings.stream_variable("tb2_c1");
    auto tb2_c2 = bindings.stream_variable("tb2_c2");
    auto&& in_tb1 = r.insert(relation::scan {
            bindings(*i_tb1),
            {
                    { bindings(t_tb1->columns()[0]), tb1_pk },
                    { bindings(t_tb1->columns()[1]), tb1_c1 },
                    { bindings(t_tb1->columns()[2]), tb1_c2 },
            },
    });
    auto&& in_tb2 = r.insert(relation::scan {
            bindings(*i_tb2),
            {
                    { bindings(t_tb2->columns()[0]), tb2_pk },
                    { bindings(t_tb2->columns()[1]), tb2_c1 },
                    { bindings(t_tb2->columns()[2]), tb2_c2 },
            },
    });

    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
            land(
                    compare(varref(tb1_c1), varref(tb2_c1)),
                    compare(varref(tb1_c1), varref(tb2_c2)))
    });

    auto&& filter = r.insert(relation::filter {
            compare(varref(tb2_pk), constant(1))
    });

    auto&& out = r.insert(relation::emit { tb1_pk, tb2_pk });

    in_tb1.output() >> join.left();
    in_tb2.output() >> join.right();
    join.output() >> filter.input();
    filter.output() >> out.input();

    auto opts = options();
    opts.runtime_features().erase(runtime_feature::index_join);
    opts.runtime_features().erase(runtime_feature::broadcast_exchange);
    opts.runtime_features().erase(runtime_feature::broadcast_join_scan);
    auto result = compiler()(opts, std::move(r));
    ASSERT_TRUE(result) << diagnostics(result);

    dump(result);
}

TEST_F(compiler_test, fix_shuffle_join_key_confuse) {
    auto t_tb1 = storages->add_table({
            "tb1",
            {
                    { "c1", t::int4() },
                    { "c2", t::character { t::varying, {} } },
            },
    });
    auto t_tb2 = storages->add_table({
            "tb2",
            {
                    { "c1", t::int4() },
                    { "c2", t::character { t::varying, {} } },
            },
    });
    auto i_tb1 = storages->add_index({
            t_tb1,
            "i_tb1",
            {},
            {},
            {
                    ::yugawara::storage::index_feature::find,
                    ::yugawara::storage::index_feature::scan,
                    ::yugawara::storage::index_feature::unique,
                    ::yugawara::storage::index_feature::primary,
            },
    });
    auto i_tb2 = storages->add_index({
            t_tb2,
            "i_tb2",
            {},
            {},
            {
                    ::yugawara::storage::index_feature::find,
                    ::yugawara::storage::index_feature::scan,
                    ::yugawara::storage::index_feature::unique,
                    ::yugawara::storage::index_feature::primary,
            },
    });

    /*
     " SELECT tb1.c1, tb1.c2
     * FROM tb1 JOIN tb2
     *   ON  tb1.c1 = tb2.c1
     *   AND tb1.c2 = tb2.c2
     */
    relation::graph_type r;

    auto tb1_c1 = bindings.stream_variable("tb1_c1");
    auto tb1_c2 = bindings.stream_variable("tb1_c2");
    auto tb2_c1 = bindings.stream_variable("tb2_c1");
    auto tb2_c2 = bindings.stream_variable("tb2_c2");
    auto&& in_tb1 = r.insert(relation::scan {
            bindings(*i_tb1),
            {
                    { bindings(t_tb1->columns()[0]), tb1_c1 },
                    { bindings(t_tb1->columns()[1]), tb1_c2 },
            },
    });
    auto&& in_tb2 = r.insert(relation::scan {
            bindings(*i_tb2),
            {
                    { bindings(t_tb2->columns()[0]), tb2_c1 },
                    { bindings(t_tb2->columns()[1]), tb2_c2 },
            },
    });

    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
            land(
                    compare(varref(tb1_c1), varref(tb2_c1)),
                    compare(varref(tb1_c2), varref(tb2_c2)))
    });

    auto&& out = r.insert(relation::emit { tb1_c1, tb2_c1 });

    in_tb1.output() >> join.left();
    in_tb2.output() >> join.right();
    join.output() >> out.input();

    auto opts = options();
    opts.runtime_features().erase(runtime_feature::index_join);
    opts.runtime_features().erase(runtime_feature::broadcast_exchange);
    opts.runtime_features().erase(runtime_feature::broadcast_join_scan);
    auto result = compiler()(opts, std::move(r));
    ASSERT_TRUE(result) << diagnostics(result);

    dump(result);
}

TEST_F(compiler_test, restricted_feature) {
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto&& in = r.insert(relation::values {
            { c0, c1 },
            {},
    });
    auto&& out = r.insert(relation::emit { c0 });
    in.output() >> out.input();

    auto opts = options();
    opts.restricted_features() += {
            restricted_feature::relation_values
    };
    auto result = compiler()(opts, std::move(r));
    EXPECT_FALSE(result);

    auto diagnostics = result.diagnostics();
    ASSERT_EQ(diagnostics.size(), 1);
    EXPECT_EQ(diagnostics[0].code(), compiler_code::unsupported_feature);
}

TEST_F(compiler_test, fix_complex_index_join) {
    auto t_t00 = storages->add_table({
            "t00",
            {
                    { "k", t::int8() },
                    { "c1", t::int8() },
                    { "c2", t::int8() },
                    { "c3", t::int8() },
            },
    });
    auto i_t00 = storages->add_index({
            t_t00,
            "i_t00",
            {
                    t_t00->columns()[0],
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
     " SELECT COUNT(*)
     * FROM t00
     * JOIN t01 ON t00.k = t01.k
     * JOIN t02 ON t00.k = t02.k
     * ...
     * JOIN t10 ON t00.k = t10.k
     * WHERE
     *   t00.k = 0
     *   t00.c1 = 1 AND
     *   t00.c2 = 2 AND
     *   t00.c3 = 3
     */
    relation::graph_type r;
    auto t00_k = bindings.stream_variable("t00_k");
    auto t00_c1 = bindings.stream_variable("t00_c1");
    auto t00_c2 = bindings.stream_variable("t00_c2");
    auto t00_c3 = bindings.stream_variable("t00_c3");
    auto&& in_t00 = r.insert(relation::scan {
            bindings(*i_t00),
            {
                    { bindings(t_t00->columns()[0]), t00_k },
                    { bindings(t_t00->columns()[1]), t00_c1 },
                    { bindings(t_t00->columns()[2]), t00_c2 },
                    { bindings(t_t00->columns()[3]), t00_c3 },
            },
    });
    optional_ptr left { in_t00.output() };

    constexpr std::size_t join_count = 10;
    for (std::size_t index = 1; index < join_count; ++index) {
        auto t_right = storages->add_table({
                string_builder {}
                    << "t" << std::setw(2) << std::setfill('0') << index
                    << string_builder::to_string,
                {
                        { "k", t::int8() },
                },
        });
        auto i_right = storages->add_index({
                t_right,
                std::string_view {
                        string_builder {}
                                << "i_"
                                << t_right->simple_name()
                                << string_builder::to_string
                },
                {
                        t_right->columns()[0],
                },
                {},
                {
                        ::yugawara::storage::index_feature::find,
                        ::yugawara::storage::index_feature::scan,
                        ::yugawara::storage::index_feature::unique,
                        ::yugawara::storage::index_feature::primary,
                },
        });
        auto right_k = bindings.stream_variable("right_k");
        auto&& in_right = r.insert(relation::scan {
                bindings(*i_right),
                {
                        { bindings(t_right->columns()[0]), right_k },
                },
        });
        auto&& join = r.insert(relation::intermediate::join {
                relation::join_kind::inner,
                compare(varref(t00_k), varref(right_k))
        });
        *left >> join.left();
        in_right.output() >> join.right();
        left.reset(join.output());
    }

    auto&& filter = r.insert(relation::filter {
            land(
                    compare(varref(t00_k), constant(0)),
                    land(
                            compare(varref(t00_c1), constant(1)),
                            land(
                                    compare(varref(t00_c2), constant(2)),
                                    compare(varref(t00_c3), constant(3))))
            )
    });

    auto agg_count = bindings.stream_variable("agg_count");
    auto&& aggregate = r.insert(relation::intermediate::aggregate {
            {},
            {
                    {
                            bindings.aggregate_function({
                                    1,
                                    "count",
                                    t::int8 {},
                                    {},
                                    true,
                            }),
                            {},
                            agg_count,
                    },
            },
    });

    auto prj_count = bindings.stream_variable("count");
    auto&& project = r.insert(relation::project {
            {
                    relation::project::column { varref {agg_count }, prj_count },
            },
    });

    auto&& out = r.insert(relation::emit { prj_count });

    *left >> filter.input();
    filter.output() >> aggregate.input();
    aggregate.output() >> project.input();
    project.output() >> out.input();

    auto opts = options();
    opts.runtime_features().erase(runtime_feature::index_join_scan);
    opts.runtime_features().erase(runtime_feature::broadcast_exchange);
    opts.runtime_features().erase(runtime_feature::broadcast_join_scan);
    auto result = compiler()(opts, std::move(r));
    ASSERT_TRUE(result) << diagnostics(result);
    ASSERT_EQ(result.statement().kind(), statement::execute::tag);

    auto&& stmt = downcast<statement::execute>(result.statement());
    EXPECT_EQ(stmt.execution_plan().size(), 3);

    dump(result);
}

TEST_F(compiler_test, feat_apply) {
    /*
     * SELECT t0.c2, tx.x1
     * FROM t0
     * APPLY tvf(t0.c1) AS tx
     * =>
     * r0:scan t0 -> (c0, c1, c2)
     * r1:apply tvf(c1) -> (0:x0, 1:x1, 2:x2)
     * r2:emit c2, x1
     * =>
     * r0:scan t0 -> (c1, c2)
     * r1:apply tvf(c1) -> (1:x1)
     * r2:emit c2, x1
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
                    { bindings(t0c2), c2 },
            },
    });
    auto&& tvf = bindings.function({
            function::declaration::minimum_user_function_id + 1,
            "tvf",
            ::takatori::type::table {
                    { "x0", ::takatori::type::int8 {} },
                    { "x1", ::takatori::type::int8 {} },
                    { "x2", ::takatori::type::int8 {} },
            },
            {
                    ::takatori::type::int8 {},
            },
            {
                    function::function_feature::table_valued_function,
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto x1 = bindings.stream_variable("x1");
    auto x2 = bindings.stream_variable("x2");
    auto& r1 = r.insert(relation::apply {
            tvf,
            {
                    scalar::variable_reference { c1 },
            },
            {
                    { 0, x0 },
                    { 1, x1 },
                    { 2, x2 },
            },
    });
    auto&& r2 = r.insert(relation::emit {
            c2,
            x1,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    auto result = compiler()(options(), std::move(r));
    ASSERT_TRUE(result);

    auto&& c = downcast<statement::execute>(result.statement());

    /*
     * p0:
     *   r0:scan t0 -> (c1, c2)
     *   r1:apply tvf(c1) -> (1:x1)
     *   r2:emit c2, x1
     */
    ASSERT_EQ(c.execution_plan().size(), 1);
    auto&& p0 = find(c.execution_plan(), r0);

    ASSERT_EQ(p0.operators().size(), 3);
    ASSERT_TRUE(p0.operators().contains(r0));
    ASSERT_TRUE(p0.operators().contains(r1));
    ASSERT_TRUE(p0.operators().contains(r2));

    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), bindings(t0c1));
    EXPECT_EQ(r0.columns()[1].source(), bindings(t0c2));
    auto&& c1p0 = r0.columns()[0].destination();
    auto&& c2p0 = r0.columns()[1].destination();

    ASSERT_EQ(r1.arguments().size(), 1);
    EXPECT_EQ(r1.arguments()[0], scalar::variable_reference { c1p0 });

    ASSERT_EQ(r1.columns().size(), 1);
    auto&& r1c1 = r1.columns()[0];
    EXPECT_EQ(r1c1.position(), 1);

    ASSERT_EQ(r2.columns().size(), 2);
    EXPECT_EQ(r2.columns()[0], c2p0);
    EXPECT_EQ(r2.columns()[1], r1c1.variable());

    dump(result);
}

TEST_F(compiler_test, feat_cte) {
    /*
     * WITH q0 AS (TABLE t0)
     * TABLE q0
     * =>
     * r1: {
     *     r0:scan t0
     *     =>
     * }
     * r2: emit (c0)
     * =>
     * r0:scan t0
     * r2:emit c0'
     */
    relation::graph_type subgragh;
    relation::graph_type graph;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = subgragh.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
                    { bindings(t0c2), c2 },
            },
    });
    auto c4 = bindings.stream_variable("c4");
    auto& r1 = graph.insert(extension::relation::subquery {
            std::move(subgragh),
            {
                    { c0, c4 },
            },
    });
    auto&& r2 = graph.insert(relation::emit {
            c4,
    });
    r1.output() >> r2.input();

    auto result = compiler()(options(), std::move(graph));
    ASSERT_TRUE(result);

    auto&& c = downcast<statement::execute>(result.statement());

    /*
     * p0:
     *   r0:scan t0 -> (c0)
     *   r2:emit x0
     */
    ASSERT_EQ(c.execution_plan().size(), 1);
    auto&& p0 = find(c.execution_plan(), r0);

    ASSERT_EQ(p0.operators().size(), 2);
    ASSERT_TRUE(p0.operators().contains(r0));
    ASSERT_TRUE(p0.operators().contains(r2));

    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0].source(), bindings(t0c0));
    auto&& c1p0 = r0.columns()[0].destination();

    ASSERT_EQ(r2.columns().size(), 1);
    EXPECT_EQ(r2.columns()[0], c1p0);

    dump(result);
}

TEST_F(compiler_test, fix_external_variable_inlining) {
    /*
     * SELECT c0, :q0 FROM t0
     * =>
     * r0:scan t0 (c0, c1, c2)
     * r1:project (x0=c0, x1=:q0)
     * r2:emit (x0, x1)
     * =>
     * r0:scan t0 (c0)
     * r1:project (x1=:q0)
     * r2:emit (c0, x1)
     */
    relation::graph_type graph;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = graph.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
                    { bindings(t0c2), c2 },
            },
    });
    auto q0 = bindings.external_variable(variable::declaration {
            "q0",
            t::int4 {},
    });
    auto x0 = bindings.stream_variable("x0");
    auto x1 = bindings.stream_variable("x1");
    auto& r1 = graph.insert(relation::project {
            {
                    relation::project::column {
                            x0,
                            scalar::variable_reference { c0 },
                    },
                    relation::project::column {
                            x1,
                            scalar::variable_reference { q0 },
                    },
            },
    });
    auto&& r2 = graph.insert(relation::emit {
            x0,
            x1,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    auto opts = options();
    opts.enable_external_variable_inlining() = false;
    auto result = compiler()(opts, std::move(graph));
    ASSERT_TRUE(result);

    auto&& c = downcast<statement::execute>(result.statement());

    /*
     * p0:
     *   r0:scan t0 (c0)
     *   r1:project (x1=:q0)
     *   r2:emit c0, x1
     */
    ASSERT_EQ(c.execution_plan().size(), 1);
    auto&& p0 = find(c.execution_plan(), r0);

    ASSERT_EQ(p0.operators().size(), 3);
    ASSERT_TRUE(p0.operators().contains(r0));
    ASSERT_TRUE(p0.operators().contains(r1));
    ASSERT_TRUE(p0.operators().contains(r2));

    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0].source(), bindings(t0c0));
    auto&& c1p0 = r0.columns()[0].destination();

    ASSERT_EQ(r1.columns().size(), 1);
    auto&& x1p0 = r1.columns()[0].variable();
    EXPECT_EQ(r1.columns()[0].value(), scalar::variable_reference { q0 });

    ASSERT_EQ(r2.columns().size(), 2);
    EXPECT_EQ(r2.columns()[0], c1p0);
    EXPECT_EQ(r2.columns()[1], x1p0);

    dump(result);
}

TEST_F(compiler_test, feat_scalar_subquery_no_broadcast) {
    // disable broadcast joins
    runtime_features.erase(runtime_feature::broadcast_exchange);

    /*
     * SELECT
     *   (VALUES(1) AS rv(cv)),
     *   t0.c0
     * FROM t0
     * =>
     * scan[t0(c0, c1, c2)] --\
     *                         join -- project[cv->c3] -- emit[c3, c0]
     * values[->cv] ----------/
     * =>
     * p0:
     *   scan[t0(c0)]:rs -- offer:o0
     * p1:
     *   values[->cv]:rv -- offer:o1
     * p2:
     *   take_cogroup:t0 -- join_group:j0 -- emit[cv, c0]:e0
     * plan:
     *   p0 -- group:g0 --\
     *                     p2
     *   p1 -- group:g0 --/
     */
    relation::graph_type subgraph;
    auto cv = bindings.stream_variable("cv");
    auto&& rv = subgraph.insert(relation::values {
            {
                    cv,
            },
            {
                    { constant(1) },
            },
    });

    relation::graph_type graph;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = graph.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
                    { bindings(t0c2), c2 },
            },
    });
    auto c3 = bindings.stream_variable("c3");
    auto&& r1 = graph.insert(relation::project {
            {
                    relation::project::column {
                            extension::scalar::subquery {
                                    std::move(subgraph),
                                    cv,
                            },
                            c3,
                    },
            },
    });
    auto&& r2 = graph.insert(relation::emit {
            c3,
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    auto result = compiler()(options(), std::move(graph));
    ASSERT_TRUE(result);

    auto&& c = downcast<statement::execute>(result.statement());

    ASSERT_EQ(c.execution_plan().size(), 5); // 3-processes and 2-exchanges
    auto&& p0 = find(c.execution_plan(), r0);
    auto&& p1 = find(c.execution_plan(), rv);
    auto&& p2 = find(c.execution_plan(), r2);
    ASSERT_EQ(p0.downstreams().size(), 1);
    ASSERT_EQ(p1.downstreams().size(), 1);
    ASSERT_EQ(p2.upstreams().size(), 2);

    // p0
    ASSERT_EQ(p0.operators().size(), 2);
    auto&& o0 = next<relation::step::offer>(r0.output());
    auto&& g0 = resolve<plan::group>(o0.destination());
    ASSERT_EQ(g0.group_keys().size(), 0);

    // p1
    ASSERT_EQ(p1.operators().size(), 2);
    auto&& o1 = next<relation::step::offer>(rv.output());
    auto&& g1 = resolve<plan::group>(o1.destination());
    ASSERT_EQ(g1.group_keys().size(), 0);

    // p2
    ASSERT_EQ(p2.operators().size(), 3);
    auto&& j0 = next<relation::step::join>(r2.input());
    auto&& t0 = next<relation::step::take_cogroup>(j0.input());

    ASSERT_EQ(t0.groups().size(), 2);
    auto&& t0g0 = t0.groups()[0];
    auto&& t0g1 = t0.groups()[1];
    EXPECT_EQ(resolve<plan::group>(t0g0.source()), g0);
    EXPECT_EQ(resolve<plan::group>(t0g1.source()), g1);

    ASSERT_EQ(t0g0.columns().size(), 1);
    auto&& c0m = t0g0.columns()[0].destination();

    ASSERT_EQ(t0g1.columns().size(), 1);
    auto&& cvm = t0g1.columns()[0].destination();

    EXPECT_EQ(j0.operator_kind(), relation::join_kind::left_outer_at_most_one);
    EXPECT_FALSE(j0.condition());

    ASSERT_EQ(r2.columns().size(), 2);
    EXPECT_EQ(r2.columns()[0], cvm);
    EXPECT_EQ(r2.columns()[1], c0m);

    dump(result);
}

TEST_F(compiler_test, feat_exists_filter_no_broadcast) {
    // disable broadcast joins
    runtime_features.erase(runtime_feature::broadcast_exchange);

    /*
     * SELECT
     *   t0.c0
     * FROM t0
     * WHERE EXISTS (VALUES (1) AS rv(cv))
     * =>
     * scan[t0(c0, c1, c2)] --\
     *                         join -- emit[c0]
     * values[->cv] ----------/
     * =>
     * p0:
     *   scan[t0(c0)]:rs -- offer:o0
     * p1:
     *   values[]:rv -- offer:o1
     * p2:
     *   take_cogroup:t0 -- join_group:j0 -- emit[c0]:e0
     * plan:
     *   p0 -- group:g0 --\
     *                     p2
     *   p1 -- group:g0 --/
     */
    relation::graph_type subgraph;
    auto cv = bindings.stream_variable("cv");
    auto&& rv = subgraph.insert(relation::values {
            {
                    cv,
            },
            {
                    { constant(1) },
            },
    });

    relation::graph_type graph;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = graph.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
                    { bindings(t0c2), c2 },
            },
    });
    auto&& r1 = graph.insert(relation::filter {
            extension::scalar::exists { std::move(subgraph) },
    });
    auto&& r2 = graph.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    auto result = compiler()(options(), std::move(graph));
    ASSERT_TRUE(result);

    auto&& c = downcast<statement::execute>(result.statement());

    ASSERT_EQ(c.execution_plan().size(), 5); // 3-processes and 2-exchanges
    auto&& p0 = find(c.execution_plan(), r0);
    auto&& p1 = find(c.execution_plan(), rv);
    auto&& p2 = find(c.execution_plan(), r2);
    ASSERT_EQ(p0.downstreams().size(), 1);
    ASSERT_EQ(p1.downstreams().size(), 1);
    ASSERT_EQ(p2.upstreams().size(), 2);

    // p0
    ASSERT_EQ(p0.operators().size(), 2);
    auto&& o0 = next<relation::step::offer>(r0.output());
    auto&& g0 = resolve<plan::group>(o0.destination());
    ASSERT_EQ(g0.group_keys().size(), 0);

    // p1
    ASSERT_EQ(p1.operators().size(), 2);
    auto&& o1 = next<relation::step::offer>(rv.output());
    auto&& g1 = resolve<plan::group>(o1.destination());
    ASSERT_EQ(g1.group_keys().size(), 0);

    // p2
    ASSERT_EQ(p2.operators().size(), 3);
    auto&& j0 = next<relation::step::join>(r2.input());
    auto&& t0 = next<relation::step::take_cogroup>(j0.input());

    ASSERT_EQ(t0.groups().size(), 2);
    auto&& t0g0 = t0.groups()[0];
    auto&& t0g1 = t0.groups()[1];
    EXPECT_EQ(resolve<plan::group>(t0g0.source()), g0);
    EXPECT_EQ(resolve<plan::group>(t0g1.source()), g1);

    ASSERT_EQ(t0g0.columns().size(), 1);
    auto&& c0m = t0g0.columns()[0].destination();

    ASSERT_EQ(t0g1.columns().size(), 0);

    EXPECT_EQ(j0.operator_kind(), relation::join_kind::semi);
    EXPECT_FALSE(j0.condition());

    ASSERT_EQ(r2.columns().size(), 1);
    EXPECT_EQ(r2.columns()[0], c0m);

    dump(result);
}

TEST_F(compiler_test, feat_quantified_compare_filter) {
    /*
     * SELECT
     *   t0.c1
     * FROM t0
     * WHERE t0.c0 IN (VALUES (1) AS rv(cv))
     * =>
     * scan[t0(c0, c1, c2)] --\
     *                         join -- emit[c0]
     * values[->cv] ----------/
     * =>
     * p0:
     *   scan[t0(c0, c1)]:rs -- offer[c0, c1]:o0
     * p1:
     *   values[cv]:rv -- offer[cv]:o1
     * p2:
     *   take_cogroup:t0 -- join_group[]:j0 -- emit[c0]:e0
     * plan:
     *   p0 -- group[c0]:g0 --\
     *                         p2
     *   p1 -- group[cv]:g0 --/
     */
    relation::graph_type subgraph;
    auto cv = bindings.stream_variable("cv");
    auto&& rv = subgraph.insert(relation::values {
            {
                    cv,
            },
            {
                    { constant(1) },
            },
    });

    relation::graph_type graph;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = graph.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
                    { bindings(t0c2), c2 },
            },
    });
    auto&& r1 = graph.insert(relation::filter {
            extension::scalar::quantified_compare {
                    scalar::comparison_operator::equal,
                    scalar::quantifier::any,
                    scalar::variable_reference { c0 },
                    std::move(subgraph),
                    cv,
            },
    });
    auto&& r2 = graph.insert(relation::emit {
            c1,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    auto result = compiler()(options(), std::move(graph));
    ASSERT_TRUE(result);

    auto&& c = downcast<statement::execute>(result.statement());

    ASSERT_EQ(c.execution_plan().size(), 5); // 3-processes and 2-exchanges
    auto&& p0 = find(c.execution_plan(), r0);
    auto&& p1 = find(c.execution_plan(), rv);
    auto&& p2 = find(c.execution_plan(), r2);
    ASSERT_EQ(p0.downstreams().size(), 1);
    ASSERT_EQ(p1.downstreams().size(), 1);
    ASSERT_EQ(p2.upstreams().size(), 2);

    // p0
    ASSERT_EQ(p0.operators().size(), 2);
    auto&& o0 = next<relation::step::offer>(r0.output());
    auto&& g0 = resolve<plan::group>(o0.destination());

    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), bindings(t0c0));
    EXPECT_EQ(r0.columns()[1].source(), bindings(t0c1));
    auto&& p0c0 = r0.columns()[0].destination();
    auto&& p0c1 = r0.columns()[1].destination();

    ASSERT_EQ(o0.columns().size(), 2);
    auto&& g0c0 = find_destination(p0c0, o0.columns());
    auto&& g0c1 = find_destination(p0c1, o0.columns());

    ASSERT_EQ(g0.group_keys().size(), 1);
    EXPECT_EQ(g0.group_keys()[0], g0c0);

    ASSERT_EQ(g0.columns().size(), 1);
    EXPECT_EQ(g0.columns()[0], g0c1);

    // p1
    ASSERT_EQ(p1.operators().size(), 2);
    auto&& o1 = next<relation::step::offer>(rv.output());
    auto&& g1 = resolve<plan::group>(o1.destination());

    ASSERT_EQ(rv.columns().size(), 1);
    auto&& p1cv = rv.columns()[0];

    ASSERT_EQ(o1.columns().size(), 1);
    auto&& g1cv = find_destination(p1cv, o1.columns());

    ASSERT_EQ(g1.group_keys().size(), 1);
    EXPECT_EQ(g1.group_keys()[0], g1cv);

    ASSERT_EQ(g1.columns().size(), 0);

    // p2
    ASSERT_EQ(p2.operators().size(), 3);
    auto&& j0 = next<relation::step::join>(r2.input());
    auto&& t0 = next<relation::step::take_cogroup>(j0.input());

    ASSERT_EQ(t0.groups().size(), 2);
    auto&& t0g0 = t0.groups()[0];
    auto&& t0g1 = t0.groups()[1];
    EXPECT_EQ(resolve<plan::group>(t0g0.source()), g0);
    EXPECT_EQ(resolve<plan::group>(t0g1.source()), g1);

    ASSERT_EQ(t0g0.columns().size(), 1);
    auto&& p2c1 = find_destination(g0c1, t0g0.columns());

    ASSERT_EQ(t0g1.columns().size(), 0);

    EXPECT_EQ(j0.operator_kind(), relation::join_kind::semi);
    EXPECT_FALSE(j0.condition());

    ASSERT_EQ(r2.columns().size(), 1);
    EXPECT_EQ(r2.columns()[0], p2c1);

    dump(result);
}

} // namespace yugawara

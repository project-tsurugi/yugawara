#include <yugawara/remote/details/element_location.h>

#include <gtest/gtest.h>

#include <takatori/scalar/binary.h>
#include <takatori/scalar/compare.h>
#include <takatori/scalar/function_call.h>
#include <takatori/scalar/variable_reference.h>

#include <takatori/relation/scan.h>
#include <takatori/relation/find.h>
#include <takatori/relation/join_find.h>
#include <takatori/relation/join_scan.h>
#include <takatori/relation/write.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/values.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/step/offer.h>
#include <takatori/relation/step/take_flat.h>
#include <takatori/relation/step/take_group.h>
#include <takatori/relation/step/take_cogroup.h>

#include <takatori/plan/process.h>
#include <takatori/plan/forward.h>
#include <takatori/plan/broadcast.h>
#include <takatori/plan/group.h>

#include <takatori/statement/execute.h>
#include <takatori/statement/write.h>
#include <takatori/statement/drop_table.h>
#include <takatori/statement/grant_table.h>
#include <takatori/statement/create_foreign_catalog.h>

#include <yugawara/storage/configurable_provider.h>
#include <yugawara/binding/factory.h>
#include <yugawara/function/declaration.h>
#include <yugawara/variable/declaration.h>

#include <yugawara/testing/utils.h>
#include <yugawara/testing/mock_symbol_location_query.h>

namespace yugawara::remote::details {

// import test utils
using namespace ::yugawara::testing;

namespace tstatement = ::takatori::statement;

using location_id_set_type = element_location::location_id_set_type;

class element_location_test : public ::testing::Test {
protected:
    binding::factory factory;

    std::shared_ptr<storage::configurable_provider> storages = std::make_shared<storage::configurable_provider>();

    std::shared_ptr<storage::table> local_table = storages->add_table({
            "L",
            {
                    { "C0", t::int4() },
            },
    });
    std::shared_ptr<storage::index> local_index = storages->add_index({ local_table, "L", });

    std::shared_ptr<storage::table> remote_table = storages->add_table({
            "R",
            {
                    { "C0", t::int4() },
            },
    });
    std::shared_ptr<storage::index> remote_index = storages->add_index({ remote_table, "R", });

    std::shared_ptr<storage::table> other_remote_table = storages->add_table({
            "O",
            {
                    { "C0", t::int4() },
            },
    });

    std::shared_ptr<storage::table> shared_table = storages->add_table({
            "S",
            {
                    { "C0", t::int4() },
            },
    });
    std::shared_ptr<storage::index> shared_index = storages->add_index({ shared_table, "S", });

    mock_symbol_location_query query {};
    remote::location_options options = location_options_of(query);

    // NOTE: element_location keeps a copy of the options, so that this creates it for each call
    template<class T>
    location_id_set_type locations(T const& element) const {
        return element_location { options }(element);
    }

    void SetUp() override {
        query.all = { 0, 7, 8 };
        query.tables.emplace(local_table.get(), location_id_set_type { 0 });
        query.tables.emplace(remote_table.get(), location_id_set_type { 7 });
        query.tables.emplace(other_remote_table.get(), location_id_set_type { 8 });
        query.tables.emplace(shared_table.get(), location_id_set_type { 0, 7 });
    }

    relation::scan make_scan(storage::index const& index) {
        return relation::scan { factory(index), {} };
    }

    // scan -> offer
    plan::process& add_scan_offer(plan::graph_type& graph, storage::index const& index, plan::exchange const& destination) {
        auto&& process = graph.insert(plan::process {});
        auto&& scan = process.operators().insert(make_scan(index));
        auto&& offer = process.operators().insert(relation::step::offer { factory(destination), {} });
        scan.output() >> offer.input();
        return process;
    }

    // scan -> emit
    plan::process& add_scan_emit(plan::graph_type& graph, storage::index const& index) {
        auto&& process = graph.insert(plan::process {});
        auto&& scan = process.operators().insert(make_scan(index));
        auto&& emit = process.operators().insert(relation::emit { std::vector<relation::emit::column> {} });
        scan.output() >> emit.input();
        return process;
    }
};

TEST_F(element_location_test, scan) {
    EXPECT_EQ(locations(make_scan(*remote_index)), (location_id_set_type { 7 }));
}

TEST_F(element_location_test, scan_local) {
    EXPECT_EQ(locations(make_scan(*local_index)), (location_id_set_type { 0 }));
}

TEST_F(element_location_test, scan_multiple_locations) {
    EXPECT_EQ(locations(make_scan(*shared_index)), (location_id_set_type { 0, 7 }));
}

TEST_F(element_location_test, scan_keys) {
    auto func = std::make_shared<function::declaration>(function::declaration {
            function::declaration::minimum_builtin_function_id + 1,
            "f",
            ::takatori::type::int4 {},
            {},
    });
    query.functions.emplace(func.get(), location_id_set_type { 0, 7 });
    relation::scan expr {
            factory(*remote_index),
            {},
            relation::scan::endpoint {
                    relation::scan::key { factory(remote_table->columns()[0]), constant() },
                    relation::endpoint_kind::inclusive,
            },
            relation::scan::endpoint {
                    relation::scan::key {
                            factory(remote_table->columns()[0]),
                            scalar::function_call { factory(func), {} },
                    },
                    relation::endpoint_kind::inclusive,
            },
    };
    EXPECT_EQ(locations(expr), (location_id_set_type { 7 }));

    query.functions[func.get()] = location_id_set_type { 0 };
    EXPECT_EQ(locations(expr), (location_id_set_type {}));
}

TEST_F(element_location_test, scan_row_slice) {
    auto decl = std::make_shared<variable::declaration>(variable::declaration {
            "p",
            ::takatori::type::int8 {},
    });
    query.variables.emplace(decl.get(), location_id_set_type { 0, 7 });
    relation::scan expr {
            factory(*remote_index),
            {},
            {},
            {},
            {},
            relation::scan::row_slice_type { {}, factory.external_variable(decl) },
    };
    EXPECT_EQ(locations(expr), (location_id_set_type { 7 }));

    query.variables[decl.get()] = location_id_set_type { 0 };
    EXPECT_EQ(locations(expr), (location_id_set_type {}));
}

TEST_F(element_location_test, find) {
    relation::find expr { factory(*remote_index), {}, {} };
    EXPECT_EQ(locations(expr), (location_id_set_type { 7 }));
}

TEST_F(element_location_test, find_keys) {
    auto decl = std::make_shared<variable::declaration>(variable::declaration {
            "p",
            ::takatori::type::int4 {},
    });
    query.variables.emplace(decl.get(), location_id_set_type { 0, 7 });
    relation::find expr {
            factory(*remote_index),
            {},
            {
                    relation::find::key {
                            factory(remote_table->columns()[0]),
                            scalar::variable_reference { factory.external_variable(decl) },
                    },
            },
    };
    EXPECT_EQ(locations(expr), (location_id_set_type { 7 }));

    query.variables[decl.get()] = location_id_set_type { 0 };
    EXPECT_EQ(locations(expr), (location_id_set_type {}));
}

TEST_F(element_location_test, join_find) {
    relation::join_find expr { relation::join_kind::inner, factory(*remote_index), {}, {} };
    EXPECT_EQ(locations(expr), (location_id_set_type { 7 }));
}

TEST_F(element_location_test, join_find_exchange) {
    plan::broadcast exchange {};
    relation::join_find expr { relation::join_kind::inner, factory(exchange), {}, {} };
    EXPECT_EQ(locations(expr), (location_id_set_type { 0 }));
}

TEST_F(element_location_test, join_scan) {
    relation::join_scan expr { relation::join_kind::inner, factory(*remote_index), {}, {}, {} };
    EXPECT_EQ(locations(expr), (location_id_set_type { 7 }));
}

TEST_F(element_location_test, join_scan_exchange) {
    plan::broadcast exchange {};
    relation::join_scan expr { relation::join_kind::inner, factory(exchange), {}, {}, {} };
    EXPECT_EQ(locations(expr), (location_id_set_type { 0 }));
}

TEST_F(element_location_test, write) {
    relation::write expr { relation::write_kind::insert, factory(*remote_index), {}, {} };
    EXPECT_EQ(locations(expr), (location_id_set_type { 7 }));
}

TEST_F(element_location_test, offer) {
    plan::forward exchange {};
    relation::step::offer expr { factory(exchange), {} };
    EXPECT_EQ(locations(expr), (location_id_set_type { 0, 7, 8 }));
}

TEST_F(element_location_test, take_flat) {
    plan::forward exchange {};
    relation::step::take_flat expr { factory(exchange), {} };
    EXPECT_EQ(locations(expr), (location_id_set_type { 0, 7, 8 }));
}

TEST_F(element_location_test, take_group) {
    plan::group exchange {};
    relation::step::take_group expr { factory(exchange), {} };
    EXPECT_EQ(locations(expr), (location_id_set_type { 0, 7, 8 }));
}

TEST_F(element_location_test, take_cogroup) {
    plan::group exchange {};
    relation::step::take_cogroup expr {
            { factory(exchange) },
    };
    EXPECT_EQ(locations(expr), (location_id_set_type { 0, 7, 8 }));
}

TEST_F(element_location_test, process_take_write) {
    plan::graph_type graph {};
    auto&& exchange = graph.insert(plan::forward {});
    auto&& process = graph.insert(plan::process {});
    auto&& take = process.operators().insert(relation::step::take_flat { factory(exchange), {} });
    auto&& write = process.operators().insert(relation::write { relation::write_kind::insert, factory(*remote_index), {}, {} });
    take.output() >> write.input();
    EXPECT_EQ(locations(process), (location_id_set_type { 7 }));
}

TEST_F(element_location_test, scan_kind_not_remote) {
    options.remote_relation_expression_kinds().erase(relation::expression_kind::scan);
    EXPECT_EQ(locations(make_scan(*remote_index)), (location_id_set_type {}));
    EXPECT_EQ(locations(make_scan(*shared_index)), (location_id_set_type { 0 }));
    EXPECT_EQ(locations(make_scan(*local_index)), (location_id_set_type { 0 }));
}

TEST_F(element_location_test, write_kind_not_remote) {
    options.remote_relation_expression_kinds().erase(relation::expression_kind::write);
    relation::write expr { relation::write_kind::insert, factory(*remote_index), {}, {} };
    EXPECT_EQ(locations(expr), (location_id_set_type {}));
}

TEST_F(element_location_test, offer_kind_not_remote) {
    options.remote_relation_expression_kinds().erase(relation::expression_kind::offer);
    plan::forward exchange {};
    relation::step::offer expr { factory(exchange), {} };
    EXPECT_EQ(locations(expr), (location_id_set_type { 0 }));
}

TEST_F(element_location_test, take_flat_kind_not_remote) {
    options.remote_relation_expression_kinds().erase(relation::expression_kind::take_flat);
    plan::forward exchange {};
    relation::step::take_flat expr { factory(exchange), {} };
    EXPECT_EQ(locations(expr), (location_id_set_type { 0 }));
}

TEST_F(element_location_test, process_kind_not_remote) {
    options.remote_relation_expression_kinds().erase(relation::expression_kind::offer);
    plan::graph_type graph {};
    auto&& exchange = graph.insert(plan::forward {});
    auto&& process = add_scan_offer(graph, *remote_index, exchange);
    EXPECT_EQ(locations(process), (location_id_set_type {}));
}

TEST_F(element_location_test, scalar_immediate) {
    EXPECT_EQ(locations(constant()), (location_id_set_type { 0, 7, 8 }));
}

TEST_F(element_location_test, scalar_stream_variable) {
    scalar::variable_reference expr { factory.stream_variable("x") };
    EXPECT_EQ(locations(expr), (location_id_set_type { 0, 7, 8 }));
}

TEST_F(element_location_test, scalar_kind_not_remote) {
    options.remote_scalar_expression_kinds().erase(scalar::expression_kind::compare);
    scalar::binary expr {
            scalar::binary_operator::conditional_and,
            scalar::compare { scalar::comparison_operator::equal, constant(1), constant(2) },
            scalar::compare { scalar::comparison_operator::equal, constant(3), constant(4) },
    };
    EXPECT_EQ(locations(expr), (location_id_set_type { 0 }));
}

TEST_F(element_location_test, scalar_function_call) {
    auto func = std::make_shared<function::declaration>(function::declaration {
            function::declaration::minimum_builtin_function_id + 1,
            "f",
            ::takatori::type::int4 {},
            {},
    });
    query.functions.emplace(func.get(), location_id_set_type { 0, 7 });
    scalar::function_call expr { factory(func), {} };
    EXPECT_EQ(locations(expr), (location_id_set_type { 0, 7 }));
}

TEST_F(element_location_test, scalar_external_variable) {
    auto decl = std::make_shared<variable::declaration>(variable::declaration {
            "p",
            ::takatori::type::int4 {},
    });
    query.variables.emplace(decl.get(), location_id_set_type { 0 });
    scalar::variable_reference expr { factory.external_variable(decl) };
    EXPECT_EQ(locations(expr), (location_id_set_type { 0 }));
}

TEST_F(element_location_test, scalar_intersection) {
    auto func = std::make_shared<function::declaration>(function::declaration {
            function::declaration::minimum_builtin_function_id + 1,
            "f",
            ::takatori::type::int4 {},
            {},
    });
    query.functions.emplace(func.get(), location_id_set_type { 0, 7 });
    auto decl = std::make_shared<variable::declaration>(variable::declaration {
            "p",
            ::takatori::type::int4 {},
    });
    query.variables.emplace(decl.get(), location_id_set_type { 7, 8 });
    scalar::compare expr {
            scalar::comparison_operator::equal,
            scalar::function_call { factory(func), {} },
            scalar::variable_reference { factory.external_variable(decl) },
    };
    EXPECT_EQ(locations(expr), (location_id_set_type { 7 }));
}

TEST_F(element_location_test, filter) {
    relation::filter expr { constant() };
    EXPECT_EQ(locations(expr), (location_id_set_type { 0, 7, 8 }));
}

TEST_F(element_location_test, filter_condition) {
    auto func = std::make_shared<function::declaration>(function::declaration {
            function::declaration::minimum_builtin_function_id + 1,
            "f",
            ::takatori::type::boolean {},
            {},
    });
    query.functions.emplace(func.get(), location_id_set_type { 0, 7 });
    relation::filter expr { scalar::function_call { factory(func), {} } };
    EXPECT_EQ(locations(expr), (location_id_set_type { 0, 7 }));
}

TEST_F(element_location_test, filter_kind_not_remote) {
    options.remote_relation_expression_kinds().erase(relation::expression_kind::filter);
    relation::filter expr { constant() };
    EXPECT_EQ(locations(expr), (location_id_set_type { 0 }));
}

TEST_F(element_location_test, filter_condition_kind_not_remote) {
    options.remote_scalar_expression_kinds().erase(scalar::expression_kind::immediate);
    relation::filter expr { constant() };
    EXPECT_EQ(locations(expr), (location_id_set_type { 0 }));
}

TEST_F(element_location_test, other_operator) {
    relation::values expr { {}, {} };
    EXPECT_EQ(locations(expr), (location_id_set_type { 0 }));
}

TEST_F(element_location_test, emit) {
    relation::emit expr { std::vector<relation::emit::column> {} };
    EXPECT_EQ(locations(expr), (location_id_set_type { 0 }));
}

TEST_F(element_location_test, exchange) {
    plan::forward exchange {};
    EXPECT_EQ(locations(exchange), (location_id_set_type { 0 }));
}

TEST_F(element_location_test, process) {
    plan::graph_type graph {};
    auto&& exchange = graph.insert(plan::forward {});
    auto&& process = add_scan_offer(graph, *remote_index, exchange);
    EXPECT_EQ(locations(process), (location_id_set_type { 7 }));
}

TEST_F(element_location_test, process_local) {
    plan::graph_type graph {};
    auto&& process = add_scan_emit(graph, *shared_index);
    EXPECT_EQ(locations(process), (location_id_set_type { 0 }));
}

TEST_F(element_location_test, process_empty) {
    plan::graph_type graph {};
    auto&& process = add_scan_emit(graph, *remote_index);
    EXPECT_EQ(locations(process), (location_id_set_type {}));
}

TEST_F(element_location_test, step) {
    plan::graph_type graph {};
    auto&& exchange = graph.insert(plan::forward {});
    auto&& process = add_scan_offer(graph, *remote_index, exchange);
    EXPECT_EQ(locations(static_cast<plan::step const&>(process)), (location_id_set_type { 7 }));
    EXPECT_EQ(locations(static_cast<plan::step const&>(exchange)), (location_id_set_type { 0 }));
}

TEST_F(element_location_test, statement_execute) {
    tstatement::execute stmt { plan::graph_type {} };
    auto&& graph = stmt.execution_plan();
    auto&& exchange = graph.insert(plan::forward {});
    add_scan_offer(graph, *shared_index, exchange);
    EXPECT_EQ(locations(stmt), (location_id_set_type { 0 }));
}

TEST_F(element_location_test, statement_execute_remote) {
    tstatement::execute stmt { plan::graph_type {} };
    auto&& graph = stmt.execution_plan();
    auto&& exchange = graph.insert(plan::forward {});
    add_scan_offer(graph, *remote_index, exchange);
    EXPECT_EQ(locations(stmt), (location_id_set_type {}));
}

TEST_F(element_location_test, statement_write) {
    tstatement::write stmt {
            tstatement::write_kind::insert,
            factory(*remote_index),
            {},
            {},
    };
    EXPECT_EQ(locations(stmt), (location_id_set_type { 7 }));
}

TEST_F(element_location_test, statement_drop_table) {
    auto schema = std::make_shared<schema::declaration>("s");
    tstatement::drop_table stmt {
            factory(schema),
            factory(remote_table),
    };
    EXPECT_EQ(locations(stmt), (location_id_set_type { 7 }));
}

TEST_F(element_location_test, statement_multiple_tables) {
    tstatement::grant_table stmt {
            {
                    { factory(shared_table), {}, {} },
                    { factory(remote_table), {}, {} },
            },
    };
    EXPECT_EQ(locations(stmt), (location_id_set_type { 7 }));
}

TEST_F(element_location_test, statement_multiple_tables_disjoint) {
    tstatement::grant_table stmt {
            {
                    { factory(remote_table), {}, {} },
                    { factory(other_remote_table), {}, {} },
            },
    };
    EXPECT_EQ(locations(stmt), (location_id_set_type {}));
}

TEST_F(element_location_test, statement_without_tables) {
    tstatement::create_foreign_catalog stmt { "x", "y" };
    EXPECT_EQ(locations(stmt), (location_id_set_type { 0 }));
}

} // namespace yugawara::remote::details

#include <yugawara/details/compute_operation_locations.h>

#include <algorithm>
#include <iterator>

#include <gtest/gtest.h>

#include <takatori/relation/scan.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/step/offer.h>
#include <takatori/relation/step/take_flat.h>

#include <takatori/plan/process.h>
#include <takatori/plan/forward.h>

#include <takatori/statement/execute.h>
#include <takatori/statement/write.h>
#include <takatori/statement/grant_table.h>

#include <yugawara/storage/configurable_provider.h>
#include <yugawara/binding/factory.h>

#include <yugawara/testing/utils.h>
#include <yugawara/testing/mock_symbol_location_query.h>

namespace yugawara::details {

// import test utils
using namespace ::yugawara::testing;

namespace tstatement = ::takatori::statement;

using location_id_set_type = remote::location_info::location_id_set_type;

class compute_operation_locations_test : public ::testing::Test {
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

    void SetUp() override {
        query.all = { 0, 7, 8 };
        query.tables.emplace(local_table.get(), location_id_set_type { 0 });
        query.tables.emplace(remote_table.get(), location_id_set_type { 7 });
        query.tables.emplace(other_remote_table.get(), location_id_set_type { 8 });
        query.tables.emplace(shared_table.get(), location_id_set_type { 0, 7 });
    }

    // scan -> offer
    plan::process& add_scan_offer(plan::graph_type& graph, storage::index const& index, plan::exchange const& destination) {
        auto&& process = graph.insert(plan::process {});
        auto&& scan = process.operators().insert(relation::scan { factory(index), {} });
        auto&& offer = process.operators().insert(relation::step::offer { factory(destination), {} });
        scan.output() >> offer.input();
        return process;
    }

    // scan -> emit
    plan::process& add_scan_emit(plan::graph_type& graph, storage::index const& index) {
        auto&& process = graph.insert(plan::process {});
        auto&& scan = process.operators().insert(relation::scan { factory(index), {} });
        auto&& emit = process.operators().insert(relation::emit { std::vector<relation::emit::column> {} });
        scan.output() >> emit.input();
        return process;
    }

    // take_flat -> emit
    plan::process& add_take_emit(plan::graph_type& graph, plan::exchange const& source) {
        auto&& process = graph.insert(plan::process {});
        auto&& take = process.operators().insert(relation::step::take_flat { factory(source), {} });
        auto&& emit = process.operators().insert(relation::emit { std::vector<relation::emit::column> {} });
        take.output() >> emit.input();
        return process;
    }

    static bool contains_local(location_id_set_type const& locations) {
        return locations.find(remote::location_info::local_location_id) != locations.end();
    }
};

TEST_F(compute_operation_locations_test, remote_scan_offer) {
    tstatement::execute stmt { plan::graph_type {} };
    auto&& graph = stmt.execution_plan();
    auto&& exchange = graph.insert(plan::forward {});
    auto&& source = add_scan_offer(graph, *remote_index, exchange);
    auto&& sink = add_take_emit(graph, exchange);

    auto result = compute_operation_locations(stmt, query);
    ASSERT_TRUE(result);
    auto&& locations = **result;
    EXPECT_EQ(locations.find(source), (location_id_set_type { 7 }));
    EXPECT_TRUE(contains_local(locations.find(exchange)));
    EXPECT_TRUE(contains_local(locations.find(sink)));
}

TEST_F(compute_operation_locations_test, local_process) {
    tstatement::execute stmt { plan::graph_type {} };
    auto&& process = add_scan_emit(stmt.execution_plan(), *local_index);

    auto result = compute_operation_locations(stmt, query);
    ASSERT_TRUE(result);
    EXPECT_TRUE(contains_local((*result)->find(process)));
}

TEST_F(compute_operation_locations_test, shared_table_process) {
    tstatement::execute stmt { plan::graph_type {} };
    auto&& graph = stmt.execution_plan();
    auto&& exchange = graph.insert(plan::forward {});
    auto&& source = add_scan_offer(graph, *shared_index, exchange);
    add_take_emit(graph, exchange);

    auto result = compute_operation_locations(stmt, query);
    ASSERT_TRUE(result);
    EXPECT_TRUE(contains_local((*result)->find(source)));
}

TEST_F(compute_operation_locations_test, empty_process) {
    tstatement::execute stmt { plan::graph_type {} };
    add_scan_emit(stmt.execution_plan(), *remote_index);

    auto result = compute_operation_locations(stmt, query);
    ASSERT_FALSE(result);
    auto&& diagnostics = result.error();
    ASSERT_EQ(diagnostics.size(), 1);
    EXPECT_EQ(diagnostics[0].code(), compiler_code::empty_operation_location);
}

TEST_F(compute_operation_locations_test, empty_process_allowed) {
    tstatement::execute stmt { plan::graph_type {} };
    auto&& process = add_scan_emit(stmt.execution_plan(), *remote_index);

    auto result = compute_operation_locations(stmt, query, true);
    ASSERT_TRUE(result);
    EXPECT_EQ((*result)->find(process), (location_id_set_type {}));
}

TEST_F(compute_operation_locations_test, statement_execute) {
    tstatement::execute stmt { plan::graph_type {} };
    auto&& graph = stmt.execution_plan();
    auto&& exchange = graph.insert(plan::forward {});
    add_scan_offer(graph, *remote_index, exchange);
    add_take_emit(graph, exchange);

    auto result = compute_operation_locations(stmt, query);
    ASSERT_TRUE(result);
    auto&& locations = **result;

    location_id_set_type expected = query.all;
    for (auto&& step : graph) {
        auto step_locations = locations.find(step);
        location_id_set_type intersection {};
        std::set_intersection(
                expected.begin(), expected.end(),
                step_locations.begin(), step_locations.end(),
                std::inserter(intersection, intersection.end()));
        expected = std::move(intersection);
    }
    EXPECT_EQ(locations.find(stmt), expected);
}

TEST_F(compute_operation_locations_test, statement_execute_local) {
    tstatement::execute stmt { plan::graph_type {} };
    add_scan_emit(stmt.execution_plan(), *local_index);

    auto result = compute_operation_locations(stmt, query);
    ASSERT_TRUE(result);
    EXPECT_TRUE(contains_local((*result)->find(stmt)));
}

TEST_F(compute_operation_locations_test, statement_write) {
    tstatement::write stmt {
            tstatement::write_kind::insert,
            factory(*remote_index),
            {},
            {},
    };

    auto result = compute_operation_locations(stmt, query);
    ASSERT_TRUE(result);
    EXPECT_EQ((*result)->find(stmt), (location_id_set_type { 7 }));
}

TEST_F(compute_operation_locations_test, statement_empty) {
    tstatement::grant_table stmt {
            {
                    { factory(remote_table), {}, {} },
                    { factory(other_remote_table), {}, {} },
            },
    };

    auto result = compute_operation_locations(stmt, query);
    ASSERT_FALSE(result);
    auto&& diagnostics = result.error();
    ASSERT_EQ(diagnostics.size(), 1);
    EXPECT_EQ(diagnostics[0].code(), compiler_code::empty_operation_location);
}

TEST_F(compute_operation_locations_test, statement_empty_allowed) {
    tstatement::grant_table stmt {
            {
                    { factory(remote_table), {}, {} },
                    { factory(other_remote_table), {}, {} },
            },
    };

    auto result = compute_operation_locations(stmt, query, true);
    ASSERT_TRUE(result);
    EXPECT_EQ((*result)->find(stmt), (location_id_set_type {}));
}

TEST_F(compute_operation_locations_test, statement_execute_empty) {
    tstatement::execute stmt { plan::graph_type {} };
    auto&& graph = stmt.execution_plan();
    auto&& exchange = graph.insert(plan::forward {});
    add_scan_offer(graph, *remote_index, exchange);
    add_take_emit(graph, exchange);

    auto result = compute_operation_locations(stmt, query);
    ASSERT_TRUE(result);
    EXPECT_EQ((*result)->find(stmt), (location_id_set_type {}));
}

} // namespace yugawara::details

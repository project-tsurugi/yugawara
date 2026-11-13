#include <yugawara/details/decide_operator_locations.h>

#include <gtest/gtest.h>

#include <optional>

#include <takatori/relation/graph.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/values.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/write.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/intermediate/join.h>

#include <yugawara/storage/configurable_provider.h>
#include <yugawara/binding/factory.h>

#include <yugawara/testing/utils.h>
#include <yugawara/testing/mock_symbol_location_query.h>

namespace yugawara::details {

// import test utils
using namespace ::yugawara::testing;

using location_id_type = remote::location_info::location_id_type;
using location_id_set_type = remote::location_info::location_id_set_type;

class decide_operator_locations_test : public ::testing::Test {
protected:
    binding::factory factory;

    std::shared_ptr<storage::configurable_provider> storages = std::make_shared<storage::configurable_provider>();

    std::shared_ptr<storage::table> local_table = add_table("L");
    std::shared_ptr<storage::index> local_index = storages->add_index({ local_table, "L", });

    std::shared_ptr<storage::table> remote_table = add_table("R");
    std::shared_ptr<storage::index> remote_index = storages->add_index({ remote_table, "R", });

    std::shared_ptr<storage::table> other_remote_table = add_table("O");
    std::shared_ptr<storage::index> other_remote_index = storages->add_index({ other_remote_table, "O", });

    std::shared_ptr<storage::table> shared_table = add_table("S");
    std::shared_ptr<storage::index> shared_index = storages->add_index({ shared_table, "S", });

    std::shared_ptr<storage::table> multi_remote_table = add_table("M");
    std::shared_ptr<storage::index> multi_remote_index = storages->add_index({ multi_remote_table, "M", });

    std::shared_ptr<storage::table> nowhere_table = add_table("N");
    std::shared_ptr<storage::index> nowhere_index = storages->add_index({ nowhere_table, "N", });

    mock_symbol_location_query query {};

    void SetUp() override {
        query.all = { 0, 7, 8 };
        query.tables.emplace(local_table.get(), location_id_set_type { 0 });
        query.tables.emplace(remote_table.get(), location_id_set_type { 7 });
        query.tables.emplace(other_remote_table.get(), location_id_set_type { 8 });
        query.tables.emplace(shared_table.get(), location_id_set_type { 0, 7 });
        query.tables.emplace(multi_remote_table.get(), location_id_set_type { 7, 8 });
        query.tables.emplace(nowhere_table.get(), location_id_set_type {});
    }

    std::shared_ptr<storage::table> add_table(std::string_view name) {
        return storages->add_table({
                name,
                {
                        { "C0", t::int4() },
                },
        });
    }

    relation::scan& add_scan(relation::graph_type& graph, storage::index const& index) {
        return graph.insert(relation::scan {
                factory(index),
                {
                        { factory(index.table().columns()[0]), factory.stream_variable("c0") },
                },
        });
    }

    relation::emit& add_emit(relation::graph_type& graph) {
        return graph.insert(relation::emit { std::vector<relation::emit::column> {} });
    }

    relation::write& add_write(relation::graph_type& graph, storage::index const& index) {
        return graph.insert(relation::write { relation::write_kind::insert, factory(index), {}, {} });
    }

    static std::optional<location_id_type> location_of(operator_location_map const& locations, relation::expression const& expr) {
        auto it = locations.find(std::addressof(expr));
        if (it == locations.end()) {
            throw std::out_of_range("missing operator");
        }
        return it->second;
    }
};

TEST_F(decide_operator_locations_test, remote_scan) {
    // scan(R) - emit
    relation::graph_type graph {};
    auto&& scan = add_scan(graph, *remote_index);
    auto&& emit = add_emit(graph);
    scan.output() >> emit.input();

    auto locations = decide_operator_locations(graph, query);
    EXPECT_EQ(locations.size(), 2);
    EXPECT_EQ(location_of(locations, scan), 7);
    EXPECT_EQ(location_of(locations, emit), 0);
}

TEST_F(decide_operator_locations_test, local_scan) {
    // scan(L) - filter - emit
    relation::graph_type graph {};
    auto&& scan = add_scan(graph, *local_index);
    auto&& filter = graph.insert(relation::filter { constant() });
    auto&& emit = add_emit(graph);
    scan.output() >> filter.input();
    filter.output() >> emit.input();

    auto locations = decide_operator_locations(graph, query);
    EXPECT_EQ(location_of(locations, scan), 0);
    EXPECT_EQ(location_of(locations, filter), 0);
    EXPECT_EQ(location_of(locations, emit), 0);
}

TEST_F(decide_operator_locations_test, shared_scan) {
    // scan(S{0, 7}) - emit
    relation::graph_type graph {};
    auto&& scan = add_scan(graph, *shared_index);
    auto&& emit = add_emit(graph);
    scan.output() >> emit.input();

    auto locations = decide_operator_locations(graph, query);
    EXPECT_EQ(location_of(locations, scan), 0);
    EXPECT_EQ(location_of(locations, emit), 0);
}

TEST_F(decide_operator_locations_test, multi_remote_scan) {
    // scan(M{7, 8}) - emit
    relation::graph_type graph {};
    auto&& scan = add_scan(graph, *multi_remote_index);
    auto&& emit = add_emit(graph);
    scan.output() >> emit.input();

    auto locations = decide_operator_locations(graph, query);
    auto location = location_of(locations, scan);
    ASSERT_TRUE(location);
    EXPECT_TRUE(*location == 7 || *location == 8) << *location;
    EXPECT_EQ(location_of(locations, emit), 0);

    // stable
    auto again = decide_operator_locations(graph, query);
    EXPECT_EQ(location_of(again, scan), location);
}

TEST_F(decide_operator_locations_test, multi_remote_write_shared_with_upstream) {
    // scan(O{8}) - write(M{7, 8})
    relation::graph_type graph {};
    auto&& scan = add_scan(graph, *other_remote_index);
    auto&& write = add_write(graph, *multi_remote_index);
    scan.output() >> write.input();

    auto locations = decide_operator_locations(graph, query);
    EXPECT_EQ(location_of(locations, scan), 8);
    EXPECT_EQ(location_of(locations, write), 8);
}

TEST_F(decide_operator_locations_test, remote_write) {
    // values - write(R)
    relation::graph_type graph {};
    auto&& values = graph.insert(relation::values { {}, {} });
    auto&& write = add_write(graph, *remote_index);
    values.output() >> write.input();

    auto locations = decide_operator_locations(graph, query);
    EXPECT_EQ(location_of(locations, values), 0);
    EXPECT_EQ(location_of(locations, write), 7);
}

TEST_F(decide_operator_locations_test, join) {
    /*
     * scan(R) -\
     *           join - emit
     * scan(O) -/
     */
    relation::graph_type graph {};
    auto&& left = add_scan(graph, *remote_index);
    auto&& right = add_scan(graph, *other_remote_index);
    auto&& join = graph.insert(relation::intermediate::join { relation::join_kind::inner });
    auto&& emit = add_emit(graph);
    left.output() >> join.left();
    right.output() >> join.right();
    join.output() >> emit.input();

    auto locations = decide_operator_locations(graph, query);
    EXPECT_EQ(location_of(locations, left), 7);
    EXPECT_EQ(location_of(locations, right), 8);
    EXPECT_EQ(location_of(locations, join), 0);
    EXPECT_EQ(location_of(locations, emit), 0);
}

TEST_F(decide_operator_locations_test, nowhere) {
    // scan(N{}) - emit
    relation::graph_type graph {};
    auto&& scan = add_scan(graph, *nowhere_index);
    auto&& emit = add_emit(graph);
    scan.output() >> emit.input();

    auto locations = decide_operator_locations(graph, query);
    EXPECT_FALSE(location_of(locations, scan));
    EXPECT_EQ(location_of(locations, emit), 0);
}

} // namespace yugawara::details

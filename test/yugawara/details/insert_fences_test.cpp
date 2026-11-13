#include <yugawara/details/insert_fences.h>

#include <gtest/gtest.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/values.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/write.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/intermediate/join.h>

#include <takatori/util/downcast.h>

#include <yugawara/extension/relation/fence.h>
#include <yugawara/storage/configurable_provider.h>
#include <yugawara/binding/factory.h>

#include <yugawara/testing/utils.h>
#include <yugawara/testing/mock_symbol_location_query.h>

namespace yugawara::details {

// import test utils
using namespace ::yugawara::testing;

using location_id_set_type = remote::location_info::location_id_set_type;

using extension::relation::fence;

class insert_fences_test : public ::testing::Test {
protected:
    binding::factory factory;

    std::shared_ptr<storage::configurable_provider> storages = std::make_shared<storage::configurable_provider>();

    std::shared_ptr<storage::table> local_table = add_table("L");
    std::shared_ptr<storage::index> local_index = storages->add_index({ local_table, "L", });

    std::shared_ptr<storage::table> remote_table = add_table("R");
    std::shared_ptr<storage::index> remote_index = storages->add_index({ remote_table, "R", });

    std::shared_ptr<storage::table> other_remote_table = add_table("O");
    std::shared_ptr<storage::index> other_remote_index = storages->add_index({ other_remote_table, "O", });

    std::shared_ptr<storage::table> nowhere_table = add_table("N");
    std::shared_ptr<storage::index> nowhere_index = storages->add_index({ nowhere_table, "N", });

    mock_symbol_location_query query {};

    void SetUp() override {
        query.all = { 0, 7, 8 };
        query.tables.emplace(local_table.get(), location_id_set_type { 0 });
        query.tables.emplace(remote_table.get(), location_id_set_type { 7 });
        query.tables.emplace(other_remote_table.get(), location_id_set_type { 8 });
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

    static std::size_t count_fences(relation::graph_type const& graph) {
        std::size_t result = 0;
        for (auto&& expr : graph) {
            if (is_fence(expr)) {
                ++result;
            }
        }
        return result;
    }

    static bool is_fence(relation::expression const& expr) {
        return expr.kind() == relation::intermediate::extension::tag
            && ::takatori::util::unsafe_downcast<relation::intermediate::extension>(expr).extension_id()
                    == fence::extension_tag;
    }

    /**
     * @brief returns the fence just after the given output port.
     */
    static fence const& fence_after(relation::expression::output_port_type const& output) {
        auto opposite = output.opposite();
        if (!opposite || !is_fence(opposite->owner())) {
            throw std::domain_error("fence is not found");
        }
        return ::takatori::util::unsafe_downcast<fence>(opposite->owner());
    }
};

TEST_F(insert_fences_test, remote_scan) {
    /*
     * scan(R) - emit
     * =>
     * scan(R) - fence{7, 0} - emit
     */
    relation::graph_type graph {};
    auto&& scan = add_scan(graph, *remote_index);
    auto&& emit = add_emit(graph);
    scan.output() >> emit.input();

    ASSERT_TRUE(insert_fences(graph, query).empty());

    ASSERT_EQ(graph.size(), 3);
    auto&& f = fence_after(scan.output());
    EXPECT_EQ(f.upstream(), 7);
    EXPECT_EQ(f.downstream(), 0);
    EXPECT_GT(f.output(), emit.input());
}

TEST_F(insert_fences_test, local) {
    /*
     * scan(L) - filter - emit
     * => (no changes)
     */
    relation::graph_type graph {};
    auto&& scan = add_scan(graph, *local_index);
    auto&& filter = graph.insert(relation::filter { constant() });
    auto&& emit = add_emit(graph);
    scan.output() >> filter.input();
    filter.output() >> emit.input();

    ASSERT_TRUE(insert_fences(graph, query).empty());

    ASSERT_EQ(graph.size(), 3);
    EXPECT_EQ(count_fences(graph), 0);
    EXPECT_GT(scan.output(), filter.input());
    EXPECT_GT(filter.output(), emit.input());
}

TEST_F(insert_fences_test, remote_write) {
    /*
     * values - write(R)
     * =>
     * values - fence{0, 7} - write(R)
     */
    relation::graph_type graph {};
    auto&& values = graph.insert(relation::values { {}, {} });
    auto&& write = graph.insert(relation::write { relation::write_kind::insert, factory(*remote_index), {}, {} });
    values.output() >> write.input();

    ASSERT_TRUE(insert_fences(graph, query).empty());

    ASSERT_EQ(graph.size(), 3);
    auto&& f = fence_after(values.output());
    EXPECT_EQ(f.upstream(), 0);
    EXPECT_EQ(f.downstream(), 7);
    EXPECT_GT(f.output(), write.input());
}

TEST_F(insert_fences_test, join_remote_local) {
    /*
     * scan(R) -\
     *           join - emit
     * scan(L) -/
     * =>
     * scan(R) - fence{7, 0} -\
     *                         join - emit
     * scan(L) ---------------/
     */
    relation::graph_type graph {};
    auto&& left = add_scan(graph, *remote_index);
    auto&& right = add_scan(graph, *local_index);
    auto&& join = graph.insert(relation::intermediate::join { relation::join_kind::inner });
    auto&& emit = add_emit(graph);
    left.output() >> join.left();
    right.output() >> join.right();
    join.output() >> emit.input();

    ASSERT_TRUE(insert_fences(graph, query).empty());

    ASSERT_EQ(graph.size(), 5);
    EXPECT_EQ(count_fences(graph), 1);
    auto&& f = fence_after(left.output());
    EXPECT_EQ(f.upstream(), 7);
    EXPECT_EQ(f.downstream(), 0);
    EXPECT_GT(f.output(), join.left());
    EXPECT_GT(right.output(), join.right());
    EXPECT_GT(join.output(), emit.input());
}

TEST_F(insert_fences_test, join_remote_remote) {
    /*
     * scan(R) -\
     *           join - emit
     * scan(O) -/
     * =>
     * scan(R) - fence{7, 0} -\
     *                         join - emit
     * scan(O) - fence{8, 0} -/
     */
    relation::graph_type graph {};
    auto&& left = add_scan(graph, *remote_index);
    auto&& right = add_scan(graph, *other_remote_index);
    auto&& join = graph.insert(relation::intermediate::join { relation::join_kind::inner });
    auto&& emit = add_emit(graph);
    left.output() >> join.left();
    right.output() >> join.right();
    join.output() >> emit.input();

    ASSERT_TRUE(insert_fences(graph, query).empty());

    ASSERT_EQ(graph.size(), 6);
    EXPECT_EQ(count_fences(graph), 2);

    auto&& fl = fence_after(left.output());
    EXPECT_EQ(fl.upstream(), 7);
    EXPECT_EQ(fl.downstream(), 0);
    EXPECT_GT(fl.output(), join.left());

    auto&& fr = fence_after(right.output());
    EXPECT_EQ(fr.upstream(), 8);
    EXPECT_EQ(fr.downstream(), 0);
    EXPECT_GT(fr.output(), join.right());
}

TEST_F(insert_fences_test, nowhere) {
    /*
     * scan(N{}) - emit
     * => (no changes, and reports the scan)
     */
    relation::graph_type graph {};
    auto&& scan = add_scan(graph, *nowhere_index);
    auto&& emit = add_emit(graph);
    scan.output() >> emit.input();

    auto diagnostics = insert_fences(graph, query);

    ASSERT_EQ(diagnostics.size(), 1);
    EXPECT_EQ(diagnostics[0].code(), compiler_code::empty_operation_location);

    ASSERT_EQ(graph.size(), 2);
    EXPECT_EQ(count_fences(graph), 0);
    EXPECT_GT(scan.output(), emit.input());
}

TEST_F(insert_fences_test, nowhere_join) {
    /*
     * scan(R) --\
     *            join - emit
     * scan(N{}) -/
     * => (no changes: no fences even for the remote scan, and reports the scan of N)
     */
    relation::graph_type graph {};
    auto&& left = add_scan(graph, *remote_index);
    auto&& right = add_scan(graph, *nowhere_index);
    auto&& join = graph.insert(relation::intermediate::join { relation::join_kind::inner });
    auto&& emit = add_emit(graph);
    left.output() >> join.left();
    right.output() >> join.right();
    join.output() >> emit.input();

    auto diagnostics = insert_fences(graph, query);

    ASSERT_EQ(diagnostics.size(), 1);
    EXPECT_EQ(diagnostics[0].code(), compiler_code::empty_operation_location);

    ASSERT_EQ(graph.size(), 4);
    EXPECT_EQ(count_fences(graph), 0);
    EXPECT_GT(left.output(), join.left());
    EXPECT_GT(right.output(), join.right());
    EXPECT_GT(join.output(), emit.input());
}

} // namespace yugawara::details

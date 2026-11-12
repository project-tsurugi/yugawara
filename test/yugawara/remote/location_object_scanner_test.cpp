#include <yugawara/remote/location_object_scanner.h>

#include <iostream>
#include <stdexcept>

#include <gtest/gtest.h>

#include <takatori/serializer/json_printer.h>

#include <takatori/relation/scan.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/step/offer.h>
#include <takatori/relation/step/take_flat.h>

#include <takatori/plan/process.h>
#include <takatori/plan/forward.h>

#include <takatori/statement/execute.h>

#include <yugawara/storage/configurable_provider.h>
#include <yugawara/binding/factory.h>

#include <yugawara/details/compute_operation_locations.h>

#include <yugawara/testing/utils.h>
#include <yugawara/testing/mock_symbol_location_query.h>

namespace yugawara::remote {

// import test utils
using namespace ::yugawara::testing;

namespace tstatement = ::takatori::statement;

using location_id_set_type = location_info::location_id_set_type;

class location_object_scanner_test : public ::testing::Test {
protected:
    binding::factory factory;

    std::shared_ptr<storage::configurable_provider> storages = std::make_shared<storage::configurable_provider>();

    std::shared_ptr<storage::table> remote_table = storages->add_table({
            "R",
            {
                    { "C0", t::int4() },
            },
    });
    std::shared_ptr<storage::index> remote_index = storages->add_index({ remote_table, "R", });

    std::shared_ptr<mock_symbol_location_query> query = std::make_shared<mock_symbol_location_query>();

    void SetUp() override {
        query->all = { 0, 7 };
        query->tables.emplace(remote_table.get(), location_id_set_type { 7 });
    }

    // [scan(R) > offer] -> forward -> [take_flat > emit]
    void build(tstatement::execute& stmt) {
        auto&& graph = stmt.execution_plan();
        auto&& exchange = graph.insert(plan::forward {});
        {
            auto&& process = graph.insert(plan::process {});
            auto&& scan = process.operators().insert(relation::scan { factory(*remote_index), {} });
            auto&& offer = process.operators().insert(relation::step::offer { factory(exchange), {} });
            scan.output() >> offer.input();
            process >> exchange;
        }
        {
            auto&& process = graph.insert(plan::process {});
            auto&& take = process.operators().insert(relation::step::take_flat { factory(exchange), {} });
            auto&& emit = process.operators().insert(relation::emit { std::vector<relation::emit::column> {} });
            take.output() >> emit.input();
            exchange >> process;
        }
    }

    static void print(location_object_scanner const& scanner, tstatement::statement const& statement) {
        std::cout << ::testing::UnitTest::GetInstance()->current_test_info()->name() << ": ";
        ::takatori::serializer::json_printer printer { std::cout };
        scanner(statement, printer);
        std::cout << std::endl;
        if (printer.depth() != 0) {
            throw std::domain_error("invalid JSON depth");
        }
    }
};

TEST_F(location_object_scanner_test, simple) {
    query->locations.emplace(0, location_info { 0, "local" });
    query->locations.emplace(7, location_info { 7, "remote" });

    tstatement::execute stmt { plan::graph_type {} };
    build(stmt);
    auto locations = details::compute_operation_locations(stmt, *query);
    ASSERT_TRUE(locations);

    location_object_scanner scanner { {}, *locations, query };
    print(scanner, stmt);
}

TEST_F(location_object_scanner_test, without_location_names) {
    tstatement::execute stmt { plan::graph_type {} };
    build(stmt);
    auto locations = details::compute_operation_locations(stmt, *query);
    ASSERT_TRUE(locations);

    location_object_scanner scanner { {}, *locations };
    print(scanner, stmt);
}

TEST_F(location_object_scanner_test, without_operation_location_query) {
    tstatement::execute stmt { plan::graph_type {} };
    build(stmt);

    location_object_scanner scanner { {}, {} };
    print(scanner, stmt);
}

} // namespace yugawara::remote

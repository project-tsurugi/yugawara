#include <yugawara/analyzer/expression_analyzer.h>

#include <memory>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include <takatori/document/basic_document.h>

#include <takatori/type/primitive.h>
#include <takatori/type/decimal.h>
#include <takatori/type/character.h>
#include <takatori/type/octet.h>
#include <takatori/type/date.h>
#include <takatori/type/time_of_day.h>
#include <takatori/type/time_point.h>

#include <takatori/value/primitive.h>
#include <takatori/value/decimal.h>
#include <takatori/value/character.h>
#include <takatori/value/date.h>
#include <takatori/value/time_of_day.h>
#include <takatori/value/time_point.h>

#include <takatori/scalar/immediate.h>
#include <takatori/scalar/variable_reference.h>

#include <takatori/relation/scan.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/step/take_flat.h>
#include <takatori/relation/step/offer.h>

#include <takatori/plan/process.h>
#include <takatori/plan/forward.h>

#include <takatori/statement/execute.h>
#include <takatori/statement/write.h>
#include <takatori/statement/create_table.h>
#include <takatori/statement/truncate_table.h>
#include <takatori/statement/drop_partition.h>

#include <takatori/util/optional_ptr.h>

#include <yugawara/binding/factory.h>
#include <yugawara/extension/type/error.h>

#include <yugawara/function/declaration.h>

#include <yugawara/storage/table.h>
#include <yugawara/storage/index.h>
#include <yugawara/storage/configurable_provider.h>

namespace yugawara::analyzer {

namespace scalar = ::takatori::scalar;
namespace relation = ::takatori::relation;
namespace plan = ::takatori::plan;
namespace statement = ::takatori::statement;
namespace t = ::takatori::type;
namespace v = ::takatori::value;

using code = expression_analyzer_code;
using vref = ::takatori::scalar::variable_reference;

class expression_analyzer_statement_test : public ::testing::Test {
public:
    bool ok() noexcept {
        return !analyzer.has_diagnostics();
    }

    ::takatori::type::data const& type(::takatori::descriptor::variable const& variable) {
        auto t = analyzer.inspect(variable);
        if (t) {
            return *repo.get(*t);
        }
        static extension::type::error error;
        return error;
    }

    friend std::ostream& operator<<(std::ostream& out, expression_analyzer_statement_test const& t) {
        for (auto&& d : t.analyzer.diagnostics()) {
            out << d << std::endl;
        }
        return out;
    }

protected:
    expression_analyzer analyzer;
    type::repository repo { true };
    binding::factory bindings;

    storage::configurable_provider storages_;
    std::shared_ptr<storage::table> t0 = storages_.add_table({
            "T0",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    std::shared_ptr<storage::table> t1 = storages_.add_table({
            "T1",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    std::shared_ptr<storage::index> i0 = storages_.add_index({ t0, "I0", });
    std::shared_ptr<storage::index> i1 = storages_.add_index({ t1, "I1" });

    bool resolve_partitioned(::takatori::type::data&& type, storage::partition::prefix_extractor extractor) {
        auto schema = std::make_shared<schema::declaration>("s");
        auto table = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
                "T0",
                {
                        { "K0", t::int4() },
                        { "K1", std::move(type) },
                        { "V", t::int4() },
                },
        });
        auto index = std::make_shared<storage::index>(storage::index {
                table,
                "I0",
                {
                        table->columns()[0],
                        table->columns()[1],
                },
        });
        index->partition_key_columns().emplace_back(table->columns()[0]);
        index->partition_key_columns().emplace_back(table->columns()[1], "P1", extractor);
        statement::create_table stmt {
                bindings(schema),
                bindings(table),
                bindings(index),
        };
        analyzer.clear_diagnostics();
        auto b = analyzer.resolve(stmt, true);
        if (b) {
            EXPECT_TRUE(ok()) << *this;
        } else {
            EXPECT_FALSE(ok());
            for (auto&& d : analyzer.diagnostics()) {
                EXPECT_EQ(d.code(), code::inconsistent_type) << d;
            }
        }
        return b;
    }

    std::optional<code> resolve_drop_partition(
            ::takatori::type::data&& type,
            storage::partition::prefix_extractor extractor,
            ::takatori::scalar::comparison_operator op,
            std::shared_ptr<::takatori::value::data const> value) {
        auto table = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
                "T0",
                {
                        { "K0", t::int4() },
                        { "K1", std::move(type) },
                        { "V", t::int4() },
                },
        });
        auto index = std::make_shared<storage::index>(storage::index {
                table,
                "I0",
                {
                        table->columns()[0],
                        table->columns()[1],
                },
        });
        index->partition_key_columns().emplace_back(table->columns()[0]);
        index->partition_key_columns().emplace_back(table->columns()[1], "P1", extractor);
        statement::drop_partition stmt {
                bindings(index),
                {
                        statement::details::partition_criteria {
                                {
                                        {
                                                bindings(table->columns()[0]),
                                                "K0",
                                                { { scalar::comparison_operator::equal, v::int4 { 1 } } },
                                        },
                                        {
                                                bindings(table->columns()[1]),
                                                "P1",
                                                std::vector<statement::details::partition_key_element::predicate_type> {
                                                        { op, std::move(value) },
                                                },
                                        },
                                },
                        },
                },
        };
        analyzer.clear_diagnostics();
        auto b = analyzer.resolve(stmt, true);
        if (b) {
            EXPECT_TRUE(ok()) << *this;
            return {};
        }
        EXPECT_FALSE(ok());
        EXPECT_EQ(analyzer.diagnostics().size(), 1) << *this;
        return analyzer.diagnostics().front().code();
    }

    ::takatori::document::basic_document doc_ {
            "testing.sql",
            "01234567890123456789012345678901234567890123456789",
    };
    std::size_t doc_cursor_ {};
};

TEST_F(expression_analyzer_statement_test, execute) {
    statement::execute stmt;
    auto&& g = stmt.execution_plan();

    auto p0c0 = bindings.stream_variable("p0c0");
    auto p1c0 = bindings.exchange_column("p1c0");
    auto p2c0 = bindings.stream_variable("p2c0");

    auto&& p0 = g.insert(plan::process {});
    auto&& p1 = g.insert(plan::forward { p1c0 });
    auto&& p2 = g.insert(plan::process {});

    p0 >> p1;
    p1 >> p2;

    auto&& scan = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0->columns()[0]), p0c0 },
            },
    });
    auto&& offer = p0.operators().insert(relation::step::offer {
            bindings(p1),
            {
                    { p0c0, p1c0 },
            },
    });
    scan.output() >> offer.input();

    auto&& take = p2.operators().insert(relation::step::take_flat {
            bindings(p1),
            {
                    { p1c0, p2c0 },
            },
    });
    auto&& emit = p2.operators().insert(relation::emit {
            p2c0,
    });
    take.output() >> emit.input();

    auto b = analyzer.resolve(stmt, true);
    ASSERT_TRUE(b);
    EXPECT_TRUE(ok());

    EXPECT_EQ(type(p0c0), t::int4());
    EXPECT_EQ(type(p1c0), t::int4());
    EXPECT_EQ(type(p2c0), t::int4());
}

TEST_F(expression_analyzer_statement_test, write) {
    statement::write stmt {
            statement::write_kind::insert,
            bindings(*i0),
            {
                    bindings(t0->columns()[0]),
                    bindings(t0->columns()[1]),
                    bindings(t0->columns()[2]),
            },
            {
                    {
                            scalar::immediate { v::int4 { 0 }, t::int4 {} },
                            scalar::immediate { v::int4 { 1 }, t::int4 {} },
                            scalar::immediate { v::int4 { 2 }, t::int4 {} },
                    },
                    {
                            scalar::immediate { v::int4 { 3 }, t::int4 {} },
                            scalar::immediate { v::int4 { 4 }, t::int4 {} },
                            scalar::immediate { v::int4 { 5 }, t::int4 {} },
                    },
            }
    };
    auto b = analyzer.resolve(stmt, true);
    ASSERT_TRUE(b);
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_statement_test, create_table) {
    auto schema = std::make_shared<schema::declaration>("s");
    statement::create_table stmt {
            bindings(schema),
            bindings(t0),
            bindings(i0),
    };
    auto b = analyzer.resolve(stmt, true);
    ASSERT_TRUE(b);
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_statement_test, create_table_default_value_immediate) {
    auto schema = std::make_shared<schema::declaration>("s");
    auto table = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T0",
            {
                    { "C0", t::int4() },
                    {
                            "C1",
                            t::int4(),
                            variable::nullable,
                            v::int4 { 100 },
                    },
                    { "C2", t::int4() },
            },
    });
    auto index = std::make_shared<storage::index>(storage::index {
            table,
            "I0",
    });
    statement::create_table stmt {
            bindings(schema),
            bindings(table),
            bindings(index),
    };
    auto b = analyzer.resolve(stmt, true);
    ASSERT_TRUE(b);
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_statement_test, create_table_default_value_sequence) {
    auto schema = std::make_shared<schema::declaration>("s");
    auto sequence = std::make_shared<storage::sequence>(storage::sequence {
            "S0",
    });
    auto table = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T0",
            {
                    { "C0", t::int4() },
                    {
                            "C1",
                            t::int4(),
                            variable::nullable,
                            { sequence },
                    },
                    { "C2", t::int4() },
            },
    });
    auto index = std::make_shared<storage::index>(storage::index {
            table,
            "I0",
    });
    statement::create_table stmt {
            bindings(schema),
            bindings(table),
            bindings(index),
    };
    auto b = analyzer.resolve(stmt, true);
    ASSERT_TRUE(b);
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_statement_test, create_table_default_value_function) {
    auto func = std::make_shared<function::declaration>(function::declaration {
            function::declaration::minimum_builtin_function_id + 1,
            "func",
            t::int4(),
            {},
    });
    auto schema = std::make_shared<schema::declaration>("s");
    auto table = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T0",
            {
                    { "C0", t::int4() },
                    {
                            "C1",
                            t::int4(),
                            variable::nullable,
                            { func },
                    },
                    { "C2", t::int4() },
            },
    });
    auto index = std::make_shared<storage::index>(storage::index {
            table,
            "I0",
    });
    statement::create_table stmt {
            bindings(schema),
            bindings(table),
            bindings(index),
    };
    auto b = analyzer.resolve(stmt, true);
    ASSERT_TRUE(b);
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_statement_test, create_table_default_value_time_of_day_tz) {
    auto schema = std::make_shared<schema::declaration>("s");
    auto table = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T0",
            {
                    {
                            "C0",
                            t::time_of_day { t::with_time_zone },
                            variable::nullable,
                            v::time_of_day { 12, 34, 56 },
                    },
            },
    });
    auto index = std::make_shared<storage::index>(storage::index {
            table,
            "I0",
    });
    statement::create_table stmt {
            bindings(schema),
            bindings(table),
            bindings(index),
    };
    auto b = analyzer.resolve(stmt, true);
    ASSERT_TRUE(b);
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_statement_test, create_table_default_value_time_point_tz) {
    auto schema = std::make_shared<schema::declaration>("s");
    auto table = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T0",
            {
                    {
                            "C0",
                            t::time_point { t::with_time_zone },
                            variable::nullable,
                            v::time_point { 2000, 1, 1, 12, 34, 56 },
                    },
            },
    });
    auto index = std::make_shared<storage::index>(storage::index {
            table,
            "I0",
    });
    statement::create_table stmt {
            bindings(schema),
            bindings(table),
            bindings(index),
    };
    auto b = analyzer.resolve(stmt, true);
    ASSERT_TRUE(b);
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_statement_test, create_table_default_value_immediate_inconsistent) {
    auto schema = std::make_shared<schema::declaration>("s");
    auto table = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T0",
            {
                    { "C0", t::int4() },
                    {
                        "C1",
                        t::int4(),
                        variable::nullable,
                        v::character { "X" },
                    },
                    { "C2", t::int4() },
            },
    });
    auto index = std::make_shared<storage::index>(storage::index {
            table,
            "I0",
    });
    statement::create_table stmt {
            bindings(schema),
            bindings(table),
            bindings(index),
    };
    auto b = analyzer.resolve(stmt, true);
    EXPECT_FALSE(b);
    EXPECT_FALSE(ok());
}

TEST_F(expression_analyzer_statement_test, create_table_default_value_sequence_inconsistent) {
    auto schema = std::make_shared<schema::declaration>("s");
    auto sequence = std::make_shared<storage::sequence>(storage::sequence {
            "S0",
    });
    auto table = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T0",
            {
                    { "C0", t::int4() },
                    {
                            "C1",
                            t::date {},
                            variable::nullable,
                            { sequence },
                    },
                    { "C2", t::int4() },
            },
    });
    auto index = std::make_shared<storage::index>(storage::index {
            table,
            "I0",
    });
    statement::create_table stmt {
            bindings(schema),
            bindings(table),
            bindings(index),
    };
    auto b = analyzer.resolve(stmt, true);
    EXPECT_FALSE(b);
    EXPECT_FALSE(ok());
}

TEST_F(expression_analyzer_statement_test, create_table_default_value_function_inconsistent_return_type) {
    auto func = std::make_shared<function::declaration>(function::declaration {
            function::declaration::minimum_builtin_function_id + 1,
            "func",
            t::int4(),
            {},
    });
    auto schema = std::make_shared<schema::declaration>("s");
    auto table = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T0",
            {
                    { "C0", t::int4() },
                    {
                            "C1",
                            t::date {},
                            variable::nullable,
                            { func },
                    },
                    { "C2", t::int4() },
            },
    });
    auto index = std::make_shared<storage::index>(storage::index {
            table,
            "I0",
    });
    statement::create_table stmt {
            bindings(schema),
            bindings(table),
            bindings(index),
    };
    auto b = analyzer.resolve(stmt, true);
    EXPECT_FALSE(b);
    EXPECT_FALSE(ok());
}

TEST_F(expression_analyzer_statement_test, create_table_default_value_function_inconsistent_parameters) {
    auto func = std::make_shared<function::declaration>(function::declaration {
            function::declaration::minimum_builtin_function_id + 1,
            "func",
            t::int4(),
            {
                    t::int4(),
            },
    });
    auto schema = std::make_shared<schema::declaration>("s");
    auto table = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T0",
            {
                    { "C0", t::int4() },
                    {
                            "C1",
                            t::int4(),
                            variable::nullable,
                            { func },
                    },
                    { "C2", t::int4() },
            },
    });
    auto index = std::make_shared<storage::index>(storage::index {
            table,
            "I0",
    });
    statement::create_table stmt {
            bindings(schema),
            bindings(table),
            bindings(index),
    };
    auto b = analyzer.resolve(stmt, true);
    EXPECT_FALSE(b);
    EXPECT_FALSE(ok());
}

TEST_F(expression_analyzer_statement_test, create_table_default_value_function_inconsistent_function_type) {
    auto func = std::make_shared<function::declaration>(function::declaration {
            function::declaration::minimum_builtin_function_id + 1,
            "func",
            t::int4(),
            {},
            { function::function_feature::table_valued_function },
    });
    auto schema = std::make_shared<schema::declaration>("s");
    auto table = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T0",
            {
                    { "C0", t::int4() },
                    {
                            "C1",
                            t::int4(),
                            variable::nullable,
                            { func },
                    },
                    { "C2", t::int4() },
            },
    });
    auto index = std::make_shared<storage::index>(storage::index {
            table,
            "I0",
    });
    statement::create_table stmt {
            bindings(schema),
            bindings(table),
            bindings(index),
    };
    auto b = analyzer.resolve(stmt, true);
    EXPECT_FALSE(b);
    EXPECT_FALSE(ok());
}

TEST_F(expression_analyzer_statement_test, create_table_partitioned) {
    namespace p = storage::partition;
    EXPECT_TRUE(resolve_partitioned(t::int4(), {}));
    EXPECT_TRUE(resolve_partitioned(t::character(4), p::character_prefix { 2 }));
    EXPECT_TRUE(resolve_partitioned(t::character(t::varying, 10), p::character_prefix { 2 }));
    EXPECT_TRUE(resolve_partitioned(t::octet(4), p::binary_prefix { 2 }));
    EXPECT_TRUE(resolve_partitioned(t::octet(t::varying, 10), p::binary_prefix { 2 }));
    EXPECT_TRUE(resolve_partitioned(t::int4(), p::floor_division { 10 }));
    EXPECT_TRUE(resolve_partitioned(t::int8(), p::floor_division { 10 }));
    EXPECT_TRUE(resolve_partitioned(t::int2(), p::floor_division { 10 }));
    EXPECT_TRUE(resolve_partitioned(t::decimal(10, 2), p::floor_division { 10 }));
    EXPECT_TRUE(resolve_partitioned(t::decimal(10, 2), p::floor_fractional { 1 }));
    EXPECT_TRUE(resolve_partitioned(t::date(), p::date_truncate { p::date_unit::month }));
    EXPECT_TRUE(resolve_partitioned(t::time_point(), p::timestamp_truncate { p::timestamp_unit::hour }));
}

TEST_F(expression_analyzer_statement_test, create_table_partitioned_inconsistent) {
    namespace p = storage::partition;
    EXPECT_FALSE(resolve_partitioned(t::int4(), p::character_prefix { 2 }));
    EXPECT_FALSE(resolve_partitioned(t::character(4), p::binary_prefix { 2 }));
    EXPECT_FALSE(resolve_partitioned(t::character(t::varying, 10), p::floor_division { 10 }));
    EXPECT_FALSE(resolve_partitioned(t::float8(), p::floor_division { 10 }));
    EXPECT_FALSE(resolve_partitioned(t::float8(), p::floor_fractional { 1 }));
    EXPECT_FALSE(resolve_partitioned(t::int8(), p::floor_fractional { 1 }));
    EXPECT_FALSE(resolve_partitioned(t::time_point(), p::date_truncate { p::date_unit::month }));
    EXPECT_FALSE(resolve_partitioned(t::date(), p::timestamp_truncate { p::timestamp_unit::day }));
    EXPECT_FALSE(resolve_partitioned(t::time_point(t::with_time_zone), p::timestamp_truncate { p::timestamp_unit::day }));
}

TEST_F(expression_analyzer_statement_test, drop_partition) {
    namespace p = storage::partition;
    using op = scalar::comparison_operator;
    EXPECT_FALSE(resolve_drop_partition(t::int4(), {}, op::equal, std::make_shared<v::int4>(1)));
    EXPECT_FALSE(resolve_drop_partition(t::int4(), {}, op::less, std::make_shared<v::int8>(1)));
    EXPECT_FALSE(resolve_drop_partition(t::decimal(10, 2), {}, op::greater_equal, std::make_shared<v::decimal>("1.5")));
    EXPECT_FALSE(resolve_drop_partition(
            t::character(t::varying, 10),
            p::character_prefix { 2 },
            op::equal,
            std::make_shared<v::character>("ab")));
    EXPECT_FALSE(resolve_drop_partition(
            t::int8(),
            p::floor_division { 1000 },
            op::equal,
            std::make_shared<v::int4>(5)));
    EXPECT_FALSE(resolve_drop_partition(
            t::date(),
            p::date_truncate { p::date_unit::year },
            op::equal,
            std::make_shared<v::int4>(2026)));
    EXPECT_FALSE(resolve_drop_partition(
            t::date(),
            p::date_truncate { p::date_unit::month },
            op::equal,
            std::make_shared<v::date>(2026, 1, 1)));
    EXPECT_FALSE(resolve_drop_partition(
            t::time_point(),
            p::timestamp_truncate { p::timestamp_unit::month },
            op::less,
            std::make_shared<v::date>(2026, 1, 1)));
    EXPECT_FALSE(resolve_drop_partition(
            t::time_point(),
            p::timestamp_truncate { p::timestamp_unit::hour },
            op::equal,
            std::make_shared<v::time_point>(::takatori::datetime::date { 2026, 1, 1 })));
}

TEST_F(expression_analyzer_statement_test, drop_partition_inconsistent) {
    namespace p = storage::partition;
    using op = scalar::comparison_operator;
    EXPECT_EQ(resolve_drop_partition(t::int4(), {}, op::equal, std::make_shared<v::character>("1")), code::inconsistent_type);
    EXPECT_EQ(resolve_drop_partition(
            t::date(),
            p::date_truncate { p::date_unit::year },
            op::equal,
            std::make_shared<v::date>(2026, 1, 1)), code::inconsistent_type);
    EXPECT_EQ(resolve_drop_partition(
            t::date(),
            p::date_truncate { p::date_unit::month },
            op::equal,
            std::make_shared<v::int4>(2026)), code::inconsistent_type);
    EXPECT_EQ(resolve_drop_partition(
            t::time_point(),
            p::timestamp_truncate { p::timestamp_unit::year },
            op::less,
            std::make_shared<v::time_point>(::takatori::datetime::date { 2026, 1, 1 })), code::inconsistent_type);
}

TEST_F(expression_analyzer_statement_test, drop_partition_approximate_number) {
    namespace p = storage::partition;
    using op = scalar::comparison_operator;
    EXPECT_EQ(resolve_drop_partition(t::int8(), {}, op::equal, std::make_shared<v::float8>(1.5)), code::inconsistent_type);
    EXPECT_EQ(resolve_drop_partition(t::decimal(10, 2), {}, op::less, std::make_shared<v::float4>(1.5F)), code::inconsistent_type);
    EXPECT_EQ(resolve_drop_partition(
            t::int8(),
            p::floor_division { 1000 },
            op::equal,
            std::make_shared<v::float8>(5.0)), code::inconsistent_type);
    EXPECT_EQ(resolve_drop_partition(
            t::decimal(10, 4),
            p::floor_fractional { 2 },
            op::greater_equal,
            std::make_shared<v::float8>(1.5)), code::inconsistent_type);
    EXPECT_EQ(resolve_drop_partition(
            t::time_point(),
            p::timestamp_truncate { p::timestamp_unit::year },
            op::equal,
            std::make_shared<v::float8>(2000.0)), code::inconsistent_type);

    // approximate numeric keys can be compared with approximate numeric values
    EXPECT_FALSE(resolve_drop_partition(t::float8(), {}, op::equal, std::make_shared<v::float8>(1.5)));
    EXPECT_FALSE(resolve_drop_partition(t::float4(), {}, op::less, std::make_shared<v::float8>(1.5)));
}

TEST_F(expression_analyzer_statement_test, drop_partition_too_many_elements) {
    statement::drop_partition stmt {
            bindings(i0),
            {
                    statement::details::partition_criteria {
                            {
                                    {
                                            bindings(t0->columns()[0]),
                                            "C0",
                                            { { scalar::comparison_operator::equal, v::int4 { 1 } } },
                                    },
                            },
                    },
            },
    };
    auto b = analyzer.resolve(stmt, true);
    EXPECT_FALSE(b);
    ASSERT_EQ(analyzer.diagnostics().size(), 1);
    EXPECT_EQ(analyzer.diagnostics().front().code(), code::inconsistent_elements);
}

TEST_F(expression_analyzer_statement_test, drop_partition_inconsistent_column) {
    auto table = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T0",
            {
                    { "K0", t::int4() },
                    { "K1", t::int4() },
            },
    });
    auto index = std::make_shared<storage::index>(storage::index {
            table,
            "I0",
            {
                    table->columns()[0],
                    table->columns()[1],
            },
    });
    index->partition_key_columns().emplace_back(table->columns()[0]);
    index->partition_key_columns().emplace_back(table->columns()[1]);
    statement::drop_partition stmt {
            bindings(index),
            {
                    statement::details::partition_criteria {
                            {
                                    {
                                            bindings(table->columns()[1]),
                                            "K1",
                                            { { scalar::comparison_operator::equal, v::int4 { 1 } } },
                                    },
                            },
                    },
            },
    };
    auto b = analyzer.resolve(stmt, true);
    EXPECT_FALSE(b);
    ASSERT_EQ(analyzer.diagnostics().size(), 1);
    EXPECT_EQ(analyzer.diagnostics().front().code(), code::unknown);
}

TEST_F(expression_analyzer_statement_test, truncate_table) {
    auto table = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T0",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    statement::truncate_table stmt {
            bindings(table),
    };
    auto b = analyzer.resolve(stmt, true);
    EXPECT_TRUE(b);
}

} // namespace yugawara::analyzer

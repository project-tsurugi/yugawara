#include <yugawara/analyzer/expression_analyzer.h>

#include <gtest/gtest.h>

#include <takatori/descriptor/variable.h>

#include <takatori/type/primitive.h>
#include <takatori/type/datetime_interval.h>
#include <takatori/type/lob.h>

#include <takatori/value/primitive.h>

#include <takatori/scalar/expression.h>
#include <takatori/scalar/variable_reference.h>
#include <takatori/scalar/compare.h>
#include <takatori/scalar/comparison_operator.h>

#include <takatori/relation/find.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/join_find.h>
#include <takatori/relation/join_scan.h>
#include <takatori/relation/intermediate/distinct.h>
#include <takatori/relation/intermediate/limit.h>
#include <takatori/relation/intermediate/union.h>
#include <takatori/relation/intermediate/intersection.h>
#include <takatori/relation/intermediate/difference.h>
#include <takatori/relation/intermediate/aggregate.h>

#include <takatori/plan/group.h>
#include <takatori/plan/aggregate.h>
#include <takatori/plan/broadcast.h>

#include <takatori/statement/create_table.h>
#include <takatori/statement/create_index.h>

#include <takatori/util/string_builder.h>

#include <yugawara/binding/factory.h>

#include <yugawara/storage/configurable_provider.h>

namespace yugawara::analyzer {

namespace s = ::takatori::scalar;
namespace r = ::takatori::relation;
namespace p = ::takatori::plan;
namespace st = ::takatori::statement;
namespace t = ::takatori::type;
namespace v = ::takatori::value;

using code = expression_analyzer_code;
using vref = ::takatori::scalar::variable_reference;

class expression_analyzer_comparison_test : public ::testing::Test {
public:
    bool ok() noexcept {
        return !analyzer.has_diagnostics();
    }

    std::string diagnostics() const {
        ::takatori::util::string_builder sb;
        for (auto& diag : analyzer.diagnostics()) {
            sb << diag << "\n";
        }
        return sb.str();
    }

    ::takatori::descriptor::variable decl(::takatori::type::data&& type) {
        auto var = bindings.local_variable();
        analyzer.variables().bind(var, std::move(type));
        return var;
    }

protected:
    expression_analyzer analyzer;
    type::repository repo;
    binding::factory bindings;

    storage::configurable_provider storages_;
};

TEST_F(expression_analyzer_comparison_test, compare_equal_int) {
    auto a = decl(t::int4 {});
    auto b = decl(t::int4 {});
    s::compare expr {
            s::comparison_operator::equal,
            s::variable_reference { a },
            s::variable_reference { b },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_TRUE(ok());
    EXPECT_EQ(*r, t::boolean());
}

TEST_F(expression_analyzer_comparison_test, compare_less_int) {
    auto a = decl(t::int4 {});
    auto b = decl(t::int4 {});
    s::compare expr {
            s::comparison_operator::less,
            s::variable_reference { a },
            s::variable_reference { b },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_TRUE(ok());
    EXPECT_EQ(*r, t::boolean());
}

TEST_F(expression_analyzer_comparison_test, compare_equal_interval) {
    auto a = decl(t::datetime_interval {});
    auto b = decl(t::datetime_interval {});
    s::compare expr {
        s::comparison_operator::equal,
        s::variable_reference { a },
        s::variable_reference { b },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_comparison_test, compare_less_interval) {
    auto a = decl(t::datetime_interval {});
    auto b = decl(t::datetime_interval {});
    s::compare expr {
        s::comparison_operator::less,
        s::variable_reference { a },
        s::variable_reference { b },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_FALSE(ok());
}

TEST_F(expression_analyzer_comparison_test, compare_equal_blob) {
    auto a = decl(t::blob {});
    auto b = decl(t::blob {});
    s::compare expr {
            s::comparison_operator::equal,
            s::variable_reference { a },
            s::variable_reference { b },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_FALSE(ok());
}

TEST_F(expression_analyzer_comparison_test, compare_less_blob) {
    auto a = decl(t::blob {});
    auto b = decl(t::blob {});
    s::compare expr {
            s::comparison_operator::less,
            s::variable_reference { a },
            s::variable_reference { b },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_FALSE(ok());
}

TEST_F(expression_analyzer_comparison_test, find_int) {
    using testee = t::int4;
    auto t1 = storages_.add_table(storage::table {
            "t1",
            {
                    { "c1", testee {} },
            },
    });
    auto i1 = storages_.add_index(storage::index {
            t1,
            "i1",
            {
                    { t1->columns()[0] },
            },
    });
    r::find expr {
            bindings(i1),
            {
                    {
                            decl(testee {}),
                            bindings(t1->columns()[0]),
                    },
            },
            {
                    r::find::key {
                            bindings(t1->columns()[0]),
                            vref { decl(testee {}) },
                    },
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_TRUE(ok());
    EXPECT_TRUE(r);
}

TEST_F(expression_analyzer_comparison_test, find_interval) {
    using testee = t::datetime_interval;
    auto t1 = storages_.add_table(storage::table {
            "t1",
            {
                    { "c1", testee {} },
            },
    });
    auto i1 = storages_.add_index(storage::index {
            t1,
            "i1",
            {
                    { t1->columns()[0] },
            },
    });
    r::find expr {
            bindings(i1),
            {
                    {
                                decl(testee {}),
                                bindings(t1->columns()[0]),
                    },
            },
            {
                    r::find::key {
                            bindings(t1->columns()[0]),
                            vref { decl(testee {}) },
                    },
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_TRUE(ok());
    EXPECT_TRUE(r);
}

TEST_F(expression_analyzer_comparison_test, find_blob) {
    using testee = t::blob;
    auto t1 = storages_.add_table(storage::table {
            "t1",
            {
                    { "c1", testee {} },
            },
    });
    auto i1 = storages_.add_index(storage::index {
            t1,
            "i1",
            {
                    { t1->columns()[0] },
            },
    });
    r::find expr {
            bindings(i1),
            {
                    {
                            decl(testee {}),
                            bindings(t1->columns()[0]),
                    },
            },
            {
                    r::find::key {
                            bindings(t1->columns()[0]),
                            vref { decl(testee {}) },
                    },
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_FALSE(ok());
    EXPECT_FALSE(r);
}

TEST_F(expression_analyzer_comparison_test, scan_int) {
    using testee = t::int4;
    auto t1 = storages_.add_table(storage::table {
            "t1",
            {
                    { "c1", testee {} },
            },
    });
    auto i1 = storages_.add_index(storage::index {
            t1,
            "i1",
            {
                    { t1->columns()[0] },
            },
    });
    r::scan expr {
            bindings(i1),
            {
                    {
                            decl(testee {}),
                            bindings(t1->columns()[0]),
                    },
            },
            r::scan::endpoint {
                    {
                            r::scan::key {
                                    bindings(t1->columns()[0]),
                                    vref { decl(testee {}) },
                            },
                    },
                    r::scan::endpoint::kind_type::inclusive,
            },
            r::scan::endpoint {
                    {
                            r::scan::key {
                                    bindings(t1->columns()[0]),
                                    vref { decl(testee {}) },
                            },
                    },
                    r::scan::endpoint::kind_type::inclusive,
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_TRUE(ok());
    EXPECT_TRUE(r);
}

TEST_F(expression_analyzer_comparison_test, scan_interval) {
    using testee = t::datetime_interval;
    auto t1 = storages_.add_table(storage::table {
            "t1",
            {
                    { "c1", testee {} },
            },
    });
    auto i1 = storages_.add_index(storage::index {
            t1,
            "i1",
            {
                    { t1->columns()[0] },
            },
    });
    r::scan expr {
            bindings(i1),
            {
                    {
                            decl(testee {}),
                            bindings(t1->columns()[0]),
                    },
            },
            r::scan::endpoint {
                    {
                            r::scan::key {
                                    bindings(t1->columns()[0]),
                                    vref { decl(testee {}) },
                            },
                    },
                    r::scan::endpoint::kind_type::inclusive,
            },
            r::scan::endpoint {
                    {
                            r::scan::key {
                                    bindings(t1->columns()[0]),
                                    vref { decl(testee {}) },
                            },
                    },
                    r::scan::endpoint::kind_type::inclusive,
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_FALSE(ok());
    EXPECT_FALSE(r);
}

TEST_F(expression_analyzer_comparison_test, scan_prefix_interval) {
    using testee = t::datetime_interval;
    auto t1 = storages_.add_table(storage::table {
            "t1",
            {
                    { "c1", testee {} },
                    { "c2", t::int4 {} },
            },
    });
    auto i1 = storages_.add_index(storage::index {
            t1,
            "i1",
            {
                    { t1->columns()[0] },
                    { t1->columns()[1] },
            },
    });
    r::scan expr {
            bindings(i1),
            {
                    {
                            decl(testee {}),
                            bindings(t1->columns()[0]),
                    },
            },
            r::scan::endpoint {
                    {
                            r::scan::key {
                                    bindings(t1->columns()[0]),
                                    vref { decl(testee {}) },
                            },
                            r::scan::key {
                                    bindings(t1->columns()[1]),
                                    vref { decl(t::int4 {}) },
                            },
                    },
                    r::scan::endpoint::kind_type::inclusive,
            },
            r::scan::endpoint {
                    {
                            r::scan::key {
                                    bindings(t1->columns()[0]),
                                    vref { decl(testee {}) },
                            },
                            r::scan::key {
                                    bindings(t1->columns()[1]),
                                    vref { decl(t::int4 {}) },
                            },
                    },
                    r::scan::endpoint::kind_type::inclusive,
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_TRUE(ok()) << diagnostics();
    EXPECT_TRUE(r);
}

TEST_F(expression_analyzer_comparison_test, scan_prefix_blob) {
    using testee = t::blob;
    auto t1 = storages_.add_table(storage::table {
            "t1",
            {
                    { "c1", testee {} },
                    { "c2", t::int4 {} },
            },
    });
    auto i1 = storages_.add_index(storage::index {
            t1,
            "i1",
            {
                    { t1->columns()[0] },
                    { t1->columns()[1] },
            },
    });
    r::scan expr {
            bindings(i1),
            {
                    {
                            decl(testee {}),
                            bindings(t1->columns()[0]),
                    },
            },
            r::scan::endpoint {
                    {
                            r::scan::key {
                                    bindings(t1->columns()[0]),
                                    vref { decl(testee {}) },
                            },
                            r::scan::key {
                                    bindings(t1->columns()[1]),
                                    vref { decl(t::int4 {}) },
                            },
                    },
                    r::scan::endpoint::kind_type::inclusive,
            },
            r::scan::endpoint {
                    {
                            r::scan::key {
                                    bindings(t1->columns()[0]),
                                    vref { decl(testee {}) },
                            },
                            r::scan::key {
                                    bindings(t1->columns()[1]),
                                    vref { decl(t::int4 {}) },
                            },
                    },
                    r::scan::endpoint::kind_type::inclusive,
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_FALSE(ok());
    EXPECT_FALSE(r);
}

TEST_F(expression_analyzer_comparison_test, join_find_int) {
    using testee = t::int4;
    auto t1 = storages_.add_table(storage::table {
            "t1",
            {
                    { "c1", testee {} },
            },
    });
    auto i1 = storages_.add_index(storage::index {
            t1,
            "i1",
            {
                    { t1->columns()[0] },
            },
    });
    r::join_find expr {
            r::join_find::operator_kind_type::inner,
            bindings(i1),
            {
                    {
                            decl(testee {}),
                            bindings(t1->columns()[0]),
                    },
            },
            {
                    r::join_find::key {
                            bindings(t1->columns()[0]),
                            vref { decl(testee {}) },
                    },
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_TRUE(ok());
    EXPECT_TRUE(r);
}

TEST_F(expression_analyzer_comparison_test, join_find_blob) {
    using testee = t::blob;
    auto t1 = storages_.add_table(storage::table {
            "t1",
            {
                    { "c1", testee {} },
            },
    });
    auto i1 = storages_.add_index(storage::index {
            t1,
            "i1",
            {
                    { t1->columns()[0] },
            },
    });
    r::join_find expr {
            r::join_find::operator_kind_type::inner,
            bindings(i1),
            {
                    {
                            decl(testee {}),
                            bindings(t1->columns()[0]),
                    },
            },
            {
                    r::join_find::key {
                            bindings(t1->columns()[0]),
                            vref { decl(testee {}) },
                    },
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_FALSE(ok());
    EXPECT_FALSE(r);
}

TEST_F(expression_analyzer_comparison_test, join_scan_int) {
    using testee = t::int4;
    auto t1 = storages_.add_table(storage::table {
            "t1",
            {
                    { "c1", testee {} },
            },
    });
    auto i1 = storages_.add_index(storage::index {
            t1,
            "i1",
            {
                    { t1->columns()[0] },
            },
    });
    r::join_scan expr {
            r::join_scan::operator_kind_type::inner,
            bindings(i1),
            {
                    {
                            decl(testee {}),
                            bindings(t1->columns()[0]),
                    },
            },
            r::join_scan::endpoint {
                    {
                            r::join_scan::key {
                                    bindings(t1->columns()[0]),
                                    vref { decl(testee {}) },
                            },
                    },
                    r::join_scan::endpoint::kind_type::inclusive,
            },
            r::join_scan::endpoint {
                    {
                            r::join_scan::key {
                                    bindings(t1->columns()[0]),
                                    vref { decl(testee {}) },
                            },
                    },
                    r::join_scan::endpoint::kind_type::inclusive,
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_TRUE(ok());
    EXPECT_TRUE(r);
}

TEST_F(expression_analyzer_comparison_test, join_scan_blob) {
    using testee = t::blob;
    auto t1 = storages_.add_table(storage::table {
            "t1",
            {
                    { "c1", testee {} },
            },
    });
    auto i1 = storages_.add_index(storage::index {
            t1,
            "i1",
            {
                    { t1->columns()[0] },
            },
    });
    r::join_scan expr {
            r::join_scan::operator_kind_type::inner,
            bindings(i1),
            {
                    {
                            decl(testee {}),
                            bindings(t1->columns()[0]),
                    },
            },
            r::join_scan::endpoint {
                    {
                            r::join_scan::key {
                                    bindings(t1->columns()[0]),
                                    vref { decl(testee {}) },
                            },
                    },
                    r::join_scan::endpoint::kind_type::inclusive,
            },
            r::join_scan::endpoint {
                    {
                            r::join_scan::key {
                                    bindings(t1->columns()[0]),
                                    vref { decl(testee {}) },
                            },
                    },
                    r::join_scan::endpoint::kind_type::inclusive,
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_FALSE(ok());
    EXPECT_FALSE(r);
}

TEST_F(expression_analyzer_comparison_test, distinct_interval) {
    using testee = t::datetime_interval;
    r::intermediate::distinct expr {
            {
                    decl(testee {}),
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_TRUE(ok());
    EXPECT_TRUE(r);
}

TEST_F(expression_analyzer_comparison_test, distinct_blob) {
    using testee = t::blob;
    r::intermediate::distinct expr {
            {
                    decl(testee {}),
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_FALSE(ok());
    EXPECT_FALSE(r);
}

TEST_F(expression_analyzer_comparison_test, limit_group_key_interval) {
    using testee = t::datetime_interval;
    r::intermediate::limit expr {
            {},
            {
                    decl(testee {}),
            },
            {},
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_TRUE(ok());
    EXPECT_TRUE(r);
}

TEST_F(expression_analyzer_comparison_test, limit_group_key_blob) {
    using testee = t::blob;
    r::intermediate::limit expr {
            {},
            {
                    decl(testee {}),
            },
            {},
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_FALSE(ok());
    EXPECT_FALSE(r);
}

TEST_F(expression_analyzer_comparison_test, limit_sort_key_int) {
    using testee = t::int4;
    r::intermediate::limit expr {
            {},
            {},
            {
                    decl(testee {}),
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_TRUE(ok());
    EXPECT_TRUE(r);
}

TEST_F(expression_analyzer_comparison_test, limit_sort_key_interval) {
    using testee = t::datetime_interval;
    r::intermediate::limit expr {
            {},
            {},
            {
                    decl(testee {}),
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_FALSE(ok());
    EXPECT_FALSE(r);
}

TEST_F(expression_analyzer_comparison_test, union_interval) {
    using testee = t::datetime_interval;
    r::intermediate::union_ expr {
            r::set_quantifier::distinct,
            {
                    {
                            decl(testee {}),
                            decl(testee {}),
                            decl(testee {}),
                    },
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_TRUE(ok());
    EXPECT_TRUE(r);
}

TEST_F(expression_analyzer_comparison_test, union_blob) {
    using testee = t::blob;
    r::intermediate::union_ expr {
            r::set_quantifier::distinct,
            {
                    {
                            decl(testee {}),
                            decl(testee {}),
                            decl(testee {}),
                    },
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_FALSE(ok());
    EXPECT_FALSE(r);
}

TEST_F(expression_analyzer_comparison_test, union_blob_left) {
    using testee = t::blob;
    r::intermediate::union_ expr {
            r::set_quantifier::distinct,
            {
                    {
                            decl(testee {}),
                            {},
                            decl(testee {}),
                    },
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_FALSE(ok());
    EXPECT_FALSE(r);
}

TEST_F(expression_analyzer_comparison_test, union_right) {
    using testee = t::blob;
    r::intermediate::union_ expr {
            r::set_quantifier::distinct,
            {
                    {
                            {},
                            decl(testee {}),
                            decl(testee {}),
                    },
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_FALSE(ok());
    EXPECT_FALSE(r);
}

TEST_F(expression_analyzer_comparison_test, union_all) {
    using testee = t::blob;
    r::intermediate::union_ expr {
            r::set_quantifier::all,
            {
                    {
                            decl(testee {}),
                            decl(testee {}),
                            decl(testee {}),
                    },
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_TRUE(ok());
    EXPECT_TRUE(r);
}

TEST_F(expression_analyzer_comparison_test, intersection_interval) {
    using testee = t::datetime_interval;
    r::intermediate::intersection expr {
            r::set_quantifier::distinct,
            {
                    {
                            decl(testee {}),
                            decl(testee {}),
                    },
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_TRUE(ok());
    EXPECT_TRUE(r);
}

TEST_F(expression_analyzer_comparison_test, intersection_blob) {
    using testee = t::blob;
    r::intermediate::intersection expr {
            r::set_quantifier::distinct,
            {
                    {
                            decl(testee {}),
                            decl(testee {}),
                    },
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_FALSE(ok());
    EXPECT_FALSE(r);
}

TEST_F(expression_analyzer_comparison_test, difference_interval) {
    using testee = t::datetime_interval;
    r::intermediate::difference expr {
            r::set_quantifier::distinct,
            {
                    {
                            decl(testee {}),
                            decl(testee {}),
                    },
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_TRUE(ok());
    EXPECT_TRUE(r);
}

TEST_F(expression_analyzer_comparison_test, difference_blob) {
    using testee = t::blob;
    r::intermediate::difference expr {
            r::set_quantifier::distinct,
            {
                    {
                            decl(testee {}),
                            decl(testee {}),
                    },
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_FALSE(ok());
    EXPECT_FALSE(r);
}

TEST_F(expression_analyzer_comparison_test, aggregate_interval) {
    using testee = t::datetime_interval;
    auto f = bindings.aggregate_function({
            1,
            "f",
            t::blob {},
            { t::blob {} },
    });
    r::intermediate::aggregate expr {
            {
                    decl(testee {}),
            },
            {
                    {
                            f,
                            { decl(t::blob {}) },
                            decl(t::blob {}),
                    },
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_TRUE(ok());
    EXPECT_TRUE(r);
}

TEST_F(expression_analyzer_comparison_test, aggregate_blob) {
    using testee = t::blob;
    auto f = bindings.aggregate_function({
            1,
            "f",
            testee {},
            { testee {} },
    });
    r::intermediate::aggregate expr {
            {
                    decl(testee {}),
            },
            {
                    {
                            f,
                            { decl(t::blob {}) },
                            decl(t::blob {}),
                    },
            },
    };
    auto r = analyzer.resolve(expr, true, false, repo);
    EXPECT_FALSE(ok());
    EXPECT_FALSE(r);
}

TEST_F(expression_analyzer_comparison_test, group_group_key_interval) {
    using testee = t::datetime_interval;
    auto c1 = bindings.exchange_column("c1");
    auto c2 = bindings.exchange_column("c2");
    analyzer.variables().bind(c1, testee {});
    analyzer.variables().bind(c2, t::blob {});
    p::group step {
            {
                    c1,
                    c2, // BLOB but not a group/sort key
            },
            {
                    c1,
            },
            {},
    };
    auto r = analyzer.resolve(step, true, false, repo);
    EXPECT_TRUE(ok());
    EXPECT_TRUE(r);
}

TEST_F(expression_analyzer_comparison_test, group_group_key_blob) {
    using testee = t::blob;
    auto c1 = bindings.exchange_column("c1");
    analyzer.variables().bind(c1, testee {});
    p::group step {
            {
                    c1,
            },
            {
                    c1,
            },
            {},
    };
    auto r = analyzer.resolve(step, true, false, repo);
    EXPECT_FALSE(ok());
    EXPECT_FALSE(r);
}

TEST_F(expression_analyzer_comparison_test, group_sort_key_int) {
    using testee = t::int4;
    auto c1 = bindings.exchange_column("c1");
    analyzer.variables().bind(c1, testee {});
    p::group step {
            {
                    c1,
            },
            {},
            {
                    c1,
            },
    };
    auto r = analyzer.resolve(step, true, false, repo);
    EXPECT_TRUE(ok());
    EXPECT_TRUE(r);
}

TEST_F(expression_analyzer_comparison_test, group_sort_key_interval) {
    using testee = t::datetime_interval;
    auto c1 = bindings.exchange_column("c1");
    analyzer.variables().bind(c1, testee {});
    p::group step {
            {
                    c1,
            },
            {},
            {
                    c1,
            },
    };
    auto r = analyzer.resolve(step, true, false, repo);
    EXPECT_FALSE(ok());
    EXPECT_FALSE(r);
}

TEST_F(expression_analyzer_comparison_test, broadcast_blob) {
    using testee = t::blob;
    auto c1 = bindings.exchange_column("c1");
    analyzer.variables().bind(c1, testee {});
    p::broadcast step {
            {
                    c1, // BLOB but not a group/sort key
            },
    };
    auto r = analyzer.resolve(step, true, false, repo);
    EXPECT_TRUE(ok());
    EXPECT_TRUE(r);
}

TEST_F(expression_analyzer_comparison_test, aggregate_group_key_interval) {
    using testee = t::datetime_interval;
    auto c1 = bindings.exchange_column("c1");
    auto c2 = bindings.exchange_column("c2");
    auto c3 = bindings.exchange_column("c3");
    analyzer.variables().bind(c1, testee {});
    analyzer.variables().bind(c2, t::blob {});
    analyzer.variables().bind(c3, t::blob {});
    auto f = bindings.aggregate_function({
            1,
            "f",
            t::blob {},
            { t::blob {} },
    });
    p::aggregate step {
            {
                    c1,
                    c2, // BLOB source column but not a group/sort key
            },
            {
                    c3, // BLOB destination column
            },
            {
                    c1,
            },
            {
                    {
                            f,
                            { c2 },
                            c3,
                    },
            },
    };
    auto r = analyzer.resolve(step, true, false, repo);
    EXPECT_TRUE(ok());
    EXPECT_TRUE(r);
}

TEST_F(expression_analyzer_comparison_test, aggregate_group_key_blob) {
    using testee = t::blob;
    auto c1 = bindings.exchange_column("c1");
    auto c2 = bindings.exchange_column("c2");
    auto c3 = bindings.exchange_column("c3");
    analyzer.variables().bind(c1, testee {});
    analyzer.variables().bind(c2, t::blob {});
    analyzer.variables().bind(c3, t::blob {});
    auto f = bindings.aggregate_function({
            1,
            "f",
            t::blob {},
            { t::blob {} },
    });
    p::aggregate step {
            {
                    c1,
                    c2, // BLOB source column but not a group/sort key
            },
            {
                    c3, // BLOB destination column
            },
            {
                    c1,
            },
            {
                    {
                            f,
                            { c2 },
                            c3,
                    },
            },
    };
    auto r = analyzer.resolve(step, true, false, repo);
    EXPECT_FALSE(ok());
    EXPECT_FALSE(r);
}

TEST_F(expression_analyzer_comparison_test, create_table_primary_key_scan_int) {
    using testee = t::int4;
    auto schema = std::make_shared<schema::declaration>("s");
    auto table = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T0",
            {
                    { "C0", testee {} },
                    { "C1", testee {} },
            },
    });
    auto index = std::make_shared<storage::index>(storage::index {
            table,
            "I0",
            { table->columns()[0] },
            {},
            {
                    storage::index_feature::primary,
                    storage::index_feature::find,
                    storage::index_feature::scan,
                    storage::index_feature::unique_constraint,
            },
    });
    st::create_table stmt {
            bindings(schema),
            bindings(table),
            bindings(index),
    };
    auto r = analyzer.resolve(stmt, true);
    EXPECT_TRUE(r);
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_comparison_test, create_table_primary_key_scan_interval) {
    using testee = t::datetime_interval;
    auto schema = std::make_shared<schema::declaration>("s");
    auto table = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T0",
            {
                    { "C0", testee {} },
                    { "C1", testee {} },
            },
    });
    auto index = std::make_shared<storage::index>(storage::index {
            table,
            "I0",
            { table->columns()[0] },
            {},
            {
                    storage::index_feature::primary,
                    storage::index_feature::find,
                    storage::index_feature::scan,
                    storage::index_feature::unique_constraint,
            },
    });
    st::create_table stmt {
            bindings(schema),
            bindings(table),
            bindings(index),
    };
    auto r = analyzer.resolve(stmt, true);
    EXPECT_FALSE(r);
    EXPECT_FALSE(ok());
}

TEST_F(expression_analyzer_comparison_test, create_table_primary_key_find_interval) {
    using testee = t::datetime_interval;
    auto schema = std::make_shared<schema::declaration>("s");
    auto table = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T0",
            {
                    { "C0", testee {} },
                    { "C1", testee {} },
            },
    });
    auto index = std::make_shared<storage::index>(storage::index {
            table,
            "I0",
            { table->columns()[0] },
            {},
            {
                    storage::index_feature::primary,
                    storage::index_feature::find,
                    storage::index_feature::unique_constraint,
            },
    });
    st::create_table stmt {
            bindings(schema),
            bindings(table),
            bindings(index),
    };
    auto r = analyzer.resolve(stmt, true);
    EXPECT_TRUE(r);
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_comparison_test, create_table_primary_key_find_blob) {
    using testee = t::blob;
    auto schema = std::make_shared<schema::declaration>("s");
    auto table = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T0",
            {
                    { "C0", testee {} },
                    { "C1", testee {} },
            },
    });
    auto index = std::make_shared<storage::index>(storage::index {
            table,
            "I0",
            { table->columns()[0] },
            {},
            {
                    storage::index_feature::primary,
                    storage::index_feature::find,
                    storage::index_feature::unique_constraint,
            },
    });
    st::create_table stmt {
            bindings(schema),
            bindings(table),
            bindings(index),
    };
    auto r = analyzer.resolve(stmt, true);
    EXPECT_FALSE(r);
    EXPECT_FALSE(ok());
}

TEST_F(expression_analyzer_comparison_test, create_index_scan_int) {
    using testee = t::int4;
    auto schema = std::make_shared<schema::declaration>("s");
    auto table = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T0",
            {
                    { "C0", testee {} },
                    { "C1", testee {} },
            },
    });
    auto index = std::make_shared<storage::index>(storage::index {
            table,
            "I0",
            { table->columns()[1] },
            {},
            {
                    storage::index_feature::primary,
                    storage::index_feature::find,
                    storage::index_feature::scan,
                    storage::index_feature::unique_constraint,
            },
    });
    st::create_index stmt {
            bindings(schema),
            bindings(index),
    };
    auto r = analyzer.resolve(stmt, true);
    EXPECT_TRUE(r);
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_comparison_test, create_index_scan_interval) {
    using testee = t::datetime_interval;
    auto schema = std::make_shared<schema::declaration>("s");
    auto table = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T0",
            {
                    { "C0", testee {} },
                    { "C1", testee {} },
            },
    });
    auto index = std::make_shared<storage::index>(storage::index {
            table,
            "I0",
            { table->columns()[1] },
            {},
            {
                    storage::index_feature::primary,
                    storage::index_feature::find,
                    storage::index_feature::scan,
                    storage::index_feature::unique_constraint,
            },
    });
    st::create_index stmt {
            bindings(schema),
            bindings(index),
    };
    auto r = analyzer.resolve(stmt, true);
    EXPECT_FALSE(r);
    EXPECT_FALSE(ok());
}

TEST_F(expression_analyzer_comparison_test, create_index_find_interval) {
    using testee = t::datetime_interval;
    auto schema = std::make_shared<schema::declaration>("s");
    auto table = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T0",
            {
                    { "C0", testee {} },
                    { "C1", testee {} },
            },
    });
    auto index = std::make_shared<storage::index>(storage::index {
            table,
            "I0",
            { table->columns()[1] },
            {},
            {
                    storage::index_feature::primary,
                    storage::index_feature::find,
                    storage::index_feature::unique_constraint,
            },
    });
    st::create_index stmt {
            bindings(schema),
            bindings(index),
    };
    auto r = analyzer.resolve(stmt, true);
    EXPECT_TRUE(r);
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_comparison_test, create_index_find_blob) {
    using testee = t::blob;
    auto schema = std::make_shared<schema::declaration>("s");
    auto table = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T0",
            {
                    { "C0", testee {} },
                    { "C1", testee {} },
            },
    });
    auto index = std::make_shared<storage::index>(storage::index {
            table,
            "I0",
            { table->columns()[1] },
            {},
            {
                    storage::index_feature::primary,
                    storage::index_feature::find,
                    storage::index_feature::unique_constraint,
            },
    });
    st::create_index stmt {
            bindings(schema),
            bindings(index),
    };
    auto r = analyzer.resolve(stmt, true);
    EXPECT_FALSE(r);
    EXPECT_FALSE(ok());
}

} // namespace yugawara::analyzer

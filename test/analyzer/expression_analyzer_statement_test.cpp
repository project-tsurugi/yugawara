#include <yugawara/analyzer/expression_analyzer.h>

#include <gtest/gtest.h>

#include <takatori/document/basic_document.h>

#include <takatori/type/primitive.h>
#include <takatori/value/primitive.h>
#include <takatori/util/optional_ptr.h>

#include <takatori/scalar/variable_reference.h>

#include <takatori/relation/scan.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/step/take_flat.h>
#include <takatori/relation/step/offer.h>

#include <takatori/plan/process.h>
#include <takatori/plan/forward.h>

#include <takatori/statement/execute.h>
#include <takatori/statement/write.h>

#include <yugawara/binding/factory.h>
#include <yugawara/extension/type/error.h>

#include <yugawara/storage/table.h>
#include <yugawara/storage/index.h>
#include <yugawara/storage/configurable_provider.h>
#include <takatori/scalar/immediate.h>

namespace yugawara::analyzer {

namespace scalar = ::takatori::scalar;
namespace relation = ::takatori::relation;
namespace plan = ::takatori::plan;
namespace statement = ::takatori::statement;
namespace t = ::takatori::type;
namespace v = ::takatori::value;

using code = type_diagnostic::code_type;
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
    type::repository repo { {}, true };
    binding::factory bindings;

    storage::configurable_provider storages_;
    std::shared_ptr<storage::table> t0 = storages_.add_table("t0", {
            "T0",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    std::shared_ptr<storage::table> t1 = storages_.add_table("t1", {
            "T1",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    std::shared_ptr<storage::index> i0 = storages_.add_index("I0", { t0, "I0", });
    std::shared_ptr<storage::index> i1 = storages_.add_index("I1", { t1, "I1" });

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

} // namespace yugawara::analyzer

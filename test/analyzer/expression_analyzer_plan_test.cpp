#include <yugawara/analyzer/expression_analyzer.h>

#include <gtest/gtest.h>

#include <takatori/document/basic_document.h>

#include <takatori/type/primitive.h>
#include <takatori/type/decimal.h>
#include <takatori/util/optional_ptr.h>

#include <takatori/scalar/variable_reference.h>

#include <takatori/relation/intermediate/dispatch.h>
#include <takatori/relation/step/offer.h>

#include <takatori/plan/process.h>
#include <takatori/plan/forward.h>
#include <takatori/plan/group.h>
#include <takatori/plan/aggregate.h>
#include <takatori/plan/broadcast.h>
#include <takatori/plan/discard.h>

#include <yugawara/binding/factory.h>
#include <yugawara/extension/type/error.h>

#include <yugawara/storage/table.h>
#include <yugawara/storage/index.h>
#include <yugawara/storage/configurable_provider.h>

#include <yugawara/aggregate/configurable_provider.h>

namespace yugawara::analyzer {

namespace s = ::takatori::scalar;
namespace r = ::takatori::relation;
namespace rs = ::takatori::relation::step;
namespace p = ::takatori::plan;
namespace t = ::takatori::type;
namespace v = ::takatori::value;

namespace ex = ::yugawara::extension::type;

using code = type_diagnostic::code_type;
using vref = ::takatori::scalar::variable_reference;

using G = ::takatori::graph::graph<r::expression>;

class expression_analyzer_plan_test : public ::testing::Test {
public:
    bool ok() noexcept {
        return !analyzer.has_diagnostics();
    }

    ::takatori::descriptor::variable decl(::takatori::type::data&& type) {
        auto var = bindings.local_variable();
        analyzer.variables().bind(var, std::move(type));
        return var;
    }

    ::takatori::type::data const& type(::takatori::descriptor::variable const& variable) {
        auto t = analyzer.inspect(variable);
        if (t) {
            return *repo.get(*t);
        }
        static extension::type::error error;
        return error;
    }

    friend std::ostream& operator<<(std::ostream& out, expression_analyzer_plan_test const& t) {
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

    ::takatori::document::basic_document doc_ {
            "testing.sql",
            "01234567890123456789012345678901234567890123456789",
    };
    std::size_t doc_cursor_ {};
};

TEST_F(expression_analyzer_plan_test, process) {
    auto t1 = storages_.add_table("T1", storage::table {
            "T1",
            {
                    { "C1", t::int4() },
            },
    });
    auto i1 = storages_.add_index("I1", storage::index {
            t1,
            "I1",
    });
    auto&& cols = t1->columns();
    auto c1 = bindings.stream_variable();

    auto&& x1 = bindings.exchange_column();
    p::forward exchange { x1, };

    p::process step;
    auto&& g = step.operators();
    auto&& e1 = g.insert(r::scan {
            bindings.index(*i1),
            {
                    r::scan::column {
                            bindings.table_column(cols[0]),
                            c1,
                    }
            },
    });
    auto&& e2 = g.insert(rs::offer {
            bindings.exchange(exchange),
            {
                    { c1, x1, },
            },
    });
    e1.output() >> e2.input();

    auto b = analyzer.resolve(step, true, false, repo);
    ASSERT_TRUE(b);
    EXPECT_TRUE(ok());

    EXPECT_EQ(type(c1), t::int4());
    EXPECT_EQ(type(x1), t::int4());
}

TEST_F(expression_analyzer_plan_test, forward) {
    auto&& x1 = bindings.exchange_column();
    p::forward step { x1, };

    analyzer.variables().bind(x1, t::int4 {});

    auto b = analyzer.resolve(step, true, false, repo);
    ASSERT_TRUE(b);
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_plan_test, group) {
    auto&& x1 = bindings.exchange_column();
    p::group step {
            {
                    x1,
            },
            {
                    x1,
            },
            {
                    { x1, p::sort_direction::ascendant, },
            },
    };
    analyzer.variables().bind(x1, t::int4 {});

    auto b = analyzer.resolve(step, true, false, repo);
    ASSERT_TRUE(b);
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_plan_test, aggregate) {
    aggregate::configurable_provider p;
    auto f = p.add({
            aggregate::declaration::minimum_system_function_id + 1,
            "count",
            r::set_quantifier::all,
            t::int8 {},
            {
                    t::int4 {},
            }
    });

    auto&& k = bindings.exchange_column();
    auto&& v = bindings.exchange_column();
    auto&& a = bindings.exchange_column();
    p::aggregate step {
            {
                    k, v,
            },
            {
                    k, a,
            },
            {
                    k,
            },
            {
                    {
                            bindings.aggregate_function(f),
                            {
                                    v,
                            },
                            a,
                    },
            },
    };
    analyzer.variables().bind(k, t::int4 {});
    analyzer.variables().bind(v, t::int4 {});

    auto b = analyzer.resolve(step, true, false, repo);
    ASSERT_TRUE(b);
    EXPECT_TRUE(ok());

    EXPECT_EQ(type(a), t::int8());
}

TEST_F(expression_analyzer_plan_test, broadcast) {
    auto&& x1 = bindings.exchange_column();
    p::broadcast step { x1, };

    analyzer.variables().bind(x1, t::int4 {});

    auto b = analyzer.resolve(step, true, false, repo);
    ASSERT_TRUE(b);
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_plan_test, discard) {
    p::discard step;

    auto b = analyzer.resolve(step, true, false, repo);
    ASSERT_TRUE(b);
    EXPECT_TRUE(ok());
}

} // namespace yugawara::analyzer

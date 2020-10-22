#include <yugawara/analyzer/expression_analyzer.h>

#include <gtest/gtest.h>

#include <takatori/document/basic_document.h>
#include <takatori/document/region.h>

#include <takatori/type/primitive.h>
#include <takatori/type/decimal.h>
#include <takatori/type/character.h>

#include <takatori/scalar/variable_reference.h>

#include <takatori/relation/scan.h>
#include <takatori/relation/values.h>
#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/join_find.h>
#include <takatori/relation/join_scan.h>
#include <takatori/relation/project.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/buffer.h>
#include <takatori/relation/intermediate/aggregate.h>
#include <takatori/relation/intermediate/distinct.h>
#include <takatori/relation/intermediate/limit.h>
#include <takatori/relation/intermediate/union.h>
#include <takatori/relation/intermediate/intersection.h>
#include <takatori/relation/intermediate/difference.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/write.h>
#include <takatori/relation/step/join.h>
#include <takatori/relation/step/aggregate.h>
#include <takatori/relation/step/intersection.h>
#include <takatori/relation/step/difference.h>
#include <takatori/relation/step/flatten.h>
#include <takatori/relation/step/take_flat.h>
#include <takatori/relation/step/take_group.h>
#include <takatori/relation/step/take_cogroup.h>
#include <takatori/relation/step/offer.h>

#include <takatori/util/optional_ptr.h>

#include <takatori/plan/forward.h>
#include <takatori/plan/group.h>

#include <yugawara/binding/factory.h>
#include <yugawara/extension/type/error.h>

#include <yugawara/storage/table.h>
#include <yugawara/storage/index.h>
#include <yugawara/storage/configurable_provider.h>

#include <yugawara/aggregate/configurable_provider.h>
#include <takatori/scalar/immediate.h>

namespace yugawara::analyzer {

namespace s = ::takatori::scalar;
namespace r = ::takatori::relation;
namespace ri = ::takatori::relation::intermediate;
namespace rs = ::takatori::relation::step;
namespace t = ::takatori::type;
namespace v = ::takatori::value;
namespace p = ::takatori::plan;

namespace ex = ::yugawara::extension::type;

using code = expression_analyzer_code;
using vref = ::takatori::scalar::variable_reference;

class expression_analyzer_relation_test : public ::testing::Test {
public:
    bool ok() noexcept {
        return !analyzer.has_diagnostics();
    }

    template<class T>
    T&& bless(T&& t) noexcept {
        t.region() = { doc_, doc_cursor_ };
        return std::forward<T>(t);
    }

    ::takatori::util::optional_ptr<expression_analyzer::diagnostic_type const> find(
            ::takatori::document::region region,
            expression_analyzer_code code) {
        if (!region) {
            throw std::invalid_argument("unknown region");
        }
        for (auto&& d : analyzer.diagnostics()) {
            if (code == d.code() && region == d.location()) {
                return d;
            }
        }
        return {};
    }

    ::takatori::descriptor::variable decl(::takatori::type::data&& type) {
        auto var = bindings.local_variable();
        analyzer.variables().bind(var, std::move(type));
        return var;
    }

    bool eq(::takatori::descriptor::variable const& a, ::takatori::descriptor::variable const& b) {
        auto&& r1 = analyzer.variables().find(a);
        auto&& r2 = analyzer.variables().find(b);
        if (!r1 || !r2) return false;
        return r1 == r2;
    }

    ::takatori::type::data const& type(::takatori::descriptor::variable const& variable) {
        auto t = analyzer.inspect(variable);
        if (t) {
            return *repo.get(*t);
        }
        static extension::type::error error;
        return error;
    }

    ::takatori::type::data const& type(::takatori::scalar::expression const& expr) {
        auto t = analyzer.expressions().find(expr);
        if (t) {
            return *repo.get(t.type());
        }
        static extension::type::error error;
        return error;
    }

    variable_resolution const& resolve(::takatori::descriptor::variable const& variable) {
        return analyzer.variables().find(variable);
    }

    friend std::ostream& operator<<(std::ostream& out, expression_analyzer_relation_test const& t) {
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

TEST_F(expression_analyzer_relation_test, scan) {
    auto t1 = storages_.add_table(storage::table {
            "T1",
            {
                    { "C1", t::int4() },
            },
    });
    auto i1 = storages_.add_index(storage::index {
            t1,
            "I1",
    });
    auto&& cols = t1->columns();
    auto c1 = bindings.stream_variable();
    r::scan expr {
            bindings(*i1),
            {
                    r::scan::column {
                            bindings(cols[0]),
                            c1,
                    }
            },
    };

    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b);
    EXPECT_TRUE(ok());

    EXPECT_EQ(type(expr.columns()[0].source()), t::int4());
    EXPECT_EQ(resolve(expr.columns()[0].source()), resolve(expr.columns()[0].destination()));
}

TEST_F(expression_analyzer_relation_test, values) {
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    r::values expr {
            { c0, c1, },
            {
                    {
                            vref { decl(t::int4 {}) },
                            vref { decl(t::boolean {}) },
                    },
            },
    };

    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b);
    EXPECT_TRUE(ok());

    EXPECT_EQ(type(expr.rows()[0].elements()[0]), t::int4());
    EXPECT_EQ(type(expr.rows()[0].elements()[1]), t::boolean());
    EXPECT_EQ(resolve(expr.columns()[0]), expr.rows()[0].elements()[0]);
    EXPECT_EQ(resolve(expr.columns()[1]), expr.rows()[0].elements()[1]);
}

TEST_F(expression_analyzer_relation_test, values_multirow) {
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    r::values expr {
            { c0, c1, },
            {
                    {
                            vref { decl(t::int4 {}) },
                            vref { decl(t::boolean {}) },
                    },
                    {
                            vref { decl(t::int8 {}) },
                            vref { decl(t::unknown {}) },
                    },
            },
    };

    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b);
    EXPECT_TRUE(ok());

    EXPECT_EQ(type(expr.rows()[0].elements()[0]), t::int4());
    EXPECT_EQ(type(expr.rows()[0].elements()[1]), t::boolean());
    EXPECT_EQ(type(expr.rows()[1].elements()[0]), t::int8());
    EXPECT_EQ(type(expr.rows()[1].elements()[1]), t::unknown());
    EXPECT_EQ(type(expr.columns()[0]), t::int8());
    EXPECT_EQ(type(expr.columns()[1]), t::boolean());
}

TEST_F(expression_analyzer_relation_test, values_inconsistent_type) {
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    r::values expr {
            { c0, },
            {
                    {
                            vref { decl(t::boolean {}) },
                    },
                    {
                            vref { decl(t::character { 1 }) },
                    },
            },
    };

    bless(expr.rows()[1].elements()[0]);
    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_FALSE(b);
    EXPECT_TRUE(find(expr.rows()[1].elements()[0].region(), code::inconsistent_type));
}

TEST_F(expression_analyzer_relation_test, values_inconsistent_elements_exval) {
    auto c0 = bindings.stream_variable("c0");
    r::values expr {
            { c0, },
            {
                    {
                            vref { decl(t::boolean {}) },
                            vref { decl(t::boolean {}) },
                    },
            },
    };

    bless(expr.rows()[0].elements()[1]);
    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_FALSE(b);
    EXPECT_TRUE(find(expr.rows()[0].elements()[1].region(), code::inconsistent_elements));
}

TEST_F(expression_analyzer_relation_test, values_inconsistent_elements_excol) {
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    r::values expr {
            { c0, c1, },
            {
                    {
                            vref { decl(t::boolean {}) },
                    },
            },
    };

    bless(expr.rows()[0].elements()[0]);
    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_FALSE(b);
    EXPECT_TRUE(find(expr.rows()[0].elements()[0].region(), code::inconsistent_elements));
}

TEST_F(expression_analyzer_relation_test, join) {
    ri::join expr {
            ri::join::operator_kind_type::inner,
            vref { decl(t::boolean {}) },
    };
    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b);
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_relation_test, join_find) {
    auto t1 = storages_.add_table(storage::table {
            "T1",
            {
                    { "C1", t::int4() },
            },
    });
    auto i1 = storages_.add_index(storage::index {
            t1,
            "I1",
    });
    auto&& cols = t1->columns();
    auto c1 = bindings.stream_variable();
    r::join_find expr {
            r::join_find::operator_kind_type::inner,
            bindings(*i1),
            {
                    r::join_find::column {
                            bindings(cols[0]),
                            c1,
                    }
            },
            {
                    r::join_find::key {
                            bindings(cols[0]),
                            vref { decl(t::int4 {}) },
                    }
            },
            vref { decl(t::boolean {}) },
    };
    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b) << *this;
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_relation_test, join_scan) {
    auto t1 = storages_.add_table(storage::table {
            "T1",
            {
                    { "C1", t::int4() },
            },
    });
    auto i1 = storages_.add_index(storage::index {
            t1,
            "I1",
    });
    auto&& cols = t1->columns();
    auto c1 = bindings.stream_variable();
    r::join_scan expr {
            r::join_scan::operator_kind_type::inner,
            bindings(*i1),
            {
                    r::join_scan::column {
                            bindings(cols[0]),
                            c1,
                    }
            },
            {
                    {
                            r::join_scan::key {
                                    bindings(cols[0]),
                                    vref { decl(t::int4 {}) },
                            },
                    },
                    r::join_scan::endpoint::kind_type::inclusive,
            },
            {
                    {
                            r::join_scan::key {
                                    bindings(cols[0]),
                                    vref { decl(t::int4 {}) },
                            },
                    },
                    r::join_scan::endpoint::kind_type::exclusive,
            },
            vref { decl(t::boolean {}) },
    };
    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b) << *this;
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_relation_test, project) {
    auto c1 = bindings.stream_variable();
    r::project expr {
            {
                    r::project::column {
                            c1,
                            vref { decl(t::int4 {}) }
                    },
            }
    };
    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b) << *this;
    EXPECT_TRUE(ok());
    EXPECT_EQ(type(c1), t::int4());
}

TEST_F(expression_analyzer_relation_test, filter) {
    r::filter expr {
            vref { decl(t::boolean {}) }
    };
    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b) << *this;
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_relation_test, buffer) {
    r::buffer expr { 2 };
    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b) << *this;
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_relation_test, aggregate) {
    aggregate::configurable_provider p;
    auto f = p.add({
            aggregate::declaration::minimum_system_function_id + 1,
            "count",
            t::int8 {},
            {
                    t::character { t::varying },
            }
    });

    auto c1 = bindings.stream_variable();
    ri::aggregate expr {
            {
                    decl(t::int4 {}),
            },
            {
                    ri::aggregate::column {
                            bindings.aggregate_function(f),
                            {
                                    decl(t::character { t::varying }),
                            },
                            c1,
                    },
            },
    };

    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b) << *this;
    EXPECT_TRUE(ok());
    EXPECT_EQ(type(c1), t::int8());
}

TEST_F(expression_analyzer_relation_test, distinct) {
    ri::distinct expr {
            decl(t::int4 {}),
            decl(t::int4 {}),
            decl(t::int4 {}),
    };
    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b) << *this;
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_relation_test, limit) {
    ri::limit expr {
            1,
            {
                    decl(t::int4 {}),
                    decl(t::int4 {}),
                    decl(t::int4 {}),
            },
    };
    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b) << *this;
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_relation_test, union) {
    auto c0 = bindings.stream_variable();
    auto c1 = bindings.stream_variable();
    auto c2 = bindings.stream_variable();
    auto c3 = bindings.stream_variable();
    ri::union_ expr {
            {
                    decl(t::int4 {}),
                    {},
                    c0,
            },
            {
                    {},
                    decl(t::int4 {}),
                    c1,
            },
            {
                    decl(t::int4 {}),
                    decl(t::int4 {}),
                    c2,
            },
            {
                    decl(t::int4 {}),
                    decl(t::int8 {}),
                    c3,
            },
    };
    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b) << *this;
    EXPECT_TRUE(ok());

    EXPECT_EQ(type(c0), t::int4());
    EXPECT_EQ(type(c1), t::int4());
    EXPECT_EQ(type(c2), t::int4());
    EXPECT_EQ(type(c3), t::int8());
}

TEST_F(expression_analyzer_relation_test, intersection) {
    ri::intersection expr {
            {
                    decl(t::int4 {}),
                    decl(t::int4 {}),
            },
            {
                    decl(t::int4 {}),
                    decl(t::int8 {}),
            },
            {
                    decl(t::int8 {}),
                    decl(t::int4 {}),
            },
    };
    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b) << *this;
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_relation_test, difference) {
    ri::difference expr {
            {
                    decl(t::int4 {}),
                    decl(t::int4 {}),
            },
            {
                    decl(t::int4 {}),
                    decl(t::int8 {}),
            },
            {
                    decl(t::int8 {}),
                    decl(t::int4 {}),
            },
    };
    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b) << *this;
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_relation_test, emit) {
    r::emit expr {
            {
                    decl(t::int4 {}),
                    decl(t::int4 {}),
            },
    };
    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b) << *this;
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_relation_test, write) {
    auto t1 = storages_.add_table(storage::table {
            "T1",
            {
                    { "C1", t::int4() },
            },
    });
    auto i1 = storages_.add_index(storage::index {
            t1,
            "I1",
    });
    r::write expr {
            r::write::operator_kind_type::insert,
            bindings(*i1),
            {
                    {
                            decl(t::int4 {}),
                            bindings(t1->columns()[0]),
                    },
            },
            {
                    {
                            decl(t::int4 {}),
                            bindings(t1->columns()[0]),
                    },
            },
    };
    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b) << *this;
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_relation_test, join_group) {
    rs::join expr {
            rs::join::operator_kind_type::inner,
            vref { decl(t::boolean {}) },
    };
    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b) << *this;
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_relation_test, aggregate_group) {
    aggregate::configurable_provider p;
    auto f = p.add({
            aggregate::declaration::minimum_system_function_id + 1,
            "count",
            t::int8 {},
            {
                    t::character { t::varying },
            }
    });
    ;
    auto c1 = bindings.stream_variable();
    rs::aggregate expr {
            {
                    ri::aggregate::column {
                            bindings.aggregate_function(f),
                            {
                                    decl(t::character { t::varying }),
                            },
                            c1,
                    },
            },
    };

    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b) << *this;
    EXPECT_TRUE(ok());
    EXPECT_EQ(type(c1), t::int8());
}

TEST_F(expression_analyzer_relation_test, intersection_group) {
    rs::intersection expr {};

    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b) << *this;
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_relation_test, difference_group) {
    rs::difference expr {};

    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b) << *this;
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_relation_test, flatten_group) {
    rs::flatten expr {};

    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b) << *this;
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_relation_test, take_flat) {
    auto&& c1 = bindings.exchange_column();
    analyzer.variables().bind(c1, t::int4 {});
    p::forward exchange {
            c1,
    };
    auto v1 = bindings.stream_variable();
    rs::take_flat expr {
            bindings(exchange),
            {
                    { c1, v1, },
            }
    };

    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b) << *this;
    EXPECT_TRUE(ok());

    EXPECT_EQ(resolve(v1), resolve(c1));
}

TEST_F(expression_analyzer_relation_test, take_group) {
    auto&& c1 = bindings.exchange_column();
    analyzer.variables().bind(c1, t::int4 {});
    p::group exchange {
            { c1, },
            { c1, },
            {},
            {},
            p::group::mode_type::equivalence,
    };
    auto v1 = bindings.stream_variable();
    rs::take_group expr {
            bindings(exchange),
            {
                    { c1, v1, },
            }
    };

    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b) << *this;
    EXPECT_TRUE(ok());

    EXPECT_EQ(resolve(v1), resolve(c1));
}

TEST_F(expression_analyzer_relation_test, take_cogroup) {
    auto&& c1 = bindings.exchange_column();
    auto&& c2 = bindings.exchange_column();
    analyzer.variables().bind(c1, t::int4 {});
    analyzer.variables().bind(c2, t::int8 {});
    p::group ex1 {
            { c1, },
            { c1, },
            {},
            {},
            p::group::mode_type::equivalence,
    };
    p::group ex2 {
            { c2, },
            { c2, },
            {},
            {},
            p::group::mode_type::equivalence,
    };
    auto v1 = bindings.stream_variable();
    auto v2 = bindings.stream_variable();
    rs::take_cogroup expr {
            {
                    bindings(ex1),
                    {
                            { c1, v1, },
                    }
            },
            {
                    bindings(ex2),
                    {
                            { c2, v2, },
                    }
            },
    };

    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b) << *this;
    EXPECT_TRUE(ok());

    EXPECT_EQ(resolve(v1), resolve(c1));
    EXPECT_EQ(resolve(v2), resolve(c2));
}

TEST_F(expression_analyzer_relation_test, offer) {
    auto&& c1 = bindings.exchange_column();
    p::forward exchange {
            c1,
    };
    rs::offer expr {
            bindings(exchange),
            {
                    { decl(t::int4 {}), c1, },
            }
    };

    auto b = analyzer.resolve(expr, true, false, repo);
    ASSERT_TRUE(b) << *this;
    EXPECT_TRUE(ok());

    EXPECT_EQ(type(c1), t::int4 {});
}

} // namespace yugawara::analyzer

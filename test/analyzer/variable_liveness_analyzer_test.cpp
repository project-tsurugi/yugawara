#include <yugawara/analyzer/variable_liveness_analyzer.h>

#include <gtest/gtest.h>

#include <takatori/graph/graph.h>

#include <takatori/scalar/let.h>
#include <takatori/scalar/variable_reference.h>

#include <takatori/relation/filter.h>
#include <takatori/relation/project.h>
#include <takatori/relation/buffer.h>
#include <takatori/relation/step/offer.h>
#include <takatori/relation/step/take_flat.h>

#include <takatori/plan/forward.h>

#include <yugawara/binding/factory.h>

#include <yugawara/analyzer/block_builder.h>
#include <yugawara/analyzer/block_algorithm.h>

#include "../error_set.h"

namespace yugawara::analyzer {

using testing::error_set;

class variable_liveness_analyzer_test : public ::testing::Test {
public:
    error_set const no_error {};
    
    template<class C>
    static error_set contains(C& container, std::initializer_list<::takatori::descriptor::variable> variables) {
        error_set errors;
        for (auto&& v : variables) {
            if (container.find(v) == container.end()) {
                errors << v;
            }
        }
        return errors;
    }

    template<class C>
    static testing::error_set eq(C& container, std::initializer_list<::takatori::descriptor::variable> variables) {
        error_set errors;
        if (container.size() != variables.size()) {
            for (auto&& v : container) {
                if (std::find(variables.begin(), variables.end(), v) == variables.end()) {
                    errors << "<" << v;
                }
            }
            for (auto&& v : variables) {
                if (container.find(v) == container.end()) {
                    errors << ">" << v;
                }
            }
        }
        return errors;
    }

    binding::factory bindings;
    ::takatori::plan::forward f1 {
            bindings.exchange_column(),
            bindings.exchange_column(),
            bindings.exchange_column(),
    };
    ::takatori::plan::forward f2 {
            bindings.exchange_column(),
            bindings.exchange_column(),
            bindings.exchange_column(),
    };
    ::takatori::plan::forward f3 {
            bindings.exchange_column(),
            bindings.exchange_column(),
            bindings.exchange_column(),
    };
};

namespace relation = ::takatori::relation;
namespace scalar = ::takatori::scalar;
using take = relation::step::take_flat;
using offer = relation::step::offer;
using buffer = relation::buffer;

using rgraph = ::takatori::graph::graph<relation::expression>;

TEST_F(variable_liveness_analyzer_test, simple) {
    rgraph rg;

    auto&& c1 = bindings.stream_variable("c1");
    auto&& c2 = bindings.stream_variable("c2");
    auto&& c3 = bindings.stream_variable("c3");
    auto&& r1 = rg.insert(take {
            bindings.exchange(f1),
            {
                    { f1.columns()[0], c1 },
                    { f1.columns()[1], c2 },
                    { f1.columns()[2], c3 },
            },
    });
    auto&& r2 = rg.insert(offer {
            bindings.exchange(f2),
            {
                    { c1, f2.columns()[0] },
                    { c1, f2.columns()[1] },
                    { c1, f2.columns()[2] },
            },
    });
    r1.output() >> r2.input();

    auto bg = block_builder::build(rg);
    variable_liveness_analyzer analyzer { bg };

    ASSERT_EQ(bg.size(), 1);
    auto&& b0 = *find_unique_head(bg); // r1 .. r2
    auto&& n0 = analyzer.inspect(b0);

    EXPECT_EQ(eq(n0.define(), {
            c1,
            c2,
            c3,
    }), no_error);
    EXPECT_EQ(eq(n0.use(), {
            c1,
            f1.columns()[0],
            f1.columns()[1],
            f1.columns()[2],
    }), no_error);
    EXPECT_EQ(eq(n0.kill(), {
            c2,
            c3,
    }), no_error);
}

TEST_F(variable_liveness_analyzer_test, filter) {
    rgraph rg;

    auto&& c1 = bindings.stream_variable("c1");
    auto&& c2 = bindings.stream_variable("c2");
    auto&& c3 = bindings.stream_variable("c3");
    auto&& r1 = rg.insert(take {
            bindings.exchange(f1),
            {
                    { f1.columns()[0], c1 },
                    { f1.columns()[1], c2 },
                    { f1.columns()[2], c3 },
            },
    });
    auto&& r2 = rg.insert(relation::filter {
            scalar::variable_reference { c2 }
    });
    auto&& r3 = rg.insert(offer {
            bindings.exchange(f2),
            {
                    { c1, f2.columns()[0] },
                    { c1, f2.columns()[1] },
                    { c1, f2.columns()[2] },
            },
    });
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    auto bg = block_builder::build(rg);
    variable_liveness_analyzer analyzer { bg };

    ASSERT_EQ(bg.size(), 1);
    auto&& b0 = *find_unique_head(bg); // r1 .. r3
    auto&& n0 = analyzer.inspect(b0);

    EXPECT_EQ(eq(n0.define(), {
            c1,
            c2,
            c3,
    }), no_error);

    EXPECT_EQ(eq(n0.use(), {
            c1,
            c2,
            f1.columns()[0],
            f1.columns()[1],
            f1.columns()[2],
    }), no_error);

    EXPECT_EQ(eq(n0.kill(), {
            c3,
    }), no_error);
}

TEST_F(variable_liveness_analyzer_test, let) {
    rgraph rg;

    auto&& c1 = bindings.stream_variable("c1");
    auto&& c2 = bindings.stream_variable("c2");
    auto&& c3 = bindings.stream_variable("c3");
    auto&& r1 = rg.insert(take {
            bindings.exchange(f1),
            {
                    { f1.columns()[0], c1 },
                    { f1.columns()[1], c2 },
                    { f1.columns()[2], c3 },
            },
    });
    auto&& l1 = bindings.local_variable("l1");
    auto&& r2 = rg.insert(relation::filter {
            scalar::let {
                    scalar::let::declarator {
                            scalar::variable_reference { c2 },
                            l1,
                    },
                    scalar::variable_reference { l1 },
            },
    });
    auto&& r3 = rg.insert(offer {
            bindings.exchange(f2),
            {
                    { c1, f2.columns()[0] },
                    { c1, f2.columns()[1] },
                    { c1, f2.columns()[2] },
            },
    });
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    auto bg = block_builder::build(rg);
    variable_liveness_analyzer analyzer { bg };

    ASSERT_EQ(bg.size(), 1);
    auto&& b0 = *find_unique_head(bg); // r1 .. r3
    auto&& n0 = analyzer.inspect(b0);

    EXPECT_EQ(eq(n0.define(), {
            c1,
            c2,
            c3,
            l1,
    }), no_error);

    EXPECT_EQ(eq(n0.use(), {
            c1,
            c2,
            l1,
            f1.columns()[0],
            f1.columns()[1],
            f1.columns()[2],
    }), no_error);

    EXPECT_EQ(eq(n0.kill(), {
            c3,
    }), no_error);
}

TEST_F(variable_liveness_analyzer_test, project) {
    rgraph rg;

    auto&& c1 = bindings.stream_variable();
    auto&& c2 = bindings.stream_variable();
    auto&& c3 = bindings.stream_variable();
    auto&& r1 = rg.insert(take {
            bindings.exchange(f1),
            {
                    { f1.columns()[0], c1 },
                    { f1.columns()[1], c2 },
                    { f1.columns()[2], c3 },
            },
    });
    auto&& p1 = bindings.stream_variable("p1");
    auto&& r2 = rg.insert(relation::project {
            relation::project::column {
                    scalar::variable_reference { c2 },
                    p1,
            },
    });
    auto&& r3 = rg.insert(offer {
            bindings.exchange(f2),
            {
                    { c1, f2.columns()[0] },
                    { p1, f2.columns()[1] },
                    { c1, f2.columns()[2] },
            },
    });
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    auto bg = block_builder::build(rg);
    variable_liveness_analyzer analyzer { bg };

    ASSERT_EQ(bg.size(), 1);
    auto&& b0 = *find_unique_head(bg); // r1 .. r3
    auto&& n0 = analyzer.inspect(b0);

    EXPECT_EQ(eq(n0.define(), {
            c1,
            c2,
            c3,
            p1,
    }), no_error);

    EXPECT_EQ(eq(n0.use(), {
            c1,
            c2,
            p1,
            f1.columns()[0],
            f1.columns()[1],
            f1.columns()[2],
    }), no_error);

    EXPECT_EQ(eq(n0.kill(), {
            c3,
    }), no_error);
}

TEST_F(variable_liveness_analyzer_test, buffer) {
    rgraph rg;

    auto&& c1 = bindings.stream_variable();
    auto&& c2 = bindings.stream_variable();
    auto&& c3 = bindings.stream_variable();
    auto&& r1 = rg.insert(take {
            bindings.exchange(f1),
            {
                    { f1.columns()[0], c1 },
                    { f1.columns()[1], c2 },
                    { f1.columns()[2], c3 },
            },
    });
    auto&& r2 = rg.insert(buffer { 2 });
    auto&& r3 = rg.insert(offer {
            bindings.exchange(f2),
            {
                    { c1, f2.columns()[0] },
                    { c1, f2.columns()[1] },
                    { c1, f2.columns()[2] },
            },
    });
    auto&& r4 = rg.insert(offer {
            bindings.exchange(f3),
            {
                    { c2, f3.columns()[0] },
                    { c2, f3.columns()[1] },
                    { c2, f3.columns()[2] },
            },
    });
    r1.output() >> r2.input();
    r2.output_ports()[0] >> r3.input();
    r2.output_ports()[1] >> r4.input();

    auto bg = block_builder::build(rg);
    variable_liveness_analyzer analyzer { bg };

    ASSERT_EQ(bg.size(), 3);
    auto&& b0 = *find_unique_head(bg); // r1 .. r2
    auto&& b1 = b0.downstream(r2.output_ports()[0]); // r3
    auto&& b2 = b0.downstream(r2.output_ports()[1]); // r4

    auto&& n0 = analyzer.inspect(b0);
    EXPECT_EQ(eq(n0.define(), {
            c1,
            c2,
            c3,
    }), no_error);

    EXPECT_EQ(eq(n0.use(), {
            f1.columns()[0],
            f1.columns()[1],
            f1.columns()[2],
    }), no_error);

    EXPECT_EQ(eq(n0.kill(), {
            c3,
    }), no_error);

    auto&& n1 = analyzer.inspect(b1);
    EXPECT_EQ(eq(n1.define(), {
    }), no_error);

    EXPECT_EQ(eq(n1.use(), {
            c1,
    }), no_error);

    EXPECT_EQ(eq(n1.kill(), {
            c2,
    }), no_error);

    auto&& n2 = analyzer.inspect(b2);
    EXPECT_EQ(eq(n2.define(), {
    }), no_error);

    EXPECT_EQ(eq(n2.use(), {
            c2,
    }), no_error);

    EXPECT_EQ(eq(n2.kill(), {
            c1,
    }), no_error);
}

} // namespace yugawara::analyzer

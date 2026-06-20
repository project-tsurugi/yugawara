#include <yugawara/analyzer/variable_liveness_analyzer.h>

#include <gtest/gtest.h>

#include <takatori/graph/graph.h>

#include <takatori/type/primitive.h>
#include <takatori/type/table.h>

#include <takatori/scalar/let.h>
#include <takatori/scalar/variable_reference.h>

#include <takatori/relation/apply.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/project.h>
#include <takatori/relation/buffer.h>
#include <takatori/relation/identify.h>
#include <takatori/relation/step/offer.h>
#include <takatori/relation/step/take_flat.h>

#include <takatori/plan/forward.h>

#include <yugawara/binding/factory.h>

#include <yugawara/analyzer/block_builder.h>
#include <yugawara/analyzer/block_algorithm.h>

#include <yugawara/testing/error_set.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer {

// import test utils
using namespace ::yugawara::testing;

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
            bindings.exchange_column("f1-1"),
            bindings.exchange_column("f1-2"),
            bindings.exchange_column("f1-3"),
    };
    ::takatori::plan::forward f2 {
            bindings.exchange_column("f2-1"),
            bindings.exchange_column("f2-2"),
            bindings.exchange_column("f2-3"),
    };
    ::takatori::plan::forward f3 {
            bindings.exchange_column("f3-1"),
            bindings.exchange_column("f3-2"),
            bindings.exchange_column("f3-3"),
    };
};

namespace ttype = ::takatori::type;
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

TEST_F(variable_liveness_analyzer_test, apply) {
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
    auto&& tvf = bindings.function({
            function::declaration::minimum_user_function_id + 1,
            "tvf",
            ttype::table {
                    { "o1", ttype::int8 {} },
                    { "o2", ttype::int8 {} },
                    { "o3", ttype::int8 {} },
            },
            {
                    ttype::int8 {},
            },
            {
                    function::function_feature::table_valued_function,
            },
    });
    auto&& c4 = bindings.stream_variable("c4");
    auto&& c5 = bindings.stream_variable("c5");
    auto&& c6 = bindings.stream_variable("c6");
    auto&& r2 = rg.insert(relation::apply {
            tvf,
            {
                    scalar::variable_reference { c2 }
            },
            {
                    { 0, c4 },
                    { 1, c5 },
                    { 2, c6 },
            },
    });
    auto&& r3 = rg.insert(offer {
            bindings.exchange(f2),
            {
                    { c1, f2.columns()[0] },
                    { c4, f2.columns()[1] },
                    { c5, f2.columns()[2] },
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
            c4,
            c5,
            c6,
    }), no_error);

    EXPECT_EQ(eq(n0.use(), {
            c1,
            c2,
            c3,
            c4,
            f1.columns()[0],
            f1.columns()[1],
            f1.columns()[2],
    }), no_error);

    EXPECT_EQ(eq(n0.kill(), {
            c3,
            c6,
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

TEST_F(variable_liveness_analyzer_test, buffer_decl_root_use_self) {
    /*
     * b1[+c@c]--+-- b2[]
     *           |
     *           +-- b3[]
     */
    rgraph rg;

    auto&& c = bindings.stream_variable("c");
    auto&& x = bindings.stream_variable("x");
    auto&& r11 = rg.insert(take {
            bindings.exchange(f1),
            {
                    { f1.columns()[0], x },
                    { f1.columns()[1], c },
            },
    });
    auto&& r12 = rg.insert(relation::filter {
            varref { c }
    });
    auto&& r13 = rg.insert(buffer { 2 });
    auto&& r21 = rg.insert(offer {
            bindings.exchange(f2),
            {
                    { x, f2.columns()[0] },
            },
    });
    auto&& r31 = rg.insert(offer {
            bindings.exchange(f3),
            {
                    { x, f3.columns()[0] },
            },
    });
    r11.output() >> r12.input();
    r12.output() >> r13.input();
    r13.output_ports()[0] >> r21.input();
    r13.output_ports()[1] >> r31.input();

    auto bg = block_builder::build(rg);
    variable_liveness_analyzer analyzer { bg };

    ASSERT_EQ(bg.size(), 3);
    auto&& b1 = *find_unique_head(bg); // r1x

    ASSERT_EQ(b1.downstreams().size(), 2);
    auto&& b2 = b1.downstreams()[0]; // r2x
    auto&& b3 = b1.downstreams()[1]; // r3x

    auto&& n1 = analyzer.inspect(b1);
    EXPECT_EQ(eq(n1.define(), { c, x }), no_error);
    EXPECT_EQ(eq(n1.use(), { c, f1.columns()[0], f1.columns()[1] }), no_error);
    EXPECT_EQ(eq(n1.kill(), {}), no_error);;

    auto&& n2 = analyzer.inspect(b2);
    EXPECT_EQ(eq(n2.define(), {}), no_error);
    EXPECT_EQ(eq(n2.use(), { x }), no_error);
    EXPECT_EQ(eq(n2.kill(), {}), no_error) << "don't kill 'c' because it is declared before branch";

    auto&& n3 = analyzer.inspect(b3);
    EXPECT_EQ(eq(n3.define(), {}), no_error);
    EXPECT_EQ(eq(n3.use(), { x }), no_error);
    EXPECT_EQ(eq(n3.kill(), {}), no_error) << "don't kill 'c' because it is declared before branch";
}

TEST_F(variable_liveness_analyzer_test, buffer_decl_root_use_none) {
    /*
     * b1[+c]--+-- b2[]
     *         |
     *         +-- b3[]
     */
    rgraph rg;

    auto&& c = bindings.stream_variable("c");
    auto&& x = bindings.stream_variable("x");
    auto&& r11 = rg.insert(take {
            bindings.exchange(f1),
            {
                    { f1.columns()[0], x },
                    { f1.columns()[1], c },
            },
    });
    auto&& r12 = rg.insert(buffer { 2 });
    auto&& r21 = rg.insert(offer {
            bindings.exchange(f2),
            {
                    { x, f2.columns()[0] },
            },
    });
    auto&& r31 = rg.insert(offer {
            bindings.exchange(f3),
            {
                    { x, f3.columns()[0] },
            },
    });
    r11.output() >> r12.input();
    r12.output_ports()[0] >> r21.input();
    r12.output_ports()[1] >> r31.input();

    auto bg = block_builder::build(rg);
    variable_liveness_analyzer analyzer { bg };

    ASSERT_EQ(bg.size(), 3);
    auto&& b1 = *find_unique_head(bg); // r1x

    ASSERT_EQ(b1.downstreams().size(), 2);
    auto&& b2 = b1.downstreams()[0]; // r2x
    auto&& b3 = b1.downstreams()[1]; // r3x

    auto&& n1 = analyzer.inspect(b1);
    EXPECT_EQ(eq(n1.define(), { c, x }), no_error);
    EXPECT_EQ(eq(n1.use(), { f1.columns()[0], f1.columns()[1] }), no_error);
    EXPECT_EQ(eq(n1.kill(), { c }), no_error);

    auto&& n2 = analyzer.inspect(b2);
    EXPECT_EQ(eq(n2.define(), {}), no_error);
    EXPECT_EQ(eq(n2.use(), { x }), no_error);
    EXPECT_EQ(eq(n2.kill(), {}), no_error);

    auto&& n3 = analyzer.inspect(b3);
    EXPECT_EQ(eq(n3.define(), {}), no_error);
    EXPECT_EQ(eq(n3.use(), { x }), no_error);
    EXPECT_EQ(eq(n3.kill(), {}), no_error);
}

TEST_F(variable_liveness_analyzer_test, buffer_decl_root_use_left) {
    /*
     * b1[+c]--+-- b2[@c]
     *         |
     *         +-- b3[]
     */
    rgraph rg;

    auto&& c = bindings.stream_variable("c");
    auto&& x = bindings.stream_variable("x");
    auto&& r11 = rg.insert(take {
            bindings.exchange(f1),
            {
                    { f1.columns()[0], x },
                    { f1.columns()[1], c },
            },
    });
    auto&& r12 = rg.insert(buffer { 2 });
    auto&& r21 = rg.insert(offer {
            bindings.exchange(f2),
            {
                    { x, f2.columns()[0] },
                    { c, f2.columns()[1] },
            },
    });
    auto&& r31 = rg.insert(offer {
            bindings.exchange(f3),
            {
                    { x, f3.columns()[0] },
            },
    });
    r11.output() >> r12.input();
    r12.output_ports()[0] >> r21.input();
    r12.output_ports()[1] >> r31.input();

    auto bg = block_builder::build(rg);
    variable_liveness_analyzer analyzer { bg };

    ASSERT_EQ(bg.size(), 3);
    auto&& b1 = *find_unique_head(bg); // r1x

    ASSERT_EQ(b1.downstreams().size(), 2);
    auto&& b2 = b1.downstreams()[0]; // r2x
    auto&& b3 = b1.downstreams()[1]; // r3x

    auto&& n1 = analyzer.inspect(b1);
    EXPECT_EQ(eq(n1.define(), { c, x }), no_error);
    EXPECT_EQ(eq(n1.use(), { f1.columns()[0], f1.columns()[1] }), no_error);
    EXPECT_EQ(eq(n1.kill(), {}), no_error);

    auto&& n2 = analyzer.inspect(b2);
    EXPECT_EQ(eq(n2.define(), {}), no_error);
    EXPECT_EQ(eq(n2.use(), { x, c }), no_error);
    EXPECT_EQ(eq(n2.kill(), {}), no_error);

    auto&& n3 = analyzer.inspect(b3);
    EXPECT_EQ(eq(n3.define(), {}), no_error);
    EXPECT_EQ(eq(n3.use(), { x }), no_error);
    EXPECT_EQ(eq(n3.kill(), {}), no_error);
}

TEST_F(variable_liveness_analyzer_test, buffer_decl_root_use_right) {
    /*
     * b1[+c]--+-- b2[]
     *         |
     *         +-- b3[@c]
     */
    rgraph rg;

    auto&& c = bindings.stream_variable("c");
    auto&& x = bindings.stream_variable("x");
    auto&& r11 = rg.insert(take {
            bindings.exchange(f1),
            {
                    { f1.columns()[0], x },
                    { f1.columns()[1], c },
            },
    });
    auto&& r12 = rg.insert(buffer { 2 });
    auto&& r21 = rg.insert(offer {
            bindings.exchange(f2),
            {
                    { x, f2.columns()[0] },
            },
    });
    auto&& r31 = rg.insert(offer {
            bindings.exchange(f3),
            {
                    { x, f3.columns()[0] },
                    { c, f3.columns()[1] },
            },
    });
    r11.output() >> r12.input();
    r12.output_ports()[0] >> r21.input();
    r12.output_ports()[1] >> r31.input();

    auto bg = block_builder::build(rg);
    variable_liveness_analyzer analyzer { bg };

    ASSERT_EQ(bg.size(), 3);
    auto&& b1 = *find_unique_head(bg); // r1x

    ASSERT_EQ(b1.downstreams().size(), 2);
    auto&& b2 = b1.downstreams()[0]; // r2x
    auto&& b3 = b1.downstreams()[1]; // r3x

    auto&& n1 = analyzer.inspect(b1);
    EXPECT_EQ(eq(n1.define(), { c, x }), no_error);
    EXPECT_EQ(eq(n1.use(), { f1.columns()[0], f1.columns()[1] }), no_error);
    EXPECT_EQ(eq(n1.kill(), {}), no_error);

    auto&& n2 = analyzer.inspect(b2);
    EXPECT_EQ(eq(n2.define(), {}), no_error);
    EXPECT_EQ(eq(n2.use(), { x }), no_error);
    EXPECT_EQ(eq(n2.kill(), {}), no_error);

    auto&& n3 = analyzer.inspect(b3);
    EXPECT_EQ(eq(n3.define(), {}), no_error);
    EXPECT_EQ(eq(n3.use(), { x, c }), no_error);
    EXPECT_EQ(eq(n3.kill(), {}), no_error);
}

TEST_F(variable_liveness_analyzer_test, buffer_decl_root_use_both) {
    /*
     * b1[+c]--+-- b2[@c]
     *         |
     *         +-- b3[@c]
     */
    rgraph rg;

    auto&& c = bindings.stream_variable("c");
    auto&& x = bindings.stream_variable("x");
    auto&& r11 = rg.insert(take {
            bindings.exchange(f1),
            {
                    { f1.columns()[0], x },
                    { f1.columns()[1], c },
            },
    });
    auto&& r12 = rg.insert(buffer { 2 });
    auto&& r21 = rg.insert(offer {
            bindings.exchange(f2),
            {
                    { x, f2.columns()[0] },
                    { c, f2.columns()[1] },
            },
    });
    auto&& r31 = rg.insert(offer {
            bindings.exchange(f3),
            {
                    { x, f3.columns()[0] },
                    { c, f3.columns()[1] },
            },
    });
    r11.output() >> r12.input();
    r12.output_ports()[0] >> r21.input();
    r12.output_ports()[1] >> r31.input();

    auto bg = block_builder::build(rg);
    variable_liveness_analyzer analyzer { bg };

    ASSERT_EQ(bg.size(), 3);
    auto&& b1 = *find_unique_head(bg); // r1x

    ASSERT_EQ(b1.downstreams().size(), 2);
    auto&& b2 = b1.downstreams()[0]; // r2x
    auto&& b3 = b1.downstreams()[1]; // r3x

    auto&& n1 = analyzer.inspect(b1);
    EXPECT_EQ(eq(n1.define(), { c, x }), no_error);
    EXPECT_EQ(eq(n1.use(), { f1.columns()[0], f1.columns()[1] }), no_error);
    EXPECT_EQ(eq(n1.kill(), {}), no_error);

    auto&& n2 = analyzer.inspect(b2);
    EXPECT_EQ(eq(n2.define(), {}), no_error);
    EXPECT_EQ(eq(n2.use(), { x, c }), no_error);
    EXPECT_EQ(eq(n2.kill(), {}), no_error);

    auto&& n3 = analyzer.inspect(b3);
    EXPECT_EQ(eq(n3.define(), {}), no_error);
    EXPECT_EQ(eq(n3.use(), { x, c }), no_error);
    EXPECT_EQ(eq(n3.kill(), {}), no_error);
}

TEST_F(variable_liveness_analyzer_test, buffer_branch_root_use_self) {
    /*
     * b1[]--+-- b2[]
     *       |
     *       +-- b3[+c@c]
     */
    rgraph rg;

    auto&& x = bindings.stream_variable("x");
    auto&& r11 = rg.insert(take {
            bindings.exchange(f1),
            {
                    { f1.columns()[0], x },
            },
    });
    auto&& r12 = rg.insert(buffer { 2 });
    auto&& r21 = rg.insert(offer {
            bindings.exchange(f2),
            {
                    { x, f2.columns()[0] },
            },
    });
    auto&& c = bindings.stream_variable("c");
    auto&& r31 = rg.insert(relation::project {
            relation::project::column {
                    varref { x },
                    c,
            },
    });
    auto&& r32 = rg.insert(offer {
            bindings.exchange(f3),
            {
                    { x, f3.columns()[0] },
                    { c, f3.columns()[1] },
            },
    });
    r11.output() >> r12.input();
    r12.output_ports()[0] >> r21.input();
    r12.output_ports()[1] >> r31.input();
    r31.output() >> r32.input();

    auto bg = block_builder::build(rg);
    variable_liveness_analyzer analyzer { bg };

    ASSERT_EQ(bg.size(), 3);
    auto&& b1 = *find_unique_head(bg); // r1x

    ASSERT_EQ(b1.downstreams().size(), 2);
    auto&& b2 = b1.downstreams()[0]; // r2x
    auto&& b3 = b1.downstreams()[1]; // r3x

    auto&& n1 = analyzer.inspect(b1);
    EXPECT_EQ(eq(n1.define(), { x }), no_error);
    EXPECT_EQ(eq(n1.use(), { f1.columns()[0] }), no_error);
    EXPECT_EQ(eq(n1.kill(), {}), no_error);

    auto&& n2 = analyzer.inspect(b2);
    EXPECT_EQ(eq(n2.define(), {}), no_error);
    EXPECT_EQ(eq(n2.use(), { x }), no_error);
    EXPECT_EQ(eq(n2.kill(), {}), no_error);

    auto&& n3 = analyzer.inspect(b3);
    EXPECT_EQ(eq(n3.define(), { c }), no_error);
    EXPECT_EQ(eq(n3.use(), { x, c }), no_error);
    EXPECT_EQ(eq(n3.kill(), {}), no_error);
}

TEST_F(variable_liveness_analyzer_test, buffer_branch_root_use_none) {
    /*
     * b1[]--+-- b2[]
     *       |
     *       +-- b3[+c]
     */
    rgraph rg;

    auto&& x = bindings.stream_variable("x");
    auto&& r11 = rg.insert(take {
            bindings.exchange(f1),
            {
                    { f1.columns()[0], x },
            },
    });
    auto&& r12 = rg.insert(buffer { 2 });
    auto&& r21 = rg.insert(offer {
            bindings.exchange(f2),
            {
                    { x, f2.columns()[0] },
            },
    });
    auto&& c = bindings.stream_variable("c");
    auto&& r31 = rg.insert(relation::project {
            relation::project::column {
                    varref { x },
                    c,
            },
    });
    auto&& r32 = rg.insert(offer {
            bindings.exchange(f3),
            {
                    { x, f3.columns()[0] },
            },
    });
    r11.output() >> r12.input();
    r12.output_ports()[0] >> r21.input();
    r12.output_ports()[1] >> r31.input();
    r31.output() >> r32.input();

    auto bg = block_builder::build(rg);
    variable_liveness_analyzer analyzer { bg };

    ASSERT_EQ(bg.size(), 3);
    auto&& b1 = *find_unique_head(bg); // r1x

    ASSERT_EQ(b1.downstreams().size(), 2);
    auto&& b2 = b1.downstreams()[0]; // r2x
    auto&& b3 = b1.downstreams()[1]; // r3x

    auto&& n1 = analyzer.inspect(b1);
    EXPECT_EQ(eq(n1.define(), { x }), no_error);
    EXPECT_EQ(eq(n1.use(), { f1.columns()[0] }), no_error);
    EXPECT_EQ(eq(n1.kill(), {}), no_error);

    auto&& n2 = analyzer.inspect(b2);
    EXPECT_EQ(eq(n2.define(), {}), no_error);
    EXPECT_EQ(eq(n2.use(), { x }), no_error);
    EXPECT_EQ(eq(n2.kill(), {}), no_error);

    auto&& n3 = analyzer.inspect(b3);
    EXPECT_EQ(eq(n3.define(), { c }), no_error);
    EXPECT_EQ(eq(n3.use(), { x }), no_error);
    EXPECT_EQ(eq(n3.kill(), { c }), no_error);
}

TEST_F(variable_liveness_analyzer_test, identify) {
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
    auto&& r2 = rg.insert(relation::identify {
            p1,
            ttype::row_id { 1 },
    });
    auto&& r3 = rg.insert(offer {
            bindings.exchange(f2),
            {
                    { c1, f2.columns()[0] },
                    { p1, f2.columns()[1] },
                    { c2, f2.columns()[2] },
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

} // namespace yugawara::analyzer

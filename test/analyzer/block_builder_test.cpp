#include <yugawara/analyzer/block_builder.h>

#include <gtest/gtest.h>

#include <takatori/graph/graph.h>

#include <takatori/relation/buffer.h>
#include <takatori/relation/step/offer.h>
#include <takatori/relation/step/take_flat.h>

#include <takatori/plan/forward.h>

#include <yugawara/binding/factory.h>

#include <yugawara/analyzer/block_algorithm.h>

namespace yugawara::analyzer {

class block_builder_test : public ::testing::Test {
public:
    binding::factory bindings;
    ::takatori::plan::forward f1 {
            bindings.exchange_column(),
    };
    ::takatori::plan::forward f2 {
            bindings.exchange_column(),
    };
    ::takatori::plan::forward f3 {
            bindings.exchange_column(),
    };
};

namespace relation = ::takatori::relation;
using take = relation::step::take_flat;
using offer = relation::step::offer;
using buffer = relation::buffer;

using rgraph = ::takatori::graph::graph<relation::expression>;

TEST_F(block_builder_test, simple) {
    rgraph rg;

    auto&& c1 = bindings.stream_variable();
    auto&& r1 = rg.insert(take {
            bindings.exchange(f1),
            {
                    { f1.columns()[0], c1 },
            },
    });
    auto&& r2 = rg.insert(offer {
            bindings.exchange(f2),
            {
                    { c1, f2.columns()[0] },
            },
    });
    r1.output() >> r2.input();

    auto bg = block_builder::build(rg);

    EXPECT_EQ(bg.size(), 1);
    auto&& b0 = *find_unique_head(bg);

    EXPECT_EQ(std::addressof(b0.front()), std::addressof(r1));
    EXPECT_EQ(std::addressof(b0.back()), std::addressof(r2));
}

TEST_F(block_builder_test, buffer) {
    rgraph rg;

    auto&& c1 = bindings.stream_variable();
    auto&& r1 = rg.insert(take {
            bindings.exchange(f1),
            {
                    { f1.columns()[0], c1 },
            },
    });
    auto&& r2 = rg.insert(buffer { 2 });
    auto&& r21 = rg.insert(offer {
            bindings.exchange(f2),
            {
                    { c1, f2.columns()[0] },
            },
    });
    auto&& r22 = rg.insert(offer {
            bindings.exchange(f3),
            {
                    { c1, f3.columns()[0] },
            },
    });
    r1.output() >> r2.input();
    r2.output_ports()[0] >> r21.input();
    r2.output_ports()[1] >> r22.input();

    auto bg = block_builder::build(rg);

    EXPECT_EQ(bg.size(), 3);
    auto&& b0 = *find_unique_head(bg);

    EXPECT_EQ(std::addressof(b0.front()), std::addressof(r1));
    ASSERT_EQ(std::addressof(b0.back()), std::addressof(r2));

    auto&& b1 = b0.downstream(r2.output_ports()[0]);
    EXPECT_EQ(std::addressof(b1.front()), std::addressof(r21));
    EXPECT_EQ(std::addressof(b1.back()), std::addressof(r21));

    auto&& b2 = b0.downstream(r2.output_ports()[1]);
    EXPECT_EQ(std::addressof(b2.front()), std::addressof(r22));
    EXPECT_EQ(std::addressof(b2.back()), std::addressof(r22));
}

} // namespace yugawara::analyzer

#include <yugawara/analyzer/details/decompose_prefix_match.h>

#include <gtest/gtest.h>

#include <takatori/value/character.h>

#include <takatori/type/character.h>

#include <takatori/scalar/match.h>

#include <takatori/relation/scan.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/emit.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/configurable_provider.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

class decompose_prefix_match_test : public ::testing::Test {
protected:
    binding::factory bindings;
    storage::configurable_provider storages;

    std::shared_ptr<storage::table> t0 = storages.add_table({
            "T0",
            {
                    { "C0", t::character { t::varying } },
            },
    });
    descriptor::variable t0c0 = bindings(t0->columns()[0]);

    std::shared_ptr<storage::index> i0 = storages.add_index({ t0, "I0", });

    scalar::immediate str(std::string s) {
        return scalar::immediate {
                v::character { std::move(s) },
                t::character { t::varying },
        };
    }

    void apply(relation::graph_type& graph) {
        decompose_prefix_match(graph);
    }
};

TEST_F(decompose_prefix_match_test, constant) {
    /*
     * scan:r0 - filter:r2 - emit:ro
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto& r1 = r.insert(relation::filter {
            scalar::match {
                    scalar::match_operator::like,
                    varref { x0 },
                    str("abc"),
                    str(""),
            },
    });
    auto& ro = r.insert(relation::emit {
            x0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    ASSERT_TRUE(r.contains(r1));
    auto&& p0 = r1;

    EXPECT_GT(p0.output(), ro.input());

    ASSERT_EQ(p0.condition(), compare(varref(x0), str("abc")));
}

TEST_F(decompose_prefix_match_test, prefix) {
    /*
     * scan:r0 - filter:r2 - emit:ro
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto& r1 = r.insert(relation::filter {
            scalar::match {
                    scalar::match_operator::like,
                    varref { x0 },
                    str("abc%"),
                    str(""),
            },
    });
    auto& ro = r.insert(relation::emit {
            x0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    ASSERT_TRUE(r.contains(r1));
    auto&& p0 = r1;

    EXPECT_GT(p0.output(), ro.input());

    ASSERT_EQ(p0.condition(), land(
            compare(varref(x0), str("abc"), scalar::comparison_operator::greater_equal),
            compare(varref(x0), str("abc\xff"), scalar::comparison_operator::less)));
}

TEST_F(decompose_prefix_match_test, general) {
    /*
     * scan:r0 - filter:r2 - emit:ro
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto& r1 = r.insert(relation::filter {
            scalar::match {
                    scalar::match_operator::like,
                    varref { x0 },
                    str("abc_"),
                    str(""),
            },
    });
    auto& ro = r.insert(relation::emit {
            x0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    ASSERT_TRUE(r.contains(r1));
    auto&& p0 = r1;

    EXPECT_GT(p0.output(), ro.input());

    ASSERT_EQ(p0.condition(), land(
            land(
                    compare(varref(x0), str("abc"), scalar::comparison_operator::greater_equal),
                    compare(varref(x0), str("abc\xff"), scalar::comparison_operator::less)),
            scalar::match {
                    scalar::match_operator::like,
                    varref { x0 },
                    str("abc_"),
                    str(""),
            }));
}

TEST_F(decompose_prefix_match_test, escape) {
    /*
     * scan:r0 - filter:r2 - emit:ro
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto& r1 = r.insert(relation::filter {
            scalar::match {
                    scalar::match_operator::like,
                    varref { x0 },
                    str("\\%abc\\%%"),
                    str("\\"),
            },
    });
    auto& ro = r.insert(relation::emit {
            x0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    ASSERT_TRUE(r.contains(r1));
    auto&& p0 = r1;

    EXPECT_GT(p0.output(), ro.input());

    ASSERT_EQ(p0.condition(), land(
            compare(varref(x0), str("%abc%"), scalar::comparison_operator::greater_equal),
            compare(varref(x0), str("%abc%\xff"), scalar::comparison_operator::less)));
}

TEST_F(decompose_prefix_match_test, mismatch_input_not_column) {
    /*
     * scan:r0 - filter:r2 - emit:ro
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto& r1 = r.insert(relation::filter {
            scalar::match {
                    scalar::match_operator::like,
                    str("abcabcabc"),
                    str("%abc"),
                    str(""),
            },
    });
    auto& ro = r.insert(relation::emit {
            x0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    ASSERT_TRUE(r.contains(r1));
    auto&& p0 = r1;

    EXPECT_GT(p0.output(), ro.input());

    ASSERT_EQ(p0.condition(), (scalar::match {
            scalar::match_operator::like,
            str("abcabcabc"),
            str("%abc"),
            str(""),
    }));
}

TEST_F(decompose_prefix_match_test, mismatch_not_prefix) {
    /*
     * scan:r0 - filter:r2 - emit:ro
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto& r1 = r.insert(relation::filter {
            scalar::match {
                    scalar::match_operator::like,
                    varref { x0 },
                    str("%abc"),
                    str(""),
            },
    });
    auto& ro = r.insert(relation::emit {
            x0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    ASSERT_TRUE(r.contains(r1));
    auto&& p0 = r1;

    EXPECT_GT(p0.output(), ro.input());

    ASSERT_EQ(p0.condition(), (scalar::match {
            scalar::match_operator::like,
            varref { x0 },
            str("%abc"),
            str(""),
    }));
}

TEST_F(decompose_prefix_match_test, mismatch_escape_not_ascii) {
    /*
     * scan:r0 - filter:r2 - emit:ro
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto& r1 = r.insert(relation::filter {
            scalar::match {
                    scalar::match_operator::like,
                    varref { x0 },
                    str("%abc"),
                    str("\u3042"),
            },
    });
    auto& ro = r.insert(relation::emit {
            x0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    ASSERT_TRUE(r.contains(r1));
    auto&& p0 = r1;

    EXPECT_GT(p0.output(), ro.input());

    ASSERT_EQ(p0.condition(), (scalar::match {
            scalar::match_operator::like,
            varref { x0 },
            str("%abc"),
            str("\u3042"),
    }));
}

} // namespace yugawara::analyzer::details

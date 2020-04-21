#include <analyzer/details/search_key_term_builder.h>

#include <gtest/gtest.h>

#include <takatori/scalar/binary.h>
#include <takatori/scalar/unary.h>

#include <takatori/relation/filter.h>

#include <yugawara/binding/factory.h>

#include "utils.h"

namespace yugawara::analyzer::details {

using ::takatori::scalar::unary;
using ::takatori::scalar::binary;
using ::takatori::scalar::unary_operator;
using ::takatori::scalar::binary_operator;
using ::takatori::scalar::comparison_operator;

using ::takatori::util::object_ownership_reference;
using ::takatori::util::rvalue_reference_wrapper;

class search_key_term_builder_test: public ::testing::Test {
protected:
    ::takatori::util::object_creator creator;
    binding::factory bindings { creator };
};

TEST_F(search_key_term_builder_test, equal) {
    auto k0 = bindings.stream_variable("k0");
    relation::filter r {
            compare(varref(k0), constant(0), comparison_operator::equal),
    };

    search_key_term_builder b { creator };
    b.add_key(k0);
    b.add_predicate(r.ownership_condition());

    auto t0 = b.find(k0);
    ASSERT_TRUE(t0);
    ASSERT_TRUE(t0->equivalent());

    ASSERT_EQ(t0->equivalent_factor(), constant(0));

    auto p0 = t0->purge_equivalent_factor();
    EXPECT_EQ(*p0, constant(0));
    EXPECT_EQ(r.condition(), boolean(true));
}

TEST_F(search_key_term_builder_test, not_equal) {
    auto k0 = bindings.stream_variable("k0");
    relation::filter r {
            compare(varref(k0), constant(0), comparison_operator::not_equal)
    };

    search_key_term_builder b { creator };
    b.add_key(k0);
    b.add_predicate(r.ownership_condition());

    auto t0 = b.find(k0);
    ASSERT_FALSE(t0);
}

TEST_F(search_key_term_builder_test, less) {
    auto k0 = bindings.stream_variable("k0");
    relation::filter r {
            compare(varref(k0), constant(0), comparison_operator::less),
    };

    search_key_term_builder b { creator };
    b.add_key(k0);
    b.add_predicate(r.ownership_condition());

    auto t0 = b.find(k0);
    ASSERT_TRUE(t0);

    ASSERT_EQ(t0->upper_factor(), constant(0));
    EXPECT_FALSE(t0->upper_inclusive());

    auto p0 = t0->purge_upper_factor();
    EXPECT_EQ(*p0, constant(0));
    EXPECT_EQ(r.condition(), boolean(true));
}

TEST_F(search_key_term_builder_test, less_equal) {
    auto k0 = bindings.stream_variable("k0");
    relation::filter r {
            compare(varref(k0), constant(0), comparison_operator::less_equal),
    };

    search_key_term_builder b { creator };
    b.add_key(k0);
    b.add_predicate(r.ownership_condition());

    auto t0 = b.find(k0);
    ASSERT_TRUE(t0);

    ASSERT_EQ(t0->upper_factor(), constant(0));
    EXPECT_TRUE(t0->upper_inclusive());

    auto p0 = t0->purge_upper_factor();
    EXPECT_EQ(*p0, constant(0));
    EXPECT_EQ(r.condition(), boolean(true));
}

TEST_F(search_key_term_builder_test, greater) {
    auto k0 = bindings.stream_variable("k0");
    relation::filter r {
            compare(varref(k0), constant(0), comparison_operator::greater),
    };

    search_key_term_builder b { creator };
    b.add_key(k0);
    b.add_predicate(r.ownership_condition());

    auto t0 = b.find(k0);
    ASSERT_TRUE(t0);

    ASSERT_EQ(t0->lower_factor(), constant(0));
    EXPECT_FALSE(t0->lower_inclusive());

    auto p0 = t0->purge_lower_factor();
    EXPECT_EQ(*p0, constant(0));
    EXPECT_EQ(r.condition(), boolean(true));
}

TEST_F(search_key_term_builder_test, greater_equal) {
    auto k0 = bindings.stream_variable("k0");
    relation::filter r {
            compare(varref(k0), constant(0), comparison_operator::greater_equal),
    };

    search_key_term_builder b { creator };
    b.add_key(k0);
    b.add_predicate(r.ownership_condition());

    auto t0 = b.find(k0);
    ASSERT_TRUE(t0);

    ASSERT_EQ(t0->lower_factor(), constant(0));
    EXPECT_TRUE(t0->lower_inclusive());

    auto p0 = t0->purge_lower_factor();
    EXPECT_EQ(*p0, constant(0));
    EXPECT_EQ(r.condition(), boolean(true));
}

TEST_F(search_key_term_builder_test, equal_not) {
    auto k0 = bindings.stream_variable("k0");
    relation::filter r {
            lnot(compare(varref(k0), constant(0), comparison_operator::equal))
    };

    search_key_term_builder b { creator };
    b.add_key(k0);
    b.add_predicate(r.ownership_condition());

    auto t0 = b.find(k0);
    ASSERT_FALSE(t0);
}

TEST_F(search_key_term_builder_test, not_equal_not) {
    auto k0 = bindings.stream_variable("k0");
    relation::filter r {
            lnot(compare(varref(k0), constant(0), comparison_operator::not_equal)),
    };

    search_key_term_builder b { creator };
    b.add_key(k0);
    b.add_predicate(r.ownership_condition());

    auto t0 = b.find(k0);
    ASSERT_TRUE(t0);

    ASSERT_EQ(t0->equivalent_factor(), constant(0));

    auto p0 = t0->purge_equivalent_factor();
    EXPECT_EQ(*p0, constant(0));
    EXPECT_EQ(r.condition(), boolean(true));
}

TEST_F(search_key_term_builder_test, less_not) {
    auto k0 = bindings.stream_variable("k0");
    relation::filter r {
            lnot(compare(varref(k0), constant(0), comparison_operator::less)),
    };

    search_key_term_builder b { creator };
    b.add_key(k0);
    b.add_predicate(r.ownership_condition());

    auto t0 = b.find(k0);
    ASSERT_TRUE(t0);

    ASSERT_EQ(t0->lower_factor(), constant(0));
    EXPECT_TRUE(t0->lower_inclusive());

    auto p0 = t0->purge_lower_factor();
    EXPECT_EQ(*p0, constant(0));
    EXPECT_EQ(r.condition(), boolean(true));
}

TEST_F(search_key_term_builder_test, less_equal_not) {
    auto k0 = bindings.stream_variable("k0");
    relation::filter r {
            lnot(compare(varref(k0), constant(0), comparison_operator::less_equal)),
    };

    search_key_term_builder b { creator };
    b.add_key(k0);
    b.add_predicate(r.ownership_condition());

    auto t0 = b.find(k0);
    ASSERT_TRUE(t0);

    ASSERT_EQ(t0->lower_factor(), constant(0));
    EXPECT_FALSE(t0->lower_inclusive());

    auto p0 = t0->purge_lower_factor();
    EXPECT_EQ(*p0, constant(0));
    EXPECT_EQ(r.condition(), boolean(true));
}

TEST_F(search_key_term_builder_test, greater_not) {
    auto k0 = bindings.stream_variable("k0");
    relation::filter r {
            lnot(compare(varref(k0), constant(0), comparison_operator::greater)),
    };

    search_key_term_builder b { creator };
    b.add_key(k0);
    b.add_predicate(r.ownership_condition());

    auto t0 = b.find(k0);
    ASSERT_TRUE(t0);

    ASSERT_EQ(t0->upper_factor(), constant(0));
    EXPECT_TRUE(t0->upper_inclusive());

    auto p0 = t0->purge_upper_factor();
    EXPECT_EQ(*p0, constant(0));
    EXPECT_EQ(r.condition(), boolean(true));
}

TEST_F(search_key_term_builder_test, greater_equal_not) {
    auto k0 = bindings.stream_variable("k0");
    relation::filter r {
            lnot(compare(varref(k0), constant(0), comparison_operator::greater_equal)),
    };

    search_key_term_builder b { creator };
    b.add_key(k0);
    b.add_predicate(r.ownership_condition());

    auto t0 = b.find(k0);
    ASSERT_TRUE(t0);

    ASSERT_EQ(t0->upper_factor(), constant(0));
    EXPECT_FALSE(t0->upper_inclusive());

    auto p0 = t0->purge_upper_factor();
    EXPECT_EQ(*p0, constant(0));
    EXPECT_EQ(r.condition(), boolean(true));
}

TEST_F(search_key_term_builder_test, equal_trans) {
    auto k0 = bindings.stream_variable("k0");
    relation::filter r {
            compare(constant(0), varref(k0), comparison_operator::equal)
    };

    search_key_term_builder b { creator };
    b.add_key(k0);
    b.add_predicate(r.ownership_condition());

    auto t0 = b.find(k0);
    ASSERT_TRUE(t0);

    ASSERT_EQ(t0->equivalent_factor(), constant(0));

    auto p0 = t0->purge_equivalent_factor();
    EXPECT_EQ(*p0, constant(0));
    EXPECT_EQ(r.condition(), boolean(true));
}

TEST_F(search_key_term_builder_test, not_equal_trans) {
    auto k0 = bindings.stream_variable("k0");
    relation::filter r {
            compare(constant(0), varref(k0), comparison_operator::not_equal),
    };

    search_key_term_builder b { creator };
    b.add_key(k0);
    b.add_predicate(r.ownership_condition());

    auto t0 = b.find(k0);
    ASSERT_FALSE(t0);
}

TEST_F(search_key_term_builder_test, less_trans) {
    auto k0 = bindings.stream_variable("k0");
    relation::filter r {
            compare(constant(0), varref(k0), comparison_operator::less),
    };

    search_key_term_builder b { creator };
    b.add_key(k0);
    b.add_predicate(r.ownership_condition());

    auto t0 = b.find(k0);
    ASSERT_TRUE(t0);

    ASSERT_EQ(t0->lower_factor(), constant(0));
    EXPECT_FALSE(t0->lower_inclusive());

    auto p0 = t0->purge_lower_factor();
    EXPECT_EQ(*p0, constant(0));
    EXPECT_EQ(r.condition(), boolean(true));
}

TEST_F(search_key_term_builder_test, less_equal_trans) {
    auto k0 = bindings.stream_variable("k0");
    relation::filter r {
            compare(constant(0), varref(k0), comparison_operator::less_equal),
    };

    search_key_term_builder b { creator };
    b.add_key(k0);
    b.add_predicate(r.ownership_condition());

    auto t0 = b.find(k0);
    ASSERT_TRUE(t0);

    ASSERT_EQ(t0->lower_factor(), constant(0));
    EXPECT_TRUE(t0->lower_inclusive());

    auto p0 = t0->purge_lower_factor();
    EXPECT_EQ(*p0, constant(0));
    EXPECT_EQ(r.condition(), boolean(true));
}

TEST_F(search_key_term_builder_test, greater_trans) {
    auto k0 = bindings.stream_variable("k0");
    relation::filter r {
            compare(constant(0), varref(k0), comparison_operator::greater),
    };

    search_key_term_builder b { creator };
    b.add_key(k0);
    b.add_predicate(r.ownership_condition());

    auto t0 = b.find(k0);
    ASSERT_TRUE(t0);

    ASSERT_EQ(t0->upper_factor(), constant(0));
    EXPECT_FALSE(t0->upper_inclusive());

    auto p0 = t0->purge_upper_factor();
    EXPECT_EQ(*p0, constant(0));
    EXPECT_EQ(r.condition(), boolean(true));
}

TEST_F(search_key_term_builder_test, greater_equal_trans) {
    auto k0 = bindings.stream_variable("k0");
    relation::filter r {
            compare(constant(0), varref(k0), comparison_operator::greater_equal),
    };

    search_key_term_builder b { creator };
    b.add_key(k0);
    b.add_predicate(r.ownership_condition());

    auto t0 = b.find(k0);
    ASSERT_TRUE(t0);

    ASSERT_EQ(t0->upper_factor(), constant(0));
    EXPECT_TRUE(t0->upper_inclusive());

    auto p0 = t0->purge_upper_factor();
    EXPECT_EQ(*p0, constant(0));
    EXPECT_EQ(r.condition(), boolean(true));
}

TEST_F(search_key_term_builder_test, range) {
    auto k0 = bindings.stream_variable("k0");
    relation::filter r {
        land(
                compare(constant(0), varref(k0), comparison_operator::less_equal),
                compare(varref(k0), constant(100), comparison_operator::less))
    };

    search_key_term_builder b { creator };
    b.add_key(k0);
    b.add_predicate(r.ownership_condition());

    auto t0 = b.find(k0);
    ASSERT_TRUE(t0);
    ASSERT_TRUE(t0->full_bounded());

    ASSERT_EQ(t0->lower_factor(), constant(0));
    EXPECT_TRUE(t0->lower_inclusive());

    ASSERT_EQ(t0->upper_factor(), constant(100));
    EXPECT_FALSE(t0->upper_inclusive());

    auto p0 = t0->purge_lower_factor();
    EXPECT_EQ(*p0, constant(0));

    auto p1 = t0->purge_upper_factor();
    EXPECT_EQ(*p1, constant(100));

    EXPECT_EQ(r.condition(), land(boolean(true), boolean(true)));
}

TEST_F(search_key_term_builder_test, dependent) {
    auto k0 = bindings.stream_variable("k0");
    auto k1 = bindings.stream_variable("k1");
    relation::filter r {
            compare(k0, k1),
    };

    search_key_term_builder b { creator };
    b.add_key(k0);
    b.add_key(k1);
    b.add_predicate(r.ownership_condition());

    EXPECT_FALSE(b.find(k0));
    EXPECT_FALSE(b.find(k1));
}

TEST_F(search_key_term_builder_test, independent) {
    auto k0 = bindings.stream_variable("k0");
    auto v1 = bindings.stream_variable("v1");
    relation::filter r {
            compare(k0, v1),
    };

    search_key_term_builder b { creator };
    b.add_key(k0);
    b.add_predicate(r.ownership_condition());

    auto t0 = b.find(k0);
    ASSERT_TRUE(t0);
    ASSERT_EQ(t0->equivalent_factor(), varref(v1));
}

} // namespace yugawara::analyzer::details

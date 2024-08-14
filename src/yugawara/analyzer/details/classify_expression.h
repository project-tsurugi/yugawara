#pragma once

#include <ostream>
#include <string>
#include <string_view>
#include <utility>

#include <takatori/scalar/expression.h>

#include <takatori/util/enum_set.h>

namespace yugawara::analyzer::details {

/**
 * @brief represents kind of expression classes.
 */
enum class expression_class : std::size_t {

    /**
     * @brief contains unknown expressions.
     */
    unknown = 0,

    /**
     * @brief a constant expression.
     */
    constant,

    /**
     * @brief a trivial expression.
     */
    trivial,

    /**
     * @brief a simple expression.
     */
    small,
};

/**
 * @brief a set of expression classes.
 */
using expression_class_set = ::takatori::util::enum_set<
        expression_class,
        expression_class::unknown,
        expression_class::small>;

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
constexpr std::string_view to_string_view(expression_class value) noexcept {
    using namespace std::string_view_literals;
    using kind = expression_class;
    switch (value) {
        case kind::unknown: return "unknown"sv;
        case kind::constant: return "constant"sv;
        case kind::trivial: return "trivial"sv;
        case kind::small: return "small"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, expression_class value) {
    return out << to_string_view(value);
}

/**
 * @brief classifies the given expression.
 * @param expression the target expression
 * @return
 */
[[nodiscard]] expression_class_set classify_expression(::takatori::scalar::expression const& expression);

} // namespace yugawara::analyzer::details

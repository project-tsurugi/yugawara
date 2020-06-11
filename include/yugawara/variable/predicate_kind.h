#pragma once

#include <ostream>
#include <string>
#include <string_view>

#include <cstdlib>

namespace yugawara::variable {

/**
 * @brief represents vocabulary kind of structured predicates.
 */
enum class predicate_kind {
    /// @brief compare with values.
    comparison,
    /// @brief negation of predicate.
    negation,
    /// @brief conjunction/disjunction of predicates.
    quantification,
};

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(predicate_kind value) noexcept {
    using namespace std::string_view_literals;
    using kind = predicate_kind;
    switch (value) {
        case kind::comparison: return "comparison"sv;
        case kind::negation: return "negation"sv;
        case kind::quantification: return "quantification"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, predicate_kind value) {
    return out << to_string_view(value);
}

} // namespace yugawara::variable

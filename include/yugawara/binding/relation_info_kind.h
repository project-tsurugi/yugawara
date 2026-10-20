#pragma once

#include <ostream>
#include <string>
#include <string_view>

#include <cstdlib>

#include <takatori/util/enum_set.h>

namespace yugawara::binding {

/**
 * @brief represents kind of relations which relational operators refer from outside of their operator graph.
 * @see relation_info
 */
enum class relation_info_kind {
    /// @brief a table index.
    index,
    /// @brief an upstream exchange.
    exchange,
    /// @brief a relation outside of the system.
    external,
};

/// @brief an enum set of relation_info_kind.
using relation_info_kind_set = ::takatori::util::enum_set<
        relation_info_kind,
        relation_info_kind::index,
        relation_info_kind::external>;

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(relation_info_kind value) noexcept {
    using namespace std::string_view_literals;
    using kind = relation_info_kind;
    switch (value) {
        case kind::index: return "index"sv;
        case kind::exchange: return "exchange"sv;
        case kind::external: return "external"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, relation_info_kind value) {
    return out << to_string_view(value);
}

} // namespace yugawara::binding

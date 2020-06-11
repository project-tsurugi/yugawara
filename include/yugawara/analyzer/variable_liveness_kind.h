#pragma once

#include <ostream>
#include <string_view>
#include <utility>

#include <takatori/util/enum_set.h>

namespace yugawara::analyzer {

/**
 * @brief represents kind of variable liveness.
 */
enum class variable_liveness_kind {
    define,
    use,
    kill,
};

/// @brief a set type of variable_liveness_kind.
using variable_liveness_kind_set = ::takatori::util::enum_set<
        variable_liveness_kind,
        variable_liveness_kind::define,
        variable_liveness_kind::kill>;

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
constexpr std::string_view to_string_view(variable_liveness_kind value) noexcept {
    using namespace std::string_view_literals;
    using kind = variable_liveness_kind;
    switch (value) {
        case kind::define: return "define"sv;
        case kind::use: return "use"sv;
        case kind::kill: return "kill"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, variable_liveness_kind value) noexcept {
    return out << to_string_view(value);
}

} // namespace yugawara::analyzer


#pragma once

#include <ostream>
#include <string_view>

#include <cstddef>
#include <cstdlib>

namespace yugawara::storage::partition {

/**
 * @brief represents a kind of prefix_extractor.
 */
enum class prefix_extractor_kind : std::size_t {
    /// @brief uses the whole column value (no prefix extraction).
    nothing = 0,
    /// @brief character_prefix.
    character_prefix,
    /// @brief binary_prefix.
    binary_prefix,
    /// @brief floor_division.
    floor_division,
    /// @brief floor_fractional.
    floor_fractional,
    /// @brief date_truncate.
    date_truncate,
    /// @brief timestamp_truncate.
    timestamp_truncate,
};

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(prefix_extractor_kind value) noexcept {
    using namespace std::string_view_literals;
    using kind = prefix_extractor_kind;
    switch (value) {
        case kind::nothing: return "nothing"sv;
        case kind::character_prefix: return "character_prefix"sv;
        case kind::binary_prefix: return "binary_prefix"sv;
        case kind::floor_division: return "floor_division"sv;
        case kind::floor_fractional: return "floor_fractional"sv;
        case kind::date_truncate: return "date_truncate"sv;
        case kind::timestamp_truncate: return "timestamp_truncate"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, prefix_extractor_kind value) {
    return out << to_string_view(value);
}

} // namespace yugawara::storage::partition

#pragma once

#include <iostream>
#include <string>
#include <string_view>

#include <cstdlib>

#include <takatori/type/data.h>

namespace yugawara::type {

/**
 * @brief represents category of data types.
 */
enum class category {
    /// @brief the boolean type category.
    boolean,
    /// @brief the number type category.
    number,
    /// @brief the character string type category.
    character_string,
    /// @brief the bit string type category.
    bit_string,
    /// @brief the temporal type category.
    temporal,
    /// @brief the time interval type category.
    time_interval,
    /// @brief the collection type category.
    collection,
    /// @brief the structure type category.
    structure,

    /**
     * @brief the pseudo category for unknown type.
     * @details unknown type is actually any member of categories.
     */
    unknown,

    /**
     * @brief the unique type category.
     * @details This is a pseudo category for types distinguished from other types.
     *      That is, individual types in this may virtually organize different categories.
     */
    unique,

    /**
     * @brief the third-party extension type category.
     * @details This is a pseudo category for extension types,
     *      and the individual types in this will decide the type conversion rule in their own way.
     */
    external,

    /**
     * @brief the unresolved type category.
     * @details This is a pseudo category for compiler intermediate types.
     *      This category includes erroneous type and pending type.
     */
    unresolved,
};

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(category value) noexcept {
    using namespace std::string_view_literals;
    using kind = category;
    switch (value) {
        case kind::unknown: return "unknown"sv;
        case kind::boolean: return "boolean"sv;
        case kind::number: return "number"sv;
        case kind::character_string: return "character_string"sv;
        case kind::bit_string: return "bit_string"sv;
        case kind::temporal: return "temporal"sv;
        case kind::time_interval: return "time_interval"sv;
        case kind::collection: return "collection"sv;
        case kind::structure: return "structure"sv;
        case kind::unique: return "unique"sv;
        case kind::external: return "external"sv;
        case kind::unresolved: return "unresolved"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, category value) noexcept {
    return out << to_string_view(value);
}

/**
 * @brief returns a category of the type.
 * @param type the target type
 * @return the category of the type
 */
category category_of(takatori::type::data const& type) noexcept;

} // namespace yugawara::type

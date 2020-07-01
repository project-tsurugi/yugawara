#pragma once

#include <memory>
#include <optional>

#include <memory>
#include <ostream>
#include <string>

#include <cstddef>

#include <takatori/util/object_creator.h>

#include <yugawara/util/move_only.h>

#include "provider.h"

namespace yugawara::schema {

/**
 * @brief represents a schema catalog.
 */
class catalog {
public:
    /// @brief the schema definition ID type.
    using definition_id_type = std::size_t;

    /// @brief the catalog name type.
    using name_type = std::basic_string<char, std::char_traits<char>, ::takatori::util::object_allocator<char>>;

    /**
     * @brief creates a new object.
     * @param definition_id the catalog definition ID
     * @param name the catalog name, may be an empty string
     * @param schema_provider the schema provider
     */
    explicit catalog(
            std::optional<definition_id_type> definition_id,
            name_type name,
            std::shared_ptr<provider> schema_provider = {}) noexcept;

    /**
     * @brief creates a new object.
     * @param name the catalog name, may be an empty string
     * @param definition_id the catalog definition ID
     * @param schema_provider the schema provider
     */
    catalog( // NOLINT
            std::string_view name,
            std::optional<definition_id_type> definition_id = {},
            std::shared_ptr<provider> schema_provider = {});

    /**
     * @brief returns the schema definition ID.
     * @return the definition ID
     * @return empty if it is not specified
     */
    [[nodiscard]] std::optional<definition_id_type> definition_id() const noexcept;

    /**
     * @brief sets the schema definition ID.
     * @param definition_id the definition ID
     * @return this
     */
    catalog& definition_id(std::optional<definition_id_type> definition_id) noexcept;

    /**
     * @brief returns the function name.
     * @return the function name
     */
    [[nodiscard]] std::string_view name() const noexcept;

    /**
     * @brief sets the function name.
     * @param name the function name
     * @return this
     */
    catalog& name(name_type name) noexcept;

    /**
     * @brief returns the schema provider of this catalog.
     * @return the provider
     */
    [[nodiscard]] provider const& schema_provider() const noexcept;

    /**
     * @brief returns the schema provider of this catalog.
     * @return the provider
     * @return empty if it is not defined
     */
    [[nodiscard]] std::shared_ptr<provider> shared_schema_provider() const noexcept;

    /**
     * @brief sets the schema provider of this catalog.
     * @param provider the schema provider
     * @return this
     */
    catalog& schema_provider(std::shared_ptr<provider> provider) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output stream
     */
    friend std::ostream& operator<<(std::ostream& out, catalog const& value);

private:
    util::move_only<std::optional<definition_id_type>> definition_id_;
    name_type name_;
    std::shared_ptr<provider> schema_provider_;
};

/**
 * @brief returns whether or not the two catalogs are same.
 * @param a the first schema
 * @param b the second schema
 * @return true if the both are same
 * @return false otherwise
 */
constexpr bool operator==(catalog const& a, catalog const& b) noexcept {
    return std::addressof(a) == std::addressof(b);
}

/**
 * @brief returns whether or not the two catalogs are different.
 * @param a the first schema
 * @param b the second schema
 * @return true if the both are different
 * @return false otherwise
 */
constexpr bool operator!=(catalog const& a, catalog const& b) noexcept {
    return !(a == b);
}

} // namespace yugawara::schema

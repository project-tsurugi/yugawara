#pragma once

#include <memory>
#include <optional>
#include <ostream>
#include <string>

#include <cstddef>

#include <yugawara/storage/provider.h>
#include <yugawara/variable/provider.h>
#include <yugawara/function/provider.h>
#include <yugawara/aggregate/provider.h>
#include <yugawara/type/provider.h>

namespace yugawara::schema {

/**
 * @brief represents a schema declaration.
 */
class declaration {
public:
    /// @brief the schema definition ID type.
    using definition_id_type = std::size_t;

    /// @brief the declaration name type.
    using name_type = std::string;

    /**
     * @brief creates a new object.
     * @param definition_id the schema definition ID
     * @param name the schema name
     * @param storage_provider the storage element provider
     * @param variable_provider the external variable declaration provider
     * @param function_provider the function declaration provider
     * @param set_function_provider the set function declaration provider
     * @param type_provider the user-defined type declaration provider
     */
    explicit declaration(
            std::optional<definition_id_type> definition_id,
            name_type name,
            std::shared_ptr<storage::provider> storage_provider = {},
            std::shared_ptr<variable::provider> variable_provider = {},
            std::shared_ptr<function::provider> function_provider = {},
            std::shared_ptr<aggregate::provider> set_function_provider = {},
            std::shared_ptr<type::provider> type_provider = {}) noexcept;

    /**
     * @brief creates a new object.
     * @param name the schema name
     * @param definition_id the schema definition ID
     * @param storage_provider the storage element provider
     * @param variable_provider the external variable declaration provider
     * @param function_provider the function declaration provider
     * @param set_function_provider the set function declaration provider
     * @param type_provider the user-defined type declaration provider
     */
    declaration( // NOLINT
            std::string_view name,
            std::optional<definition_id_type> definition_id = {},
            std::shared_ptr<storage::provider> storage_provider = {},
            std::shared_ptr<variable::provider> variable_provider = {},
            std::shared_ptr<function::provider> function_provider = {},
            std::shared_ptr<aggregate::provider> set_function_provider = {},
            std::shared_ptr<type::provider> type_provider = {});

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
    declaration& definition_id(std::optional<definition_id_type> definition_id) noexcept;

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
    declaration& name(name_type name) noexcept;

    /**
     * @brief returns the provider of storage elements in this schema.
     * @return the provider
     */
    [[nodiscard]] storage::provider const& storage_provider() const noexcept;

    /**
     * @brief returns the provider of storage elements in this schema.
     * @return the provider
     * @return empty if it is not defined
     */
    [[nodiscard]] std::shared_ptr<storage::provider> shared_storage_provider() const noexcept;

    /**
     * @brief sets the storage element provider.
     * @param provider the storage element provider
     * @return this
     */
    declaration& storage_provider(std::shared_ptr<storage::provider> provider) noexcept;

    /**
     * @brief returns the provider of variable declarations in this schema.
     * @return the provider
     */
    [[nodiscard]] variable::provider const& variable_provider() const noexcept;

    /**
     * @brief returns the provider of variable declarations in this schema.
     * @return the provider
     * @return empty if it is not defined
     */
    [[nodiscard]] std::shared_ptr<variable::provider> shared_variable_provider() const noexcept;

    /**
     * @brief sets the variable declaration provider.
     * @param provider the variable declaration provider
     * @return this
     */
    declaration& variable_provider(std::shared_ptr<variable::provider> provider) noexcept;

    /**
     * @brief returns the provider of function declarations in this schema.
     * @return the provider
     */
    [[nodiscard]] function::provider const& function_provider() const noexcept;

    /**
     * @brief returns the provider of function declarations in this schema.
     * @return the provider
     * @return empty if it is not defined
     */
    [[nodiscard]] std::shared_ptr<function::provider> shared_function_provider() const noexcept;

    /**
     * @brief sets the function declaration provider.
     * @param provider the function declaration provider
     * @return this
     */
    declaration& function_provider(std::shared_ptr<function::provider> provider) noexcept;

    /**
     * @brief returns the provider of set function declarations in this schema.
     * @return the provider
     */
    [[nodiscard]] aggregate::provider const& set_function_provider() const noexcept;

    /**
     * @brief returns the provider of set function declarations in this schema.
     * @return the provider
     * @return empty if it is not defined
     */
    [[nodiscard]] std::shared_ptr<aggregate::provider> shared_set_function_provider() const noexcept;

    /**
     * @brief sets the set function declaration provider.
     * @param provider the set function declaration provider
     * @return this
     */
    declaration& set_function_provider(std::shared_ptr<aggregate::provider> provider) noexcept;

    /**
     * @brief returns the provider of user-defined type declarations in this schema.
     * @return the provider
     */
    [[nodiscard]] type::provider const& type_provider() const noexcept;

    /**
     * @brief returns the provider of user-defined type declarations in this schema.
     * @return the provider
     * @return empty if it is not defined
     */
    [[nodiscard]] std::shared_ptr<type::provider> shared_type_provider() const noexcept;

    /**
     * @brief sets the user-defined type declaration provider.
     * @param provider the user-defined type declaration provider
     * @return this
     */
    declaration& type_provider(std::shared_ptr<type::provider> provider) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output stream
     */
    friend std::ostream& operator<<(std::ostream& out, declaration const& value);

private:
    std::optional<definition_id_type> definition_id_ {};
    name_type name_;
    std::shared_ptr<storage::provider> storage_provider_;
    std::shared_ptr<variable::provider> variable_provider_;
    std::shared_ptr<function::provider> function_provider_;
    std::shared_ptr<aggregate::provider> set_function_provider_;
    std::shared_ptr<type::provider> type_provider_;
};

/**
 * @brief returns whether or not the two schemas are same.
 * @param a the first schema
 * @param b the second schema
 * @return true if the both are same
 * @return false otherwise
 */
constexpr bool operator==(declaration const& a, declaration const& b) noexcept {
    return std::addressof(a) == std::addressof(b);
}

/**
 * @brief returns whether or not the two schemas are different.
 * @param a the first schema
 * @param b the second schema
 * @return true if the both are different
 * @return false otherwise
 */
constexpr bool operator!=(declaration const& a, declaration const& b) noexcept {
    return !(a == b);
}

} // namespace yugawara::schema

#pragma once

#include <memory>
#include <ostream>
#include <unordered_map>
#include <vector>

#include <takatori/descriptor/variable.h>
#include <takatori/type/data.h>

#include <takatori/util/clone_tag.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/sequence_view.h>

#include "relation_info.h"
#include "relation_info_kind.h"

namespace yugawara::binding {

/**
 * @brief refers relations outside of the system.
 * @details The compiler never generates this kind of relations.
 *      The runtime environment may rebuild the compiled execution plan, and replace exchanges referred from
 *      `take_flat` or `offer` with this kind of relations.
 * @note The descriptor which wraps this object is compared by its relation ID and columns,
 *      so that the columns must not be modified after it is wrapped.
 * @note Each column must be an exchange column (binding::variable_info_kind::exchange_column),
 *      because relational operators which refer this relation resolve it as an exchange column.
 */
class external_relation_info : public relation_info {
public:
    /// @brief the external relation kind.
    static constexpr relation_info_kind tag = relation_info_kind::external;

    /// @brief the relation ID type.
    using relation_id_type = std::size_t;

    /// @brief the pointer type of type hints.
    using type_pointer = std::shared_ptr<::takatori::type::data const>;

    /**
     * @brief creates a new object.
     * @param relation_id the ID of the relation outside of the system
     * @param columns the columns of the relation
     */
    explicit external_relation_info(
            relation_id_type relation_id,
            std::vector<::takatori::descriptor::variable> columns) noexcept;

    /**
     * @brief creates a new object.
     * @param other the copy source
     */
    explicit external_relation_info(::takatori::util::clone_tag_t, external_relation_info const& other);

    /**
     * @brief creates a new object.
     * @param other the move source
     */
    explicit external_relation_info(::takatori::util::clone_tag_t, external_relation_info&& other);

    /**
     * @brief returns the class ID.
     * @return the class ID
     */
    [[nodiscard]] std::size_t class_id() const noexcept final;

    [[nodiscard]] kind_type kind() const noexcept override;
    [[nodiscard]] external_relation_info* clone() const& override;
    [[nodiscard]] external_relation_info* clone() && override;

    /**
     * @brief returns the ID of the relation outside of the system.
     * @return the relation ID
     */
    [[nodiscard]] relation_id_type relation_id() const noexcept;

    /**
     * @brief returns the columns of this relation.
     * @return the columns
     */
    [[nodiscard]] std::vector<::takatori::descriptor::variable>& columns() noexcept;

    /// @copydoc columns()
    [[nodiscard]] std::vector<::takatori::descriptor::variable> const& columns() const noexcept;

    /**
     * @brief returns the columns which this relation accepts.
     * @return the input columns, same as columns()
     */
    [[nodiscard]] ::takatori::util::sequence_view<::takatori::descriptor::variable const> input_columns() const noexcept;

    /**
     * @brief returns the columns which this relation provides.
     * @return the output columns, same as columns()
     */
    [[nodiscard]] ::takatori::util::sequence_view<::takatori::descriptor::variable const> output_columns() const noexcept;

    /**
     * @brief returns the type hint of the given column.
     * @details This returns the registered hint even if the column is not in columns().
     * @param column the target column
     * @return the type hint
     * @return empty if it is not registered
     */
    [[nodiscard]] ::takatori::util::optional_ptr<::takatori::type::data const> find_type(
            ::takatori::descriptor::variable const& column) const;

    /**
     * @brief returns the type hint of the given column as its shared pointer.
     * @param column the target column
     * @return the type hint
     * @return empty if it is not registered
     * @see find_type()
     */
    [[nodiscard]] type_pointer find_type_shared(::takatori::descriptor::variable const& column) const;

    /**
     * @brief sets the type hint of the given column.
     * @details If the column already has a type hint, this replaces it.
     *      If the given type is empty, this removes the existing type hint.
     *      The column need not be in columns(), but such the hints are not shown in the outputs.
     * @param column the target column
     * @param type the type hint, or empty to remove it
     * @return this
     */
    external_relation_info& add_type(::takatori::descriptor::variable column, type_pointer type);

    /**
     * @brief sets the type hint of the given column.
     * @details If the column already has a type hint, this replaces it.
     * @param column the target column
     * @param type the type hint
     * @return this
     * @attention this may take a copy of argument
     * @see add_type(::takatori::descriptor::variable, type_pointer)
     */
    external_relation_info& add_type(::takatori::descriptor::variable column, ::takatori::type::data&& type);

    /**
     * @brief returns whether or not the two relations are equivalent.
     * @details This compares the relation IDs and the columns, but does not consider the type hints.
     * @param a the first relation
     * @param b the second relation
     * @return true if a == b
     * @return false otherwise
     */
    friend bool operator==(external_relation_info const& a, external_relation_info const& b) noexcept;

    /**
     * @brief returns whether or not the two relations are different.
     * @param a the first relation
     * @param b the second relation
     * @return true if a != b
     * @return false otherwise
     */
    friend bool operator!=(external_relation_info const& a, external_relation_info const& b) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @details This shows the type hints only for the variables in columns().
     * @param out the target output
     * @param value the target value
     * @return the output stream
     */
    friend std::ostream& operator<<(std::ostream& out, external_relation_info const& value);

protected:
    [[nodiscard]] bool equals(::takatori::descriptor::relation::entity_type const& other) const noexcept override;
    [[nodiscard]] std::size_t hash() const noexcept override;
    std::ostream& print_to(std::ostream& out) const override;

private:
    relation_id_type relation_id_;
    std::vector<::takatori::descriptor::variable> columns_;
    std::unordered_map<::takatori::descriptor::variable, type_pointer> type_hints_ {};
};

} // namespace yugawara::binding

/**
 * @brief std::hash specialization for yugawara::binding::external_relation_info.
 */
template<>
struct std::hash<yugawara::binding::external_relation_info> : public std::hash<yugawara::binding::relation_info> {};

#pragma once

#include <ostream>
#include <string_view>

#include <takatori/util/object_creator.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/reference_list_view.h>

#include "relation_kind.h"
#include "column.h"

namespace yugawara::storage {

class provider;

/**
 * @brief an abstract interface of external durable relations.
 * @note this does not represent indices of tables nor relations on exchange step.
 */
class relation {
public:
    /**
     * @brief creates a new instance.
     */
    relation() = default;

    /**
     * @brief destroys this object.
     */
    virtual ~relation() = default;

    relation(relation const& other) = delete;
    relation& operator=(relation const& other) = delete;
    relation(relation&& other) noexcept = delete;
    relation& operator=(relation&& other) noexcept = delete;

    /**
     * @brief returns the kind of this relation.
     * @return the relation kind
     */
    [[nodiscard]] virtual relation_kind kind() const noexcept = 0;

    /**
     * @brief returns the simple name of this relation.
     * @return the simple name
     * @return empty if the relation does not have its explicit name
     */
    [[nodiscard]] virtual std::string_view simple_name() const noexcept = 0;

    /**
     * @brief returns the view of columns in this relation.
     * @return the columns
     */
    [[nodiscard]] virtual column_list_view columns() const noexcept = 0;

    /**
     * @brief returns a clone of this object.
     * @param creator the object creator
     * @return the created clone
     */
    [[nodiscard]] virtual relation* clone(takatori::util::object_creator creator) const& = 0;

    /// @copydoc clone()
    [[nodiscard]] virtual relation* clone(takatori::util::object_creator creator) && = 0;

    /**
     * @brief returns the owner of this relation.
     * @return the owner
     * @return empty if there are no provides that can provide this relation
     */
    [[nodiscard]] ::takatori::util::optional_ptr<provider const> owner() const noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output
     */
    friend std::ostream& operator<<(std::ostream& out, relation const& value);

protected:
    /**
     * @brief appends string representation of this object into the given output.
     * @param out the target output
     * @return the output
     */
    virtual std::ostream& print_to(std::ostream& out) const = 0;

private:
    provider const* owner_ {};

    friend provider;
};

} // namespace yugawara::storage

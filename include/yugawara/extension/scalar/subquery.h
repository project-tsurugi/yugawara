#pragma once

#include <takatori/descriptor/variable.h>

#include <takatori/scalar/extension.h>

#include <takatori/relation/graph.h>

#include <takatori/util/clone_tag.h>

#include "extension_id.h"

namespace yugawara::extension::scalar {

/**
 * @brief pseudo scalar subquery expression.
 */
class subquery final : public ::takatori::scalar::extension {
public:
    /// @brief the extension ID of this type.
    static constexpr extension_id_type extension_tag = subquery_id;

    /// @brief the query graph type.
    using graph_type = ::takatori::relation::expression::graph_type;

    /// @brief the output port type.
    using output_port_type = ::takatori::relation::expression::output_port_type;

    /// @brief output column type.
    using column_type = ::takatori::descriptor::variable;

    /**
     * @brief creates a new object.
     * @param query_graph the query graph which represents this subquery operation
     * @param output_column the output column in the subquery
     */
    explicit subquery(graph_type query_graph, column_type output_column) noexcept;

    /**
     * @brief creates a new object.
     * @param other the copy source
     */
    explicit subquery(::takatori::util::clone_tag_t, subquery const& other);

    /**
     * @brief creates a new object.
     * @param other the move source
     */
    explicit subquery(::takatori::util::clone_tag_t, subquery&& other);

    /**
     * @brief returns a clone of this object.
     * @return the created clone
     */
    [[nodiscard]] subquery* clone() const& override;

    /// @copydoc clone()
    [[nodiscard]] subquery* clone() && override;

    /**
     * @brief returns the extension ID of this type.
     * @return the extension ID
     */
    [[nodiscard]] extension_id_type extension_id() const noexcept override;

    /**
     * @brief returns the query graph which represents this subquery operation.
     * @return the query graph
     */
    [[nodiscard]] graph_type& query_graph() noexcept;

    /// @copydoc query_graph()
    [[nodiscard]] graph_type const& query_graph() const noexcept;

    /**
     * @brief returns the output column in the subquery.
     * @return the output column
     */
    [[nodiscard]] column_type& output_column() noexcept;

    /// @copydoc output_column()
    [[nodiscard]] column_type const& output_column() const noexcept;

    /**
     * @brief returns the output port of subquery if defined.
     * @return the subquery output port
     * @return empty if it is not defined or ambiguous
     */
    [[nodiscard]] ::takatori::util::optional_ptr<output_port_type> find_output_port() noexcept;

    /// @copydoc find_output_port()
    [[nodiscard]] ::takatori::util::optional_ptr<output_port_type const> find_output_port() const noexcept;

    /**
     * @brief returns whether the two elements are equivalent.
     * @param a the first element
     * @param b the second element
     * @return true if a == b
     * @return false otherwise
     */
    friend bool operator==(subquery const& a, subquery const& b) noexcept;

    /**
     * @brief returns whether the two elements are different.
     * @param a the first element
     * @param b the second element
     * @return true if a != b
     * @return false otherwise
     */
    friend bool operator!=(subquery const& a, subquery const& b) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output
     */
    friend std::ostream& operator<<(std::ostream& out, subquery const& value);

protected:
    /**
     * @brief returns whether this extension is equivalent to the target one.
     * @param other the target extension
     * @return true if the both are equivalent
     * @return false otherwise
     */
    [[nodiscard]] bool equals(extension const& other) const noexcept override;

    /**
     * @brief appends string representation of this object into the given output.
     * @param out the target output
     * @return the output
     */
    std::ostream& print_to(std::ostream& out) const override;

private:
    graph_type query_graph_;
    column_type output_column_;
};

} // namespace yugawara::extension::scalar

#pragma once

#include <takatori/descriptor/variable.h>

#include <takatori/scalar/comparison_operator.h>
#include <takatori/scalar/extension.h>
#include <takatori/scalar/quantifier.h>

#include <takatori/relation/graph.h>

#include <takatori/util/clone_tag.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/ownership_reference.h>

#include "extension_id.h"

namespace yugawara::extension::scalar {

/**
 * @brief pseudo scalar quantified compare predicate expression.
 * @see ::takatori::scalar::compare
 */
class quantified_compare final : public ::takatori::scalar::extension {
public:
    /// @brief the extension ID of this type.
    static constexpr extension_id_type extension_tag = quantified_compare_id;

    /// @brief comparison operator kind.
    using operator_kind_type = ::takatori::scalar::comparison_operator;

    /// @brief quantifier type.
    using quantifier_type = ::takatori::scalar::quantifier;

    /// @brief the query graph type.
    using graph_type = ::takatori::relation::expression::graph_type;

    /// @brief output column type.
    using column_type = ::takatori::descriptor::variable;

    /// @brief the output port type.
    using output_port_type = ::takatori::relation::expression::output_port_type;

    /**
     * @brief creates a new object.
     * @param operator_kind the comparison operator kind
     * @param quantifier the quantifier
     * @param left the left operand
     * @param query_graph the right operand query graph
     * @param right_column the output column in the subquery
     */
    explicit quantified_compare(
            operator_kind_type operator_kind,
            quantifier_type quantifier,
            std::unique_ptr<expression> left,
            graph_type query_graph,
            column_type right_column) noexcept;

    /**
     * @brief creates a new object.
     * @param operator_kind the comparison operator kind
     * @param quantifier the quantifier
     * @param left the left operand
     * @param query_graph the right operand query graph
     * @param right_column the output column in the subquery
     * @attention this may take copies of given expressions
     */
    explicit quantified_compare(
            operator_kind_type operator_kind,
            quantifier_type quantifier,
            expression&& left,
            graph_type query_graph,
            column_type right_column);

    /**
     * @brief creates a new object.
     * @param other the copy source
     */
    explicit quantified_compare(::takatori::util::clone_tag_t, quantified_compare const& other);

    /**
     * @brief creates a new object.
     * @param other the move source
     */
    explicit quantified_compare(::takatori::util::clone_tag_t, quantified_compare&& other);

    /**
     * @brief returns a clone of this object.
     * @return the created clone
     */
    [[nodiscard]] quantified_compare* clone() const& override;

    /// @copydoc clone()
    [[nodiscard]] quantified_compare* clone() && override;

    /**
     * @brief returns the extension ID of this type.
     * @return the extension ID
     */
    [[nodiscard]] extension_id_type extension_id() const noexcept override;

    /**
     * @brief returns the operator kind.
     * @return operator kind
     */
    [[nodiscard]] operator_kind_type& operator_kind() noexcept;

    /// @copydoc operator_kind()
    [[nodiscard]] operator_kind_type operator_kind() const noexcept;

    /**
     * @brief returns the operator kind.
     * @return operator kind
     */
    [[nodiscard]] quantifier_type& quantifier() noexcept;

    /// @copydoc quantifier()
    [[nodiscard]] quantifier_type quantifier() const noexcept;

    /**
     * @brief returns the left operand.
     * @return the left operand
     * @warning undefined behavior if the operand is absent
     */
    [[nodiscard]] expression& left() noexcept;

    /**
     * @brief returns the left operand.
     * @return the left operand
     * @warning undefined behavior if the operand is absent
     */
    [[nodiscard]] expression const& left() const noexcept;

    /**
     * @brief returns the left operand.
     * @return the left operand
     * @return empty if the operand is absent
     */
    [[nodiscard]] ::takatori::util::optional_ptr<expression> optional_left() noexcept;

    /// @copydoc optional_left()
    [[nodiscard]] ::takatori::util::optional_ptr<expression const> optional_left() const noexcept;

    /**
     * @brief releases the left operand.
     * @return the left operand
     * @return empty if the operand is absent
     */
    [[nodiscard]] std::unique_ptr<expression> release_left() noexcept;

    /**
     * @brief sets the left operand.
     * @param left the replacement
     * @return this
     */
    quantified_compare& left(std::unique_ptr<expression> left) noexcept;

    /**
     * @brief returns ownership reference of the left operand.
     * @return the left operand
     */
    [[nodiscard]] ::takatori::util::ownership_reference<expression> ownership_left();

    /**
     * @brief returns the right operand query graph.
     * @return the query graph
     */
    [[nodiscard]] graph_type& query_graph() noexcept;

    /// @copydoc query_graph()
    [[nodiscard]] graph_type const& query_graph() const noexcept;

    /**
     * @brief returns the right operand column in the subquery.
     * @return the right operand column
     */
    [[nodiscard]] column_type& right_column() noexcept;

    /// @copydoc right_column()
    [[nodiscard]] column_type const& right_column() const noexcept;

    /**
     * @brief returns the output port of quantified compare if defined.
     * @return the quantified compare output port
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
    friend bool operator==(quantified_compare const& a, quantified_compare const& b) noexcept;

    /**
     * @brief returns whether the two elements are different.
     * @param a the first element
     * @param b the second element
     * @return true if a != b
     * @return false otherwise
     */
    friend bool operator!=(quantified_compare const& a, quantified_compare const& b) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output
     */
    friend std::ostream& operator<<(std::ostream& out, quantified_compare const& value);

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
    operator_kind_type operator_kind_;
    quantifier_type quantifier_;
    std::unique_ptr<expression> left_;
    graph_type query_graph_;
    column_type right_column_;
};

} // namespace yugawara::extension::scalar

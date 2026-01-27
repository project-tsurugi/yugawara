#pragma once

#include <vector>

#include <takatori/relation/intermediate/extension.h>
#include <takatori/relation/details/mapping_element.h>

#include <takatori/util/clone_tag.h>
#include <takatori/util/optional_ptr.h>

#include "extension_id.h"

namespace yugawara::extension::relation {

/**
 * @brief pseudo relation expression that represents a subquery.
 */
class subquery final : public ::takatori::relation::intermediate::extension {
public:
    /// @brief the extension ID of this type.
    static constexpr extension_id_type extension_tag = subquery_id;

    /// @brief variable mapping type.
    using mapping_type = ::takatori::relation::details::mapping_element;

    /**
     * @brief creates a new object.
     * @param query_graph the query graph which represents this subquery operation
     * @param mappings the variable mappings
     *      each source represents the subquery output column,
     *      and destination represents the outer query column
     */
    explicit subquery(graph_type query_graph, std::vector<mapping_type> mappings) noexcept;

    /**
     * @brief creates a new object.
     * @param other the copy source
     */
    explicit subquery(::takatori::util::clone_tag_t, subquery const& other);

    /**
     * @brief creates a new object.
     * @param other the move source
     */
    explicit subquery(::takatori::util::clone_tag_t, subquery&& other) noexcept;

    [[nodiscard]] ::takatori::util::sequence_view<input_port_type> input_ports() noexcept override;
    [[nodiscard]] ::takatori::util::sequence_view<input_port_type const> input_ports() const noexcept override;
    [[nodiscard]] ::takatori::util::sequence_view<output_port_type> output_ports() noexcept override;
    [[nodiscard]] ::takatori::util::sequence_view<output_port_type const> output_ports() const noexcept override;
    [[nodiscard]] subquery* clone() const& override;
    [[nodiscard]] subquery* clone() && override;
    [[nodiscard]] extension_id_type extension_id() const noexcept override;

    /**
     * @brief returns the output port of this expression.
     * @return the output port
     */
    [[nodiscard]] output_port_type& output() noexcept;

    /// @copydoc output()
    [[nodiscard]] output_port_type const& output() const noexcept;

    /**
     * @brief returns the query graph which represents this subquery operation.
     * @return the query graph
     */
    [[nodiscard]] graph_type& query_graph() noexcept;

    /// @copydoc query_graph()
    [[nodiscard]] graph_type const& query_graph() const noexcept;

    /**
     * @brief returns the variable mappings.
     *      each source represents the subquery output column,
     *      and destination represents the outer query column
     * @return the variable mappings
     */
    [[nodiscard]] std::vector<mapping_type>& mappings() noexcept;

    /// @copydoc mappings()
    [[nodiscard]] std::vector<mapping_type> const& mappings() const noexcept;

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
     * @details This operation does not consider which the input/output ports are connected to.
     * @param a the first element
     * @param b the second element
     * @return true if a == b
     * @return false otherwise
     */
    friend bool operator==(subquery const& a, subquery const& b) noexcept;

    /**
     * @brief returns whether the two elements are different.
     * @details This operation does not consider which the input/output ports are connected to.
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
    [[nodiscard]] bool equals(extension const& other) const noexcept override;
    std::ostream& print_to(std::ostream& out) const override;

private:
    output_port_type output_;
    graph_type query_graph_;
    std::vector<mapping_type> mappings_;
};

} // namespace yugawara::extension::relation

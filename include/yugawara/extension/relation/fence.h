#pragma once

#include <ostream>

#include <takatori/relation/intermediate/extension.h>

#include <takatori/util/clone_tag.h>

#include <yugawara/remote/location_info.h>

#include "extension_id.h"

namespace yugawara::extension::relation {

/**
 * @brief pseudo relation expression that represents a boundary of the execution locations.
 * @details This passes through the input rows to the output without any changes.
 *      The upstream and downstream operators of this expression may be executed in the different locations,
 *      and the optimizations must not move or merge the operators across this expression.
 *      This appears only in the intermediate execution plan,
 *      and it will be removed or replaced with a forward exchange while building the step execution plan.
 */
class fence final : public ::takatori::relation::intermediate::extension {
public:
    /// @brief the extension ID of this type.
    static constexpr extension_id_type extension_tag = fence_id;

    /// @brief the location ID type.
    using location_id_type = remote::location_info::location_id_type;

    /**
     * @brief creates a new object.
     * @param upstream the execution location of the upstream operators
     * @param downstream the execution location of the downstream operators
     */
    explicit fence(location_id_type upstream, location_id_type downstream) noexcept;

    /**
     * @brief creates a new object.
     * @param other the copy source
     */
    explicit fence(::takatori::util::clone_tag_t, fence const& other) noexcept;

    /**
     * @brief creates a new object.
     * @param other the move source
     */
    explicit fence(::takatori::util::clone_tag_t, fence&& other) noexcept;

    [[nodiscard]] ::takatori::util::sequence_view<input_port_type> input_ports() noexcept override;
    [[nodiscard]] ::takatori::util::sequence_view<input_port_type const> input_ports() const noexcept override;
    [[nodiscard]] ::takatori::util::sequence_view<output_port_type> output_ports() noexcept override;
    [[nodiscard]] ::takatori::util::sequence_view<output_port_type const> output_ports() const noexcept override;
    [[nodiscard]] fence* clone() const& override;
    [[nodiscard]] fence* clone() && override;
    [[nodiscard]] extension_id_type extension_id() const noexcept override;

    /**
     * @brief returns the input port of this expression.
     * @return the input port
     */
    [[nodiscard]] input_port_type& input() noexcept;

    /// @copydoc input()
    [[nodiscard]] input_port_type const& input() const noexcept;

    /**
     * @brief returns the output port of this expression.
     * @return the output port
     */
    [[nodiscard]] output_port_type& output() noexcept;

    /// @copydoc output()
    [[nodiscard]] output_port_type const& output() const noexcept;

    /**
     * @brief returns the execution location of the upstream operators.
     * @return the upstream location ID
     */
    [[nodiscard]] location_id_type upstream() const noexcept;

    /**
     * @brief sets the execution location of the upstream operators.
     * @param location the upstream location ID
     * @return this
     */
    fence& upstream(location_id_type location) noexcept;

    /**
     * @brief returns the execution location of the downstream operators.
     * @return the downstream location ID
     */
    [[nodiscard]] location_id_type downstream() const noexcept;

    /**
     * @brief sets the execution location of the downstream operators.
     * @param location the downstream location ID
     * @return this
     */
    fence& downstream(location_id_type location) noexcept;

    /**
     * @brief returns whether the two elements are equivalent.
     * @details This operation does not consider which the input/output ports are connected to.
     * @param a the first element
     * @param b the second element
     * @return true if a == b
     * @return false otherwise
     */
    friend bool operator==(fence const& a, fence const& b) noexcept;

    /**
     * @brief returns whether the two elements are different.
     * @details This operation does not consider which the input/output ports are connected to.
     * @param a the first element
     * @param b the second element
     * @return true if a != b
     * @return false otherwise
     */
    friend bool operator!=(fence const& a, fence const& b) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output
     */
    friend std::ostream& operator<<(std::ostream& out, fence const& value);

protected:
    [[nodiscard]] bool equals(extension const& other) const noexcept override;
    std::ostream& print_to(std::ostream& out) const override;

private:
    input_port_type input_;
    output_port_type output_;
    location_id_type upstream_;
    location_id_type downstream_;
};

} // namespace yugawara::extension::relation

#pragma once

#include <takatori/plan/exchange.h>

#include "relation_info.h"
#include "relation_info_kind.h"

namespace yugawara::binding {

/**
 * @brief refers existing exchange step.
 */
class exchange_info : public relation_info {
public:
    /// @brief the external relation kind.
    static constexpr relation_info_kind tag = relation_info_kind::exchange;

    /**
     * @brief creates a new object.
     * @param declaration the original declaration
     */
    explicit exchange_info(::takatori::plan::exchange const& declaration) noexcept;

    /**
     * @brief creates a new object.
     * @param other the copy source
     * @param creator the object creator
     */
    explicit exchange_info(exchange_info const& other, ::takatori::util::object_creator creator);

    /**
     * @brief creates a new object.
     * @param other the move source
     * @param creator the object creator
     */
    explicit exchange_info(exchange_info&& other, ::takatori::util::object_creator creator);

    kind_type kind() const noexcept override;
    exchange_info* clone(takatori::util::object_creator creator) const& override;
    exchange_info* clone(takatori::util::object_creator creator) && override;

    /**
     * @brief returns the origin of the exchange step.
     * @details If the target exchange step was disposed, this object will become invalid.
     * @return the exchange step
     */
    ::takatori::plan::exchange const& declaration() const noexcept;

    /// FIXME: transform view

    /**
     * @brief returns whether or not the both exchange steps are identical.
     * @param a the first relation
     * @param b the second relation
     * @return true if a == b
     * @return false otherwise
     */
    friend bool operator==(exchange_info const& a, exchange_info const& b) noexcept;

    /**
     * @brief returns whether or not the two relations are different.
     * @param a the first relation
     * @param b the second relation
     * @return true if a != b
     * @return false otherwise
     */
    friend bool operator!=(exchange_info const& a, exchange_info const& b) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output stream
     */
    friend std::ostream& operator<<(std::ostream& out, exchange_info const& value);

protected:
    bool equals(relation_info const& other) const noexcept override;
    std::size_t hash() const noexcept override;
    std::ostream& print_to(std::ostream& out) const override;

private:
    ::takatori::plan::exchange const* declaration_;
};

} // namespace yugawara::binding

/**
 * @brief std::hash specialization for yugawara::binding::exchange_info.
 */
template<>
struct std::hash<yugawara::binding::exchange_info> : public std::hash<yugawara::binding::relation_info> {};

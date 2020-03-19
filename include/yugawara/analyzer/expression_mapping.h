#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

#include <takatori/scalar/expression.h>
#include <takatori/type/data.h>
#include <takatori/util/object_creator.h>

namespace yugawara::analyzer {

/**
 * @brief holds information of each variable binding.
 */
class expression_mapping {
public:
    /**
     * @brief creates a new instance.
     */
    expression_mapping() noexcept = default;

    /**
     * @brief creates a new instance.
     * @param creator the object creator
     */
    explicit expression_mapping(::takatori::util::object_creator creator) noexcept;

    /**
     * @brief returns the resolved type for the expression.
     * @param expression the target expression
     * @return the resolved type
     * @return empty if the target expression has not been resolved yet
     */
    std::shared_ptr<::takatori::type::data const> find(::takatori::scalar::expression const& expression) const;

    /**
     * @brief sets the resolved type for the expression.
     * @attention this is designed only for testing
     * @param expression the target expression
     * @param type the expression type
     * @param overwrite whether or not the overwrite the existing result if it exists
     * @return the bound type
     * @throws std::domain_error if the specified variable has been resolved with `overwrite=true`
     */
    std::shared_ptr<::takatori::type::data const> const& bind(
            ::takatori::scalar::expression const& expression,
            std::shared_ptr<::takatori::type::data const> type,
            bool overwrite = false);

    /**
     * @brief sets the resolved type for the expression.
     * @param expression the target expression
     * @param type the expression type
     * @param overwrite whether or not the overwrite the existing result if it exists
     * @return the bound type
     * @throws std::domain_error if the specified variable has been resolved with `overwrite=true`
     * @note this is defined only for testing
     */
    std::shared_ptr<::takatori::type::data const> const& bind(
            ::takatori::scalar::expression const& expression,
            ::takatori::type::data&& type,
            bool overwrite = false);

    /**
     * @brief returns the object creator.
     * @return the object creator
     */
    takatori::util::object_creator get_object_creator() const;

private:
    std::unordered_map<
            ::takatori::scalar::expression const*,
            std::shared_ptr<::takatori::type::data const>,
            std::hash<::takatori::scalar::expression const*>,
            std::equal_to<>,
            ::takatori::util::object_allocator<std::pair<
                    ::takatori::scalar::expression const* const,
                    std::shared_ptr<::takatori::type::data>>>> mapping_;
};

} // namespace yugawara::analyzer

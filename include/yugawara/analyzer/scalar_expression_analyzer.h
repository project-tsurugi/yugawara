#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include <takatori/descriptor/variable.h>
#include <takatori/relation/expression.h>
#include <takatori/scalar/expression.h>
#include <takatori/type/data.h>
#include <takatori/util/object_creator.h>
#include <takatori/util/sequence_view.h>

#include "type_diagnostic.h"

#include <yugawara/type/repository.h>
#include <yugawara/binding/variable_info.h>

namespace yugawara::analyzer {

/**
 * @brief analyzes the resulting type of scalar expressions.
 */
class scalar_expression_analyzer {
public:
    /**
     * @brief creates a new instance.
     */
    scalar_expression_analyzer() noexcept = default;

    /**
     * @brief creates a new instance.
     * @param creator the object creator
     */
    explicit scalar_expression_analyzer(::takatori::util::object_creator creator) noexcept;

    /**
     * @brief computes the type of the given scalar expression.
     * @details the including variables must be resolved before this operation, or this will return an erroneous type
     * @param expression the target expression
     * @param repo the type repository
     * @return the resolved type if resolution was succeeded
     * @return erroneous type if the expression was wrong
     * @return pending type if the input contains erroneous or pending type and the resulting type will refer its input
     * @throws std::domain_error if the specified expression contains unresolved variables
     * @note this operation may add diagnostics() if the expression was wrong, but the resulting type will not be
     *      an erroneous type (e.g. the resulting type does not depends on the input expressions)
     * @note if resolution was not success, this may return some special types generated out of the given repository
     */
    std::shared_ptr<::takatori::type::data const> resolve(
            ::takatori::scalar::expression const& expression,
            type::repository& repo = type::default_repository());

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
     * @return this
     * @throws std::domain_error if the specified expression has been resolved as the different type
     */
    scalar_expression_analyzer& bind(
            ::takatori::scalar::expression const& expression,
            std::shared_ptr<::takatori::type::data const> type);

    /**
     * @brief returns the resolved type for the expression.
     * @param variable the target variable
     * @return the resolved type
     * @return empty if the target expression has not been resolved yet
     */
    std::shared_ptr<::takatori::type::data const> find(::takatori::descriptor::variable const& variable) const;

    /**
     * @brief sets the resolved type for the variable.
     * @param variable the target variable
     * @param type the expression type
     * @return this
     * @throws std::domain_error if the specified variable has been resolved as the different type
     */
    scalar_expression_analyzer& bind(
            ::takatori::descriptor::variable const& variable,
            std::shared_ptr<::takatori::type::data const> type);

    /**
     * @brief sets the resolved type for the variable.
     * @param variable the target variable
     * @param type the expression type
     * @return this
     * @throws std::domain_error if the specified variable has been resolved as the different type
     * @note this is defined only for testing
     */
    scalar_expression_analyzer& bind(
            ::takatori::descriptor::variable const& variable,
            ::takatori::type::data&& type);

    /**
     * @brief returns whether or not this saw any diagnostics while analyzing scalar expressions.
     * @return true if there are any diagnostics
     * @return false otherwise
     */
    bool has_diagnostics() const noexcept;

    /**
     * @brief returns diagnostics while analyzing scalar expressions.
     * @return the diagnostics
     */
    ::takatori::util::sequence_view<type_diagnostic const> diagnostics() const noexcept;

    /**
     * @brief remove all diagnostics while analyzing scalar expression.
     */
    void clear_diagnostics() noexcept;

    /**
     * @brief returns the object creator.
     * @return the object creator
     */
    takatori::util::object_creator get_object_creator() const;

private:
    std::unordered_map<
            ::takatori::descriptor::variable,
            std::shared_ptr<::takatori::type::data const>,
            std::hash<::takatori::descriptor::variable>,
            std::equal_to<>,
            ::takatori::util::object_allocator<std::pair<
                    ::takatori::descriptor::variable const,
                    std::shared_ptr<::takatori::type::data>>>> variables_;

    std::unordered_map<
            ::takatori::scalar::expression const*,
            std::shared_ptr<::takatori::type::data const>,
            std::hash<::takatori::scalar::expression const*>,
            std::equal_to<>,
            ::takatori::util::object_allocator<std::pair<
                    ::takatori::scalar::expression const* const,
                    std::shared_ptr<::takatori::type::data>>>> expressions_;

    std::vector<type_diagnostic, ::takatori::util::object_allocator<type_diagnostic>> diagnostics_;
};

} // namespace yugawara::analyzer

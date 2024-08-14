#pragma once

#include <tsl/hopscotch_map.h>

#include <takatori/descriptor/variable.h>

#include <takatori/scalar/expression.h>

#include <takatori/util/ownership_reference.h>

namespace yugawara::analyzer::details {

class inline_variables {
public:
    using map_type = ::tsl::hopscotch_map<
            ::takatori::descriptor::variable::reference_type,
            std::unique_ptr<::takatori::scalar::expression>>;

    /**
     * @brief reserves the space for the variables to be inlined.
     * @param size the number of variables to be inlined
     */
    void reserve(std::size_t size);

    /**
     * @brief declares a variable to be inlined.
     * @param variable the target variable
     * @param replacement the replacement expression
     * @return this
     */
    void declare(
            ::takatori::descriptor::variable variable,
            std::unique_ptr<::takatori::scalar::expression> replacement);

    /**
     * @brief applies the inlining of the variables to the target expression.
     * @param target the target expression
     */
    void apply(::takatori::util::ownership_reference<::takatori::scalar::expression> target) const;

private:
    std::vector<::takatori::descriptor::variable> variables_ {};
    map_type replacements_;
};

} // namespace yugawara::analyzer::details

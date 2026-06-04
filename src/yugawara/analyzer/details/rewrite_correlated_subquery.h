#pragma once

#include <vector>

#include <takatori/descriptor/variable.h>

#include <takatori/relation/expression.h>
#include <takatori/relation/details/mapping_element.h>

#include <yugawara/diagnostic.h>

#include <yugawara/analyzer/intermediate_plan_normalizer_code.h>

#include <yugawara/extension/scalar/subquery.h>
#include <yugawara/extension/scalar/exists.h>
#include <yugawara/extension/scalar/quantified_compare.h>

namespace yugawara::analyzer::details {

class correlated_subquery_input {
public:
    using mapping_type = ::takatori::relation::details::mapping_element;

    correlated_subquery_input(
            ::takatori::relation::expression::input_port_type& input_port,
            std::vector<mapping_type> mappings) noexcept;

    ::takatori::relation::expression::input_port_type& input_port() noexcept;

    /**
     * @brief input parameter mappings (subquery parameter -> input parameter).
     * @return parameter mappings
     */
    std::vector<mapping_type>& mappings() noexcept;

private:
    ::takatori::relation::expression::input_port_type& input_port_;
    std::vector<mapping_type> mappings_;
};

class rewrite_correlated_subquery_result {
public:
    using diagnostic_code_type = intermediate_plan_normalizer_code;
    using diagnostic_type = diagnostic<diagnostic_code_type>;
    using input_type = correlated_subquery_input;
    using mapping_type = ::takatori::relation::details::mapping_element;

    explicit rewrite_correlated_subquery_result(std::vector<diagnostic_type> diagnostics) noexcept;

    rewrite_correlated_subquery_result(
            std::vector<input_type> inputs,
            std::vector<mapping_type> output_mappings) noexcept;

    [[nodiscard]] explicit operator bool() const noexcept;

    [[nodiscard]] std::vector<diagnostic_type>& diagnostics() noexcept;

    [[nodiscard]] std::vector<input_type>& inputs() noexcept;

    /**
     * @brief output parameter mappings (subquery parameter -> output parameter).
     * @return parameter mappings
     */
    std::vector<mapping_type>& output_mappings() noexcept;

private:
    std::vector<diagnostic_type> diagnostics_ {};
    std::vector<correlated_subquery_input> inputs_ {};
    std::vector<mapping_type> output_mappings_ {};
};

rewrite_correlated_subquery_result rewrite_correlated_subquery(extension::scalar::subquery& expr);

rewrite_correlated_subquery_result rewrite_correlated_subquery(extension::scalar::exists& expr);

rewrite_correlated_subquery_result rewrite_correlated_subquery(extension::scalar::quantified_compare& expr);

} // namespace yugawara::analyzer::details

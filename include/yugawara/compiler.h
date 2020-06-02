#pragma once

#include <vector>

#include <takatori/scalar/expression.h>
#include <takatori/relation/graph.h>
#include <takatori/plan/graph.h>
#include <takatori/statement/statement.h>
#include <takatori/util/object_creator.h>

#include <yugawara/util/either.h>

#include "compiler_options.h"
#include "compiled_info.h"
#include "compiler_result.h"

namespace yugawara {

/**
 * @brief compiles execution plan or statements.
 */
class compiler {
public:
    /// @brief the compiler options type.
    using options_type = compiler_options;

    /// @brief the compiler result type.
    using result_type = compiler_result;

    /// @brief the inspection result type.
    using info_type = compiled_info;

    /// @brief diagnostic information type.
    using diagnostic_type = result_type::diagnostic_type;

    /**
     * @brief compiles the given intermediate plan.
     * @param options the compiler options
     * @param plan the target intermediate plan
     * @return the compilation result
     * @return an invalid result object if compilation was failed
     */
    [[nodiscard]] result_type operator()(options_type const& options, ::takatori::relation::graph_type&& plan);

    /**
     * @brief compiles the given statement.
     * @param options the compiler options
     * @param statement the target statement
     * @return the compilation result
     * @return an invalid result object if compilation was failed
     */
    [[nodiscard]] result_type operator()(options_type const& options, ::takatori::util::unique_object_ptr<::takatori::statement::statement> statement);

    /**
     * @brief compiles the given statement.
     * @param options the compiler options
     * @param statement the target statement
     * @return the compilation result
     * @return an invalid result object if compilation was failed
     * @attention This may take a copy of the arguments
     */
    [[nodiscard]] result_type operator()(options_type const& options, ::takatori::statement::statement&& statement);

    /**
     * @brief inspects the compiled expression.
     * @note This function is mainly designed for testing.
     * @param expression the source expression
     * @return the inspection information
     * @return diagnostics if the source was not valid
     */
    [[nodiscard]] util::either<std::vector<diagnostic_type>, info_type> inspect(::takatori::scalar::expression const& expression);

    /**
     * @brief inspects the compiled relational operator graph.
     * @note This function is mainly designed for testing.
     * @param graph the source relational operator graph
     * @return the inspection information
     * @return diagnostics if the source was not valid
     */
    [[nodiscard]] util::either<std::vector<diagnostic_type>, info_type> inspect(::takatori::relation::graph_type const& graph);

    /**
     * @brief inspects the compiled step graph.
     * @note This function is mainly designed for testing.
     * @param graph the source step graph
     * @return the inspection information
     * @return diagnostics if the source was not valid
     */
    [[nodiscard]] util::either<std::vector<diagnostic_type>, info_type> inspect(::takatori::plan::graph_type const& graph);

    /**
     * @brief inspects the compiled step graph.
     * @note This function is mainly designed for testing.
     * @param statement the source statement
     * @return the inspection information
     * @return diagnostics if the source was not valid
     */
    [[nodiscard]] util::either<std::vector<diagnostic_type>, info_type> inspect(::takatori::statement::statement const& statement);
};

} // namespace yugawara


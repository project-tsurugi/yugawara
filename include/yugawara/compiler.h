#pragma once

#include <takatori/relation/graph.h>
#include <takatori/statement/statement.h>
#include <takatori/util/object_creator.h>

#include "compiler_options.h"
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
};

} // namespace yugawara


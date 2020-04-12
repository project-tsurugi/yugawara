#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <takatori/util/object_creator.h>

#include "relation_info.h"
#include "index_info.h"
#include "exchange_info.h"
#include "variable_info.h"
#include "table_column_info.h"
#include "external_variable_info.h"
#include "function_info.h"
#include "aggregate_function_info.h"

namespace yugawara::binding {

/**
 * @brief provides binding descriptor.
 * @details Developers can create this class's objects directly.
 * @note To extract binding information from descriptors, please use ::yugawara::binding::unwrap() to them.
 */
class factory {
public:
    /// @details a vector of variables.
    using variable_vector = std::vector<::takatori::descriptor::variable, ::takatori::util::object_allocator<::takatori::descriptor::variable>>;

    /**
     * @brief creates a new object.
     * @param creator the object creator
     */
    explicit factory(::takatori::util::object_creator creator = {}) noexcept;

    /**
     * @brief creates a new index descriptor.
     * @param declaration the original declaration
     */
    ::takatori::descriptor::relation index(storage::index const& declaration);

    /**
     * @brief creates a new exchange descriptor.
     * @param declaration the original declaration
     */
    ::takatori::descriptor::relation exchange(::takatori::plan::exchange const& declaration);

    /**
     * @brief returns a variable descriptor for the existing table column.
     * @param declaration the target table column
     * @return the corresponded variable descriptor
     */
    ::takatori::descriptor::variable table_column(storage::column const& declaration);

    /**
     * @brief returns variable descriptors for the existing table columns.
     * @param columns the target table columns
     * @return the corresponded variable descriptors, ordered as same as the original columns
     */
    variable_vector table_columns(storage::column_list_view const& columns);

    /**
     * @brief returns a variable descriptor for a new exchange column.
     * @param label the variable label (for debugging)
     * @return the created variable descriptor
     */
    ::takatori::descriptor::variable exchange_column(std::string_view label = {});

    /**
     * @brief creates a new external variable descriptor.
     * @param declaration the original declaration
     */
    ::takatori::descriptor::variable external_variable(variable::declaration const& declaration);

    /**
     * @brief returns a variable descriptor for a new stream column.
     * @param label the variable label (for debugging)
     * @return the created variable descriptor
     */
    ::takatori::descriptor::variable stream_variable(std::string_view label = {});

    /**
     * @brief returns a variable descriptor for a local variable declared in the scalar expression.
     * @param label the variable label (for debugging)
     * @return the created variable descriptor
     */
    ::takatori::descriptor::variable local_variable(std::string_view label = {});

    /**
     * @brief creates a new function descriptor.
     * @param declaration the original declaration
     */
    ::takatori::descriptor::function function(std::shared_ptr<function::declaration const> declaration);

    /**
     * @brief creates a new function descriptor.
     * @param declaration the original declaration
     * @attention this may take a copy of argument
     */
    ::takatori::descriptor::function function(function::declaration&& declaration);

    /**
     * @brief creates a new aggregate function descriptor.
     * @param declaration the original declaration
     */
    ::takatori::descriptor::aggregate_function aggregate_function(std::shared_ptr<aggregate::declaration const> declaration);

    /**
     * @brief creates a new aggregate function descriptor.
     * @param declaration the original declaration
     * @attention this may take a copy of argument
     */
    ::takatori::descriptor::aggregate_function aggregate_function(aggregate::declaration&& declaration);

    /// @copydoc index()
    ::takatori::descriptor::relation operator()(storage::index const& declaration);

    /// @copydoc exchange()
    ::takatori::descriptor::relation operator()(::takatori::plan::exchange const& declaration);

    /// @copydoc table_column()
    ::takatori::descriptor::variable operator()(storage::column const& declaration);

    /// @copydoc external_variable()
    ::takatori::descriptor::variable operator()(variable::declaration const& declaration);

    /// @copydoc function(std::shared_ptr<function::declaration const>)
    ::takatori::descriptor::function operator()(std::shared_ptr<function::declaration const> declaration);

    /// @copydoc function(function::declaration&&)
    ::takatori::descriptor::function operator()(function::declaration&& declaration);

    /// @copydoc aggregate_function(std::shared_ptr<aggregate::declaration const>)
    ::takatori::descriptor::aggregate_function operator()(std::shared_ptr<aggregate::declaration const> declaration);

    /// @copydoc aggregate_function(aggregate::declaration&&)
    ::takatori::descriptor::aggregate_function operator()(aggregate::declaration&& declaration);

private:
    ::takatori::util::object_creator creator_;
};

} // namespace yugawara::binding

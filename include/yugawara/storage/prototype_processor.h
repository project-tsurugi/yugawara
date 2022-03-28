#pragma once

#include <functional>

#include <yugawara/compiler_code.h>
#include <yugawara/diagnostic.h>
#include <yugawara/schema/declaration.h>

#include "table.h"
#include "index.h"
#include "sequence.h"
#include "provider.h"

namespace yugawara::storage {

/**
 * @brief an abstract interface that handles storage element prototypes.
 * @see basic_prototype_processor
 */
class prototype_processor {
public:
    /// @brief diagnostic code type.
    using code_type = compiler_code;

    /// @brief diagnostic information type.
    using diagnostic_type = diagnostic<code_type>;

    /// @brief diagnostic consumer type.
    using diagnostic_consumer_type = std::function<void(diagnostic_type const&)>;

    /**
     * @brief creates a new instance.
     */
    prototype_processor() = default;

    /**
     * @brief destroys this object.
     */
    virtual ~prototype_processor() = default;

    prototype_processor(prototype_processor const& other) = delete;
    prototype_processor& operator=(prototype_processor const& other) = delete;
    prototype_processor(prototype_processor&& other) noexcept = delete;
    prototype_processor& operator=(prototype_processor&& other) noexcept = delete;

    /**
     * @brief processes a prototype of primary index and underlying table information.
     * @details Developers should redefine table and primary index information.
     * @param location the corresponded schema in compile-time
     * @param prototype the primary index prototype
     * @param diagnostic_consumer accepts diagnostic while processing the index
     * @return the processed index
     * @return empty if error was occurred
     */
    [[nodiscard]] virtual std::shared_ptr<index const> operator()(
            schema::declaration const& location,
            index const& prototype,
            diagnostic_consumer_type const& diagnostic_consumer) = 0;

    /**
     * @brief processes a prototype of index information and rebind it to the given table.
     * @details Developers should redefine index information.
     * @param location the corresponded schema in compile-time
     * @param prototype the index information
     * @param rebind_target the target table which the resulting index refers to
     * @param diagnostic_consumer accepts diagnostic while processing the index
     * @return the processed index
     * @return empty if error was occurred
     * @note the passed prototype must not be a primary index
     */
    [[nodiscard]] virtual std::shared_ptr<index const> operator()(
            schema::declaration const& location,
            index const& prototype,
            std::shared_ptr<table const> rebind_target,
            diagnostic_consumer_type const& diagnostic_consumer) = 0;
};

} // namespace yugawara::storage

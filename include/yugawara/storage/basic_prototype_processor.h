#pragma once

#include <yugawara/schema/declaration.h>

#include "table.h"

#include "prototype_processor.h"

namespace yugawara::storage {

/**
 * @brief basic implementation of prototype_processor.
 * @details
 *  This implementation just creates a copy of original storage elements,
 *  and NOT add them to any registries.
 *  Developers can inherit this class and edit the processed elements.
 */
class basic_prototype_processor : public prototype_processor {
public:
    /**
     * @brief creates a new instance.
     */
    basic_prototype_processor() = default;

    [[nodiscard]] std::shared_ptr<index const> operator()(
            schema::declaration const& location,
            index const& prototype,
            diagnostic_consumer_type const& diagnostic_consumer) override;

    [[nodiscard]] std::shared_ptr<index const> operator()(
            schema::declaration const& location,
            index const& prototype,
            std::shared_ptr<table const> rebind_target,
            diagnostic_consumer_type const& diagnostic_consumer) override;

protected:
    /**
     * @brief additionally processes the table and primary index information.
     * @details
     *  This is an extension point to complete table and primary index information.
     *  The original implementation does nothing and just return `true`.
     * @param location the corresponded schema in compile-time
     * @param table_prototype the processed table information
     * @param primary_index_prototype the processed primary index information
     * @param diagnostic_consumer accepts diagnostic while processing the index
     * @return true if there are no errors during this operation
     * @return false otherwise
     */
    virtual bool ensure(
            schema::declaration const& location,
            table& table_prototype,
            index& primary_index_prototype,
            diagnostic_consumer_type const& diagnostic_consumer);

    /**
     * @brief additionally processes the table and primary index information.
     * @details
     *  This is an extension point to complete table and primary index information.
     *  The original implementation does nothing and just return `true`.
     * @param location the corresponded schema in compile-time
     * @param secondary_index_prototype the processed index information
     * @param diagnostic_consumer accepts diagnostic while processing the index
     * @return true if there are no errors during this operation
     * @return false otherwise
     * @note the passed prototype must not be a primary index
     */
    virtual bool ensure(
            schema::declaration const& location,
            index& secondary_index_prototype,
            diagnostic_consumer_type const& diagnostic_consumer);
};

} // namespace yugawara::storage

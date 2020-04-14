#pragma once

#include <takatori/util/object_creator.h>

#include <yugawara/storage/provider.h>

namespace yugawara::analyzer::details {

/**
 * @brief settings for intermediate execution plan optimizer.
 */
class intermediate_plan_optimizer_options {
public:
    /**
     * @brief creates a new instance with default options.
     */
    intermediate_plan_optimizer_options() = default;

    /**
     * @brief creates a new instance with default options.
     * @param creator the object creator
     */
    explicit intermediate_plan_optimizer_options(::takatori::util::object_creator creator) noexcept;

    /**
     * @brief returns the storage provider.
     * @return the storage provider
     */
    [[nodiscard]] storage::provider const& storage_provider() const noexcept;

    /**
     * @brief sets the storage provider.
     * @details The provider object must not be disposed until this object was disposed.
     *      If the storage provider was invalidated, index selection becomes disabled.
     * @param provider the storage provider
     * @return this
     */
    intermediate_plan_optimizer_options& storage_provider(storage::provider const& provider) noexcept;

    /**
     * @brief returns the object creator.
     * @return the object creator
     */
    [[nodiscard]] ::takatori::util::object_creator get_object_creator() const noexcept;

private:
    ::takatori::util::object_creator creator_ {};
    storage::provider const* storage_provider_ {};
};

} // namespace yugawara::analyzer::details

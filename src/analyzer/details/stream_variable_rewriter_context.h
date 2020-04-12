#pragma once

#include <functional>
#include <unordered_map>

#include <takatori/descriptor/variable.h>
#include <takatori/scalar/expression.h>
#include <takatori/util/optional_ptr.h>

namespace yugawara::analyzer::details {

/**
 * @brief replaces stream variables in the scalar and relation expressions.
 */
class stream_variable_rewriter_context {
public:
    using consumer_type = std::function<void(::takatori::descriptor::variable const&)>;

    explicit stream_variable_rewriter_context(::takatori::util::object_creator creator) noexcept;

    ::takatori::util::optional_ptr<::takatori::descriptor::variable const> find(
            ::takatori::descriptor::variable const& variable) const;

    /**
     * @brief enumerates the used but not defined variables.
     * @param consumer the result callback, which will be passed all variables that is not defined (before rewrite)
     */
    void each_undefined(consumer_type const& consumer);

    /**
     * @brief rewrites the variable only if it was used.
     * @param variable the target stream variable
     * @return true if it was used, and then the variable would be modified
     * @return false otherwise, but the variable would not be modified
     */
    bool try_rewrite_define(::takatori::descriptor::variable& variable);

    /**
     * @brief rewrites the variable to be used.
     * @details This only rewrites the stream variables,
     *      and will keep the external and frame variables.
     * @param variable the target variables
     * @throws std::invalid_argument if the variable represents a table or exchange column
     */
    void rewrite_use(::takatori::descriptor::variable& variable);

    /**
     * @brief registers an variable alias.
     * @param source the source variable
     * @param alias the alias variable
     */
    void alias(::takatori::descriptor::variable const& source, ::takatori::descriptor::variable const& alias);

    void clear();

    ::takatori::util::object_creator get_object_creator() const noexcept {
        return mappings_.get_allocator().resource();
    }

private:
    enum class status_t {
        undefined,
        defined,
        alias,
    };

    struct entry {
        ::takatori::descriptor::variable variable;
        status_t status { status_t::undefined };
    };

    std::unordered_map<
            ::takatori::descriptor::variable,
            entry,
            std::hash<::takatori::descriptor::variable>,
            std::equal_to<>,
            ::takatori::util::object_allocator<std::pair<
                    ::takatori::descriptor::variable const,
                    entry>>> mappings_;
};

} // namespace yugawara::analyzer::details

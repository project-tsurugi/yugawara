#pragma once

#include <unordered_map>

#include <tsl/hopscotch_set.h>
#include <tsl/hopscotch_map.h>

#include <takatori/descriptor/variable.h>
#include <takatori/scalar/expression.h>
#include <takatori/scalar/comparison_operator.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/ownership_reference.h>

#include "search_key_term.h"

namespace yugawara::analyzer::details {

/**
 * @brief builds search_key_term from predicates.
 */
class search_key_term_builder {
public:
    /**
     * @brief reserve the key variables space.
     * @param count the number of key variables
     */
    void reserve_keys(std::size_t count);

    /**
     * @brief registers the key variable.
     * @param key the key variable
     */
    void add_key(::takatori::descriptor::variable key);

    /**
     * @brief registers a predicate.
     * @note You should register key variables before this operation.
     * @param predicate the predicate expression
     */
    void add_predicate(::takatori::util::ownership_reference<::takatori::scalar::expression> predicate);

    [[nodiscard]] bool empty() const noexcept;

    /**
     * @brief returns the search term for the given key variable.
     * @param key the target key variable
     * @return the corresponded search term
     */
    [[nodiscard]] ::takatori::util::optional_ptr<search_key_term> find(::takatori::descriptor::variable const& key);

    void clear();

    struct factor_info {
        ::takatori::scalar::comparison_operator operator_kind {};
        ::takatori::util::ownership_reference<::takatori::scalar::expression> term;
        ::takatori::util::ownership_reference<::takatori::scalar::expression> factor;
    };

    using term_map = ::tsl::hopscotch_map<
            ::takatori::descriptor::variable,
            search_key_term,
            std::hash<::takatori::descriptor::variable>,
            std::equal_to<>>;

    using variable_set = ::tsl::hopscotch_set<
            ::takatori::descriptor::variable,
            std::hash<::takatori::descriptor::variable>,
            std::equal_to<>>;

    using factor_info_map = std::unordered_multimap<
            ::takatori::descriptor::variable,
            factor_info,
            std::hash<::takatori::descriptor::variable>,
            std::equal_to<>>;

private:
    variable_set keys_;
    factor_info_map factors_;
    term_map terms_;

    void check_before_build();
    void build_terms();
    void build_term(factor_info_map::iterator begin, factor_info_map::iterator end);
};

} // namespace yugawara::analyzer::details

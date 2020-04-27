#pragma once

#include <optional>

#include <takatori/descriptor/variable.h>
#include <takatori/scalar/expression.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/ownership_reference.h>

#include <yugawara/storage/details/search_key_element.h>

namespace yugawara::analyzer::details {

/**
 * @brief a term of scan search key.
 */
class search_key_term {
public:
    using expression_ref = ::takatori::util::object_ownership_reference<::takatori::scalar::expression>;
    using index_search_key_type = storage::details::search_key_element;

    search_key_term(
            ::takatori::util::object_creator creator,
            expression_ref term,
            expression_ref factor) noexcept;

    search_key_term(
            ::takatori::util::object_creator creator,
            std::optional<expression_ref> lower_term,
            std::optional<expression_ref> lower_factor,
            bool lower_inclusive,
            std::optional<expression_ref> upper_term,
            std::optional<expression_ref> upper_factor,
            bool upper_inclusive) noexcept;

    [[nodiscard]] explicit operator bool() const noexcept;

    [[nodiscard]] bool equivalent() const noexcept;
    [[nodiscard]] bool full_bounded() const;
    [[nodiscard]] ::takatori::util::optional_ptr<::takatori::descriptor::variable const> equivalent_key() const;
    [[nodiscard]] ::takatori::util::optional_ptr<::takatori::scalar::expression const> equivalent_factor() const;
    [[nodiscard]] ::takatori::util::optional_ptr<::takatori::scalar::expression const> lower_factor() const;
    [[nodiscard]] bool lower_inclusive() const;
    [[nodiscard]] ::takatori::util::optional_ptr<::takatori::scalar::expression const> upper_factor() const;
    [[nodiscard]] bool upper_inclusive() const;

    [[nodiscard]] index_search_key_type build_index_search_key(storage::column const& column) const;

    [[nodiscard]] ::takatori::util::unique_object_ptr<::takatori::scalar::expression> purge_equivalent_factor();
    [[nodiscard]] ::takatori::util::unique_object_ptr<::takatori::scalar::expression> clone_equivalent_factor();
    [[nodiscard]] ::takatori::util::unique_object_ptr<::takatori::scalar::expression> purge_lower_factor();
    [[nodiscard]] ::takatori::util::unique_object_ptr<::takatori::scalar::expression> purge_upper_factor();

private:
    ::takatori::util::object_creator creator_;
    std::optional<expression_ref> equivalent_term_ {};
    std::optional<expression_ref> equivalent_factor_ {};
    std::optional<expression_ref> lower_term_ {};
    std::optional<expression_ref> lower_factor_ {};
    bool lower_inclusive_ {};
    std::optional<expression_ref> upper_term_ {};
    std::optional<expression_ref> upper_factor_ {};
    bool upper_inclusive_ {};

    ::takatori::util::unique_object_ptr<::takatori::scalar::expression> purge(
            std::optional<expression_ref>& term,
            std::optional<expression_ref>& factor);
};

} // namespace yugawara::analyzer::details

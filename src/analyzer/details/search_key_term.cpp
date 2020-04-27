#include "search_key_term.h"

#include <stdexcept>

#include <takatori/scalar/variable_reference.h>

#include <takatori/util/clonable.h>
#include <takatori/util/downcast.h>

#include "boolean_constants.h"

namespace yugawara::analyzer::details {

namespace scalar = ::takatori::scalar;

using ::takatori::util::clone_unique;
using ::takatori::util::object_creator;
using ::takatori::util::optional_ptr;
using ::takatori::util::unique_object_ptr;
using ::takatori::util::unsafe_downcast;

using expression_ref = ::takatori::util::object_ownership_reference<scalar::expression>;

search_key_term::search_key_term(
        object_creator creator,
        expression_ref term,
        expression_ref factor) noexcept
    : creator_(creator)
    , equivalent_term_(std::move(term))
    , equivalent_factor_(std::move(factor))
{}

search_key_term::search_key_term(
        object_creator creator,
        std::optional<expression_ref> lower_term,
        std::optional<expression_ref> lower_factor,
        bool lower_inclusive,
        std::optional<expression_ref> upper_term,
        std::optional<expression_ref> upper_factor,
        bool upper_inclusive) noexcept
    : creator_(creator)
    , lower_term_(std::move(lower_term))
    , lower_factor_(std::move(lower_factor))
    , lower_inclusive_(lower_inclusive)
    , upper_term_(std::move(upper_term))
    , upper_factor_(std::move(upper_factor))
    , upper_inclusive_(upper_inclusive)
{}

search_key_term::operator bool() const noexcept {
    return equivalent_factor_ || lower_factor_ || upper_factor_;
}

static optional_ptr<scalar::expression> get_if(std::optional<expression_ref> const& ref) {
    if (!ref) {
        return {};
    }
    return ref->get();
}

search_key_term::index_search_key_type search_key_term::build_index_search_key(storage::column const& column) const {
    if (equivalent()) {
        return {
                column,
                *equivalent_factor(),
        };
    }
    return {
            column,
            lower_factor(),
            lower_inclusive_,
            upper_factor(),
            upper_inclusive_,
    };
}

bool search_key_term::equivalent() const noexcept {
    return static_cast<bool>(equivalent_factor_);
}

bool search_key_term::full_bounded() const {
    return lower_factor_ && upper_factor_;
}

::takatori::util::optional_ptr<::takatori::descriptor::variable const> search_key_term::equivalent_key() const {
    if (auto factor = get_if(equivalent_factor_)) {
        if (factor->kind() == scalar::variable_reference::tag) {
            auto&& vref = unsafe_downcast<scalar::variable_reference>(*factor);
            return vref.variable();
        }
    }
    return {};
}

optional_ptr<scalar::expression const> search_key_term::equivalent_factor() const {
    return get_if(equivalent_factor_);
}

optional_ptr<scalar::expression const> search_key_term::lower_factor() const {
    return get_if(lower_factor_);
}

bool search_key_term::lower_inclusive() const {
    return lower_inclusive_;
}

optional_ptr<scalar::expression const> search_key_term::upper_factor() const {
    return get_if(upper_factor_);
}

bool search_key_term::upper_inclusive() const {
    return upper_inclusive_;
}

unique_object_ptr<scalar::expression> search_key_term::purge_equivalent_factor() {
    return purge(equivalent_term_, equivalent_factor_);
}

unique_object_ptr<scalar::expression> search_key_term::clone_equivalent_factor() {
    return clone_unique(equivalent_factor_->value(), creator_);
}

unique_object_ptr<scalar::expression> search_key_term::purge_lower_factor() {
    return purge(lower_term_, lower_factor_);
}

unique_object_ptr<scalar::expression> search_key_term::purge_upper_factor() {
    return purge(upper_term_, upper_factor_);
}

unique_object_ptr<scalar::expression> search_key_term::purge(
        std::optional<expression_ref>& term,
        std::optional<expression_ref>& factor) {
    if (term) {
        // FIXME: improve performance
        auto copy = clone_unique(std::move(factor->value()), creator_);
        *term = boolean_expression(true, creator_);
        return copy;
    }
    throw std::domain_error("cannot purge undefined term");
}

} // namespace yugawara::analyzer::details

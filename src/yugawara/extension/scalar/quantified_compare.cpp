#include <yugawara/extension/scalar/quantified_compare.h>

#include <takatori/util/clonable.h>
#include <takatori/util/downcast.h>

#include "../subquery_util.h"

namespace yugawara::extension::scalar {

using ::takatori::util::clone_unique;
using ::takatori::util::optional_ptr;
using ::takatori::util::unsafe_downcast;

quantified_compare::quantified_compare(
        operator_kind_type operator_kind,
        quantifier_type quantifier,
        std::unique_ptr<expression> left,
        graph_type query_graph,
        column_type right_column) noexcept:
    operator_kind_ { operator_kind },
    quantifier_ { quantifier },
    left_ { std::move(left) },
    query_graph_ { std::move(query_graph) },
    right_column_ { std::move(right_column) }
{}

quantified_compare::quantified_compare(
        operator_kind_type operator_kind,
        quantifier_type quantifier,
        expression&& left,
        graph_type query_graph,
        column_type right_column) :
    quantified_compare {
            operator_kind,
            quantifier,
            clone_unique(std::move(left)),
            std::move(query_graph),
            std::move(right_column),
    }
{}

quantified_compare::quantified_compare(takatori::util::clone_tag_t, quantified_compare const& other) :
    quantified_compare {
            other.operator_kind_,
            other.quantifier_,
            clone_unique(*other.left_),
            {},
            other.right_column_,
    }
{
    ::takatori::relation::merge_into(other.query_graph_, query_graph_);
}

quantified_compare::quantified_compare(takatori::util::clone_tag_t, quantified_compare&& other) :
    quantified_compare {
            other.operator_kind_,
            other.quantifier_,
            std::move(other.left_),
            std::move(other.query_graph_),
            std::move(other.right_column_),
    }
{}

quantified_compare* quantified_compare::clone() const & {
    return new quantified_compare(::takatori::util::clone_tag, *this); // NOLINT
}

quantified_compare* quantified_compare::clone() && {
    return new quantified_compare(::takatori::util::clone_tag, std::move(*this)); // NOLINT
}

takatori::scalar::extension::extension_id_type quantified_compare::extension_id() const noexcept {
    return extension_tag;
}

quantified_compare::operator_kind_type& quantified_compare::operator_kind() noexcept {
    return operator_kind_;
}

quantified_compare::operator_kind_type quantified_compare::operator_kind() const noexcept {
    return operator_kind_;
}

quantified_compare::quantifier_type& quantified_compare::quantifier() noexcept {
    return quantifier_;
}

quantified_compare::quantifier_type quantified_compare::quantifier() const noexcept {
    return quantifier_;
}

::takatori::scalar::expression& quantified_compare::left() noexcept {
    return *left_;
}

::takatori::scalar::expression const& quantified_compare::left() const noexcept {
    return *left_;
}

optional_ptr<::takatori::scalar::expression> quantified_compare::optional_left() noexcept {
    return ::takatori::util::optional_ptr { left_.get() };
}

optional_ptr<::takatori::scalar::expression const> quantified_compare::optional_left() const noexcept {
    return ::takatori::util::optional_ptr { left_.get() };
}

std::unique_ptr<::takatori::scalar::expression> quantified_compare::release_left() noexcept {
    auto result = std::move(left_);
    left_.reset();
    return result;
}

quantified_compare& quantified_compare::left(std::unique_ptr<expression> left) noexcept {
    left_ = std::move(left);
    return *this;
}

::takatori::util::ownership_reference<::takatori::scalar::expression> quantified_compare::ownership_left() {
    return ::takatori::util::ownership_reference { left_ };
}

quantified_compare::graph_type& quantified_compare::query_graph() noexcept {
    return query_graph_;
}

quantified_compare::graph_type const& quantified_compare::query_graph() const noexcept {
    return query_graph_;
}

quantified_compare::column_type& quantified_compare::right_column() noexcept {
    return right_column_;
}

quantified_compare::column_type const& quantified_compare::right_column() const noexcept {
    return right_column_;
}

optional_ptr<quantified_compare::output_port_type> quantified_compare::find_output_port() noexcept {
    return find_output_port_internal<output_port_type>(query_graph_);
}

optional_ptr<quantified_compare::output_port_type const> quantified_compare::find_output_port() const noexcept {
    return find_output_port_internal<output_port_type>(query_graph_);
}

bool operator==(quantified_compare const& a, quantified_compare const& b) noexcept {
    return std::addressof(a) == std::addressof(b);
}

bool operator!=(quantified_compare const& a, quantified_compare const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, quantified_compare const& value) {
    return out << to_string_view(static_cast<extension_id>(value.extension_id())) << "("
               << "operator_kind=" << to_string_view(value.operator_kind()) << ", "
               << "quantifier=" << to_string_view(value.quantifier()) << ", "
               << "left=" << *value.optional_left() << ", "
               << "#steps=" << value.query_graph().size() << ", "
               << "right_column=" << value.right_column() << ")";
}

bool quantified_compare::equals(extension const& other) const noexcept {
    if (this == std::addressof(other)) {
        return true;
    }
    return extension_tag == other.extension_id() && *this == unsafe_downcast<quantified_compare>(other);
}

std::ostream& quantified_compare::print_to(std::ostream& out) const {
    return out << *this;
}

} // namespace yugawara::extension::scalar

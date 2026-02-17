#include <yugawara/extension/relation/subquery.h>

#include <takatori/relation/graph.h>

#include <takatori/util/downcast.h>
#include <takatori/util/print_support.h>
#include <takatori/util/vector_print_support.h>

#include "../subquery_util.h"

namespace yugawara::extension::relation {

using ::takatori::util::optional_ptr;
using ::takatori::util::unsafe_downcast;

subquery::subquery(graph_type query_graph, std::vector<mapping_type> mappings, bool is_clone) noexcept :
    output_ { *this, 0 },
    query_graph_ { std::move(query_graph) },
    mappings_ { std::move(mappings) },
    is_clone_ { is_clone }
{}

subquery::subquery(takatori::util::clone_tag_t, subquery const& other) :
    subquery {
            {},
            other.mappings_,
            true,
    }
{
    ::takatori::relation::merge_into(other.query_graph_, query_graph_);
}

subquery::subquery(takatori::util::clone_tag_t, subquery&& other) noexcept :
    subquery {
            std::move(other.query_graph_),
            std::move(other.mappings_),
            other.is_clone_,
    }
{}

::takatori::util::sequence_view<::takatori::relation::expression::input_port_type> subquery::input_ports() noexcept {
    return {};
}

::takatori::util::sequence_view<::takatori::relation::expression::input_port_type const> subquery::input_ports() const noexcept {
    return {};
}

::takatori::util::sequence_view<::takatori::relation::expression::output_port_type> subquery::output_ports() noexcept {
    return ::takatori::util::sequence_view { std::addressof(output_) };
}

::takatori::util::sequence_view<::takatori::relation::expression::output_port_type const> subquery::output_ports() const noexcept {
    return ::takatori::util::sequence_view { std::addressof(output_) };
}

subquery* subquery::clone() const & {
    return new subquery(::takatori::util::clone_tag, *this); // NOLINT
}

subquery* subquery::clone() && {
    return new subquery(::takatori::util::clone_tag, std::move(*this)); // NOLINT
}

::takatori::relation::intermediate::extension::extension_id_type subquery::extension_id() const noexcept {
    return extension_tag;
}

::takatori::relation::expression::output_port_type& subquery::output() noexcept {
    return output_;
}

::takatori::relation::expression::output_port_type const& subquery::output() const noexcept {
    return output_;
}

takatori::relation::expression::graph_type& subquery::query_graph() noexcept {
    return query_graph_;
}

takatori::relation::expression::graph_type const& subquery::query_graph() const noexcept {
    return query_graph_;
}

std::vector<subquery::mapping_type>& subquery::mappings() noexcept {
    return mappings_;
}

std::vector<subquery::mapping_type> const& subquery::mappings() const noexcept {
    return mappings_;
}

optional_ptr<takatori::relation::expression::output_port_type> subquery::find_output_port() noexcept {
    return find_output_port_internal<output_port_type>(query_graph_);
}

optional_ptr<takatori::relation::expression::output_port_type const> subquery::find_output_port() const noexcept {
    return find_output_port_internal<output_port_type>(query_graph_);
}

bool& subquery::is_clone() noexcept {
    return is_clone_;
}

bool subquery::is_clone() const noexcept {
    return is_clone_;
}

bool operator==(subquery const& a, subquery const& b) noexcept {
    return std::addressof(a) == std::addressof(b);
}

bool operator!=(subquery const& a, subquery const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, subquery const& value) {
    return out << to_string_view(static_cast<extension_id>(value.extension_id())) << "("
               << "#steps=" << value.query_graph().size() << ", "
               << "mappings=" << ::takatori::util::print_support { value.mappings() } << ","
               << "is_clone=" << ::takatori::util::print_support { value.is_clone() } << ")";
}

bool subquery::equals(extension const& other) const noexcept {
    if (this == std::addressof(other)) {
        return true;
    }
    return extension_tag == other.extension_id() && *this == unsafe_downcast<subquery>(other);
}

std::ostream& subquery::print_to(std::ostream& out) const {
    return out << *this;
}

} // namespace yugawara::extension::relation

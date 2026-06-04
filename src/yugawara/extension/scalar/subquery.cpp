#include <yugawara/extension/scalar/subquery.h>

#include <takatori/util/downcast.h>
#include <takatori/util/vector_print_support.h>

#include "../subquery_util.h"

namespace yugawara::extension::scalar {

using ::takatori::util::optional_ptr;
using ::takatori::util::unsafe_downcast;

subquery::subquery(
        graph_type query_graph,
        std::vector<parameter_type> parameters,
        column_type output_column) noexcept :
    query_graph_ { std::move(query_graph) },
    parameters_ { std::move(parameters) },
    output_column_ { std::move(output_column) }
{}

subquery::subquery(graph_type query_graph, column_type output_column) noexcept :
    subquery {
            std::move(query_graph),
            {},
            std::move(output_column)
    }
{}

subquery::subquery(takatori::util::clone_tag_t, subquery const& other) :
    subquery {
            {},
            other.parameters_,
            other.output_column_,
    }
{
    ::takatori::relation::merge_into(other.query_graph_, query_graph_);
}

subquery::subquery(takatori::util::clone_tag_t, subquery&& other) :
    subquery {
            std::move(other.query_graph_),
            std::move(other.parameters_),
            std::move(other.output_column_),
    }
{}

subquery* subquery::clone() const & {
    return new subquery(::takatori::util::clone_tag, *this); // NOLINT
}

subquery* subquery::clone() && {
    return new subquery(::takatori::util::clone_tag, std::move(*this)); // NOLINT
}

::takatori::scalar::extension::extension_id_type subquery::extension_id() const noexcept {
    return extension_tag;
}

subquery::graph_type& subquery::query_graph() noexcept {
    return query_graph_;
}

subquery::graph_type const& subquery::query_graph() const noexcept {
    return query_graph_;
}

std::vector<subquery::parameter_type>& subquery::parameters() noexcept {
    return parameters_;
}

std::vector<subquery::parameter_type> const& subquery::parameters() const noexcept {
    return parameters_;
}

subquery::column_type& subquery::output_column() noexcept {
    return output_column_;
}

subquery::column_type const& subquery::output_column() const noexcept {
    return output_column_;
}

optional_ptr<subquery::output_port_type> subquery::find_output_port() noexcept {
    return find_output_port_internal<output_port_type>(query_graph_);
}

optional_ptr<subquery::output_port_type const> subquery::find_output_port() const noexcept {
    return find_output_port_internal<output_port_type>(query_graph_);
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
               << "parameters=" << ::takatori::util::print_support { value.parameters() } << ", "
               << "output_column=" << value.output_column() << ")";
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

} // namespace yugawara::extension::scalar

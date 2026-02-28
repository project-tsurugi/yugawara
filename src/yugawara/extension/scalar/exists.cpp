#include <yugawara/extension/scalar/exists.h>

#include <takatori/util/downcast.h>

#include "../subquery_util.h"

namespace yugawara::extension::scalar {

using ::takatori::util::optional_ptr;
using ::takatori::util::unsafe_downcast;

exists::exists(graph_type query_graph) noexcept :
    query_graph_ { std::move(query_graph) }
{}

exists::exists(takatori::util::clone_tag_t, exists const& other) :
    exists {
            {},
    }
{
    ::takatori::relation::merge_into(other.query_graph_, query_graph_);
}

exists::exists(takatori::util::clone_tag_t, exists&& other) :
    exists {
            std::move(other.query_graph_),
    }
{}

exists* exists::clone() const & {
    return new exists(::takatori::util::clone_tag, *this); // NOLINT
}

exists* exists::clone() && {
    return new exists(::takatori::util::clone_tag, std::move(*this)); // NOLINT
}

::takatori::scalar::extension::extension_id_type exists::extension_id() const noexcept {
    return extension_tag;
}

exists::graph_type& exists::query_graph() noexcept {
    return query_graph_;
}

exists::graph_type const& exists::query_graph() const noexcept {
    return query_graph_;
}

optional_ptr<exists::output_port_type> exists::find_output_port() noexcept {
    return find_output_port_internal<output_port_type>(query_graph_);
}

optional_ptr<exists::output_port_type const> exists::find_output_port() const noexcept {
    return find_output_port_internal<output_port_type>(query_graph_);
}

bool operator==(exists const& a, exists const& b) noexcept {
    return std::addressof(a) == std::addressof(b);
}

bool operator!=(exists const& a, exists const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, exists const& value) {
    return out << to_string_view(static_cast<extension_id>(value.extension_id())) << "("
               << "#steps=" << value.query_graph().size() << ")";
}

bool exists::equals(extension const& other) const noexcept {
    if (this == std::addressof(other)) {
        return true;
    }
    return extension_tag == other.extension_id() && *this == unsafe_downcast<exists>(other);
}

std::ostream& exists::print_to(std::ostream& out) const {
    return out << *this;
}

} // namespace yugawara::extension::scalar

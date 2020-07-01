#include <yugawara/schema/search_path.h>

namespace yugawara::schema {

search_path::search_path(vector_type elements) noexcept :
    elements_ { std::move(elements) }
{}

search_path::search_path(std::initializer_list<pointer> elements) :
    search_path {
            vector_type { elements },
    }
{}

search_path::vector_type& search_path::elements() noexcept {
    return elements_;
}

search_path::vector_type const& search_path::elements() const noexcept {
    return elements_;
}

bool operator==(search_path const& a, search_path const& b) noexcept {
    return a.elements() == b.elements();
}

bool operator!=(search_path const& a, search_path const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, search_path const& value) {
    out << "{";
    bool cont = false;
    for (auto&& p : value.elements()) {
        if (cont) {
            out << ", ";
        } else {
            out << " ";
            cont = true;
        }
        out << p->name();
    }
    if (cont) {
        out << " ";
    }
    out << "}";
    return out;
}

} // namespace yugawara::schema

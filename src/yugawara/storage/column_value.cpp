#include <yugawara/storage/column_value.h>

#include <takatori/util/clonable.h>
#include <takatori/util/print_support.h>

namespace yugawara::storage {

using ::takatori::util::clone_shared;

column_value::column_value(std::shared_ptr<::takatori::value::data const> element) noexcept :
    entity_ { std::move(element) }
{}

column_value::column_value(::takatori::util::rvalue_ptr<::takatori::value::data> element) :
    column_value {
        clone_shared(element),
    }
{}

column_value::column_value(std::shared_ptr<storage::sequence const> source) noexcept :
    entity_ { std::move(source) }
{}

namespace {

template<column_value::kind_type>
bool eq(column_value const& a, column_value const& b) = delete;

template<>
bool eq<column_value::kind_type::immediate>(column_value const& a, column_value const& b) {
    auto&& t1 = a.element<column_value::kind_type::immediate>();
    auto&& t2 = b.element<column_value::kind_type::immediate>();
    return *t1 == *t2;
}

template<>
bool eq<column_value::kind_type::sequence>(column_value const& a, column_value const& b) {
    auto&& t1 = a.element<column_value::kind_type::sequence>();
    auto&& t2 = b.element<column_value::kind_type::sequence>();
    return *t1 == *t2;
}

template<column_value::kind_type>
std::ostream& print(std::ostream& out, column_value const& value) {
    return out << "column_value("
               << "kind=" << value.kind() << ")";
}

template<>
std::ostream& print<column_value_kind::immediate>(std::ostream& out, column_value const& value) {
    using ::takatori::util::print_support;
    return out << "column_value("
               << "kind=" << value.kind() << ", "
               << "element=" << print_support { value.element<column_value_kind::immediate>() } << ")";
}

template<>
std::ostream& print<column_value_kind::sequence>(std::ostream& out, column_value const& value) {
    using ::takatori::util::print_support;
    return out << "column_value("
               << "kind=" << value.kind() << ", "
               << "element=" << print_support { value.element<column_value_kind::sequence>() } << ")";
}

} // namespace

bool operator==(column_value const& a, column_value const& b) {
    if (a.kind() != b.kind()) {
        return false;
    }
    using kind = column_value::kind_type;
    switch (a.kind()) {
        case kind::nothing: return true;
        case kind::immediate: return eq<kind::immediate>(a, b);
        case kind::sequence: return eq<kind::sequence>(a, b);
    }
    std::abort();
}

bool operator!=(column_value const& a, column_value const& b) {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, column_value const& value) {
    using kind = column_value::kind_type;
    switch (value.kind()) {
        case kind::nothing: return print<kind::nothing>(out, value);
        case kind::immediate: return print<kind::immediate>(out, value);
        case kind::sequence: return print<kind::sequence>(out, value);
    }
    std::abort();
}

} // namespace yugawara::storage

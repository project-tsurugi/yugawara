#include <yugawara/storage/details/search_key_element.h>

#include <takatori/util//print_support.h>

namespace yugawara::storage::details {

search_key_element::search_key_element(
        class column const& column,
        ::takatori::scalar::expression const& value) noexcept
    : column_(std::addressof(column))
    , equivalent_value_(value)
{}

search_key_element::search_key_element(
        class column const& column,
        ::takatori::util::optional_ptr<::takatori::scalar::expression const> lower_value,
        bool lower_inclusive,
        ::takatori::util::optional_ptr<::takatori::scalar::expression const> upper_value,
        bool upper_inclusive) noexcept
    : column_(std::addressof(column))
    , lower_value_(std::move(lower_value))
    , lower_inclusive_(lower_inclusive)
    , upper_value_(std::move(upper_value))
    , upper_inclusive_(upper_inclusive)
{}

class column const& search_key_element::column() const noexcept {
    return *column_;
}

bool search_key_element::equivalent() const {
    return equivalent_value_.has_value();
}

bool search_key_element::bounded() const {
    return lower_value_ && upper_value_;
}

::takatori::util::optional_ptr<::takatori::scalar::expression const> search_key_element::equivalent_value() const {
    return equivalent_value_;
}

::takatori::util::optional_ptr<::takatori::scalar::expression const> search_key_element::lower_value() const {
    return lower_value_;
}

bool search_key_element::lower_inclusive() const {
    return lower_inclusive_;
}

::takatori::util::optional_ptr<::takatori::scalar::expression const> search_key_element::upper_value() const {
    return upper_value_;
}

bool search_key_element::upper_inclusive() const {
    return upper_inclusive_;
}

std::ostream& operator<<(std::ostream& out, search_key_element const& value) {
    using ::takatori::util::print_support;
    if (value.equivalent()) {
        return out << "term("
                   << "column= " << value.column() << ", "
                   << "value=" << value.equivalent_value() << ")";
    }
    return out << "term("
               << "column= " << value.column() << ", "
               << "lower_value=" << value.lower_value() << ", "
               << "lower_inclusive=" << print_support { value.lower_inclusive() } << ", "
               << "upper_value=" << value.upper_value() << ", "
               << "upper_inclusive=" << print_support { value.upper_inclusive() } << ")";
}

} // namespace yugawara::storage::details

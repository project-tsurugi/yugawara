#include <yugawara/storage/details/index_key_element.h>

#include <takatori/util/optional_print_support.h>

namespace yugawara::storage::details {

std::ostream& operator<<(std::ostream& out, index_key_element const& value) {
    return out << "key" << "("
               << "column=" << value.column() << ", "
               << "direction=" << takatori::util::print_support { value.direction() } << ")";
}

} // namespace yugawara::storage::details

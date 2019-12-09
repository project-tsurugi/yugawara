#include "yugawara/storage/relation.h"

namespace yugawara::storage {

std::ostream& operator<<(std::ostream& out, relation const& value) {
    return value.print_to(out);
}

} // namespace yugawara::storage

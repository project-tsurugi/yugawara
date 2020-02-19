#include <yugawara/binding/relation_info.h>

#include <takatori/util/downcast.h>

namespace yugawara::binding {

bool operator==(relation_info const& a, relation_info const& b) noexcept {
    return a.equals(b);
}

bool operator!=(relation_info const& a, relation_info const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, relation_info const& value) {
    return value.print_to(out);
}

bool relation_info::equals(takatori::util::object const& other) const noexcept {
    if (auto* that = dynamic_cast<relation_info const*>(std::addressof(other)); that != nullptr) {
        return equals(*that);
    }
    return false;
}

relation_info& extract(relation_info::descriptor_type const& descriptor) {
    return *std::dynamic_pointer_cast<relation_info>(descriptor.shared_entity());
}

} // namespace yugawara::binding

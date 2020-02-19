#include <yugawara/binding/variable_info.h>

namespace yugawara::binding {

bool operator==(variable_info const& a, variable_info const& b) noexcept {
    return a.equals(b);
}

bool operator!=(variable_info const& a, variable_info const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, variable_info const& value) {
    return value.print_to(out);
}

bool variable_info::equals(takatori::util::object const& other) const noexcept {
    if (auto* that = dynamic_cast<variable_info const*>(std::addressof(other)); that != nullptr) {
        return equals(*that);
    }
    return false;
}

variable_info& extract(variable_info::descriptor_type const& descriptor) {
    return *std::dynamic_pointer_cast<variable_info>(descriptor.shared_entity());
}

} // namespace yugawara::binding

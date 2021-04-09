#include <yugawara/binding/variable_info.h>

#include <takatori/util/downcast.h>

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

variable_info::descriptor_type wrap(std::shared_ptr<variable_info> info) noexcept {
    return variable_info::descriptor_type { std::move(info) };
}

variable_info& unwrap(variable_info::descriptor_type const& descriptor) {
    return ::takatori::util::unsafe_downcast<variable_info>(descriptor.entity());
}

} // namespace yugawara::binding

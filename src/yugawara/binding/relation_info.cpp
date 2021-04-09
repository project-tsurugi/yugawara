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

relation_info::descriptor_type wrap(std::shared_ptr<relation_info> info) noexcept {
    return relation_info::descriptor_type { std::move(info) };
}

relation_info& unwrap(relation_info::descriptor_type const& descriptor) {
    return ::takatori::util::unsafe_downcast<relation_info>(descriptor.entity());
}

} // namespace yugawara::binding

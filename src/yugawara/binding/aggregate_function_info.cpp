#include <yugawara/binding/aggregate_function_info.h>

#include <takatori/util/downcast.h>
#include <takatori/util/hash.h>

namespace yugawara::binding {

aggregate_function_info::aggregate_function_info(declaration_pointer declaration) noexcept
    : declaration_(std::move(declaration))
{}

aggregate::declaration const& aggregate_function_info::declaration() const noexcept {
    return *declaration_;
}

aggregate_function_info& aggregate_function_info::declaration(declaration_pointer declaration) noexcept {
    declaration_ = std::move(declaration);
    return *this;
}

aggregate_function_info::declaration_pointer const& aggregate_function_info::shared_declaration() const noexcept {
    return declaration_;
}

bool operator==(aggregate_function_info const& a, aggregate_function_info const& b) noexcept {
    return a.declaration().definition_id() == b.declaration().definition_id();
}

bool operator!=(aggregate_function_info const& a, aggregate_function_info const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, aggregate_function_info const& value) {
    return out << value.declaration();
}

bool aggregate_function_info::equals(takatori::util::object const& other) const noexcept {
    if (this == std::addressof(other)) {
        return true;
    }
    if (auto* that = ::takatori::util::downcast<aggregate_function_info>(std::addressof(other)); that != nullptr) {
        return *this == *that;
    }
    return false;
}

std::size_t aggregate_function_info::hash() const noexcept {
    return takatori::util::hash(declaration_->definition_id(), descriptor_type::tag);
}

std::ostream& aggregate_function_info::print_to(std::ostream& out) const {
    return out << *this;
}

aggregate_function_info::descriptor_type wrap(std::shared_ptr<aggregate_function_info> info) noexcept {
    return aggregate_function_info::descriptor_type { std::move(info) };
}

aggregate_function_info& unwrap(aggregate_function_info::descriptor_type const& descriptor) {
    return ::takatori::util::unsafe_downcast<aggregate_function_info>(descriptor.entity());
}

} // namespace yugawara::binding

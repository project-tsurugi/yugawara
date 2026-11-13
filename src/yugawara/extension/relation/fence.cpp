#include <yugawara/extension/relation/fence.h>

#include <takatori/util/downcast.h>

namespace yugawara::extension::relation {

using ::takatori::util::unsafe_downcast;

fence::fence(location_id_type upstream, location_id_type downstream) noexcept :
    input_ { *this, 0 },
    output_ { *this, 0 },
    upstream_ { upstream },
    downstream_ { downstream }
{}

fence::fence(::takatori::util::clone_tag_t, fence const& other) noexcept :
    fence {
            other.upstream_,
            other.downstream_,
    }
{}

fence::fence(::takatori::util::clone_tag_t, fence&& other) noexcept :
    fence {
            other.upstream_,
            other.downstream_,
    }
{}

::takatori::util::sequence_view<::takatori::relation::expression::input_port_type> fence::input_ports() noexcept {
    return ::takatori::util::sequence_view { std::addressof(input_) };
}

::takatori::util::sequence_view<::takatori::relation::expression::input_port_type const> fence::input_ports() const noexcept {
    return ::takatori::util::sequence_view { std::addressof(input_) };
}

::takatori::util::sequence_view<::takatori::relation::expression::output_port_type> fence::output_ports() noexcept {
    return ::takatori::util::sequence_view { std::addressof(output_) };
}

::takatori::util::sequence_view<::takatori::relation::expression::output_port_type const> fence::output_ports() const noexcept {
    return ::takatori::util::sequence_view { std::addressof(output_) };
}

fence* fence::clone() const & {
    return new fence(::takatori::util::clone_tag, *this); // NOLINT
}

fence* fence::clone() && {
    return new fence(::takatori::util::clone_tag, std::move(*this)); // NOLINT
}

::takatori::relation::intermediate::extension::extension_id_type fence::extension_id() const noexcept {
    return extension_tag;
}

::takatori::relation::expression::input_port_type& fence::input() noexcept {
    return input_;
}

::takatori::relation::expression::input_port_type const& fence::input() const noexcept {
    return input_;
}

::takatori::relation::expression::output_port_type& fence::output() noexcept {
    return output_;
}

::takatori::relation::expression::output_port_type const& fence::output() const noexcept {
    return output_;
}

fence::location_id_type fence::upstream() const noexcept {
    return upstream_;
}

fence& fence::upstream(location_id_type location) noexcept {
    upstream_ = location;
    return *this;
}

fence::location_id_type fence::downstream() const noexcept {
    return downstream_;
}

fence& fence::downstream(location_id_type location) noexcept {
    downstream_ = location;
    return *this;
}

bool operator==(fence const& a, fence const& b) noexcept {
    return a.upstream_ == b.upstream_
        && a.downstream_ == b.downstream_;
}

bool operator!=(fence const& a, fence const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, fence const& value) {
    return out << to_string_view(static_cast<extension_id>(value.extension_id())) << "("
               << "upstream=" << value.upstream_ << ", "
               << "downstream=" << value.downstream_ << ")";
}

bool fence::equals(extension const& other) const noexcept {
    if (this == std::addressof(other)) {
        return true;
    }
    return extension_tag == other.extension_id() && *this == unsafe_downcast<fence>(other);
}

std::ostream& fence::print_to(std::ostream& out) const {
    return out << *this;
}

} // namespace yugawara::extension::relation

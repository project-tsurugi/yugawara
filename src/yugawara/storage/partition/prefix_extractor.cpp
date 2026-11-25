#include <yugawara/storage/partition/prefix_extractor.h>

#include <type_traits>

namespace yugawara::storage::partition {

namespace {

template<prefix_extractor_kind Kind>
bool equals(prefix_extractor::entity_type const& a, prefix_extractor::entity_type const& b) noexcept {
    constexpr auto index = static_cast<std::size_t>(Kind);
    auto const* x = std::get_if<index>(&a);
    auto const* y = std::get_if<index>(&b);
    if (x == nullptr || y == nullptr) {
        return x == y;
    }
    return *x == *y;
}

} // namespace

bool operator==(prefix_extractor const& a, prefix_extractor const& b) noexcept {
    if (a.kind() != b.kind()) {
        return false;
    }
    using kind = prefix_extractor::kind_type;
    switch (a.kind()) {
        case kind::nothing: return true;
        case kind::character_prefix: return equals<kind::character_prefix>(a.entity_, b.entity_);
        case kind::binary_prefix: return equals<kind::binary_prefix>(a.entity_, b.entity_);
        case kind::floor_division: return equals<kind::floor_division>(a.entity_, b.entity_);
        case kind::floor_fractional: return equals<kind::floor_fractional>(a.entity_, b.entity_);
        case kind::date_truncate: return equals<kind::date_truncate>(a.entity_, b.entity_);
        case kind::timestamp_truncate: return equals<kind::timestamp_truncate>(a.entity_, b.entity_);
    }
    return false;
}

bool operator!=(prefix_extractor const& a, prefix_extractor const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, prefix_extractor const& value) {
    out << "prefix_extractor("
        << "kind=" << value.kind();
    std::visit([&](auto const& element) {
        if constexpr (!std::is_same_v<std::decay_t<decltype(element)>, std::monostate>) {
            out << ", "
                << "element=" << element;
        }
    }, value.entity_);
    return out << ")";
}

} // namespace yugawara::storage::partition

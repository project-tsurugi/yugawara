#include <yugawara/storage/partition/prefix_extractor.h>

#include <type_traits>

#include <takatori/type/primitive.h>
#include <takatori/type/decimal.h>
#include <takatori/type/character.h>
#include <takatori/type/octet.h>
#include <takatori/type/date.h>
#include <takatori/type/time_point.h>
#include <takatori/type/extension.h>

#include <takatori/util/downcast.h>

#include <yugawara/type/conversion.h>
#include <yugawara/extension/type/error.h>
#include <yugawara/extension/type/pending.h>

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

namespace {

bool is_parameter_applicable(::takatori::type::data const& type, ::takatori::type::data const& parameter) noexcept {
    return ::yugawara::type::is_parameter_application_convertible(type, parameter) == util::ternary::yes;
}

bool is_stop_type(::takatori::type::data const& type) noexcept {
    if (type.kind() == ::takatori::type::extension::tag) {
        auto&& ext = ::takatori::util::unsafe_downcast<::takatori::type::extension>(type);
        return extension::type::error::is_instance(ext)
            || extension::type::pending::is_instance(ext);
    }
    return false;
}

} // namespace

bool is_applicable(prefix_extractor_kind kind, ::takatori::type::data const& source_type) noexcept {
    using k = prefix_extractor_kind;
    switch (kind) {
        case k::nothing:
            return true;
        case k::character_prefix:
            return is_parameter_applicable(source_type, ::takatori::type::character { ::takatori::type::varying });
        case k::binary_prefix:
            return is_parameter_applicable(source_type, ::takatori::type::octet { ::takatori::type::varying });
        case k::floor_division:
        case k::floor_fractional:
            return is_parameter_applicable(source_type, ::takatori::type::decimal {});
        case k::date_truncate:
            return is_parameter_applicable(source_type, ::takatori::type::date {});
        case k::timestamp_truncate:
            return is_parameter_applicable(source_type, ::takatori::type::time_point {});
    }
    return false;
}

std::shared_ptr<::takatori::type::data const> compute_type(
        prefix_extractor const& extractor,
        ::takatori::type::data const& source_type,
        ::yugawara::type::repository& repo) {
    using k = prefix_extractor_kind;
    using t = ::takatori::type::type_kind;
    if (is_stop_type(source_type)) {
        return repo.get(extension::type::pending {});
    }
    if (!is_applicable(extractor.kind(), source_type)) {
        return repo.get(extension::type::error {});
    }
    switch (extractor.kind()) {
        case k::nothing:
            return repo.get(source_type);
        case k::character_prefix:
            return repo.get(::takatori::type::character { ::takatori::type::varying });
        case k::binary_prefix:
            return repo.get(::takatori::type::octet { ::takatori::type::varying });
        case k::floor_division:
        case k::floor_fractional:
            switch (source_type.kind()) {
                case t::int1:
                case t::int2:
                case t::int4:
                    return repo.get(::takatori::type::int4 {});
                case t::int8:
                    return repo.get(::takatori::type::int8 {});
                case t::decimal:
                    return repo.get(::takatori::type::decimal {});
                default:
                    break;
            }
            break;
        case k::date_truncate:
            switch (extractor.element<k::date_truncate>().unit()) {
                case date_unit::year:
                    return repo.get(::takatori::type::int4 {});
                case date_unit::month:
                case date_unit::day:
                    return repo.get(::takatori::type::date {});
            }
            break;
        case k::timestamp_truncate:
            switch (extractor.element<k::timestamp_truncate>().unit()) {
                case timestamp_unit::year:
                    return repo.get(::takatori::type::int4 {});
                case timestamp_unit::month:
                case timestamp_unit::day:
                    return repo.get(::takatori::type::date {});
                case timestamp_unit::hour:
                case timestamp_unit::minute:
                case timestamp_unit::second:
                    return repo.get(::takatori::type::time_point {});
            }
            break;
    }
    return repo.get(extension::type::error {});
}

} // namespace yugawara::storage::partition

#pragma once

#include <cstddef>

#include <takatori/descriptor/variable.h>

#include <takatori/util/downcast.h>
#include <takatori/util/exception.h>
#include <takatori/util/hash.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/string_builder.h>

#include <yugawara/binding/variable_info.h>
#include <yugawara/binding/variable_info_kind.h>

namespace yugawara::binding {

static constexpr variable_info_kind_set variable_info_impl_supported {
        variable_info_kind::exchange_column,
        variable_info_kind::frame_variable,
        variable_info_kind::stream_variable,
        variable_info_kind::local_variable,
};

template<variable_info_kind Kind>
class variable_info_impl : public variable_info {

    static_assert(variable_info_impl_supported.contains(Kind));

public:
    static constexpr variable_info_kind tag = Kind;

    explicit variable_info_impl(std::string label = {}) noexcept
        : label_(std::move(label))
    {}

    [[nodiscard]] variable_info_kind kind() const noexcept override {
        return tag;
    }

    [[nodiscard]] std::string_view label() const noexcept {
        return label_;
    }

    friend constexpr bool operator==(variable_info_impl const& a, variable_info_impl const& b) noexcept {
        return std::addressof(a) == std::addressof(b);
    }

    friend constexpr bool operator!=(variable_info_impl const& a, variable_info_impl const& b) noexcept {
        return !(a == b);
    }

    friend std::ostream& operator<<(std::ostream& out, variable_info_impl const& value) {
        if (value.label().empty()) {
            return out << tag << "(" << std::addressof(value) << ")";
        }
        return out << tag << "(" << value.label() << ")";
    }

protected:
    [[nodiscard]] bool equals(variable_info const& other) const noexcept override {
        return other.kind() == tag && *this == takatori::util::unsafe_downcast<variable_info_impl>(other);
    }

    [[nodiscard]] std::size_t hash() const noexcept override {
        return takatori::util::hash(this);
    }

    std::ostream& print_to(std::ostream& out) const override {
        return out << *this;
    }

private:
    std::string label_;
};

template<variable_info_kind Kind>
[[nodiscard]] ::takatori::util::optional_ptr<variable_info_impl<Kind> const> extract_if(::takatori::descriptor::variable const& descriptor) {
    auto&& info = unwrap(descriptor);
    if (info.kind() == Kind) {
        using ::takatori::util::unsafe_downcast;
        return unsafe_downcast<variable_info_impl<Kind>>(info);
    }
    return {};
}

template<variable_info_kind Kind>
[[nodiscard]] variable_info_impl<Kind> const& extract(::takatori::descriptor::variable const& descriptor) {
    if (auto r = extract_if<Kind>(descriptor)) {
        return *r;
    }
    using ::takatori::util::string_builder;
    using ::takatori::util::throw_exception;
    throw_exception(std::invalid_argument(string_builder {}
            << "cannot extract from descriptor: "
            << "expected=" << Kind << ", "
            << "actual=" << unwrap(descriptor).kind()
            << string_builder::to_string));
}

} // namespace yugawara::binding

#pragma once

#include <cstddef>

#include <takatori/util/downcast.h>
#include <takatori/util/hash.h>

#include <yugawara/binding/variable_info.h>
#include <yugawara/binding/variable_info_kind.h>

namespace yugawara::binding {

template<variable_info_kind Kind>
class variable_info_impl : public variable_info {
public:
    static constexpr variable_info_kind tag = Kind;

    explicit variable_info_impl(std::string label = {}) noexcept
        : label_(std::move(label))
    {}

    [[nodiscard]] [[nodiscard]] variable_info_kind kind() const noexcept override {
        return tag;
    }

    [[nodiscard]] [[nodiscard]] std::string_view label() const noexcept {
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
    [[nodiscard]] [[nodiscard]] bool equals(variable_info const& other) const noexcept override {
        return other.kind() == tag && *this == takatori::util::unsafe_downcast<variable_info_impl>(other);
    }

    [[nodiscard]] [[nodiscard]] std::size_t hash() const noexcept override {
        return takatori::util::hash(this);
    }

    std::ostream& print_to(std::ostream& out) const override {
        return out << *this;
    }

private:
    std::string label_;
};

} // namespace yugawara::binding

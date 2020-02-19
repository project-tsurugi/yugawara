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

    constexpr explicit variable_info_impl(std::size_t id) noexcept
        : id_(id)
    {}

    variable_info_kind kind() const noexcept override {
        return tag;
    }

    friend constexpr bool operator==(variable_info_impl const& a, variable_info_impl const& b) noexcept {
        return a.id_ == b.id_;
    }

    friend constexpr bool operator!=(variable_info_impl const& a, variable_info_impl const& b) noexcept {
        return !(a == b);
    }

    friend std::ostream& operator<<(std::ostream& out, variable_info_impl const& value) {
        return out << tag << "(" << value.id_ << ")";
    }

protected:
    bool equals(variable_info const& other) const noexcept override {
        return other.kind() == tag && *this == takatori::util::unsafe_downcast<variable_info_impl>(other);
    }

    std::size_t hash() const noexcept override {
        return takatori::util::hash(tag, id_);
    }

    std::ostream& print_to(std::ostream& out) const override {
        return out << *this;
    }

private:
    std::size_t id_;
};

} // namespace yugawara::binding

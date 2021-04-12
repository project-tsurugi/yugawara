#pragma once

#include <utility>

#include <takatori/descriptor/descriptor_kind.h>
#include <takatori/util/detect.h>
#include <takatori/util/downcast.h>

namespace yugawara::binding {

namespace impl {

static constexpr std::size_t class_id_magic = 6'000'020'143'847ULL;

static constexpr std::size_t descriptor_kind_magnitude = 1'000'000;

template<class T>
using tagged_t = decltype(std::declval<T>().kind());

template<class T>
using is_tagged = ::takatori::util::is_detected<tagged_t, T>;

template<class T, bool = is_tagged<T>::value>
struct class_id;

template<class T>
struct class_id<T, false> {
    static constexpr std::size_t value = class_id_magic
            + static_cast<std::size_t>(T::descriptor_type::tag) * descriptor_kind_magnitude
            + 0;
};

template<class T>
struct class_id<T, true> {
    static constexpr std::size_t value = class_id_magic
            + static_cast<std::size_t>(T::descriptor_type::tag) * descriptor_kind_magnitude
            + static_cast<std::size_t>(T::tag)
            + 1;
};

} // namespace impl

template<class T>
static constexpr std::size_t class_id_delegate(T const& self) noexcept {
    (void) self;
    return impl::class_id<T>::value;
}

template<class T, class U>
static bool equals_delegate(T const& self, U const& other) noexcept {
    static_assert(!std::is_base_of_v<T, U>);
    static_assert(std::is_base_of_v<U, T>);
    if (std::addressof(self) == std::addressof(other)) {
        return true;
    }
    return self.class_id() == other.class_id()
        && self == ::takatori::util::unsafe_downcast<T>(other);
}

} // namespace yugawara::binding

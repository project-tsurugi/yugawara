#pragma once

#include <type_traits>

#include "predicate.h"
#include "predicate_kind.h"

#include "comparison.h"
#include "negation.h"
#include "quantification.h"

#include "takatori/util/callback.h"

namespace yugawara::variable {

/// @private
namespace impl {

/// @private
template<class Callback, class E, class... Args>
inline auto dispatch_predicate(Callback&& callback, E&& object, Args&&... args) {
    using ::takatori::util::polymorphic_callback;
    switch (object.kind()) {
        case comparison::tag: return polymorphic_callback<comparison>(std::forward<Callback>(callback), std::forward<E>(object), std::forward<Args>(args)...);
        case negation::tag: return polymorphic_callback<negation>(std::forward<Callback>(callback), std::forward<E>(object), std::forward<Args>(args)...);
        case quantification::tag: return polymorphic_callback<quantification>(std::forward<Callback>(callback), std::forward<E>(object), std::forward<Args>(args)...);
    }
    std::abort();
}

} // namespace impl

/**
 * @brief invoke callback function for individual subclasses of predicate.
 * @details if the input object has type of T, this may invoke Callback::operator()(T&, Args...).
 * @attention You must declare all callback functions for individual subclasses,
 * or declare `Callback::operator()(predicate&, Args...)` as "default" callback function.
 * Each return type of callback function must be identical.
 * @tparam Callback the callback object type
 * @tparam Args the callback argument types
 * @param callback the callback object
 * @param object the target predicate
 * @param args the callback arguments
 * @return the callback result
 */
template<class Callback, class... Args>
inline auto dispatch(Callback&& callback, predicate& object, Args&&... args) {
    return impl::dispatch_predicate(std::forward<Callback>(callback), object, std::forward<Args>(args)...);
}

/**
 * @brief invoke callback function for individual subclasses of predicate.
 * @details if the input object has type of T, this may invoke Callback::operator()(T&, Args...).
 * @attention You must declare all callback functions for individual subclasses,
 * or declare `Callback::operator()(predicate const&, Args...)` as "default" callback function.
 * Each return type of callback function must be identical.
 * @tparam Callback the callback object type
 * @tparam Args the callback argument types
 * @param callback the callback object
 * @param object the target predicate
 * @param args the callback arguments
 * @return the callback result
 */
template<class Callback, class... Args>
inline auto dispatch(Callback&& callback, predicate const& object, Args&&... args) {
    return impl::dispatch_predicate(std::forward<Callback>(callback), object, std::forward<Args>(args)...);
}

/**
 * @brief invoke callback function for individual subclasses of predicate.
 * @details if the input object has type of T, this may invoke Callback::operator()(T&&, Args...).
 * @attention You must declare all callback functions for individual subclasses,
 * or declare `Callback::operator()(predicate&&, Args...)` as "default" callback function.
 * Each return type of callback function must be identical.
 * @tparam Callback the callback object type
 * @tparam Args the callback argument types
 * @param callback the callback object
 * @param object the target predicate
 * @param args the callback arguments
 * @return the callback result
 */
template<class Callback, class... Args>
inline auto dispatch(Callback&& callback, predicate&& object, Args&&... args) {
    return impl::dispatch_predicate(std::forward<Callback>(callback), std::move(object), std::forward<Args>(args)...);
}

} // namespace yugawara::variable

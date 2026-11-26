#pragma once

#include <takatori/type/data.h>
#include <takatori/type/extension.h>

#include <takatori/util/downcast.h>

#include <yugawara/type/conversion.h>
#include <yugawara/extension/type/error.h>
#include <yugawara/extension/type/pending.h>

namespace yugawara::storage::partition::details {

/**
 * @brief returns whether or not the parameter application conversion from the given type to the parameter type is applicable.
 * @param type the source type
 * @param parameter the parameter type
 * @return true if it is applicable
 * @return false otherwise, or it is unknown
 */
inline bool is_parameter_applicable(::takatori::type::data const& type, ::takatori::type::data const& parameter) noexcept {
    return ::yugawara::type::is_parameter_application_convertible(type, parameter) == util::ternary::yes;
}

/**
 * @brief returns whether or not the given type is erroneous or pending.
 * @param type the target type
 * @return true if it is erroneous or pending
 * @return false otherwise
 */
inline bool is_stop_type(::takatori::type::data const& type) noexcept {
    if (type.kind() == ::takatori::type::extension::tag) {
        auto&& ext = ::takatori::util::unsafe_downcast<::takatori::type::extension>(type);
        return extension::type::error::is_instance(ext)
            || extension::type::pending::is_instance(ext);
    }
    return false;
}

} // namespace yugawara::storage::partition::details

#pragma once

#include <memory>

#include <takatori/type/data.h>

#include "repository.h"

#include <yugawara/util/ternary.h>

namespace yugawara::type {

/**
 * @brief converts to itself.
 * @param type the target type
 * @param repo the type repository
 * @return type itself if conversion was succeeded
 * @return erroneous type if the given type if unsupported in this system (may not occur)
 * @return pending type if the input contains erroneous or pending type
 * @note if the conversion was not success, this may return some special types generated out of the given repository
 */
std::shared_ptr<::takatori::type::data const> identity_conversion(
        ::takatori::type::data const& type,
        repository& repo = default_repository());

/**
 * @brief promotes the boolean type.
 * @param type the target type
 * @param repo the type repository
 * @return the promoted type if conversion was succeeded
 * @return erroneous type if the input is not valid for this conversion
 * @return pending type if the input contains erroneous or pending type
 * @note if the conversion was not success, this may return some special types generated out of the given repository
 */
std::shared_ptr<::takatori::type::data const> unary_boolean_promotion(
        ::takatori::type::data const& type,
        repository& repo = default_repository());

/**
 * @brief promotes the boolean type with the another type.
 * @param type the type to be promoted
 * @param with the opposite type
 * @param repo the type repository
 * @return the promoted type if conversion was succeeded
 * @return erroneous type if the input is not valid for this conversion
 * @return pending type if the input contains erroneous or pending type
 * @note if the conversion was not success, this may return some special types generated out of the given repository
 */
std::shared_ptr<::takatori::type::data const> binary_boolean_promotion(
        ::takatori::type::data const& type,
        ::takatori::type::data const& with,
        repository& repo = default_repository());

/**
 * @brief promotes the numeric type.
 * @param type the target type
 * @param repo the type repository
 * @return the promoted type if conversion was succeeded
 * @return erroneous type if the input is not valid for this conversion
 * @return pending type if the input contains erroneous or pending type
 * @note if the conversion was not success, this may return some special types generated out of the given repository
 */
std::shared_ptr<::takatori::type::data const> unary_numeric_promotion(
        ::takatori::type::data const& type,
        repository& repo = default_repository());

/**
 * @brief promotes an exact numeric type as a minimal decimal type which can represents the original number.
 * @param type the target type
 * @param repo the type repository
 * @return the promoted type if conversion was succeeded
 * @return erroneous type if the input is not valid for this conversion
 * @return pending type if the input contains erroneous or pending type
 * @note if the conversion was not success, this may return some special types generated out of the given repository
 */
std::shared_ptr<::takatori::type::data const> unary_decimal_promotion(
        ::takatori::type::data const& type,
        repository& repo = default_repository());

/**
 * @brief promotes the numeric type with the another type.
 * @param type the type to be promoted
 * @param with the opposite type
 * @param repo the type repository
 * @return the promoted type if conversion was succeeded
 * @return erroneous type if the input is not valid for this conversion
 * @return pending type if the input contains erroneous or pending type
 * @note if the conversion was not success, this may return some special types generated out of the given repository
 */
std::shared_ptr<::takatori::type::data const> binary_numeric_promotion(
        ::takatori::type::data const& type,
        ::takatori::type::data const& with,
        repository& repo = default_repository());

/**
 * @brief promotes the character string type.
 * @param type the target type
 * @param repo the type repository
 * @return the promoted type if conversion was succeeded
 * @return erroneous type if the input is not valid for this conversion
 * @return pending type if the input contains erroneous or pending type
 * @note if the conversion was not success, this may return some special types generated out of the given repository
 */
std::shared_ptr<::takatori::type::data const> unary_character_string_promotion(
        ::takatori::type::data const& type,
        repository& repo = default_repository());

/**
 * @brief promotes the character string type with the another type.
 * @param type the type to be promoted
 * @param with the opposite type
 * @param repo the type repository
 * @return the promoted type if conversion was succeeded
 * @return erroneous type if the input is not valid for this conversion
 * @return pending type if the input contains erroneous or pending type
 * @note if the conversion was not success, this may return some special types generated out of the given repository
 */
std::shared_ptr<::takatori::type::data const> binary_character_string_promotion(
        ::takatori::type::data const& type,
        ::takatori::type::data const& with,
        repository& repo = default_repository());

/**
 * @brief promotes the octet string type.
 * @param type the target type
 * @param repo the type repository
 * @return the promoted type if conversion was succeeded
 * @return erroneous type if the input is not valid for this conversion
 * @return pending type if the input contains erroneous or pending type
 * @note if the conversion was not success, this may return some special types generated out of the given repository
 */
std::shared_ptr<::takatori::type::data const> unary_octet_string_promotion(
        ::takatori::type::data const& type,
        repository& repo = default_repository());

/**
 * @brief promotes the octet string type with the another type.
 * @param type the type to be promoted
 * @param with the opposite type
 * @param repo the type repository
 * @return the promoted type if conversion was succeeded
 * @return erroneous type if the input is not valid for this conversion
 * @return pending type if the input contains erroneous or pending type
 * @note if the conversion was not success, this may return some special types generated out of the given repository
 */
std::shared_ptr<::takatori::type::data const> binary_octet_string_promotion(
        ::takatori::type::data const& type,
        ::takatori::type::data const& with,
        repository& repo = default_repository());

/**
 * @brief promotes the bit string type.
 * @param type the target type
 * @param repo the type repository
 * @return the promoted type if conversion was succeeded
 * @return erroneous type if the input is not valid for this conversion
 * @return pending type if the input contains erroneous or pending type
 * @note if the conversion was not success, this may return some special types generated out of the given repository
 */
std::shared_ptr<::takatori::type::data const> unary_bit_string_promotion(
        ::takatori::type::data const& type,
        repository& repo = default_repository());

/**
 * @brief promotes the bit string type with the another type.
 * @param type the type to be promoted
 * @param with the opposite type
 * @param repo the type repository
 * @return the promoted type if conversion was succeeded
 * @return erroneous type if the input is not valid for this conversion
 * @return pending type if the input contains erroneous or pending type
 * @note if the conversion was not success, this may return some special types generated out of the given repository
 */
std::shared_ptr<::takatori::type::data const> binary_bit_string_promotion(
        ::takatori::type::data const& type,
        ::takatori::type::data const& with,
        repository& repo = default_repository());

/**
 * @brief promotes the temporal type.
 * @param type the target type
 * @param repo the type repository
 * @return the promoted type if conversion was succeeded
 * @return erroneous type if the input is not valid for this conversion
 * @return pending type if the input contains erroneous or pending type
 * @note if the conversion was not success, this may return some special types generated out of the given repository
 */
std::shared_ptr<::takatori::type::data const> unary_temporal_promotion(
        ::takatori::type::data const& type,
        repository& repo = default_repository());

/**
 * @brief promotes the temporal type with the another type.
 * @param type the type to be promoted
 * @param with the opposite type
 * @param repo the type repository
 * @return the promoted type if conversion was succeeded
 * @return erroneous type if the input is not valid for this conversion
 * @return pending type if the input contains erroneous or pending type
 * @note if the conversion was not success, this may return some special types generated out of the given repository
 */
std::shared_ptr<::takatori::type::data const> binary_temporal_promotion(
        ::takatori::type::data const& type,
        ::takatori::type::data const& with,
        repository& repo = default_repository());

/**
 * @brief promotes the time-interval type.
 * @param type the target type
 * @param repo the type repository
 * @return the promoted type if conversion was succeeded
 * @return erroneous type if the input is not valid for this conversion
 * @return pending type if the input contains erroneous or pending type
 * @note if the conversion was not success, this may return some special types generated out of the given repository
 */
std::shared_ptr<::takatori::type::data const> unary_time_interval_promotion(
        ::takatori::type::data const& type,
        repository& repo = default_repository());

/**
 * @brief promotes the time_interval type with the another type.
 * @param type the type to be promoted
 * @param with the opposite type
 * @param repo the type repository
 * @return the promoted type if conversion was succeeded
 * @return erroneous type if the input is not valid for this conversion
 * @return pending type if the input contains erroneous or pending type
 * @note if the conversion was not success, this may return some special types generated out of the given repository
 */
std::shared_ptr<::takatori::type::data const> binary_time_interval_promotion(
        ::takatori::type::data const& type,
        ::takatori::type::data const& with,
        repository& repo = default_repository());

/**
 * @brief obtains the "common type" of the given type.
 * @param type the type to be unified
 * @param repo the type repository
 * @return the common type if conversion was succeeded
 * @return erroneous type if the input is not valid for this conversion
 * @return pending type if the input contains erroneous or pending type
 * @note if the conversion was not success, this may return some special types generated out of the given repository
 */
std::shared_ptr<::takatori::type::data const> unifying_conversion(
        ::takatori::type::data const& type,
        repository& repo = default_repository());

/**
 * @brief obtains the "common type" of the given types.
 * @param type the type to be unified
 * @param with the opposite type
 * @param repo the type repository
 * @return the common type if conversion was succeeded
 * @return erroneous type if the input is not valid for this conversion
 * @return pending type if the input contains erroneous or pending type
 * @note if the conversion was not success, this may return some special types generated out of the given repository
 */
std::shared_ptr<::takatori::type::data const> unifying_conversion(
        ::takatori::type::data const& type,
        ::takatori::type::data const& with,
        repository& repo = default_repository());

/// @private
namespace impl {

/// @private
bool is_conversion_stop_type(::takatori::type::data const& type) noexcept;

} // namespace impl

/**
 * @brief obtains the "common type" of the given types.
 * @tparam Iterator the iterator type
 * @tparam Sentinel the sentinel type
 * @param first the iterator of the types
 * @param last the sentinel of the above iterator
 * @param repo the type repository
 * @return the common type if conversion was succeeded
 * @return erroneous type if the input is not valid for this conversion
 * @return pending type if the input contains erroneous or pending type
 * @note if the conversion was not success, this may return some special types generated out of the given repository
 */
template<class Iterator, class Sentinel>
std::shared_ptr<::takatori::type::data const> unifying_conversion(
        Iterator first,
        Sentinel last,
        repository& repo = default_repository()) {
    if (first == last) { // empty
        return {};
    }
    auto it = first;
    ::takatori::type::data const& left = *it;
    ++it;
    if (it == last) {
        return unifying_conversion(left, repo);
    }
    auto result = unifying_conversion(left, *it, repo);
    ++it;
    for (; it != last; ++it) {
        if (impl::is_conversion_stop_type(*result)) return result;
        result = unifying_conversion(*result, *it, repo);
    }
    return result;
}

/**
 * @brief returns whether or not the "assignment conversion" is applicable to the given types.
 * @param type the type to be converted
 * @param target the conversion target type
 * @return yes if the conversion is applicable
 * @return no if the conversion is not applicable
 * @return unknown if the parameters contain an erroneous or pending type
 */
util::ternary is_assignment_convertible(::takatori::type::data const& type, ::takatori::type::data const& target) noexcept;

/**
 * @brief returns whether or not the "cast conversion" is applicable to the given types.
 * @param type the type to be converted
 * @param target the conversion target type
 * @return yes if the conversion is applicable
 * @return no if the conversion is not applicable
 * @return unknown if the parameters contain an erroneous or pending type
 */
util::ternary is_cast_convertible(::takatori::type::data const& type, ::takatori::type::data const& target) noexcept;

/**
 * @brief returns whether or not the "widening conversion" is applicable to the given types.
 * @param type the type to be converted
 * @param target the conversion target type
 * @return yes if the conversion is applicable
 * @return no if the conversion is not applicable
 * @return unknown if the parameters contain an erroneous or pending type
 */
util::ternary is_widening_convertible(::takatori::type::data const& type, ::takatori::type::data const& target) noexcept;

} // namespace yugawara::type

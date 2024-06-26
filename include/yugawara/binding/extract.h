#pragma once

#include <type_traits>

#include <takatori/descriptor/variable.h>
#include <takatori/descriptor/function.h>
#include <takatori/descriptor/aggregate_function.h>
#include <takatori/descriptor/declared_type.h>
#include <takatori/descriptor/relation.h>
#include <takatori/descriptor/schema.h>
#include <takatori/descriptor/storage.h>

#include <takatori/plan/exchange.h>

#include <takatori/util/maybe_shared_ptr.h>
#include <takatori/util/optional_ptr.h>

#include <yugawara/variable/declaration.h>
#include <yugawara/function/declaration.h>
#include <yugawara/aggregate/declaration.h>
#include <yugawara/schema/declaration.h>
#include <yugawara/storage/table.h>
#include <yugawara/storage/index.h>
#include <yugawara/storage/column.h>

#include "variable_info_kind.h"
#include "relation_info_kind.h"

namespace yugawara::binding {

/**
 * @brief returns the kind of descriptor entity.
 * @param descriptor the target descriptor
 * @return the kind of descriptor
 */
variable_info_kind kind_of(::takatori::descriptor::variable const& descriptor);

/**
 * @brief returns the kind of descriptor entity.
 * @param descriptor the target descriptor
 * @return the kind of descriptor
 */
relation_info_kind kind_of(::takatori::descriptor::relation const& descriptor);

/// @cond
namespace impl {

template<class Desc, class Entity, class = void>
struct descriptor_extract : std::false_type {};

template<>
struct descriptor_extract<::takatori::descriptor::variable, variable::declaration> : std::true_type {
    using descriptor_type = ::takatori::descriptor::variable;
    using entity_type = variable::declaration;
    using result_type = ::takatori::util::optional_ptr<entity_type const>;
    using pointer_type = std::shared_ptr<entity_type const>;
    [[nodiscard]] static result_type extract(descriptor_type const& desc, bool fail_if_mismatch);
    [[nodiscard]] static pointer_type extract_shared(descriptor_type const& desc, bool fail_if_mismatch);
};

template<>
struct descriptor_extract<::takatori::descriptor::variable, storage::column> : std::true_type {
    using descriptor_type = ::takatori::descriptor::variable;
    using entity_type = storage::column;
    using result_type = ::takatori::util::optional_ptr<entity_type const>;
    [[nodiscard]] static result_type extract(descriptor_type const& desc, bool fail_if_mismatch);
};

template<>
struct descriptor_extract<::takatori::descriptor::function, function::declaration> : std::true_type {
    using descriptor_type = ::takatori::descriptor::function;
    using entity_type = function::declaration;
    using result_type = ::takatori::util::optional_ptr<entity_type const>;
    using pointer_type = std::shared_ptr<entity_type const>;
    [[nodiscard]] static result_type extract(descriptor_type const& desc, bool fail_if_mismatch);
    [[nodiscard]] static pointer_type extract_shared(descriptor_type const& desc, bool fail_if_mismatch);
};

template<>
struct descriptor_extract<::takatori::descriptor::aggregate_function, aggregate::declaration> : std::true_type {
    using descriptor_type = ::takatori::descriptor::aggregate_function;
    using entity_type = aggregate::declaration;
    using result_type = ::takatori::util::optional_ptr<entity_type const>;
    using pointer_type = std::shared_ptr<entity_type const>;
    [[nodiscard]] static result_type extract(descriptor_type const& desc, bool fail_if_mismatch);
    [[nodiscard]] static pointer_type extract_shared(descriptor_type const& desc, bool fail_if_mismatch);
};

template<>
struct descriptor_extract<::takatori::descriptor::schema, schema::declaration> : std::true_type {
    using descriptor_type = ::takatori::descriptor::schema;
    using entity_type = schema::declaration;
    using result_type = ::takatori::util::optional_ptr<entity_type const>;
    using pointer_type = ::takatori::util::maybe_shared_ptr<entity_type const>;
    [[nodiscard]] static result_type extract(descriptor_type const& desc, bool fail_if_mismatch);
    [[nodiscard]] static pointer_type extract_shared(descriptor_type const& desc, bool fail_if_mismatch);
};

template<>
struct descriptor_extract<::takatori::descriptor::storage, storage::table> : std::true_type {
    using descriptor_type = ::takatori::descriptor::storage;
    using entity_type = storage::table;
    using result_type = ::takatori::util::optional_ptr<entity_type const>;
    using pointer_type = std::shared_ptr<entity_type const>;
    [[nodiscard]] static result_type extract(descriptor_type const& desc, bool fail_if_mismatch);
    [[nodiscard]] static pointer_type extract_shared(descriptor_type const& desc, bool fail_if_mismatch);
};

template<>
struct descriptor_extract<::takatori::descriptor::relation, storage::index> : std::true_type {
    using descriptor_type = ::takatori::descriptor::relation;
    using entity_type = storage::index;
    using result_type = ::takatori::util::optional_ptr<entity_type const>;
    using pointer_type = ::takatori::util::maybe_shared_ptr<entity_type const>;
    [[nodiscard]] static result_type extract(descriptor_type const& desc, bool fail_if_mismatch);
    [[nodiscard]] static pointer_type extract_shared(descriptor_type const& desc, bool fail_if_mismatch);
};

template<>
struct descriptor_extract<::takatori::descriptor::relation, ::takatori::plan::exchange> : std::true_type {
    using descriptor_type = ::takatori::descriptor::relation;
    using entity_type = ::takatori::plan::exchange;
    using result_type = ::takatori::util::optional_ptr<entity_type const>;
    [[nodiscard]] static result_type extract(descriptor_type const& desc, bool fail_if_mismatch);
};

} // namespace impl
/// @endcond

/**
 * @brief provides whether or not the descriptor can have the given type of entity.
 * @tparam Descriptor the descriptor type
 * @tparam Entity the entity type
 */
template<class Descriptor, class Entity>
inline constexpr bool descriptor_has_entity_v = impl::descriptor_extract<Descriptor, Entity>::value;

/**
 * @brief provides the shared entity type of the descriptor.
 * @tparam Descriptor the descriptor type
 * @tparam Entity the entity type
 */
template<class Descriptor, class Entity>
using descriptor_shared_type = typename impl::descriptor_extract<Descriptor, Entity>::pointer_type;

/**
 * @copydoc extract_if(::takatori::descriptor::function const& descriptor)
 * @see kind_of(::takatori::descriptor::variable const&)
 */
template<class T>
[[nodiscard]] std::enable_if_t<
        descriptor_has_entity_v<::takatori::descriptor::variable, T>,
        ::takatori::util::optional_ptr<T const>>
extract_if(::takatori::descriptor::variable const& descriptor) {
    using extractor = impl::descriptor_extract<::takatori::descriptor::variable, T>;
    return extractor::extract(descriptor, false);
}

/**
 * @brief extracts entity of the descriptor only if it is available.
 * @tparam T the entity type
 * @param descriptor the target descriptor
 * @return the corresponded entity
 * @return empty if the descriptor does not have such the entity type
 */
template<class T = function::declaration>
[[nodiscard]] std::enable_if_t<
        descriptor_has_entity_v<::takatori::descriptor::function, T>,
        ::takatori::util::optional_ptr<T const>>
extract_if(::takatori::descriptor::function const& descriptor) {
    using extractor = impl::descriptor_extract<::takatori::descriptor::function, T>;
    return extractor::extract(descriptor, false);
}

/// @copydoc extract_if(::takatori::descriptor::function const& descriptor)
template<class T = aggregate::declaration>
[[nodiscard]] std::enable_if_t<
        descriptor_has_entity_v<::takatori::descriptor::aggregate_function, T>,
        ::takatori::util::optional_ptr<T const>>
extract_if(::takatori::descriptor::aggregate_function const& descriptor) {
    using extractor = impl::descriptor_extract<::takatori::descriptor::aggregate_function, T>;
    return extractor::extract(descriptor, false);
}

/// @copydoc extract_if(::takatori::descriptor::function const& descriptor)
template<class T = schema::declaration>
[[nodiscard]] std::enable_if_t<
        descriptor_has_entity_v<::takatori::descriptor::schema, T>,
        ::takatori::util::optional_ptr<T const>>
extract_if(::takatori::descriptor::schema const& descriptor) {
    using extractor = impl::descriptor_extract<::takatori::descriptor::schema, T>;
    return extractor::extract(descriptor, false);
}

/// @copydoc extract_if(::takatori::descriptor::function const& descriptor)
template<class T = storage::table>
[[nodiscard]] std::enable_if_t<
        descriptor_has_entity_v<::takatori::descriptor::storage, T>,
        ::takatori::util::optional_ptr<T const>>
extract_if(::takatori::descriptor::storage const& descriptor) {
    using extractor = impl::descriptor_extract<::takatori::descriptor::storage, T>;
    return extractor::extract(descriptor, false);
}

/// @copydoc extract_if(::takatori::descriptor::function const& descriptor)
template<class T>
[[nodiscard]] std::enable_if_t<
        descriptor_has_entity_v<::takatori::descriptor::declared_type, T>,
        ::takatori::util::optional_ptr<T const>>
extract_if(::takatori::descriptor::declared_type const& descriptor) {
    using extractor = impl::descriptor_extract<::takatori::descriptor::declared_type, T>;
    return extractor::extract(descriptor, false);
}

/**
/// @copydoc extract_if(::takatori::descriptor::function const& descriptor)
 * @see kind_of(::takatori::descriptor::relation const&)
 */
template<class T>
[[nodiscard]] std::enable_if_t<
        descriptor_has_entity_v<::takatori::descriptor::relation, T>,
        ::takatori::util::optional_ptr<T const>>
extract_if(::takatori::descriptor::relation const& descriptor) {
    using extractor = impl::descriptor_extract<::takatori::descriptor::relation, T>;
    return extractor::extract(descriptor, false);
}

/**
 * @copydoc extract(::takatori::descriptor::function const& descriptor)
 * @see kind_of(::takatori::descriptor::variable const&)
 */
template<class T>
[[nodiscard]] std::enable_if_t<
        descriptor_has_entity_v<::takatori::descriptor::variable, T>,
        T const&>
extract(::takatori::descriptor::variable const& descriptor) {
    using extractor = impl::descriptor_extract<::takatori::descriptor::variable, T>;
    return *extractor::extract(descriptor, true);
}

/**
 * @brief extracts entity of the descriptor.
 * @tparam T the entity type
 * @param descriptor the target descriptor
 * @return the corresponded entity
 * @throws std::invalid_argument if the descriptor does not have such the entity type
 */
template<class T = function::declaration>
[[nodiscard]] std::enable_if_t<
        descriptor_has_entity_v<::takatori::descriptor::function, T>,
        T const&>
extract(::takatori::descriptor::function const& descriptor) {
    using extractor = impl::descriptor_extract<::takatori::descriptor::function, T>;
    return *extractor::extract(descriptor, true);
}

/// @copydoc extract(::takatori::descriptor::function const& descriptor)
template<class T = aggregate::declaration>
[[nodiscard]] std::enable_if_t<
        descriptor_has_entity_v<::takatori::descriptor::aggregate_function, T>,
        T const&>
extract(::takatori::descriptor::aggregate_function const& descriptor) {
    using extractor = impl::descriptor_extract<::takatori::descriptor::aggregate_function, T>;
    return *extractor::extract(descriptor, true);
}

/// @copydoc extract(::takatori::descriptor::function const& descriptor)
template<class T = schema::declaration>
[[nodiscard]] std::enable_if_t<
        descriptor_has_entity_v<::takatori::descriptor::schema, T>,
        T const&>
extract(::takatori::descriptor::schema const& descriptor) {
    using extractor = impl::descriptor_extract<::takatori::descriptor::schema, T>;
    return *extractor::extract(descriptor, true);
}

/// @copydoc extract(::takatori::descriptor::function const& descriptor)
template<class T = storage::table>
[[nodiscard]] std::enable_if_t<
        descriptor_has_entity_v<::takatori::descriptor::storage, T>,
        T const&>
extract(::takatori::descriptor::storage const& descriptor) {
    using extractor = impl::descriptor_extract<::takatori::descriptor::storage, T>;
    return *extractor::extract(descriptor, true);
}

/// @copydoc extract(::takatori::descriptor::function const& descriptor)
template<class T>
[[nodiscard]] std::enable_if_t<
        descriptor_has_entity_v<::takatori::descriptor::declared_type, T>,
        T const&>
extract(::takatori::descriptor::declared_type const& descriptor) {
    using extractor = impl::descriptor_extract<::takatori::descriptor::declared_type, T>;
    return *extractor::extract(descriptor, true);
}

/**
/// @copydoc extract(::takatori::descriptor::function const& descriptor)
 * @see kind_of(::takatori::descriptor::relation const&)
 */
template<class T>
[[nodiscard]] std::enable_if_t<
        descriptor_has_entity_v<::takatori::descriptor::relation, T>,
        T const&>
extract(::takatori::descriptor::relation const& descriptor) {
    using extractor = impl::descriptor_extract<::takatori::descriptor::relation, T>;
    return *extractor::extract(descriptor, true);
}

/**
 * @copydoc extract_shared(::takatori::descriptor::function const& descriptor)
 * @see kind_of(::takatori::descriptor::variable const&)
 */
template<class T>
[[nodiscard]] std::enable_if_t<
        descriptor_has_entity_v<::takatori::descriptor::variable, T>,
        descriptor_shared_type<::takatori::descriptor::variable, T>>
extract_shared(::takatori::descriptor::variable const& descriptor) {
    using extractor = impl::descriptor_extract<::takatori::descriptor::variable, T>;
    return extractor::extract_shared(descriptor, false);
}

/**
 * @brief extracts entity of the descriptor as its shared pointer.
 * @tparam T the entity type
 * @param descriptor the target descriptor
 * @return the corresponded entity
 * @return empty if the descriptor does not have such the entity type
 */
template<class T = function::declaration>
[[nodiscard]] std::enable_if_t<
        descriptor_has_entity_v<::takatori::descriptor::function, T>,
        descriptor_shared_type<::takatori::descriptor::function, T>>
extract_shared(::takatori::descriptor::function const& descriptor) {
    using extractor = impl::descriptor_extract<::takatori::descriptor::function, T>;
    return extractor::extract_shared(descriptor, false);
}

/// @copydoc extract_shared(::takatori::descriptor::function const& descriptor)
template<class T = aggregate::declaration>
[[nodiscard]] std::enable_if_t<
        descriptor_has_entity_v<::takatori::descriptor::aggregate_function, T>,
        descriptor_shared_type<::takatori::descriptor::aggregate_function, T>>
extract_shared(::takatori::descriptor::aggregate_function const& descriptor) {
    using extractor = impl::descriptor_extract<::takatori::descriptor::aggregate_function, T>;
    return extractor::extract_shared(descriptor, false);
}

/// @copydoc extract_shared(::takatori::descriptor::function const& descriptor)
template<class T = schema::declaration>
[[nodiscard]] std::enable_if_t<
        descriptor_has_entity_v<::takatori::descriptor::schema, T>,
        descriptor_shared_type<::takatori::descriptor::schema, T>>
extract_shared(::takatori::descriptor::schema const& descriptor) {
    using extractor = impl::descriptor_extract<::takatori::descriptor::schema, T>;
    return extractor::extract_shared(descriptor, false);
}

/// @copydoc extract_shared(::takatori::descriptor::function const& descriptor)
template<class T = storage::table>
[[nodiscard]] std::enable_if_t<
        descriptor_has_entity_v<::takatori::descriptor::storage, T>,
        descriptor_shared_type<::takatori::descriptor::storage, T>>
extract_shared(::takatori::descriptor::storage const& descriptor) {
    using extractor = impl::descriptor_extract<::takatori::descriptor::storage, T>;
    return extractor::extract_shared(descriptor, false);
}

/// @copydoc extract_shared(::takatori::descriptor::function const& descriptor)
template<class T>
[[nodiscard]] std::enable_if_t<
        descriptor_has_entity_v<::takatori::descriptor::declared_type, T>,
        descriptor_shared_type<::takatori::descriptor::declared_type, T>>
extract_shared(::takatori::descriptor::declared_type const& descriptor) {
    using extractor = impl::descriptor_extract<::takatori::descriptor::declared_type, T>;
    return extractor::extract_shared(descriptor, false);
}

/**
/// @copydoc extract_shared(::takatori::descriptor::function const& descriptor)
 * @see kind_of(::takatori::descriptor::relation const&)
 */
template<class T>
[[nodiscard]] std::enable_if_t<
        descriptor_has_entity_v<::takatori::descriptor::relation, T>,
        descriptor_shared_type<::takatori::descriptor::relation, T>>
extract_shared(::takatori::descriptor::relation const& descriptor) {
    using extractor = impl::descriptor_extract<::takatori::descriptor::relation, T>;
    return extractor::extract_shared(descriptor, false);
}

} // namespace yugawara::binding

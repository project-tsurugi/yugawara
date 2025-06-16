#include <yugawara/binding/extract.h>

#include <takatori/util/downcast.h>
#include <takatori/util/exception.h>
#include <takatori/util/string_builder.h>

#include <yugawara/binding/variable_info.h>
#include <yugawara/binding/function_info.h>
#include <yugawara/binding/aggregate_function_info.h>
#include <yugawara/binding/schema_info.h>
#include <yugawara/binding/table_info.h>
#include <yugawara/binding/relation_info.h>

#include <yugawara/binding/table_column_info.h>
#include <yugawara/binding/external_variable_info.h>

#include <yugawara/binding/index_info.h>
#include <yugawara/binding/exchange_info.h>

namespace yugawara::binding {

namespace descriptor = ::takatori::descriptor;

using ::takatori::util::optional_ptr;
using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;
using ::takatori::util::unsafe_downcast;

variable_info_kind kind_of(descriptor::variable const& descriptor) {
    return unwrap(descriptor).kind();
}

relation_info_kind kind_of(descriptor::relation const& descriptor) {
    return unwrap(descriptor).kind();
}

template<class Info, class Desc>
static optional_ptr<Info const> unwrap_as(Desc const& desc, bool fail_if_mismatch) {
    using info_type = Info;
    auto&& info = unwrap(desc);
    if (info.kind() == info_type::tag) {
        return unsafe_downcast<info_type>(info);
    }
    if (fail_if_mismatch) {
        throw_exception(std::invalid_argument(string_builder {}
                << "cannot extract from descriptor: "
                << "expected=" << info_type::tag << ", "
                << "actual=" << info.kind()
                << string_builder::to_string));
    }
    return {};
}

using external_variable_extract = impl::descriptor_extract<descriptor::variable, variable::declaration>;
external_variable_extract::result_type external_variable_extract::extract(descriptor_type const& desc, bool fail_if_mismatch) {
    if (auto info = unwrap_as<external_variable_info>(desc, fail_if_mismatch)) {
        return info->declaration();
    }
    return {};
}

external_variable_extract::pointer_type external_variable_extract::extract_shared(descriptor_type const& desc, bool fail_if_mismatch) {
    if (auto info = unwrap_as<external_variable_info>(desc, fail_if_mismatch)) {
        return info->shared_declaration();
    }
    return {};
}

using table_column_extract = impl::descriptor_extract<descriptor::variable, storage::column>;
table_column_extract::result_type table_column_extract::extract(descriptor_type const& desc, bool fail_if_mismatch) {
    if (auto info = unwrap_as<table_column_info>(desc, fail_if_mismatch)) {
        return info->column();
    }
    return {};
}

using function_extract = impl::descriptor_extract<descriptor::function, function::declaration>;
function_extract::result_type function_extract::extract(descriptor_type const& desc, bool) {
    return unwrap(desc).declaration();
}

function_extract::pointer_type function_extract::extract_shared(descriptor_type const& desc, bool) {
    return unwrap(desc).shared_declaration();
}

using aggregate_function_extract = impl::descriptor_extract<descriptor::aggregate_function, aggregate::declaration>;
aggregate_function_extract::result_type aggregate_function_extract::extract(descriptor_type const& desc, bool) {
    return unwrap(desc).declaration();
}

aggregate_function_extract::pointer_type aggregate_function_extract::extract_shared(descriptor_type const& desc, bool) {
    return unwrap(desc).shared_declaration();
}

using schema_extract = impl::descriptor_extract<descriptor::schema, schema::declaration>;
schema_extract::result_type schema_extract::extract(descriptor_type const& desc, bool) {
    return unwrap(desc).declaration();
}

schema_extract::pointer_type schema_extract::extract_shared(descriptor_type const& desc, bool) {
    return unwrap(desc).shared_declaration();
}

using storage_extract = impl::descriptor_extract<descriptor::storage, storage::table>;
storage_extract::result_type storage_extract::extract(descriptor_type const& desc, bool) {
    return unwrap(desc).declaration();
}

storage_extract::pointer_type storage_extract::extract_shared(descriptor_type const& desc, bool) {
    return unwrap(desc).shared_declaration();
}

using index_extract = impl::descriptor_extract<descriptor::relation, storage::index>;
index_extract::result_type index_extract::extract(descriptor_type const& desc, bool fail_if_mismatch) {
    if (auto info = unwrap_as<index_info>(desc, fail_if_mismatch)) {
        return info->declaration();
    }
    return {};
}

index_extract::pointer_type index_extract::extract_shared(descriptor_type const& desc, bool fail_if_mismatch) {
    if (auto info = unwrap_as<index_info>(desc, fail_if_mismatch)) {
        return info->maybe_shared_declaration();
    }
    return {};
}

using exchange_extract = impl::descriptor_extract<descriptor::relation, ::takatori::plan::exchange>;
exchange_extract::result_type exchange_extract::extract(descriptor_type const& desc, bool fail_if_mismatch) {
    if (auto info = unwrap_as<exchange_info>(desc, fail_if_mismatch)) {
        return info->declaration();
    }
    return {};
}

} // namespace yugawara::binding

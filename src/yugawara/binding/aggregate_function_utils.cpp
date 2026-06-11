#include "aggregate_function_utils.h"

#include <string_view>
#include <takatori/type/decimal.h>

#include <takatori/type/primitive.h>
#include <takatori/value/decimal.h>
#include <takatori/value/primitive.h>

#include <yugawara/aggregate/declaration.h>
#include <yugawara/binding/aggregate_function_info.h>

namespace yugawara::binding {

aggregate_function_default_kind default_kind_of(::takatori::descriptor::aggregate_function const& function) {
    using std::string_view_literals::operator ""sv;
    constexpr std::string_view count_name = "count"sv;
    constexpr std::string_view count_distinct_name { "count$distinct"sv };
    static_assert(count_distinct_name.size() == count_name.size() + aggregate::declaration::name_suffix_distinct.size());
    static_assert(count_distinct_name.find(count_name) == 0);
    static_assert(count_distinct_name.find(aggregate::declaration::name_suffix_distinct) == count_name.size());

    auto&& decl = unwrap(function).declaration();
    auto&& name = decl.name();

    // well known COUNT function will return zero.
    if (name == count_name || name == count_distinct_name) {
        auto params = decl.parameter_types().size();
        if (params != 0 && params != 1) {
            return aggregate_function_default_kind::unknown;
        }
        auto return_type = decl.return_type().kind();
        using type_kind = ::takatori::type::type_kind;
        if (return_type != type_kind::int4 && return_type != type_kind::int8 && return_type != type_kind::decimal) {
            return aggregate_function_default_kind::unknown;
        }
        return aggregate_function_default_kind::zero;
    }

    // other aggregate functions will return NULL to their default.
    return aggregate_function_default_kind::null;
}

std::unique_ptr<takatori::scalar::immediate> default_value_of(takatori::descriptor::aggregate_function const& function) {
    auto&& decl = unwrap(function).declaration();
    auto kind = default_kind_of(function);
    switch (kind) {
        case aggregate_function_default_kind::null:
            return std::make_unique<takatori::scalar::immediate>(
                    std::make_shared<::takatori::value::unknown>(),
                    decl.shared_return_type());
        case aggregate_function_default_kind::zero:
            switch (decl.return_type().kind()) {
                case ::takatori::type::int4::tag:
                    return std::make_unique<takatori::scalar::immediate>(
                            std::make_shared<::takatori::value::int4>(0),
                            decl.shared_return_type());
                case ::takatori::type::int8::tag:
                    return std::make_unique<takatori::scalar::immediate>(
                            std::make_shared<::takatori::value::int8>(0),
                            decl.shared_return_type());
                case ::takatori::type::decimal::tag:
                    return std::make_unique<takatori::scalar::immediate>(
                            std::make_shared<::takatori::value::decimal>(::takatori::decimal::triple { 0, 0 }),
                            decl.shared_return_type());
            default:
                return {};
            }
        default:
            return {};
    }
}

} // namespace yugawara::binding

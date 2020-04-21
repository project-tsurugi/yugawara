#pragma once

#include <memory>

#include <takatori/type/data.h>
#include <takatori/value/data.h>
#include <takatori/scalar/expression.h>

#include <takatori/util/object_creator.h>

namespace yugawara::analyzer::details {

std::shared_ptr<::takatori::type::data const> boolean_type();

std::shared_ptr<::takatori::value::data const> boolean_value(bool value);

::takatori::scalar::expression const& boolean_expression(bool value);

::takatori::util::unique_object_ptr<::takatori::scalar::expression> boolean_expression(
        bool value,
        ::takatori::util::object_creator creator);


} // namespace yugawara::analyzer::details
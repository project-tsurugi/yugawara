#pragma once

#include <memory>

#include <takatori/type/data.h>
#include <takatori/value/data.h>
#include <takatori/scalar/expression.h>

namespace yugawara::analyzer::details {

std::shared_ptr<::takatori::type::data const> boolean_type();

std::shared_ptr<::takatori::value::data const> boolean_value(bool value);

::takatori::scalar::expression const& boolean_expression(bool value);

std::unique_ptr<::takatori::scalar::expression> make_boolean_expression(bool value);


} // namespace yugawara::analyzer::details
#include "boolean_constants.h"

#include <takatori/type/primitive.h>
#include <takatori/value/primitive.h>
#include <takatori/scalar/immediate.h>

namespace yugawara::analyzer::details {

namespace type = ::takatori::type;
namespace value = ::takatori::value;
namespace scalar = ::takatori::scalar;

std::shared_ptr<type::data const> boolean_type() {
    static std::shared_ptr<type::data const> result
            = std::allocate_shared<type::boolean>(std::allocator<type::boolean>());
    return result;
}

std::shared_ptr<::takatori::value::data const> boolean_value(bool value) {
    if (value) {
        static std::shared_ptr<value::data const> const result
                = std::allocate_shared<value::boolean>(std::allocator<value::boolean>(), true);
        return result;
    }
    static std::shared_ptr<value::data const> const result
            = std::allocate_shared<value::boolean>(std::allocator<value::boolean>(), false);
    return result;
}

::takatori::scalar::expression const& boolean_expression(bool value) {
    if (value) {
        static scalar::immediate result {
                boolean_value(true),
                boolean_type(),
        };
        return result;
    }
    static scalar::immediate result {
            boolean_value(false),
            boolean_type(),
    };
    return result;
}

std::unique_ptr<scalar::expression> make_boolean_expression(bool value) {
    return std::make_unique<scalar::immediate>(boolean_value(value), boolean_type());
}

} // namespace yugawara::analyzer::details

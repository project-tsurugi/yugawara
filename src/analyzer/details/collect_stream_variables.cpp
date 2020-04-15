#include "collect_stream_variables.h"

#include <takatori/scalar/walk.h>

#include <yugawara/binding/variable_info.h>

namespace yugawara::analyzer::details {

namespace descriptor = ::takatori::descriptor;
namespace scalar = ::takatori::scalar;

namespace {

class engine {
public:
    explicit engine(variable_consumer const& consumer) noexcept
        : consumer_(consumer)
    {}

    constexpr void operator()(scalar::expression const&) noexcept {}

    void operator()(scalar::variable_reference const& expr) {
        accept(expr.variable());
    }

private:
    variable_consumer const& consumer_;

    void accept(descriptor::variable const& variable) {
        if (binding::unwrap(variable).kind() == binding::variable_info_kind::stream_variable) {
            consumer_(variable);
        }
    }
};

} // namespace

void collect_stream_variables(
        scalar::expression const& expression,
        variable_consumer const& consumer) {
    engine e { consumer };
    scalar::walk(e, expression);
}

} // namespace yugawara::analyzer::details

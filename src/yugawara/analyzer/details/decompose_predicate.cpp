#include "decompose_predicate.h"

#include <takatori/scalar/binary.h>
#include <takatori/util/downcast.h>

namespace yugawara::analyzer::details {

namespace scalar = ::takatori::scalar;
using ::takatori::util::ownership_reference;
using ::takatori::util::unsafe_downcast;

namespace {

class engine {
public:
    explicit engine(predicate_consumer const& consumer) noexcept
        : consumer_(consumer)
    {}

    bool consume(scalar::expression& expr) {
        // find (A AND B)
        if (expr.kind() != scalar::binary::tag) {
            return false;
        }

        auto&& binary = unsafe_downcast<scalar::binary>(expr);
        if (binary.operator_kind() != scalar::binary_operator::conditional_and) {
            return false;
        }

        // consume terms except (A AND B) AND ..., recursively
        if (!consume(binary.left())) {
            accept(binary.ownership_left());
        }
        if (!consume(binary.right())) {
            accept(binary.ownership_right());
        }
        return true;
    }

private:
    predicate_consumer const& consumer_;

    void accept(ownership_reference<scalar::expression>&& ownership) {
        consumer_(std::move(ownership));
    }
};

} // namespace

void decompose_predicate(scalar::expression& expression, predicate_consumer const& consumer) {
    engine e { consumer };
    e.consume(expression);
}

void decompose_predicate(ownership_reference<scalar::expression> expression, predicate_consumer const& consumer) {
    engine e { consumer };
    if (!e.consume(*expression)) {
        consumer(std::move(expression));
    }
}

} // namespace yugawara::analyzer::details

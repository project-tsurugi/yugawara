#pragma once

#include <takatori/serializer/object_acceptor.h>
#include <takatori/serializer/object_scanner.h>

#include <yugawara/variable/comparison.h>
#include <yugawara/variable/negation.h>
#include <yugawara/variable/quantification.h>

namespace yugawara::serializer::details {

class predicate_scanner {
public:
    explicit predicate_scanner(
            ::takatori::serializer::object_scanner const& scanner,
            ::takatori::serializer::object_acceptor& acceptor) noexcept;

    void scan(variable::predicate const& element);

    void operator()(variable::comparison const& element);
    void operator()(variable::negation const& element);
    void operator()(variable::quantification const& element);

private:
    ::takatori::serializer::object_scanner const& scanner_;
    ::takatori::serializer::object_acceptor& acceptor_;
};

} // namespace yugawara::serializer::details

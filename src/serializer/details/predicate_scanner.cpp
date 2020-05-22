#include "predicate_scanner.h"

#include <yugawara/variable/dispatch.h>

namespace yugawara::serializer::details {

using namespace std::string_view_literals;

predicate_scanner::predicate_scanner(
        takatori::serializer::object_scanner const& scanner,
        takatori::serializer::object_acceptor& acceptor) noexcept
    : scanner_(scanner)
    , acceptor_(acceptor)
{}

void predicate_scanner::scan(variable::predicate const& element) {
    acceptor_.struct_begin();

    acceptor_.property_begin("kind"sv);
    acceptor_.string(to_string_view(element.kind()));
    acceptor_.property_end();

    variable::dispatch(*this, element);

    acceptor_.struct_end();
}

void predicate_scanner::operator()(variable::comparison const& element) {
    acceptor_.property_begin("operator_kind"sv);
    acceptor_.string(to_string_view(element.operator_kind()));
    acceptor_.property_end();

    acceptor_.property_begin("value"sv);
    if (auto v = element.optional_value()) {
        scanner_(*v, acceptor_);
    }
    acceptor_.property_end();
}

void predicate_scanner::operator()(variable::negation const& element) {
    acceptor_.property_begin("operand"sv);
    if (auto v = element.optional_operand()) {
        scan(*v);
    }
    acceptor_.property_end();
}

void predicate_scanner::operator()(variable::quantification const& element) {
    acceptor_.property_begin("operator_kind"sv);
    acceptor_.string(to_string_view(element.operator_kind()));
    acceptor_.property_end();

    acceptor_.property_begin("operands"sv);
    acceptor_.array_begin();
    for (auto&& v : element.operands()) {
        scan(v);
    }
    acceptor_.array_end();
    acceptor_.property_end();
}

} // namespace yugawara::serializer::details

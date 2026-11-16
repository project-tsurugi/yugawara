#include <yugawara/remote/location_options.h>

namespace yugawara::remote {

using ::takatori::util::maybe_shared_ptr;

maybe_shared_ptr<symbol_location_query const>& location_options::symbol_location_query() noexcept {
    return symbol_location_query_;
}

maybe_shared_ptr<symbol_location_query const> const& location_options::symbol_location_query() const noexcept {
    return symbol_location_query_;
}

bool& location_options::allow_empty_operation_location() noexcept {
    return allow_empty_operation_location_;
}

bool location_options::allow_empty_operation_location() const noexcept {
    return allow_empty_operation_location_;
}

location_options::relation_expression_kind_set& location_options::remote_relation_expression_kinds() noexcept {
    return remote_relation_expression_kinds_;
}

location_options::relation_expression_kind_set const& location_options::remote_relation_expression_kinds() const noexcept {
    return remote_relation_expression_kinds_;
}

location_options::scalar_expression_kind_set& location_options::remote_scalar_expression_kinds() noexcept {
    return remote_scalar_expression_kinds_;
}

location_options::scalar_expression_kind_set const& location_options::remote_scalar_expression_kinds() const noexcept {
    return remote_scalar_expression_kinds_;
}

location_options::step_kind_set& location_options::remote_step_kinds() noexcept {
    return remote_step_kinds_;
}

location_options::step_kind_set const& location_options::remote_step_kinds() const noexcept {
    return remote_step_kinds_;
}

location_options::statement_kind_set& location_options::remote_statement_kinds() noexcept {
    return remote_statement_kinds_;
}

location_options::statement_kind_set const& location_options::remote_statement_kinds() const noexcept {
    return remote_statement_kinds_;
}

} // namespace yugawara::remote

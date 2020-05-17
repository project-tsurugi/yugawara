#include <yugawara/aggregate/declaration.h>

#include <takatori/util/vector_print_support.h>
#include <takatori/util/clonable.h>

#include "../function/utils.h"

namespace yugawara::aggregate {

declaration::declaration(
        definition_id_type definition_id,
        name_type name,
        quantifier_type quantifier,
        type_pointer return_type,
        std::vector<type_pointer, takatori::util::object_allocator<type_pointer>> parameter_types,
        bool incremental) noexcept
    : definition_id_(definition_id)
    , name_(std::move(name))
    , quantifier_(quantifier)
    , return_type_(std::move(return_type))
    , parameter_types_(std::move(parameter_types))
    , incremental_(incremental)
{}

declaration::declaration(
        definition_id_type definition_id,
        std::string_view name,
        quantifier_type quantifier,
        takatori::type::data&& return_type,
        std::initializer_list<takatori::util::rvalue_reference_wrapper<takatori::type::data>> parameter_types,
        bool incremental)
    : declaration(
            definition_id,
            decltype(name_) { name },
            quantifier,
            takatori::util::clone_shared(std::move(return_type)),
            decltype(parameter_types_) {},
            incremental)
{
    parameter_types_.reserve(parameter_types.size());
    for (auto&& rvref : parameter_types) {
        parameter_types_.emplace_back(takatori::util::clone_shared(rvref));
    }
}

declaration::definition_id_type declaration::definition_id() const noexcept {
    return definition_id_;
}

declaration& declaration::definition_id(declaration::definition_id_type definition_id) noexcept {
    definition_id_ = definition_id;
    return *this;
}

bool declaration::is_defined() const noexcept {
    return definition_id_ != unresolved_definition_id;
}

declaration::operator bool() const noexcept {
    return is_defined();
}

std::string_view declaration::name() const noexcept {
    return name_;
}

declaration& declaration::name(declaration::name_type name) noexcept {
    name_ = std::move(name);
    return *this;
}

declaration::quantifier_type declaration::quantifier() const noexcept {
    return quantifier_;
}

declaration& declaration::quantifier(quantifier_type quantifier) noexcept {
    quantifier_ = quantifier;
    return *this;
}

takatori::type::data const& declaration::return_type() const noexcept {
    return *return_type_;
}

::takatori::util::optional_ptr<takatori::type::data const> declaration::optional_return_type() const noexcept {
    return ::takatori::util::optional_ptr { return_type_.get() };
}

declaration::type_pointer const& declaration::shared_return_type() const noexcept {
    return return_type_;
}

declaration& declaration::return_type(declaration::type_pointer return_type) noexcept {
    return_type_ = std::move(return_type);
    return *this;
}

declaration::type_list_view declaration::parameter_types() const noexcept {
    return type_list_view { parameter_types_ };
}

std::vector<declaration::type_pointer, takatori::util::object_allocator<declaration::type_pointer>>& declaration::shared_parameter_types() noexcept {
    return parameter_types_;
}

std::vector<declaration::type_pointer, takatori::util::object_allocator<declaration::type_pointer>> const& declaration::shared_parameter_types() const noexcept {
    return parameter_types_;
}

bool declaration::incremental() const noexcept {
    return incremental_;
}

declaration& declaration::incremental(bool enabled) noexcept {
    incremental_ = enabled;
    return *this;
}

bool declaration::has_wider_parameters(declaration const& other) const noexcept {
    return function::utils::each_is_widening_convertible(parameter_types_, other.parameter_types_);
}

std::ostream& operator<<(std::ostream& out, declaration const& value) {
    return out << "aggregate_function("
               << "definition_id=" << value.definition_id() << ", "
               << "name=" << value.name() << ", "
               << "quantifier=" << value.quantifier() << ", "
               << "return_type=" << value.optional_return_type() << ", "
               << "parameter_types=" << takatori::util::print_support { value.shared_parameter_types() } << ", "
               << "incremental=" << takatori::util::print_support { value.incremental() } << ")";
}

} // namespace yugawara::aggregate

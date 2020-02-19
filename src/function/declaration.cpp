#include <yugawara/function/declaration.h>

#include <takatori/util/vector_print_support.h>
#include <takatori/util/clonable.h>

namespace yugawara::function {

declaration::declaration(
        definition_id_type definition_id,
        name_type name,
        type_pointer return_type,
        std::vector<type_pointer, takatori::util::object_allocator<type_pointer>> parameter_types) noexcept
    : definition_id_(definition_id)
    , name_(std::move(name))
    , return_type_(std::move(return_type))
    , parameter_types_(std::move(parameter_types))
{}

declaration::declaration(
        definition_id_type definition_id,
        std::string_view name,
        takatori::type::data&& return_type,
        std::initializer_list<takatori::util::rvalue_reference_wrapper<takatori::type::data>> parameter_types)
    : declaration(
            definition_id,
            decltype(name_) { name },
            takatori::util::clone_shared(std::move(return_type)),
            decltype(parameter_types_) {})
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

std::string_view declaration::name() const noexcept {
    return name_;
}

declaration& declaration::name(declaration::name_type name) noexcept {
    name_ = std::move(name);
    return *this;
}

takatori::type::data const& declaration::return_type() const noexcept {
    return *return_type_;
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

std::ostream& operator<<(std::ostream& out, declaration const& value) {
    return out << "function("
               << "definition_id=" << value.definition_id() << ", "
               << "name=" << value.name() << ", "
               << "return_type=" << takatori::util::print_support { value.shared_return_type() } << ", "
               << "parameter_types=" << takatori::util::print_support { value.shared_parameter_types() } << ")";
}

} // namespace yugawara::function

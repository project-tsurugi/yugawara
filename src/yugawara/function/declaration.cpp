#include <yugawara/function/declaration.h>

#include <takatori/util/vector_print_support.h>
#include <takatori/util/clonable.h>
#include <yugawara/schema/declaration.h>

namespace yugawara::function {

declaration::declaration(
        definition_id_type definition_id,
        name_type name,
        type_pointer return_type,
        std::vector<type_pointer> parameter_types,
        feature_set_type feature_set,
        description_type description) noexcept:
    definition_id_ { definition_id },
    name_ { std::move(name) },
    return_type_ { std::move(return_type) },
    parameter_types_ { std::move(parameter_types) },
    feature_set_ { feature_set },
    description_ { std::move(description) }
{}

declaration::declaration(
        definition_id_type definition_id,
        name_type name,
        type_pointer return_type,
        std::vector<type_pointer> parameter_types,
        description_type description) noexcept:
    declaration {
            definition_id,
            std::move(name),
            std::move(return_type),
            std::move(parameter_types),
            feature_set_type { function_feature::scalar_function },
            std::move(description),
    }
{}

declaration::declaration(
        definition_id_type definition_id,
        std::string_view name,
        takatori::type::data&& return_type,
        std::initializer_list<takatori::util::rvalue_reference_wrapper<takatori::type::data>> parameter_types,
        feature_set_type feature_set,
        description_type description):
    declaration {
            definition_id,
            decltype(name_) { name },
            takatori::util::clone_shared(std::move(return_type)),
            decltype(parameter_types_) {},
            feature_set,
            std::move(description),
    }
{
    parameter_types_.reserve(parameter_types.size());
    for (auto&& rvref : parameter_types) {
        parameter_types_.emplace_back(takatori::util::clone_shared(rvref));
    }
}

declaration::declaration(
        definition_id_type definition_id,
        std::string_view name,
        takatori::type::data&& return_type,
        std::initializer_list<takatori::util::rvalue_reference_wrapper<takatori::type::data>> parameter_types,
        description_type description):
    declaration {
            definition_id,
            name,
            std::move(return_type),
            parameter_types,
            feature_set_type { function_feature::scalar_function },
            std::move(description),
    }
{}

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

std::vector<declaration::type_pointer>& declaration::shared_parameter_types() noexcept {
    return parameter_types_;
}

std::vector<declaration::type_pointer> const& declaration::shared_parameter_types() const noexcept {
    return parameter_types_;
}

declaration::feature_set_type& declaration::features() noexcept {
    return feature_set_;
}

declaration::feature_set_type const& declaration::features() const noexcept {
    return feature_set_;
}

declaration::description_type const& declaration::description() const noexcept {
    return description_;
}

declaration& declaration::description(description_type description) noexcept {
    description_ = std::move(description);
    return *this;
}

std::ostream& operator<<(std::ostream& out, declaration const& value) {
    return out << "function("
               << "definition_id=" << value.definition_id() << ", "
               << "name=" << value.name() << ", "
               << "return_type=" << value.optional_return_type() << ", "
               << "parameter_types=" << takatori::util::print_support { value.shared_parameter_types() } << ", "
               << "features=" << value.features() << ", "
               << "description=" << value.description() << ")";
}

} // namespace yugawara::function

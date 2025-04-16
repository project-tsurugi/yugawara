#include <yugawara/schema/declaration.h>

#include <takatori/util/optional_print_support.h>

#include <yugawara/storage/null_provider.h>
#include <yugawara/variable/null_provider.h>
#include <yugawara/function/null_provider.h>
#include <yugawara/aggregate/null_provider.h>
#include <yugawara/type/null_provider.h>

namespace yugawara::schema {

using ::takatori::util::optional_ptr;

declaration::declaration(
        std::optional<definition_id_type> definition_id,
        name_type name,
        std::shared_ptr<storage::provider> storage_provider,
        std::shared_ptr<variable::provider> variable_provider,
        std::shared_ptr<function::provider> function_provider,
        std::shared_ptr<aggregate::provider> set_function_provider,
        std::shared_ptr<type::provider> type_provider,
        description_type description) noexcept :
    definition_id_ { definition_id },
    name_ { std::move(name) },
    storage_provider_ { std::move(storage_provider) },
    variable_provider_ { std::move(variable_provider) },
    function_provider_ { std::move(function_provider) },
    set_function_provider_ { std::move(set_function_provider) },
    type_provider_ { std::move(type_provider) },
    description_ { std::move(description) }
{}

declaration::declaration(
        std::string_view name,
        std::optional<definition_id_type> definition_id,
        std::shared_ptr<storage::provider> storage_provider,
        std::shared_ptr<variable::provider> variable_provider,
        std::shared_ptr<function::provider> function_provider,
        std::shared_ptr<aggregate::provider> set_function_provider,
        std::shared_ptr<type::provider> type_provider,
        description_type description) :
    declaration {
            definition_id,
            name_type { name },
            std::move(storage_provider),
            std::move(variable_provider),
            std::move(function_provider),
            std::move(set_function_provider),
            std::move(type_provider),
            std::move(description),
    }
{}

std::optional<declaration::definition_id_type> declaration::definition_id() const noexcept {
    return definition_id_;
}

declaration& declaration::definition_id(std::optional<definition_id_type> definition_id) noexcept {
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

storage::provider& declaration::storage_provider() const noexcept {
    if (auto* p = storage_provider_.get(); p != nullptr) {
        return *p;
    }
    static storage::null_provider empty;
    return empty;
}

std::shared_ptr<storage::provider> declaration::shared_storage_provider() const noexcept {
    return storage_provider_;
}

declaration& declaration::storage_provider(std::shared_ptr<storage::provider> provider) noexcept {
    storage_provider_ = std::move(provider);
    return *this;
}

variable::provider& declaration::variable_provider() const noexcept {
    if (auto* p = variable_provider_.get(); p != nullptr) {
        return *p;
    }
    static variable::null_provider empty;
    return empty;
}

std::shared_ptr<variable::provider> declaration::shared_variable_provider() const noexcept {
    return variable_provider_;
}

declaration& declaration::variable_provider(std::shared_ptr<variable::provider> provider) noexcept {
    variable_provider_ = std::move(provider);
    return *this;
}

function::provider& declaration::function_provider() const noexcept {
    if (auto* p = function_provider_.get(); p != nullptr) {
        return *p;
    }
    static function::null_provider empty;
    return empty;
}

std::shared_ptr<function::provider> declaration::shared_function_provider() const noexcept {
    return function_provider_;
}

declaration& declaration::function_provider(std::shared_ptr<function::provider> provider) noexcept {
    function_provider_ = std::move(provider);
    return *this;
}

aggregate::provider& declaration::set_function_provider() const noexcept {
    if (auto* p = set_function_provider_.get(); p != nullptr) {
        return *p;
    }
    static aggregate::null_provider empty;
    return empty;
}

std::shared_ptr<aggregate::provider> declaration::shared_set_function_provider() const noexcept {
    return set_function_provider_;
}

declaration& declaration::set_function_provider(std::shared_ptr<aggregate::provider> provider) noexcept {
    set_function_provider_ = std::move(provider);
    return *this;
}

type::provider& declaration::type_provider() const noexcept {
    if (auto* p = type_provider_.get(); p != nullptr) {
        return *p;
    }
    static type::null_provider empty;
    return empty;
}

std::shared_ptr<type::provider> declaration::shared_type_provider() const noexcept {
    return type_provider_;
}

declaration& declaration::type_provider(std::shared_ptr<type::provider> provider) noexcept {
    type_provider_ = std::move(provider);
    return *this;
}

declaration::description_type const& declaration::description() const noexcept {
    return description_;
}

declaration& declaration::description(description_type description) noexcept {
    description_ = std::move(description);
    return *this;
}

optional_ptr<provider const> declaration::owner() const noexcept {
    return optional_ptr { owner_ };
}

std::ostream& operator<<(std::ostream& out, declaration const& value) {
    using ::takatori::util::print_support;
    return out << "schema("
               << "definition_id=" << print_support { value.definition_id() } << ", "
               << "name=" << value.name() << ", "
               << "storage_provider=" << value.storage_provider_ << ", "
               << "variable_provider=" << value.variable_provider_ << ", "
               << "function_provider=" << value.function_provider_ << ", "
               << "set_function_provider=" << value.set_function_provider_
               << "type_provider=" << value.type_provider_ << ", "
               << "description=" << value.description() << ")";
}

} // namespace yugawara::schema

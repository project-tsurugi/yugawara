#include <yugawara/schema/declaration.h>

#include <takatori/util/optional_print_support.h>

#include <yugawara/storage/configurable_provider.h>
#include <yugawara/variable/configurable_provider.h>
#include <yugawara/function/configurable_provider.h>
#include <yugawara/aggregate/configurable_provider.h>
#include <yugawara/type/configurable_provider.h>

namespace yugawara::schema {

declaration::declaration(
        std::optional<definition_id_type> definition_id,
        name_type name,
        std::shared_ptr<storage::provider> storage_provider,
        std::shared_ptr<variable::provider> variable_provider,
        std::shared_ptr<function::provider> function_provider,
        std::shared_ptr<aggregate::provider> set_function_provider,
        std::shared_ptr<type::provider> type_provider) noexcept :
    definition_id_ { definition_id },
    name_ { std::move(name) },
    storage_provider_ { std::move(storage_provider) },
    variable_provider_ { std::move(variable_provider) },
    function_provider_ { std::move(function_provider) },
    set_function_provider_ { std::move(set_function_provider) },
    type_provider_ { std::move(type_provider) }
{}

declaration::declaration(
        std::string_view name,
        std::optional<definition_id_type> definition_id,
        std::shared_ptr<storage::provider> storage_provider,
        std::shared_ptr<variable::provider> variable_provider,
        std::shared_ptr<function::provider> function_provider,
        std::shared_ptr<aggregate::provider> set_function_provider,
        std::shared_ptr<type::provider> type_provider) :
    declaration {
            definition_id,
            name_type { name },
            std::move(storage_provider),
            std::move(variable_provider),
            std::move(function_provider),
            std::move(set_function_provider),
            std::move(type_provider),
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

storage::provider const& declaration::storage_provider() const noexcept {
    if (auto const* p = storage_provider_.get(); p != nullptr) {
        return *p;
    }
    static storage::configurable_provider const empty;
    return empty;
}

std::shared_ptr<storage::provider> declaration::shared_storage_provider() const noexcept {
    return storage_provider_;
}

declaration& declaration::storage_provider(std::shared_ptr<storage::provider> provider) noexcept {
    storage_provider_ = std::move(provider);
    return *this;
}

variable::provider const& declaration::variable_provider() const noexcept {
    if (auto const* p = variable_provider_.get(); p != nullptr) {
        return *p;
    }
    static variable::configurable_provider const empty;
    return empty;
}

std::shared_ptr<variable::provider> declaration::shared_variable_provider() const noexcept {
    return variable_provider_;
}

declaration& declaration::variable_provider(std::shared_ptr<variable::provider> provider) noexcept {
    variable_provider_ = std::move(provider);
    return *this;
}

function::provider const& declaration::function_provider() const noexcept {
    if (auto const* p = function_provider_.get(); p != nullptr) {
        return *p;
    }
    static function::configurable_provider const empty;
    return empty;
}

std::shared_ptr<function::provider> declaration::shared_function_provider() const noexcept {
    return function_provider_;
}

declaration& declaration::function_provider(std::shared_ptr<function::provider> provider) noexcept {
    function_provider_ = std::move(provider);
    return *this;
}

aggregate::provider const& declaration::set_function_provider() const noexcept {
    if (auto const* p = set_function_provider_.get(); p != nullptr) {
        return *p;
    }
    static aggregate::configurable_provider const empty;
    return empty;
}

std::shared_ptr<aggregate::provider> declaration::shared_set_function_provider() const noexcept {
    return set_function_provider_;
}

declaration& declaration::set_function_provider(std::shared_ptr<aggregate::provider> provider) noexcept {
    set_function_provider_ = std::move(provider);
    return *this;
}

type::provider const& declaration::type_provider() const noexcept {
    if (auto const* p = type_provider_.get(); p != nullptr) {
        return *p;
    }
    static type::configurable_provider const empty;
    return empty;
}

std::shared_ptr<type::provider> declaration::shared_type_provider() const noexcept {
    return type_provider_;
}

declaration& declaration::type_provider(std::shared_ptr<type::provider> provider) noexcept {
    type_provider_ = std::move(provider);
    return *this;
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
               << "type_provider=" << value.type_provider_ << ")";
}

} // namespace yugawara::schema

#include <yugawara/schema/catalog.h>

#include <takatori/util/optional_print_support.h>

#include <yugawara/schema/configurable_provider.h>

namespace yugawara::schema {

catalog::catalog(
        std::optional<definition_id_type> definition_id,
        name_type name,
        std::shared_ptr<provider> schema_provider) noexcept :
    definition_id_ { definition_id },
    name_ { std::move(name) },
    schema_provider_ { std::move(schema_provider) }
{}

catalog::catalog(
        std::string_view name,
        std::optional<definition_id_type> definition_id,
        std::shared_ptr<provider> schema_provider) :
    catalog {
            definition_id,
            name_type { name },
            std::move(schema_provider),
    }
{}

std::optional<catalog::definition_id_type> catalog::definition_id() const noexcept {
    return *definition_id_;
}

catalog& catalog::definition_id(std::optional<definition_id_type> definition_id) noexcept {
    definition_id_ = definition_id;
    return *this;
}

std::string_view catalog::name() const noexcept {
    return name_;
}

catalog& catalog::name(catalog::name_type name) noexcept {
    name_ = std::move(name);
    return *this;
}

provider const& catalog::schema_provider() const noexcept {
    if (schema_provider_) {
        return *schema_provider_;
    }
    static configurable_provider const empty;
    return empty;
}

std::shared_ptr<schema::provider> catalog::shared_schema_provider() const noexcept {
    return schema_provider_;
}

catalog& catalog::schema_provider(std::shared_ptr<schema::provider> provider) noexcept {
    schema_provider_ = std::move(provider);
    return *this;
}

std::ostream& operator<<(std::ostream& out, catalog const& value) {
    using ::takatori::util::print_support;
    return out << "catalog("
               << "definition_id=" << print_support { value.definition_id() } << ", "
               << "name=" << value.name() << ", "
               << "schema_provider=" << value.schema_provider_ << ")";
}

} // namespace yugawara::schema

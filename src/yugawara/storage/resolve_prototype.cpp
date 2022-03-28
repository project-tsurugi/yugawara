#include "resolve_prototype.h"

#include <takatori/statement/dispatch.h>

#include <yugawara/binding/extract.h>
#include <yugawara/binding/factory.h>

namespace yugawara::storage {

namespace {

class engine {
public:
    using diagnostic_type = prototype_processor::diagnostic_type;

    explicit engine(prototype_processor& processor) noexcept :
        processor_ { processor }
    {}

    void operator()(::takatori::statement::create_table& stmt) {
        auto&& schema = binding::extract(stmt.schema());
        std::shared_ptr<table const> rebind;
        if (auto&& index_proto = binding::extract_if<storage::index>(stmt.primary_key())) {
            auto result = processor_(
                    schema,
                    *index_proto,
                    [this](auto& diagnostic) { diagnostics_.push_back(diagnostic); });
            if (result) {
                rebind = result->shared_table();
                stmt.definition() = merge_region(stmt.definition(), result->shared_table());
                stmt.primary_key() = merge_region(stmt.primary_key(), std::move(result));
            }
        } else {
            // FIXME: error
        }

        if (!rebind) {
            return;
        }
        for (auto&& unique_key : stmt.unique_keys()) {
            if (auto&& index_proto = binding::extract_if<storage::index>(unique_key)) {
                auto result = processor_(
                        schema,
                        *index_proto,
                        rebind,
                        [this](auto& diagnostic) { diagnostics_.push_back(diagnostic); });
                if (result) {
                    unique_key = merge_region(unique_key, std::move(result));
                }
            } else {
                // FIXME: error
            }
        }
    }

    void operator()(::takatori::statement::create_index& stmt) {
        auto&& schema = binding::extract(stmt.schema());
        if (auto&& index_proto = binding::extract_if<storage::index>(stmt.definition())) {
            auto result = processor_(
                    schema,
                    *index_proto,
                    index_proto->shared_table(),
                    [this](auto& diagnostic) { diagnostics_.push_back(diagnostic); });
            if (result) {
                stmt.definition() = merge_region(stmt.definition(), std::move(result));
            }
        } else {
            // FIXME: error
        }
    }

    constexpr void operator()(::takatori::statement::statement const&) noexcept {
        // do nothing
    }

    [[nodiscard]] std::vector<diagnostic_type> take_diagnostics() noexcept {
        return std::move(diagnostics_);
    }

private:
    prototype_processor& processor_;
    std::vector<diagnostic_type> diagnostics_ {};

    template<class T, class U>
    static T merge_region(T const& old, U&& replacement) {
        binding::factory factory {};
        T result = factory(std::forward<U>(replacement));
        result.region() = old.region();
        return result;
    }
};

} // namespace

std::vector<prototype_processor::diagnostic_type> resolve_prototype(
        ::takatori::statement::statement& source,
        prototype_processor& storage_processor) {
    engine e { storage_processor };
    takatori::statement::dispatch(e, source);
    return e.take_diagnostics();
}

std::vector<prototype_processor::diagnostic_type> resolve_create_table(
        ::takatori::statement::create_table& source,
        prototype_processor& storage_processor) {
    engine e { storage_processor };
    e(source);
    return e.take_diagnostics();
}

std::vector<prototype_processor::diagnostic_type> resolve_create_index(
        ::takatori::statement::create_index& source,
        prototype_processor& storage_processor) {
    engine e { storage_processor };
    e(source);
    return e.take_diagnostics();
}
} // namespace yugawara::storage
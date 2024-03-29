#include <yugawara/analyzer/details/step_plan_builder_options.h>

#include <takatori/util/exception.h>
#include <takatori/util/string_builder.h>

namespace yugawara::analyzer::details {

using ::takatori::util::throw_exception;

runtime_feature_set& step_plan_builder_options::runtime_features() noexcept {
    return runtime_features_;
}

runtime_feature_set const& step_plan_builder_options::runtime_features() const noexcept {
    return runtime_features_;
}

template<class T>
[[noreturn]] static void raise_duplicate(T const& expr) {
    using ::takatori::util::string_builder;
    throw_exception(std::invalid_argument(string_builder {}
            << "the operation is already registered: "
            << expr
            << string_builder::to_string));
}

void step_plan_builder_options::add(::takatori::relation::intermediate::join const& expr, join_info info) {
    if (auto [it, success] = join_hints_.emplace(std::addressof(expr), info); !success) {
        (void) it;
        raise_duplicate(expr);
    }
}

::takatori::util::optional_ptr<join_info const> step_plan_builder_options::find(
        ::takatori::relation::intermediate::join const& expr) const {
    if (auto it = join_hints_.find(std::addressof(expr)); it != join_hints_.end()) {
        return it->second;
    }
    return {};
}

void step_plan_builder_options::add(::takatori::relation::intermediate::aggregate const& expr, aggregate_info info) {
    if (auto [it, success] = aggregate_hints_.emplace(std::addressof(expr), std::move(info)); !success) { // NOLINT
        (void) it;
        raise_duplicate(expr);
    }
}

::takatori::util::optional_ptr<aggregate_info const> step_plan_builder_options::find(
        ::takatori::relation::intermediate::aggregate const& expr) const {
    if (auto it = aggregate_hints_.find(std::addressof(expr)); it != aggregate_hints_.end()) {
        return it->second;
    }
    return {};
}

} // namespace yugawara::analyzer::details

#include "rewrite_correlated_subquery.h"

#include <algorithm>
#include <array>
#include <deque>
#include <functional>
#include <optional>

#include <tsl/hopscotch_map.h>
#include <tsl/hopscotch_set.h>

#include <takatori/scalar/walk.h>

#include <takatori/relation/intermediate/dispatch.h>

#include <takatori/util/assertion.h>
#include <takatori/util/downcast.h>
#include <takatori/util/exception.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/string_builder.h>

#include <yugawara/binding/factory.h>

#include <yugawara/extension/scalar/extension_id.h>
#include <yugawara/extension/scalar/subquery.h>
#include <yugawara/extension/scalar/exists.h>
#include <yugawara/extension/scalar/quantified_compare.h>

#include <yugawara/extension/relation/extension_id.h>
#include <yugawara/extension/relation/subquery.h>

namespace yugawara::analyzer::details {

namespace descriptor = ::takatori::descriptor;
namespace scalar = ::takatori::scalar;
namespace relation = ::takatori::relation;

using ::takatori::util::optional_ptr;
using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;
using ::takatori::util::unsafe_downcast;

using diagnostic_code_type = rewrite_correlated_subquery_result::diagnostic_code_type;
using diagnostic_type = rewrite_correlated_subquery_result::diagnostic_type;

namespace {

class inspector_task {
public:
    inspector_task(relation::expression& target) noexcept: // NOLINT
        target_ { target },
        required_ { true }
    {}

    inspector_task(inspector_task prototype, relation::expression& target) noexcept: // NOLINT
        target_ { target },
        required_ { prototype.required_ },
        saw_parameters_ { std::move(prototype.saw_parameters_) },
        parent_task_ { std::move(prototype.parent_task_) },
        branch_ { prototype.branch_ }
    {}

    inspector_task(
            std::weak_ptr<inspector_task> parent,
            relation::expression& target,
            std::size_t branch,
            bool required) noexcept:
        target_ { target },
        required_ { required },
        parent_task_ { std::move(parent) },
        branch_ { branch }
    {}

    [[nodiscard]] relation::expression& target() noexcept {
        return target_;
    }

    [[nodiscard]] bool required() const noexcept {
        return required_;
    }

    [[nodiscard]] bool has_parameter_access() const noexcept {
        return !saw_parameters_.empty();
    }

    void saw_parameter(descriptor::variable& parameter) {
        if (!saw_parameters_.contains(parameter)) {
            saw_parameters_.emplace(parameter);
        }
    }

    [[nodiscard]] std::shared_ptr<inspector_task> parent_task() noexcept {
        return parent_task_.lock();
    }

    [[nodiscard]] std::size_t max_child() const noexcept {
        if (std::get<1>(child_tasks_) != nullptr) {
            return 2;
        }
        if (std::get<0>(child_tasks_) != nullptr) {
            return 1;
        }
        return 0;
    }

    [[nodiscard]] std::shared_ptr<inspector_task> find_child_shared(std::size_t branch) const{
        if (branch >= child_tasks_.size()) {
            return {};
        }
        if (auto&& ptr = child_tasks_.at(branch)) {
            return ptr;
        }
        return {};
    }

    [[nodiscard]] optional_ptr<inspector_task> find_child(std::size_t branch) const {
        if (branch >= child_tasks_.size()) {
            return {};
        }
        if (auto&& ptr = child_tasks_.at(branch)) {
            return *ptr;
        }
        return {};
    }

    void set_child(std::shared_ptr<inspector_task> child) {
        auto index = child->branch_;
        auto& container = child_tasks_.at(index);
        if (container != nullptr) {
            throw_exception(std::logic_error { "conflict branch number" });
        }
        container = std::move(child);
    }

    void remove_child(inspector_task& child) {
        auto index = child.branch_;
        auto&& container = child_tasks_.at(index);
        if (container.get() != std::addressof(child)) {
            throw_exception(std::logic_error { "inconsistent child entry" });
        }
        container = {};
    }

private:
    relation::expression& target_;
    bool required_ {};
    ::tsl::hopscotch_set<::takatori::descriptor::variable> saw_parameters_ {};
    std::weak_ptr<inspector_task> parent_task_ {};

    static constexpr std::size_t child_tasks_size = 2;
    std::array<std::shared_ptr<inspector_task>, child_tasks_size> child_tasks_ {};
    std::size_t branch_ {};
};

class inspector {
public:
    explicit inspector(std::vector<::takatori::descriptor::variable> const& parameters):
        parameters_ { parameters.begin(), parameters.end() }
    {}

    [[nodiscard]] std::vector<diagnostic_type> release_diagnostics() {
        std::vector<diagnostic_type> results = std::move(diagnostics_);
        diagnostics_.clear();
        return results;
    }

    [[nodiscard]] std::shared_ptr<inspector_task> process(relation::expression& expression) {
        work_list_.emplace_back(expression);
        while (!work_list_.empty()) {
            auto&& task = work_list_.front();
            relation::intermediate::dispatch(*this, task.target(), task);
            if (!diagnostics_.empty()) {
                return {};
            }
            work_list_.pop_front();
        }
        auto result = fill_child_to_parent();
        return result;
    }

    std::shared_ptr<inspector_task> fill_child_to_parent() {
        std::deque<std::shared_ptr<inspector_task>> work {};
        for (auto&& task_ptr: upstream_tasks_) {
            work.emplace_back(std::move(task_ptr));
        }
        upstream_tasks_.clear();

        std::shared_ptr<inspector_task> result {};
        ::tsl::hopscotch_set<inspector_task*> saw_tasks {};
        while (!work.empty()) {
            auto task = std::move(work.front());
            work.pop_front();
            if (saw_tasks.contains(task.get())) {
                continue;
            }
            saw_tasks.emplace(task.get());
            if (auto parent = task->parent_task()) {
                parent->set_child(std::move(task));
                work.emplace_back(std::move(parent));
            } else {
                if (result != nullptr && result != task) {
                    throw_exception(std::logic_error { "multiple roots" });
                }
                result = std::move(task);
            }
        }
        if (result == nullptr) {
            throw_exception(std::logic_error { "empty root" });
        }
        return result;
    }

    // relation expressions

    void operator()(relation::find& expr, inspector_task& task) {
        auto original_state = std::exchange(state_, inspection_state::scan_parameter);
        process_keys(task, expr, expr.keys());
        state_ = original_state;
        register_upstream(std::move(task));
    }

    void operator()(relation::scan& expr, inspector_task& task) {
        auto original_state = std::exchange(state_, inspection_state::scan_parameter);
        process_keys(task, expr, expr.lower().keys());
        process_keys(task, expr, expr.upper().keys());
        state_ = original_state;
        register_upstream(std::move(task));
    }

    void operator()(relation::join_find& expr, inspector_task& task) {
        process_keys(task, expr, expr.keys());
        if (auto&& condition = expr.condition()) {
            scalar::walk(*this, *condition, task);
        }
        schedule_next(std::move(task), expr.left());
    }

    void operator()(relation::join_scan& expr, inspector_task& task) {
        process_keys(task, expr, expr.lower().keys());
        process_keys(task, expr, expr.upper().keys());
        if (auto&& condition = expr.condition()) {
            scalar::walk(*this, *condition, task);
        }
        schedule_next(std::move(task), expr.left());
    }

    void operator()(relation::apply& expr, inspector_task& task) {
        for (auto&& argument : expr.arguments()) {
            scalar::walk(*this, argument, task);
        }
        schedule_next(std::move(task), expr.input());
    }

    void operator()(relation::project& expr, inspector_task& task) {
        for (auto&& column : expr.columns()) {
            scalar::walk(*this, column.value(), task);
        }
        schedule_next(std::move(task), expr.input());
    }

    void operator()(relation::filter& expr, inspector_task& task) {
        scalar::walk(*this, expr.condition(), task);
        schedule_next(std::move(task), expr.input());
    }

    void operator()(relation::buffer const& expr, inspector_task const& task) {
        (void) task;
        report(diagnostic_code_type::unsupported_feature, expr,
                "nesting correlated subquery is not supported (found \"buffer\" operator)");
    }

    void operator()(relation::identify& expr, inspector_task& task) {
        schedule_next(std::move(task), expr.input());
    }

    void operator()(relation::emit& expr, inspector_task& task) {
        for (auto&& column : expr.columns()) {
            saw_variable(task, expr, column.source());
        }
        schedule_next(std::move(task), expr.input());
    }

    void operator()(relation::write& expr, inspector_task& task) {
        for (auto&& key : expr.keys()) {
            saw_variable(task, expr, key.source());
        }
        for (auto&& column : expr.columns()) {
            saw_variable(task, expr, column.source());
        }
        schedule_next(std::move(task), expr.input());
    }

    void operator()(relation::values& expr, inspector_task& task) {
        if (expr.rows().size() != 1) {
            report(diagnostic_code_type::unsupported_feature, expr,
                    "VALUES can have only a single row in correlated subquery");
            return;
        }
        auto&& row_elements = expr.rows().at(0).elements();
        if (row_elements.size() != expr.columns().size()) {
            throw_exception(std::domain_error { "inconsistent value column size" });
        }
        for (auto&& row : expr.rows()) {
            for (auto&& element : row.elements()) {
                scalar::walk(*this, element, task);
            }
        }
        register_upstream(std::move(task));
    }

    void operator()(relation::intermediate::join& expr, inspector_task& task) {
        process_keys(task, expr, expr.lower().keys());
        process_keys(task, expr, expr.upper().keys());
        if (auto&& condition = expr.condition()) {
            scalar::walk(*this, *condition, task);
        }
        using relation::join_kind;
        switch (expr.operator_kind()) {
            case join_kind::inner:
                schedule_split(std::move(task), expr.left(), expr.right(), true);
                break;
            case join_kind::left_outer:
            case join_kind::left_outer_at_most_one:
            case join_kind::semi:
            case join_kind::anti:
                schedule_split(std::move(task), expr.left(), expr.right(), false);
                break;
            case join_kind::full_outer:
                report(diagnostic_code_type::unsupported_feature, expr,
                        "full outer join in correlated subquery is not supported");
                break;
        }
    }

    void operator()(relation::intermediate::aggregate& expr, inspector_task& task) {
        if (expr.group_keys().empty()) {
            report(diagnostic_code_type::unsupported_feature, expr,
                    "aggregation without GROUP BY in correlated subquery is not supported");
            return;
        }
        for (auto&& key : expr.group_keys()) {
            saw_variable(task, expr, key);
        }
        for (auto&& column : expr.columns()) {
            for (auto&& argument : column.arguments()) {
                saw_variable(task, expr, argument);
            }
        }
        schedule_next(std::move(task), expr.input());
    }

    void operator()(relation::intermediate::distinct& expr, inspector_task& task) {
        for (auto&& key : expr.group_keys()) {
            saw_variable(task, expr, key);
        }
        schedule_next(std::move(task), expr.input());
    }

    void operator()(relation::intermediate::limit& expr, inspector_task& task) {
        for (auto&& key : expr.group_keys()) {
            saw_variable(task, expr, key);
        }
        for (auto&& key : expr.sort_keys()) {
            saw_variable(task, expr, key.variable());
        }
        schedule_next(std::move(task), expr.input());
    }

    void operator()(relation::intermediate::union_& expr, inspector_task& task) {
        auto left = upstream(expr.left());
        auto right = upstream(expr.right());
        if (!left || !right) {
            return;
        }
        schedule_split(std::move(task), expr.left(), expr.right(), true);
    }

    void operator()(relation::intermediate::intersection& expr, inspector_task& task) {
        process_binary_set_operation(expr, task);
    }

    void operator()(relation::intermediate::difference& expr, inspector_task& task) {
        process_binary_set_operation(expr, task);
    }

    template<class Expr>
    void process_binary_set_operation(Expr& expr, inspector_task& task) {
        auto left = upstream(expr.left());
        auto right = upstream(expr.right());
        if (!left || !right) {
            return;
        }
        schedule_split(std::move(task), expr.left(), expr.right(), true);
    }

    void operator()(relation::intermediate::escape& expr, inspector_task& task) {
        schedule_next(std::move(task), expr.input());
    }

    void operator()(::takatori::relation::intermediate::extension& expr, inspector_task& task) {
        switch (expr.extension_id()) {
            case extension::relation::subquery::extension_tag:
                operator()(unsafe_downcast<extension::relation::subquery&>(expr), task);
                break;
            default:
                throw_exception(std::domain_error {
                    string_builder {}
                            << "unknown extension of relation expression: "
                            << "extension_id=" << expr.extension_id()
                            << string_builder::to_string
                });
        }
    }

    void operator()(extension::relation::subquery& expr, inspector_task& task) {
        if (expr.is_clone()) {
            throw_exception(std::logic_error { "unhandled cloned table subquery" });
        }
        auto output = expr.find_output_port();
        if (!output) {
            throw_exception(std::logic_error { "unhandled table subquery without output" });
        }
        // split task because subgraph is orphaned
        auto required = task.required();
        auto parent = std::make_shared<inspector_task>(std::move(task));
        all_tasks_.emplace_back(parent);
        work_list_.emplace_back(std::move(parent), output->owner(), 0, required);
    }

    // scalar expressions

    void operator()(scalar::expression const& expr, inspector_task const& task) const noexcept {
        (void) expr;
        (void) task;
    }

    void operator()(scalar::variable_reference& expr, inspector_task& task) {
        saw_variable(task, expr, expr.variable());
    }

    void operator()(scalar::extension const& expr, inspector_task const& task) {
        (void) task;
        switch (expr.extension_id()) {
            case extension::scalar::subquery::extension_tag:
            case extension::scalar::exists::extension_tag:
            case extension::scalar::quantified_compare::extension_tag:
                report(diagnostic_code_type::unsupported_feature, expr,
                        "nesting correlated subqueries is not supported");
                break;
            default:
                throw_exception(std::domain_error(string_builder {}
                        << "unknown extension of scalar expression: "
                        << expr.extension_id()
                        << string_builder::to_string));
        }
    }

private:
    ::tsl::hopscotch_set<::takatori::descriptor::variable> parameters_;
    std::deque<inspector_task> work_list_ {};
    std::vector<std::shared_ptr<inspector_task>> all_tasks_ {};
    std::vector<std::shared_ptr<inspector_task>> upstream_tasks_ {};
    std::vector<diagnostic_type> diagnostics_ {};

    enum class inspection_state {
        normal,
        scan_parameter,
    };
    inspection_state state_ { inspection_state::normal };

    void report(diagnostic_type diagnostic) {
        diagnostics_.emplace_back(std::move(diagnostic));
    }

    void report(
            diagnostic_code_type code,
            scalar::expression const& occasion,
            std::string message) {
        report(diagnostic_type { code, std::move(message), occasion.region() });
    }

    void report(
            diagnostic_code_type code,
            relation::expression const& occasion,
            std::string message) {
        report(diagnostic_type { code, std::move(message), occasion.region() });
    }

    template<class Expr>
    void saw_variable(inspector_task& task, Expr const& expr, ::takatori::descriptor::variable& variable) {
        if (!parameters_.contains(variable)) {
            return;
        }
        if (state_ == inspection_state::scan_parameter) {
            report(diagnostic_code_type::unsupported_feature, expr, "scan parameter cannot contain correlated columns");
            return;
        }
        task.saw_parameter(variable);
    }

    optional_ptr<relation::expression> upstream(relation::expression::input_port_type& input) {
        auto opposite = input.opposite();
        if (!opposite) {
            throw_exception(std::logic_error { "orphaned input" });
        }
        return opposite->owner();
    }

    void register_upstream(inspector_task task) {
        auto shared = std::make_shared<inspector_task>(std::move(task));
        all_tasks_.emplace_back(shared);
        upstream_tasks_.emplace_back(std::move(shared));
    }

    void schedule_next(inspector_task task, relation::expression::input_port_type& input) {
        if (auto next = upstream(input)) {
            work_list_.emplace_back(std::move(task), *next);
        }
    }

    void schedule_split(
            inspector_task task,
            relation::expression::input_port_type& left,
            relation::expression::input_port_type& right,
            bool right_required) {
        auto left_next = upstream(left);
        auto right_next = upstream(right);
        if (left_next && right_next) {
            auto required = task.required();
            auto parent = std::make_shared<inspector_task>(std::move(task));
            all_tasks_.emplace_back(parent);
            work_list_.emplace_back(parent, *left_next, 0, required);
            work_list_.emplace_back(std::move(parent), *right_next, 1, required && right_required);
        }
    }

    template<class Expr, class Keys>
    void process_keys(inspector_task& task, Expr const& expr, Keys&& keys) {
        for (auto& key : keys) {
            saw_variable(task, expr, key.variable());
            scalar::walk(*this, key.value(), task);
        }
    }
};

class pruner {
public:
    void process(std::shared_ptr<inspector_task> task) {
        work_list_.emplace_back(std::move(task));
        while (!work_list_.empty()) {
            auto&& next = work_list_.front();
            relation::intermediate::dispatch(*this, next->target(), *next);
            for (std::size_t index = 0, size = next->max_child(); index < size; ++index) {
                if (auto child = next->find_child_shared(index)) {
                    work_list_.emplace_back(std::move(child));
                }
            }
            work_list_.pop_front();
        }
    }

    void operator()(relation::expression const& expr, inspector_task const& task) const noexcept {
        (void) expr;
        (void) task;
    }

    void operator()(relation::intermediate::join const& expr, inspector_task& task) {
        using relation::join_kind;
        switch (expr.operator_kind()) {
            case join_kind::inner:
                // prune right first
                if (!prune(task, 1)) {
                    // or, if right contains parameters, we try prune left
                    prune(task, 0);
                }
                break;

            case join_kind::left_outer:
            case join_kind::left_outer_at_most_one:
                // keep left term because parameters will become null in right term.
                prune(task, 1);
                break;

            case join_kind::semi:
            case join_kind::anti:
                // keep left term because right term will be discarded.
                prune(task, 1);
                break;

            case join_kind::full_outer:
                // keep both
                break;
        }
    }

    void operator()(relation::intermediate::union_ const& expr, inspector_task const& task) const noexcept {
        (void) expr;
        (void) task;
        // NOTE: must keep both upstreams to flow parameters
    }

    void operator()(relation::intermediate::intersection const& expr, inspector_task& task) {
        (void) expr;
        // keep left term because right term will be discarded
        prune(task, 1);
    }

    void operator()(relation::intermediate::difference const& expr, inspector_task& task) {
        (void) expr;
        // keep left term because right term will be discarded
        prune(task, 1);
    }

private:
    std::deque<std::shared_ptr<inspector_task>> work_list_ {};

    bool prune(inspector_task& task, std::size_t branch) {
        auto child = task.find_child(branch);
        if (!child) {
            throw_exception(std::logic_error { "missing child" });
        }
        if (!saw_parameter(*child)) {
            task.remove_child(*child);
            return true;
        }
        return false;
    }

    [[nodiscard]] bool saw_parameter(inspector_task const& task) const {
        if (task.has_parameter_access()) {
            return true;
        }
        for (std::size_t index = 0, size = task.max_child(); index < size; ++index) {
            if (auto child = task.find_child(index)) {
                if (saw_parameter(*child)) {
                    return true;
                }
            }
        }
        return false;
    }
};

class rewriter_context {
public:
    explicit rewriter_context(std::shared_ptr<inspector_task> task) noexcept:
        task_ { std::move(task) }
    {}

    [[nodiscard]] bool empty() const noexcept {
        return mappings_.empty();
    }

    [[nodiscard]] std::shared_ptr<inspector_task> const& task() const noexcept {
        return task_;
    }

    [[nodiscard]] ::tsl::hopscotch_map<descriptor::variable, descriptor::variable>& mappings() noexcept {
        return mappings_;
    }

    [[nodiscard]] ::tsl::hopscotch_map<descriptor::variable, descriptor::variable> const& mappings() const noexcept {
        return mappings_;
    }

    bool redeclare(descriptor::variable const& original, descriptor::variable replacement) {
        if (auto iter = mappings_.find(original); iter != mappings_.end()) {
            iter.value() = std::move(replacement);
            return true;
        }
        return false;
    }

    // replace if the variable is original parameter
    bool replace(descriptor::variable& variable) const {
        if (auto found = find(variable)) {
            variable = *found;
            return true;
        }
        return false;
    }

    [[nodiscard]] optional_ptr<descriptor::variable const> find(descriptor::variable const& variable) const {
        if (auto iter = mappings_.find(variable); iter != mappings_.end()) {
            return iter.value();
        }
        return {};
    }

    [[nodiscard]] descriptor::variable const& get(descriptor::variable const& variable) const {
        if (auto found = find(variable)) {
            return *found;
        }
        throw_exception(std::logic_error { "missing variable" });
    }

    [[nodiscard]] std::optional<correlated_subquery_input>& upstream_input() {
        return upstream_input_;
    }

    [[nodiscard]] std::optional<correlated_subquery_input> const& upstream_input() const {
        return upstream_input_;
    }

private:
    std::shared_ptr<inspector_task> task_;
    ::tsl::hopscotch_map<descriptor::variable, descriptor::variable> mappings_ {};

    std::optional<correlated_subquery_input> upstream_input_;
};

class rewriter {
public:
    explicit rewriter(std::vector<descriptor::variable> const& parameters):
        parameters_ { parameters }
    {}

    rewrite_correlated_subquery_result process(std::shared_ptr<inspector_task> const& root) {
        BOOST_ASSERT(root->parent_task() == nullptr);

        auto tasks = sort_tasks(root);
        std::vector<correlated_subquery_input> inputs {};
        for (auto&& task : tasks) {
            if (auto input = process_task(task)) {
                inputs.emplace_back(std::move(*input));
            }
        }

        std::vector<rewrite_correlated_subquery_result::mapping_type> output_mappings {};
        output_mappings.reserve(parameters_.size());
        auto iter = context_map_.find(root);
        if (iter == context_map_.end()) {
            throw_exception(std::logic_error { "missing context for root task" });
        }
        auto& root_context = iter.value();
        for (auto&& parameter : parameters_) {
            auto replacement = parameter;
            if (!root_context.replace(replacement)) {
                throw_exception(std::logic_error { "missing mapping for parameter" });
            }
            output_mappings.emplace_back(parameter, std::move(replacement));
        }

        return { std::move(inputs), std::move(output_mappings) };
    }

    void operator()(relation::find& expr, rewriter_context& context) {
        BOOST_ASSERT(context.empty());
        // NOTE: we don't check find keys, because there must not contain any parameters by inspector.
        /*
         * find --*
         * =>
         * find -\
         *        +-- join --*
         *   () -/
         */
        process_input_operator(expr, context);
    }

    void operator()(relation::scan& expr, rewriter_context& context) {
        BOOST_ASSERT(context.empty());
        // NOTE: we don't check scan keys, because there must not contain any parameters by inspector.

        /*
         * scan --*
         * =>
         * scan -\
         *        +-- join --*
         *   () -/
         */
        process_input_operator(expr, context);
    }

    template<class Expr>
    void process_input_operator(Expr& expr, rewriter_context& context) {
        auto&& graph = expr.owner();
        auto&& join = graph.template emplace<relation::intermediate::join>(relation::join_kind::inner);
        if (auto downstream = expr.output().reconnect_to(join.right())) {
            join.output().connect_to(*downstream);
        }
        auto input_mappings = prepare_input_context(context);
        context.upstream_input().emplace(join.left(), std::move(input_mappings));
    }

     std::vector<relation::details::mapping_element> prepare_input_context(rewriter_context& context) {
        auto&& context_mappings = context.mappings();
        context_mappings.reserve(parameters_.size());
        auto&& input_mappings = std::vector<relation::details::mapping_element>();
        input_mappings.reserve(parameters_.size());
        binding::factory f {};
        for (auto&& parameter : parameters_) {
            auto replacement = f.stream_variable(parameter);
            context_mappings.emplace(parameter, replacement);
            input_mappings.emplace_back(parameter, std::move(replacement));
        }
        return input_mappings;
    }

    void operator()(relation::join_find& expr, rewriter_context const& context) {
        BOOST_ASSERT(!context.empty());
        process_keys(context, expr.keys());
        if (auto&& condition = expr.condition()) {
            scalar::walk(*this, *condition, context);
        }
    }

    void operator()(relation::join_scan& expr, rewriter_context const& context) {
        BOOST_ASSERT(!context.empty());
        process_keys(context, expr.lower().keys());
        process_keys(context, expr.upper().keys());
        if (auto&& condition = expr.condition()) {
            scalar::walk(*this, *condition, context);
        }
    }

    void operator()(relation::apply& expr, rewriter_context const& context) {
        BOOST_ASSERT(!context.empty());
        for (auto&& argument : expr.arguments()) {
            scalar::walk(*this, argument, context);
        }
    }

    void operator()(relation::project& expr, rewriter_context const& context) {
        BOOST_ASSERT(!context.empty());
        for (auto&& column : expr.columns()) {
            scalar::walk(*this, column.value(), context);
        }
    }

    void operator()(relation::filter& expr, rewriter_context const& context) {
        BOOST_ASSERT(!context.empty());
        scalar::walk(*this, expr.condition(), context);
    }

    void operator()(relation::buffer const& expr, rewriter_context const& context) {
        BOOST_ASSERT(!context.empty());
        (void) expr;
        BOOST_ASSERT_MSG(false, "buffer operator is filtered");
    }

    void operator()(relation::identify const& expr, rewriter_context const& context) {
        BOOST_ASSERT(!context.empty());
        (void) expr;
    }

    void operator()(relation::emit& expr, rewriter_context const& context) {
        BOOST_ASSERT(!context.empty());
        for (auto&& column : expr.columns()) {
            context.replace(column.source());
        }
    }

    void operator()(relation::write& expr, rewriter_context const& context) {
        BOOST_ASSERT(!context.empty());
        for (auto&& key : expr.keys()) {
            context.replace(key.source());
        }
        for (auto&& column : expr.columns()) {
            context.replace(column.source());
        }
    }

    void operator()(relation::values& expr, rewriter_context& context) {
        BOOST_ASSERT(context.empty());

        /*
         * values --*
         * =>
         * () -- project --*
         */
        auto&& value_columns = expr.columns();
        BOOST_ASSERT(expr.rows().size() == 1);
        auto&& row_elements = expr.rows().at(0).elements();
        BOOST_ASSERT(row_elements.size() == value_columns.size());

        // build in reverse-ordered, and then flip it
        std::vector<relation::project::column> project_columns {};
        project_columns.reserve(value_columns.size());
        while (!value_columns.empty()) {
            auto value_column = std::move(value_columns.back());
            value_columns.pop_back();
            auto row_element = row_elements.release_back();
            project_columns.emplace_back(std::move(value_column), std::move(row_element));
        }
        std::reverse(project_columns.begin(), project_columns.end());

        auto&& graph = expr.owner();
        auto&& project = graph.emplace<relation::project>(std::move(project_columns));
        if (auto downstream = expr.output().opposite()) {
            downstream->reconnect_to(project.output());
        }
        graph.erase(expr);

        auto input_mappings = prepare_input_context(context);
        // process created 'project' operator.
        operator()(project, context);

        context.upstream_input().emplace(project.input(), std::move(input_mappings));
    }

    void operator()(relation::intermediate::join& expr, rewriter_context& context) {
        BOOST_ASSERT(context.empty());

        auto&& left_context = find_upstream_context(context, 0);
        auto&& right_context = find_upstream_context(context, 1);
        BOOST_ASSERT(left_context || right_context);

        optional_ptr<rewriter_context> upstream_context {};
        if (left_context) {
            upstream_context = left_context;
        } else {
            upstream_context = right_context;
        }

        process_keys(*upstream_context, expr.lower().keys());
        process_keys(*upstream_context, expr.upper().keys());
        if (auto&& condition = expr.condition()) {
            scalar::walk(*this, *condition, *upstream_context);
        }

        // if both are decorated, must inject join conditions of each parameter set
        if (left_context && right_context) {
            auto current = expr.release_condition();
            for (auto&& parameter : parameters_) {
                auto left_parameter = left_context->get(parameter);
                auto right_parameter = right_context->get(parameter);
                // L.parameter <=> R.parameter
                auto term = std::make_unique<scalar::compare>(
                        scalar::comparison_operator::is_not_distinct_from,
                        std::make_unique<scalar::variable_reference>(std::move(left_parameter)),
                        std::make_unique<scalar::variable_reference>(std::move(right_parameter)));
                if (current) {
                    current = std::make_unique<scalar::binary>(
                            scalar::binary_operator::conditional_and,
                            std::move(current),
                            std::move(term));
                } else {
                    current = std::move(term);
                }
            }
            expr.condition(std::move(current));
        }

        // inherit upstream context
        context.mappings() = upstream_context->mappings();
    }

    void operator()(relation::intermediate::aggregate& expr, rewriter_context const& context) {
        BOOST_ASSERT(!context.empty());

        auto&& group_keys = expr.group_keys();
        for (auto&& key : group_keys) {
            context.replace(key);
        }
        for (auto&& column : expr.columns()) {
            for (auto&& argument : column.arguments()) {
                context.replace(argument);
            }
        }

        // inject parameters into group keys
        group_keys.reserve(group_keys.size() + parameters_.size());
        for (auto&& parameter : parameters_) {
            auto inject = context.get(parameter);
            group_keys.emplace_back(std::move(inject));
        }
    }

    void operator()(relation::intermediate::distinct& expr, rewriter_context const& context) {
        BOOST_ASSERT(!context.empty());

        auto&& group_keys = expr.group_keys();
        for (auto&& key : group_keys) {
            context.replace(key);
        }

        // inject parameters into group keys
        group_keys.reserve(group_keys.size() + parameters_.size());
        for (auto&& parameter : parameters_) {
            auto inject = context.get(parameter);
            group_keys.emplace_back(std::move(inject));
        }
    }

    void operator()(relation::intermediate::limit& expr, rewriter_context const& context) {
        BOOST_ASSERT(!context.empty());

        auto&& group_keys = expr.group_keys();
        for (auto&& key : group_keys) {
            context.replace(key);
        }
        for (auto&& key : expr.sort_keys()) {
            context.replace(key.variable());
        }

        // inject parameters into group keys
        group_keys.reserve(group_keys.size() + parameters_.size());
        for (auto&& parameter : parameters_) {
            auto inject = context.get(parameter);
            group_keys.emplace_back(std::move(inject));
        }
    }

    void operator()(relation::intermediate::union_& expr, rewriter_context& context) {
        BOOST_ASSERT(context.empty());

        auto&& left_context = get_upstream_context(context, 0);
        auto&& right_context = get_upstream_context(context, 1);

        auto&& union_mappings = expr.mappings();
        for (auto&& mapping : union_mappings) {
            if (auto&& left = mapping.left()) {
                left_context.replace(*left);
            }
            if (auto&& right = mapping.right()) {
                right_context.replace(*right);
            }
        }
        binding::factory f {};
        union_mappings.reserve(union_mappings.size() + parameters_.size());
        auto&& parameter_mappings = context.mappings();
        parameter_mappings.reserve(parameters_.size());
        for (auto&& parameter : parameters_) {
            auto&& left_key = left_context.get(parameter);
            auto&& right_key = right_context.get(parameter);
            auto&& union_key = f.stream_variable(left_key);
            union_mappings.emplace_back(left_key, right_key, union_key);
            context.redeclare(parameter, union_key);
            parameter_mappings.emplace(parameter, std::move(union_key));
        }
    }

    void operator()(relation::intermediate::intersection& expr, rewriter_context& context) {
        process_binary_set_operator(expr, context);
    }

    void operator()(relation::intermediate::difference& expr, rewriter_context& context) {
        process_binary_set_operator(expr, context);
    }

    template<class Expr>
    void process_binary_set_operator(Expr& expr, rewriter_context& context) {
        BOOST_ASSERT(context.empty());
        auto&& left_context = get_upstream_context(context, 0);
        auto&& right_context = find_upstream_context(context, 1);

        auto&& pairs = expr.group_key_pairs();
        for (auto&& pair : pairs) {
            left_context.replace(pair.left());
            if (right_context) {
                right_context->replace(pair.right());
            }
        }
        if (right_context) {
            // binary operation between decorated relations
            pairs.reserve(pairs.size() + parameters_.size());
            for (auto&& parameter : parameters_) {
                auto&& left_key = left_context.get(parameter);
                auto&& right_key = right_context->get(parameter);
                pairs.emplace_back(left_key, right_key);
            }
        }

        // inherit left context
        context.mappings() = left_context.mappings();
    }

    void operator()(relation::intermediate::escape& expr, rewriter_context& context) {
        BOOST_ASSERT(!context.empty());
        auto&& escape_mappings = expr.mappings();
        ::tsl::hopscotch_map<descriptor::variable, descriptor::variable> existing_mappings {};
        existing_mappings.reserve(escape_mappings.size());
        for (auto&& mapping : escape_mappings) {
            if (context.find(mapping.source())) {
                // if existing explicit mapping is found, use it as a context mapping
                auto [iter, inserted] = existing_mappings.emplace(mapping.source(), mapping.destination());
                context.replace(mapping.source());
                if (inserted) {
                    context.redeclare(iter->first, iter.value());
                }
            }
        }

        // escape other parameters and redeclare parameters
        escape_mappings.reserve(escape_mappings.size() + parameters_.size());
        binding::factory f {};
        for (auto&& parameter : parameters_) {
            if (!existing_mappings.contains(parameter)) {
                auto source = context.get(parameter);
                auto destination = f.stream_variable(parameter);
                context.redeclare(parameter, destination);
                escape_mappings.emplace_back(std::move(source), std::move(destination));
            }
        }
    }

    void operator()(::takatori::relation::intermediate::extension& expr, rewriter_context& context) {
        switch (expr.extension_id()) {
            case extension::relation::subquery::extension_tag:
                operator()(unsafe_downcast<extension::relation::subquery&>(expr), context);
                break;
            default:
                throw_exception(std::domain_error {
                    string_builder {}
                            << "unknown extension of relation expression: "
                            << "extension_id=" << expr.extension_id()
                            << string_builder::to_string
                });
        }
    }

    void operator()(extension::relation::subquery& expr, rewriter_context& context) {
        BOOST_ASSERT(context.empty());
        auto&& upstream_context = get_upstream_context(context, 0);

        auto&& query_output_mappings = expr.mappings();
        for (auto&& mapping : query_output_mappings) {
            upstream_context.replace(mapping.source());
        }

        // expose parameters from relation subquery
        context.mappings() = upstream_context.mappings();
        query_output_mappings.reserve(query_output_mappings.size() + parameters_.size());
        for (auto&& parameter : parameters_) {
            auto source = context.get(parameter);
            auto destination = context.get(parameter);
            context.redeclare(parameter, destination);
            query_output_mappings.emplace_back(std::move(source), std::move(destination));
        }
    }

    // scalar expressions

    void operator()(scalar::expression const& expr, rewriter_context const& context) const noexcept {
        (void) expr;
        (void) context;
    }

    void operator()(scalar::variable_reference& expr, rewriter_context const& context) const {
        context.replace(expr.variable());
    }

    [[noreturn]] void operator()(scalar::extension const& expr, rewriter_context const& context) const {
        (void) context;
        switch (expr.extension_id()) {
            case extension::scalar::subquery::extension_tag:
            case extension::scalar::exists::extension_tag:
            case extension::scalar::quantified_compare::extension_tag:
                throw_exception(std::logic_error { "unexpected subqueries" });
            default:
                throw_exception(std::domain_error(string_builder {}
                        << "unknown extension of scalar expression: "
                        << expr.extension_id()
                        << string_builder::to_string));
        }
    }

private:
    std::vector<descriptor::variable> parameters_;
    ::tsl::hopscotch_map<std::shared_ptr<inspector_task>, rewriter_context> context_map_ {};

    [[nodiscard]] std::vector<std::shared_ptr<inspector_task>> sort_tasks(std::shared_ptr<inspector_task> root) {
        std::vector<std::shared_ptr<inspector_task>> results {};
        std::deque<std::shared_ptr<inspector_task>> work_list {};
        work_list.emplace_back(std::move(root));
        while (!work_list.empty()) {
            auto&& next = work_list.front();
            for (std::size_t index = 0, size = next->max_child(); index < size; ++index) {
                // extract right to left because process reverse ordered
                std::size_t rindex = size - index - 1U;
                if (auto child = next->find_child_shared(rindex)) {
                    work_list.emplace_back(std::move(child));
                }
            }
            results.emplace_back(std::move(next));
            work_list.pop_front();
        }
        std::reverse(results.begin(), results.end());
        return results;
    }

    [[nodiscard]] std::optional<correlated_subquery_input> process_task(std::shared_ptr<inspector_task> const& task) {
        optional_ptr<relation::expression> successor_head {};
        if (auto&& successor = task->parent_task()) {
            successor_head.reset(successor->target());
        }
        // collect expressions to process first, because expression graph may be modified in succeeding operation
        std::vector<std::reference_wrapper<relation::expression>> expression_list {};
        optional_ptr current { task->target() };
        while (true) {
            expression_list.emplace_back(std::ref(*current));
            auto next = next_operator(*current);
            // continue until end of graph or next task head
            if (!next || successor_head.get() == next.get()) {
                break;
            }
            current = next;
        }
        rewriter_context context { task };
        for (auto&& expr : expression_list) {
            relation::intermediate::dispatch(*this, expr.get(), context);
        }
        auto result = std::move(context.upstream_input());
        context_map_.emplace(task, std::move(context));
        return result;
    }

    [[nodiscard]] optional_ptr<relation::expression> next_operator(relation::expression& expr) {
        if (expr.output_ports().empty()) {
            return {};
        }
        if (expr.output_ports().size() != 1) {
            throw_exception(std::logic_error { "output port of rewrite target must have upto one output" });
        }
        if (auto opposite = expr.output_ports().at(0).opposite()) {
            return optional_ptr { opposite->owner() };
        }
        // end of table subquery must not have opposite
        return {};
    }

    [[nodiscard]] rewriter_context& get_upstream_context(rewriter_context const& context, std::size_t branch) {
        auto&& child_task = context.task()->find_child_shared(branch);
        if (!child_task) {
            throw_exception(std::logic_error { "missing child task (may be pruned out)" });
        }
        if (auto iter = context_map_.find(child_task); iter != context_map_.end()) {
            return iter.value();
        }
        throw_exception(std::logic_error { "missing upstream context (broken step)" });
    }

    [[nodiscard]] optional_ptr<rewriter_context> find_upstream_context(rewriter_context const& context, std::size_t branch) {
        auto&& child_task = context.task()->find_child_shared(branch);
        if (!child_task) {
            return {};
        }
        if (auto iter = context_map_.find(child_task); iter != context_map_.end()) {
            return iter.value();
        }
        return {};
    }

    template<class Keys>
    void process_keys(rewriter_context const& context, Keys&& keys) {
        for (auto& key : keys) {
            context.replace(key.variable());
            scalar::walk(*this, key.value(), context);
        }
    }
};

rewrite_correlated_subquery_result process_correlated_subquery(
        relation::expression::output_port_type& output_port,
        std::vector<relation::details::mapping_element> const& parameter_mappings,
        std::optional<descriptor::variable> const& output_column) {
    if (parameter_mappings.empty()) {
        throw_exception(std::logic_error { "parameters must not be empty" });
    }
    if (output_column) {
        auto iter = std::find_if(
                    parameter_mappings.begin(),
                    parameter_mappings.end(),
                    [&](auto&& mapping) {
                        return mapping.destination() == *output_column;
                    });
        if (iter != parameter_mappings.end()) {
            return rewrite_correlated_subquery_result {
                    {
                            diagnostic_type {
                                    diagnostic_code_type::unsupported_feature,
                                    "parameter output must not be a correlated column",
                                    output_port.owner().region(),
                            }
                    },
            };
        }
    }
    std::vector<descriptor::variable> parameters {};
    parameters.reserve(parameter_mappings.size());
    for (auto&& mapping : parameter_mappings) {
        parameters.emplace_back(mapping.destination());
    }
    inspector inspect { parameters };
    auto root = inspect.process(output_port.owner());
    if (!root) {
        auto diagnostics = inspect.release_diagnostics();
        return rewrite_correlated_subquery_result { std::move(diagnostics) };
    }
    pruner prune {};
    prune.process(root);

    rewriter rewrite { parameters };
    auto result = rewrite.process(root);
    return result;
}

} // namespace

correlated_subquery_input::correlated_subquery_input(
        relation::expression::input_port_type& input_port,
        std::vector<mapping_type> mappings) noexcept:
    input_port_ { input_port },
    mappings_ { std::move(mappings) }
{}

takatori::relation::expression::input_port_type& correlated_subquery_input::input_port() noexcept {
    return input_port_;
}

std::vector<takatori::relation::details::mapping_element>& correlated_subquery_input::mappings() noexcept {
    return mappings_;
}

rewrite_correlated_subquery_result::rewrite_correlated_subquery_result(
        std::vector<diagnostic_type> diagnostics) noexcept:
    diagnostics_ { std::move(diagnostics) }
{}

rewrite_correlated_subquery_result::rewrite_correlated_subquery_result(
        std::vector<input_type> inputs,
        std::vector<mapping_type> output_mappings) noexcept:
    inputs_ { std::move(inputs) },
    output_mappings_ { std::move(output_mappings) }
{}

rewrite_correlated_subquery_result::operator bool() const noexcept {
    return diagnostics_.empty();
}

std::vector<diagnostic_type>& rewrite_correlated_subquery_result::diagnostics() noexcept {
    return diagnostics_;
}

std::vector<rewrite_correlated_subquery_result::input_type>& rewrite_correlated_subquery_result::inputs() noexcept {
    return inputs_;
}

std::vector<relation::details::mapping_element>& rewrite_correlated_subquery_result::output_mappings() noexcept {
    return output_mappings_;
}

rewrite_correlated_subquery_result rewrite_correlated_subquery(extension::scalar::subquery& expr) {
    if (expr.parameters().empty()) {
        throw_exception(std::logic_error { "parameters must not be empty" });
    }
    auto output_port = expr.find_output_port();
    if (!output_port) {
        throw_exception(std::logic_error { "missing output port" });
    }
    std::optional output_column_opt { expr.output_column() };
    auto result = process_correlated_subquery(*output_port, expr.parameters(), output_column_opt);
    expr.output_column() = std::move(*output_column_opt);
    return result;
}

rewrite_correlated_subquery_result rewrite_correlated_subquery(extension::scalar::exists& expr) {
    if (expr.parameters().empty()) {
        throw_exception(std::logic_error { "parameters must not be empty" });
    }
    auto output_port = expr.find_output_port();
    if (!output_port) {
        throw_exception(std::logic_error { "missing output port" });
    }
    std::optional<descriptor::variable> output_column_opt {};
    auto result = process_correlated_subquery(*output_port, expr.parameters(), output_column_opt);
    return result;
}

rewrite_correlated_subquery_result rewrite_correlated_subquery(extension::scalar::quantified_compare& expr) {
    if (expr.parameters().empty()) {
        throw_exception(std::logic_error { "parameters must not be empty" });
    }
    auto output_port = expr.find_output_port();
    if (!output_port) {
        throw_exception(std::logic_error { "missing output port" });
    }
    std::optional output_column_opt { expr.right_column() };
    auto result = process_correlated_subquery(*output_port, expr.parameters(), output_column_opt);
    expr.right_column() = std::move(*output_column_opt);
    return result;
}

} // namespace yugawara::analyzer::details

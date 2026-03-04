#include "expand_subquery.h"

#include <deque>
#include <memory>
#include <tuple>
#include <type_traits>
#include <vector>

#include <takatori/scalar/dispatch.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/intermediate/dispatch.h>

#include <takatori/tree/tree_element_vector.h>
#include <takatori/type/primitive.h>

#include <takatori/util/assertion.h>
#include <takatori/util/clonable.h>
#include <takatori/util/downcast.h>
#include <takatori/util/enum_tag.h>
#include <takatori/util/exception.h>
#include <takatori/util/string_builder.h>
#include <takatori/value/primitive.h>

#include <yugawara/diagnostic.h>

#include <yugawara/binding/factory.h>

#include <yugawara/extension/relation/extension_id.h>
#include <yugawara/extension/relation/subquery.h>
#include <yugawara/extension/scalar/extension_id.h>
#include <yugawara/extension/scalar/subquery.h>
#include <yugawara/extension/scalar/exists.h>
#include <yugawara/extension/scalar/quantified_compare.h>

#include <yugawara/analyzer/intermediate_plan_normalizer_code.h>

#include "rewrite_stream_variables.h"

namespace yugawara::analyzer::details {

namespace {

using ::takatori::util::clone_unique;
using ::takatori::util::downcast;
using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;
using ::takatori::util::unsafe_downcast;

using diagnostic_code_type = intermediate_plan_normalizer_code;
using diagnostic_type = diagnostic<diagnostic_code_type>;

void bypass(
        ::takatori::relation::expression::output_port_type& insertion_point,
        ::takatori::relation::project& insertion) {
    /*
     * before:
     *  upstream -- insertion_point
     *
     *  (absent) -- insertion -- (absent)
     *
     * after:
     *  upstream -- insertion -- insertion_point
     */
    BOOST_ASSERT(!insertion.input().opposite()); // NOLINT
    BOOST_ASSERT(!insertion.output().opposite()); // NOLINT
    if (auto downstream = insertion_point.reconnect_to(insertion.input())) {
        insertion.output().connect_to(*downstream);
    }
}

void bypass(
        ::takatori::relation::expression::input_port_type& insertion_point,
        ::takatori::relation::intermediate::join& insertion) {
    /*
     * before:
     *  upstream -- insertion_point
     *
     *  (absent) -[L]-\
     *                 join -- (absent)
     *  existing -[R]-/
     *
     * after:
     *  upstream -[L]-\
     *                 join -- insertion_point
     *  existing -[R]-/
     */
    BOOST_ASSERT(!insertion.left().opposite());
    BOOST_ASSERT(!insertion.output().opposite());
    if (auto upstream = insertion_point.reconnect_to(insertion.output())) {
        upstream->connect_to(insertion.left());
    }
}

class expand_subquery_command {
public:
    expand_subquery_command() = default;
    virtual ~expand_subquery_command() = default;
    expand_subquery_command(const expand_subquery_command&) = delete;
    expand_subquery_command& operator=(const expand_subquery_command&) = delete;
    expand_subquery_command(expand_subquery_command&&) = delete;
    expand_subquery_command& operator=(expand_subquery_command&&) = delete;
};

class expand_relation_subquery_command final : public expand_subquery_command {
public:
    explicit expand_relation_subquery_command(extension::relation::subquery& target) :
        target_ { target }
    {}

    [[nodiscard]] extension::relation::subquery& target() noexcept {
        return target_;
    }

private:
    extension::relation::subquery& target_;
};

class expand_scalar_expression_command : public expand_subquery_command {
public:
    [[nodiscard]] virtual ::takatori::scalar::expression& target() noexcept = 0;
    virtual void locate(::takatori::relation::expression::input_port_type& insertion_point) = 0;
};

class expand_scalar_subquery_command final : public expand_scalar_expression_command {
public:
    explicit expand_scalar_subquery_command(std::unique_ptr<extension::scalar::subquery> target) noexcept:
        target_ { std::move(target) }
    {}

    [[nodiscard]] extension::scalar::subquery& target() noexcept override {
        return *target_;
    }

    [[nodiscard]] ::takatori::relation::expression::input_port_type& insertion_point() {
        if (insertion_point_ == nullptr) {
            throw_exception(std::logic_error { "not located" });
        }
        return *insertion_point_;
    }

    void locate(::takatori::relation::expression::input_port_type& insertion_point) override {
        if (insertion_point_ != nullptr) {
            throw_exception(std::logic_error { "already located" });
        }
        insertion_point_ = std::addressof(insertion_point);
    }

private:
    std::unique_ptr<extension::scalar::subquery> target_;
    ::takatori::relation::expression::input_port_type* insertion_point_ {};
};

class expand_exists_filter_command final : public expand_scalar_expression_command {
public:
    explicit expand_exists_filter_command(
            std::unique_ptr<extension::scalar::exists> target,
            bool is_conditional_not) noexcept:
        target_ { std::move(target) },
        is_conditional_not_ { is_conditional_not }
    {}

    [[nodiscard]] extension::scalar::exists& target() noexcept override {
        return *target_;
    }

    [[nodiscard]] bool is_conditional_not() const noexcept {
        return is_conditional_not_;
    }

    [[nodiscard]] ::takatori::relation::expression::input_port_type& insertion_point() {
        if (insertion_point_ == nullptr) {
            throw_exception(std::logic_error { "not located" });
        }
        return *insertion_point_;
    }

    void locate(::takatori::relation::expression::input_port_type& insertion_point) override {
        if (insertion_point_ != nullptr) {
            throw_exception(std::logic_error { "already located" });
        }
        insertion_point_ = std::addressof(insertion_point);
    }

private:
    std::unique_ptr<extension::scalar::exists> target_;
    bool is_conditional_not_;
    ::takatori::relation::expression::input_port_type* insertion_point_ {};
};

class expand_exists_value_command final : public expand_scalar_expression_command {
public:
    explicit expand_exists_value_command(
            std::unique_ptr<extension::scalar::exists> target,
            ::takatori::descriptor::variable output_column) noexcept:
        target_ { std::move(target) },
        output_column_ { std::move(output_column) }
    {}

    [[nodiscard]] extension::scalar::exists& target() noexcept override {
        return *target_;
    }

    [[nodiscard]] ::takatori::descriptor::variable output_column() const noexcept {
        return output_column_;
    }

    [[nodiscard]] ::takatori::relation::expression::input_port_type& insertion_point() {
        if (insertion_point_ == nullptr) {
            throw_exception(std::logic_error { "not located" });
        }
        return *insertion_point_;
    }

    void locate(::takatori::relation::expression::input_port_type& insertion_point) override {
        if (insertion_point_ != nullptr) {
            throw_exception(std::logic_error { "already located" });
        }
        insertion_point_ = std::addressof(insertion_point);
    }

private:
    std::unique_ptr<extension::scalar::exists> target_;
    ::takatori::descriptor::variable output_column_;
    ::takatori::relation::expression::input_port_type* insertion_point_ {};
};

class expand_quantified_compare_filter_command final : public expand_scalar_expression_command {
public:
    explicit expand_quantified_compare_filter_command(
            std::unique_ptr<extension::scalar::quantified_compare> target) noexcept:
        target_ { std::move(target) }
    {}

    [[nodiscard]] extension::scalar::quantified_compare& target() noexcept override {
        return *target_;
    }

    [[nodiscard]] ::takatori::relation::expression::input_port_type& insertion_point() {
        if (insertion_point_ == nullptr) {
            throw_exception(std::logic_error { "not located" });
        }
        return *insertion_point_;
    }

    void locate(::takatori::relation::expression::input_port_type& insertion_point) override {
        if (insertion_point_ != nullptr) {
            throw_exception(std::logic_error { "already located" });
        }
        insertion_point_ = std::addressof(insertion_point);
    }

private:
    std::unique_ptr<extension::scalar::quantified_compare> target_;
    ::takatori::relation::expression::input_port_type* insertion_point_ {};
};

enum class insertion_context_kind {
    scan_criteria,
    join_criteria,
    filter_condition,
    generic_value,
};

class insertion_context {
public:
    explicit insertion_context(insertion_context_kind context_kind) noexcept :
        context_kind_ { context_kind }
    {}

    [[nodiscard]] insertion_context_kind context_kind() const noexcept {
        return context_kind_;
    }

    [[nodiscard]] std::vector<diagnostic_type>& diagnostics() noexcept {
        return diagnostics_;
    }

    [[nodiscard]] std::vector<std::unique_ptr<expand_scalar_expression_command>>& commands() noexcept {
        return commands_;
    }

    [[nodiscard]] std::vector<::takatori::relation::expression*>& worklist() noexcept {
        return worklist_;
    }

    void report(diagnostic_type diagnostic) {
        diagnostics_.emplace_back(std::move(diagnostic));
    }

    void report(
            diagnostic_code_type code,
            ::takatori::scalar::expression const& occasion,
            std::string message) {
        report(diagnostic_type { code, std::move(message), occasion.region() });
    }

    void add_worklist(::takatori::relation::graph_type& graph) {
        for (auto& node : graph) {
            worklist_.push_back(std::addressof(node));
        }
    }

private:
    insertion_context_kind context_kind_;
    std::vector<diagnostic_type> diagnostics_ {};
    std::vector<std::unique_ptr<expand_scalar_expression_command>> commands_ {};
    std::vector<::takatori::relation::expression*> worklist_ {};

    friend class insertion_context_operand_block;
};

class insertion_context_operand_block {
public:
    explicit insertion_context_operand_block(insertion_context& context) noexcept :
        context_ { context },
        escaped_ { context.context_kind_ }
    {
        if (context.context_kind_ == insertion_context_kind::filter_condition) {
            context.context_kind_ = insertion_context_kind::generic_value;
        }
    }

    ~insertion_context_operand_block() {
        context_.context_kind_ = escaped_;
    }

    insertion_context_operand_block(insertion_context_operand_block const&) = delete;
    insertion_context_operand_block& operator=(insertion_context_operand_block const&) = delete;
    insertion_context_operand_block(insertion_context_operand_block&&) = delete;
    insertion_context_operand_block& operator=(insertion_context_operand_block&&) = delete;

private:
    insertion_context& context_;
    insertion_context_kind escaped_ {};
};


class collector {
public:
    collector() = default;

    [[nodiscard]] std::vector<diagnostic_type> collect(takatori::relation::graph_type& graph) {
        add_worklist(graph);
        while (!worklist_.empty() && diagnostics_.empty()) {
            auto* node = worklist_.front();
            worklist_.pop_front();
            collect(*node);
        }

        auto diagnostics = std::move(diagnostics_);
        diagnostics_.clear();

        return diagnostics;
    }

    std::vector<std::unique_ptr<expand_subquery_command>> release() noexcept {
        std::vector results { std::move(commands_) };
        commands_.clear();
        worklist_.clear();
        return results;
    }

    // ---- relation expressions

    void operator()(::takatori::relation::find& expr) {
        insertion_context context { insertion_context_kind::scan_criteria };
        if (!collect_keys(context, expr.keys())) {
            return;
        }
        validate_no_commands(expr, std::move(context));
    }

    void operator()(::takatori::relation::scan& expr) {
        insertion_context context { insertion_context_kind::scan_criteria };
        if (!collect_keys(context, expr.lower().keys())) {
            return;
        }
        if (!collect_keys(context, expr.upper().keys())) {
            return;
        }
        validate_no_commands(expr, std::move(context));
    }

    void operator()(::takatori::relation::join_find& expr) {
        insertion_context context { insertion_context_kind::join_criteria };
        if (!collect_keys(context, expr.keys())) {
            return;
        }
        if (auto&& condition = expr.condition()) {
            if (auto&& replacement = collect(context, *condition)) {
                expr.condition(std::move(replacement));
            }
            if (merge_error(context)) {
                return;
            }
        }
        validate_no_commands(expr, std::move(context));
    }

    void operator()(::takatori::relation::join_scan& expr) {
        insertion_context context { insertion_context_kind::join_criteria };
        if (!collect_keys(context, expr.lower().keys())) {
            return;
        }
        if (!collect_keys(context, expr.upper().keys())) {
            return;
        }
        if (auto&& condition = expr.condition()) {
            if (auto&& replacement = collect(context, *condition)) {
                expr.condition(std::move(replacement));
            }
            if (merge_error(context)) {
                return;
            }
        }
        validate_no_commands(expr, std::move(context));
    }

    void operator()(::takatori::relation::apply& expr) {
        insertion_context context { insertion_context_kind::generic_value };
        if (!collect_vector(context, expr.arguments())) {
            return;
        }
        merge_commands(expr.input(), std::move(context));
    }

    void operator()(::takatori::relation::project& expr) {
        insertion_context context { insertion_context_kind::generic_value };
        for (auto&& column : expr.columns()) {
            if (auto&& replacement = collect(context, column.value())) {
                column.value(std::move(replacement));
            }
            if (merge_error(context)) {
                return;
            }
            // FIXME: check commands and split columns if necessary in correlated subqueries
        }
        merge_commands(expr.input(), std::move(context));
    }

    void operator()(::takatori::relation::filter& expr) {
        insertion_context context { insertion_context_kind::filter_condition };
        if (auto&& replacement = collect(context, expr.condition())) {
            expr.condition(std::move(replacement));
        }
        if (merge_error(context)) {
            return;
        }
        merge_commands(expr.input(), std::move(context));
    }

    void operator()(::takatori::relation::buffer const& expr) const noexcept {
        // NOTE: nothing to do
        (void) expr;
    }

    void operator()(::takatori::relation::identify const& expr) const noexcept {
        // NOTE: nothing to do
        (void) expr;
    }

    void operator()(::takatori::relation::emit const& expr) const noexcept {
        // NOTE: nothing to do
        (void) expr;
    }

    void operator()(::takatori::relation::write const& expr) const noexcept {
        // NOTE: nothing to do
        (void) expr;
    }

    void operator()(::takatori::relation::values& expr) {
        // first, extract splits for each row
        auto splits = collect_values(expr);

        // if there is no rewrite, we use original `values` expression.
        if (splits.empty()) {
            return;
        }

        // special case: if values has only one row, and it contains subqueries,
        // we can directly rewrite the row without sharding.
        if (expr.rows().size() == 1) {
            BOOST_ASSERT(splits.size() == 1); // NOLINT
            BOOST_ASSERT(std::get<0>(splits.front()) == 0); // NOLINT
            process_values_row(expr, std::move(std::get<1>(splits.front())));
            return;
        }

        // otherwise we need to split the `values` expression into multiple shards.
        auto shards = migrate_values_into_shards(expr, std::move(splits));
        BOOST_ASSERT(shards.size() >= 2); // NOLINT

        // then, we assemble shards using `union[all]`
        auto&& replacement = assemble_values_shards(expr, shards);

        // then, rewrite subqueries in each shard
        for (auto&& [shard, row_splits] : shards) {
            if (!row_splits.empty()) {
                process_values_row(*shard, std::move(row_splits));
            }
        }

        // finally, we rewrite original `value` into the assembled `union[all]`, and remove the original expression.
        if (auto downstream = expr.output().opposite()) {
            downstream->reconnect_to(replacement.output());
        }
        expr.owner().erase(expr);
    }

private:
    struct split_info {
        std::size_t column_index;
        std::unique_ptr<::takatori::scalar::expression> replacement;
        insertion_context context;
    };

    [[nodiscard]] std::vector<std::tuple<std::size_t, std::vector<split_info>>> collect_values(
            ::takatori::relation::values& expr) {
        std::vector<std::tuple<std::size_t, std::vector<split_info>>> splits {};
        std::size_t row_index = 0;
        auto&& rows = expr.rows();
        for (auto&& row : rows) {
            auto&& row_data = row.elements();
            std::vector<split_info> row_splits {};
            std::size_t column_index = 0;
            for (auto iter_values = row_data.begin(); iter_values != row_data.end();) {
                auto&& value = *iter_values;
                insertion_context context { insertion_context_kind::generic_value };
                if (auto&& replacement = collect(context, value)) {
                    if (merge_error(context)) {
                        return {};
                    }
                    row_splits.emplace_back(split_info {
                            column_index,
                            std::move(replacement),
                            std::move(context)
                    });
                    // remove the current column and fill it by subquery expansion.
                    row_data.erase(iter_values);
                } else {
                    if (merge_error(context)) {
                        return {};
                    }
                    ++iter_values;
                }
                ++column_index;
            }
            if (!row_splits.empty()) {
                splits.emplace_back(row_index, std::move(row_splits));
            }
            ++row_index;
        }
        return splits;
    }

    [[nodiscard]] std::vector<std::tuple<::takatori::relation::values*, std::vector<split_info>>> migrate_values_into_shards(
            ::takatori::relation::values& expr,
            std::vector<std::tuple<std::size_t, std::vector<split_info>>> splits) {
        std::vector<std::tuple<::takatori::relation::values*, std::vector<split_info>>> shards {};
        shards.reserve(splits.size() * 2 + 1);
        std::size_t row_start = 0;
        for (auto&& [row_index, row_splits] : splits) {
            BOOST_ASSERT(row_index >= row_start); // NOLINT
            if (row_index > row_start) {
                auto&& shard = migrate_values_shard(expr, row_start, row_index);
                shards.emplace_back(std::addressof(shard), std::vector<split_info> {});
            }

            auto&& shard = migrate_values_shard(expr, row_index, row_index + 1);
            shards.emplace_back(std::addressof(shard), std::move(row_splits));
            row_start = row_index + 1;
        }
        if (expr.rows().size() > row_start) {
            auto&& shard = migrate_values_shard(expr, row_start, expr.rows().size());
            shards.emplace_back(std::addressof(shard), std::vector<split_info> {});
        }
        return shards;
    }

    [[nodiscard]] ::takatori::relation::values& migrate_values_shard(
            ::takatori::relation::values& expr,
            std::size_t row_start,
            std::size_t row_end) {
        BOOST_ASSERT(row_start < row_end); // NOLINT
        std::vector<::takatori::relation::values::column> columns {};
        columns.reserve(expr.columns().size());
        for (std::size_t index = 0, size = expr.columns().size(); index < size; ++index) {
            columns.emplace_back(binding::factory {}.stream_variable());
        }
        std::vector<::takatori::relation::values::row> rows {};
        rows.reserve(row_end - row_start);
        for (std::size_t i = row_start; i < row_end; ++i) {
            rows.emplace_back(std::move(expr.rows().at(i)));
        }
        return expr.owner().emplace<::takatori::relation::values>(std::move(columns), std::move(rows));
    }

    void process_values_row(::takatori::relation::values& expr, std::vector<split_info> row_splits) {
        BOOST_ASSERT(expr.rows().size() == 1); // NOLINT
        BOOST_ASSERT(!row_splits.empty()); // NOLINT
        auto&& project = expr.owner().emplace<::takatori::relation::project>(
                std::vector<::takatori::relation::project::column> {});
        auto&& project_columns = project.columns();
        project.columns().reserve(row_splits.size());
        for (auto&& [column_index, replacement, context] : row_splits) {
            auto column = expr.columns().at(column_index);
            project_columns.emplace_back(std::move(column), std::move(replacement));
            merge_commands(project.input(), std::move(context));
        }
        for (auto riter = row_splits.rbegin(); riter != row_splits.rend(); ++riter) {
            auto column_index = riter->column_index;
            expr.columns().erase(
                expr.columns().begin() + static_cast<std::make_signed_t<std::size_t>>(column_index));
        }
        bypass(expr.output(), project);
    }

    [[nodiscard]] ::takatori::relation::intermediate::union_& assemble_values_shards(
            ::takatori::relation::values& expr,
            std::vector<std::tuple<::takatori::relation::values*, std::vector<split_info>>>& shards) {
        BOOST_ASSERT(shards.size() >= 2); // NOLINT

        ::takatori::relation::expression::output_port_type* current_output = nullptr;
        std::vector<::takatori::relation::values::column> current_columns {};
        for (auto&& [shard, _] : shards) {
            if (current_output == nullptr) {
                current_output = std::addressof(shard->output());
                BOOST_ASSERT(current_columns.empty()); // NOLINT
                for (auto&& column : shard->columns()) {
                    current_columns.emplace_back(column);
                }
                continue;
            }
            BOOST_ASSERT(current_columns.size() == shard->columns().size()); // NOLINT
            std::vector<::takatori::relation::intermediate::union_::mapping> mappings {};
            mappings.reserve(current_columns.size());
            binding::factory factory {};
            for (std::size_t i = 0; i < current_columns.size(); ++i) {
                mappings.emplace_back(
                    std::move(current_columns.at(i)),
                    shard->columns().at(i),
                    factory.stream_variable());
            }
            auto&& union_all = expr.owner().emplace<::takatori::relation::intermediate::union_>(
                ::takatori::relation::set_quantifier::all,
                std::move(mappings));
            union_all.left().connect_to(*current_output);
            union_all.right().connect_to(shard->output());
            current_output = std::addressof(union_all.output());
            current_columns.clear();
            for (auto&& mapping : union_all.mappings()) {
                current_columns.emplace_back(mapping.destination());
            }
        }
        BOOST_ASSERT(current_output != nullptr); // NOLINT
        BOOST_ASSERT(current_output->owner().kind() == ::takatori::relation::intermediate::union_::tag); // NOLINT
        auto&& replacement = unsafe_downcast<::takatori::relation::intermediate::union_>(current_output->owner());

        BOOST_ASSERT(replacement.mappings().size() == expr.columns().size()); // NOLINT
        for (std::size_t i = 0; i < replacement.mappings().size(); ++i) {
            auto& mapping = replacement.mappings().at(i);
            auto& column = expr.columns().at(i);
            mapping.destination() = std::move(column);
        }
        return replacement;
    }

public:
    void operator()(::takatori::relation::intermediate::join& expr) {
        insertion_context context { insertion_context_kind::join_criteria };
        if (!collect_keys(context, expr.lower().keys())) {
            return;
        }
        if (!collect_keys(context, expr.upper().keys())) {
            return;
        }
        if (auto&& condition = expr.condition()) {
            if (auto&& replacement = collect(context, *condition)) {
                expr.condition(std::move(replacement));
            }
            if (merge_error(context)) {
                return;
            }
        }
        validate_no_commands(expr, std::move(context));
    }

    void operator()(::takatori::relation::intermediate::aggregate const& expr) const noexcept {
        // NOTE: nothing to do
        (void) expr;
    }

    void operator()(::takatori::relation::intermediate::distinct const& expr) const noexcept {
        // NOTE: nothing to do
        (void) expr;
    }

    void operator()(::takatori::relation::intermediate::limit const& expr) const noexcept {
        // NOTE: nothing to do
        (void) expr;
    }

    void operator()(::takatori::relation::intermediate::union_ const& expr) const noexcept {
        // NOTE: nothing to do
        (void) expr;
    }

    void operator()(::takatori::relation::intermediate::intersection const& expr) const noexcept {
        // NOTE: nothing to do
        (void) expr;
    }

    void operator()(::takatori::relation::intermediate::difference const& expr) const noexcept {
        // NOTE: nothing to do
        (void) expr;
    }

    void operator()(::takatori::relation::intermediate::escape const& expr) const noexcept {
        // NOTE: nothing to do
        (void) expr;
    }

    void operator()(::takatori::relation::intermediate::extension& expr) {
        switch (expr.extension_id()) {
            case extension::relation::subquery::extension_tag:
                operator()(unsafe_downcast<extension::relation::subquery&>(expr));
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

    void operator()(extension::relation::subquery& expr) {
        if (expr.is_clone()) {
            // NOTE: if this is a clone, the variables in the subquery may be shared with another subquery.
            rewrite_stream_variables(expr);
            expr.is_clone() = false;
        }
        commands_.emplace_back(std::make_unique<expand_relation_subquery_command>(expr));
        add_worklist(expr.query_graph());
    }

    // ---- scalar expressions

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(
            ::takatori::scalar::immediate const& expr,
            insertion_context const& context) const noexcept {
        // NOTE: no special operands
        (void) context;
        (void) expr;
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(
            ::takatori::scalar::variable_reference const& expr,
            insertion_context const& context) const noexcept {
        // NOTE: no special operands
        (void) context;
        (void) expr;
        return {};
    }

    using conditional_not_t = ::takatori::util::enum_tag_t<::takatori::scalar::unary_operator::conditional_not>;

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(
            ::takatori::scalar::unary& expr,
            insertion_context& context) const {
        if (expr.operator_kind() == ::takatori::scalar::unary_operator::conditional_not) {
            // NOTE: to handle `NOT EXISTS` or `NOT x IN (subquery)`, we try check operand first
            if (auto replacement = ::takatori::scalar::dispatch(*this, expr.operand(), conditional_not_t {}, context)) {
                return replacement;
            }
        }
        insertion_context_operand_block block { context };
        if (auto&& replacement = collect(context, expr.operand())) {
            expr.operand(std::move(replacement));
        }
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(
            ::takatori::scalar::expression const& expr,
            conditional_not_t,
            insertion_context const& context) const noexcept {
        (void) expr;
        (void) context;
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(
            ::takatori::scalar::cast& expr,
            insertion_context& context) const {
        insertion_context_operand_block block { context };
        if (auto&& replacement = collect(context, expr.operand())) {
            expr.operand(std::move(replacement));
        }
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(
            ::takatori::scalar::binary& expr,
            insertion_context& context) const {
        if (expr.operator_kind() == ::takatori::scalar::binary_operator::conditional_and) {
            // ... AND ... -> keep if context_kind::filter_condition
            if (auto&& replacement = collect(context, expr.left())) {
                expr.left(std::move(replacement));
            }
            if (auto&& replacement = collect(context, expr.right())) {
                expr.right(std::move(replacement));
            }
        } else {
            insertion_context_operand_block block { context };
            if (auto&& replacement = collect(context, expr.left())) {
                expr.left(std::move(replacement));
            }
            if (auto&& replacement = collect(context, expr.right())) {
                expr.right(std::move(replacement));
            }
        }
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(
            ::takatori::scalar::compare& expr,
            insertion_context& context) const {
        insertion_context_operand_block block { context };
        if (auto&& replacement = collect(context, expr.left())) {
            expr.left(std::move(replacement));
        }
        if (auto&& replacement = collect(context, expr.right())) {
            expr.right(std::move(replacement));
        }
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(
            ::takatori::scalar::match& expr,
            insertion_context& context) const {
        insertion_context_operand_block block { context };
        if (auto&& replacement = collect(context, expr.input())) {
            expr.input(std::move(replacement));
        }
        if (auto&& replacement = collect(context, expr.pattern())) {
            expr.pattern(std::move(replacement));
        }
        if (auto&& replacement = collect(context, expr.escape())) {
            expr.escape(std::move(replacement));
        }
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(
            ::takatori::scalar::conditional& expr,
            insertion_context& context) const {
        insertion_context_operand_block block { context };
        for (auto&& alternative : expr.alternatives()) {
            if (auto&& replacement = collect(context, alternative.condition())) {
                alternative.condition(std::move(replacement));
            }
            if (auto&& replacement = collect(context, alternative.body())) {
                alternative.body(std::move(replacement));
            }
        }
        if (auto&& default_expression = expr.default_expression()) {
            if (auto&& replacement = collect(context, *default_expression)) {
                expr.default_expression(std::move(replacement));
            }
        }
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(
            ::takatori::scalar::coalesce& expr,
            insertion_context& context) const {
        insertion_context_operand_block block { context };
        auto&& alternatives = expr.alternatives();
        for (auto iter = alternatives.begin(); iter != alternatives.end(); ++iter) {
            if (auto&& replacement = collect(context, *iter)) {
                alternatives.put(iter, std::move(replacement));
            }
        }
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(
            ::takatori::scalar::let& expr,
            insertion_context& context) const {
        insertion_context_operand_block block { context };
        for (auto&& declarator : expr.variables()) {
            if (auto&& replacement = collect(context, declarator.value())) {
                declarator.value(std::move(replacement));
            }
        }
        if (auto&& replacement = collect(context, expr.body())) {
            expr.body(std::move(replacement));
        }
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(
            ::takatori::scalar::function_call& expr,
            insertion_context& context) const {
        insertion_context_operand_block block { context };
        auto&& arguments = expr.arguments();
        for (auto iter = arguments.begin(); iter != arguments.end(); ++iter) {
            if (auto&& replacement = collect(context, *iter)) {
                arguments.put(iter, std::move(replacement));
            }
        }
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(
            ::takatori::scalar::extension& expr,
            insertion_context& context) const {
        switch (expr.extension_id()) {
            case extension::scalar::subquery::extension_tag:
                return operator()(unsafe_downcast<extension::scalar::subquery>(expr), context);
            case extension::scalar::exists::extension_tag:
                return operator()(unsafe_downcast<extension::scalar::exists>(expr), context);
            case extension::scalar::quantified_compare::extension_tag:
                return operator()(unsafe_downcast<extension::scalar::quantified_compare>(expr), context);
            default:
                throw_exception(std::domain_error {
                    string_builder {}
                            << "unknown extension of scalar expression: "
                            << "extension_id=" << expr.extension_id()
                            << string_builder::to_string
                });
        }
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(
            ::takatori::scalar::extension& expr,
            conditional_not_t,
            insertion_context& context) const {
        switch (expr.extension_id()) {
            case extension::scalar::exists::extension_tag:
                return operator()(unsafe_downcast<extension::scalar::exists>(expr), conditional_not_t {}, context);
            case extension::scalar::quantified_compare::extension_tag:
                return operator()(unsafe_downcast<extension::scalar::quantified_compare>(expr), conditional_not_t {}, context);
            default:
                return {};
        }
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(
            extension::scalar::subquery& expr,
            insertion_context& context) const {
        if (!check_subquery_context_kind(expr, context)) {
            return {};
        }
        auto replacement = std::make_unique<::takatori::scalar::variable_reference>(expr.output_column());
        replacement->region() = expr.region();
        context.add_worklist(expr.query_graph());
        auto command = std::make_unique<expand_scalar_subquery_command>(clone_unique(std::move(expr)));

        context.commands().emplace_back(std::move(command));
        return replacement;
    }

private:
    [[nodiscard]] bool check_subquery_context_kind(
            ::takatori::scalar::expression const& expr,
            insertion_context& context) const {
        switch (context.context_kind()) {
            case insertion_context_kind::scan_criteria:
                context.report(
                    intermediate_plan_normalizer_code::unsupported_scalar_subquery_placement,
                    expr,
                    "subquery is not allowed in scan criteria"
                );
                return false;
            case insertion_context_kind::join_criteria:
                context.report(
                    intermediate_plan_normalizer_code::unsupported_scalar_subquery_placement,
                    expr,
                    "subquery is not allowed in join criteria"
                );
                return false;
            default:
                return true;
        }
    }

public:
    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(
            extension::scalar::exists& expr,
            insertion_context& context) const {
        return process_exists(expr, false, context);
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(
            extension::scalar::exists& expr,
            conditional_not_t,
            insertion_context& context) const {
        return process_exists(expr, true, context);
    }

private:
    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> process_exists(
            extension::scalar::exists& expr,
            bool is_conditional_not,
            insertion_context& context) const {
        if (!check_subquery_context_kind(expr, context)) {
            return {};
        }
        if (context.context_kind() == insertion_context_kind::filter_condition) {
            // resolve `EXISTS` as a filter condition...
            return process_exists_filter(expr, is_conditional_not, context);
        }
        // otherwise, resolve `EXISTS` as a generic boolean expression.
        return process_exists_value(expr, is_conditional_not, context);
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> process_exists_filter(
            extension::scalar::exists& expr,
            bool is_conditional_not,
            insertion_context& context) const {
        /* NOTE:
         * `EXISTS` is appeared as a top-level filter condition.
         * We replace it as just `TRUE` and insert semi/anti join
         * before the filter to check if the subquery returns any rows.
         */
        auto replacement = std::make_unique<::takatori::scalar::immediate>(
                std::make_shared<::takatori::value::boolean>(true),
                std::make_shared<::takatori::type::boolean>());
        replacement->region() = expr.region();

        context.add_worklist(expr.query_graph());
        auto command = std::make_unique<expand_exists_filter_command>(
                clone_unique(std::move(expr)),
                is_conditional_not);

        context.commands().emplace_back(std::move(command));
        return replacement;
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> process_exists_value(
            extension::scalar::exists& expr,
            bool is_conditional_not,
            insertion_context& context) const {
        /**
         * NOTE:
         * `EXISTS` is appeared as a generic boolean expression (e.g. operand of other predicate).
         * We replace it as `x IS TRUE` or `x IS NULL` and insert left outer join with the subquery.
         */
        auto output_column = binding::factory().stream_variable();
        std::unique_ptr<::takatori::scalar::expression> replacement;
        if (is_conditional_not) {
            // `NOT EXISTS` -> `x IS NULL`
            replacement = std::make_unique<::takatori::scalar::unary>(
                    ::takatori::scalar::unary_operator::is_null,
                    std::make_unique<::takatori::scalar::variable_reference>(output_column));
        } else {
            // `EXISTS` -> `x IS TRUE`
            replacement = std::make_unique<::takatori::scalar::unary>(
                    ::takatori::scalar::unary_operator::is_true,
                    std::make_unique<::takatori::scalar::variable_reference>(output_column));
        }
        replacement->region() = expr.region();

        context.add_worklist(expr.query_graph());
        auto command = std::make_unique<expand_exists_value_command>(
                clone_unique(std::move(expr)),
                std::move(output_column));

        context.commands().emplace_back(std::move(command));
        return replacement;
    }

public:
    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(
            extension::scalar::quantified_compare& expr,
            insertion_context& context) const {
        return process_quantified_compare(expr, false, context);
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(
            extension::scalar::quantified_compare& expr,
            conditional_not_t,
            insertion_context& context) const {
        return process_quantified_compare(expr, true, context);
    }

private:
    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> process_quantified_compare(
            extension::scalar::quantified_compare& expr,
            bool is_conditional_not,
            insertion_context& context) const {
        if (!check_subquery_context_kind(expr, context)) {
            return {};
        }
        if (context.context_kind() == insertion_context_kind::filter_condition) {
            // resolve `IN (subquery)` as a filter condition...
            return process_quantified_compare_filter(expr, is_conditional_not, context);
        }
        context.report(
            intermediate_plan_normalizer_code::unsupported_scalar_subquery_placement,
            expr,
            "quantified comparison (including IN (subquery)) is only allowed on top-level filter conditions"
        );
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> process_quantified_compare_filter(
            extension::scalar::quantified_compare& expr,
            bool is_conditional_not,
            insertion_context& context) const {
        // apply de Morgan's law if this is under `NOT` operator.
        auto n_operator = normalize(expr.operator_kind(), is_conditional_not);
        auto n_quantifier = normalize(expr.quantifier(), is_conditional_not);
        if (n_quantifier == ::takatori::scalar::quantifier::all) {
            context.report(
                intermediate_plan_normalizer_code::unsupported_feature,
                expr,
                "compare with 'ALL', 'NOT ANY', or 'NOT IN' with subquery is not supported"
            );
            return {};
        }

        // rewrite predicate itself
        expr.operator_kind() = n_operator;
        expr.quantifier() = n_quantifier;

        /* NOTE:
         * `x IN (subquery)` or `x op ANY (subquery)` is appeared as a top-level filter condition.
         * We replace it as just `TRUE` and insert semi join with non-quantified comparison before the original filter.
         */
        auto replacement = std::make_unique<::takatori::scalar::immediate>(
                std::make_shared<::takatori::value::boolean>(true),
                std::make_shared<::takatori::type::boolean>());
        replacement->region() = expr.region();

        context.add_worklist(expr.query_graph());

        auto command = std::make_unique<expand_quantified_compare_filter_command>(
                clone_unique(std::move(expr)));

        context.commands().emplace_back(std::move(command));
        return replacement;
    }

    [[nodiscard]] ::takatori::scalar::comparison_operator normalize(
            ::takatori::scalar::comparison_operator operator_,
            bool is_conditional_not) const noexcept {
        if (is_conditional_not) {
            return ~operator_;
        }
        return operator_;
    }

    [[nodiscard]] ::takatori::scalar::quantifier normalize(
            ::takatori::scalar::quantifier quantifier,
            bool is_conditional_not) const noexcept {
        if (is_conditional_not) {
            if (quantifier == ::takatori::scalar::quantifier::any) {
                return ::takatori::scalar::quantifier::all;
            }
            return ::takatori::scalar::quantifier::any;
        }
        return quantifier;
    }


    std::vector<std::unique_ptr<expand_subquery_command>> commands_ {};
    std::deque<::takatori::relation::expression*> worklist_ {};
    std::vector<diagnostic_type> diagnostics_ {};

    void add_worklist(::takatori::relation::graph_type& graph) {
        for (auto& node : graph) {
            worklist_.push_back(std::addressof(node));
        }
    }

    void collect(::takatori::relation::expression& expr) {
        ::takatori::relation::intermediate::dispatch(*this, expr);
    }

    /**
     * @brief processes scalar expression.
     * @param context the context of the scalar expression.
     * @param expr the scalar expression to process.
     * @return replacement expression if the input expression will be replaced.
     * @return nullopt if the input expression will not be replaced.
     */
    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> collect(
            insertion_context& context,
            ::takatori::scalar::expression& expr) const {
        return ::takatori::scalar::dispatch(*this, expr, context);
    }

    [[nodiscard]] bool merge_error(insertion_context& context) {
        if (context.diagnostics().empty()) {
            return false;
        }
        diagnostics_.reserve(diagnostics_.size() + context.diagnostics().size());
        for (auto&& diag : context.diagnostics()) {
            diagnostics_.emplace_back(std::move(diag));
        }
        context.diagnostics().clear();
        return true;
    }

    void merge_commands(
            ::takatori::relation::expression::input_port_type& insertion_point,
            insertion_context&& context) {
        for (auto&& command : context.commands()) {
            command->locate(insertion_point);
        }
        merge_collection(diagnostics_, context.diagnostics());
        merge_collection(commands_, context.commands());
        merge_collection(worklist_, context.worklist());
    }

    void validate_no_commands(::takatori::relation::expression const& expr, insertion_context&& context) {
        if (!context.commands().empty()) {
            diagnostics_.emplace_back(
                intermediate_plan_normalizer_code::unknown,
                string_builder {}
                        << "unsupported subquery placement: "
                        << to_string_view(expr.kind())
                        << string_builder::to_string,
                expr.region());
        }
        merge_collection(diagnostics_, context.diagnostics());
    }

    template<class TSource, class TDestination>
    void merge_collection(std::vector<TSource>& source, std::vector<TDestination>& destination) {
        source.reserve(source.size() + destination.size());
        merge_collection0(source, destination);
    }

    template<class TSource, class TDestination>
    void merge_collection(std::deque<TSource>& source, std::vector<TDestination>& destination) {
        merge_collection0(source, destination);
    }

    template<class TSource, class TDestination>
    void merge_collection0(TSource& source, std::vector<TDestination>& destination) {
        for (auto&& element : destination) {
            source.emplace_back(std::move(element));
        }
        destination.clear();
    }

    template<class Keys>
    [[nodiscard]] bool collect_keys(insertion_context& context, Keys& keys) {
        for (auto&& key : keys) {
            if (auto&& replacement = collect(context, key.value())) {
                key.value(std::move(replacement));
            }
            if (merge_error(context)) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool collect_vector(
            insertion_context& context,
            ::takatori::tree::tree_element_vector<::takatori::scalar::expression>& values) {
        for (auto iter = values.begin(); iter != values.end(); ++iter) {
            if (auto&& replacement = collect(context, *iter)) {
                values.put(iter, std::move(replacement));
            }
            if (merge_error(context)) {
                return false;
            }
        }
        return true;
    }
};

void process(expand_subquery_command& command);
void process(expand_relation_subquery_command& command);
void process(expand_scalar_subquery_command& command);
void process(expand_exists_filter_command& command);
void process(expand_exists_value_command& command);
void process(expand_quantified_compare_filter_command& command);

void process(expand_subquery_command& command) {
    if (auto* cmd = downcast<expand_relation_subquery_command>(&command)) {
        process(*cmd);
        return;
    }
    if (auto* cmd = downcast<expand_scalar_subquery_command>(&command)) {
        process(*cmd);
        return;
    }
    if (auto* cmd = downcast<expand_exists_filter_command>(&command)) {
        process(*cmd);
        return;
    }
    if (auto* cmd = downcast<expand_exists_value_command>(&command)) {
        process(*cmd);
        return;
    }
    if (auto* cmd = downcast<expand_quantified_compare_filter_command>(&command)) {
        process(*cmd);
        return;
    }
    throw_exception(std::domain_error { "unknown command for expanding subqueries" });
}

void process(expand_relation_subquery_command& command) {
    auto&& expr = command.target();
    auto&& graph = expr.owner();

    // NOTE: rewrite variables before come here
    BOOST_ASSERT(!expr.is_clone()); // NOLINT

    std::vector<::takatori::relation::project::column> mappings {};
    mappings.reserve(expr.mappings().size());
    for (auto&& mapping : expr.mappings()) {
        mappings.emplace_back(
            std::make_unique<::takatori::scalar::variable_reference>(std::move(mapping.source())),
            std::move(mapping.destination())
        );
    }
    auto&& escape = graph.insert(::takatori::relation::project {
            std::move(mappings)
    });

    auto subquery_output = expr.find_output_port();
    if (!subquery_output) {
        throw_exception(std::domain_error { "subquery has no output port" });
    }
    auto&& downstream = expr.output().opposite();
    if (!downstream) {
        throw_exception(std::domain_error { "dangling subquery" });
    }
    escape.input().connect_to(*subquery_output);
    downstream->reconnect_to(escape.output());

    ::takatori::relation::merge_into(std::move(expr.query_graph()), graph);
    graph.erase(expr);
}

void process(expand_scalar_subquery_command& command) {
    auto&& expr = command.target();
    auto&& insertion_point = command.insertion_point();

    auto&& graph = insertion_point.owner().owner();
    auto&& join = graph.emplace<::takatori::relation::intermediate::join>(
        ::takatori::relation::join_kind::left_outer_at_most_one,
        // ON TRUE
        std::unique_ptr<::takatori::scalar::expression> {});
    join.region() = expr.region();

    auto subquery_output = expr.find_output_port();
    if (!subquery_output) {
        throw_exception(std::domain_error { "subquery has no output port" });
    }
    join.right().connect_to(*subquery_output);
    bypass(insertion_point, join);

    ::takatori::relation::merge_into(std::move(expr.query_graph()), graph);
}

void process(expand_exists_filter_command& command) {
    auto&& expr = command.target();
    auto&& insertion_point = command.insertion_point();

    /* NOTE:
     *
     * main   subquery
     *  |        |
     * SEMI/ANTI JOIN ON TRUE
     */
    auto&& graph = insertion_point.owner().owner();
    auto&& join = graph.emplace<::takatori::relation::intermediate::join>(
        // EXISTS -> semi, NOT EXISTS -> anti
        command.is_conditional_not() ? ::takatori::relation::join_kind::anti : ::takatori::relation::join_kind::semi,
        // ON TRUE
        std::unique_ptr<::takatori::scalar::expression> {});
    join.region() = expr.region();

    // FIXME: for optimization, insert FETCH FIRST 1 ROW

    auto subquery_output = expr.find_output_port();
    if (!subquery_output) {
        throw_exception(std::domain_error { "subquery has no output port" });
    }
    join.right().connect_to(*subquery_output);
    bypass(insertion_point, join);

    ::takatori::relation::merge_into(std::move(expr.query_graph()), graph);
}

void process(expand_exists_value_command& command) {
    auto&& expr = command.target();
    auto&& insertion_point = command.insertion_point();

    /* NOTE:
     *        subquery
     *           |
     *        project[TRUE -> output_column]
     *           |
     * main   FETCH FIRST 1 ROW
     *  |        |
     * LEFT OUTER JOIN ON TRUE
     *  |
     * ... [output_column IS TRUE/NULL]
     */
    auto&& graph = insertion_point.owner().owner();
    std::vector<::takatori::relation::project::column> project_column {};
    project_column.reserve(1);
    project_column.emplace_back(
            std::make_unique<::takatori::scalar::immediate>(
                    std::make_shared<::takatori::value::boolean>(true),
                    std::make_shared<::takatori::type::boolean>()),
            command.output_column());

    auto&& project = graph.emplace<::takatori::relation::project>(std::move(project_column));

    auto&& fetch_first = graph.emplace<::takatori::relation::intermediate::limit>(1);

    auto&& join = graph.emplace<::takatori::relation::intermediate::join>(
        // LEFT OUTER JOIN
        ::takatori::relation::join_kind::left_outer,
        // ON TRUE
        std::unique_ptr<::takatori::scalar::expression> {});
    join.region() = expr.region();

    auto subquery_output = expr.find_output_port();
    if (!subquery_output) {
        throw_exception(std::domain_error { "subquery has no output port" });
    }

    subquery_output->connect_to(project.input());
    project.output().connect_to(fetch_first.input());
    fetch_first.output().connect_to(join.right());
    bypass(insertion_point, join);

    ::takatori::relation::merge_into(std::move(expr.query_graph()), graph);
}

void process(expand_quantified_compare_filter_command& command) {
    auto&& expr = command.target();
    BOOST_ASSERT(expr.quantifier() == ::takatori::scalar::quantifier::any); // NOLINT
    auto&& insertion_point = command.insertion_point();

    /* NOTE:
     *
     * main   subquery
     *  |        |
     * SEMI JOIN ON (left <comp> vref(right_column))
     */
    auto&& graph = insertion_point.owner().owner();
    auto&& join = graph.emplace<::takatori::relation::intermediate::join>(
        ::takatori::relation::join_kind::semi,
        // ON (non-quantified comparison)
        std::make_unique<::takatori::scalar::compare>(
            expr.operator_kind(),
            expr.release_left(),
            std::make_unique<::takatori::scalar::variable_reference>(expr.right_column())));
    join.region() = expr.region();

    auto subquery_output = expr.find_output_port();
    if (!subquery_output) {
        throw_exception(std::domain_error { "subquery has no output port" });
    }
    join.right().connect_to(*subquery_output);
    bypass(insertion_point, join);

    ::takatori::relation::merge_into(std::move(expr.query_graph()), graph);
}

} // namespace

std::vector<diagnostic_type> expand_subquery(takatori::relation::graph_type& graph) {
    collector subquery_collector {};
    auto diagnostics = subquery_collector.collect(graph);
    if (!diagnostics.empty()) {
        return diagnostics;
    }
    auto commands = subquery_collector.release();
    for (auto& command : commands) {
        process(*command);
        command = {};
    }
    return {};
}

} // namespace yugawara::analyzer::details

#include "rewrite_stream_variables.h"

#include <takatori/relation/graph.h>
#include <takatori/relation/intermediate/dispatch.h>
#include <takatori/relation/step/dispatch.h>

#include <takatori/plan/dispatch.h>

#include <takatori/util/assertion.h>
#include <takatori/util/downcast.h>
#include <takatori/util/exception.h>
#include <takatori/util/string_builder.h>

#include <yugawara/extension/relation/extension_id.h>
#include <yugawara/extension/relation/subquery.h>

#include "stream_variable_rewriter_context.h"
#include "scalar_expression_variable_rewriter.h"

namespace yugawara::analyzer::details {

namespace descriptor = ::takatori::descriptor;
namespace relation = ::takatori::relation;
namespace plan = ::takatori::plan;

using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;
using ::takatori::util::unsafe_downcast;

namespace {

using exchange_map_type = exchange_column_info_map;

void remove_escape(stream_variable_rewriter_context& context, relation::graph_type& graph) {
    for (auto it = graph.begin(); it != graph.end();) {
        auto&& expr = *it;
        if (expr.kind() == relation::intermediate::escape::tag) {
            auto&& esc = unsafe_downcast<relation::intermediate::escape>(expr);
            auto incoming = esc.input().opposite();
            auto outgoing = esc.output().opposite();
            BOOST_ASSERT(incoming); // NOLINT
            BOOST_ASSERT(outgoing); // NOLINT
            esc.input().disconnect_all();
            esc.output().disconnect_all();
            incoming->connect_to(*outgoing);

            for (auto&& column : esc.mappings()) {
                context.alias(column.source(), column.destination());
            }

            it = graph.erase(it);
        } else {
            ++it;
        }
    }
}

class engine {
public:
    explicit engine(
            exchange_map_type& exchange_map,
            stream_variable_rewriter_context& context,
            scalar_expression_variable_rewriter& scalar_rewriter,
            bool check_undefined = true) noexcept:
        exchange_map_ { exchange_map },
        context_ { context },
        scalar_rewriter_ { scalar_rewriter },
        check_undefined_ { check_undefined }
    {}

    void process(relation::expression& expr) {
        if (relation::is_available_in_intermediate_plan(expr.kind())) {
            relation::intermediate::dispatch(*this, expr);
        } else if (relation::is_available_in_step_plan(expr.kind())) {
            relation::step::dispatch(*this, expr);
        } else {
            throw_exception(std::domain_error(string_builder {}
                    << "unsupported relation expression: "
                    << expr
                    << string_builder::to_string));
        }
    }

    void force_raise_undefined() {
        raise_undefined(true);
    }

    void operator()(plan::process& step) {
        remove_escape(context_, step.operators());
        relation::sort_from_downstream(
                step.operators(),
                [&](relation::expression& expr) {
                    if (!relation::is_available_in_step_plan(expr.kind())) {
                        throw_exception(std::invalid_argument(string_builder {}
                                << "must be a step plan operator: "
                                << expr
                                << string_builder::to_string));
                    }
                    relation::step::dispatch(*this, expr);
                });
        context_.clear();
    }

    void operator()(relation::expression& expr) {
        throw_exception(std::domain_error(string_builder {}
                << "unsupported relation expression: "
                << expr
                << string_builder::to_string));
    }

    void operator()(relation::find& expr) {
        BOOST_ASSERT(!exchange_map_type::is_target(expr.source())); // NOLINT
        process_keys(expr.keys());
        define_used_columns(expr.columns());
        raise_undefined();
    }

    void operator()(relation::scan& expr) {
        BOOST_ASSERT(!exchange_map_type::is_target(expr.source())); // NOLINT
        process_keys(expr.lower().keys());
        process_keys(expr.upper().keys());
        define_used_columns(expr.columns());
        raise_undefined();
    }

    void operator()(relation::values& expr) {
        auto&& columns = expr.columns();
        auto&& rows = expr.rows();
        std::size_t index = 0;
        for (auto&& it = columns.begin(); it != columns.end();) {
            if (context_.try_rewrite_define(*it)) {
                ++index;
                ++it;
                continue;
            }
            // removes column in table data
            for (auto&& row : rows) {
                auto&& es = row.elements();
                // NOTE: maybe redundant guard
                if (index < es.size()) {
                    es.erase(es.cbegin() + index); // NOLINT
                }
            }
            it = columns.erase(it);
        }
        raise_undefined();
    }

    void operator()(relation::join_find& expr) {
        rewrite(expr.condition());
        if (exchange_map_type::is_target(expr.source())) {
            auto&& info = exchange_map_.get(expr.source());
            process_keys(info, expr.keys());
            define_used_columns(info, expr.columns());
        } else {
            process_keys(expr.keys());
            define_used_columns(expr.columns());
        }
    }

    void operator()(relation::join_scan& expr) {
        rewrite(expr.condition());
        if (exchange_map_type::is_target(expr.source())) {
            auto&& info = exchange_map_.get(expr.source());
            process_keys(info, expr.lower().keys());
            process_keys(info, expr.upper().keys());
            define_used_columns(info, expr.columns());
        } else {
            process_keys(expr.lower().keys());
            process_keys(expr.upper().keys());
            define_used_columns(expr.columns());
        }
    }

    void operator()(relation::apply& expr) {
        for (auto&& argument : expr.arguments()) {
            rewrite(argument);
        }
        auto&& columns = expr.columns();
        for (auto it = columns.begin(); it != columns.end();) {
            if (context_.try_rewrite_define(it->variable())) {
                ++it;
            } else {
                it = columns.erase(it);
            }
        }
    }

    void operator()(relation::project& expr) {
        auto&& columns = expr.columns();
        for (std::size_t i = 0; i < columns.size();) {
            auto index = columns.size() - i - 1;
            auto&& column = columns[index];
            if (context_.try_rewrite_define(column.variable())) {
                rewrite(column.value());
                ++i;
            } else {
                columns.erase(columns.begin() + index); // NOLINT
            }
        }
    }

    void operator()(relation::filter& expr) {
        rewrite(expr.condition());
    }

    void operator()(relation::buffer const&) {
        // no column references
    }

    void operator()(relation::identify& expr) {
        (void) context_.try_rewrite_define(expr.variable());
    }

    void operator()(relation::emit& expr) {
        for (auto&& column : expr.columns()) {
            context_.rewrite_use(column.source());
        }
    }

    void operator()(relation::write& expr) {
        rewrite_source_columns(expr.keys());
        rewrite_source_columns(expr.columns());
    }

    void operator()(relation::intermediate::join& expr) {
        rewrite(expr.condition());
        process_keys(expr.lower().keys());
        process_keys(expr.upper().keys());
    }

    void operator()(relation::intermediate::aggregate& expr) {
        rewrite_variables(expr.group_keys());
        auto&& columns = expr.columns();
        for (auto&& it = columns.begin(); it != columns.end();) {
            auto&& column = *it;
            if (context_.try_rewrite_define(column.destination())) {
                rewrite_variables(column.arguments());
                ++it;
            } else {
                // no one refer the variable
                it = columns.erase(it);
            }
        }
    }

    void operator()(relation::intermediate::distinct& expr) {
        rewrite_variables(expr.group_keys());
    }

    void operator()(relation::intermediate::limit& expr) {
        rewrite_variables(expr.group_keys());
        for (auto&& sort_key : expr.sort_keys()) {
            context_.rewrite_use(sort_key.variable());
        }
    }

    void operator()(relation::intermediate::union_& expr) {
        if (expr.quantifier() == relation::set_quantifier::all) {
            auto&& mappings = expr.mappings();
            for (auto&& it = mappings.begin(); it != mappings.end();) {
                auto&& mapping = *it;
                if (context_.try_rewrite_define(mapping.destination())) {
                    if (auto&& source = mapping.left()) {
                        context_.rewrite_use(*source);
                    }
                    if (auto&& source = mapping.right()) {
                        context_.rewrite_use(*source);
                    }
                    ++it;
                } else {
                    it = mappings.erase(it);
                }
            }
        } else {
            for (auto&& mapping : expr.mappings()) {
                context_.force_rewrite_define(mapping.destination());
                if (auto&& source = mapping.left()) {
                    context_.rewrite_use(*source);
                }
                if (auto&& source = mapping.right()) {
                    context_.rewrite_use(*source);
                }
            }
        }
    }

    void operator()(relation::intermediate::intersection& expr) {
        for (auto&& pair : expr.group_key_pairs()) {
            context_.rewrite_use(pair.left());
            context_.rewrite_use(pair.right());
        }
    }

    void operator()(relation::intermediate::difference& expr) {
        for (auto&& pair : expr.group_key_pairs()) {
            context_.rewrite_use(pair.left());
            context_.rewrite_use(pair.right());
        }
    }

    void operator()(relation::intermediate::escape& expr) {
        auto&& mappings = expr.mappings();
        for (auto it = mappings.begin(); it != mappings.end();) {
            auto&& mapping = *it;
            if (context_.try_rewrite_define(mapping.destination())) {
                context_.rewrite_use(mapping.source());
                ++it;
            } else {
                it = mappings.erase(it);
            }
        }
    }

    void operator()(relation::intermediate::extension& expr) {
        if (extension::relation::is_known_compiler_extension(expr.extension_id())) {
            switch (static_cast<extension::relation::extension_id>(expr.extension_id())) {
                case extension::relation::subquery::extension_tag:
                    operator()(unsafe_downcast<extension::relation::subquery>(expr));
                    return;

                case extension::relation::error_id:
                case extension::relation::min_unused_id:
                    // unknown compiler extension
                    break;
            }
        }
        throw_exception(std::domain_error(string_builder {}
                << "unknown extension of relation expression: "
                << expr.extension_id()
                << string_builder::to_string));
    }

    void operator()(extension::relation::subquery& expr) {
        // NOTE: keep variables in subgraph
        auto&& mappings = expr.mappings();
        for (auto it = mappings.begin(); it != mappings.end();) {
            auto&& mapping = *it;
            if (context_.try_rewrite_define(mapping.destination())) {
                ++it;
            } else {
                it = mappings.erase(it);
            }
        }
        raise_undefined();
    }

    void operator()(relation::step::join& expr) {
        rewrite(expr.condition());
    }

    void operator()(relation::step::aggregate& expr) {
        auto&& columns = expr.columns();
        for (auto&& it = columns.begin(); it != columns.end();) {
            auto&& column = *it;
            if (context_.try_rewrite_define(column.destination())) {
                rewrite_variables(column.arguments());
                ++it;
            } else {
                // no one refer the variable
                it = columns.erase(it);
            }
        }
    }

    void operator()(relation::step::intersection const&) {
        // no column references
    }

    void operator()(relation::step::difference const&) {
        // no column references
    }

    void operator()(relation::step::flatten const&) {
        // no column references
    }

    void operator()(relation::step::take_flat& expr) {
        auto&& info = exchange_map_.get(expr.source());
        define_used_columns(info, expr.columns());
        raise_undefined();
    }

    void operator()(relation::step::take_group& expr) {
        auto&& info = exchange_map_.get(expr.source());
        define_used_columns(info, expr.columns());
        raise_undefined();
    }

    void operator()(relation::step::take_cogroup& expr) {
        for (auto&& group : expr.groups()) {
            auto&& info = exchange_map_.get(group.source());
            define_used_columns(info, group.columns());
        }
        raise_undefined();
    }

    void operator()(relation::step::offer& expr) {
        auto&& info = exchange_map_.get(expr.destination());
        auto&& columns = expr.columns();
        for (auto&& it = columns.begin(); it != columns.end();) {
            auto&& column = *it;
            if (info.is_touched(column.destination())) {
                context_.rewrite_use(column.source());
                ++it;
            } else {
                it = columns.erase(it);
            }
        }
    }

    void operator()(plan::forward& step) {
        auto&& info = exchange_map_.get(step);
        retain_touched_columns(info, step.columns());
    }

    void operator()(plan::group& step) {
        auto&& info = exchange_map_.get(step);

        // always retains group keys and sort keys
        touch_columns(info, step.group_keys());
        touch_sort_keys(info, step.sort_keys());

        retain_touched_columns(info, step.columns());
    }

    void operator()(plan::aggregate& step) {
        auto&& info = exchange_map_.get(step);

        // always retains group keys
        touch_columns(info, step.group_keys());

        // remove unused aggregations, destination columns
        auto&& aggregations = step.aggregations();
        for (auto&& it = aggregations.begin(); it != aggregations.end();) {
            auto&& aggregation = *it;
            if (info.is_touched(aggregation.destination())) {
                ++it;
            } else {
                it = aggregations.erase(it);
            }
        }
        retain_touched_columns(info, step.destination_columns());

        // prepare for upstreams
        info.clear_touched();

        // keep all sources are touched
        touch_columns(info, step.group_keys());
        for (auto&& aggregation : aggregations) {
            touch_columns(info, aggregation.arguments());
        }
        retain_touched_columns(info, step.source_columns());
    }

    void operator()(plan::broadcast& step) {
        auto&& info = exchange_map_.get(step);
        retain_touched_columns(info, step.columns());
    }

    constexpr void operator()(plan::discard&) {
        // no op
    }

private:
    exchange_map_type& exchange_map_;
    stream_variable_rewriter_context& context_;
    scalar_expression_variable_rewriter& scalar_rewriter_;
    bool check_undefined_ {};

    void rewrite(::takatori::scalar::expression& expr) {
        scalar_rewriter_(context_, expr);
    }

    void rewrite(::takatori::util::optional_ptr<::takatori::scalar::expression> expr) {
        if (expr) {
            rewrite(*expr);
        }
    }

    template<class Variables>
    void rewrite_variables(Variables& variables) {
        for (auto&& variable : variables) {
            context_.rewrite_use(variable);
        }
    }

    template<class Columns>
    void rewrite_source_columns(Columns& columns) {
        for (auto&& column : columns) {
            context_.rewrite_use(column.source());
        }
    }

    void raise_undefined(bool force = false) {
        if (!check_undefined_ && !force) {
            return;
        }
        context_.each_undefined([&](descriptor::variable const& variable) {
            throw_exception(std::domain_error(string_builder {}
                    << "undefined variable: "
                    << variable
                    << string_builder::to_string));
        });
    }

    template<class Columns>
    void define_used_columns(Columns& columns) {
        for (auto&& it = columns.begin(); it != columns.end();) {
            auto&& column = *it;
            if (context_.try_rewrite_define(column.destination())) {
                ++it;
            } else {
                it = columns.erase(it);
            }
        }
    }

    template<class Keys>
    void process_keys(Keys& keys) {
        for (auto&& key : keys) {
            rewrite(key.value());
        }
    }

    template<class Columns>
    void define_used_columns(exchange_column_info& info, Columns& columns) {
        for (auto&& it = columns.begin(); it != columns.end();) {
            auto&& column = *it;
            if (context_.try_rewrite_define(column.destination())) {
                info.touch(column.source());
                ++it;
            } else {
                it = columns.erase(it);
            }
        }
    }

    template<class Keys>
    void process_keys(exchange_column_info& info, Keys& keys) {
        for (auto&& key : keys) {
            info.touch(key.variable());
            rewrite(key.value());
        }
    }

    template<class Columns>
    void retain_touched_columns(exchange_column_info const& info, Columns& columns) {
        for (auto&& it = columns.begin(); it != columns.end();) {
            auto&& column = *it;
            if (info.is_touched(column)) {
                ++it;
            } else {
                it = columns.erase(it);
            }
        }
    }

    template<class Columns>
    void touch_columns(exchange_column_info& info, Columns& columns) {
        for (auto&& column : columns) {
            info.touch(column);
        }
    }

    template<class Keys>
    void touch_sort_keys(exchange_column_info& info, Keys& keys) {
        for (auto&& key : keys) {
            info.touch(key.variable());
        }
    }
};

} // namespace

void rewrite_stream_variables(
        exchange_column_info_map& exchange_map,
        plan::graph_type& graph) {
    stream_variable_rewriter_context context {};
    scalar_expression_variable_rewriter scalar_rewriter {};
    engine e { exchange_map, context, scalar_rewriter };
    plan::sort_from_downstream(
            graph,
            [&](plan::step& step) {
                plan::dispatch(e, step);
            });
}

void rewrite_stream_variables(extension::relation::subquery& subquery) {
    exchange_column_info_map exchange_map {};
    stream_variable_rewriter_context context {};
    scalar_expression_variable_rewriter scalar_rewriter {};
    engine e { exchange_map, context, scalar_rewriter, false };

    for (auto&& mapping : subquery.mappings()) {
        context.rewrite_use(mapping.source());
    }

    relation::sort_from_downstream(
            subquery.query_graph(),
            [&](relation::expression& expr) {
                e.process(expr);
            });
    e.force_raise_undefined();
}

} // namespace yugawara::analyzer::details

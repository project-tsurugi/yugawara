#include "collect_exchange_columns.h"

#include <numeric>

#include <takatori/relation/graph.h>
#include <takatori/relation/intermediate/escape.h>
#include <takatori/relation/step/dispatch.h>
#include <takatori/plan/dispatch.h>

#include <takatori/util/assertion.h>
#include <takatori/util/exception.h>
#include <takatori/util/string_builder.h>

#include <yugawara/binding/extract.h>

#include "container_pool.h"

namespace yugawara::analyzer::details {

using buffer_pool_type = container_pool<std::vector<::takatori::descriptor::variable>>;

namespace descriptor = ::takatori::descriptor;
namespace relation = ::takatori::relation;
namespace plan = ::takatori::plan;

using ::takatori::util::unsafe_downcast;
using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;

namespace {

bool is_exchange(descriptor::relation const& desc) {
    return binding::kind_of(desc) == binding::relation_info_kind::exchange;
}

void expand(
        buffer_pool_type::value_type& buffer,
        buffer_pool_type::value_type::size_type n) {
    buffer.reserve(buffer.size() + n);
}

void rewrite_exchange_column(exchange_column_info const& info, descriptor::variable& column) {
    if (auto c = info.find(column)) {
        column = *c;
        return;
    }
    throw_exception(std::domain_error(string_builder {}
            << "unregistered column: "
            << column
            << string_builder::to_string));
}

template<class Upstream, class Expr>
auto& get_upstream(Expr& expr) {
    auto&& upstream = expr.input().opposite()->owner();
    if (upstream.kind() != Upstream::tag) {
        throw_exception(std::domain_error(string_builder {}
                << "inconsistent upstream operator: " << upstream
                << ", but required " << Upstream::tag
                << " at " << expr
                << string_builder::to_string));
    }
    return unsafe_downcast<Upstream>(upstream);
}

class engine {
public:
    using buffer_type = buffer_pool_type::value_type;

    explicit engine(exchange_column_info_map& exchange_map, buffer_pool_type& buffer_pool) noexcept
        : exchange_map_(exchange_map)
        , buffer_pool_(buffer_pool)
    {}

    void operator()(::takatori::plan::process& step) {
        relation::expression* head { nullptr };
        relation::enumerate_top(step.operators(), [&](relation::expression& expr) {
            if (head != nullptr) {
                throw_exception(std::domain_error(string_builder {}
                        << "multiple head operators in process: "
                        << *head
                        << " <=> "
                        << expr
                        << string_builder::to_string));
            }
            head = std::addressof(expr);
        });
        if (head == nullptr) {
            throw_exception(std::domain_error(string_builder {}
                    << "no head operators in process: "
                    << step
                    << string_builder::to_string));
        }

        auto available_columns = buffer_pool_.acquire();
        scan(head, available_columns);
        available_columns.clear();
        buffer_pool_.release(std::move(available_columns));
    }

    // --- operators

    void operator()(relation::find const& expr, buffer_type& available_columns) {
        analyze_read_like(expr, available_columns);
    }

    void operator()(relation::scan const& expr, buffer_type& available_columns) {
        analyze_read_like(expr, available_columns);
    }

    void operator()(relation::join_find& expr, buffer_type& available_columns) {
        analyze_join_read_like(expr, available_columns);

        if (is_exchange(expr.source())) {
            auto&& info = exchange_map_.get(expr.source());
            rewrite_exchange_search_keys(info, expr.keys());
        }
    }

    void operator()(relation::join_scan& expr, buffer_type& available_columns) {
        analyze_join_read_like(expr, available_columns);

        if (is_exchange(expr.source())) {
            auto&& info = exchange_map_.get(expr.source());
            rewrite_exchange_search_keys(info, expr.lower().keys());
            rewrite_exchange_search_keys(info, expr.upper().keys());
        }
    }

    void operator()(relation::project const& expr, buffer_type& available_columns) {
        expand(available_columns, expr.columns().size());
        for (auto&& column : expr.columns()) {
            available_columns.emplace_back(column.variable());
        }
    }

    void operator()(relation::filter const&, buffer_type&) {
        // just through all columns
    }

    void operator()(relation::buffer const&, buffer_type&) {
        // just through all columns
    }

    void operator()(relation::identify const& expr, buffer_type& available_columns) {
        expand(available_columns, 1);
        available_columns.emplace_back(expr.variable());
    }

    void operator()(relation::emit const&, buffer_type& available_columns) {
        // no more succeeding operations
        available_columns.clear();
    }

    void operator()(relation::write const&, buffer_type& available_columns) {
        // no more succeeding operations
        available_columns.clear();
    }

    void operator()(relation::values const& expr, buffer_type& available_columns) {
        expand(available_columns, expr.columns().size());
        for (auto&& column : expr.columns()) {
            available_columns.emplace_back(column);
        }
    }

    void operator()(relation::intermediate::escape const& expr, buffer_type& available_columns) {
        /* NOTE:
         * escape is not a step plan operator, but it will be required in later process.
         * Two variables come from the different paths potentially have the same source,
         * then the escape operator will achieve distinguishing them.
         */
        available_columns.clear();
        append_destination_columns(expr.mappings(), available_columns);
    }

    void operator()(relation::step::join const& expr, buffer_type& available_columns) {
        // pick upstream co-group setting
        auto kind = expr.operator_kind();
        if (kind == relation::join_kind::semi || kind == relation::join_kind::anti) {
            // left semi/anti join only flow columns in the left most input
            shrink_to_first_group(expr, available_columns);
        }
    }

    void operator()(relation::step::aggregate const& expr, buffer_type& available_columns) {
        available_columns.clear();
        auto&& take = get_upstream<relation::step::take_group>(expr);
        auto&& exchange = binding::extract<plan::exchange>(take.source());
        if (exchange.kind() != plan::group::tag) {
            throw_exception(std::domain_error(string_builder {}
                    << "inconsistent take_group: "
                    << expr
                    << " <=> "
                    << exchange.kind()
                    << string_builder::to_string));
        }
        auto&& group = unsafe_downcast<plan::group>(exchange);

        available_columns.reserve(group.group_keys().size() + expr.columns().size());

        // downstream only can access group keys and aggregation results
        for (auto&& group_key : group.group_keys()) {
            auto&& columns = take.columns();
            auto it = std::find_if(columns.begin(), columns.end(), [&](auto& column) {
                return column.source() == group_key;
            });
            if (it != columns.end()) {
                available_columns.emplace_back(it->destination());
            }
        }
        append_destination_columns(expr.columns(), available_columns);
    }

    void operator()(relation::step::intersection const& expr, buffer_type& available_columns) {
        shrink_to_first_group(expr, available_columns);
    }

    void operator()(relation::step::difference const& expr, buffer_type& available_columns) {
        shrink_to_first_group(expr, available_columns);
    }

    void operator()(relation::step::flatten const&, buffer_type&) {
        // just through all columns
    }

    void operator()(relation::step::take_flat& expr, buffer_type& available_columns) {
        BOOST_ASSERT(available_columns.empty()); // NOLINT
        analyze_take_like(expr, available_columns);
    }

    void operator()(relation::step::take_group& expr, buffer_type& available_columns) {
        BOOST_ASSERT(available_columns.empty()); // NOLINT
        analyze_take_like(expr, available_columns);
    }

    void operator()(relation::step::take_cogroup& expr, buffer_type& available_columns) {
        BOOST_ASSERT(available_columns.empty()); // NOLINT
        expand(available_columns, std::accumulate(
                expr.groups().begin(),
                expr.groups().end(),
                std::size_t { 0 },
                [](std::size_t left, auto& right) {
                    return left + right.columns().size();
                }));
        for (auto&& group : expr.groups()) {
            analyze_take_like(group, available_columns);
        }
    }

    void operator()(relation::step::offer& expr, buffer_type& available_columns) {
        auto&& info = exchange_map_.create_or_get(expr.destination());
        auto&& columns = expr.columns();
        if (columns.empty()) {
            // no columns are defined, then we offer all available columns in the context
            columns.reserve(available_columns.size());
            for (auto&& variable : available_columns) {
                auto allocated = info.allocate(variable);
                columns.emplace_back(std::move(variable), std::move(allocated));
            }
        } else {
            // columns already exist, but the destinations are still stream variables
            for (auto&& column : columns) {
                BOOST_ASSERT(std::find( // NOLINT
                        available_columns.begin(),
                        available_columns.end(),
                        column.source()) != available_columns.end());
                auto allocated = info.allocate(column.destination());
                column.destination() = std::move(allocated);
            }
        }
        // no more downstream operators
        available_columns.clear();
    }

    // --- exchange steps

    void operator()(::takatori::plan::forward& step) {
        auto&& info = exchange_map_.get(step);
        if (step.columns().empty()) {
            fill_exchange_columns(info, step.columns());
        } else {
            rebuild_exchange_columns(info, step.columns());
        }
    }

    void operator()(::takatori::plan::group& step) {
        auto&& info = exchange_map_.get(step);
        if (step.columns().empty()) {
            fill_exchange_columns(info, step.columns());
        } else {
            rebuild_exchange_columns(info, step.columns());
        }
        rewrite_exchange_columns(info, step.group_keys());
        rewrite_exchange_sort_keys(info, step.sort_keys());
    }

    void operator()(::takatori::plan::aggregate& step) {
        auto&& info = exchange_map_.get(step);

        // propagate source columns
        if (step.source_columns().empty()) {
            auto&& columns = step.source_columns();
            columns.reserve(info.count());
            info.each_mapping([&](descriptor::variable const&, descriptor::variable const& exchange_column) {
                columns.emplace_back(exchange_column);
            });
        } else {
            // NOTE: we just invoke rewrite_ instead of rebuild_,
            // because this exchange will rebuild it later by manually
            rewrite_exchange_columns(info, step.source_columns());
        }

        // rewrite group keys
        mapping_buffer_.reserve(step.group_keys().size() + step.aggregations().size());
        for (auto&& key : step.group_keys()) {
            auto origin = key;
            rewrite_exchange_column(info, key);
            mapping_buffer_.emplace_back(std::move(origin), key);
        }

        // rewrite aggregations
        for (auto&& aggregation : step.aggregations()) {
            rewrite_exchange_columns(info, aggregation.arguments()); // not destination
            {
                auto origin = aggregation.destination();
                auto replacement = info.allocate(aggregation.destination());
                aggregation.destination() = replacement;
                mapping_buffer_.emplace_back(std::move(origin), std::move(replacement));
            }
        }

        // make source columns invisible, because aggregate has different source/destination columns
        info.clear();
        for (auto&& mapping : mapping_buffer_) {
            info.bind(std::move(mapping.source()), std::move(mapping.destination()));
        }
        mapping_buffer_.clear();

        // compute destination
        if (step.destination_columns().empty()) {
            fill_exchange_columns(info, step.destination_columns());
        } else {
            rewrite_exchange_columns(info, step.destination_columns());
        }
    }

    void operator()(::takatori::plan::broadcast& step) {
        auto&& info = exchange_map_.get(step);
        if (step.columns().empty()) {
            fill_exchange_columns(info, step.columns());
        } else {
            rebuild_exchange_columns(info, step.columns());
        }
    }

    void operator()(::takatori::plan::discard const&) {
        // nothing to do
    }

private:
    exchange_column_info_map& exchange_map_;
    buffer_pool_type& buffer_pool_;
    std::vector<relation::details::mapping_element> mapping_buffer_;

    void scan(relation::expression* current, buffer_type& available_columns) {
        while (true) {
            // analyze the expr
            if (relation::is_available_in_step_plan(current->kind())) {
                relation::step::dispatch(*this, *current, available_columns);
            } else if (current->kind() == relation::expression_kind::escape) {
                // special case: escape operator may retain the current phase
                std::invoke(*this, unsafe_downcast<relation::intermediate::escape>(*current), available_columns);
            } else {
                throw_exception(std::domain_error(string_builder {}
                        << "unsupported operator is still retained: "
                        << *current
                        << string_builder::to_string));
            }

            auto outputs = current->output_ports();
            if (outputs.empty()) {
                break;
            }
            // visit to downstreams excepts the last one
            for (std::size_t i = 0, n = outputs.size() - 1; i < n; ++i) {
                auto&& next_expr = outputs[i].opposite()->owner();
                auto next_available_columns = buffer_pool_.acquire();
                next_available_columns = available_columns;
                scan(std::addressof(next_expr), next_available_columns);
                next_available_columns.clear();
                buffer_pool_.release(std::move(next_available_columns));
            }
            // visit to the last one
            auto&& next_expr = outputs.back().opposite()->owner();
            current = std::addressof(next_expr);
        }
    }

    template<class Columns>
    void fill_exchange_columns(exchange_column_info const& info, Columns& columns) {
        BOOST_ASSERT(columns.empty()); // NOLINT
        columns.reserve(info.count());
        info.each_mapping([&](descriptor::variable const&, descriptor::variable const& exchange_column) {
            columns.emplace_back(exchange_column);
        });
    }

    template<class Columns>
    void rewrite_exchange_columns(exchange_column_info const& info, Columns& columns) {
        for (auto&& column : columns) {
            rewrite_exchange_column(info, column);
        }
    }

    template<class Keys>
    void rewrite_exchange_search_keys(exchange_column_info const& info, Keys& keys) {
        for (auto&& key : keys) {
            rewrite_exchange_column(info, key.variable());
        }
    }

    template<class Keys>
    void rewrite_exchange_sort_keys(exchange_column_info const& info, Keys& keys) {
        for (auto&& key : keys) {
            rewrite_exchange_column(info, key.variable());
        }
    }

    template<class Columns>
    void rebuild_exchange_columns(exchange_column_info& info, Columns& columns) {
        BOOST_ASSERT(mapping_buffer_.empty()); // NOLINT
        mapping_buffer_.reserve(columns.size());
        for (auto&& column : columns) {
            auto origin = column;
            rewrite_exchange_column(info, column);
            mapping_buffer_.emplace_back(std::move(origin), column);
        }

        // clear info and rebuild from the scratch
        info.clear();
        for (auto&& column : mapping_buffer_) {
            info.bind(std::move(column.source()), std::move(column.destination()));
        }
        mapping_buffer_.clear();
    }

    template<class Expr>
    void analyze_read_like(Expr const& expr, buffer_type& available_columns) {
        BOOST_ASSERT(available_columns.empty()); // NOLINT
        append_destination_columns(expr.columns(), available_columns);
    }

    template<class Columns>
    void append_destination_columns(Columns const& columns, buffer_type& available_columns) const {
        expand(available_columns, columns.size());
        for (auto&& column : columns) {
            available_columns.emplace_back(column.destination());
        }
    }

    template<class Columns>
    void fill_source_exchange_columns(descriptor::relation const& desc, Columns& columns) const {
        BOOST_ASSERT(columns.empty()); // NOLINT
        auto&& info = exchange_map_.get(desc);
        info.each_mapping([&](descriptor::variable const& origin, descriptor::variable const& exchange_column) {
            columns.emplace_back(exchange_column, origin);
        });
    }

    template<class Expr>
    void analyze_join_read_like(Expr& expr, buffer_type& available_columns) {
        if (is_exchange(expr.source())) {
            fill_source_exchange_columns(expr.source(), expr.columns());
        }
        auto kind = expr.operator_kind();
        if (kind != relation::join_kind::semi && kind != relation::join_kind::anti) {
            append_destination_columns(expr.columns(), available_columns);
        }
    }

    template<class Channel>
    void analyze_take_like(Channel& channel, buffer_type& available_columns) {
        BOOST_ASSERT(is_exchange(channel.source())); // NOLINT
        fill_source_exchange_columns(channel.source(), channel.columns());
        append_destination_columns(channel.columns(), available_columns);
    }

    template<class Expr>
    void shrink_to_first_group(Expr const& expr, buffer_type& available_columns) {
        auto&& take = get_upstream<relation::step::take_cogroup>(expr);
        auto first_group_size = take.groups().front().columns().size();
        available_columns.erase(available_columns.begin() + first_group_size, available_columns.end());
    }
};

} // namespace

exchange_column_info_map collect_exchange_columns(::takatori::plan::graph_type& graph) {
    exchange_column_info_map exchange_map {};
    buffer_pool_type buffer_pool {};
    engine e { exchange_map, buffer_pool };
    plan::sort_from_upstream(
            graph,
            [&](plan::step& step) { plan::dispatch(e, step); });
    return exchange_map;
}

} // namespace yugawara::analyzer::details

#include "collect_exchange_steps.h"

#include <algorithm>
#include <memory>
#include <optional>

#include <tsl/hopscotch_map.h>
#include <tsl/hopscotch_set.h>

#include <takatori/scalar/variable_reference.h>
#include <takatori/scalar/binary.h>
#include <takatori/scalar/compare.h>

#include <takatori/relation/emit.h>

#include <takatori/relation/intermediate/dispatch.h>

#include <takatori/relation/step/join.h>
#include <takatori/relation/step/aggregate.h>
#include <takatori/relation/step/intersection.h>
#include <takatori/relation/step/difference.h>

#include <takatori/relation/step/take_flat.h>
#include <takatori/relation/step/take_group.h>
#include <takatori/relation/step/take_cogroup.h>
#include <takatori/relation/step/flatten.h>
#include <takatori/relation/step/offer.h>

#include <takatori/plan/forward.h>
#include <takatori/plan/group.h>
#include <takatori/plan/aggregate.h>
#include <takatori/plan/broadcast.h>

#include <takatori/util/assertion.h>
#include <takatori/util/downcast.h>
#include <takatori/util/exception.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/string_builder.h>

#include <yugawara/binding/factory.h>
#include <yugawara/extension/relation/fence.h>

#include "detect_join_endpoint_style.h"
#include "row_slice_utils.h"
#include "path_capability.h"
#include "ordered_relation.h"

namespace yugawara::analyzer::details {

namespace {

namespace descriptor = ::takatori::descriptor;
namespace scalar = ::takatori::scalar;
namespace relation = ::takatori::relation;
namespace plan = ::takatori::plan;

using ::takatori::util::optional_ptr;
using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;
using ::takatori::util::unsafe_downcast;

optional_ptr<descriptor::variable> extract_if_variable_ref(scalar::expression& expr) {
    if (expr.kind() == scalar::variable_reference::tag) {
        return unsafe_downcast<scalar::variable_reference>(expr).variable();
    }
    return {};
}

bool is_empty(relation::intermediate::limit const& expr) {
    return expr.group_keys().empty()
            && expr.sort_keys().empty()
            && !expr.row_slice();
}

class engine {
private:
    template<class T>
    [[nodiscard]] auto to_descriptor(T&& decl) const noexcept {
        return binding::factory {}(std::forward<T>(decl));
    }

public:
    explicit engine(
            relation::graph_type& source,
            plan::graph_type& destination,
            step_plan_builder_options const& options) noexcept:
        source_ { source },
        destination_ { destination },
        options_ { options },
        capability_options_ { make_path_capability_options(options.runtime_features()) }
    {}

    void operator()() {
        // NOTE: decide how to rewrite each operator before rewriting,
        // because rewriting may break the upstream/downstream of the operators
        std::vector<relation::expression*> sorted {};
        relation::sort_from_upstream(source_, [&](relation::expression& expr) {
            sorted.emplace_back(std::addressof(expr));
        });
        for (auto* expr : sorted) {
            decide(*expr);
        }
        for (auto* expr : targets_) {
            relation::intermediate::dispatch(*this, *expr);
        }
        targets_.clear();
        for (auto&& p : added_) {
            source_.insert(std::move(p));
        }
        added_.clear();
    }

    void operator()(relation::expression& expr) {
        throw_exception(std::domain_error(string_builder {}
                << "other than step plan operator will remain: "
                << expr
                << string_builder::to_string));
    }

    void operator()(relation::intermediate::extension& expr) {
        if (expr.extension_id() == extension::relation::fence::extension_tag) {
            auto&& fence = unsafe_downcast<extension::relation::fence>(expr);
            if (redundant_fences_.contains(std::addressof(fence))) {
                remove_fence(fence);
                return;
            }
            process_fence(fence);
            return;
        }
        // fallback to the default behavior for unknown extensions
        operator()(static_cast<relation::expression&>(expr));
    }

    void operator()(relation::intermediate::join& expr) {
        if (!options_.runtime_features().contains(runtime_feature::broadcast_exchange)) {
            process_cogroup_join(expr);
            return;
        }
        using kind = join_info::strategy_type;
        if (auto info = options_.find(expr)) {
            switch (info->strategy()) {
                case kind::cogroup:
                    process_cogroup_join(expr);
                    return;
                case kind::broadcast:
                    process_broadcast_join(expr);
                    return;
            }
            std::abort();
        }
        auto availables = available_join_strategies(expr);
        if (availables.contains(kind::cogroup)) {
            process_cogroup_join(expr);
        } else if (availables.contains(kind::broadcast)) {
            process_broadcast_join(expr);
        } else {
            process_cogroup_join(expr);
        }
    }

    void operator()(relation::intermediate::aggregate& expr) {
        shrink_duplicated_variables(expr.group_keys());

        if (!options_.runtime_features().contains(runtime_feature::aggregate_exchange)) {
            process_aggregate_group(expr);
            return;
        }
        using kind = aggregate_info::strategy_type;
        if (auto info = options_.find(expr)) {
            switch (info->strategy()) {
                case kind::group:
                    process_aggregate_group(expr);
                    return;
                case kind::exchange:
                    process_aggregate_exchange(expr);
                    return;
            }
            std::abort();
        }
        auto availables = available_aggregate_strategies(expr);
        if (availables.contains(kind::exchange)) {
            process_aggregate_exchange(expr);
        } else {
            process_aggregate_group(expr);
        }
    }

    void operator()(relation::intermediate::distinct& expr) {
        shrink_duplicated_variables(expr.group_keys());

        /*
         * .. - distinct_relation{k} - ..
         * =>
         * .. - offer - [group{k, limit=1}] - take_group - flatten - ..
         */
        auto&& exchange = destination_.emplace<plan::group>(
                empty<descriptor::variable>(),
                std::move(expr.group_keys()),
                empty<plan::group::sort_key>(),
                plan::group::row_slice_type { {}, 1 },
                plan::group_mode::equivalence);
        auto&& offer = add_offer(exchange);

        auto&& take = add<relation::step::take_group>(
                to_descriptor(exchange),
                empty<relation::step::take_group::column>());
        auto&& replacement = add<relation::step::flatten>();
        take.output() >> replacement.input();

        migrate(expr.input(), offer.input());
        migrate(expr.output(), replacement.output());

        source_.erase(expr);
    }

    void operator()(relation::intermediate::limit& expr) {
        if (redundant_limits_.contains(std::addressof(expr))) {
            remove_limit(expr);
            return;
        }
        shrink_duplicated_variables(expr.group_keys());

        if (auto iter = ordered_limits_.find(std::addressof(expr)); iter != ordered_limits_.end()) {
            process_limit_flat(expr, iter->second);
        } else if (expr.group_keys().empty() && expr.sort_keys().empty()) {
            process_limit_flat(expr, {});
        } else {
            process_limit_group(expr);
        }
    }

    void operator()(relation::intermediate::union_& expr) {
        if (expr.quantifier() == relation::set_quantifier::all) {
            process_union_all(expr);
        } else {
            process_union_distinct(expr);
        }
    }

    void operator()(relation::intermediate::intersection& expr) {
        if (expr.quantifier() == relation::set_quantifier::all) {
            process_binary_group_all<relation::step::intersection>(expr);
        } else {
            process_binary_group_distinct(expr, relation::join_kind::semi);
        }
    }

    void operator()(relation::intermediate::difference& expr) {
        if (expr.quantifier() == relation::set_quantifier::all) {
            process_binary_group_all<relation::step::difference>(expr);
        } else {
            process_binary_group_distinct(expr, relation::join_kind::anti);
        }
    }

private:
    relation::graph_type& source_;
    plan::graph_type& destination_;
    step_plan_builder_options const& options_;
    path_capability_options capability_options_;

    std::vector<relation::expression*> targets_;
    std::vector<std::unique_ptr<relation::expression>> added_;
    ::tsl::hopscotch_map<relation::intermediate::limit const*, plan::forward::ordering_type> ordered_limits_ {};
    ::tsl::hopscotch_set<relation::intermediate::limit const*> redundant_limits_ {};
    ::tsl::hopscotch_set<extension::relation::fence const*> redundant_fences_ {};

    template<class T>
    [[nodiscard]] std::vector<T> empty() const noexcept {
        std::vector<T> result {};
        return result;
    }

    template<class T, class... Args>
    T& add(Args&&... args) {
        auto&& p = std::make_unique<T>(std::forward<Args>(args)...);
        auto&& r = *p;
        added_.emplace_back(std::move(p));
        return r;
    }

    relation::step::offer& add_offer(plan::exchange const& declaration) {
        return add<relation::step::offer>(
                to_descriptor(declaration),
                empty<relation::step::offer::column>());
    }

    template<class Port>
    static void migrate(Port& source, Port& destination) {
        if (!source.opposite()) {
            throw_exception(std::domain_error(string_builder {}
                    << "source port must have opposite: "
                    << source
                    << string_builder::to_string));
        }
        if (destination.opposite()) {
            throw_exception(std::domain_error(string_builder {}
                    << "destination port must be orphaned: "
                    << destination
                    << string_builder::to_string));
        }
        auto&& opposite = *source.opposite();
        source.disconnect_from(opposite);
        destination.connect_to(opposite);
    }

    void process_cogroup_join(relation::intermediate::join& expr) {
        auto style = detect_join_endpoint_style(expr);
        if (style != join_endpoint_style::nothing && style != join_endpoint_style::key_pairs) {
            throw_exception(std::domain_error(string_builder {}
                    << "range information must be absent for co-group based join: "
                    << expr
                    << string_builder::to_string));
        }
        BOOST_ASSERT(expr.lower().keys().size() == expr.upper().keys().size()); // NOLINT

        /*
         * .. -\
         *      join_relation{k, c} - ..
         * .. -/
         * =>
         * .. - offer - [group{k}] -\
         *                           take_cogroup{k} - join_group{c} - ..
         * .. - offer - [group{k}] -/
         */
        auto&& keys = expr.lower().keys();
        auto left_group_keys = empty<descriptor::variable>();
        auto right_group_keys = empty<descriptor::variable>();
        left_group_keys.reserve(keys.size());
        right_group_keys.reserve(keys.size());

        auto condition = expr.release_condition();
        for (auto&& key : keys) {
            auto&& left = *extract_if_variable_ref(key.value());
            auto&& right = key.variable();
            if (key.semantics() == ::takatori::relation::comparison_semantics_kind::ternary) {
                // repair NULL-aware comparison
                auto cmp = std::make_unique<::takatori::scalar::compare>(
                        ::takatori::scalar::comparison_operator::equal,
                        std::make_unique<::takatori::scalar::variable_reference>(left),
                        std::make_unique<::takatori::scalar::variable_reference>(right));
                if (condition == nullptr) {
                    condition = std::move(cmp);
                } else {
                    condition = std::make_unique<::takatori::scalar::binary>(
                            ::takatori::scalar::binary_operator::conditional_and,
                            std::move(condition),
                            std::move(cmp));
                }
            }
            left_group_keys.emplace_back(std::move(left));
            right_group_keys.emplace_back(std::move(right));
        }
        auto&& left_exchange = destination_.emplace<plan::group>(
                empty<descriptor::variable>(),
                std::move(left_group_keys),
                empty<plan::group::sort_key>(),
                plan::group::row_slice_type {},
                plan::group_mode::equivalence);
        auto&& left_offer = add_offer(left_exchange);

        auto&& right_exchange = destination_.emplace<plan::group>(
                empty<descriptor::variable>(),
                std::move(right_group_keys),
                empty<plan::group::sort_key>(),
                plan::group::row_slice_type {},
                plan::group_mode::equivalence);
        auto&& right_offer = add_offer(right_exchange);

        auto groups = empty<relation::step::take_cogroup::group>();
        groups.reserve(2);
        groups.emplace_back(
                to_descriptor(left_exchange),
                empty<relation::step::take_cogroup::group::column>(),
                is_left_mandatory(expr.operator_kind()));
        groups.emplace_back(
                to_descriptor(right_exchange),
                empty<relation::step::take_cogroup::group::column>(),
                is_right_mandatory(expr.operator_kind()));

        auto&& take = add<relation::step::take_cogroup>(std::move(groups));
        auto&& replacement = add<relation::step::join>(
                expr.operator_kind(),
                std::move(condition));
        take.output() >> replacement.input();

        migrate(expr.left(), left_offer.input());
        migrate(expr.right(), right_offer.input());
        migrate(expr.output(), replacement.output());

        source_.erase(expr);
    }

    void process_broadcast_join(relation::intermediate::join& expr) {
        auto style = detect_join_endpoint_style(expr);
        if (style == join_endpoint_style::key_pairs || style == join_endpoint_style::prefix) {
            process_broadcast_join_find(expr);
        } else {
            process_broadcast_join_scan(expr);
        }
    }

    void process_broadcast_join_find(relation::intermediate::join& expr) {
        BOOST_ASSERT(expr.lower().keys().size() == expr.upper().keys().size()); // NOLINT
        /*
         *  (left) .. -\
         *              join_relation{k, c} - ..
         * (right) .. -/
         * =>
         *                          (left) .. join_find{k, c} - ...
         *                                    /
         * (right) .. - offer - [broadcast] -/
         */
        auto&& keys = expr.lower().keys();

        auto join_key = empty<relation::join_find::key>();
        join_key.reserve(keys.size());
        for (auto&& key : keys) {
            join_key.emplace_back(std::move(key.variable()), key.release_value());
        }

        auto&& exchange = destination_.emplace<plan::broadcast>();
        auto&& offer = add_offer(exchange);

        auto&& replacement = add<relation::join_find>(
                expr.operator_kind(),
                to_descriptor(exchange),
                empty<relation::join_find::column>(),
                std::move(join_key),
                expr.release_condition());

        migrate(expr.left(), replacement.left());
        migrate(expr.right(), offer.input());
        migrate(expr.output(), replacement.output());

        source_.erase(expr);
    }

    void process_broadcast_join_scan(relation::intermediate::join& expr) {
        /*
         *  (left) .. -\
         *              join_relation{k, c} - ..
         * (right) .. -/
         * =>
         *                          (left) .. join_scan{k, c} - ...
         *                                    /
         * (right) .. - offer - [broadcast] -/
         */
        /*
         *  (left) .. -\
         *              join_relation{k, c} - ..
         * (right) .. -/
         * =>
         *                          (left) .. join_find - ...
         *                                    /
         * (right) .. - offer - [broadcast] -/
         */
        auto&& exchange = destination_.emplace<plan::broadcast>();
        auto&& offer = add_offer(exchange);

        auto&& replacement = add<relation::join_scan>(
                expr.operator_kind(),
                to_descriptor(exchange),
                empty<relation::join_scan::column>(),
                migrate_endpoint<relation::join_scan::endpoint>(std::move(expr.lower())),
                migrate_endpoint<relation::join_scan::endpoint>(std::move(expr.upper())),
                expr.release_condition());

        migrate(expr.left(), replacement.left());
        migrate(expr.right(), offer.input());
        migrate(expr.output(), replacement.output());

        source_.erase(expr);
    }

    template<class T, class Endpoint>
    T migrate_endpoint(Endpoint&& endpoint) {
        static_assert(!std::is_reference_v<Endpoint>);
        T destination {};
        destination.keys().reserve(endpoint.keys().size());
        for (auto&& key : endpoint.keys()) {
            destination.keys().emplace_back(std::move(key.variable()), key.release_value());
        }
        destination.kind(endpoint.kind());
        return destination;
    }

    /*
     * NOTE: is_left_mandatory() and is_right_mandatory() decide the `mandatory` flag of each co-group input
     * of the cogroup join (take_cogroup + join_group).
     * The co-group row is never generated for the join key if the mandatory group is empty for the key
     * (see takatori::relation::details::cogroup_element), so that the group can be mandatory only if
     * the join outputs nothing for the keys where the group is empty.
     * Making a group non-mandatory is always safe (the join just receives the extra co-group rows),
     * but making it mandatory by mistake drops the rows which the join must output.
     *
     * - inner, semi: both groups are mandatory
     * - left_outer, left_outer_at_most_one, anti: only the left group is mandatory
     * - full_outer: neither group is mandatory
     *
     * e.g. the anti join outputs the left rows whose key has no right rows, so that the right group
     * must not be mandatory.
     */

    static bool is_left_mandatory(relation::join_kind k) {
        static constexpr relation::join_kind_set left_mandatory {
                relation::join_kind::inner,
                relation::join_kind::left_outer,
                relation::join_kind::left_outer_at_most_one,
                relation::join_kind::semi,
                relation::join_kind::anti,
        };
        return left_mandatory.contains(k);
    }

    static bool is_right_mandatory(relation::join_kind k) {
        static constexpr relation::join_kind_set right_mandatory {
                relation::join_kind::inner,
                relation::join_kind::semi,
        };
        return right_mandatory.contains(k);
    }

    void shrink_duplicated_variables(std::vector<descriptor::variable>& variables) {
        if (variables.empty()) {
            return;
        }
        ::tsl::hopscotch_set<descriptor::variable::entity_type*> saw {};
        saw.reserve(variables.size());
        for (auto iter = variables.begin(); iter != variables.end();) {
            auto key_ptr = iter->shared_entity().get();
            auto [inserted, result] = saw.insert(key_ptr);
            (void) inserted;
            if (result) {
                ++iter;
            } else {
                iter = variables.erase(iter);
            }
        }
    }

    void process_aggregate_exchange(relation::intermediate::aggregate& expr) {
        /*
         * .. - aggregate_relation{k, f} - ..
         * =>
         * .. - offer - [aggregate{k, f}] - take_group - flatten - ..
         */
        auto&& exchange = destination_.emplace<plan::aggregate>(
                empty<descriptor::variable>(),
                empty<descriptor::variable>(),
                std::move(expr.group_keys()),
                std::move(expr.columns()),
                plan::group_mode::equivalence_or_whole);
        auto&& offer = add_offer(exchange);

        auto&& take = add<relation::step::take_group>(
                to_descriptor(exchange),
                empty<relation::step::take_group::column>());
        auto&& replacement = add<relation::step::flatten>();
        take.output() >> replacement.input();

        migrate(expr.input(), offer.input());
        migrate(expr.output(), replacement.output());

        source_.erase(expr);
    }

    void process_aggregate_group(relation::intermediate::aggregate& expr) {
        /*
         * .. - aggregate_relation{k, f} - ..
         * =>
         * .. - offer - group{k} - take_group - aggregate_group{f} - ..
         */
        auto&& exchange = destination_.emplace<plan::group>(
                empty<descriptor::variable>(),
                std::move(expr.group_keys()),
                empty<plan::group::sort_key>(),
                plan::group::row_slice_type {},
                plan::group_mode::equivalence_or_whole);
        auto&& offer = add_offer(exchange);

        auto&& take = add<relation::step::take_group>(
                to_descriptor(exchange),
                empty<relation::step::take_group::column>());
        auto&& replacement = add<relation::step::aggregate>(
                std::move(expr.columns()));
        take.output() >> replacement.input();

        migrate(expr.input(), offer.input());
        migrate(expr.output(), replacement.output());

        source_.erase(expr);
    }

    /**
     * @brief decides how to rewrite the given operator, and then schedules it into targets_ if it must be rewritten.
     * @details This must be called for each operator in order from upstream to downstream,
     *      before rewriting any operators.
     *      Unknown operators are also scheduled, and they will be rejected on rewriting.
     * @param expr the target operator
     */
    void decide(relation::expression& expr) {
        switch (expr.kind()) {
            case relation::intermediate::limit::tag:
                decide_limit(unsafe_downcast<relation::intermediate::limit>(expr));
                targets_.emplace_back(std::addressof(expr));
                return;
            case relation::intermediate::escape::tag:
                // NOTE: keep escape until stream_variable_rewriter was finished, to distinguish the same rooted variables
                return;
            case relation::emit::tag:
                decide_emit(unsafe_downcast<relation::emit>(expr));
                // NOTE: emit only controls `preserve_output` of the upstream limits, and is never rewritten itself
                return;
            case relation::intermediate::extension::tag:
                decide_extension(unsafe_downcast<relation::intermediate::extension>(expr));
                targets_.emplace_back(std::addressof(expr));
                return;
            default:
                if (!relation::is_available_in_step_plan(expr.kind())) {
                    targets_.emplace_back(std::addressof(expr));
                }
                return;
        }
    }

    /**
     * @brief decides whether the given limit is removed, or becomes a forward exchange with ordering.
     * @details First, this adjusts the adjacent upstream limit so that it also satisfies the given limit
     *      (see merge_into_upstream()).
     *      The limit is removed if it is redundant (see is_redundant()), and then it is registered into
     *      redundant_limits_.
     *      Otherwise, the limit can be replaced with a forward exchange only if its input rows are already in the order
     *      of its sort keys, that is, the source scan has the direction which provides the rows in the order.
     *      The source scan may be in the upstream of other limits which become forward exchanges with ordering,
     *      and then the nearest one must preserve the order of its output rows (see trace_ordered_upstream()).
     *      If this limit becomes a forward exchange with ordering, it is registered into ordered_limits_.
     *      The ordering of the exchange may be extended later by its downstream (see require_preserve_output()).
     * @param expr the target limit
     */
    void decide_limit(relation::intermediate::limit& expr) {
        // NOTE: the limit always becomes redundant if the upstream limit is adjusted
        if (merge_into_upstream(expr) || is_redundant(expr)) {
            redundant_limits_.emplace(std::addressof(expr));
            return;
        }
        if (!options_.runtime_features().contains(runtime_feature::ordered_forward)) {
            return;
        }
        if (!expr.group_keys().empty() || expr.sort_keys().empty()) {
            return;
        }
        auto upstream = trace_ordered_upstream(expr.input());
        // NOTE: the scan direction is available only if it provides the rows in the order of the limit
        if (!upstream.scan) {
            return;
        }
        auto&& scan = *upstream.scan;
        if (!scan.direction() || find_scan_direction(scan, expr) != scan.direction()) {
            return;
        }
        // NOTE: the input order decides which rows are in the slice
        ordered_limits_.emplace(
                std::addressof(expr),
                plan::forward::ordering_type { plan::forward_ordering::respect_input });
        if (upstream.limit) {
            // NOTE: this limit depends on the order of the rows from the upstream limit
            require_preserve_output(*upstream.limit);
        }
    }

    /**
     * @brief adjusts the adjacent upstream limit so that it also satisfies the given limit.
     * @details The upstream limit has the longer sort keys of them, and the smaller row count of them.
     *      Then, the given limit becomes redundant (see is_redundant()).
     *      This only adjusts the upstream if both do not have any group keys nor the start position,
     *      their row counts can be merged (see merge_row_slice()), and the sort keys of either one are
     *      a prefix of the other ones.
     *      If the upstream sort keys become longer, it also requires that the upstream has one or more sort keys,
     *      and it does not become a forward exchange with ordering.
     *      Otherwise, the upstream may become a group exchange only for the longer sort keys,
     *      or it may not provide the rows in the order of the longer sort keys.
     *      This must be called after all upstream operators are decided (see decide()).
     * @param expr the downstream limit
     * @return true if the upstream limit is adjusted, and then the given limit is redundant
     * @return false otherwise
     */
    bool merge_into_upstream(relation::intermediate::limit& expr) {
        if (!expr.group_keys().empty()) {
            return false;
        }
        auto found = find_adjacent_upstream_limit(expr);
        if (!found) {
            return false;
        }
        auto&& upstream = *found;
        if (!upstream.group_keys().empty()) {
            return false;
        }
        // NOTE: this also rejects the row slices with the effective start position
        auto slice = merge_row_slice(upstream.row_slice(), expr.row_slice());
        if (!slice) {
            return false;
        }
        auto&& upstream_keys = upstream.sort_keys();
        auto&& keys = expr.sort_keys();
        if (keys.size() <= upstream_keys.size()) {
            // NOTE: the top rows of the upstream limit are also the top rows of the downstream one
            if (!std::equal(keys.begin(), keys.end(), upstream_keys.begin())) {
                return false;
            }
        } else {
            // NOTE: the upstream limit which becomes a forward exchange depends on the row order of its input
            if (upstream_keys.empty() || ordered_limits_.contains(std::addressof(upstream))) {
                return false;
            }
            // NOTE: the top rows in the order of the downstream sort keys are also the top rows of the upstream one
            if (!std::equal(upstream_keys.begin(), upstream_keys.end(), keys.begin())) {
                return false;
            }
            upstream.sort_keys() = keys;
        }
        upstream.row_slice() = std::move(*slice);
        BOOST_ASSERT(is_redundant(expr)); // NOLINT
        return true;
    }

    /**
     * @brief returns the adjacent upstream limit of the given limit, except the redundant ones.
     * @param expr the downstream limit
     * @return the adjacent upstream limit
     * @return empty if there is no such the limit
     */
    [[nodiscard]] optional_ptr<relation::intermediate::limit> find_adjacent_upstream_limit(
            relation::intermediate::limit& expr) const {
        auto upstream = expr.input().opposite();
        while (upstream && upstream->owner().kind() == relation::intermediate::limit::tag) {
            auto&& limit = unsafe_downcast<relation::intermediate::limit>(upstream->owner());
            // NOTE: the redundant limits will be removed, and their input rows are directly passed to the downstream
            if (!redundant_limits_.contains(std::addressof(limit))) {
                return limit;
            }
            upstream = limit.input().opposite();
        }
        return {};
    }

    /**
     * @brief returns whether or not the given limit is redundant, that is, it never changes its input rows.
     * @details This returns `true` if the limit is empty, or the nearest upstream `scan` or `limit` (except the
     *      redundant ones) already provides the rows which satisfy the given limit.
     *      This walks the upstream through the operators which preserve the row order, as same as
     *      trace_ordered_upstream().
     *      This must be called after all upstream operators are decided (see decide()).
     * @param expr the target limit
     * @return true if the limit is redundant
     * @return false otherwise
     */
    [[nodiscard]] bool is_redundant(relation::intermediate::limit& expr) const {
        if (is_empty(expr)) {
            return true;
        }
        // FIXME: currently we do not remove the limit with the effective start position (other than immediate `0`)
        if (!expr.group_keys().empty() || has_start(expr.row_slice())) {
            return false;
        }
        auto path = find_ordered_source(expr, capability_options_);
        // NOTE: the redundant limits never change their input rows
        while (path.end && path.end->kind() == relation::intermediate::limit::tag) {
            auto&& limit = unsafe_downcast<relation::intermediate::limit>(*path.end);
            if (!redundant_limits_.contains(std::addressof(limit))) {
                return satisfies_count(limit, expr, path.capabilities)
                        && satisfies_order(limit, expr);
            }
            auto capabilities = path.capabilities;
            path = find_ordered_source(limit, capability_options_);
            path.capabilities &= capabilities;
        }
        if (path.end && path.end->kind() == relation::scan::tag) {
            auto&& scan = unsafe_downcast<relation::scan>(*path.end);
            return satisfies_count(scan, expr, path.capabilities)
                    && satisfies_order(scan, expr);
        }
        return false;
    }

    [[nodiscard]] bool satisfies_count(
            relation::scan const& scan,
            relation::intermediate::limit const& expr,
            path_capability_set const& capabilities) const {
        // NOTE: the given limit must not have any group keys nor the start position (see is_redundant())
        BOOST_ASSERT(expr.group_keys().empty() && !has_start(expr.row_slice())); // NOLINT
        auto&& count = expr.row_slice().count();
        if (count.is_absent()) {
            return true;
        }
        // NOTE: the rows from the scan must not be increased before the limit
        if (!capabilities.contains(path_capability::at_most_one)) {
            return false;
        }
        // NOTE: the scan must honor its row count
        if (!options_.runtime_features().contains(runtime_feature::scan_limit)) {
            return false;
        }
        auto&& slice = scan.row_slice();
        // FIXME: currently we do not consider the scan with the effective start position (other than immediate `0`)
        if (has_start(slice)) {
            return false;
        }
        // FIXME: currently we do not remove the limit with the variable count, even if the scan has the same one
        auto scan_count = slice.count().immediate_if();
        auto limit_count = count.immediate_if();
        return scan_count && limit_count && *scan_count <= *limit_count;
    }

    [[nodiscard]] bool satisfies_order(relation::scan const& scan, relation::intermediate::limit const& expr) const {
        // NOTE: the given limit must not have any group keys nor the start position (see is_redundant())
        BOOST_ASSERT(expr.group_keys().empty() && !has_start(expr.row_slice())); // NOLINT
        if (expr.sort_keys().empty()) {
            return true;
        }
        // NOTE: the scan must provide an ordered relation in its direction
        if (!options_.runtime_features().contains(runtime_feature::ordered_scan)) {
            return false;
        }
        auto&& direction = scan.direction();
        return direction && find_scan_direction(scan, expr) == direction;
    }

    [[nodiscard]] static bool satisfies_count(
            relation::intermediate::limit const& upstream,
            relation::intermediate::limit const& expr,
            path_capability_set const& capabilities) {
        // NOTE: the given limit must not have any group keys nor the start position (see is_redundant())
        BOOST_ASSERT(expr.group_keys().empty() && !has_start(expr.row_slice())); // NOLINT
        auto&& count = expr.row_slice().count();
        if (count.is_absent()) {
            return true;
        }
        // NOTE: the rows from the upstream limit must not be increased before the limit
        if (!capabilities.contains(path_capability::at_most_one)) {
            return false;
        }
        auto&& slice = upstream.row_slice();
        // FIXME: currently we do not consider the upstream with the effective start position (other than immediate `0`)
        if (!upstream.group_keys().empty() || has_start(slice)) {
            return false;
        }
        // NOTE: also accepts the same variables
        if (slice.count() == count) {
            return true;
        }
        // FIXME: currently we do not compare the different variable counts, or the variable and the immediate ones
        auto upstream_count = slice.count().immediate_if();
        auto limit_count = count.immediate_if();
        return upstream_count && limit_count && *upstream_count <= *limit_count;
    }

    [[nodiscard]] static bool satisfies_order(
            relation::intermediate::limit const& upstream,
            relation::intermediate::limit const& expr) {
        // NOTE: the given limit must not have any group keys nor the start position (see is_redundant())
        BOOST_ASSERT(expr.group_keys().empty() && !has_start(expr.row_slice())); // NOLINT
        // NOTE: the upstream limit provides the rows in the order of its sort keys,
        // and they are also in the order of the given limit only if its sort keys are a prefix of the upstream ones
        auto&& upstream_keys = upstream.sort_keys();
        auto&& keys = expr.sort_keys();
        return upstream.group_keys().empty()
                && keys.size() <= upstream_keys.size()
                && std::equal(keys.begin(), keys.end(), upstream_keys.begin());
    }

    /**
     * @brief decides whether or not the given emit depends on the row order of the upstream limit.
     * @details If the emit requires to preserve the order of its input rows, the nearest upstream limit which
     *      becomes a forward exchange with ordering must preserve the order of its output rows.
     * @param expr the target emit
     */
    void decide_emit(relation::emit& expr) {
        if (!expr.ordering().contains(relation::emit_ordering::preserve_output)) {
            return;
        }
        auto upstream = trace_ordered_upstream(expr.input());
        if (upstream.limit) {
            require_preserve_output(*upstream.limit);
        }
    }

    /**
     * @brief the result of trace_ordered_upstream().
     */
    struct ordered_upstream {
        /// @brief the source scan of the row order, or empty if it is not found.
        optional_ptr<relation::scan> scan {};

        /// @brief the nearest upstream limit which becomes a forward exchange with ordering, or empty if it is absent.
        optional_ptr<relation::intermediate::limit> limit {};
    };

    /**
     * @brief traces the upstream from the given input port, through the operators which preserve the row order.
     * @details This passes through the upstream limits which become forward exchanges with ordering
     *      (that is, they are registered in ordered_limits_), because they can preserve the order of their input rows.
     *      This also passes through the redundant limits (that is, they are registered in redundant_limits_),
     *      because they will be removed.
     *      This stops at the other limits, because the path capabilities of limits are always empty
     *      (see find_path_capabilities()).
     *      This must be called after all upstream operators are decided (see decide()).
     * @param input the input port where the trace starts
     * @return the source scan and the nearest upstream limit with ordering
     */
    [[nodiscard]] ordered_upstream trace_ordered_upstream(relation::expression::input_port_type& input) {
        ordered_upstream result {};
        auto path = find_ordered_source(input, capability_options_);
        while (path.end && path.end->kind() == relation::intermediate::limit::tag) {
            auto&& limit = unsafe_downcast<relation::intermediate::limit>(*path.end);
            // NOTE: the redundant limits will be removed, and their input rows are directly passed to the downstream
            if (!redundant_limits_.contains(std::addressof(limit))) {
                if (!ordered_limits_.contains(std::addressof(limit))) {
                    return result;
                }
                if (!result.limit) {
                    result.limit = limit;
                }
            }
            path = find_ordered_source(limit, capability_options_);
        }
        if (path.end && path.end->kind() == relation::scan::tag) {
            result.scan = unsafe_downcast<relation::scan>(*path.end);
        }
        return result;
    }

    /**
     * @brief makes the forward exchange of the given limit preserve the order of its output rows.
     * @param expr the limit which becomes a forward exchange with ordering
     */
    void require_preserve_output(relation::intermediate::limit const& expr) {
        auto iter = ordered_limits_.find(std::addressof(expr));
        BOOST_ASSERT(iter != ordered_limits_.end()); // NOLINT
        iter.value().insert(plan::forward_ordering::preserve_output);
    }

    /**
     * @brief decides how to rewrite the given extension operator.
     * @details A fence becomes a forward exchange, or is just removed if its upstream is also a fence,
     *      because the consecutive fences must be merged into a single exchange.
     * @param expr the target extension operator
     */
    void decide_extension(relation::intermediate::extension const& expr) {
        if (expr.extension_id() != extension::relation::fence::extension_tag) {
            return;
        }
        auto&& fence = unsafe_downcast<extension::relation::fence>(expr);
        if (auto upstream = fence.input().opposite()) {
            auto&& owner = upstream->owner();
            if (owner.kind() == relation::intermediate::extension::tag
                    && unsafe_downcast<relation::intermediate::extension>(owner).extension_id()
                            == extension::relation::fence::extension_tag) {
                redundant_fences_.emplace(std::addressof(fence));
            }
        }
    }

    void process_fence(extension::relation::fence& expr) {
        /*
         * .. - fence - ..
         * =>
         * .. - offer - [forward] - take_flat - ..
         */
        auto&& exchange = destination_.emplace<plan::forward>(plan::forward::row_slice_type {});
        auto&& offer = add_offer(exchange);

        auto&& take = add<relation::step::take_flat>(
                to_descriptor(exchange),
                empty<relation::step::take_flat::column>());

        migrate(expr.input(), offer.input());
        migrate(expr.output(), take.output());

        source_.erase(expr);
    }

    void remove_fence(extension::relation::fence& expr) {
        /*
         * .. - fence - fence{redundant} - ..
         * =>
         * .. - fence - ..
         */
        auto upstream = expr.input().opposite();
        auto downstream = expr.output().opposite();
        if (!upstream || !downstream) {
            throw_exception(std::domain_error(string_builder {}
                    << "disconnected fence: "
                    << expr
                    << string_builder::to_string));
        }
        upstream->reconnect_to(*downstream);
        source_.erase(expr);
    }

    void remove_limit(relation::intermediate::limit& expr) {
        /*
         * .. - limit_relation{...} - ..
         * =>
         * .. - ..
         */
        auto upstream = expr.input().opposite();
        auto downstream = expr.output().opposite();
        if (!upstream || !downstream) {
            throw_exception(std::domain_error(string_builder {}
                    << "disconnected limit: "
                    << expr
                    << string_builder::to_string));
        }
        upstream->reconnect_to(*downstream);
        source_.erase(expr);
    }

    void process_limit_flat(relation::intermediate::limit& expr, plan::forward::ordering_type ordering) {
        /*
         * .. - limit_relation{count=N, group=(), sort=()} - ..
         * =>
         * .. - offer -  [forward{limit=N}] - take_flat - ..
         *
         * or, if the input rows are already in the order of the sort keys:
         * .. - limit_relation{count=N, group=(), sort=S} - ..
         * =>
         * .. - offer -  [forward{limit=N, ordering=O}] - take_flat - ..
         */
        auto&& exchange = destination_.emplace<plan::forward>(std::move(expr.row_slice()), ordering);
        auto&& offer = add_offer(exchange);

        auto&& take = add<relation::step::take_flat>(
                to_descriptor(exchange),
                empty<relation::step::take_flat::column>());

        migrate(expr.input(), offer.input());
        migrate(expr.output(), take.output());

        source_.erase(expr);
    }

    void process_limit_group(relation::intermediate::limit& expr) {
        /*
         * .. - limit_relation{count=N, group=G, sort=S} - ..
         * =>
         * .. - offer - [group{group=G, sort=S, limit=N}] - take_group - flatten - ..
         */
        auto&& exchange = destination_.emplace<plan::group>(
                empty<descriptor::variable>(),
                std::move(expr.group_keys()),
                std::move(expr.sort_keys()),
                std::move(expr.row_slice()),
                plan::group_mode::equivalence);
        auto&& offer = add_offer(exchange);

        auto&& take = add<relation::step::take_group>(
                to_descriptor(exchange),
                empty<relation::step::take_group::column>());
        auto&& replacement = add<relation::step::flatten>();
        take.output() >> replacement.input();

        migrate(expr.input(), offer.input());
        migrate(expr.output(), replacement.output());

        source_.erase(expr);
    }

    void process_union_all(relation::intermediate::union_& expr) {
        /*
         * .. -\
         *      union_relation{all} - ..
         * .. -/
         * =>
         * .. - offer -\
         *              [forward] - take_flat - ..
         * .. - offer -/
         */
        auto left_columns = empty<relation::step::offer::column>();
        auto right_columns = empty<relation::step::offer::column>();
        auto outputs = empty<descriptor::variable>();
        left_columns.reserve(expr.mappings().size());
        right_columns.reserve(expr.mappings().size());
        outputs.reserve(expr.mappings().size());
        for (auto&& mapping : expr.mappings()) {
            // NOTE: fills offer columns temporary, but the destinations are still stream variables
            if (mapping.left()) {
                left_columns.emplace_back(mapping.left().value(), mapping.destination());
            }
            if (mapping.right()) {
                right_columns.emplace_back(mapping.right().value(), mapping.destination());
            }
            outputs.emplace_back(mapping.destination());
        }
        auto&& exchange = destination_.emplace<plan::forward>(
                outputs,
                plan::forward::row_slice_type {});
        auto&& left_offer = add<relation::step::offer>(
                to_descriptor(exchange),
                std::move(left_columns));
        auto&& right_offer = add<relation::step::offer>(
                to_descriptor(exchange),
                std::move(right_columns));

        auto&& take = add<relation::step::take_flat>(
                to_descriptor(exchange),
                empty<relation::step::take_group::column>());

        migrate(expr.left(), left_offer.input());
        migrate(expr.right(), right_offer.input());
        migrate(expr.output(), take.output());

        source_.erase(expr);
    }

    void process_union_distinct(relation::intermediate::union_& expr) {
        /*
         * .. -\
         *      union_relation{k, distinct} - ..
         * .. -/
         * =>
         * .. - offer -\
         *              [group{k, limit=1}] - take_group - flatten - ..
         * .. - offer -/
         */
        auto left_columns = empty<relation::step::offer::column>();
        auto right_columns = empty<relation::step::offer::column>();
        auto outputs = empty<descriptor::variable>();
        left_columns.reserve(expr.mappings().size());
        right_columns.reserve(expr.mappings().size());
        outputs.reserve(expr.mappings().size());
        for (auto&& mapping : expr.mappings()) {
            if (!mapping.left() || !mapping.right()) {
                throw_exception(std::domain_error(string_builder {}
                        << "union_relation with distinct must not have any empty input columns: "
                        << expr
                        << string_builder::to_string));
            }
            left_columns.emplace_back(mapping.left().value(), mapping.destination());
            right_columns.emplace_back(mapping.right().value(), mapping.destination());
            outputs.emplace_back(mapping.destination());
        }
        auto&& exchange = destination_.emplace<plan::group>(
                outputs,
                outputs, // all columns are group key
                empty<plan::group::sort_key>(),
                plan::group::row_slice_type { {}, 1 },
                plan::group_mode::equivalence);
        auto&& left_offer = add<relation::step::offer>(
                to_descriptor(exchange),
                std::move(left_columns));
        auto&& right_offer = add<relation::step::offer>(
                to_descriptor(exchange),
                std::move(right_columns));

        auto&& take = add<relation::step::take_group>(
                to_descriptor(exchange),
                empty<relation::step::take_group::column>()); // FIXME: empty?
        auto&& replacement = add<relation::step::flatten>();
        take.output() >> replacement.input();

        migrate(expr.left(), left_offer.input());
        migrate(expr.right(), right_offer.input());
        migrate(expr.output(), replacement.output());

        source_.erase(expr);
    }

    template<class Step, class Intermediate>
    void process_binary_group_all(Intermediate& expr) {
        /*
         * .. -\
         *      <OP>_relation{k, q} - ..
         * .. -/
         * =>
         * .. - offer - [group{k, limit=n}] -\
         *                                    take_cogroup{k} - <OP>_group{c} - ..
         * .. - offer - [group{k, limit=n}] -/
         */
        auto left_group_key = empty<descriptor::variable>();
        auto right_group_key = empty<descriptor::variable>();
        left_group_key.reserve(expr.group_key_pairs().size());
        right_group_key.reserve(expr.group_key_pairs().size());
        for (auto&& pair : expr.group_key_pairs()) {
            left_group_key.emplace_back(pair.left());
            right_group_key.emplace_back(pair.right());
        }

        plan::group::row_slice_type row_slice {};
        if (expr.quantifier() == relation::set_quantifier::distinct) {
            row_slice.count() = 1;
        }
        auto&& left_exchange = destination_.emplace<plan::group>(
                empty<descriptor::variable>(),
                std::move(left_group_key),
                empty<plan::group::sort_key>(),
                row_slice,
                plan::group_mode::equivalence);
        auto&& left_offer = add_offer(left_exchange);

        auto&& right_exchange = destination_.emplace<plan::group>(
                empty<descriptor::variable>(),
                std::move(right_group_key),
                empty<plan::group::sort_key>(),
                std::move(row_slice),
                plan::group_mode::equivalence);
        auto&& right_offer = add_offer(right_exchange);

        auto groups = empty<relation::step::take_cogroup::group>();
        groups.reserve(2);
        groups.emplace_back(
                to_descriptor(left_exchange),
                empty<relation::step::take_cogroup::group::column>(),
                true);
        groups.emplace_back(
                to_descriptor(right_exchange),
                empty<relation::step::take_cogroup::group::column>(),
                false);

        auto&& take = add<relation::step::take_cogroup>(std::move(groups));
        auto&& replacement = add<Step>();
        take.output() >> replacement.input();

        migrate(expr.left(), left_offer.input());
        migrate(expr.right(), right_offer.input());
        migrate(expr.output(), replacement.output());

        source_.erase(expr);
    }

    template<class Intermediate>
    void process_binary_group_distinct(Intermediate& expr, relation::join_kind join_kind) {
        /*
         * .. -\
         *      <OP>_relation{k, q} - ..
         * .. -/
         * =>
         * .. - offer - [group{k, limit=n}] -\
         *                                    take_cogroup{k} - join[semi/anti]{c} - ..
         * .. - offer - [group{k, limit=n}] -/
         */
        auto left_group_key = empty<descriptor::variable>();
        auto right_group_key = empty<descriptor::variable>();
        left_group_key.reserve(expr.group_key_pairs().size());
        right_group_key.reserve(expr.group_key_pairs().size());
        for (auto&& pair : expr.group_key_pairs()) {
            left_group_key.emplace_back(pair.left());
            right_group_key.emplace_back(pair.right());
        }

        plan::group::row_slice_type row_slice {};
        if (expr.quantifier() == relation::set_quantifier::distinct) {
            row_slice.count() = 1;
        }
        auto&& left_exchange = destination_.emplace<plan::group>(
                empty<descriptor::variable>(),
                std::move(left_group_key),
                empty<plan::group::sort_key>(),
                row_slice,
                plan::group_mode::equivalence);
        auto&& left_offer = add_offer(left_exchange);

        auto&& right_exchange = destination_.emplace<plan::group>(
                empty<descriptor::variable>(),
                std::move(right_group_key),
                empty<plan::group::sort_key>(),
                std::move(row_slice),
                plan::group_mode::equivalence);
        auto&& right_offer = add_offer(right_exchange);

        auto groups = empty<relation::step::take_cogroup::group>();
        groups.reserve(2);
        groups.emplace_back(
                to_descriptor(left_exchange),
                empty<relation::step::take_cogroup::group::column>(),
                true);
        groups.emplace_back(
                to_descriptor(right_exchange),
                empty<relation::step::take_cogroup::group::column>(),
                false);

        auto&& take = add<relation::step::take_cogroup>(std::move(groups));
        auto&& replacement = add<relation::step::join>(join_kind);
        take.output() >> replacement.input();

        migrate(expr.left(), left_offer.input());
        migrate(expr.right(), right_offer.input());
        migrate(expr.output(), replacement.output());

        source_.erase(expr);
    }
};

} // namespace

void collect_exchange_steps(relation::graph_type& source, plan::graph_type& destination,
        step_plan_builder_options const& options) {
    engine e { source, destination, options };
    e();
}

} // namespace yugawara::analyzer::details

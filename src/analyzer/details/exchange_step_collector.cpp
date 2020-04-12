#include "exchange_step_collector.h"

#include <stdexcept>

#include <takatori/scalar/variable_reference.h>

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

#include <takatori/util/downcast.h>
#include <takatori/util/enum_tag.h>
#include <takatori/util/string_builder.h>
#include <yugawara/binding/factory.h>

namespace yugawara::analyzer::details {

namespace {

namespace descriptor = ::takatori::descriptor;
namespace scalar = ::takatori::scalar;
namespace relation = ::takatori::relation;
namespace plan = ::takatori::plan;

using ::takatori::util::string_builder;

class engine {
private:
    template<class T>
    auto to_descriptor(T&& decl) const noexcept {
        return binding::factory { get_object_creator() }(std::forward<T>(decl));
    }

public:
    explicit engine(
            relation::graph_type& source,
            plan::graph_type& destination,
            step_planning_info const& info) noexcept
        : source_(source)
        , destination_(destination)
        , info_(info)
        , cursor_(source_.begin())
        , added_(info.get_object_creator().template allocator<decltype(added_)::value_type>())
    {}

    void operator()() {
        while (cursor_ != source_.end()) {
            relation::intermediate::dispatch(*this, *cursor_);
        }
        for (auto&& p : added_) {
            source_.insert(std::move(p));
        }
        added_.clear();
    }

    void operator()(relation::expression& expr) {
        if (!relation::is_available_in_step_plan(expr.kind())) {
            throw std::domain_error(string_builder {}
                    << "other than step plan operator will remain: "
                    << expr
                    << string_builder::to_string);
        }
        ++cursor_;
    }

    void operator()(relation::intermediate::join& expr) {
        if (auto info = info_.find(expr)) {
            using kind = join_info::strategy_type;
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
        process_cogroup_join(expr);
    }

    void operator()(relation::intermediate::aggregate& expr) {
        if (is_incremental(expr)) {
            process_aggregate_incremental(expr);
        } else {
            process_aggregate_group(expr);
        }
    }

    void operator()(relation::intermediate::distinct& expr) {
        /*
         * .. - distinct_relation{k} - ..
         * =>
         * .. - offer - [group{k, limit=1}] - take_group - flatten - ..
         */
        auto&& exchange = destination_.emplace<plan::group>(
                empty<descriptor::variable>(),
                std::move(expr.group_keys()),
                empty<plan::group::sort_key>(),
                1,
                plan::group_mode::equivalence,
                get_object_creator());
        auto&& offer = add_offer(exchange);

        auto&& take = add<relation::step::take_group>(
                to_descriptor(exchange),
                empty<relation::step::take_group::column>(),
                get_object_creator());
        auto&& replacement = add<relation::step::flatten>();
        take.output() >> replacement.input();

        migrate(expr.input(), offer.input());
        migrate(expr.output(), replacement.output());

        cursor_ = source_.erase(cursor_);
    }

    void operator()(relation::intermediate::limit& expr) {
        if (expr.group_keys().empty() && expr.sort_keys().empty()) {
            process_limit_flat(expr);
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
        process_binary_group<relation::step::intersection>(expr);
    }

    void operator()(relation::intermediate::difference& expr) {
        process_binary_group<relation::step::difference>(expr);
    }

    void operator()(relation::intermediate::escape&) {
        // NOTE: keep escape until stream_variable_rewriter was finished, to distinguish the same rooted variables
        ++cursor_;
    }

    ::takatori::util::object_creator get_object_creator() const noexcept {
        return info_.get_object_creator();
    }

private:
    relation::graph_type& source_;
    plan::graph_type& destination_;
    step_planning_info const& info_;

    relation::graph_type::iterator cursor_;
    std::vector<
            ::takatori::util::unique_object_ptr<relation::expression>,
            ::takatori::util::object_allocator<::takatori::util::unique_object_ptr<relation::expression>>> added_;

    template<class T>
    std::vector<T, ::takatori::util::object_allocator<T>> empty() const noexcept {
        std::vector<T, ::takatori::util::object_allocator<T>> result { get_object_creator().allocator<T>() };
        return result;
    }

    template<class T, class... Args>
    T& add(Args&&... args) {
        // NOTE: must use the source relational operator graph's object creator, to avoid clone
        auto&& p = source_.get_object_creator().create_unique<T>(std::forward<Args>(args)...);
        auto&& r = *p;
        added_.emplace_back(std::move(p));
        return r;
    }

    relation::step::offer& add_offer(plan::exchange const& declaration) {
        return add<relation::step::offer>(
                to_descriptor(declaration),
                empty<relation::step::offer::column>(),
                get_object_creator());
    }

    template<class Port>
    static void migrate(Port& source, Port& destination) {
        if (!source.opposite()) {
            throw std::domain_error(string_builder {}
                    << "source port must have opposite: "
                    << source
                    << string_builder::to_string);
        }
        if (destination.opposite()) {
            throw std::domain_error(string_builder {}
                    << "destination port must be orphaned: "
                    << destination
                    << string_builder::to_string);
        }
        auto&& opposite = *source.opposite();
        source.disconnect_from(opposite);
        destination.connect_to(opposite);
    }

    void process_cogroup_join(relation::intermediate::join& expr) {
        if (expr.lower() || expr.upper()) {
            throw std::domain_error(string_builder {}
                    << "range information must be absent for co-group based join: "
                    << expr
                    << string_builder::to_string);
        }
        /*
         * .. -\
         *      join_relation{k, c} - ..
         * .. -/
         * =>
         * .. - offer - [group{k}] -\
         *                           take_cogroup{k} - join_group{c} - ..
         * .. - offer - [group{k}] -/
         */
        auto left_group_keys = empty<descriptor::variable>();
        auto right_group_keys = empty<descriptor::variable>();
        left_group_keys.reserve(expr.key_pairs().size());
        right_group_keys.reserve(expr.key_pairs().size());
        for (auto&& key_pair : expr.key_pairs()) {
            left_group_keys.emplace_back(std::move(key_pair.left()));
            right_group_keys.emplace_back(std::move(key_pair.right()));
        }
        auto&& left_exchange = destination_.emplace<plan::group>(
                empty<descriptor::variable>(),
                std::move(left_group_keys),
                empty<plan::group::sort_key>(),
                std::nullopt,
                plan::group_mode::equivalence,
                get_object_creator());
        auto&& left_offer = add_offer(left_exchange);

        auto&& right_exchange = destination_.emplace<plan::group>(
                empty<descriptor::variable>(),
                std::move(right_group_keys),
                empty<plan::group::sort_key>(),
                std::nullopt,
                plan::group_mode::equivalence,
                get_object_creator());
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

        auto&& take = add<relation::step::take_cogroup>(std::move(groups), get_object_creator());
        auto&& replacement = add<relation::step::join>(
                expr.operator_kind(),
                expr.release_condition(),
                get_object_creator());
        take.output() >> replacement.input();

        migrate(expr.left(), left_offer.input());
        migrate(expr.right(), right_offer.input());
        migrate(expr.output(), replacement.output());

        cursor_ = source_.erase(cursor_);
    }

    void process_broadcast_join(relation::intermediate::join& expr) {
        if (expr.lower() || expr.upper()) {
            process_broadcast_join_scan(expr);
        } else {
            process_broadcast_join_find(expr);
        }
    }

    void process_broadcast_join_find(relation::intermediate::join& expr) {
        assert(!expr.lower() && !expr.upper()); // NOLINT
        /*
         *  (left) .. -\
         *              join_relation{k, c} - ..
         * (right) .. -/
         * =>
         *                          (left) .. join_find{k, c} - ...
         *                                    /
         * (right) .. - offer - [broadcast] -/
         */
        auto creator = get_object_creator();

        auto join_key = empty<relation::join_find::key>();
        join_key.reserve(expr.key_pairs().size());
        for (auto&& key_pair : expr.key_pairs()) {
            // NOTE: search_key_element must be a column on the external relation
            join_key.emplace_back(
                    creator.create_unique<scalar::variable_reference>(std::move(key_pair.left())),
                    std::move(key_pair.right()));
        }

        auto&& exchange = destination_.emplace<plan::broadcast>();
        auto&& offer = add_offer(exchange);

        auto&& replacement = add<relation::join_find>(
                expr.operator_kind(),
                to_descriptor(exchange),
                empty<relation::join_find::column>(),
                std::move(join_key),
                expr.release_condition(),
                creator);

        migrate(expr.left(), replacement.left());
        migrate(expr.right(), offer.input());
        migrate(expr.output(), replacement.output());

        cursor_ = source_.erase(cursor_);
    }

    void process_broadcast_join_scan(relation::intermediate::join& expr) {
        if (!expr.key_pairs().empty()) {
            throw std::domain_error(string_builder {}
                    << "equi join key pairs must be absent for broadcast based join: "
                    << expr
                    << string_builder::to_string);
        }
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
        auto creator = get_object_creator();

        auto&& exchange = destination_.emplace<plan::broadcast>();
        auto&& offer = add_offer(exchange);

        auto&& replacement = add<relation::join_scan>(
                expr.operator_kind(),
                to_descriptor(exchange),
                empty<relation::join_scan::column>(),
                migrate_endpoint<relation::join_scan::endpoint>(std::move(expr.lower()), creator),
                migrate_endpoint<relation::join_scan::endpoint>(std::move(expr.upper()), creator),
                expr.release_condition(),
                creator);

        migrate(expr.left(), replacement.left());
        migrate(expr.right(), offer.input());
        migrate(expr.output(), replacement.output());

        cursor_ = source_.erase(cursor_);
    }

    template<class T, class Endpoint>
    T migrate_endpoint(Endpoint&& endpoint, ::takatori::util::object_creator creator) {
        static_assert(!std::is_reference_v<Endpoint>);
        T destination { creator };
        destination.keys().reserve(endpoint.keys().size());
        for (auto&& key : endpoint.keys()) {
            destination.keys().emplace_back(std::move(key.variable()), key.release_value());
        }
        destination.kind(endpoint.kind());
        return destination;
    }

    static bool is_left_mandatory(relation::join_kind k) noexcept {
        static relation::join_kind_set const mandatories {
                relation::join_kind::inner,
                relation::join_kind::left_outer,
                relation::join_kind::semi,
                relation::join_kind::anti,
        };
        return mandatories.contains(k);
    }

    static bool is_right_mandatory(relation::join_kind k) noexcept {
        static relation::join_kind_set const mandatories {
                relation::join_kind::inner,
                relation::join_kind::semi,
                relation::join_kind::anti,
        };
        return mandatories.contains(k);
    }

    void process_aggregate_incremental(relation::intermediate::aggregate& expr) {
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
                plan::group_mode::equivalence_or_whole,
                get_object_creator());
        auto&& offer = add_offer(exchange);

        auto&& take = add<relation::step::take_group>(
                to_descriptor(exchange),
                empty<relation::step::take_group::column>(),
                get_object_creator());
        auto&& replacement = add<relation::step::flatten>();
        take.output() >> replacement.input();

        migrate(expr.input(), offer.input());
        migrate(expr.output(), replacement.output());

        cursor_ = source_.erase(cursor_);
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
                std::nullopt,
                plan::group_mode::equivalence_or_whole,
                get_object_creator());
        auto&& offer = add_offer(exchange);

        auto&& take = add<relation::step::take_group>(
                to_descriptor(exchange),
                empty<relation::step::take_group::column>(),
                get_object_creator());
        auto&& replacement = add<relation::step::aggregate>(
                std::move(expr.columns()),
                get_object_creator());
        take.output() >> replacement.input();

        migrate(expr.input(), offer.input());
        migrate(expr.output(), replacement.output());

        cursor_ = source_.erase(cursor_);
    }

    bool is_incremental(relation::intermediate::aggregate const& expr) const {
        if (auto info = info_.find(expr)) {
            return info->incremental();
        }
        return aggregate_info::default_incremental;
    }

    void process_limit_flat(relation::intermediate::limit& expr) {
        /*
         * .. - limit_relation{count=N, group=(), sort=()} - ..
         * =>
         * .. - offer -  [forward{limit=N}] - take_flat - ..
         */
        auto&& exchange = destination_.emplace<plan::forward>(expr.count(), get_object_creator());
        auto&& offer = add_offer(exchange);

        auto&& take = add<relation::step::take_flat>(
                to_descriptor(exchange),
                empty<relation::step::take_flat::column>(),
                get_object_creator());

        migrate(expr.input(), offer.input());
        migrate(expr.output(), take.output());

        cursor_ = source_.erase(cursor_);
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
                expr.count(),
                plan::group_mode::equivalence,
                get_object_creator());
        auto&& offer = add_offer(exchange);

        auto&& take = add<relation::step::take_group>(
                to_descriptor(exchange),
                empty<relation::step::take_group::column>(),
                get_object_creator());
        auto&& replacement = add<relation::step::flatten>();
        take.output() >> replacement.input();

        migrate(expr.input(), offer.input());
        migrate(expr.output(), replacement.output());

        cursor_ = source_.erase(cursor_);
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
                std::nullopt,
                get_object_creator());
        auto&& left_offer = add<relation::step::offer>(
                to_descriptor(exchange),
                std::move(left_columns),
                get_object_creator());
        auto&& right_offer = add<relation::step::offer>(
                to_descriptor(exchange),
                std::move(right_columns),
                get_object_creator());

        auto&& take = add<relation::step::take_flat>(
                to_descriptor(exchange),
                empty<relation::step::take_group::column>(),
                get_object_creator());

        migrate(expr.left(), left_offer.input());
        migrate(expr.right(), right_offer.input());
        migrate(expr.output(), take.output());

        cursor_ = source_.erase(cursor_);
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
                throw std::domain_error(string_builder {}
                        << "union_relation with distinct must not have any empty input columns: "
                        << expr
                        << string_builder::to_string);
            }
            left_columns.emplace_back(mapping.left().value(), mapping.destination());
            right_columns.emplace_back(mapping.right().value(), mapping.destination());
            outputs.emplace_back(mapping.destination());
        }
        auto&& exchange = destination_.emplace<plan::group>(
                outputs,
                outputs, // all columns are group key
                empty<plan::group::sort_key>(),
                1,
                plan::group_mode::equivalence,
                get_object_creator());
        auto&& left_offer = add<relation::step::offer>(
                to_descriptor(exchange),
                std::move(left_columns),
                get_object_creator());
        auto&& right_offer = add<relation::step::offer>(
                to_descriptor(exchange),
                std::move(right_columns),
                get_object_creator());

        auto&& take = add<relation::step::take_group>(
                to_descriptor(exchange),
                empty<relation::step::take_group::column>(), // FIXME: empty?
                get_object_creator());
        auto&& replacement = add<relation::step::flatten>();
        take.output() >> replacement.input();

        migrate(expr.left(), left_offer.input());
        migrate(expr.right(), right_offer.input());
        migrate(expr.output(), replacement.output());

        cursor_ = source_.erase(cursor_);
    }

    template<class Step, class Intermediate>
    void process_binary_group(Intermediate& expr) {
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

        std::optional<plan::group::size_type> limit {};
        if (expr.quantifier() == relation::set_quantifier::distinct) {
            limit.emplace(1);
        }
        auto&& left_exchange = destination_.emplace<plan::group>(
                empty<descriptor::variable>(),
                std::move(left_group_key),
                empty<plan::group::sort_key>(),
                limit,
                plan::group_mode::equivalence,
                get_object_creator());
        auto&& left_offer = add_offer(left_exchange);

        auto&& right_exchange = destination_.emplace<plan::group>(
                empty<descriptor::variable>(),
                std::move(right_group_key),
                empty<plan::group::sort_key>(),
                std::move(limit),
                plan::group_mode::equivalence,
                get_object_creator());
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

        auto&& take = add<relation::step::take_cogroup>(std::move(groups), get_object_creator());
        auto&& replacement = add<Step>(get_object_creator());
        take.output() >> replacement.input();

        migrate(expr.left(), left_offer.input());
        migrate(expr.right(), right_offer.input());
        migrate(expr.output(), replacement.output());

        cursor_ = source_.erase(cursor_);
    }
};

} // namespace

void collect_exchange_steps(relation::graph_type& source, plan::graph_type& destination,
        step_planning_info const& info) {
    engine e { source, destination, info };
    e();
}

} // namespace yugawara::analyzer::details

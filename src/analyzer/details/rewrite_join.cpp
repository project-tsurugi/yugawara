#include "rewrite_join.h"

#include <optional>

#include <cassert>

#include <takatori/scalar/binary.h>

#include <takatori/relation/scan.h>
#include <takatori/relation/join_scan.h>
#include <takatori/relation/join_find.h>
#include <takatori/relation/intermediate/join.h>

#include <takatori/relation/filter.h>

#include <takatori/util/clonable.h>
#include <takatori/util/downcast.h>
#include <takatori/util/optional_ptr.h>

#include <yugawara/binding/factory.h>

#include "boolean_constants.h"
#include "scan_key_collector.h"
#include "rewrite_criteria.h"

namespace yugawara::analyzer::details {

namespace scalar = ::takatori::scalar;
namespace relation = ::takatori::relation;

using search_key = storage::details::search_key_element;
using attribute = details::index_estimator_result_attribute;

using ::takatori::relation::join_kind;
using ::takatori::relation::join_kind_set;
using ::takatori::util::clone_unique;
using ::takatori::util::object_allocator;
using ::takatori::util::object_creator;
using ::takatori::util::object_ownership_reference;
using ::takatori::util::optional_ptr;
using ::takatori::util::sequence_view;
using ::takatori::util::unique_object_ptr;
using ::takatori::util::unsafe_downcast;

using expression_ref = object_ownership_reference<scalar::expression>;

template<class T>
using object_vector = std::vector<T, object_allocator<T>>;

namespace {

class engine {
public:
    explicit engine(
            relation::graph_type& graph,
            storage::provider const& storage_provider,
            index_estimator& index_estimator,
            object_creator creator)
        : graph_(graph)
        , storage_provider_(storage_provider)
        , index_estimator_(index_estimator)
        , collector_(creator)
        , term_buf_(creator.allocator())
        , search_key_buf_(creator.allocator())
        , filter_buf_(creator.allocator())
        , saved_terms_(creator.allocator())
        , saved_filters_(creator.allocator())
    {}

    bool process(relation::intermediate::join& expr) {
        process_right(expr);
        process_left(expr);
        bool erase = false;
        if (saved_scan_) {
            auto condition = apply_result(expr, *saved_scan_, *saved_index_, saved_left_, saved_terms_);
            merge_filters(std::move(condition), saved_filters_);
            erase = true;
        }

        // clear the round data
        collector_.clear();
        term_buf_.clear();
        search_key_buf_.clear();
        saved_scan_ = {};
        saved_index_ = {};
        saved_left_ = {};
        saved_result_ = {};
        saved_terms_.clear();
        saved_filters_.clear();

        return erase;
    }

    [[nodiscard]] ::takatori::util::object_creator get_object_creator() const noexcept {
        return collector_.get_object_creator();
    }

private:
    relation::graph_type& graph_;
    storage::provider const& storage_provider_;
    index_estimator& index_estimator_;
    scan_key_collector collector_;

    object_vector<scan_key_collector::term*> term_buf_;
    object_vector<search_key> search_key_buf_;
    object_vector<relation::filter*> filter_buf_;

    optional_ptr<relation::scan> saved_scan_ {};
    optional_ptr<storage::index const> saved_index_ {};
    bool saved_left_ {};
    std::optional<index_estimator::result> saved_result_ {};
    object_vector<scan_key_collector::term*> saved_terms_;
    object_vector<relation::filter*> saved_filters_;

    void process_left(relation::intermediate::join& expr) {
        static constexpr join_kind_set allow {
                join_kind::inner,
        };
        if (!allow.contains(expr.operator_kind())) {
            return;
        }
        auto scan = find_scan(expr.left(), false);
        if (!scan || !collector_(*scan, true)) {
            return;
        }
        storage_provider_.each_table_index(
                collector_.table(),
                [&](std::string_view, std::shared_ptr<storage::index const> const& entry) {
                    estimate(*scan, *entry, true);
                });
        filter_buf_.clear();
    }

    void process_right(relation::intermediate::join& expr) {
        static constexpr join_kind_set allow {
                join_kind::inner,
                join_kind::semi,
                join_kind::anti,
                join_kind::left_outer,
        };
        static constexpr join_kind_set direct {
                join_kind::anti,
                join_kind::left_outer,
        };
        if (!allow.contains(expr.operator_kind())) {
            return;
        }
        auto scan = find_scan(expr.right(), direct.contains(expr.operator_kind()));
        if (!scan || !collector_(*scan, true)) {
            return;
        }
        storage_provider_.each_table_index(
                collector_.table(),
                [&](std::string_view, std::shared_ptr<storage::index const> const& entry) {
                    estimate(*scan, *entry, false);
                });
        filter_buf_.clear();
    }

    optional_ptr<relation::scan> find_scan(relation::expression::input_port_type& input, bool direct) {
        assert(filter_buf_.empty()); // NOLINT
        auto current = input.opposite();
        while (true) {
            assert(current); // NOLINT
            auto&& expr = current->owner();
            using kind = relation::expression_kind;
            switch (expr.kind()) {
                case kind::scan: {
                    auto&& result = unsafe_downcast<relation::scan>(expr);
                    if (result.lower() || result.upper() || result.limit()) {
                        return {};
                    }
                    return result;
                }

                case kind::filter: {
                    if (direct) {
                        return {};
                    }
                    auto&& filter = unsafe_downcast<relation::filter>(expr);
                    filter_buf_.emplace_back(std::addressof(filter));
                    current = filter.input().opposite();
                    break; // continue to upstream
                }

                default:
                    return {};
            }
        }
    }

    void estimate(relation::scan& expr, storage::index const& index, bool left) {
        build_search_key(index);
        if (search_key_buf_.empty()) {
            // index based join must have at least one term
            return;
        }
        auto result = index_estimator_(
                index,
                search_key_buf_,
                collector_.sort_keys(),
                collector_.referring());

        // FIXME: use more statistics of individual join inputs
        if (is_better(result)) {
            save_result(expr, index, left, std::move(result), term_buf_, filter_buf_);
        }
        term_buf_.clear();
        search_key_buf_.clear();
    }

    void build_search_key(storage::index const& index) {
        assert(term_buf_.empty()); // NOLINT
        assert(search_key_buf_.empty()); // NOLINT

        auto&& keys = index.keys();
        term_buf_.reserve(keys.size());
        search_key_buf_.reserve(keys.size());

        for (auto&& key : keys) {
            if (auto term = collector_.find(key.column())) {
                assert(term); // NOLINT

                term_buf_.emplace_back(term.get());
                search_key_buf_.emplace_back(term->build_index_search_key(key.column()));

                // FIXME: support more complex conditions, with pre/post filters
                if (!term->equivalent()) {
                    break;
                }
            } else {
                // no more keys
                break;
            }
        }
    }

    bool is_better(index_estimator::result const& result) {
        if (!saved_result_) {
            return true;
        }
        auto&& saved = *saved_result_;
        if (!saved.attributes().contains(attribute::single_row)
                && result.attributes().contains(attribute::single_row)) {
            return true;
        }
        return saved.score() < result.score();
    }

    void save_result(
            relation::scan& expr,
            storage::index const& index,
            bool left,
            index_estimator::result result,
            sequence_view<scan_key_collector::term*> terms,
            sequence_view<relation::filter*> filters) {
        saved_scan_ = expr;
        saved_index_ = index;
        saved_left_ = left;
        saved_result_.emplace(std::move(result));
        saved_terms_.assign(terms.begin(), terms.end());
        saved_filters_.assign(filters.begin(), filters.end());
    }

    expression_ref apply_result(
            relation::intermediate::join& expr,
            relation::scan& source,
            storage::index const& index,
            bool left,
            sequence_view<scan_key_collector::term*> terms) {
        if (left) {
            // swap left <=> right upstreams
            auto&& old_left = expr.left().opposite();
            auto&& old_right = expr.right().opposite();
            expr.left().disconnect_from(*old_left);
            expr.right().disconnect_from(*old_right);
            expr.left().connect_to(*old_right);
            expr.right().connect_to(*old_left);
        }
        auto&& result = *saved_result_;
        if (result.attributes().contains(attribute::find)) {
            return rewrite_as_join_find(expr, source, index, terms);
        }
        return rewrite_as_join_scan(expr, source, index, terms);
    }

    expression_ref rewrite_as_join_find(
            relation::intermediate::join& expr,
            relation::scan& source,
            storage::index const& index,
            sequence_view<scan_key_collector::term*> terms) {
        assert(terms.size() <= index.keys().size()); // NOLINT

        object_vector<relation::join_find::key> keys { get_object_creator().allocator() };
        fill_search_key(index, keys, terms, get_object_creator());

        binding::factory bindings { get_object_creator() };
        auto&& result = expr.owner().emplace<relation::join_find>(
                expr.operator_kind(),
                bindings(index),
                std::move(source.columns()),
                std::move(keys),
                expr.release_condition(),
                get_object_creator());

        // reconnect
        assert(expr.left().opposite()); // NOLINT
        assert(expr.right().opposite()); // NOLINT
        assert(expr.output().opposite()); // NOLINT
        auto&& upstream = *expr.left().opposite();
        auto&& downstream = *expr.output().opposite();
        expr.left().disconnect_all();
        expr.right().disconnect_all();
        expr.output().disconnect_all();
        result.left().connect_to(upstream);
        result.output().connect_to(downstream);

        graph_.erase(source);

        return result.ownership_condition();
    }

    expression_ref rewrite_as_join_scan(
            relation::intermediate::join& expr,
            relation::scan& source,
            storage::index const& index,
            sequence_view<scan_key_collector::term*> terms) {
        assert(terms.size() <= index.keys().size()); // NOLINT

        binding::factory bindings { get_object_creator() };
        relation::join_scan::endpoint lower { get_object_creator() };
        relation::join_scan::endpoint upper { get_object_creator() };

        fill_endpoints(index, lower, upper, terms, get_object_creator());

        auto&& result = expr.owner().emplace<relation::join_scan>(
                expr.operator_kind(),
                bindings(index),
                std::move(source.columns()),
                std::move(lower),
                std::move(upper),
                expr.release_condition(),
                get_object_creator());

        // reconnect
        assert(expr.left().opposite()); // NOLINT
        assert(expr.right().opposite()); // NOLINT
        assert(expr.output().opposite()); // NOLINT
        auto&& upstream = *expr.left().opposite();
        auto&& downstream = *expr.output().opposite();
        expr.left().disconnect_all();
        expr.right().disconnect_all(); // make scan orphaned
        expr.output().disconnect_all();
        result.left().connect_to(upstream);
        result.output().connect_to(downstream);

        graph_.erase(source);

        return result.ownership_condition();
    }

    void merge_filters(expression_ref condition, sequence_view<relation::filter*> filters) {
        unique_object_ptr<scalar::expression> result;
        if (auto&& current = condition.find();
                current
                && current != boolean_expression(true)) {
            // FIXME: condition.release()
            result = clone_unique(std::move(*current), get_object_creator());
        }
        for (auto* filter : filters) {
            if (filter->condition() != boolean_expression(true)) {
                if (result) {
                    result = get_object_creator().create_unique<scalar::binary>(
                            scalar::binary_operator::conditional_and,
                            std::move(result),
                            filter->release_condition());
                } else {
                    result = filter->release_condition();
                }
            }
            graph_.erase(*filter);
        }
        condition = std::move(result);
    }
};

} // namespace

// FIXME: revise

void rewrite_join(
        relation::graph_type& graph,
        storage::provider const& storage_provider,
        class index_estimator& index_estimator,
        object_creator creator) {
    engine e { graph, storage_provider, index_estimator, creator };
    for (auto it = graph.begin(); it != graph.end();) {
        auto&& expr = *it;
        if (expr.kind() == relation::intermediate::join::tag) {
            auto&& join = unsafe_downcast<relation::intermediate::join>(expr);
            if (join.key_pairs().empty() && !join.lower() && !join.upper()) {
                auto erase = e.process(join);
                if (erase) {
                    it = graph.erase(it);
                    continue;
                }
            }
        }
        ++it;
    }
}

} // namespace yugawara::analyzer::details

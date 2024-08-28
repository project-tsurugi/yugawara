#include "collect_join_keys.h"

#include <optional>

#include <takatori/relation/intermediate/join.h>

#include <takatori/util/downcast.h>
#include <takatori/util/sequence_view.h>

#include "stream_variable_flow_info.h"
#include "search_key_term_builder.h"
#include "rewrite_criteria.h"

namespace yugawara::analyzer::details {

namespace descriptor = ::takatori::descriptor;
namespace relation = ::takatori::relation;

using ::takatori::relation::join_kind;

using ::takatori::util::sequence_view;
using ::takatori::util::unsafe_downcast;

using feature = collect_join_keys_feature;
using volume_info = flow_volume_info::volume_info;

namespace {

class engine {
public:
    explicit engine(
            flow_volume_info const& flow_volume,
            collect_join_keys_feature_set features) :
        flow_volume_ { flow_volume },
        features_ { features }
    {}

    void process(relation::intermediate::join& expr) {
        if (features_.empty() || !expr.condition()) {
            return;
        }
        collect_keys(expr);
        build(expr);

        left_term_builder_.clear();
        right_term_builder_.clear();
        left_key_buf_.clear();
        right_key_buf_.clear();
        left_term_buf_.clear();
        right_term_buf_.clear();
    }

private:
    flow_volume_info const& flow_volume_;
    collect_join_keys_feature_set features_;
    stream_variable_flow_info flow_info_;
    search_key_term_builder left_term_builder_;
    search_key_term_builder right_term_builder_;
    std::vector<descriptor::variable> left_key_buf_;
    std::vector<descriptor::variable> right_key_buf_;
    std::vector<std::pair<descriptor::variable const*, search_key_term*>> left_term_buf_;
    std::vector<std::pair<descriptor::variable const*, search_key_term*>> right_term_buf_;
    mutable std::vector<std::pair<descriptor::variable const*, search_key_term*>> temp_term_buf_;

    static bool enable_broadcast(collect_join_keys_feature_set features) noexcept {
        return features.contains(feature::broadcast_find);
    }

    static bool commutative(relation::intermediate::join const& expr) noexcept {
        // only inner join is commutative
        return expr.operator_kind() == join_kind::inner;
    }

    void collect_keys(relation::intermediate::join& expr) {
        flow_info_.each(expr.left(), [&](descriptor::variable const& key) {
            left_key_buf_.emplace_back(key);
        });
        flow_info_.each(expr.right(), [&](descriptor::variable const& key) {
            right_key_buf_.emplace_back(key);
        });
    }

    enum class volume_cmp {
        left_large,
        tie,
        right_large,
    };

    void build(relation::intermediate::join& expr) {
        // FIXME: consider how choose cogroup / broadcast
        auto left_vol = flow_volume_.find(expr.left());
        auto right_vol = flow_volume_.find(expr.right());
        auto cmp = compare_volume(left_vol, right_vol);

        auto local_features = features_;
        if (expr.operator_kind() == join_kind::full_outer) {
            local_features.erase(feature::broadcast_find);
            local_features.erase(feature::broadcast_scan);
        }

        auto right_kp = build_terms(
                right_term_buf_, expr,
                right_key_buf_, left_key_buf_,
                right_term_builder_, local_features);
        if (cmp == volume_cmp::right_large) {
            right_term_buf_.erase(right_term_buf_.begin() + right_kp, right_term_buf_.end()); // NOLINT
        }

        if (commutative(expr) && enable_broadcast(local_features) && cmp != volume_cmp::left_large) {
            build_terms(
                    left_term_buf_, expr,
                    left_key_buf_, right_key_buf_,
                    left_term_builder_, local_features);
            if (is_prefer_left_build()) {
                flow_info_.erase(expr);
                swap_upstream(expr.left(), expr.right());
                organize_key(expr, left_term_buf_);
                return;
            }
        }
        organize_key(expr, right_term_buf_);
    }

    static volume_cmp compare_volume(
            std::optional<volume_info> const& left,
            std::optional<volume_info> const& right) {
        if (!left || !right) {
            return volume_cmp::tie;
        }
        constexpr double row_threshold = 1'000;
        if (left->row_count < row_threshold && right->row_count < row_threshold) {
            return volume_cmp::tie;
        }
        double lvol = left->row_count * left->column_size;
        double rvol = right->row_count * right->column_size;

        constexpr double large_ratio = 100;
        if (lvol > rvol * large_ratio) {
            return volume_cmp::left_large;
        }
        if (rvol > lvol * large_ratio) {
            return volume_cmp::right_large;
        }
        return volume_cmp::tie;
    }

    bool is_prefer_left_build() {
        // FIXME: use selectivity
        return evaluate(left_term_buf_) > evaluate(right_term_buf_);
    }

    static std::size_t evaluate(sequence_view<std::pair<descriptor::variable const*, search_key_term*> const> terms) {
        std::size_t result = 0;
        for (auto&& pair : terms) {
            auto&& term = *pair.second;
            if (term.equivalent()) {
                result += 3;
            } else if (term.full_bounded()) {
                result += 2;
            } else {
                result += 1;
            }
        }
        return result;
    }

    //NOLINTNEXTLINE(readability-function-cognitive-complexity)
    std::size_t build_terms(
            std::vector<std::pair<descriptor::variable const*, search_key_term*>>& results,
            relation::intermediate::join& expr,
            sequence_view<descriptor::variable const> build_key,
            sequence_view<descriptor::variable const> probe_key,
            search_key_term_builder& builder,
            collect_join_keys_feature_set features) const {
        for (auto&& v : build_key) {
            builder.add_key(v);
        }
        builder.add_predicate(expr.ownership_condition());
        results.reserve(build_key.size());

        temp_term_buf_.clear();
        temp_term_buf_.reserve(build_key.size());

        // key pairs
        for (auto&& key : build_key) {
            auto [begin, end] = builder.search(key);
            for (auto iter = begin; iter != end; ++iter) {
                auto&& term = iter->second;
                if (auto opposite = term.equivalent_key();
                        opposite &&
                        std::find(probe_key.begin(), probe_key.end(), *opposite) != probe_key.end()) {
                    results.emplace_back(std::addressof(key), std::addressof(term));
                } else if (term.equivalent()) {
                    temp_term_buf_.emplace_back(std::addressof(key), std::addressof(term));
                }
            }
        }
        // remind the number of key pairs to compare cogroup / broadcast
        std::size_t key_pairs = results.size();

        // other equivalents
        if (features.contains(feature::broadcast_find)) {
            for (auto&& term : temp_term_buf_) {
                results.emplace_back(term);
            }
        }

        // find best range
        if (features.contains(feature::broadcast_scan)) {
            std::optional<std::pair<descriptor::variable const*, search_key_term*>> candidate {};
            for (auto&& key : build_key) {
                if (auto term = builder.find(key); term && !term->equivalent()) {
                    // FIXME: use stats
                    if (term->full_bounded()) {
                        candidate.emplace(std::addressof(key), term.get());
                        break;
                    }
                    if (!candidate) {
                        candidate.emplace(std::addressof(key), term.get());
                    }
                }
            }
            if (candidate) {
                results.emplace_back(std::move(*candidate));
            }
        }

        return key_pairs;
    }

    static void organize_key(
            relation::intermediate::join& expr,
            sequence_view<std::pair<descriptor::variable const*, search_key_term*>> terms) {
        if (terms.empty()) {
            return;
        }
        auto&& lower = expr.lower();
        auto&& upper = expr.upper();
        for (std::size_t i = 0, n = terms.size(); i < n; ++i) {
            auto [key, term] = terms[i];
            bool last = (i == n - 1);
            add_endpoint_term(lower, upper, *key, *term, last);
        }
    }
};

} // namespace

void collect_join_keys(
        relation::graph_type& graph,
        flow_volume_info const& flow_volume,
        collect_join_keys_feature_set features) {
    if (features.contains(feature::broadcast_scan)) {
        features.insert(feature::broadcast_find);
    }
    engine e { flow_volume, features };
    for (auto&& expr : graph) {
        if (expr.kind() == relation::intermediate::join::tag) {
            auto&& join = unsafe_downcast<relation::intermediate::join>(expr);
            if (!join.lower() || !join.upper()) {
                e.process(join);
            }
        }
    }
}

} // namespace yugawara::analyzer::details

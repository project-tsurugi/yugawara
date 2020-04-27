#include "default_index_estimator.h"

#include <takatori/util/string_builder.h>

namespace yugawara::analyzer::details {

using ::yugawara::storage::index_feature;

using ::takatori::util::sequence_view;
using ::takatori::util::string_builder;

using attribute = details::index_estimator_result_attribute;
using attribute_set = details::index_estimator_result_attribute_set;

namespace {

attribute_set check_keys(
        storage::index const& index,
        sequence_view<index_estimator::search_key const> search_keys) {
    if (search_keys.empty()) {
        // always full scan if search key is absent
        return {};
    }
    if (index.keys().size() < search_keys.size()) {
        throw std::domain_error(string_builder {}
                << "inconsistent index key: "
                << search_keys
                << string_builder::to_string);
    }
    bool saw_proper = false;
    bool saw_half = false;
    for (std::size_t i = 0, n = search_keys.size(); i < n; ++i) {
        if (index.keys()[i].column() != search_keys[i].column()) {
            throw std::domain_error(string_builder {}
                    << "inconsistent index key: "
                    << search_keys
                    << string_builder::to_string);
        }
        if (saw_half && search_keys[i].bounded()) {
            // key must be < 1-dimensional range
            return {};
        }
        if (!saw_proper && !search_keys[i].equivalent()) {
            saw_proper = true;
        }
        if (!search_keys[i].bounded()) {
            saw_half = true;
        }
    }
    attribute_set result;
    if (index.features().contains(index_feature::find)
            && index.keys().size() == search_keys.size()
            && !saw_proper) {
        result.insert(attribute::find);
        if (index.features().contains(index_feature::unique)) {
            result.insert(attribute::single_row);
        }
    }
    if (index.features().contains(index_feature::scan)) {
        result.insert(attribute::range_scan);
    }
    return result;
}

bool is_sorted(storage::index const& index, sequence_view<index_estimator::sort_key const> sort_keys) {
    if (sort_keys.empty()
            || index.keys().size() > sort_keys.size()
            || !index.features().contains(index_feature::scan)) {
        return false;
    }
    for (std::size_t i = 0, n = index.keys().size(); i < n; ++i) {
        auto&& a = index.keys()[i];
        auto&& b = sort_keys[i];
        if (a.column() != b.column() || a.direction() != b.direction()) {
            return false;
        }
    }
    return true;
}

bool is_index_only(
        storage::index const& index,
        sequence_view<std::reference_wrapper<storage::column const> const> values) {
    auto&& ks = index.keys();
    auto&& vs = index.values();
    for (storage::column const& c : values) {
        if (std::find(vs.begin(), vs.end(), c) != vs.end()) {
            continue;
        }
        if (std::find_if(ks.begin(), ks.end(), [&](auto&& d) { return d.column() == c; }) != ks.end()) {
            continue;
        }
        return false;
    }
    // FIXME: check if it is in values?
    return true;
}

double key_selectivity(sequence_view<index_estimator::search_key const> search_keys) {
    constexpr double base_selectivity = 1.0;
    constexpr double equivalent_selectivity = 0.25;
    constexpr double full_bound_selectivity = 0.5;
    constexpr double half_bound_selectivity = 0.75;
    double result = base_selectivity;
    if (search_keys.empty()) {
        return result;
    }
    for (auto&& k : search_keys) {
        if (k.equivalent()) {
            result *= equivalent_selectivity;
        } else if (k.bounded()) {
            result *= full_bound_selectivity;
        } else {
            result *= half_bound_selectivity;
        }
    }
    return result;
}

} // namespace

index_estimator::result default_index_estimator::operator()(
        storage::index const& index,
        sequence_view<search_key const> search_keys,
        sequence_view<sort_key const> sort_keys,
        sequence_view<column_ref const> values) {
    auto attributes = check_keys(index, search_keys);
    attributes[attribute::sorted] = is_sorted(index, sort_keys);
    attributes[attribute::index_only] = is_index_only(index, values);

    double selectivity = key_selectivity(search_keys);
    constexpr double unique_selectivity = 0.125;
    std::optional<result::size_type> count {};
    if (attributes[attribute::single_row]) {
        selectivity *= unique_selectivity;
        count.emplace(1);
    }
    double score = 1 / selectivity;
    if (attributes[attribute::index_only]) {
        score *= 2;
    }
    return { score, std::move(count), attributes, };
}

} // namespace yugawara::analyzer::details

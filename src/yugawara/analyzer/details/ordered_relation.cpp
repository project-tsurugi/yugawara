#include "ordered_relation.h"

#include <algorithm>
#include <functional>
#include <vector>

#include <yugawara/binding/extract.h>

#include <yugawara/storage/column.h>
#include <yugawara/storage/index.h>
#include <yugawara/storage/index_feature.h>
#include <yugawara/storage/provider.h>
#include <yugawara/storage/table.h>
#include <yugawara/storage/details/index_key_element.h>

namespace yugawara::analyzer::details {

namespace descriptor = ::takatori::descriptor;
namespace relation = ::takatori::relation;

using ::takatori::util::optional_ptr;

using column_ref = std::reference_wrapper<storage::column const>;

namespace {

path_capability_set find_single_path_capabilities(
        relation::expression const& expr,
        path_capability_options const& options) {
    // NOTE: each order preserving operator must have exactly one input and one output
    auto&& inputs = expr.input_ports();
    auto&& outputs = expr.output_ports();
    if (inputs.size() != 1 || outputs.size() != 1) {
        return {};
    }
    return find_path_capabilities(inputs[0], outputs[0], options);
}

optional_ptr<relation::expression> find_upstream(relation::expression::input_port_type& port) {
    if (auto upstream = port.opposite()) {
        return upstream->owner();
    }
    return {};
}

optional_ptr<relation::expression> find_downstream(relation::expression::output_port_type& port) {
    if (auto downstream = port.opposite()) {
        return downstream->owner();
    }
    return {};
}

optional_ptr<storage::column const> find_source_column(relation::scan const& scan, descriptor::variable const& variable) {
    for (auto&& column : scan.columns()) {
        if (column.destination() == variable) {
            return binding::extract_if<storage::column>(column.source());
        }
    }
    return {};
}

std::vector<column_ref> collect_fixed_columns(relation::scan const& scan) {
    std::vector<column_ref> results {};
    auto&& lower = scan.lower().keys();
    auto&& upper = scan.upper().keys();
    for (std::size_t i = 0, n = std::min(lower.size(), upper.size()); i < n; ++i) {
        auto&& l = lower[i];
        auto&& u = upper[i];
        // NOTE: the leading keys which have the same value on both ends have the single value in the range
        if (l.variable() != u.variable() || l.value() != u.value()) {
            break;
        }
        auto column = binding::extract_if<storage::column>(l.variable());
        if (!column) {
            break;
        }
        results.emplace_back(*column);
    }
    return results;
}

} // namespace

ordered_path find_ordered_source(
        relation::expression::input_port_type& input,
        path_capability_options const& options) {
    path_capability_set capabilities {
            path_capability::preserves_order,
            path_capability::at_least_one,
            path_capability::at_most_one,
    };
    auto current = find_upstream(input);
    while (current) {
        auto found = find_single_path_capabilities(*current, options);
        if (!found.contains(path_capability::preserves_order)) {
            break;
        }
        capabilities &= found;
        current = find_upstream(current->input_ports()[0]);
    }
    return {
            current,
            capabilities,
    };
}

ordered_path find_ordered_source(relation::intermediate::limit& expr, path_capability_options const& options) {
    return find_ordered_source(expr.input(), options);
}

ordered_path find_ordered_destination(
        relation::expression::output_port_type& output,
        path_capability_options const& options) {
    path_capability_set capabilities {
            path_capability::preserves_order,
            path_capability::at_least_one,
            path_capability::at_most_one,
    };
    auto current = find_downstream(output);
    while (current) {
        auto found = find_single_path_capabilities(*current, options);
        if (!found.contains(path_capability::preserves_order)) {
            break;
        }
        capabilities &= found;
        current = find_downstream(current->output_ports()[0]);
    }
    return {
            current,
            capabilities,
    };
}

ordered_path find_ordered_destination(relation::intermediate::limit& expr, path_capability_options const& options) {
    return find_ordered_destination(expr.output(), options);
}

ordered_path find_ordered_destination(relation::scan& expr, path_capability_options const& options) {
    return find_ordered_destination(expr.output(), options);
}

std::optional<std::vector<storage::details::index_key_element>> resolve_sort_keys(
        relation::scan const& scan,
        relation::intermediate::limit const& expr) {
    std::vector<storage::details::index_key_element> results {};
    results.reserve(expr.sort_keys().size());
    for (auto&& key : expr.sort_keys()) {
        auto column = find_source_column(scan, key.variable());
        if (!column) {
            return {};
        }
        results.emplace_back(*column, key.direction());
    }
    return results;
}

std::optional<relation::scan_direction> find_scan_direction(
        relation::scan const& scan,
        relation::intermediate::limit const& expr) {
    auto sort_keys = resolve_sort_keys(scan, expr);
    if (!sort_keys) {
        return {};
    }
    auto&& index = binding::extract<storage::index>(scan.source());
    auto fixed_columns = collect_fixed_columns(scan);
    return find_scan_direction(index, *sort_keys, fixed_columns);
}

using ::takatori::relation::scan_direction;
using ::takatori::util::sequence_view;

using index_key = storage::details::index_key_element;

namespace {

bool contains(sequence_view<column_ref const> columns, storage::column const& column) {
    return std::any_of(columns.begin(), columns.end(), [&](column_ref const& c) {
        return c.get() == column;
    });
}

/*
 * returns the scan direction that orders the index key in the same direction as the sort key.
 * Reading the index backward reverses the order of all its keys, so that the directions of
 * all matched sort keys must be the same as the index keys, or must be reversed from them.
 * For example, with the primary key (a, b), "ORDER BY a DESC, b DESC" is satisfied by reading backward,
 * but "ORDER BY a DESC, b" is never satisfied in either direction.
 * Note that, this also reverses the position of nulls, and it is still consistent with the requested order,
 * because nulls are treated as the smallest values in both the index keys and the sort keys.
 */
scan_direction scan_direction_of(
        storage::details::index_key_element const& index_key,
        storage::details::index_key_element const& sort_key) {
    if (index_key.direction() == sort_key.direction()) {
        return scan_direction::forward;
    }
    return scan_direction::backward;
}

/**
 * @brief the order of the entries on an index.
 */
struct index_order {
    /// @brief the sort keys of the entries.
    std::vector<index_key> keys;

    /// @brief whether or not the entries that tie on all the keys are at most one.
    bool unique;
};

/*
 * returns the order of the entries on the given index.
 * If the entries that tie on the index key are sorted by the primary key, the primary key columns follow the index key,
 * except the columns already in the index key, because they have the same value in the tied entries.
 * For example, with the secondary index (v) and the primary key (k), the entries are sorted by (v, k).
 * The primary key is unique and never null, so that the entries that tie on the resulting keys are at most one,
 * even if the index key has nulls.
 */
index_order find_index_order(storage::index const& index) {
    auto&& index_keys = index.keys();
    index_order result {
            { index_keys.begin(), index_keys.end() },
            index.features().contains(storage::index_feature::primary),
    };
    if (!index.features().contains(storage::index_feature::ties_ordered_by_primary_key)) {
        return result;
    }
    auto&& table = index.table();
    auto storages = table.owner();
    if (!storages) {
        return result;
    }
    auto primary = storages->find_primary_index(table);
    if (!primary) {
        return result;
    }
    for (auto&& key : primary->keys()) {
        bool found = std::any_of(result.keys.begin(), result.keys.end(), [&](index_key const& k) {
            return k.column() == key.column();
        });
        if (!found) {
            result.keys.emplace_back(key);
        }
    }
    result.unique = true;
    return result;
}

} // namespace

std::optional<scan_direction> find_scan_direction(
        storage::index const& index,
        sequence_view<storage::details::index_key_element const> sort_keys,
        sequence_view<column_ref const> fixed_columns) {
    auto order = find_index_order(index);
    auto&& keys = order.keys;

    // the index columns that are already matched with the sort keys
    std::vector<column_ref> matched {};
    matched.reserve(keys.size());

    // the direction decided by the first matched sort key, or empty if no sort keys are matched yet
    std::optional<scan_direction> decided {};

    std::size_t key_index = 0;
    for (auto&& sort_key : sort_keys) {
        auto&& column = sort_key.column();
        /*
         * the fixed or already sorted columns never change the order, for example, with the primary key (a, b):
         * - fixed columns: "WHERE a = 1 ORDER BY b" - the scan range is "a = 1", and then the column "a" is fixed.
         *   We skip the "a" in the index keys (see below), and then "b" matches the next index key.
         *   Also, "WHERE a = 1 ORDER BY a DESC, b" skips the sort key "a DESC" here, even if its direction is different.
         * - matched columns: "ORDER BY a, b, a DESC" - the last sort key "a DESC" never changes the order,
         *   because it only orders the rows that tie on the preceding sort keys, and they always have the same "a".
         */
        if (contains(fixed_columns, column) || contains(matched, column)) {
            continue;
        }
        while (key_index < keys.size() && contains(fixed_columns, keys[key_index].column())) {
            ++key_index;
        }
        if (key_index >= keys.size()) {
            /*
             * all index keys are matched or fixed, but some sort keys still remain.
             * If the order of the index is unique, the rows that tie on the preceding sort keys are at most one,
             * because the primary key is unique and never null (see find_index_order()).
             * Then, the order of the rows is already determined, and the rest of the sort keys never change it.
             * For example, with the primary key (a, b), "WHERE a = 1 ORDER BY b, c" ignores the sort key "c".
             *
             * Otherwise, the index may have two or more rows that tie on the preceding sort keys,
             * for example, with the secondary index (v) and the primary key (k), "ORDER BY v, w":
             * - non-unique index: the rows (k=1, v=10, w=9) and (k=2, v=10, w=1) tie on "v",
             *   and the index does not order them by "w".
             * - unique index: the rows (k=1, v=NULL, w=9) and (k=2, v=NULL, w=1) also tie on "v",
             *   because the unique index allows two or more nulls.
             * Also, "ORDER BY v, k" is not satisfied here, unless the index declares that the rows that tie on
             * its keys are sorted by the primary key, even if the current implementation does so.
             */
            if (order.unique) {
                break;
            }
            return {};
        }
        auto&& key = keys[key_index];
        if (key.column() != column) {
            return {};
        }
        auto current = scan_direction_of(key, sort_key);
        if (decided && *decided != current) {
            return {};
        }
        decided = current;
        matched.emplace_back(column);
        ++key_index;
    }
    // NOTE: if no sort keys are matched (e.g. all of them are fixed), we prefer the forward direction
    return decided.value_or(scan_direction::forward);
}

} // namespace yugawara::analyzer::details

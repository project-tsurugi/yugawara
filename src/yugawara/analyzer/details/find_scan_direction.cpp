#include "find_scan_direction.h"

#include <algorithm>
#include <vector>

#include <yugawara/storage/index_feature.h>

namespace yugawara::analyzer::details {

using ::takatori::relation::scan_direction;
using ::takatori::util::sequence_view;

using column_ref = std::reference_wrapper<storage::column const>;

namespace {

bool contains(sequence_view<column_ref const> columns, storage::column const& column) {
    return std::any_of(columns.begin(), columns.end(), [&](column_ref const& c) {
        return c.get() == column;
    });
}

} // namespace

std::optional<scan_direction> find_scan_direction(
        storage::index const& index,
        sequence_view<storage::details::index_key_element const> sort_keys,
        sequence_view<column_ref const> fixed_columns) {
    auto&& keys = index.keys();
    bool unique_keys = index.features().contains(storage::index_feature::primary);

    // the index columns that are already matched with the sort keys
    std::vector<column_ref> matched {};
    matched.reserve(keys.size());

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
             * If the index is the primary index, the rows that tie on the preceding sort keys are at most one,
             * because the primary key is unique and never null.
             * Then, the order of the rows is already determined, and the rest of the sort keys never change it.
             * For example, with the primary key (a, b), "WHERE a = 1 ORDER BY b, c" ignores the sort key "c".
             *
             * Otherwise, the index may have two or more rows that tie on the preceding sort keys,
             * for example, with the secondary index (v) and the primary key (k), "ORDER BY v, w":
             * - non-unique index: the rows (k=1, v=10, w=9) and (k=2, v=10, w=1) tie on "v",
             *   and the index does not order them by "w".
             * - unique index: the rows (k=1, v=NULL, w=9) and (k=2, v=NULL, w=1) also tie on "v",
             *   because the unique index allows two or more nulls.
             * Also, "ORDER BY v, k" is not satisfied here, because the index never guarantees that
             * the primary keys follow its keys, even if the current implementation does so.
             */
            if (unique_keys) {
                return scan_direction::forward;
            }
            return {};
        }
        auto&& key = keys[key_index];
        if (key.column() != column || key.direction() != sort_key.direction()) {
            // FIXME: find backward direction if all directions are reversed
            return {};
        }
        matched.emplace_back(column);
        ++key_index;
    }
    return scan_direction::forward;
}

} // namespace yugawara::analyzer::details

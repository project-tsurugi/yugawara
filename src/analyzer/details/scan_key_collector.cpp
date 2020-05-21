#include "scan_key_collector.h"

#include <takatori/relation/intermediate/dispatch.h>

#include <takatori/util/exception.h>
#include <takatori/util/string_builder.h>

#include <yugawara/binding/extract.h>

#include <yugawara/storage/index.h>
#include <yugawara/storage/column.h>

#include "decompose_predicate.h"

namespace yugawara::analyzer::details {

namespace descriptor = ::takatori::descriptor;
namespace scalar = ::takatori::scalar;
namespace relation = ::takatori::relation;

using ::takatori::util::object_ownership_reference;
using ::takatori::util::optional_ptr;
using ::takatori::util::sequence_view;
using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;

using expression_ref = object_ownership_reference<scalar::expression>;

namespace {

storage::column const* extract_column(descriptor::variable const& variable, bool mandatory = true) {
    if (auto column = binding::extract_if<storage::column>(variable)) {
        return column.get();
    }
    if (mandatory) {
        throw_exception(std::domain_error(string_builder {}
                << "must be table columns: "
                << variable
                << string_builder::to_string));
    }
    return nullptr;
}

class factor_collector {
public:
    using output_port_type = relation::expression::output_port_type;
    using sort_key = scan_key_collector::sort_key;

    explicit factor_collector(
            search_key_term_builder& builder,
            std::vector<sort_key, ::takatori::util::object_allocator<sort_key>>& sort_keys,
            bool include_join) noexcept
        : builder_(builder)
        , sort_keys_(sort_keys)
        , for_join_(include_join)
    {}

    void process(output_port_type& output) {
        optional_ptr current { output };
        while (current) {
            if (!current->opposite()) {
                throw_exception(std::domain_error(string_builder {}
                        << "disconnected output: "
                        << *current
                        << string_builder::to_string));
            }
            auto&& opposite = *current->opposite();
            current = relation::intermediate::dispatch(*this, opposite.owner());
        }
    }

    constexpr optional_ptr<output_port_type> operator()(relation::expression const&) noexcept {
        return {};
    }

    optional_ptr<output_port_type> operator()(relation::filter& expr) {
        builder_.add_predicate(expr.ownership_condition());
        return expr.output();
    }

    optional_ptr<output_port_type> operator()(relation::intermediate::join& expr) {
        if (!for_join_) {
            return {};
        }
        if (expr.condition()) {
            builder_.add_predicate(expr.ownership_condition());
            found_join_ = true;
        }
        return {};
    }

    // FIXME: limit (ignore if include_join)

    [[nodiscard]] bool reach_join() const noexcept {
        return found_join_;
    }

private:
    search_key_term_builder& builder_;
    std::vector<sort_key, ::takatori::util::object_allocator<sort_key>> sort_keys_;
    bool for_join_;
    bool found_join_ { false };
};

} // namespace

scan_key_collector::scan_key_collector(::takatori::util::object_creator creator)
    : creator_(creator)
    , term_builder_(creator)
    , column_map_(creator.allocator())
    , sort_keys_(creator.allocator())
    , referring_(creator.allocator())
{}

bool scan_key_collector::operator()(relation::scan& expression, bool include_join) {
    clear();

    // must not have any endpoint settings
    if (expression.lower() || expression.upper() || expression.limit()) {
        return false;
    }

    auto&& columns = expression.columns();
    term_builder_.reserve_keys(columns.size());
    referring_.reserve(columns.size());
    column_map_.reserve(columns.size());

    // create column info
    for (auto&& mapping : columns) {
        term_builder_.add_key(mapping.destination());

        auto&& column = extract_column(mapping.source());
        column_map_.emplace(column, mapping.destination());
        referring_.emplace_back(std::ref(*column));
    }

    // collect factors
    factor_collector collector { term_builder_, sort_keys_, include_join };
    collector.process(expression.output());
    if (term_builder_.empty() || (include_join && !collector.reach_join())) {
        return false;
    }

    // set table info
    auto&& index = binding::extract<storage::index>(expression.source());
    table_.reset(index.table());

    return true;
}

void scan_key_collector::clear() {
    table_ = {};
    term_builder_.clear();
    column_map_.clear();
    sort_keys_.clear();
    referring_.clear();
}

storage::table const& scan_key_collector::table() const {
    return *table_;
}

takatori::util::optional_ptr<scan_key_collector::term> scan_key_collector::find(storage::column const& column) {
    if (auto it = column_map_.find(std::addressof(column)); it != column_map_.end()) {
        return term_builder_.find(it->second);
    }
    return {};
}

sequence_view<scan_key_collector::sort_key const> scan_key_collector::sort_keys() const {
    return sort_keys_;
}

sequence_view<scan_key_collector::column_ref const> scan_key_collector::referring() const {
    return referring_;
}

::takatori::util::object_creator scan_key_collector::get_object_creator() const noexcept {
    return creator_;
}

} // namespace yugawara::analyzer::details

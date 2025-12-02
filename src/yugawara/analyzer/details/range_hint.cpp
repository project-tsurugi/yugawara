#include "range_hint.h"

#include <takatori/util/clonable.h>

#include <yugawara/type/conversion.h>

#include <yugawara/extension/type/error.h>

#include "compare_value.h"

namespace yugawara::analyzer::details {

using ::takatori::util::clone_unique;

namespace {

[[nodiscard]] compare_result compare_immediate(
        ::takatori::scalar::immediate const& left,
        ::takatori::scalar::immediate const& right) {
    // check if two values are potentially comparable
    auto result = type::unifying_conversion(left.type(), right.type());
    if (extension::type::is_error(*result)) {
        return compare_result::undefined;
    }
    return compare(left.value(), right.value());
}

} // namespace

void range_hint_entry::intersect_lower(::takatori::scalar::immediate const& value, bool inclusive) {
    if (lower_type_ == range_hint_type::infinity) {
        lower_type_ = inclusive ? range_hint_type::inclusive : range_hint_type::exclusive;
        lower_value_ = clone_unique(value);
        return;
    }
    if (!std::holds_alternative<immediate_type>(lower_value_)) {
        // keep the host variable
        return;
    }
    auto&& existing = std::get<immediate_type>(lower_value_);
    auto result = compare_immediate(*existing, value);

    // try to shrink column value range about lower bound ..
    if (result == compare_result::undefined) {
        // keep the original
        return;
    }
    if (result == compare_result::equal) {
        // current == incoming < column -> may change inclusiveness
        if (lower_type_ == range_hint_type::inclusive && !inclusive) {
            // current <= column && value < column -> make exclusive
            lower_type_ = range_hint_type::exclusive;
            lower_value_ = clone_unique(value);
        }
        return;
    }
    if (result == compare_result::less) {
        // current < incoming -> shrink bound
        lower_type_ = inclusive ? range_hint_type::inclusive : range_hint_type::exclusive;
        lower_value_ = clone_unique(value);
        return;
    }
    // current > incoming -> keep the current
}

void range_hint_entry::intersect_lower(variable_type const& value, bool inclusive) {
    if (lower_type_ == range_hint_type::infinity || !std::holds_alternative<variable_type>(lower_value_)) {
        lower_type_ = inclusive ? range_hint_type::inclusive : range_hint_type::exclusive;
        lower_value_ = value;
        return;
    }
    auto&& existing = std::get<variable_type>(lower_value_);
    if (existing != value) {
        // keep the current variable
        return;
    }

    // try to shrink inclusiveness
    if (lower_type_ == range_hint_type::inclusive && !inclusive) {
        lower_type_ = range_hint_type::exclusive;
    }
}

void range_hint_entry::intersect_upper(::takatori::scalar::immediate const& value, bool inclusive) {
    if (upper_type_ == range_hint_type::infinity) {
        upper_type_ = inclusive ? range_hint_type::inclusive : range_hint_type::exclusive;
        upper_value_ = clone_unique(value);
        return;
    }
    if (!std::holds_alternative<immediate_type>(upper_value_)) {
        // keep the host variable
        return;
    }
    auto&& existing = std::get<immediate_type>(upper_value_);
    auto result = compare_immediate(*existing, value);

    // try to shrink column value range about upper bound ..
    if (result == compare_result::undefined) {
        // keep the original
        return;
    }
    if (result == compare_result::equal) {
        // current == incoming > column -> may change inclusiveness
        if (upper_type_ == range_hint_type::inclusive && !inclusive) {
            // current >= column && value > column -> make exclusive
            upper_type_ = range_hint_type::exclusive;
            upper_value_ = clone_unique(value);
        }
        return;
    }
    if (result == compare_result::less) {
        // current < incoming -> keep the current
        return;
    }
    // current > incoming -> shrink bound
    upper_type_ = inclusive ? range_hint_type::inclusive : range_hint_type::exclusive;
    upper_value_ = clone_unique(value);
}

void range_hint_entry::intersect_upper(variable_type const& value, bool inclusive) {
    if (upper_type_ == range_hint_type::infinity || !std::holds_alternative<variable_type>(upper_value_)) {
        upper_type_ = inclusive ? range_hint_type::inclusive : range_hint_type::exclusive;
        upper_value_ = value;
        return;
    }
    auto&& existing = std::get<variable_type>(upper_value_);
    if (existing != value) {
        // keep the current variable
        return;
    }

    // try to shrink inclusiveness
    if (upper_type_ == range_hint_type::inclusive && !inclusive) {
        upper_type_ = range_hint_type::exclusive;
    }
}

void range_hint_entry::union_lower(takatori::scalar::immediate const& value, bool inclusive) {
    if (lower_type_ == range_hint_type::infinity) {
        return;
    }
    if (!std::holds_alternative<immediate_type>(lower_value_)) {
        // different bound values are not comparable -> make infinity
        lower_type_ = range_hint_type::infinity;
        lower_value_ = {};
        return;
    }

    auto&& existing = std::get<immediate_type>(lower_value_);
    auto result = compare_immediate(*existing, value);

    // try to cover column value range about lower bound ..
    if (result == compare_result::undefined) {
        // not comparable -> make infinity
        lower_type_ = range_hint_type::infinity;
        lower_value_ = {};
        return;
    }
    if (result == compare_result::equal) {
        // existing == incoming -> may change inclusiveness
        if (lower_type_ == range_hint_type::exclusive && inclusive) {
            // existing < column || incoming <= column -> make inclusive
            lower_type_ = range_hint_type::inclusive;
            lower_value_ = clone_unique(value);
        }
        return;
    }
    if (result == compare_result::less) {
        // existing < incoming -> keep the current
        return;
    }
    // existing > incoming -> cover bound
    lower_type_ = inclusive ? range_hint_type::inclusive : range_hint_type::exclusive;
    lower_value_ = clone_unique(value);
}

void range_hint_entry::union_lower(variable_type const& value, bool inclusive) {
    if (lower_type_ == range_hint_type::infinity) {
        return;
    }
    if (!std::holds_alternative<variable_type>(lower_value_)) {
        // different bound values are not comparable -> make infinity
        lower_type_ = range_hint_type::infinity;
        lower_value_ = {};
        return;
    }
    auto&& existing = std::get<variable_type>(lower_value_);
    if (existing != value) {
        // different variables are not comparable -> make infinity
        lower_type_ = range_hint_type::infinity;
        lower_value_ = {};
        return;
    }
    // same variable -> may change inclusiveness
    if (lower_type_ == range_hint_type::exclusive && inclusive) {
        // existing < column || incoming <= column -> make inclusive
        lower_type_ = range_hint_type::inclusive;
        lower_value_ = value;
    }
}

void range_hint_entry::union_upper(takatori::scalar::immediate const& value, bool inclusive) {
    if (upper_type_ == range_hint_type::infinity) {
        return;
    }
    if (!std::holds_alternative<immediate_type>(upper_value_)) {
        // different bound values are not comparable -> make infinity
        upper_type_ = range_hint_type::infinity;
        upper_value_ = {};
        return;
    }
    auto&& existing = std::get<immediate_type>(upper_value_);
    auto result = compare_immediate(*existing, value);

    // try to cover column value range about upper bound ..
    if (result == compare_result::undefined) {
        // not comparable -> make infinity
        upper_type_ = range_hint_type::infinity;
        upper_value_ = {};
        return;
    }
    if (result == compare_result::equal) {
        // existing == incoming -> may change inclusiveness
        if (upper_type_ == range_hint_type::exclusive && inclusive) {
            // existing > column || incoming >= column -> make inclusive
            upper_type_ = range_hint_type::inclusive;
            upper_value_ = clone_unique(value);
        }
        return;
    }
    if (result == compare_result::less) {
        // existing < incoming -> cover bound
        upper_type_ = inclusive ? range_hint_type::inclusive : range_hint_type::exclusive;
        upper_value_ = clone_unique(value);
        return;
    }
    // existing > incoming -> keep the current
}

void range_hint_entry::union_upper(variable_type const& value, bool inclusive) {
    if (upper_type_ == range_hint_type::infinity) {
        return;
    }
    if (!std::holds_alternative<variable_type>(upper_value_)) {
        // different bound values are not comparable -> make infinity
        upper_type_ = range_hint_type::infinity;
        upper_value_ = {};
        return;
    }
    auto&& existing = std::get<variable_type>(upper_value_);
    if (existing != value) {
        // different variables are not comparable -> make infinity
        upper_type_ = range_hint_type::infinity;
        upper_value_ = {};
        return;
    }
    // same variable -> may change inclusiveness
    if (upper_type_ == range_hint_type::exclusive && inclusive) {
        // existing > column || incoming >= column -> make inclusive
        upper_type_ = range_hint_type::inclusive;
        upper_value_ = value;
    }
}

void range_hint_entry::intersect_merge(range_hint_entry&& other) {
    intersect_lower(other.lower_type_, std::move(other.lower_value_));
    intersect_upper(other.upper_type_, std::move(other.upper_value_));
}

void range_hint_entry::union_merge(range_hint_entry&& other) {
    union_lower(other.lower_type_, std::move(other.lower_value_));
    union_upper(other.upper_type_, std::move(other.upper_value_));
}

bool range_hint_entry::empty() const noexcept {
    return lower_type_ == range_hint_type::infinity &&
        upper_type_ == range_hint_type::infinity;
}

range_hint_type range_hint_entry::lower_type() const noexcept {
    return lower_type_;
}

range_hint_type range_hint_entry::upper_type() const noexcept {
    return upper_type_;
}

range_hint_entry::value_type& range_hint_entry::lower_value() noexcept {
    return lower_value_;
}

range_hint_entry::value_type& range_hint_entry::upper_value() noexcept {
    return upper_value_;
}

void range_hint_entry::intersect_lower(range_hint_type type, value_type value) {
    if (type == range_hint_type::infinity) {
        // do nothing
    } else if (std::holds_alternative<immediate_type>(value)) {
        intersect_lower(*std::get<immediate_type>(value), type == range_hint_type::inclusive);
    } else {
        intersect_lower(std::get<variable_type>(value), type == range_hint_type::inclusive);
    }
}

void range_hint_entry::intersect_upper(range_hint_type type, value_type value) {
    if (type == range_hint_type::infinity) {
        // do nothing
    } else if (std::holds_alternative<immediate_type>(value)) {
        intersect_upper(*std::get<immediate_type>(value), type == range_hint_type::inclusive);
        return;
    } else {
        intersect_upper(std::get<variable_type>(value), type == range_hint_type::inclusive);
    }
}

void range_hint_entry::union_lower(range_hint_type type, value_type value) {
    if (type == range_hint_type::infinity) {
        lower_type_ = range_hint_type::infinity;
        lower_value_ = {};
    } else if (std::holds_alternative<immediate_type>(value)) {
        union_lower(*std::get<immediate_type>(value), type == range_hint_type::inclusive);
    } else {
        union_lower(std::get<variable_type>(value), type == range_hint_type::inclusive);
    }
}

void range_hint_entry::union_upper(range_hint_type type, value_type value) {
    if (type == range_hint_type::infinity) {
        upper_type_ = range_hint_type::infinity;
        upper_value_ = {};
    } else if (std::holds_alternative<immediate_type>(value)) {
        union_upper(*std::get<immediate_type>(value), type == range_hint_type::inclusive);
    } else {
        union_upper(std::get<variable_type>(value), type == range_hint_type::inclusive);
    }
}

bool range_hint_map::contains(key_type const& key) const noexcept {
    if (auto iter = entries_.find(key); iter != entries_.end()) {
        return !iter->second.empty();
    }
    return false;
}

range_hint_map::value_type& range_hint_map::get(key_type const& key) {
    auto [iter, created] = entries_.try_emplace(key);
    (void) created;
    return iter->second;
}

void range_hint_map::consume(consumer_type const& consumer) {
    for (auto&& [key, value] : entries_) {
        if (!value.empty()) {
            consumer(key, std::move(value));
        }
    }
    entries_.clear();
}

void range_hint_map::intersect_merge(range_hint_map&& other) {
    other.consume([this](key_type const& key, value_type&& value) -> void {
        get(key).intersect_merge(std::move(value));
    });
}

void range_hint_map::union_merge(range_hint_map&& other) {
    for (auto&& [key, value] : entries_) {
        if (!other.contains(key)) {
            value = {};
        }
    }
    other.consume([this](key_type const& key, value_type&& value) -> void {
        get(key).union_merge(std::move(value));
    });
}

} // namespace yugawara::analyzer::details

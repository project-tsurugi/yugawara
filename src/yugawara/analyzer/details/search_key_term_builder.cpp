#include "search_key_term_builder.h"

#include <takatori/scalar/dispatch.h>
#include <takatori/scalar/walk.h>

#include <takatori/util/downcast.h>
#include <takatori/util/exception.h>

#include "decompose_predicate.h"

namespace yugawara::analyzer::details {

namespace descriptor = ::takatori::descriptor;
namespace scalar = ::takatori::scalar;

using ::takatori::util::ownership_reference;
using ::takatori::util::optional_ptr;
using ::takatori::util::throw_exception;
using ::takatori::util::unsafe_downcast;

using expression_ref = ownership_reference<scalar::expression>;

namespace {

using variable_set = search_key_term_builder::variable_set;
using factor_info = search_key_term_builder::factor_info;
using term_map = search_key_term_builder::term_map;
using factor_info_map = search_key_term_builder::factor_info_map;

class key_variable_detector {
public:
    explicit key_variable_detector(variable_set const& keys) noexcept
        : keys_(keys)
    {}

    constexpr void operator()(scalar::expression const&) {}

    bool operator()(scalar::variable_reference const& expr) {
        found_ = found_ || is(expr.variable());
        return false;
    }

    bool is(descriptor::variable const& variable) {
        return keys_.find(variable) != keys_.end();
    }

    optional_ptr<descriptor::variable const> extract_if(scalar::expression const& expr) {
        if (expr.kind() == scalar::variable_reference::tag) {
            auto&& variable = unsafe_downcast<scalar::variable_reference>(expr).variable();
            if (is(variable)) {
                return variable;
            }
        }
        return {};
    }

    [[nodiscard]] bool contains(scalar::expression const& expr) {
        found_ = false;
        scalar::walk(*this, expr);
        return found_;
    }

private:
    variable_set const& keys_;
    bool found_ { false };
};

class factor_collector {
public:
    explicit factor_collector(
            variable_set const& keys,
            factor_info_map& factors) noexcept :
        keys_ { keys },
        factor_info_map_ { factors }
    {}

    void operator()(expression_ref&& condition) {
        decompose_predicate(std::move(condition), [&](expression_ref&& term) {
            dispatch(*term, std::move(term));
        });
    }

    constexpr void operator()(scalar::expression const&, expression_ref const&, bool = false) noexcept {}

    void operator()(scalar::unary& expr, expression_ref&& ref, bool negate) {
        if (expr.operator_kind() == scalar::unary_operator::conditional_not) {
            dispatch(expr.operand(), std::move(ref), !negate);
        }
    }

    void operator()(scalar::compare& expr, expression_ref&& ref, bool negate) {
        if (auto key = keys_.extract_if(expr.left()); key && !keys_.contains(expr.right())) {
            factor_info_map_.emplace(
                    *key,
                    factor_info {
                            transform(expr.operator_kind(), negate, false),
                            std::move(ref),
                            expr.ownership_right(),
                    });
            return;
        }
        if (auto key = keys_.extract_if(expr.right()); key && !keys_.contains(expr.left())) {
            factor_info_map_.emplace(
                    *key,
                    factor_info {
                            transform(expr.operator_kind(), negate, true),
                            std::move(ref),
                            expr.ownership_left(),
                    });
            return;
        }
    }

    // FIXME: variable reference of boolean columns: e.g. "WHERE NOT TBL.BOOLEAN_COLUMN"

private:
    key_variable_detector keys_;
    factor_info_map& factor_info_map_;

    void dispatch(scalar::expression& expr, expression_ref&& ref, bool negate = false) {
        scalar::dispatch(*this, expr, std::move(ref), negate);
    }

    constexpr scalar::comparison_operator transform(scalar::comparison_operator v, bool negation, bool transposition) {
        if (negation) {
            return transform(~v, false, transposition);
        }
        if (transposition) {
            return transform(transpose(v), false, false);
        }
        return v;
    }
};


} // namespace

void search_key_term_builder::reserve_keys(std::size_t count) {
    keys_.reserve(count);
}

void search_key_term_builder::add_key(descriptor::variable key) {
    check_before_build();
    keys_.emplace(std::move(key));
}

void search_key_term_builder::add_predicate(expression_ref predicate) {
    check_before_build();
    factor_collector collector { keys_, factors_ };
    collector(std::move(predicate));
}

bool search_key_term_builder::empty() const noexcept {
    return factors_.empty() && terms_.empty();
}

optional_ptr<search_key_term> search_key_term_builder::find(descriptor::variable const& key) {
    if (!factors_.empty()) {
        build_terms();
    }
    if (auto it = terms_.find(key); it != terms_.end()) {
        return it->second;
    }
    return {};
}

std::pair<term_map::iterator, term_map::iterator> search_key_term_builder::search(descriptor::variable const& key) {
    if (!factors_.empty()) {
        build_terms();
    }
    auto [begin, end] = terms_.equal_range(key);
    return std::make_pair(begin, end);
}

void search_key_term_builder::clear() {
    keys_.clear();
    factors_.clear();
    terms_.clear();
}

void search_key_term_builder::check_before_build() {
    if (!terms_.empty()) {
        throw_exception(std::domain_error("cannot reconfigure after build"));
    }
}

void search_key_term_builder::build_terms() {
    terms_.reserve(keys_.size());
    for (auto&& v : keys_) {
        auto [begin, end] = factors_.equal_range(v);
        build_term(begin, end);
    }
    factors_.clear();
}

void search_key_term_builder::build_term(factor_info_map::iterator begin, factor_info_map::iterator end) {
    optional_ptr<descriptor::variable const> key_variable {};
    std::optional<expression_ref> lower_term {};
    std::optional<expression_ref> lower_factor {};
    bool lower_inclusive {};
    std::optional<expression_ref> upper_term {};
    std::optional<expression_ref> upper_factor {};
    bool upper_inclusive {};
    for (auto it = begin; it != end; ++it) {
        auto&& info = it->second;
        using kind = scalar::comparison_operator;
        switch (info.operator_kind) {
            case kind::equal:
                // equivalent
                terms_.emplace(
                        it->first,
                        search_key_term {
                                std::move(info.term),
                                std::move(info.factor),
                        });
                break;

            case kind::less:
            case kind::less_equal:
                // k > N :: lower bound
                if (upper_term) {
                    break; // already set
                }
                key_variable = it->first;
                upper_term.emplace(std::move(info.term));
                upper_factor.emplace(std::move(info.factor));
                upper_inclusive = (info.operator_kind == kind::less_equal);
                break;

            case kind::greater:
            case kind::greater_equal:
                // k < N :: upper bound
                if (lower_term) {
                    break; // already set
                }
                key_variable = it->first;
                lower_term.emplace(std::move(info.term));
                lower_factor.emplace(std::move(info.factor));
                lower_inclusive = (info.operator_kind == kind::greater_equal);
                break;

            case kind::not_equal:
                // unsupported
                break;
        }
    }

    if (key_variable) {
        // range
        terms_.emplace(
                *key_variable,
                search_key_term {
                        std::move(lower_term),
                        std::move(lower_factor),
                        lower_inclusive,
                        std::move(upper_term),
                        std::move(upper_factor),
                        upper_inclusive,
                });
    }
}

} // namespace yugawara::analyzer::details

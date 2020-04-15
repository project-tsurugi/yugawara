#include "push_down_filters.h"

#include <stdexcept>

#include <cassert>

#include <boost/dynamic_bitset.hpp>

#include <tsl/hopscotch_set.h>

#include <takatori/type/boolean.h>
#include <takatori/value/boolean.h>
#include <takatori/scalar/immediate.h>
#include <takatori/scalar/binary.h>
#include <takatori/relation/intermediate/dispatch.h>

#include <takatori/util/clonable.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/string_builder.h>

#include "decompose_predicate.h"
#include "collect_stream_variables.h"
#include "stream_variable_flow_info.h"

namespace yugawara::analyzer::details {

namespace descriptor = ::takatori::descriptor;
namespace scalar = ::takatori::scalar;
namespace relation = ::takatori::relation;

using ::takatori::util::clone_unique;
using ::takatori::util::enum_tag;
using ::takatori::util::enum_tag_t;
using ::takatori::util::object_ownership_reference;
using ::takatori::util::unique_object_ptr;
using ::takatori::util::string_builder;

namespace {

[[nodiscard]] unique_object_ptr<scalar::expression> true_expression(::takatori::util::object_creator creator) {
    static auto boolean_type = std::allocate_shared<::takatori::type::boolean>(std::allocator<::takatori::type::boolean> {});
    static auto boolean_value = std::allocate_shared<::takatori::value::boolean>(std::allocator<::takatori::type::boolean> {}, true);
    return creator.create_unique<scalar::immediate>(boolean_value, boolean_type);
}

class predicate_info {
public:
    using index_type = std::size_t;
    using reference_count_type = std::size_t;

    explicit predicate_info(
            index_type index,
            object_ownership_reference<scalar::expression> expression,
            ::takatori::util::object_creator creator)
        : index_(index)
        , expression_(std::move(expression))
        , uses_(creator.allocator())
    {
        collect_stream_variables(*expression_, [this](descriptor::variable const& v) {
            uses_.emplace(v);
        });
    }

    [[nodiscard]] index_type index() const noexcept {
        return index_;
    }

    [[nodiscard]] auto& uses() noexcept {
        return uses_;
    }

    [[nodiscard]] auto& uses() const noexcept {
        return uses_;
    }

    [[nodiscard]] bool use(descriptor::variable const& variable) const {
        return uses_.find(variable) != uses_.end();
    }

    [[nodiscard]] bool available() const noexcept {
        return reference_count_ != 0;
    }

    void add_count(reference_count_type count = 1) noexcept {
        reference_count_ += count;
    }

    void leave() {
        assert(available()); // NOLINT
        --reference_count_;
    }

    [[nodiscard]] unique_object_ptr<scalar::expression> release() {
        assert(available()); // NOLINT
        if (--reference_count_ == 0) {
            auto result = clone_unique(std::move(*expression_), get_object_creator());
            expression_ = true_expression(get_object_creator());
            return result;
        }
        uses_.clear();
        return clone_unique(*expression_, get_object_creator());
    }

    [[nodiscard]] ::takatori::util::object_creator get_object_creator() const noexcept {
        return uses_.get_allocator().resource();
    }

private:
    index_type index_;
    object_ownership_reference<scalar::expression> expression_;
    ::tsl::hopscotch_set<
            descriptor::variable,
            std::hash<descriptor::variable>,
            std::equal_to<>,
            ::takatori::util::object_allocator<descriptor::variable>> uses_;
    reference_count_type reference_count_ { 1 };
};

// NOTE: avoid error of Boost polymorphic_allocator, this is equivalent to std::pair
class task_info {
public:
    using mask_type = ::boost::dynamic_bitset<std::uintptr_t, ::takatori::util::object_allocator<std::uintptr_t>>;

    explicit task_info(relation::expression& expression, mask_type&& mask)
        : expression_(std::addressof(expression))
        , mask_(std::move(mask))
    {}

    [[nodiscard]] relation::expression& expression() noexcept {
        return *expression_;
    }

    [[nodiscard]] mask_type& mask() noexcept {
        return mask_;
    }

private:
    relation::expression* expression_;
    mask_type mask_;
};

class engine {
public:
    using mask_type = task_info::mask_type;
    using task_type = task_info;

    explicit engine(relation::graph_type& graph, ::takatori::util::object_creator creator) noexcept
        : graph_(graph)
        , tasks_(creator.allocator())
        , predicates_(creator.allocator())
        , flow_info_(creator)
    {}

    void process() {
        for (auto&& e : graph_) {
            if (e.output_ports().empty()) {
                schedule(e, empty_mask());
            }
        }
        while (!tasks_.empty()) {
            auto task = std::move(tasks_.back());
            tasks_.pop_back();
            relation::intermediate::dispatch(*this, task.expression(), std::move(task.mask()));
        }
    }

    void operator()(relation::find& expr, mask_type&& mask) {
        flush_all(expr.output(), mask);
        // no more upstreams
    }

    void operator()(relation::scan& expr, mask_type&& mask) {
        flush_all(expr.output(), mask);
        // no more upstreams
    }

    void operator()(relation::intermediate::join& expr, mask_type&& mask) {
        relation::dispatch(*this, expr.operator_kind(), expr, std::move(mask));
    }

    void operator()(enum_tag_t<relation::join_kind::inner>, relation::intermediate::join& expr, mask_type&& mask) {
        // remain incoming predicates
        auto incoming_size = predicates_.size();

        // inner joins can consider the join condition terms as same as incoming terms
        analyze_predicates(expr.ownership_condition(), mask);

        // compute if each term is applicable on left/right upstream
        auto left_mask = create_input_mask(expr.left(), mask);
        auto right_mask = create_input_mask(expr.right(), mask);

        // flush incoming terms
        for (mask_type::size_type i = mask.find_first(); i != mask_type::npos; i = mask.find_next(i)) {
            auto&& predicate = predicates_[i];
            bool incoming = i < incoming_size;
            bool l = left_mask[i];
            bool r = right_mask[i];
            if (l && r) {
                // L & R -  carry each
                predicate.add_count();
                mask.reset(i);
            } else if (l || r) {
                // L | R - just carry to left/right
                mask.reset(i);
            } else {
                // - push down to the join condition
                if (incoming) {
                    // schedule push into the condition
                } else {
                    // already in the condition
                    predicate.leave();
                    mask.reset(i);
                }
            }
        }
        // merge all incoming terms into join condition
        merge_predicates(expr.ownership_condition(), std::move(mask));

        // carry to upstream
        pass(expr.left(), std::move(left_mask));
        pass(expr.right(), std::move(right_mask));
    }

    void operator()(enum_tag_t<relation::join_kind::left_outer>, relation::intermediate::join& expr, mask_type&& mask) {
        // NOTE: outer join does not decompose the join condition

        // compute if each term is applicable on left upstream
        auto left_mask = create_input_mask(expr.left(), mask);

        // push down incoming terms to the join condition
        for (mask_type::size_type i = mask.find_first(); i != mask_type::npos; i = mask.find_next(i)) {
            auto&& predicate = predicates_[i];
            bool l = left_mask[i];
            if (l) {
                // L - just carry to left
                mask.reset(i);
            } else {
                // schedule push down into the condition, and also keep the original one
                flush_copy(expr.output(), predicate);
            }
        }
        // merge all incoming terms into join condition
        merge_predicates(expr.ownership_condition(), std::move(mask));

        // carry to left
        pass(expr.left(), std::move(left_mask));
        pass(expr.right(), empty_mask());
    }

    void operator()(enum_tag_t<relation::join_kind::full_outer>, relation::intermediate::join& expr, mask_type&& mask) {
        // NOTE: outer join does not decompose the join condition

        // push down incoming terms to the join condition
        for (mask_type::size_type i = mask.find_first(); i != mask_type::npos; i = mask.find_next(i)) {
            auto&& predicate = predicates_[i];
            // schedule push down into the condition, and also keep the original one
            flush_copy(expr.output(), predicate);
        }
        // merge all incoming terms into join condition
        merge_predicates(expr.ownership_condition(), std::move(mask));

        pass(expr.left(), empty_mask());
        pass(expr.right(), empty_mask());
    }

    void operator()(enum_tag_t<relation::join_kind::semi>, relation::intermediate::join& expr, mask_type&& mask) {
        // same as inner join
        operator()(enum_tag<relation::join_kind::inner>, expr, std::move(mask));
    }

    void operator()(enum_tag_t<relation::join_kind::anti>, relation::intermediate::join& expr, mask_type&& mask) {
        // FIXME: verify algorithm
        // same as left anti semi join
        operator()(enum_tag<relation::join_kind::left_outer>, expr, std::move(mask));
    }

    void operator()(relation::join_find& expr, mask_type&& mask) {
        // NOTE: index join is not supported here, it will be generated in the later optimization phase
        flush_all(expr.output(), mask);
        pass(expr.left(), empty_mask());
    }

    void operator()(relation::join_scan& expr, mask_type&& mask) {
        // NOTE: index join is not supported here, it will be generated in the later optimization phase
        flush_all(expr.output(), mask);
        pass(expr.left(), empty_mask());
    }

    void operator()(relation::project& expr, mask_type&& mask) {
        for (auto&& column : expr.columns()) {
            for (mask_type::size_type i = mask.find_first(); i != mask_type::npos; i = mask.find_next(i)) {
                auto&& predicate = predicates_[i];
                if (predicate.use(column.variable())) {
                    flush(expr.output(), predicate);
                    mask.reset(i);
                }
            }
        }
        pass(expr.input(), std::move(mask));
    }

    void operator()(relation::filter& expr, mask_type&& mask) {
        analyze_predicates(expr.ownership_condition(), mask);
        pass(expr.input(), std::move(mask));
    }

    void operator()(relation::intermediate::aggregate& expr, mask_type&& mask) {
        assert(work_.empty()); // NOLINT
        work_.reserve(expr.group_keys().size());
        for (auto&& key : expr.group_keys()) {
            work_.emplace(key);
        }
        // check if each term only use the aggregate group key
        for (mask_type::size_type i = mask.find_first(); i != mask_type::npos; i = mask.find_next(i)) {
            auto&& predicate = predicates_[i];
            for (auto&& use : predicate.uses()) {
                if (work_.find(use) == work_.end()) {
                    // found use other than the group key, just flush
                    flush(expr.output(), predicate);
                    mask.reset(i);
                    break;
                }
            }
        }
        work_.clear();
        pass(expr.input(), std::move(mask));
    }

    void operator()(relation::intermediate::distinct& expr, mask_type&& mask) {
        process_distinct_like(expr, std::move(mask));
    }

    void operator()(relation::intermediate::limit& expr, mask_type&& mask) {
        process_distinct_like(expr, std::move(mask));
    }

    void operator()(relation::intermediate::union_& expr, mask_type&& mask) {
        // FIXME: impl
        // rewrite variables and carry into left or right
        flush_all(expr.output(), mask);
        pass(expr.left(), empty_mask());
        pass(expr.right(), empty_mask());
    }

    void operator()(relation::intermediate::intersection& expr, mask_type&& mask) {
        // FIXME: check for distinct
        pass(expr.left(), std::move(mask));
        pass(expr.right(), empty_mask());
    }

    void operator()(relation::intermediate::difference& expr, mask_type&& mask) {
        // FIXME: check for distinct
        pass(expr.left(), std::move(mask));
        pass(expr.right(), empty_mask());
    }

    void operator()(relation::emit& expr, mask_type&& mask) {
        pass(expr.input(), std::move(mask));
    }

    void operator()(relation::write& expr, mask_type&& mask) {
        pass(expr.input(), std::move(mask));
    }

    void operator()(relation::intermediate::escape& expr, mask_type&& mask) {
        // selection cannot push over escape
        flush_all(expr.output(), mask);
        pass(expr.input(), std::move(mask));
    }

    void operator()(relation::buffer& expr, mask_type&&) {
        throw std::domain_error(string_builder {}
                << "unsupported operator: "
                << expr
                << string_builder::to_string);
    }

    [[nodiscard]] ::takatori::util::object_creator get_object_creator() const noexcept {
        return tasks_.get_allocator().resource();
    }

private:
    relation::graph_type& graph_;
    std::vector<task_type, ::takatori::util::object_allocator<task_type>> tasks_;
    std::vector<predicate_info, ::takatori::util::object_allocator<predicate_info>> predicates_;
    stream_variable_flow_info flow_info_; // FIXME: reuse flow info?
    ::tsl::hopscotch_set<
            descriptor::variable,
            std::hash<descriptor::variable>,
            std::equal_to<>,
            ::takatori::util::object_allocator<descriptor::variable>> work_;


    void schedule(relation::expression& expr, mask_type&& mask) {
        tasks_.emplace_back(expr, std::move(mask));
    }

    void pass(relation::expression::input_port_type& upstream, mask_type&& mask) {
        if (!upstream.opposite()) {
            throw std::domain_error(string_builder {}
                    << "orphaned port: "
                    << upstream
                    << string_builder::to_string);
        }
        auto&& next = upstream.opposite()->owner();

        // NOTE: always flush all predicates before buffer operators, and never process them
        if (next.kind() == relation::buffer::tag) {
            flush_all(*upstream.opposite(), mask);
            return;
        }

        // reschedule with the upstream operator
        schedule(next, std::move(mask));
    }

    mask_type empty_mask() {
        return mask_type { get_object_creator().allocator() };
    }

    void analyze_predicates(object_ownership_reference<scalar::expression>&& expr, mask_type& mask) {
        if (expr) {
            mask.resize(predicates_.size(), false);
            decompose_predicate(std::move(expr), [this](object_ownership_reference<scalar::expression>&& term) {
                predicates_.emplace_back(predicates_.size(), std::move(term), get_object_creator());
            });
            mask.resize(predicates_.size(), true);
        }
    }

    void flush(relation::expression::output_port_type& upstream, predicate_info& predicate) {
        auto&& selection = graph_.emplace<relation::filter>(predicate.release());
        auto&& downstream = *upstream.opposite();
        upstream.disconnect_from(downstream);
        selection.input().connect_to(upstream);
        selection.output().connect_to(downstream);
    }

    void flush_copy(relation::expression::output_port_type& upstream, predicate_info& predicate) {
        predicate.add_count();
        flush(upstream, predicate);
    }

    void flush_all(relation::expression::output_port_type& upstream, mask_type& mask) {
        for (mask_type::size_type i = mask.find_first(); i != mask_type::npos; i = mask.find_next(i)) {
            flush(upstream, predicates_[i]);
        }
        mask.clear();
    }

    void merge_predicates(object_ownership_reference<scalar::expression> expr, mask_type&& mask) {
        if (mask.none()) {
            return;
        }
        unique_object_ptr<scalar::expression> result;
        for (mask_type::size_type i = mask.find_first(); i != mask_type::npos; i = mask.find_next(i)) {
            auto&& predicate = predicates_[i];
            if (result) {
                result = get_object_creator().create_unique<scalar::binary>(
                        scalar::binary_operator::conditional_and,
                        std::move(result),
                        predicate.release());
            } else {
                result = predicate.release();
            }
        }
        expr = std::move(result);
        mask.clear();
    }

    mask_type create_input_mask(relation::expression::input_port_type const& input, mask_type& mask) {
        auto result = empty_mask();
        result.resize(mask.size(), false);
        for (mask_type::size_type i = mask.find_first(); i != mask_type::npos; i = mask.find_next(i)) {
            auto&& predicate = predicates_[i];
            bool declared = true;
            for (auto&& use : predicate.uses()) {
                if (!flow_info_.find(use, input)) {
                    declared = false;
                    break;
                }
            }
            result.set(i, declared);
        }
        return result;
    }

    template<class Expr>
    void process_distinct_like(Expr& expr, mask_type&& mask) {
        // check if each term only use the distinct group key
        assert(work_.empty()); // NOLINT
        work_.reserve(expr.group_keys().size());
        for (auto&& key : expr.group_keys()) {
            work_.emplace(key);
        }
        for (mask_type::size_type i = mask.find_first(); i != mask_type::npos; i = mask.find_next(i)) {
            auto&& predicate = predicates_[i];
            for (auto&& use : predicate.uses()) {
                if (work_.find(use) == work_.end()) {
                    // found use other than the group key, put a copy of selection
                    flush_copy(expr.output(), predicate);
                    break;
                }
            }
        }
        work_.clear();
        pass(expr.input(), std::move(mask));
    }
};

} // namespace

void push_down_selections(relation::graph_type& graph, ::takatori::util::object_creator creator) {
    engine e { graph, creator };
    e.process();
}

} // namespace yugawara::analyzer::details

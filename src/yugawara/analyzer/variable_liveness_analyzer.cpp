#include <yugawara/analyzer/variable_liveness_analyzer.h>

#include <tsl/hopscotch_map.h>

#include <takatori/scalar/dispatch.h>
#include <takatori/scalar/walk.h>

#include <takatori/relation/intermediate/dispatch.h>
#include <takatori/relation/step/dispatch.h>

#include <takatori/util/assertion.h>
#include <takatori/util/exception.h>
#include <takatori/util/string_builder.h>

#include <yugawara/binding/extract.h>

namespace yugawara::analyzer {

using block_info = variable_liveness_info;
using block_info_map = variable_liveness_analyzer::info_map;

namespace scalar = ::takatori::scalar;
namespace relation = ::takatori::relation;

using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;

namespace {

bool is_definable(::takatori::descriptor::variable const& v) {
    static constexpr binding::variable_info_kind_set definables {
            binding::variable_info_kind::stream_variable,
            binding::variable_info_kind::frame_variable,
            binding::variable_info_kind::local_variable,
    };
    return definables.contains(binding::kind_of(v));
}

class define_use_analyzer {
public:
    explicit constexpr define_use_analyzer(
            block_info_map& blocks,
            bool record_define,
            bool record_use) noexcept:
        blocks_ { blocks },
        record_define_ { record_define },
        record_use_ { record_use }
    {}

    void operator()() {
        for (auto&& [block_ptr, info] : blocks_) {
            for (auto&& r : *block_ptr) {
                if (!relation::is_available_in_step_plan(r.kind())) {
                    throw_exception(std::domain_error(string_builder {}
                            << "unsupported relational operator (only step plan): "
                            << r.kind()
                            << string_builder::to_string));
                }
                relation::step::dispatch(*this, r, info);
            }
        }
    }

    constexpr void operator()(scalar::expression const&, block_info&) noexcept {}

    void operator()(scalar::let const& expr, block_info& info) {
        for (auto&& decl : expr.variables()) {
            define(decl.variable(), info);
        }
    }

    void operator()(scalar::variable_reference const& expr, block_info& info) {
        use(expr.variable(), info);
    }

    void operator()(relation::find const& expr, block_info& info) {
        for (auto&& column : expr.columns()) {
            use(column.source(), info);
            define(column.destination(), info);
        }
        for (auto&& key : expr.keys()) {
            scalar::walk(*this, key.value(), info);
        }
    }

    void operator()(relation::scan const& expr, block_info& info) {
        for (auto&& column : expr.columns()) {
            use(column.source(), info);
            define(column.destination(), info);
        }
        for (auto&& key : expr.lower().keys()) {
            scalar::walk(*this, key.value(), info);
        }
        for (auto&& key : expr.upper().keys()) {
            scalar::walk(*this, key.value(), info);
        }
    }

    void operator()(relation::join_find const& expr, block_info& info) {
        for (auto&& column : expr.columns()) {
            use(column.source(), info);
            define(column.destination(), info);
        }
        for (auto&& key : expr.keys()) {
            scalar::walk(*this, key.value(), info);
        }
        if (expr.condition()) {
            scalar::walk(*this, *expr.condition(), info);
        }
    }

    void operator()(relation::values const& expr, block_info& info) {
        for (auto&& row : expr.rows()) {
            for (auto&& e : row.elements()) {
                scalar::walk(*this, e, info);
            }
        }
        for (auto&& column : expr.columns()) {
            define(column, info);
        }
    }

    void operator()(relation::join_scan const& expr, block_info& info) {
        for (auto&& column : expr.columns()) {
            use(column.source(), info);
            define(column.destination(), info);
        }
        for (auto&& key : expr.lower().keys()) {
            scalar::walk(*this, key.value(), info);
        }
        for (auto&& key : expr.upper().keys()) {
            scalar::walk(*this, key.value(), info);
        }
        if (expr.condition()) {
            scalar::walk(*this, *expr.condition(), info);
        }
    }

    void operator()(relation::apply const& expr, block_info& info) {
        for (auto&& argument : expr.arguments()) {
            scalar::walk(*this, argument, info);
        }
        for (auto&& column : expr.columns()) {
            define(column.variable(), info);
        }
    }

    void operator()(relation::project const& expr, block_info& info) {
        for (auto&& column : expr.columns()) {
            define(column.variable(), info);
            scalar::walk(*this, column.value(), info);
        }
    }

    void operator()(relation::filter const& expr, block_info& info) {
        scalar::walk(*this, expr.condition(), info);
    }

    void operator()(relation::buffer const&, block_info&) {
        // no definitions & uses
    }

    void operator()(relation::identify const& expr, block_info& info) {
        define(expr.variable(), info);
    }

    void operator()(relation::emit const& expr, block_info& info) {
        for (auto&& column : expr.columns()) {
            use(column.source(), info);
        }
    }

    void operator()(relation::write const& expr, block_info& info) {
        // NOTE: we does not include expr.columns().destination() into "define",
        // because this does not actually define the column
        for (auto&& key : expr.keys()) {
            use(key.source(), info);
        }
        for (auto&& column : expr.columns()) {
            use(column.source(), info);
        }
    }

    void operator()(relation::step::join const& expr, block_info& info) {
        if (expr.condition()) {
            scalar::walk(*this, *expr.condition(), info);
        }
    }

    void operator()(relation::step::aggregate const& expr, block_info& info) {
        for (auto&& column : expr.columns()) {
            for (auto&& arg : column.arguments()) {
                use(arg, info);
            }
            define(column.destination(), info);
        }
    }

    void operator()(relation::step::intersection const&, block_info&) {
        // no definitions or uses
    }

    void operator()(relation::step::difference const&, block_info&) {
        // no definitions or uses
    }

    void operator()(relation::step::flatten const&, block_info&) {
        // no definitions or uses
    }

    void operator()(relation::step::take_flat const& expr, block_info& info) {
        for (auto&& column : expr.columns()) {
            use(column.source(), info);
            define(column.destination(), info);
        }
    }

    void operator()(relation::step::take_group const& expr, block_info& info) {
        for (auto&& column : expr.columns()) {
            use(column.source(), info);
            define(column.destination(), info);
        }
    }

    void operator()(relation::step::take_cogroup const& expr, block_info& info) {
        for (auto&& group : expr.groups()) {
            for (auto&& column : group.columns()) {
                use(column.source(), info);
                define(column.destination(), info);
            }
        }
    }

    void operator()(relation::step::offer const& expr, block_info& info) {
        // NOTE: we does not include expr.columns().destination() into "define",
        // because this does not actually define the column
        for (auto&& column : expr.columns()) {
            use(column.source(), info);
        }
    }

private:
    block_info_map& blocks_;
    bool record_define_;
    bool record_use_;

    void define(::takatori::descriptor::variable const& variable, block_info& info) const {
        if (record_define_ && is_definable(variable)) {
            info.define().emplace(variable);
        }
    }

    void use(::takatori::descriptor::variable const& variable, block_info& info) const {
        if (record_use_) {
            info.use().emplace(variable);
        }
    }
};

struct liveness {
    block const* const define {};
    block const* use {};
    bool inherited {};
};

using liveness_map = tsl::hopscotch_map<
        std::reference_wrapper<::takatori::descriptor::variable const>,
        liveness,
        std::hash<::takatori::descriptor::variable>,
        std::equal_to<::takatori::descriptor::variable>>; // NOLINT(modernize-use-transparent-functors)

class kill_analyzer {
public:
    explicit kill_analyzer(block_info_map& blocks) noexcept : blocks_(blocks) {}

    void operator()() {
        liveness_map lvs {};
        block const* first {};
        for (auto&& [bp, info] : blocks_) {
            (void) info;
            if (bp->upstreams().empty()) {
                if (first == nullptr) {
                    first = bp;
                } else {
                    throw_exception(std::domain_error("multiple entries"));
                }
            }
        }
        process(first, lvs);
        if (!lvs.empty()) {
            throw_exception(std::domain_error("unhandled variable"));
        }
    }

private:
    block_info_map& blocks_;

    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    void process(block const* bp, liveness_map& lvs) {
        auto&& info = get_info(bp);

        // collect all defined variables
        for (auto&& v : info.define()) {
            auto [iter, success] = lvs.try_emplace(v, liveness { bp }); // define here
            (void) iter;
            if (!success) {
                throw_exception(std::domain_error(string_builder {}
                        << "multiple definition: " << v
                        << " in block " << bp->front()
                        << string_builder::to_string));
            }
        }

        // marks all used blocks
        for (auto&& variable : info.use()) {
            if (is_definable(variable)) {
                if (auto it = lvs.find(variable); it != lvs.end()) {
                    auto&& liveness = it.value();
                    liveness.use = bp;
                } else {
                    throw_exception(std::domain_error(string_builder {}
                            << "undefined variable: " << variable
                            << " in block " << bp->front()
                            << string_builder::to_string));
                }
            }
        }

        auto succs = bp->downstreams();
        if (succs.empty()) {
            // kill each variable if it is not used in this *tail* block
            for (auto iter = lvs.begin(); iter != lvs.end();) {
                auto&& variable = iter.key();
                auto&& liveness = iter.value();
                if (liveness.use != bp && !liveness.inherited) {
                    info.kill().emplace(variable);
                    // remove killed entry
                    iter = lvs.erase(iter);
                } else {
                    ++iter;
                }
            }
        } else {
            // block may end with buffer operator (succs.size() >= 2).
            // NOTE: in current implementation, we never kill variables declared before this block including this.
            std::vector<liveness_map> branches {};
            branches.reserve(succs.size());
            for (auto&& succ : succs) {
                auto const* succp = std::addressof(succ);
                // mark all defined variables as "inherited" to avoid killing in branches
                auto&& succ_lvs = branches.emplace_back(lvs);
                for (auto iter = succ_lvs.begin(); iter != succ_lvs.end(); ++iter) {
                    auto&& liveness = iter.value();
                    liveness.use = nullptr;
                    liveness.inherited = true;
                }
                process(succp, succ_lvs);
            }
            // inherit use info from branches
            for (auto&& succ_lvs : branches) {
                for (auto succ_lvs_iter = succ_lvs.begin(); succ_lvs_iter != succ_lvs.end(); ++succ_lvs_iter) {
                    auto&& variable = succ_lvs_iter.key();
                    auto&& succ_liveness = succ_lvs_iter.value();
                    // NOTE: may mark kill if the inherited variable is not used in individual branches
                    if (!succ_liveness.inherited || succ_liveness.use == nullptr) {
                        continue;
                    }
                    if (auto iter = lvs.find(variable); iter != lvs.end()) {
                        auto&& decl_liveness = iter.value();
                        if (decl_liveness.use != succ_liveness.use) {
                            decl_liveness.use = succ_liveness.use;
                        }
                    }
                }
            }
        }

        // remove liveness map entries defined in this block
        for (auto iter = lvs.begin(); iter != lvs.end();) {
            auto&& variable = iter.key();
            auto&& liveness = iter.value();
            if (liveness.define == bp) {
                // kill variable if no one use this variable
                if (liveness.use == nullptr) {
                    info.kill().emplace(variable);
                }
                iter = lvs.erase(iter);
            } else {
                ++iter;
            }
        }
    }

    block_info& get_info(block const* bp) {
        auto it = blocks_.find(bp);
        if (it == blocks_.end()) {
            throw_exception(std::domain_error(string_builder {}
                    << "unregistered block: " << bp->front()
                    << string_builder::to_string));
        }
        return it->second;
    }
};

} // namespace

variable_liveness_analyzer::variable_liveness_analyzer(::takatori::graph::graph<block> const& blocks)
{
    blocks_.reserve(blocks.size());
    for (auto&& block : blocks) {
        blocks_.emplace(std::addressof(block), info {});
    }
}

variable_liveness_analyzer::info& variable_liveness_analyzer::inspect(block const& target, kind_set require) {
    auto it = blocks_.find(std::addressof(target));
    if (it == blocks_.end()) {
        throw_exception(std::invalid_argument("block is out of scope"));
    }
    auto&& r = it->second;

    // unset requests if already resolved
    require -= resolved_;

    if (!require.empty()) {
        // "kill" requires "define" and "use"
        if (require[kind_type::kill]) {
            require[kind_type::define] = true;
            require[kind_type::use] = true;
        }
        if (require[kind_type::define] || require[kind_type::use]) {
            define_use_analyzer analyzer { blocks_, require[kind_type::define], require[kind_type::use] };
            analyzer();
            resolved_[kind_type::define] = true;
            resolved_[kind_type::use] = true;
        }
        if (require[kind_type::kill]) {
            BOOST_ASSERT(resolved_[kind_type::define]); // NOLINT
            BOOST_ASSERT(resolved_[kind_type::use]); // NOLINT

            kill_analyzer analyzer { blocks_ };
            analyzer();
            resolved_[kind_type::kill] = true;
        }
    }
    return r;
}


} // namespace yugawara::analyzer

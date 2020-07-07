#include <yugawara/analyzer/variable_liveness_analyzer.h>

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
            binding::variable_info_kind::local_variable,
    };
    return definables.contains(binding::kind_of(v));
}

class define_use_analyzer {
public:
    explicit constexpr define_use_analyzer(
            block_info_map& blocks,
            bool record_define,
            bool record_use) noexcept
        : blocks_(blocks)
        , record_define_(record_define)
        , record_use_(record_use)
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

    void define(::takatori::descriptor::variable const& variable, block_info& info) {
        if (record_define_ && is_definable(variable)) {
            info.define().emplace(variable);
        }
    }

    void use(::takatori::descriptor::variable const& variable, block_info& info) {
        if (record_use_) {
            info.use().emplace(variable);
        }
    }
};

struct liveness {
    block const* const define {};
    block const* use {};
};

// FIXME: using hopscotch_map is prefer, but some compilation errors will occur around polymorphic_allocator
using liveness_map = std::unordered_map<
        std::reference_wrapper<::takatori::descriptor::variable const>,
        liveness,
        std::hash<::takatori::descriptor::variable>,
        std::equal_to<::takatori::descriptor::variable>, // NOLINT
        ::takatori::util::object_allocator<std::pair<
                std::reference_wrapper<::takatori::descriptor::variable const> const,
                liveness>>>;

class kill_analyzer {
public:
    explicit kill_analyzer(block_info_map& blocks) noexcept : blocks_(blocks) {}

    void operator()() {
        liveness_map lvs {
                blocks_.get_allocator().resource()
        };
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

    // find use in branches for each defined variable
    std::vector<block const*, ::takatori::util::object_allocator<block const*>> still_live {
            blocks_.get_allocator().resource()
    };

    void process(block const* bp, liveness_map& lvs) {
        auto&& info = get_info(bp);

        // collect all defined variables
        for (auto&& v : info.define()) {
            auto [it, success] = lvs.try_emplace(v, liveness { bp }); // define here
            (void) it;
            if (!success) {
                throw_exception(std::domain_error(string_builder {}
                        << "multiple definition: " << v
                        << " in block " << bp->front()
                        << string_builder::to_string));
            }
        }

        // marks all used blocks
        for (auto&& v : info.use()) {
            if (is_definable(v)) {
                if (auto it = lvs.find(v); it != lvs.end()) {
                    auto&& lv = it->second;
                    lv.use = bp;
                } else {
                    throw_exception(std::domain_error(string_builder {}
                            << "undefined variable: " << v
                            << " in block " << bp->front()
                            << string_builder::to_string));
                }
            }
        }

        auto succs = bp->downstreams();
        if (succs.empty()) {
            // kill used blocks
            for (auto it = lvs.begin(); it != lvs.end();) {
                auto&& lv = it->second;
                if (lv.use == bp) { // last use = self
                    // NOTE: we don't record as kill because this is on tail of graph (trivial kill)
                    it = lvs.erase(it);
                } else {
                    ++it;
                }
            }
        } else if (succs.size() == 1) {
            // NOTE: continue to the succeeding block
            auto succp = std::addressof(succs[0]);
            process(succp, lvs);

            // kill used blocks
            auto&& succ_info = get_info(succp);
            for (auto it = lvs.begin(); it != lvs.end();) {
                auto&& lv = it->second;
                if (lv.use == bp) { // last use = self
                    succ_info.kill().emplace(it->first);
                    it = lvs.erase(it);
                } else {
                    ++it;
                }
            }
        } else {
            std::vector<liveness_map, ::takatori::util::object_allocator<liveness_map>> branches {
                    blocks_.get_allocator().resource()
            };
            branches.reserve(succs.size());
            for (auto&& succ : succs) {
                auto* succp = std::addressof(succ);
                process(succp, branches.emplace_back(lvs));
            }
            still_live.reserve(succs.size());
            for (auto it = lvs.begin(); it != lvs.end();) {
                still_live.clear();
                auto&& v = it->first;
                std::size_t index = 0;
                for (auto&& lvs_branch : branches) {
                    if (lvs_branch.find(v) != lvs_branch.end()) {
                        // liveness entry still exists
                        still_live.emplace_back(std::addressof(succs[index]));
                    }
                    ++index;
                }
                // used in all branches
                if (still_live.empty()) {
                    // just remove the entry (already killed in branch)
                    it = lvs.erase(it);
                } else if (still_live.size() < branches.size()) {
                    // kill the first block of branches, ...
                    for (auto* bp_branch : still_live) {
                        auto&& info_branch = get_info(bp_branch);
                        info_branch.kill().emplace(v);
                    }

                    // and remove the entry
                    it = lvs.erase(it);
                } else {
                    // no branches use the variable
                    ++it;
                }
            }
        }

        // kill defined (but not used) variables
        for (auto it = lvs.begin(); it != lvs.end();) {
            auto&& [v, lv] = *it;
            if (lv.define == bp) {
                BOOST_ASSERT(lv.use == nullptr); // NOLINT
                info.kill().emplace(v);
                it = lvs.erase(it);
            } else {
                ++it;
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
    : variable_liveness_analyzer(blocks, blocks.get_object_creator())
{}

variable_liveness_analyzer::variable_liveness_analyzer(
        ::takatori::graph::graph<block> const& blocks,
        ::takatori::util::object_creator creator)
    : blocks_(creator.allocator())
{
    blocks_.reserve(blocks.size());
    for (auto&& block : blocks) {
        blocks_.emplace(std::addressof(block), info { creator });
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

#include "remove_redundant_stream_variables.h"

#include <tsl/hopscotch_set.h>

#include <takatori/scalar/walk.h>
#include <takatori/relation/intermediate/dispatch.h>

#include <takatori/util/assertion.h>
#include <takatori/util/exception.h>
#include <takatori/util/string_builder.h>

#include <yugawara/binding/extract.h>

namespace yugawara::analyzer::details {

namespace descriptor = ::takatori::descriptor;
namespace scalar = ::takatori::scalar;
namespace relation = ::takatori::relation;

using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;

namespace {

class engine {
public:
    explicit engine(::takatori::util::object_creator creator)
        : used_(creator.allocator())
    {}

    void process(relation::graph_type& graph) {
        relation::sort_from_downstream(
                graph,
                [this](relation::expression& expr) {
                    relation::intermediate::dispatch(*this, expr);
                },
                used_.get_allocator().resource());
    }

    void operator()(relation::expression const& expr) {
        throw_exception(std::domain_error(string_builder {}
                << "unsupported relation expression: "
                << expr
                << string_builder::to_string));
    }

    void operator()(relation::find& expr) {
        remove_unused_mappings(expr.columns());
        // key must not contain variables declared here
    }

    void operator()(relation::scan& expr) {
        remove_unused_mappings(expr.columns());
        // endpoints must not contain variables declared here
    }

    void operator()(relation::intermediate::join& expr) {
        collect(expr.condition());
        collect_keys(expr.lower().keys());
        collect_keys(expr.upper().keys());
    }

    void operator()(relation::join_find& expr) {
        collect(expr.condition());
        remove_unused_mappings(expr.columns());
        collect_keys(expr.keys());
    }

    void operator()(relation::join_scan& expr) {
        collect(expr.condition());
        remove_unused_mappings(expr.columns());
        collect_keys(expr.lower().keys());
        touch_search_keys(expr.lower().keys());
        collect_keys(expr.upper().keys());
        touch_search_keys(expr.upper().keys());
    }

    void operator()(relation::project& expr) {
        remove_unused_declarators(expr.columns());
    }

    void operator()(relation::filter& expr) {
        collect(expr.condition());
    }

    constexpr void operator()(relation::buffer const&) noexcept {
        // no use/defs
    }

    void operator()(relation::intermediate::aggregate& expr) {
        if (!expr.columns().empty()) {
            remove_unused_mappings(expr.columns());
        }
        for (auto&& column : expr.columns()) {
            touch_variables(column.arguments());
        }
        touch_variables(expr.group_keys());
    }

    void operator()(relation::intermediate::distinct& expr) {
        touch_variables(expr.group_keys());
    }

    void operator()(relation::intermediate::limit& expr) {
        touch_variables(expr.group_keys());
        touch_sort_keys(expr.sort_keys());
    }

    void operator()(relation::intermediate::union_& expr) {
        remove_unused_mappings(expr.mappings());
        for (auto&& mapping : expr.mappings()) {
            if (auto v = mapping.left()) {
                use(*v);
            }
            if (auto v = mapping.right()) {
                use(*v);
            }
        }
    }

    void operator()(relation::intermediate::intersection& expr) {
        touch_key_pairs(expr.group_key_pairs());
    }

    void operator()(relation::intermediate::difference& expr) {
        touch_key_pairs(expr.group_key_pairs());
    }

    void operator()(relation::emit const& expr) {
        touch_mappings(expr.columns());
    }

    void operator()(relation::write const& expr) {
        touch_mappings(expr.keys());
        touch_mappings(expr.columns());
    }

    void operator()(relation::intermediate::escape& expr) {
        remove_unused_mappings(expr.mappings());
        touch_mappings(expr.mappings());
    }

    // scalar expressions

    constexpr void operator()(scalar::expression const&) noexcept {}

    void operator()(scalar::variable_reference const& expr) {
        auto kind = binding::kind_of(expr.variable());
        if (kind == binding::variable_info_kind::stream_variable
                || kind == binding::variable_info_kind::local_variable) {
            used_.emplace(expr.variable());
        }
    }

    bool operator()(scalar::let & expr) {
        collect(expr.body());
        remove_unused_declarators(expr.variables());
        return false; // suppress auto walk
    }


private:
    tsl::hopscotch_set<
            descriptor::variable,
            std::hash<descriptor::variable>,
            std::equal_to<>,
            ::takatori::util::object_allocator<descriptor::variable>> used_;

    void use(descriptor::variable variable) {
        used_.emplace(std::move(variable));
    }

    bool is_used(descriptor::variable const& variable) {
        return used_.find(variable) != used_.end();
    }

    void collect(scalar::expression& expr) {
        scalar::walk(*this, expr);
    }

    void collect(::takatori::util::optional_ptr<scalar::expression> expr) {
        if (expr) {
            collect(*expr);
        }
    }

    template<class Keys>
    void collect_keys(Keys& keys) {
        for (auto&& key : keys) {
            collect(key.value());
        }
    }

    template<class Columns>
    void remove_unused_mappings(Columns& columns) {
        BOOST_ASSERT(!columns.empty()); // NOLINT
        for (auto&& it = columns.begin(); it != columns.end();) {
            if (is_used(it->destination())) {
                ++it;
            } else {
                it = columns.erase(it);
            }
        }
    }

    template<class Declarators>
    void remove_unused_declarators(Declarators& declarators) {
        for (std::size_t ri = 0; ri < declarators.size();) {
            auto i = declarators.size() - ri - 1;
            auto&& decl = declarators[i];
            if (is_used(decl.variable())) {
                collect(decl.value());
                ++ri;
            } else {
                declarators.erase(declarators.begin() + i);
            }
        }
    }

    template<class Variables>
    void touch_variables(Variables& variables) {
        used_.reserve(used_.size() + variables.size());
        for (auto&& variable : variables) {
            use(variable);
        }
    }

    template<class Mappings>
    void touch_mappings(Mappings& mappings) {
        used_.reserve(used_.size() + mappings.size());
        for (auto&& mapping : mappings) {
            use(mapping.source());
        }
    }

    template<class Keys>
    void touch_search_keys(Keys& keys) {
        used_.reserve(used_.size() + keys.size());
        for (auto&& key : keys) {
            use(key.variable());
        }
    }

    template<class Keys>
    void touch_sort_keys(Keys& keys) {
        used_.reserve(used_.size() + keys.size());
        for (auto&& key : keys) {
            use(key.variable());
        }
    }

    template<class Pairs>
    void touch_key_pairs(Pairs& pairs) {
        used_.reserve(used_.size() + pairs.size() * 2);
        for (auto&& pair : pairs) {
            use(pair.left());
            use(pair.right());
        }
    }
};

} // namespace

void remove_redundant_stream_variables(
        ::takatori::relation::graph_type& graph,
        ::takatori::util::object_creator creator) {
    engine e { creator };
    e.process(graph);
}

} // namespace yugawara::analyzer::details

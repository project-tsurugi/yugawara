#include "remove_variable_aliases.h"

#include <tsl/hopscotch_map.h>

#include <takatori/descriptor/variable.h>
#include <takatori/scalar/walk.h>
#include <takatori/relation/intermediate/dispatch.h>

#include <takatori/util/downcast.h>
#include <takatori/util/exception.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/string_builder.h>

#include <yugawara/binding/extract.h>

namespace yugawara::analyzer::details {

namespace descriptor = ::takatori::descriptor;
namespace scalar = ::takatori::scalar;
namespace relation = ::takatori::relation;

using ::takatori::util::optional_ptr;
using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;
using ::takatori::util::unsafe_downcast;

namespace {

class engine {
public:
    explicit engine(
            relation::graph_type& graph,
            bool extract_host_variables) :
        graph_ { graph },
        extract_external_variables_ { extract_host_variables }
    {}

    void process() {
        relation::sort_from_upstream(graph_, [&](relation::expression& expr) {
            relation::intermediate::dispatch(*this, expr);
        });
    }

    void operator()(scalar::expression const& expr) const {
        (void) expr;
    }

    void operator()(scalar::variable_reference& expr) {
        rewrite(expr.variable());
    }

    void operator()(relation::find const& expr) const {
        (void) expr;
        // no-op because most upstream operators can't include aliases
    }

    void operator()(relation::scan const& expr) const {
        (void) expr;
        // no-op because most upstream operators can't include aliases
    }

    void operator()(relation::values const& expr) const {
        (void) expr;
        // no-op because most upstream operators can't include aliases
    }

    void operator()(relation::join_find& expr) {
        if (!has_alias()) {
            return; // no aliases to rewrite
        }
        // keep `sources` because they are from just the external relation
        rewrite_keys(expr.keys());
        rewrite_if(expr.condition());
    }

    void operator()(relation::join_scan& expr) {
        if (!has_alias()) {
            return; // no aliases to rewrite
        }
        // keep `columns.source()` because they are from just the external relation
        rewrite_keys(expr.lower().keys());
        rewrite_keys(expr.upper().keys());
        rewrite_if(expr.condition());
    }

    void operator()(relation::apply& expr) {
        if (!has_alias()) {
            return; // no aliases to rewrite
        }
        rewrite_sequence(expr.arguments());
        // keep columns.*variable() because they are output variables
    }

    void operator()(relation::project& expr) {
        // always process this operation, because this may define new aliases
        auto&& columns = expr.columns();
        for (auto iter = columns.begin(); iter != columns.end();) {
            auto&& column = *iter;

            // rewrite the value expression before checking if it is an alias
            rewrite(column.value());

            // register if this column is an alias definition
            if (auto original = extract_variable(column.value())) {
                define_alias(*original, column.variable());

                // removes this alias column
                iter = columns.erase(iter);
            } else {
                ++iter;
            }
        }
    }

    void operator()(relation::filter& expr) {
        if (!has_alias()) {
            return; // no aliases to rewrite
        }
        rewrite(expr.condition());
    }

    void operator()(relation::buffer const& expr) const {
        // no-op because this operator does not use any variables
        (void) expr;
    }

    void operator()(relation::identify const& expr) const {
        // no-op because this operator does not use any variables
        (void) expr;
    }

    void operator()(relation::emit& expr) {
        if (!has_alias()) {
            return; // no aliases to rewrite
        }
        rewrite_sources(expr.columns());
    }

    void operator()(relation::write& expr) {
        if (!has_alias()) {
            return; // no aliases to rewrite
        }
        rewrite_sources(expr.keys());
        rewrite_sources(expr.columns());
    }

    void operator()(relation::intermediate::join& expr) {
        if (!has_alias()) {
            return; // no aliases to rewrite
        }
        rewrite_if(expr.condition());
        rewrite_keys(expr.lower().keys());
        rewrite_keys(expr.upper().keys());
    }

    void operator()(relation::intermediate::aggregate& expr) {
        if (!has_alias()) {
            return; // no aliases to rewrite
        }
        rewrite_sequence(expr.group_keys());
        for (auto&& column : expr.columns()) {
            rewrite_sequence(column.arguments());
        }
    }

    void operator()(relation::intermediate::distinct& expr) {
        if (!has_alias()) {
            return; // no aliases to rewrite
        }
        rewrite_sequence(expr.group_keys());
    }

    void operator()(relation::intermediate::limit& expr) {
        if (!has_alias()) {
            return; // no aliases to rewrite
        }
        rewrite_sequence(expr.group_keys());
        rewrite_variables(expr.sort_keys());
    }

    void operator()(relation::intermediate::union_& expr) {
        if (!has_alias()) {
            return; // no aliases to rewrite
        }
        for (auto&& element : expr.mappings()) {
            rewrite_if(element.left());
            rewrite_if(element.right());
        }
    }

    void operator()(relation::intermediate::intersection& expr) {
        if (!has_alias()) {
            return; // no aliases to rewrite
        }
        for (auto&& element : expr.group_key_pairs()) {
            rewrite(element.left());
            rewrite(element.right());
        }
    }

    void operator()(relation::intermediate::difference& expr) {
        if (!has_alias()) {
            return; // no aliases to rewrite
        }
        for (auto&& element : expr.group_key_pairs()) {
            rewrite(element.left());
            rewrite(element.right());
        }
    }

    void operator()(relation::intermediate::escape& expr) {
        if (!has_alias()) {
            return; // no aliases to rewrite
        }
        rewrite_sources(expr.mappings());
    }

    void operator()(relation::intermediate::extension const& expr) const {
        throw_exception(std::domain_error(string_builder {}
                << "unsupported relation expression: "
                << expr
                << string_builder::to_string));
    }

private:
    relation::graph_type& graph_;
    bool extract_external_variables_;
    ::tsl::hopscotch_map<
        descriptor::variable,
        descriptor::variable> alias_map_; // alias -> original

    [[nodiscard]] bool has_alias() const {
        return !alias_map_.empty();
    }

    void define_alias(descriptor::variable const& original, descriptor::variable const& alias) {
        auto [iter, success] = alias_map_.try_emplace(alias, original);
        if (!success) {
            throw_exception(std::logic_error("redefine variable alias"));
        }
    }

    [[nodiscard]] optional_ptr<descriptor::variable const> extract_variable(scalar::expression const& expr) const {
        if (expr.kind() != scalar::variable_reference::tag) {
            return {};
        }
        auto&& var_ref = unsafe_downcast<scalar::variable_reference>(expr);
        auto var_kind = binding::kind_of(var_ref.variable());

        // always extract stream variables
        if (var_kind == binding::variable_info_kind::stream_variable) {
            return var_ref.variable();
        }
        // optionally extract external variables
        if (extract_external_variables_ && var_kind == binding::variable_info_kind::external_variable) {
            return var_ref.variable();
        }
        // otherwise, do not extract
        return {};
    }

    void rewrite(descriptor::variable& variable) {
        auto it = alias_map_.find(variable);
        if (it != alias_map_.end()) {
            variable = it->second;
        }
    }

    void rewrite_if(std::optional<descriptor::variable>& variable) {
        if (variable) {
            rewrite(*variable);
        }
    }

    void rewrite(scalar::expression& expr) {
        if (!has_alias()) {
            return; // no aliases to rewrite
        }
        scalar::walk(*this, expr);
    }

    void rewrite_if(optional_ptr<scalar::expression> expr) {
        if (expr) {
            rewrite(*expr);
        }
    }

    template<class Sequence>
    void rewrite_keys(Sequence& keys) {
        for (auto&& key : keys) {
            rewrite(key.variable());
            rewrite(key.value());
        }
    }

    template<class Sequence>
    void rewrite_sequence(Sequence& sequence) {
        for (auto&& item : sequence) {
            rewrite(item);
        }
    }

    template<class Sequence>
    void rewrite_sources(Sequence& sequence) {
        for (auto&& item : sequence) {
            rewrite(item.source());
        }
    }

    template<class Sequence>
    void rewrite_variables(Sequence& sequence) {
        for (auto&& item : sequence) {
            rewrite(item.variable());
        }
    }
};

} // namespace

void remove_variable_aliases(relation::graph_type& graph, bool extract_external_variables) {
    engine e {
            graph,
            extract_external_variables,
    };
    e.process();
}

} // namespace yugawara::analyzer::details

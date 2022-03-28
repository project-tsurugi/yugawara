#include <yugawara/compiler.h>

#include <takatori/statement/execute.h>

#include <takatori/util/assertion.h>
#include <takatori/util/clonable.h>

#include <yugawara/analyzer/expression_analyzer.h>
#include <yugawara/analyzer/intermediate_plan_optimizer.h>
#include <yugawara/analyzer/step_plan_builder.h>

#include <yugawara/storage/basic_prototype_processor.h>

#include "storage/resolve_prototype.h"

namespace yugawara {

using options_type = compiler::options_type;
using result_type = compiler::result_type;
using diagnostic_type = result_type::diagnostic_type;
using code_type = result_type::code_type;
using info_type = result_type::info_type;

namespace scalar = ::takatori::scalar;
namespace relation = ::takatori::relation;
namespace plan = ::takatori::plan;
namespace statement = ::takatori::statement;

using ::takatori::util::clone_unique;

using ::yugawara::util::either;

namespace {

code_type convert(analyzer::expression_analyzer_code source) noexcept {
    using from = decltype(source);
    using to = code_type;
    switch (source) {
        case from::unknown: return to::unknown;
        case from::unsupported_type: return to::unsupported_type;
        case from::ambiguous_type: return to::ambiguous_type;
        case from::inconsistent_type: return to::inconsistent_type;
        case from::unresolved_variable: return to::unresolved_variable;
        case from::inconsistent_elements: return to::inconsistent_elements;
    }
    std::abort();
}

std::vector<diagnostic_type> build_error(analyzer::expression_analyzer const& analyzer) {
    auto&& diagnostics = analyzer.diagnostics();
    std::vector<diagnostic_type> results;
    results.reserve(diagnostics.size());
    for (auto&& d : diagnostics) {
        results.emplace_back(
                convert(d.code()),
                d.message(),
                d.location());
    }
    return results;
}

either<std::vector<diagnostic_type>, info_type> do_inspect(std::function<void(analyzer::expression_analyzer&)> const& action) {
    auto exprs = std::make_shared<analyzer::expression_mapping>();
    auto vars = std::make_shared<analyzer::variable_mapping>();
    analyzer::expression_analyzer analyzer { exprs, vars };
    action(analyzer);
    if (analyzer.has_diagnostics()) {
        return build_error(analyzer);
    }
    return info_type { exprs, vars };
}

class engine {
public:
    explicit engine(options_type const& options)
        : options_(options)
        , expression_mapping_(std::make_shared<analyzer::expression_mapping>())
        , variable_mapping_(std::make_shared<analyzer::variable_mapping>())
        , expression_analyzer_ {
                expression_mapping_,
                variable_mapping_,
        }
        , type_repository_ { true }
    {
        expression_analyzer_.allow_unresolved(false);
    }

    result_type compile(relation::graph_type&& plan) {
        expression_analyzer_.resolve(plan, true, type_repository_);
        if (expression_analyzer_.has_diagnostics()) {
            return result_type { build_error(expression_analyzer_) };
        }
        expression_mapping_->clear();
        variable_mapping_->clear();

        auto steps = do_compile(std::move(plan));

        auto stmt = std::make_unique<statement::execute>(std::move(steps));
        return compile(std::move(stmt));
    }

    result_type compile(std::unique_ptr<statement::statement> stmt) {
        {
            storage::prototype_processor* proc = options_.storage_processor().get();
            if (proc == nullptr) {
                static storage::basic_prototype_processor default_storage_processor {};
                proc = std::addressof(default_storage_processor);
            }
            if (auto diagnostics = storage::resolve_prototype(*stmt, *proc); !diagnostics.empty()) {
                return result_type { diagnostics };
            }
        }

        expression_analyzer_.resolve(*stmt, true, type_repository_);
        if (expression_analyzer_.has_diagnostics()) {
            return result_type { build_error(expression_analyzer_) };
        }
        return build_success(std::move(stmt));
    }

private:
    options_type const& options_;
    std::shared_ptr<analyzer::expression_mapping> expression_mapping_;
    std::shared_ptr<analyzer::variable_mapping> variable_mapping_;
    analyzer::expression_analyzer expression_analyzer_;
    type::repository type_repository_;

    result_type build_success(std::unique_ptr<statement::statement> result) {
        BOOST_ASSERT(!expression_analyzer_.has_diagnostics()); // NOLINT
        return {
                std::move(result),
                info_type {
                        std::move(expression_mapping_),
                        std::move(variable_mapping_),
                },
        };
    }

    plan::graph_type do_compile(relation::graph_type&& intermediate) {
        do_optimize(intermediate);
        return do_plan(std::move(intermediate));
    }

    void do_optimize(relation::graph_type& graph) {
        analyzer::intermediate_plan_optimizer sub {};
        sub.options().runtime_features() = options_.runtime_features();
        sub.options().index_estimator(options_.index_estimator());
        sub(graph);
    }

    plan::graph_type do_plan(relation::graph_type&& graph) {
        analyzer::step_plan_builder sub {};
        sub.options().runtime_features() = options_.runtime_features();
        return sub(std::move(graph));
    }
};


} // namespace

result_type compiler::operator()(compiler_options const& options, relation::graph_type&& plan) {
    engine e { options };
    return e.compile(std::move(plan));
}

result_type compiler::operator()(options_type const& options, std::unique_ptr<statement::statement> statement) {
    engine e { options };
    return e.compile(std::move(statement));
}

result_type compiler::operator()(options_type const& options, statement::statement&& statement) {
    return operator()(options, clone_unique(std::move(statement)));
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
either<std::vector<diagnostic_type>, info_type> compiler::inspect(scalar::expression const& expression) {
    return do_inspect([&](analyzer::expression_analyzer& a) {
        type::repository repo { true };
        a.resolve(expression, true, repo);
    });
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
either<std::vector<diagnostic_type>, info_type> compiler::inspect(relation::graph_type const& graph) {
    return do_inspect([&](analyzer::expression_analyzer& a) {
        type::repository repo { true };
        a.resolve(graph, true, repo);
    });
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
either<std::vector<diagnostic_type>, info_type> compiler::inspect(plan::graph_type const& graph) {
    return do_inspect([&](analyzer::expression_analyzer& a) {
        type::repository repo { true };
        a.resolve(graph, true, repo);
    });
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
either<std::vector<diagnostic_type>, info_type> compiler::inspect(statement::statement const& statement) {
    return do_inspect([&](analyzer::expression_analyzer& a) {
        type::repository repo { true };
        a.resolve(statement, true, repo);
    });
}

} // namespace yugawara

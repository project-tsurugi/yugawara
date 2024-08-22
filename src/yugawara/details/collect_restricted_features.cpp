#include "collect_restricted_features.h"

#include <takatori/relation/step/dispatch.h>
#include <takatori/plan/dispatch.h>
#include <takatori/statement/dispatch.h>

#include <takatori/util/string_builder.h>

namespace yugawara::details {

namespace trelation = ::takatori::relation;
namespace tplan = ::takatori::plan;
namespace tstatement = ::takatori::statement;

using ::takatori::util::string_builder;

using diagnostic_type = diagnostic<compiler_code>;

namespace {

class engine {
public:
    static constexpr restricted_feature_set mask_execute_statement
            = restricted_feature_scalar_expressions
            | restricted_feature_relation_expressions
            | restricted_feature_exchange_steps
            ;

    static constexpr restricted_feature_set mask_process_step
            = restricted_feature_scalar_expressions
            | restricted_feature_relation_expressions
            ;

    explicit engine(restricted_feature_set features) noexcept :
        features_ { features }
    {}

    std::vector<diagnostic_type> release() {
        return std::move(diagnostics_);
    }

    void process(tstatement::statement const& statement) {
        tstatement::dispatch(*this, statement);
    }

    void process(tplan::step const& step) {
        tplan::dispatch(*this, step);
    }

    void process(trelation::expression const& expression) {
        trelation::step::dispatch(*this, expression);
    }

    void operator()(tstatement::statement const&) {
        // do nothing
    }

    void operator()(tstatement::execute const& statement) {
        // check if sub-elements can be restricted: execute -> (step, relation, scalar)
        auto restricted = features_ & mask_execute_statement;
        if (restricted.empty()) {
            return;
        }

        // check steps
        for (auto&& sub : statement.execution_plan()) {
            process(sub);
        }
    }

    void operator()(tstatement::write const& statement) {
        if (statement.operator_kind() == tstatement::write_kind::update) {
            validate(restricted_feature::statement_write_update, statement.region());
        }
        if (statement.operator_kind() == tstatement::write_kind::delete_) {
            validate(restricted_feature::statement_write_delete, statement.region());
        }
    }

    void operator()(tplan::step const&) {
        // do nothing
    }

    void operator()(tplan::process const& step) {
        auto restricted = features_ & mask_process_step;
        if (restricted.empty()) {
            return;
        }

        // check relation expressions
        for (auto&& sub : step.operators()) {
            process(sub);
        }
    }

    void operator()(tplan::aggregate const&) {
        validate(restricted_feature::exchange_aggregate, {});
    }

    void operator()(tplan::broadcast const&) {
        validate(restricted_feature::exchange_broadcast, {});
    }

    void operator()(tplan::discard const&) {
        validate(restricted_feature::exchange_discard, {});
    }

    void operator()(tplan::forward const&) {
        validate(restricted_feature::exchange_forward, {});
    }

    void operator()(tplan::group const&) {
        validate(restricted_feature::exchange_group, {});
    }

    void operator()(trelation::expression const&) {
        // do nothing
    }

    void operator()(trelation::buffer const& expression) {
        validate(restricted_feature::relation_buffer, expression.region());
    }

    void operator()(trelation::identify const& expression) {
        validate(restricted_feature::relation_identify, expression.region());
    }

    void operator()(trelation::join_find const& expression) {
        validate(restricted_feature::relation_join_find, expression.region());
        process_join_like(expression);
    }

    void operator()(trelation::join_scan const& expression) {
        validate(restricted_feature::relation_join_scan, expression.region());
        process_join_like(expression);
    }

    void operator()(trelation::step::join const& expression) {
        process_join_like(expression);
    }

    template<class T>
    void process_join_like(T const& expression) {
        switch (expression.operator_kind()) {
            case trelation::join_kind::semi:
                validate(restricted_feature::relation_semi_join, expression.region());
                break;
            case trelation::join_kind::anti:
                validate(restricted_feature::relation_anti_join, expression.region());
                break;
            default:
                break; // ok
        }
    }

    void operator()(trelation::write const& expression) {
        if (expression.operator_kind() == trelation::write_kind::insert ||
                expression.operator_kind() == trelation::write_kind::insert_overwrite ||
                expression.operator_kind() == trelation::write_kind::insert_skip) {
            validate(restricted_feature::relation_write_insert, expression.region());
        }
    }

    void operator()(trelation::values const& expression) {
        validate(restricted_feature::relation_values, expression.region());
    }

    void operator()(trelation::step::difference const& expression) {
        validate(restricted_feature::relation_difference, expression.region());
    }

    void operator()(trelation::step::flatten const& expression) {
        validate(restricted_feature::relation_flatten, expression.region());
    }

    void operator()(trelation::step::intersection const& expression) {
        validate(restricted_feature::relation_intersection, expression.region());
    }

private:
    restricted_feature_set const features_; // NOLINT(*-avoid-const-or-ref-data-members) const for safety
    std::vector<diagnostic_type> diagnostics_ {};

    void validate(restricted_feature feature, takatori::document::region region) {
        if (features_.contains(feature)) {
            diagnostics_.emplace_back(
                    compiler_code::unsupported_feature,
                    string_builder {}
                            << feature
                            << " is unsupported"
                            << string_builder::to_string,
                    region);
        }
    }
};

} // namespace


std::vector<diagnostic_type>
collect_restricted_features(restricted_feature_set features, tstatement::statement const& statement) {
    engine e { features };
    e.process(statement);
    return e.release();
}

} // namespace yugawara::details
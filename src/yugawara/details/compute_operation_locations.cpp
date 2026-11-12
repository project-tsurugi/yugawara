#include "compute_operation_locations.h"

#include <stdexcept>
#include <unordered_map>
#include <utility>

#include <takatori/plan/process.h>
#include <takatori/statement/execute.h>

#include <takatori/util/downcast.h>
#include <takatori/util/string_builder.h>

#include "element_location.h"

namespace yugawara::details {

namespace tplan = ::takatori::plan;
namespace tstatement = ::takatori::statement;

using ::takatori::util::string_builder;
using ::takatori::util::unsafe_downcast;

using location_id_type = remote::location_info::location_id_type;
using location_id_set_type = remote::location_info::location_id_set_type;
using diagnostic_type = diagnostic<compiler_code>;
using result_type = util::either<
        std::vector<diagnostic_type>,
        std::shared_ptr<remote::operation_location_query const>>;

namespace {

class operation_location_table : public remote::operation_location_query {
public:
    using step_map_type = std::unordered_map<tplan::step const*, location_id_set_type>;

    operation_location_table(
            step_map_type steps,
            tstatement::statement const& statement,
            location_id_set_type statement_locations) noexcept :
        steps_ { std::move(steps) },
        statement_ { std::addressof(statement) },
        statement_locations_ { std::move(statement_locations) }
    {}

    [[nodiscard]] location_id_set_type find(tplan::step const& operation) const override {
        if (auto iter = steps_.find(std::addressof(operation)); iter != steps_.end()) {
            return iter->second;
        }
        throw std::invalid_argument("the step is not in the compiled statement");
    }

    [[nodiscard]] location_id_set_type find(tstatement::statement const& operation) const override {
        if (std::addressof(operation) == statement_) {
            return statement_locations_;
        }
        throw std::invalid_argument("the statement is not the compiled one");
    }

private:
    step_map_type steps_;
    tstatement::statement const* statement_; // only for identity, never dereferenced
    location_id_set_type statement_locations_;
};

class engine {
public:
    explicit engine(remote::symbol_location_query const& query) noexcept :
        query_ { query },
        locations_ { query }
    {}

    [[nodiscard]] result_type process(tstatement::statement const& statement) {
        operation_location_table::step_map_type steps {};
        if (statement.kind() == tstatement::execute::tag) {
            for (auto&& step : unsafe_downcast<tstatement::execute>(statement).execution_plan()) {
                auto step_locations = locations_(step);
                if (step_locations.empty()) {
                    report_empty(step);
                }
                steps.emplace(std::addressof(step), std::move(step_locations));
            }
        }
        auto statement_locations = locations_(statement);
        // NOTE: `execute` statements may be empty, because their steps can be executed in different locations
        if (statement.kind() != tstatement::execute::tag && statement_locations.empty()) {
            report_empty(statement);
        }
        if (!diagnostics_.empty()) {
            return std::move(diagnostics_);
        }
        std::shared_ptr<remote::operation_location_query const> result = std::make_shared<operation_location_table const>(
                std::move(steps),
                statement,
                std::move(statement_locations));
        return result;
    }

private:
    remote::symbol_location_query const& query_;
    element_location locations_;
    std::vector<diagnostic_type> diagnostics_ {};

    void report_empty(tplan::step const& step) {
        if (step.kind() == tplan::process::tag) {
            report_empty(unsafe_downcast<tplan::process>(step));
            return;
        }
        diagnostics_.emplace_back(
                compiler_code::empty_operation_location,
                string_builder {}
                        << "there are no locations where the "
                        << step.kind()
                        << " step can be executed"
                        << string_builder::to_string,
                ::takatori::document::region {});
    }

    void report_empty(tplan::process const& process) {
        // NOTE: only operators which cannot be executed in the local location make the process empty
        string_builder builder {};
        builder << "there are no locations where the process can be executed; "
                << "operators which cannot be executed in the local location: ";
        ::takatori::document::region region {};
        bool first = true;
        for (auto&& expression : process.operators()) {
            auto locations = locations_(expression);
            if (locations.find(remote::location_info::local_location_id) != locations.end()) {
                continue;
            }
            if (first) {
                region = expression.region();
            } else {
                builder << ", ";
            }
            builder << expression.kind() << " ";
            print_locations(builder, locations);
            first = false;
        }
        diagnostics_.emplace_back(
                compiler_code::empty_operation_location,
                builder << string_builder::to_string,
                region);
    }

    void print_locations(string_builder& builder, location_id_set_type const& locations) const {
        builder << "{";
        bool first = true;
        for (auto id : locations) {
            if (!first) {
                builder << ", ";
            }
            print_location(builder, id);
            first = false;
        }
        builder << "}";
    }

    void print_location(string_builder& builder, location_id_type id) const {
        try {
            builder << query_.of(id).location_name();
        } catch (std::out_of_range const&) {
            builder << "#" << id;
        }
    }

    void report_empty(tstatement::statement const& statement) {
        diagnostics_.emplace_back(
                compiler_code::empty_operation_location,
                string_builder {}
                        << "there are no locations where the "
                        << statement.kind()
                        << " statement can be executed"
                        << string_builder::to_string,
                statement.region());
    }
};

} // namespace

result_type compute_operation_locations(
        tstatement::statement const& statement,
        remote::symbol_location_query const& query) {
    engine e { query };
    return e.process(statement);
}

} // namespace yugawara::details

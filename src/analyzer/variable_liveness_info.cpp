#include <yugawara/analyzer/variable_liveness_info.h>

namespace yugawara::analyzer {

variable_liveness_info::variable_liveness_info(::takatori::util::object_creator creator) noexcept
    : define_(creator.allocator<typename variables::value_type>())
    , use_(creator.allocator<typename variables::value_type>())
    , kill_(creator.allocator<typename variables::value_type>())
{}

variable_liveness_info::variables& variable_liveness_info::define() noexcept {
    return define_;
}

variable_liveness_info::variables const& variable_liveness_info::define() const noexcept {
    return define_;
}

variable_liveness_info::variables& variable_liveness_info::use() noexcept {
    return use_;
}

variable_liveness_info::variables const& variable_liveness_info::use() const noexcept {
    return use_;
}

variable_liveness_info::variables& variable_liveness_info::kill() noexcept {
    return kill_;
}

variable_liveness_info::variables const& variable_liveness_info::kill() const noexcept {
    return kill_;
}

static void print_variables(std::ostream& out, variable_liveness_info::variables const& variables) {
    out << "{";
    if (!variables.empty()) {
        out << " ";
        bool first = true;
        for (auto&& v : variables) {
            if (!first) {
                out << ",";
            }
            out << " " << v;
            first = false;
        }
        out << " ";
    }
    out << "}";
}

std::ostream& operator<<(std::ostream& out, variable_liveness_info const& value) {
    out << "variable_liveness_info(";
    out << "define=";
    print_variables(out, value.define());
    out << ", " << "use=";
    print_variables(out, value.use());
    out << ", " << "kill=";
    print_variables(out, value.kill());
    out << ")";
    return out;
}

} // namespace yugawara::analyzer

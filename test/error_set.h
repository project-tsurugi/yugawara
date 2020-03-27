#pragma once

#include <iostream>
#include <string>
#include <vector>

#include <takatori/util/string_builder.h>
#include <takatori/util/vector_print_support.h>

namespace yugawara::testing {

class error_set {
public:
    void append(std::string error) {
        errors_.emplace_back(error);
    }

    std::vector<std::string>& errors() noexcept {
        return errors_;
    }

    std::vector<std::string> const& errors() const noexcept {
        return errors_;
    }

private:
    std::vector<std::string> errors_;
};

inline bool operator==(error_set const& a, error_set const& b) noexcept {
    return a.errors() == b.errors();
}

inline error_set operator+(error_set const& a, error_set const& b) {
    error_set result;
    auto it = result.errors().begin();
    it = result.errors().insert(it, a.errors().begin(), a.errors().end());
    result.errors().insert(it, b.errors().begin(), b.errors().end());
    return result;
}

template<class T>
inline error_set& operator<<(error_set& out, T&& value) {
    using ::takatori::util::string_builder;
    out.append(string_builder {}
            << value
            << string_builder::to_string);
    return out;
}

inline std::ostream& operator<<(std::ostream& out, error_set const& value) {
    return out << ::takatori::util::print_support { value.errors() };
}

} // namespace yugawara::testing

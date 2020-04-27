#pragma once

#include <functional>

#include <tsl/hopscotch_map.h>

#include <takatori/descriptor/variable.h>
#include <takatori/relation/expression.h>
#include <takatori/util/optional_ptr.h>

namespace yugawara::analyzer::details {

class stream_variable_flow_info_entry {
public:
    using consumer_type = std::function<void(::takatori::descriptor::variable const& defined)>;

    stream_variable_flow_info_entry() = default;

    explicit stream_variable_flow_info_entry(::takatori::util::object_creator creator);

    [[nodiscard]] ::takatori::util::optional_ptr<::takatori::relation::expression const> originator() const noexcept;

    void originator(::takatori::util::optional_ptr<::takatori::relation::expression const> originator) noexcept;

    [[nodiscard]] bool is_separator() const noexcept;

    [[nodiscard]] ::takatori::util::optional_ptr<::takatori::relation::expression const> find(
            ::takatori::descriptor::variable const& variable) const;

    void each(consumer_type const& consumer) const;

    void declare(::takatori::descriptor::variable variable, ::takatori::relation::expression const& declaration);

private:
    ::takatori::util::optional_ptr<::takatori::relation::expression const> originator_ {};
    tsl::hopscotch_map<
            ::takatori::descriptor::variable,
            ::takatori::relation::expression const*,
            std::hash<::takatori::descriptor::variable>,
            std::equal_to<>,
            ::takatori::util::object_allocator<std::pair<
                    ::takatori::descriptor::variable,
                    ::takatori::relation::expression const*>>> declarations_;
};

} // namespace yugawara::analyzer::details

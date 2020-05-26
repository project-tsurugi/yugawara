#include <yugawara/extension/scalar/aggregate_function_call.h>

#include <takatori/tree/tree_element_vector_forward.h>

#include <takatori/util/downcast.h>

namespace yugawara::extension::scalar {

using namespace ::takatori;
using namespace ::takatori::scalar;

aggregate_function_call::aggregate_function_call(descriptor::aggregate_function function, util::reference_vector<expression> arguments) noexcept
    : function_(std::move(function))
    , arguments_(*this, std::move(arguments))
{}

aggregate_function_call::aggregate_function_call(
        descriptor::aggregate_function function,
        std::initializer_list<util::rvalue_reference_wrapper<expression>> arguments)
    : aggregate_function_call(
            std::move(function),
            { arguments.begin(), arguments.end() })
{}

aggregate_function_call::aggregate_function_call(aggregate_function_call const& other, util::object_creator creator)
    : aggregate_function_call(
            other.function_,
            tree::forward(creator, other.arguments_))
{}

aggregate_function_call::aggregate_function_call(aggregate_function_call&& other, util::object_creator creator)
    : aggregate_function_call(
            std::move(other.function_),
            tree::forward(creator, std::move(other.arguments_)))
{}

aggregate_function_call::extension_id_type aggregate_function_call::extension_id() const noexcept {
    return extension_tag;
}

aggregate_function_call* aggregate_function_call::clone(util::object_creator creator) const& {
    return creator.create_object<aggregate_function_call>(*this, creator);
}

aggregate_function_call* aggregate_function_call::clone(util::object_creator creator) && {
    return creator.create_object<aggregate_function_call>(std::move(*this), creator);
}

descriptor::aggregate_function& aggregate_function_call::function() noexcept {
    return function_;
}

descriptor::aggregate_function const& aggregate_function_call::function() const noexcept {
    return function_;
}

tree::tree_element_vector<expression>& aggregate_function_call::arguments() noexcept {
    return arguments_;
}

tree::tree_element_vector<expression> const& aggregate_function_call::arguments() const noexcept {
    return arguments_;
}

bool operator==(aggregate_function_call const& a, aggregate_function_call const& b) noexcept {
    return a.function() == b.function()
        && a.arguments() == b.arguments();
}

bool operator!=(aggregate_function_call const& a, aggregate_function_call const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, aggregate_function_call const& value) {
    return out << to_string_view(static_cast<extension_id>(aggregate_function_call::extension_tag)) << "("
               << "function=" << value.function() << ", "
               << "arguments=" << value.arguments() <<  ")";
}

bool aggregate_function_call::equals(extension const& other) const noexcept {
    return extension_tag == other.extension_id() && *this == util::unsafe_downcast<aggregate_function_call>(other);
}

std::ostream& aggregate_function_call::print_to(std::ostream& out) const {
    return out << *this;
}

} // namespace yugawara::extension::scalar

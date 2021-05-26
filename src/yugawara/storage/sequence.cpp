#include <yugawara/storage/sequence.h>

#include <takatori/util/optional_print_support.h>

#include <yugawara/storage/provider.h>

namespace yugawara::storage {

using ::takatori::util::optional_ptr;

sequence::sequence(
        std::optional<definition_id_type> definition_id,
        simple_name_type simple_name,
        value_type initial_value,
        value_type increment_value,
        value_type min_value,
        value_type max_value,
        bool cycle) noexcept :
    definition_id_ { definition_id },
    simple_name_ { std::move(simple_name) },
    initial_value_ { initial_value },
    increment_value_ { increment_value },
    min_value_ { min_value },
    max_value_ { max_value },
    cycle_ { cycle }
{}

sequence::sequence(
        simple_name_type simple_name,
        value_type initial_value,
        value_type increment_value,
        value_type min_value,
        value_type max_value,
        bool cycle) noexcept :
    sequence {
            {},
            std::move(simple_name),
            initial_value,
            increment_value,
            min_value,
            max_value,
            cycle,
    }
{}

std::optional<sequence::definition_id_type> sequence::definition_id() const noexcept {
    return definition_id_;
}

sequence& sequence::definition_id(std::optional<definition_id_type> definition_id) noexcept {
    definition_id_ = definition_id;
    return *this;
}

std::string_view sequence::simple_name() const noexcept {
    return simple_name_;
}

sequence& sequence::simple_name(simple_name_type simple_name) noexcept {
    simple_name_ = std::move(simple_name);
    return *this;
}

sequence::value_type sequence::initial_value() const noexcept {
    return initial_value_;
}

sequence& sequence::initial_value(value_type value) noexcept {
    initial_value_ = value;
    return *this;
}

sequence::value_type sequence::increment_value() const noexcept {
    return increment_value_;
}

sequence& sequence::increment_value(value_type value) noexcept {
    increment_value_ = value;
    return *this;
}

sequence::value_type sequence::min_value() const noexcept {
    return min_value_;
}

sequence& sequence::min_value(value_type value) noexcept {
    min_value_ = value;
    return *this;
}

sequence::value_type sequence::max_value() const noexcept {
    return max_value_;
}

sequence& sequence::max_value(value_type value) noexcept {
    max_value_ = value;
    return *this;
}

bool sequence::cycle() const noexcept {
    return cycle_;
}

sequence& sequence::cycle(bool enabled) noexcept {
    cycle_ = enabled;
    return *this;
}

::takatori::util::optional_ptr<provider const> sequence::owner() const noexcept {
    return optional_ptr { owner_ };
}

std::ostream& operator<<(std::ostream& out, sequence const& value) {
    using ::takatori::util::print_support;
    return out << "sequence" << "("
               << "definition_id=" << print_support { value.definition_id() } << ", "
               << "simple_name=" << value.simple_name() << ", "
               << "initial_value=" << value.initial_value() << ", "
               << "increment_value=" << value.increment_value() << ", "
               << "min_value=" << value.min_value() << ", "
               << "max_value=" << value.max_value() << ", "
               << "cycle=" << print_support { value.cycle() } << ")";
}

} // namespace yugawara::storage

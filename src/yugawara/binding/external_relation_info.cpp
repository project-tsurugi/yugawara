#include <yugawara/binding/external_relation_info.h>

#include <takatori/util/clonable.h>
#include <takatori/util/hash.h>

#include "class_id_util.h"

namespace yugawara::binding {

namespace descriptor = ::takatori::descriptor;

using ::takatori::util::clone_shared;
using ::takatori::util::optional_ptr;

external_relation_info::external_relation_info(
        relation_id_type relation_id,
        std::vector<descriptor::variable> columns) noexcept :
    relation_id_ { relation_id },
    columns_ { std::move(columns) }
{}

external_relation_info::external_relation_info(::takatori::util::clone_tag_t, external_relation_info const& other) :
    relation_id_ { other.relation_id_ },
    columns_ { other.columns_ },
    type_hints_ { other.type_hints_ }
{}

external_relation_info::external_relation_info(::takatori::util::clone_tag_t, external_relation_info&& other) :
    relation_id_ { other.relation_id_ },
    columns_ { std::move(other.columns_) },
    type_hints_ { std::move(other.type_hints_) }
{}

std::size_t external_relation_info::class_id() const noexcept {
    return class_id_delegate(*this);
}

relation_info_kind external_relation_info::kind() const noexcept {
    return tag;
}

external_relation_info* external_relation_info::clone() const& {
    return new external_relation_info(::takatori::util::clone_tag, *this); // NOLINT
}

external_relation_info* external_relation_info::clone() && {
    return new external_relation_info(::takatori::util::clone_tag, std::move(*this)); // NOLINT;
}

external_relation_info::relation_id_type external_relation_info::relation_id() const noexcept {
    return relation_id_;
}

std::vector<descriptor::variable>& external_relation_info::columns() noexcept {
    return columns_;
}

std::vector<descriptor::variable> const& external_relation_info::columns() const noexcept {
    return columns_;
}

::takatori::util::sequence_view<descriptor::variable const> external_relation_info::input_columns() const noexcept {
    return ::takatori::util::sequence_view { columns_ };
}

::takatori::util::sequence_view<descriptor::variable const> external_relation_info::output_columns() const noexcept {
    return ::takatori::util::sequence_view { columns_ };
}

optional_ptr<::takatori::type::data const> external_relation_info::find_type(descriptor::variable const& column) const {
    if (auto it = type_hints_.find(column); it != type_hints_.end()) {
        return *it->second;
    }
    return {};
}

external_relation_info::type_pointer external_relation_info::find_type_shared(descriptor::variable const& column) const {
    if (auto it = type_hints_.find(column); it != type_hints_.end()) {
        return it->second;
    }
    return {};
}

external_relation_info& external_relation_info::add_type(descriptor::variable column, type_pointer type) {
    if (type) {
        type_hints_.insert_or_assign(std::move(column), std::move(type));
    } else {
        type_hints_.erase(column);
    }
    return *this;
}

external_relation_info& external_relation_info::add_type(descriptor::variable column, ::takatori::type::data&& type) {
    return add_type(std::move(column), clone_shared(std::move(type)));
}

bool operator==(external_relation_info const& a, external_relation_info const& b) noexcept {
    if (std::addressof(a) == std::addressof(b)) {
        return true;
    }
    return a.relation_id_ == b.relation_id_
        && a.columns_ == b.columns_;
}

bool operator!=(external_relation_info const& a, external_relation_info const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, external_relation_info const& value) {
    out << value.kind() << "("
        << "relation_id=" << value.relation_id() << ", "
        << "columns=[";
    bool first = true;
    for (auto&& column : value.columns()) {
        if (!first) {
            out << ", ";
        }
        first = false;
        out << column;
    }
    out << "], "
        << "hints=[";
    first = true;
    for (auto&& column : value.columns()) {
        if (auto type = value.find_type(column)) {
            if (!first) {
                out << ", ";
            }
            first = false;
            out << "{"
                << "column=" << column << ", "
                << "type=" << *type
                << "}";
        }
    }
    return out << "])";
}

bool external_relation_info::equals(descriptor::relation::entity_type const& other) const noexcept {
    return equals_delegate(*this, other);
}

std::size_t external_relation_info::hash() const noexcept {
    auto result = ::takatori::util::hash(descriptor_type::tag, tag, relation_id_);
    for (auto&& column : columns_) {
        result = result * ::takatori::util::hash_prime + std::hash<descriptor::variable> {}(column);
    }
    return result;
}

std::ostream& external_relation_info::print_to(std::ostream& out) const {
    return out << *this;
}

} // namespace yugawara::binding

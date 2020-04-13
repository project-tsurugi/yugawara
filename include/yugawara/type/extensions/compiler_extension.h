#pragma once

#include <iostream>
#include <type_traits>

#include <takatori/type/extension.h>
#include <takatori/util/downcast.h>

#include "extension_kind.h"

namespace yugawara::type::extensions {

/**
 * @brief a simple type extension for this compiler.
 * @tparam ExtensionId the extension ID
 */
template<takatori::type::extension::extension_id_type ExtensionId>
class compiler_extension : public takatori::type::extension {

    static_assert(ExtensionId >= min_id);
    static_assert(ExtensionId <= max_id);

public:
    /// @brief the extension ID of this type.
    static constexpr extension_id_type extension_tag = ExtensionId;

    /**
     * @brief returns a clone of this object.
     * @param creator the object creator
     * @return the created clone
     */
    [[nodiscard]] compiler_extension* clone(takatori::util::object_creator creator) const& override {
        return creator.create_object<compiler_extension>();
    }

    /// @copydoc clone()
    [[nodiscard]] compiler_extension* clone(takatori::util::object_creator creator) && override {
        return creator.create_object<compiler_extension>();
    }

    /**
     * @brief returns the extension ID of this type.
     * @return the extension ID
     */
    [[nodiscard]] extension_id_type extension_id() const noexcept override {
        return extension_tag;
    }

    /**
     * @brief returns whether or not the given type model is an instance of this class.
     * @param type the target type model
     * @return true if the model is an instance of this class
     */
    [[nodiscard]] static bool is_instance(takatori::type::data const& type) noexcept {
        if (type.kind() == takatori::type::type_kind::extension) {
            return is_instance(takatori::util::downcast<takatori::type::extension>(type));
        }
        return false;
    }

    /// @copydoc is_instance(takatori::type::data const&)
    [[nodiscard]] static bool is_instance(takatori::type::extension const& type) noexcept {
        return type.extension_id() == extension_tag;
    }

    /**
     * @brief returns whether or not the two elements are equivalent.
     * @param a the first element
     * @param b the second element
     * @return true if a == b
     * @return false otherwise
     */
    friend bool operator==(compiler_extension const& a, compiler_extension const& b) noexcept {
        return a.extension_id() == b.extension_id();
    }

    /**
     * @brief returns whether or not the two elements are different.
     * @param a the first element
     * @param b the second element
     * @return true if a != b
     * @return false otherwise
     */
    friend bool operator!=(compiler_extension const& a, compiler_extension const& b) noexcept {
        return !(a == b);
    }

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output
     */
    friend std::ostream& operator<<(std::ostream& out, compiler_extension const& value) {
        (void) value;
        if (is_known_compiler_extension(extension_tag)) {
            return out << static_cast<extension_kind>(extension_tag) << "()";
        }
        return out << "compiler_extension("
                   << "extension_id=" << extension_tag << ")";
    }

protected:
    /**
     * @brief returns whether or not this type is equivalent to the target one.
     * @param other the target type
     * @return true if the both are equivalent
     * @return false otherwise
     */
    [[nodiscard]] bool equals(extension const& other) const noexcept override {
        return extension_tag == other.extension_id();
    }

    /**
     * @brief returns hash code of this object.
     * @details The returned digest value should compute from only type specific information.
     * @return the computed hash code
     */
    [[nodiscard]] std::size_t hash() const noexcept override {
        return static_cast<std::size_t>(extension_tag);
    }

    /**
     * @brief appends string representation of this object into the given output.
     * @param out the target output
     * @return the output
     */
    std::ostream& print_to(std::ostream& out) const override {
        return out << *this;
    }
};

} // namespace yugawara::type::extensions

/**
 * @brief specialization for yugawara::type::extensions::simple_extension.
 * @tparam ExtensionId the extension ID
 */
template<takatori::type::extension::extension_id_type ExtensionId>
struct std::hash<yugawara::type::extensions::compiler_extension<ExtensionId>> : hash<takatori::type::data> {};

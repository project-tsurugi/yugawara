#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <cstddef>

#include <takatori/type/data.h>
#include <takatori/util/object_creator.h>
#include <takatori/util/reference_list_view.h>
#include <takatori/util/rvalue_reference_wrapper.h>
#include <takatori/util/smart_pointer_extractor.h>

namespace yugawara::function {

/**
 * @brief represents a function declaration.
 */
class declaration {
public:
    /// @brief the function definition ID type.
    using definition_id_type = std::size_t;

    /// @brief represents the declaration is unresolved.
    static constexpr definition_id_type unresolved_definition_id = 0;

    /// @brief the minimum function definition ID for built-in functions.
    static constexpr definition_id_type minimum_system_function_id = 100;

    /// @brief the minimum function definition ID for built-in functions.
    static constexpr definition_id_type minimum_builtin_function_id = 10'000;

    /// @brief the minimum function definition ID for user defined functions.
    static constexpr definition_id_type minimum_user_function_id = 20'000;

    /// @brief the declaration name type.
    using name_type = std::basic_string<char, std::char_traits<char>, takatori::util::object_allocator<char>>;

    /// @brief the smart pointer of parameter/result type.
    using type_pointer = std::shared_ptr<takatori::type::data const>;

    /// @brief the type list view type.
    using type_list_view = takatori::util::reference_list_view<takatori::util::smart_pointer_extractor<type_pointer>>;

    /**
     * @brief creates a new object.
     * @param definition_id the definition ID
     * @param name the function name
     * @param return_type the return type
     * @param parameter_types the parameter types
     */
    explicit declaration(
            definition_id_type definition_id,
            name_type name,
            type_pointer return_type,
            std::vector<type_pointer, takatori::util::object_allocator<type_pointer>> parameter_types) noexcept;

    /**
     * @brief creates a new object.
     * @param definition_id the definition ID
     * @param name the function name
     * @param return_type the return type
     * @param parameter_types the parameter types
     * @attention this may take copy of arguments
     */
    declaration(
            definition_id_type definition_id,
            std::string_view name,
            takatori::type::data&& return_type,
            std::initializer_list<takatori::util::rvalue_reference_wrapper<takatori::type::data>> parameter_types);

    /**
     * @brief returns the function definition ID.
     * @details The ID must be unique in the system for individual functions.
     * @return the function definition ID
     */
    definition_id_type definition_id() const noexcept;

    /**
     * @brief sets the function definition ID.
     * @param definition_id the definition ID
     * @return this
     */
    declaration& definition_id(definition_id_type definition_id) noexcept;

    /**
     * @brief returns whether or not this function is well defined.
     * @return true if it is defined
     * @return false otherwise
     */
    bool is_defined() const noexcept;

    /// @copydoc is_defined()
    explicit operator bool() const noexcept;

    /**
     * @brief returns the function name.
     * @return the function name
     */
    std::string_view name() const noexcept;

    /**
     * @brief sets the function name.
     * @param name the function name
     * @return this
     */
    declaration& name(name_type name) noexcept;

    /**
     * @brief returns the return type of the function.
     * @return the return type
     */
    takatori::type::data const& return_type() const noexcept;

    /**
     * @brief returns the return type of the function as its shared pointer.
     * @return the return type
     */
    type_pointer const& shared_return_type() const noexcept;

    /**
     * @brief sets the return type of the function.
     * @param return_type the return type
     * @return this
     */
    declaration& return_type(type_pointer return_type) noexcept;

    /**
     * @brief returns the parameter types of the function.
     * @return the parameter types
     */
    type_list_view parameter_types() const noexcept;

    /**
     * @brief returns the parameter types of the function as individual shared pointers.
     * @return the parameter types
     */
    std::vector<type_pointer, takatori::util::object_allocator<type_pointer>>& shared_parameter_types() noexcept;

    /// @copydoc parameter_types()
    std::vector<type_pointer, takatori::util::object_allocator<type_pointer>> const& shared_parameter_types() const noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output stream
     */
    friend std::ostream& operator<<(std::ostream& out, declaration const& value);

private:
    definition_id_type definition_id_;
    name_type name_;
    type_pointer return_type_;
    std::vector<type_pointer, takatori::util::object_allocator<type_pointer>> parameter_types_;

    // FIXME: multi-output functions
};

} // namespace yugawara::function
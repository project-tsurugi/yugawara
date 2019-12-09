#pragma once

#include <functional>
#include <iostream>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <takatori/util/object_creator.h>

#include "table.h"
#include "index_feature.h"
#include "details/index_key_element.h"

namespace yugawara::storage {

/**
 * @brief the index key element.
 */
class index {
public:
    /// @brief the simple name type.
    using simple_name_type = std::basic_string<char, std::char_traits<char>, takatori::util::object_allocator<char>>;

    /// @brief the key piece type.
    using key = details::index_key_element;

    /// @brief the column reference type.
    using column_ref = std::reference_wrapper<column const>;

    /**
     * @brief creates a new object.
     * @param table the origin table
     * @param simple_name the simple name, may be empty string
     * @param keys the index keys
     * @param values the extra columns
     * @param features the index features
     */
    explicit index(
            std::shared_ptr<class table const> table,
            simple_name_type simple_name,
            std::vector<key, takatori::util::object_allocator<key>> keys,
            std::vector<column_ref, takatori::util::object_allocator<column_ref>> values,
            index_feature_set features) noexcept;

    /**
     * @brief creates a new object.
     * @param table the origin table
     * @param simple_name the simple name, may be empty string
     * @param keys the index keys
     * @param values the extra columns
     * @param features the index features
     */
    index(
            std::shared_ptr<class table const> table,
            std::string_view simple_name,
            std::initializer_list<key> keys = {},
            std::initializer_list<column_ref> values = {},
            index_feature_set features = { index_feature::find, index_feature::scan });

    /**
     * @brief returns the origin table of this index.
     * @return the origin table
     */
    class table const& table() const noexcept;

    /**
     * @brief returns the origin table of this index for sharing.
     * @return the origin table for sharing
     */
    std::shared_ptr<class table const> const& shared_table() const noexcept;

    /**
     * @brief sets origin table.
     * @param table the origin table
     * @return this
     */
    index& table(std::shared_ptr<class table const> table) noexcept;

    /**
     * @brief returns the simple name of this table.
     * @return the simple name
     * @return empty string if it is not defined
     */
    std::string_view simple_name() const noexcept;

    /**
     * @brief sets the simple name of this table.
     * @param simple_name the simple name
     * @return this
     */
    index& simple_name(simple_name_type simple_name) noexcept;

    /**
     * @brief returns the index key elements.
     * @return the index key elements
     */
    std::vector<key, takatori::util::object_allocator<key>>& keys() noexcept;

    /// @copydoc keys()
    std::vector<key, takatori::util::object_allocator<key>> const& keys() const noexcept;

    /**
     * @brief returns the extra columns.
     * @return the extra columns
     */
    std::vector<column_ref, takatori::util::object_allocator<column_ref>>& values() noexcept;

    /// @copydoc values()
    std::vector<column_ref, takatori::util::object_allocator<column_ref>> const& values() const noexcept;

    /**
     * @brief returns the available features of this index.
     * @return the available features
     */
    index_feature_set& features() noexcept;

    /// @copydoc features()
    index_feature_set const& features() const noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output
     */
    friend std::ostream& operator<<(std::ostream& out, index const& value);

private:
    std::shared_ptr<class table const> table_;
    simple_name_type simple_name_;
    std::vector<key, takatori::util::object_allocator<key>> keys_;
    std::vector<column_ref, takatori::util::object_allocator<column_ref>> values_;
    index_feature_set features_;
};

} // namespace yugawara::storage

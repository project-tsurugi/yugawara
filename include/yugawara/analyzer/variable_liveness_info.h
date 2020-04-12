#pragma once

#include <iostream>
#include <unordered_set>

#include <takatori/descriptor/variable.h>
#include <takatori/util/object_creator.h>

namespace yugawara::analyzer {

/**
 * @brief provides liveness of variables in each block.
 * @see block
 * @see yugawara::binding::variable_info
 */
class variable_liveness_info {
public:
    /// @brief the set of variables.
    using variables = std::unordered_set<
            ::takatori::descriptor::variable,
            std::hash<::takatori::descriptor::variable>,
            std::equal_to<>,
            ::takatori::util::object_allocator<::takatori::descriptor::variable>>;

    /**
     * @brief creates a new instance.
     */
    variable_liveness_info() = default;

    /**
     * @brief creates a new instance.
     * @param creator the object creator
     */
    explicit variable_liveness_info(::takatori::util::object_creator creator) noexcept;

    /**
     * @brief returns the variables defined in this block.
     * @details This may include the following kind of variables:
     *
     *      - binding::variable_info_kind::stream_variable
     *      - binding::variable_info_kind::local_variable
     * @return the defined variables
     * @return empty if this kind of variables have not been computed yet
     */
    variables& define() noexcept;

    /// @copydoc define()
    variables const& define() const noexcept;

    /**
     * @brief returns the variables referred in this block.
     * @details This may include the following kind of variables:
     *
     *      - binding::variable_info_kind::table_column
     *      - binding::variable_info_kind::exchange_column
     *      - binding::variable_info_kind::frame_variable
     *      - binding::variable_info_kind::stream_variable
     *      - binding::variable_info_kind::local_variable
     *      - binding::variable_info_kind::external_variable
     * @note This only contains variables which are *explicitly* referred.
     *      For example, takatori::relation::buffer will implicitly refer all variables in the relation,
     *      but they does not present in the result of this function.
     * @note This DOES NOT contain table columns of takatori::relation::write's target.
     * @return the referred variables
     * @return empty if this kind of variables have not been computed yet
     */
    variables& use() noexcept;

    /// @copydoc use()
    variables const& use() const noexcept;

    /**
     * @brief returns the variables which will not be refer after this block (including this block).
     * @details That is, these variables can remove from the storage before starting this block.
     *
     *      This may include the following kind of variables:
     *
     *      - binding::variable_info_kind::stream_variable
     *      - binding::variable_info_kind::local_variable
     * @note This is symmetrically placed with define()
     * @note If the variables appear both define() and kill(), it should never be used anywhere
     * @return variables to be killed
     * @return empty if this kind of variables have not been computed yet
     */
    variables& kill() noexcept;

    /// @copydoc kill()
    variables const& kill() const noexcept;

private:
    variables define_ {};
    variables use_ {};
    variables kill_ {};
};

/**
 * @brief appends a string representation of the given value.
 * @param out the target output stream
 * @param value the target value
 * @return the output stream
 */
std::ostream& operator<<(std::ostream& out, variable_liveness_info const& value);

} // namespace yugawara::analyzer

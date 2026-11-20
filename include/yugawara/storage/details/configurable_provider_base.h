#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>

#include <takatori/util/maybe_shared_ptr.h>
#include <takatori/util/optional_ptr.h>

#include <yugawara/storage/provider.h>

namespace yugawara::storage::details {

/**
 * @brief the contents of storage::basic_configurable_provider, which do not depend on the mutex type.
 * @details This holds the registered elements and provides operations for them without any locks.
 *      The derived class must acquire the lock before calling the operations (except parent()):
 *      the reader lock for find_*() and each_*(), and the writer lock for add_*() and remove_*().
 * @note The operations of this class may call the parent provider and the given consumers.
 */
class configurable_provider_base : public provider {
public:
    /**
     * @brief returns the parent provider.
     * @return the parent provider
     * @return empty if there are no parents
     */
    [[nodiscard]] ::takatori::util::optional_ptr<provider const> parent() const noexcept;

protected:
    /**
     * @brief creates a new object.
     * @param parent the parent provider (nullable)
     */
    explicit configurable_provider_base(::takatori::util::maybe_shared_ptr<provider const> parent) noexcept;

    /// @copydoc provider::find_relation()
    [[nodiscard]] std::shared_ptr<relation const> internal_find_relation(std::string_view id) const;

    /// @copydoc provider::each_relation()
    void internal_each_relation(relation_consumer_type const& consumer) const;

    /**
     * @brief adds a relation.
     * @param element the relation to add
     * @param overwrite true to overwrite the existing entry
     * @throws std::invalid_argument if the target entry already exists and `overwrite=false`
     * @throws std::invalid_argument if the target entry has been already owned another provider
     */
    void internal_add_relation(std::shared_ptr<relation const> element, bool overwrite);

    /**
     * @brief adds a table.
     * @details This is same as internal_add_relation(),
     *      except that this only checks tables in the parent provider for the duplicate.
     * @param element the table to add
     * @param overwrite true to overwrite the existing entry
     * @throws std::invalid_argument if the target entry already exists and `overwrite=false`
     * @throws std::invalid_argument if the target entry has been already owned another provider
     */
    void internal_add_table(std::shared_ptr<table const> element, bool overwrite);

    /**
     * @brief removes the relation in this provider.
     * @param id the relation ID
     * @return true if successfully removed
     * @return false if there is no such a relation
     */
    bool internal_remove_relation(std::string_view id);

    /// @copydoc provider::find_index()
    [[nodiscard]] std::shared_ptr<index const> internal_find_index(std::string_view id) const;

    /// @copydoc provider::each_index()
    void internal_each_index(index_consumer_type const& consumer) const;

    /// @copydoc provider::find_primary_index()
    [[nodiscard]] std::shared_ptr<index const> internal_find_primary_index(class table const& table) const;

    /// @copydoc provider::each_table_index()
    void internal_each_table_index(class table const& table, index_consumer_type const& consumer) const;

    /**
     * @brief adds an index.
     * @param element the index to add
     * @param overwrite true to overwrite the existing entry
     * @throws std::invalid_argument if the target entry already exists and `overwrite=false`
     */
    void internal_add_index(std::shared_ptr<index const> element, bool overwrite);

    /**
     * @brief removes the index in this provider.
     * @param id the index ID
     * @return true if successfully removed
     * @return false if there is no such an index
     */
    bool internal_remove_index(std::string_view id);

    /// @copydoc provider::find_sequence()
    [[nodiscard]] std::shared_ptr<sequence const> internal_find_sequence(std::string_view id) const;

    /// @copydoc provider::each_sequence()
    void internal_each_sequence(sequence_consumer_type const& consumer) const;

    /**
     * @brief adds a sequence.
     * @param element the sequence to add
     * @param overwrite true to overwrite the existing entry
     * @throws std::invalid_argument if the target entry already exists and `overwrite=false`
     */
    void internal_add_sequence(std::shared_ptr<sequence const> element, bool overwrite);

    /**
     * @brief removes the sequence in this provider.
     * @param id the sequence ID
     * @return true if successfully removed
     * @return false if there is no such a sequence
     */
    bool internal_remove_sequence(std::string_view id);

    /**
     * @brief checks whether the given ID is the simple name of the element.
     * @param id the element ID
     * @param simple_name the simple name of the element
     * @param message the error message
     * @throws std::invalid_argument if they are different
     */
    static void check_element_id(std::string_view id, std::string_view simple_name, char const* message);

private:
    using key_type = std::string;
    using relation_map_type = std::map<
            key_type,
            std::shared_ptr<relation const>,
            std::less<>>;

    using index_map_type = std::map<
            key_type,
            std::shared_ptr<index const>,
            std::less<>>;

    using sequence_map_type = std::map<
            key_type,
            std::shared_ptr<sequence const>,
            std::less<>>;

    // the key of elements associated with tables: the address of the table, and the element ID.
    // The registered element must hold its table, so that the address is not reused while it is registered.
    using table_scoped_key_type = std::tuple<class table const*, key_type>;

    // compares (table, ID) pairs, including the ones with std::string_view as the ID
    struct table_scoped_key_compare {
        using is_transparent = void;

        template<class Left, class Right>
        bool operator()(Left const& left, Right const& right) const noexcept {
            auto const* left_table = std::get<0>(left);
            auto const* right_table = std::get<0>(right);
            if (left_table != right_table) {
                // NOTE: std::less is a total order even for the pointers to unrelated objects
                return std::less<class table const*> {}(left_table, right_table);
            }
            return std::string_view { std::get<1>(left) } < std::string_view { std::get<1>(right) };
        }
    };

    // indices_ ordered by their origin table and ID
    using table_index_map_type = std::map<
            table_scoped_key_type,
            std::shared_ptr<index const>,
            table_scoped_key_compare>;

    ::takatori::util::maybe_shared_ptr<provider const> parent_;
    relation_map_type relations_;
    index_map_type indices_;
    table_index_map_type table_indices_;
    sequence_map_type sequences_;

    [[nodiscard]] table_index_map_type::const_iterator lower_bound_table_index(class table const* table) const;
    void remove_table_index(std::string_view key, index const& element) noexcept;

    template<auto Find, class Container, class Element>
    void internal_add(Container& container, std::shared_ptr<Element const> element, bool overwrite);

    template<class Container>
    bool internal_remove(Container& container, std::string_view id);
};

} // namespace yugawara::storage::details

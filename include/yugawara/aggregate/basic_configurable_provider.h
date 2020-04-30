#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include <takatori/util/object_creator.h>

#include "provider.h"

namespace yugawara::aggregate {

/**
 * @brief an implementation of provider that can configurable its contents.
 * @tparam Compare the (less than) comparator type, must satisfy *Compare*
 * @tparam Mutex the mutex type, must satisfy *DefaultConstructible* and *BasicLockable*
 * @note This class works as thread-safe only if the mutex works right
 */
template<class Compare, class Mutex>
class basic_configurable_provider : public provider {
public:
    /// @brief the mutex type.
    using mutex_type = Mutex;

    /// @brief the comparator type.
    using compare_type = Compare;

    /**
     * @brief creates a new object.
     * @param parent the parent provider (nullable)
     * @param creator the object creator
     * @param compare the name comparator object
     */
    explicit basic_configurable_provider(
            std::shared_ptr<provider const> parent = {},
            takatori::util::object_creator creator = {},
            compare_type const& compare = compare_type{}) noexcept
        : parent_(std::move(parent))
        , declarations_(
                compare,
                creator.allocator(std::in_place_type<typename decltype(declarations_)::value_type>))
    {}

    using provider::each;

    void each(consumer_type const& consumer) const override {
        internal_each(consumer);
        if (parent_) {
            parent_->each(consumer);
        }
    }

    void each(
            std::string_view name,
            ::takatori::relation::set_quantifier quantifier,
            std::size_t parameter_count,
            consumer_type const& consumer) const override {
        internal_each(name, quantifier, parameter_count, consumer);
        if (parent_) {
            parent_->each(name, quantifier, parameter_count, consumer);
        }
    }

    /**
     * @brief adds a function declaration.
     * @param element the the target declaration
     * @return the added element
     */
    std::shared_ptr<declaration> const& add(std::shared_ptr<declaration> element) {
        key_type key { element->name(), get_object_creator().allocator(std::in_place_type<char>) };
        std::lock_guard lock { mutex_ };
        auto iter = declarations_.emplace(std::move(key), std::move(element));
        return iter->second;
    }

    /// @copydoc add()
    std::shared_ptr<declaration> const& add(declaration&& element) {
        auto ptr = get_object_creator().template create_shared<declaration>(std::move(element));
        return add(std::move(ptr));
    }

    /**
     * @brief removes the target declaration directly added into this provider.
     * @param element the target declaration
     * @return true if successfully removed
     * @return false otherwise
     */
    bool remove(declaration const& element) {
        auto&& name = element.name();
        auto&& id = element.definition_id();

        std::lock_guard lock { mutex_ };
        auto first = declarations_.lower_bound(name);
        auto last = declarations_.upper_bound(name);
        for (auto iter = first; iter != last; ++iter) {
            if (auto&& found = iter->second; found && found->definition_id() == id) {
                declarations_.erase(iter);
                return true;
            }
        }
        return false;
    }

    /**
     * @brief returns the object creator.
     * @return the object creator
     */
    [[nodiscard]] ::takatori::util::object_creator get_object_creator() const {
        return declarations_.get_allocator();
    }

private:
    using key_type = std::basic_string<char, std::char_traits<char>, takatori::util::object_allocator<char>>;
    using value_type = std::shared_ptr<declaration>;
    using map_type = std::multimap<
            key_type,
            value_type,
            compare_type,
            takatori::util::object_allocator<std::pair<key_type const, value_type>>>;

    std::shared_ptr<provider const> parent_;
    map_type declarations_;
    mutable mutex_type mutex_ {};


    void internal_each(consumer_type const& consumer) const {
        std::lock_guard lock { mutex_ };
        for (auto&& [name, declaration] : declarations_) {
            (void) name;
            consumer(declaration);
        }
    }

    void internal_each(
            std::string_view name,
            ::takatori::relation::set_quantifier quantifier,
            std::size_t parameter_count,
            consumer_type const& consumer) const {
        std::lock_guard lock { mutex_ };
        auto first = declarations_.lower_bound(name);
        auto last = declarations_.upper_bound(name);
        for (auto iter = first; iter != last; ++iter) {
            if (auto&& found = iter->second;
                    found
                    && found->name() == name
                    && found->quantifier() == quantifier
                    && found->parameter_types().size() == parameter_count) {
                consumer(found);
            }
        }
    }
};

} // namespace yugawara::aggregate

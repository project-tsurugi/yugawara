#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include <takatori/util/exception.h>
#include <takatori/util/object_creator.h>
#include <takatori/util/detect.h>

#include "provider.h"

namespace yugawara::variable {

/// @cond IMPL_DEFS
namespace impl {

template<class T>
using compare_is_transparent_t = typename T::is_transparent;

} // namespace impl
/// @endcond

/**
 * @brief an implementation of provider that can configurable its contents.
 * @tparam Compare the (less than) comparator type, must satisfy *Compare* and have `is_transparent`
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

    static_assert(takatori::util::is_detected_v<impl::compare_is_transparent_t, compare_type>);

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

    void each(consumer_type const& consumer) const override {
        internal_each(consumer);
        if (parent_) {
            parent_->each([&](auto& d) {
                if (!d) return;
                if (auto iter = declarations_.find(d->name()); iter == declarations_.end()) {
                    consumer(d);
                }
            });
        }
    }

    [[nodiscard]] std::shared_ptr<declaration const> find(std::string_view name) const override {
        if (auto& found = internal_find(name)) {
            return found;
        }
        if (parent_) {
            if (auto found = parent_->find(name)) {
                return found;
            }
        }
        return {};
    }

    /**
     * @brief adds an external variable declaration.
     * @param element the the target declaration
     * @param overwrite true to overwrite the existing entry
     * @return the added element
     * @throws std::invalid_argument if the target entry already exists and `overwrite=false`
     * @note This operation may **hide** elements defined in parent providers if `overwrite=true`
     */
    std::shared_ptr<declaration> const& add(std::shared_ptr<declaration> element, bool overwrite = false) {
        using ::takatori::util::throw_exception;
        key_type key { element->name(), get_object_creator().allocator(std::in_place_type<char>) };
        std::lock_guard lock { mutex_ };
        if (overwrite) {
            auto [iter, success] = declarations_.insert_or_assign(std::move(key), std::move(element));
            (void) success;
            return iter->second;
        }
        if (parent_ && parent_->find(key)) {
            throw_exception(std::invalid_argument(std::string("already exists in parent provider: ") += key));
        }
        if (auto [iter, success] = declarations_.try_emplace(std::move(key), std::move(element)); success) {
            return iter->second;
        }
        // NOTE: if try_emplace was failed, `key` must be not changed
        throw_exception(std::invalid_argument(std::string("already exists: ") += key));
    }

    /// @copydoc add()
    std::shared_ptr<declaration> const& add(declaration&& element, bool overwrite = false) {
        auto ptr = get_object_creator().template create_shared<declaration>(std::move(element));
        return add(std::move(ptr), overwrite);
    }

    /**
     * @brief removes the target declaration directly added into this provider.
     * @param element the target declaration
     * @return true if successfully removed
     * @return false otherwise
     */
    bool remove(declaration const& element) {
        auto&& name = element.name();

        std::lock_guard lock { mutex_ };
        if (auto iter = declarations_.find(name); iter != declarations_.end()) {
            declarations_.erase(iter);
            return true;
        }
        return false;
    }

    /**
     * @brief returns the object creator.
     * @return the object creator
     */
    [[nodiscard]] takatori::util::object_creator get_object_creator() const {
        return declarations_.get_allocator();
    }

private:
    using key_type = std::basic_string<char, std::char_traits<char>, takatori::util::object_allocator<char>>;
    using value_type = std::shared_ptr<declaration>;
    using map_type = std::map<
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

    std::shared_ptr<declaration> const& internal_find(std::string_view name) const {
        std::lock_guard lock { mutex_ };
        if (auto iter = declarations_.find(name); iter != declarations_.end()) {
            return iter->second;
        }
        static std::shared_ptr<declaration> not_found {};
        return not_found;
    }
};

} // namespace yugawara::variable

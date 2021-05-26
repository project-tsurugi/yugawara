#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

#include <takatori/util/detect.h>
#include <takatori/util/exception.h>
#include <takatori/util/maybe_shared_ptr.h>
#include <takatori/util/string_builder.h>

#include <yugawara/util/maybe_shared_lock.h>

#include "provider.h"

namespace yugawara::variable {

/**
 * @brief an implementation of provider that can configurable its contents.
 * @tparam Mutex the mutex type, must satisfy *DefaultConstructible* and *BasicLockable*
 * @note This class works as thread-safe only if the mutex works right
 */
template<class Mutex>
class basic_configurable_provider : public provider {
public:
    /// @brief the mutex type.
    using mutex_type = Mutex;

    /// @brief the writer lock type.
    using writer_lock_type = std::unique_lock<mutex_type>;

    /// @brief the readers lock type.
    using reader_lock_type = util::maybe_shared_lock<mutex_type>;

    /**
     * @brief creates a new object.
     * @param parent the parent provider (nullable)
     */
    explicit basic_configurable_provider(
            ::takatori::util::maybe_shared_ptr<provider const> parent = {}) noexcept :
        parent_ { std::move(parent) }
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
        using ::takatori::util::string_builder;
        key_type key { element->name() };
        writer_lock_type lock { mutex_ };
        if (overwrite) {
            auto [iter, success] = declarations_.insert_or_assign(std::move(key), std::move(element));
            (void) success;
            return iter->second;
        }
        if (parent_ && parent_->find(key)) {
            throw_exception(std::invalid_argument(string_builder {}
                    << "already exists in parent provider: "
                    << key
                    << string_builder::to_string));
        }
        if (auto [iter, success] = declarations_.try_emplace(std::move(key), std::move(element)); success) {
            return iter->second;
        }
        // NOTE: if try_emplace was failed, `key` must be not changed
        throw_exception(std::invalid_argument(string_builder {}
                << "already exists: "
                << key
                << string_builder::to_string));
    }

    /// @copydoc add()
    std::shared_ptr<declaration> const& add(declaration&& element, bool overwrite = false) {
        auto ptr = std::make_shared<declaration>(std::move(element));
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

        writer_lock_type lock { mutex_ };
        if (auto iter = declarations_.find(name); iter != declarations_.end()) {
            declarations_.erase(iter);
            return true;
        }
        return false;
    }

private:
    using key_type = std::string;
    using value_type = std::shared_ptr<declaration>;
    using map_type = std::map<
            key_type,
            value_type,
            std::less<>>;

    ::takatori::util::maybe_shared_ptr<provider const> parent_;
    map_type declarations_;
    mutable mutex_type mutex_ {};

    void internal_each(consumer_type const& consumer) const {
        reader_lock_type lock { mutex_ };
        for (auto&& [name, declaration] : declarations_) {
            (void) name;
            consumer(declaration);
        }
    }

    std::shared_ptr<declaration> const& internal_find(std::string_view name) const {
        reader_lock_type lock { mutex_ };
        if (auto iter = declarations_.find(name); iter != declarations_.end()) {
            return iter->second;
        }
        static std::shared_ptr<declaration> not_found {};
        return not_found;
    }
};

} // namespace yugawara::variable

#pragma once

#include <ostream>
#include <string>

#include <takatori/document/region.h>

namespace yugawara {

/**
 * @brief diagnostic information.
 * @tparam T the diagnostic code type
 */
template<class T>
class diagnostic {
public:
    /// @brief the diagnostic code type.
    using code_type = T;
    /// @brief the diagnostic message type.
    using message_type = std::string;
    /// @brief the document location type.
    using location_type = ::takatori::document::region;

    /**
     * @brief creates a new empty instance.
     */
    diagnostic() noexcept = default;

    /**
     * @brief creates a new instance.
     * @param code the diagnostic code
     * @param message the optional diagnostic message
     * @param location the diagnostic location
     */
    diagnostic(code_type code, message_type message, location_type location) noexcept
        : code_(std::move(code))
        , message_(std::move(message))
        , location_(location)
    {}

    /**
     * @brief creates a new instance.
     * @param code the diagnostic code
     * @param location the diagnostic location
     */
    diagnostic(code_type code, location_type location) noexcept
        : diagnostic(std::move(code), {}, location)
    {}

    /**
     * @brief returns the diagnostic code.
     * @return the diagnostic code
     */
    [[nodiscard]] code_type code() const noexcept {
        return code_;
    }

    /**
     * @brief returns the diagnostic message.
     * @return the diagnostic message
     */
    [[nodiscard]] message_type const& message() const noexcept {
        return message_;
    }

    /**
     * @brief returns the diagnostic location.
     * @return the diagnostic location
     */
    [[nodiscard]] location_type const& location() const noexcept {
        return location_;
    }

private:
    code_type code_ {};
    message_type message_ {};
    location_type location_ {};
};

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output stream
 */
template<class T>
inline std::ostream& operator<<(std::ostream& out, diagnostic<T> const& value) {
    return out << "diagnostic("
               << "code=" << value.code() << ", "
               << "message='" << value.message() << "', "
               << "location=" << value.location() << ")";
}

} // namespace yugawara


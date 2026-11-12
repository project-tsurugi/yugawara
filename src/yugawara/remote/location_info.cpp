#include <yugawara/remote/location_info.h>

#include <utility>

namespace yugawara::remote {

location_info::location_info(location_id_type location_id, std::string location_name) noexcept :
    location_id_ { location_id },
    location_name_ { std::move(location_name) }
{}

location_info::location_id_type location_info::location_id() const noexcept {
    return location_id_;
}

std::string_view location_info::location_name() const noexcept {
    return location_name_;
}

} // namespace yugawara::remote

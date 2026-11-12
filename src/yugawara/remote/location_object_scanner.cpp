#include <yugawara/remote/location_object_scanner.h>

#include <stdexcept>
#include <string_view>
#include <utility>

#include <takatori/util/optional_ptr.h>

namespace yugawara::remote {

using namespace std::string_view_literals;

using ::takatori::serializer::object_acceptor;
using ::takatori::util::optional_ptr;

namespace {

[[nodiscard]] optional_ptr<location_info const> find_location_info(
        ::takatori::util::maybe_shared_ptr<symbol_location_query const> const& query,
        location_info::location_id_type location_id) {
    if (!query) {
        return {};
    }
    try {
        return query->of(location_id);
    } catch (std::out_of_range const&) {
        // the location information is not available
        return {};
    }
}

} // namespace

location_object_scanner::location_object_scanner(
        ::yugawara::serializer::object_scanner base,
        std::shared_ptr<operation_location_query const> operation_location_query,
        ::takatori::util::maybe_shared_ptr<symbol_location_query const> symbol_location_query) noexcept :
    ::yugawara::serializer::object_scanner { std::move(base) },
    operation_location_query_ { std::move(operation_location_query) },
    symbol_location_query_ { std::move(symbol_location_query) }
{}

void location_object_scanner::properties(::takatori::plan::step const& element, object_acceptor& acceptor) const {
    ::yugawara::serializer::object_scanner::properties(element, acceptor);
    if (operation_location_query_) {
        accept_locations(operation_location_query_->find(element), acceptor);
    }
}

void location_object_scanner::properties(::takatori::statement::statement const& element, object_acceptor& acceptor) const {
    ::yugawara::serializer::object_scanner::properties(element, acceptor);
    if (operation_location_query_) {
        accept_locations(operation_location_query_->find(element), acceptor);
    }
}

void location_object_scanner::accept_locations(location_id_set_type const& locations, object_acceptor& acceptor) const {
    acceptor.property_begin("operation_locations"sv);
    acceptor.array_begin();
    for (auto location_id : locations) { // ordered by location ID
        acceptor.struct_begin();

        acceptor.property_begin("location_id"sv);
        acceptor.unsigned_integer(location_id);
        acceptor.property_end();

        acceptor.property_begin("location_name"sv);
        if (auto info = find_location_info(symbol_location_query_, location_id)) {
            acceptor.string(info->location_name());
        }
        acceptor.property_end();

        acceptor.struct_end();
    }
    acceptor.array_end();
    acceptor.property_end();
}

} // namespace yugawara::remote

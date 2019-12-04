#include "yugawara/type/repository.h"

namespace yugawara::type {

repository& default_repository() noexcept {
    static repository repo;
    return repo;
}

} // namespace yugawara::type

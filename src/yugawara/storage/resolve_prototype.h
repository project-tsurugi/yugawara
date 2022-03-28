#pragma once

#include <memory>
#include <vector>

#include <takatori/statement/statement.h>
#include <takatori/statement/create_table.h>
#include <takatori/statement/create_index.h>

#include <yugawara/storage/prototype_processor.h>

namespace yugawara::storage {

std::vector<prototype_processor::diagnostic_type> resolve_prototype(
        ::takatori::statement::statement& source,
        prototype_processor& storage_processor);

std::vector<prototype_processor::diagnostic_type> resolve_create_table(
        ::takatori::statement::create_table& source,
        prototype_processor& storage_processor);

std::vector<prototype_processor::diagnostic_type> resolve_create_index(
        ::takatori::statement::create_index& source,
        prototype_processor& storage_processor);

} // namespace yugawara::storage

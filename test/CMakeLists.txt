function (add_test_executable source_file)
    get_filename_component(test_name "${source_file}" NAME_WE)
    add_executable(${test_name}
        ${source_file}
    )
    target_include_directories(${test_name}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(${test_name}
        PRIVATE yugawara-impl
        PRIVATE yugawara-test-main
    )
    add_test(
        NAME ${test_name}
        COMMAND ${test_name} --gtest_output=xml:${test_name}_gtest_result.xml
    )
endfunction (add_test_executable)

add_library(yugawara-test-main STATIC
    main.cpp
)

target_link_libraries(yugawara-test-main
    PUBLIC gtest
)

# type system
add_test_executable(type/type_category_test.cpp)

# variable models
add_test_executable(variable/variable_declaration_test.cpp)
add_test_executable(variable/nullity_test.cpp)
add_test_executable(variable/comparison_test.cpp)
add_test_executable(variable/negation_test.cpp)
add_test_executable(variable/quantification_test.cpp)
add_test_executable(variable/criteria_test.cpp)
add_test_executable(variable/predicate_dispatch_test.cpp)
add_test_executable(variable/variable_configurable_provider_test.cpp)

# function information
add_test_executable(function/function_declaration_test.cpp)
add_test_executable(function/function_configurable_provider_test.cpp)

# aggregate function information
add_test_executable(aggregate/aggregate_declaration_test.cpp)
add_test_executable(aggregate/aggregate_configurable_provider_test.cpp)

# external storage information
add_test_executable(storage/column_test.cpp)
add_test_executable(storage/table_test.cpp)
add_test_executable(storage/index_test.cpp)
add_test_executable(storage/storage_configurable_provider_test.cpp)

# analyzer
add_test_executable(analyzer/type_diagnostic_test.cpp)
add_test_executable(analyzer/expression_analyzer_scalar_test.cpp)
add_test_executable(analyzer/expression_analyzer_relation_test.cpp)
add_test_executable(analyzer/expression_analyzer_plan_test.cpp)
add_test_executable(analyzer/block_test.cpp)
add_test_executable(analyzer/block_builder_test.cpp)
add_test_executable(analyzer/variable_liveness_analyzer_test.cpp)
add_test_executable(analyzer/step_plan_builder_test.cpp)

# planner
add_test_executable(analyzer/details/exchange_step_collector_test.cpp)
add_test_executable(analyzer/details/process_step_collector_test.cpp)
add_test_executable(analyzer/details/step_relation_collector_test.cpp)
add_test_executable(analyzer/details/scalar_expression_variable_rewriter_test.cpp)
add_test_executable(analyzer/details/exchange_column_collector_test.cpp)
add_test_executable(analyzer/details/stream_variable_rewriter_test.cpp)

# optimizer
add_test_executable(analyzer/details/projection_decomposer_test.cpp)
add_test_executable(analyzer/details/redundant_stream_variable_remover_test.cpp)
add_test_executable(analyzer/details/stream_variable_flow_info_test.cpp)
add_test_executable(analyzer/details/predicate_decomposer_test.cpp)
add_test_executable(analyzer/details/stream_variable_collector_test.cpp)

# utilities
add_test_executable(util/either_test.cpp)
add_test_executable(util/object_cache_test.cpp)
add_test_executable(util/object_repository_test.cpp)
add_test_executable(util/ternary_test.cpp)

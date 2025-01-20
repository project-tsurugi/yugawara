set(test_target yugawara-test)

add_executable(${test_target}
    main.cpp
)

target_include_directories(${test_target}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${test_target}
    PUBLIC gtest
    PRIVATE yugawara-impl
)

function (add_test_executable source_file)
    get_filename_component(test_name "${source_file}" NAME_WE)
    target_sources(${test_target}
        PRIVATE ${source_file}
    )
    add_test(
        NAME ${test_name}
        COMMAND ${test_target} --gtest_filter=${test_name}.* --gtest_output=xml:${test_name}_gtest_result.xml
    )
endfunction (add_test_executable)

# root
add_test_executable(yugawara/compiler_test.cpp)
add_test_executable(yugawara/details/collect_restricted_features_test.cpp)

# type system
add_test_executable(yugawara/type/type_category_test.cpp)
add_test_executable(yugawara/type/type_conversion_unifying_test.cpp)
add_test_executable(yugawara/type/type_conversion_assignment_test.cpp)
add_test_executable(yugawara/type/type_conversion_casting_test.cpp)
add_test_executable(yugawara/type/type_conversion_overload_test.cpp)

# variable models
add_test_executable(yugawara/variable/variable_declaration_test.cpp)
add_test_executable(yugawara/variable/nullity_test.cpp)
add_test_executable(yugawara/variable/comparison_test.cpp)
add_test_executable(yugawara/variable/negation_test.cpp)
add_test_executable(yugawara/variable/quantification_test.cpp)
add_test_executable(yugawara/variable/criteria_test.cpp)
add_test_executable(yugawara/variable/predicate_dispatch_test.cpp)
add_test_executable(yugawara/variable/variable_configurable_provider_test.cpp)

# function information
add_test_executable(yugawara/function/function_declaration_test.cpp)
add_test_executable(yugawara/function/function_configurable_provider_test.cpp)

# aggregate function information
add_test_executable(yugawara/aggregate/aggregate_declaration_test.cpp)
add_test_executable(yugawara/aggregate/aggregate_configurable_provider_test.cpp)

# external storage information
add_test_executable(yugawara/storage/column_test.cpp)
add_test_executable(yugawara/storage/column_value_test.cpp)
add_test_executable(yugawara/storage/table_test.cpp)
add_test_executable(yugawara/storage/index_test.cpp)
add_test_executable(yugawara/storage/sequence_test.cpp)
add_test_executable(yugawara/storage/storage_basic_prototype_processor_test.cpp)
add_test_executable(yugawara/storage/storage_configurable_provider_test.cpp)

# schema information
add_test_executable(yugawara/schema/schema_declaration_test.cpp)
add_test_executable(yugawara/schema/schema_configurable_provider_test.cpp)
add_test_executable(yugawara/schema/catalog_test.cpp)

# extensions
add_test_executable(yugawara/extension/scalar/aggregate_function_call_test.cpp)

# analyzer
add_test_executable(yugawara/analyzer/expression_analyzer_scalar_test.cpp)
add_test_executable(yugawara/analyzer/expression_analyzer_relation_test.cpp)
add_test_executable(yugawara/analyzer/expression_analyzer_plan_test.cpp)
add_test_executable(yugawara/analyzer/expression_analyzer_statement_test.cpp)
add_test_executable(yugawara/analyzer/block_test.cpp)
add_test_executable(yugawara/analyzer/block_builder_test.cpp)
add_test_executable(yugawara/analyzer/variable_liveness_analyzer_test.cpp)
add_test_executable(yugawara/analyzer/step_plan_builder_test.cpp)

# planner
add_test_executable(yugawara/analyzer/details/collect_exchange_steps_test.cpp)
add_test_executable(yugawara/analyzer/details/collect_process_steps_test.cpp)
add_test_executable(yugawara/analyzer/details/step_relation_collector_test.cpp)
add_test_executable(yugawara/analyzer/details/scalar_expression_variable_rewriter_test.cpp)
add_test_executable(yugawara/analyzer/details/collect_exchange_columns_test.cpp)
add_test_executable(yugawara/analyzer/details/rewrite_stream_variables_test.cpp)

# optimizer
add_test_executable(yugawara/analyzer/details/decompose_predicate_test.cpp)
add_test_executable(yugawara/analyzer/details/remove_redundant_stream_variables_test.cpp)
add_test_executable(yugawara/analyzer/details/stream_variable_flow_info_test.cpp)
add_test_executable(yugawara/analyzer/details/predicate_decomposer_test.cpp)
add_test_executable(yugawara/analyzer/details/collect_stream_variables_test.cpp)
add_test_executable(yugawara/analyzer/details/push_down_filters_test.cpp)
add_test_executable(yugawara/analyzer/details/simplify_predicate_test.cpp)
add_test_executable(yugawara/analyzer/details/remove_redundant_conditions_test.cpp)
add_test_executable(yugawara/analyzer/details/search_key_term_builder_test.cpp)
add_test_executable(yugawara/analyzer/details/scan_key_collector_test.cpp)
add_test_executable(yugawara/analyzer/details/rewrite_join_test.cpp)
add_test_executable(yugawara/analyzer/details/collect_join_keys_test.cpp)
add_test_executable(yugawara/analyzer/details/rewrite_scan_test.cpp)
add_test_executable(yugawara/analyzer/details/classify_expression_test.cpp)
add_test_executable(yugawara/analyzer/details/inline_variables_test.cpp)
add_test_executable(yugawara/analyzer/details/collect_local_variables_test.cpp)
add_test_executable(yugawara/analyzer/intermediate_plan_optimizer_test.cpp)

# serializer
add_test_executable(yugawara/serializer/object_scanner_test.cpp)

# utilities
add_test_executable(yugawara/util/either_test.cpp)
add_test_executable(yugawara/util/move_only_test.cpp)
add_test_executable(yugawara/util/object_cache_test.cpp)
add_test_executable(yugawara/util/object_repository_test.cpp)
add_test_executable(yugawara/util/ternary_test.cpp)

add_library(yugawara

    # root
    yugawara/compiler.cpp
    yugawara/compiler_options.cpp
    yugawara/compiled_info.cpp
    yugawara/compiler_result.cpp

    # storage information
    yugawara/storage/relation.cpp
    yugawara/storage/table.cpp
    yugawara/storage/column.cpp
    yugawara/storage/column_value.cpp
    yugawara/storage/index.cpp
    yugawara/storage/sequence.cpp
    yugawara/storage/provider.cpp
    yugawara/storage/basic_prototype_processor.cpp
    yugawara/storage/details/index_key_element.cpp
    yugawara/storage/details/search_key_element.cpp

    yugawara/storage/resolve_prototype.cpp

    # type system
    yugawara/type/category.cpp
    yugawara/type/conversion.cpp
    yugawara/type/repository.cpp

    # function information
    yugawara/function/declaration.cpp
    yugawara/function/utils.cpp

    # aggregate function information
    yugawara/aggregate/declaration.cpp

    # descriptor entities
    yugawara/binding/relation_info.cpp
    yugawara/binding/exchange_info.cpp
    yugawara/binding/index_info.cpp
    yugawara/binding/variable_info.cpp
    yugawara/binding/table_column_info.cpp
    yugawara/binding/external_variable_info.cpp
    yugawara/binding/function_info.cpp
    yugawara/binding/aggregate_function_info.cpp
    yugawara/binding/schema_info.cpp
    yugawara/binding/table_info.cpp
    yugawara/binding/extract.cpp
    yugawara/binding/factory.cpp

    # variable binding
    yugawara/variable/declaration.cpp
    yugawara/variable/criteria.cpp
    yugawara/variable/predicate.cpp
    yugawara/variable/comparison.cpp
    yugawara/variable/negation.cpp
    yugawara/variable/quantification.cpp

    # schema
    yugawara/schema/declaration.cpp
    yugawara/schema/catalog.cpp
    yugawara/schema/search_path.cpp

    # extensions
    yugawara/extension/scalar/aggregate_function_call.cpp

    # analyzer
    yugawara/analyzer/expression_mapping.cpp
    yugawara/analyzer/expression_resolution.cpp
    yugawara/analyzer/variable_mapping.cpp
    yugawara/analyzer/variable_resolution.cpp
    yugawara/analyzer/expression_analyzer.cpp
    yugawara/analyzer/block.cpp
    yugawara/analyzer/block_algorithm.cpp
    yugawara/analyzer/block_builder.cpp
    yugawara/analyzer/variable_liveness_info.cpp
    yugawara/analyzer/variable_liveness_analyzer.cpp

    # planner
    yugawara/analyzer/step_plan_builder.cpp
    yugawara/analyzer/join_strategy.cpp
    yugawara/analyzer/join_info.cpp
    yugawara/analyzer/aggregate_strategy.cpp
    yugawara/analyzer/aggregate_info.cpp
    yugawara/analyzer/details/block_expression_util.cpp
    yugawara/analyzer/details/step_plan_builder_options.cpp
    yugawara/analyzer/details/collect_process_steps.cpp
    yugawara/analyzer/details/collect_exchange_steps.cpp
    yugawara/analyzer/details/collect_step_relations.cpp
    yugawara/analyzer/details/stream_variable_rewriter_context.cpp
    yugawara/analyzer/details/exchange_column_info.cpp
    yugawara/analyzer/details/exchange_column_info_map.cpp
    yugawara/analyzer/details/collect_exchange_columns.cpp
    yugawara/analyzer/details/scalar_expression_variable_rewriter.cpp
    yugawara/analyzer/details/rewrite_stream_variables.cpp

    # optimizer
    yugawara/analyzer/intermediate_plan_optimizer.cpp
    yugawara/analyzer/details/boolean_constants.cpp
    yugawara/analyzer/details/intermediate_plan_optimizer_options.cpp
    yugawara/analyzer/details/decompose_projections.cpp
    yugawara/analyzer/details/remove_redundant_stream_variables.cpp
    yugawara/analyzer/details/stream_variable_flow_info.cpp
    yugawara/analyzer/details/stream_variable_flow_info_entry.cpp
    yugawara/analyzer/details/decompose_predicate.cpp
    yugawara/analyzer/details/collect_stream_variables.cpp
    yugawara/analyzer/details/push_down_filters.cpp
    yugawara/analyzer/details/simplify_predicate.cpp
    yugawara/analyzer/details/remove_redundant_conditions.cpp
    yugawara/analyzer/details/index_estimator_result.cpp
    yugawara/analyzer/details/default_index_estimator.cpp
    yugawara/analyzer/details/search_key_term.cpp
    yugawara/analyzer/details/search_key_term_builder.cpp
    yugawara/analyzer/details/scan_key_collector.cpp
    yugawara/analyzer/details/rewrite_join.cpp
    yugawara/analyzer/details/collect_join_keys.cpp
    yugawara/analyzer/details/rewrite_scan.cpp

    # analyzer misc.
    yugawara/analyzer/details/detect_join_endpoint_style.cpp

    # serializer
    yugawara/serializer/object_scanner.cpp
    yugawara/serializer/details/binding_scanner.cpp
    yugawara/serializer/details/resolution_scanner.cpp
    yugawara/serializer/details/predicate_scanner.cpp
    yugawara/serializer/details/extension_type_property_scanner.cpp
    yugawara/serializer/details/extension_scalar_property_scanner.cpp
)

target_include_directories(yugawara
    PRIVATE .
)

target_link_libraries(yugawara
    PUBLIC yugawara-api
    PUBLIC takatori
    PRIVATE tsl::hopscotch_map
)

set_target_properties(yugawara
    PROPERTIES
        SOVERSION ${PROJECT_VERSION}
)

# Add INSTALL_RPATH from CMAKE_INSTALL_PREFIX and CMAKE_PREFIX_PATH
# The default behavior of CMake omits RUNPATH if it is already in CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES.
if (FORCE_INSTALL_RPATH)
    get_target_property(rpath yugawara INSTALL_RPATH)

    # add ${CMAKE_INSTALL_PREFIX}/lib if it is not in system link directories
    get_filename_component(p "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}" ABSOLUTE)
    list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${p}" is_system)
    if (is_system STREQUAL "-1")
        list(APPEND rpath "${p}")
    endif()

    # add each ${CMAKE_PREFIX_PATH}/lib
    foreach (p IN LISTS CMAKE_PREFIX_PATH)
        get_filename_component(p "${p}/${CMAKE_INSTALL_LIBDIR}" ABSOLUTE)
        list(APPEND rpath "${p}")
    endforeach()

    if (rpath)
        set_target_properties(yugawara
            PROPERTIES
            INSTALL_RPATH "${rpath}"
            )
    endif()

    # add other than */lib paths
    set_target_properties(yugawara
        PROPERTIES
        INSTALL_RPATH_USE_LINK_PATH ON
        )
endif (FORCE_INSTALL_RPATH)

install(
    TARGETS
        yugawara
    EXPORT
        ${export_name}
    LIBRARY
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT Runtime
    ARCHIVE
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/${export_name}
        COMPONENT Development
    RUNTIME
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Runtime
)

# for tests
add_library(yugawara-impl INTERFACE)

target_include_directories(yugawara-impl
    INTERFACE .
)

target_link_libraries(yugawara-impl
    INTERFACE yugawara
    INTERFACE tsl::hopscotch_map
)

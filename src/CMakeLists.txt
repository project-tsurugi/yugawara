add_library(yugawara

    # storage information
    storage/relation.cpp
    storage/table.cpp
    storage/column.cpp
    storage/index.cpp
    storage/provider.cpp
    storage/details/index_key_element.cpp
    storage/details/search_key_element.cpp

    # type system
    type/category.cpp
    type/conversion.cpp
    type/repository.cpp

    # function information
    function/declaration.cpp

    # aggregate function information
    aggregate/declaration.cpp

    # descriptor entities
    binding/relation_info.cpp
    binding/exchange_info.cpp
    binding/index_info.cpp
    binding/variable_info.cpp
    binding/table_column_info.cpp
    binding/external_variable_info.cpp
    binding/function_info.cpp
    binding/aggregate_function_info.cpp
    binding/factory.cpp

    # variable binding
    variable/declaration.cpp
    variable/criteria.cpp
    variable/predicate.cpp
    variable/comparison.cpp
    variable/negation.cpp
    variable/quantification.cpp

    # analyzer
    analyzer/variable_mapping.cpp
    analyzer/expression_mapping.cpp
    analyzer/expression_analyzer.cpp
    analyzer/type_diagnostic.cpp
    analyzer/variable_resolution.cpp
    analyzer/block.cpp
    analyzer/block_algorithm.cpp
    analyzer/block_builder.cpp
    analyzer/variable_liveness_info.cpp
    analyzer/variable_liveness_analyzer.cpp

    # planner
    analyzer/step_plan_builder.cpp
    analyzer/join_strategy.cpp
    analyzer/join_info.cpp
    analyzer/aggregate_info.cpp
    analyzer/details/block_expression_util.cpp
    analyzer/details/step_plan_builder_options.cpp
    analyzer/details/collect_process_steps.cpp
    analyzer/details/collect_exchange_steps.cpp
    analyzer/details/collect_step_relations.cpp
    analyzer/details/stream_variable_rewriter_context.cpp
    analyzer/details/exchange_column_info.cpp
    analyzer/details/exchange_column_info_map.cpp
    analyzer/details/collect_exchange_columns.cpp
    analyzer/details/scalar_expression_variable_rewriter.cpp
    analyzer/details/rewrite_stream_variables.cpp

    # optimizer
    analyzer/intermediate_plan_optimizer.cpp
    analyzer/details/boolean_constants.cpp
    analyzer/details/intermediate_plan_optimizer_options.cpp
    analyzer/details/decompose_projections.cpp
    analyzer/details/remove_redundant_stream_variables.cpp
    analyzer/details/stream_variable_flow_info.cpp
    analyzer/details/stream_variable_flow_info_entry.cpp
    analyzer/details/decompose_predicate.cpp
    analyzer/details/collect_stream_variables.cpp
    analyzer/details/push_down_filters.cpp
    analyzer/details/simplify_predicate.cpp
    analyzer/details/remove_redundant_conditions.cpp
    analyzer/details/index_estimator_result.cpp
    analyzer/details/default_index_estimator.cpp
    analyzer/details/search_key_term.cpp
    analyzer/details/search_key_term_builder.cpp
    analyzer/details/scan_key_collector.cpp
    analyzer/details/rewrite_join.cpp
    analyzer/details/collect_join_keys.cpp
    analyzer/details/rewrite_scan.cpp

    # analyzer misc.
    analyzer/details/detect_join_endpoint_style.cpp

    # serializer
    serializer/object_scanner.cpp
    serializer/details/binding_scanner.cpp
    serializer/details/variable_resolution_scanner.cpp
    serializer/details/predicate_scanner.cpp
)

add_executable(yugawara-exec
    main.cpp
)

target_include_directories(yugawara
    PRIVATE .
)

target_include_directories(yugawara-exec
    PRIVATE .
)

target_link_libraries(yugawara
    PUBLIC yugawara-api
    PUBLIC takatori
    PRIVATE tsl::hopscotch_map
)

target_link_libraries(yugawara-exec
    PUBLIC yugawara
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

set_target_properties(yugawara-exec
    PROPERTIES
        SOVERSION ${PROJECT_VERSION}
)

install(
    TARGETS
        yugawara
        yugawara-exec
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

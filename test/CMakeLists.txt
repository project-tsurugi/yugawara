function (add_test_executable source_file)
    get_filename_component(test_name "${source_file}" NAME_WE)
    add_executable(${test_name}
        ${source_file}
    )
    target_include_directories(${test_name}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(${test_name}
        PRIVATE yugawara
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

# utilities
add_test_executable(util/either_test.cpp)
add_test_executable(util/object_cache_test.cpp)
add_test_executable(util/object_repository_test.cpp)

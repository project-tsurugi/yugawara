add_library(yugawara
    type/conversion.cpp
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
)

target_link_libraries(yugawara-exec
    PUBLIC yugawara
)

set_target_properties(yugawara
    PROPERTIES
        SOVERSION ${PROJECT_VERSION}
)

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

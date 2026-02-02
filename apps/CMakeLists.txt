if(NOT ENABLE_NTEST)
    message("FOO" ${NS_ALL_OBJECTS})
    add_executable(test test.c ${NS_ALL_OBJECTS})
    target_include_directories(test PRIVATE ${NS_ALL_INCLUDES})
    target_link_libraries(test PRIVATE m backtrace pthread)
endif()

add_ns_executable(nspprint
    SOURCES
        tools/nspprint.c
    DEPENDENCIES
        ${LIBS}/nsroutines
        ${LIBS}/nspager
    EXTERNAL_LIBS
        backtrace
        pthread
)

add_ns_executable(walfprint
    SOURCES
        tools/walfprint.c
    DEPENDENCIES
        ${LIBS}/nsroutines
        ${LIBS}/nspager
    EXTERNAL_LIBS
        backtrace
        pthread
)

add_ns_executable(client_server
    SOURCES
        client_server.c
    DEPENDENCIES
        ${LIBS}/nsroutines
        ${LIBS}/nsnet
        ${LIBS}/nspager
    EXTERNAL_LIBS
        backtrace
        pthread
)

add_subdirectory(scratch)

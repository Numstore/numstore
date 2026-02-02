file(GLOB NSFSLITE_EXAMPLES CONFIGURE_DEPENDS example*.c)

foreach(EXAMPLE_SRC ${NSFSLITE_EXAMPLES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SRC} NAME_WE)

    add_ns_executable(${EXAMPLE_NAME}
        SOURCES
            ${EXAMPLE_SRC}
        DEPENDENCIES
            ${LIBS}/nsusecases/nsfslite
        EXTERNAL_LIBS
            backtrace
            pthread
    )
endforeach()

file(GLOB COMPILER_EXAMPLES CONFIGURE_DEPENDS example*.c)

foreach(EXAMPLE_SRC ${COMPILER_EXAMPLES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SRC} NAME_WE)

    add_ns_executable(${EXAMPLE_NAME}
        SOURCES
            ${EXAMPLE_SRC}
        DEPENDENCIES
            ${LIBS}/nscompiler
        EXTERNAL_LIBS
            backtrace
            pthread
    )
endforeach()

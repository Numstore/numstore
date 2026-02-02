file(GLOB NSFILE_EXAMPLES CONFIGURE_DEPENDS example*.c)

foreach(EXAMPLE_SRC ${NSFILE_EXAMPLES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SRC} NAME_WE)

    add_ns_executable(${EXAMPLE_NAME}
        SOURCES
            ${EXAMPLE_SRC}
        DEPENDENCIES
            ${LIBS}/nsusecases/nsfile
        EXTERNAL_LIBS
            backtrace
            pthread
    )
endforeach()

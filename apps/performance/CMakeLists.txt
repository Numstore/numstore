file(GLOB PERFORMANCE_EXE CONFIGURE_DEPENDS *.c)

foreach(EXE_SRC ${PERFORMANCE_EXE})
  get_filename_component(EXE_NAME ${EXE_SRC} NAME_WE)

  add_ns_executable(${EXE_NAME}
        SOURCES
            ${EXE_SRC}
        DEPENDENCIES
            ${LIBS}/nsfile
        EXTERNAL_LIBS
            backtrace
            pthread
    )
endforeach()


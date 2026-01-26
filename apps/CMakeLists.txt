if(NOT ENABLE_NTEST)
      add_executable(test
        test.c
        $<TARGET_OBJECTS:nscore_objects>
        $<TARGET_OBJECTS:nstypes_objects>
        $<TARGET_OBJECTS:nspager_objects>
        $<TARGET_OBJECTS:nsrptcursor_objects>
        $<TARGET_OBJECTS:nsvcursor_objects>
        $<TARGET_OBJECTS:nsfslite_objects>
        $<TARGET_OBJECTS:nsusecase_objects>
        $<TARGET_OBJECTS:nscompiler_objects>
        $<TARGET_OBJECTS:nslite_objects>
    )

    target_include_directories(test PRIVATE
        ../libs/nscore/include
        ../libs/nstypes/include
        ../libs/nspager/include
        ../libs/cursors/nsrptcursor/include
        ../libs/cursors/nsvcursor/include
        ../libs/apps/nsfslite/include
        ../libs/nsusecase/include
        ../libs/nscompiler_objects/include
        ../libs/nslite/include
    )

    target_link_libraries(test PRIVATE m backtrace pthread)
endif()

add_ns_executable(nspprint
    SOURCES
        tools/nspprint.c
    DEPENDENCIES
        ${LIBS}/nsusecase
        ${LIBS}/nspager
    EXTERNAL_LIBS
        backtrace
        pthread
)

add_ns_executable(walfprint
    SOURCES
        tools/walfprint.c
    DEPENDENCIES
        ${LIBS}/nsusecase
        ${LIBS}/nspager
    EXTERNAL_LIBS
        backtrace
        pthread
)

add_subdirectory(examples/nsfslite)

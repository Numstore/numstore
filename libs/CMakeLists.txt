add_subdirectory(nscore)
add_subdirectory(nstypes)
add_subdirectory(cursors/nsrptcursor)
add_subdirectory(cursors/nsvcursor)
add_subdirectory(nspager)
add_subdirectory(nsroutines)
add_subdirectory(nsfslite)
add_subdirectory(nscompiler)
add_subdirectory(nsnet)
add_subdirectory(nsdb)
add_subdirectory(nsapps/nsserver)

add_library(numstore STATIC ${NS_ALL_OBJECTS})
target_include_directories(numstore PUBLIC ${NS_ALL_INCLUDES})
target_link_libraries(numstore PUBLIC backtrace pthread)

install(TARGETS numstore
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

foreach(INC_DIR ${NS_ALL_INCLUDES})
    install(DIRECTORY ${INC_DIR}/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h"
    )
endforeach()

# deps/ConfigureLinenoise.cmake
add_library(linenoise STATIC ${CMAKE_CURRENT_SOURCE_DIR}/linenoise/linenoise.c)
target_include_directories(linenoise PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/linenoise)
target_compile_options(linenoise PRIVATE -w)

set(LINENOISE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/linenoise PARENT_SCOPE)
set(LINENOISE_LIBRARIES linenoise PARENT_SCOPE)

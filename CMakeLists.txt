cmake_minimum_required(VERSION 3.16)
project(nstore C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/add_lemon_parser.cmake)
include(cmake/add_ns_executable.cmake)
include(cmake/add_ns_library.cmake)
include(cmake/add_python_module.cmake)
include(cmake/add_jni_library.cmake)

# Define libs base directory for use in subdirectories
set(LIBS ${CMAKE_SOURCE_DIR}/libs)

# Add libs directory to include path for config.h
include_directories(${CMAKE_SOURCE_DIR}/libs)

##################### Build Options

# Options to control NDEBUG and NTEST flags
option(ENABLE_NDEBUG "Enable NDEBUG flag (disable assertions)" OFF)
option(ENABLE_NTEST "Enable NTEST flag (disable tests)" OFF)
option(ENABLE_NLOG "Enable NLOG flag (disable logging)" OFF)
option(ENABLE_GPROF "Enable gprof profiling support" ON)

##################### Debug / Release

set(CMAKE_C_FLAGS_DEBUG "-O0        \
	-g                                \
	-rdynamic                         \
	-Wall                             \
	-Wextra                           \
	-Werror                           \
	-Wno-unused-but-set-variable      \
	-Wno-unused-variable              \
	-pedantic-errors                  \
	-Wshadow                          \
	-Wstrict-prototypes               \
	-Wmissing-prototypes              \
	-Wmissing-declarations            \
	-Wno-unused-parameter             \
	-Wsign-compare                    \
	-lbacktrace"
)

# Add Clang-specific flags
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wno-gnu-zero-variadic-macro-arguments")
endif()

set(CMAKE_C_FLAGS_RELEASE "-O4      \
	-march=native -mtune=native       \
	-flto                             \
	-Wall                             \
	-Wextra                           \
	-Werror                           \
	-Wno-unused-but-set-variable      \
	-Wno-unused-variable              \
	-Wno-maybe-uninitialized          \
	-pedantic-errors                  \
	-Wshadow                          \
	-Wstrict-prototypes               \
	-Wmissing-prototypes              \
	-Wmissing-declarations            \
	-Wno-unused-parameter             \
	-Wsign-compare")

# Apply conditional flags based on options
if(ENABLE_NDEBUG)
	add_compile_definitions(NDEBUG)
endif()

if(ENABLE_NTEST)
	add_compile_definitions(NTEST)
endif()

if(ENABLE_NLOG)
	add_compile_definitions(NLOG)
endif()

if(ENABLE_GPROF)
	add_compile_options(-pg)
	add_link_options(-pg)
endif()

add_subdirectory(thirdparty)
add_subdirectory(libs)
add_subdirectory(apps)

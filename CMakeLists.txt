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
option(ENABLE_GPROF "Enable gprof profiling support" OFF)

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

##################### SUBDIRECTORIES
add_subdirectory(libs/nscore)
add_subdirectory(libs/nstypes)
add_subdirectory(libs/cursors/nsrptcursor)
add_subdirectory(libs/cursors/nsvcursor)
add_subdirectory(libs/nspager)
add_subdirectory(libs/nsusecase)
#add_subdirectory(libs/apps/nsdblite)
add_subdirectory(libs/apps/nsfslite)
add_subdirectory(libs/apps/nslite)
add_subdirectory(libs/nscompiler)
add_subdirectory(apps)

##################### COMBINED STATIC LIBRARY

# Create one combined static library from all component libraries
add_library(numstore STATIC
    $<TARGET_OBJECTS:nscore_objects>
    $<TARGET_OBJECTS:nstypes_objects>
    $<TARGET_OBJECTS:nsrptcursor_objects>
    $<TARGET_OBJECTS:nsvcursor_objects>
    $<TARGET_OBJECTS:nspager_objects>
    $<TARGET_OBJECTS:nsusecase_objects>
    $<TARGET_OBJECTS:nsfslite_objects>
    $<TARGET_OBJECTS:nslite_objects>
    $<TARGET_OBJECTS:nscompiler_objects>
)

# Set up include directories for the combined library
target_include_directories(numstore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/nscore/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/nstypes/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/cursors/nsrptcursor/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/cursors/nsvcursor/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/nspager/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/nsusecase/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/apps/nsfslite/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/apps/nslite/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/nscompiler/include>
    $<INSTALL_INTERFACE:include>
)

# Link external libraries
target_link_libraries(numstore PUBLIC backtrace pthread)

##################### INSTALLATION

# Install the combined static library
install(TARGETS numstore
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

# Install all public headers from each component library
install(DIRECTORY
    libs/nscore/include/
    libs/nstypes/include/
    libs/cursors/nsrptcursor/include/
    libs/nspager/include/
    libs/nsusecase/include/
    libs/apps/nsfslite/include/
    libs/apps/nslite/include/
    libs/nscompiler/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

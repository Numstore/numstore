set(NS_ALL_OBJECTS "" CACHE STRING "" FORCE)
set(NS_ALL_INCLUDES "" CACHE STRING "" FORCE)
set(NS_ALL_DEPENDENCIES "" CACHE STRING "" FORCE)

macro(add_ns_library TARGET)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs 
      SOURCES 
      TEST_SOURCES 
      DEPENDENCIES 
      EXTERNAL_LIBS 
      EXTERNAL_INCLUDES)

    cmake_parse_arguments(ARG 
      "${options}" 
      "${oneValueArgs}" 
      "${multiValueArgs}"
       ${ARGN})

    # Combine sources
    set(ALL_SOURCES ${ARG_SOURCES})
    if(NOT ENABLE_NTEST AND ARG_TEST_SOURCES)
        list(APPEND ALL_SOURCES ${ARG_TEST_SOURCES})
    endif()

    # Create object library
    add_library(${TARGET}_objects OBJECT ${ALL_SOURCES})
    target_include_directories(${TARGET}_objects PUBLIC include)

    # Build private includes from dependencies and external includes
    set(PRIVATE_INCLUDES .)
    foreach(DEP ${ARG_DEPENDENCIES})
        list(APPEND PRIVATE_INCLUDES ${DEP}/include)
    endforeach()
    if(ARG_EXTERNAL_INCLUDES)
        list(APPEND PRIVATE_INCLUDES ${ARG_EXTERNAL_INCLUDES})
    endif()

    target_include_directories(
      ${TARGET}_objects 
      PRIVATE 
      ${PRIVATE_INCLUDES})

    # Link object library to dependency objects
    foreach(DEP ${ARG_DEPENDENCIES})
        string(REGEX REPLACE ".*/([^/]+)$" "\\1" DEP_TARGET "${DEP}")
        target_link_libraries(
          ${TARGET}_objects 
          PUBLIC 
          ${DEP_TARGET}_objects)
    endforeach()

    # Link external libraries
    if(ARG_EXTERNAL_LIBS)
        target_link_libraries(
          ${TARGET}_objects 
          PUBLIC 
          ${ARG_EXTERNAL_LIBS})
    endif()

    # Create static library from objects
    add_library(${TARGET} STATIC $<TARGET_OBJECTS:${TARGET}_objects>)
    target_include_directories(${TARGET} PUBLIC include)

    # Link static library to dependencies
    foreach(DEP ${ARG_DEPENDENCIES})
        string(REGEX REPLACE ".*/([^/]+)$" "\\1" DEP_TARGET "${DEP}")
        target_link_libraries(${TARGET} PUBLIC ${DEP_TARGET})
    endforeach()

    # Link external libraries
    if(ARG_EXTERNAL_LIBS)
        target_link_libraries(${TARGET} PUBLIC ${ARG_EXTERNAL_LIBS})
    endif()

  # in add_ns_library, replace the three set() lines with:
  set(NS_ALL_OBJECTS "${NS_ALL_OBJECTS};$<TARGET_OBJECTS:${TARGET}_objects>" CACHE STRING "" FORCE)
  set(NS_ALL_INCLUDES "${NS_ALL_INCLUDES};${CMAKE_CURRENT_SOURCE_DIR}/include" CACHE STRING "" FORCE)
  set(NS_ALL_DEPENDENCIES "${NS_ALL_DEPENDENCIES};${ARG_DEPENDENCIES}" CACHE STRING "" FORCE)
endmacro()

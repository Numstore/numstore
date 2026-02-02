file(GLOB NSFSLITE_SRC CONFIGURE_DEPENDS *.c)

add_ns_library(nsfslite
  SOURCES
    ${NSFSLITE_SRC}
  DEPENDENCIES
    ${LIBS}/nscore
    ${LIBS}/nspager
    ${LIBS}/nscompiler
    ${LIBS}/cursors/nsrptcursor
    ${LIBS}/cursors/nsvcursor
)

#add_python_module(
#    TARGET _nsfslite
#    MODULE_NAME "_nsfslite"
#    SOURCES
#        ${CMAKE_CURRENT_SOURCE_DIR}/pybind/nsfslite_py.c
#    LIBRARY_DEPENDENCY nsfslite
#    OUTPUT_DIR "${CMAKE_SOURCE_DIR}/python/numstore/nsfslite"
#)
#
#add_jni_library(
#    TARGET nsfslite_jni
#    MODULE_NAME "nsfslite_jni"
#    SOURCES
#        ${CMAKE_CURRENT_SOURCE_DIR}/jnibind/nsfslite_jni.c
#    LIBRARY_DEPENDENCY nsfslite
#    OUTPUT_DIR "${CMAKE_SOURCE_DIR}/java/src/main/resources/native"
#)

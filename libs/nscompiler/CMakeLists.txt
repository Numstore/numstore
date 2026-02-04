# Collect all compiler source files
file(GLOB COMPILER_SRC CONFIGURE_DEPENDS *.c)
file(GLOB COMPILER_PARSERS CONFIGURE_DEPENDS parsers/*.c)

list(APPEND COMPILER_SRC ${COMPILER_PARSERS})

add_ns_library(nscompiler
  SOURCES
    ${COMPILER_SRC}
  DEPENDENCIES
    ${LIBS}/nstypes
)

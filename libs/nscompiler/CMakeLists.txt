# Collect all compiler source files
file(GLOB COMPILER_SRC CONFIGURE_DEPENDS *.c)

add_ns_library(nscompiler
  SOURCES
    ${COMPILER_SRC}
  DEPENDENCIES
    ${LIBS}/nstypes
)

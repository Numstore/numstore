# Collect all compiler source files
file(GLOB NSFILECLI_SRC CONFIGURE_DEPENDS *.c)

add_ns_library(nsfilecli
  SOURCES
    ${NSFILECLI_SRC}
  DEPENDENCIES
    ${LIBS}/nstypes
    ${LIBS}/nsusecases/nsfile
    ${LIBS}/nscompiler
)

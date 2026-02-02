# Collect all compiler source files
file(GLOB SERVER_APP_SRC CONFIGURE_DEPENDS *.c)

add_ns_library(nsserver
  SOURCES
    ${SERVER_APP_SRC}
  DEPENDENCIES
    ${LIBS}/nstypes
    ${LIBS}/nsnet
    ${LIBS}/nscore
)

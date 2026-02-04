file(GLOB NSDB_SRC CONFIGURE_DEPENDS *.c)

add_ns_library(nsdb
  SOURCES
    ${NSDB_SRC}
  DEPENDENCIES
    ${LIBS}/nscore
    ${LIBS}/nspager
    ${LIBS}/nscompiler
    ${LIBS}/cursors/nsrptcursor
    ${LIBS}/cursors/nsvcursor
)


file(GLOB NSDBLITE_SRC CONFIGURE_DEPENDS *.c)

add_ns_library(nsdblite
  SOURCES
    ${NSDBLITE_SRC}
  DEPENDENCIES
    ${LIBS}/nscore
    ${LIBS}/nspager
)

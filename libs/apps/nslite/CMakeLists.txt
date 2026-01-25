file(GLOB NSLITE_SRC CONFIGURE_DEPENDS *.c)

add_ns_library(nslite
  SOURCES
    ${NSLITE_SRC}
  DEPENDENCIES
    ${LIBS}/nscore
    ${LIBS}/nspager
    ${LIBS}/cursors/nsrptcursor
)


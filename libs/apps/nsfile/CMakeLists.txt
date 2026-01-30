file(GLOB NSFILE_SRC CONFIGURE_DEPENDS *.c)

add_ns_library(nsfile
  SOURCES
    ${NSFILE_SRC}
  DEPENDENCIES
    ${LIBS}/nscore
    ${LIBS}/nspager
    ${LIBS}/cursors/nsrptcursor
    ${LIBS}/apps/nslite
)


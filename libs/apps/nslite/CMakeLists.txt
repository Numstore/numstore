file(GLOB NSLITE_SRC CONFIGURE_DEPENDS *.c)
file(GLOB NSLITE_TEST_SRC CONFIGURE_DEPENDS "testing/*.c")

add_ns_library(nslite
  SOURCES
    ${NSLITE_SRC}
  TEST_SOURCES
  ${NSLITE_TEST_SRC}
  DEPENDENCIES
    ${LIBS}/nscore
    ${LIBS}/nspager
    ${LIBS}/cursors/nsrptcursor
)


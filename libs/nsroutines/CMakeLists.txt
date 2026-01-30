file(GLOB NSUSECASE_SRC CONFIGURE_DEPENDS *.c)
file(GLOB NSUSECASE_TEST_SRC CONFIGURE_DEPENDS "testing/*.c")

add_ns_library(nsroutines
  SOURCES
    ${NSUSECASE_SRC}
  TEST_SOURCES
    ${NSUSECASE_TEST_SRC}
  DEPENDENCIES
    ${LIBS}/nspager
    ${LIBS}/nsusecases/nsfslite
)

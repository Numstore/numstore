file(GLOB NSNET_SRC CONFIGURE_DEPENDS *.c)

add_ns_library(nsnet
  SOURCES
    ${NSNET_SRC}
  DEPENDENCIES
    ${LIBS}/nscore
)

file(GLOB NSCORE_SRC CONFIGURE_DEPENDS *.c)
file(GLOB NSCORE_TESTING_SRC CONFIGURE_DEPENDS *.c)

if(WIN32)
  message("TODO")
elseif(APPLE)
  message("TODO")
elseif(EMSCRIPTEN)
  message("TODO")
else()
  file(GLOB OS_SRC CONFIGURE_DEPENDS ./intf/os_posix/*.c)
  list(APPEND NSCORE_SRC ${OS_SRC})
endif()

add_ns_library(nscore
  SOURCES
    ${NSCORE_SRC}
  TEST_SOURCES
    ${NSCORE_TEST_SRC}
  EXTERNAL_LIBS
      backtrace
      pthread
)

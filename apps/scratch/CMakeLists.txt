file(GLOB NSSCRATCH CONFIGURE_DEPENDS *.c)

foreach(SCRATCH ${NSSCRATCH}) 
  get_filename_component(SCRATCH_TARGET ${SCRATCH} NAME_WE)
  add_ns_executable(${SCRATCH_TARGET}
      SOURCES
        ${SCRATCH}
      DEPENDENCIES
        ${NS_ALL_DEPENDENCIES}
      EXTERNAL_LIBS
        backtrace
        pthread
  )
endforeach()

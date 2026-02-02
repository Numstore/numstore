ExternalProject_Add(zstd
    SOURCE_DIR        ${CMAKE_SOURCE_DIR}/thirdparty/zstd
    BINARY_DIR        ${CMAKE_BINARY_DIR}/thirdparty/zstd
    SOURCE_SUBDIR     build/cmake
    CMAKE_ARGS
        -DZSTD_BUILD_TESTS=OFF
        -DZSTD_BUILD_PROGRAMS=OFF
        -DZSTD_BUILD_SHARED=OFF
    INSTALL_COMMAND   ""
)

set(ZSTD_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/zstd/lib)
set(ZSTD_LIBRARIES   ${CMAKE_BINARY_DIR}/thirdparty/zstd/lib/libzstd.a)

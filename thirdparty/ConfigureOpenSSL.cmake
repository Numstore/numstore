ExternalProject_Add(openssl
    SOURCE_DIR        ${CMAKE_SOURCE_DIR}/thirdparty/openssl
    BINARY_DIR        ${CMAKE_BINARY_DIR}/thirdparty/openssl
    CONFIGURE_COMMAND ${CMAKE_SOURCE_DIR}/thirdparty/openssl/config
                      no-shared
                      --prefix=${CMAKE_BINARY_DIR}/thirdparty/openssl/install
    BUILD_COMMAND     make -j${CMAKE_JOB_POOL_SIZE}
    INSTALL_COMMAND   make install
)

set(OPENSSL_INCLUDE_DIR ${CMAKE_BINARY_DIR}/thirdparty/openssl/install/include)
set(OPENSSL_LIBRARIES
    ${CMAKE_BINARY_DIR}/thirdparty/openssl/install/lib/libssl.a
    ${CMAKE_BINARY_DIR}/thirdparty/openssl/install/lib/libcrypto.a
)

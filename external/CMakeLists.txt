cmake_minimum_required(VERSION 3.12.0)

project(sockpuppet-dependencies CXX)

include(ExternalProject)
ExternalProject_Add(libressl
  URL https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-4.1.0.tar.gz
  URL_MD5 18079dcf72a398d8c188e67e30c1dc13
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/libressl
  SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/libressl/src
  STAMP_DIR ${CMAKE_CURRENT_BINARY_DIR}/libressl/stamp
  BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/libressl/build
  CMAKE_ARGS
    -DLIBRESSL_TESTS:BOOL=OFF
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/libressl/install
  INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/libressl/install
)

message("OPENSSL_ROOT_DIR should be pointed to: ${CMAKE_CURRENT_BINARY_DIR}/libressl/install")

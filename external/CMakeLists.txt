cmake_minimum_required(VERSION 3.1.0)

project(sockpuppet-dependencies CXX)

include(ExternalProject)
ExternalProject_Add(libressl
  URL https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-3.8.0.tar.gz
  URL_MD5 ab3b1dfd6480d930a3f345178ba5d6a1
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

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  add_custom_target(libressl-rename ALL
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/libressl/install/lib/${CMAKE_SHARED_LIBRARY_PREFIX}crypto-51${CMAKE_STATIC_LIBRARY_SUFFIX}" "${CMAKE_CURRENT_BINARY_DIR}/libressl/install/lib/${CMAKE_SHARED_LIBRARY_PREFIX}crypto${CMAKE_STATIC_LIBRARY_SUFFIX}"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/libressl/install/lib/${CMAKE_SHARED_LIBRARY_PREFIX}ssl-54${CMAKE_STATIC_LIBRARY_SUFFIX}" "${CMAKE_CURRENT_BINARY_DIR}/libressl/install/lib/${CMAKE_SHARED_LIBRARY_PREFIX}ssl${CMAKE_STATIC_LIBRARY_SUFFIX}"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/libressl/install/lib/${CMAKE_SHARED_LIBRARY_PREFIX}tls-27${CMAKE_STATIC_LIBRARY_SUFFIX}" "${CMAKE_CURRENT_BINARY_DIR}/libressl/install/lib/${CMAKE_SHARED_LIBRARY_PREFIX}tls${CMAKE_STATIC_LIBRARY_SUFFIX}"
    DEPENDS libressl
    COMMENT "renaming, to make libressl build compatible with FindOpenSSL.cmake's OPENSSL_ROOT_DIR"
  )
endif()

message("OPENSSL_ROOT_DIR should be pointed to: ${CMAKE_CURRENT_BINARY_DIR}/libressl/install")

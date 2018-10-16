cmake_minimum_required(VERSION 3.0.0)

project(sockpuppet CXX)

if(${CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT})
  set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "" FORCE)
endif()
option(BUILD_TESTS "Build test executables" TRUE)
option(BUILD_EXAMPLES "Build example executables" TRUE)

add_library(sockpuppet
  src/socket.cpp
  include/socket.h
  src/socket_address.cpp
  include/socket_address.h
  src/socket_address_priv.cpp
  src/socket_address_priv.h
  src/socket_guard.cpp
  src/socket_guard.h
  src/socket_priv.cpp
  src/socket_priv.h
)

include_directories(include)

if(${BUILD_TESTS})
  add_subdirectory(test)
endif()
if(${BUILD_EXAMPLES})
  add_subdirectory(examples)
endif()

install(DIRECTORY include/
  DESTINATION include
  FILES_MATCHING PATTERN "*.h"
)
install(TARGETS sockpuppet
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
)

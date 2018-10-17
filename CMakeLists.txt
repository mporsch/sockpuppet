cmake_minimum_required(VERSION 3.0.0)

project(sockpuppet CXX)

if(${CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT})
  # install to the build directory by default (instead of some system folder)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "" FORCE)
endif()
option(BUILD_TESTS "Build test executables" TRUE)
option(BUILD_EXAMPLES "Build example executables" TRUE)
option(BUILD_SHARED_LIBS "Build shared instead of static library" FALSE)

if(${BUILD_SHARED_LIBS})
  # auto-generate DLL exports
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()

# this is influenced by the BUILD_SHARED_LIBS setting
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

# install the library includes
install(DIRECTORY include/
  DESTINATION include
  FILES_MATCHING PATTERN "*.h"
)
# install the library itself
install(TARGETS sockpuppet
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
# install the demo project
install(DIRECTORY demo/ DESTINATION demo)
install(FILES examples/sockpuppet_http_server.cpp DESTINATION demo)

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

if(MINGW)
  # allow MinGW to drop compatibility with legacy windows
  add_definitions(-D_WIN32_WINNT=0x0600)
endif(MINGW)

# this is influenced by the BUILD_SHARED_LIBS setting
add_library(sockpuppet
  include/resource_pool.h
  src/socket.cpp
  include/socket.h
  src/socket_address.cpp
  include/socket_address.h
  src/socket_address_priv.cpp
  src/socket_address_priv.h
  src/socket_async.cpp
  include/socket_async.h
  src/socket_async_priv.cpp
  src/socket_async_priv.h
  src/socket_buffered.cpp
  include/socket_buffered.h
  src/socket_buffered_priv.cpp
  src/socket_buffered_priv.h
  src/socket_guard.cpp
  src/socket_guard.h
  src/socket_priv.cpp
  src/socket_priv.h
)

include_directories(include)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  target_link_libraries(sockpuppet WS2_32.Lib IPHLPAPI.lib)
endif()

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

cmake_minimum_required(VERSION 3.0.0)

project(sockpuppet CXX)

if(${CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT})
  # install to the build directory by default
  set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "" FORCE)
endif()
option(BUILD_TESTS "Build test executables" TRUE)
option(BUILD_EXAMPLES "Build example executables" TRUE)
option(BUILD_SHARED_LIBS "Build shared instead of static library" FALSE)

if(${BUILD_SHARED_LIBS})
  # auto-generate DLL exports for Windows
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()

if(MINGW)
  # allow MinGW to drop compatibility with legacy Windows
  add_definitions(-D_WIN32_WINNT=0x0600)
endif(MINGW)

set(SOCKPUPPET_PUBLIC_HEADERS
  include/resource_pool.h
  include/socket.h
  include/socket_address.h
  include/socket_async.h
  include/socket_buffered.h
)

# this is influenced by the BUILD_SHARED_LIBS setting
add_library(sockpuppet
  src/socket.cpp
  src/socket_address.cpp
  src/socket_address_priv.cpp
  src/socket_address_priv_unix.cpp
  src/socket_address_priv_win.cpp
  src/socket_address_priv.h
  src/socket_async.cpp
  src/socket_async_priv.cpp
  src/socket_async_priv.h
  src/socket_buffered.cpp
  src/socket_buffered_priv.cpp
  src/socket_buffered_priv.h
  src/socket_guard.cpp
  src/socket_guard.h
  src/socket_priv.cpp
  src/socket_priv.h
  src/util_unix.cpp
  src/util_win.cpp
  src/util.h
  ${SOCKPUPPET_PUBLIC_HEADERS}
)
set_target_properties(sockpuppet PROPERTIES PUBLIC_HEADER "${SOCKPUPPET_PUBLIC_HEADERS}")

include_directories(include)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  target_link_libraries(sockpuppet WS2_32.Lib)
endif()

if(${BUILD_TESTS})
  add_subdirectory(test)
endif()
if(${BUILD_EXAMPLES})
  add_subdirectory(examples)
endif()

# install the library and includes
install(TARGETS sockpuppet
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  PUBLIC_HEADER DESTINATION include
)
# install the demo project
set(DEMO_CMAKE_FILE_CONFIGURE_CODE
  "configure_file(
    \"${CMAKE_SOURCE_DIR}/examples/CMakeLists.txt.in\"
    \"${CMAKE_INSTALL_PREFIX}/demo/CMakeLists.txt\"
    @ONLY
  )"
)
install(CODE ${DEMO_CMAKE_FILE_CONFIGURE_CODE})
install(FILES examples/sockpuppet_http_server.cpp DESTINATION demo)

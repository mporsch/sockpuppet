cmake_minimum_required(VERSION 3.1.0)

project(sockpuppet CXX)
set(CMAKE_CXX_STANDARD 17)

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
endif()

set(SOCKPUPPET_PUBLIC_HEADERS
  include/sockpuppet/address.h
  include/sockpuppet/socket.h
  include/sockpuppet/socket_async.h
  include/sockpuppet/socket_buffered.h
)

# this is influenced by the BUILD_SHARED_LIBS setting
add_library(sockpuppet
  src/address.cpp
  src/address_priv.cpp
  src/address_priv_unix.cpp
  src/address_priv_win.cpp
  src/address_priv.h
  src/driver_priv.cpp
  src/driver_priv.h
  src/error_code.cpp
  src/error_code_unix.cpp
  src/error_code_win.cpp
  src/error_code.h
  src/socket.cpp
  src/socket_async.cpp
  src/socket_async_priv.cpp
  src/socket_async_priv.h
  src/socket_buffered.cpp
  src/socket_buffered_priv.cpp
  src/socket_buffered_priv.h
  src/socket_priv.cpp
  src/socket_priv.h
  src/todo_priv.cpp
  src/todo_priv.h
  src/winsock_guard.cpp
  src/winsock_guard.h
  ${SOCKPUPPET_PUBLIC_HEADERS}
)
set_target_properties(sockpuppet PROPERTIES PUBLIC_HEADER "${SOCKPUPPET_PUBLIC_HEADERS}")

# set/forward include directory
target_include_directories(sockpuppet PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  # link with WinSock
  if(${BUILD_SHARED_LIBS})
    target_link_libraries(sockpuppet PRIVATE WS2_32.Lib)
  else()
    target_link_libraries(sockpuppet PUBLIC WS2_32.Lib)
  endif()
endif()

if(${BUILD_TESTS})
  add_subdirectory(test)
endif()
if(${BUILD_EXAMPLES})
  add_subdirectory(examples)
endif()

# install the library, includes and CMake package
install(TARGETS sockpuppet
  EXPORT sockpuppet-config
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  PUBLIC_HEADER DESTINATION include/sockpuppet
)
install(EXPORT sockpuppet-config DESTINATION cmake)
export(TARGETS sockpuppet FILE sockpuppet-config.cmake)

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

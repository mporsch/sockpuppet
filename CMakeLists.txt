cmake_minimum_required(VERSION 3.12.0)

project(sockpuppet CXX)

if(${CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT})
  # install to the build directory by default
  set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "" FORCE)
endif()

option(SOCKPUPPET_BUILD_TESTS "Build test executables" TRUE)
option(SOCKPUPPET_BUILD_EXAMPLES "Build example executables" TRUE)
option(SOCKPUPPET_BUILD_SHARED_LIBS "Build shared instead of static library" FALSE)
option(SOCKPUPPET_WITH_TLS "Build with support for encrypted connections using OpenSSL (or compatible)" FALSE)

set(BUILD_SHARED_LIBS ${SOCKPUPPET_BUILD_SHARED_LIBS})
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
  src/address_impl.cpp
  src/address_impl_unix.cpp
  src/address_impl_win.cpp
  src/address_impl.h
  src/driver_impl.cpp
  src/driver_impl.h
  src/error_code.cpp
  src/error_code_tls.cpp
  src/error_code_unix.cpp
  src/error_code_win.cpp
  src/error_code.h
  src/socket.cpp
  src/socket_async.cpp
  src/socket_async_impl.cpp
  src/socket_async_impl.h
  src/socket_buffered.cpp
  src/socket_buffered_impl.cpp
  src/socket_buffered_impl.h
  src/socket_impl.cpp
  src/socket_impl.h
  src/socket_tls_impl.cpp
  src/socket_tls_impl.h
  src/ssl_guard.cpp
  src/ssl_guard.h
  src/todo_impl.cpp
  src/todo_impl.h
  src/wait.cpp
  src/wait.h
  src/winsock_guard.cpp
  src/winsock_guard.h
  ${SOCKPUPPET_PUBLIC_HEADERS}
)
set_target_properties(sockpuppet PROPERTIES PUBLIC_HEADER "${SOCKPUPPET_PUBLIC_HEADERS}")

# internals and public includes depend on C++17
target_compile_features(sockpuppet PUBLIC cxx_std_17)

# set/forward include directory
target_include_directories(sockpuppet PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

if(${SOCKPUPPET_WITH_TLS})
  # LibreSSL works fine as well, just had to remove version suffix from installed lib filenames
  message("OPENSSL_ROOT_DIR points to: ${OPENSSL_ROOT_DIR}")
  find_package(OpenSSL REQUIRED)

  # add define for preprocessor switches in source files, public includes and export file
  target_compile_definitions(sockpuppet PUBLIC SOCKPUPPET_WITH_TLS)

  list(APPEND DEPENDENCIES
    OpenSSL::SSL
    OpenSSL::Crypto
    $<$<PLATFORM_ID:Windows>:bcrypt>
    $<$<PLATFORM_ID:Linux>:pthread>
  )
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  # avoid some unwanted windows header symbols
  target_compile_definitions(sockpuppet PRIVATE NOMINMAX NOCRYPT)

  # link with WinSock
  list(APPEND DEPENDENCIES ws2_32)
endif()

if(${BUILD_SHARED_LIBS})
  target_link_libraries(sockpuppet PRIVATE ${DEPENDENCIES})
else()
  target_link_libraries(sockpuppet PUBLIC ${DEPENDENCIES})
endif()

if(${SOCKPUPPET_BUILD_TESTS})
  add_subdirectory(test)
endif()
if(${SOCKPUPPET_BUILD_EXAMPLES})
  add_subdirectory(examples)
endif()

# install the library, includes and CMake package
export(TARGETS sockpuppet FILE sockpuppet-config.cmake)
install(EXPORT sockpuppet-config DESTINATION lib/cmake/sockpuppet)
install(TARGETS sockpuppet
  EXPORT sockpuppet-config
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  PUBLIC_HEADER DESTINATION include/sockpuppet
)

# install the demo project
if(${BUILD_SHARED_LIBS})
  configure_file(examples/CMakeLists.txt.shared.in examples/CMakeLists.txt)
elseif(${SOCKPUPPET_WITH_TLS})
  configure_file(examples/CMakeLists.txt.static.tls.in examples/CMakeLists.txt)
else()
  configure_file(examples/CMakeLists.txt.static.in examples/CMakeLists.txt)
endif()
install(FILES
    examples/sockpuppet_http_server.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/examples/CMakeLists.txt"
  DESTINATION demo
)

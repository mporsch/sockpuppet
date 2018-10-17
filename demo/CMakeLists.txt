cmake_minimum_required(VERSION 3.0.0)

project(sockpuppet_demo CXX)

if(${CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT})
  # install to the build directory by default (instead of some system folder)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "" FORCE)
endif()
set(SOCKPUPPET_DIR "" CACHE PATH "Installation directory of sockpuppet")

include_directories(${SOCKPUPPET_DIR}/include)
link_directories(${SOCKPUPPET_DIR}/lib)

add_executable(sockpuppet_http_server sockpuppet_http_server.cpp)
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_link_libraries(sockpuppet_http_server sockpuppet pthread)
else()
  target_link_libraries(sockpuppet_http_server sockpuppet)
endif()

install(TARGETS sockpuppet_http_server
  RUNTIME DESTINATION bin
)

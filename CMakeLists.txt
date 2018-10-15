cmake_minimum_required (VERSION 3.0.0)

project (sockpuppet CXX)

add_library (sockpuppet
  socket.cpp
  socket.h
  socket_address.cpp
  socket_address.h
  socket_address_priv.cpp
  socket_address_priv.h
  socket_guard.cpp
  socket_guard.h
)

add_executable (sockpuppet_address_test sockpuppet_address_test.cpp)
target_link_libraries (sockpuppet_address_test sockpuppet)

add_executable (sockpuppet_udp_test sockpuppet_udp_test.cpp)
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_link_libraries (sockpuppet_udp_test sockpuppet pthread)
else()
  target_link_libraries (sockpuppet_udp_test sockpuppet)
endif()

add_executable (sockpuppet_tcp_test sockpuppet_tcp_test.cpp)
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_link_libraries (sockpuppet_tcp_test sockpuppet pthread)
else()
  target_link_libraries (sockpuppet_tcp_test sockpuppet)
endif()

add_executable (sockpuppet_udp_client sockpuppet_udp_client.cpp)
target_link_libraries (sockpuppet_udp_client sockpuppet)

add_executable (sockpuppet_udp_server sockpuppet_udp_server.cpp)
target_link_libraries (sockpuppet_udp_server sockpuppet)

add_executable (sockpuppet_tcp_client sockpuppet_tcp_client.cpp)
target_link_libraries (sockpuppet_tcp_client sockpuppet)

add_executable (sockpuppet_tcp_server sockpuppet_tcp_server.cpp)
target_link_libraries (sockpuppet_tcp_server sockpuppet)

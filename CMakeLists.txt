cmake_minimum_required (VERSION 3.0.0)

project (sockpuppet CXX)

add_executable (sockpuppet_address_test sockpuppet_address_test.cpp socket_address.cpp socket_address.h socket_address_priv.cpp socket_address_priv.h socket_guard.cpp socket_guard.h)

add_executable (sockpuppet_test_client sockpuppet_test_client.cpp socket.cpp socket.h socket_address.cpp socket_address.h socket_address_priv.cpp socket_address_priv.h socket_guard.cpp socket_guard.h)
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_link_libraries (sockpuppet_test_client pthread)
endif()

add_executable (sockpuppet_test_server sockpuppet_test_server.cpp socket.cpp socket.h socket_address.cpp socket_address.h socket_address_priv.cpp socket_address_priv.h socket_guard.cpp socket_guard.h)
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_link_libraries (sockpuppet_test_client pthread)
endif()

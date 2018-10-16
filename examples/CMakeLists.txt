add_executable(sockpuppet_udp_client sockpuppet_udp_client.cpp)
target_link_libraries(sockpuppet_udp_client sockpuppet)

add_executable(sockpuppet_udp_server sockpuppet_udp_server.cpp)
target_link_libraries(sockpuppet_udp_server sockpuppet)

add_executable(sockpuppet_tcp_client sockpuppet_tcp_client.cpp)
target_link_libraries(sockpuppet_tcp_client sockpuppet)

add_executable(sockpuppet_tcp_server sockpuppet_tcp_server.cpp)
target_link_libraries(sockpuppet_tcp_server sockpuppet)

add_executable(sockpuppet_http_server sockpuppet_http_server.cpp)
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_link_libraries(sockpuppet_http_server sockpuppet pthread)
else()
  target_link_libraries(sockpuppet_http_server sockpuppet)
endif()

install(TARGETS sockpuppet_udp_client DESTINATION bin)
install(TARGETS sockpuppet_udp_server DESTINATION bin)
install(TARGETS sockpuppet_tcp_client DESTINATION bin)
install(TARGETS sockpuppet_tcp_server DESTINATION bin)
install(TARGETS sockpuppet_http_server DESTINATION bin)

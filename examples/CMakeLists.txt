add_executable(sockpuppet_udp_client sockpuppet_udp_client.cpp)
target_link_libraries(sockpuppet_udp_client sockpuppet)

add_executable(sockpuppet_udp_server sockpuppet_udp_server.cpp)
target_link_libraries(sockpuppet_udp_server sockpuppet)

add_executable(sockpuppet_tcp_client sockpuppet_tcp_client.cpp)
target_link_libraries(sockpuppet_tcp_client sockpuppet)

add_executable(sockpuppet_tcp_server sockpuppet_tcp_server.cpp)
target_link_libraries(sockpuppet_tcp_server sockpuppet)

add_executable(sockpuppet_http_server sockpuppet_http_server.cpp)
target_link_libraries(sockpuppet_http_server sockpuppet)

add_executable(sockpuppet_http_client sockpuppet_http_client.cpp)
target_link_libraries(sockpuppet_http_client sockpuppet)

add_executable(sockpuppet_silent_song sockpuppet_silent_song.cpp)
target_link_libraries(sockpuppet_silent_song sockpuppet)

install(TARGETS sockpuppet_udp_client DESTINATION bin)
install(TARGETS sockpuppet_udp_server DESTINATION bin)
install(TARGETS sockpuppet_tcp_client DESTINATION bin)
install(TARGETS sockpuppet_tcp_server DESTINATION bin)
install(TARGETS sockpuppet_http_server DESTINATION bin)
install(TARGETS sockpuppet_http_client DESTINATION bin)
install(TARGETS sockpuppet_silent_song DESTINATION bin)

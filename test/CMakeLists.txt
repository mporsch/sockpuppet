set(HELPER_INCLUDE_DIR "" CACHE PATH "Path to \"helper\" repo (optional)")

if(EXISTS ${HELPER_INCLUDE_DIR})
  add_definitions(-DHAVE_HELPER)
  include_directories(${HELPER_INCLUDE_DIR})
endif()

add_executable(sockpuppet_address_test sockpuppet_address_test.cpp)
target_link_libraries(sockpuppet_address_test sockpuppet)

add_executable(sockpuppet_udp_test sockpuppet_udp_test.cpp)
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_link_libraries(sockpuppet_udp_test sockpuppet pthread)
else()
  target_link_libraries(sockpuppet_udp_test sockpuppet)
endif()

add_executable(sockpuppet_tcp_test sockpuppet_tcp_test.cpp)
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_link_libraries(sockpuppet_tcp_test sockpuppet pthread)
else()
  target_link_libraries(sockpuppet_tcp_test sockpuppet)
endif()

install(TARGETS sockpuppet_address_test DESTINATION bin)
install(TARGETS sockpuppet_udp_test DESTINATION bin)
install(TARGETS sockpuppet_tcp_test DESTINATION bin)
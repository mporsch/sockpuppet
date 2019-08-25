set(HELPER_INCLUDE_DIR "" CACHE PATH "Path to \"helper\" repo (optional)")

if(EXISTS ${HELPER_INCLUDE_DIR})
  add_definitions(-DHAVE_HELPER)
  include_directories(${HELPER_INCLUDE_DIR})
endif()

add_executable(sockpuppet_address_test sockpuppet_address_test.cpp)
target_link_libraries(sockpuppet_address_test sockpuppet)

add_executable(sockpuppet_util_test sockpuppet_util_test.cpp)
target_link_libraries(sockpuppet_util_test sockpuppet)

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

add_executable(sockpuppet_udp_buffered_test sockpuppet_udp_buffered_test.cpp)
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_link_libraries(sockpuppet_udp_buffered_test sockpuppet pthread)
else()
  target_link_libraries(sockpuppet_udp_buffered_test sockpuppet)
endif()

add_executable(sockpuppet_tcp_buffered_test sockpuppet_tcp_buffered_test.cpp)
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_link_libraries(sockpuppet_tcp_buffered_test sockpuppet pthread)
else()
  target_link_libraries(sockpuppet_tcp_buffered_test sockpuppet)
endif()

add_executable(sockpuppet_udp_async_test sockpuppet_udp_async_test.cpp)
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_link_libraries(sockpuppet_udp_async_test sockpuppet pthread)
else()
  target_link_libraries(sockpuppet_udp_async_test sockpuppet)
endif()

add_executable(sockpuppet_tcp_async_test sockpuppet_tcp_async_test.cpp)
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_link_libraries(sockpuppet_tcp_async_test sockpuppet pthread)
else()
  target_link_libraries(sockpuppet_tcp_async_test sockpuppet)
endif()

enable_testing()
add_test(NAME sockpuppet_address_test COMMAND sockpuppet_address_test)
add_test(NAME sockpuppet_udp_test COMMAND sockpuppet_udp_test)
add_test(NAME sockpuppet_tcp_test COMMAND sockpuppet_tcp_test)
add_test(NAME sockpuppet_udp_buffered_test COMMAND sockpuppet_udp_buffered_test)
add_test(NAME sockpuppet_tcp_buffered_test COMMAND sockpuppet_tcp_buffered_test)
add_test(NAME sockpuppet_udp_async_test COMMAND sockpuppet_udp_async_test)
add_test(NAME sockpuppet_tcp_async_test COMMAND sockpuppet_tcp_async_test)
add_custom_target(run_tests
  COMMAND ${CMAKE_CTEST_COMMAND} -C $<CONFIGURATION> --verbose
  DEPENDS sockpuppet_address_test
          sockpuppet_udp_test
          sockpuppet_tcp_test
          sockpuppet_udp_buffered_test
          sockpuppet_tcp_buffered_test
          sockpuppet_udp_async_test
          sockpuppet_tcp_async_test
)

install(TARGETS sockpuppet_address_test DESTINATION bin)
install(TARGETS sockpuppet_udp_test DESTINATION bin)
install(TARGETS sockpuppet_tcp_test DESTINATION bin)
install(TARGETS sockpuppet_udp_buffered_test DESTINATION bin)
install(TARGETS sockpuppet_tcp_buffered_test DESTINATION bin)
install(TARGETS sockpuppet_udp_async_test DESTINATION bin)
install(TARGETS sockpuppet_tcp_async_test DESTINATION bin)

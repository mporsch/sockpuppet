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

add_executable(sockpuppet_udp_buffered_test sockpuppet_udp_buffered_test.cpp sockpuppet_test_common.h)
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_link_libraries(sockpuppet_udp_buffered_test sockpuppet pthread)
else()
  target_link_libraries(sockpuppet_udp_buffered_test sockpuppet)
endif()

add_executable(sockpuppet_tcp_buffered_test sockpuppet_tcp_buffered_test.cpp sockpuppet_test_common.h)
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

add_executable(sockpuppet_tcp_async_performance_test sockpuppet_tcp_async_performance_test.cpp sockpuppet_test_common.h)
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_link_libraries(sockpuppet_tcp_async_performance_test sockpuppet pthread)
else()
  target_link_libraries(sockpuppet_tcp_async_performance_test sockpuppet)
endif()

add_executable(sockpuppet_internals_test sockpuppet_internals_test.cpp)
target_link_libraries(sockpuppet_internals_test sockpuppet)

add_executable(sockpuppet_scheduler_test sockpuppet_scheduler_test.cpp)
target_link_libraries(sockpuppet_scheduler_test sockpuppet)

enable_testing()
add_test(NAME sockpuppet_address_test COMMAND sockpuppet_address_test WORKING_DIRECTORY $<TARGET_FILE_DIR:sockpuppet>)
add_test(NAME sockpuppet_udp_test COMMAND sockpuppet_udp_test WORKING_DIRECTORY $<TARGET_FILE_DIR:sockpuppet>)
add_test(NAME sockpuppet_tcp_test COMMAND sockpuppet_tcp_test WORKING_DIRECTORY $<TARGET_FILE_DIR:sockpuppet>)
add_test(NAME sockpuppet_udp_buffered_test COMMAND sockpuppet_udp_buffered_test WORKING_DIRECTORY $<TARGET_FILE_DIR:sockpuppet>)
add_test(NAME sockpuppet_tcp_buffered_test COMMAND sockpuppet_tcp_buffered_test WORKING_DIRECTORY $<TARGET_FILE_DIR:sockpuppet>)
add_test(NAME sockpuppet_udp_async_test COMMAND sockpuppet_udp_async_test WORKING_DIRECTORY $<TARGET_FILE_DIR:sockpuppet>)
add_test(NAME sockpuppet_tcp_async_test COMMAND sockpuppet_tcp_async_test WORKING_DIRECTORY $<TARGET_FILE_DIR:sockpuppet>)
add_test(NAME sockpuppet_tcp_async_performance_test COMMAND sockpuppet_tcp_async_performance_test WORKING_DIRECTORY $<TARGET_FILE_DIR:sockpuppet>)
add_test(NAME sockpuppet_internals_test COMMAND sockpuppet_internals_test WORKING_DIRECTORY $<TARGET_FILE_DIR:sockpuppet>)
add_test(NAME sockpuppet_scheduler_test COMMAND sockpuppet_scheduler_test WORKING_DIRECTORY $<TARGET_FILE_DIR:sockpuppet>)

add_custom_target(build_tests
  DEPENDS sockpuppet_address_test
          sockpuppet_udp_test
          sockpuppet_tcp_test
          sockpuppet_udp_buffered_test
          sockpuppet_tcp_buffered_test
          sockpuppet_udp_async_test
          sockpuppet_tcp_async_test
          sockpuppet_tcp_async_performance_test
          sockpuppet_internals_test
          sockpuppet_scheduler_test
)

add_custom_target(run_tests
  COMMAND ${CMAKE_CTEST_COMMAND} -C $<CONFIGURATION> --verbose
  DEPENDS build_tests
)

install(TARGETS sockpuppet_address_test DESTINATION bin)
install(TARGETS sockpuppet_udp_test DESTINATION bin)
install(TARGETS sockpuppet_tcp_test DESTINATION bin)
install(TARGETS sockpuppet_udp_buffered_test DESTINATION bin)
install(TARGETS sockpuppet_tcp_buffered_test DESTINATION bin)
install(TARGETS sockpuppet_udp_async_test DESTINATION bin)
install(TARGETS sockpuppet_tcp_async_test DESTINATION bin)
install(TARGETS sockpuppet_tcp_async_performance_test DESTINATION bin)
install(TARGETS sockpuppet_internals_test DESTINATION bin)
install(TARGETS sockpuppet_scheduler_test DESTINATION bin)

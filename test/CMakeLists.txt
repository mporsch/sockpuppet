# build binaries next to the (shared) library and test certificate and key
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(WITH_TLS)
  # find openssl binary, hint to where we previously built/found it
  find_program(OPENSSL_BINARY openssl
    REQUIRED
    HINTS ${OPENSSL_ROOT_DIR}
  )

  add_custom_target(generate_certificate
    COMMAND ${OPENSSL_BINARY} req -newkey rsa:2048 -new -nodes -x509 -days 3650 -batch -keyout ${CMAKE_BINARY_DIR}/test_key.pem -out ${CMAKE_BINARY_DIR}/test_cert.pem
    BYPRODUCTS ${CMAKE_BINARY_DIR}/test_key.pem ${CMAKE_BINARY_DIR}/test_cert.pem
    COMMENT "Generating a self-signed certificate and private key to test with"
  )
endif(WITH_TLS)

# link everything with the static/shared library (and pthread where needed)
link_libraries(sockpuppet $<$<PLATFORM_ID:Linux>:pthread>)

add_executable(sockpuppet_address_test sockpuppet_address_test.cpp)
add_executable(sockpuppet_udp_test sockpuppet_udp_test.cpp)
add_executable(sockpuppet_tcp_test sockpuppet_tcp_test.cpp sockpuppet_test_common.h)
add_executable(sockpuppet_udp_buffered_test sockpuppet_udp_buffered_test.cpp sockpuppet_test_common.h)
add_executable(sockpuppet_tcp_buffered_test sockpuppet_tcp_buffered_test.cpp sockpuppet_test_common.h)
add_executable(sockpuppet_udp_async_test sockpuppet_udp_async_test.cpp)
add_executable(sockpuppet_tcp_async_test sockpuppet_tcp_async_test.cpp sockpuppet_test_common.h)
add_executable(sockpuppet_tcp_async_performance_test sockpuppet_tcp_async_performance_test.cpp sockpuppet_test_common.h)
add_executable(sockpuppet_internals_test sockpuppet_internals_test.cpp)
add_executable(sockpuppet_todo_test sockpuppet_todo_test.cpp)
if(WITH_TLS)
  add_executable(sockpuppet_tls_test sockpuppet_tcp_test.cpp sockpuppet_test_common.h)
  target_compile_definitions(sockpuppet_tls_test PRIVATE TEST_TLS)
  add_dependencies(sockpuppet_tls_test generate_certificate)

  add_executable(sockpuppet_tls_buffered_test sockpuppet_tcp_buffered_test.cpp sockpuppet_test_common.h)
  target_compile_definitions(sockpuppet_tls_buffered_test PRIVATE TEST_TLS)
  add_dependencies(sockpuppet_tls_buffered_test generate_certificate)

  add_executable(sockpuppet_tls_async_test sockpuppet_tcp_async_test.cpp sockpuppet_test_common.h)
  target_compile_definitions(sockpuppet_tls_async_test PRIVATE TEST_TLS)
  add_dependencies(sockpuppet_tls_async_test generate_certificate)

  add_executable(sockpuppet_tls_async_performance_test sockpuppet_tcp_async_performance_test.cpp sockpuppet_test_common.h)
  target_compile_definitions(sockpuppet_tls_async_performance_test PRIVATE TEST_TLS)
  add_dependencies(sockpuppet_tls_async_performance_test generate_certificate)
endif(WITH_TLS)

enable_testing()
add_test(NAME sockpuppet_address_test COMMAND sockpuppet_address_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME sockpuppet_udp_test COMMAND sockpuppet_udp_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME sockpuppet_tcp_test COMMAND sockpuppet_tcp_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME sockpuppet_udp_buffered_test COMMAND sockpuppet_udp_buffered_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME sockpuppet_tcp_buffered_test COMMAND sockpuppet_tcp_buffered_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME sockpuppet_udp_async_test COMMAND sockpuppet_udp_async_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME sockpuppet_tcp_async_test COMMAND sockpuppet_tcp_async_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME sockpuppet_tcp_async_performance_test COMMAND sockpuppet_tcp_async_performance_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME sockpuppet_internals_test COMMAND sockpuppet_internals_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME sockpuppet_todo_test COMMAND sockpuppet_todo_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
if(WITH_TLS)
  add_test(NAME sockpuppet_tls_test COMMAND sockpuppet_tls_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  add_test(NAME sockpuppet_tls_buffered_test COMMAND sockpuppet_tls_buffered_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  add_test(NAME sockpuppet_tls_async_test COMMAND sockpuppet_tls_async_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  add_test(NAME sockpuppet_tls_async_performance_test COMMAND sockpuppet_tls_async_performance_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif(WITH_TLS)

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
          sockpuppet_todo_test
)
if(WITH_TLS)
  add_dependencies(build_tests
    sockpuppet_tls_test
    sockpuppet_tls_buffered_test
    sockpuppet_tls_async_test
    sockpuppet_tls_async_performance_test
  )
endif(WITH_TLS)

add_custom_target(run_tests
  COMMAND ${CMAKE_CTEST_COMMAND} -C $<CONFIGURATION> --verbose
  DEPENDS build_tests
)

install(TARGETS sockpuppet_address_test DESTINATION test)
install(TARGETS sockpuppet_udp_test DESTINATION test)
install(TARGETS sockpuppet_tcp_test DESTINATION test)
install(TARGETS sockpuppet_udp_buffered_test DESTINATION test)
install(TARGETS sockpuppet_tcp_buffered_test DESTINATION test)
install(TARGETS sockpuppet_udp_async_test DESTINATION test)
install(TARGETS sockpuppet_tcp_async_test DESTINATION test)
install(TARGETS sockpuppet_tcp_async_performance_test DESTINATION test)
install(TARGETS sockpuppet_internals_test DESTINATION test)
install(TARGETS sockpuppet_todo_test DESTINATION test)
if(WITH_TLS)
  install(FILES ${CMAKE_BINARY_DIR}/test_key.pem ${CMAKE_BINARY_DIR}/test_cert.pem DESTINATION test)
  install(TARGETS sockpuppet_tls_test DESTINATION test)
  install(TARGETS sockpuppet_tls_buffered_test DESTINATION test)
  install(TARGETS sockpuppet_tls_async_test DESTINATION test)
  install(TARGETS sockpuppet_tls_async_performance_test DESTINATION test)
endif(WITH_TLS)

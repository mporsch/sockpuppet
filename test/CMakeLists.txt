# build binaries next to the (shared) library and test certificate and key
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(WITH_TLS)
  # find openssl binary, hint to where we previously built/found it
  find_program(OPENSSL_BINARY openssl
    REQUIRED
    HINTS ${OPENSSL_ROOT_DIR}
  )

  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/test_key.pem ${CMAKE_BINARY_DIR}/test_cert.pem
    COMMAND ${OPENSSL_BINARY} req -newkey rsa:2048 -new -nodes -x509 -days 3650 -subj "/" -batch -keyout ${CMAKE_BINARY_DIR}/test_key.pem -out ${CMAKE_BINARY_DIR}/test_cert.pem
    COMMENT "Generating a self-signed certificate and private key to test with"
  )
  add_custom_target(generate_certificate DEPENDS ${CMAKE_BINARY_DIR}/test_key.pem ${CMAKE_BINARY_DIR}/test_cert.pem)
endif(WITH_TLS)

# link everything with the static/shared library (and pthread where needed)
link_libraries(sockpuppet $<$<PLATFORM_ID:Linux>:pthread>)

if(WITH_TLS)
  add_executable(sockpuppet_tls_async_performance_test sockpuppet_tcp_async_performance_test.cpp sockpuppet_test_common.h)
  target_compile_definitions(sockpuppet_tls_async_performance_test PRIVATE TEST_TLS)
  add_dependencies(sockpuppet_tls_async_performance_test generate_certificate)
endif(WITH_TLS)

enable_testing()
if(WITH_TLS)
  add_test(NAME sockpuppet_tls_async_performance_test COMMAND sockpuppet_tls_async_performance_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif(WITH_TLS)

add_custom_target(build_tests)
if(WITH_TLS)
  add_dependencies(build_tests
    sockpuppet_tls_async_performance_test
  )
endif(WITH_TLS)

add_custom_target(run_tests
  COMMAND ${CMAKE_CTEST_COMMAND} -C $<CONFIGURATION> --verbose
  DEPENDS build_tests
)

if(WITH_TLS)
  install(FILES ${CMAKE_BINARY_DIR}/test_key.pem ${CMAKE_BINARY_DIR}/test_cert.pem DESTINATION test)
  install(TARGETS sockpuppet_tls_async_performance_test DESTINATION test)
endif(WITH_TLS)

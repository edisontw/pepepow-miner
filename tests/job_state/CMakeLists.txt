cmake_minimum_required(VERSION 3.20)
project(pepepow_job_state_tests LANGUAGES CXX)
enable_testing()
foreach(name IN ITEMS job_state stratum_line_framer stratum_difficulty_decoder stratum_notify_decoder stratum_subscribe_decoder offline_stratum_session offline_stratum_controller stratum_submit_decoder stratum_request_encoder)
  add_executable(test_${name} ../test_${name}.cpp)
  target_compile_features(test_${name} PRIVATE cxx_std_20)
  target_include_directories(test_${name} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../include")
  if(MSVC)
    target_compile_options(test_${name} PRIVATE /W4 /WX)
  else()
    target_compile_options(test_${name} PRIVATE -Wall -Wextra -Werror -pedantic)
  endif()
  add_test(NAME ${name} COMMAND test_${name})
endforeach()

# A runnable offline CLI: no network, hashing or live pool submission.
add_executable(offline_stratum_replay ../../tools/offline_stratum_replay.cpp)
target_compile_features(offline_stratum_replay PRIVATE cxx_std_20)
target_include_directories(offline_stratum_replay PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../include")
if(MSVC)
  target_compile_options(offline_stratum_replay PRIVATE /W4 /WX)
else()
  target_compile_options(offline_stratum_replay PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
add_test(NAME offline_replay_help COMMAND offline_stratum_replay --help)
add_test(NAME offline_replay_health COMMAND offline_stratum_replay --health-json)
set_tests_properties(offline_replay_health PROPERTIES
  PASS_REGULAR_EXPRESSION "\\\"g0\\\":\\\"NOT PASSED\\\".*\\\"mining_ready\\\":false")
add_test(NAME offline_replay_handshake COMMAND offline_stratum_replay --self-test)

# An incomplete final JSONL frame must not be silently accepted.
add_test(NAME offline_replay_truncated COMMAND offline_stratum_replay --self-test-truncated)
set_tests_properties(offline_replay_truncated PROPERTIES WILL_FAIL TRUE)

# Oversized framed input after a valid handshake must fail the replay CLI.
add_test(NAME offline_replay_malformed COMMAND offline_stratum_replay --self-test-malformed)
set_tests_properties(offline_replay_malformed PROPERTIES WILL_FAIL TRUE)

# Reconnect must clear partial frames and reject stale transport generations.
add_test(NAME offline_replay_reconnect COMMAND offline_stratum_replay --self-test-reconnect)

# Cancelled IDs cannot be reused, including across reconnects.
add_test(NAME offline_replay_cancel COMMAND offline_stratum_replay --self-test-cancel)

# Invalid credentials must fail before any Stratum request is emitted.
add_test(NAME offline_replay_invalid_login COMMAND offline_stratum_replay "")
set_tests_properties(offline_replay_invalid_login PROPERTIES WILL_FAIL TRUE)

# Unexpected replies must not be accepted merely because handshake succeeded.
add_test(NAME offline_replay_unexpected COMMAND offline_stratum_replay --self-test-unexpected)
set_tests_properties(offline_replay_unexpected PROPERTIES WILL_FAIL TRUE)

# EOF before subscribe reply must not report a healthy handshake.
add_test(NAME offline_replay_missing_subscribe COMMAND offline_stratum_replay --self-test-missing-subscribe)
set_tests_properties(offline_replay_missing_subscribe PROPERTIES WILL_FAIL TRUE)

# A subscribe reply without authorize reply leaves an outstanding request.
add_test(NAME offline_replay_missing_authorize COMMAND offline_stratum_replay --self-test-missing-authorize)
set_tests_properties(offline_replay_missing_authorize PROPERTIES WILL_FAIL TRUE)

# Native loopback socket smoke test: OS-assigned 127.0.0.1 port only.
add_executable(test_stratum_socket_loopback ../test_stratum_socket_loopback.cpp)
target_compile_features(test_stratum_socket_loopback PRIVATE cxx_std_20)
target_include_directories(test_stratum_socket_loopback PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../include")
if(MSVC)
  target_compile_options(test_stratum_socket_loopback PRIVATE /W4 /WX)
  target_link_libraries(test_stratum_socket_loopback PRIVATE ws2_32)
else()
  target_compile_options(test_stratum_socket_loopback PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
add_test(NAME stratum_socket_loopback COMMAND test_stratum_socket_loopback)
set_tests_properties(stratum_socket_loopback PROPERTIES TIMEOUT 10)

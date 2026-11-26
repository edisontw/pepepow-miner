cmake_minimum_required(VERSION 3.24)

# Visual Studio's CUDA build customization reads CudaToolkitDir from MSBuild
# project globals. Set it before project() so CMake's CUDA compiler
# try-compile inherits the same Toolkit root as the real targets.
if(WIN32 AND CMAKE_GENERATOR MATCHES "^Visual Studio")
  if(NOT CUDAToolkit_ROOT AND DEFINED ENV{CUDA_PATH})
    set(CUDAToolkit_ROOT "$ENV{CUDA_PATH}")
  endif()
  if(CUDAToolkit_ROOT)
    list(APPEND CMAKE_VS_GLOBALS "CudaToolkitDir=${CUDAToolkit_ROOT}/")
  endif()
  list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CMAKE_VS_GLOBALS)
endif()

project(pepepow_cuda_device_probe LANGUAGES CXX CUDA)

include(CTest)
find_package(CUDAToolkit 13.3 REQUIRED)

option(PEPEPOW_CUDA_CORRECTNESS_TESTS
  "Build the CUDA batch contract and pinned-Core CPU recheck tests" OFF)
option(PEPEPOW_CUDA_HOOHASH_STAGE_TESTS
  "Build the first HooHash CUDA stage and pinned-Core BLAKE3 oracle" OFF)
set(PEPEPOW_CPU_ORACLE "" CACHE FILEPATH
  "Path to pepepow-kat built against the pinned PEPEPOW Core/OpenLibm reference")
set(PEPEPOW_CORE_SOURCE_DIR "" CACHE PATH
  "Path to the exact pinned PEPEPOW Core source for stage-level BLAKE3 tests")
set(PEPEPOW_CUDA_TEST_ARCHITECTURE "75" CACHE STRING
  "CUDA architecture used by the short nonce-coverage test kernel")
if(BUILD_TESTING)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
endif()

# CMake reports the nvcc component version (for example, 13.3.73), while
# version.json carries the CUDA Toolkit release (for example, 13.3.1).
set(PEPEPOW_CUDA_TOOLKIT_RELEASE "${CUDAToolkit_VERSION}")
set(_pepepow_cuda_root "${CUDAToolkit_ROOT}")
if(NOT _pepepow_cuda_root AND CUDAToolkit_BIN_DIR)
  get_filename_component(_pepepow_cuda_root "${CUDAToolkit_BIN_DIR}" DIRECTORY)
endif()
if(_pepepow_cuda_root AND EXISTS "${_pepepow_cuda_root}/version.json")
  file(READ "${_pepepow_cuda_root}/version.json" _pepepow_cuda_version_json)
  string(JSON _pepepow_cuda_release ERROR_VARIABLE _pepepow_cuda_json_error
    GET "${_pepepow_cuda_version_json}" cuda version)
  if(NOT _pepepow_cuda_json_error)
    set(PEPEPOW_CUDA_TOOLKIT_RELEASE "${_pepepow_cuda_release}")
  endif()
endif()

function(configure_cuda_probe_target target)
  target_compile_features(${target} PRIVATE cxx_std_17)
  target_compile_definitions(${target} PRIVATE
    "PEPEPOW_CUDA_TOOLKIT_VERSION=\"${PEPEPOW_CUDA_TOOLKIT_RELEASE}\"")
  target_link_libraries(${target} PRIVATE CUDA::cudart)
  set_target_properties(${target} PROPERTIES
    CUDA_ARCHITECTURES OFF
    CUDA_RUNTIME_LIBRARY Shared
    CUDA_STANDARD 17
    CUDA_STANDARD_REQUIRED ON)
endfunction()

add_executable(pepepow-cuda-device-probe cuda_device_probe.cu)
configure_cuda_probe_target(pepepow-cuda-device-probe)

if(PEPEPOW_CUDA_CORRECTNESS_TESTS)
  if(NOT BUILD_TESTING)
    message(FATAL_ERROR "PEPEPOW_CUDA_CORRECTNESS_TESTS requires BUILD_TESTING=ON")
  endif()
  if(NOT PEPEPOW_CPU_ORACLE OR NOT EXISTS "${PEPEPOW_CPU_ORACLE}")
    message(FATAL_ERROR
      "Set PEPEPOW_CPU_ORACLE to the real pinned-Core pepepow-kat executable")
  endif()

  add_executable(pepepow-cuda-batch-coverage cuda_batch_coverage.cu)
  configure_cuda_probe_target(pepepow-cuda-batch-coverage)
  target_include_directories(pepepow-cuda-batch-coverage PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
  set_target_properties(pepepow-cuda-batch-coverage PROPERTIES
    CUDA_ARCHITECTURES "${PEPEPOW_CUDA_TEST_ARCHITECTURE}")

  add_executable(pepepow-cuda-target-boundary-test
    ../../tests/test_cuda_target_boundaries.cpp)
  target_compile_features(pepepow-cuda-target-boundary-test PRIVATE cxx_std_17)
  target_include_directories(pepepow-cuda-target-boundary-test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
  add_test(NAME cuda_target_boundary_contract
    COMMAND "$<TARGET_FILE:pepepow-cuda-target-boundary-test>")

  add_test(NAME cuda_correctness_cpu_oracle_and_batch_contract
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/test_cuda_correctness_harness.py"
            --oracle "${PEPEPOW_CPU_ORACLE}"
            --coverage-probe "$<TARGET_FILE:pepepow-cuda-batch-coverage>")
  set_tests_properties(cuda_correctness_cpu_oracle_and_batch_contract
    PROPERTIES TIMEOUT 30)
endif()

if(PEPEPOW_CUDA_HOOHASH_STAGE_TESTS)
  if(NOT BUILD_TESTING)
    message(FATAL_ERROR "PEPEPOW_CUDA_HOOHASH_STAGE_TESTS requires BUILD_TESTING=ON")
  endif()
  if(NOT CMAKE_C_COMPILER_LOADED)
    enable_language(C)
  endif()
  if(NOT PEPEPOW_CORE_SOURCE_DIR OR
     NOT EXISTS "${PEPEPOW_CORE_SOURCE_DIR}/src/crypto/blake3.c" OR
     NOT EXISTS "${PEPEPOW_CORE_SOURCE_DIR}/src/crypto/blake3_dispatch.c" OR
     NOT EXISTS "${PEPEPOW_CORE_SOURCE_DIR}/src/crypto/blake3_portable.c")
    message(FATAL_ERROR
      "Set PEPEPOW_CORE_SOURCE_DIR to pinned PEPEPOW Core source containing BLAKE3")
  endif()

  add_library(pepepow-core-blake3-reference STATIC
    "${PEPEPOW_CORE_SOURCE_DIR}/src/crypto/blake3.c"
    "${PEPEPOW_CORE_SOURCE_DIR}/src/crypto/blake3_dispatch.c"
    "${PEPEPOW_CORE_SOURCE_DIR}/src/crypto/blake3_portable.c")
  target_include_directories(pepepow-core-blake3-reference PUBLIC
    "${PEPEPOW_CORE_SOURCE_DIR}/src")
  set_target_properties(pepepow-core-blake3-reference PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON)

  add_executable(pepepow-hoohash-blake3-cpu-oracle blake3_cpu_oracle.c)
  target_link_libraries(pepepow-hoohash-blake3-cpu-oracle PRIVATE
    pepepow-core-blake3-reference)

  add_executable(pepepow-cuda-hoohash-blake3-stage
    cuda_hoohash_blake3_stage.cu)
  configure_cuda_probe_target(pepepow-cuda-hoohash-blake3-stage)
  set_target_properties(pepepow-cuda-hoohash-blake3-stage PROPERTIES
    CUDA_ARCHITECTURES "${PEPEPOW_CUDA_TEST_ARCHITECTURE}")

  add_test(NAME hoohash_blake3_stage_full_digest_parity
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/test_cuda_blake3_stage.py"
            --oracle "$<TARGET_FILE:pepepow-hoohash-blake3-cpu-oracle>"
            --device-stage "$<TARGET_FILE:pepepow-cuda-hoohash-blake3-stage>")
  set_tests_properties(hoohash_blake3_stage_full_digest_parity
    PROPERTIES TIMEOUT 30)
endif()

if(BUILD_TESTING)
  # This separate test-only binary emits deterministic error fixtures before
  # touching CUDA. It never impersonates the real device probe executable.
  add_executable(cuda-device-probe-error-fixtures cuda_device_probe.cu)
  target_compile_definitions(cuda-device-probe-error-fixtures PRIVATE
    PEPEPOW_CUDA_DEVICE_PROBE_TEST_FIXTURES=1)
  configure_cuda_probe_target(cuda-device-probe-error-fixtures)

  add_test(NAME cuda_device_probe_json_contract
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/test_cuda_device_probe.py"
            --probe "$<TARGET_FILE:pepepow-cuda-device-probe>"
            --fixtures "$<TARGET_FILE:cuda-device-probe-error-fixtures>")
  set_tests_properties(cuda_device_probe_json_contract PROPERTIES TIMEOUT 15)
endif()

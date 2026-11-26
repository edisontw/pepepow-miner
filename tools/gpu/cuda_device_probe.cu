#include <cuda_runtime.h>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#ifndef PEPEPOW_CUDA_TOOLKIT_VERSION
#define PEPEPOW_CUDA_TOOLKIT_VERSION "unknown"
#endif

namespace {

enum class ProbeError {
  no_device,
  driver_unavailable,
  initialization_failed,
  query_failed,
  invalid_arguments,
};

struct VersionInfo {
  int driver_api_supported = -1;
  int runtime = -1;
};

int exitCode(ProbeError error) {
  switch (error) {
    case ProbeError::no_device:
      return 2;
    case ProbeError::driver_unavailable:
      return 3;
    case ProbeError::initialization_failed:
      return 4;
    case ProbeError::invalid_arguments:
      return 64;
    case ProbeError::query_failed:
      return 5;
  }
  return 5;
}

const char* errorCode(ProbeError error) {
  switch (error) {
    case ProbeError::no_device:
      return "no_device";
    case ProbeError::driver_unavailable:
      return "driver_unavailable";
    case ProbeError::initialization_failed:
      return "initialization_failed";
    case ProbeError::invalid_arguments:
      return "invalid_arguments";
    case ProbeError::query_failed:
      return "query_failed";
  }
  return "query_failed";
}

const char* errorMessage(ProbeError error) {
  switch (error) {
    case ProbeError::no_device:
      return "No CUDA-capable device is available.";
    case ProbeError::driver_unavailable:
      return "No compatible NVIDIA CUDA driver is available.";
    case ProbeError::initialization_failed:
      return "CUDA runtime initialization failed.";
    case ProbeError::invalid_arguments:
      return "Usage: pepepow-cuda-device-probe";
    case ProbeError::query_failed:
      return "A CUDA Runtime API device query failed.";
  }
  return "A CUDA Runtime API device query failed.";
}

const char* cudaErrorName(cudaError_t error) {
  switch (error) {
    case cudaSuccess:
      return "cudaSuccess";
    case cudaErrorNoDevice:
      return "cudaErrorNoDevice";
    case cudaErrorInsufficientDriver:
      return "cudaErrorInsufficientDriver";
    case cudaErrorInitializationError:
      return "cudaErrorInitializationError";
    default:
      return "cudaErrorOther";
  }
}

ProbeError classifyCudaError(cudaError_t error) {
  switch (error) {
    case cudaErrorNoDevice:
      return ProbeError::no_device;
    case cudaErrorInsufficientDriver:
      return ProbeError::driver_unavailable;
    case cudaErrorInitializationError:
      return ProbeError::initialization_failed;
    default:
      return ProbeError::query_failed;
  }
}

std::string jsonEscape(const char* value) {
  std::ostringstream output;
  output << '"';
  for (const unsigned char byte : std::string(value == nullptr ? "" : value)) {
    switch (byte) {
      case '"':
        output << "\\\"";
        break;
      case '\\':
        output << "\\\\";
        break;
      case '\b':
        output << "\\b";
        break;
      case '\f':
        output << "\\f";
        break;
      case '\n':
        output << "\\n";
        break;
      case '\r':
        output << "\\r";
        break;
      case '\t':
        output << "\\t";
        break;
      default:
        if (byte < 0x20) {
          output << "\\u00" << std::hex << std::setw(2) << std::setfill('0')
                 << static_cast<unsigned int>(byte) << std::dec;
        } else {
          output << static_cast<char>(byte);
        }
    }
  }
  output << '"';
  return output.str();
}

std::string versionJson(int encodedVersion) {
  if (encodedVersion < 0) {
    return "null";
  }
  const int major = encodedVersion / 1000;
  const int minor = (encodedVersion % 1000) / 10;
  std::ostringstream output;
  output << "{\"encoded\":" << encodedVersion << ",\"major\":" << major
         << ",\"minor\":" << minor << '}';
  return output.str();
}

std::string versionsJson(const VersionInfo& versions) {
  std::ostringstream output;
  output << "{\"driver_api_supported\":"
         << versionJson(versions.driver_api_supported)
         << ",\"runtime\":" << versionJson(versions.runtime)
         << ",\"toolkit_build\":" << jsonEscape(PEPEPOW_CUDA_TOOLKIT_VERSION)
         << '}';
  return output.str();
}

int emitError(ProbeError error, cudaError_t cudaError,
              const VersionInfo& versions) {
  std::cout << "{\"schema_version\":1,\"status\":\"error\",\"cuda_versions\":"
            << versionsJson(versions)
            << ",\"devices\":[],\"error\":{\"code\":"
            << jsonEscape(errorCode(error)) << ",\"cuda_error\":"
            << jsonEscape(cudaErrorName(cudaError)) << ",\"message\":"
            << jsonEscape(errorMessage(error)) << ",\"exit_code\":"
            << exitCode(error) << "}}\n";
  return exitCode(error);
}

std::string deviceJson(int index, const cudaDeviceProp& properties) {
  int fp64Ratio = 0;
  const cudaError_t fp64Query = cudaDeviceGetAttribute(
      &fp64Ratio, cudaDevAttrSingleToDoublePrecisionPerfRatio, index);
  const bool fp64AttributeAvailable = fp64Query == cudaSuccess;
  if (!fp64AttributeAvailable) {
    // The FP64 attribute is informational. A missing attribute must not hide
    // the rest of the device record or leave a sticky runtime error behind.
    static_cast<void>(cudaGetLastError());
  }

  std::ostringstream output;
  output << "{\"index\":" << index << ",\"name\":"
         << jsonEscape(properties.name) << ",\"compute_capability\":{\"major\":"
         << properties.major << ",\"minor\":" << properties.minor
         << ",\"architecture\":\"sm_" << properties.major * 10 + properties.minor
         << "\"},\"global_memory_bytes\":"
         << static_cast<std::uint64_t>(properties.totalGlobalMem)
         << ",\"shared_memory_per_block_bytes\":"
         << static_cast<std::uint64_t>(properties.sharedMemPerBlock)
         << ",\"shared_memory_per_block_optin_bytes\":"
         << static_cast<std::uint64_t>(properties.sharedMemPerBlockOptin)
         << ",\"registers_per_block\":" << properties.regsPerBlock
         << ",\"registers_per_multiprocessor\":"
         << properties.regsPerMultiprocessor << ",\"multiprocessor_count\":"
         << properties.multiProcessorCount << ",\"warp_size\":"
         << properties.warpSize << ",\"max_threads_per_block\":"
         << properties.maxThreadsPerBlock << ",\"fp64\":{\"attribute_available\":"
         << (fp64AttributeAvailable ? "true" : "false")
         << ",\"supported\":";
  if (fp64AttributeAvailable) {
    output << (fp64Ratio > 0 ? "true" : "false");
  } else {
    output << "null";
  }
  output << ",\"single_to_double_precision_perf_ratio\":";
  if (fp64AttributeAvailable) {
    output << fp64Ratio;
  } else {
    output << "null";
  }
  output << ",\"limitation\":\"This relative throughput attribute does not establish CPU, OpenLibm, or cross-architecture digest parity.\"}}";
  return output.str();
}

int runProbe() {
  VersionInfo versions;
  int runtimeVersion = 0;
  cudaError_t error = cudaRuntimeGetVersion(&runtimeVersion);
  if (error != cudaSuccess) {
    return emitError(classifyCudaError(error), error, versions);
  }
  versions.runtime = runtimeVersion;

  int driverVersion = 0;
  error = cudaDriverGetVersion(&driverVersion);
  if (error != cudaSuccess) {
    return emitError(classifyCudaError(error), error, versions);
  }
  versions.driver_api_supported = driverVersion;
  if (driverVersion == 0) {
    return emitError(ProbeError::driver_unavailable, cudaSuccess, versions);
  }

  int deviceCount = 0;
  error = cudaGetDeviceCount(&deviceCount);
  if (error != cudaSuccess) {
    static_cast<void>(cudaGetLastError());
    return emitError(classifyCudaError(error), error, versions);
  }
  if (deviceCount == 0) {
    return emitError(ProbeError::no_device, cudaSuccess, versions);
  }

  std::vector<std::string> devices;
  devices.reserve(static_cast<std::size_t>(deviceCount));
  for (int index = 0; index < deviceCount; ++index) {
    cudaDeviceProp properties{};
    error = cudaGetDeviceProperties(&properties, index);
    if (error != cudaSuccess) {
      static_cast<void>(cudaGetLastError());
      return emitError(classifyCudaError(error), error, versions);
    }
    devices.push_back(deviceJson(index, properties));
  }

  std::cout << "{\"schema_version\":1,\"status\":\"ok\",\"cuda_versions\":"
            << versionsJson(versions) << ",\"devices\":[";
  for (std::size_t index = 0; index < devices.size(); ++index) {
    if (index != 0) {
      std::cout << ',';
    }
    std::cout << devices[index];
  }
  std::cout << "],\"error\":null}\n";
  return 0;
}

#if defined(PEPEPOW_CUDA_DEVICE_PROBE_TEST_FIXTURES)
int runErrorFixture(const std::string& argument) {
  VersionInfo versions;
  ProbeError error = ProbeError::query_failed;
  cudaError_t runtimeError = cudaErrorUnknown;
  if (argument == "--fixture-error=no_device") {
    error = ProbeError::no_device;
    runtimeError = cudaErrorNoDevice;
  } else if (argument == "--fixture-error=driver_unavailable") {
    error = ProbeError::driver_unavailable;
    runtimeError = cudaErrorInsufficientDriver;
  } else if (argument == "--fixture-error=initialization_failed") {
    error = ProbeError::initialization_failed;
    runtimeError = cudaErrorInitializationError;
  } else if (argument == "--fixture-error=query_failed") {
    error = ProbeError::query_failed;
    runtimeError = cudaErrorUnknown;
  } else {
    return -1;
  }
  return emitError(error, runtimeError, versions);
}
#endif

}  // namespace

int main(int argc, char** argv) {
  if (argc == 1) {
    return runProbe();
  }

#if defined(PEPEPOW_CUDA_DEVICE_PROBE_TEST_FIXTURES)
  if (argc == 2) {
    const int result = runErrorFixture(argv[1]);
    if (result >= 0) {
      return result;
    }
  }
#endif

  VersionInfo versions;
  return emitError(ProbeError::invalid_arguments, cudaSuccess, versions);
}

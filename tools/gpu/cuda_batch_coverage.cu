#include <cuda_runtime.h>

#include "pepepow/header_nonce.h"

#include <charconv>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint32_t kMaximumBatch = 4096;
constexpr std::uint64_t kNonceSpace = std::uint64_t{1} << 32;

enum class ErrorCode {
  no_device,
  driver_unavailable,
  initialization_failed,
  query_failed,
  invalid_arguments,
};

struct Request {
  std::string header_hex;
  std::uint8_t header[PEPEPOW_HEADER_SIZE]{};
  std::uint32_t first = 0;
  std::uint32_t count = 0;
};

__global__ void emitNonceCoverage(std::uint32_t first, std::uint32_t count,
                                  std::uint32_t* covered) {
  const std::uint64_t slot =
      static_cast<std::uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  if (slot < count) {
    covered[slot] = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(first) + slot);
  }
}

const char* errorCodeName(ErrorCode error) {
  switch (error) {
    case ErrorCode::no_device:
      return "no_device";
    case ErrorCode::driver_unavailable:
      return "driver_unavailable";
    case ErrorCode::initialization_failed:
      return "initialization_failed";
    case ErrorCode::query_failed:
      return "query_failed";
    case ErrorCode::invalid_arguments:
      return "invalid_arguments";
  }
  return "query_failed";
}

int exitCode(ErrorCode error) {
  switch (error) {
    case ErrorCode::no_device:
      return 2;
    case ErrorCode::driver_unavailable:
      return 3;
    case ErrorCode::initialization_failed:
      return 4;
    case ErrorCode::query_failed:
      return 5;
    case ErrorCode::invalid_arguments:
      return 64;
  }
  return 5;
}

ErrorCode classify(cudaError_t error) {
  switch (error) {
    case cudaErrorNoDevice:
      return ErrorCode::no_device;
    case cudaErrorInsufficientDriver:
      return ErrorCode::driver_unavailable;
    case cudaErrorInitializationError:
      return ErrorCode::initialization_failed;
    default:
      return ErrorCode::query_failed;
  }
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
    case cudaErrorMemoryAllocation:
      return "cudaErrorMemoryAllocation";
    default:
      return "cudaErrorOther";
  }
}

bool decodeHex(std::string_view text, std::uint8_t* output,
               std::size_t output_size) {
  if (text.size() != output_size * 2) return false;
  const auto nibble = [](char value) -> int {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
  };
  for (std::size_t i = 0; i < output_size; ++i) {
    const int high = nibble(text[2 * i]);
    const int low = nibble(text[2 * i + 1]);
    if (high < 0 || low < 0) return false;
    output[i] = static_cast<std::uint8_t>((high << 4) | low);
  }
  return true;
}

std::string jsonEscape(std::string_view value) {
  std::ostringstream output;
  output << '"';
  for (const unsigned char byte : value) {
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

bool parseU32(std::string_view text, std::uint32_t& value) {
  if (text.empty()) return false;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

bool parseArguments(int argc, char** argv, Request& request) {
  bool have_header = false;
  bool have_first = false;
  bool have_count = false;
  for (int index = 1; index < argc; index += 2) {
    if (index + 1 >= argc) return false;
    const std::string_view key(argv[index]);
    const std::string_view value(argv[index + 1]);
    if (key == "--header-hex" && !have_header) {
      request.header_hex.assign(value);
      have_header = decodeHex(value, request.header, sizeof(request.header));
      if (!have_header) return false;
    } else if (key == "--nonce-start" && !have_first) {
      have_first = parseU32(value, request.first);
      if (!have_first) return false;
    } else if (key == "--nonce-count" && !have_count) {
      have_count = parseU32(value, request.count);
      if (!have_count) return false;
    } else {
      return false;
    }
  }

  std::uint32_t header_nonce = 0;
  if (!have_header || !have_first || !have_count || request.count == 0 ||
      request.count > kMaximumBatch ||
      static_cast<std::uint64_t>(request.first) + request.count > kNonceSpace ||
      pepepow_header_get_nonce(request.header, sizeof(request.header),
                               &header_nonce) != 0 ||
      header_nonce != request.first) {
    return false;
  }
  return true;
}

void emitError(ErrorCode code, cudaError_t cuda_error,
               const Request* request = nullptr) {
  std::cout << "{\"schema_version\":1,\"status\":\"error\",\"contract\":";
  if (request == nullptr) {
    std::cout << "null";
  } else {
    std::cout << "{\"header_size_bytes\":80,\"nonce_byte_order\":\"little-endian\",\"header_hex\":\""
              << request->header_hex << "\",\"nonce_start\":"
              << request->first << ",\"nonce_count\":" << request->count
              << '}';
  }
  std::cout << ",\"device\":null,\"covered_nonces\":[],\"candidates\":[],\"error\":{\"code\":\""
            << errorCodeName(code) << "\",\"cuda_error\":\""
            << cudaErrorName(cuda_error) << "\",\"exit_code\":"
            << exitCode(code) << "}}\n";
}

int runCoverage(const Request& request) {
  int device_count = 0;
  cudaError_t error = cudaGetDeviceCount(&device_count);
  if (error != cudaSuccess) {
    static_cast<void>(cudaGetLastError());
    const ErrorCode code = classify(error);
    emitError(code, error, &request);
    return exitCode(code);
  }
  if (device_count == 0) {
    emitError(ErrorCode::no_device, cudaSuccess, &request);
    return exitCode(ErrorCode::no_device);
  }

  cudaDeviceProp device{};
  error = cudaGetDeviceProperties(&device, 0);
  if (error != cudaSuccess) {
    static_cast<void>(cudaGetLastError());
    const ErrorCode code = classify(error);
    emitError(code, error, &request);
    return exitCode(code);
  }

  std::uint32_t* device_coverage = nullptr;
  error = cudaMalloc(&device_coverage,
                     static_cast<std::size_t>(request.count) * sizeof(std::uint32_t));
  if (error != cudaSuccess) {
    const ErrorCode code = classify(error);
    emitError(code, error, &request);
    return exitCode(code);
  }

  constexpr unsigned threads_per_block = 256;
  const unsigned blocks =
      (request.count + threads_per_block - 1) / threads_per_block;
  emitNonceCoverage<<<blocks, threads_per_block>>>(request.first, request.count,
                                                     device_coverage);
  error = cudaGetLastError();
  if (error == cudaSuccess) error = cudaDeviceSynchronize();

  std::vector<std::uint32_t> coverage(request.count);
  if (error == cudaSuccess) {
    error = cudaMemcpy(coverage.data(), device_coverage,
                       coverage.size() * sizeof(std::uint32_t),
                       cudaMemcpyDeviceToHost);
  }
  const cudaError_t free_error = cudaFree(device_coverage);
  if (error == cudaSuccess && free_error != cudaSuccess) error = free_error;
  if (error != cudaSuccess) {
    static_cast<void>(cudaGetLastError());
    const ErrorCode code = classify(error);
    emitError(code, error, &request);
    return exitCode(code);
  }

  std::cout << "{\"schema_version\":1,\"status\":\"ok\",\"contract\":{\"header_size_bytes\":80,\"nonce_byte_order\":\"little-endian\",\"header_hex\":\""
            << request.header_hex << "\",\"nonce_start\":" << request.first
            << ",\"nonce_count\":" << request.count
            << "},\"device\":{\"index\":0,\"name\":"
            << jsonEscape(device.name) << "},\"covered_nonces\":[";
  for (std::size_t index = 0; index < coverage.size(); ++index) {
    if (index != 0) std::cout << ',';
    std::cout << coverage[index];
  }
  std::cout << "],\"candidates\":[],\"error\":null}\n";
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  Request request;
  if (!parseArguments(argc, argv, request)) {
    emitError(ErrorCode::invalid_arguments, cudaSuccess);
    return exitCode(ErrorCode::invalid_arguments);
  }
  return runCoverage(request);
}

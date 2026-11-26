#include <cuda_runtime.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>

namespace {

__device__ __constant__ std::uint32_t kIv[8] = {
    0x6A09E667U, 0xBB67AE85U, 0x3C6EF372U, 0xA54FF53AU,
    0x510E527FU, 0x9B05688CU, 0x1F83D9ABU, 0x5BE0CD19U};
__device__ __constant__ std::uint8_t kMessagePermutation[16] = {
    2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8};

enum class ErrorCode {
  no_device,
  driver_unavailable,
  initialization_failed,
  cuda_failure,
  invalid_arguments,
};

__device__ std::uint32_t rotateRight(std::uint32_t value,
                                     unsigned int amount) {
  return (value >> amount) | (value << (32U - amount));
}

__device__ std::uint32_t load32LittleEndian(const std::uint8_t *bytes) {
  return static_cast<std::uint32_t>(bytes[0]) |
         (static_cast<std::uint32_t>(bytes[1]) << 8U) |
         (static_cast<std::uint32_t>(bytes[2]) << 16U) |
         (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

__device__ void mix(std::uint32_t state[16], int a, int b, int c, int d,
                    std::uint32_t x, std::uint32_t y) {
  state[a] = state[a] + state[b] + x;
  state[d] = rotateRight(state[d] ^ state[a], 16);
  state[c] += state[d];
  state[b] = rotateRight(state[b] ^ state[c], 12);
  state[a] = state[a] + state[b] + y;
  state[d] = rotateRight(state[d] ^ state[a], 8);
  state[c] += state[d];
  state[b] = rotateRight(state[b] ^ state[c], 7);
}

__device__ void compress(const std::uint32_t chaining_value[8],
                         const std::uint8_t block[64],
                         std::uint32_t block_length, std::uint32_t flags,
                         std::uint32_t output[16]) {
  std::uint32_t state[16];
  std::uint32_t message[16];
  for (int index = 0; index < 8; ++index) {
    state[index] = chaining_value[index];
  }
  for (int index = 0; index < 4; ++index) {
    state[8 + index] = kIv[index];
  }
  state[12] = 0;
  state[13] = 0;
  state[14] = block_length;
  state[15] = flags;
  for (int index = 0; index < 16; ++index) {
    message[index] = load32LittleEndian(block + 4 * index);
  }

  for (int round = 0; round < 7; ++round) {
    mix(state, 0, 4, 8, 12, message[0], message[1]);
    mix(state, 1, 5, 9, 13, message[2], message[3]);
    mix(state, 2, 6, 10, 14, message[4], message[5]);
    mix(state, 3, 7, 11, 15, message[6], message[7]);
    mix(state, 0, 5, 10, 15, message[8], message[9]);
    mix(state, 1, 6, 11, 12, message[10], message[11]);
    mix(state, 2, 7, 8, 13, message[12], message[13]);
    mix(state, 3, 4, 9, 14, message[14], message[15]);

    if (round != 6) {
      std::uint32_t permuted[16];
      for (int index = 0; index < 16; ++index) {
        permuted[index] = message[kMessagePermutation[index]];
      }
      for (int index = 0; index < 16; ++index) {
        message[index] = permuted[index];
      }
    }
  }

  for (int index = 0; index < 8; ++index) {
    output[index] = state[index] ^ state[index + 8];
    output[index + 8] = state[index + 8] ^ chaining_value[index];
  }
}

__device__ void store32LittleEndian(std::uint32_t value,
                                    std::uint8_t *bytes) {
  bytes[0] = static_cast<std::uint8_t>(value);
  bytes[1] = static_cast<std::uint8_t>(value >> 8U);
  bytes[2] = static_cast<std::uint8_t>(value >> 16U);
  bytes[3] = static_cast<std::uint8_t>(value >> 24U);
}

__device__ void blake3Hash80(const std::uint8_t input[80],
                            std::uint8_t output[32]) {
  std::uint8_t block[64];
  std::uint32_t chaining_value[8];
  std::uint32_t compressed[16];
  for (int index = 0; index < 64; ++index) {
    block[index] = input[index];
  }
  for (int index = 0; index < 8; ++index) {
    chaining_value[index] = kIv[index];
  }

  // The 64-byte block starts the only BLAKE3 chunk, but is not its final block.
  compress(chaining_value, block, 64, 1, compressed);
  for (int index = 0; index < 8; ++index) {
    chaining_value[index] = compressed[index];
  }

  for (int index = 0; index < 64; ++index) {
    block[index] = index < 16 ? input[64 + index] : 0;
  }
  // CHUNK_END | ROOT; the 32-byte digest is the first root output block.
  compress(chaining_value, block, 16, 2 | 8, compressed);
  for (int index = 0; index < 8; ++index) {
    store32LittleEndian(compressed[index], output + 4 * index);
  }
}

__global__ void hashOneHeader(const std::uint8_t *input,
                              std::uint8_t *output) {
  if (blockIdx.x == 0 && threadIdx.x == 0) {
    blake3Hash80(input, output);
  }
}

const char *errorName(ErrorCode error) {
  switch (error) {
    case ErrorCode::no_device:
      return "no_device";
    case ErrorCode::driver_unavailable:
      return "driver_unavailable";
    case ErrorCode::initialization_failed:
      return "initialization_failed";
    case ErrorCode::cuda_failure:
      return "cuda_failure";
    case ErrorCode::invalid_arguments:
      return "invalid_arguments";
  }
  return "cuda_failure";
}

int exitCode(ErrorCode error) {
  switch (error) {
    case ErrorCode::no_device:
      return 2;
    case ErrorCode::driver_unavailable:
      return 3;
    case ErrorCode::initialization_failed:
      return 4;
    case ErrorCode::cuda_failure:
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
      return ErrorCode::cuda_failure;
  }
}

const char *cudaErrorName(cudaError_t error) {
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

void emitError(ErrorCode code, cudaError_t cuda_error,
               std::string_view mode) {
  std::cout << "{\"schema_version\":1,\"status\":\"error\","
            << "\"stage\":\"hoohash_v110_blake3_80\",\"input_mode\":\""
            << mode << "\",\"input_size_bytes\":80,\"digest_hex\":null,"
            << "\"error\":{\"code\":\"" << errorName(code)
            << "\",\"cuda_error\":\"" << cudaErrorName(cuda_error)
            << "\",\"exit_code\":" << exitCode(code) << "}}\n";
}

bool decodeHeader(std::string_view text, std::uint8_t output[80]) {
  if (text.size() != 160) return false;
  const auto nibble = [](char value) -> int {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
  };
  for (std::size_t index = 0; index < 80; ++index) {
    const int high = nibble(text[2 * index]);
    const int low = nibble(text[2 * index + 1]);
    if (high < 0 || low < 0) return false;
    output[index] = static_cast<std::uint8_t>((high << 4) | low);
  }
  return true;
}

bool parseMode(std::string_view text, std::string_view &mode) {
  if (text == "header" || text == "matrix-seed") {
    mode = text;
    return true;
  }
  return false;
}

std::string hexDigest(const std::uint8_t digest[32]) {
  static constexpr char digits[] = "0123456789abcdef";
  std::string output(64, '0');
  for (std::size_t index = 0; index < 32; ++index) {
    output[2 * index] = digits[digest[index] >> 4U];
    output[2 * index + 1] = digits[digest[index] & 0x0fU];
  }
  return output;
}

}  // namespace

int main(int argc, char **argv) {
  std::uint8_t host_input[80];
  std::string_view mode;
  if (argc != 5 || std::string_view(argv[1]) != "--header-hex" ||
      !decodeHeader(argv[2], host_input) ||
      std::string_view(argv[3]) != "--mode" || !parseMode(argv[4], mode)) {
    emitError(ErrorCode::invalid_arguments, cudaSuccess, "invalid");
    return exitCode(ErrorCode::invalid_arguments);
  }
  if (mode == "matrix-seed") {
    std::memset(host_input + 76, 0, 4);
  }

  int device_count = 0;
  cudaError_t error = cudaGetDeviceCount(&device_count);
  if (error != cudaSuccess) {
    const ErrorCode code = classify(error);
    emitError(code, error, mode);
    return exitCode(code);
  }
  if (device_count <= 0) {
    emitError(ErrorCode::no_device, cudaErrorNoDevice, mode);
    return exitCode(ErrorCode::no_device);
  }

  std::uint8_t *device_input = nullptr;
  std::uint8_t *device_output = nullptr;
  error = cudaMalloc(reinterpret_cast<void **>(&device_input), sizeof(host_input));
  if (error == cudaSuccess) {
    error = cudaMalloc(reinterpret_cast<void **>(&device_output), 32);
  }
  if (error == cudaSuccess) {
    error = cudaMemcpy(device_input, host_input, sizeof(host_input),
                       cudaMemcpyHostToDevice);
  }
  if (error == cudaSuccess) {
    hashOneHeader<<<1, 1>>>(device_input, device_output);
    error = cudaGetLastError();
  }
  if (error == cudaSuccess) {
    error = cudaDeviceSynchronize();
  }

  std::uint8_t host_digest[32]{};
  if (error == cudaSuccess) {
    error = cudaMemcpy(host_digest, device_output, sizeof(host_digest),
                       cudaMemcpyDeviceToHost);
  }
  const cudaError_t free_output = device_output == nullptr
                                      ? cudaSuccess
                                      : cudaFree(device_output);
  const cudaError_t free_input = device_input == nullptr
                                     ? cudaSuccess
                                     : cudaFree(device_input);
  if (error == cudaSuccess && free_output != cudaSuccess) error = free_output;
  if (error == cudaSuccess && free_input != cudaSuccess) error = free_input;
  if (error != cudaSuccess) {
    const ErrorCode code = classify(error);
    emitError(code, error, mode);
    return exitCode(code);
  }

  std::cout << "{\"schema_version\":1,\"status\":\"ok\","
            << "\"stage\":\"hoohash_v110_blake3_80\",\"input_mode\":\""
            << mode << "\",\"input_size_bytes\":80,\"digest_hex\":\""
            << hexDigest(host_digest) << "\",\"error\":null}\n";
  return 0;
}

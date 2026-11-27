#include <cuda_runtime.h>

#include "cuda_hoohash_fp64_diagnostic.cuh"

#if defined(PEPEPOW_CUDA_BOUNDED_SEARCH_ENGINE)
#define NOMINMAX
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <thread>
#include <limits>
#include "pepepow/uint256_target.h"
#endif
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

[[maybe_unused]] constexpr std::uint32_t kMaximumBatch = 10000;
constexpr char kOutputMagic[8] = {'P', 'P', 'W', 'D', 'I', 'G', '1', '\0'};

__device__ __constant__ std::uint32_t kBlake3Iv[8] = {
    0x6A09E667U, 0xBB67AE85U, 0x3C6EF372U, 0xA54FF53AU,
    0x510E527FU, 0x9B05688CU, 0x1F83D9ABU, 0x5BE0CD19U};
__device__ __constant__ std::uint8_t kBlake3Permutation[16] = {
    2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8};

enum class ErrorCode {
  no_device,
  driver_unavailable,
  initialization_failed,
  cuda_failure,
  output_failure,
  invalid_arguments,
};

__device__ __forceinline__ std::uint32_t rotateRight32(
    std::uint32_t value, unsigned int amount) {
  return (value >> amount) | (value << (32U - amount));
}

__device__ __forceinline__ std::uint32_t load32LittleEndian(
    const std::uint8_t *bytes) {
  return static_cast<std::uint32_t>(bytes[0]) |
         (static_cast<std::uint32_t>(bytes[1]) << 8U) |
         (static_cast<std::uint32_t>(bytes[2]) << 16U) |
         (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

__device__ __forceinline__ void blake3Mix(std::uint32_t state[16], int a,
                                          int b, int c, int d,
                                          std::uint32_t x,
                                          std::uint32_t y) {
  state[a] = state[a] + state[b] + x;
  state[d] = rotateRight32(state[d] ^ state[a], 16U);
  state[c] += state[d];
  state[b] = rotateRight32(state[b] ^ state[c], 12U);
  state[a] = state[a] + state[b] + y;
  state[d] = rotateRight32(state[d] ^ state[a], 8U);
  state[c] += state[d];
  state[b] = rotateRight32(state[b] ^ state[c], 7U);
}

__device__ void blake3Compress(const std::uint32_t chaining_value[8],
                               const std::uint8_t block[64],
                               std::uint32_t block_length,
                               std::uint32_t flags,
                               std::uint32_t output[16]) {
  std::uint32_t state[16];
  std::uint32_t message[16];
  for (int index = 0; index < 8; ++index) state[index] = chaining_value[index];
  for (int index = 0; index < 4; ++index) state[8 + index] = kBlake3Iv[index];
  state[12] = 0;
  state[13] = 0;
  state[14] = block_length;
  state[15] = flags;
  for (int index = 0; index < 16; ++index) {
    message[index] = load32LittleEndian(block + 4 * index);
  }

  for (int round = 0; round < 7; ++round) {
    blake3Mix(state, 0, 4, 8, 12, message[0], message[1]);
    blake3Mix(state, 1, 5, 9, 13, message[2], message[3]);
    blake3Mix(state, 2, 6, 10, 14, message[4], message[5]);
    blake3Mix(state, 3, 7, 11, 15, message[6], message[7]);
    blake3Mix(state, 0, 5, 10, 15, message[8], message[9]);
    blake3Mix(state, 1, 6, 11, 12, message[10], message[11]);
    blake3Mix(state, 2, 7, 8, 13, message[12], message[13]);
    blake3Mix(state, 3, 4, 9, 14, message[14], message[15]);
    if (round != 6) {
      std::uint32_t permuted[16];
      for (int index = 0; index < 16; ++index) {
        permuted[index] = message[kBlake3Permutation[index]];
      }
      for (int index = 0; index < 16; ++index) message[index] = permuted[index];
    }
  }

  for (int index = 0; index < 8; ++index) {
    output[index] = state[index] ^ state[index + 8];
    output[index + 8] = state[index + 8] ^ chaining_value[index];
  }
}

__device__ __forceinline__ void store32LittleEndian(
    std::uint32_t value, std::uint8_t *bytes) {
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
  for (int index = 0; index < 64; ++index) block[index] = input[index];
  for (int index = 0; index < 8; ++index) chaining_value[index] = kBlake3Iv[index];
  blake3Compress(chaining_value, block, 64U, 1U, compressed);
  for (int index = 0; index < 8; ++index) chaining_value[index] = compressed[index];
  for (int index = 0; index < 64; ++index) {
    block[index] = index < 16 ? input[64 + index] : 0U;
  }
  blake3Compress(chaining_value, block, 16U, 2U | 8U, compressed);
  for (int index = 0; index < 8; ++index) {
    store32LittleEndian(compressed[index], output + 4 * index);
  }
}

__device__ void blake3Hash32(const std::uint8_t input[32],
                             std::uint8_t output[32]) {
  std::uint8_t block[64]{};
  std::uint32_t compressed[16];
  for (int index = 0; index < 32; ++index) block[index] = input[index];
  blake3Compress(kBlake3Iv, block, 32U, 1U | 2U | 8U, compressed);
  for (int index = 0; index < 8; ++index) {
    store32LittleEndian(compressed[index], output + 4 * index);
  }
}

__device__ __forceinline__ std::uint32_t read32BigEndian(
    const std::uint8_t *bytes) {
  return (static_cast<std::uint32_t>(bytes[0]) << 24U) |
         (static_cast<std::uint32_t>(bytes[1]) << 16U) |
         (static_cast<std::uint32_t>(bytes[2]) << 8U) |
         static_cast<std::uint32_t>(bytes[3]);
}

__device__ __forceinline__ std::uint64_t read64LittleEndian(
    const std::uint8_t *bytes) {
  std::uint64_t value = 0;
  for (unsigned int index = 0; index < 8; ++index) {
    value |= static_cast<std::uint64_t>(bytes[index]) << (index * 8U);
  }
  return value;
}

__device__ __forceinline__ std::uint64_t rotateLeft64(
    std::uint64_t value, unsigned int amount) {
  return (value << amount) | (value >> (64U - amount));
}

__global__ void prepareHashInputs(const std::uint8_t *header,
                                 std::uint32_t nonce_start,
                                 std::uint32_t count,
                                 std::uint8_t *first_passes,
                                 std::uint8_t *matrix_seed) {
  const std::uint32_t index = blockIdx.x * blockDim.x + threadIdx.x;
  if (index >= count) return;

  std::uint8_t candidate[80];
  for (int byte = 0; byte < 80; ++byte) candidate[byte] = header[byte];
  const std::uint32_t nonce = nonce_start + index;
  candidate[76] = static_cast<std::uint8_t>(nonce);
  candidate[77] = static_cast<std::uint8_t>(nonce >> 8U);
  candidate[78] = static_cast<std::uint8_t>(nonce >> 16U);
  candidate[79] = static_cast<std::uint8_t>(nonce >> 24U);
  blake3Hash80(candidate, first_passes + static_cast<std::size_t>(index) * 32U);

  if (index == 0U) {
    candidate[76] = 0U;
    candidate[77] = 0U;
    candidate[78] = 0U;
    candidate[79] = 0U;
    blake3Hash80(candidate, matrix_seed);
  }
}

__global__ void generateMatrix(const std::uint8_t *matrix_seed,
                               double *matrix) {
  if (blockIdx.x != 0U || threadIdx.x != 0U) return;
  std::uint64_t s0 = read64LittleEndian(matrix_seed);
  std::uint64_t s1 = read64LittleEndian(matrix_seed + 8);
  std::uint64_t s2 = read64LittleEndian(matrix_seed + 16);
  std::uint64_t s3 = read64LittleEndian(matrix_seed + 24);
  for (std::uint32_t element = 0; element < 64U * 64U; ++element) {
    const std::uint64_t value = rotateLeft64(s0 + s3, 23U) + s0;
    const std::uint64_t transition = s1 << 17U;
    s2 ^= s0;
    s3 ^= s1;
    s1 ^= s2;
    s0 ^= s3;
    s2 ^= transition;
    s3 = rotateLeft64(s3, 45U);
    const std::uint32_t lower = static_cast<std::uint32_t>(value);
    const double normalized = __ddiv_rn(static_cast<double>(lower),
                                        static_cast<double>(0xffffffffU));
    matrix[element] = normalized * 1000000.0;
  }
}

__global__ void calculateDigests(const std::uint8_t *first_passes,
                                 const double *matrix,
                                 std::uint32_t nonce_start,
                                 std::uint32_t count,
                                 std::uint8_t *digests) {
  const std::uint32_t index = blockIdx.x * blockDim.x + threadIdx.x;
  if (index >= count) return;
  const std::uint8_t *first_pass = first_passes +
      static_cast<std::size_t>(index) * 32U;

  std::uint32_t hash_xor = 0U;
  for (unsigned int word = 0; word < 8; ++word) {
    hash_xor ^= read32BigEndian(first_pass + word * 4U);
  }
  std::uint8_t vector[64];
  for (unsigned int byte = 0; byte < 32; ++byte) {
    vector[2U * byte] = static_cast<std::uint8_t>(first_pass[byte] >> 4U);
    vector[2U * byte + 1U] = static_cast<std::uint8_t>(first_pass[byte] & 0x0fU);
  }

  const std::uint32_t nonce = nonce_start + index;
  const double nonce_mod = static_cast<double>(nonce & 0xffU);
  double sw = 0.0;
  std::uint64_t even_product = 0U;
  std::uint8_t scaled_values[32];
  for (unsigned int row = 0; row < 64U; ++row) {
    double product = 0.0;
    for (unsigned int column = 0; column < 64U; ++column) {
      const double matrix_value = matrix[row * 64U + column];
      const double vector_value = static_cast<double>(vector[column]);
      double output_value = 0.0;
      if (sw <= 0.02) {
        const double input = matrix_value * static_cast<double>(hash_xor) *
                            vector_value + nonce_mod;
        output_value = pepepow_cuda_v110::safeComplexTransform(input) *
                       vector_value * 1234.0;
      } else {
        output_value = matrix_value * 0.0001 * vector_value;
      }
      product += output_value;
      sw = pepepow_cuda_v110::transformFactor(product);
    }
    if ((row & 1U) == 0U) {
      even_product = __double2ull_rz(product);
    } else {
      const std::uint64_t odd_product = __double2ull_rz(product);
      scaled_values[row / 2U] = static_cast<std::uint8_t>(
          (even_product + odd_product) & 0xffU);
    }
  }

  std::uint8_t result[32];
  for (unsigned int byte = 0; byte < 32U; ++byte) {
    result[byte] = static_cast<std::uint8_t>(first_pass[byte] ^ scaled_values[byte]);
  }
  std::uint8_t digest[32];
  blake3Hash32(result, digest);
  std::uint8_t *candidate_digest = digests + static_cast<std::size_t>(index) * 32U;
  for (unsigned int byte = 0; byte < 32U; ++byte) {
    candidate_digest[byte] = digest[31U - byte];
  }
}

#if defined(PEPEPOW_CUDA_BOUNDED_SEARCH_ENGINE)
__global__ void collectTargetCandidates(const std::uint8_t *digests,
                                        std::uint8_t *target,
                                        std::uint32_t nonce_start,
                                        std::uint32_t count,
                                        std::uint32_t *candidate_count,
                                        std::uint32_t *candidate_nonces,
                                        std::uint8_t *candidate_digests) {
  const std::uint32_t index = blockIdx.x * blockDim.x + threadIdx.x;
  if (index >= count) return;
  const std::uint8_t *digest = digests + static_cast<std::size_t>(index) * 32U;
  bool meets_target = true;
  // Core's uint256 target helpers interpret the hash and target as
  // little-endian integers, comparing the most-significant byte first.
  for (int byte = 31; byte >= 0; --byte) {
    if (digest[byte] < target[byte]) break;
    if (digest[byte] > target[byte]) {
      meets_target = false;
      break;
    }
  }
  if (!meets_target) return;

  const std::uint32_t slot = atomicAdd(candidate_count, 1U);
  candidate_nonces[slot] = nonce_start + index;
  std::uint8_t *output = candidate_digests + static_cast<std::size_t>(slot) * 32U;
  for (unsigned int byte = 0; byte < 32U; ++byte) output[byte] = digest[byte];
}
#endif

struct KernelResources {
  int threads_per_block = 0;
  int grid_blocks = 0;
  int registers_per_thread = 0;
  std::size_t local_memory_bytes_per_thread = 0;
  std::size_t static_shared_memory_bytes_per_block = 0;
  int warps_per_block = 0;
  int theoretical_max_active_blocks_per_sm = 0;
  int theoretical_active_warps_per_sm = 0;
  int max_warps_per_sm = 0;
  double theoretical_warp_occupancy_percent = 0.0;
};

template <typename Kernel>
cudaError_t queryKernelResources(Kernel kernel, int threads_per_block,
                                 int grid_blocks,
                                 const cudaDeviceProp &properties,
                                 KernelResources &resources) {
  if (threads_per_block <= 0 || grid_blocks <= 0 ||
      properties.warpSize <= 0 ||
      properties.maxThreadsPerMultiProcessor <= 0) {
    return cudaErrorInvalidConfiguration;
  }
  cudaFuncAttributes attributes{};
  cudaError_t error = cudaFuncGetAttributes(&attributes, kernel);
  if (error != cudaSuccess) return error;
  int max_active_blocks = 0;
  error = cudaOccupancyMaxActiveBlocksPerMultiprocessor(
      &max_active_blocks, kernel, threads_per_block, 0U);
  if (error != cudaSuccess) return error;

  const int max_warps = properties.maxThreadsPerMultiProcessor /
                        properties.warpSize;
  const int warps_per_block =
      (threads_per_block + properties.warpSize - 1) / properties.warpSize;
  const int theoretical_active_warps =
      max_active_blocks * warps_per_block < max_warps
          ? max_active_blocks * warps_per_block : max_warps;
  resources.threads_per_block = threads_per_block;
  resources.grid_blocks = grid_blocks;
  resources.registers_per_thread = attributes.numRegs;
  resources.local_memory_bytes_per_thread = attributes.localSizeBytes;
  resources.static_shared_memory_bytes_per_block = attributes.sharedSizeBytes;
  resources.warps_per_block = warps_per_block;
  resources.theoretical_max_active_blocks_per_sm = max_active_blocks;
  resources.theoretical_active_warps_per_sm = theoretical_active_warps;
  resources.max_warps_per_sm = max_warps;
  resources.theoretical_warp_occupancy_percent =
      100.0 * static_cast<double>(theoretical_active_warps) /
      static_cast<double>(max_warps);
  return cudaSuccess;
}

void writeKernelResources(std::ostream &output,
                          const KernelResources &resources) {
  output << "{\"threads_per_block\":" << resources.threads_per_block
         << ",\"grid_blocks\":" << resources.grid_blocks
         << ",\"registers_per_thread\":" << resources.registers_per_thread
         << ",\"local_memory_bytes_per_thread\":"
         << resources.local_memory_bytes_per_thread
         << ",\"static_shared_memory_bytes_per_block\":"
         << resources.static_shared_memory_bytes_per_block
         << ",\"warps_per_block\":" << resources.warps_per_block
         << ",\"theoretical_max_active_blocks_per_sm\":"
         << resources.theoretical_max_active_blocks_per_sm
         << ",\"theoretical_active_warps_per_sm\":"
         << resources.theoretical_active_warps_per_sm
         << ",\"max_warps_per_sm\":" << resources.max_warps_per_sm
         << ",\"theoretical_warp_occupancy_percent\":"
         << resources.theoretical_warp_occupancy_percent << '}';
}

const char *errorName(ErrorCode error) {
  switch (error) {
    case ErrorCode::no_device: return "no_device";
    case ErrorCode::driver_unavailable: return "driver_unavailable";
    case ErrorCode::initialization_failed: return "initialization_failed";
    case ErrorCode::cuda_failure: return "cuda_failure";
    case ErrorCode::output_failure: return "output_failure";
    case ErrorCode::invalid_arguments: return "invalid_arguments";
  }
  return "cuda_failure";
}

int exitCode(ErrorCode error) {
  switch (error) {
    case ErrorCode::no_device: return 2;
    case ErrorCode::driver_unavailable: return 3;
    case ErrorCode::initialization_failed: return 4;
    case ErrorCode::cuda_failure: return 5;
    case ErrorCode::output_failure: return 6;
    case ErrorCode::invalid_arguments: return 64;
  }
  return 5;
}

ErrorCode classify(cudaError_t error) {
  switch (error) {
    case cudaErrorNoDevice: return ErrorCode::no_device;
    case cudaErrorInsufficientDriver: return ErrorCode::driver_unavailable;
    case cudaErrorInitializationError: return ErrorCode::initialization_failed;
    default: return ErrorCode::cuda_failure;
  }
}

const char *cudaErrorName(cudaError_t error) {
  switch (error) {
    case cudaSuccess: return "cudaSuccess";
    case cudaErrorNoDevice: return "cudaErrorNoDevice";
    case cudaErrorInsufficientDriver: return "cudaErrorInsufficientDriver";
    case cudaErrorInitializationError: return "cudaErrorInitializationError";
    case cudaErrorMemoryAllocation: return "cudaErrorMemoryAllocation";
    default: return "cudaErrorOther";
  }
}

void emitError(ErrorCode code, cudaError_t cuda_error) {
  std::cout << "{\"schema_version\":1,\"status\":\"error\","
            << "\"stage\":\"hoohash_v110_full_digest_experiment\","
            << "\"nonce_start\":null,\"nonce_count\":null,"
            << "\"error\":{\"code\":\"" << errorName(code)
            << "\",\"cuda_error\":\"" << cudaErrorName(cuda_error)
            << "\",\"exit_code\":" << exitCode(code) << "}}\n";
}

bool decodeHeader(std::string_view text, std::uint8_t output[80]) {
  if (text.size() != 160U) return false;
  auto nibble = [](char value) -> int {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
  };
  for (std::size_t index = 0; index < 80U; ++index) {
    const int high = nibble(text[index * 2U]);
    const int low = nibble(text[index * 2U + 1U]);
    if (high < 0 || low < 0) return false;
    output[index] = static_cast<std::uint8_t>((high << 4) | low);
  }
  return true;
}

bool parseUint32(std::string_view text, std::uint32_t &value) {
  if (text.empty()) return false;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

void writeLittleEndian32(std::ostream &output, std::uint32_t value) {
  const char bytes[4] = {static_cast<char>(value),
                         static_cast<char>(value >> 8U),
                         static_cast<char>(value >> 16U),
                         static_cast<char>(value >> 24U)};
  output.write(bytes, sizeof(bytes));
}

std::string jsonEscape(std::string_view value) {
  std::string escaped;
  for (const unsigned char character : value) {
    if (character == '"' || character == '\\') {
      escaped.push_back('\\');
      escaped.push_back(static_cast<char>(character));
    } else if (character < 0x20U) {
      constexpr char digits[] = "0123456789abcdef";
      escaped.append("\\u00");
      escaped.push_back(digits[character >> 4U]);
      escaped.push_back(digits[character & 0x0fU]);
    } else {
      escaped.push_back(static_cast<char>(character));
    }
  }
  return escaped;
}

bool writeTranscript(const std::string &path, std::uint32_t count,
                     const std::vector<std::uint8_t> &digests) {
  std::ofstream output(path, std::ios::binary);
  if (!output.write(kOutputMagic, sizeof(kOutputMagic))) return false;
  writeLittleEndian32(output, count);
  output.write(reinterpret_cast<const char *>(digests.data()),
               static_cast<std::streamsize>(digests.size()));
  return output.good();
}

[[maybe_unused]] int run(const std::uint8_t header[80], std::uint32_t nonce_start,
                         std::uint32_t count, const std::string &output_path) {
  int device_count = 0;
  cudaError_t error = cudaGetDeviceCount(&device_count);
  if (error != cudaSuccess) {
    const ErrorCode code = classify(error);
    emitError(code, error);
    return exitCode(code);
  }
  if (device_count <= 0) {
    emitError(ErrorCode::no_device, cudaErrorNoDevice);
    return exitCode(ErrorCode::no_device);
  }

  std::uint8_t *device_header = nullptr;
  std::uint8_t *device_first_passes = nullptr;
  std::uint8_t *device_matrix_seed = nullptr;
  double *device_matrix = nullptr;
  std::uint8_t *device_digests = nullptr;
  cudaEvent_t pipeline_begin = nullptr;
  cudaEvent_t prepare_end = nullptr;
  cudaEvent_t matrix_end = nullptr;
  cudaEvent_t pipeline_end = nullptr;
  float pipeline_elapsed_ms = 0.0F;
  float prepare_elapsed_ms = 0.0F;
  float matrix_elapsed_ms = 0.0F;
  float digest_elapsed_ms = 0.0F;
  const std::size_t digest_bytes = static_cast<std::size_t>(count) * 32U;
  error = cudaMalloc(reinterpret_cast<void **>(&device_header), 80U);
  if (error == cudaSuccess) {
    error = cudaMalloc(reinterpret_cast<void **>(&device_first_passes), digest_bytes);
  }
  if (error == cudaSuccess) {
    error = cudaMalloc(reinterpret_cast<void **>(&device_matrix_seed), 32U);
  }
  if (error == cudaSuccess) {
    error = cudaMalloc(reinterpret_cast<void **>(&device_matrix), 4096U * sizeof(double));
  }
  if (error == cudaSuccess) {
    error = cudaMalloc(reinterpret_cast<void **>(&device_digests), digest_bytes);
  }
  if (error == cudaSuccess) {
    error = cudaMemcpy(device_header, header, 80U, cudaMemcpyHostToDevice);
  }
  if (error == cudaSuccess) error = cudaEventCreate(&pipeline_begin);
  if (error == cudaSuccess) error = cudaEventCreate(&prepare_end);
  if (error == cudaSuccess) error = cudaEventCreate(&matrix_end);
  if (error == cudaSuccess) error = cudaEventCreate(&pipeline_end);
  if (error == cudaSuccess) error = cudaEventRecord(pipeline_begin);

  if (error == cudaSuccess) {
    constexpr unsigned int threads = 128U;
    const unsigned int blocks = (count + threads - 1U) / threads;
    prepareHashInputs<<<blocks, threads>>>(device_header, nonce_start, count,
                                           device_first_passes,
                                           device_matrix_seed);
    error = cudaGetLastError();
    if (error == cudaSuccess) error = cudaEventRecord(prepare_end);
  }
  if (error == cudaSuccess) {
    generateMatrix<<<1, 1>>>(device_matrix_seed, device_matrix);
    error = cudaGetLastError();
    if (error == cudaSuccess) error = cudaEventRecord(matrix_end);
  }
  if (error == cudaSuccess) {
    constexpr unsigned int threads = 128U;
    const unsigned int blocks = (count + threads - 1U) / threads;
    calculateDigests<<<blocks, threads>>>(device_first_passes, device_matrix,
                                          nonce_start, count, device_digests);
    error = cudaGetLastError();
  }
  if (error == cudaSuccess) error = cudaEventRecord(pipeline_end);
  if (error == cudaSuccess) error = cudaEventSynchronize(pipeline_end);
  if (error == cudaSuccess) {
    error = cudaEventElapsedTime(&pipeline_elapsed_ms,
                                 pipeline_begin, pipeline_end);
  }
  if (error == cudaSuccess) {
    error = cudaEventElapsedTime(&prepare_elapsed_ms, pipeline_begin,
                                 prepare_end);
  }
  if (error == cudaSuccess) {
    error = cudaEventElapsedTime(&matrix_elapsed_ms, prepare_end, matrix_end);
  }
  if (error == cudaSuccess) {
    error = cudaEventElapsedTime(&digest_elapsed_ms, matrix_end, pipeline_end);
  }

  std::vector<std::uint8_t> host_digests(digest_bytes);
  if (error == cudaSuccess) {
    error = cudaMemcpy(host_digests.data(), device_digests, digest_bytes,
                       cudaMemcpyDeviceToHost);
  }
  const cudaError_t free_digests = device_digests == nullptr
      ? cudaSuccess : cudaFree(device_digests);
  const cudaError_t free_matrix = device_matrix == nullptr
      ? cudaSuccess : cudaFree(device_matrix);
  const cudaError_t free_seed = device_matrix_seed == nullptr
      ? cudaSuccess : cudaFree(device_matrix_seed);
  const cudaError_t free_first_passes = device_first_passes == nullptr
      ? cudaSuccess : cudaFree(device_first_passes);
  const cudaError_t free_header = device_header == nullptr
      ? cudaSuccess : cudaFree(device_header);
  const cudaError_t destroy_pipeline_end = pipeline_end == nullptr
      ? cudaSuccess : cudaEventDestroy(pipeline_end);
  const cudaError_t destroy_matrix_end = matrix_end == nullptr
      ? cudaSuccess : cudaEventDestroy(matrix_end);
  const cudaError_t destroy_prepare_end = prepare_end == nullptr
      ? cudaSuccess : cudaEventDestroy(prepare_end);
  const cudaError_t destroy_pipeline_begin = pipeline_begin == nullptr
      ? cudaSuccess : cudaEventDestroy(pipeline_begin);
  if (error == cudaSuccess && free_digests != cudaSuccess) error = free_digests;
  if (error == cudaSuccess && free_matrix != cudaSuccess) error = free_matrix;
  if (error == cudaSuccess && free_seed != cudaSuccess) error = free_seed;
  if (error == cudaSuccess && free_first_passes != cudaSuccess) error = free_first_passes;
  if (error == cudaSuccess && free_header != cudaSuccess) error = free_header;
  if (error == cudaSuccess && destroy_pipeline_end != cudaSuccess) {
    error = destroy_pipeline_end;
  }
  if (error == cudaSuccess && destroy_matrix_end != cudaSuccess) {
    error = destroy_matrix_end;
  }
  if (error == cudaSuccess && destroy_prepare_end != cudaSuccess) {
    error = destroy_prepare_end;
  }
  if (error == cudaSuccess && destroy_pipeline_begin != cudaSuccess) {
    error = destroy_pipeline_begin;
  }
  if (error != cudaSuccess) {
    const ErrorCode code = classify(error);
    emitError(code, error);
    return exitCode(code);
  }
  if (!(pipeline_elapsed_ms > 0.0F) || !std::isfinite(pipeline_elapsed_ms)) {
    emitError(ErrorCode::cuda_failure, cudaErrorUnknown);
    return exitCode(ErrorCode::cuda_failure);
  }
  if (prepare_elapsed_ms < 0.0F || !std::isfinite(prepare_elapsed_ms) ||
      matrix_elapsed_ms < 0.0F || !std::isfinite(matrix_elapsed_ms) ||
      digest_elapsed_ms < 0.0F || !std::isfinite(digest_elapsed_ms)) {
    emitError(ErrorCode::cuda_failure, cudaErrorUnknown);
    return exitCode(ErrorCode::cuda_failure);
  }

  if (!writeTranscript(output_path, count, host_digests)) {
    emitError(ErrorCode::output_failure, cudaSuccess);
    return exitCode(ErrorCode::output_failure);
  }

  cudaDeviceProp properties{};
  error = cudaGetDeviceProperties(&properties, 0);
  if (error != cudaSuccess) {
    const ErrorCode code = classify(error);
    emitError(code, error);
    return exitCode(code);
  }
  constexpr int digest_threads = 128;
  const int digest_blocks = static_cast<int>(
      (count + static_cast<std::uint32_t>(digest_threads) - 1U) /
      static_cast<std::uint32_t>(digest_threads));
  KernelResources prepare_resources{};
  KernelResources matrix_resources{};
  KernelResources digest_resources{};
  error = queryKernelResources(prepareHashInputs, digest_threads,
                               digest_blocks, properties, prepare_resources);
  if (error == cudaSuccess) {
    error = queryKernelResources(generateMatrix, 1, 1, properties,
                                 matrix_resources);
  }
  if (error == cudaSuccess) {
    error = queryKernelResources(calculateDigests, digest_threads,
                                 digest_blocks, properties, digest_resources);
  }
  if (error != cudaSuccess) {
    const ErrorCode code = classify(error);
    emitError(code, error);
    return exitCode(code);
  }
  int driver_version = 0;
  int runtime_version = 0;
  error = cudaDriverGetVersion(&driver_version);
  if (error == cudaSuccess) error = cudaRuntimeGetVersion(&runtime_version);
  if (error != cudaSuccess) {
    const ErrorCode code = classify(error);
    emitError(code, error);
    return exitCode(code);
  }
  std::cout << "{\"schema_version\":1,\"status\":\"ok\","
            << std::setprecision(17)
            << "\"stage\":\"hoohash_v110_full_digest_experiment\","
            << "\"nonce_start\":" << nonce_start
            << ",\"nonce_count\":" << count
            << ",\"digest_count\":" << count
            << ",\"gpu_pipeline_ms\":" << pipeline_elapsed_ms
            << ",\"gpu_pipeline_digests_per_second\":"
            << (static_cast<double>(count) * 1000.0 /
                static_cast<double>(pipeline_elapsed_ms))
            << ",\"gpu_pipeline_timing_scope\":\"CUDA event intervals bracket "
               "the three ordered kernels; exclude host setup, allocation, "
               "transfer, oracle, and output; intervals may include device idle "
               "time while the host queues launches; not miner hashrate\""
            << ",\"kernel_event_intervals_ms\":{"
            << "\"prepare_hash_inputs\":" << prepare_elapsed_ms
            << ",\"matrix_generation\":" << matrix_elapsed_ms
            << ",\"digest_calculation\":" << digest_elapsed_ms << '}'
            << ",\"kernel_resource_model\":\"CUDA Runtime function attributes "
               "and max-active-block calculation; theoretical per-SM occupancy, "
               "not measured achieved occupancy\""
            << ",\"warp_size\":" << properties.warpSize
            << ",\"kernel_resources\":{\"prepareHashInputs\":";
  writeKernelResources(std::cout, prepare_resources);
  std::cout << ",\"generateMatrix\":";
  writeKernelResources(std::cout, matrix_resources);
  std::cout << ",\"calculateDigests\":";
  writeKernelResources(std::cout, digest_resources);
  std::cout << '}'
            << ",\"device\":{\"index\":0,\"name\":\""
            << jsonEscape(properties.name) << "\",\"compute_capability\":\""
            << properties.major << '.' << properties.minor << "\"},"
            << "\"cuda_versions\":{\"driver_api\":" << driver_version
            << ",\"runtime_api\":" << runtime_version
            << ",\"toolkit_build\":\"" << PEPEPOW_CUDA_TOOLKIT_VERSION
            << "\"},\"error\":null}\n";
  return 0;
}

#if defined(PEPEPOW_CUDA_BOUNDED_SEARCH_ENGINE)
constexpr std::uint32_t kMaximumSearchBatch = 256U;
constexpr std::uint64_t kNonceSpaceSize = std::uint64_t{1} << 32U;
constexpr std::uint32_t kMaximumSearchTimeoutMs = 60000U;
volatile std::sig_atomic_t g_search_cancel_requested = 0;
std::string g_search_cancel_file;

void requestSearchCancellation(int) { g_search_cancel_requested = 1; }

bool searchCancellationRequested() {
  if (g_search_cancel_requested != 0) return true;
  if (g_search_cancel_file.empty()) return false;
  std::error_code error;
  const bool exists = std::filesystem::exists(g_search_cancel_file, error);
  return error || exists;
}

const char *searchCancellationReason() {
  return g_search_cancel_requested != 0 ? "signal" : "cancel_file";
}

std::uint32_t readLittleEndian32Host(const std::uint8_t *data) {
  return static_cast<std::uint32_t>(data[0]) |
         (static_cast<std::uint32_t>(data[1]) << 8U) |
         (static_cast<std::uint32_t>(data[2]) << 16U) |
         (static_cast<std::uint32_t>(data[3]) << 24U);
}

bool decodeTarget(std::string_view text, std::uint8_t output[32]) {
  if (text.size() != 64U) return false;
  auto nibble = [](char value) -> int {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
  };
  for (std::size_t index = 0; index < 32U; ++index) {
    const int high = nibble(text[index * 2U]);
    const int low = nibble(text[index * 2U + 1U]);
    if (high < 0 || low < 0) return false;
    output[index] = static_cast<std::uint8_t>((high << 4) | low);
  }
  return true;
}

bool parseUint64(std::string_view text, std::uint64_t &value) {
  if (text.empty()) return false;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

std::string encodeHex(const std::uint8_t *data, std::size_t size) {
  constexpr char digits[] = "0123456789abcdef";
  std::string output(size * 2U, '0');
  for (std::size_t index = 0; index < size; ++index) {
    output[index * 2U] = digits[data[index] >> 4U];
    output[index * 2U + 1U] = digits[data[index] & 0x0fU];
  }
  return output;
}

enum class SearchError {
  no_device,
  driver_unavailable,
  initialization_failed,
  cuda_failure,
  verifier_unavailable,
  gpu_digest_mismatch,
  cpu_target_reject,
  candidate_contract_failure,
  invalid_arguments,
};

const char *searchErrorName(SearchError error) {
  switch (error) {
    case SearchError::no_device: return "no_device";
    case SearchError::driver_unavailable: return "driver_unavailable";
    case SearchError::initialization_failed: return "initialization_failed";
    case SearchError::cuda_failure: return "cuda_failure";
    case SearchError::verifier_unavailable: return "verifier_unavailable";
    case SearchError::gpu_digest_mismatch: return "gpu_digest_mismatch";
    case SearchError::cpu_target_reject: return "cpu_target_reject";
    case SearchError::candidate_contract_failure: return "candidate_contract_failure";
    case SearchError::invalid_arguments: return "invalid_arguments";
  }
  return "cuda_failure";
}

int searchErrorExitCode(SearchError error) {
  switch (error) {
    case SearchError::no_device: return 2;
    case SearchError::driver_unavailable: return 3;
    case SearchError::initialization_failed: return 4;
    case SearchError::cuda_failure: return 5;
    case SearchError::verifier_unavailable: return 6;
    case SearchError::gpu_digest_mismatch: return 7;
    case SearchError::cpu_target_reject: return 8;
    case SearchError::candidate_contract_failure: return 9;
    case SearchError::invalid_arguments: return 64;
  }
  return 5;
}

SearchError classifySearchCudaError(cudaError_t error) {
  switch (error) {
    case cudaErrorNoDevice: return SearchError::no_device;
    case cudaErrorInsufficientDriver: return SearchError::driver_unavailable;
    case cudaErrorInitializationError: return SearchError::initialization_failed;
    default: return SearchError::cuda_failure;
  }
}

void emitSearchError(SearchError code, cudaError_t cuda_error,
                     std::uint64_t nonce_start, std::uint64_t nonce_count,
                     std::uint64_t processed_count,
                     std::uint32_t failed_nonce = 0U,
                     std::string_view gpu_digest = {},
                     std::string_view cpu_digest = {}) {
  std::cout << "{\"schema_version\":1,\"status\":\"error\","
            << "\"engine\":\"cuda_hoohash_v110_bounded_search\","
            << "\"nonce_start\":" << nonce_start
            << ",\"nonce_count\":" << nonce_count
            << ",\"processed_count\":" << processed_count
            << ",\"error\":{\"code\":\"" << searchErrorName(code)
            << "\",\"cuda_error\":\"" << cudaErrorName(cuda_error)
            << "\",\"exit_code\":" << searchErrorExitCode(code);
  if (code == SearchError::gpu_digest_mismatch ||
      code == SearchError::cpu_target_reject) {
    std::cout << ",\"nonce\":" << failed_nonce
              << ",\"gpu_digest_hex\":\"" << gpu_digest
              << "\",\"cpu_digest_hex\":\"" << cpu_digest << '"';
  }
  std::cout << "}}\n";
}

struct SearchDeviceBuffers {
  std::uint8_t *header = nullptr;
  std::uint8_t *target = nullptr;
  std::uint8_t *first_passes = nullptr;
  std::uint8_t *matrix_seed = nullptr;
  double *matrix = nullptr;
  std::uint8_t *digests = nullptr;
  std::uint32_t *candidate_count = nullptr;
  std::uint32_t *candidate_nonces = nullptr;
  std::uint8_t *candidate_digests = nullptr;

  cudaError_t allocate(std::uint32_t batch_capacity) {
    const std::size_t digest_bytes = static_cast<std::size_t>(batch_capacity) * 32U;
    cudaError_t error = cudaMalloc(reinterpret_cast<void **>(&header), 80U);
    if (error == cudaSuccess) error = cudaMalloc(reinterpret_cast<void **>(&target), 32U);
    if (error == cudaSuccess) {
      error = cudaMalloc(reinterpret_cast<void **>(&first_passes), digest_bytes);
    }
    if (error == cudaSuccess) {
      error = cudaMalloc(reinterpret_cast<void **>(&matrix_seed), 32U);
    }
    if (error == cudaSuccess) {
      error = cudaMalloc(reinterpret_cast<void **>(&matrix), 4096U * sizeof(double));
    }
    if (error == cudaSuccess) error = cudaMalloc(reinterpret_cast<void **>(&digests), digest_bytes);
    if (error == cudaSuccess) {
      error = cudaMalloc(reinterpret_cast<void **>(&candidate_count), sizeof(std::uint32_t));
    }
    if (error == cudaSuccess) {
      error = cudaMalloc(reinterpret_cast<void **>(&candidate_nonces),
                         static_cast<std::size_t>(batch_capacity) * sizeof(std::uint32_t));
    }
    if (error == cudaSuccess) {
      error = cudaMalloc(reinterpret_cast<void **>(&candidate_digests), digest_bytes);
    }
    return error;
  }

  cudaError_t release() {
    cudaError_t first_error = cudaSuccess;
    auto free_buffer = [&first_error](auto *&buffer) {
      if (buffer == nullptr) return;
      const cudaError_t error = cudaFree(buffer);
      if (first_error == cudaSuccess && error != cudaSuccess) first_error = error;
      buffer = nullptr;
    };
    free_buffer(candidate_digests);
    free_buffer(candidate_nonces);
    free_buffer(candidate_count);
    free_buffer(digests);
    free_buffer(matrix);
    free_buffer(matrix_seed);
    free_buffer(first_passes);
    free_buffer(target);
    free_buffer(header);
    return first_error;
  }

  ~SearchDeviceBuffers() { (void)release(); }
};

struct SearchCandidate {
  std::uint32_t nonce = 0;
  std::array<std::uint8_t, 32> digest{};
};

class TemporaryTranscript {
 public:
  TemporaryTranscript() {
#ifdef _WIN32
    char directory[MAX_PATH + 1U]{};
    const DWORD length = GetTempPathA(static_cast<DWORD>(sizeof(directory)), directory);
    if (length == 0U || length >= sizeof(directory)) return;
    if (GetTempFileNameA(directory, "ppw", 0U, path_) == 0U) path_[0] = '\0';
#else
    char pattern[] = "/tmp/pepepow-cuda-search-XXXXXX";
    const int descriptor = mkstemp(pattern);
    if (descriptor < 0) return;
    close(descriptor);
    path_ = pattern;
#endif
  }

  TemporaryTranscript(const TemporaryTranscript &) = delete;
  TemporaryTranscript &operator=(const TemporaryTranscript &) = delete;

  ~TemporaryTranscript() {
#ifdef _WIN32
    if (path_[0] != '\0') DeleteFileA(path_);
#else
    if (!path_.empty()) std::remove(path_.c_str());
#endif
  }

  bool valid() const {
#ifdef _WIN32
    return path_[0] != '\0';
#else
    return !path_.empty();
#endif
  }

  const char *path() const {
#ifdef _WIN32
    return path_;
#else
    return path_.c_str();
#endif
  }

 private:
#ifdef _WIN32
  char path_[MAX_PATH + 1U]{};
#else
  std::string path_;
#endif
};

std::string quoteWindowsArgument(std::string_view argument) {
  std::string quoted("\"");
  std::size_t backslashes = 0U;
  for (const char character : argument) {
    if (character == '\\') {
      ++backslashes;
      continue;
    }
    if (character == '"') {
      quoted.append(backslashes * 2U + 1U, '\\');
      quoted.push_back('"');
      backslashes = 0U;
      continue;
    }
    quoted.append(backslashes, '\\');
    backslashes = 0U;
    quoted.push_back(character);
  }
  quoted.append(backslashes * 2U, '\\');
  quoted.push_back('"');
  return quoted;
}

bool runCoreOracle(const std::string &oracle_path, const std::string &header_hex,
                   std::uint32_t nonce_start, std::uint32_t nonce_count,
                   const char *output_path) {
  const std::string start_text = std::to_string(nonce_start);
  const std::string count_text = std::to_string(nonce_count);
#ifdef _WIN32
  const std::vector<std::string> arguments = {
      oracle_path, "--header-hex", header_hex, "--nonce-start", start_text,
      "--nonce-count", count_text, "--output", output_path};
  std::string command_line;
  for (const auto &argument : arguments) {
    if (!command_line.empty()) command_line.push_back(' ');
    command_line.append(quoteWindowsArgument(argument));
  }
  HANDLE null_output = CreateFileA("NUL", GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                  nullptr);
  if (null_output == INVALID_HANDLE_VALUE) return false;
  STARTUPINFOA startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  startup.hStdOutput = null_output;
  startup.hStdError = null_output;
  PROCESS_INFORMATION process{};
  const BOOL created = CreateProcessA(
      oracle_path.c_str(), command_line.data(), nullptr, nullptr, TRUE,
      CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
  CloseHandle(null_output);
  if (!created) return false;
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::seconds(10);
  DWORD wait_result = WAIT_TIMEOUT;
  while (std::chrono::steady_clock::now() < deadline) {
    wait_result = WaitForSingleObject(process.hProcess, 10U);
    if (wait_result == WAIT_OBJECT_0 || wait_result == WAIT_FAILED) break;
    if (searchCancellationRequested()) break;
  }
  if (wait_result != WAIT_OBJECT_0) {
    TerminateProcess(process.hProcess, 124U);
    (void)WaitForSingleObject(process.hProcess, 1000U);
  }
  DWORD exit_code = 1U;
  const BOOL got_exit_code = GetExitCodeProcess(process.hProcess, &exit_code);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return wait_result == WAIT_OBJECT_0 && got_exit_code != FALSE && exit_code == 0U;
#else
  const std::vector<std::string> arguments = {
      oracle_path, "--header-hex", header_hex, "--nonce-start", start_text,
      "--nonce-count", count_text, "--output", output_path};
  std::vector<char *> argv;
  argv.reserve(arguments.size() + 1U);
  for (const auto &argument : arguments) {
    argv.push_back(const_cast<char *>(argument.c_str()));
  }
  argv.push_back(nullptr);
  const pid_t child = fork();
  if (child < 0) return false;
  if (child == 0) {
    const int null_output = open("/dev/null", O_WRONLY);
    if (null_output >= 0) {
      (void)dup2(null_output, STDOUT_FILENO);
      (void)dup2(null_output, STDERR_FILENO);
      if (null_output > STDERR_FILENO) close(null_output);
    }
    execv(oracle_path.c_str(), argv.data());
    _exit(127);
  }
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::seconds(10);
  int status = 0;
  while (std::chrono::steady_clock::now() < deadline) {
    const pid_t waited = waitpid(child, &status, WNOHANG);
    if (waited == child) return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (waited < 0 && errno != EINTR) return false;
    if (searchCancellationRequested()) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  (void)kill(child, SIGKILL);
  (void)waitpid(child, &status, 0);
  return false;
#endif
}

enum class RecheckStatus {
  ok,
  oracle_unavailable,
  transcript_invalid,
  digest_mismatch,
  target_reject,
};

class CoreOracleRechecker {
 public:
  explicit CoreOracleRechecker(std::string path) : path_(std::move(path)) {}

  bool available() const {
    std::error_code error;
    return std::filesystem::is_regular_file(path_, error) && !error;
  }

  RecheckStatus recheck(const std::uint8_t header[80],
                        const std::uint8_t target[32],
                        const std::vector<SearchCandidate> &candidates,
                        std::uint32_t &failed_nonce,
                        std::string &gpu_digest_hex,
                        std::string &cpu_digest_hex) const {
    if (candidates.empty()) return RecheckStatus::ok;
    const auto bounds = std::minmax_element(
        candidates.begin(), candidates.end(),
        [](const SearchCandidate &lhs, const SearchCandidate &rhs) {
          return lhs.nonce < rhs.nonce;
        });
    const std::uint32_t first_nonce = bounds.first->nonce;
    const std::uint32_t last_nonce = bounds.second->nonce;
    const std::uint32_t count = last_nonce - first_nonce + 1U;
    TemporaryTranscript transcript;
    if (!transcript.valid()) return RecheckStatus::oracle_unavailable;
    if (!runCoreOracle(path_, encodeHex(header, 80U), first_nonce, count,
                       transcript.path())) {
      return RecheckStatus::oracle_unavailable;
    }

    std::ifstream input(transcript.path(), std::ios::binary);
    char magic[8]{};
    std::uint8_t count_bytes[4]{};
    input.read(magic, sizeof(magic));
    input.read(reinterpret_cast<char *>(count_bytes), sizeof(count_bytes));
    const std::uint32_t transcript_count = readLittleEndian32Host(count_bytes);
    if (!input || std::memcmp(magic, kOutputMagic, sizeof(kOutputMagic)) != 0 ||
        transcript_count != count) {
      return RecheckStatus::transcript_invalid;
    }
    std::vector<std::uint8_t> digests(static_cast<std::size_t>(count) * 32U);
    input.read(reinterpret_cast<char *>(digests.data()),
               static_cast<std::streamsize>(digests.size()));
    if (!input || input.peek() != std::char_traits<char>::eof()) {
      return RecheckStatus::transcript_invalid;
    }

    for (const auto &candidate : candidates) {
      const std::size_t offset =
          static_cast<std::size_t>(candidate.nonce - first_nonce) * 32U;
      const std::uint8_t *cpu_digest = digests.data() + offset;
      if (std::memcmp(cpu_digest, candidate.digest.data(), 32U) != 0) {
        failed_nonce = candidate.nonce;
        gpu_digest_hex = encodeHex(candidate.digest.data(), 32U);
        cpu_digest_hex = encodeHex(cpu_digest, 32U);
        return RecheckStatus::digest_mismatch;
      }
      if (!pepepow_uint256_meets_target_le(cpu_digest, target)) {
        failed_nonce = candidate.nonce;
        gpu_digest_hex = encodeHex(candidate.digest.data(), 32U);
        cpu_digest_hex = encodeHex(cpu_digest, 32U);
        return RecheckStatus::target_reject;
      }
    }
    return RecheckStatus::ok;
  }

 private:
  std::string path_;
};

void emitSearchSuccess(const char *search_status, const char *stop_reason,
                       std::uint32_t nonce_start, std::uint64_t nonce_count,
                       std::uint64_t processed_count, std::uint32_t batch_size,
                       std::uint64_t batch_count,
                       const std::vector<SearchCandidate> &candidates,
                       const cudaDeviceProp &properties, int driver_version,
                       int runtime_version) {
  std::cout << "{\"schema_version\":1,\"status\":\""
            << (std::string_view(search_status) == "cancelled" ? "cancelled" : "ok")
            << "\",\"engine\":\"cuda_hoohash_v110_bounded_search\","
            << "\"search_status\":\"" << search_status << '"'
            << ",\"stop_reason\":";
  if (stop_reason == nullptr) std::cout << "null";
  else std::cout << '"' << stop_reason << '"';
  std::cout << ",\"nonce_start\":" << nonce_start
            << ",\"nonce_count\":" << nonce_count
            << ",\"processed_count\":" << processed_count
            << ",\"next_nonce\":" << (static_cast<std::uint64_t>(nonce_start) + processed_count)
            << ",\"batch_size\":" << batch_size
            << ",\"batch_count\":" << batch_count
            << ",\"matrix_cache\":{\"builds\":1,\"reuses\":"
            << (batch_count == 0U ? 0U : batch_count - 1U)
            << ",\"key\":\"80-byte header with nonce bytes 76..79 zeroed\"}"
            << ",\"target_encoding\":\"little_endian_uint256\""
            << ",\"cpu_recheck\":\"pinned_core_full_digest_and_target\""
            << ",\"candidates\":[";
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    if (index != 0U) std::cout << ',';
    std::cout << "{\"nonce\":" << candidates[index].nonce
              << ",\"digest_hex\":\""
              << encodeHex(candidates[index].digest.data(), 32U) << "\"}";
  }
  std::cout << "],\"device\":{\"index\":0,\"name\":\""
            << jsonEscape(properties.name) << "\",\"compute_capability\":\""
            << properties.major << '.' << properties.minor << "\"}"
            << ",\"cuda_versions\":{\"driver_api\":" << driver_version
            << ",\"runtime_api\":" << runtime_version
            << ",\"toolkit_build\":\"" << PEPEPOW_CUDA_TOOLKIT_VERSION
            << "\"},\"error\":null}\n";
}

int runBoundedSearch(const std::uint8_t header[80],
                     const std::uint8_t target[32], std::uint32_t nonce_start,
                     std::uint64_t nonce_count, std::uint32_t batch_size,
                     std::uint32_t timeout_ms, const char *cpu_oracle_path,
                     const char *cancel_file_path) {
  CoreOracleRechecker verifier(cpu_oracle_path);
  if (!verifier.available()) {
    emitSearchError(SearchError::verifier_unavailable, cudaSuccess, nonce_start,
                    nonce_count, 0U);
    return searchErrorExitCode(SearchError::verifier_unavailable);
  }

  std::signal(SIGINT, requestSearchCancellation);
  std::signal(SIGTERM, requestSearchCancellation);
#ifdef SIGBREAK
  std::signal(SIGBREAK, requestSearchCancellation);
#endif
  g_search_cancel_requested = 0;
  g_search_cancel_file = cancel_file_path;

  int device_count = 0;
  cudaError_t error = cudaGetDeviceCount(&device_count);
  if (error != cudaSuccess) {
    const SearchError code = classifySearchCudaError(error);
    emitSearchError(code, error, nonce_start, nonce_count, 0U);
    return searchErrorExitCode(code);
  }
  if (device_count <= 0) {
    emitSearchError(SearchError::no_device, cudaErrorNoDevice, nonce_start,
                    nonce_count, 0U);
    return searchErrorExitCode(SearchError::no_device);
  }
  error = cudaSetDevice(0);

  SearchDeviceBuffers buffers;
  if (error == cudaSuccess) error = buffers.allocate(batch_size);
  if (error == cudaSuccess) error = cudaMemcpy(buffers.header, header, 80U,
                                                cudaMemcpyHostToDevice);
  if (error == cudaSuccess) error = cudaMemcpy(buffers.target, target, 32U,
                                                cudaMemcpyHostToDevice);
  if (error == cudaSuccess) {
    // The zero-nonce header seed and its 32 KiB FP64 matrix are invariant for
    // this immutable job. Build once, then reuse them across every bounded batch.
    prepareHashInputs<<<1, 1>>>(buffers.header, nonce_start, 1U,
                                buffers.first_passes, buffers.matrix_seed);
    error = cudaGetLastError();
  }
  if (error == cudaSuccess) {
    generateMatrix<<<1, 1>>>(buffers.matrix_seed, buffers.matrix);
    error = cudaGetLastError();
  }
  if (error == cudaSuccess) error = cudaDeviceSynchronize();

  const auto started = std::chrono::steady_clock::now();
  const auto deadline = started + std::chrono::milliseconds(timeout_ms);
  std::uint64_t processed_count = 0U;
  std::uint64_t batch_count = 0U;
  std::vector<SearchCandidate> verified_candidates;
  const cudaDeviceProp properties = [&error]() {
    cudaDeviceProp result{};
    const cudaError_t property_error = cudaGetDeviceProperties(&result, 0);
    if (error == cudaSuccess && property_error != cudaSuccess) error = property_error;
    return result;
  }();
  int driver_version = 0;
  int runtime_version = 0;
  if (error == cudaSuccess) error = cudaDriverGetVersion(&driver_version);
  if (error == cudaSuccess) error = cudaRuntimeGetVersion(&runtime_version);

  const char *search_status = "exhausted";
  const char *stop_reason = nullptr;
  while (error == cudaSuccess && processed_count < nonce_count) {
    if (searchCancellationRequested()) {
      search_status = "cancelled";
      stop_reason = searchCancellationReason();
      break;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      search_status = "cancelled";
      stop_reason = "timeout";
      break;
    }

    const std::uint32_t count = static_cast<std::uint32_t>(std::min<std::uint64_t>(
        nonce_count - processed_count, batch_size));
    const std::uint32_t batch_start = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(nonce_start) + processed_count);
    const unsigned int blocks = (count + 127U) / 128U;
    prepareHashInputs<<<blocks, 128>>>(buffers.header, batch_start, count,
                                       buffers.first_passes, buffers.matrix_seed);
    error = cudaGetLastError();
    if (error == cudaSuccess) {
      calculateDigests<<<blocks, 128>>>(buffers.first_passes, buffers.matrix,
                                        batch_start, count, buffers.digests);
      error = cudaGetLastError();
    }
    if (error == cudaSuccess) error = cudaMemset(buffers.candidate_count, 0,
                                                  sizeof(std::uint32_t));
    if (error == cudaSuccess) {
      collectTargetCandidates<<<blocks, 128>>>(
          buffers.digests, buffers.target, batch_start, count,
          buffers.candidate_count, buffers.candidate_nonces,
          buffers.candidate_digests);
      error = cudaGetLastError();
    }

    std::uint32_t candidate_count = 0U;
    if (error == cudaSuccess) {
      error = cudaMemcpy(&candidate_count, buffers.candidate_count,
                         sizeof(candidate_count), cudaMemcpyDeviceToHost);
    }
    if (error == cudaSuccess && candidate_count > count) {
      error = cudaErrorUnknown;
    }
    std::vector<std::uint32_t> candidate_nonces(candidate_count);
    std::vector<std::uint8_t> candidate_digests(
        static_cast<std::size_t>(candidate_count) * 32U);
    if (error == cudaSuccess && candidate_count != 0U) {
      error = cudaMemcpy(candidate_nonces.data(), buffers.candidate_nonces,
                         candidate_nonces.size() * sizeof(std::uint32_t),
                         cudaMemcpyDeviceToHost);
    }
    if (error == cudaSuccess && candidate_count != 0U) {
      error = cudaMemcpy(candidate_digests.data(), buffers.candidate_digests,
                         candidate_digests.size(), cudaMemcpyDeviceToHost);
    }
    if (error != cudaSuccess) break;

    processed_count += count;
    ++batch_count;
    if (searchCancellationRequested()) {
      search_status = "cancelled";
      stop_reason = searchCancellationReason();
      break;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      search_status = "cancelled";
      stop_reason = "timeout";
      break;
    }

    std::vector<SearchCandidate> batch_candidates;
    batch_candidates.reserve(candidate_count);
    for (std::uint32_t index = 0; index < candidate_count; ++index) {
      SearchCandidate candidate{};
      candidate.nonce = candidate_nonces[index];
      std::copy_n(candidate_digests.data() + static_cast<std::size_t>(index) * 32U,
                  32U, candidate.digest.begin());
      batch_candidates.push_back(candidate);
    }
    std::sort(batch_candidates.begin(), batch_candidates.end(),
              [](const SearchCandidate &lhs, const SearchCandidate &rhs) {
                return lhs.nonce < rhs.nonce;
              });
    bool candidate_contract_failure = false;
    for (std::size_t index = 0; index < batch_candidates.size(); ++index) {
      if (searchCancellationRequested()) {
        search_status = "cancelled";
        stop_reason = searchCancellationReason();
        break;
      }
      if (std::chrono::steady_clock::now() >= deadline) {
        search_status = "cancelled";
        stop_reason = "timeout";
        break;
      }
      const auto &candidate = batch_candidates[index];
      if (candidate.nonce < batch_start ||
          static_cast<std::uint64_t>(candidate.nonce) >=
              static_cast<std::uint64_t>(batch_start) + count ||
          (index != 0U && batch_candidates[index - 1U].nonce == candidate.nonce)) {
        candidate_contract_failure = true;
        break;
      }
    }
    if (error != cudaSuccess) break;
    if (candidate_contract_failure) {
      (void)buffers.release();
      emitSearchError(SearchError::candidate_contract_failure, cudaSuccess,
                      nonce_start, nonce_count, processed_count);
      return searchErrorExitCode(SearchError::candidate_contract_failure);
    }
    if (searchCancellationRequested() || stop_reason != nullptr) {
      verified_candidates.clear();
      search_status = "cancelled";
      if (searchCancellationRequested()) stop_reason = searchCancellationReason();
      else if (stop_reason == nullptr) stop_reason = "timeout";
      break;
    }
    if (!batch_candidates.empty()) {
      std::uint32_t failed_nonce = 0U;
      std::string gpu_digest_hex;
      std::string cpu_digest_hex;
      const RecheckStatus recheck = verifier.recheck(
          header, target, batch_candidates, failed_nonce, gpu_digest_hex,
          cpu_digest_hex);
      if (recheck != RecheckStatus::ok) {
        if (searchCancellationRequested()) {
          search_status = "cancelled";
          stop_reason = searchCancellationReason();
          break;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
          search_status = "cancelled";
          stop_reason = "timeout";
          break;
        }
        SearchError code = SearchError::verifier_unavailable;
        if (recheck == RecheckStatus::transcript_invalid) {
          code = SearchError::candidate_contract_failure;
        } else if (recheck == RecheckStatus::digest_mismatch) {
          code = SearchError::gpu_digest_mismatch;
        } else if (recheck == RecheckStatus::target_reject) {
          code = SearchError::cpu_target_reject;
        }
        (void)buffers.release();
        emitSearchError(code, cudaSuccess, nonce_start, nonce_count,
                        processed_count, failed_nonce, gpu_digest_hex,
                        cpu_digest_hex);
        return searchErrorExitCode(code);
      }
      verified_candidates = std::move(batch_candidates);
    }
    if (!verified_candidates.empty()) {
      search_status = "found";
      break;
    }
  }

  const cudaError_t release_error = buffers.release();
  if (error == cudaSuccess && release_error != cudaSuccess) error = release_error;
  if (error != cudaSuccess) {
    const SearchError code = classifySearchCudaError(error);
    emitSearchError(code, error, nonce_start, nonce_count, processed_count);
    return searchErrorExitCode(code);
  }
  emitSearchSuccess(search_status, stop_reason, nonce_start, nonce_count,
                    processed_count, batch_size, batch_count,
                    verified_candidates, properties, driver_version,
                    runtime_version);
  return std::string_view(search_status) == "cancelled" ? 130 : 0;
}
#endif

}  // namespace

int main(int argc, char **argv) {
#if defined(PEPEPOW_CUDA_BOUNDED_SEARCH_ENGINE)
  std::uint8_t header[80]{};
  std::uint8_t target[32]{};
  std::uint32_t nonce_start = 0U;
  std::uint32_t batch_size = 128U;
  std::uint32_t timeout_ms = 30000U;
  std::uint64_t nonce_count = 0U;
  const char *cpu_oracle_path = nullptr;
  const char *cancel_file_path = nullptr;
  if (argc != 17 || std::string_view(argv[1]) != "--header-hex" ||
      !decodeHeader(argv[2], header) ||
      std::string_view(argv[3]) != "--nonce-start" ||
      !parseUint32(argv[4], nonce_start) ||
      std::string_view(argv[5]) != "--nonce-count" ||
      !parseUint64(argv[6], nonce_count) || nonce_count == 0U ||
      nonce_count > kNonceSpaceSize - nonce_start ||
      readLittleEndian32Host(header + 76U) != nonce_start ||
      std::string_view(argv[7]) != "--target-hex" ||
      !decodeTarget(argv[8], target) ||
      std::string_view(argv[9]) != "--batch-size" ||
      !parseUint32(argv[10], batch_size) || batch_size == 0U ||
      batch_size > kMaximumSearchBatch ||
      std::string_view(argv[11]) != "--timeout-ms" ||
      !parseUint32(argv[12], timeout_ms) || timeout_ms == 0U ||
      timeout_ms > kMaximumSearchTimeoutMs ||
      std::string_view(argv[13]) != "--cpu-oracle" || argv[14][0] == '\0' ||
      std::string_view(argv[15]) != "--cancel-file" || argv[16][0] == '\0') {
    emitSearchError(SearchError::invalid_arguments, cudaSuccess, nonce_start,
                    nonce_count, 0U);
    return searchErrorExitCode(SearchError::invalid_arguments);
  }
  cpu_oracle_path = argv[14];
  cancel_file_path = argv[16];
  return runBoundedSearch(header, target, nonce_start, nonce_count, batch_size,
                          timeout_ms, cpu_oracle_path, cancel_file_path);
#else
  std::uint8_t header[80]{};
  std::uint32_t nonce_start = 0;
  std::uint32_t count = 0;
  if (argc != 9 || std::string_view(argv[1]) != "--header-hex" ||
      !decodeHeader(argv[2], header) ||
      std::string_view(argv[3]) != "--nonce-start" ||
      !parseUint32(argv[4], nonce_start) ||
      std::string_view(argv[5]) != "--nonce-count" ||
      !parseUint32(argv[6], count) || count == 0U ||
      count > kMaximumBatch ||
      static_cast<std::uint64_t>(nonce_start) + count - 1U > UINT32_MAX ||
      std::string_view(argv[7]) != "--output" || argv[8][0] == '\0') {
    emitError(ErrorCode::invalid_arguments, cudaSuccess);
    return exitCode(ErrorCode::invalid_arguments);
  }
  return run(header, nonce_start, count, argv[8]);
#endif
}

#include <cuda_runtime.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t kElementsPerSeed = 64U * 64U;
constexpr std::uint32_t kMaximumSeeds = 64;
constexpr std::size_t kSeedBytes = 32;
constexpr std::size_t kObservationBytes = 52;
constexpr char kInputMagic[8] = {'P', 'P', 'W', 'S', 'E', 'E', 'D', '1'};
constexpr char kOutputMagic[8] = {'P', 'P', 'W', 'O', 'B', 'S', '1', '\0'};

struct Observation {
  std::uint64_t value;
  std::uint64_t next_state[4];
  std::uint32_t lower;
  std::uint64_t matrix_bits;
};

struct InitialState {
  std::uint64_t state[4];
};

static_assert(sizeof(Observation) == 56,
              "the bounded CUDA observation allocation assumes 56-byte records");
static_assert(offsetof(Observation, matrix_bits) == 48,
              "CUDA observation matrix bits must remain naturally aligned");

enum class StageError {
  no_device,
  driver_unavailable,
  initialization_failed,
  cuda_failure,
  output_failure,
  invalid_arguments,
};

__device__ __forceinline__ std::uint64_t readLittleEndian64(
    const std::uint8_t *bytes) {
  std::uint64_t result = 0;
  for (unsigned int index = 0; index < 8; ++index) {
    result |= static_cast<std::uint64_t>(bytes[index]) << (index * 8U);
  }
  return result;
}

__device__ __forceinline__ std::uint64_t rotateLeft64(
    std::uint64_t value, unsigned int amount) {
  return (value << amount) | (value >> (64U - amount));
}

__global__ void generateMatrixTranscript(const std::uint8_t *seeds,
                                         InitialState *initial_states,
                                         Observation *observations,
                                         std::uint32_t seed_count) {
  const std::uint32_t seed_index = blockIdx.x;
  if (seed_index >= seed_count || threadIdx.x != 0) return;

  const std::uint8_t *seed = seeds + seed_index * kSeedBytes;
  std::uint64_t s0 = readLittleEndian64(seed);
  std::uint64_t s1 = readLittleEndian64(seed + 8);
  std::uint64_t s2 = readLittleEndian64(seed + 16);
  std::uint64_t s3 = readLittleEndian64(seed + 24);
  initial_states[seed_index].state[0] = s0;
  initial_states[seed_index].state[1] = s1;
  initial_states[seed_index].state[2] = s2;
  initial_states[seed_index].state[3] = s3;

  Observation *case_observations = observations + seed_index * kElementsPerSeed;
  for (std::uint32_t element = 0; element < kElementsPerSeed; ++element) {
    // Match the pinned Core's actual xoshiro256++-form output expression and
    // state transition. Unsigned arithmetic wraps modulo 2^64.
    const std::uint64_t value = rotateLeft64(s0 + s3, 23U) + s0;
    const std::uint64_t transition = s1 << 17U;

    s2 ^= s0;
    s3 ^= s1;
    s1 ^= s2;
    s0 ^= s3;
    s2 ^= transition;
    s3 = rotateLeft64(s3, 45U);

    const std::uint32_t lower = static_cast<std::uint32_t>(value & 0xffffffffULL);
    const double normalized = __ddiv_rn(static_cast<double>(lower),
                                        static_cast<double>(0xffffffffU));
    const double matrix_value = normalized * 1000000.0;

    Observation &observation = case_observations[element];
    observation.value = value;
    observation.next_state[0] = s0;
    observation.next_state[1] = s1;
    observation.next_state[2] = s2;
    observation.next_state[3] = s3;
    observation.lower = lower;
    observation.matrix_bits = static_cast<std::uint64_t>(
        __double_as_longlong(matrix_value));
  }
}

std::uint32_t readLittleEndian32(const std::uint8_t *bytes) {
  return static_cast<std::uint32_t>(bytes[0]) |
         (static_cast<std::uint32_t>(bytes[1]) << 8U) |
         (static_cast<std::uint32_t>(bytes[2]) << 16U) |
         (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

void writeLittleEndian32(std::ostream &output, std::uint32_t value) {
  const char bytes[4] = {static_cast<char>(value),
                         static_cast<char>(value >> 8U),
                         static_cast<char>(value >> 16U),
                         static_cast<char>(value >> 24U)};
  output.write(bytes, sizeof(bytes));
}

void writeLittleEndian64(std::ostream &output, std::uint64_t value) {
  char bytes[8];
  for (unsigned int index = 0; index < 8; ++index) {
    bytes[index] = static_cast<char>(value >> (index * 8U));
  }
  output.write(bytes, sizeof(bytes));
}

bool readSeedCorpus(const std::string &path, std::vector<std::uint8_t> &seeds,
                    std::uint32_t &seed_count) {
  std::ifstream input(path, std::ios::binary);
  char header[12];
  if (!input.read(header, sizeof(header))) return false;
  if (std::memcmp(header, kInputMagic, sizeof(kInputMagic)) != 0) return false;
  seed_count = readLittleEndian32(
      reinterpret_cast<const std::uint8_t *>(header + 8));
  if (seed_count == 0 || seed_count > kMaximumSeeds) return false;
  seeds.resize(static_cast<std::size_t>(seed_count) * kSeedBytes);
  if (!input.read(reinterpret_cast<char *>(seeds.data()),
                  static_cast<std::streamsize>(seeds.size()))) {
    return false;
  }
  return input.peek() == std::ifstream::traits_type::eof();
}

const char *stageErrorName(StageError error) {
  switch (error) {
    case StageError::no_device:
      return "no_device";
    case StageError::driver_unavailable:
      return "driver_unavailable";
    case StageError::initialization_failed:
      return "initialization_failed";
    case StageError::cuda_failure:
      return "cuda_failure";
    case StageError::output_failure:
      return "output_failure";
    case StageError::invalid_arguments:
      return "invalid_arguments";
  }
  return "cuda_failure";
}

int stageExitCode(StageError error) {
  switch (error) {
    case StageError::no_device:
      return 2;
    case StageError::driver_unavailable:
      return 3;
    case StageError::initialization_failed:
      return 4;
    case StageError::cuda_failure:
      return 5;
    case StageError::output_failure:
      return 6;
    case StageError::invalid_arguments:
      return 64;
  }
  return 5;
}

StageError classify(cudaError_t error) {
  switch (error) {
    case cudaErrorNoDevice:
      return StageError::no_device;
    case cudaErrorInsufficientDriver:
      return StageError::driver_unavailable;
    case cudaErrorInitializationError:
      return StageError::initialization_failed;
    default:
      return StageError::cuda_failure;
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
    case cudaErrorInvalidConfiguration:
      return "cudaErrorInvalidConfiguration";
    default:
      return "cudaErrorOther";
  }
}

void emitError(StageError code, cudaError_t cuda_error) {
  std::cout << "{\"schema_version\":1,\"status\":\"error\","
            << "\"stage\":\"hoohash_v110_matrix_prng_4096\","
            << "\"error\":{\"code\":\"" << stageErrorName(code)
            << "\",\"cuda_error\":";
  if (code == StageError::output_failure) {
    std::cout << "null";
  } else {
    std::cout << "\"" << cudaErrorName(cuda_error) << "\"";
  }
  std::cout << ",\"exit_code\":" << stageExitCode(code) << "}}\n";
}

bool writeTranscript(const std::string &path, const std::vector<std::uint8_t> &seeds,
                     std::uint32_t seed_count,
                     const std::vector<InitialState> &initial_states,
                     const std::vector<Observation> &observations) {
  std::ofstream output(path, std::ios::binary);
  if (!output.write(kOutputMagic, sizeof(kOutputMagic))) return false;
  writeLittleEndian32(output, seed_count);
  for (std::uint32_t seed_index = 0; seed_index < seed_count; ++seed_index) {
    output.write(reinterpret_cast<const char *>(seeds.data() + seed_index * kSeedBytes),
                 kSeedBytes);
    for (unsigned int state_index = 0; state_index < 4; ++state_index) {
      writeLittleEndian64(output, initial_states[seed_index].state[state_index]);
    }
    const Observation *case_observations =
        observations.data() + seed_index * kElementsPerSeed;
    for (std::uint32_t element = 0; element < kElementsPerSeed; ++element) {
      const Observation &observation = case_observations[element];
      writeLittleEndian64(output, observation.value);
      for (unsigned int state_index = 0; state_index < 4; ++state_index) {
        writeLittleEndian64(output, observation.next_state[state_index]);
      }
      writeLittleEndian32(output, observation.lower);
      writeLittleEndian64(output, observation.matrix_bits);
    }
  }
  return output.good();
}

int run(const std::string &input_path, const std::string &output_path) {
  std::vector<std::uint8_t> host_seeds;
  std::uint32_t seed_count = 0;
  if (!readSeedCorpus(input_path, host_seeds, seed_count)) {
    emitError(StageError::invalid_arguments, cudaSuccess);
    return stageExitCode(StageError::invalid_arguments);
  }

  int device_count = 0;
  cudaError_t error = cudaGetDeviceCount(&device_count);
  if (error != cudaSuccess) {
    const StageError code = classify(error);
    emitError(code, error);
    return stageExitCode(code);
  }
  if (device_count == 0) {
    emitError(StageError::no_device, cudaErrorNoDevice);
    return stageExitCode(StageError::no_device);
  }
  error = cudaSetDevice(0);
  if (error == cudaSuccess) error = cudaFree(nullptr);
  if (error != cudaSuccess) {
    const StageError code = classify(error);
    emitError(code, error);
    return stageExitCode(code);
  }

  const std::size_t observation_count =
      static_cast<std::size_t>(seed_count) * kElementsPerSeed;
  const std::size_t seed_bytes = host_seeds.size();
  const std::size_t observation_bytes = observation_count * sizeof(Observation);
  std::vector<InitialState> host_initial_states(seed_count);
  std::vector<Observation> host_observations(observation_count);
  std::uint8_t *device_seeds = nullptr;
  InitialState *device_initial_states = nullptr;
  Observation *device_observations = nullptr;

  error = cudaMalloc(reinterpret_cast<void **>(&device_seeds), seed_bytes);
  if (error == cudaSuccess) {
    error = cudaMalloc(reinterpret_cast<void **>(&device_initial_states),
                       static_cast<std::size_t>(seed_count) * sizeof(InitialState));
  }
  if (error == cudaSuccess) {
    error = cudaMalloc(reinterpret_cast<void **>(&device_observations),
                       observation_bytes);
  }
  if (error == cudaSuccess) {
    error = cudaMemcpy(device_seeds, host_seeds.data(), seed_bytes,
                       cudaMemcpyHostToDevice);
  }
  if (error == cudaSuccess) {
    generateMatrixTranscript<<<seed_count, 1>>>(device_seeds,
                                                device_initial_states,
                                                device_observations,
                                                seed_count);
    error = cudaGetLastError();
  }
  if (error == cudaSuccess) error = cudaDeviceSynchronize();
  if (error == cudaSuccess) {
    error = cudaMemcpy(host_initial_states.data(), device_initial_states,
                       static_cast<std::size_t>(seed_count) * sizeof(InitialState),
                       cudaMemcpyDeviceToHost);
  }
  if (error == cudaSuccess) {
    error = cudaMemcpy(host_observations.data(), device_observations,
                       observation_bytes, cudaMemcpyDeviceToHost);
  }

  const cudaError_t free_observations =
      device_observations == nullptr ? cudaSuccess : cudaFree(device_observations);
  const cudaError_t free_initial_states =
      device_initial_states == nullptr ? cudaSuccess : cudaFree(device_initial_states);
  const cudaError_t free_seeds =
      device_seeds == nullptr ? cudaSuccess : cudaFree(device_seeds);
  if (error == cudaSuccess && free_observations != cudaSuccess) error = free_observations;
  if (error == cudaSuccess && free_initial_states != cudaSuccess) error = free_initial_states;
  if (error == cudaSuccess && free_seeds != cudaSuccess) error = free_seeds;
  if (error != cudaSuccess) {
    const StageError code = classify(error);
    emitError(code, error);
    return stageExitCode(code);
  }

  if (!writeTranscript(output_path, host_seeds, seed_count, host_initial_states,
                       host_observations)) {
    emitError(StageError::output_failure, cudaSuccess);
    return stageExitCode(StageError::output_failure);
  }

  std::cout << "{\"schema_version\":1,\"status\":\"ok\","
            << "\"stage\":\"hoohash_v110_matrix_prng_4096\","
            << "\"seed_count\":" << seed_count
            << ",\"elements_per_seed\":" << kElementsPerSeed
            << ",\"observation_record_bytes\":" << kObservationBytes
            << ",\"error\":null}\n";
  return 0;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 5 || std::string(argv[1]) != "--input" ||
      std::string(argv[3]) != "--output") {
    emitError(StageError::invalid_arguments, cudaSuccess);
    return stageExitCode(StageError::invalid_arguments);
  }
  return run(argv[2], argv[4]);
}

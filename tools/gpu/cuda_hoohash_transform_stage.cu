#include <cuda_runtime.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t kMaximumInputs = 32768;
constexpr std::size_t kInputRecordBytes = 9;
constexpr std::size_t kOutputRecordBytes = 165;
constexpr std::size_t kMaximumSafeIterations = 512;
constexpr char kInputMagic[8] = {'P', 'P', 'W', 'T', 'R', 'I', 'N', '1'};
constexpr char kOutputMagic[8] = {'P', 'P', 'W', 'T', 'R', 'O', '1', '\0'};
constexpr double kPi = 3.14159265358979323846;
constexpr double kEpsilon = 1e-9;
constexpr double kComplexTransformMultiplier = 0.000001;
static_assert(kOutputRecordBytes == 3U * 8U + 4U + 6U * 8U + 1U + 11U * 8U,
              "serialized transform record size changed");

struct InputRecord {
  std::uint64_t bits;
  std::uint32_t safe_enabled;
};

struct OutputRecord {
  std::uint64_t input_bits;
  std::uint64_t factor_one_bits;
  std::uint64_t factor_two_bits;
  std::uint32_t branch;
  std::uint64_t medium_bits;
  std::uint64_t intermediate_bits;
  std::uint64_t high_bits;
  std::uint64_t complex_bits;
  std::uint64_t safe_bits;
  std::uint64_t transform_factor_bits;
  std::uint32_t safe_enabled;
  std::uint64_t medium_sine_bits;
  std::uint64_t medium_cosine_bits;
  std::uint64_t medium_sum_bits;
  std::uint64_t intermediate_sine_bits;
  std::uint64_t high_abs_plus_one_bits;
  std::uint64_t high_sqrt_bits;
  std::uint64_t complex_argument_bits;
  std::uint64_t sincos_sine_bits;
  std::uint64_t sincos_cosine_bits;
  std::uint64_t sincos_sum_bits;
  std::uint64_t sincos_medium_bits;
};

static_assert(offsetof(OutputRecord, medium_bits) == 32,
              "CUDA transform result fields must remain naturally aligned");

enum class StageError {
  no_device,
  driver_unavailable,
  initialization_failed,
  cuda_failure,
  output_failure,
  invalid_arguments,
};

__device__ __forceinline__ double mediumComplexNonLinear(double value) {
  return exp(sin(value) + cos(value));
}

__device__ __forceinline__ double intermediateComplexNonLinear(double value) {
  if (fabs(value - kPi / 2.0) < kEpsilon ||
      fabs(value - 3.0 * kPi / 2.0) < kEpsilon) {
    return 0.0;
  }
  const double sine = sin(value);
  return sine * sine;
}

__device__ __forceinline__ double highComplexNonLinear(double value) {
  return __ddiv_rn(1.0, sqrt(fabs(value) + 1.0));
}

__device__ __forceinline__ void complexFactors(double value,
                                                double &factor_one,
                                                double &factor_two) {
  const double scaled_one = value * kComplexTransformMultiplier;
  const double scaled_two = value * kComplexTransformMultiplier;
  const double quotient_one = __ddiv_rn(scaled_one, 8.0);
  const double quotient_two = __ddiv_rn(scaled_two, 4.0);
  factor_one = quotient_one - floor(quotient_one);
  factor_two = quotient_two - floor(quotient_two);
}

__device__ __forceinline__ double transformArgument(double value,
                                                    double factor_two,
                                                    std::uint32_t operation) {
  const double adjustment = 1.0 + factor_two;
  switch (operation) {
    case 0:
      return value + adjustment;
    case 1:
      return value - adjustment;
    case 2:
      return value * adjustment;
    default:
      return __ddiv_rn(value, adjustment);
  }
}

__device__ __forceinline__ std::uint32_t transformBranch(double factor_one,
                                                         double factor_two) {
  const std::uint32_t family = factor_one < 0.33 ? 0U :
                               factor_one < 0.66 ? 1U : 2U;
  const std::uint32_t operation = factor_two < 0.25 ? 0U :
                                  factor_two < 0.5 ? 1U :
                                  factor_two < 0.75 ? 2U : 3U;
  return family * 4U + operation;
}

__device__ __forceinline__ double applyTransform(double value,
                                                 double factor_two,
                                                 std::uint32_t family,
                                                 std::uint32_t operation) {
  const double argument = transformArgument(value, factor_two, operation);
  if (family == 0U) return mediumComplexNonLinear(argument);
  if (family == 1U) return intermediateComplexNonLinear(argument);
  return highComplexNonLinear(argument);
}

__device__ __forceinline__ double complexNonLinear(double value,
                                                   double factor_one,
                                                   double factor_two) {
  const std::uint32_t branch = transformBranch(factor_one, factor_two);
  return applyTransform(value, factor_two, branch / 4U, branch % 4U);
}

__device__ __forceinline__ double safeComplexTransform(double value) {
  // Preserve the pinned Core loop exactly: the transformed value is computed
  // once and is intentionally not recomputed after scaling the input.
  double factor_one = 0.0;
  double factor_two = 0.0;
  complexFactors(value, factor_one, factor_two);
  const double transformed_value =
      complexNonLinear(value, factor_one, factor_two);
  double scaled_input = value;
  double rounds = 1.0;
  std::size_t iterations = 0;
  while (isnan(transformed_value) || isinf(transformed_value)) {
    if (++iterations > kMaximumSafeIterations) {
      return __longlong_as_double(0x7ff8000000000000LL);
    }
    scaled_input *= 0.1;
    if (scaled_input <= 0.0000000000001) return 0.0 * rounds;
    rounds++;
  }
  return transformed_value * rounds;
}

__device__ __forceinline__ double transformFactor(double value) {
  const double quotient = __ddiv_rn(value, 1024.0);
  return quotient - floor(quotient);
}

__device__ __forceinline__ std::uint64_t binary64Bits(double value) {
  return static_cast<std::uint64_t>(__double_as_longlong(value));
}

__global__ void evaluateTransforms(const InputRecord *inputs,
                                   OutputRecord *outputs,
                                   std::uint32_t count) {
  const std::uint32_t index = blockIdx.x * blockDim.x + threadIdx.x;
  if (index >= count) return;

  const InputRecord input = inputs[index];
  const double value = __longlong_as_double(static_cast<long long>(input.bits));
  double factor_one = 0.0;
  double factor_two = 0.0;
  complexFactors(value, factor_one, factor_two);
  const std::uint32_t branch = transformBranch(factor_one, factor_two);
  const double complex = applyTransform(value, factor_two, branch / 4U,
                                        branch % 4U);
  const double safe = input.safe_enabled
                          ? safeComplexTransform(value)
                          : 0.0;
  const double medium_sine = sin(value);
  const double medium_cosine = cos(value);
  const double medium_sum = medium_sine + medium_cosine;
  const double intermediate_sine = sin(value);
  const double high_abs_plus_one = fabs(value) + 1.0;
  const double high_sqrt = sqrt(high_abs_plus_one);
  const double complex_argument =
      transformArgument(value, factor_two, branch % 4U);
  double sincos_sine = 0.0;
  double sincos_cosine = 0.0;
  sincos(value, &sincos_sine, &sincos_cosine);
  const double sincos_sum = sincos_sine + sincos_cosine;
  const double sincos_medium = exp(sincos_sum);

  OutputRecord result{};
  result.input_bits = input.bits;
  result.factor_one_bits = binary64Bits(factor_one);
  result.factor_two_bits = binary64Bits(factor_two);
  result.branch = branch;
  result.medium_bits = binary64Bits(mediumComplexNonLinear(value));
  result.intermediate_bits = binary64Bits(intermediateComplexNonLinear(value));
  result.high_bits = binary64Bits(highComplexNonLinear(value));
  result.complex_bits = binary64Bits(complex);
  result.safe_bits = binary64Bits(safe);
  result.transform_factor_bits = binary64Bits(transformFactor(value));
  result.safe_enabled = input.safe_enabled;
  result.medium_sine_bits = binary64Bits(medium_sine);
  result.medium_cosine_bits = binary64Bits(medium_cosine);
  result.medium_sum_bits = binary64Bits(medium_sum);
  result.intermediate_sine_bits = binary64Bits(intermediate_sine);
  result.high_abs_plus_one_bits = binary64Bits(high_abs_plus_one);
  result.high_sqrt_bits = binary64Bits(high_sqrt);
  result.complex_argument_bits = binary64Bits(complex_argument);
  result.sincos_sine_bits = binary64Bits(sincos_sine);
  result.sincos_cosine_bits = binary64Bits(sincos_cosine);
  result.sincos_sum_bits = binary64Bits(sincos_sum);
  result.sincos_medium_bits = binary64Bits(sincos_medium);
  outputs[index] = result;
}

std::uint32_t readLittleEndian32(const std::uint8_t *bytes) {
  return static_cast<std::uint32_t>(bytes[0]) |
         (static_cast<std::uint32_t>(bytes[1]) << 8U) |
         (static_cast<std::uint32_t>(bytes[2]) << 16U) |
         (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

std::uint64_t readLittleEndian64(const std::uint8_t *bytes) {
  std::uint64_t value = 0;
  for (unsigned int index = 0; index < 8; ++index) {
    value |= static_cast<std::uint64_t>(bytes[index]) << (index * 8U);
  }
  return value;
}

void writeLittleEndian32(std::ostream &output, std::uint32_t value) {
  char bytes[4] = {static_cast<char>(value),
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

bool readInputCorpus(const std::string &path, std::vector<InputRecord> &inputs) {
  std::ifstream input(path, std::ios::binary);
  std::uint8_t header[12];
  if (!input.read(reinterpret_cast<char *>(header), sizeof(header)) ||
      std::memcmp(header, kInputMagic, sizeof(kInputMagic)) != 0) {
    return false;
  }
  const std::uint32_t count = readLittleEndian32(header + 8);
  if (count == 0 || count > kMaximumInputs) return false;
  inputs.resize(count);
  for (std::uint32_t index = 0; index < count; ++index) {
    std::uint8_t record[kInputRecordBytes];
    if (!input.read(reinterpret_cast<char *>(record), sizeof(record))) {
      return false;
    }
    if (record[8] > 1) return false;
    const std::uint64_t bits = readLittleEndian64(record);
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    if (record[8] != 0 && !std::isfinite(value)) return false;
    inputs[index] = {bits, record[8]};
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

void writeJsonString(const char *value) {
  std::cout << '"';
  for (const unsigned char *cursor =
           reinterpret_cast<const unsigned char *>(value);
       *cursor != 0; ++cursor) {
    switch (*cursor) {
      case '"':
        std::cout << "\\\"";
        break;
      case '\\':
        std::cout << "\\\\";
        break;
      case '\b':
        std::cout << "\\b";
        break;
      case '\f':
        std::cout << "\\f";
        break;
      case '\n':
        std::cout << "\\n";
        break;
      case '\r':
        std::cout << "\\r";
        break;
      case '\t':
        std::cout << "\\t";
        break;
      default:
        if (*cursor < 0x20) {
          constexpr char hex[] = "0123456789abcdef";
          std::cout << "\\u00" << hex[*cursor >> 4U] << hex[*cursor & 0x0fU];
        } else {
          std::cout << static_cast<char>(*cursor);
        }
    }
  }
  std::cout << '"';
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
            << "\"stage\":\"hoohash_v110_fp64_transform_diagnostic\","
            << "\"error\":{\"code\":\"" << stageErrorName(code)
            << "\",\"cuda_error\":";
  if (code == StageError::output_failure || code == StageError::invalid_arguments) {
    std::cout << "null";
  } else {
    std::cout << "\"" << cudaErrorName(cuda_error) << "\"";
  }
  std::cout << ",\"exit_code\":" << stageExitCode(code) << "}}\n";
}

bool writeTranscript(const std::string &path,
                     const std::vector<OutputRecord> &outputs) {
  std::ofstream output(path, std::ios::binary);
  if (!output.write(kOutputMagic, sizeof(kOutputMagic))) return false;
  writeLittleEndian32(output, static_cast<std::uint32_t>(outputs.size()));
  for (const OutputRecord &record : outputs) {
    writeLittleEndian64(output, record.input_bits);
    writeLittleEndian64(output, record.factor_one_bits);
    writeLittleEndian64(output, record.factor_two_bits);
    writeLittleEndian32(output, record.branch);
    writeLittleEndian64(output, record.medium_bits);
    writeLittleEndian64(output, record.intermediate_bits);
    writeLittleEndian64(output, record.high_bits);
    writeLittleEndian64(output, record.complex_bits);
    writeLittleEndian64(output, record.safe_bits);
    writeLittleEndian64(output, record.transform_factor_bits);
    output.put(static_cast<char>(record.safe_enabled));
    writeLittleEndian64(output, record.medium_sine_bits);
    writeLittleEndian64(output, record.medium_cosine_bits);
    writeLittleEndian64(output, record.medium_sum_bits);
    writeLittleEndian64(output, record.intermediate_sine_bits);
    writeLittleEndian64(output, record.high_abs_plus_one_bits);
    writeLittleEndian64(output, record.high_sqrt_bits);
    writeLittleEndian64(output, record.complex_argument_bits);
    writeLittleEndian64(output, record.sincos_sine_bits);
    writeLittleEndian64(output, record.sincos_cosine_bits);
    writeLittleEndian64(output, record.sincos_sum_bits);
    writeLittleEndian64(output, record.sincos_medium_bits);
  }
  return output.good();
}

int run(const std::string &input_path, const std::string &output_path) {
  std::vector<InputRecord> host_inputs;
  if (!readInputCorpus(input_path, host_inputs)) {
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
  cudaDeviceProp device_properties{};
  int driver_api_supported = 0;
  int runtime_version = 0;
  if (error == cudaSuccess) {
    error = cudaGetDeviceProperties(&device_properties, 0);
  }
  if (error == cudaSuccess) {
    error = cudaDriverGetVersion(&driver_api_supported);
  }
  if (error == cudaSuccess) {
    error = cudaRuntimeGetVersion(&runtime_version);
  }
  if (error != cudaSuccess) {
    const StageError code = classify(error);
    emitError(code, error);
    return stageExitCode(code);
  }

  const std::size_t input_bytes = host_inputs.size() * sizeof(InputRecord);
  const std::size_t output_bytes = host_inputs.size() * sizeof(OutputRecord);
  std::vector<OutputRecord> host_outputs(host_inputs.size());
  InputRecord *device_inputs = nullptr;
  OutputRecord *device_outputs = nullptr;
  error = cudaMalloc(reinterpret_cast<void **>(&device_inputs), input_bytes);
  if (error == cudaSuccess) {
    error = cudaMalloc(reinterpret_cast<void **>(&device_outputs), output_bytes);
  }
  if (error == cudaSuccess) {
    error = cudaMemcpy(device_inputs, host_inputs.data(), input_bytes,
                       cudaMemcpyHostToDevice);
  }
  if (error == cudaSuccess) {
    constexpr unsigned int kThreads = 256;
    const unsigned int blocks = static_cast<unsigned int>(
        (host_inputs.size() + kThreads - 1U) / kThreads);
    evaluateTransforms<<<blocks, kThreads>>>(device_inputs, device_outputs,
                                             static_cast<std::uint32_t>(
                                                 host_inputs.size()));
    error = cudaGetLastError();
  }
  if (error == cudaSuccess) error = cudaDeviceSynchronize();
  if (error == cudaSuccess) {
    error = cudaMemcpy(host_outputs.data(), device_outputs, output_bytes,
                       cudaMemcpyDeviceToHost);
  }

  const cudaError_t free_outputs =
      device_outputs == nullptr ? cudaSuccess : cudaFree(device_outputs);
  const cudaError_t free_inputs =
      device_inputs == nullptr ? cudaSuccess : cudaFree(device_inputs);
  if (error == cudaSuccess && free_outputs != cudaSuccess) error = free_outputs;
  if (error == cudaSuccess && free_inputs != cudaSuccess) error = free_inputs;
  if (error != cudaSuccess) {
    const StageError code = classify(error);
    emitError(code, error);
    return stageExitCode(code);
  }

  if (!writeTranscript(output_path, host_outputs)) {
    emitError(StageError::output_failure, cudaSuccess);
    return stageExitCode(StageError::output_failure);
  }

  std::cout << "{\"schema_version\":1,\"status\":\"ok\","
            << "\"stage\":\"hoohash_v110_fp64_transform_diagnostic\","
            << "\"input_count\":" << host_inputs.size()
            << ",\"device\":{\"index\":0,\"name\":";
  writeJsonString(device_properties.name);
  std::cout << ",\"compute_capability\":\""
            << device_properties.major << "." << device_properties.minor
            << "\",\"global_memory_bytes\":"
            << static_cast<unsigned long long>(device_properties.totalGlobalMem)
            << "},\"cuda_versions\":{\"driver_api_supported\":"
            << driver_api_supported << ",\"runtime\":" << runtime_version
            << ",\"toolkit_build\":\"" << PEPEPOW_CUDA_TOOLKIT_VERSION
            << "\"},\"error\":null}\n";
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

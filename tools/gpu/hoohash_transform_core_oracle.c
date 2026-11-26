#include <fenv.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openlibm_math.h>

#ifndef PEPEPOW_CORE_HOOHASH_C_PATH
#error "PEPEPOW_CORE_HOOHASH_C_PATH must name the pinned Core source file"
#endif

/* Include the unmodified pinned Core translation unit so the test-only
 * wrapper invokes its file-local transform functions directly. */
#include PEPEPOW_CORE_HOOHASH_C_PATH

/* OpenLibm exports this BSD extension, but its feature macro is intentionally
 * not enabled for the full pinned-Core translation unit. */
extern void sincos(double, double *, double *);

enum {
  kInputHeaderBytes = 12,
  kInputRecordBytes = 9,
  kOutputHeaderBytes = 12,
  kOutputRecordBytes = 165,
  kMaximumInputs = 32768
};

static const uint8_t kInputMagic[8] = {'P', 'P', 'W', 'T', 'R', 'I', 'N', '1'};
static const uint8_t kOutputMagic[8] = {'P', 'P', 'W', 'T', 'R', 'O', '1', 0};

static uint32_t read_le32(const uint8_t bytes[4]) {
  return ((uint32_t)bytes[0]) |
         ((uint32_t)bytes[1] << 8) |
         ((uint32_t)bytes[2] << 16) |
         ((uint32_t)bytes[3] << 24);
}

static uint64_t read_le64(const uint8_t bytes[8]) {
  uint64_t value = 0;
  for (unsigned int index = 0; index < 8; ++index) {
    value |= (uint64_t)bytes[index] << (index * 8U);
  }
  return value;
}

static void write_le32(uint8_t bytes[4], uint32_t value) {
  for (unsigned int index = 0; index < 4; ++index) {
    bytes[index] = (uint8_t)(value >> (index * 8U));
  }
}

static void write_le64(uint8_t bytes[8], uint64_t value) {
  for (unsigned int index = 0; index < 8; ++index) {
    bytes[index] = (uint8_t)(value >> (index * 8U));
  }
}

static uint64_t double_bits(double value) {
  uint64_t bits = 0;
  memcpy(&bits, &value, sizeof(bits));
  return bits;
}

static uint32_t selected_branch(double input) {
  const double factor_one =
      (input * COMPLEX_TRANSFORM_MULTIPLIER) / 8.0 -
      floor((input * COMPLEX_TRANSFORM_MULTIPLIER) / 8.0);
  const double factor_two =
      (input * COMPLEX_TRANSFORM_MULTIPLIER) / 4.0 -
      floor((input * COMPLEX_TRANSFORM_MULTIPLIER) / 4.0);

  const uint32_t family = factor_one < 0.33 ? 0U :
                          factor_one < 0.66 ? 1U : 2U;
  const uint32_t operation = factor_two < 0.25 ? 0U :
                             factor_two < 0.5 ? 1U :
                             factor_two < 0.75 ? 2U : 3U;
  return family * 4U + operation;
}

static double selected_argument(double input, double factor_two,
                                uint32_t branch) {
  const double adjustment = 1.0 + factor_two;
  switch (branch % 4U) {
    case 0:
      return input + adjustment;
    case 1:
      return input - adjustment;
    case 2:
      return input * adjustment;
    default:
      return input / adjustment;
  }
}

static int fail(const char *message) {
  fprintf(stderr, "pinned Core transform oracle: %s\n", message);
  return 1;
}

static int run(const char *input_path, const char *output_path) {
  FILE *input = fopen(input_path, "rb");
  if (input == NULL) return fail("cannot open input corpus");

  uint8_t header[kInputHeaderBytes];
  if (fread(header, 1, sizeof(header), input) != sizeof(header) ||
      memcmp(header, kInputMagic, sizeof(kInputMagic)) != 0) {
    fclose(input);
    return fail("input must start with PPWTRIN1 and a little-endian count");
  }
  const uint32_t count = read_le32(header + 8);
  if (count == 0 || count > kMaximumInputs) {
    fclose(input);
    return fail("input count must be in the range 1..32768");
  }
  if (fesetround(FE_TONEAREST) != 0) {
    fclose(input);
    return fail("cannot select round-to-nearest mode");
  }

  FILE *output = fopen(output_path, "wb");
  if (output == NULL) {
    fclose(input);
    return fail("cannot open output transcript");
  }
  uint8_t output_header[kOutputHeaderBytes];
  memcpy(output_header, kOutputMagic, sizeof(kOutputMagic));
  write_le32(output_header + 8, count);
  if (fwrite(output_header, 1, sizeof(output_header), output) !=
      sizeof(output_header)) {
    fclose(input);
    fclose(output);
    return fail("cannot write output header");
  }

  for (uint32_t index = 0; index < count; ++index) {
    uint8_t input_record[kInputRecordBytes];
    if (fread(input_record, 1, sizeof(input_record), input) !=
        sizeof(input_record)) {
      fclose(input);
      fclose(output);
      return fail("input ended before the declared record count");
    }
    const uint64_t input_bits = read_le64(input_record);
    const uint8_t safe_enabled = input_record[8];
    if (safe_enabled > 1) {
      fclose(input);
      fclose(output);
      return fail("safe flag must be zero or one");
    }
    double value = 0.0;
    memcpy(&value, &input_bits, sizeof(value));
    if (safe_enabled && !isfinite(value)) {
      fclose(input);
      fclose(output);
      return fail("SafeComplexTransform inputs must be finite and bounded");
    }

    const double scaled_one = value * COMPLEX_TRANSFORM_MULTIPLIER;
    const double scaled_two = value * COMPLEX_TRANSFORM_MULTIPLIER;
    const double factor_one = scaled_one / 8.0 - floor(scaled_one / 8.0);
    const double factor_two = scaled_two / 4.0 - floor(scaled_two / 4.0);
    const double medium = MediumComplexNonLinear(value);
    const double intermediate = IntermediateComplexNonLinear(value);
    const double high = HighComplexNonLinear(value);
    const double complex = ComplexNonLinear(value);
    const double safe = safe_enabled ? SafeComplexTransform(value) : 0.0;
    const double transform_factor = TransformFactor(value);
    const double medium_sine = sin(value);
    const double medium_cosine = cos(value);
    const double medium_sum = medium_sine + medium_cosine;
    const double intermediate_sine = sin(value);
    const double high_abs_plus_one = fabs(value) + 1.0;
    const double high_sqrt = sqrt(high_abs_plus_one);
    const double complex_argument = selected_argument(
        value, factor_two, selected_branch(value));
    double openlibm_sincos_sine = 0.0;
    double openlibm_sincos_cosine = 0.0;
    sincos(value, &openlibm_sincos_sine, &openlibm_sincos_cosine);
    const double openlibm_sincos_sum =
        openlibm_sincos_sine + openlibm_sincos_cosine;
    const double openlibm_sincos_medium = exp(openlibm_sincos_sum);

    uint8_t record[kOutputRecordBytes];
    size_t offset = 0;
#define WRITE_BITS(field)                                      \
    do {                                                       \
      write_le64(record + offset, double_bits(field));         \
      offset += 8;                                             \
    } while (0)
    write_le64(record + offset, input_bits);
    offset += 8;
    WRITE_BITS(factor_one);
    WRITE_BITS(factor_two);
    write_le32(record + offset, selected_branch(value));
    offset += 4;
    WRITE_BITS(medium);
    WRITE_BITS(intermediate);
    WRITE_BITS(high);
    WRITE_BITS(complex);
    WRITE_BITS(safe);
    WRITE_BITS(transform_factor);
    record[offset++] = safe_enabled;
    WRITE_BITS(medium_sine);
    WRITE_BITS(medium_cosine);
    WRITE_BITS(medium_sum);
    WRITE_BITS(intermediate_sine);
    WRITE_BITS(high_abs_plus_one);
    WRITE_BITS(high_sqrt);
    WRITE_BITS(complex_argument);
    WRITE_BITS(openlibm_sincos_sine);
    WRITE_BITS(openlibm_sincos_cosine);
    WRITE_BITS(openlibm_sincos_sum);
    WRITE_BITS(openlibm_sincos_medium);
#undef WRITE_BITS
    if (offset != sizeof(record) ||
        fwrite(record, 1, sizeof(record), output) != sizeof(record)) {
      fclose(input);
      fclose(output);
      return fail("cannot write transform observation");
    }
  }

  const int trailing = fgetc(input);
  fclose(input);
  if (trailing != EOF) {
    fclose(output);
    return fail("input corpus contains trailing bytes");
  }
  if (fclose(output) != 0) return fail("cannot flush output transcript");

  printf("PINNED CORE FP64 transform oracle: %u inputs, round-to-nearest\n",
         count);
  return 0;
}

int main(int argc, char **argv) {
  if (argc != 5 || strcmp(argv[1], "--input") != 0 ||
      strcmp(argv[3], "--output") != 0) {
    return fail("usage: oracle --input INPUT.bin --output TRANSCRIPT.bin");
  }
  return run(argv[2], argv[4]);
}

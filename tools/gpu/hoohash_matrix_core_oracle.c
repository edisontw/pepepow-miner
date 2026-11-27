#include <stdint.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PEPEPOW_CORE_HOOHASH_C_PATH
#error "PEPEPOW_CORE_HOOHASH_C_PATH must name the pinned Core source file"
#endif

/* Include the unmodified pinned Core translation unit so this test-only
 * wrapper can observe its file-local PRNG and matrix functions. */
#include PEPEPOW_CORE_HOOHASH_C_PATH

#if DBL_MANT_DIG != 53 || DBL_MAX_EXP != 1024
#error "the pinned-Core matrix oracle requires IEEE 754 binary64"
#endif

enum {
    kSeedBytes = 32,
    kMatrixElements = 64 * 64,
    kMaxSeeds = 64,
    kObservationBytes = 52
};

static const uint8_t kInputMagic[8] = {'P', 'P', 'W', 'S', 'E', 'E', 'D', '1'};
static const uint8_t kOutputMagic[8] = {'P', 'P', 'W', 'O', 'B', 'S', '1', 0};

static uint32_t read_le32(const uint8_t bytes[4]) {
    return ((uint32_t)bytes[0]) |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static void write_le32(uint8_t bytes[4], uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static void write_le64(uint8_t bytes[8], uint64_t value) {
    for (unsigned int i = 0; i < 8; ++i) {
        bytes[i] = (uint8_t)(value >> (8U * i));
    }
}

static int fail(const char *message) {
    fprintf(stderr, "pinned Core matrix oracle: %s\n", message);
    return 1;
}

static int run(const char *input_path, const char *output_path) {
    FILE *input = fopen(input_path, "rb");
    if (input == NULL) return fail("cannot open input corpus");

    uint8_t header[12];
    if (fread(header, 1, sizeof(header), input) != sizeof(header) ||
        memcmp(header, kInputMagic, sizeof(kInputMagic)) != 0) {
        fclose(input);
        return fail("input must start with PPWSEED1 and a little-endian count");
    }
    const uint32_t count = read_le32(header + 8);
    if (count == 0 || count > kMaxSeeds) {
        fclose(input);
        return fail("seed count must be in the range 1..64");
    }

    FILE *output = fopen(output_path, "wb");
    if (output == NULL) {
        fclose(input);
        return fail("cannot open output file");
    }
    uint8_t output_header[12];
    memcpy(output_header, kOutputMagic, sizeof(kOutputMagic));
    write_le32(output_header + 8, count);
    if (fwrite(output_header, 1, sizeof(output_header), output) != sizeof(output_header)) {
        fclose(input);
        fclose(output);
        return fail("cannot write observation header");
    }

    for (uint32_t seed_index = 0; seed_index < count; ++seed_index) {
        uint8_t seed[kSeedBytes];
        if (fread(seed, 1, sizeof(seed), input) != sizeof(seed)) {
            fclose(input);
            fclose(output);
            return fail("input corpus ended before all seeds were read");
        }

        double matrix[64][64];
        generateHoohashMatrix(seed, matrix);

        xoshiro_state state;
        xoshiro_init(&state, seed);
        const uint64_t initial_state[4] = {state.s0, state.s1, state.s2, state.s3};

        uint8_t record_header[kSeedBytes + 4 * 8];
        memcpy(record_header, seed, sizeof(seed));
        for (unsigned int i = 0; i < 4; ++i) {
            write_le64(record_header + kSeedBytes + i * 8, initial_state[i]);
        }
        if (fwrite(record_header, 1, sizeof(record_header), output) != sizeof(record_header)) {
            fclose(input);
            fclose(output);
            return fail("cannot write observation seed and initial state");
        }

        for (uint32_t element = 0; element < kMatrixElements; ++element) {
            const uint64_t value = xoshiro_gen(&state);
            const uint32_t lower = (uint32_t)(value & UINT64_C(0xffffffff));
            uint64_t matrix_bits = 0;
            const double matrix_value = matrix[element / 64][element % 64];
            memcpy(&matrix_bits, &matrix_value, sizeof(matrix_bits));

            const uint64_t next_state[4] = {state.s0, state.s1, state.s2, state.s3};
            uint8_t observation[kObservationBytes];
            write_le64(observation, value);
            for (unsigned int i = 0; i < 4; ++i) {
                write_le64(observation + 8 + i * 8, next_state[i]);
            }
            write_le32(observation + 40, lower);
            write_le64(observation + 44, matrix_bits);
            if (fwrite(observation, 1, sizeof(observation), output) != sizeof(observation)) {
                fclose(input);
                fclose(output);
                return fail("cannot write matrix observation");
            }
        }
    }

    int trailing = fgetc(input);
    fclose(input);
    if (trailing != EOF) {
        fclose(output);
        return fail("input corpus contains trailing bytes");
    }
    if (fclose(output) != 0) return fail("cannot flush observation file");

    printf("PINNED CORE matrix oracle: %u seeds, %u elements per seed\n",
           count, (unsigned int)kMatrixElements);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 5 || strcmp(argv[1], "--input") != 0 ||
        strcmp(argv[3], "--output") != 0) {
        return fail("usage: oracle --input SEEDS.bin --output OBSERVATIONS.bin");
    }
    return run(argv[2], argv[4]);
}

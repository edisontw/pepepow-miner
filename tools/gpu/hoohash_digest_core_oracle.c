#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PEPEPOW_CORE_HOOHASH_C_PATH
#error "PEPEPOW_CORE_HOOHASH_C_PATH must name the pinned Core source file"
#endif

/* Include the unmodified pinned Core translation unit as the digest oracle. */
#include PEPEPOW_CORE_HOOHASH_C_PATH

enum {
    kHeaderBytes = 80,
    kDigestBytes = 32,
    kMaximumBatch = 10000
};

static const uint8_t kOutputMagic[8] = {'P', 'P', 'W', 'D', 'I', 'G', '1', 0};

static int hex_nibble(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

static int decode_header(const char *text, uint8_t output[kHeaderBytes]) {
    if (strlen(text) != kHeaderBytes * 2) return 0;
    for (size_t index = 0; index < kHeaderBytes; ++index) {
        const int high = hex_nibble(text[index * 2]);
        const int low = hex_nibble(text[index * 2 + 1]);
        if (high < 0 || low < 0) return 0;
        output[index] = (uint8_t)((high << 4) | low);
    }
    return 1;
}

static int parse_uint32(const char *text, uint32_t *value) {
    char *end = NULL;
    errno = 0;
    const unsigned long parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed > UINT32_MAX) {
        return 0;
    }
    *value = (uint32_t)parsed;
    return 1;
}

static int write_le32(FILE *file, uint32_t value) {
    const uint8_t bytes[4] = {
        (uint8_t)value, (uint8_t)(value >> 8),
        (uint8_t)(value >> 16), (uint8_t)(value >> 24)
    };
    return fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes);
}

static int run(const char *header_hex, const char *start_text,
               const char *count_text, const char *output_path) {
    uint8_t header[kHeaderBytes];
    uint32_t nonce_start = 0;
    uint32_t count = 0;
    if (!decode_header(header_hex, header) ||
        !parse_uint32(start_text, &nonce_start) ||
        !parse_uint32(count_text, &count) || count == 0 ||
        count > kMaximumBatch ||
        (uint64_t)nonce_start + count - 1 > UINT32_MAX) {
        fprintf(stderr, "pinned Core digest oracle: invalid header or nonce range\n");
        return 64;
    }

    FILE *output = fopen(output_path, "wb");
    if (output == NULL) {
        fprintf(stderr, "pinned Core digest oracle: cannot open output\n");
        return 1;
    }
    int ok = fwrite(kOutputMagic, 1, sizeof(kOutputMagic), output) ==
             sizeof(kOutputMagic) && write_le32(output, count);
    for (uint32_t index = 0; ok && index < count; ++index) {
        const uint32_t nonce = nonce_start + index;
        header[76] = (uint8_t)nonce;
        header[77] = (uint8_t)(nonce >> 8);
        header[78] = (uint8_t)(nonce >> 16);
        header[79] = (uint8_t)(nonce >> 24);
        uint8_t digest[kDigestBytes];
        hoohashv110(header, sizeof(header), digest);
        ok = fwrite(digest, 1, sizeof(digest), output) == sizeof(digest);
    }
    if (fclose(output) != 0) ok = 0;
    if (!ok) {
        fprintf(stderr, "pinned Core digest oracle: write failed\n");
        return 1;
    }
    printf("PINNED CORE digest oracle: %u complete 32-byte digests\n", count);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 9 || strcmp(argv[1], "--header-hex") != 0 ||
        strcmp(argv[3], "--nonce-start") != 0 ||
        strcmp(argv[5], "--nonce-count") != 0 ||
        strcmp(argv[7], "--output") != 0) {
        fprintf(stderr, "usage: oracle --header-hex HEX --nonce-start N "
                        "--nonce-count N --output FILE\n");
        return 64;
    }
    return run(argv[2], argv[4], argv[6], argv[8]);
}

/* Offline full-digest KAT frontend. The linked reference is supplied separately.
 * Input/output encoding is hoohashv110 function bytes, NOT RPC/display hex.
 * Exit codes: 0 match, 1 mismatch, 2 invalid command/input.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern void hoohashv110(const void *data, size_t len, uint8_t output[32]);

static int nibble(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int decode(const char *text, uint8_t *out, size_t bytes) {
    size_t i;
    if (strlen(text) != bytes * 2) return 0;
    for (i = 0; i < bytes; ++i) {
        int hi = nibble((unsigned char)text[2 * i]);
        int lo = nibble((unsigned char)text[2 * i + 1]);
        if (hi < 0 || lo < 0) return 0;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return 1;
}

static void print_digest(const char *label, const uint8_t bytes[32]) {
    size_t i;
    printf("%s", label);
    for (i = 0; i < 32; ++i) printf("%02x", (unsigned)bytes[i]);
    putchar('\n');
}

int main(int argc, char **argv) {
    uint8_t header[80], expected[32], actual[32];
    if (argc != 3 || !decode(argv[1], header, sizeof header) ||
                     !decode(argv[2], expected, sizeof expected)) {
        fputs("Usage: pepepow-kat HEADER_HEX_160 EXPECTED_FUNCTION_BYTES_HEX_64\n", stderr);
        return 2;
    }
    /* Poison instead of zeros: a reference that does not write output should
       be diagnosed via independent KATs, not silently normalized to zero. */
    memset(actual, 0xa5, sizeof actual);
    hoohashv110(header, sizeof header, actual);
    print_digest("actual=", actual);
    print_digest("expected=", expected);
    if (memcmp(actual, expected, sizeof actual) != 0) {
        fputs("MISMATCH\n", stderr);
        return 1;
    }
    puts("MATCH");
    return 0;
}

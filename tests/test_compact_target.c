#include "pepepow/compact_target.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Full-width byte vectors for the Bitcoin-derived Core SetCompact boundaries. */
static void check(uint32_t bits, int valid, const uint8_t expected[32]) {
    uint8_t out[32];
    memset(out, 0xa5, sizeof out);
    if (pepepow_decode_compact(bits, out) != valid ||
        memcmp(out, expected, sizeof out) != 0) {
        fprintf(stderr, "compact target mismatch for %08" PRIx32 "\n", bits);
        exit(EXIT_FAILURE);
    }
}

int main(void) {
    check(0x0101ffffu, 1, (const uint8_t[32]){1}); /* discard low mantissa bytes */
    check(0x0201ffffu, 1, (const uint8_t[32]){0xff, 1});
    check(0x037fffffu, 1, (const uint8_t[32]){0xff, 0xff, 0x7f});
    check(0x04123456u, 1, (const uint8_t[32]){0, 0x56, 0x34, 0x12});
    check(0x1d00ffffu, 1, (const uint8_t[32]){[26] = 0xff, [27] = 0xff});
    check(0x207fffffu, 1, (const uint8_t[32]){[29] = 0xff, [30] = 0xff, [31] = 0x7f});
    check(0x21000001u, 1, (const uint8_t[32]){[30] = 1});
    check(0x2100ffffu, 1, (const uint8_t[32]){[30] = 0xff, [31] = 0xff});
    check(0x220000ffu, 1, (const uint8_t[32]){[31] = 0xff});

    const uint32_t invalid[] = {
        0x00000000u, 0x007fffffu, 0x0100ffffu, 0x020000ffu,
        0x1d80ffffu, 0x03800001u, 0x21010000u, 0x22000100u, 0x23000001u,
    };
    const uint8_t zero[32] = {0};
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; ++i)
        check(invalid[i], 0, zero);
    if (pepepow_decode_compact(0x1d00ffffu, NULL)) return EXIT_FAILURE;
    puts("compact target: 19 boundary vectors passed");
    return EXIT_SUCCESS;
}

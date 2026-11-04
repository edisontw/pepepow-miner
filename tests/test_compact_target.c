#include "pepepow/compact_target.h"
#include <assert.h>
#include <stdio.h>
static void check(uint32_t bits, size_t offset, uint8_t value) {
    uint8_t out[32]; size_t i;
    assert(pepepow_decode_compact(bits, out));
    for (i = 0; i < 32; ++i) assert(out[i] == (i == offset ? value : 0));
}
int main(void) {
    uint8_t out[32];
    /* 0x01010000 = 1; 0x02010000 = 256; 0x21000001 = 2^240. */
    check(0x01010000u, 0, 1);
    check(0x02010000u, 1, 1);
    check(0x21000001u, 30, 1);
    assert(pepepow_decode_compact(0x1d00ffffu, out));
    assert(out[26] == 0xff && out[27] == 0xff);
    assert(out[25] == 0 && out[28] == 0);
    assert(!pepepow_decode_compact(0x1d80ffffu, out)); /* negative */
    assert(!pepepow_decode_compact(0x23010000u, out)); /* overflow */
    assert(!pepepow_decode_compact(0x00000000u, out)); /* zero */
    assert(!pepepow_decode_compact(0x01000001u, out)); /* truncates to zero */
    assert(!pepepow_decode_compact(0x1d00ffffu, NULL));
    puts("compact target: PASS");
    return 0;
}

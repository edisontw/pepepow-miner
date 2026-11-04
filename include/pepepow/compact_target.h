#ifndef PEPEPOW_COMPACT_TARGET_H
#define PEPEPOW_COMPACT_TARGET_H
#include <stdint.h>
#include <stddef.h>
/* Decode Bitcoin-derived nBits into 32 little-endian target bytes.
 * Return 1 only for a positive, non-overflowing, non-negative target.
 * Consensus PoW limit checks are intentionally left to the caller.
 */
static inline int pepepow_decode_compact(uint32_t bits, uint8_t out[32]) {
    uint32_t size = bits >> 24;
    uint32_t word = bits & 0x007fffffu;
    size_t i;
    if (!out) return 0;
    for (i = 0; i < 32; ++i) out[i] = 0;
    if (!word || (bits & 0x00800000u)) return 0;
    if (size > 34u || (word > 0xffu && size > 33u) ||
        (word > 0xffffu && size > 32u)) return 0;
    if (size <= 3u) {
        word >>= 8u * (3u - size);
        if (!word) return 0;
        for (i = 0; i < 4u && i < 32u; ++i) out[i] = (uint8_t)(word >> (8u * i));
    } else {
        size_t shift = (size_t)(size - 3u);
        for (i = 0; i < 3u; ++i) {
            if (shift + i < 32u) out[shift + i] = (uint8_t)(word >> (8u * i));
        }
    }
    return 1;
}
#endif

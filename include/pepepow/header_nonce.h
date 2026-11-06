#ifndef PEPEPOW_HEADER_NONCE_H
#define PEPEPOW_HEADER_NONCE_H

#include <stddef.h>
#include <stdint.h>

#define PEPEPOW_HEADER_SIZE 80u
#define PEPEPOW_NONCE_OFFSET 76u

/* Writes the 32-bit nonce in Bitcoin-derived little-endian wire order.
 * Returns 0 on success, -1 for NULL or a non-80-byte header.
 * Does not compute or validate HooHash. */
static inline int pepepow_header_set_nonce(uint8_t *header, size_t length,
                                          uint32_t nonce) {
    if (header == NULL || length != PEPEPOW_HEADER_SIZE) return -1;
    header[PEPEPOW_NONCE_OFFSET] = (uint8_t)nonce;
    header[PEPEPOW_NONCE_OFFSET + 1u] = (uint8_t)(nonce >> 8);
    header[PEPEPOW_NONCE_OFFSET + 2u] = (uint8_t)(nonce >> 16);
    header[PEPEPOW_NONCE_OFFSET + 3u] = (uint8_t)(nonce >> 24);
    return 0;
}

static inline int pepepow_header_get_nonce(const uint8_t *header, size_t length,
                                          uint32_t *nonce) {
    if (header == NULL || nonce == NULL || length != PEPEPOW_HEADER_SIZE) return -1;
    *nonce = (uint32_t)header[PEPEPOW_NONCE_OFFSET]
           | ((uint32_t)header[PEPEPOW_NONCE_OFFSET + 1u] << 8)
           | ((uint32_t)header[PEPEPOW_NONCE_OFFSET + 2u] << 16)
           | ((uint32_t)header[PEPEPOW_NONCE_OFFSET + 3u] << 24);
    return 0;
}
#endif

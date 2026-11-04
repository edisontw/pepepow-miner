#include "../include/pepepow/header_nonce.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void check(uint32_t nonce) {
    uint8_t header[PEPEPOW_HEADER_SIZE];
    memset(header, 0xa5, sizeof(header));
    assert(pepepow_header_set_nonce(header, sizeof(header), nonce) == 0);
    for (size_t i = 0; i < PEPEPOW_NONCE_OFFSET; ++i) assert(header[i] == 0xa5);
    for (size_t i = 0; i < 4; ++i) assert(header[76 + i] == (uint8_t)(nonce >> (8 * i)));
    uint32_t actual = 0;
    assert(pepepow_header_get_nonce(header, sizeof(header), &actual) == 0);
    assert(actual == nonce);
}

int main(void) {
    const uint32_t edges[] = {0, 1, 255, 256, UINT32_MAX, 0x12345678u};
    for (size_t i = 0; i < sizeof(edges)/sizeof(edges[0]); ++i) check(edges[i]);
    for (uint32_t n = 0; n < 256; ++n) check(n);
    uint8_t header[80] = {0};
    uint32_t nonce = 0;
    assert(pepepow_header_set_nonce(NULL, 80, 0) == -1);
    assert(pepepow_header_set_nonce(header, 79, 0) == -1);
    assert(pepepow_header_set_nonce(header, 81, 0) == -1);
    assert(pepepow_header_get_nonce(NULL, 80, &nonce) == -1);
    assert(pepepow_header_get_nonce(header, 80, NULL) == -1);
    assert(pepepow_header_get_nonce(header, 79, &nonce) == -1);
    puts("header nonce contract: PASS");
    return 0;
}

/* SYNTHETIC TEST DOUBLE. Not HooHash, not a mining implementation.
 * Linked only into kat-contract-test to exercise parser/comparator/exit codes.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
void hoohashv110(const void *data, size_t len, uint8_t out[32]) {
    const uint8_t *header = (const uint8_t *)data;
    size_t i;
    if (len != 80) abort();
    fputs("SYNTHETIC_REFERENCE_NOT_HOOHASH\n", stderr);
    for (i = 0; i < 32; ++i)
        out[i] = header[i] ^ header[i + 32] ^ header[i + 48];
}

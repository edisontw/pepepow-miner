#ifndef PEPEPOW_UINT256_TARGET_H
#define PEPEPOW_UINT256_TARGET_H

#include <stddef.h>
#include <stdint.h>

/*
 * Compare two 256-bit unsigned integers encoded least-significant byte first.
 * Returns -1, 0, or 1; this is a pure integer helper, NOT a HooHash validator.
 * Callers must explicitly convert any raw digest / RPC display encoding before
 * using this function. Neither network target nor share target is calculated.
 */
static inline int pepepow_uint256_cmp_le(const uint8_t lhs[32],
                                         const uint8_t rhs[32]) {
    for (size_t i = 32; i-- > 0;) {
        if (lhs[i] < rhs[i]) return -1;
        if (lhs[i] > rhs[i]) return 1;
    }
    return 0;
}

static inline int pepepow_uint256_meets_target_le(const uint8_t hash[32],
                                                   const uint8_t target[32]) {
    return pepepow_uint256_cmp_le(hash, target) <= 0;
}
#endif

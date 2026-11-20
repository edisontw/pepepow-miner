#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pepepow/uint256_target.h"

int main(void) {
    uint8_t a[32] = {0}, b[32] = {0};
    assert(pepepow_uint256_cmp_le(a, b) == 0);
    assert(pepepow_uint256_meets_target_le(a, b));

    a[0] = 255; b[1] = 1;
    assert(pepepow_uint256_cmp_le(a, b) < 0);
    assert(pepepow_uint256_cmp_le(b, a) > 0);
    assert(pepepow_uint256_meets_target_le(a, b));
    assert(!pepepow_uint256_meets_target_le(b, a));

    memset(a, 255, 32); memset(b, 255, 32);
    assert(pepepow_uint256_cmp_le(a, b) == 0);
    b[31] = 254;
    assert(pepepow_uint256_cmp_le(a, b) > 0);

    memset(a, 0, 32); memset(b, 0, 32);
    for (int i = 0; i < 32; i++) {
        a[i] = 1;
        assert(pepepow_uint256_cmp_le(a, b) > 0);
        a[i] = 0;
    }
    puts("uint256 target comparison: PASS");
    return 0;
}

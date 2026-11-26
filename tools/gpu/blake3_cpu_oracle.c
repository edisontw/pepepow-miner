#include "crypto/blake3.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int nibble(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}

static int decode_header(const char *text, uint8_t header[80]) {
  if (strlen(text) != 160) return 0;
  for (size_t index = 0; index < 80; ++index) {
    const int high = nibble(text[index * 2]);
    const int low = nibble(text[index * 2 + 1]);
    if (high < 0 || low < 0) return 0;
    header[index] = (uint8_t)((high << 4) | low);
  }
  return 1;
}

int main(int argc, char **argv) {
  uint8_t header[80];
  uint8_t digest[32];
  blake3_hasher hasher;

  if (argc != 3 || !decode_header(argv[1], header) ||
      (strcmp(argv[2], "header") != 0 && strcmp(argv[2], "matrix-seed") != 0)) {
    fputs("usage: pepepow-hoohash-blake3-cpu-oracle <header-hex-160> <header|matrix-seed>\n",
          stderr);
    return 64;
  }

  if (strcmp(argv[2], "matrix-seed") == 0) {
    memset(header + 76, 0, 4);
  }
  blake3_hasher_init(&hasher);
  blake3_hasher_update(&hasher, header, sizeof(header));
  blake3_hasher_finalize(&hasher, digest, sizeof(digest));
  for (size_t index = 0; index < sizeof(digest); ++index) {
    printf("%02x", digest[index]);
  }
  putchar('\n');
  return 0;
}

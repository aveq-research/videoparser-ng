/**
 * @file md5.h
 * @brief MD5 (RFC 1321) for the test programs, to compare decoded pictures
 *
 * @copyright Copyright (c) 2026, videoparser-ng contributors.
 */

#ifndef VIDEOPARSER_TEST_MD5_H
#define VIDEOPARSER_TEST_MD5_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct md5_context {
  uint32_t state[4];
  uint64_t length; /* bytes */
  uint8_t buffer[64];
} md5_context;

static const uint32_t md5_k[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a,
    0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340,
    0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
    0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
    0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
    0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

static const uint8_t md5_r[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

static void md5_block(md5_context *ctx, const uint8_t *block) {
  uint32_t w[16];
  for (int i = 0; i < 16; i++) {
    w[i] = (uint32_t)block[i * 4] | ((uint32_t)block[i * 4 + 1] << 8) |
           ((uint32_t)block[i * 4 + 2] << 16) |
           ((uint32_t)block[i * 4 + 3] << 24);
  }
  uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2],
           d = ctx->state[3];
  for (int i = 0; i < 64; i++) {
    uint32_t f;
    int g;
    if (i < 16) {
      f = (b & c) | (~b & d);
      g = i;
    } else if (i < 32) {
      f = (d & b) | (~d & c);
      g = (5 * i + 1) % 16;
    } else if (i < 48) {
      f = b ^ c ^ d;
      g = (3 * i + 5) % 16;
    } else {
      f = c ^ (b | ~d);
      g = (7 * i) % 16;
    }
    uint32_t t = d;
    d = c;
    c = b;
    uint32_t x = a + f + md5_k[i] + w[g];
    b = b + ((x << md5_r[i]) | (x >> (32 - md5_r[i])));
    a = t;
  }
  ctx->state[0] += a;
  ctx->state[1] += b;
  ctx->state[2] += c;
  ctx->state[3] += d;
}

static void md5_init(md5_context *ctx) {
  ctx->state[0] = 0x67452301;
  ctx->state[1] = 0xefcdab89;
  ctx->state[2] = 0x98badcfe;
  ctx->state[3] = 0x10325476;
  ctx->length = 0;
}

static void md5_update(md5_context *ctx, const uint8_t *data, size_t size) {
  size_t used = (size_t)(ctx->length % 64);
  ctx->length += size;
  if (used) {
    size_t take = 64 - used < size ? 64 - used : size;
    memcpy(ctx->buffer + used, data, take);
    data += take;
    size -= take;
    if (used + take < 64) {
      return;
    }
    md5_block(ctx, ctx->buffer);
  }
  while (size >= 64) {
    md5_block(ctx, data);
    data += 64;
    size -= 64;
  }
  memcpy(ctx->buffer, data, size);
}

static void md5_final(md5_context *ctx, uint8_t digest[16]) {
  uint64_t bits = ctx->length * 8;
  uint8_t pad[72] = {0x80};
  size_t used = (size_t)(ctx->length % 64);
  size_t pad_size = used < 56 ? 56 - used : 120 - used;
  for (int i = 0; i < 8; i++) {
    pad[pad_size + i] = (uint8_t)(bits >> (8 * i));
  }
  md5_update(ctx, pad, pad_size + 8);
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      digest[i * 4 + j] = (uint8_t)(ctx->state[i] >> (8 * j));
    }
  }
}

#endif /* VIDEOPARSER_TEST_MD5_H */

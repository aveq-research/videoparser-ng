/**
 * @file json_double.h
 * @brief Format a double as nlohmann::json does
 *
 * A C port of the Grisu2 implementation in nlohmann::json 3.11.2
 * (VideoParserCli/include/json.hpp, namespace dtoa_impl, and
 * serializer::dump_float), so that the C API test program writes numbers
 * byte for byte like the CLI.
 *
 * The original code is licensed under the MIT license:
 * Copyright (c) 2013-2022 Niels Lohmann <https://nlohmann.me>, and
 * Copyright (c) 2009 Florian Loitsch <https://florian.loitsch.com/> for the
 * Grisu2 algorithm.
 */

#ifndef VIDEOPARSER_JSON_DOUBLE_H
#define VIDEOPARSER_JSON_DOUBLE_H

#include <math.h>
#include <stdint.h>
#include <string.h>

typedef struct jd_diyfp {
  uint64_t f;
  int e;
} jd_diyfp;

static jd_diyfp jd_make(uint64_t f, int e) {
  jd_diyfp x;
  x.f = f;
  x.e = e;
  return x;
}

static jd_diyfp jd_sub(jd_diyfp x, jd_diyfp y) { return jd_make(x.f - y.f, x.e); }

static jd_diyfp jd_mul(jd_diyfp x, jd_diyfp y) {
  const uint64_t u_lo = x.f & 0xFFFFFFFFu;
  const uint64_t u_hi = x.f >> 32u;
  const uint64_t v_lo = y.f & 0xFFFFFFFFu;
  const uint64_t v_hi = y.f >> 32u;
  const uint64_t p0 = u_lo * v_lo;
  const uint64_t p1 = u_lo * v_hi;
  const uint64_t p2 = u_hi * v_lo;
  const uint64_t p3 = u_hi * v_hi;
  const uint64_t p0_hi = p0 >> 32u;
  const uint64_t p1_lo = p1 & 0xFFFFFFFFu;
  const uint64_t p1_hi = p1 >> 32u;
  const uint64_t p2_lo = p2 & 0xFFFFFFFFu;
  const uint64_t p2_hi = p2 >> 32u;
  uint64_t q = p0_hi + p1_lo + p2_lo;
  q += (uint64_t)1 << (64u - 32u - 1u); /* round, ties up */
  const uint64_t h = p3 + p2_hi + p1_hi + (q >> 32u);
  return jd_make(h, x.e + y.e + 64);
}

static jd_diyfp jd_normalize(jd_diyfp x) {
  while ((x.f >> 63u) == 0) {
    x.f <<= 1u;
    x.e--;
  }
  return x;
}

static jd_diyfp jd_normalize_to(jd_diyfp x, int target_exponent) {
  const int delta = x.e - target_exponent;
  return jd_make(x.f << delta, target_exponent);
}

/* Normalized value and boundaries of a finite, positive double */
static void jd_compute_boundaries(double value, jd_diyfp *w, jd_diyfp *minus,
                                  jd_diyfp *plus) {
  const int k_precision = 53;
  const int k_bias = 1024 - 1 + (k_precision - 1);
  const int k_min_exp = 1 - k_bias;
  const uint64_t k_hidden_bit = (uint64_t)1 << (k_precision - 1);
  uint64_t bits;
  memcpy(&bits, &value, sizeof(bits));
  const uint64_t e = bits >> (k_precision - 1);
  const uint64_t f = bits & (k_hidden_bit - 1);
  const int is_denormal = e == 0;
  const jd_diyfp v = is_denormal ? jd_make(f, k_min_exp)
                                 : jd_make(f + k_hidden_bit, (int)e - k_bias);
  const int lower_boundary_is_closer = f == 0 && e > 1;
  const jd_diyfp m_plus = jd_make(2 * v.f + 1, v.e - 1);
  const jd_diyfp m_minus = lower_boundary_is_closer
                               ? jd_make(4 * v.f - 1, v.e - 2)
                               : jd_make(2 * v.f - 1, v.e - 1);
  const jd_diyfp w_plus = jd_normalize(m_plus);
  const jd_diyfp w_minus = jd_normalize_to(m_minus, w_plus.e);
  *w = jd_normalize(v);
  *minus = w_minus;
  *plus = w_plus;
}

#define JD_ALPHA (-60)
#define JD_GAMMA (-32)

typedef struct jd_cached_power {
  uint64_t f;
  int e;
  int k;
} jd_cached_power;

static jd_cached_power jd_get_cached_power(int e) {
  static const jd_cached_power k_cached_powers[79] = {
      {0xAB70FE17C79AC6CA, -1060, -300}, {0xFF77B1FCBEBCDC4F, -1034, -292},
      {0xBE5691EF416BD60C, -1007, -284}, {0x8DD01FAD907FFC3C, -980, -276},
      {0xD3515C2831559A83, -954, -268},  {0x9D71AC8FADA6C9B5, -927, -260},
      {0xEA9C227723EE8BCB, -901, -252},  {0xAECC49914078536D, -874, -244},
      {0x823C12795DB6CE57, -847, -236},  {0xC21094364DFB5637, -821, -228},
      {0x9096EA6F3848984F, -794, -220},  {0xD77485CB25823AC7, -768, -212},
      {0xA086CFCD97BF97F4, -741, -204},  {0xEF340A98172AACE5, -715, -196},
      {0xB23867FB2A35B28E, -688, -188},  {0x84C8D4DFD2C63F3B, -661, -180},
      {0xC5DD44271AD3CDBA, -635, -172},  {0x936B9FCEBB25C996, -608, -164},
      {0xDBAC6C247D62A584, -582, -156},  {0xA3AB66580D5FDAF6, -555, -148},
      {0xF3E2F893DEC3F126, -529, -140},  {0xB5B5ADA8AAFF80B8, -502, -132},
      {0x87625F056C7C4A8B, -475, -124},  {0xC9BCFF6034C13053, -449, -116},
      {0x964E858C91BA2655, -422, -108},  {0xDFF9772470297EBD, -396, -100},
      {0xA6DFBD9FB8E5B88F, -369, -92},   {0xF8A95FCF88747D94, -343, -84},
      {0xB94470938FA89BCF, -316, -76},   {0x8A08F0F8BF0F156B, -289, -68},
      {0xCDB02555653131B6, -263, -60},   {0x993FE2C6D07B7FAC, -236, -52},
      {0xE45C10C42A2B3B06, -210, -44},   {0xAA242499697392D3, -183, -36},
      {0xFD87B5F28300CA0E, -157, -28},   {0xBCE5086492111AEB, -130, -20},
      {0x8CBCCC096F5088CC, -103, -12},   {0xD1B71758E219652C, -77, -4},
      {0x9C40000000000000, -50, 4},      {0xE8D4A51000000000, -24, 12},
      {0xAD78EBC5AC620000, 3, 20},       {0x813F3978F8940984, 30, 28},
      {0xC097CE7BC90715B3, 56, 36},      {0x8F7E32CE7BEA5C70, 83, 44},
      {0xD5D238A4ABE98068, 109, 52},     {0x9F4F2726179A2245, 136, 60},
      {0xED63A231D4C4FB27, 162, 68},     {0xB0DE65388CC8ADA8, 189, 76},
      {0x83C7088E1AAB65DB, 216, 84},     {0xC45D1DF942711D9A, 242, 92},
      {0x924D692CA61BE758, 269, 100},    {0xDA01EE641A708DEA, 295, 108},
      {0xA26DA3999AEF774A, 322, 116},    {0xF209787BB47D6B85, 348, 124},
      {0xB454E4A179DD1877, 375, 132},    {0x865B86925B9BC5C2, 402, 140},
      {0xC83553C5C8965D3D, 428, 148},    {0x952AB45CFA97A0B3, 455, 156},
      {0xDE469FBD99A05FE3, 481, 164},    {0xA59BC234DB398C25, 508, 172},
      {0xF6C69A72A3989F5C, 534, 180},    {0xB7DCBF5354E9BECE, 561, 188},
      {0x88FCF317F22241E2, 588, 196},    {0xCC20CE9BD35C78A5, 614, 204},
      {0x98165AF37B2153DF, 641, 212},    {0xE2A0B5DC971F303A, 667, 220},
      {0xA8D9D1535CE3B396, 694, 228},    {0xFB9B7CD9A4A7443C, 720, 236},
      {0xBB764C4CA7A44410, 747, 244},    {0x8BAB8EEFB6409C1A, 774, 252},
      {0xD01FEF10A657842C, 800, 260},    {0x9B10A4E5E9913129, 827, 268},
      {0xE7109BFBA19C0C9D, 853, 276},    {0xAC2820D9623BF429, 880, 284},
      {0x80444B5E7AA7CF85, 907, 292},    {0xBF21E44003ACDD2D, 933, 300},
      {0x8E679C2F5E44FF8F, 960, 308},    {0xD433179D9C8CB841, 986, 316},
      {0x9E19DB92B4E31BA9, 1013, 324},
  };
  const int k_cached_powers_min_dec_exp = -300;
  const int k_cached_powers_dec_step = 8;
  const int f = JD_ALPHA - e - 1;
  const int k = (f * 78913) / (1 << 18) + (int)(f > 0);
  const int index =
      (-k_cached_powers_min_dec_exp + k + (k_cached_powers_dec_step - 1)) /
      k_cached_powers_dec_step;
  return k_cached_powers[index];
}

static int jd_find_largest_pow10(uint32_t n, uint32_t *pow10) {
  if (n >= 1000000000) {
    *pow10 = 1000000000;
    return 10;
  }
  if (n >= 100000000) {
    *pow10 = 100000000;
    return 9;
  }
  if (n >= 10000000) {
    *pow10 = 10000000;
    return 8;
  }
  if (n >= 1000000) {
    *pow10 = 1000000;
    return 7;
  }
  if (n >= 100000) {
    *pow10 = 100000;
    return 6;
  }
  if (n >= 10000) {
    *pow10 = 10000;
    return 5;
  }
  if (n >= 1000) {
    *pow10 = 1000;
    return 4;
  }
  if (n >= 100) {
    *pow10 = 100;
    return 3;
  }
  if (n >= 10) {
    *pow10 = 10;
    return 2;
  }
  *pow10 = 1;
  return 1;
}

static void jd_grisu2_round(char *buf, int len, uint64_t dist, uint64_t delta,
                            uint64_t rest, uint64_t ten_k) {
  while (rest < dist && delta - rest >= ten_k &&
         (rest + ten_k < dist || dist - rest > rest + ten_k - dist)) {
    buf[len - 1]--;
    rest += ten_k;
  }
}

static void jd_grisu2_digit_gen(char *buffer, int *length,
                                int *decimal_exponent, jd_diyfp m_minus,
                                jd_diyfp w, jd_diyfp m_plus) {
  uint64_t delta = jd_sub(m_plus, m_minus).f;
  uint64_t dist = jd_sub(m_plus, w).f;
  const jd_diyfp one = jd_make((uint64_t)1 << -m_plus.e, m_plus.e);
  uint32_t p1 = (uint32_t)(m_plus.f >> -one.e);
  uint64_t p2 = m_plus.f & (one.f - 1);
  uint32_t pow10 = 0;
  const int k = jd_find_largest_pow10(p1, &pow10);
  int n = k;
  while (n > 0) {
    const uint32_t d = p1 / pow10;
    const uint32_t r = p1 % pow10;
    buffer[(*length)++] = (char)('0' + d);
    p1 = r;
    n--;
    const uint64_t rest = ((uint64_t)p1 << -one.e) + p2;
    if (rest <= delta) {
      *decimal_exponent += n;
      const uint64_t ten_n = (uint64_t)pow10 << -one.e;
      jd_grisu2_round(buffer, *length, dist, delta, rest, ten_n);
      return;
    }
    pow10 /= 10;
  }
  int m = 0;
  for (;;) {
    p2 *= 10;
    const uint64_t d = p2 >> -one.e;
    const uint64_t r = p2 & (one.f - 1);
    buffer[(*length)++] = (char)('0' + d);
    p2 = r;
    m++;
    delta *= 10;
    dist *= 10;
    if (p2 <= delta) {
      break;
    }
  }
  *decimal_exponent -= m;
  const uint64_t ten_m = one.f;
  jd_grisu2_round(buffer, *length, dist, delta, p2, ten_m);
}

static void jd_grisu2(char *buf, int *len, int *decimal_exponent,
                      double value) {
  jd_diyfp v, m_minus, m_plus;
  jd_compute_boundaries(value, &v, &m_minus, &m_plus);
  const jd_cached_power cached = jd_get_cached_power(m_plus.e);
  const jd_diyfp c_minus_k = jd_make(cached.f, cached.e);
  const jd_diyfp w = jd_mul(v, c_minus_k);
  const jd_diyfp w_minus = jd_mul(m_minus, c_minus_k);
  const jd_diyfp w_plus = jd_mul(m_plus, c_minus_k);
  const jd_diyfp mm = jd_make(w_minus.f + 1, w_minus.e);
  const jd_diyfp mp = jd_make(w_plus.f - 1, w_plus.e);
  *decimal_exponent = -cached.k;
  jd_grisu2_digit_gen(buf, len, decimal_exponent, mm, w, mp);
}

static char *jd_append_exponent(char *buf, int e) {
  if (e < 0) {
    e = -e;
    *buf++ = '-';
  } else {
    *buf++ = '+';
  }
  uint32_t k = (uint32_t)e;
  if (k < 10) {
    *buf++ = '0';
    *buf++ = (char)('0' + k);
  } else if (k < 100) {
    *buf++ = (char)('0' + k / 10);
    k %= 10;
    *buf++ = (char)('0' + k);
  } else {
    *buf++ = (char)('0' + k / 100);
    k %= 100;
    *buf++ = (char)('0' + k / 10);
    k %= 10;
    *buf++ = (char)('0' + k);
  }
  return buf;
}

static char *jd_format_buffer(char *buf, int len, int decimal_exponent,
                              int min_exp, int max_exp) {
  const int k = len;
  const int n = len + decimal_exponent;
  if (k <= n && n <= max_exp) {
    memset(buf + k, '0', (size_t)(n - k));
    buf[n + 0] = '.';
    buf[n + 1] = '0';
    return buf + (n + 2);
  }
  if (0 < n && n <= max_exp) {
    memmove(buf + (n + 1), buf + n, (size_t)(k - n));
    buf[n] = '.';
    return buf + (k + 1);
  }
  if (min_exp < n && n <= 0) {
    memmove(buf + (2 + -n), buf, (size_t)k);
    buf[0] = '0';
    buf[1] = '.';
    memset(buf + 2, '0', (size_t)-n);
    return buf + (2 + -n + k);
  }
  if (k == 1) {
    buf += 1;
  } else {
    memmove(buf + 2, buf + 1, (size_t)(k - 1));
    buf[1] = '.';
    buf += 1 + k;
  }
  *buf++ = 'e';
  return jd_append_exponent(buf, n - 1);
}

/**
 * @brief Write a double as nlohmann::json writes it
 *
 * @param out Buffer of at least 64 bytes; receives a null-terminated string
 * @param value The value; NaN and infinity give "null"
 */
static void json_format_double(char *out, double value) {
  if (!isfinite(value)) {
    strcpy(out, "null");
    return;
  }
  char *first = out;
  if (signbit(value)) {
    value = -value;
    *first++ = '-';
  }
  if (value == 0) {
    strcpy(first, "0.0");
    return;
  }
  int len = 0;
  int decimal_exponent = 0;
  jd_grisu2(first, &len, &decimal_exponent, value);
  /* kMinExp = -4, kMaxExp = digits10 of double = 15 */
  char *end = jd_format_buffer(first, len, decimal_exponent, -4, 15);
  *end = '\0';
}

#endif /* VIDEOPARSER_JSON_DOUBLE_H */

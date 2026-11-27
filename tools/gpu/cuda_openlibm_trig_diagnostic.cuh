#pragma once

#include <cstdint>

namespace pepepow_cuda_detail {

/*
 * Diagnostic-only CUDA translation of pinned OpenLibm sincos and its
 * argument-reduction routines:
 *   src/s_sincos.c, src/e_rem_pio2.c, src/k_rem_pio2.c
 * JuliaMath/openlibm commit 9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d.
 *
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 * Permission to use, copy, modify, and distribute this software is freely
 * granted, provided that this notice is preserved.
 *
 * Copyright (C) 2013 Elliot Saba. All rights reserved.
 * Permission to use, copy, modify, and distribute this software is freely
 * granted, provided that this notice is preserved.
 *
 * The 2/pi table is the binary64-sized table from pinned src/k_rem_pio2.c.
 * This bounded diagnostic is not a production hash path.
 */
static __device__ __constant__ std::int32_t kOpenlibmTwoOverPi[66] = {
  0xA2F983, 0x6E4E44, 0x1529FC, 0x2757D1, 0xF534DD, 0xC0DB62,
  0x95993C, 0x439041, 0xFE5163, 0xABDEBB, 0xC561B7, 0x246E3A,
  0x424DD2, 0xE00649, 0x2EEA09, 0xD1921C, 0xFE1DEB, 0x1CB129,
  0xA73EE8, 0x8235F5, 0x2EBB44, 0x84E99C, 0x7026B4, 0x5F7E41,
  0x3991D6, 0x398353, 0x39F49C, 0x845F8B, 0xBDF928, 0x3B1FF8,
  0x97FFDE, 0x05980F, 0xEF2F11, 0x8B5A0A, 0x6D1F6D, 0x367ECF,
  0x27CB09, 0xB74F46, 0x3F669E, 0x5FEA2D, 0x7527BA, 0xC7EBE5,
  0xF17B3D, 0x0739F7, 0x8A5292, 0xEA6BFB, 0x5FB11F, 0x8D5D08,
  0x560330, 0x46FC7B, 0x6BABF0, 0xCFBC20, 0x9AF436, 0x1DA9E3,
  0x91615E, 0xE61B08, 0x659985, 0x5F14A0, 0x68408D, 0xFFD880,
  0x4D7327, 0x310606, 0x1556CA, 0x73A8C9, 0x60E27B, 0xC08C6B,
};

__device__ __forceinline__ std::uint32_t openlibmHighWord(double value) {
  return static_cast<std::uint32_t>(
      static_cast<std::uint64_t>(__double_as_longlong(value)) >> 32U);
}

__device__ __forceinline__ std::uint32_t openlibmLowWord(double value) {
  return static_cast<std::uint32_t>(
      static_cast<std::uint64_t>(__double_as_longlong(value)));
}

__device__ __forceinline__ double openlibmFromWords(std::uint32_t high,
                                                    std::uint32_t low) {
  const std::uint64_t bits = (static_cast<std::uint64_t>(high) << 32U) | low;
  return __longlong_as_double(static_cast<long long>(bits));
}

__device__ __forceinline__ int openlibmKernelRemPio2(const double *x,
                                                      double *y,
                                                      int e0,
                                                      int nx) {
  constexpr double zero = 0.0;
  constexpr double one = 1.0;
  constexpr double two24 = 1.67772160000000000000e+07;
  constexpr double two_minus_24 = 5.96046447753906250000e-08;
  constexpr double pio2_parts[8] = {
      1.57079625129699707031e+00,
      7.54978941586159635335e-08,
      5.39030252995776476554e-15,
      3.28200341580791294123e-22,
      1.27065575308067607349e-29,
      1.22933308981111328932e-36,
      2.73370053816464559624e-44,
      2.16741683877804819444e-51,
  };

  int jz = 0;
  const int jx = nx - 1;
  int jv = (e0 - 3) / 24;
  if (jv < 0) jv = 0;
  int q0 = e0 - 24 * (jv + 1);
  constexpr int jk = 4;
  constexpr int jp = 4;
  int iq[20];
  double f[20];
  double fq[20];
  double q[20];
  int carry = 0;
  int k = 0;
  int n = 0;
  int ih = 0;
  double z = 0.0;
  double fw = 0.0;

  int j = jv - jx;
  const int m = jx + jk;
  for (int i = 0; i <= m; ++i, ++j) {
    f[i] = (j < 0) ? zero : static_cast<double>(kOpenlibmTwoOverPi[j]);
  }
  for (int i = 0; i <= jk; ++i) {
    for (j = 0, fw = 0.0; j <= jx; ++j) {
      fw += x[j] * f[jx + i - j];
    }
    q[i] = fw;
  }
  jz = jk;

recompute:
  z = q[jz];
  for (int i = 0, index = jz; index > 0; ++i, --index) {
    fw = static_cast<double>(static_cast<std::int32_t>(two_minus_24 * z));
    iq[i] = static_cast<std::int32_t>(z - two24 * fw);
    z = q[index - 1] + fw;
  }

  z = scalbn(z, q0);
  z -= 8.0 * floor(z * 0.125);
  n = static_cast<std::int32_t>(z);
  z -= static_cast<double>(n);
  ih = 0;
  if (q0 > 0) {
    const int i = iq[jz - 1] >> (24 - q0);
    n += i;
    iq[jz - 1] -= i << (24 - q0);
    ih = iq[jz - 1] >> (23 - q0);
  } else if (q0 == 0) {
    ih = iq[jz - 1] >> 23;
  } else if (z >= 0.5) {
    ih = 2;
  }

  if (ih > 0) {
    ++n;
    carry = 0;
    for (int i = 0; i < jz; ++i) {
      j = iq[i];
      if (carry == 0) {
        if (j != 0) {
          carry = 1;
          iq[i] = 0x1000000 - j;
        }
      } else {
        iq[i] = 0xffffff - j;
      }
    }
    if (q0 > 0) {
      if (q0 == 1) iq[jz - 1] &= 0x7fffff;
      if (q0 == 2) iq[jz - 1] &= 0x3fffff;
    }
    if (ih == 2) {
      z = one - z;
      if (carry != 0) z -= scalbn(one, q0);
    }
  }

  if (z == zero) {
    j = 0;
    for (int i = jz - 1; i >= jk; --i) j |= iq[i];
    if (j == 0) {
      int k = 1;
      while (iq[jk - k] == 0) ++k;
      for (int i = jz + 1; i <= jz + k; ++i) {
        f[jx + i] = static_cast<double>(kOpenlibmTwoOverPi[jv + i]);
        for (j = 0, fw = 0.0; j <= jx; ++j) {
          fw += x[j] * f[jx + i - j];
        }
        q[i] = fw;
      }
      jz += k;
      z = q[jz];
      goto recompute;
    }
  }

  if (z == 0.0) {
    --jz;
    q0 -= 24;
    while (iq[jz] == 0) {
      --jz;
      q0 -= 24;
    }
  } else {
    z = scalbn(z, -q0);
    if (z >= two24) {
      fw = static_cast<double>(static_cast<std::int32_t>(two_minus_24 * z));
      iq[jz] = static_cast<std::int32_t>(z - two24 * fw);
      ++jz;
      q0 += 24;
      iq[jz] = static_cast<std::int32_t>(fw);
    } else {
      iq[jz] = static_cast<std::int32_t>(z);
    }
  }

  fw = scalbn(one, q0);
  for (int i = jz; i >= 0; --i) {
    q[i] = fw * static_cast<double>(iq[i]);
    fw *= two_minus_24;
  }
  for (int i = jz; i >= 0; --i) {
    for (fw = 0.0, k = 0; k <= jp && k <= jz - i; ++k) {
      fw += pio2_parts[k] * q[i + k];
    }
    fq[jz - i] = fw;
  }

  fw = 0.0;
  for (int i = jz; i >= 0; --i) fw += fq[i];
  y[0] = (ih == 0) ? fw : -fw;
  fw = fq[0] - fw;
  for (int i = 1; i <= jz; ++i) fw += fq[i];
  y[1] = (ih == 0) ? fw : -fw;
  return n & 7;
}

__device__ __forceinline__ int openlibmRemPio2(double x, double *y) {
  constexpr double zero = 0.0;
  constexpr double two24 = 1.67772160000000000000e+07;
  constexpr double inverse_pio2 = 6.36619772367581382433e-01;
  constexpr double pio2_1 = 1.57079632673412561417e+00;
  constexpr double pio2_1_tail = 6.07710050650619224932e-11;
  constexpr double pio2_2 = 6.07710050630396597660e-11;
  constexpr double pio2_2_tail = 2.02226624879595063154e-21;
  constexpr double pio2_3 = 2.02226624871116645580e-21;
  constexpr double pio2_3_tail = 8.47842766036889956997e-32;

  const std::uint32_t high = openlibmHighWord(x);
  const std::uint32_t ix = high & 0x7fffffffU;
  const bool positive = (high & 0x80000000U) == 0;
  double z = 0.0;
  double w = 0.0;
  double r = 0.0;
  double fn = 0.0;
  double tx[3];
  double ty[2];
  int e0 = 0;
  int i = 0;
  int j = 0;
  int nx = 0;
  int n = 0;

  if (ix <= 0x400f6a7aU) {
    if ((ix & 0x000fffffU) == 0x000921fbU) goto medium;
    if (ix <= 0x4002d97cU) {
      if (positive) {
        z = x - pio2_1;
        y[0] = z - pio2_1_tail;
        y[1] = (z - y[0]) - pio2_1_tail;
        return 1;
      }
      z = x + pio2_1;
      y[0] = z + pio2_1_tail;
      y[1] = (z - y[0]) + pio2_1_tail;
      return -1;
    }
    if (positive) {
      z = x - 2.0 * pio2_1;
      y[0] = z - 2.0 * pio2_1_tail;
      y[1] = (z - y[0]) - 2.0 * pio2_1_tail;
      return 2;
    }
    z = x + 2.0 * pio2_1;
    y[0] = z + 2.0 * pio2_1_tail;
    y[1] = (z - y[0]) + 2.0 * pio2_1_tail;
    return -2;
  }

  if (ix <= 0x401c463bU) {
    if (ix <= 0x4015fdbcU) {
      if (ix == 0x4012d97cU) goto medium;
      if (positive) {
        z = x - 3.0 * pio2_1;
        y[0] = z - 3.0 * pio2_1_tail;
        y[1] = (z - y[0]) - 3.0 * pio2_1_tail;
        return 3;
      }
      z = x + 3.0 * pio2_1;
      y[0] = z + 3.0 * pio2_1_tail;
      y[1] = (z - y[0]) + 3.0 * pio2_1_tail;
      return -3;
    }
    if (ix == 0x401921fbU) goto medium;
    if (positive) {
      z = x - 4.0 * pio2_1;
      y[0] = z - 4.0 * pio2_1_tail;
      y[1] = (z - y[0]) - 4.0 * pio2_1_tail;
      return 4;
    }
    z = x + 4.0 * pio2_1;
    y[0] = z + 4.0 * pio2_1_tail;
    y[1] = (z - y[0]) + 4.0 * pio2_1_tail;
    return -4;
  }

  if (ix < 0x413921fbU) {
medium:
    fn = x * inverse_pio2 + 0x1.8p52;
    fn = fn - 0x1.8p52;
    n = __double2int_rn(fn);
    r = x - fn * pio2_1;
    w = fn * pio2_1_tail;
    j = static_cast<int>(ix >> 20U);
    y[0] = r - w;
    int i_high = static_cast<int>(openlibmHighWord(y[0]));
    i = j - ((static_cast<std::uint32_t>(i_high) >> 20U) & 0x7ffU);
    if (i > 16) {
      const double t = r;
      w = fn * pio2_2;
      r = t - w;
      w = fn * pio2_2_tail - ((t - r) - w);
      y[0] = r - w;
      i_high = static_cast<int>(openlibmHighWord(y[0]));
      i = j - ((static_cast<std::uint32_t>(i_high) >> 20U) & 0x7ffU);
      if (i > 49) {
        const double t = r;
        w = fn * pio2_3;
        r = t - w;
        w = fn * pio2_3_tail - ((t - r) - w);
        y[0] = r - w;
      }
    }
    y[1] = (r - y[0]) - w;
    return n;
  }

  if (ix >= 0x7ff00000U) {
    y[0] = x - x;
    y[1] = y[0];
    return 0;
  }
  const std::uint32_t low = openlibmLowWord(x);
  e0 = static_cast<int>(ix >> 20U) - 1046;
  z = openlibmFromWords(
      ix - static_cast<std::uint32_t>(e0 << 20), low);
  for (i = 0; i < 2; ++i) {
    tx[i] = static_cast<double>(static_cast<std::int32_t>(z));
    z = (z - tx[i]) * two24;
  }
  tx[2] = z;
  nx = 3;
  while (tx[nx - 1] == zero) --nx;
  n = openlibmKernelRemPio2(tx, ty, e0, nx);
  if (!positive) {
    y[0] = -ty[0];
    y[1] = -ty[1];
    return -n;
  }
  y[0] = ty[0];
  y[1] = ty[1];
  return n;
}

__device__ __forceinline__ void openlibmKernelSincos(double x,
                                                      double y,
                                                      int iy,
                                                      double *sine,
                                                      double *cosine) {
  constexpr double one = 1.0;
  constexpr double half = 0.5;
  constexpr double s1 = -1.66666666666666324348e-01;
  constexpr double s2 = 8.33333333332248946124e-03;
  constexpr double s3 = -1.98412698298579493134e-04;
  constexpr double s4 = 2.75573137070700676789e-06;
  constexpr double s5 = -2.50507602534068634195e-08;
  constexpr double s6 = 1.58969099521155010221e-10;
  constexpr double c1 = 4.16666666666666019037e-02;
  constexpr double c2 = -1.38888888888741095749e-03;
  constexpr double c3 = 2.48015872894767294178e-05;
  constexpr double c4 = -2.75573143513906633035e-07;
  constexpr double c5 = 2.08757232129817482790e-09;
  constexpr double c6 = -1.13596475577881948265e-11;

  const double z = x * x;
  const double w = z * z;
  const double cosine_r =
      z * (c1 + z * (c2 + z * c3)) + w * w * (c4 + z * (c5 + z * c6));
  const double hz = 0.5 * z;
  const double cosine_v = one - hz;
  *cosine = cosine_v + (((one - cosine_v) - hz) + (z * cosine_r - x * y));

  const double sine_r = s2 + z * (s3 + z * s4) + z * w * (s5 + z * s6);
  const double sine_v = z * x;
  if (iy == 0) {
    *sine = x + sine_v * (s1 + z * sine_r);
  } else {
    *sine = x - ((z * (half * y - sine_v * sine_r) - y) - sine_v * s1);
  }
}

__device__ __forceinline__ void openlibmSinCosDiagnostic(double x,
                                                         double *sine,
                                                         double *cosine) {
  constexpr double one = 1.0;
  double y[2];
  const std::uint32_t ix = openlibmHighWord(x) & 0x7fffffffU;
  if (ix <= 0x3fe921fbU) {
    if (ix < 0x3e46a09eU && static_cast<int>(x) == 0) {
      *sine = x;
      *cosine = one;
      return;
    }
    openlibmKernelSincos(x, 0.0, 0, sine, cosine);
    return;
  }
  if (ix >= 0x7ff00000U) {
    *sine = x - x;
    *cosine = x - x;
    return;
  }

  const int n = openlibmRemPio2(x, y);
  double reduced_sine = 0.0;
  double reduced_cosine = 0.0;
  openlibmKernelSincos(y[0], y[1], 1, &reduced_sine, &reduced_cosine);
  switch (n & 3) {
    case 0:
      *cosine = reduced_cosine;
      *sine = reduced_sine;
      break;
    case 1:
      *cosine = -reduced_sine;
      *sine = reduced_cosine;
      break;
    case 2:
      *cosine = -reduced_cosine;
      *sine = -reduced_sine;
      break;
    default:
      *cosine = reduced_sine;
      *sine = -reduced_cosine;
      break;
  }
}

__device__ __forceinline__ double openlibmIntermediateDiagnostic(double x) {
  constexpr double pi_over_two = 1.57079632679489661923;
  constexpr double three_pi_over_two = 4.71238898038468985769;
  constexpr double epsilon = 1.0e-9;
  if (fabs(x - pi_over_two) < epsilon ||
      fabs(x - three_pi_over_two) < epsilon) {
    return 0.0;
  }
  double sine = 0.0;
  double cosine = 0.0;
  openlibmSinCosDiagnostic(x, &sine, &cosine);
  return sine * sine;
}

}  // namespace pepepow_cuda_detail

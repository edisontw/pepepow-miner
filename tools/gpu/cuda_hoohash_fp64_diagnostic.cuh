#pragma once

#include <cstdint>

#include "cuda_openlibm_trig_diagnostic.cuh"

/*
 * Diagnostic-only CUDA translation of pinned OpenLibm __ieee754_exp from
 * JuliaMath/openlibm src/e_exp.c, commit
 * 9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d (Git blob
 * 639b9fe18dbae273d3182146bb9aebbd59c9e362).
 *
 * Copyright (C) 2004 by Sun Microsystems, Inc. All rights reserved.
 * Permission to use, copy, modify, and distribute this software is freely
 * granted, provided that this notice is preserved.
 *
 * This header connects already validated OpenLibm translations for a
 * full-digest correctness experiment. It is not a production miner path.
 */
namespace pepepow_cuda_v110 {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEpsilon = 1.0e-9;
constexpr double kComplexTransformMultiplier = 0.000001;

__device__ __forceinline__ double openlibmExp(double value) {
  constexpr double one = 1.0;
  constexpr double half[2] = {0.5, -0.5};
  constexpr double huge = 1.0e300;
  constexpr double overflow_threshold = 7.09782712893383973096e+02;
  constexpr double underflow_threshold = -7.45133219101941108420e+02;
  constexpr double ln2_high[2] = {
      6.93147180369123816490e-01, -6.93147180369123816490e-01};
  constexpr double ln2_low[2] = {
      1.90821492927058770002e-10, -1.90821492927058770002e-10};
  constexpr double inverse_ln2 = 1.44269504088896338700e+00;
  constexpr double p1 = 1.66666666666666019037e-01;
  constexpr double p2 = -2.77777777770155933842e-03;
  constexpr double p3 = 6.61375632143793436117e-05;
  constexpr double p4 = -1.65339022054652515390e-06;
  constexpr double p5 = 4.13813679705723846039e-08;
  constexpr double two_to_minus_1000 = 9.33263618503218878990e-302;

  const std::uint64_t raw =
      static_cast<std::uint64_t>(__double_as_longlong(value));
  const std::uint32_t high_word = static_cast<std::uint32_t>(raw >> 32U);
  const std::uint32_t absolute_high_word = high_word & 0x7fffffffU;
  const std::uint32_t sign = high_word >> 31U;
  if (absolute_high_word >= 0x40862e42U) {
    if (absolute_high_word >= 0x7ff00000U) {
      const std::uint32_t low_word = static_cast<std::uint32_t>(raw);
      if (((absolute_high_word & 0x000fffffU) | low_word) != 0U) {
        return value + value;
      }
      return sign == 0U ? value : 0.0;
    }
    if (value > overflow_threshold) return huge * huge;
    if (value < underflow_threshold) {
      return two_to_minus_1000 * two_to_minus_1000;
    }
  }

  if (value == 1.0) return 2.718281828459045235360;

  double x = value;
  double high = 0.0;
  double low = 0.0;
  double scale = 1.0;
  std::int32_t exponent = 0;
  if (absolute_high_word > 0x3fd62e42U) {
    if (absolute_high_word < 0x3ff0a2b2U) {
      high = x - ln2_high[sign];
      low = ln2_low[sign];
      exponent = 1 - static_cast<std::int32_t>(sign) -
                 static_cast<std::int32_t>(sign);
    } else {
      exponent = static_cast<std::int32_t>(inverse_ln2 * x + half[sign]);
      const double exponent_as_double = static_cast<double>(exponent);
      high = x - exponent_as_double * ln2_high[0];
      low = exponent_as_double * ln2_low[0];
    }
    x = high - low;
  } else if (absolute_high_word < 0x3e300000U) {
    if (huge + x > one) return one + x;
  }

  const double squared = x * x;
  if (exponent >= -1021) {
    const std::uint32_t scale_high_word = static_cast<std::uint32_t>(
        0x3ff00000 + exponent * 0x00100000);
    scale = __longlong_as_double(static_cast<long long>(
        static_cast<std::uint64_t>(scale_high_word) << 32U));
  } else {
    const std::uint32_t scale_high_word = static_cast<std::uint32_t>(
        0x3ff00000 + (exponent + 1000) * 0x00100000);
    scale = __longlong_as_double(static_cast<long long>(
        static_cast<std::uint64_t>(scale_high_word) << 32U));
  }
  const double c = x - squared *
      (p1 + squared * (p2 + squared * (p3 + squared * (p4 + squared * p5))));
  if (exponent == 0) return one - ((x * c) / (c - 2.0) - x);
  const double y = one - ((low - (x * c) / (2.0 - c)) - high);
  if (exponent >= -1021) {
    if (exponent == 1024) return y * 2.0 * 0x1p1023;
    return y * scale;
  }
  return y * scale * two_to_minus_1000;
}

__device__ __forceinline__ double complexNonLinear(double value) {
  const double scaled_one = value * kComplexTransformMultiplier;
  const double scaled_two = value * kComplexTransformMultiplier;
  const double quotient_one = __ddiv_rn(scaled_one, 8.0);
  const double quotient_two = __ddiv_rn(scaled_two, 4.0);
  const double factor_one = quotient_one - floor(quotient_one);
  const double factor_two = quotient_two - floor(quotient_two);
  const std::uint32_t family = factor_one < 0.33 ? 0U :
                               factor_one < 0.66 ? 1U : 2U;
  const std::uint32_t operation = factor_two < 0.25 ? 0U :
                                  factor_two < 0.5 ? 1U :
                                  factor_two < 0.75 ? 2U : 3U;
  const double adjustment = 1.0 + factor_two;
  double argument = 0.0;
  switch (operation) {
    case 0:
      argument = value + adjustment;
      break;
    case 1:
      argument = value - adjustment;
      break;
    case 2:
      argument = value * adjustment;
      break;
    default:
      argument = __ddiv_rn(value, adjustment);
      break;
  }

  if (family == 0U) {
    double sine = 0.0;
    double cosine = 0.0;
    pepepow_cuda_detail::openlibmSinCosDiagnostic(
        argument, &sine, &cosine);
    return openlibmExp(sine + cosine);
  }
  if (family == 1U) {
    return pepepow_cuda_detail::openlibmIntermediateDiagnostic(argument);
  }
  return __ddiv_rn(1.0, __dsqrt_rn(fabs(argument) + 1.0));
}

__device__ __forceinline__ double safeComplexTransform(double input) {
  const double transformed_value = complexNonLinear(input);
  double scaled_input = input;
  double rounds = 1.0;
  while (isnan(transformed_value) || isinf(transformed_value)) {
    scaled_input *= 0.1;
    if (scaled_input <= 0.0000000000001) return 0.0 * rounds;
    rounds += 1.0;
  }
  return transformed_value * rounds;
}

__device__ __forceinline__ double transformFactor(double value) {
  const double quotient = __ddiv_rn(value, 1024.0);
  return quotient - floor(quotient);
}

}  // namespace pepepow_cuda_v110

// Copyright 2010-2021 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ortools/util/fp_utils.h"

#include <limits.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "absl/base/casts.h"
#include "absl/base/internal/endian.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/util/bitset.h"

namespace operations_research {

namespace {

void ReorderAndCapTerms(double* min, double* max) {
  if (*min > *max) std::swap(*min, *max);
  if (*min > 0.0) *min = 0.0;
  if (*max < 0.0) *max = 0.0;
}

template <bool use_bounds>
void ComputeScalingErrors(const std::vector<double>& input,
                          const std::vector<double>& lb,
                          const std::vector<double>& ub, double scaling_factor,
                          double* max_relative_coeff_error,
                          double* max_scaled_sum_error) {
  double max_error = 0.0;
  double min_error = 0.0;
  *max_relative_coeff_error = 0.0;
  const int size = input.size();
  for (int i = 0; i < size; ++i) {
    const double x = input[i];
    if (x == 0.0) continue;
    const double scaled = x * scaling_factor;

    if (scaled == 0.0) {
      *max_relative_coeff_error = std::numeric_limits<double>::infinity();
    } else {
      *max_relative_coeff_error = std::max(
          *max_relative_coeff_error, std::abs(std::round(scaled) / scaled - 1));
    }

    const double error = std::round(scaled) - scaled;
    const double error_lb = (use_bounds ? error * lb[i] : -error);
    const double error_ub = (use_bounds ? error * ub[i] : error);
    max_error += std::max(error_lb, error_ub);
    min_error += std::min(error_lb, error_ub);
  }
  *max_scaled_sum_error = std::max(std::abs(max_error), std::abs(min_error));
}

template <bool use_bounds>
void GetBestScalingOfDoublesToInt64(const std::vector<double>& input,
                                    const std::vector<double>& lb,
                                    const std::vector<double>& ub,
                                    int64_t max_absolute_sum,
                                    double* scaling_factor) {
  const double kInfinity = std::numeric_limits<double>::infinity();

  // We start by initializing the returns value to the "error" state.
  *scaling_factor = 0;

  // Abort in the "error" state if max_absolute_sum doesn't make sense.
  if (max_absolute_sum < 0) return;

  // Our scaling scaling_factor will be 2^factor_exponent.
  //
  // TODO(user): Consider using a non-power of two factor if the error can't be
  // zero? Note however that using power of two has the extra advantage that
  // subsequent int64_t -> double -> scaled back to int64_t will loose no extra
  // information.
  int factor_exponent = 0;
  uint64_t sum_min = 0;  // negated.
  uint64_t sum_max = 0;
  bool recompute_sum = false;
  bool is_first_value = true;
  const int msb = MostSignificantBitPosition64(max_absolute_sum);
  const int size = input.size();
  for (int i = 0; i < size; ++i) {
    const double x = input[i];
    double min_term = use_bounds ? x * lb[i] : -x;
    double max_term = use_bounds ? x * ub[i] : x;
    ReorderAndCapTerms(&min_term, &max_term);

    // If min_term or max_term is not finite, then abort in the "error" state.
    if (!(min_term > -kInfinity && max_term < kInfinity)) return;

    // A value of zero can just be skipped (and needs to because the code below
    // doesn't handle it correctly).
    if (min_term == 0.0 && max_term == 0.0) continue;

    // Compute the greatest candidate such that
    // round(fabs(c).2^candidate) <= max_absolute_sum.
    const double c = std::max(-min_term, max_term);
    int candidate = msb - ilogb(c);
    if (std::round(ldexp(std::abs(c), candidate)) > max_absolute_sum) {
      --candidate;
    }
    DCHECK_LE(std::abs(static_cast<int64_t>(round(ldexp(c, candidate)))),
              max_absolute_sum);

    // Update factor_exponent which is the min of all the candidates.
    if (is_first_value || candidate < factor_exponent) {
      is_first_value = false;
      factor_exponent = candidate;
      recompute_sum = true;
    } else {
      // Update the sum of absolute values of the numbers seen so far.
      sum_min -=
          static_cast<int64_t>(std::round(ldexp(min_term, factor_exponent)));
      sum_max +=
          static_cast<int64_t>(std::round(ldexp(max_term, factor_exponent)));
      if (sum_min > max_absolute_sum || sum_max > max_absolute_sum) {
        factor_exponent--;
        recompute_sum = true;
      }
    }

    // TODO(user): This is not super efficient, but note that in practice we
    // will only scan the vector of values about log(size) times. It is possible
    // to maintain an upper bound on the abs_sum in linear time instead, but the
    // code and corner cases are a lot more involved. Also, we currently only
    // use this code in situations where its run-time is negligeable compared to
    // the rest.
    while (recompute_sum) {
      sum_min = 0;
      sum_max = 0;
      for (int j = 0; j <= i; ++j) {
        const double x = input[j];
        double min_term = use_bounds ? x * lb[j] : -x;
        double max_term = use_bounds ? x * ub[j] : x;
        ReorderAndCapTerms(&min_term, &max_term);
        sum_min -=
            static_cast<int64_t>(std::round(ldexp(min_term, factor_exponent)));
        sum_max +=
            static_cast<int64_t>(std::round(ldexp(max_term, factor_exponent)));
      }
      if (sum_min > max_absolute_sum || sum_max > max_absolute_sum) {
        factor_exponent--;
      } else {
        recompute_sum = false;
      }
    }
  }
  *scaling_factor = ldexp(1.0, factor_exponent);
}

}  // namespace

void ComputeScalingErrors(const std::vector<double>& input,
                          const std::vector<double>& lb,
                          const std::vector<double>& ub, double scaling_factor,
                          double* max_relative_coeff_error,
                          double* max_scaled_sum_error) {
  ComputeScalingErrors<true>(input, lb, ub, scaling_factor,
                             max_relative_coeff_error, max_scaled_sum_error);
}

double GetBestScalingOfDoublesToInt64(const std::vector<double>& input,
                                      const std::vector<double>& lb,
                                      const std::vector<double>& ub,
                                      int64_t max_absolute_sum) {
  double scaling_factor;
  GetBestScalingOfDoublesToInt64<true>(input, lb, ub, max_absolute_sum,
                                       &scaling_factor);
  return scaling_factor;
}

void GetBestScalingOfDoublesToInt64(const std::vector<double>& input,
                                    int64_t max_absolute_sum,
                                    double* scaling_factor,
                                    double* max_relative_coeff_error) {
  double max_scaled_sum_error;
  GetBestScalingOfDoublesToInt64<false>(input, {}, {}, max_absolute_sum,
                                        scaling_factor);
  ComputeScalingErrors<false>(input, {}, {}, *scaling_factor,
                              max_relative_coeff_error, &max_scaled_sum_error);
}

int64_t ComputeGcdOfRoundedDoubles(const std::vector<double>& x,
                                   double scaling_factor) {
  int64_t gcd = 0;
  for (int i = 0; i < x.size() && gcd != 1; ++i) {
    int64_t value = std::abs(std::round(x[i] * scaling_factor));
    DCHECK_GE(value, 0);
    if (value == 0) continue;
    if (gcd == 0) {
      gcd = value;
      continue;
    }
    // GCD(gcd, value) = GCD(value, gcd % value);
    while (value != 0) {
      const int64_t r = gcd % value;
      gcd = value;
      value = r;
    }
  }
  DCHECK_GE(gcd, 0);
  return gcd > 0 ? gcd : 1;
}

int fast_ilogb(double value) {
  static_assert(CHAR_BIT == 8);
  static_assert(sizeof(double) == 8);
  // Get little-endian bit-representation of the floating point value.
  const uint64_t bit_rep =
      absl::little_endian::FromHost64(absl::bit_cast<uint64_t>(value));
  return static_cast<int>((bit_rep >> 52) & 0x7FF) - 1023;
}

void fast_scalbn_inplace(double& mutable_value, int exponent) {
  mutable_value = fast_scalbn(mutable_value, exponent);
}

double fast_scalbn(double value, int exponent) {
  if (value == 0.0) return 0.0;
  uint64_t bit_rep =
      absl::little_endian::FromHost64(absl::bit_cast<uint64_t>(value));
  // Binary representation is: (sign-bit)(11 exponent bits)(52 mantissa bits)
  constexpr uint64_t kExponentMask(0x7FF0000000000000);
  // This addition relies on the fact that signed numbers are written in
  // two-s complement, and is correct as long as the sum does not
  // overflow/underflow the result.
  const uint64_t value_exponent =
      (bit_rep + (static_cast<uint64_t>(exponent) << 52)) & kExponentMask;
  bit_rep &= ~kExponentMask;
  bit_rep |= value_exponent;
  return absl::bit_cast<double>(absl::little_endian::ToHost64(bit_rep));
}

}  // namespace operations_research

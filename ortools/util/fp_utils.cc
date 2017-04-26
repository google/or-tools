// Copyright 2010-2014 Google
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

#include <cmath>

#include "ortools/util/bitset.h"

namespace operations_research {

namespace {
void ReorderAndCapTerms(double* min, double* max) {
  if (*min > *max) std::swap(*min, *max);
  if (*min > 0.0) *min = 0.0;
  if (*max < 0.0) *max = 0.0;
}
}  // namespace

template <bool use_bounds>
void GetBestScalingOfDoublesToInt64(const std::vector<double>& input,
                                    const std::vector<double>& lb,
                                    const std::vector<double>& ub,
                                    int64 max_absolute_sum,
                                    double* scaling_factor,
                                    double* max_relative_coeff_error,
                                    double* max_scaled_sum_error) {
  const double kInfinity = std::numeric_limits<double>::infinity();

  // We start by initializing the returns value to the "error" state.
  *scaling_factor = 0;
  *max_relative_coeff_error = kInfinity;
  *max_scaled_sum_error = kInfinity;

  // Abort in the "error" state if max_absolute_sum doesn't make sense.
  if (max_absolute_sum < 0) return;

  // Our scaling scaling_factor will be 2^factor_exponent.
  //
  // TODO(user): Consider using a non-power of two factor if the error can't be
  // zero? Note however that using power of two has the extra advantage that
  // subsequent int64 -> double -> scaled back to int64 will loose no extra
  // information.
  int factor_exponent = 0;
  uint64 sum_min = 0;  // negated.
  uint64 sum_max = 0;
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
    DCHECK_LE(std::abs(static_cast<int64>(round(ldexp(c, candidate)))),
              max_absolute_sum);

    // Update factor_exponent which is the min of all the candidates.
    if (is_first_value || candidate < factor_exponent) {
      is_first_value = false;
      factor_exponent = candidate;
      recompute_sum = true;
    } else {
      // Update the sum of absolute values of the numbers seen so far.
      sum_min -=
          static_cast<int64>(std::round(ldexp(min_term, factor_exponent)));
      sum_max +=
          static_cast<int64>(std::round(ldexp(max_term, factor_exponent)));
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
            static_cast<int64>(std::round(ldexp(min_term, factor_exponent)));
        sum_max +=
            static_cast<int64>(std::round(ldexp(max_term, factor_exponent)));
      }
      if (sum_min > max_absolute_sum || sum_max > max_absolute_sum) {
        factor_exponent--;
      } else {
        recompute_sum = false;
      }
    }
  }
  *scaling_factor = ldexp(1.0, factor_exponent);

  // Compute the worst case errors.
  double max_positive_error = 0.0;
  double max_negative_error = 0.0;
  *max_relative_coeff_error = 0.0;
  for (int i = 0; i < size; ++i) {
    const double x = input[i];
    if (x == 0.0) continue;
    const double scaled = std::abs(ldexp(x, factor_exponent));
    *max_relative_coeff_error = std::max(
        *max_relative_coeff_error, std::abs(std::round(scaled) / scaled - 1));
    const double error = std::round(scaled) - scaled;
    const double error_a = error * (use_bounds ? x * lb[i] : -x);
    const double error_b = error * (use_bounds ? x * ub[i] : x);
    max_positive_error += std::max(0.0, std::max(error_a, error_b));
    max_negative_error += std::max(0.0, std::max(-error_a, -error_b));
  }
  *max_scaled_sum_error = std::max(max_positive_error, max_negative_error);
}

void GetBestScalingOfDoublesToInt64(const std::vector<double>& input,
                                    const std::vector<double>& lb,
                                    const std::vector<double>& ub,
                                    int64 max_absolute_sum,
                                    double* scaling_factor,
                                    double* max_relative_coeff_error,
                                    double* max_scaled_sum_error) {
  return GetBestScalingOfDoublesToInt64<true>(
      input, lb, ub, max_absolute_sum, scaling_factor, max_relative_coeff_error,
      max_scaled_sum_error);
}

void GetBestScalingOfDoublesToInt64(const std::vector<double>& input,
                                    int64 max_absolute_sum,
                                    double* scaling_factor,
                                    double* max_relative_coeff_error) {
  double max_scaled_sum_error;
  return GetBestScalingOfDoublesToInt64<false>(
      input, {}, {}, max_absolute_sum, scaling_factor, max_relative_coeff_error,
      &max_scaled_sum_error);
}

int64 ComputeGcdOfRoundedDoubles(const std::vector<double>& x,
                                 double scaling_factor) {
  int64 gcd = 0;
  for (int i = 0; i < x.size() && gcd != 1; ++i) {
    int64 value = round(fabs(x[i] * scaling_factor));
    DCHECK_GE(value, 0);
    if (value == 0) continue;
    if (gcd == 0) {
      gcd = value;
      continue;
    }
    // GCD(gcd, value) = GCD(value, gcd % value);
    while (value != 0) {
      const int64 r = gcd % value;
      gcd = value;
      value = r;
    }
  }
  DCHECK_GE(gcd, 0);
  return gcd > 0 ? gcd : 1;
}

}  // namespace operations_research

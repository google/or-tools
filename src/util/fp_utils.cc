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

#include "util/fp_utils.h"

#include <cmath>

#include "util/bitset.h"

namespace operations_research {

void GetBestScalingOfDoublesToInt64(const std::vector<double>& input,
                                    int64 max_absolute_sum,
                                    double* scaling_factor,
                                    double* relative_error) {
  const double kInfinity = std::numeric_limits<double>::infinity();

  // We start by initializing the returns value to the "error" state.
  *scaling_factor = 0;
  *relative_error = kInfinity;

  // Abort in the "error" state if max_absolute_sum doesn't make sense.
  if (max_absolute_sum < 0) return;

  // Our scaling scaling_factor will be 2^factor_exponent.
  //
  // TODO(user): Consider using 'max_abs_sum / abs_sum' as a factor if the
  // error can't be zero? Note however that using power of two has the extra
  // advantage that subsequent int64 -> double -> scaled back to int64 will
  // loose no extra information.
  int factor_exponent = 0;
  uint64 abs_sum = 0;
  bool recompute_abs_sum = false;
  bool is_first_value = true;
  const int msb = MostSignificantBitPosition64(max_absolute_sum);
  const int size = input.size();
  for (int i = 0; i < size; ++i) {
    const double x = input[i];

    // A value of zero can just be skipped (and needs to because the code below
    // doesn't handle it correctly).
    if (x == 0.0) continue;

    // If x is not finite, then abort in the "error" state.
    if (!(x > -kInfinity && x < kInfinity)) return;

    // Compute the greatest candidate such that
    // round(fabs(x).2^candidate) <= max_absolute_sum.
    int candidate = msb - ilogb(x);
    if (round(ldexp(fabs(x), candidate)) > max_absolute_sum) --candidate;
    DCHECK_LE(std::abs(static_cast<int64>(round(ldexp(x, candidate)))),
              max_absolute_sum);

    // Update factor_exponent which is the min of all the candidates.
    if (is_first_value || candidate < factor_exponent) {
      is_first_value = false;
      factor_exponent = candidate;
      recompute_abs_sum = true;
    } else {
      // Update the sum of absolute values of the numbers seen so far.
      abs_sum += std::abs(static_cast<int64>(round(ldexp(x, factor_exponent))));
      if (abs_sum > max_absolute_sum) {
        factor_exponent--;
        recompute_abs_sum = true;
      }
    }

    // TODO(user): This is not super efficient, but note that in practice we
    // will only scan the vector of values about log(size) times. It is possible
    // to maintain an upper bound on the abs_sum in linear time instead, but the
    // code and corner cases are a lot more involved. Also, we currently only
    // use this code in situations where its run-time is negligeable compared to
    // the rest.
    while (recompute_abs_sum) {
      abs_sum = 0;
      for (int j = 0; j <= i; ++j) {
        const double x = input[j];
        abs_sum +=
            std::abs(static_cast<int64>(round(ldexp(x, factor_exponent))));
      }
      recompute_abs_sum = false;
      if (abs_sum > max_absolute_sum) {
        factor_exponent--;
        recompute_abs_sum = true;
      }
    }
  }

  // Compute the relative_error.
  *relative_error = 0.0;
  *scaling_factor = ldexp(1.0, factor_exponent);
  for (const double x : input) {
    if (x == 0.0) continue;
    const double scaled = fabs(ldexp(x, factor_exponent));
    *relative_error =
        std::max(*relative_error, fabs(round(scaled) / scaled - 1));
  }
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

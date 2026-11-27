// Copyright 2010-2025 Google LLC
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

#include <algorithm>
#include <bit>
#include <cfenv>  // NOLINT
#include <cmath>
#include <cstdint>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/types.h"
#include "ortools/util/bitset.h"
#include "ortools/util/full_precision_inequalities.h"
#include "ortools/util/saturated_arithmetic.h"

// To enable floating-point traps on Android/Bionic or BSD platforms, extend
// the preprocessor guard below with the corresponding platform macros (such as
// `__BIONIC__`, `__FreeBSD__`, `__NetBSD__`, or `__OpenBSD__`). Because these
// platforms evolve over time, consult their current `<fenv.h>` documentation
// (or an AI assistant) for the required feature-test macros (e.g.,
// `__BSD_VISIBLE`, `_NETBSD_SOURCE`, or `_GNU_SOURCE`).
#if defined(__GLIBC__) && defined(__x86_64__)
#define OR_TOOLS_FP_TRAPS_GNU 1
#if !defined(_GNU_SOURCE)
#error "glibc only declares feenableexcept() when _GNU_SOURCE is defined."
#endif  // !defined(_GNU_SOURCE)
#endif  // defined(__GLIBC__) && defined(__x86_64__)

namespace operations_research {

ScopedFloatingPointEnv::ScopedFloatingPointEnv(const int excepts) {
#if defined(OR_TOOLS_FP_TRAPS_GNU)
  // feenableexcept() returns the previously enabled exceptions, or -1 if the
  // requested exceptions could not be enabled.
  saved_excepts_ = feenableexcept(excepts & FE_ALL_EXCEPT);
  exceptions_enabled_ = (saved_excepts_ != -1);
#else
  (void)excepts;
  (void)saved_excepts_;
#endif  // defined(OR_TOOLS_FP_TRAPS_GNU)
}

ScopedFloatingPointEnv::~ScopedFloatingPointEnv() {
#if defined(OR_TOOLS_FP_TRAPS_GNU)
  if (exceptions_enabled_) {
    CHECK_NE(-1, fedisableexcept(FE_ALL_EXCEPT));
    CHECK_NE(-1, feenableexcept(saved_excepts_));
  }
#endif  // defined(OR_TOOLS_FP_TRAPS_GNU)
}

namespace {

// Returns the reordered and capped min and max of the two terms.
// The minimum of the two cannot be larger than 0.0 and the maximum cannot be
// smaller than 0.0.
// For example, if a = 1.0 and b = -2.0, returns {-2.0, 1.0}.
// If a = 1.0 and b = 2.0, returns {0.0, 2.0}.
// If a = -1.0 and b = -2.0, returns {-2.0, 0.0}.
void ReorderAndCapTerms(double* min, double* max) {
  if (*min > *max) std::swap(*min, *max);
  if (*min > 0.0) *min = 0.0;
  if (*max < 0.0) *max = 0.0;
}

template <bool use_bounds>
void ComputeScalingErrors(absl::Span<const double> input,
                          absl::Span<const double> lb,
                          absl::Span<const double> ub, double scaling_factor,
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
void GetBestScalingOfDoublesToInt64(absl::Span<const double> input,
                                    absl::Span<const double> lb,
                                    absl::Span<const double> ub,
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
    int candidate = msb - FastILogB(c);
    if (candidate >= std::numeric_limits<double>::max_exponent) {
      candidate = std::numeric_limits<double>::max_exponent - 1;
    }
    if (std::round(FastLdExp(std::abs(c), candidate)) > max_absolute_sum) {
      --candidate;
    }
    DCHECK_LE(
        std::abs(static_cast<int64_t>(std::round(FastLdExp(c, candidate)))),
        max_absolute_sum);

    // Update factor_exponent which is the min of all the candidates.
    if (is_first_value || candidate < factor_exponent) {
      is_first_value = false;
      factor_exponent = candidate;
      recompute_sum = true;
    } else {
      // Update the sum of absolute values of the numbers seen so far.
      sum_min -= static_cast<int64_t>(
          std::round(FastLdExp(min_term, factor_exponent)));
      sum_max += static_cast<int64_t>(
          std::round(FastLdExp(max_term, factor_exponent)));
      if (sum_min > static_cast<uint64_t>(max_absolute_sum) ||
          sum_max > static_cast<uint64_t>(max_absolute_sum)) {
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
        sum_min -= static_cast<int64_t>(
            std::round(FastLdExp(min_term, factor_exponent)));
        sum_max += static_cast<int64_t>(
            std::round(FastLdExp(max_term, factor_exponent)));
      }
      if (sum_min > static_cast<uint64_t>(max_absolute_sum) ||
          sum_max > static_cast<uint64_t>(max_absolute_sum)) {
        factor_exponent--;
      } else {
        recompute_sum = false;
      }
    }
  }
  *scaling_factor = FastTwoExp(factor_exponent);
}

// Tries to find an odd rational multiplier for `coefficients` within
// `fp_limit`, shifting any coefficient with `frexp` exponent `exp <
// min_target_exp` up by `2^(min_target_exp - exp)` before rational
// approximation:
// - `min_target_exp = k` (with a 7-bit ULP safety margin relative to
//   `tolerance`) strips up to `k` bits of power-of-two denominator (e.g.
//   recovering `5^6` from `1e-6 = 2^-6 * 5^-6`).
// - `min_target_exp = 0` normalizes `< 0.5` coefficients into `[0.5, 1.0)`,
//   making 8-to-10-digit decimals (such as `333.33333333` in `timtab1CUTS`)
//   invariant to power-of-two variable down-scaling without amplifying their
//   truncation error by `2^k`.
// - `min_target_exp = int::min()` leaves raw fractional parts untouched so
//   unscaled truncated rationals like `1/450` or `1/1440` are not shifted.
double FindOddRationalScaling(absl::Span<const double> coefficients,
                              double tolerance, double fp_limit,
                              int min_target_exp) {
  double multiplier = 1.0;
  const int64_t limit =
      static_cast<int64_t>(std::min(std::abs(fp_limit), 1e15));
  for (const double coeff : coefficients) {
    if (coeff == 0.0) continue;
    const double abs_coeff = std::abs(coeff);
    const int exp = FastILogB(abs_coeff) + 1;
    const int shift = min_target_exp > exp ? min_target_exp - exp : 0;
    const double scaled = FastLdExp(abs_coeff, shift);
    const double frac = scaled - std::floor(scaled);
    int64_t f =
        FindRationalFactor(multiplier * frac, limit, multiplier * tolerance);
    if (f == 0) return 0.0;
    const int tz_f = std::countr_zero(static_cast<uint64_t>(f));
    f >>= tz_f;
    multiplier *= f;
    if (multiplier > limit) return 0.0;
    const int max_pow2 = MaxPowerOfTwoExponentWithinBound(multiplier, limit);
    if (shift + tz_f > max_pow2) {
      // `FindRationalFactor` relied on `2^(shift + tz_f)`, which exceeds
      // `limit / multiplier`. Verify whether `multiplier * 2^r` already
      // realizes an integer on `coeff` within `limit`.
      const int r = std::min(max_pow2, std::max(0, 53 - exp));
      double frac_r = FastLdExp(abs_coeff, r);
      frac_r -= std::floor(frac_r);
      const double prod = multiplier * frac_r;
      if (std::abs(prod - std::round(prod)) >=
          FastLdExp(multiplier * tolerance, r)) {
        return 0.0;
      }
    }
  }
  return multiplier;
}

}  // namespace

double RoundScaledLowerBound(double y, double tol) {
  if (!std::isfinite(y)) return y;
  const double r = std::round(y);
  const double w = TightSum().Add(r).Add(-y) <= tol ? r : std::floor(y);
  return w == 0.0 ? 0.0 : w;
}

// This uses the best rational approximation of x via continuous fractions.
// It is probably not the best implementation, but according to the unit test,
// it seems to do the job.
int64_t FindRationalFactor(double x, int64_t limit, double tolerance) {
  const double initial_x = x;
  x = std::abs(x);
  x -= std::floor(x);
  int64_t current_q = 1;
  int64_t prev_q = 0;
  while (current_q < limit) {
    const double q = static_cast<double>(current_q);
    const double qx = q * initial_x;
    const double qtolerance = q * tolerance;
    if (std::abs(qx - std::round(qx)) < qtolerance) {
      return current_q;
    }
    x = 1 / x;
    const double floored_x = std::floor(x);
    if (floored_x >= static_cast<double>(kint64max)) {
      return 0;
    }
    const int64_t new_q =
        CapAdd(prev_q, CapProd(static_cast<int64_t>(floored_x), current_q));
    prev_q = current_q;
    current_q = new_q;
    x -= std::floor(x);
  }
  return 0;
}

double FindBestScalingAndComputeErrors(
    absl::Span<const double> coefficients,
    absl::Span<const double> lower_bounds,
    absl::Span<const double> upper_bounds, int64_t max_absolute_activity,
    double wanted_absolute_activity_precision, double* relative_coeff_error,
    double* scaled_sum_error, double* max_scaling_factor) {
  // Starts by computing the highest possible factor.
  double scaling_factor = GetBestScalingOfDoublesToInt64(
      coefficients, lower_bounds, upper_bounds, max_absolute_activity);
  if (max_scaling_factor != nullptr) *max_scaling_factor = scaling_factor;
  if (scaling_factor == 0.0) return scaling_factor;

  // Returns the smallest factor of the form 2^i that gives us a relative sum
  // error of wanted_absolute_activity_precision and still makes sure we will
  // have no integer overflow.
  //
  // Important: the loop is written in such a way that ComputeScalingErrors()
  // is called on the last factor.
  //
  // TODO(user): Make this faster.
  double x = std::min(scaling_factor, 1.0);
  for (; x <= scaling_factor; x *= 2) {
    ComputeScalingErrors(coefficients, lower_bounds, upper_bounds, x,
                         relative_coeff_error, scaled_sum_error);
    if (*scaled_sum_error < wanted_absolute_activity_precision * x) break;

    // This could happen if we always have enough precision.
    if (x == scaling_factor) break;
  }
  scaling_factor = x;
  DCHECK(std::isfinite(scaling_factor));

  // Because we deal with an approximate input, scaling with a power of 2 might
  // not be the best choice. It is also possible the user used rational coeffs
  // and then converted them to double (1/2, 1/3, 4/5, etc...). This scaling
  // will recover such rational input and might result in a smaller overall
  // coefficient which is good.
  //
  // Note that if our current precision is already above the requested one,
  // we choose integer scaling if we get a better precision.
  //
  // We try two tolerances and three mantissa shift targets (`k`, `0`, and no
  // shift). FindRationalFactor() accepts a denominator q as soon as
  // |q * x - round(q * x)| < q * tolerance, so with a tolerance of 1e-8 any
  // convergent with q >= 1e4 is accepted and we cannot reliably recover
  // denominators like 15625 (i.e. coefficients that are multiples of 1e-6),
  // the result even depends on the coefficient order. We thus first try a
  // tight tolerance to recover exact rationals. But the loose tolerance also
  // acts as a search for a smaller factor with an acceptable error when the
  // coefficients are not exact rationals (this matters for the headroom of
  // continuous variables), so we still try it afterwards.
  for (const double rational_tolerance : {1e-10, 1e-8}) {
    const int tol_exp = FastILogB(rational_tolerance) + 1;
    const int k = std::clamp(53 + tol_exp - 7, 0, 52);
    for (const int min_target_exp : {k, 0, std::numeric_limits<int>::min()}) {
      double integer_factor = FindOddRationalScaling(
          coefficients, rational_tolerance, scaling_factor, min_target_exp);
      DCHECK(std::isfinite(integer_factor));
      if (integer_factor == 0 || integer_factor >= scaling_factor) continue;

      double local_relative_coeff_error;
      double local_scaled_sum_error;

      // As long as integer_factor * 2^x is smaller than scaling_factor,
      // consider it!
      bool use_rational = false;
      while (true) {
        ComputeScalingErrors(coefficients, lower_bounds, upper_bounds,
                             integer_factor, &local_relative_coeff_error,
                             &local_scaled_sum_error);

        if (local_scaled_sum_error <
            wanted_absolute_activity_precision * integer_factor) {
          // Smaller factor under the wanted precision, select it.
          use_rational = true;
          break;
        }

        if (integer_factor * 2 >= scaling_factor) break;
        integer_factor *= 2;
      }

      // If use_rational was set, or we have a smaller error, go for it.
      DCHECK_LT(integer_factor, scaling_factor);
      if (use_rational || local_scaled_sum_error * scaling_factor <=
                              *scaled_sum_error * integer_factor) {
        *relative_coeff_error = local_relative_coeff_error;
        *scaled_sum_error = local_scaled_sum_error;
        scaling_factor = integer_factor;
      }
    }
  }

  DCHECK(std::isfinite(scaling_factor));
  return scaling_factor;
}

void ComputeScalingErrors(absl::Span<const double> input,
                          absl::Span<const double> lb,
                          absl::Span<const double> ub, double scaling_factor,
                          double* max_relative_coeff_error,
                          double* max_scaled_sum_error) {
  ComputeScalingErrors<true>(input, lb, ub, scaling_factor,
                             max_relative_coeff_error, max_scaled_sum_error);
}

double GetBestScalingOfDoublesToInt64(absl::Span<const double> input,
                                      absl::Span<const double> lb,
                                      absl::Span<const double> ub,
                                      int64_t max_absolute_sum) {
  double scaling_factor;
  GetBestScalingOfDoublesToInt64<true>(input, lb, ub, max_absolute_sum,
                                       &scaling_factor);
  DCHECK(std::isfinite(scaling_factor));
  return scaling_factor;
}

void GetBestScalingOfDoublesToInt64(absl::Span<const double> input,
                                    int64_t max_absolute_sum,
                                    double* scaling_factor,
                                    double* max_relative_coeff_error) {
  double max_scaled_sum_error;
  GetBestScalingOfDoublesToInt64<false>(input, {}, {}, max_absolute_sum,
                                        scaling_factor);
  ComputeScalingErrors<false>(input, {}, {}, *scaling_factor,
                              max_relative_coeff_error, &max_scaled_sum_error);
  DCHECK(std::isfinite(*scaling_factor));
}

int64_t ComputeGcdOfRoundedDoubles(absl::Span<const double> x,
                                   double scaling_factor) {
  DCHECK(std::isfinite(scaling_factor));
  int64_t gcd = 0;
  const int size = static_cast<int>(x.size());
  for (int i = 0; i < size && gcd != 1; ++i) {
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

void TightScalingErrorHelper::LoadUnscaledConstraint(
    absl::Span<const double> input_coeffs, absl::Span<const int64_t> input_lbs,
    absl::Span<const int64_t> input_ubs) {
  LoadUnscaledConstraint(input_coeffs, input_coeffs, input_coeffs, input_lbs,
                         input_ubs);
}

void TightScalingErrorHelper::LoadUnscaledConstraint(
    absl::Span<const double> input_coeffs,
    absl::Span<const double> input_coeffs_lb,
    absl::Span<const double> input_coeffs_ub,
    absl::Span<const int64_t> input_lbs, absl::Span<const int64_t> input_ubs) {
  // To simplify code, lets make all coefficient positive.
  // Being super-fast here shouldn't matter that much since we will only
  // do that once per constraint.
  const int num_terms = input_coeffs.size();
  DCHECK_EQ(input_coeffs_lb.size(), num_terms);
  DCHECK_EQ(input_coeffs_ub.size(), num_terms);
  DCHECK_EQ(input_lbs.size(), num_terms);
  DCHECK_EQ(input_ubs.size(), num_terms);
  coeffs_.assign(input_coeffs.begin(), input_coeffs.end());
  coeffs_lb_.assign(input_coeffs_lb.begin(), input_coeffs_lb.end());
  coeffs_ub_.assign(input_coeffs_ub.begin(), input_coeffs_ub.end());
  lbs_.assign(input_lbs.begin(), input_lbs.end());
  ubs_.assign(input_ubs.begin(), input_ubs.end());
  has_overflow_ = false;
  for (int i = 0; i < num_terms; ++i) {
    DCHECK(std::isfinite(coeffs_[i]));
    DCHECK(std::isfinite(coeffs_lb_[i]));
    DCHECK(std::isfinite(coeffs_ub_[i]));
    DCHECK_LE(coeffs_lb_[i], coeffs_[i]);
    DCHECK_LE(coeffs_[i], coeffs_ub_[i]);
    CHECK(coeffs_lb_[i] != 0.0 || coeffs_ub_[i] != 0.0);
    DCHECK_GT(lbs_[i], kint64min);
    DCHECK_LT(ubs_[i], kint64max);
    DCHECK_LE(lbs_[i], ubs_[i]);

    // Canonicalize the domain and the coefficient.
    // We only care about the set of integer feasible solutions.
    if (coeffs_ub_[i] < 0.0 || (coeffs_ub_[i] == 0.0 && coeffs_lb_[i] < 0.0)) {
      std::swap(lbs_[i], ubs_[i]);
      lbs_[i] = -lbs_[i];
      ubs_[i] = -ubs_[i];
      coeffs_[i] = -coeffs_[i];
      std::swap(coeffs_lb_[i], coeffs_ub_[i]);
      coeffs_lb_[i] = -coeffs_lb_[i];
      coeffs_ub_[i] = -coeffs_ub_[i];
    }
    CHECK_GE(coeffs_lb_[i], 0.0);
    CHECK_GE(coeffs_[i], 0.0);
  }
}

void TightScalingErrorHelper::LoadBothConstraints(
    absl::Span<const double> scaled_coeffs,
    absl::Span<const double> rounded_coeffs,
    absl::Span<const int64_t> input_lbs, absl::Span<const int64_t> input_ubs) {
  LoadBothConstraints(scaled_coeffs, scaled_coeffs, rounded_coeffs, input_lbs,
                      input_ubs);
}

void TightScalingErrorHelper::LoadBothConstraints(
    absl::Span<const double> scaled_coeffs_lb,
    absl::Span<const double> scaled_coeffs_ub,
    absl::Span<const double> rounded_coeffs,
    absl::Span<const int64_t> input_lbs, absl::Span<const int64_t> input_ubs) {
  // To simplify code, lets make all coefficient positive.
  // Being super-fast here shouldn't matter that much since we will only
  // do that once per constraint.
  const int num_terms = scaled_coeffs_lb.size();
  DCHECK_EQ(scaled_coeffs_ub.size(), num_terms);
  DCHECK_EQ(rounded_coeffs.size(), num_terms);
  DCHECK_EQ(input_lbs.size(), num_terms);
  DCHECK_EQ(input_ubs.size(), num_terms);
  scaled_coeffs_lb_.assign(scaled_coeffs_lb.begin(), scaled_coeffs_lb.end());
  scaled_coeffs_ub_.assign(scaled_coeffs_ub.begin(), scaled_coeffs_ub.end());
  rounded_coeffs_.assign(rounded_coeffs.begin(), rounded_coeffs.end());
  lbs_.assign(input_lbs.begin(), input_lbs.end());
  ubs_.assign(input_ubs.begin(), input_ubs.end());
  has_overflow_ = false;
  for (int i = 0; i < num_terms; ++i) {
    DCHECK(std::isfinite(scaled_coeffs_lb_[i]));
    DCHECK(std::isfinite(scaled_coeffs_ub_[i]));
    DCHECK(std::isfinite(rounded_coeffs_[i]));
    DCHECK_LE(scaled_coeffs_lb_[i], scaled_coeffs_ub_[i]);
    if (scaled_coeffs_lb_[i] == 0.0 && scaled_coeffs_ub_[i] == 0.0 &&
        rounded_coeffs_[i] == 0.0) {
      lbs_[i] = 0;
      ubs_[i] = 0;
      continue;
    }
    DCHECK_GT(input_lbs[i], kint64min);
    DCHECK_LT(input_ubs[i], kint64max);
    DCHECK_LE(input_lbs[i], input_ubs[i]);

    // Canonicalize the domain and the coefficient.
    // We only care about the set of integer feasible solutions.
    if (scaled_coeffs_ub_[i] < 0.0 ||
        (scaled_coeffs_ub_[i] == 0.0 &&
         (scaled_coeffs_lb_[i] < 0.0 || rounded_coeffs_[i] < 0.0))) {
      std::swap(lbs_[i], ubs_[i]);
      lbs_[i] = -lbs_[i];
      ubs_[i] = -ubs_[i];
      std::swap(scaled_coeffs_lb_[i], scaled_coeffs_ub_[i]);
      scaled_coeffs_lb_[i] = -scaled_coeffs_lb_[i];
      scaled_coeffs_ub_[i] = -scaled_coeffs_ub_[i];
      rounded_coeffs_[i] = -rounded_coeffs_[i];
    }

    // Coeff should be of the same sign!
    CHECK_GE(scaled_coeffs_lb_[i], 0.0);
    CHECK_GE(rounded_coeffs_[i], 0.0);
    if (!std::isfinite(scaled_coeffs_ub_[i] * kint64max) ||
        !std::isfinite(rounded_coeffs_[i] * kint64max)) {
      has_overflow_ = true;
      return;
    }
  }
}

void TightScalingErrorHelper::LoadScalingFactor(double scaling_factor) {
  CHECK(std::isfinite(scaling_factor));
  CHECK_GE(scaling_factor, 0.0);
  const int num_terms = coeffs_.size();
  scaled_coeffs_lb_.resize(num_terms);
  scaled_coeffs_ub_.resize(num_terms);
  rounded_coeffs_.resize(num_terms);
  has_overflow_ = false;
  for (int i = 0; i < num_terms; ++i) {
    // The callers scale with std::round(coeff * scaling_factor), where the
    // product is rounded to the nearest double, while the exact unscaled
    // coefficient lies in [coeffs_lb_[i], coeffs_ub_[i]].
    const double rounded = std::round(coeffs_[i] * scaling_factor);
    const double scaled_lb = ProdBounds(coeffs_lb_[i], scaling_factor).first;
    const double scaled_ub = ProdBounds(coeffs_ub_[i], scaling_factor).second;
    if (!std::isfinite(scaled_ub * kint64max) ||
        !std::isfinite(rounded * kint64max)) {
      has_overflow_ = true;
      return;
    }
    scaled_coeffs_lb_[i] = scaled_lb;
    scaled_coeffs_ub_[i] = scaled_ub;
    rounded_coeffs_[i] = rounded;
  }
}

template <bool is_ge_constraint, bool minimize, bool compute_error>
double TightScalingErrorHelper::Optimize(CoeffBounds coeffs,
                                         CoeffBounds new_coeffs,
                                         TightSum& slack,
                                         TightSum& result_sum) {
  constexpr double kInf = std::numeric_limits<double>::infinity();
  constexpr double kWorstBound = minimize ? -kInf : kInf;

  if (has_overflow_ || !slack.IsFinite() || !result_sum.IsFinite()) {
    result_sum.Clear();
    result_sum.Add(kWorstBound);
    return kWorstBound;
  }

  // All coefficients have already been canonicalized to be non-negative:
  // for >= constraints we start at the lower bound (Xi = initial_i + Yi) and
  // greedily move variables toward their upper bound; the reverse for <=
  // constraints (Xi = initial_i - Yi). With kSign = 1 for >= constraints and
  // -1 for <= constraints, Yi = kSign * (Xi - initial_i) is in [0, range_i]
  // and the problem becomes the knapsack:
  //   sum coeffs[i] * Yi >= slack  (relaxation, minimize == is_ge_constraint)
  //   sum coeffs[i] * Yi <= slack  (restriction, minimize != is_ge_constraint)
  // with slack = kSign * bound - sum coeffs[i] * (kSign * initial_i). At
  // optimality, all variables but at most one are at one of their bounds, so
  // the result is the sum of one term delta_i * Xi per variable.
  //
  // Each coefficient is only known within an interval, and we use for each
  // quantity the end of the interval that keeps the result conservative:
  // - The slack must be a lower bound (relaxation) or an upper bound
  //   (restriction) of its exact value. This fixes the end of coeffs[i] used
  //   for coeffs[i] * initial_i (coeff_at_initial below).
  // - Since Yi >= 0, the feasible set only grows if we use the upper
  //   (relaxation) or lower (restriction) end of coeffs[i] as knapsack weight,
  //   and a larger feasible set only moves the optimum in the conservative
  //   direction.
  // - The objective is split as
  //     sum (new_coeffs[i] - coeffs[i]) * initial_i + kSign * sum delta_i * Yi
  //   where delta_i is a conservative bound on new_coeffs[i] - coeffs[i] (the
  //   factor Yi is non-negative) and the constant part is bounded using, for
  //   coeffs[i] * initial_i, the same end as in the slack so that both cancel
  //   out exactly.
  // The loop below adds, for each variable,
  //   delta_i * Xi = delta_i * initial_i + kSign * delta_i * Yi,
  // which is the bound above whenever delta_i * initial_i is the bound used
  // for the constant part. This is the common case, and the rare exceptions
  // (the coefficient interval is not a point and initial_i has the sign for
  // which the slack uses the other end, or delta_i is not exact) are handled
  // by adding the difference to result_sum.
  constexpr int64_t kSign = is_ge_constraint ? 1 : -1;
  constexpr bool kRelaxation = (minimize == is_ge_constraint);

  const int num_terms = coeffs.lb.size();
  items_.clear();
  items_.reserve(num_terms);
  for (int i = 0; i < num_terms; ++i) {
    DCHECK_GE(coeffs.lb[i], 0);
    DCHECK_LE(coeffs.lb[i], coeffs.ub[i]);

    // For >= constraint we start at the lower bound and greedily move
    // variables to their upper bound. The reverse for the <= constraints.
    const int64_t initial_bound = is_ge_constraint ? lbs_[i] : ubs_[i];
    const int64_t other_bound = is_ge_constraint ? ubs_[i] : lbs_[i];
    const int64_t signed_initial_bound = kSign * initial_bound;

    // End of coeffs[i] used for coeffs[i] * initial_i: the slack must be a
    // lower bound for a relaxation and an upper bound for a restriction.
    const double coeff_at_initial = (kRelaxation == (signed_initial_bound >= 0))
                                        ? coeffs.ub[i]
                                        : coeffs.lb[i];
    slack.AddProd(-coeff_at_initial, signed_initial_bound);

    // Knapsack weight and cost of one unit of Yi (before the kSign factor). The
    // cost new_coeffs[i] - coeffs[i] is rounded towards the conservative
    // direction (it is exact in practice).
    const double coeff = kRelaxation ? coeffs.ub[i] : coeffs.lb[i];
    const double new_coeff =
        ((kSign > 0) == minimize) ? new_coeffs.lb[i] : new_coeffs.ub[i];
    double delta = new_coeff;

    // Conservative bound on the constant part of the objective at
    // Xi = initial_i, which is delta * initial_i except in rare cases.
    const double new_coeff_at_initial = ((initial_bound >= 0) == minimize)
                                            ? new_coeffs.lb[i]
                                            : new_coeffs.ub[i];
    bool constant_part_is_delta_initial = new_coeff == new_coeff_at_initial;
    if constexpr (compute_error) {
      const TightSum cost = TightSum().Add(new_coeff).Add(-coeff);
      delta = kRelaxation ? cost.TightLowerBound() : cost.TightUpperBound();
      constant_part_is_delta_initial =
          constant_part_is_delta_initial && coeff == coeff_at_initial &&
          cost.TightLowerBound() == cost.TightUpperBound();
    }
    if (initial_bound != 0 && !constant_part_is_delta_initial) {
      // Replace the delta * initial_i added below by the conservative bound of
      // (new_coeffs[i] - coeffs[i]) * initial_i.
      result_sum.AddProd(new_coeff_at_initial, initial_bound);
      if constexpr (compute_error) {
        result_sum.AddProd(-coeff_at_initial, initial_bound);
      }
      result_sum.AddProd(-delta, initial_bound);
    }

    // When maximizing the error on a >= constraint (or minimizing on a <=
    // constraint) for restriction mode, variables with new_coeffs[i] == 0 are
    // unconstrained in the rounded inequality: moving them increases the
    // original activity without changing the rounded one, so they must stay at
    // initial_bound and are not added as knapsack items.
    if (!kRelaxation && compute_error && new_coeffs.lb[i] == 0.0 &&
        new_coeffs.ub[i] == 0.0) {
      result_sum.AddProd(delta, initial_bound);
      continue;
    }
    items_.push_back(KnapsackItem{
        .coeff = coeff,
        .delta = delta,
        .range = ubs_[i] - lbs_[i],
        .initial_bound = initial_bound,
        .other_bound = other_bound,
    });
  }

  if (slack > 0.0) {
    // Sort by relative efficiency (delta_i / coeff_i).
    absl::c_stable_sort(items_, [](const KnapsackItem& u,
                                   const KnapsackItem& w) {
      if (u.coeff == 0.0 || w.coeff == 0.0) {
        return w.coeff == 0.0 && u.coeff > 0.0;
      }
      if constexpr (kRelaxation) {
        return TightSum().AddProd(u.delta, w.coeff).AddProd(-w.delta, u.coeff) <
               0.0;
      } else {
        return TightSum().AddProd(u.delta, w.coeff).AddProd(-w.delta, u.coeff) >
               0.0;
      }
    });
  }

  // Greedily fill up to slack. Note that at optimality, a single variable is
  // not at its lower/upper bound.
  for (const KnapsackItem& item : items_) {
    if (slack <= 0.0 || item.coeff == 0.0 || item.range == 0) {
      result_sum.AddProd(item.delta, item.initial_bound);
      continue;
    }
    slack.AddProd(-item.coeff, item.range);
    if (slack >= 0.0) {
      // Slack remained positive after subtracting the item, take the entire
      // item.
      result_sum.AddProd(item.delta, item.other_bound);
      continue;
    }
    // Split item: put the item back and take the exact fractional amount.
    slack.AddProd(item.coeff, item.range);
    result_sum.AddProd(item.delta, item.initial_bound);

    if (item.delta == item.coeff) {
      // Avoiding computing (x * slack)/x for the simple case where no rounding
      // was done.
      result_sum.AddProd(slack, /*negate=*/!is_ge_constraint);
      slack.Clear();
      continue;
    }
    // Precise division trick:
    // q1 = rounded(slack / item.coeff)  (which way we round is not important)
    // q2 = lb((slack - q1 * item.coeff) / item.coeff)
    // So `q1 + q2` is a strict lower bound of `slack / item.coeff`, but with
    // the error one would have with double-double precision!
    const double step_delta = is_ge_constraint ? item.delta : -item.delta;
    const double q1 = slack.TightLowerBound() / item.coeff;
    result_sum.AddProd(step_delta, q1);
    slack.AddProd(-q1, item.coeff);
    if ((step_delta > 0.0) == minimize) {
      const double rem_lb = slack.TightLowerBound();
      if (rem_lb != 0.0) {
        const double q2 = FastStepDown(rem_lb / item.coeff);
        result_sum.AddProd(step_delta, q2);
      }
    } else {
      const double rem_ub = slack.TightUpperBound();
      if (rem_ub != 0.0) {
        const double q2 = FastStepUp(rem_ub / item.coeff);
        result_sum.AddProd(step_delta, q2);
      }
    }
    slack.Clear();
  }

  return minimize ? result_sum.TightLowerBound() : result_sum.TightUpperBound();
}

template double TightScalingErrorHelper::Optimize<true, true, true>(CoeffBounds,
                                                                    CoeffBounds,
                                                                    TightSum&,
                                                                    TightSum&);
template double TightScalingErrorHelper::Optimize<true, false, true>(
    CoeffBounds, CoeffBounds, TightSum&, TightSum&);
template double TightScalingErrorHelper::Optimize<false, false, true>(
    CoeffBounds, CoeffBounds, TightSum&, TightSum&);
template double TightScalingErrorHelper::Optimize<false, true, true>(
    CoeffBounds, CoeffBounds, TightSum&, TightSum&);
template double TightScalingErrorHelper::Optimize<true, true, false>(
    CoeffBounds, CoeffBounds, TightSum&, TightSum&);

}  // namespace operations_research

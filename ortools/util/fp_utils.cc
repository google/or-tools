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
#include <cfenv>  // NOLINT
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/util/bitset.h"

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
    int candidate = msb - ilogb(c);
    if (candidate >= std::numeric_limits<double>::max_exponent) {
      candidate = std::numeric_limits<double>::max_exponent - 1;
    }
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
        sum_min -=
            static_cast<int64_t>(std::round(ldexp(min_term, factor_exponent)));
        sum_max +=
            static_cast<int64_t>(std::round(ldexp(max_term, factor_exponent)));
      }
      if (sum_min > static_cast<uint64_t>(max_absolute_sum) ||
          sum_max > static_cast<uint64_t>(max_absolute_sum)) {
        factor_exponent--;
      } else {
        recompute_sum = false;
      }
    }
  }
  *scaling_factor = ldexp(1.0, factor_exponent);
}

}  // namespace

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
    absl::Span<const double> input_coeffs, absl::Span<const double> input_lbs,
    absl::Span<const double> input_ubs) {
  // To simplify code, lets make all coefficient positive.
  // Beeing super-fast here shouldn't matter that much since we will only
  // do that once per constraint.
  const int num_terms = input_coeffs.size();
  coeffs_.assign(input_coeffs.begin(), input_coeffs.end());
  lbs_.assign(input_lbs.begin(), input_lbs.end());
  ubs_.assign(input_ubs.begin(), input_ubs.end());
  for (int i = 0; i < num_terms; ++i) {
    // Bounds should only be integer.
    // TODO(user): If we want to compute bound EXACTLY, take int64_t instead ?
    CHECK_EQ(std::round(lbs_[i]), lbs_[i]);
    CHECK_EQ(std::round(ubs_[i]), ubs_[i]);
    CHECK_NE(coeffs_[i], 0.0);

    // Canonicalized the domain and the coefficient.
    // We only care about the set of integer feasible solution.
    if (coeffs_[i] < 0) {
      std::swap(lbs_[i], ubs_[i]);
      lbs_[i] = -lbs_[i];
      ubs_[i] = -ubs_[i];
      coeffs_[i] = -coeffs_[i];
    }
  }
}

void TightScalingErrorHelper::LoadScalingFactor(double scaling_factor) {
  const int num_terms = coeffs_.size();
  scaled_coeffs_.resize(num_terms);
  rounded_coeffs_.resize(num_terms);
  for (int i = 0; i < num_terms; ++i) {
    const double scaled = scaling_factor * coeffs_[i];
    scaled_coeffs_[i] = scaled;
    rounded_coeffs_[i] = std::round(scaled);
  }
}

}  // namespace operations_research

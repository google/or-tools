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
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/types.h"
#include "ortools/util/bitset.h"
#include "ortools/util/full_precision_inequalities.h"

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
    absl::Span<const double> input_coeffs, absl::Span<const int64_t> input_lbs,
    absl::Span<const int64_t> input_ubs) {
  // To simplify code, lets make all coefficient positive.
  // Being super-fast here shouldn't matter that much since we will only
  // do that once per constraint.
  const int num_terms = input_coeffs.size();
  DCHECK_EQ(input_lbs.size(), num_terms);
  DCHECK_EQ(input_ubs.size(), num_terms);
  coeffs_.assign(input_coeffs.begin(), input_coeffs.end());
  lbs_.assign(input_lbs.begin(), input_lbs.end());
  ubs_.assign(input_ubs.begin(), input_ubs.end());
  has_overflow_ = false;
  for (int i = 0; i < num_terms; ++i) {
    DCHECK(std::isfinite(coeffs_[i]));
    CHECK_NE(coeffs_[i], 0.0);
    DCHECK_GT(lbs_[i], kint64min);
    DCHECK_LT(ubs_[i], kint64max);
    DCHECK_LE(lbs_[i], ubs_[i]);

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

void TightScalingErrorHelper::LoadBothConstraints(
    absl::Span<const double> scaled_coeffs,
    absl::Span<const double> rounded_coeffs,
    absl::Span<const int64_t> input_lbs, absl::Span<const int64_t> input_ubs) {
  // To simplify code, lets make all coefficient positive.
  // Being super-fast here shouldn't matter that much since we will only
  // do that once per constraint.
  const int num_terms = scaled_coeffs.size();
  DCHECK_EQ(rounded_coeffs.size(), num_terms);
  DCHECK_EQ(input_lbs.size(), num_terms);
  DCHECK_EQ(input_ubs.size(), num_terms);
  scaled_coeffs_.assign(scaled_coeffs.begin(), scaled_coeffs.end());
  rounded_coeffs_.assign(rounded_coeffs.begin(), rounded_coeffs.end());
  lbs_.assign(input_lbs.begin(), input_lbs.end());
  ubs_.assign(input_ubs.begin(), input_ubs.end());
  has_overflow_ = false;
  for (int i = 0; i < num_terms; ++i) {
    DCHECK(std::isfinite(scaled_coeffs[i]));
    DCHECK(std::isfinite(rounded_coeffs[i]));
    if (scaled_coeffs_[i] == 0.0 && rounded_coeffs_[i] == 0.0) {
      lbs_[i] = 0;
      ubs_[i] = 0;
      continue;
    }
    DCHECK_GT(input_lbs[i], kint64min);
    DCHECK_LT(input_ubs[i], kint64max);
    DCHECK_LE(input_lbs[i], input_ubs[i]);

    // Canonicalize the domain and the coefficient.
    // We only care about the set of integer feasible solutions.
    if (scaled_coeffs_[i] < 0.0 ||
        (scaled_coeffs_[i] == 0.0 && rounded_coeffs_[i] < 0.0)) {
      std::swap(lbs_[i], ubs_[i]);
      lbs_[i] = -lbs_[i];
      ubs_[i] = -ubs_[i];
      scaled_coeffs_[i] = -scaled_coeffs_[i];
      rounded_coeffs_[i] = -rounded_coeffs_[i];
    }

    // Coeff should be of the same sign!
    CHECK_GE(rounded_coeffs_[i], 0.0);
    if (!std::isfinite(scaled_coeffs_[i] * kint64max) ||
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
  scaled_coeffs_.resize(num_terms);
  rounded_coeffs_.resize(num_terms);
  has_overflow_ = false;
  for (int i = 0; i < num_terms; ++i) {
    const double scaled = scaling_factor * coeffs_[i];
    const double rounded = std::round(scaled);
    if (!std::isfinite(rounded * kint64max)) {
      has_overflow_ = true;
      return;
    }
    scaled_coeffs_[i] = scaled;
    rounded_coeffs_[i] = rounded;
  }
}

template <bool is_ge_constraint, bool minimize, bool compute_error>
double TightScalingErrorHelper::Optimize(absl::Span<const double> coeffs,
                                         absl::Span<const double> new_coeffs,
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
  // for >= constraints we start at the lower bound (x_i = lb_i + y_i) and
  // greedily move variables toward their upper bound; the reverse for <=
  // constraints (x_i = ub_i - y_i).
  constexpr int64_t kSign = is_ge_constraint ? 1 : -1;

  const int num_terms = coeffs.size();
  items_.clear();
  items_.reserve(num_terms);
  for (int i = 0; i < num_terms; ++i) {
    const double coeff = coeffs[i];
    DCHECK_GE(coeff, 0);

    // For >= constraint we start at the lower bound and greedily move
    // variables to their upper bound. The reverse for the <= constraints.
    const int64_t initial_bound = is_ge_constraint ? lbs_[i] : ubs_[i];
    const int64_t other_bound = is_ge_constraint ? ubs_[i] : lbs_[i];
    slack.AddProd(-kSign * coeff, initial_bound);

    const double delta = compute_error ? new_coeffs[i] - coeff : new_coeffs[i];

    // When maximizing the error on a >= constraint (or minimizing on a <=
    // constraint) for restriction mode, variables with new_coeffs[i] == 0 are
    // unconstrained in the rounded inequality: moving them increases the
    // original activity without changing the rounded one, so they must stay at
    // initial_bound and are not added as knapsack items.
    if (is_ge_constraint != minimize && compute_error && new_coeffs[i] == 0.0) {
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
      if constexpr (is_ge_constraint == minimize) {
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

template double TightScalingErrorHelper::Optimize<true, true, true>(
    absl::Span<const double>, absl::Span<const double>, TightSum&, TightSum&);
template double TightScalingErrorHelper::Optimize<true, false, true>(
    absl::Span<const double>, absl::Span<const double>, TightSum&, TightSum&);
template double TightScalingErrorHelper::Optimize<false, false, true>(
    absl::Span<const double>, absl::Span<const double>, TightSum&, TightSum&);
template double TightScalingErrorHelper::Optimize<false, true, true>(
    absl::Span<const double>, absl::Span<const double>, TightSum&, TightSum&);
template double TightScalingErrorHelper::Optimize<true, true, false>(
    absl::Span<const double>, absl::Span<const double>, TightSum&, TightSum&);

}  // namespace operations_research

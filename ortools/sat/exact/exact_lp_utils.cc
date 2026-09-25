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

#include "ortools/sat/exact/exact_lp_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>

#include "absl/log/check.h"
#include "absl/numeric/bits.h"
#include "absl/numeric/int128.h"
#include "ortools/base/types.h"

namespace operations_research {
namespace sat {

// `multiplier` is used to scale the constraint before doing the conversion.
// Having a wrong multiplier will reduce the chances of success, but will not
// affect the correctness of the result.
std::optional<LinearConstraintProto> ConvertConstraintToIntegerIfPossibleImpl(
    const MPConstraintProto& mp_constraint,
    absl::Span<const int64_t> int_var_lower_bounds,
    absl::Span<const int64_t> int_var_upper_bounds,
    int64_t max_sum_of_abs_activity, double multiplier) {
  if (mp_constraint.lower_bound() == -std::numeric_limits<double>::infinity() &&
      mp_constraint.upper_bound() == std::numeric_limits<double>::infinity()) {
    LinearConstraintProto tautology;
    tautology.add_domain(0);
    tautology.add_domain(0);
    return tautology;
  }

  if (mp_constraint.lower_bound() > mp_constraint.upper_bound()) {
    LinearConstraintProto unsat;
    unsat.add_domain(1);
    unsat.add_domain(1);
    return unsat;
  }

  if (mp_constraint.var_index().empty()) {
    LinearConstraintProto unsat_or_tautology;
    const bool is_impossible =
        mp_constraint.lower_bound() > 0 || mp_constraint.upper_bound() < 0;
    const int val = is_impossible ? 1 : 0;
    unsat_or_tautology.add_domain(val);
    unsat_or_tautology.add_domain(val);
    return unsat_or_tautology;
  }

  double sum_abs_activity = 0.0;
  for (int i = 0; i < mp_constraint.var_index_size(); ++i) {
    const int var_idx = mp_constraint.var_index(i);
    const int64_t lb = int_var_lower_bounds[var_idx];
    const int64_t ub = int_var_upper_bounds[var_idx];

    if (mp_constraint.coefficient(i) == 0.0 || (lb == 0 && ub == 0)) continue;

    if (lb == kint64min || ub == kint64max) {
      // The bounds of the variable were truncated so we cannot use them to
      // bound the error.
      return std::nullopt;
    }

    // TODO(user): Handle the precision loss of this multiplication.
    const double coeff = std::round(mp_constraint.coefficient(i) * multiplier);

    // TODO(user): Handle the precision loss of this dot product..
    sum_abs_activity += std::abs(coeff) * std::max(std::abs(lb), std::abs(ub));
  }

  // This implicitly guards against infinite variable bounds.
  if (!std::isfinite(sum_abs_activity) ||
      sum_abs_activity > max_sum_of_abs_activity) {
    return std::nullopt;
  }

  // Supposing coeff_i is our scaled coefficient:
  //   coeff_i = round(coeff_i) + delta_i
  // our constraint can be rewritten as:
  //   sum(coeff_i * x_i) <= rhs  <=>
  //   sum(round(coeff_i) * x_i) <= rhs - sum(delta_i * x_i).
  // Since x_i are integers, we can rewrite this as:
  //   sum(round(coeff_i) * x_i) <= floor(rhs - sum(delta_i * x_i)).
  // If floor(rhs - sum(delta_i * x_i)) has always the same value for all
  // possible values of x_i, then we can convert the constraint to an integer
  // one. The exact same logic applies to the lower bound, but using ceil.
  double max_sum_delta_xi = 0.0;
  double min_sum_delta_xi = 0.0;
  for (int i = 0; i < mp_constraint.var_index_size(); ++i) {
    const int var_idx = mp_constraint.var_index(i);
    const int64_t var_max = int_var_upper_bounds[var_idx];
    const int64_t var_min = int_var_lower_bounds[var_idx];
    if (mp_constraint.coefficient(i) == 0.0 || (var_min == 0 && var_max == 0)) {
      continue;
    }
    // TODO(user): Handle the precision loss of this multiplication.
    const double coeff = mp_constraint.coefficient(i) * multiplier;
    const double delta = coeff - std::round(coeff);
    // No precision loss when subtracting the integer part.
    DCHECK_EQ(delta + std::round(coeff), coeff);

    // TODO(user): Handle the precision loss of this multiplication.
    min_sum_delta_xi += delta * (delta > 0 ? var_min : var_max);
    max_sum_delta_xi += delta * (delta > 0 ? var_max : var_min);
  }

  // A double has 53 bits of precision (approx 9e15). If the delta sum exceeds
  // ~1e12, we lose the fractional resolution required to safely evaluate
  // ub_frac/lb_frac.
  if (std::max(std::abs(max_sum_delta_xi), std::abs(min_sum_delta_xi)) > 1e12) {
    return std::nullopt;
  }

  // TODO(user): Handle the precision loss of this multiplication.
  const double ub = mp_constraint.upper_bound() * multiplier;
  const double lb = mp_constraint.lower_bound() * multiplier;

  // Default to maximum activity limits
  const int64_t max_act = static_cast<int64_t>(std::ceil(sum_abs_activity));
  int64_t integer_ub = max_act;
  int64_t integer_lb = -max_act;

  if (!std::isinf(ub)) {
    const double ub_int = std::floor(ub);
    const double ub_frac = ub - ub_int;  // Strictly in [0.0, 1.0)

    const double ub_min_frac = ub_frac - min_sum_delta_xi;
    const double ub_max_frac = ub_frac - max_sum_delta_xi;

    // This is the main check for whether floor(rhs - sum(delta_i * x_i)) has
    // always the same value explained in the long comment above.
    if (std::floor(ub_min_frac) != std::floor(ub_max_frac)) {
      // floor(rhs - sum(delta_i * x_i)) can take multiple values.
      return std::nullopt;
    }

    // Safely clamp in double-space to avoid UB on cast, then add the fractional
    // shift in 128-bit integer space to prevent precision loss or off-by-one.
    if (ub_int <= static_cast<double>(kint64min)) {
      integer_ub = -max_act - 1;
    } else if (ub_int >= static_cast<double>(kint64max)) {
      integer_ub = max_act;
    } else {
      const absl::int128 exact_ub =
          absl::int128(ub_int) + absl::int128(std::floor(ub_max_frac));
      if (exact_ub < -max_act) {
        integer_ub = -max_act - 1;
      } else if (exact_ub >= max_act) {
        integer_ub = max_act;
      } else {
        integer_ub = static_cast<int64_t>(exact_ub);
      }
    }
  }

  if (!std::isinf(lb)) {
    const double lb_int = std::ceil(lb);
    const double lb_frac = lb - lb_int;  // Strictly in (-1.0, 0.0]

    const double lb_min_frac = lb_frac - min_sum_delta_xi;
    const double lb_max_frac = lb_frac - max_sum_delta_xi;

    if (std::ceil(lb_min_frac) != std::ceil(lb_max_frac)) {
      return std::nullopt;
    }

    if (lb_int <= static_cast<double>(kint64min)) {
      integer_lb = -max_act;
    } else if (lb_int >= static_cast<double>(kint64max)) {
      integer_lb = max_act + 1;
    } else {
      const absl::int128 exact_lb =
          absl::int128(lb_int) + absl::int128(std::ceil(lb_max_frac));
      if (exact_lb > max_act) {
        integer_lb = max_act + 1;
      } else if (exact_lb <= -max_act) {
        integer_lb = -max_act;
      } else {
        integer_lb = static_cast<int64_t>(exact_lb);
      }
    }
  }

  LinearConstraintProto linear_constraint;
  for (int i = 0; i < mp_constraint.var_index_size(); ++i) {
    const int var_idx = mp_constraint.var_index(i);
    if (mp_constraint.coefficient(i) == 0.0 ||
        (int_var_lower_bounds[var_idx] == 0 &&
         int_var_upper_bounds[var_idx] == 0)) {
      continue;
    }
    const double coeff = mp_constraint.coefficient(i) * multiplier;
    linear_constraint.add_coeffs(static_cast<int64_t>(std::round(coeff)));
    linear_constraint.add_vars(var_idx);
  }

  if (integer_lb <= integer_ub) {
    linear_constraint.mutable_domain()->Reserve(2);
    linear_constraint.mutable_domain()->Add(integer_lb);
    linear_constraint.mutable_domain()->Add(integer_ub);
  }

  return linear_constraint;
}

std::optional<LinearConstraintProto> ConvertConstraintToIntegerIfPossible(
    const MPConstraintProto& mp_constraint,
    absl::Span<const int64_t> int_var_lower_bounds,
    absl::Span<const int64_t> int_var_upper_bounds,
    int64_t max_sum_of_abs_activity) {
  // TODO(user): experiment with [1] to detect that constraints like
  // 0.3333333*x + 0.5*y < 10 can be converted. It should also work when
  // powers of 2 work.
  //
  // [1] Forišek, Michal. "Approximating rational numbers by fractions."
  // International Conference on Fun with Algorithms. Berlin, Heidelberg:
  // Springer Berlin Heidelberg, 2007.

  int exact_shift = kint32min;
  double sum_abs_coeff_times_var = 0.0;
  for (int i = 0; i < mp_constraint.var_index_size(); ++i) {
    const double coeff = mp_constraint.coefficient(i);
    const int var_idx = mp_constraint.var_index(i);
    const int64_t lb = int_var_lower_bounds[var_idx];
    const int64_t ub = int_var_upper_bounds[var_idx];
    if (coeff == 0.0 || (lb == 0 && ub == 0)) continue;

    int exp;
    const double frac = std::frexp(std::abs(coeff), &exp);

    // frac is bounded within [0.5, 1.0).
    // Scaling by 2^53 guarantees an exact 53-bit integer representation.
    const uint64_t mantissa = static_cast<uint64_t>(std::ldexp(frac, 53));

    const int tz = absl::countr_zero(mantissa);
    const int required_shift = 53 - exp - tz;

    exact_shift = std::max(exact_shift, required_shift);

    sum_abs_coeff_times_var +=
        std::abs(coeff) * std::max(std::abs(static_cast<double>(lb)),
                                   std::abs(static_cast<double>(ub)));
  }

  // If all coefficients were 0.0, default the shift to 0 to avoid underflow
  if (exact_shift == kint32min) {
    exact_shift = 0;
  }

  // Use our exact shift if it doesn't overflow the activity limit.
  if (std::ldexp(sum_abs_coeff_times_var, exact_shift) <=
      max_sum_of_abs_activity) {
    return ConvertConstraintToIntegerIfPossibleImpl(
        mp_constraint, int_var_lower_bounds, int_var_upper_bounds,
        max_sum_of_abs_activity, std::ldexp(1.0, exact_shift));
  } else {
    // If we could not scale exactly, try with a multiplier of 1.0. This makes
    // less puzzling if we don't convert a constraint that looks like:
    // `10000.00001*x + 20000.00001*y <= 20000`
    return ConvertConstraintToIntegerIfPossibleImpl(
        mp_constraint, int_var_lower_bounds, int_var_upper_bounds,
        max_sum_of_abs_activity, 1.0);
  }
}

}  // namespace sat
}  // namespace operations_research

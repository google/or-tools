// Copyright 2010-2022 Google LLC
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

#include "ortools/sat/cuts.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

namespace {

// Minimum amount of violation of the cut constraint by the solution. This
// is needed to avoid numerical issues and adding cuts with minor effect.
const double kMinCutViolation = 1e-4;

}  // namespace

// Compute the larger t <= max_t such that t * rhs_remainder >= divisor / 2.
//
// This is just a separate function as it is slightly faster to compute the
// result only once.
IntegerValue GetFactorT(IntegerValue rhs_remainder, IntegerValue divisor,
                        IntegerValue max_t) {
  DCHECK_GE(max_t, 1);
  return rhs_remainder == 0
             ? max_t
             : std::min(max_t, CeilRatio(divisor / 2, rhs_remainder));
}

std::function<IntegerValue(IntegerValue)> GetSuperAdditiveRoundingFunction(
    IntegerValue rhs_remainder, IntegerValue divisor, IntegerValue t,
    IntegerValue max_scaling) {
  DCHECK_GE(max_scaling, 1);
  DCHECK_GE(t, 1);

  // Adjust after the multiplication by t.
  rhs_remainder *= t;
  DCHECK_LT(rhs_remainder, divisor);

  // Make sure we don't have an integer overflow below. Note that we assume that
  // divisor and the maximum coeff magnitude are not too different (maybe a
  // factor 1000 at most) so that the final result will never overflow.
  max_scaling =
      std::min(max_scaling, std::numeric_limits<int64_t>::max() / divisor);

  const IntegerValue size = divisor - rhs_remainder;
  if (max_scaling == 1 || size == 1) {
    // TODO(user): Use everywhere a two step computation to avoid overflow?
    // First divide by divisor, then multiply by t. For now, we limit t so that
    // we never have an overflow instead.
    return [t, divisor](IntegerValue coeff) {
      return FloorRatio(t * coeff, divisor);
    };
  } else if (size <= max_scaling) {
    return [size, rhs_remainder, t, divisor](IntegerValue coeff) {
      const IntegerValue t_coeff = t * coeff;
      const IntegerValue ratio = FloorRatio(t_coeff, divisor);
      const IntegerValue remainder = PositiveRemainder(t_coeff, divisor);
      const IntegerValue diff = remainder - rhs_remainder;
      return size * ratio + std::max(IntegerValue(0), diff);
    };
  } else if (max_scaling.value() * rhs_remainder.value() < divisor) {
    // Because of our max_t limitation, the rhs_remainder might stay small.
    //
    // If it is "too small" we cannot use the code below because it will not be
    // valid. So we just divide divisor into max_scaling bucket. The
    // rhs_remainder will be in the bucket 0.
    //
    // Note(user): This seems the same as just increasing t, modulo integer
    // overflows. Maybe we should just always do the computation like this so
    // that we can use larger t even if coeff is close to kint64max.
    return [t, divisor, max_scaling](IntegerValue coeff) {
      const IntegerValue t_coeff = t * coeff;
      const IntegerValue ratio = FloorRatio(t_coeff, divisor);
      const IntegerValue remainder = PositiveRemainder(t_coeff, divisor);
      const IntegerValue bucket = FloorRatio(remainder * max_scaling, divisor);
      return max_scaling * ratio + bucket;
    };
  } else {
    // We divide (size = divisor - rhs_remainder) into (max_scaling - 1) buckets
    // and increase the function by 1 / max_scaling for each of them.
    //
    // Note that for different values of max_scaling, we get a family of
    // functions that do not dominate each others. So potentially, a max scaling
    // as low as 2 could lead to the better cut (this is exactly the Letchford &
    // Lodi function).
    //
    // Another interesting fact, is that if we want to compute the maximum alpha
    // for a constraint with 2 terms like:
    //    divisor * Y + (ratio * divisor + remainder) * X
    //               <= rhs_ratio * divisor + rhs_remainder
    // so that we have the cut:
    //              Y + (ratio + alpha) * X  <= rhs_ratio
    // This is the same as computing the maximum alpha such that for all integer
    // X > 0 we have CeilRatio(alpha * divisor * X, divisor)
    //    <= CeilRatio(remainder * X - rhs_remainder, divisor).
    // We can prove that this alpha is of the form (n - 1) / n, and it will
    // be reached by such function for a max_scaling of n.
    //
    // TODO(user): This function is not always maximal when
    // size % (max_scaling - 1) == 0. Improve?
    return [size, rhs_remainder, t, divisor, max_scaling](IntegerValue coeff) {
      const IntegerValue t_coeff = t * coeff;
      const IntegerValue ratio = FloorRatio(t_coeff, divisor);
      const IntegerValue remainder = PositiveRemainder(t_coeff, divisor);
      const IntegerValue diff = remainder - rhs_remainder;
      const IntegerValue bucket =
          diff > 0 ? CeilRatio(diff * (max_scaling - 1), size)
                   : IntegerValue(0);
      return max_scaling * ratio + bucket;
    };
  }
}

// TODO(user): This has been optimized a bit, but we can probably do even better
// as it still takes around 25% percent of the run time when all the cuts are on
// for the opm*mps.gz problems and others.
void IntegerRoundingCutHelper::ComputeCut(
    RoundingOptions options, const std::vector<double>& lp_values,
    const std::vector<IntegerValue>& lower_bounds,
    const std::vector<IntegerValue>& upper_bounds,
    ImpliedBoundsProcessor* ib_processor, LinearConstraint* cut) {
  const int size = lp_values.size();
  if (size == 0) return;
  CHECK_EQ(lower_bounds.size(), size);
  CHECK_EQ(upper_bounds.size(), size);
  CHECK_EQ(cut->vars.size(), size);
  CHECK_EQ(cut->coeffs.size(), size);
  CHECK_EQ(cut->lb, kMinIntegerValue);

  // To optimize the computation of the best divisor below, we only need to
  // look at the indices with a shifted lp value that is not close to zero.
  //
  // TODO(user): sort by decreasing lp_values so that our early abort test in
  // the critical loop below has more chance of returning early? I tried but it
  // didn't seems to change much though.
  relevant_indices_.clear();
  relevant_lp_values_.clear();
  relevant_coeffs_.clear();
  relevant_bound_diffs_.clear();
  divisors_.clear();
  adjusted_coeffs_.clear();

  // Compute the maximum magnitude for non-fixed variables.
  IntegerValue tmp(0);
  for (int i = 0; i < size; ++i) {
    if (lower_bounds[i] == upper_bounds[i]) continue;
    const IntegerValue magnitude = IntTypeAbs(cut->coeffs[i]);
    tmp = std::max(tmp, magnitude);
  }
  const IntegerValue max_magnitude = tmp;

  // Shift each variable using its lower/upper bound so that no variable can
  // change sign. We eventually do a change of variable to its negation so
  // that all variable are non-negative.
  bool overflow = false;
  change_sign_at_postprocessing_.assign(size, false);
  for (int i = 0; i < size; ++i) {
    if (cut->coeffs[i] == 0) continue;
    const IntegerValue magnitude = IntTypeAbs(cut->coeffs[i]);

    // We might change them below.
    IntegerValue lb = lower_bounds[i];
    double lp_value = lp_values[i];

    const IntegerValue ub = upper_bounds[i];
    const IntegerValue bound_diff =
        IntegerValue(CapSub(ub.value(), lb.value()));
    // Note that since we use ToDouble() this code works fine with lb/ub at
    // min/max integer value.
    //
    // TODO(user): Experiments with different heuristics. Other solver also
    // seems to try a bunch of possibilities in a "postprocess" phase once
    // the divisor is chosen. Try that.
    {
      // when the magnitude of the entry become smaller and smaller we bias
      // towards a positive coefficient. This is because after rounding this
      // will likely become zero instead of -divisor and we need the lp value
      // to be really close to its bound to compensate.
      const double lb_dist = std::abs(lp_value - ToDouble(lb));
      const double ub_dist = std::abs(lp_value - ToDouble(ub));
      const double bias =
          std::max(1.0, 0.1 * ToDouble(max_magnitude) / ToDouble(magnitude));
      if ((bias * lb_dist > ub_dist && cut->coeffs[i] < 0) ||
          (lb_dist > bias * ub_dist && cut->coeffs[i] > 0)) {
        change_sign_at_postprocessing_[i] = true;
        cut->coeffs[i] = -cut->coeffs[i];
        lp_value = -lp_value;
        lb = -ub;
      }
    }

    // Always shift to lb.
    // coeff * X = coeff * (X - shift) + coeff * shift.
    lp_value -= ToDouble(lb);
    if (!AddProductTo(-cut->coeffs[i], lb, &cut->ub)) {
      overflow = true;
      break;
    }

    // Deal with fixed variable, no need to shift back in this case, we can
    // just remove the term.
    if (bound_diff == 0) {
      cut->coeffs[i] = IntegerValue(0);
      lp_value = 0.0;
    }

    if (std::abs(lp_value) > 1e-2) {
      relevant_coeffs_.push_back(cut->coeffs[i]);
      relevant_indices_.push_back(i);
      relevant_lp_values_.push_back(lp_value);
      relevant_bound_diffs_.push_back(bound_diff);
      divisors_.push_back(magnitude);
    }
  }

  // TODO(user): Maybe this shouldn't be called on such constraint.
  if (relevant_coeffs_.empty()) {
    VLOG(2) << "Issue, nothing to cut.";
    *cut = LinearConstraint(IntegerValue(0), IntegerValue(0));
    return;
  }
  CHECK_NE(max_magnitude, 0);

  // Our heuristic will try to generate a few different cuts, and we will keep
  // the most violated one scaled by the l2 norm of the relevant position.
  //
  // TODO(user): Experiment for the best value of this initial violation
  // threshold. Note also that we use the l2 norm on the restricted position
  // here. Maybe we should change that? On that note, the L2 norm usage seems a
  // bit weird to me since it grows with the number of term in the cut. And
  // often, we already have a good cut, and we make it stronger by adding extra
  // terms that do not change its activity.
  //
  // The discussion above only concern the best_scaled_violation initial value.
  // The remainder_threshold allows to not consider cuts for which the final
  // efficacity is clearly lower than 1e-3 (it is a bound, so we could generate
  // cuts with a lower efficacity than this).
  double best_scaled_violation = 0.01;
  const IntegerValue remainder_threshold(max_magnitude / 1000);

  // Make sure that when we multiply the rhs or the coefficient by a factor t,
  // we do not have an integer overflow. Actually, we need a bit more room
  // because we might round down a value to the next multiple of
  // max_magnitude.
  //
  // The cut->ub might have grown quite a bit with the bound substitution, so
  // we need to include it too since we will apply the rounding function on it.
  {
    const IntegerValue m = std::max(max_magnitude, IntTypeAbs(cut->ub));
    const IntegerValue overflow_threshold = kMaxIntegerValue / 2;
    if (overflow || m >= overflow_threshold) {
      VLOG(2) << "Issue, overflow.";
      *cut = LinearConstraint(IntegerValue(0), IntegerValue(0));
      return;
    }
    tmp = overflow_threshold / m;
  }
  const IntegerValue max_t = tmp;

  // There is no point trying twice the same divisor or a divisor that is too
  // small. Note that we use a higher threshold than the remainder_threshold
  // because we can boost the remainder thanks to our adjusting heuristic below
  // and also because this allows to have cuts with a small range of
  // coefficients.
  //
  // TODO(user): Note that the std::sort() is visible in some cpu profile.
  {
    int new_size = 0;
    const IntegerValue divisor_threshold = max_magnitude / 10;
    for (int i = 0; i < divisors_.size(); ++i) {
      if (divisors_[i] <= divisor_threshold) continue;
      divisors_[new_size++] = divisors_[i];
    }
    divisors_.resize(new_size);
  }
  gtl::STLSortAndRemoveDuplicates(&divisors_, std::greater<IntegerValue>());

  // TODO(user): Avoid quadratic algorithm? Note that we are quadratic in
  // relevant_indices not the full cut->coeffs.size(), but this is still too
  // much on some problems.
  IntegerValue best_divisor(0);
  for (const IntegerValue divisor : divisors_) {
    // Skip if we don't have the potential to generate a good enough cut.
    const IntegerValue initial_rhs_remainder =
        cut->ub - FloorRatio(cut->ub, divisor) * divisor;
    if (initial_rhs_remainder <= remainder_threshold) continue;

    IntegerValue temp_ub = cut->ub;
    adjusted_coeffs_.clear();

    // We will adjust coefficient that are just under an exact multiple of
    // divisor to an exact multiple. This is meant to get rid of small errors
    // that appears due to rounding error in our exact computation of the
    // initial constraint given to this class.
    //
    // Each adjustement will cause the initial_rhs_remainder to increase, and we
    // do not want to increase it above divisor. Our threshold below guarantees
    // this. Note that the higher the rhs_remainder becomes, the more the
    // function f() has a chance to reduce the violation, so it is not always a
    // good idea to use all the slack we have between initial_rhs_remainder and
    // divisor.
    //
    // TODO(user): If possible, it might be better to complement these
    // variables. Even if the adjusted lp_values end up larger, if we loose less
    // when taking f(), then we will have a better violation.
    const IntegerValue adjust_threshold =
        (divisor - initial_rhs_remainder - 1) / IntegerValue(size);
    if (adjust_threshold > 0) {
      // Even before we finish the adjust, we can have a lower bound on the
      // activily loss using this divisor, and so we can abort early. This is
      // similar to what is done below in the function.
      bool early_abort = false;
      double loss_lb = 0.0;
      const double threshold = ToDouble(initial_rhs_remainder);

      for (int i = 0; i < relevant_coeffs_.size(); ++i) {
        // Compute the difference of coeff with the next multiple of divisor.
        const IntegerValue coeff = relevant_coeffs_[i];
        const IntegerValue remainder =
            CeilRatio(coeff, divisor) * divisor - coeff;

        if (divisor - remainder <= initial_rhs_remainder) {
          // We do not know exactly f() yet, but it will always round to the
          // floor of the division by divisor in this case.
          loss_lb += ToDouble(divisor - remainder) * relevant_lp_values_[i];
          if (loss_lb >= threshold) {
            early_abort = true;
            break;
          }
        }

        // Adjust coeff of the form k * divisor - epsilon.
        const IntegerValue diff = relevant_bound_diffs_[i];
        if (remainder > 0 && remainder <= adjust_threshold &&
            CapProd(diff.value(), remainder.value()) <= adjust_threshold) {
          temp_ub += remainder * diff;
          adjusted_coeffs_.push_back({i, coeff + remainder});
        }
      }

      if (early_abort) continue;
    }

    // Create the super-additive function f().
    const IntegerValue rhs_remainder =
        temp_ub - FloorRatio(temp_ub, divisor) * divisor;
    if (rhs_remainder == 0) continue;

    const auto f = GetSuperAdditiveRoundingFunction(
        rhs_remainder, divisor, GetFactorT(rhs_remainder, divisor, max_t),
        options.max_scaling);

    // As we round coefficients, we will compute the loss compared to the
    // current scaled constraint activity. As soon as this loss crosses the
    // slack, then we known that there is no violation and we can abort early.
    //
    // TODO(user): modulo the scaling, we could compute the exact threshold
    // using our current best cut. Note that we also have to account the change
    // in slack due to the adjust code above.
    const double scaling = ToDouble(f(divisor)) / ToDouble(divisor);
    const double threshold = scaling * ToDouble(rhs_remainder);
    double loss = 0.0;

    // Apply f() to the cut and compute the cut violation. Note that it is
    // okay to just look at the relevant indices since the other have a lp
    // value which is almost zero. Doing it like this is faster, and even if
    // the max_magnitude might be off it should still be relevant enough.
    double violation = -ToDouble(f(temp_ub));
    double l2_norm = 0.0;
    bool early_abort = false;
    int adjusted_coeffs_index = 0;
    for (int i = 0; i < relevant_coeffs_.size(); ++i) {
      IntegerValue coeff = relevant_coeffs_[i];

      // Adjust coeff according to our previous computation if needed.
      if (adjusted_coeffs_index < adjusted_coeffs_.size() &&
          adjusted_coeffs_[adjusted_coeffs_index].first == i) {
        coeff = adjusted_coeffs_[adjusted_coeffs_index].second;
        adjusted_coeffs_index++;
      }

      if (coeff == 0) continue;
      const IntegerValue new_coeff = f(coeff);
      const double new_coeff_double = ToDouble(new_coeff);
      const double lp_value = relevant_lp_values_[i];

      l2_norm += new_coeff_double * new_coeff_double;
      violation += new_coeff_double * lp_value;
      loss += (scaling * ToDouble(coeff) - new_coeff_double) * lp_value;
      if (loss >= threshold) {
        early_abort = true;
        break;
      }
    }
    if (early_abort) continue;

    // Here we scale by the L2 norm over the "relevant" positions. This seems
    // to work slighly better in practice.
    violation /= sqrt(l2_norm);
    if (violation > best_scaled_violation) {
      best_scaled_violation = violation;
      best_divisor = divisor;
    }
  }

  if (best_divisor == 0) {
    *cut = LinearConstraint(IntegerValue(0), IntegerValue(0));
    return;
  }

  // Adjust coefficients.
  //
  // TODO(user): It might make sense to also adjust the one with a small LP
  // value, but then the cut will be slighlty different than the one we computed
  // above. Try with and without maybe?
  const IntegerValue initial_rhs_remainder =
      cut->ub - FloorRatio(cut->ub, best_divisor) * best_divisor;
  const IntegerValue adjust_threshold =
      (best_divisor - initial_rhs_remainder - 1) / IntegerValue(size);
  if (adjust_threshold > 0) {
    for (int i = 0; i < relevant_indices_.size(); ++i) {
      const int index = relevant_indices_[i];
      const IntegerValue diff = relevant_bound_diffs_[i];
      if (diff > adjust_threshold) continue;

      // Adjust coeff of the form k * best_divisor - epsilon.
      const IntegerValue coeff = cut->coeffs[index];
      const IntegerValue remainder =
          CeilRatio(coeff, best_divisor) * best_divisor - coeff;
      if (CapProd(diff.value(), remainder.value()) <= adjust_threshold) {
        cut->ub += remainder * diff;
        cut->coeffs[index] += remainder;
      }
    }
  }

  // Create the super-additive function f().
  //
  // TODO(user): Try out different rounding function and keep the best. We can
  // change max_t and max_scaling. It might not be easy to choose which cut is
  // the best, but we can at least know for sure if one dominate the other
  // completely. That is, if for all coeff f(coeff)/f(divisor) is greater than
  // or equal to the same value for another function f.
  const IntegerValue rhs_remainder =
      cut->ub - FloorRatio(cut->ub, best_divisor) * best_divisor;
  IntegerValue factor_t = GetFactorT(rhs_remainder, best_divisor, max_t);
  auto f = GetSuperAdditiveRoundingFunction(rhs_remainder, best_divisor,
                                            factor_t, options.max_scaling);

  // Look amongst all our possible function f() for one that dominate greedily
  // our current best one. Note that we prefer lower scaling factor since that
  // result in a cut with lower coefficients.
  remainders_.clear();
  for (int i = 0; i < size; ++i) {
    const IntegerValue coeff = cut->coeffs[i];
    const IntegerValue r =
        coeff - FloorRatio(coeff, best_divisor) * best_divisor;
    if (r > rhs_remainder) remainders_.push_back(r);
  }
  gtl::STLSortAndRemoveDuplicates(&remainders_);
  if (remainders_.size() <= 100) {
    best_rs_.clear();
    for (const IntegerValue r : remainders_) {
      best_rs_.push_back(f(r));
    }
    IntegerValue best_d = f(best_divisor);

    // Note that the complexity seems high 100 * 2 * options.max_scaling, but
    // this only run on cuts that are already efficient and the inner loop tend
    // to abort quickly. I didn't see this code in the cpu profile so far.
    for (const IntegerValue t :
         {IntegerValue(1), GetFactorT(rhs_remainder, best_divisor, max_t)}) {
      for (IntegerValue s(2); s <= options.max_scaling; ++s) {
        const auto g =
            GetSuperAdditiveRoundingFunction(rhs_remainder, best_divisor, t, s);
        int num_strictly_better = 0;
        rs_.clear();
        const IntegerValue d = g(best_divisor);
        for (int i = 0; i < best_rs_.size(); ++i) {
          const IntegerValue temp = g(remainders_[i]);
          if (temp * best_d < best_rs_[i] * d) break;
          if (temp * best_d > best_rs_[i] * d) num_strictly_better++;
          rs_.push_back(temp);
        }
        if (rs_.size() == best_rs_.size() && num_strictly_better > 0) {
          f = g;
          factor_t = t;
          best_rs_ = rs_;
          best_d = d;
        }
      }
    }
  }

  // Starts to apply f() to the cut. We only apply it to the rhs here, the
  // coefficient will be done after the potential lifting of some Booleans.
  cut->ub = f(cut->ub);
  tmp_terms_.clear();

  // Lift some implied bounds Booleans. Note that we will add them after
  // "size" so they will be ignored in the second loop below.
  num_lifted_booleans_ = 0;
  if (ib_processor != nullptr) {
    for (int i = 0; i < size; ++i) {
      const IntegerValue coeff = cut->coeffs[i];
      if (coeff == 0) continue;

      IntegerVariable var = cut->vars[i];
      if (change_sign_at_postprocessing_[i]) {
        var = NegationOf(var);
      }

      const ImpliedBoundsProcessor::BestImpliedBoundInfo info =
          ib_processor->GetCachedImpliedBoundInfo(var);

      // Avoid overflow.
      if (CapProd(CapProd(std::abs(coeff.value()), factor_t.value()),
                  info.bound_diff.value()) ==
          std::numeric_limits<int64_t>::max()) {
        continue;
      }

      // Because X = bound_diff * B + S
      // We can replace coeff * X by the expression before applying f:
      //   = f(coeff * bound_diff) * B + f(coeff) * [X - bound_diff * B]
      //   = f(coeff) * X + (f(coeff * bound_diff) - f(coeff) * bound_diff] B
      // So we can "lift" B into the cut.
      const IntegerValue coeff_b =
          f(coeff * info.bound_diff) - f(coeff) * info.bound_diff;
      CHECK_GE(coeff_b, 0);
      if (coeff_b == 0) continue;

      ++num_lifted_booleans_;
      if (info.is_positive) {
        tmp_terms_.push_back({info.bool_var, coeff_b});
      } else {
        tmp_terms_.push_back({info.bool_var, -coeff_b});
        cut->ub = CapAdd(-coeff_b.value(), cut->ub.value());
      }
    }
  }

  // Apply f() to the cut.
  //
  // Remove the bound shifts so the constraint is expressed in the original
  // variables.
  for (int i = 0; i < size; ++i) {
    IntegerValue coeff = cut->coeffs[i];
    if (coeff == 0) continue;
    coeff = f(coeff);
    if (coeff == 0) continue;
    if (change_sign_at_postprocessing_[i]) {
      if (!AddProductTo(coeff, -upper_bounds[i], &cut->ub)) {
        // Abort with a trivially satisfied cut.
        cut->Clear();
        return;
      }
      tmp_terms_.push_back({cut->vars[i], -coeff});
    } else {
      if (!AddProductTo(coeff, lower_bounds[i], &cut->ub)) {
        // Abort with a trivially satisfied cut.
        cut->Clear();
        return;
      }
      tmp_terms_.push_back({cut->vars[i], coeff});
    }
  }

  // Basic post-processing.
  CleanTermsAndFillConstraint(&tmp_terms_, cut);
  RemoveZeroTerms(cut);
  DivideByGCD(cut);
}

bool CoverCutHelper::TrySimpleKnapsack(
    const LinearConstraint base_ct, const std::vector<double>& lp_values,
    const std::vector<IntegerValue>& lower_bounds,
    const std::vector<IntegerValue>& upper_bounds) {
  const int base_size = lp_values.size();

  // Fill terms with a rewrite of the base constraint where all coeffs &
  // variables are positive by using either (X - LB) or (UB - X) as new
  // variables.
  terms_.clear();
  IntegerValue rhs = base_ct.ub;
  IntegerValue sum_of_diff(0);
  IntegerValue max_base_magnitude(0);
  for (int i = 0; i < base_size; ++i) {
    const IntegerValue coeff = base_ct.coeffs[i];
    const IntegerValue positive_coeff = IntTypeAbs(coeff);
    max_base_magnitude = std::max(max_base_magnitude, positive_coeff);
    const IntegerValue bound_diff = upper_bounds[i] - lower_bounds[i];
    if (!AddProductTo(positive_coeff, bound_diff, &sum_of_diff)) {
      return false;
    }
    const IntegerValue diff = positive_coeff * bound_diff;
    if (coeff > 0) {
      if (!AddProductTo(-coeff, lower_bounds[i], &rhs)) return false;
      terms_.push_back(
          {i, ToDouble(upper_bounds[i]) - lp_values[i], positive_coeff, diff});
    } else {
      if (!AddProductTo(-coeff, upper_bounds[i], &rhs)) return false;
      terms_.push_back(
          {i, lp_values[i] - ToDouble(lower_bounds[i]), positive_coeff, diff});
    }
  }
  const IntegerValue base_rhs = rhs;

  // Try a simple cover heuristic.
  // Look for violated CUT of the form: sum (UB - X) or (X - LB) >= 1.
  double activity = 0.0;
  int new_size = 0;
  std::sort(terms_.begin(), terms_.end(), [](const Term& a, const Term& b) {
    if (a.dist_to_max_value == b.dist_to_max_value) {
      // Prefer low coefficients if the distance is the same.
      return a.positive_coeff < b.positive_coeff;
    }
    return a.dist_to_max_value < b.dist_to_max_value;
  });
  for (int i = 0; i < terms_.size(); ++i) {
    const Term& term = terms_[i];
    activity += term.dist_to_max_value;

    // As an heuristic we select all the term so that the sum of distance
    // to the upper bound is <= 1.0. If the corresponding rhs is negative, then
    // we will have a cut of violation at least 0.0. Note that this violation
    // can be improved by the lifting.
    //
    // TODO(user): experiment with different threshold (even greater than one).
    // Or come up with an algo that incorporate the lifting into the heuristic.
    if (activity > 1.0) {
      new_size = i;  // before this entry.
      break;
    }

    rhs -= term.diff;
  }

  // If the rhs is now negative, we have a cut.
  //
  // Note(user): past this point, now that a given "base" cover has been chosen,
  // we basically compute the cut (of the form sum X <= bound) with the maximum
  // possible violation. Note also that we lift as much as possible, so we don't
  // necessarily optimize for the cut efficacity though. But we do get a
  // stronger cut.
  if (rhs >= 0) return false;
  if (new_size == 0) return false;

  // Transform to a minimal cover. We want to greedily remove the largest coeff
  // first, so we have more chance for the "lifting" below which can increase
  // the cut violation. If the coeff are the same, we prefer to remove high
  // distance from upper bound first.
  //
  // We compute the cut at the same time.
  terms_.resize(new_size);
  std::sort(terms_.begin(), terms_.end(), [](const Term& a, const Term& b) {
    if (a.positive_coeff == b.positive_coeff) {
      return a.dist_to_max_value > b.dist_to_max_value;
    }
    return a.positive_coeff > b.positive_coeff;
  });
  in_cut_.assign(base_ct.vars.size(), false);
  cut_.ClearTerms();
  cut_.lb = kMinIntegerValue;
  cut_.ub = IntegerValue(-1);
  IntegerValue max_coeff(0);
  for (const Term term : terms_) {
    if (term.diff + rhs < 0) {
      rhs += term.diff;
      continue;
    }
    in_cut_[term.index] = true;
    max_coeff = std::max(max_coeff, term.positive_coeff);
    cut_.vars.push_back(base_ct.vars[term.index]);
    if (base_ct.coeffs[term.index] > 0) {
      cut_.coeffs.push_back(IntegerValue(1));
      cut_.ub += upper_bounds[term.index];
    } else {
      cut_.coeffs.push_back(IntegerValue(-1));
      cut_.ub -= lower_bounds[term.index];
    }
  }

  // Generate alternative cut.
  GenerateLetchfordSouliLifting(base_rhs, base_ct, lower_bounds, upper_bounds,
                                in_cut_);

  // In case the max_coeff variable is not binary, it might be possible to
  // tighten the cut a bit more.
  //
  // Note(user): I never observed this on the miplib so far.
  if (max_coeff == 0) return true;
  if (max_coeff < -rhs) {
    const IntegerValue m = FloorRatio(-rhs - 1, max_coeff);
    rhs += max_coeff * m;
    cut_.ub -= m;
  }
  CHECK_LT(rhs, 0);

  // Lift all at once the variables not used in the cover.
  //
  // We have a cut of the form sum_i X_i <= b that we will lift into
  // sum_i scaling X_i + sum f(base_coeff_j) X_j <= b * scaling.
  //
  // Using the super additivity of f() and how we construct it,
  // we know that: sum_j base_coeff_j X_j <= N * max_coeff + (max_coeff - slack)
  // implies that: sum_j f(base_coeff_j) X_j <= N * scaling.
  //
  // 1/ cut > b -(N+1)  => original sum + (N+1) * max_coeff >= rhs + slack
  // 2/ rewrite 1/ as : scaling * cut >= scaling * b - scaling * N => ...
  // 3/ lift > N * scaling => lift_sum > N * max_coeff + (max_coeff - slack)
  // And adding 2/ + 3/ we prove what we want:
  // cut * scaling + lift > b * scaling => original_sum + lift_sum > rhs.
  const IntegerValue slack = -rhs;
  const IntegerValue remainder = max_coeff - slack;
  max_base_magnitude = std::max(max_base_magnitude, IntTypeAbs(cut_.ub));
  const IntegerValue max_scaling(std::min(
      IntegerValue(60), FloorRatio(kMaxIntegerValue, max_base_magnitude)));
  const auto f = GetSuperAdditiveRoundingFunction(remainder, max_coeff,
                                                  IntegerValue(1), max_scaling);

  const IntegerValue scaling = f(max_coeff);
  if (scaling > 1) {
    for (int i = 0; i < cut_.coeffs.size(); ++i) cut_.coeffs[i] *= scaling;
    cut_.ub *= scaling;
  }

  num_lifting_ = 0;
  for (int i = 0; i < base_size; ++i) {
    if (in_cut_[i]) continue;
    const IntegerValue positive_coeff = IntTypeAbs(base_ct.coeffs[i]);
    const IntegerValue new_coeff = f(positive_coeff);
    if (new_coeff == 0) continue;

    ++num_lifting_;
    if (base_ct.coeffs[i] > 0) {
      // Add new_coeff * (X - LB)
      cut_.coeffs.push_back(new_coeff);
      cut_.vars.push_back(base_ct.vars[i]);
      cut_.ub += lower_bounds[i] * new_coeff;
    } else {
      // Add new_coeff * (UB - X)
      cut_.coeffs.push_back(-new_coeff);
      cut_.vars.push_back(base_ct.vars[i]);
      cut_.ub -= upper_bounds[i] * new_coeff;
    }
  }

  if (scaling > 1) DivideByGCD(&cut_);
  return true;
}

void CoverCutHelper::GenerateLetchfordSouliLifting(
    IntegerValue base_rhs, const LinearConstraint base_ct,
    const std::vector<IntegerValue>& lower_bounds,
    const std::vector<IntegerValue>& upper_bounds,
    const std::vector<bool>& in_cover) {
  alt_cut_.Clear();

  // Collect the weight in the cover.
  IntegerValue sum(0);
  std::vector<IntegerValue> cover_weights;
  const int base_size = lower_bounds.size();
  for (int i = 0; i < base_size; ++i) {
    if (in_cover[i]) {
      cover_weights.push_back(IntTypeAbs(base_ct.coeffs[i]));
      sum += cover_weights.back();

      // TODO(user): we currently only deal with Boolean in the cover. Fix.
      if (upper_bounds[i] - lower_bounds[i] != 1) return;
    }
  }
  CHECK_GT(sum, base_rhs);

  // Compute the correct threshold so that if we round down larger weights to
  // p/q. We have sum of the weight in cover == base_rhs.
  IntegerValue p(0);
  IntegerValue q(0);
  IntegerValue previous_sum(0);
  std::sort(cover_weights.begin(), cover_weights.end());
  const int cover_size = cover_weights.size();
  for (int i = 0; i < cover_size; ++i) {
    q = IntegerValue(cover_weights.size() - i);
    if (previous_sum + cover_weights[i] * q > base_rhs) {
      p = base_rhs - previous_sum;
      break;
    }
    previous_sum += cover_weights[i];
  }
  CHECK_GE(q, 1);

  // Compute thresholds.
  // For the first q values, thresholds[i] is the smallest integer such that
  // q * threshold[i] > p * (i + 1).
  std::vector<IntegerValue> thresholds;
  for (int i = 0; i < q; ++i) {
    // TODO(user): compute this in an overflow-safe way.
    if (CapProd(p.value(), i + 1) >= std::numeric_limits<int64_t>::max() - 1) {
      return;
    }
    thresholds.push_back(CeilRatio(p * (i + 1) + 1, q));
  }
  // For the other values, we just add the weights.
  std::reverse(cover_weights.begin(), cover_weights.end());
  for (int i = q.value(); i < cover_size; ++i) {
    thresholds.push_back(thresholds.back() + cover_weights[i]);
  }
  CHECK_EQ(thresholds.back(), base_rhs + 1);

  // Generate the cut.
  //
  // Our algo is quadratic in worst case, but large coefficients should be
  // rare, and in practice we don't really see this.
  //
  // Note that this work for non-Boolean since we can just "theorically" split
  // them as a sum of Booleans :) Probably a cleaner proof exist by just using
  // the super-additivity of the lifting function on [0, rhs].
  alt_cut_.Clear();
  alt_cut_.lb = kMinIntegerValue;
  alt_cut_.ub = IntegerValue(cover_size - 1);
  int lifted_size = 0;
  for (int i = 0; i < base_size; ++i) {
    const IntegerValue coeff = IntTypeAbs(base_ct.coeffs[i]);
    IntegerValue cut_coeff(1);
    if (coeff < thresholds[0]) {
      if (!in_cover[i]) continue;
    } else {
      // Find the largest index <= coeff.
      //
      // TODO(user): For exact multiple of p/q we can increase the coeff by 1/2.
      // See section in the paper on getting maximal super additive function.
      for (int i = 1; i < cover_size; ++i) {
        if (coeff < thresholds[i]) break;
        cut_coeff = IntegerValue(i + 1);
      }
    }

    ++lifted_size;
    if (base_ct.coeffs[i] > 0) {
      // Add new_coeff * (X - LB)
      alt_cut_.coeffs.push_back(cut_coeff);
      alt_cut_.vars.push_back(base_ct.vars[i]);
      alt_cut_.ub += lower_bounds[i] * cut_coeff;
    } else {
      // Add new_coeff * (UB - X)
      alt_cut_.coeffs.push_back(-cut_coeff);
      alt_cut_.vars.push_back(base_ct.vars[i]);
      alt_cut_.ub -= upper_bounds[i] * cut_coeff;
    }
  }
}

CutGenerator CreatePositiveMultiplicationCutGenerator(AffineExpression z,
                                                      AffineExpression x,
                                                      AffineExpression y,
                                                      int linearization_level,
                                                      Model* model) {
  CutGenerator result;
  if (z.var != kNoIntegerVariable) result.vars.push_back(z.var);
  if (x.var != kNoIntegerVariable) result.vars.push_back(x.var);
  if (y.var != kNoIntegerVariable) result.vars.push_back(y.var);

  IntegerTrail* const integer_trail = model->GetOrCreate<IntegerTrail>();
  Trail* trail = model->GetOrCreate<Trail>();

  result.generate_cuts =
      [z, x, y, linearization_level, model, trail, integer_trail](
          const absl::StrongVector<IntegerVariable, double>& lp_values,
          LinearConstraintManager* manager) {
        if (trail->CurrentDecisionLevel() > 0 && linearization_level == 1) {
          return true;
        }
        const int64_t x_lb = integer_trail->LevelZeroLowerBound(x).value();
        const int64_t x_ub = integer_trail->LevelZeroUpperBound(x).value();
        const int64_t y_lb = integer_trail->LevelZeroLowerBound(y).value();
        const int64_t y_ub = integer_trail->LevelZeroUpperBound(y).value();

        // if x or y are fixed, the McCormick equations are exact.
        if (x_lb == x_ub || y_lb == y_ub) return true;

        // TODO(user): Compute a better bound (int_max / 4 ?).
        constexpr int64_t kMaxSafeInteger = (int64_t{1} << 53) - 1;

        // Check for overflow with the product of expression bounds and the
        // product of one expression bound times the constant part of the other
        // expression.
        const int64_t x_max_amp = std::max(std::abs(x_lb), std::abs(x_ub));
        const int64_t y_max_amp = std::max(std::abs(y_lb), std::abs(y_ub));
        if (CapProd(x_max_amp, y_max_amp) > kMaxSafeInteger) return true;

        const double x_lp_value = x.LpValue(lp_values);
        const double y_lp_value = y.LpValue(lp_values);
        const double z_lp_value = z.LpValue(lp_values);

        // TODO(user): As the bounds change monotonically, these cuts
        // dominate any previous one.  try to keep a reference to the cut and
        // replace it. Alternatively, add an API for a level-zero bound change
        // callback.

        // Cut -z + x_coeff * x + y_coeff* y <= rhs
        auto try_add_above_cut = [&](int64_t x_coeff, int64_t y_coeff,
                                     int64_t rhs) {
          if (-z_lp_value + x_lp_value * x_coeff + y_lp_value * y_coeff >=
              rhs + kMinCutViolation) {
            // Checks for overflows.
            if (CapProd(x_max_amp, std::abs(x_coeff)) > kMaxSafeInteger) return;
            if (CapProd(y_max_amp, std::abs(y_coeff)) > kMaxSafeInteger) return;

            LinearConstraintBuilder cut(model, /*lb=*/kMinIntegerValue,
                                        /*ub=*/IntegerValue(rhs));
            cut.AddTerm(z, IntegerValue(-1));
            if (x_coeff != 0) cut.AddTerm(x, IntegerValue(x_coeff));
            if (y_coeff != 0) cut.AddTerm(y, IntegerValue(y_coeff));
            manager->AddCut(cut.Build(), "PositiveProduct", lp_values);
          }
        };

        // Cut -z + x_coeff * x + y_coeff* y >= rhs
        auto try_add_below_cut = [&](int64_t x_coeff, int64_t y_coeff,
                                     int64_t rhs) {
          if (-z_lp_value + x_lp_value * x_coeff + y_lp_value * y_coeff <=
              rhs - kMinCutViolation) {
            // Checks for overflow.
            if (CapProd(x_max_amp, std::abs(x_coeff)) > kMaxSafeInteger) return;
            if (CapProd(y_max_amp, std::abs(y_coeff)) > kMaxSafeInteger) return;

            LinearConstraintBuilder cut(model, /*lb=*/IntegerValue(rhs),
                                        /*ub=*/kMaxIntegerValue);
            cut.AddTerm(z, IntegerValue(-1));
            if (x_coeff != 0) cut.AddTerm(x, IntegerValue(x_coeff));
            if (y_coeff != 0) cut.AddTerm(y, IntegerValue(y_coeff));
            manager->AddCut(cut.Build(), "PositiveProduct", lp_values);
          }
        };

        // McCormick relaxation of bilinear constraints. These 4 cuts are the
        // exact facets of the x * y polyhedron for a bounded x and y.
        //
        // Each cut correspond to plane that contains two of the line
        // (x=x_lb), (x=x_ub), (y=y_lb), (y=y_ub). The easiest to
        // understand them is to draw the x*y curves and see the 4
        // planes that correspond to the convex hull of the graph.
        try_add_above_cut(y_lb, x_lb, x_lb * y_lb);
        try_add_above_cut(y_ub, x_ub, x_ub * y_ub);
        try_add_below_cut(y_ub, x_lb, x_lb * y_ub);
        try_add_below_cut(y_lb, x_ub, x_ub * y_lb);
        return true;
      };

  return result;
}

CutGenerator CreateSquareCutGenerator(AffineExpression y, AffineExpression x,
                                      int linearization_level, Model* model) {
  CutGenerator result;
  if (x.var != kNoIntegerVariable) result.vars.push_back(x.var);
  if (y.var != kNoIntegerVariable) result.vars.push_back(y.var);

  Trail* trail = model->GetOrCreate<Trail>();
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  result.generate_cuts =
      [y, x, linearization_level, trail, integer_trail, model](
          const absl::StrongVector<IntegerVariable, double>& lp_values,
          LinearConstraintManager* manager) {
        if (trail->CurrentDecisionLevel() > 0 && linearization_level == 1) {
          return true;
        }
        const int64_t x_ub = integer_trail->LevelZeroUpperBound(x).value();
        const int64_t x_lb = integer_trail->LevelZeroLowerBound(x).value();

        if (x_lb == x_ub) return true;

        // Check for potential overflows.
        if (x_ub > (int64_t{1} << 31)) return true;
        DCHECK_GE(x_lb, 0);

        const double y_lp_value = y.LpValue(lp_values);
        const double x_lp_value = x.LpValue(lp_values);

        // First cut: target should be below the line:
        //     (x_lb, x_lb ^ 2) to (x_ub, x_ub ^ 2).
        // The slope of that line is (ub^2 - lb^2) / (ub - lb) = ub + lb.
        const int64_t y_lb = x_lb * x_lb;
        const int64_t above_slope = x_ub + x_lb;
        const double max_lp_y = y_lb + above_slope * (x_lp_value - x_lb);
        if (y_lp_value >= max_lp_y + kMinCutViolation) {
          // cut: y <= (x_lb + x_ub) * x - x_lb * x_ub
          LinearConstraintBuilder above_cut(model, kMinIntegerValue,
                                            IntegerValue(-x_lb * x_ub));
          above_cut.AddTerm(y, IntegerValue(1));
          above_cut.AddTerm(x, IntegerValue(-above_slope));
          manager->AddCut(above_cut.Build(), "SquareUpper", lp_values);
        }

        // Second cut: target should be above all the lines
        //     (value, value ^ 2) to (value + 1, (value + 1) ^ 2)
        // The slope of that line is 2 * value + 1
        //
        // Note that we only add one of these cuts. The one for x_lp_value in
        // [value, value + 1].
        const int64_t x_floor = static_cast<int64_t>(std::floor(x_lp_value));
        const int64_t below_slope = 2 * x_floor + 1;
        const double min_lp_y =
            below_slope * x_lp_value - x_floor - x_floor * x_floor;
        if (min_lp_y >= y_lp_value + kMinCutViolation) {
          // cut: y >= below_slope * (x - x_floor) + x_floor ^ 2
          //    : y >= below_slope * x - x_floor ^ 2 - x_floor
          LinearConstraintBuilder below_cut(
              model, IntegerValue(-x_floor - x_floor * x_floor),
              kMaxIntegerValue);
          below_cut.AddTerm(y, IntegerValue(1));
          below_cut.AddTerm(x, -IntegerValue(below_slope));
          manager->AddCut(below_cut.Build(), "SquareLower", lp_values);
        }
        return true;
      };

  return result;
}

void ImpliedBoundsProcessor::ProcessUpperBoundedConstraint(
    const absl::StrongVector<IntegerVariable, double>& lp_values,
    LinearConstraint* cut) {
  ProcessUpperBoundedConstraintWithSlackCreation(
      /*substitute_only_inner_variables=*/false, IntegerVariable(0), lp_values,
      cut, nullptr);
}

ImpliedBoundsProcessor::BestImpliedBoundInfo
ImpliedBoundsProcessor::GetCachedImpliedBoundInfo(IntegerVariable var) const {
  auto it = cache_.find(var);
  if (it != cache_.end()) return it->second;
  return BestImpliedBoundInfo();
}

ImpliedBoundsProcessor::BestImpliedBoundInfo
ImpliedBoundsProcessor::ComputeBestImpliedBound(
    IntegerVariable var,
    const absl::StrongVector<IntegerVariable, double>& lp_values) {
  auto it = cache_.find(var);
  if (it != cache_.end()) return it->second;
  BestImpliedBoundInfo result;
  const IntegerValue lb = integer_trail_->LevelZeroLowerBound(var);
  for (const ImpliedBoundEntry& entry :
       implied_bounds_->GetImpliedBounds(var)) {
    // Only process entries with a Boolean variable currently part of the LP
    // we are considering for this cut.
    //
    // TODO(user): the more we use cuts, the less it make sense to have a
    // lot of small independent LPs.
    if (!lp_vars_.contains(PositiveVariable(entry.literal_view))) {
      continue;
    }

    // The equation is X = lb + diff * Bool + Slack where Bool is in [0, 1]
    // and slack in [0, ub - lb].
    const IntegerValue diff = entry.lower_bound - lb;
    CHECK_GE(diff, 0);
    const double bool_lp_value = entry.is_positive
                                     ? lp_values[entry.literal_view]
                                     : 1.0 - lp_values[entry.literal_view];
    const double slack_lp_value =
        lp_values[var] - ToDouble(lb) - bool_lp_value * ToDouble(diff);

    // If the implied bound equation is not respected, we just add it
    // to implied_bound_cuts, and skip the entry for now.
    if (slack_lp_value < -1e-4) {
      LinearConstraint ib_cut;
      ib_cut.lb = kMinIntegerValue;
      std::vector<std::pair<IntegerVariable, IntegerValue>> terms;
      if (entry.is_positive) {
        // X >= Indicator * (bound - lb) + lb
        terms.push_back({entry.literal_view, diff});
        terms.push_back({var, IntegerValue(-1)});
        ib_cut.ub = -lb;
      } else {
        // X >= -Indicator * (bound - lb) + bound
        terms.push_back({entry.literal_view, -diff});
        terms.push_back({var, IntegerValue(-1)});
        ib_cut.ub = -entry.lower_bound;
      }
      CleanTermsAndFillConstraint(&terms, &ib_cut);
      ib_cut_pool_.AddCut(std::move(ib_cut), "IB", lp_values);
      continue;
    }

    // We look for tight implied bounds, and amongst the tightest one, we
    // prefer larger coefficient in front of the Boolean.
    if (slack_lp_value + 1e-4 < result.slack_lp_value ||
        (slack_lp_value < result.slack_lp_value + 1e-4 &&
         diff > result.bound_diff)) {
      result.bool_lp_value = bool_lp_value;
      result.slack_lp_value = slack_lp_value;

      result.bound_diff = diff;
      result.is_positive = entry.is_positive;
      result.bool_var = entry.literal_view;
    }
  }
  cache_[var] = result;
  return result;
}

void ImpliedBoundsProcessor::RecomputeCacheAndSeparateSomeImpliedBoundCuts(
    const absl::StrongVector<IntegerVariable, double>& lp_values) {
  cache_.clear();
  for (const IntegerVariable var :
       implied_bounds_->VariablesWithImpliedBounds()) {
    if (!lp_vars_.contains(PositiveVariable(var))) continue;
    ComputeBestImpliedBound(var, lp_values);
  }
}

void ImpliedBoundsProcessor::ProcessUpperBoundedConstraintWithSlackCreation(
    bool substitute_only_inner_variables, IntegerVariable first_slack,
    const absl::StrongVector<IntegerVariable, double>& lp_values,
    LinearConstraint* cut, std::vector<SlackInfo>* slack_infos) {
  if (cache_.empty()) return;  // Nothing to do.
  tmp_terms_.clear();
  IntegerValue new_ub = cut->ub;
  bool changed = false;

  // TODO(user): we could relax a bit this test.
  int64_t overflow_detection = 0;

  const int size = cut->vars.size();
  for (int i = 0; i < size; ++i) {
    IntegerVariable var = cut->vars[i];
    IntegerValue coeff = cut->coeffs[i];

    // Starts by positive coefficient.
    // TODO(user): Not clear this is best.
    if (coeff < 0) {
      coeff = -coeff;
      var = NegationOf(var);
    }

    // Find the best implied bound to use.
    // TODO(user): We could also use implied upper bound, that is try with
    // NegationOf(var).
    const BestImpliedBoundInfo info = GetCachedImpliedBoundInfo(var);
    const int old_size = tmp_terms_.size();

    // Shall we keep the original term ?
    bool keep_term = false;
    if (info.bool_var == kNoIntegerVariable) keep_term = true;
    if (CapProd(std::abs(coeff.value()), info.bound_diff.value()) ==
        std::numeric_limits<int64_t>::max()) {
      keep_term = true;
    }

    // TODO(user): On some problem, not replacing the variable at their bound
    // by an implied bounds seems beneficial. This is especially the case on
    // g200x740.mps.gz
    //
    // Note that in ComputeCut() the variable with an LP value at the bound do
    // not contribute to the cut efficacity (no loss) but do contribute to the
    // various heuristic based on the coefficient magnitude.
    if (substitute_only_inner_variables) {
      const IntegerValue lb = integer_trail_->LevelZeroLowerBound(var);
      const IntegerValue ub = integer_trail_->LevelZeroUpperBound(var);
      if (lp_values[var] - ToDouble(lb) < 1e-2) keep_term = true;
      if (ToDouble(ub) - lp_values[var] < 1e-2) keep_term = true;
    }

    // This is when we do not add slack.
    if (slack_infos == nullptr) {
      // We do not want to loose anything, so we only replace if the slack lp is
      // zero.
      if (info.slack_lp_value > 1e-6) keep_term = true;
    }

    if (keep_term) {
      tmp_terms_.push_back({var, coeff});
    } else {
      // Substitute.
      const IntegerValue lb = integer_trail_->LevelZeroLowerBound(var);
      const IntegerValue ub = integer_trail_->LevelZeroUpperBound(var);

      SlackInfo slack_info;
      slack_info.lp_value = info.slack_lp_value;
      slack_info.lb = 0;
      slack_info.ub = ub - lb;

      if (info.is_positive) {
        // X = Indicator * diff + lb + Slack
        tmp_terms_.push_back({info.bool_var, coeff * info.bound_diff});
        if (!AddProductTo(-coeff, lb, &new_ub)) {
          VLOG(2) << "Overflow";
          return;
        }
        if (slack_infos != nullptr) {
          tmp_terms_.push_back({first_slack, coeff});
          first_slack += 2;

          // slack = X - Indicator * info.bound_diff - lb;
          slack_info.terms.push_back({var, IntegerValue(1)});
          slack_info.terms.push_back({info.bool_var, -info.bound_diff});
          slack_info.offset = -lb;
          slack_infos->push_back(slack_info);
        }
      } else {
        // X = (1 - Indicator) * (diff) + lb + Slack
        // X = -Indicator * (diff) + lb + diff + Slack
        tmp_terms_.push_back({info.bool_var, -coeff * info.bound_diff});
        if (!AddProductTo(-coeff, lb + info.bound_diff, &new_ub)) {
          VLOG(2) << "Overflow";
          return;
        }
        if (slack_infos != nullptr) {
          tmp_terms_.push_back({first_slack, coeff});
          first_slack += 2;

          // slack = X + Indicator * info.bound_diff - lb - diff;
          slack_info.terms.push_back({var, IntegerValue(1)});
          slack_info.terms.push_back({info.bool_var, +info.bound_diff});
          slack_info.offset = -lb - info.bound_diff;
          slack_infos->push_back(slack_info);
        }
      }
      changed = true;
    }

    // Add all the new terms coefficient to the overflow detection to avoid
    // issue when merging terms referring to the same variable.
    for (int i = old_size; i < tmp_terms_.size(); ++i) {
      overflow_detection =
          CapAdd(overflow_detection, std::abs(tmp_terms_[i].second.value()));
    }
  }

  if (overflow_detection >= kMaxIntegerValue) {
    VLOG(2) << "Overflow";
    return;
  }
  if (!changed) return;

  // Update the cut.
  //
  // Note that because of our overflow_detection variable, there should be
  // no integer overflow when we merge identical terms.
  cut->lb = kMinIntegerValue;  // Not relevant.
  cut->ub = new_ub;
  CleanTermsAndFillConstraint(&tmp_terms_, cut);
}

std::string SingleNodeFlow::DebugString() const {
  return absl::StrCat("#in:", in_flow.size(), " #out:", out_flow.size(),
                      " demand:", demand.value(), " #bool:", num_bool,
                      " #lb:", num_to_lb, " #ub:", num_to_ub);
}

bool FlowCoverCutHelper::TryXminusLB(IntegerVariable var, double lp_value,
                                     IntegerValue lb, IntegerValue ub,
                                     IntegerValue coeff,
                                     ImpliedBoundsProcessor* ib_helper,
                                     SingleNodeFlow* result) const {
  const ImpliedBoundsProcessor::BestImpliedBoundInfo ib =
      ib_helper->GetCachedImpliedBoundInfo(NegationOf(var));
  if (ib.bool_var == kNoIntegerVariable) return false;
  if (ib.bound_diff != ub - lb) return false;

  // We have -var >= (ub - lb) bool - ub;
  // so (var - lb) <= -(ub - lb) * bool + ub - lb;
  // and (var - lb) <= bound_diff * (1 - bool).
  FlowInfo info;
  if (ib.is_positive) {
    info.bool_lp_value = 1 - ib.bool_lp_value;
    info.bool_expr.vars.push_back(ib.bool_var);
    info.bool_expr.coeffs.push_back(-1);
    info.bool_expr.offset = 1;
  } else {
    info.bool_lp_value = ib.bool_lp_value;
    info.bool_expr.vars.push_back(ib.bool_var);
    info.bool_expr.coeffs.push_back(1);
  }
  info.capacity = IntTypeAbs(coeff) * (ub - lb);
  info.flow_lp_value = ToDouble(IntTypeAbs(coeff)) * (lp_value - ToDouble(lb));
  info.flow_expr.vars.push_back(var);
  info.flow_expr.coeffs.push_back(IntTypeAbs(coeff));
  info.flow_expr.offset = -lb * IntTypeAbs(coeff);

  // We use (var - lb) so sign is preserved
  result->demand -= coeff * lb;
  if (coeff > 0) {
    result->in_flow.push_back(info);
  } else {
    result->out_flow.push_back(info);
  }
  return true;
}

bool FlowCoverCutHelper::TryUBminusX(IntegerVariable var, double lp_value,
                                     IntegerValue lb, IntegerValue ub,
                                     IntegerValue coeff,
                                     ImpliedBoundsProcessor* ib_helper,
                                     SingleNodeFlow* result) const {
  const ImpliedBoundsProcessor::BestImpliedBoundInfo ib =
      ib_helper->GetCachedImpliedBoundInfo(var);
  if (ib.bool_var == kNoIntegerVariable) return false;
  if (ib.bound_diff != ub - lb) return false;

  // We have var >= (ub - lb) bool + lb.
  // so ub - var <= ub - (ub - lb) * bool - lb.
  // and (ub - var) <= bound_diff * (1 - bool).
  FlowInfo info;
  if (ib.is_positive) {
    info.bool_lp_value = 1 - ib.bool_lp_value;
    info.bool_expr.vars.push_back(ib.bool_var);
    info.bool_expr.coeffs.push_back(-1);
    info.bool_expr.offset = 1;
  } else {
    info.bool_lp_value = ib.bool_lp_value;
    info.bool_expr.vars.push_back(ib.bool_var);
    info.bool_expr.coeffs.push_back(1);
  }
  info.capacity = IntTypeAbs(coeff) * (ub - lb);
  info.flow_lp_value = ToDouble(IntTypeAbs(coeff)) * (ToDouble(ub) - lp_value);
  info.flow_expr.vars.push_back(var);
  info.flow_expr.coeffs.push_back(-IntTypeAbs(coeff));
  info.flow_expr.offset = ub * IntTypeAbs(coeff);

  // We reverse the sign because we use (ub - var) here.
  // So coeff * var = -coeff * (ub - var) + coeff * ub;
  result->demand -= coeff * ub;
  if (coeff > 0) {
    result->out_flow.push_back(info);
  } else {
    result->in_flow.push_back(info);
  }
  return true;
}

SingleNodeFlow FlowCoverCutHelper::ComputeFlowCoverRelaxation(
    const LinearConstraint& base_ct,
    const absl::StrongVector<IntegerVariable, double>& lp_values,
    IntegerTrail* integer_trail, ImpliedBoundsProcessor* ib_helper) {
  SingleNodeFlow result;
  result.demand = base_ct.ub;

  // Stats.
  int num_bool = 0;
  int num_to_ub = 0;
  int num_to_lb = 0;

  const int size = base_ct.vars.size();
  for (int i = 0; i < size; ++i) {
    // We can either use (X - LB) or (UB - X) for a variable in [0, capacity].
    const IntegerVariable var = base_ct.vars[i];
    const IntegerValue coeff = base_ct.coeffs[i];

    // Hack: abort if coefficient in the base constraint are too large.
    if (IntTypeAbs(coeff) > 1'000'000) {
      result.clear();
      return result;
    }

    const IntegerValue lb = integer_trail->LevelZeroLowerBound(var);
    const IntegerValue ub = integer_trail->LevelZeroUpperBound(var);
    const IntegerValue capacity(
        CapProd(IntTypeAbs(coeff).value(), (ub - lb).value()));
    if (capacity >= kMaxIntegerValue) {
      // Abort.
      result.clear();
      return result;
    }
    if (lb == ub) {
      // Fixed variable shouldn't really appear here.
      result.demand -= coeff * lb;
      continue;
    }

    // We have a Boolean, this is an easy case.
    if (ub - lb == 1) {
      ++num_bool;
      FlowInfo info;
      info.bool_lp_value = (lp_values[var] - ToDouble(lb));
      info.capacity = capacity;
      info.bool_expr.vars.push_back(var);
      info.bool_expr.coeffs.push_back(1);
      info.bool_expr.offset = -lb;

      info.flow_lp_value = ToDouble(capacity) * info.bool_lp_value;
      info.flow_expr.vars.push_back(var);
      info.flow_expr.coeffs.push_back(info.capacity);
      info.flow_expr.offset = -lb * info.capacity;

      // coeff * x = coeff * (x - lb) + coeff * lb;
      result.demand -= coeff * lb;
      if (coeff > 0) {
        result.in_flow.push_back(info);
      } else {
        result.out_flow.push_back(info);
      }
      continue;
    }

    // TODO(user): Improve our logic to decide what implied bounds to use. We
    // rely on the best implied bounds, not necessarily one implying var at its
    // level zero bound like we need here.
    const double lp = lp_values[var];
    const bool prefer_lb = (lp - ToDouble(lb)) > (ToDouble(ub) - lp);
    if (prefer_lb) {
      if (TryXminusLB(var, lp, lb, ub, coeff, ib_helper, &result)) {
        ++num_to_lb;
        continue;
      }
      if (TryUBminusX(var, lp, lb, ub, coeff, ib_helper, &result)) {
        ++num_to_ub;
        continue;
      }
    } else {
      if (TryUBminusX(var, lp, lb, ub, coeff, ib_helper, &result)) {
        ++num_to_ub;
        continue;
      }
      if (TryXminusLB(var, lp, lb, ub, coeff, ib_helper, &result)) {
        ++num_to_lb;
        continue;
      }
    }

    // Abort.
    // TODO(user): Technically we can always use a arc usage Boolean fixed to 1.
    result.clear();
    return result;
  }

  result.num_bool = num_bool;
  result.num_to_ub = num_to_ub;
  result.num_to_lb = num_to_lb;
  return result;
}

// Reference: "Lifted flow cover inequalities for mixed 0-1 integer programs".
// Zonghao Gu, George L. Nemhauser, Martin W.P. Savelsbergh. 1999.
bool FlowCoverCutHelper::GenerateCut(const SingleNodeFlow& data) {
  if (data.empty()) return false;
  const double tolerance = 1e-2;

  // We are looking for two subsets CI (in-flow subset) and CO (out-flow subset)
  // so that sum_CI capa - sum_CO capa = demand + slack, slack > 0.
  //
  // Moreover we want to maximize sum_CI bool_lp_value + sum_CO bool_lp_value.
  std::vector<bool> in_cover(data.in_flow.size(), false);
  std::vector<bool> out_cover(data.out_flow.size(), false);

  // Start by selecting all the possible in_flow (except low bool value) and
  // all the out_flow with a bool value close to one.
  IntegerValue slack;
  {
    IntegerValue sum_in(0);
    IntegerValue sum_out(0);
    for (int i = 0; i < data.in_flow.size(); ++i) {
      const FlowInfo& info = data.in_flow[i];
      if (info.bool_lp_value > tolerance) {
        in_cover[i] = true;
        sum_in += info.capacity;
      }
    }
    for (int i = 0; i < data.out_flow.size(); ++i) {
      const FlowInfo& info = data.out_flow[i];
      if (info.bool_lp_value > 1 - tolerance) {
        out_cover[i] = true;
        sum_out += info.capacity;
      }
    }

    // This is the best slack we can hope for.
    slack = sum_in - sum_out - data.demand;
  }
  if (slack <= 0) return false;

  // Now greedily remove item from the in_cover and add_item to the out_cover
  // as long as we have remaining slack. We prefer item with a high score an
  // low slack variation.
  //
  // Note that this is just the classic greedy heuristic of a knapsack problem.
  if (slack > 1) {
    struct Item {
      bool correspond_to_in_flow;
      int index;
      double score;
    };
    std::vector<Item> actions;
    for (int i = 0; i < data.in_flow.size(); ++i) {
      if (!in_cover[i]) continue;
      const FlowInfo& info = data.in_flow[i];
      if (info.bool_lp_value > 1 - tolerance) continue;  // Do not remove these.
      actions.push_back(
          {true, i, (1 - info.bool_lp_value) / ToDouble(info.capacity)});
    }
    for (int i = 0; i < data.out_flow.size(); ++i) {
      if (out_cover[i]) continue;
      const FlowInfo& info = data.out_flow[i];
      if (info.bool_lp_value < tolerance) continue;  // Do not add these.
      actions.push_back(
          {false, i, info.bool_lp_value / ToDouble(info.capacity)});
    }

    // Sort by decreasing score.
    std::sort(actions.begin(), actions.end(),
              [](const Item& a, const Item& b) { return a.score > b.score; });

    // Greedily remove/add item as long as we have slack.
    for (const Item& item : actions) {
      if (item.correspond_to_in_flow) {
        const IntegerValue delta = data.in_flow[item.index].capacity;
        if (delta >= slack) continue;
        slack -= delta;
        in_cover[item.index] = false;
      } else {
        const IntegerValue delta = data.out_flow[item.index].capacity;
        if (delta >= slack) continue;
        slack -= delta;
        out_cover[item.index] = true;
      }
    }
  }

  // The non-lifted simple generalized flow cover inequality (SGFCI) cut will be
  // demand - sum_CI flow_i - sum_CI++ (capa_i - slack)(1 - bool_i)
  //        + sum_CO capa_i + sum_L- slack * bool_i + sum_L-- flow_i >=0
  //
  // Where CI++ are the arc with capa > slack in CI.
  // And L is O \ CO. L- arc with capa > slack and L-- the other.
  //
  // TODO(user): Also try to generate the extended generalized flow cover
  // inequality (EGFCI).
  CHECK_GT(slack, 0);

  // For display only.
  slack_ = slack;
  num_in_ignored_ = 0;
  num_in_flow_ = 0;
  num_in_bin_ = 0;
  num_out_capa_ = 0;
  num_out_flow_ = 0;
  num_out_bin_ = 0;

  cut_builder_.Clear();
  for (int i = 0; i < data.in_flow.size(); ++i) {
    const FlowInfo& info = data.in_flow[i];
    if (!in_cover[i]) {
      num_in_ignored_++;
      continue;
    }
    num_in_flow_++;
    cut_builder_.AddLinearExpression(info.flow_expr, -1);
    if (info.capacity > slack) {
      num_in_bin_++;
      const IntegerValue coeff = info.capacity - slack;
      cut_builder_.AddConstant(-coeff);
      cut_builder_.AddLinearExpression(info.bool_expr, coeff);
    }
  }
  for (int i = 0; i < data.out_flow.size(); ++i) {
    const FlowInfo& info = data.out_flow[i];
    if (out_cover[i]) {
      num_out_capa_++;
      cut_builder_.AddConstant(info.capacity);
    } else if (info.capacity > slack) {
      num_out_bin_++;
      cut_builder_.AddLinearExpression(info.bool_expr, slack);
    } else {
      num_out_flow_++;
      cut_builder_.AddLinearExpression(info.flow_expr);
    }
  }

  // TODO(user): Lift the cut.
  cut_ = cut_builder_.BuildConstraint(-data.demand, kMaxIntegerValue);
  return true;
}

bool ImpliedBoundsProcessor::DebugSlack(IntegerVariable first_slack,
                                        const LinearConstraint& initial_cut,
                                        const LinearConstraint& cut,
                                        const std::vector<SlackInfo>& info) {
  tmp_terms_.clear();
  IntegerValue new_ub = cut.ub;
  for (int i = 0; i < cut.vars.size(); ++i) {
    // Simple copy for non-slack variables.
    if (cut.vars[i] < first_slack) {
      tmp_terms_.push_back({cut.vars[i], cut.coeffs[i]});
      continue;
    }

    // Replace slack by its definition.
    const IntegerValue multiplier = cut.coeffs[i];
    const int index = (cut.vars[i].value() - first_slack.value()) / 2;
    for (const std::pair<IntegerVariable, IntegerValue>& term :
         info[index].terms) {
      tmp_terms_.push_back({term.first, term.second * multiplier});
    }
    new_ub -= multiplier * info[index].offset;
  }

  LinearConstraint tmp_cut;
  tmp_cut.lb = kMinIntegerValue;  // Not relevant.
  tmp_cut.ub = new_ub;
  CleanTermsAndFillConstraint(&tmp_terms_, &tmp_cut);
  MakeAllVariablesPositive(&tmp_cut);

  // We need to canonicalize the initial_cut too for comparison. Note that we
  // only use this for debug, so we don't care too much about the memory and
  // extra time.
  // TODO(user): Expose CanonicalizeConstraint() from the manager.
  LinearConstraint tmp_copy;
  tmp_terms_.clear();
  for (int i = 0; i < initial_cut.vars.size(); ++i) {
    tmp_terms_.push_back({initial_cut.vars[i], initial_cut.coeffs[i]});
  }
  tmp_copy.lb = kMinIntegerValue;  // Not relevant.
  tmp_copy.ub = new_ub;
  CleanTermsAndFillConstraint(&tmp_terms_, &tmp_copy);
  MakeAllVariablesPositive(&tmp_copy);

  if (tmp_cut == tmp_copy) return true;

  LOG(INFO) << first_slack;
  LOG(INFO) << tmp_copy.DebugString();
  LOG(INFO) << cut.DebugString();
  LOG(INFO) << tmp_cut.DebugString();
  return false;
}

namespace {

int64_t SumOfKMinValues(const absl::btree_set<int64_t>& values, int k) {
  int count = 0;
  int64_t sum = 0;
  for (const int64_t value : values) {
    sum += value;
    if (++count >= k) return sum;
  }
  return sum;
}

void TryToGenerateAllDiffCut(
    const std::vector<std::pair<double, AffineExpression>>& sorted_exprs_lp,
    const IntegerTrail& integer_trail,
    const absl::StrongVector<IntegerVariable, double>& lp_values,
    LinearConstraintManager* manager, Model* model) {
  std::vector<AffineExpression> current_set_exprs;
  const int num_exprs = sorted_exprs_lp.size();
  absl::btree_set<int64_t> min_values;
  absl::btree_set<int64_t> negated_max_values;
  double sum = 0.0;
  for (auto value_expr : sorted_exprs_lp) {
    sum += value_expr.first;
    const AffineExpression expr = value_expr.second;
    if (integer_trail.IsFixed(expr)) {
      const int64_t value = integer_trail.FixedValue(expr).value();
      min_values.insert(value);
      negated_max_values.insert(-value);
    } else {
      int count = 0;
      const int64_t coeff = expr.coeff.value();
      const int64_t constant = expr.constant.value();
      for (const int64_t value :
           integer_trail.InitialVariableDomain(expr.var).Values()) {
        if (coeff > 0) {
          min_values.insert(value * coeff + constant);
        } else {
          negated_max_values.insert(-(value * coeff + constant));
        }
        if (++count >= num_exprs) break;
      }

      count = 0;
      for (const int64_t value :
           integer_trail.InitialVariableDomain(expr.var).Negation().Values()) {
        if (coeff > 0) {
          negated_max_values.insert(value * coeff - constant);
        } else {
          min_values.insert(-value * coeff + constant);
        }
        if (++count >= num_exprs) break;
      }
    }
    current_set_exprs.push_back(expr);
    const int64_t required_min_sum =
        SumOfKMinValues(min_values, current_set_exprs.size());
    const int64_t required_max_sum =
        -SumOfKMinValues(negated_max_values, current_set_exprs.size());
    if (sum < required_min_sum || sum > required_max_sum) {
      LinearConstraintBuilder cut(model, IntegerValue(required_min_sum),
                                  IntegerValue(required_max_sum));
      for (AffineExpression expr : current_set_exprs) {
        cut.AddTerm(expr, IntegerValue(1));
      }
      manager->AddCut(cut.Build(), "all_diff", lp_values);
      // NOTE: We can extend the current set but it is more helpful to generate
      // the cut on a different set of variables so we reset the counters.
      sum = 0.0;
      current_set_exprs.clear();
      min_values.clear();
      negated_max_values.clear();
    }
  }
}

}  // namespace

CutGenerator CreateAllDifferentCutGenerator(
    const std::vector<AffineExpression>& exprs, Model* model) {
  CutGenerator result;
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();

  for (const AffineExpression& expr : exprs) {
    if (!integer_trail->IsFixed(expr)) {
      result.vars.push_back(expr.var);
    }
  }
  gtl::STLSortAndRemoveDuplicates(&result.vars);

  Trail* trail = model->GetOrCreate<Trail>();
  result.generate_cuts =
      [exprs, integer_trail, trail, model](
          const absl::StrongVector<IntegerVariable, double>& lp_values,
          LinearConstraintManager* manager) {
        // These cuts work at all levels but the generator adds too many cuts on
        // some instances and degrade the performance so we only use it at level
        // 0.
        if (trail->CurrentDecisionLevel() > 0) return true;
        std::vector<std::pair<double, AffineExpression>> sorted_exprs;
        for (const AffineExpression expr : exprs) {
          if (integer_trail->LevelZeroLowerBound(expr) ==
              integer_trail->LevelZeroUpperBound(expr)) {
            continue;
          }
          sorted_exprs.push_back(std::make_pair(expr.LpValue(lp_values), expr));
        }
        std::sort(sorted_exprs.begin(), sorted_exprs.end(),
                  [](std::pair<double, AffineExpression>& a,
                     const std::pair<double, AffineExpression>& b) {
                    return a.first < b.first;
                  });
        TryToGenerateAllDiffCut(sorted_exprs, *integer_trail, lp_values,
                                manager, model);
        // Other direction.
        std::reverse(sorted_exprs.begin(), sorted_exprs.end());
        TryToGenerateAllDiffCut(sorted_exprs, *integer_trail, lp_values,
                                manager, model);
        return true;
      };
  VLOG(1) << "Created all_diff cut generator of size: " << exprs.size();
  return result;
}

namespace {
// Returns max((w2i - w1i)*Li, (w2i - w1i)*Ui).
IntegerValue MaxCornerDifference(const IntegerVariable var,
                                 const IntegerValue w1_i,
                                 const IntegerValue w2_i,
                                 const IntegerTrail& integer_trail) {
  const IntegerValue lb = integer_trail.LevelZeroLowerBound(var);
  const IntegerValue ub = integer_trail.LevelZeroUpperBound(var);
  return std::max((w2_i - w1_i) * lb, (w2_i - w1_i) * ub);
}

// This is the coefficient of zk in the cut, where k = max_index.
// MPlusCoefficient_ki = max((wki - wI(i)i) * Li,
//                           (wki - wI(i)i) * Ui)
//                     = max corner difference for variable i,
//                       target expr I(i), max expr k.
// The coefficient of zk is Sum(i=1..n)(MPlusCoefficient_ki) + bk
IntegerValue MPlusCoefficient(
    const std::vector<IntegerVariable>& x_vars,
    const std::vector<LinearExpression>& exprs,
    const absl::StrongVector<IntegerVariable, int>& variable_partition,
    const int max_index, const IntegerTrail& integer_trail) {
  IntegerValue coeff = exprs[max_index].offset;
  // TODO(user): This algo is quadratic since GetCoefficientOfPositiveVar()
  // is linear. This can be optimized (better complexity) if needed.
  for (const IntegerVariable var : x_vars) {
    const int target_index = variable_partition[var];
    if (max_index != target_index) {
      coeff += MaxCornerDifference(
          var, GetCoefficientOfPositiveVar(var, exprs[target_index]),
          GetCoefficientOfPositiveVar(var, exprs[max_index]), integer_trail);
    }
  }
  return coeff;
}

// Compute the value of
// rhs = wI(i)i * xi + Sum(k=1..d)(MPlusCoefficient_ki * zk)
// for variable xi for given target index I(i).
double ComputeContribution(
    const IntegerVariable xi_var, const std::vector<IntegerVariable>& z_vars,
    const std::vector<LinearExpression>& exprs,
    const absl::StrongVector<IntegerVariable, double>& lp_values,
    const IntegerTrail& integer_trail, const int target_index) {
  CHECK_GE(target_index, 0);
  CHECK_LT(target_index, exprs.size());
  const LinearExpression& target_expr = exprs[target_index];
  const double xi_value = lp_values[xi_var];
  const IntegerValue wt_i = GetCoefficientOfPositiveVar(xi_var, target_expr);
  double contrib = ToDouble(wt_i) * xi_value;
  for (int expr_index = 0; expr_index < exprs.size(); ++expr_index) {
    if (expr_index == target_index) continue;
    const LinearExpression& max_expr = exprs[expr_index];
    const double z_max_value = lp_values[z_vars[expr_index]];
    const IntegerValue corner_value = MaxCornerDifference(
        xi_var, wt_i, GetCoefficientOfPositiveVar(xi_var, max_expr),
        integer_trail);
    contrib += ToDouble(corner_value) * z_max_value;
  }
  return contrib;
}
}  // namespace

CutGenerator CreateLinMaxCutGenerator(
    const IntegerVariable target, const std::vector<LinearExpression>& exprs,
    const std::vector<IntegerVariable>& z_vars, Model* model) {
  CutGenerator result;
  std::vector<IntegerVariable> x_vars;
  result.vars = {target};
  const int num_exprs = exprs.size();
  for (int i = 0; i < num_exprs; ++i) {
    result.vars.push_back(z_vars[i]);
    x_vars.insert(x_vars.end(), exprs[i].vars.begin(), exprs[i].vars.end());
  }
  gtl::STLSortAndRemoveDuplicates(&x_vars);
  // All expressions should only contain positive variables.
  DCHECK(std::all_of(x_vars.begin(), x_vars.end(), [](IntegerVariable var) {
    return VariableIsPositive(var);
  }));
  result.vars.insert(result.vars.end(), x_vars.begin(), x_vars.end());

  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  result.generate_cuts =
      [x_vars, z_vars, target, num_exprs, exprs, integer_trail, model](
          const absl::StrongVector<IntegerVariable, double>& lp_values,
          LinearConstraintManager* manager) {
        absl::StrongVector<IntegerVariable, int> variable_partition(
            lp_values.size(), -1);
        absl::StrongVector<IntegerVariable, double> variable_partition_contrib(
            lp_values.size(), std::numeric_limits<double>::infinity());
        for (int expr_index = 0; expr_index < num_exprs; ++expr_index) {
          for (const IntegerVariable var : x_vars) {
            const double contribution = ComputeContribution(
                var, z_vars, exprs, lp_values, *integer_trail, expr_index);
            const double prev_contribution = variable_partition_contrib[var];
            if (contribution < prev_contribution) {
              variable_partition[var] = expr_index;
              variable_partition_contrib[var] = contribution;
            }
          }
        }

        LinearConstraintBuilder cut(model, /*lb=*/IntegerValue(0),
                                    /*ub=*/kMaxIntegerValue);
        double violation = lp_values[target];
        cut.AddTerm(target, IntegerValue(-1));

        for (const IntegerVariable xi_var : x_vars) {
          const int input_index = variable_partition[xi_var];
          const LinearExpression& expr = exprs[input_index];
          const IntegerValue coeff = GetCoefficientOfPositiveVar(xi_var, expr);
          if (coeff != IntegerValue(0)) {
            cut.AddTerm(xi_var, coeff);
          }
          violation -= ToDouble(coeff) * lp_values[xi_var];
        }
        for (int expr_index = 0; expr_index < num_exprs; ++expr_index) {
          const IntegerVariable z_var = z_vars[expr_index];
          const IntegerValue z_coeff = MPlusCoefficient(
              x_vars, exprs, variable_partition, expr_index, *integer_trail);
          if (z_coeff != IntegerValue(0)) {
            cut.AddTerm(z_var, z_coeff);
          }
          violation -= ToDouble(z_coeff) * lp_values[z_var];
        }
        if (violation > 1e-2) {
          manager->AddCut(cut.Build(), "LinMax", lp_values);
        }
        return true;
      };
  return result;
}

namespace {

IntegerValue EvaluateMaxAffine(
    const std::vector<std::pair<IntegerValue, IntegerValue>>& affines,
    IntegerValue x) {
  IntegerValue y = kMinIntegerValue;
  for (const auto& p : affines) {
    y = std::max(y, x * p.first + p.second);
  }
  return y;
}

}  // namespace

bool BuildMaxAffineUpConstraint(
    const LinearExpression& target, IntegerVariable var,
    const std::vector<std::pair<IntegerValue, IntegerValue>>& affines,
    Model* model, LinearConstraintBuilder* builder) {
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  const IntegerValue x_min = integer_trail->LevelZeroLowerBound(var);
  const IntegerValue x_max = integer_trail->LevelZeroUpperBound(var);

  const IntegerValue y_at_min = EvaluateMaxAffine(affines, x_min);
  const IntegerValue y_at_max = EvaluateMaxAffine(affines, x_max);

  const IntegerValue delta_x = x_max - x_min;
  const IntegerValue delta_y = y_at_max - y_at_min;

  // target <= y_at_min + (delta_y / delta_x) * (var - x_min)
  // delta_x * target <= delta_x * y_at_min + delta_y * (var - x_min)
  // -delta_y * var + delta_x * target <= delta_x * y_at_min - delta_y * x_min
  //
  // Checks the rhs for overflows.
  if (AtMinOrMaxInt64(CapProd(delta_x.value(), y_at_min.value())) ||
      AtMinOrMaxInt64(CapProd(delta_y.value(), x_min.value()))) {
    return false;
  }

  builder->ResetBounds(kMinIntegerValue, delta_x * y_at_min - delta_y * x_min);
  builder->AddLinearExpression(target, delta_x);
  builder->AddTerm(var, -delta_y);

  // Prevent to create constraints that can overflow.
  if (!ValidateLinearConstraintForOverflow(builder->Build(), *integer_trail)) {
    VLOG(2) << "Linear constraint can cause overflow: " << builder->Build();

    return false;
  }

  return true;
}

CutGenerator CreateMaxAffineCutGenerator(
    LinearExpression target, IntegerVariable var,
    std::vector<std::pair<IntegerValue, IntegerValue>> affines,
    const std::string cut_name, Model* model) {
  CutGenerator result;
  result.vars = target.vars;
  result.vars.push_back(var);
  gtl::STLSortAndRemoveDuplicates(&result.vars);

  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  result.generate_cuts =
      [target, var, affines, cut_name, integer_trail, model](
          const absl::StrongVector<IntegerVariable, double>& lp_values,
          LinearConstraintManager* manager) {
        if (integer_trail->IsFixed(var)) return true;
        LinearConstraintBuilder builder(model);
        if (BuildMaxAffineUpConstraint(target, var, affines, model, &builder)) {
          manager->AddCut(builder.Build(), cut_name, lp_values);
        }
        return true;
      };
  return result;
}

CutGenerator CreateCliqueCutGenerator(
    const std::vector<IntegerVariable>& base_variables, Model* model) {
  // Filter base_variables to only keep the one with a literal view, and
  // do the conversion.
  std::vector<IntegerVariable> variables;
  std::vector<Literal> literals;
  absl::flat_hash_map<LiteralIndex, IntegerVariable> positive_map;
  absl::flat_hash_map<LiteralIndex, IntegerVariable> negative_map;
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  for (const IntegerVariable var : base_variables) {
    if (integer_trail->LowerBound(var) != IntegerValue(0)) continue;
    if (integer_trail->UpperBound(var) != IntegerValue(1)) continue;
    const LiteralIndex literal_index = encoder->GetAssociatedLiteral(
        IntegerLiteral::GreaterOrEqual(var, IntegerValue(1)));
    if (literal_index != kNoLiteralIndex) {
      variables.push_back(var);
      literals.push_back(Literal(literal_index));
      positive_map[literal_index] = var;
      negative_map[Literal(literal_index).NegatedIndex()] = var;
    }
  }
  CutGenerator result;
  result.vars = variables;
  auto* implication_graph = model->GetOrCreate<BinaryImplicationGraph>();
  result.generate_cuts =
      [variables, literals, implication_graph, positive_map, negative_map,
       model](const absl::StrongVector<IntegerVariable, double>& lp_values,
              LinearConstraintManager* manager) {
        std::vector<double> packed_values;
        for (int i = 0; i < literals.size(); ++i) {
          packed_values.push_back(lp_values[variables[i]]);
        }
        const std::vector<std::vector<Literal>> at_most_ones =
            implication_graph->GenerateAtMostOnesWithLargeWeight(literals,
                                                                 packed_values);

        for (const std::vector<Literal>& at_most_one : at_most_ones) {
          // We need to express such "at most one" in term of the initial
          // variables, so we do not use the
          // LinearConstraintBuilder::AddLiteralTerm() here.
          LinearConstraintBuilder builder(
              model, IntegerValue(std::numeric_limits<int64_t>::min()),
              IntegerValue(1));
          for (const Literal l : at_most_one) {
            if (positive_map.contains(l.Index())) {
              builder.AddTerm(positive_map.at(l.Index()), IntegerValue(1));
            } else {
              // Add 1 - X to the linear constraint.
              builder.AddTerm(negative_map.at(l.Index()), IntegerValue(-1));
              builder.AddConstant(IntegerValue(1));
            }
          }

          manager->AddCut(builder.Build(), "clique", lp_values);
        }
        return true;
      };
  return result;
}

}  // namespace sat
}  // namespace operations_research

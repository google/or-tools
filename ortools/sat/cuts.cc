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

std::string CutTerm::DebugString() const {
  return absl::StrCat("coeff=", coeff.value(), " lp=", lp_value,
                      " range=", bound_diff.value(), " expr=[",
                      expr_coeffs[0].value(), ",", expr_coeffs[1].value(), "]",
                      " * [V", expr_vars[0].value(), ",V", expr_vars[1].value(),
                      "]", " + ", expr_offset.value());
}

std::string CutData::DebugString() const {
  std::string result = absl::StrCat("CutData rhs=", rhs.value(), "\n");
  for (const CutTerm& term : terms) {
    absl::StrAppend(&result, term.DebugString(), "\n");
  }
  return result;
}

bool CutTerm::Complement(IntegerValue* rhs) {
  // We replace coeff * X by coeff * (X - bound_diff + bound_diff)
  // which gives -coeff * complement(X) + coeff * bound_diff;
  if (!AddProductTo(-coeff, bound_diff, rhs)) return false;

  // We keep the same expression variable.
  for (int i = 0; i < 2; ++i) {
    expr_coeffs[i] = -expr_coeffs[i];
  }
  expr_offset = bound_diff - expr_offset;

  // Note that this is not involutive because of floating point error. Fix?
  lp_value = ToDouble(bound_diff) - lp_value;
  coeff = -coeff;
  return true;
}

// To try to minimize the risk of overflow, we switch to the bound closer
// to the lp_value. Since most of our base constraint for cut are tight,
// hopefully this is not too bad.
bool CutData::AppendOneTerm(IntegerVariable var, IntegerValue coeff,
                            double lp_value, IntegerValue lb, IntegerValue ub) {
  if (coeff == 0) return true;
  const IntegerValue bound_diff = ub - lb;

  // Complement the variable so that it is always closer to its lb.
  bool complement = false;
  const double lb_dist = std::abs(lp_value - ToDouble(lb));
  const double ub_dist = std::abs(lp_value - ToDouble(ub));
  if (ub_dist < lb_dist) {
    complement = true;
  }

  // See formula below, the constant term is either coeff * lb or coeff * ub.
  if (!AddProductTo(-coeff, complement ? ub : lb, &rhs)) {
    return false;
  }

  // Deal with fixed variable, no need to shift back in this case, we can
  // just remove the term.
  if (bound_diff == 0) return true;

  CutTerm entry;
  entry.expr_vars[0] = var;
  entry.expr_coeffs[1] = 0;
  entry.bound_diff = bound_diff;
  if (complement) {
    // X = -(UB - X) + UB
    entry.expr_coeffs[0] = -IntegerValue(1);
    entry.expr_offset = ub;
    entry.coeff = -coeff;
    entry.lp_value = ToDouble(ub) - lp_value;
  } else {
    // C = (X - LB) + LB
    entry.expr_coeffs[0] = IntegerValue(1);
    entry.expr_offset = -lb;
    entry.coeff = coeff;
    entry.lp_value = lp_value - ToDouble(lb);
  }
  terms.push_back(entry);
  return true;
}

bool CutData::FillFromLinearConstraint(
    const LinearConstraint& base_ct,
    const absl::StrongVector<IntegerVariable, double>& lp_values,
    IntegerTrail* integer_trail) {
  rhs = base_ct.ub;
  terms.clear();
  const int num_terms = base_ct.vars.size();
  for (int i = 0; i < num_terms; ++i) {
    const IntegerVariable var = base_ct.vars[i];
    if (!AppendOneTerm(var, base_ct.coeffs[i], lp_values[base_ct.vars[i]],
                       integer_trail->LevelZeroLowerBound(var),
                       integer_trail->LevelZeroUpperBound(var))) {
      return false;
    }
  }
  return true;
}

bool CutData::FillFromParallelVectors(
    const LinearConstraint& base_ct, const std::vector<double>& lp_values,
    const std::vector<IntegerValue>& lower_bounds,
    const std::vector<IntegerValue>& upper_bounds) {
  rhs = base_ct.ub;
  terms.clear();

  const int size = lp_values.size();
  if (size == 0) return true;

  CHECK_EQ(lower_bounds.size(), size);
  CHECK_EQ(upper_bounds.size(), size);
  CHECK_EQ(base_ct.vars.size(), size);
  CHECK_EQ(base_ct.coeffs.size(), size);
  CHECK_EQ(base_ct.lb, kMinIntegerValue);

  for (int i = 0; i < size; ++i) {
    if (!AppendOneTerm(base_ct.vars[i], base_ct.coeffs[i], lp_values[i],
                       lower_bounds[i], upper_bounds[i])) {
      return false;
    }
  }

  return true;
}

void CutData::Canonicalize() {
  num_relevant_entries = 0;
  max_magnitude = IntTypeAbs(rhs);
  for (int i = 0; i < terms.size(); ++i) {
    CutTerm& entry = terms[i];
    max_magnitude = std::max(max_magnitude, IntTypeAbs(entry.coeff));
    if (entry.HasRelevantLpValue()) {
      std::swap(terms[num_relevant_entries], entry);
      ++num_relevant_entries;
    }
  }

  // Sort by larger lp_value first.
  std::sort(terms.begin(), terms.begin() + num_relevant_entries,
            [](const CutTerm& a, const CutTerm& b) {
              return a.lp_value > b.lp_value;
            });
}

void CutDataBuilder::ClearIndices() {
  num_merges_ = 0;
  constraint_is_indexed_ = false;
  direct_index_.clear();
  complemented_index_.clear();
}

void CutDataBuilder::RegisterAllBooleansTerms(const CutData& cut) {
  constraint_is_indexed_ = true;
  const int size = cut.terms.size();
  for (int i = 0; i < size; ++i) {
    const CutTerm& term = cut.terms[i];
    if (term.bound_diff != 1) continue;
    if (!term.IsSimple()) continue;
    if (term.expr_coeffs[0] > 0) {
      direct_index_[term.expr_vars[0]] = i;
    } else {
      complemented_index_[term.expr_vars[0]] = i;
    }
  }
}

void CutDataBuilder::AddOrMergeTerm(const CutTerm& term, IntegerValue t,
                                    CutData* cut) {
  if (!constraint_is_indexed_) {
    RegisterAllBooleansTerms(*cut);
  }

  DCHECK(term.IsSimple());
  const IntegerVariable var = term.expr_vars[0];
  const int new_index = cut->terms.size();
  const auto [it, inserted] =
      term.expr_coeffs[0] > 0 ? direct_index_.insert({var, new_index})
                              : complemented_index_.insert({var, new_index});
  const int entry_index = it->second;
  if (inserted) {
    cut->terms.push_back(term);
  } else {
    // We can only merge the term if term.coeff + old_coeff do not overflow and
    // if t * new_coeff do not overflow.
    //
    // If we cannot merge the term, we will keep them separate. The produced cut
    // will be less strong, but can still be used.
    const int64_t new_coeff =
        CapAdd(cut->terms[entry_index].coeff.value(), term.coeff.value());
    const int64_t overflow_check = CapProd(t.value(), new_coeff);
    if (AtMinOrMaxInt64(new_coeff) || AtMinOrMaxInt64(overflow_check)) {
      // If we cannot merge the term, we keep them separate.
      cut->terms.push_back(term);
    } else {
      ++num_merges_;
      cut->terms[entry_index].coeff = IntegerValue(new_coeff);
    }
  }
}

bool CutDataBuilder::ConvertToLinearConstraint(const CutData& cut,
                                               LinearConstraint* output) {
  tmp_map_.clear();
  IntegerValue new_rhs = cut.rhs;
  for (const CutTerm& term : cut.terms) {
    for (int i = 0; i < 2; ++i) {
      if (term.expr_coeffs[i] == 0) continue;
      if (!AddProductTo(term.coeff, term.expr_coeffs[i],
                        &tmp_map_[term.expr_vars[i]])) {
        return false;
      }
    }
    if (!AddProductTo(-term.coeff, term.expr_offset, &new_rhs)) {
      return false;
    }
  }

  output->ClearTerms();
  output->lb = kMinIntegerValue;
  output->ub = new_rhs;
  for (const auto [var, coeff] : tmp_map_) {
    if (coeff == 0) continue;
    output->vars.push_back(var);
    output->coeffs.push_back(coeff);
  }
  DivideByGCD(output);
  return true;
}

namespace {

// Minimum amount of violation of the cut constraint by the solution. This
// is needed to avoid numerical issues and adding cuts with minor effect.
const double kMinCutViolation = 1e-4;

IntegerValue CapProdI(IntegerValue a, IntegerValue b) {
  return IntegerValue(CapProd(a.value(), b.value()));
}

IntegerValue CapSubI(IntegerValue a, IntegerValue b) {
  return IntegerValue(CapSub(a.value(), b.value()));
}

IntegerValue CapAddI(IntegerValue a, IntegerValue b) {
  return IntegerValue(CapAdd(a.value(), b.value()));
}

bool ProdOverflow(IntegerValue t, IntegerValue value) {
  return AtMinOrMaxInt64(CapProd(t.value(), value.value()));
}

}  // namespace

// Compute the larger t <= max_t such that t * rhs_remainder >= divisor / 2.
//
// This is just a separate function as it is slightly faster to compute the
// result only once.
IntegerValue GetFactorT(IntegerValue rhs_remainder, IntegerValue divisor,
                        IntegerValue max_magnitude) {
  // Make sure that when we multiply the rhs or the coefficient by a factor t,
  // we do not have an integer overflow. Note that the rhs should be counted
  // in max_magnitude since we will apply f() on it.
  IntegerValue max_t(std::numeric_limits<int64_t>::max());
  if (max_magnitude != 0) {
    max_t = max_t / max_magnitude;
  }
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

IntegerRoundingCutHelper::~IntegerRoundingCutHelper() {
  if (!VLOG_IS_ON(1)) return;
  if (shared_stats_ == nullptr) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back({"rounding_cut/num_initial_ibs_", total_num_initial_ibs_});
  stats.push_back(
      {"rounding_cut/num_initial_merges_", total_num_initial_merges_});
  stats.push_back({"rounding_cut/num_pos_lifts", total_num_pos_lifts_});
  stats.push_back({"rounding_cut/num_neg_lifts", total_num_neg_lifts_});
  stats.push_back(
      {"rounding_cut/num_post_complements", total_num_post_complements_});
  stats.push_back({"rounding_cut/num_overflows", total_num_overflow_abort_});
  stats.push_back({"rounding_cut/num_adjusts", total_num_coeff_adjust_});
  stats.push_back({"rounding_cut/num_merges", total_num_merges_});
  stats.push_back({"rounding_cut/num_bumps", total_num_bumps_});
  stats.push_back(
      {"rounding_cut/num_final_complements", total_num_final_complements_});
  stats.push_back({"rounding_cut/num_dominating_f", total_num_dominating_f_});
  shared_stats_->AddStats(stats);
}

double IntegerRoundingCutHelper::GetScaledViolation(
    IntegerValue divisor, IntegerValue max_scaling,
    IntegerValue remainder_threshold, const CutData& cut) {
  IntegerValue rhs = cut.rhs;
  IntegerValue max_magnitude = cut.max_magnitude;
  const IntegerValue initial_rhs_remainder = PositiveRemainder(rhs, divisor);
  if (initial_rhs_remainder < remainder_threshold) return 0.0;

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
  // TODO(user): We could see if for a fixed function f, the increase is
  // interesting?
  // before: f(rhs) - f(coeff) * lp_value
  // after: f(rhs + increase * bound_diff) - f(coeff + increase) * lp_value.
  adjusted_coeffs_.clear();
  const IntegerValue adjust_threshold =
      (divisor - initial_rhs_remainder - 1) /
      IntegerValue(std::max(1000, cut.num_relevant_entries));
  if (adjust_threshold > 0) {
    // Even before we finish the adjust, we can have a lower bound on the
    // activily loss using this divisor, and so we can abort early. This is
    // similar to what is done below.
    double max_violation = ToDouble(initial_rhs_remainder);
    for (int i = 0; i < cut.num_relevant_entries; ++i) {
      const CutTerm& entry = cut.terms[i];
      const IntegerValue remainder = PositiveRemainder(entry.coeff, divisor);
      if (remainder == 0) continue;
      if (remainder <= initial_rhs_remainder) {
        // We do not know exactly f() yet, but it will always round to the
        // floor of the division by divisor in this case.
        max_violation -= ToDouble(remainder) * entry.lp_value;
        if (max_violation <= 1e-3) return 0.0;
        continue;
      }

      // Adjust coeff of the form k * divisor - epsilon.
      const IntegerValue adjust = divisor - remainder;
      const IntegerValue prod = CapProdI(adjust, entry.bound_diff);
      if (prod <= adjust_threshold) {
        rhs += prod;
        const IntegerValue new_coeff = entry.coeff + adjust;
        adjusted_coeffs_.push_back({i, new_coeff});
        max_magnitude = std::max(max_magnitude, IntTypeAbs(new_coeff));
      }
    }
  }

  max_magnitude = std::max(max_magnitude, IntTypeAbs(rhs));
  const IntegerValue rhs_remainder = PositiveRemainder(rhs, divisor);
  const IntegerValue t = GetFactorT(rhs_remainder, divisor, max_magnitude);
  const auto f =
      GetSuperAdditiveRoundingFunction(rhs_remainder, divisor, t, max_scaling);

  // As we round coefficients, we will compute the loss compared to the
  // current scaled constraint activity. As soon as this loss crosses the
  // slack, then we known that there is no violation and we can abort early.
  //
  // TODO(user): modulo the scaling, we could compute the exact threshold
  // using our current best cut. Note that we also have to account the change
  // in slack due to the adjust code above.
  const double scaling = ToDouble(f(divisor)) / ToDouble(divisor);
  double max_violation = scaling * ToDouble(rhs_remainder);

  // Apply f() to the cut and compute the cut violation. Note that it is
  // okay to just look at the relevant indices since the other have a lp
  // value which is almost zero. Doing it like this is faster, and even if
  // the max_magnitude might be off it should still be relevant enough.
  double violation = -ToDouble(f(rhs));
  double l2_norm = 0.0;
  int adjusted_coeffs_index = 0;
  for (int i = 0; i < cut.num_relevant_entries; ++i) {
    const CutTerm& entry = cut.terms[i];

    // Adjust coeff according to our previous computation if needed.
    IntegerValue coeff = entry.coeff;
    if (adjusted_coeffs_index < adjusted_coeffs_.size() &&
        adjusted_coeffs_[adjusted_coeffs_index].first == i) {
      coeff = adjusted_coeffs_[adjusted_coeffs_index].second;
      adjusted_coeffs_index++;
    }

    if (coeff == 0) continue;
    const IntegerValue new_coeff = f(coeff);
    const double new_coeff_double = ToDouble(new_coeff);
    const double lp_value = entry.lp_value;

    // TODO(user): Shall we compute the norm after slack are substituted back?
    // it might be widely different. Another reason why this might not be
    // the best measure.
    l2_norm += new_coeff_double * new_coeff_double;
    violation += new_coeff_double * lp_value;
    max_violation -= (scaling * ToDouble(coeff) - new_coeff_double) * lp_value;
    if (max_violation <= 1e-3) return 0.0;
  }
  if (l2_norm == 0.0) return 0.0;

  // Here we scale by the L2 norm over the "relevant" positions. This seems
  // to work slighly better in practice.
  //
  // Note(user): The non-relevant position have an LP value of zero. If their
  // coefficient is positive, it seems good not to take it into account in the
  // norm since the larger this coeff is, the stronger the cut. If the coeff
  // is negative though, a large coeff means a small increase from zero of the
  // lp value will make the cut satisfied, so we might want to look at them.
  return violation / sqrt(l2_norm);
}

bool IntegerRoundingCutHelper::HasComplementedImpliedBound(
    const CutTerm& entry, ImpliedBoundsProcessor* ib_processor) {
  if (ib_processor == nullptr) return false;
  if (!entry.IsSimple()) return false;
  if (entry.bound_diff == 1) return false;
  const ImpliedBoundsProcessor::BestImpliedBoundInfo info =
      ib_processor->GetCachedImpliedBoundInfo(
          entry.expr_coeffs[0] > 0 ? NegationOf(entry.expr_vars[0])
                                   : entry.expr_vars[0]);
  return info.bool_var != kNoIntegerVariable;
}

// TODO(user): This is slow, 50% of run time on a2c1s1.pb.gz. Optimize!
bool IntegerRoundingCutHelper::ComputeCut(
    RoundingOptions options, const CutData& base_ct,
    ImpliedBoundsProcessor* ib_processor) {
  // Try IB before heuristic?
  // This should be better except it can mess up the norm and the divisors.
  best_cut_ = base_ct;
  if (options.use_ib_before_heuristic && ib_processor != nullptr) {
    cut_builder_.ClearIndices();
    const int old_size = static_cast<int>(best_cut_.terms.size());
    bool abort = true;
    for (int i = 0; i < old_size; ++i) {
      if (best_cut_.terms[i].bound_diff <= 1) continue;
      if (!best_cut_.terms[i].HasRelevantLpValue()) continue;

      if (options.prefer_positive_ib && best_cut_.terms[i].coeff < 0) {
        // We complement the term before trying the implied bound.
        if (best_cut_.terms[i].Complement(&best_cut_.rhs)) {
          if (ib_processor->TryToExpandWithLowerImpliedbound(
                  IntegerValue(1), i,
                  /*complement=*/true, &best_cut_, &cut_builder_)) {
            ++total_num_initial_ibs_;
            abort = false;
            continue;
          }
          best_cut_.terms[i].Complement(&best_cut_.rhs);
        }
      }

      if (ib_processor->TryToExpandWithLowerImpliedbound(
              IntegerValue(1), i,
              /*complement=*/true, &best_cut_, &cut_builder_)) {
        abort = false;
        ++total_num_initial_ibs_;
      }
    }
    total_num_initial_merges_ += cut_builder_.NumMergesSinceLastClear();

    // TODO(user): We assume that this is called with and without the option
    // use_ib_before_heuristic, so that we can abort if no IB has been applied
    // since then we will redo the computation. This is not really clean.
    if (abort) return false;
  }

  // Our heuristic will try to generate a few different cuts, and we will keep
  // the most violated one scaled by the l2 norm of the relevant position.
  //
  // TODO(user): Experiment for the best value of this initial violation
  // threshold. Note also that we use the l2 norm on the restricted position
  // here. Maybe we should change that? On that note, the L2 norm usage seems
  // a bit weird to me since it grows with the number of term in the cut. And
  // often, we already have a good cut, and we make it stronger by adding
  // extra terms that do not change its activity.
  //
  // The discussion above only concern the best_scaled_violation initial
  // value. The remainder_threshold allows to not consider cuts for which the
  // final efficacity is clearly lower than 1e-3 (it is a bound, so we could
  // generate cuts with a lower efficacity than this).
  //
  // TODO(user): If the rhs is small and close to zero, we might want to
  // consider different way of complementing the variables.
  best_cut_.Canonicalize();
  const IntegerValue remainder_threshold(
      std::max(IntegerValue(1), best_cut_.max_magnitude / 1000));
  if (best_cut_.rhs >= 0 && best_cut_.rhs < remainder_threshold) {
    return false;
  }

  // There is no point trying twice the same divisor or a divisor that is too
  // small. Note that we use a higher threshold than the remainder_threshold
  // because we can boost the remainder thanks to our adjusting heuristic
  // below and also because this allows to have cuts with a small range of
  // coefficients.
  divisors_.clear();
  for (const CutTerm& entry : best_cut_.terms) {
    // Note that because of the slacks, initial coeff are here too.
    const IntegerValue magnitude = IntTypeAbs(entry.coeff);
    if (magnitude <= remainder_threshold) continue;
    divisors_.push_back(magnitude);
  }
  if (divisors_.empty()) return false;
  gtl::STLSortAndRemoveDuplicates(&divisors_, std::greater<IntegerValue>());

  // Note that most of the time is spend here since we call this function on
  // many linear equation, and just a few of them have a good enough scaled
  // violation. We can spend more time afterwards to tune the cut.
  //
  // TODO(user): Avoid quadratic algorithm? Note that we are quadratic in
  // relevant positions not the full cut size, but this is still too much on
  // some problems.
  IntegerValue best_divisor(0);
  double best_scaled_violation = 1e-3;
  for (const IntegerValue divisor : divisors_) {
    // Note that the function will abort right away if PositiveRemainder() is
    // not good enough, so it is quick for bad divisor.
    const double violation = GetScaledViolation(divisor, options.max_scaling,
                                                remainder_threshold, best_cut_);
    if (violation > best_scaled_violation) {
      best_scaled_violation = violation;
      best_adjusted_coeffs_ = adjusted_coeffs_;
      best_divisor = divisor;
    }
  }
  if (best_divisor == 0) return false;

  // Try best_divisor divided by small number.
  for (int div = 2; div < 9; ++div) {
    const IntegerValue divisor = best_divisor / IntegerValue(div);
    if (divisor <= 1) continue;
    const double violation = GetScaledViolation(divisor, options.max_scaling,
                                                remainder_threshold, best_cut_);
    if (violation > best_scaled_violation) {
      best_scaled_violation = violation;
      best_adjusted_coeffs_ = adjusted_coeffs_;
      best_divisor = divisor;
    }
  }

  // Re try complementation on the transformed cut.
  for (CutTerm& entry : best_cut_.terms) {
    if (!entry.HasRelevantLpValue()) break;
    if (entry.coeff % best_divisor == 0) continue;

    // Temporary try to complement this variable.
    if (!entry.Complement(&best_cut_.rhs)) continue;

    const double violation = GetScaledViolation(
        best_divisor, options.max_scaling, remainder_threshold, best_cut_);
    if (violation > best_scaled_violation) {
      // keep the change.
      ++total_num_post_complements_;
      best_scaled_violation = violation;
      best_adjusted_coeffs_ = adjusted_coeffs_;
    } else {
      // Restore.
      entry.Complement(&best_cut_.rhs);
    }
  }

  // Adjust coefficients as computed by the best GetScaledViolation().
  for (const auto [index, new_coeff] : best_adjusted_coeffs_) {
    ++total_num_coeff_adjust_;
    CutTerm& entry = best_cut_.terms[index];
    const IntegerValue remainder = new_coeff - entry.coeff;
    CHECK_GT(remainder, 0);
    entry.coeff = new_coeff;
    best_cut_.rhs += remainder * entry.bound_diff;
    best_cut_.max_magnitude =
        std::max(best_cut_.max_magnitude, IntTypeAbs(new_coeff));
  }
  best_cut_.max_magnitude =
      std::max(best_cut_.max_magnitude, IntTypeAbs(best_cut_.rhs));

  // Create the base super-additive function f().
  const IntegerValue rhs_remainder =
      PositiveRemainder(best_cut_.rhs, best_divisor);
  IntegerValue factor_t =
      GetFactorT(rhs_remainder, best_divisor, best_cut_.max_magnitude);
  auto f = GetSuperAdditiveRoundingFunction(rhs_remainder, best_divisor,
                                            factor_t, options.max_scaling);

  // Look amongst all our possible function f() for one that dominate greedily
  // our current best one. Note that we prefer lower scaling factor since that
  // result in a cut with lower coefficients.
  //
  // We only look at relevant position and ignore the other. Not sure this is
  // the best approach.
  remainders_.clear();
  for (const CutTerm& entry : best_cut_.terms) {
    if (!entry.HasRelevantLpValue()) break;
    const IntegerValue coeff = entry.coeff;
    const IntegerValue r = PositiveRemainder(coeff, best_divisor);
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
         {IntegerValue(1),
          GetFactorT(rhs_remainder, best_divisor, best_cut_.max_magnitude)}) {
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
          ++total_num_dominating_f_;
          f = g;
          factor_t = t;
          best_rs_ = rs_;
          best_d = d;
        }
      }
    }
  }

  // Use implied bounds to "lift" Booleans into the cut.
  // This should lead to stronger cuts even if the norms migth be worse.
  num_ib_used_ = 0;
  if (ib_processor != nullptr) {
    cut_builder_.ClearIndices();
    const int old_size = best_cut_.terms.size();
    for (int i = 0; i < old_size; ++i) {
      CutTerm& term = best_cut_.terms[i];

      // We only want to expand non-Boolean and non-slack term!
      if (term.bound_diff <= 1) continue;
      if (!term.IsSimple()) continue;

      if (ib_processor->TryToExpandWithLowerImpliedbound(
              factor_t, i, /*complement=*/false, &best_cut_, &cut_builder_)) {
        ++num_ib_used_;
        ++total_num_pos_lifts_;
        continue;
      }

      // Use the implied bound on (-X) if it is beneficial to do so.
      // Like complementing, this is not always good.
      //
      // We have comp(X) = diff - X = diff * B + S
      //               X = diff * (1 - B) - S.
      // So if we applies f, we will get:
      //    f(coeff * diff) * (1 - B) + f(-coeff) * S
      // and substituing S = diff * (1 - B) - X, we get:
      //    -f(-coeff) * X + [f(coeff * diff) + f(-coeff) * diff] (1 - B).
      //
      // TODO(user): Note that while the violation might be higher, if the slack
      // becomes large this will result in a less powerfull cut. Shall we do
      // that? It is a bit the same problematic with complementing.
      //
      // TODO(user): If the slack is close to zero, then this transformation
      // will always increase the violation. So we could potentially do it in
      // Before our divisor selection heuristic. But the norm of the final cut
      // will increase too.
      if (!HasComplementedImpliedBound(term, ib_processor)) continue;
      const ImpliedBoundsProcessor::BestImpliedBoundInfo info =
          ib_processor->GetCachedImpliedBoundInfo(
              term.expr_coeffs[0] > 0 ? NegationOf(term.expr_vars[0])
                                      : term.expr_vars[0]);
      // we have (var + offset) \in [0, bound_diff] so the lb of -var is
      // -(bound_diff - offset).
      const IntegerValue lb = term.expr_offset - term.bound_diff;
      const IntegerValue bound_diff = info.implied_bound - lb;
      // We do not want overflow when computing f().
      if (ProdOverflow(factor_t, CapProdI(term.coeff, bound_diff))) {
        continue;
      }

      // We only consider IB that span the full range here.
      if (bound_diff != term.bound_diff) continue;

      // Note that -f(-coeff) >= f(coeff) but coeff_b <= 0.
      const IntegerValue coeff_b =
          f(term.coeff * bound_diff) + f(-term.coeff) * bound_diff;
      CHECK_LE(coeff_b, 0);
      const double lp1 = ToDouble(f(term.coeff)) * term.lp_value;
      const double lp2 = -ToDouble(f(-term.coeff)) * term.lp_value +
                         ToDouble(coeff_b) * (1 - info.bool_lp_value);
      if (lp2 > lp1 + 1e-2) {
        // Create the Boolean term for (1 - B) in X = diff * (1 - B) - S
        // We reverse the is_positive meaning here since we have (1 - B).
        CutTerm bool_term;
        bool_term.coeff = bound_diff * term.coeff;
        bool_term.expr_vars[0] = info.bool_var;
        bool_term.expr_coeffs[1] = 0;
        bool_term.bound_diff = IntegerValue(1);
        bool_term.lp_value = 1.0 - info.bool_lp_value;
        if (!info.is_positive) {
          bool_term.expr_coeffs[0] = IntegerValue(1);
          bool_term.expr_offset = IntegerValue(0);
        } else {
          bool_term.expr_coeffs[0] = IntegerValue(-1);
          bool_term.expr_offset = IntegerValue(1);
        }

        // Create the slack term in X = diff * (1 - B) - S
        CutTerm slack_term;
        slack_term.coeff = -term.coeff;
        slack_term.expr_vars[0] = term.expr_vars[0];
        slack_term.expr_coeffs[0] = -term.expr_coeffs[0];
        slack_term.expr_vars[1] = bool_term.expr_vars[0];
        slack_term.expr_coeffs[1] = bound_diff * bool_term.expr_coeffs[0];
        slack_term.expr_offset =
            bound_diff * bool_term.expr_offset - term.expr_offset;
        slack_term.lp_value = info.SlackLpValue(lb);
        slack_term.bound_diff = term.bound_diff;

        // Commit the change.
        ++num_ib_used_;
        ++total_num_neg_lifts_;
        term = slack_term;
        cut_builder_.AddOrMergeTerm(bool_term, factor_t, &best_cut_);
      }
    }
    total_num_merges_ += cut_builder_.NumMergesSinceLastClear();
  }

  // More complementation, but for the same f.
  // If we can do that, it probably means our heuristics above are not great.
  for (int i = 0; i < 3; ++i) {
    const int64_t saved = total_num_final_complements_;
    for (CutTerm& entry : best_cut_.terms) {
      // Complementing an entry gives:
      // [a * X <= b] -> [-a * (diff - X) <= b - a * diff]
      //
      // We will compare what happen when we apply f:
      // [f(b) - f(a) * lp(X)] -> [f(b - a * diff) - f(-a) * (diff - lp(X))].
      //
      // If lp(X) is zero, then the transformation is always worse.
      // Because f(b - a * diff) >= f(b) + f(-a) * diff by super-additivity.
      //
      // However the larger is X, the better it gets since at diff, we have
      // f(b) >= f(b - a * diff) + f(a * diff) >= f(b - a * diff) + f(a) * diff.
      //
      // TODO(user): It is still unclear if we have a * X + b * (1 - X) <= rhs
      // for a Boolean X, what is the best way to apply f and if we should merge
      // the terms. If there is no other terms, best is probably
      // f(rhs - a) * X + f(rhs - b) * (1 - X).
      if (entry.coeff % best_divisor == 0) continue;
      if (!entry.HasRelevantLpValue()) continue;

      // Avoid potential overflow here.
      const IntegerValue prod(CapProdI(entry.bound_diff, entry.coeff));
      if (ProdOverflow(factor_t, prod)) continue;
      if (ProdOverflow(factor_t, CapSubI(best_cut_.rhs, prod))) continue;

      const double lp1 = ToDouble(f(best_cut_.rhs)) -
                         ToDouble(f(entry.coeff)) * entry.lp_value;
      const double lp2 = ToDouble(f(best_cut_.rhs - prod)) -
                         ToDouble(f(-entry.coeff)) *
                             (ToDouble(entry.bound_diff) - entry.lp_value);
      if (lp2 + 1e-2 < lp1) {
        if (!entry.Complement(&best_cut_.rhs)) continue;
        ++total_num_final_complements_;
      }
    }
    if (total_num_final_complements_ == saved) break;
  }

  // Apply f() to the best_cut_ with a potential improvement for one Boolean:
  //
  // If we have a Boolean X, and a cut: terms + a * X <= b;
  // By setting X to true or false, we have two inequalities:
  //   terms <= b      if X == 0
  //   terms <= b - a  if X == 1
  // We can apply f to both inequalities and recombine:
  //   f(terms) <= f(b) * (1 - X) + f(b - a) * X
  // Which change the final coeff of X from f(a) to [f(b) - f(b - a)].
  // This can only improve the cut since f(b) >= f(b - a) + f(a)
  //
  // Note that we re-Canonicalize after our possible complementation so that the
  // "improvement" is applied to larger lp_value first.
  best_cut_.Canonicalize();
  bool improved = false;
  const IntegerValue rhs = best_cut_.rhs;
  const IntegerValue f_rhs = f(best_cut_.rhs);
  best_cut_.rhs = f_rhs;
  for (CutTerm& entry : best_cut_.terms) {
    const IntegerValue f_coeff = f(entry.coeff);
    if (!improved && entry.bound_diff == 1 &&
        !ProdOverflow(factor_t, CapSubI(rhs, entry.coeff))) {
      const IntegerValue alternative = f_rhs - f(rhs - entry.coeff);
      DCHECK_GE(alternative, f_coeff);
      if (alternative > f_coeff) {
        ++total_num_bumps_;
        improved = true;
        entry.coeff = alternative;
        continue;
      }
    }
    entry.coeff = f_coeff;
  }

  if (!cut_builder_.ConvertToLinearConstraint(best_cut_, &cut_)) {
    ++total_num_overflow_abort_;
    return false;
  }
  return true;
}

CoverCutHelper::~CoverCutHelper() {
  if (!VLOG_IS_ON(1)) return;
  if (shared_stats_ == nullptr) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back({"cover_cut/num_overflows", total_num_overflow_abort_});
  stats.push_back({"cover_cut/num_lifting", total_num_lifting_});
  stats.push_back({"cover_cut/num_implied_bounds", total_num_ibs_});
  shared_stats_->AddStats(stats);
}

// Try a simple cover heuristic.
// Look for violated CUT of the form: sum (UB - X) or (X - LB) >= 1.
int CoverCutHelper::GetCoverSize(int relevant_size, IntegerValue* rhs) {
  if (relevant_size == 0) return 0;

  // Sorting can be slow, so we start by splitting the vector in 3 parts
  // [can always be in cover, candidates, can never be in cover].
  int part1 = 0;
  const double threshold = 1.0 / static_cast<double>(relevant_size);
  for (int i = 0; i < relevant_size;) {
    const double dist = base_ct_.terms[i].LpDistToMaxValue();
    if (dist < threshold) {
      // Move to part 1.
      std::swap(base_ct_.terms[i], base_ct_.terms[part1]);
      ++i;
      ++part1;
    } else if (dist < 0.9999) {
      // Keep in part 2.
      ++i;
    } else {
      // Exclude entirely (part 3).
      --relevant_size;
      std::swap(base_ct_.terms[i], base_ct_.terms[relevant_size]);
    }
  }
  std::sort(base_ct_.terms.begin() + part1,
            base_ct_.terms.begin() + relevant_size,
            [](const CutTerm& a, const CutTerm& b) {
              const double dist_a = a.LpDistToMaxValue();
              const double dist_b = b.LpDistToMaxValue();
              if (dist_a == dist_b) {
                // Prefer low coefficients if the distance is the same.
                return a.coeff < b.coeff;
              }
              return dist_a < dist_b;
            });

  double activity = 0.0;
  int cover_size = relevant_size;
  *rhs = base_ct_.rhs;
  for (int i = 0; i < relevant_size; ++i) {
    const CutTerm& term = base_ct_.terms[i];
    activity += term.LpDistToMaxValue();

    // As an heuristic we select all the term so that the sum of distance
    // to the upper bound is <= 1.0. If the corresponding rhs is negative, then
    // we will have a cut of violation at least 0.0. Note that this violation
    // can be improved by the lifting.
    //
    // TODO(user): experiment with different threshold (even greater than one).
    // Or come up with an algo that incorporate the lifting into the heuristic.
    if (activity > 0.9999) {
      cover_size = i;  // before this entry.
      break;
    }

    if (!AddProductTo(-term.coeff, term.bound_diff, rhs)) {
      // Abort early if we run into overflow.
      // In that case, rhs must be negative, and we can try this cover still.
      cover_size = i;
      DCHECK_LT(*rhs, 0);
      break;
    }
  }

  // If the rhs is now negative, we have a cut.
  //
  // Note(user): past this point, now that a given "base" cover has been chosen,
  // we basically compute the cut (of the form sum X <= bound) with the maximum
  // possible violation. Note also that we lift as much as possible, so we don't
  // necessarily optimize for the cut efficacity though. But we do get a
  // stronger cut.
  if (*rhs >= 0) return 0;
  if (cover_size == 0) return 0;

  // Transform to a minimal cover. We want to greedily remove the largest coeff
  // first, so we have more chance for the "lifting" below which can increase
  // the cut violation. If the coeff are the same, we prefer to remove high
  // distance from upper bound first.
  std::sort(base_ct_.terms.begin(), base_ct_.terms.begin() + cover_size,
            [](const CutTerm& a, const CutTerm& b) {
              if (a.coeff == b.coeff) {
                return a.LpDistToMaxValue() > b.LpDistToMaxValue();
              }
              return a.coeff > b.coeff;
            });
  for (int i = 0; i < cover_size; ++i) {
    const CutTerm& t = base_ct_.terms[i];
    if (t.bound_diff * t.coeff + *rhs >= 0) continue;
    *rhs += t.bound_diff * t.coeff;
    std::swap(base_ct_.terms[i], base_ct_.terms[--cover_size]);
  }
  DCHECK_GT(cover_size, 0);

  return cover_size;
}

bool CoverCutHelper::MakeAllTermsPositive(CutData* cut) {
  // Make sure each coeff is positive.
  //
  // TODO(user): maybe we should do it all at once to avoid some overflow
  // condition.
  for (CutTerm& term : cut->terms) {
    if (term.coeff >= 0) continue;
    if (!term.Complement(&cut->rhs)) {
      ++total_num_overflow_abort_;
      return false;
    }
  }

  // We should have aborted early if the base constraint was already infeasible.
  CHECK_GE(cut->rhs, 0);
  return true;
}

bool CoverCutHelper::TrySimpleKnapsack(const CutData& input,
                                       ImpliedBoundsProcessor* ib_processor) {
  cut_.Clear();
  base_ct_ = input;

  if (ib_processor != nullptr) {
    cut_builder_.ClearIndices();
    const int old_size = static_cast<int>(base_ct_.terms.size());
    for (int i = 0; i < old_size; ++i) {
      // We only look at non-Boolean with an lp value not close to the upper
      // bound.
      const CutTerm& term = base_ct_.terms[i];
      if (term.bound_diff <= 1) continue;
      if (term.lp_value + 1e-4 > static_cast<double>(term.bound_diff.value())) {
        continue;
      }

      if (ib_processor->TryToExpandWithLowerImpliedbound(
              IntegerValue(1), i,
              /*complement=*/false, &base_ct_, &cut_builder_)) {
        ++total_num_ibs_;
      }
    }
  }

  IntegerValue rhs;
  const int base_size = static_cast<int>(base_ct_.terms.size());
  const int cover_size = GetCoverSize(base_size, &rhs);
  if (cover_size == 0) return false;

  // The cut is just that the sum of variable cannot be at their max value.
  base_ct_.rhs = IntegerValue(-1);
  IntegerValue max_coeff(0);
  for (int i = 0; i < cover_size; ++i) {
    max_coeff = std::max(max_coeff, base_ct_.terms[i].coeff);
    base_ct_.terms[i].coeff = IntegerValue(1);
    base_ct_.rhs += base_ct_.terms[i].bound_diff;
  }
  CHECK_GT(max_coeff, 0);

  // In case the max_coeff variable is not binary, it might be possible to
  // tighten the cut a bit more.
  //
  // Note(user): I never observed this on the miplib so far.
  if (max_coeff < -rhs) {
    const IntegerValue m = FloorRatio(-rhs - 1, max_coeff);
    rhs += max_coeff * m;
    base_ct_.rhs -= m;
  }
  CHECK_LT(rhs, 0);

  IntegerValue max_base_magnitude = max_coeff;
  max_base_magnitude = std::max(max_base_magnitude, IntTypeAbs(base_ct_.rhs));
  for (int i = cover_size; i < base_size; ++i) {
    max_base_magnitude = std::max(max_base_magnitude, base_ct_.terms[i].coeff);
  }
  const IntegerValue max_scaling(std::min(
      IntegerValue(60), FloorRatio(kMaxIntegerValue, max_base_magnitude)));

  // Lift all at once the variables not used in the cover.
  //
  // We have a cut of the form sum_i X_i <= b that we will lift into
  // sum_i scaling X_i + sum f(base_coeff_j) X_j <= b * scaling.
  //
  // Using the super additivity of f() and how we construct it, for all N >= 0
  // we know that: sum_j base_coeff_j X_j <= N * max_coeff + (max_coeff - slack)
  // implies that: sum_j f(base_coeff_j) X_j <= N * scaling.
  // So by inverting the implication we have:
  // 1/ lift > N * scaling => lift_sum > N * max_coeff + (max_coeff - slack)
  // We also have:
  // 2/ cut > b -(N+1)  => original sum + (N+1) * max_coeff >= rhs + slack
  //
  // Now if we assume  cut * scaling + lift > b * scaling,
  // by taking the largest N >=0 such that lift > N * scaling, we have
  // lift <= (N + 1) * scaling, so cut * scaling > (b - (N+1))  * scaling
  //
  // And adding scaling * 2/ + 1/ we prove what we want:
  // cut * scaling + lift > b * scaling => original_sum + lift_sum > rhs.
  const IntegerValue slack = -rhs;
  const IntegerValue remainder = max_coeff - slack;
  const auto f = GetSuperAdditiveRoundingFunction(remainder, max_coeff,
                                                  IntegerValue(1), max_scaling);

  const IntegerValue scaling = f(max_coeff);
  if (scaling > 1) {
    for (int i = 0; i < cover_size; ++i) {
      base_ct_.terms[i].coeff *= scaling;
    }
    base_ct_.rhs *= scaling;
  }

  num_lifting_ = 0;
  for (int i = cover_size; i < base_size; ++i) {
    const IntegerValue positive_coeff = base_ct_.terms[i].coeff;
    const IntegerValue new_coeff = f(positive_coeff);
    base_ct_.terms[i].coeff = new_coeff;
    if (new_coeff != 0) ++num_lifting_;
  }
  total_num_lifting_ += num_lifting_;

  if (!cut_builder_.ConvertToLinearConstraint(base_ct_, &cut_)) {
    cut_.Clear();
    ++total_num_overflow_abort_;
    return false;
  }
  if (scaling > 1) DivideByGCD(&cut_);
  return true;
}

bool CoverCutHelper::TryWithLetchfordSouliLifting(
    const CutData& input, ImpliedBoundsProcessor* ib_processor) {
  cut_.Clear();
  base_ct_ = input;

  // Perform IB expansion with no restriction, all coeff should still be
  // positive.
  //
  // TODO(user): Merge Boolean terms that are complement of each other.
  if (ib_processor != nullptr) {
    cut_builder_.ClearIndices();
    const int old_size = static_cast<int>(base_ct_.terms.size());
    for (int i = 0; i < old_size; ++i) {
      if (base_ct_.terms[i].bound_diff <= 1) continue;
      if (ib_processor->TryToExpandWithLowerImpliedbound(
              IntegerValue(1), i,
              /*complement=*/false, &base_ct_, &cut_builder_)) {
        ++total_num_ibs_;
      }
    }
  }

  // TODO(user): we currently only deal with Boolean in the cover. Fix.
  const int num_bools =
      std::partition(base_ct_.terms.begin(), base_ct_.terms.end(),
                     [](const CutTerm& t) { return t.bound_diff == 1; }) -
      base_ct_.terms.begin();
  if (num_bools == 0) return false;

  IntegerValue rhs;
  const int cover_size = GetCoverSize(num_bools, &rhs);
  if (cover_size == 0) return false;

  // Collect the weight in the cover.
  IntegerValue sum(0);
  std::vector<IntegerValue> cover_weights;
  for (int i = 0; i < cover_size; ++i) {
    CHECK_EQ(base_ct_.terms[i].bound_diff, 1);
    CHECK_GT(base_ct_.terms[i].coeff, 0);
    cover_weights.push_back(base_ct_.terms[i].coeff);
    sum = CapAddI(sum, base_ct_.terms[i].coeff);
  }
  if (AtMinOrMaxInt64(sum.value())) {
    ++total_num_overflow_abort_;
    return false;
  }
  CHECK_GT(sum, base_ct_.rhs);

  // Compute the correct threshold so that if we round down larger weights to
  // p/q. We have sum of the weight in cover == base_rhs.
  IntegerValue p(0);
  IntegerValue q(0);
  IntegerValue previous_sum(0);
  std::sort(cover_weights.begin(), cover_weights.end());
  for (int i = 0; i < cover_size; ++i) {
    q = IntegerValue(cover_weights.size() - i);
    if (previous_sum + cover_weights[i] * q > base_ct_.rhs) {
      p = base_ct_.rhs - previous_sum;
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
      ++total_num_overflow_abort_;
      return false;
    }
    thresholds.push_back(CeilRatio(p * (i + 1) + 1, q));
  }

  // For the other values, we just add the weights.
  std::reverse(cover_weights.begin(), cover_weights.end());
  for (int i = q.value(); i < cover_size; ++i) {
    thresholds.push_back(thresholds.back() + cover_weights[i]);
  }
  CHECK_EQ(thresholds.back(), base_ct_.rhs + 1);

  // Generate the cut.
  //
  // Our algo is quadratic in worst case, but large coefficients should be
  // rare, and in practice we don't really see this.
  //
  // Note that this work for non-Boolean since we can just "theorically" split
  // them as a sum of Booleans :) Probably a cleaner proof exist by just using
  // the super-additivity of the lifting function on [0, rhs].
  temp_cut_.rhs = IntegerValue(cover_size - 1);
  temp_cut_.terms.clear();

  num_lifting_ = 0;
  const int base_size = static_cast<int>(base_ct_.terms.size());
  for (int i = 0; i < base_size; ++i) {
    const CutTerm& term = base_ct_.terms[i];
    const IntegerValue coeff = term.coeff;
    IntegerValue cut_coeff(1);
    if (coeff < thresholds[0]) {
      if (i >= cover_size) continue;
    } else {
      // Find the largest index <= coeff.
      //
      // TODO(user): For exact multiple of p/q we can increase the coeff by 1/2.
      // See section in the paper on getting maximal super additive function.
      for (int i = 1; i < cover_size; ++i) {
        if (coeff < thresholds[i]) break;
        cut_coeff = IntegerValue(i + 1);
      }
      if (cut_coeff != 0 && i >= cover_size) ++num_lifting_;
      if (cut_coeff > 1 && i < cover_size) ++num_lifting_;  // happen?
    }

    temp_cut_.terms.push_back(term);
    temp_cut_.terms.back().coeff = cut_coeff;
  }
  if (!cut_builder_.ConvertToLinearConstraint(temp_cut_, &cut_)) {
    cut_.Clear();
    ++total_num_overflow_abort_;
    return false;
  }
  return true;
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

        // Check for overflow with the product of expression bounds and the
        // product of one expression bound times the constant part of the other
        // expression.
        const int64_t x_max_amp = std::max(std::abs(x_lb), std::abs(x_ub));
        const int64_t y_max_amp = std::max(std::abs(y_lb), std::abs(y_ub));
        constexpr int64_t kMaxSafeInteger = (int64_t{1} << 53) - 1;
        if (CapProd(y_max_amp, x_max_amp) > kMaxSafeInteger) return true;
        if (CapProd(y_max_amp, std::abs(x.constant.value())) >
            kMaxSafeInteger) {
          return true;
        }
        if (CapProd(x_max_amp, std::abs(y.constant.value())) >
            kMaxSafeInteger) {
          return true;
        }

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

LinearConstraint ComputeHyperplanAboveSquare(AffineExpression x,
                                             AffineExpression square,
                                             IntegerValue x_lb,
                                             IntegerValue x_ub, Model* model) {
  const IntegerValue above_slope = x_ub + x_lb;
  LinearConstraintBuilder above_hyperplan(model, kMinIntegerValue,
                                          -x_lb * x_ub);
  above_hyperplan.AddTerm(square, 1);
  above_hyperplan.AddTerm(x, IntegerValue(-above_slope));
  return above_hyperplan.Build();
}

LinearConstraint ComputeHyperplanBelowSquare(AffineExpression x,
                                             AffineExpression square,
                                             IntegerValue x_value,
                                             Model* model) {
  const IntegerValue below_slope = 2 * x_value + 1;
  LinearConstraintBuilder below_hyperplan(model, -x_value - x_value * x_value,
                                          kMaxIntegerValue);
  below_hyperplan.AddTerm(square, 1);
  below_hyperplan.AddTerm(x, -below_slope);
  return below_hyperplan.Build();
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
        const IntegerValue x_ub = integer_trail->LevelZeroUpperBound(x);
        const IntegerValue x_lb = integer_trail->LevelZeroLowerBound(x);
        DCHECK_GE(x_lb, 0);

        if (x_lb == x_ub) return true;

        // Check for potential overflows.
        if (x_ub > (int64_t{1} << 31)) return true;
        DCHECK_GE(x_lb, 0);
        manager->AddCut(ComputeHyperplanAboveSquare(x, y, x_lb, x_ub, model),
                        "SquareUpper", lp_values);

        const IntegerValue x_floor =
            static_cast<int64_t>(std::floor(x.LpValue(lp_values)));
        manager->AddCut(ComputeHyperplanBelowSquare(x, y, x_floor, model),
                        "SquareLower", lp_values);
        return true;
      };

  return result;
}

ImpliedBoundsProcessor::BestImpliedBoundInfo
ImpliedBoundsProcessor::GetCachedImpliedBoundInfo(IntegerVariable var) const {
  auto it = cache_.find(var);
  if (it != cache_.end()) {
    BestImpliedBoundInfo result = it->second;
    if (result.bool_var == kNoIntegerVariable) return BestImpliedBoundInfo();
    if (integer_trail_->IsFixed(result.bool_var)) return BestImpliedBoundInfo();
    return result;
  }
  return BestImpliedBoundInfo();
}

ImpliedBoundsProcessor::BestImpliedBoundInfo
ImpliedBoundsProcessor::ComputeBestImpliedBound(
    IntegerVariable var,
    const absl::StrongVector<IntegerVariable, double>& lp_values) {
  auto it = cache_.find(var);
  if (it != cache_.end()) return it->second;
  BestImpliedBoundInfo result;
  double result_slack_lp_value = std::numeric_limits<double>::infinity();
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
    if (slack_lp_value + 1e-4 < result_slack_lp_value ||
        (slack_lp_value < result_slack_lp_value + 1e-4 &&
         entry.lower_bound > result.implied_bound)) {
      result_slack_lp_value = slack_lp_value;
      result.var_lp_value = lp_values[var];
      result.bool_lp_value = bool_lp_value;
      result.implied_bound = entry.lower_bound;
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

// Important: The cut_builder_ must have been reset.
bool ImpliedBoundsProcessor::TryToExpandWithLowerImpliedbound(
    IntegerValue factor_t, int i, bool complement, CutData* cut,
    CutDataBuilder* builder) {
  CutTerm& term = cut->terms[i];

  // We only want to expand non-Boolean and non-slack term!
  if (term.bound_diff <= 1) return false;
  if (!term.IsSimple()) return false;
  CHECK_EQ(IntTypeAbs(term.expr_coeffs[0]), 1);

  // Try lower bounded direction for implied bound.
  // This kind should always be beneficial if it exists:
  //
  // Because X = bound_diff * B + S
  // We can replace coeff * X by the expression before applying f:
  //   = f(coeff * bound_diff) * B + f(coeff) * [X - bound_diff * B]
  //   = f(coeff) * X + (f(coeff * bound_diff) - f(coeff) * bound_diff] * B
  // So we can "lift" B into the cut with a non-negative coefficient.
  //
  // Note that this lifting is really the same as if we used that implied
  // bound before since in f(coeff * bound_diff) * B + f(coeff) * S, if we
  // replace S by its value [X - bound_diff * B] we get the same result.
  //
  // TODO(user): Ignore if bound_diff == 1 ? But we can still merge B with
  // another entry if it exists, so it can still be good in this case.
  //
  // TODO(user): Only do it if coeff_b > 0 ? But again we could still merge
  // B with an existing Boolean for a better cut even if coeff_b == 0.
  const IntegerVariable ib_var = term.expr_coeffs[0] > 0
                                     ? term.expr_vars[0]
                                     : NegationOf(term.expr_vars[0]);
  const ImpliedBoundsProcessor::BestImpliedBoundInfo info =
      GetCachedImpliedBoundInfo(ib_var);
  const IntegerValue lb = -term.expr_offset;
  const IntegerValue bound_diff = info.implied_bound - lb;
  if (bound_diff <= 0) return false;
  if (info.bool_var == kNoIntegerVariable) return false;
  if (ProdOverflow(factor_t, CapProdI(term.coeff, bound_diff))) return false;

  // We have X = info.diff * Boolean + slack.
  CutTerm bool_term;
  bool_term.coeff = term.coeff * bound_diff;
  bool_term.expr_vars[0] = info.bool_var;
  bool_term.expr_coeffs[1] = 0;
  bool_term.bound_diff = IntegerValue(1);
  bool_term.lp_value = info.bool_lp_value;
  if (info.is_positive) {
    bool_term.expr_coeffs[0] = IntegerValue(1);
    bool_term.expr_offset = IntegerValue(0);
  } else {
    bool_term.expr_coeffs[0] = IntegerValue(-1);
    bool_term.expr_offset = IntegerValue(1);
  }

  // Create slack.
  // The expression is term.exp - bound_diff * bool_term
  // The variable shouldn't be the same.
  DCHECK_NE(term.expr_vars[0], bool_term.expr_vars[0]);
  CutTerm slack_term;
  slack_term.expr_vars[0] = term.expr_vars[0];
  slack_term.expr_coeffs[0] = term.expr_coeffs[0];
  slack_term.expr_vars[1] = bool_term.expr_vars[0];
  slack_term.expr_coeffs[1] = -bound_diff * bool_term.expr_coeffs[0];
  slack_term.expr_offset =
      term.expr_offset - bound_diff * bool_term.expr_offset;

  slack_term.lp_value = info.SlackLpValue(lb);
  slack_term.coeff = term.coeff;
  slack_term.bound_diff = term.bound_diff;

  // It should be good to use IB, but sometime we have things like
  // 7.3 = 2 * bool@1 + 5.3 and the expanded Boolean is at its upper bound.
  // It is always good to complement such variable.
  //
  // Note that here we do more and just complement anything closer to UB.
  //
  // TODO(user): Because of merges, we might have entry with a coefficient of
  // zero than are not useful. Remove them.
  if (complement) {
    if (bool_term.lp_value > 0.5) {
      bool_term.Complement(&cut->rhs);
    }
    if (slack_term.lp_value >
        0.5 * static_cast<double>(slack_term.bound_diff.value())) {
      slack_term.Complement(&cut->rhs);
    }
  }

  term = slack_term;
  builder->AddOrMergeTerm(bool_term, factor_t, cut);
  return true;
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
  if (ib.implied_bound != -lb) return false;

  // We have -var >= (ub - lb) bool - ub;
  // so (var - lb) <= -(ub - lb) * bool + ub - lb;
  // and (var - lb) <= bound_diff * (1 - bool).
  FlowInfo info;
  if (ib.is_positive) {
    info.bool_lp_value = 1 - ib.bool_lp_value;
    info.bool_expr.var = ib.bool_var;
    info.bool_expr.coeff = -1;
    info.bool_expr.constant = 1;
  } else {
    info.bool_lp_value = ib.bool_lp_value;
    info.bool_expr.var = ib.bool_var;
    info.bool_expr.coeff = 1;
  }
  info.capacity = IntTypeAbs(coeff) * (ub - lb);
  info.flow_lp_value = ToDouble(IntTypeAbs(coeff)) * (lp_value - ToDouble(lb));
  info.flow_expr.var = var;
  info.flow_expr.coeff = IntTypeAbs(coeff);
  info.flow_expr.constant = -lb * IntTypeAbs(coeff);

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
  if (ib.implied_bound != ub) return false;

  // We have var >= (ub - lb) bool + lb.
  // so ub - var <= ub - (ub - lb) * bool - lb.
  // and (ub - var) <= bound_diff * (1 - bool).
  FlowInfo info;
  if (ib.is_positive) {
    info.bool_lp_value = 1 - ib.bool_lp_value;
    info.bool_expr.var = ib.bool_var;
    info.bool_expr.coeff = -1;
    info.bool_expr.constant = 1;
  } else {
    info.bool_lp_value = ib.bool_lp_value;
    info.bool_expr.var = ib.bool_var;
    info.bool_expr.coeff = 1;
  }
  info.capacity = IntTypeAbs(coeff) * (ub - lb);
  info.flow_lp_value = ToDouble(IntTypeAbs(coeff)) * (ToDouble(ub) - lp_value);
  info.flow_expr.var = var;
  info.flow_expr.coeff = -IntTypeAbs(coeff);
  info.flow_expr.constant = ub * IntTypeAbs(coeff);

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

bool FlowCoverCutHelper::ComputeFlowCoverRelaxationAndGenerateCut(
    const LinearConstraint& base_ct,
    const absl::StrongVector<IntegerVariable, double>& lp_values,
    IntegerTrail* integer_trail, ImpliedBoundsProcessor* ib_helper) {
  if (!ComputeFlowCoverRelaxation(base_ct, lp_values, &snf_, integer_trail,
                                  ib_helper)) {
    return false;
  }
  return GenerateCut(snf_);
}

bool FlowCoverCutHelper::ComputeFlowCoverRelaxation(
    const LinearConstraint& base_ct,
    const absl::StrongVector<IntegerVariable, double>& lp_values,
    SingleNodeFlow* snf, IntegerTrail* integer_trail,
    ImpliedBoundsProcessor* ib_helper) {
  snf->clear();
  snf->demand = base_ct.ub;
  const int size = base_ct.vars.size();
  for (int i = 0; i < size; ++i) {
    // We can either use (X - LB) or (UB - X) for a variable in [0, capacity].
    const IntegerVariable var = base_ct.vars[i];
    const IntegerValue coeff = base_ct.coeffs[i];

    // Hack: abort if coefficient in the base constraint are too large.
    if (IntTypeAbs(coeff) > 1'000'000) return false;

    const IntegerValue lb = integer_trail->LevelZeroLowerBound(var);
    const IntegerValue ub = integer_trail->LevelZeroUpperBound(var);
    const IntegerValue capacity(
        CapProd(IntTypeAbs(coeff).value(), (ub - lb).value()));
    if (capacity >= kMaxIntegerValue) return false;
    if (lb == ub) {
      // Fixed variable shouldn't really appear here.
      snf->demand -= coeff * lb;
      continue;
    }

    // We have a Boolean, this is an easy case.
    if (ub - lb == 1) {
      ++snf->num_bool;
      FlowInfo info;
      info.bool_lp_value = (lp_values[var] - ToDouble(lb));
      info.capacity = capacity;
      info.bool_expr.var = var;
      info.bool_expr.coeff = 1;
      info.bool_expr.constant = -lb;

      info.flow_lp_value = ToDouble(capacity) * info.bool_lp_value;
      info.flow_expr.var = var;
      info.flow_expr.coeff = info.capacity;
      info.flow_expr.constant = -lb * info.capacity;

      // coeff * x = coeff * (x - lb) + coeff * lb;
      snf->demand -= coeff * lb;
      if (coeff > 0) {
        snf->in_flow.push_back(info);
      } else {
        snf->out_flow.push_back(info);
      }
      continue;
    }

    // TODO(user): Improve our logic to decide what implied bounds to use. We
    // rely on the best implied bounds, not necessarily one implying var at its
    // level zero bound like we need here.
    const double lp = lp_values[var];
    const bool prefer_lb = (lp - ToDouble(lb)) > (ToDouble(ub) - lp);
    if (prefer_lb) {
      if (TryXminusLB(var, lp, lb, ub, coeff, ib_helper, snf)) {
        ++snf->num_to_lb;
        continue;
      }
      if (TryUBminusX(var, lp, lb, ub, coeff, ib_helper, snf)) {
        ++snf->num_to_ub;
        continue;
      }
    } else {
      if (TryUBminusX(var, lp, lb, ub, coeff, ib_helper, snf)) {
        ++snf->num_to_ub;
        continue;
      }
      if (TryXminusLB(var, lp, lb, ub, coeff, ib_helper, snf)) {
        ++snf->num_to_lb;
        continue;
      }
    }

    // Abort.
    // TODO(user): Technically we can always use a arc usage Boolean fixed to 1.
    return false;
  }

  return true;
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
    IntegerValue sum_in = 0;
    IntegerValue sum_out = 0;
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
    cut_builder_.AddTerm(info.flow_expr, -1);
    if (info.capacity > slack) {
      num_in_bin_++;
      const IntegerValue coeff = info.capacity - slack;
      cut_builder_.AddConstant(-coeff);
      cut_builder_.AddTerm(info.bool_expr, coeff);
    }
  }
  for (int i = 0; i < data.out_flow.size(); ++i) {
    const FlowInfo& info = data.out_flow[i];
    if (out_cover[i]) {
      num_out_capa_++;
      cut_builder_.AddConstant(info.capacity);
    } else if (info.capacity > slack) {
      num_out_bin_++;
      cut_builder_.AddTerm(info.bool_expr, slack);
    } else {
      num_out_flow_++;
      cut_builder_.AddTerm(info.flow_expr, 1);
    }
  }

  // TODO(user): Lift the cut.
  cut_ = cut_builder_.BuildConstraint(-data.demand, kMaxIntegerValue);
  return true;
}

void SumOfAllDiffLowerBounder::Clear() {
  min_values_.clear();
  expr_mins_.clear();
}

void SumOfAllDiffLowerBounder::Add(const AffineExpression& expr, int num_exprs,
                                   const IntegerTrail& integer_trail) {
  expr_mins_.push_back(integer_trail.LevelZeroLowerBound(expr).value());

  if (integer_trail.IsFixed(expr)) {
    min_values_.insert(integer_trail.FixedValue(expr));
  } else {
    if (expr.coeff > 0) {
      int count = 0;
      for (const IntegerValue value :
           integer_trail.InitialVariableDomain(expr.var).Values()) {
        min_values_.insert(expr.ValueAt(value));
        if (++count >= num_exprs) break;
      }
    } else {
      int count = 0;
      for (const IntegerValue value :
           integer_trail.InitialVariableDomain(expr.var).Negation().Values()) {
        min_values_.insert(-expr.ValueAt(value));
        if (++count >= num_exprs) break;
      }
    }
  }
}

IntegerValue SumOfAllDiffLowerBounder::SumOfMinDomainValues() {
  int count = 0;
  IntegerValue sum = 0;
  for (const IntegerValue value : min_values_) {
    sum += value;
    if (++count >= expr_mins_.size()) return sum;
  }
  return sum;
}

IntegerValue SumOfAllDiffLowerBounder::SumOfDifferentMins() {
  std::sort(expr_mins_.begin(), expr_mins_.end());
  IntegerValue tmp_value = kMinIntegerValue;
  IntegerValue result = 0;
  for (const IntegerValue value : expr_mins_) {
    // Make sure values are different.
    tmp_value = std::max(tmp_value + 1, value);
    result += tmp_value;
  }
  return result;
}

IntegerValue SumOfAllDiffLowerBounder::GetBestLowerBound(std::string& suffix) {
  const IntegerValue domain_bound = SumOfMinDomainValues();
  const IntegerValue alldiff_bound = SumOfDifferentMins();
  if (domain_bound > alldiff_bound) {
    suffix = "d";
    return domain_bound;
  }
  suffix = alldiff_bound > domain_bound ? "a" : "e";
  return alldiff_bound;
}

namespace {

void TryToGenerateAllDiffCut(
    const std::vector<std::pair<double, AffineExpression>>& sorted_exprs_lp,
    const IntegerTrail& integer_trail,
    const absl::StrongVector<IntegerVariable, double>& lp_values,
    TopNCuts& top_n_cuts, Model* model) {
  const int num_exprs = sorted_exprs_lp.size();

  std::vector<AffineExpression> current_set_exprs;
  SumOfAllDiffLowerBounder diff_mins;
  SumOfAllDiffLowerBounder negated_diff_maxes;

  double sum = 0.0;

  for (const auto& [expr_lp, expr] : sorted_exprs_lp) {
    sum += expr_lp;
    diff_mins.Add(expr, num_exprs, integer_trail);
    negated_diff_maxes.Add(expr.Negated(), num_exprs, integer_trail);
    current_set_exprs.push_back(expr);
    CHECK_EQ(current_set_exprs.size(), diff_mins.size());
    CHECK_EQ(current_set_exprs.size(), negated_diff_maxes.size());
    std::string min_suffix;
    const IntegerValue required_min_sum =
        diff_mins.GetBestLowerBound(min_suffix);
    std::string max_suffix;
    const IntegerValue required_max_sum =
        -negated_diff_maxes.GetBestLowerBound(max_suffix);
    if (sum < ToDouble(required_min_sum) - kMinCutViolation ||
        sum > ToDouble(required_max_sum) + kMinCutViolation) {
      LinearConstraintBuilder cut(model, required_min_sum, required_max_sum);
      for (AffineExpression expr : current_set_exprs) {
        cut.AddTerm(expr, IntegerValue(1));
      }
      top_n_cuts.AddCut(cut.Build(),
                        absl::StrCat("AllDiff_", min_suffix, max_suffix),
                        lp_values);
      // NOTE: We can extend the current set but it is more helpful to generate
      // the cut on a different set of variables so we reset the counters.
      sum = 0.0;
      current_set_exprs.clear();
      diff_mins.Clear();
      negated_diff_maxes.Clear();
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

        TopNCuts top_n_cuts(5);
        std::sort(sorted_exprs.begin(), sorted_exprs.end(),
                  [](std::pair<double, AffineExpression>& a,
                     const std::pair<double, AffineExpression>& b) {
                    return a.first < b.first;
                  });
        TryToGenerateAllDiffCut(sorted_exprs, *integer_trail, lp_values,
                                top_n_cuts, model);
        // Other direction.
        std::reverse(sorted_exprs.begin(), sorted_exprs.end());
        TryToGenerateAllDiffCut(sorted_exprs, *integer_trail, lp_values,
                                top_n_cuts, model);
        top_n_cuts.TransferToManager(lp_values, manager);
        return true;
      };
  VLOG(2) << "Created all_diff cut generator of size: " << exprs.size();
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

          manager->AddCut(builder.Build(), "Clique", lp_values);
        }
        return true;
      };
  return result;
}

}  // namespace sat
}  // namespace operations_research

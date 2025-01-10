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

#include "ortools/sat/cuts.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/meta/type_traits.h"
#include "absl/numeric/int128.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

namespace {

// TODO(user): the function ToDouble() does some testing for min/max integer
// value and we don't need that here.
double AsDouble(IntegerValue v) { return static_cast<double>(v.value()); }

}  // namespace

std::string CutTerm::DebugString() const {
  return absl::StrCat("coeff=", coeff.value(), " lp=", lp_value,
                      " range=", bound_diff.value(), " expr=[",
                      expr_coeffs[0].value(), ",", expr_coeffs[1].value(), "]",
                      " * [V", expr_vars[0].value(), ",V", expr_vars[1].value(),
                      "]", " + ", expr_offset.value());
}

std::string CutData::DebugString() const {
  std::string result = absl::StrCat("CutData rhs=", rhs, "\n");
  for (const CutTerm& term : terms) {
    absl::StrAppend(&result, term.DebugString(), "\n");
  }
  return result;
}

void CutTerm::Complement(absl::int128* rhs) {
  // We replace coeff * X by coeff * (X - bound_diff + bound_diff)
  // which gives -coeff * complement(X) + coeff * bound_diff;
  *rhs -= absl::int128(coeff.value()) * absl::int128(bound_diff.value());

  // We keep the same expression variable.
  for (int i = 0; i < 2; ++i) {
    expr_coeffs[i] = -expr_coeffs[i];
  }
  expr_offset = bound_diff - expr_offset;

  // Note that this is not involutive because of floating point error. Fix?
  lp_value = static_cast<double>(bound_diff.value()) - lp_value;
  coeff = -coeff;

  // Swap the implied bound info.
  std::swap(cached_implied_lb, cached_implied_ub);
}

void CutTerm::ReplaceExpressionByLiteral(IntegerVariable var) {
  CHECK_EQ(bound_diff, 1);
  expr_coeffs[1] = 0;
  if (VariableIsPositive(var)) {
    expr_vars[0] = var;
    expr_coeffs[0] = 1;
    expr_offset = 0;
  } else {
    expr_vars[0] = PositiveVariable(var);
    expr_coeffs[0] = -1;
    expr_offset = 1;
  }
}

IntegerVariable CutTerm::GetUnderlyingLiteralOrNone() const {
  if (expr_coeffs[1] != 0) return kNoIntegerVariable;
  if (bound_diff != 1) return kNoIntegerVariable;

  if (expr_coeffs[0] > 0) {
    if (expr_coeffs[0] != 1) return kNoIntegerVariable;
    if (expr_offset != 0) return kNoIntegerVariable;
    CHECK(VariableIsPositive(expr_vars[0]));
    return expr_vars[0];
  }

  if (expr_coeffs[0] != -1) return kNoIntegerVariable;
  if (expr_offset != 1) return kNoIntegerVariable;
  CHECK(VariableIsPositive(expr_vars[0]));
  return NegationOf(expr_vars[0]);
}

// To try to minimize the risk of overflow, we switch to the bound closer
// to the lp_value. Since most of our base constraint for cut are tight,
// hopefully this is not too bad.
bool CutData::AppendOneTerm(IntegerVariable var, IntegerValue coeff,
                            double lp_value, IntegerValue lb, IntegerValue ub) {
  if (coeff == 0) return true;
  const IntegerValue bound_diff = ub - lb;

  // Complement the variable so that it is has a positive coefficient.
  const bool complement = coeff < 0;

  // See formula below, the constant term is either coeff * lb or coeff * ub.
  rhs -= absl::int128(coeff.value()) *
         absl::int128(complement ? ub.value() : lb.value());

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
    entry.lp_value = static_cast<double>(ub.value()) - lp_value;
  } else {
    // C = (X - LB) + LB
    entry.expr_coeffs[0] = IntegerValue(1);
    entry.expr_offset = -lb;
    entry.coeff = coeff;
    entry.lp_value = lp_value - static_cast<double>(lb.value());
  }
  terms.push_back(entry);
  return true;
}

bool CutData::FillFromLinearConstraint(
    const LinearConstraint& base_ct,
    const util_intops::StrongVector<IntegerVariable, double>& lp_values,
    IntegerTrail* integer_trail) {
  rhs = absl::int128(base_ct.ub.value());
  terms.clear();
  const int num_terms = base_ct.num_terms;
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
    IntegerValue ub, absl::Span<const IntegerVariable> vars,
    absl::Span<const IntegerValue> coeffs, absl::Span<const double> lp_values,
    absl::Span<const IntegerValue> lower_bounds,
    absl::Span<const IntegerValue> upper_bounds) {
  rhs = absl::int128(ub.value());
  terms.clear();

  const int size = lp_values.size();
  if (size == 0) return true;

  CHECK_EQ(vars.size(), size);
  CHECK_EQ(coeffs.size(), size);
  CHECK_EQ(lower_bounds.size(), size);
  CHECK_EQ(upper_bounds.size(), size);

  for (int i = 0; i < size; ++i) {
    if (!AppendOneTerm(vars[i], coeffs[i], lp_values[i], lower_bounds[i],
                       upper_bounds[i])) {
      return false;
    }
  }

  return true;
}

void CutData::ComplementForPositiveCoefficients() {
  for (CutTerm& term : terms) {
    if (term.coeff >= 0) continue;
    term.Complement(&rhs);
  }
}

void CutData::ComplementForSmallerLpValues() {
  for (CutTerm& term : terms) {
    if (term.lp_value <= term.LpDistToMaxValue()) continue;
    term.Complement(&rhs);
  }
}

bool CutData::AllCoefficientsArePositive() const {
  for (const CutTerm& term : terms) {
    if (term.coeff < 0) return false;
  }
  return true;
}

void CutData::SortRelevantEntries() {
  num_relevant_entries = 0;
  max_magnitude = 0;
  for (CutTerm& entry : terms) {
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

double CutData::ComputeViolation() const {
  double violation = -static_cast<double>(rhs);
  for (const CutTerm& term : terms) {
    violation += term.lp_value * static_cast<double>(term.coeff.value());
  }
  return violation;
}

double CutData::ComputeEfficacy() const {
  double violation = -static_cast<double>(rhs);
  double norm = 0.0;
  for (const CutTerm& term : terms) {
    const double coeff = static_cast<double>(term.coeff.value());
    violation += term.lp_value * coeff;
    norm += coeff * coeff;
  }
  return violation / std::sqrt(norm);
}

// We can only merge the term if term.coeff + old_coeff do not overflow and
// if t * new_coeff do not overflow.
//
// If we cannot merge the term, we will keep them separate. The produced cut
// will be less strong, but can still be used.
bool CutDataBuilder::MergeIfPossible(IntegerValue t, CutTerm& to_add,
                                     CutTerm& target) {
  DCHECK_EQ(to_add.expr_vars[0], target.expr_vars[0]);
  DCHECK_EQ(to_add.expr_coeffs[0], target.expr_coeffs[0]);

  const IntegerValue new_coeff = CapAddI(to_add.coeff, target.coeff);
  if (AtMinOrMaxInt64I(new_coeff) || ProdOverflow(t, new_coeff)) {
    return false;
  }

  to_add.coeff = 0;  // Clear since we merge it.
  target.coeff = new_coeff;
  return true;
}

// We only deal with coeff * Bool or coeff * (1 - Bool)
//
// TODO(user): Because of merges, we might have entry with a coefficient of
// zero than are not useful. Remove them?
int CutDataBuilder::AddOrMergeBooleanTerms(absl::Span<CutTerm> new_terms,
                                           IntegerValue t, CutData* cut) {
  if (new_terms.empty()) return 0;

  bool_index_.clear();
  secondary_bool_index_.clear();
  int num_merges = 0;

  // Fill the maps.
  int i = 0;
  for (CutTerm& term : new_terms) {
    const IntegerVariable var = term.expr_vars[0];
    auto& map = term.expr_coeffs[0] > 0 ? bool_index_ : secondary_bool_index_;
    const auto [it, inserted] = map.insert({var, i});
    if (!inserted) {
      if (MergeIfPossible(t, term, new_terms[it->second])) {
        ++num_merges;
      }
    }
    ++i;
  }

  // Loop over the cut now. Note that we loop with indices as we might add new
  // terms in the middle of the loop.
  for (CutTerm& term : cut->terms) {
    if (term.bound_diff != 1) continue;
    if (!term.IsSimple()) continue;

    const IntegerVariable var = term.expr_vars[0];
    auto& map = term.expr_coeffs[0] > 0 ? bool_index_ : secondary_bool_index_;
    auto it = map.find(var);
    if (it == map.end()) continue;

    // We found a match, try to merge the map entry into the cut.
    // Note that we don't waste time erasing this entry from the map since
    // we should have no duplicates in the original cut.
    if (MergeIfPossible(t, new_terms[it->second], term)) {
      ++num_merges;
    }
  }

  // Finally add the terms we couldn't merge.
  for (const CutTerm& term : new_terms) {
    if (term.coeff == 0) continue;
    cut->terms.push_back(term);
  }

  return num_merges;
}

// TODO(user): Divide by gcd first to avoid possible overflow in the
// conversion? it is however unlikely given that our coeffs should be small.
ABSL_DEPRECATED("Only used in tests, this will be removed.")
bool CutDataBuilder::ConvertToLinearConstraint(const CutData& cut,
                                               LinearConstraint* output) {
  tmp_map_.clear();
  if (cut.rhs > absl::int128(std::numeric_limits<int64_t>::max()) ||
      cut.rhs < absl::int128(std::numeric_limits<int64_t>::min())) {
    return false;
  }
  IntegerValue new_rhs = static_cast<int64_t>(cut.rhs);
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

  output->lb = kMinIntegerValue;
  output->ub = new_rhs;
  output->resize(tmp_map_.size());
  int new_size = 0;
  for (const auto [var, coeff] : tmp_map_) {
    if (coeff == 0) continue;
    output->vars[new_size] = var;
    output->coeffs[new_size] = coeff;
    ++new_size;
  }
  output->resize(new_size);
  DivideByGCD(output);
  return true;
}

namespace {

// Minimum amount of violation of the cut constraint by the solution. This
// is needed to avoid numerical issues and adding cuts with minor effect.
const double kMinCutViolation = 1e-4;

IntegerValue PositiveRemainder(absl::int128 dividend,
                               IntegerValue positive_divisor) {
  DCHECK_GT(positive_divisor, 0);
  const IntegerValue m =
      static_cast<int64_t>(dividend % absl::int128(positive_divisor.value()));
  return m < 0 ? m + positive_divisor : m;
}

// We use the fact that f(k * divisor + rest) = k * f(divisor) + f(rest)
absl::int128 ApplyToInt128(const std::function<IntegerValue(IntegerValue)>& f,
                           IntegerValue divisor, absl::int128 value) {
  const IntegerValue rest = PositiveRemainder(value, divisor);
  const absl::int128 k =
      (value - absl::int128(rest.value())) / absl::int128(divisor.value());
  const absl::int128 result =
      k * absl::int128(f(divisor).value()) + absl::int128(f(rest).value());
  return result;
}

// Apply f() to the cut with a potential improvement for one Boolean:
//
// If we have a Boolean X, and a cut: terms + a * X <= b;
// By setting X to true or false, we have two inequalities:
//   terms <= b      if X == 0
//   terms <= b - a  if X == 1
// We can apply f to both inequalities and recombine:
//   f(terms) <= f(b) * (1 - X) + f(b - a) * X
// Which change the final coeff of X from f(a) to [f(b) - f(b - a)].
// This can only improve the cut since f(b) >= f(b - a) + f(a)
int ApplyWithPotentialBump(const std::function<IntegerValue(IntegerValue)>& f,
                           const IntegerValue divisor, CutData* cut) {
  int bump_index = -1;
  double bump_score = -1.0;
  IntegerValue bump_coeff;
  const IntegerValue remainder = PositiveRemainder(cut->rhs, divisor);
  const IntegerValue f_remainder = f(remainder);
  cut->rhs = ApplyToInt128(f, divisor, cut->rhs);
  for (int i = 0; i < cut->terms.size(); ++i) {
    CutTerm& entry = cut->terms[i];
    const IntegerValue f_coeff = f(entry.coeff);
    if (entry.bound_diff == 1) {
      // TODO(user): we probably don't need int128 here, but we need
      // t * (remainder - entry.coeff) not to overflow, and we can't really be
      // sure.
      const IntegerValue alternative =
          entry.coeff > 0
              ? f_remainder - f(remainder - entry.coeff)
              : f_remainder - IntegerValue(static_cast<int64_t>(ApplyToInt128(
                                  f, divisor,
                                  absl::int128(remainder.value()) -
                                      absl::int128(entry.coeff.value()))));
      DCHECK_GE(alternative, f_coeff);
      if (alternative > f_coeff) {
        const double score = ToDouble(alternative - f_coeff) * entry.lp_value;
        if (score > bump_score) {
          bump_index = i;
          bump_score = score;
          bump_coeff = alternative;
        }
      }
    }
    entry.coeff = f_coeff;
  }
  if (bump_index >= 0) {
    cut->terms[bump_index].coeff = bump_coeff;
    return 1;
  }
  return 0;
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

std::function<IntegerValue(IntegerValue)> GetSuperAdditiveStrengtheningFunction(
    IntegerValue positive_rhs, IntegerValue min_magnitude) {
  CHECK_GT(positive_rhs, 0);
  CHECK_GT(min_magnitude, 0);

  if (min_magnitude >= CeilRatio(positive_rhs, 2)) {
    return [positive_rhs](IntegerValue v) {
      if (v >= 0) return IntegerValue(0);
      if (v > -positive_rhs) return IntegerValue(-1);
      return IntegerValue(-2);
    };
  }

  // The transformation only work if 2 * second_threshold >= positive_rhs.
  //
  // TODO(user): Limit the number of value used with scaling like above.
  min_magnitude = std::min(min_magnitude, FloorRatio(positive_rhs, 2));
  const IntegerValue second_threshold = positive_rhs - min_magnitude;
  return [positive_rhs, min_magnitude, second_threshold](IntegerValue v) {
    if (v >= 0) return IntegerValue(0);
    if (v <= -positive_rhs) return -positive_rhs;
    if (v <= -second_threshold) return -second_threshold;

    // This should actually never happen by the definition of min_magnitude.
    // But with it, the function is supper-additive even if min_magnitude is not
    // correct.
    if (v >= -min_magnitude) return -min_magnitude;

    // TODO(user): we might want to intoduce some step to reduce the final
    // magnitude of the cut.
    return v;
  };
}

std::function<IntegerValue(IntegerValue)>
GetSuperAdditiveStrengtheningMirFunction(IntegerValue positive_rhs,
                                         IntegerValue scaling) {
  if (scaling >= positive_rhs) {
    // Simple case, no scaling required.
    return [positive_rhs](IntegerValue v) {
      if (v >= 0) return IntegerValue(0);
      if (v <= -positive_rhs) return -positive_rhs;
      return v;
    };
  }

  // We need to scale.
  scaling =
      std::min(scaling, IntegerValue(std::numeric_limits<int64_t>::max()) /
                            positive_rhs);
  if (scaling == 1) {
    return [](IntegerValue v) {
      if (v >= 0) return IntegerValue(0);
      return IntegerValue(-1);
    };
  }

  // We divide [-positive_rhs + 1, 0] into (scaling - 1) bucket.
  return [positive_rhs, scaling](IntegerValue v) {
    if (v >= 0) return IntegerValue(0);
    if (v <= -positive_rhs) return -scaling;
    return FloorRatio(v * (scaling - 1), (positive_rhs - 1));
  };
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
  absl::int128 rhs = cut.rhs;
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
    double max_violation = static_cast<double>(initial_rhs_remainder.value());
    for (int i = 0; i < cut.num_relevant_entries; ++i) {
      const CutTerm& entry = cut.terms[i];
      const IntegerValue remainder = PositiveRemainder(entry.coeff, divisor);
      if (remainder == 0) continue;
      if (remainder <= initial_rhs_remainder) {
        // We do not know exactly f() yet, but it will always round to the
        // floor of the division by divisor in this case.
        max_violation -=
            static_cast<double>(remainder.value()) * entry.lp_value;
        if (max_violation <= 1e-3) return 0.0;
        continue;
      }

      // Adjust coeff of the form k * divisor - epsilon.
      const IntegerValue adjust = divisor - remainder;
      const IntegerValue prod = CapProdI(adjust, entry.bound_diff);
      if (prod <= adjust_threshold) {
        rhs += absl::int128(prod.value());
        const IntegerValue new_coeff = entry.coeff + adjust;
        adjusted_coeffs_.push_back({i, new_coeff});
        max_magnitude = std::max(max_magnitude, IntTypeAbs(new_coeff));
      }
    }
  }

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
  double violation = -static_cast<double>(ApplyToInt128(f, divisor, rhs));
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

// TODO(user): This is slow, 50% of run time on a2c1s1.pb.gz. Optimize!
bool IntegerRoundingCutHelper::ComputeCut(
    RoundingOptions options, const CutData& base_ct,
    ImpliedBoundsProcessor* ib_processor) {
  // Try IB before heuristic?
  // This should be better except it can mess up the norm and the divisors.
  cut_ = base_ct;
  if (options.use_ib_before_heuristic && ib_processor != nullptr) {
    std::vector<CutTerm>* new_bool_terms =
        ib_processor->ClearedMutableTempTerms();
    for (CutTerm& term : cut_.terms) {
      if (term.bound_diff <= 1) continue;
      if (!term.HasRelevantLpValue()) continue;

      if (options.prefer_positive_ib && term.coeff < 0) {
        // We complement the term before trying the implied bound.
        term.Complement(&cut_.rhs);
        if (ib_processor->TryToExpandWithLowerImpliedbound(
                IntegerValue(1),
                /*complement=*/true, &term, &cut_.rhs, new_bool_terms)) {
          ++total_num_initial_ibs_;
          continue;
        }
        term.Complement(&cut_.rhs);
      }

      if (ib_processor->TryToExpandWithLowerImpliedbound(
              IntegerValue(1),
              /*complement=*/true, &term, &cut_.rhs, new_bool_terms)) {
        ++total_num_initial_ibs_;
      }
    }

    // TODO(user): We assume that this is called with and without the option
    // use_ib_before_heuristic, so that we can abort if no IB has been applied
    // since then we will redo the computation. This is not really clean.
    if (new_bool_terms->empty()) return false;
    total_num_initial_merges_ +=
        ib_processor->MutableCutBuilder()->AddOrMergeBooleanTerms(
            absl::MakeSpan(*new_bool_terms), IntegerValue(1), &cut_);
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
  cut_.SortRelevantEntries();
  const IntegerValue remainder_threshold(
      std::max(IntegerValue(1), cut_.max_magnitude / 1000));
  if (cut_.rhs >= 0 && cut_.rhs < remainder_threshold.value()) {
    return false;
  }

  // There is no point trying twice the same divisor or a divisor that is too
  // small. Note that we use a higher threshold than the remainder_threshold
  // because we can boost the remainder thanks to our adjusting heuristic
  // below and also because this allows to have cuts with a small range of
  // coefficients.
  divisors_.clear();
  for (const CutTerm& entry : cut_.terms) {
    // Note that because of the slacks, initial coeff are here too.
    const IntegerValue magnitude = IntTypeAbs(entry.coeff);
    if (magnitude <= remainder_threshold) continue;
    divisors_.push_back(magnitude);

    // If we have too many divisor to try, restrict to the first ones which
    // should correspond to the highest lp values.
    if (divisors_.size() > 50) break;
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
                                                remainder_threshold, cut_);
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
                                                remainder_threshold, cut_);
    if (violation > best_scaled_violation) {
      best_scaled_violation = violation;
      best_adjusted_coeffs_ = adjusted_coeffs_;
      best_divisor = divisor;
    }
  }

  // Re try complementation on the transformed cut.
  // TODO(user): This can be quadratic! we don't want to try too much of them.
  // Or optimize the algo, we should be able to be more incremental here.
  // see on g200x740.pb.gz for instance.
  for (CutTerm& entry : cut_.terms) {
    if (!entry.HasRelevantLpValue()) break;
    if (entry.coeff % best_divisor == 0) continue;

    // Temporary complement this variable.
    entry.Complement(&cut_.rhs);

    const double violation = GetScaledViolation(
        best_divisor, options.max_scaling, remainder_threshold, cut_);
    if (violation > best_scaled_violation) {
      // keep the change.
      ++total_num_post_complements_;
      best_scaled_violation = violation;
      best_adjusted_coeffs_ = adjusted_coeffs_;
    } else {
      // Restore.
      entry.Complement(&cut_.rhs);
    }
  }

  // Adjust coefficients as computed by the best GetScaledViolation().
  for (const auto [index, new_coeff] : best_adjusted_coeffs_) {
    ++total_num_coeff_adjust_;
    CutTerm& entry = cut_.terms[index];
    const IntegerValue remainder = new_coeff - entry.coeff;
    CHECK_GT(remainder, 0);
    entry.coeff = new_coeff;
    cut_.rhs += absl::int128(remainder.value()) *
                absl::int128(entry.bound_diff.value());
    cut_.max_magnitude = std::max(cut_.max_magnitude, IntTypeAbs(new_coeff));
  }

  // Create the base super-additive function f().
  const IntegerValue rhs_remainder = PositiveRemainder(cut_.rhs, best_divisor);
  IntegerValue factor_t =
      GetFactorT(rhs_remainder, best_divisor, cut_.max_magnitude);
  auto f = GetSuperAdditiveRoundingFunction(rhs_remainder, best_divisor,
                                            factor_t, options.max_scaling);

  // Look amongst all our possible function f() for one that dominate greedily
  // our current best one. Note that we prefer lower scaling factor since that
  // result in a cut with lower coefficients.
  //
  // We only look at relevant position and ignore the other. Not sure this is
  // the best approach.
  remainders_.clear();
  for (const CutTerm& entry : cut_.terms) {
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
          GetFactorT(rhs_remainder, best_divisor, cut_.max_magnitude)}) {
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
  // This should lead to stronger cuts even if the norms might be worse.
  num_ib_used_ = 0;
  if (ib_processor != nullptr) {
    const auto [num_lb, num_ub, num_merges] =
        ib_processor->PostprocessWithImpliedBound(f, factor_t, &cut_);
    total_num_pos_lifts_ += num_lb;
    total_num_neg_lifts_ += num_ub;
    total_num_merges_ += num_merges;
    num_ib_used_ = num_lb + num_ub;
  }

  // More complementation, but for the same f.
  // If we can do that, it probably means our heuristics above are not great.
  for (int i = 0; i < 3; ++i) {
    const int64_t saved = total_num_final_complements_;
    for (CutTerm& entry : cut_.terms) {
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
      const IntegerValue remainder = PositiveRemainder(cut_.rhs, best_divisor);
      if (ProdOverflow(factor_t, prod)) continue;
      if (ProdOverflow(factor_t, CapSubI(remainder, prod))) continue;

      const double lp1 =
          ToDouble(f(remainder)) - ToDouble(f(entry.coeff)) * entry.lp_value;
      const double lp2 = ToDouble(f(remainder - prod)) -
                         ToDouble(f(-entry.coeff)) *
                             (ToDouble(entry.bound_diff) - entry.lp_value);
      if (lp2 + 1e-2 < lp1) {
        entry.Complement(&cut_.rhs);
        ++total_num_final_complements_;
      }
    }
    if (total_num_final_complements_ == saved) break;
  }

  total_num_bumps_ += ApplyWithPotentialBump(f, best_divisor, &cut_);
  return true;
}

CoverCutHelper::~CoverCutHelper() {
  if (!VLOG_IS_ON(1)) return;
  if (shared_stats_ == nullptr) return;

  std::vector<std::pair<std::string, int64_t>> stats;
  const auto add_stats = [&stats](absl::string_view name, const CutStats& s) {
    stats.push_back(
        {absl::StrCat(name, "num_overflows"), s.num_overflow_aborts});
    stats.push_back({absl::StrCat(name, "num_lifting"), s.num_lifting});
    stats.push_back({absl::StrCat(name, "num_initial_ib"), s.num_initial_ibs});
    stats.push_back({absl::StrCat(name, "num_implied_lb"), s.num_lb_ibs});
    stats.push_back({absl::StrCat(name, "num_implied_ub"), s.num_ub_ibs});
    stats.push_back({absl::StrCat(name, "num_bumps"), s.num_bumps});
    stats.push_back({absl::StrCat(name, "num_cuts"), s.num_cuts});
    stats.push_back({absl::StrCat(name, "num_merges"), s.num_merges});
  };
  add_stats("cover_cut/", cover_stats_);
  add_stats("flow_cut/", flow_stats_);
  add_stats("ls_cut/", ls_stats_);
  shared_stats_->AddStats(stats);
}

namespace {

struct LargeCoeffFirst {
  bool operator()(const CutTerm& a, const CutTerm& b) const {
    if (a.coeff == b.coeff) {
      return a.LpDistToMaxValue() > b.LpDistToMaxValue();
    }
    return a.coeff > b.coeff;
  }
};

struct SmallContribFirst {
  bool operator()(const CutTerm& a, const CutTerm& b) const {
    const double contrib_a = a.lp_value * static_cast<double>(a.coeff.value());
    const double contrib_b = b.lp_value * static_cast<double>(b.coeff.value());
    return contrib_a < contrib_b;
  }
};

struct LargeContribFirst {
  bool operator()(const CutTerm& a, const CutTerm& b) const {
    const double contrib_a = a.lp_value * static_cast<double>(a.coeff.value());
    const double contrib_b = b.lp_value * static_cast<double>(b.coeff.value());
    return contrib_a > contrib_b;
  }
};

struct LargeLpValueFirst {
  bool operator()(const CutTerm& a, const CutTerm& b) const {
    if (a.lp_value == b.lp_value) {
      // Prefer high coefficients if the distance is the same.
      // We have more chance to get a cover this way.
      return a.coeff > b.coeff;
    }
    return a.lp_value > b.lp_value;
  }
};

// When minimizing a cover we want to remove bad score (large dist) divided by
// item size. Note that here we assume item are "boolean" fully taken or not.
// for general int we use (lp_dist / bound_diff) / (coeff * bound_diff) which
// lead to the same formula as for Booleans.
struct KnapsackAdd {
  bool operator()(const CutTerm& a, const CutTerm& b) const {
    const double contrib_a =
        a.LpDistToMaxValue() / static_cast<double>(a.coeff.value());
    const double contrib_b =
        b.LpDistToMaxValue() / static_cast<double>(b.coeff.value());
    return contrib_a < contrib_b;
  }
};
struct KnapsackRemove {
  bool operator()(const CutTerm& a, const CutTerm& b) const {
    const double contrib_a =
        a.LpDistToMaxValue() / static_cast<double>(a.coeff.value());
    const double contrib_b =
        b.LpDistToMaxValue() / static_cast<double>(b.coeff.value());
    return contrib_a > contrib_b;
  }
};

}  // namespace.

// Transform to a minimal cover. We want to greedily remove the largest coeff
// first, so we have more chance for the "lifting" below which can increase
// the cut violation. If the coeff are the same, we prefer to remove high
// distance from upper bound first.
template <class Compare>
int CoverCutHelper::MinimizeCover(int cover_size, absl::int128 slack) {
  CHECK_GT(slack, 0);
  absl::Span<CutTerm> terms = absl::MakeSpan(cut_.terms);
  std::sort(terms.begin(), terms.begin() + cover_size, Compare());
  for (int i = 0; i < cover_size;) {
    const CutTerm& t = terms[i];
    const absl::int128 contrib =
        absl::int128(t.bound_diff.value()) * absl::int128(t.coeff.value());
    if (contrib < slack) {
      slack -= contrib;
      std::swap(terms[i], terms[--cover_size]);
    } else {
      ++i;
    }
  }
  DCHECK_GT(cover_size, 0);
  return cover_size;
}

template <class CompareAdd, class CompareRemove>
int CoverCutHelper::GetCoverSize(int relevant_size) {
  if (relevant_size == 0) return 0;
  absl::Span<CutTerm> terms = absl::MakeSpan(cut_.terms);

  // Take first all at variable at upper bound, and ignore the one at lower
  // bound.
  int part1 = 0;
  for (int i = 0; i < relevant_size;) {
    CutTerm& term = terms[i];
    const double dist = term.LpDistToMaxValue();
    if (dist < 1e-6) {
      // Move to part 1.
      std::swap(term, terms[part1]);
      ++i;
      ++part1;
    } else if (term.lp_value > 1e-6) {
      // Keep in part 2.
      ++i;
    } else {
      // Exclude entirely (part 3).
      --relevant_size;
      std::swap(term, terms[relevant_size]);
    }
  }
  std::sort(terms.begin() + part1, terms.begin() + relevant_size, CompareAdd());

  // We substract the initial rhs to avoid overflow.
  DCHECK_GE(cut_.rhs, 0);
  absl::int128 max_shifted_activity = -cut_.rhs;
  absl::int128 shifted_round_up = -cut_.rhs;
  int cover_size = 0;
  for (; cover_size < relevant_size; ++cover_size) {
    if (max_shifted_activity > 0) break;
    const CutTerm& term = terms[cover_size];
    max_shifted_activity += absl::int128(term.coeff.value()) *
                            absl::int128(term.bound_diff.value());
    shifted_round_up += absl::int128(term.coeff.value()) *
                        std::min(absl::int128(term.bound_diff.value()),
                                 absl::int128(std::ceil(term.lp_value - 1e-6)));
  }

  DCHECK_GE(cover_size, 0);
  if (shifted_round_up <= 0) {
    return 0;
  }
  return MinimizeCover<CompareRemove>(cover_size, max_shifted_activity);
}

// Try a simple cover heuristic.
// Look for violated CUT of the form: sum (UB - X) or (X - LB) >= 1.
int CoverCutHelper::GetCoverSizeForBooleans() {
  absl::Span<CutTerm> terms = absl::MakeSpan(cut_.terms);

  // Sorting can be slow, so we start by splitting the vector in 3 parts
  // - Can always be in cover
  // - Candidates that needs sorting
  // - At most one can be in cover (we keep the max).
  int part1 = 0;
  int relevant_size = terms.size();
  int best_in_part3 = -1;
  const double threshold = 1.0 - 1.0 / static_cast<double>(terms.size());
  for (int i = 0; i < relevant_size;) {
    const double lp_value = terms[i].lp_value;

    // Exclude non-Boolean.
    if (terms[i].bound_diff > 1) {
      --relevant_size;
      std::swap(terms[i], terms[relevant_size]);
      continue;
    }

    if (lp_value >= threshold) {
      // Move to part 1.
      std::swap(terms[i], terms[part1]);
      ++i;
      ++part1;
    } else if (lp_value > 0.5) {
      // Keep in part 2.
      ++i;
    } else {
      // Only keep the max (part 3).
      --relevant_size;
      std::swap(terms[i], terms[relevant_size]);

      if (best_in_part3 == -1 ||
          LargeLpValueFirst()(terms[relevant_size], terms[best_in_part3])) {
        best_in_part3 = relevant_size;
      }
    }
  }

  if (best_in_part3 != -1) {
    std::swap(terms[relevant_size], terms[best_in_part3]);
    ++relevant_size;
  }

  // Sort by decreasing Lp value.
  std::sort(terms.begin() + part1, terms.begin() + relevant_size,
            LargeLpValueFirst());

  double activity = 0.0;
  int cover_size = relevant_size;
  absl::int128 slack = -cut_.rhs;
  for (int i = 0; i < relevant_size; ++i) {
    const CutTerm& term = terms[i];
    activity += term.LpDistToMaxValue();

    // As an heuristic we select all the term so that the sum of distance
    // to the upper bound is <= 1.0. If the corresponding slack is positive,
    // then we will have a cut of violation at least 0.0. Note that this
    // violation can be improved by the lifting.
    //
    // TODO(user): experiment with different threshold (even greater than one).
    // Or come up with an algo that incorporate the lifting into the heuristic.
    if (activity > 0.9999) {
      cover_size = i;  // before this entry.
      break;
    }

    // TODO(user): Stop if we overflow int128 max! Note that because we scale
    // things so that the max coeff is 2^52, this is unlikely.
    slack += absl::int128(term.coeff.value()) *
             absl::int128(term.bound_diff.value());
  }

  // If the rhs is now negative, we have a cut.
  //
  // Note(user): past this point, now that a given "base" cover has been chosen,
  // we basically compute the cut (of the form sum X <= bound) with the maximum
  // possible violation. Note also that we lift as much as possible, so we don't
  // necessarily optimize for the cut efficacity though. But we do get a
  // stronger cut.
  if (slack <= 0) {
    return 0;
  }
  if (cover_size == 0) return 0;
  return MinimizeCover<LargeCoeffFirst>(cover_size, slack);
}

void CoverCutHelper::InitializeCut(const CutData& input_ct) {
  num_lifting_ = 0;
  cut_ = input_ct;

  // We should have dealt with an infeasible constraint before.
  // Note that because of our scaling, it is unlikely we will overflow int128.
  CHECK_GE(cut_.rhs, 0);
  DCHECK(cut_.AllCoefficientsArePositive());
}

bool CoverCutHelper::TrySimpleKnapsack(const CutData& input_ct,
                                       ImpliedBoundsProcessor* ib_processor) {
  InitializeCut(input_ct);

  // Tricky: This only work because the cut absl128 rhs is not changed by these
  // operations.
  if (ib_processor != nullptr) {
    std::vector<CutTerm>* new_bool_terms =
        ib_processor->ClearedMutableTempTerms();
    for (CutTerm& term : cut_.terms) {
      // We only look at non-Boolean with an lp value not close to the upper
      // bound.
      if (term.bound_diff <= 1) continue;
      if (term.lp_value + 1e-4 > AsDouble(term.bound_diff)) continue;

      if (ib_processor->TryToExpandWithLowerImpliedbound(
              IntegerValue(1),
              /*complement=*/false, &term, &cut_.rhs, new_bool_terms)) {
        ++cover_stats_.num_initial_ibs;
      }
    }

    ib_processor->MutableCutBuilder()->AddOrMergeBooleanTerms(
        absl::MakeSpan(*new_bool_terms), IntegerValue(1), &cut_);
  }

  bool has_relevant_int = false;
  for (const CutTerm& term : cut_.terms) {
    if (term.HasRelevantLpValue() && term.bound_diff > 1) {
      has_relevant_int = true;
      break;
    }
  }

  const int base_size = static_cast<int>(cut_.terms.size());
  const int cover_size =
      has_relevant_int
          ? GetCoverSize<LargeContribFirst, LargeCoeffFirst>(base_size)
          : GetCoverSizeForBooleans();
  if (!has_relevant_int && ib_processor == nullptr) {
    // If some implied bound substitution are possible, we do not cache anything
    // currently because the logic is currently sighlty different between the
    // two code. Fix?
    has_bool_base_ct_ = true;
    bool_cover_size_ = cover_size;
    if (cover_size == 0) return false;
    bool_base_ct_ = cut_;
  }
  if (cover_size == 0) return false;

  // The cut is just obtained by complementing the variable in the cover and
  // applying the MIR super additive function.
  //
  // Note that since all coeff in the cover will now be negative. If we do no
  // scaling, and if we use max_coeff_in_cover to construct f(), they will be
  // mapped by f() to -1 and we get the classical cover inequality. With scaling
  // we can get a strictly dominating cut though.
  //
  // TODO(user): we don't have to pick max_coeff_in_cover and could use the
  // coefficient of the most fractional variable. Or like in the MIR code try
  // a few of them. Currently, the cut in the test is worse if we don't take
  // the max_coeff_in_cover though, so we need more understanding.
  //
  // TODO(user): It seems we could use a more advanced lifting function
  // described later in the paper. Investigate.
  IntegerValue best_coeff = 0;
  double best_score = -1.0;
  IntegerValue max_coeff_in_cover(0);
  for (int i = 0; i < cover_size; ++i) {
    CutTerm& term = cut_.terms[i];
    max_coeff_in_cover = std::max(max_coeff_in_cover, term.coeff);
    const double score =
        std::abs(term.lp_value - std::floor(term.lp_value + 1e-6)) *
        ToDouble(term.coeff);
    if (score > best_score) {
      best_score = score;
      best_coeff = term.coeff;
    }
    term.Complement(&cut_.rhs);
  }
  CHECK_LT(cut_.rhs, 0);  // Because we complemented a cover.

  // TODO(user): Experiment without this line that basically disable scoring.
  best_coeff = max_coeff_in_cover;

  // TODO(user): experiment with different value of scaling and param t.
  std::function<IntegerValue(IntegerValue)> f;
  {
    IntegerValue max_magnitude = 0;
    for (const CutTerm& term : cut_.terms) {
      max_magnitude = std::max(max_magnitude, IntTypeAbs(term.coeff));
    }
    const IntegerValue max_scaling(std::min(
        IntegerValue(6000), FloorRatio(kMaxIntegerValue, max_magnitude)));
    const IntegerValue remainder = PositiveRemainder(cut_.rhs, best_coeff);
    f = GetSuperAdditiveRoundingFunction(remainder, best_coeff, IntegerValue(1),
                                         max_scaling);
  }

  if (ib_processor != nullptr) {
    const auto [num_lb, num_ub, num_merges] =
        ib_processor->PostprocessWithImpliedBound(f, /*factor_t=*/1, &cut_);
    cover_stats_.num_lb_ibs += num_lb;
    cover_stats_.num_ub_ibs += num_ub;
    cover_stats_.num_merges += num_merges;
  }

  cover_stats_.num_bumps += ApplyWithPotentialBump(f, best_coeff, &cut_);

  // Update counters.
  for (int i = cover_size; i < cut_.terms.size(); ++i) {
    if (cut_.terms[i].coeff != 0) ++num_lifting_;
  }
  cover_stats_.num_lifting += num_lifting_;
  ++cover_stats_.num_cuts;
  return true;
}

bool CoverCutHelper::TrySingleNodeFlow(const CutData& input_ct,
                                       ImpliedBoundsProcessor* ib_processor) {
  InitializeCut(input_ct);

  // TODO(user): Change the heuristic to depends on the lp_value of the implied
  // bounds. This way we can exactly match what happen in the old
  // FlowCoverCutHelper.
  const int base_size = static_cast<int>(cut_.terms.size());
  const int cover_size = GetCoverSize<KnapsackAdd, KnapsackRemove>(base_size);
  if (cover_size == 0) return false;

  // After complementing the term in the cover, we have
  // sum -ci.X + other_terms <= -slack;
  for (int i = 0; i < cover_size; ++i) {
    cut_.terms[i].Complement(&cut_.rhs);

    // We do not support complex terms, but we shouldn't get any.
    if (cut_.terms[i].expr_coeffs[1] != 0) return false;
  }

  // The algorithm goes as follow:
  // - Compute heuristically a minimal cover.
  // - We have sum_cover ci.Xi >= slack  where Xi is distance to upper bound.
  // - Apply coefficient strenghtening if ci > slack.
  //
  // Using implied bound we have two cases (all coeffs positive):
  // 1/ ci.Xi = ci.fi.Bi + ci.Si : always good.
  // 2/ ci.Xi = ci.di.Bi - ci.Si <= di.Bi: good if Si lp_value is zero.
  //
  // Note that if everything is Boolean, we just get a normal cover and coeff
  // strengthening just result in all coeff at 1, so worse than our cover
  // heuristic.
  CHECK_LT(cut_.rhs, 0);
  if (cut_.rhs <= absl::int128(std::numeric_limits<int64_t>::min())) {
    return false;
  }

  bool has_large_coeff = false;
  for (const CutTerm& term : cut_.terms) {
    if (IntTypeAbs(term.coeff) > 1'000'000) {
      has_large_coeff = true;
      break;
    }
  }

  // TODO(user): Shouldn't we just use rounding f() with maximum coeff to allows
  // lift of all other terms? but then except for the heuristic the cut is
  // really similar to the cover cut.
  const IntegerValue positive_rhs = -static_cast<int64_t>(cut_.rhs);
  IntegerValue min_magnitude = kMaxIntegerValue;
  for (int i = 0; i < cover_size; ++i) {
    const IntegerValue magnitude = IntTypeAbs(cut_.terms[i].coeff);
    min_magnitude = std::min(min_magnitude, magnitude);
  }
  const bool use_scaling =
      has_large_coeff || min_magnitude == 1 || min_magnitude >= positive_rhs;
  auto f = use_scaling ? GetSuperAdditiveStrengtheningMirFunction(
                             positive_rhs, /*scaling=*/6000)
                       : GetSuperAdditiveStrengtheningFunction(positive_rhs,
                                                               min_magnitude);

  if (ib_processor != nullptr) {
    const auto [num_lb, num_ub, num_merges] =
        ib_processor->PostprocessWithImpliedBound(f, /*factor_t=*/1, &cut_);
    flow_stats_.num_lb_ibs += num_lb;
    flow_stats_.num_ub_ibs += num_ub;
    flow_stats_.num_merges += num_merges;
  }

  // Lifting.
  {
    IntegerValue period = positive_rhs;
    for (const CutTerm& term : cut_.terms) {
      if (term.coeff > 0) continue;
      period = std::max(period, -term.coeff);
    }

    // Compute good period.
    // We don't want to extend it in the simpler case where f(x)=-1 if x < 0.
    //
    // TODO(user): If the Mir*() function is used, we don't need to extend that
    // much the period. Fix.
    if (f(-period + FloorRatio(period, 2)) != f(-period)) {
      // TODO(user): do exact binary search to find highest x in
      // [-max_neg_magnitude, 0] such that f(x) == f(-max_neg_magnitude) ? not
      // really needed though since we know that we have this equality:
      CHECK_EQ(f(-period), f(-positive_rhs));
      period = std::max(period, CapProdI(2, positive_rhs) - 1);
    }

    f = ExtendNegativeFunction(f, period);
  }

  // Generate the cut.
  cut_.rhs = absl::int128(f(-positive_rhs).value());
  for (CutTerm& term : cut_.terms) {
    const IntegerValue old_coeff = term.coeff;
    term.coeff = f(term.coeff);
    if (old_coeff > 0 && term.coeff != 0) ++flow_stats_.num_lifting;
  }
  ++flow_stats_.num_cuts;
  return true;
}

bool CoverCutHelper::TryWithLetchfordSouliLifting(
    const CutData& input_ct, ImpliedBoundsProcessor* ib_processor) {
  int cover_size;
  if (has_bool_base_ct_) {
    // We already called GetCoverSizeForBooleans() and ib_processor was nullptr,
    // so reuse that info.
    CHECK(ib_processor == nullptr);
    cover_size = bool_cover_size_;
    if (cover_size == 0) return false;
    InitializeCut(bool_base_ct_);
  } else {
    InitializeCut(input_ct);

    // Perform IB expansion with no restriction, all coeff should still be
    // positive.
    //
    // TODO(user): Merge Boolean terms that are complement of each other.
    if (ib_processor != nullptr) {
      std::vector<CutTerm>* new_bool_terms =
          ib_processor->ClearedMutableTempTerms();
      for (CutTerm& term : cut_.terms) {
        if (term.bound_diff <= 1) continue;
        if (ib_processor->TryToExpandWithLowerImpliedbound(
                IntegerValue(1),
                /*complement=*/false, &term, &cut_.rhs, new_bool_terms)) {
          ++ls_stats_.num_initial_ibs;
        }
      }

      ib_processor->MutableCutBuilder()->AddOrMergeBooleanTerms(
          absl::MakeSpan(*new_bool_terms), IntegerValue(1), &cut_);
    }

    // TODO(user): we currently only deal with Boolean in the cover. Fix.
    cover_size = GetCoverSizeForBooleans();
  }
  if (cover_size == 0) return false;

  // We don't support big rhs here.
  // Note however than since this only deal with Booleans, it is less likely.
  if (cut_.rhs > absl::int128(std::numeric_limits<int64_t>::max())) {
    ++ls_stats_.num_overflow_aborts;
    return false;
  }
  const IntegerValue rhs = static_cast<int64_t>(cut_.rhs);

  // Collect the weight in the cover.
  IntegerValue sum(0);
  std::vector<IntegerValue> cover_weights;
  for (int i = 0; i < cover_size; ++i) {
    CHECK_EQ(cut_.terms[i].bound_diff, 1);
    CHECK_GT(cut_.terms[i].coeff, 0);
    cover_weights.push_back(cut_.terms[i].coeff);
    sum = CapAddI(sum, cut_.terms[i].coeff);
  }
  if (AtMinOrMaxInt64(sum.value())) {
    ++ls_stats_.num_overflow_aborts;
    return false;
  }
  CHECK_GT(sum, rhs);

  // Compute the correct threshold so that if we round down larger weights to
  // p/q. We have sum of the weight in cover == base_rhs.
  IntegerValue p(0);
  IntegerValue q(0);
  IntegerValue previous_sum(0);
  std::sort(cover_weights.begin(), cover_weights.end());
  for (int i = 0; i < cover_size; ++i) {
    q = IntegerValue(cover_weights.size() - i);
    if (previous_sum + cover_weights[i] * q > rhs) {
      p = rhs - previous_sum;
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
      ++ls_stats_.num_overflow_aborts;
      return false;
    }
    thresholds.push_back(CeilRatio(p * (i + 1) + 1, q));
  }

  // For the other values, we just add the weights.
  std::reverse(cover_weights.begin(), cover_weights.end());
  for (int i = q.value(); i < cover_size; ++i) {
    thresholds.push_back(thresholds.back() + cover_weights[i]);
  }
  CHECK_EQ(thresholds.back(), rhs + 1);

  // Generate the cut.
  //
  // Our algo is quadratic in worst case, but large coefficients should be
  // rare, and in practice we don't really see this.
  //
  // Note that this work for non-Boolean since we can just "theorically" split
  // them as a sum of Booleans :) Probably a cleaner proof exist by just using
  // the super-additivity of the lifting function on [0, rhs].
  temp_cut_.rhs = cover_size - 1;
  temp_cut_.terms.clear();

  const int base_size = static_cast<int>(cut_.terms.size());
  for (int i = 0; i < base_size; ++i) {
    const CutTerm& term = cut_.terms[i];
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
      if (cut_coeff != 0 && i >= cover_size) ++ls_stats_.num_lifting;
      if (cut_coeff > 1 && i < cover_size) ++ls_stats_.num_lifting;  // happen?
    }

    temp_cut_.terms.push_back(term);
    temp_cut_.terms.back().coeff = cut_coeff;
  }

  cut_ = temp_cut_;
  ++ls_stats_.num_cuts;
  return true;
}

BoolRLTCutHelper::~BoolRLTCutHelper() {
  if (!VLOG_IS_ON(1)) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back({"bool_rlt/num_tried", num_tried_});
  stats.push_back({"bool_rlt/num_tried_factors", num_tried_factors_});
  shared_stats_->AddStats(stats);
}

void BoolRLTCutHelper::Initialize(
    const absl::flat_hash_map<IntegerVariable, glop::ColIndex>& lp_vars) {
  product_detector_->InitializeBooleanRLTCuts(lp_vars, *lp_values_);
  enabled_ = !product_detector_->BoolRLTCandidates().empty();
}

// TODO(user): do less work, add more stats.
bool BoolRLTCutHelper::TrySimpleSeparation(const CutData& input_ct) {
  if (!enabled_) return false;

  ++num_tried_;
  DCHECK(input_ct.AllCoefficientsArePositive());

  // We will list the interesting "factor" to try to multiply + linearize the
  // input constraint with.
  absl::flat_hash_set<IntegerVariable> to_try_set;
  std::vector<IntegerVariable> to_try;

  // We can look at the linearized factor * term and bound the activity delta
  // rescaled by 1 / factor.
  //
  // CASE          linearized_term       delta = term/factor - previous
  //
  // DROP,                             0                               0 - X
  // MC_CORMICK,  factor * ub - (ub - X)      (ub - X) * (1 - 1/factor) <= 0
  // SQUARE,                    factor=X                               1 - X
  // RLT,                     factor - P               1 - X - P/X  <= 1 - X
  //
  // TODO(user): detect earlier that a factor is not worth checking because
  // we already loose too much with the DROP/MC_CORMICK cases ? Filter more ?
  // I think we can probably evaluate the factor efficiency during the first
  // loop which usually have a small complexity compared to num_factor_to_try
  // times num filtered terms.
  filtered_input_.terms.clear();
  filtered_input_.rhs = input_ct.rhs;

  const auto& candidates = product_detector_->BoolRLTCandidates();
  for (const CutTerm& term : input_ct.terms) {
    // The only options are DROP or MC_CORMICK, but the later will unlikely win.
    //
    // TODO(user): we never use factor with lp value < 1e-4, but we could use a
    // factor equal to 1.0 I think. Double check.
    if (!term.IsBoolean() && term.lp_value <= 1e-6) {
      continue;
    }

    // Here the MC_CORMICK will not loose much. And SQUARE or RLT cannot win
    // much, so we can assume there is no loss and just look for violated
    // subconstraint.
    if (term.LpDistToMaxValue() <= 1e-6) {
      filtered_input_.rhs -= absl::int128(term.coeff.value()) *
                             absl::int128(term.bound_diff.value());
      continue;
    }

    // Convert to var or -var (to mean 1 - var).
    //
    // TODO(user): We could keep for each factor the max gain, so that we
    // can decided if it is not even worth trying a factor.
    const IntegerVariable var = term.GetUnderlyingLiteralOrNone();
    if (var != kNoIntegerVariable && candidates.contains(NegationOf(var))) {
      for (const IntegerVariable factor : candidates.at(NegationOf(var))) {
        if (to_try_set.insert(factor).second) to_try.push_back(factor);
      }
    }
    filtered_input_.terms.push_back(term);
  }

  // TODO(user): Avoid constructing the cut just to evaluate its efficacy.
  double best_efficacy = 1e-3;
  IntegerVariable best_factor = kNoIntegerVariable;
  for (const IntegerVariable factor : to_try) {
    ++num_tried_factors_;
    if (!TryProduct(factor, filtered_input_)) continue;
    const double efficacy = cut_.ComputeEfficacy();
    if (efficacy > best_efficacy) {
      best_efficacy = efficacy;
      best_factor = factor;
    }
  }

  // If we found a good factor, applies it to the non-filtered base constraint.
  if (best_factor != kNoIntegerVariable) {
    return TryProduct(best_factor, input_ct);
  }
  return false;
}

namespace {

// Each bool * term can be linearized in a couple of way.
// We will choose the best one.
enum class LinearizationOption {
  DROP,
  MC_CORMICK,
  RLT,
  SQUARE,
};

}  // namespace

double BoolRLTCutHelper::GetLiteralLpValue(IntegerVariable var) const {
  if (VariableIsPositive(var)) return (*lp_values_)[var];
  return 1.0 - (*lp_values_)[PositiveVariable(var)];
}

bool BoolRLTCutHelper::TryProduct(IntegerVariable factor,
                                  const CutData& input) {
  cut_.terms.clear();
  cut_.rhs = 0;
  absl::int128 old_rhs = input.rhs;

  const double factor_lp = GetLiteralLpValue(factor);

  // Consider each term in sequence and linearize them.
  for (CutTerm term : input.terms) {
    LinearizationOption best_option = LinearizationOption::DROP;

    // Recover the "IntegerVariable" literal if any.
    const IntegerVariable var = term.GetUnderlyingLiteralOrNone();
    const bool is_literal = var != kNoIntegerVariable;

    // First option is if factor == var.
    if (factor == var) {
      // The term can be left unchanged.
      // Note that we "win" (factor_lp - factor_lp ^ 2) * coeff activity.
      best_option = LinearizationOption::SQUARE;
      cut_.terms.push_back(term);
      continue;
    }

    // If factor == NegationOf(var) our best linearization is unfortunately
    // just product >= 0.
    if (NegationOf(factor) == var) {
      best_option = LinearizationOption::DROP;
      continue;
    }

    // TODO(user): If l implies x and y, we have x * y >= l.
    // We have to choose l as high as possible if multiple choices.

    // We start by the lp value for the drop option: simply dropping the term
    // since we know it is >= 0. We will choose the option with the highest
    // lp value, which is the one that "loose" the least activity.
    double best_lp = 0.0;

    // Second option, is complement it and use x * (1 - y) <= (1 - y):
    // x * [1 - (1 - y)] = x - x * (1 - y) >= x - (1 - y)
    const double complement_lp =
        static_cast<double>(term.bound_diff.value()) - term.lp_value;
    const double mc_cormick_lp = factor_lp - complement_lp;
    if (mc_cormick_lp > best_lp) {
      best_option = LinearizationOption::MC_CORMICK;
      best_lp = mc_cormick_lp;
    }

    // Last option is complement it and use a relation x * (1 - y) <= u.
    // so the lp is x - u. Note that this can be higher than x * y if the
    // bilinear relation is violated by the lp solution.
    if (is_literal) {
      // TODO(user): only consider variable within current lp.
      const IntegerVariable ub_lit =
          product_detector_->LiteralProductUpperBound(factor, NegationOf(var));
      if (ub_lit != kNoIntegerVariable) {
        const double lit_lp = GetLiteralLpValue(ub_lit);
        if (factor_lp - lit_lp > best_lp) {
          // We do it right away since we have all we need.
          best_option = LinearizationOption::RLT;

          // First complement to update rhs.
          term.Complement(&old_rhs);

          // Now we replace the term data.
          term.lp_value = lit_lp;
          term.ReplaceExpressionByLiteral(ub_lit);
          cut_.terms.push_back(term);
          continue;
        }
      }
    }

    if (best_option == LinearizationOption::DROP) continue;

    // We just keep the complemented term.
    CHECK(best_option == LinearizationOption::MC_CORMICK);
    term.Complement(&old_rhs);
    cut_.terms.push_back(term);
  }

  // Finally we add the - factor * old_rhs term.
  // We can only do that if the old_rhs is not too big.
  //
  // TODO(user): Avoid right away large constraint coming from gomory...
  const absl::int128 limit(int64_t{1} << 50);
  if (old_rhs > limit || old_rhs < -limit) return false;

  CutTerm term;
  term.coeff = -IntegerValue(static_cast<int64_t>(old_rhs));
  term.lp_value = factor_lp;
  term.bound_diff = 1;
  term.ReplaceExpressionByLiteral(factor);
  cut_.terms.push_back(term);

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

  result.generate_cuts = [z, x, y, linearization_level, model, trail,
                          integer_trail](LinearConstraintManager* manager) {
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
    if (CapProd(y_max_amp, std::abs(x.constant.value())) > kMaxSafeInteger) {
      return true;
    }
    if (CapProd(x_max_amp, std::abs(y.constant.value())) > kMaxSafeInteger) {
      return true;
    }

    const auto& lp_values = manager->LpValues();
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
        manager->AddCut(cut.Build(), "PositiveProduct");
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
        manager->AddCut(cut.Build(), "PositiveProduct");
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
  above_hyperplan.AddTerm(x, -above_slope);
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
  result.generate_cuts = [y, x, linearization_level, trail, integer_trail,
                          model](LinearConstraintManager* manager) {
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
                    "SquareUpper");

    const IntegerValue x_floor =
        static_cast<int64_t>(std::floor(x.LpValue(manager->LpValues())));
    manager->AddCut(ComputeHyperplanBelowSquare(x, y, x_floor, model),
                    "SquareLower");
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
    const util_intops::StrongVector<IntegerVariable, double>& lp_values) {
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
    const util_intops::StrongVector<IntegerVariable, double>& lp_values) {
  cache_.clear();
  for (const IntegerVariable var :
       implied_bounds_->VariablesWithImpliedBounds()) {
    if (!lp_vars_.contains(PositiveVariable(var))) continue;
    ComputeBestImpliedBound(var, lp_values);
  }
}

bool ImpliedBoundsProcessor::DecomposeWithImpliedLowerBound(
    const CutTerm& term, IntegerValue factor_t, CutTerm& bool_term,
    CutTerm& slack_term) {
  // We only want to expand non-Boolean and non-slack term!
  if (term.bound_diff <= 1) return false;
  if (!term.IsSimple()) return false;
  DCHECK_EQ(IntTypeAbs(term.expr_coeffs[0]), 1);

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
  if (term.cached_implied_lb < 0) return false;
  const BestImpliedBoundInfo info = cached_data_[term.cached_implied_lb];
  const IntegerValue lb = -term.expr_offset;
  const IntegerValue bound_diff = info.implied_bound - lb;
  if (bound_diff <= 0) return false;
  if (ProdOverflow(factor_t, CapProdI(term.coeff, bound_diff))) return false;

  // We have X/-X = info.diff * Boolean + slack.
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
  slack_term.expr_vars[0] = term.expr_vars[0];
  slack_term.expr_coeffs[0] = term.expr_coeffs[0];
  slack_term.expr_vars[1] = bool_term.expr_vars[0];
  slack_term.expr_coeffs[1] = -bound_diff * bool_term.expr_coeffs[0];
  slack_term.expr_offset =
      term.expr_offset - bound_diff * bool_term.expr_offset;

  slack_term.lp_value = info.SlackLpValue(lb);
  slack_term.coeff = term.coeff;
  slack_term.bound_diff = term.bound_diff;

  return true;
}

// We use the fact that calling DecomposeWithImpliedLowerBound() with
// term.Complement() give us almost what we want. You have
// -complement(X) = -diff.B - slack
// - (diff - X) = -diff.(1 -(1- B)) - slack
// X = diff.(1 - B) - slack;
bool ImpliedBoundsProcessor::DecomposeWithImpliedUpperBound(
    const CutTerm& term, IntegerValue factor_t, CutTerm& bool_term,
    CutTerm& slack_term) {
  absl::int128 unused = 0;
  CutTerm complement = term;
  complement.Complement(&unused);
  if (!DecomposeWithImpliedLowerBound(complement, factor_t, bool_term,
                                      slack_term)) {
    return false;
  }
  // This is required not to have a constant term which might mess up our cut
  // heuristics.
  if (IntTypeAbs(bool_term.coeff) !=
      CapProdI(IntTypeAbs(term.bound_diff), IntTypeAbs(term.coeff))) {
    return false;
  }
  bool_term.Complement(&unused);
  CHECK_EQ(unused, absl::int128(0));
  return true;
}

std::tuple<int, int, int> ImpliedBoundsProcessor::PostprocessWithImpliedBound(
    const std::function<IntegerValue(IntegerValue)>& f, IntegerValue factor_t,
    CutData* cut) {
  int num_applied_lb = 0;
  int num_applied_ub = 0;

  CutTerm bool_term;
  CutTerm slack_term;
  CutTerm ub_bool_term;
  CutTerm ub_slack_term;

  tmp_terms_.clear();
  for (CutTerm& term : cut->terms) {
    if (term.bound_diff <= 1) continue;
    if (!term.IsSimple()) continue;

    // Score is just the final lp value.
    // Higher is better since it is a <= constraint.
    double base_score;
    bool expand = false;
    if (DecomposeWithImpliedLowerBound(term, factor_t, bool_term, slack_term)) {
      // This side is always good.
      //   c.X = c.d.B + c.S
      // applying f to the result we have f(c.d).B + f(c).[X - d.B]
      // which give f(c).X + [f(c.d) - f(c).d].B
      // and the second term is always positive by super-additivity.
      expand = true;
      base_score = AsDouble(f(bool_term.coeff)) * bool_term.lp_value +
                   AsDouble(f(slack_term.coeff)) * slack_term.lp_value;
    } else {
      base_score = AsDouble(f(term.coeff)) * term.lp_value;
    }

    // Test if it is better to use this "bad" side.
    //
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
    if (DecomposeWithImpliedUpperBound(term, factor_t, ub_bool_term,
                                       ub_slack_term)) {
      const double score =
          AsDouble(f(ub_bool_term.coeff)) * ub_bool_term.lp_value +
          AsDouble(f(ub_slack_term.coeff)) * ub_slack_term.lp_value;
      // Note that because the slack is of the opposite sign, we might
      // loose more, so we prefer to be a bit defensive.
      if (score > base_score + 1e-2) {
        ++num_applied_ub;
        term = ub_slack_term;
        tmp_terms_.push_back(ub_bool_term);
        continue;
      }
    }

    if (expand) {
      ++num_applied_lb;
      term = slack_term;
      tmp_terms_.push_back(bool_term);
    }
  }

  const int num_merges = cut_builder_.AddOrMergeBooleanTerms(
      absl::MakeSpan(tmp_terms_), factor_t, cut);

  return {num_applied_lb, num_applied_ub, num_merges};
}

bool ImpliedBoundsProcessor::TryToExpandWithLowerImpliedbound(
    IntegerValue factor_t, bool complement, CutTerm* term, absl::int128* rhs,
    std::vector<CutTerm>* new_bool_terms) {
  CutTerm bool_term;
  CutTerm slack_term;
  if (!DecomposeWithImpliedLowerBound(*term, factor_t, bool_term, slack_term)) {
    return false;
  }

  // It should be good to use IB, but sometime we have things like
  // 7.3 = 2 * bool@1 + 5.3 and the expanded Boolean is at its upper bound.
  // It is always good to complement such variable.
  //
  // Note that here we do more and just complement anything closer to UB.
  if (complement) {
    if (bool_term.lp_value > 0.5) {
      bool_term.Complement(rhs);
    }
    if (slack_term.lp_value > 0.5 * AsDouble(slack_term.bound_diff)) {
      slack_term.Complement(rhs);
    }
  }

  *term = slack_term;
  new_bool_terms->push_back(bool_term);
  return true;
}

bool ImpliedBoundsProcessor::CacheDataForCut(IntegerVariable first_slack,
                                             CutData* cut) {
  cached_data_.clear();

  const int size = cut->terms.size();
  for (int i = 0; i < size; ++i) {
    const CutTerm& term = cut->terms[i];
    if (!term.IsSimple()) continue;
    if (term.IsBoolean()) continue;
    if (term.expr_vars[0] >= first_slack) continue;

    // Cache the BestImpliedBoundInfo if relevant.
    const IntegerVariable ib_var = term.expr_coeffs[0] > 0
                                       ? term.expr_vars[0]
                                       : NegationOf(term.expr_vars[0]);
    BestImpliedBoundInfo lb_info = GetCachedImpliedBoundInfo(ib_var);
    if (lb_info.bool_var != kNoIntegerVariable) {
      cut->terms[i].cached_implied_lb = cached_data_.size();
      cached_data_.emplace_back(std::move(lb_info));
    }
    BestImpliedBoundInfo ub_info =
        GetCachedImpliedBoundInfo(NegationOf(ib_var));
    if (ub_info.bool_var != kNoIntegerVariable) {
      cut->terms[i].cached_implied_ub = cached_data_.size();
      cached_data_.emplace_back(std::move(ub_info));
    }
  }

  return !cached_data_.empty();
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
    sum = CapAddI(sum, value);
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
    absl::Span<const std::pair<double, AffineExpression>> sorted_exprs_lp,
    const IntegerTrail& integer_trail,
    const util_intops::StrongVector<IntegerVariable, double>& lp_values,
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
    if (required_max_sum == std::numeric_limits<IntegerValue>::max()) continue;
    DCHECK_LE(required_min_sum, required_max_sum);
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
    absl::Span<const AffineExpression> exprs, Model* model) {
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
      [exprs = std::vector<AffineExpression>(exprs.begin(), exprs.end()),
       integer_trail, trail, model](LinearConstraintManager* manager) {
        // These cuts work at all levels but the generator adds too many cuts on
        // some instances and degrade the performance so we only use it at level
        // 0.
        if (trail->CurrentDecisionLevel() > 0) return true;
        const auto& lp_values = manager->LpValues();
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
        top_n_cuts.TransferToManager(manager);
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
    absl::Span<const IntegerVariable> x_vars,
    absl::Span<const LinearExpression> exprs,
    const util_intops::StrongVector<IntegerVariable, int>& variable_partition,
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
    const IntegerVariable xi_var, absl::Span<const IntegerVariable> z_vars,
    absl::Span<const LinearExpression> exprs,
    const util_intops::StrongVector<IntegerVariable, double>& lp_values,
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

CutGenerator CreateLinMaxCutGenerator(const IntegerVariable target,
                                      absl::Span<const LinearExpression> exprs,
                                      absl::Span<const IntegerVariable> z_vars,
                                      Model* model) {
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
      [x_vars,
       z_vars = std::vector<IntegerVariable>(z_vars.begin(), z_vars.end()),
       target, num_exprs,
       exprs = std::vector<LinearExpression>(exprs.begin(), exprs.end()),
       integer_trail, model](LinearConstraintManager* manager) {
        const auto& lp_values = manager->LpValues();
        util_intops::StrongVector<IntegerVariable, int> variable_partition(
            lp_values.size(), -1);
        util_intops::StrongVector<IntegerVariable, double>
            variable_partition_contrib(lp_values.size(),
                                       std::numeric_limits<double>::infinity());
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
          manager->AddCut(cut.Build(), "LinMax");
        }
        return true;
      };
  return result;
}

namespace {

IntegerValue EvaluateMaxAffine(
    absl::Span<const std::pair<IntegerValue, IntegerValue>> affines,
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
    absl::Span<const std::pair<IntegerValue, IntegerValue>> affines,
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
  if (ProdOverflow(delta_x, y_at_min) || ProdOverflow(delta_x, y_at_max) ||
      ProdOverflow(delta_y, x_min) || ProdOverflow(delta_y, x_max)) {
    return false;
  }

  // Checks target * delta_x for overflow.
  int64_t abs_magnitude = std::abs(target.offset.value());
  for (int i = 0; i < target.vars.size(); ++i) {
    const IntegerVariable var = target.vars[i];
    const IntegerValue var_min = integer_trail->LevelZeroLowerBound(var);
    const IntegerValue var_max = integer_trail->LevelZeroUpperBound(var);
    abs_magnitude = CapAdd(
        CapProd(std::max(std::abs(var_min.value()), std::abs(var_max.value())),
                std::abs(target.coeffs[i].value())),
        abs_magnitude);
  }
  if (AtMinOrMaxInt64(abs_magnitude) ||
      AtMinOrMaxInt64(CapProd(abs_magnitude, delta_x.value()))) {
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
  result.generate_cuts = [target, var, affines, cut_name, integer_trail,
                          model](LinearConstraintManager* manager) {
    if (integer_trail->IsFixed(var)) return true;
    LinearConstraintBuilder builder(model);
    if (BuildMaxAffineUpConstraint(target, var, affines, model, &builder)) {
      manager->AddCut(builder.Build(), cut_name);
    }
    return true;
  };
  return result;
}

CutGenerator CreateCliqueCutGenerator(
    absl::Span<const IntegerVariable> base_variables, Model* model) {
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
  result.only_run_at_level_zero = true;
  result.generate_cuts = [variables, literals, implication_graph, positive_map,
                          negative_map,
                          model](LinearConstraintManager* manager) {
    std::vector<double> packed_values;
    std::vector<double> packed_reduced_costs;
    const auto& lp_values = manager->LpValues();
    const auto& reduced_costs = manager->ReducedCosts();
    for (int i = 0; i < literals.size(); ++i) {
      packed_values.push_back(lp_values[variables[i]]);
      packed_reduced_costs.push_back(reduced_costs[variables[i]]);
    }
    const std::vector<std::vector<Literal>> at_most_ones =
        implication_graph->GenerateAtMostOnesWithLargeWeight(
            literals, packed_values, packed_reduced_costs);

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

      manager->AddCut(builder.Build(), "Clique");
    }
    return true;
  };
  return result;
}

}  // namespace sat
}  // namespace operations_research

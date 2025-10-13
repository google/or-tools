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

#ifndef OR_TOOLS_SAT_CUTS_H_
#define OR_TOOLS_SAT_CUTS_H_

#include <stdint.h>

#include <array>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/numeric/int128.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

// A "cut" generator on a set of IntegerVariable.
//
// The generate_cuts() function can get the current LP solution with
// manager->LpValues(). Note that a CutGenerator should:
// - Only look at the lp_values positions that corresponds to its 'vars' or
//   their negation.
// - Only add cuts in term of the same variables or their negation.
struct CutGenerator {
  bool only_run_at_level_zero = false;
  std::vector<IntegerVariable> vars;
  absl::AnyInvocable<bool(LinearConstraintManager* manager)> generate_cuts;
};

// To simplify cut generation code, we use a more complex data structure than
// just a LinearConstraint to represent a cut with shifted/complemented variable
// and implied bound substitution.
struct CutTerm {
  bool IsBoolean() const { return bound_diff == 1; }
  bool IsSimple() const { return expr_coeffs[1] == 0; }
  bool HasRelevantLpValue() const { return lp_value > 1e-2; }
  bool IsFractional() const {
    return std::abs(lp_value - std::round(lp_value)) > 1e-4;
  }
  double LpDistToMaxValue() const {
    return static_cast<double>(bound_diff.value()) - lp_value;
  }

  std::string DebugString() const;

  // Do the subtitution X -> (1 - X') and update the rhs.
  //
  // Our precondition on the sum of variable domains fitting an int64_t should
  // ensure that this can never overflow.
  void Complement(absl::int128* rhs);

  // This assumes bound_diff == 1. It replaces the inner expression by either
  // var or 1 - var depending on the positiveness of var.
  void ReplaceExpressionByLiteral(IntegerVariable var);

  // If the term correspond to literal_view or (1 - literal_view) return the
  // integer variable representation of that literal. Returns kNoIntegerVariable
  // if this is not the case.
  IntegerVariable GetUnderlyingLiteralOrNone() const;

  // Each term is of the form coeff * X where X is a variable with given
  // lp_value and with a domain in [0, bound_diff]. Note X is always >= 0.
  double lp_value = 0.0;
  IntegerValue coeff = IntegerValue(0);
  IntegerValue bound_diff = IntegerValue(0);

  // X = the given LinearExpression.
  // We only support size 1 or 2 here which allow to inline the memory.
  // When a coefficient is zero, we don't care about the variable.
  //
  // TODO(user): We might want to store that elsewhere, as sorting CutTerm is a
  // bit slow and we don't need to look at that in most places. Same for the
  // cached_implied_lb/ub below.
  IntegerValue expr_offset = IntegerValue(0);
  std::array<IntegerVariable, 2> expr_vars;
  std::array<IntegerValue, 2> expr_coeffs;

  // Refer to cached_data_ in ImpliedBoundsProcessor.
  int cached_implied_lb = -1;
  int cached_implied_ub = -1;
};

// Our cut are always of the form linear_expression <= rhs.
struct CutData {
  // We need level zero bounds and LP relaxation values to fill a CutData.
  // Returns false if we encounter any integer overflow.
  bool FillFromLinearConstraint(
      const LinearConstraint& base_ct,
      const util_intops::StrongVector<IntegerVariable, double>& lp_values,
      IntegerTrail* integer_trail);

  bool FillFromParallelVectors(IntegerValue ub,
                               absl::Span<const IntegerVariable> vars,
                               absl::Span<const IntegerValue> coeffs,
                               absl::Span<const double> lp_values,
                               absl::Span<const IntegerValue> lower_bounds,
                               absl::Span<const IntegerValue> upper_bounds);

  bool AppendOneTerm(IntegerVariable var, IntegerValue coeff, double lp_value,
                     IntegerValue lb, IntegerValue ub);

  // These functions transform the cut by complementation.
  bool AllCoefficientsArePositive() const;
  void ComplementForPositiveCoefficients();
  void ComplementForSmallerLpValues();

  // Computes and returns the cut violation.
  double ComputeViolation() const;
  double ComputeEfficacy() const;

  // This sorts terms by decreasing lp values and fills both
  // num_relevant_entries and max_magnitude.
  void SortRelevantEntries();

  std::string DebugString() const;

  // Note that we use a 128 bit rhs so we can freely complement variable without
  // running into overflow.
  absl::int128 rhs;
  std::vector<CutTerm> terms;

  // Only filled after SortRelevantEntries().
  IntegerValue max_magnitude;
  int num_relevant_entries;
};

// Stores temporaries used to build or manipulate a CutData.
class CutDataBuilder {
 public:
  // Returns false if we encounter an integer overflow.
  bool ConvertToLinearConstraint(const CutData& cut, LinearConstraint* output);

  // These function allow to merges entries corresponding to the same variable
  // and complementation. That is (X - lb) and (ub - X) are NOT merged and kept
  // as separate terms. Note that we currently only merge Booleans since this
  // is the only case we need.
  //
  // Return num_merges.
  int AddOrMergeBooleanTerms(absl::Span<CutTerm> terms, IntegerValue t,
                             CutData* cut);

 private:
  bool MergeIfPossible(IntegerValue t, CutTerm& to_add, CutTerm& target);

  absl::flat_hash_map<IntegerVariable, int> bool_index_;
  absl::flat_hash_map<IntegerVariable, int> secondary_bool_index_;
  absl::btree_map<IntegerVariable, IntegerValue> tmp_map_;
};

// Given an upper-bounded linear relation (sum terms <= ub), this algorithm
// inspects the integer variable appearing in the sum and try to replace each of
// them by a tight lower bound (>= coeff * binary + lb) using the implied bound
// repository. By tight, we mean that it will take the same value under the
// current LP solution.
//
// We use a class to reuse memory of the tmp terms.
class ImpliedBoundsProcessor {
 public:
  // We will only replace IntegerVariable appearing in lp_vars_.
  ImpliedBoundsProcessor(absl::Span<const IntegerVariable> lp_vars_,
                         IntegerTrail* integer_trail,
                         ImpliedBounds* implied_bounds)
      : lp_vars_(lp_vars_.begin(), lp_vars_.end()),
        integer_trail_(integer_trail),
        implied_bounds_(implied_bounds) {}

  // See if some of the implied bounds equation are violated and add them to
  // the IB cut pool if it is the case.
  //
  // Important: This must be called before we process any constraints with a
  // different lp_values or level zero bounds.
  void RecomputeCacheAndSeparateSomeImpliedBoundCuts(
      const util_intops::StrongVector<IntegerVariable, double>& lp_values);

  // This assumes the term is simple: expr[0] = var - LB / UB - var. We use an
  // implied lower bound on this expr, independently of the term.coeff sign.
  //
  // If possible, returns true and express X = bool_term + slack_term.
  // If coeff of X is positive, then all coeff will be positive here.
  bool DecomposeWithImpliedLowerBound(const CutTerm& term,
                                      IntegerValue factor_t, CutTerm& bool_term,
                                      CutTerm& slack_term);

  // This assumes the term is simple: expr[0] = var - LB / UB - var. We use
  // an implied upper bound on this expr, independently of term.coeff sign.
  //
  // If possible, returns true and express X = bool_term + slack_term.
  // If coeff of X is positive, then bool_term will have a positive coeff but
  // slack_term will have a negative one.
  bool DecomposeWithImpliedUpperBound(const CutTerm& term,
                                      IntegerValue factor_t, CutTerm& bool_term,
                                      CutTerm& slack_term);

  // We are about to apply the super-additive function f() to the CutData. Use
  // implied bound information to eventually substitute and make the cut
  // stronger. Returns the number of {lb_ib, ub_ib, merges} applied.
  //
  // This should lead to stronger cuts even if the norms migth be worse.
  std::tuple<int, int, int> PostprocessWithImpliedBound(
      const std::function<IntegerValue(IntegerValue)>& f, IntegerValue factor_t,
      CutData* cut);

  // Precomputes quantities used by all cut generation.
  // This allows to do that once rather than 6 times.
  // Return false if there are no exploitable implied bounds.
  bool CacheDataForCut(IntegerVariable first_slack, CutData* cut);

  bool TryToExpandWithLowerImpliedbound(IntegerValue factor_t, bool complement,
                                        CutTerm* term, absl::int128* rhs,
                                        std::vector<CutTerm>* new_bool_terms);

  // This can be used to share the hash-map memory.
  CutDataBuilder* MutableCutBuilder() { return &cut_builder_; }

  // This can be used as a temporary storage for
  // TryToExpandWithLowerImpliedbound().
  std::vector<CutTerm>* ClearedMutableTempTerms() {
    tmp_terms_.clear();
    return &tmp_terms_;
  }

  // Add a new variable that could be used in the new cuts.
  // Note that the cache must be computed to take this into account.
  void AddLpVariable(IntegerVariable var) { lp_vars_.insert(var); }

  // Once RecomputeCacheAndSeparateSomeImpliedBoundCuts() has been called,
  // we can get the best implied bound for each variables.
  //
  // Note that because the variable level zero lower bound might change since
  // the time this was cached, we just store the implied bound here.
  struct BestImpliedBoundInfo {
    double var_lp_value = 0.0;
    double bool_lp_value = 0.0;
    IntegerValue implied_bound;

    // When VariableIsPositive(bool_var) then it is when this is one that the
    // bound is implied. Otherwise it is when this is zero.
    IntegerVariable bool_var = kNoIntegerVariable;

    double SlackLpValue(IntegerValue lb) const {
      const double bool_term =
          static_cast<double>((implied_bound - lb).value()) * bool_lp_value;
      return var_lp_value - static_cast<double>(lb.value()) - bool_term;
    }

    std::string DebugString() const {
      return absl::StrCat("var - lb == (", implied_bound.value(),
                          " - lb) * bool(", bool_lp_value, ") + slack.");
    }
  };
  BestImpliedBoundInfo GetCachedImpliedBoundInfo(IntegerVariable var) const;

  // As we compute the best implied bounds for each variable, we add violated
  // cuts here.
  TopNCuts& IbCutPool() { return ib_cut_pool_; }

 private:
  BestImpliedBoundInfo ComputeBestImpliedBound(
      IntegerVariable var,
      const util_intops::StrongVector<IntegerVariable, double>& lp_values);

  absl::flat_hash_set<IntegerVariable> lp_vars_;
  mutable absl::flat_hash_map<IntegerVariable, BestImpliedBoundInfo> cache_;

  // Temporary data used by CacheDataForCut().
  std::vector<CutTerm> tmp_terms_;
  CutDataBuilder cut_builder_;
  std::vector<BestImpliedBoundInfo> cached_data_;

  TopNCuts ib_cut_pool_ = TopNCuts(50);

  // Data from the constructor.
  IntegerTrail* integer_trail_;
  ImpliedBounds* implied_bounds_;
};

// Visible for testing. Returns a function f on integers such that:
// - f is non-decreasing.
// - f is super-additive: f(a) + f(b) <= f(a + b)
// - 1 <= f(divisor) <= max_scaling
// - For all x, f(x * divisor) = x * f(divisor)
// - For all x, f(x * divisor + remainder) = x * f(divisor)
//
// Preconditions:
// - 0 <= remainder < divisor.
// - 1 <= max_scaling.
//
// This is used in IntegerRoundingCut() and is responsible for "strengthening"
// the cut. Just taking f(x) = x / divisor result in the non-strengthened cut
// and using any function that stricly dominate this one is better.
//
// Algorithm:
// - We first scale by a factor t so that rhs_remainder >= divisor / 2.
// - Then, if max_scaling == 2, we use the function described
//   in "Strenghtening Chvatal-Gomory cuts and Gomory fractional cuts", Adam N.
//   Letchfrod, Andrea Lodi.
// - Otherwise, we use a generalization of this which is a discretized version
//   of the classical MIR rounding function that only take the value of the
//   form "an_integer / max_scaling". As max_scaling goes to infinity, this
//   converge to the real-valued MIR function.
//
// Note that for each value of max_scaling we will get a different function.
// And that there is no dominance relation between any of these functions. So
// it could be nice to try to generate a cut using different values of
// max_scaling.
IntegerValue GetFactorT(IntegerValue rhs_remainder, IntegerValue divisor,
                        IntegerValue max_magnitude);
std::function<IntegerValue(IntegerValue)> GetSuperAdditiveRoundingFunction(
    IntegerValue rhs_remainder, IntegerValue divisor, IntegerValue t,
    IntegerValue max_scaling);

// If we have an equation sum ci.Xi >= rhs with everything positive, and all
// ci are >= min_magnitude then any ci >= rhs can be set to rhs. Also if
// some ci are in [rhs - min, rhs) then they can be strenghtened to rhs - min.
//
// If we apply this to the negated equation (sum -ci.Xi + sum cj.Xj <= -rhs)
// with potentially positive terms, this reduce to apply a super-additive
// function:
//
// Plot look like:
//            x=-rhs   x=0
//              |       |
// y=0 :        |       ---------------------------------
//              |    ---
//              |   /
//              |---
// y=-rhs -------
//
// TODO(user): Extend it for ci >= max_magnitude, we can probaly "lift" such
// coefficient.
std::function<IntegerValue(IntegerValue)> GetSuperAdditiveStrengtheningFunction(
    IntegerValue positive_rhs, IntegerValue min_magnitude);

// Similar to above but with scaling of the linear part to just have at most
// scaling values.
std::function<IntegerValue(IntegerValue)>
GetSuperAdditiveStrengtheningMirFunction(IntegerValue positive_rhs,
                                         IntegerValue scaling);

// Given a super-additive non-decreasing function f(), we periodically extend
// its restriction from [-period, 0] to Z. Such extension is not always
// super-additive and it is up to the caller to know when this is true or not.
inline std::function<IntegerValue(IntegerValue)> ExtendNegativeFunction(
    std::function<IntegerValue(IntegerValue)> base_f, IntegerValue period) {
  const IntegerValue m = -base_f(-period);
  return [m, period, base_f](IntegerValue v) {
    const IntegerValue r = PositiveRemainder(v, period);
    const IntegerValue output_r = m + base_f(r - period);
    return FloorRatio(v, period) * m + output_r;
  };
}

// Exploit AtMosteOne from the model to derive stronger cut.
// In the MIP community, using amo in this context is know as GUB (generalized
// upper bound).
class GUBHelper {
 public:
  explicit GUBHelper(Model* model)
      : implications_(model->GetOrCreate<BinaryImplicationGraph>()),
        integer_encoder_(model->GetOrCreate<IntegerEncoder>()),
        integer_trail_(model->GetOrCreate<IntegerTrail>()),
        shared_stats_(model->GetOrCreate<SharedStatistics>()) {}
  ~GUBHelper();

  void ApplyWithPotentialBumpAndGUB(
      const std::function<IntegerValue(IntegerValue)>& f, IntegerValue divisor,
      CutData* cut);

  // Stats on the last call to ApplyWithPotentialBumpAndGUB().
  int last_num_lifts() const { return last_num_lifts_; }
  int last_num_bumps() const { return last_num_bumps_; }
  int last_num_gubs() const { return last_num_gubs_; }

 private:
  BinaryImplicationGraph* implications_;
  IntegerEncoder* integer_encoder_;
  IntegerTrail* integer_trail_;
  SharedStatistics* shared_stats_;

  struct LiteralInCut {
    int index;
    Literal literal;
    IntegerValue original_coeff;
    bool complemented;
  };

  // One call stats.
  int last_num_lifts_;
  int last_num_bumps_;
  int last_num_gubs_;

  // Tmp data.
  std::vector<IntegerValue> initial_coeffs_;
  std::vector<LiteralInCut> literals_;
  absl::flat_hash_set<Literal> already_seen_;
  absl::flat_hash_map<int, int> amo_count_;

  // Stats.
  int64_t num_improvements_ = 0;
  int64_t num_todo_both_complements_ = 0;
};

// Given an upper bounded linear constraint, this function tries to transform it
// to a valid cut that violate the given LP solution using integer rounding.
// Note that the returned cut might not always violate the LP solution, in which
// case it can be discarded.
//
// What this does is basically take the integer division of the constraint by an
// integer. If the coefficients where doubles, this would be the same as scaling
// the constraint and then rounding. We choose the coefficient of the most
// fractional variable (rescaled by its coefficient) as the divisor, but there
// are other possible alternatives.
//
// Note that if the constraint is tight under the given lp solution, and if
// there is a unique variable not at one of its bounds and fractional, then we
// are guaranteed to generate a cut that violate the current LP solution. This
// should be the case for Chvatal-Gomory base constraints modulo our loss of
// precision while doing exact integer computations.
//
// Precondition:
// - We assumes that the given initial constraint is tight using the given lp
//   values. This could be relaxed, but for now it should always be the case, so
//   we log a message and abort if not, to ease debugging.
// - The IntegerVariable of the cuts are not used here. We assumes that the
//   first three vectors are in one to one correspondence with the initial order
//   of the variable in the cut.
//
// TODO(user): There is a bunch of heuristic involved here, and we could spend
// more effort tuning them. In particular, one can try many heuristics and keep
// the best looking cut (or more than one). This is not on the critical code
// path, so we can spend more effort in finding good cuts.
struct RoundingOptions {
  IntegerValue max_scaling = IntegerValue(60);
  bool use_ib_before_heuristic = true;
  bool prefer_positive_ib = true;
};
class IntegerRoundingCutHelper {
 public:
  explicit IntegerRoundingCutHelper(Model* model)
      : gub_helper_(model),
        shared_stats_(model->GetOrCreate<SharedStatistics>()) {}

  ~IntegerRoundingCutHelper();

  // Returns true on success. The cut can be accessed via cut().
  bool ComputeCut(RoundingOptions options, const CutData& base_ct,
                  ImpliedBoundsProcessor* ib_processor = nullptr);

  // If successful, info about the last generated cut.
  const CutData& cut() const { return cut_; }

  // Single line of text that we append to the cut log line.
  std::string Info() const { return absl::StrCat("ib_lift=", num_ib_used_); }

 private:
  double GetScaledViolation(IntegerValue divisor, IntegerValue max_scaling,
                            IntegerValue remainder_threshold,
                            const CutData& cut);

  GUBHelper gub_helper_;
  SharedStatistics* shared_stats_;

  // The helper is just here to reuse the memory for these vectors.
  std::vector<IntegerValue> divisors_;
  std::vector<IntegerValue> remainders_;
  std::vector<IntegerValue> rs_;
  std::vector<IntegerValue> best_rs_;

  int64_t num_ib_used_ = 0;
  CutData cut_;

  std::vector<std::pair<int, IntegerValue>> adjusted_coeffs_;
  std::vector<std::pair<int, IntegerValue>> best_adjusted_coeffs_;

  // Overall stats.
  int64_t total_num_dominating_f_ = 0;
  int64_t total_num_pos_lifts_ = 0;
  int64_t total_num_neg_lifts_ = 0;
  int64_t total_num_post_complements_ = 0;
  int64_t total_num_overflow_abort_ = 0;
  int64_t total_num_coeff_adjust_ = 0;
  int64_t total_num_merges_ = 0;
  int64_t total_num_bumps_ = 0;
  int64_t total_num_final_complements_ = 0;

  int64_t total_num_initial_ibs_ = 0;
  int64_t total_num_initial_merges_ = 0;
};

// Helper to find knapsack cover cuts.
class CoverCutHelper {
 public:
  explicit CoverCutHelper(Model* model)
      : gub_helper_(model),
        shared_stats_(model->GetOrCreate<SharedStatistics>()) {}

  ~CoverCutHelper();

  // Try to find a cut with a knapsack heuristic. This assumes an input with all
  // coefficients positive. If this returns true, you can get the cut via cut().
  //
  // This uses a lifting procedure similar to what is described in "Lifting the
  // Knapsack Cover Inequalities for the Knapsack Polytope", Adam N. Letchfod,
  // Georgia Souli. In particular the section "Lifting via mixed-integer
  // rounding".
  bool TrySimpleKnapsack(const CutData& input_ct,
                         ImpliedBoundsProcessor* ib_processor = nullptr);

  // Applies the lifting procedure described in "On Lifted Cover Inequalities: A
  // New Lifting Procedure with Unusual Properties", Adam N. Letchford, Georgia
  // Souli. This assumes an input with all coefficients positive.
  //
  // The algo is pretty simple, given a cover C for a given rhs. We compute
  // a rational weight p/q so that sum_C min(w_i, p/q) = rhs. Note that q is
  // pretty small (lower or equal to the size of C). The generated cut is then
  // of the form
  //  sum X_i in C for which w_i <= p / q
  //  + sum gamma_i X_i for the other variable  <= |C| - 1.
  //
  // gamma_i being the smallest k such that w_i <= sum of the k + 1 largest
  // min(w_i, p/q) for i in C. In particular, it is zero if w_i <= p/q.
  //
  // Note that this accept a general constraint that has been canonicalized to
  // sum coeff_i * X_i <= base_rhs. Each coeff_i >= 0 and each X_i >= 0.
  //
  // TODO(user): Generalize to non-Boolean, or use a different cover heuristic
  // for this:
  // - We want a Boolean only cover currently.
  // - We can always use implied bound for this, since there is more chance
  //   for a Bool only cover.
  // - Also, f() should be super additive on the value <= rhs, i.e. f(a + b) >=
  //   f(a) + f(b), so it is always good to use implied bounds of the form X =
  //   bound * B + Slack.
  bool TryWithLetchfordSouliLifting(
      const CutData& input_ct, ImpliedBoundsProcessor* ib_processor = nullptr);

  // It turns out that what FlowCoverCutHelper is doing is really just finding a
  // cover and generating a cut via coefficient strenghtening instead of MIR
  // rounding. This more generic version should just always outperform our old
  // code.
  bool TrySingleNodeFlow(const CutData& input_ct,
                         ImpliedBoundsProcessor* ib_processor = nullptr);

  // If successful, info about the last generated cut.
  const CutData& cut() const { return cut_; }

  // Single line of text that we append to the cut log line.
  std::string Info() const { return absl::StrCat("lift=", num_lifting_); }

  void ClearCache() { has_bool_base_ct_ = false; }

 private:
  void InitializeCut(const CutData& input_ct);

  // This looks at base_ct_ and reoder the terms so that the first ones are in
  // the cover. return zero if no interesting cover was found.
  template <class CompareAdd, class CompareRemove>
  int GetCoverSize(int relevant_size);

  // Same as GetCoverSize() but only look at Booleans, and use a different
  // heuristic.
  int GetCoverSizeForBooleans();

  template <class Compare>
  int MinimizeCover(int cover_size, absl::int128 slack);

  GUBHelper gub_helper_;
  SharedStatistics* shared_stats_;

  // Here to reuse memory, cut_ is both the input and the output.
  CutData cut_;
  CutData temp_cut_;

  // Hack to not sort twice.
  bool has_bool_base_ct_ = false;
  CutData bool_base_ct_;
  int bool_cover_size_ = 0;

  // Stats.
  int64_t num_lifting_ = 0;

  // Stats for the various type of cuts generated here.
  struct CutStats {
    int64_t num_cuts = 0;
    int64_t num_initial_ibs = 0;
    int64_t num_lb_ibs = 0;
    int64_t num_ub_ibs = 0;
    int64_t num_merges = 0;
    int64_t num_bumps = 0;
    int64_t num_lifting = 0;
    int64_t num_gubs = 0;
    int64_t num_overflow_aborts = 0;
  };
  CutStats flow_stats_;
  CutStats cover_stats_;
  CutStats ls_stats_;
};

// Separate RLT cuts.
//
// See for instance "Efficient Separation of RLT Cuts for Implicit and Explicit
// Bilinear Products", Ksenia Bestuzheva, Ambros Gleixner, Tobias Achterberg,
// https://arxiv.org/abs/2211.13545
class BoolRLTCutHelper {
 public:
  explicit BoolRLTCutHelper(Model* model)
      : product_detector_(model->GetOrCreate<ProductDetector>()),
        shared_stats_(model->GetOrCreate<SharedStatistics>()),
        lp_values_(model->GetOrCreate<ModelLpValues>()) {};
  ~BoolRLTCutHelper();

  // Precompute data according to the current lp relaxation.
  // This also restrict any Boolean to be currently appearing in the LP.
  void Initialize(absl::Span<const IntegerVariable> lp_vars);

  // Tries RLT separation of the input constraint. Returns true on success.
  bool TrySimpleSeparation(const CutData& input_ct);

  // If successful, this contains the last generated cut.
  const CutData& cut() const { return cut_; }

  // Single line of text that we append to the cut log line.
  std::string Info() const { return ""; }

 private:
  // LP value of a literal encoded as an IntegerVariable.
  // That is lit(X) = X if X positive or 1 - X otherwise.
  double GetLiteralLpValue(IntegerVariable var) const;

  // Multiplies input by lit(factor) and linearize in the best possible way.
  // The result will be stored in cut_.
  bool TryProduct(IntegerVariable factor, const CutData& input);

  bool enabled_ = false;
  CutData filtered_input_;
  CutData cut_;

  ProductDetector* product_detector_;
  SharedStatistics* shared_stats_;
  ModelLpValues* lp_values_;

  int64_t num_tried_ = 0;
  int64_t num_tried_factors_ = 0;
};

// A cut generator for z = x * y (x and y >= 0).
CutGenerator CreatePositiveMultiplicationCutGenerator(AffineExpression z,
                                                      AffineExpression x,
                                                      AffineExpression y,
                                                      int linearization_level,
                                                      Model* model);

// Above hyperplan for square = x * x: square should be below the line
//     (x_lb, x_lb ^ 2) to (x_ub, x_ub ^ 2).
// The slope of that line is (ub^2 - lb^2) / (ub - lb) = ub + lb.
//     square <= (x_lb + x_ub) * x - x_lb * x_ub
// This only works for positive x.
LinearConstraint ComputeHyperplanAboveSquare(AffineExpression x,
                                             AffineExpression square,
                                             IntegerValue x_lb,
                                             IntegerValue x_ub, Model* model);

// Below hyperplan for square = x * x: y should be above the line
//     (x_value, x_value ^ 2) to (x_value + 1, (x_value + 1) ^ 2)
// The slope of that line is 2 * x_value + 1
//     square >= below_slope * (x - x_value) + x_value ^ 2
//     square >= below_slope * x - x_value ^ 2 - x_value
LinearConstraint ComputeHyperplanBelowSquare(AffineExpression x,
                                             AffineExpression square,
                                             IntegerValue x_value,
                                             Model* model);

// A cut generator for y = x ^ 2 (x >= 0).
// It will dynamically add a linear inequality to push y closer to the parabola.
CutGenerator CreateSquareCutGenerator(AffineExpression y, AffineExpression x,
                                      int linearization_level, Model* model);

// A cut generator for all_diff(xi). Let the united domain of all xi be D. Sum
// of any k-sized subset of xi need to be greater or equal to the sum of
// smallest k values in D and lesser or equal to the sum of largest k values in
// D. The cut generator first sorts the variables based on LP values and adds
// cuts of the form described above if they are violated by lp solution. Note
// that all the fixed variables are ignored while generating cuts.
CutGenerator CreateAllDifferentCutGenerator(
    absl::Span<const AffineExpression> exprs, Model* model);

// Consider the Lin Max constraint with d expressions and n variables in the
// form: target = max {exprs[k] = Sum (wki * xi + bk)}. k in {1,..,d}.
//   Li = lower bound of xi
//   Ui = upper bound of xi.
// Let zk be in {0,1} for all k in {1,..,d}.
// The target = exprs[k] when zk = 1.
//
// The following is a valid linearization for Lin Max.
//   target >= exprs[k], for all k in {1,..,d}
//   target <= Sum (wli * xi) + Sum((Nlk + bk) * zk), for all l in {1,..,d}
// Where Nlk is a large number defined as:
//   Nlk = Sum (max((wki - wli)*Li, (wki - wli)*Ui))
//       = Sum (max corner difference for variable i, target expr l, max expr k)
//
// Consider a partition of variables xi into set {1,..,d} as I.
// i.e. I(i) = j means xi is mapped to jth index.
// The following inequality is valid and sharp cut for the lin max constraint
// described above.
//
// target <= Sum(i=1..n)(wI(i)i * xi + Sum(k=1..d)(MPlusCoefficient_ki * zk))
//           + Sum(k=1..d)(bk * zk) ,
// Where MPlusCoefficient_ki = max((wki - wI(i)i) * Li,
//                                 (wki - wI(i)i) * Ui)
//                           = max corner difference for variable i,
//                             target expr I(i), max expr k.
//
// For detailed proof of validity, refer
// Reference: "Strong mixed-integer programming formulations for trained neural
// networks" by Ross Anderson et. (https://arxiv.org/pdf/1811.01988.pdf).
//
// In the cut generator, we compute the most violated partition I by computing
// the rhs value (wI(i)i * lp_value(xi) + Sum(k=1..d)(MPlusCoefficient_ki * zk))
// for each variable for each partition index. We choose the partition index
// that gives lowest rhs value for a given variable.
//
// Note: This cut generator requires all expressions to contain only positive
// vars.
CutGenerator CreateLinMaxCutGenerator(IntegerVariable target,
                                      absl::Span<const LinearExpression> exprs,
                                      absl::Span<const IntegerVariable> z_vars,
                                      Model* model);

// Helper for the affine max constraint.
//
// This function will reset the bounds of the builder.
bool BuildMaxAffineUpConstraint(
    const LinearExpression& target, IntegerVariable var,
    absl::Span<const std::pair<IntegerValue, IntegerValue>> affines,
    Model* model, LinearConstraintBuilder* builder);

// By definition, the Max of affine functions is convex. The linear polytope is
// bounded by all affine functions on the bottom, and by a single hyperplane
// that join the two points at the extreme of the var domain, and their y-values
// of the max of the affine functions.
CutGenerator CreateMaxAffineCutGenerator(
    LinearExpression target, IntegerVariable var,
    std::vector<std::pair<IntegerValue, IntegerValue>> affines,
    std::string cut_name, Model* model);

// Extracts the variables that have a Literal view from base variables and
// create a generator that will returns constraint of the form "at_most_one"
// between such literals.
CutGenerator CreateCliqueCutGenerator(
    absl::Span<const IntegerVariable> base_variables, Model* model);

// Utility class for the AllDiff cut generator.
class SumOfAllDiffLowerBounder {
 public:
  void Clear();
  void Add(const AffineExpression& expr, int num_expr,
           const IntegerTrail& integer_trail);

  // Return int_max if the sum overflows.
  IntegerValue SumOfMinDomainValues();
  IntegerValue SumOfDifferentMins();
  IntegerValue GetBestLowerBound(std::string& suffix);

  int size() const { return expr_mins_.size(); }

 private:
  absl::btree_set<IntegerValue> min_values_;
  std::vector<IntegerValue> expr_mins_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CUTS_H_

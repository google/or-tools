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

#ifndef OR_TOOLS_SAT_CUTS_H_
#define OR_TOOLS_SAT_CUTS_H_

#include <array>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

// A "cut" generator on a set of IntegerVariable.
//
// The generate_cuts() function will usually be called with the current LP
// optimal solution (but should work for any lp_values). Note that a
// CutGenerator should:
// - Only look at the lp_values positions that corresponds to its 'vars' or
//   their negation.
// - Only add cuts in term of the same variables or their negation.
struct CutGenerator {
  bool only_run_at_level_zero = false;
  std::vector<IntegerVariable> vars;
  std::function<bool(
      const absl::StrongVector<IntegerVariable, double>& lp_values,
      LinearConstraintManager* manager)>
      generate_cuts;
};

// To simplify cut generation code, we use a more complex data structure than
// just a LinearConstraint to represent a cut with shifted/complemented variable
// and implied bound substitution.
struct CutTerm {
  bool IsBoolean() const { return bound_diff == 1; }
  bool IsSimple() const { return expr_coeffs[1] == 0; }
  bool HasRelevantLpValue() const { return lp_value > 1e-2; }
  double LpDistToMaxValue() const {
    return static_cast<double>(bound_diff.value()) - lp_value;
  }

  std::string DebugString() const;

  // Returns false and do nothing if this would cause an overflow.
  // Otherwise do the subtitution X -> (1 - X') and update the rhs.
  bool Complement(IntegerValue* rhs);

  // Each term is of the form coeff * X where X is a variable with given
  // lp_value and with a domain in [0, bound_diff]. Note X is always >= 0.
  double lp_value = 0.0;
  IntegerValue coeff = IntegerValue(0);
  IntegerValue bound_diff = IntegerValue(0);

  // X = the given LinearExpression.
  // We only support size 1 or 2 here which allow to inline the memory.
  // When a coefficient is zero, we don't care about the variable.
  IntegerValue expr_offset = IntegerValue(0);
  std::array<IntegerVariable, 2> expr_vars;
  std::array<IntegerValue, 2> expr_coeffs;
};

// Our cut are always of the form linear_expression <= rhs.
struct CutData {
  // We need level zero bounds and LP relaxation values to fill a CutData.
  // Returns false if we encounter any integer overflow.
  bool FillFromLinearConstraint(
      const LinearConstraint& base_ct,
      const absl::StrongVector<IntegerVariable, double>& lp_values,
      IntegerTrail* integer_trail);

  bool FillFromParallelVectors(const LinearConstraint& base_ct,
                               const std::vector<double>& lp_values,
                               const std::vector<IntegerValue>& lower_bounds,
                               const std::vector<IntegerValue>& upper_bounds);

  bool AppendOneTerm(IntegerVariable var, IntegerValue coeff, double lp_value,
                     IntegerValue lb, IntegerValue ub);

  IntegerValue rhs;
  std::vector<CutTerm> terms;

  // This sorts terms and fill both num_relevant_entries and max_magnitude.
  void Canonicalize();
  IntegerValue max_magnitude;
  int num_relevant_entries;
};

// Stores temporaries used to build or manipulate a CutData.
class CutDataBuilder {
 public:
  // These function allow to merges entries corresponding to the same variable
  // and complementation. That is (X - lb) and (ub - X) are NOT merged and kept
  // as separate terms. Note that we currently only merge Booleans since this
  // is the only case we need.
  void ClearIndices();
  void AddOrMergeTerm(const CutTerm& term, IntegerValue t, CutData* cut);
  int NumMergesSinceLastClear() const { return num_merges_; }

  // Returns false if we encounter an integer overflow.
  bool ConvertToLinearConstraint(const CutData& cut, LinearConstraint* output);

 private:
  void RegisterAllBooleansTerms(const CutData& cut);

  int num_merges_ = 0;
  bool constraint_is_indexed_ = false;
  absl::flat_hash_map<IntegerVariable, int> direct_index_;
  absl::flat_hash_map<IntegerVariable, int> complemented_index_;
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
      const absl::StrongVector<IntegerVariable, double>& lp_values);

  bool TryToExpandWithLowerImpliedbound(IntegerValue factor_t, int i,
                                        bool complement, CutData* cut,
                                        CutDataBuilder* builder);

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
    bool is_positive;
    IntegerValue implied_bound;
    IntegerVariable bool_var = kNoIntegerVariable;

    double SlackLpValue(IntegerValue lb) const {
      const double bool_term = ToDouble(implied_bound - lb) * bool_lp_value;
      return var_lp_value - ToDouble(lb) - bool_term;
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
      const absl::StrongVector<IntegerVariable, double>& lp_values);

  absl::flat_hash_set<IntegerVariable> lp_vars_;
  mutable absl::flat_hash_map<IntegerVariable, BestImpliedBoundInfo> cache_;

  TopNCuts ib_cut_pool_ = TopNCuts(50);

  // Data from the constructor.
  IntegerTrail* integer_trail_;
  ImpliedBounds* implied_bounds_;
};

// A single node flow relaxation is a constraint of the form
//     Sum in_flow - Sum out_flow <= demand
// where each flow variable F_i is in [0, capacity_i] and satisfy
//     F_i <= capacity_i B_i
// with B_i a Boolean representing the arc usage.
//
// From a generic constraint sum coeff_i X_i <= b, we try to put it in this
// format. We can first transform all variables to be in [0, max_value].
//
// Then we cover different cases:
// 1/ A coeff * Boolean, can be easily transformed.
// 2/ A coeff * Integer in [0, capacity] with Bool => integer == 0 too.
// 3/ For a general integer, we can always use a Bool == 1 for the arc usage.
//
// TODO(user): cover case 3/. We loose a lot of relaxation here, except if
// the variable is at is upper/lower bound.
//
// TODO(user): Altough the cut should still be correct, we might use the same
// Boolean more than once in the implied bound. Or this Boolean might already
// appear in the constraint. Not sure if we can do something smarter here.
struct FlowInfo {
  // Flow is always in [0, capacity] with the given current value in the
  // lp relaxation. Now that we usually only consider tight constraint were
  // flow_lp_value = capacity * bool_lp_value.
  IntegerValue capacity;
  double flow_lp_value;
  double bool_lp_value;

  // The definition of the flow variable and the arc usage variable in term
  // of original problem variables. After we compute a cut on the flow and
  // usage variable, we can just directly substitute these variable by the
  // expression here to have a cut in term of the original problem variables.
  AffineExpression flow_expr;
  AffineExpression bool_expr;
};

struct SingleNodeFlow {
  bool empty() const { return in_flow.empty() && out_flow.empty(); }
  void clear() {
    demand = IntegerValue(0);
    in_flow.clear();
    out_flow.clear();
    num_bool = 0;
    num_to_lb = 0;
    num_to_ub = 0;
  }
  std::string DebugString() const;

  IntegerValue demand;
  std::vector<FlowInfo> in_flow;
  std::vector<FlowInfo> out_flow;

  // Stats filled during extraction.
  int num_bool = 0;
  int num_to_lb = 0;
  int num_to_ub = 0;
};

class FlowCoverCutHelper {
 public:
  // Extract a SingleNodeFlow relaxation from the base_ct and try to generate
  // a cut from it.
  bool ComputeFlowCoverRelaxationAndGenerateCut(
      const LinearConstraint& base_ct,
      const absl::StrongVector<IntegerVariable, double>& lp_values,
      IntegerTrail* integer_trail, ImpliedBoundsProcessor* ib_helper);

  // Try to generate a cut for the given single node flow problem. Returns true
  // if a cut was generated. It can be accessed by cut().
  bool GenerateCut(const SingleNodeFlow& data);

  // If successful, info about the last generated cut.
  const LinearConstraint& cut() const { return cut_; }

  // Single line of text that we append to the cut log line.
  std::string Info() const {
    return absl::StrCat(" slack=", slack_.value(), " #in=", num_in_ignored_,
                        "|", num_in_flow_, "|", num_in_bin_,
                        " #out:", num_out_capa_, "|", num_out_flow_, "|",
                        num_out_bin_);
  }

 private:
  // Try to extract a nice SingleNodeFlow relaxation for the given upper bounded
  // linear constraint.
  bool ComputeFlowCoverRelaxation(
      const LinearConstraint& base_ct,
      const absl::StrongVector<IntegerVariable, double>& lp_values,
      SingleNodeFlow* snf, IntegerTrail* integer_trail,
      ImpliedBoundsProcessor* ib_helper);

  // Helpers used by ComputeFlowCoverRelaxation() to convert one linear term.
  bool TryXminusLB(IntegerVariable var, double lp_value, IntegerValue lb,
                   IntegerValue ub, IntegerValue coeff,
                   ImpliedBoundsProcessor* ib_helper,
                   SingleNodeFlow* result) const;
  bool TryUBminusX(IntegerVariable var, double lp_value, IntegerValue lb,
                   IntegerValue ub, IntegerValue coeff,
                   ImpliedBoundsProcessor* ib_helper,
                   SingleNodeFlow* result) const;

  // Temporary memory to avoid reallocating the vector.
  SingleNodeFlow snf_;

  // Stats, mainly to debug/investigate the code.
  IntegerValue slack_;
  int num_in_ignored_;
  int num_in_flow_;
  int num_in_bin_;
  int num_out_capa_;
  int num_out_flow_;
  int num_out_bin_;

  LinearConstraintBuilder cut_builder_;
  LinearConstraint cut_;
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
  ~IntegerRoundingCutHelper();

  // Returns true on success. The cut can be accessed via cut().
  bool ComputeCut(RoundingOptions options, const CutData& base_ct,
                  ImpliedBoundsProcessor* ib_processor = nullptr);

  // If successful, info about the last generated cut.
  const LinearConstraint& cut() const { return cut_; }

  void SetSharedStatistics(SharedStatistics* stats) { shared_stats_ = stats; }

  // Single line of text that we append to the cut log line.
  std::string Info() const { return absl::StrCat("ib_lift=", num_ib_used_); }

 private:
  bool HasComplementedImpliedBound(const CutTerm& entry,
                                   ImpliedBoundsProcessor* ib_processor);

  double GetScaledViolation(IntegerValue divisor, IntegerValue max_scaling,
                            IntegerValue remainder_threshold,
                            const CutData& cut);

  // The helper is just here to reuse the memory for these vectors.
  std::vector<IntegerValue> divisors_;
  std::vector<IntegerValue> remainders_;
  std::vector<IntegerValue> rs_;
  std::vector<IntegerValue> best_rs_;

  int64_t num_ib_used_ = 0;
  CutData best_cut_;
  CutDataBuilder cut_builder_;
  LinearConstraint cut_;

  std::vector<std::pair<int, IntegerValue>> adjusted_coeffs_;
  std::vector<std::pair<int, IntegerValue>> best_adjusted_coeffs_;

  // Overall stats.
  SharedStatistics* shared_stats_ = nullptr;
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
  ~CoverCutHelper();

  // Complements term to make sure all coeff are positive, returns false on
  // overflow.
  //
  // Important: This must be called on the input of both Try*() functions. It
  // is separated as an optimization to share the loop rather than do it in
  // both functions.
  bool MakeAllTermsPositive(CutData* cut);

  // Try to find a cut with a knapsack heuristic.
  // If this returns true, you can get the cut via cut().
  bool TrySimpleKnapsack(const CutData& input,
                         ImpliedBoundsProcessor* ib_processor = nullptr);

  // Applies the lifting procedure described in "On Lifted Cover Inequalities: A
  // New Lifting Procedure with Unusual Properties", Adam N. Letchford, Georgia
  // Souli.
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
      const CutData& input, ImpliedBoundsProcessor* ib_processor = nullptr);

  // If successful, info about the last generated cut.
  const LinearConstraint& cut() const { return cut_; }

  // Single line of text that we append to the cut log line.
  std::string Info() const { return absl::StrCat("lift=", num_lifting_); }

  void SetSharedStatistics(SharedStatistics* stats) { shared_stats_ = stats; }

 private:
  // This looks at base_ct_ and reoder the terms so that the first ones are in
  // the cover. return zero if no interesting cover was found.
  int GetCoverSize(int relevant_size, IntegerValue* rhs);

  // Here to reuse memory.
  CutData base_ct_;
  CutData temp_cut_;
  CutDataBuilder cut_builder_;

  // Stats.
  SharedStatistics* shared_stats_ = nullptr;
  int64_t num_lifting_ = 0;

  int64_t total_num_lifting_ = 0;
  int64_t total_num_ibs_ = 0;
  int64_t total_num_overflow_abort_ = 0;

  // Stores the cut for output.
  LinearConstraint cut_;
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
    const std::vector<AffineExpression>& exprs, Model* model);

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
CutGenerator CreateLinMaxCutGenerator(
    IntegerVariable target, const std::vector<LinearExpression>& exprs,
    const std::vector<IntegerVariable>& z_vars, Model* model);

// Helper for the affine max constraint.
//
// This function will reset the bounds of the builder.
bool BuildMaxAffineUpConstraint(
    const LinearExpression& target, IntegerVariable var,
    const std::vector<std::pair<IntegerValue, IntegerValue>>& affines,
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
    const std::vector<IntegerVariable>& base_variables, Model* model);

// Utility class for the AllDiff cut generator.
class SumOfAllDiffLowerBounder {
 public:
  void Clear();
  void Add(const AffineExpression& expr, int num_expr,
           const IntegerTrail& integer_trail);

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

// Copyright 2010-2018 Google LLC
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

#include <utility>
#include <vector>

#include "ortools/base/int_type.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/util/time_limit.h"

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
  std::vector<IntegerVariable> vars;
  std::function<void(
      const absl::StrongVector<IntegerVariable, double>& lp_values,
      LinearConstraintManager* manager)>
      generate_cuts;
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

  // Processes and updates the given cut.
  void ProcessUpperBoundedConstraint(
      const absl::StrongVector<IntegerVariable, double>& lp_values,
      LinearConstraint* cut);

  // Same as ProcessUpperBoundedConstraint() but instead of just using
  // var >= coeff * binary + lb we use var == slack + coeff * binary + lb where
  // slack is a new temporary variable that we create.
  //
  // The new slack will be such that slack_infos[(slack - first_slack) / 2]
  // contains its definition so that we can properly handle it in the cut
  // generation and substitute it back later.
  struct SlackInfo {
    // This slack is equal to sum of terms + offset.
    std::vector<std::pair<IntegerVariable, IntegerValue>> terms;
    IntegerValue offset;

    // The slack bounds and current lp_value.
    IntegerValue lb = IntegerValue(0);
    IntegerValue ub = IntegerValue(0);
    double lp_value = 0.0;
  };
  void ProcessUpperBoundedConstraintWithSlackCreation(
      bool substitute_only_inner_variables, IntegerVariable first_slack,
      const absl::StrongVector<IntegerVariable, double>& lp_values,
      LinearConstraint* cut, std::vector<SlackInfo>* slack_infos);

  // See if some of the implied bounds equation are violated and add them to
  // the IB cut pool if it is the case.
  void SeparateSomeImpliedBoundCuts(
      const absl::StrongVector<IntegerVariable, double>& lp_values);

  // Only used for debugging.
  //
  // Substituting back the slack created by the function above should give
  // exactly the same cut as the original one.
  bool DebugSlack(IntegerVariable first_slack,
                  const LinearConstraint& initial_cut,
                  const LinearConstraint& cut,
                  const std::vector<SlackInfo>& info);

  // Add a new variable that could be used in the new cuts.
  void AddLpVariable(IntegerVariable var) { lp_vars_.insert(var); }

  // Must be called before we process any constraints with a different
  // lp_values or level zero bounds.
  void ClearCache() const { cache_.clear(); }

  struct BestImpliedBoundInfo {
    double bool_lp_value = 0.0;
    double slack_lp_value = std::numeric_limits<double>::infinity();
    bool is_positive;
    IntegerValue bound_diff;
    IntegerVariable bool_var = kNoIntegerVariable;
  };
  BestImpliedBoundInfo GetCachedImpliedBoundInfo(IntegerVariable var);

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

  // Temporary memory used by ProcessUpperBoundedConstraint().
  mutable std::vector<std::pair<IntegerVariable, IntegerValue>> tmp_terms_;
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
                        IntegerValue max_t);
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
// more effort tunning them. In particular, one can try many heuristics and keep
// the best looking cut (or more than one). This is not on the critical code
// path, so we can spend more effort in finding good cuts.
struct RoundingOptions {
  IntegerValue max_scaling = IntegerValue(60);
};
class IntegerRoundingCutHelper {
 public:
  void ComputeCut(RoundingOptions options, const std::vector<double>& lp_values,
                  const std::vector<IntegerValue>& lower_bounds,
                  const std::vector<IntegerValue>& upper_bounds,
                  ImpliedBoundsProcessor* ib_processor, LinearConstraint* cut);

  // Returns the number of implied bound lifted Booleans in the last
  // ComputeCut() call. Useful for investigation.
  int NumLiftedBooleans() const { return num_lifted_booleans_; }

 private:
  // The helper is just here to reuse the memory for these vectors.
  std::vector<int> relevant_indices_;
  std::vector<double> relevant_lp_values_;
  std::vector<IntegerValue> relevant_coeffs_;
  std::vector<IntegerValue> relevant_bound_diffs_;
  std::vector<IntegerValue> divisors_;
  std::vector<std::pair<int, IntegerValue>> adjusted_coeffs_;
  std::vector<IntegerValue> remainders_;
  std::vector<bool> change_sign_at_postprocessing_;
  std::vector<IntegerValue> rs_;
  std::vector<IntegerValue> best_rs_;

  int num_lifted_booleans_ = 0;
  std::vector<std::pair<IntegerVariable, IntegerValue>> tmp_terms_;
};

// Helper to find knapsack or flow cover cuts (not yet implemented).
class CoverCutHelper {
 public:
  // Try to find a cut with a knapsack heuristic.
  // If this returns true, you can get the cut via cut().
  bool TrySimpleKnapsack(const LinearConstraint base_ct,
                         const std::vector<double>& lp_values,
                         const std::vector<IntegerValue>& lower_bounds,
                         const std::vector<IntegerValue>& upper_bounds);

  // If successful, info about the last generated cut.
  LinearConstraint* mutable_cut() { return &cut_; }
  const LinearConstraint& cut() const { return cut_; }

  // Single line of text that we append to the cut log line.
  const std::string Info() { return absl::StrCat("lift=", num_lifting_); }

 private:
  struct Term {
    int index;
    double dist_to_max_value;
    IntegerValue positive_coeff;  // abs(coeff in original constraint).
    IntegerValue diff;
  };
  std::vector<Term> terms_;
  std::vector<bool> in_cut_;

  LinearConstraint cut_;
  int num_lifting_;
};

// If a variable is away from its upper bound by more than value 1.0, then it
// cannot be part of a cover that will violate the lp solution. This method
// returns a reduced constraint by removing such variables from the given
// constraint.
LinearConstraint GetPreprocessedLinearConstraint(
    const LinearConstraint& constraint,
    const absl::StrongVector<IntegerVariable, double>& lp_values,
    const IntegerTrail& integer_trail);

// Returns true if sum of all the variables in the given constraint is less than
// or equal to constraint upper bound. This method assumes that all the
// coefficients are non negative.
bool ConstraintIsTriviallyTrue(const LinearConstraint& constraint,
                               const IntegerTrail& integer_trail);

// If the left variables in lp solution satisfies following inequality, we prove
// that there does not exist any knapsack cut which is violated by the solution.
// Let |Cmin| = smallest possible cover size.
// Let S = smallest (var_ub - lp_values[var]) first |Cmin| variables.
// Let cut lower bound = sum_(var in S)(var_ub - lp_values[var])
// For any cover,
// If cut lower bound >= 1
// ==> sum_(var in S)(var_ub - lp_values[var]) >= 1
// ==> sum_(var in cover)(var_ub - lp_values[var]) >= 1
// ==> The solution already satisfies cover. Since this is true for all covers,
// this method returns false in such cases.
// This method assumes that the constraint is preprocessed and has only non
// negative coefficients.
bool CanBeFilteredUsingCutLowerBound(
    const LinearConstraint& preprocessed_constraint,
    const absl::StrongVector<IntegerVariable, double>& lp_values,
    const IntegerTrail& integer_trail);

// Struct to help compute upper bound for knapsack instance.
struct KnapsackItem {
  double profit;
  double weight;
  bool operator>(const KnapsackItem& other) const {
    return profit * other.weight > other.profit * weight;
  }
};

// Gets upper bound on profit for knapsack instance by solving the linear
// relaxation.
double GetKnapsackUpperBound(std::vector<KnapsackItem> items, double capacity);

// Returns true if the linear relaxation upper bound for the knapsack instance
// shows that this constraint cannot be used to form a cut. This method assumes
// that all the coefficients are non negative.
bool CanBeFilteredUsingKnapsackUpperBound(
    const LinearConstraint& constraint,
    const absl::StrongVector<IntegerVariable, double>& lp_values,
    const IntegerTrail& integer_trail);

// Returns true if the given constraint passes all the filters described above.
// This method assumes that the constraint is preprocessed and has only non
// negative coefficients.
bool CanFormValidKnapsackCover(
    const LinearConstraint& preprocessed_constraint,
    const absl::StrongVector<IntegerVariable, double>& lp_values,
    const IntegerTrail& integer_trail);

// Converts the given constraint into canonical knapsack form (described
// below) and adds it to 'knapsack_constraints'.
// Canonical knapsack form:
//  - Constraint has finite upper bound.
//  - All coefficients are positive.
// For constraint with finite lower bound, this method also adds the negation of
// the given constraint after converting it to canonical knapsack form.
void ConvertToKnapsackForm(const LinearConstraint& constraint,
                           std::vector<LinearConstraint>* knapsack_constraints,
                           IntegerTrail* integer_trail);

// Returns true if the cut is lifted. Lifting procedure is described below.
//
// First we decide a lifting sequence for the binary variables which are not
// already in cut. We lift the cut for each lifting candidate one by one.
//
// Given the original constraint where the lifting candidate is fixed to one, we
// compute the maximum value the cut can take and still be feasible using a
// knapsack problem. We can then lift the variable in the cut using the
// difference between the cut upper bound and this maximum value.
bool LiftKnapsackCut(
    const LinearConstraint& constraint,
    const absl::StrongVector<IntegerVariable, double>& lp_values,
    const std::vector<IntegerValue>& cut_vars_original_coefficients,
    const IntegerTrail& integer_trail, TimeLimit* time_limit,
    LinearConstraint* cut);

// A cut generator that creates knpasack cover cuts.
//
// For a constraint of type
// \sum_{i=1..n}(a_i * x_i) <= b
// where x_i are integer variables with upper bound u_i, a cover of size k is a
// subset C of {1 , .. , n} such that \sum_{c \in C}(a_c * u_c) > b.
//
// A knapsack cover cut is a constraint of the form
// \sum_{c \in C}(u_c - x_c) >= 1
// which is equivalent to \sum_{c \in C}(x_c) <= \sum_{c \in C}(u_c) - 1.
// In other words, in a feasible solution, at least some of the variables do
// not take their maximum value.
//
// If all x_i are binary variables then the cover cut becomes
// \sum_{c \in C}(x_c) <= |C| - 1.
//
// The major difficulty for generating Knapsack cover cuts is finding a minimal
// cover set C that cut a given floating point solution. There are many ways to
// heuristically generate the cover but the following method that uses a
// solution of the LP relaxation of the constraint works the best.
//
// Look at a given linear relaxation solution for the integer problem x'
// and try to solve the following knapsack problem:
//   Minimize \sum_{i=1..n}(z_i * (u_i - x_i')),
//   such that \sum_{i=1..n}(a_i * u_i * z_i) > b,
// where z_i is a binary decision variable and x_i' are values of the variables
// in the given relaxation solution x'. If the objective of the optimal solution
// of this problem is less than 1, this algorithm does not generate any cuts.
// Otherwise, it adds a knapsack cover cut in the form
//   \sum_{i=1..n}(z_i' * x_i) <= cb,
// where z_i' is the value of z_i in the optimal solution of the above
// problem and cb is the upper bound for the cut constraint. Note that the above
// problem can be converted into a standard kanpsack form by replacing z_i by 1
// - y_i. In that case the problem becomes
//   Maximize \sum_{i=1..n}((u_i - x_i') * (y_i - 1)),
//   such that
//     \sum_{i=1..n}(a_i * u_i * y_i) <= \sum_{i=1..n}(a_i * u_i) - b - 1.
//
// Solving this knapsack instance would help us find the smallest cover with
// maximum LP violation.
//
// Cut strengthning:
// Let lambda = \sum_{c \in C}(a_c * u_c) - b and max_coeff = \max_{c
// \in C}(a_c), then cut can be strengthened as
//   \sum_{c \in C}(u_c - x_c) >= ceil(lambda / max_coeff)
//
// For further information about knapsack cover cuts see
// A. Atamtürk, Cover and Pack Inequalities for (Mixed) Integer Programming
// Annals of Operations Research Volume 139, Issue 1 , pp 21-38, 2005.
// TODO(user): Implement cut lifting.
CutGenerator CreateKnapsackCoverCutGenerator(
    const std::vector<LinearConstraint>& base_constraints,
    const std::vector<IntegerVariable>& vars, Model* model);

// A cut generator for z = x * y (x and y >= 0).
CutGenerator CreatePositiveMultiplicationCutGenerator(IntegerVariable z,
                                                      IntegerVariable x,
                                                      IntegerVariable y,
                                                      Model* model);

// A cut generator for y = x ^ 2 (x >= 0).
// It will dynamically add a linear inequality to push y closer to the parabola.
CutGenerator CreateSquareCutGenerator(IntegerVariable y, IntegerVariable x,
                                      Model* model);

// A cut generator for all_diff(xi). Let the united domain of all xi be D. Sum
// of any k-sized subset of xi need to be greater or equal to the sum of
// smallest k values in D and lesser or equal to the sum of largest k values in
// D. The cut generator first sorts the variables based on LP values and adds
// cuts of the form described above if they are violated by lp solution. Note
// that all the fixed variables are ignored while generating cuts.
CutGenerator CreateAllDifferentCutGenerator(
    const std::vector<IntegerVariable>& vars, Model* model);

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
    const IntegerVariable target, const std::vector<LinearExpression>& exprs,
    const std::vector<IntegerVariable>& z_vars, Model* model);

// For a given set of intervals and demands, we compute the maximum energy of
// each task and make sure it is less than the span of the intervals * its
// capacity.
//
// If an interval is optional, it contributes
//    min_demand * min_size * presence_literal
// amount of total energy.
//
// If an interval is performed, it contributes either min_demand * size or
// demand * min_size. We choose the most violated formulation.
//
// The maximum energy is capacity * span of intervals at level 0.
CutGenerator CreateCumulativeCutGenerator(
    const std::vector<IntervalVariable>& intervals,
    const IntegerVariable capacity, const std::vector<IntegerVariable>& demands,
    Model* model);

// For a given set of intervals and demands, we first compute the mandatory part
// of the interval as [start_max , end_min]. We use this to calculate mandatory
// demands for each start_max time points for eligible intervals.
// Since the sum of these mandatory demands must be smaller or equal to the
// capacity, we create a cut representing that.
//
// If an interval is optional, it contributes min_demand * presence_literal
// amount of demand to the mandatory demands sum. So the final cut is generated
// as follows:
//   sum(demands of always present intervals)
//   + sum(presence_literal * min_of_demand) <= capacity.
CutGenerator CreateOverlappingCumulativeCutGenerator(
    const std::vector<IntervalVariable>& intervals,
    const IntegerVariable capacity, const std::vector<IntegerVariable>& demands,
    Model* model);

// For a given set of intervals, we first compute the min and max of all
// intervals. Then we create a cut that indicates that all intervals must fit
// in that span.
//
// If an interval is optional, it contributes min_size * presence_literal
// amount of demand to the mandatory demands sum. So the final cut is generated
// as follows:
//   sum(sizes of always present intervals)
//   + sum(presence_literal * min_of_size) <= span of all intervals.
CutGenerator CreateNoOverlapCutGenerator(
    const std::vector<IntervalVariable>& intervals, Model* model);

// For a given set of intervals in a no_overlap constraint, we detect violated
// mandatory precedences and create a cut for these.
CutGenerator CreateNoOverlapPrecedenceCutGenerator(
    const std::vector<IntervalVariable>& intervals, Model* model);

// Extracts the variables that have a Literal view from base variables and
// create a generator that will returns constraint of the form "at_most_one"
// between such literals.
CutGenerator CreateCliqueCutGenerator(
    const std::vector<IntegerVariable>& base_variables, Model* model);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CUTS_H_

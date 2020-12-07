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

#ifndef OR_TOOLS_SAT_LINEAR_PROGRAMMING_CONSTRAINT_H_
#define OR_TOOLS_SAT_LINEAR_PROGRAMMING_CONSTRAINT_H_

#include <limits>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ortools/base/int_type.h"
#include "ortools/glop/revised_simplex.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_data_utils.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/sat/util.h"
#include "ortools/sat/zero_half_cuts.h"
#include "ortools/util/rev.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// Stores for each IntegerVariable its temporary LP solution.
//
// This is shared between all LinearProgrammingConstraint because in the corner
// case where we have many different LinearProgrammingConstraint and a lot of
// variable, we could theoretically use up a quadratic amount of memory
// otherwise.
//
// TODO(user): find a better way?
struct LinearProgrammingConstraintLpSolution
    : public absl::StrongVector<IntegerVariable, double> {
  LinearProgrammingConstraintLpSolution() {}
};

// Helper struct to combine info generated from solving LP.
struct LPSolveInfo {
  glop::ProblemStatus status;
  double lp_objective = -std::numeric_limits<double>::infinity();
  IntegerValue new_obj_bound = kMinIntegerValue;
};

// Simple class to combine linear expression efficiently. First in a sparse
// way that switch to dense when the number of non-zeros grows.
class ScatteredIntegerVector {
 public:
  // This must be called with the correct size before any other functions are
  // used.
  void ClearAndResize(int size);

  // Does vector[col] += value and return false in case of overflow.
  bool Add(glop::ColIndex col, IntegerValue value);

  // Similar to Add() but for multiplier * terms.
  // Returns false in case of overflow.
  bool AddLinearExpressionMultiple(
      IntegerValue multiplier,
      const std::vector<std::pair<glop::ColIndex, IntegerValue>>& terms);

  // This is not const only because non_zeros is sorted. Note that sorting the
  // non-zeros make the result deterministic whether or not we were in sparse
  // mode.
  //
  // TODO(user): Ideally we should convert to IntegerVariable as late as
  // possible. Prefer to use GetTerms().
  void ConvertToLinearConstraint(
      const std::vector<IntegerVariable>& integer_variables,
      IntegerValue upper_bound, LinearConstraint* result);

  // Similar to ConvertToLinearConstraint().
  std::vector<std::pair<glop::ColIndex, IntegerValue>> GetTerms();

  // We only provide the const [].
  IntegerValue operator[](glop::ColIndex col) const {
    return dense_vector_[col];
  }

 private:
  // If is_sparse is true we maintain the non_zeros positions and bool vector
  // of dense_vector_. Otherwise we don't. Note that we automatically switch
  // from sparse to dense as needed.
  bool is_sparse_ = true;
  std::vector<glop::ColIndex> non_zeros_;
  absl::StrongVector<glop::ColIndex, bool> is_zeros_;

  // The dense representation of the vector.
  absl::StrongVector<glop::ColIndex, IntegerValue> dense_vector_;
};

// A SAT constraint that enforces a set of linear inequality constraints on
// integer variables using an LP solver.
//
// The propagator uses glop's revised simplex for feasibility and propagation.
// It uses the Reduced Cost Strengthening technique, a classic in mixed integer
// programming, for instance see the thesis of Tobias Achterberg,
// "Constraint Integer Programming", sections 7.7 and 8.8, algorithm 7.11.
// http://nbn-resolving.de/urn:nbn:de:0297-zib-11129
//
// Per-constraint bounds propagation is NOT done by this constraint,
// it should be done by redundant constraints, as reduced cost propagation
// may miss some filtering.
//
// Note that this constraint works with double floating-point numbers, so one
// could be worried that it may filter too much in case of precision issues.
// However, by default, we interpret the LP result by recomputing everything
// in integer arithmetic, so we are exact.
class LinearProgrammingDispatcher;
class LinearProgrammingConstraint : public PropagatorInterface,
                                    ReversibleInterface {
 public:
  typedef glop::RowIndex ConstraintIndex;

  explicit LinearProgrammingConstraint(Model* model);
  ~LinearProgrammingConstraint() override;

  // Add a new linear constraint to this LP.
  void AddLinearConstraint(const LinearConstraint& ct);

  // Set the coefficient of the variable in the objective. Calling it twice will
  // overwrite the previous value.
  void SetObjectiveCoefficient(IntegerVariable ivar, IntegerValue coeff);

  // The main objective variable should be equal to the linear sum of
  // the arguments passed to SetObjectiveCoefficient().
  void SetMainObjectiveVariable(IntegerVariable ivar) { objective_cp_ = ivar; }

  // Register a new cut generator with this constraint.
  void AddCutGenerator(CutGenerator generator);

  // Returns the LP value and reduced cost of a variable in the current
  // solution. These functions should only be called when HasSolution() is true.
  //
  // Note that this solution is always an OPTIMAL solution of an LP above or
  // at the current decision level. We "erase" it when we backtrack over it.
  bool HasSolution() const { return lp_solution_is_set_; }
  double SolutionObjectiveValue() const { return lp_objective_; }
  double GetSolutionValue(IntegerVariable variable) const;
  double GetSolutionReducedCost(IntegerVariable variable) const;
  bool SolutionIsInteger() const { return lp_solution_is_integer_; }

  // PropagatorInterface API.
  bool Propagate() override;
  bool IncrementalPropagate(const std::vector<int>& watch_indices) override;
  void RegisterWith(Model* model);

  // ReversibleInterface API.
  void SetLevel(int level) override;

  int NumVariables() const { return integer_variables_.size(); }
  const std::vector<IntegerVariable>& integer_variables() const {
    return integer_variables_;
  }
  std::string DimensionString() const { return lp_data_.GetDimensionString(); }

  // Returns a IntegerLiteral guided by the underlying LP constraints.
  //
  // This looks at all unassigned 0-1 variables, takes the one with
  // a support value closest to 0.5, and tries to assign it to 1.
  // If all 0-1 variables have an integer support, returns kNoLiteralIndex.
  // Tie-breaking is done using the variable natural order.
  //
  // TODO(user): This fixes to 1, but for some problems fixing to 0
  // or to the std::round(support value) might work better. When this is the
  // case, change behaviour automatically?
  std::function<IntegerLiteral()> HeuristicLpMostInfeasibleBinary(Model* model);

  // Returns a IntegerLiteral guided by the underlying LP constraints.
  //
  // This computes the mean of reduced costs over successive calls,
  // and tries to fix the variable which has the highest reduced cost.
  // Tie-breaking is done using the variable natural order.
  // Only works for 0/1 variables.
  //
  // TODO(user): Try to get better pseudocosts than averaging every time
  // the heuristic is called. MIP solvers initialize this with strong branching,
  // then keep track of the pseudocosts when doing tree search. Also, this
  // version only branches on var >= 1 and keeps track of reduced costs from var
  // = 1 to var = 0. This works better than the conventional MIP where the
  // chosen variable will be argmax_var min(pseudocost_var(0->1),
  // pseudocost_var(1->0)), probably because we are doing DFS search where MIP
  // does BFS. This might depend on the model, more trials are necessary. We
  // could also do exponential smoothing instead of decaying every N calls, i.e.
  // pseudo = a * pseudo + (1-a) reduced.
  std::function<IntegerLiteral()> HeuristicLpReducedCostBinary(Model* model);

  // Returns a IntegerLiteral guided by the underlying LP constraints.
  //
  // This computes the mean of reduced costs over successive calls,
  // and tries to fix the variable which has the highest reduced cost.
  // Tie-breaking is done using the variable natural order.
  std::function<IntegerLiteral()> HeuristicLpReducedCostAverageBranching();

  // Average number of nonbasic variables with zero reduced costs.
  double average_degeneracy() const {
    return average_degeneracy_.CurrentAverage();
  }

  int64 total_num_simplex_iterations() const {
    return total_num_simplex_iterations_;
  }

 private:
  // Helper methods for branching. Returns true if branching on the given
  // variable helps with more propagation or finds a conflict.
  bool BranchOnVar(IntegerVariable var);
  LPSolveInfo SolveLpForBranching();

  // Helper method to fill reduced cost / dual ray reason in 'integer_reason'.
  // Generates a set of IntegerLiterals explaining why the best solution can not
  // be improved using reduced costs. This is used to generate explanations for
  // both infeasibility and bounds deductions.
  void FillReducedCostReasonIn(const glop::DenseRow& reduced_costs,
                               std::vector<IntegerLiteral>* integer_reason);

  // Reinitialize the LP from a potentially new set of constraints.
  // This fills all data structure and properly rescale the underlying LP.
  //
  // Returns false if the problem is UNSAT (it can happen when presolve is off
  // and some LP constraint are trivially false).
  bool CreateLpFromConstraintManager();

  // Solve the LP, returns false if something went wrong in the LP solver.
  bool SolveLp();

  // Add a "MIR" cut obtained by first taking the linear combination of the
  // row of the matrix according to "integer_multipliers" and then trying
  // some integer rounding heuristic.
  //
  // Return true if a new cut was added to the cut manager.
  bool AddCutFromConstraints(
      const std::string& name,
      const std::vector<std::pair<glop::RowIndex, IntegerValue>>&
          integer_multipliers);

  // Second half of AddCutFromConstraints().
  bool PostprocessAndAddCut(
      const std::string& name, const std::string& info,
      IntegerVariable first_new_var, IntegerVariable first_slack,
      const std::vector<ImpliedBoundsProcessor::SlackInfo>& ib_slack_infos,
      LinearConstraint* cut);

  // Computes and adds the corresponding type of cuts.
  // This can currently only be called at the root node.
  void AddCGCuts();
  void AddMirCuts();
  void AddZeroHalfCuts();

  // Updates the bounds of the LP variables from the CP bounds.
  void UpdateBoundsOfLpVariables();

  // Use the dual optimal lp values to compute an EXACT lower bound on the
  // objective. Fills its reason and perform reduced cost strenghtening.
  // Returns false in case of conflict.
  bool ExactLpReasonning();

  // Same as FillDualRayReason() but perform the computation EXACTLY. Returns
  // false in the case that the problem is not provably infeasible with exact
  // computations, true otherwise.
  bool FillExactDualRayReason();

  // Returns number of non basic variables with zero reduced costs.
  int64 CalculateDegeneracy();

  // From a set of row multipliers (at LP scale), scale them back to the CP
  // world and then make them integer (eventually multiplying them by a new
  // scaling factor returned in *scaling).
  //
  // Note that this will loose some precision, but our subsequent computation
  // will still be exact as it will work for any set of multiplier.
  std::vector<std::pair<glop::RowIndex, IntegerValue>> ScaleLpMultiplier(
      bool take_objective_into_account,
      const glop::DenseColumn& dense_lp_multipliers, glop::Fractional* scaling,
      int max_pow = 62) const;

  // Computes from an integer linear combination of the integer rows of the LP a
  // new constraint of the form "sum terms <= upper_bound". All computation are
  // exact here.
  //
  // Returns false if we encountered any integer overflow.
  bool ComputeNewLinearConstraint(
      const std::vector<std::pair<glop::RowIndex, IntegerValue>>&
          integer_multipliers,
      ScatteredIntegerVector* scattered_vector,
      IntegerValue* upper_bound) const;

  // Simple heuristic to try to minimize |upper_bound - ImpliedLB(terms)|. This
  // should make the new constraint tighter and correct a bit the imprecision
  // introduced by rounding the floating points values.
  void AdjustNewLinearConstraint(
      std::vector<std::pair<glop::RowIndex, IntegerValue>>* integer_multipliers,
      ScatteredIntegerVector* scattered_vector,
      IntegerValue* upper_bound) const;

  // Shortcut for an integer linear expression type.
  using LinearExpression = std::vector<std::pair<glop::ColIndex, IntegerValue>>;

  // Converts a dense represenation of a linear constraint to a sparse one
  // expressed in terms of IntegerVariable.
  void ConvertToLinearConstraint(
      const absl::StrongVector<glop::ColIndex, IntegerValue>& dense_vector,
      IntegerValue upper_bound, LinearConstraint* result);

  // Compute the implied lower bound of the given linear expression using the
  // current variable bound. Return kMinIntegerValue in case of overflow.
  IntegerValue GetImpliedLowerBound(const LinearConstraint& terms) const;

  // Tests for possible overflow in the propagation of the given linear
  // constraint.
  bool PossibleOverflow(const LinearConstraint& constraint);

  // Reduce the coefficient of the constraint so that we cannot have overflow
  // in the propagation of the given linear constraint. Note that we may loose
  // some strength by doing so.
  //
  // We make sure that any partial sum involving any variable value in their
  // domain do not exceed 2 ^ max_pow.
  void PreventOverflow(LinearConstraint* constraint, int max_pow = 62);

  // Fills integer_reason_ with the reason for the implied lower bound of the
  // given linear expression. We relax the reason if we have some slack.
  void SetImpliedLowerBoundReason(const LinearConstraint& terms,
                                  IntegerValue slack);

  // Fills the deductions vector with reduced cost deductions that can be made
  // from the current state of the LP solver. The given delta should be the
  // difference between the cp objective upper bound and lower bound given by
  // the lp.
  void ReducedCostStrengtheningDeductions(double cp_objective_delta);

  // Returns the variable value on the same scale as the CP variable value.
  glop::Fractional GetVariableValueAtCpScale(glop::ColIndex var);

  // Gets or creates an LP variable that mirrors a CP variable.
  // The variable should be a positive reference.
  glop::ColIndex GetOrCreateMirrorVariable(IntegerVariable positive_variable);

  // This must be called on an OPTIMAL LP and will update the data for
  // LPReducedCostAverageDecision().
  void UpdateAverageReducedCosts();

  // Callback underlying LPReducedCostAverageBranching().
  IntegerLiteral LPReducedCostAverageDecision();

  // Updates the simplex iteration limit for the next visit.
  // As per current algorithm, we use a limit which is dependent on size of the
  // problem and drop it significantly if degeneracy is detected. We use
  // DUAL_FEASIBLE status as a signal to correct the prediction. The next limit
  // is capped by 'min_iter' and 'max_iter'. Note that this is enabled only for
  // linearization level 2 and above.
  void UpdateSimplexIterationLimit(const int64 min_iter, const int64 max_iter);

  // This epsilon is related to the precision of the value/reduced_cost returned
  // by the LP once they have been scaled back into the CP domain. So for large
  // domain or cost coefficient, we may have some issues.
  static const double kCpEpsilon;

  // Same but at the LP scale.
  static const double kLpEpsilon;

  // Class responsible for managing all possible constraints that may be part
  // of the LP.
  LinearConstraintManager constraint_manager_;

  // Initial problem in integer form.
  // We always sort the inner vectors by increasing glop::ColIndex.
  struct LinearConstraintInternal {
    IntegerValue lb;
    IntegerValue ub;
    LinearExpression terms;
  };
  LinearExpression integer_objective_;
  IntegerValue integer_objective_offset_ = IntegerValue(0);
  IntegerValue objective_infinity_norm_ = IntegerValue(0);
  absl::StrongVector<glop::RowIndex, LinearConstraintInternal> integer_lp_;
  absl::StrongVector<glop::RowIndex, IntegerValue> infinity_norms_;

  // Underlying LP solver API.
  glop::LinearProgram lp_data_;
  glop::RevisedSimplex simplex_;
  int64 next_simplex_iter_ = 500;

  // For the scaling.
  glop::LpScalingHelper scaler_;

  // Temporary data for cuts.
  ZeroHalfCutHelper zero_half_cut_helper_;
  CoverCutHelper cover_cut_helper_;
  IntegerRoundingCutHelper integer_rounding_cut_helper_;
  LinearConstraint cut_;

  ScatteredIntegerVector tmp_scattered_vector_;

  std::vector<double> tmp_lp_values_;
  std::vector<IntegerValue> tmp_var_lbs_;
  std::vector<IntegerValue> tmp_var_ubs_;
  std::vector<glop::RowIndex> tmp_slack_rows_;
  std::vector<IntegerValue> tmp_slack_bounds_;

  // Structures used for mirroring IntegerVariables inside the underlying LP
  // solver: an integer variable var is mirrored by mirror_lp_variable_[var].
  // Note that these indices are dense in [0, mirror_lp_variable_.size()] so
  // they can be used as vector indices.
  //
  // TODO(user): This should be absl::StrongVector<glop::ColIndex,
  // IntegerVariable>.
  std::vector<IntegerVariable> integer_variables_;
  absl::flat_hash_map<IntegerVariable, glop::ColIndex> mirror_lp_variable_;

  // We need to remember what to optimize if an objective is given, because
  // then we will switch the objective between feasibility and optimization.
  bool objective_is_defined_ = false;
  IntegerVariable objective_cp_;

  // Singletons from Model.
  const SatParameters& sat_parameters_;
  Model* model_;
  TimeLimit* time_limit_;
  IntegerTrail* integer_trail_;
  Trail* trail_;
  IntegerEncoder* integer_encoder_;
  ModelRandomGenerator* random_;

  // Used while deriving cuts.
  ImpliedBoundsProcessor implied_bounds_processor_;

  // The dispatcher for all LP propagators of the model, allows to find which
  // LinearProgrammingConstraint has a given IntegerVariable.
  LinearProgrammingDispatcher* dispatcher_;

  std::vector<IntegerLiteral> integer_reason_;
  std::vector<IntegerLiteral> deductions_;
  std::vector<IntegerLiteral> deductions_reason_;

  // Repository of IntegerSumLE that needs to be kept around for the lazy
  // reasons. Those are new integer constraint that are created each time we
  // solve the LP to a dual-feasible solution. Propagating these constraints
  // both improve the objective lower bound but also perform reduced cost
  // fixing.
  int rev_optimal_constraints_size_ = 0;
  std::vector<std::unique_ptr<IntegerSumLE>> optimal_constraints_;

  // Last OPTIMAL solution found by a call to the underlying LP solver.
  // On IncrementalPropagate(), if the bound updates do not invalidate this
  // solution, Propagate() will not find domain reductions, no need to call it.
  int lp_solution_level_ = 0;
  bool lp_solution_is_set_ = false;
  bool lp_solution_is_integer_ = false;
  double lp_objective_;
  std::vector<double> lp_solution_;
  std::vector<double> lp_reduced_cost_;

  // If non-empty, this is the last known optimal lp solution at root-node. If
  // the variable bounds changed, or cuts where added, it is possible that this
  // solution is no longer optimal though.
  std::vector<double> level_zero_lp_solution_;

  // True if the last time we solved the exact same LP at level zero, no cuts
  // and no lazy constraints where added.
  bool lp_at_level_zero_is_final_ = false;

  // Same as lp_solution_ but this vector is indexed differently.
  LinearProgrammingConstraintLpSolution& expanded_lp_solution_;

  // Linear constraints cannot be created or modified after this is registered.
  bool lp_constraint_is_registered_ = false;

  std::vector<CutGenerator> cut_generators_;

  // Store some statistics for HeuristicLPReducedCostAverage().
  bool compute_reduced_cost_averages_ = false;
  int num_calls_since_reduced_cost_averages_reset_ = 0;
  std::vector<double> sum_cost_up_;
  std::vector<double> sum_cost_down_;
  std::vector<int> num_cost_up_;
  std::vector<int> num_cost_down_;
  std::vector<double> rc_scores_;

  // All the entries before rev_rc_start_ in the sorted positions correspond
  // to fixed variables and can be ignored.
  int rev_rc_start_ = 0;
  RevRepository<int> rc_rev_int_repository_;
  std::vector<std::pair<double, int>> positions_by_decreasing_rc_score_;

  // Defined as average number of nonbasic variables with zero reduced costs.
  IncrementalAverage average_degeneracy_;
  bool is_degenerate_ = false;

  // Used by the strong branching heuristic.
  int branching_frequency_ = 1;
  int64 count_since_last_branching_ = 0;

  // Sum of all simplex iterations performed by this class. This is useful to
  // test the incrementality and compare to other solvers.
  int64 total_num_simplex_iterations_ = 0;

  // Some stats on the LP statuses encountered.
  std::vector<int64> num_solves_by_status_;
};

// A class that stores which LP propagator is associated to each variable.
// We need to give the hash_map a name so it can be used as a singleton in our
// model.
//
// Important: only positive variable do appear here.
class LinearProgrammingDispatcher
    : public absl::flat_hash_map<IntegerVariable,
                                 LinearProgrammingConstraint*> {
 public:
  explicit LinearProgrammingDispatcher(Model* model) {}
};

// A class that stores the collection of all LP constraints in a model.
class LinearProgrammingConstraintCollection
    : public std::vector<LinearProgrammingConstraint*> {
 public:
  LinearProgrammingConstraintCollection() {}
};

// Cut generator for the circuit constraint, where in any feasible solution, the
// arcs that are present (variable at 1) must form a circuit through all the
// nodes of the graph. Self arc are forbidden in this case.
//
// In more generality, this currently enforce the resulting graph to be strongly
// connected. Note that we already assume basic constraint to be in the lp, so
// we do not add any cuts for components of size 1.
CutGenerator CreateStronglyConnectedGraphCutGenerator(
    int num_nodes, const std::vector<int>& tails, const std::vector<int>& heads,
    const std::vector<Literal>& literals, Model* model);

// Almost the same as CreateStronglyConnectedGraphCutGenerator() but for each
// components, computes the demand needed to serves it, and depending on whether
// it contains the depot (node zero) or not, compute the minimum number of
// vehicle that needs to cross the component border.
CutGenerator CreateCVRPCutGenerator(int num_nodes,
                                    const std::vector<int>& tails,
                                    const std::vector<int>& heads,
                                    const std::vector<Literal>& literals,
                                    const std::vector<int64>& demands,
                                    int64 capacity, Model* model);
}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_LINEAR_PROGRAMMING_CONSTRAINT_H_

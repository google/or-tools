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

#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ortools/base/int_type.h"
#include "ortools/glop/revised_simplex.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/matrix_scaler.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/sat/util.h"
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
    : public gtl::ITIVector<IntegerVariable, double> {
  LinearProgrammingConstraintLpSolution() {}
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

  // Returns a LiteralIndex guided by the underlying LP constraints.
  // This looks at all unassigned 0-1 variables, takes the one with
  // a support value closest to 0.5, and tries to assign it to 1.
  // If all 0-1 variables have an integer support, returns kNoLiteralIndex.
  // Tie-breaking is done using the variable natural order.
  //
  // TODO(user): This fixes to 1, but for some problems fixing to 0
  // or to the std::round(support value) might work better. When this is the
  // case, change behaviour automatically?
  std::function<LiteralIndex()> HeuristicLPMostInfeasibleBinary(Model* model);

  // Returns a LiteralIndex guided by the underlying LP constraints.
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
  std::function<LiteralIndex()> HeuristicLPPseudoCostBinary(Model* model);

  // Returns a LiteralIndex guided by the underlying LP constraints.
  // This computes the mean of reduced costs over successive calls,
  // and tries to fix the variable which has the highest reduced cost.
  // Tie-breaking is done using the variable natural order.
  std::function<LiteralIndex()> LPReducedCostAverageBranching();

  // Average number of nonbasic variables with zero reduced costs.
  double average_degeneracy() const {
    return average_degeneracy_.CurrentAverage();
  }

 private:
  // Reinitialize the LP from a potentially new set of constraints.
  // This fills all data structure and properly rescale the underlying LP.
  void CreateLpFromConstraintManager();

  // Solve the LP, returns false if something went wrong in the LP solver.
  bool SolveLp();

  // Add a "MIR" cut obtained by first taking the linear combination of the
  // row of the matrix according to "integer_multipliers" and then trying
  // some integer rounding heuristic.
  void AddCutFromConstraints(
      const std::string& name,
      const std::vector<std::pair<glop::RowIndex, IntegerValue>>&
          integer_multipliers);

  // Computes and adds Chvatal-Gomory cuts.
  // This can currently only be called at the root node.
  void AddCGCuts();

  // Computes and adds MIR cuts.
  // This can currently only be called at the root node.
  void AddMirCuts();

  // The factor to multiply a CP variable value to get the value in the LP side.
  glop::Fractional CpToLpScalingFactor(glop::ColIndex col) const;
  glop::Fractional LpToCpScalingFactor(glop::ColIndex col) const;

  // Updates the bounds of the LP variables from the CP bounds.
  void UpdateBoundsOfLpVariables();

  // Generates a set of IntegerLiterals explaining why the best solution can not
  // be improved using reduced costs. This is used to generate explanations for
  // both infeasibility and bounds deductions.
  void FillReducedCostsReason();

  // Use the dual optimal lp values to compute an EXACT lower bound on the
  // objective. Fills its reason and perform reduced cost strenghtening.
  // Returns false in case of conflict.
  bool ExactLpReasonning();

  // Same as FillReducedCostReason() but for the case of a DUAL_UNBOUNDED
  // problem. This exploit the dual ray as a reason for the primal infeasiblity.
  void FillDualRayReason();

  // Same as FillDualRayReason() but perform the computation EXACTLY. Returns
  // false in the case that the problem is not provably infeasible with exact
  // computations, true otherwise.
  bool FillExactDualRayReason();

  // Returns number of non basic variables with zero reduced costs.
  int64 CalculateDegeneracy() const;

  // From a set of row multipliers (at LP scale), scale them back to the CP
  // world and then make them integer (eventually multiplying them by a new
  // scaling factor returned in *scaling).
  //
  // Note that this will loose some precision, but our subsequent computation
  // will still be exact as it will work for any set of multiplier.
  std::vector<std::pair<glop::RowIndex, IntegerValue>> ScaleLpMultiplier(
      bool take_objective_into_account, bool use_constraint_status,
      const glop::DenseColumn& dense_lp_multipliers, glop::Fractional* scaling,
      int max_pow = 62) const;

  // Computes from an integer linear combination of the integer rows of the LP a
  // new constraint of the form "sum terms <= upper_bound". All computation are
  // exact here.
  //
  // Returns false if we encountered any integer overflow.
  bool ComputeNewLinearConstraint(
      bool use_constraint_status,
      const std::vector<std::pair<glop::RowIndex, IntegerValue>>&
          integer_multipliers,
      gtl::ITIVector<glop::ColIndex, IntegerValue>* dense_terms,
      IntegerValue* upper_bound) const;

  // Simple heuristic to try to minimize |upper_bound - ImpliedLB(terms)|. This
  // should make the new constraint tighter and correct a bit the imprecision
  // introduced by rounding the floating points values.
  void AdjustNewLinearConstraint(
      std::vector<std::pair<glop::RowIndex, IntegerValue>>* integer_multipliers,
      gtl::ITIVector<glop::ColIndex, IntegerValue>* dense_terms,
      IntegerValue* upper_bound) const;

  // Shortcut for an integer linear expression type.
  using LinearExpression = std::vector<std::pair<glop::ColIndex, IntegerValue>>;

  // Converts a dense represenation of a linear constraint to a sparse one
  // expressed in terms of IntegerVariable.
  LinearConstraint ConvertToLinearConstraint(
      const gtl::ITIVector<glop::ColIndex, IntegerValue>& dense_vector,
      IntegerValue upper_bound);

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

  // Callback underlying LPReducedCostAverageBranching().
  LiteralIndex LPReducedCostAverageDecision();

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
  IntegerValue objective_infinity_norm_ = IntegerValue(0);
  std::vector<LinearConstraintInternal> integer_lp_;
  gtl::ITIVector<glop::RowIndex, IntegerValue> infinity_norms_;

  // Underlying LP solver API.
  glop::LinearProgram lp_data_;
  glop::RevisedSimplex simplex_;

  // For the scaling.
  glop::SparseMatrixScaler scaler_;
  double bound_scaling_factor_;

  // Structures used for mirroring IntegerVariables inside the underlying LP
  // solver: an integer variable var is mirrored by mirror_lp_variable_[var].
  // Note that these indices are dense in [0, mirror_lp_variable_.size()] so
  // they can be used as vector indices.
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
  SearchHeuristicsVector* model_heuristics_;
  IntegerEncoder* integer_encoder_;

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

  // Defined as average number of nonbasic variables with zero reduced costs.
  IncrementalAverage average_degeneracy_;
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
    const std::vector<IntegerVariable>& vars);

// Almost the same as CreateStronglyConnectedGraphCutGenerator() but for each
// components, computes the demand needed to serves it, and depending on whether
// it contains the depot (node zero) or not, compute the minimum number of
// vehicle that needs to cross the component border.
CutGenerator CreateCVRPCutGenerator(int num_nodes,
                                    const std::vector<int>& tails,
                                    const std::vector<int>& heads,
                                    const std::vector<IntegerVariable>& vars,
                                    const std::vector<int64>& demands,
                                    int64 capacity);
}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_LINEAR_PROGRAMMING_CONSTRAINT_H_

// Copyright 2010-2017 Google
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

#include <unordered_map>
#include <utility>
#include <vector>

#include "ortools/base/int_type.h"
#include "ortools/glop/revised_simplex.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/matrix_scaler.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// One linear constraint on a set of Integer variables.
// There should be no duplicate variables.
struct LinearConstraint {
  double lb;
  double ub;
  std::vector<IntegerVariable> vars;
  std::vector<double> coeffs;
};

// A "cut" generator on a set of IntegerVariable. The generate_cuts() function
// will be called with the value of these variables in the current LP optimal
// solution and can return a list of extra constraints to add to the relaxation
// in terms of the same variables.
struct CutGenerator {
  std::vector<IntegerVariable> vars;
  std::function<std::vector<LinearConstraint>(
      const std::vector<double>& lp_solution)>
      generate_cuts;
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
// Workflow: create a LinearProgrammingConstraint instance, make linear
// inequality constraints, call RegisterWith() to finalize the set of linear
// constraints. A linear constraint a x + b y + c z <= k, with x y z
// IntegerVariables, can be created by calling:
// auto ct = lp->CreateNewConstraint(-std::numeric_limits<double>::infinity(),
//                                   k);
// lp->SetCoefficient(ct, x, a);
// lp->SetCoefficient(ct, y, b);
// lp->SetCoefficient(ct, z, c);
//
// Note that this constraint works with double floating-point numbers, so one
// could be worried that it may filter too much in case of precision issues.
// However, the underlying LP solver reports infeasibility only if the problem
// is still infeasible by relaxing the bounds by some small relative value.
// Thus the constraint will tend to filter less than it could, not the opposite.
class LinearProgrammingDispatcher;
class LinearProgrammingConstraint : public PropagatorInterface {
 public:
  typedef glop::RowIndex ConstraintIndex;

  explicit LinearProgrammingConstraint(Model* model);

  // User API, see header description.
  ConstraintIndex CreateNewConstraint(double lb, double ub);

  // TODO(user): Allow Literals to appear in linear constraints.
  // TODO(user): Calling SetCoefficient() twice on the same
  // (constraint, variable) pair will overwrite coefficients where accumulating
  // them might be desired, this is a common mistake, change API.
  void SetCoefficient(ConstraintIndex ct, IntegerVariable ivar,
                      double coefficient);

  // Set the coefficient of the variable in the objective. Calling it twice will
  // overwrite the previous value.
  void SetObjectiveCoefficient(IntegerVariable ivar, double coeff);

  // The main objective variable should be equal to the linear sum of
  // the arguments passed to SetObjectiveCoefficient().
  void SetMainObjectiveVariable(IntegerVariable ivar) { objective_cp_ = ivar; }

  // Register a new cut generator with this constraint.
  void AddCutGenerator(CutGenerator generator);

  // Returns the LP value and reduced cost of a variable in the current
  // solution.
  double GetSolutionValue(IntegerVariable variable) const;
  double GetSolutionReducedCost(IntegerVariable variable) const;

  // PropagatorInterface API.
  bool Propagate() override;
  bool IncrementalPropagate(const std::vector<int>& watch_indices) override;
  void RegisterWith(GenericLiteralWatcher* watcher);

  std::string DimensionString() const { return lp_data_.GetDimensionString(); }

 private:
  // Generates a set of IntegerLiterals explaining why the best solution can not
  // be improved using reduced costs. This is used to generate explanations for
  // both infeasibility and bounds deductions.
  void FillReducedCostsReason();

  // Same as FillReducedCostReason() but for the case of a DUAL_UNBOUNDED
  // problem. This exploit the dual ray as a reason for the primal infeasiblity.
  void FillDualRayReason();

  // Fills the deductions vector with reduced cost deductions that can be made
  // from the current state of the LP solver. The given delta should be the
  // difference between the cp objective upper bound and lower bound given by
  // the lp.
  void ReducedCostStrengtheningDeductions(double cp_objective_delta);

  // Gets or creates an LP variable that mirrors a CP variable.
  // The variable should be a positive reference.
  glop::ColIndex GetOrCreateMirrorVariable(IntegerVariable positive_variable);

  // Returns the variable value on the same scale as the CP variable value.
  glop::Fractional GetVariableValueAtCpScale(glop::ColIndex var);

  // TODO(user): use solver's precision epsilon.
  static const double kEpsilon;

  // Underlying LP solver API.
  glop::LinearProgram lp_data_;
  glop::RevisedSimplex simplex_;

  // For the scaling.
  glop::SparseMatrixScaler scaler_;

  // Structures used for mirroring IntegerVariables inside the underlying LP
  // solver: integer_variables_[i] is mirrored by mirror_lp_variables_[i],
  // both are used in IncrementalPropagate() and Propagate() calls;
  // integer_variable_to_index_ is used to find which mirroring variable's
  // coefficient must be modified on SetCoefficient().
  std::unordered_map<IntegerVariable, int> integer_variable_to_index_;
  std::vector<IntegerVariable> integer_variables_;
  std::vector<glop::ColIndex> mirror_lp_variables_;

  // We need to remember what to optimize if an objective is given, because
  // then we will switch the objective between feasibility and optimization.
  bool objective_is_defined_ = false;
  IntegerVariable objective_cp_;
  std::vector<std::pair<glop::ColIndex, double>> objective_lp_;

  // Structures for propagators.
  const SatParameters sat_parameters_;
  IntegerTrail* integer_trail_;
  Trail* trail_;
  std::vector<IntegerLiteral> integer_reason_;
  std::vector<IntegerLiteral> deductions_;

  // Last solution found by a call to the underlying LP solver.
  // On IncrementalPropagate(), if the bound updates do not invalidate this
  // solution, Propagate() will not find domain reductions, no need to call it.
  std::vector<double> lp_solution_;
  std::vector<double> lp_reduced_cost_;

  // Linear constraints cannot be created or modified after this is registered.
  bool lp_constraint_is_registered_ = false;

  // Time limit (shared with, owned by the sat solver).
  TimeLimit* time_limit_;

  // The dispatcher for all LP propagators of the model, allows to find which
  // LinearProgrammingConstraint has a given IntegerVariable.
  LinearProgrammingDispatcher* dispatcher_;

  int num_cuts_ = 0;
  std::vector<CutGenerator> cut_generators_;
};

// A class that stores which LP propagator is associated to each variable.
// We need to give the hash_map a name so it can be used as a singleton
// in our model.
class LinearProgrammingDispatcher
    : public std::unordered_map<IntegerVariable, LinearProgrammingConstraint*> {
 public:
  explicit LinearProgrammingDispatcher(Model* model) {}
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
//
// TODO(user): Try to get better pseudocosts than averaging every time the
// heuristic is called. MIP solvers initialize this with strong branching, then
// keep track of the pseudocosts when doing tree search. Also, this version only
// branches on var >= 1 and keeps track of reduced costs from var = 1 to var =
// 0. This works better than the conventional MIP where the chosen variable will
// be argmax_var std::min(pseudocost_var(0->1), pseudocost_var(1->0)), probably
// because we are doing DFS search where MIP does BFS. This might depend on the
// model, more trials are necessary. We could also do exponential smoothing
// instead of decaying every N calls, i.e. pseudo = a * pseudo + (1-a) reduced.
std::function<LiteralIndex()> HeuristicLPPseudoCostBinary(Model* model);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_LINEAR_PROGRAMMING_CONSTRAINT_H_

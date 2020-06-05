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

#ifndef OR_TOOLS_SAT_FEASIBILITY_PUMP_H_
#define OR_TOOLS_SAT_FEASIBILITY_PUMP_H_

#include "ortools/glop/revised_simplex.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_data_utils.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/cp_model_loader.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"

namespace operations_research {
namespace sat {

struct FeasibilityPumpLpSolution
    : public gtl::ITIVector<IntegerVariable, double> {
  FeasibilityPumpLpSolution() {}
};

class FeasibilityPump {
 public:
  explicit FeasibilityPump(Model* model);
  ~FeasibilityPump();

  typedef glop::RowIndex ConstraintIndex;

  void SetMaxFPIterations(int max_iter) {
    max_fp_iterations_ = std::max(1, max_iter);
  }

  // Add a new linear constraint to this LP.
  void AddLinearConstraint(const LinearConstraint& ct);

  // Set the coefficient of the variable in the objective. Calling it twice will
  // overwrite the previous value. Note that this doesn't set the objective
  // coefficient if the variable doesn't appear in any constraints. So this has
  // to be called after all the constraints are added.
  void SetObjectiveCoefficient(IntegerVariable ivar, IntegerValue coeff);

  // Returns the LP value of a variable in the current
  // solution. These functions should only be called when HasSolution() is true.
  bool HasLPSolution() const { return lp_solution_is_set_; }
  double LPSolutionObjectiveValue() const { return lp_objective_; }
  double GetLPSolutionValue(IntegerVariable variable) const;
  bool LPSolutionIsInteger() const { return lp_solution_is_integer_; }
  double LPSolutionFractionality() const { return lp_solution_fractionality_; }

  // Returns the Integer solution value of a variable in the current rounded
  // solution. These functions should only be called when HasIntegerSolution()
  // is true.
  bool HasIntegerSolution() const { return integer_solution_is_set_; }
  int64 IntegerSolutionObjectiveValue() const {
    return integer_solution_objective_;
  }
  bool IntegerSolutionIsFeasible() const {
    return integer_solution_is_feasible_;
  }
  int64 GetIntegerSolutionValue(IntegerVariable variable) const;

  void Solve();

 private:
  // Solve the LP, returns false if something went wrong in the LP solver.
  bool SolveLp();

  // Round the fractional LP solution values to nearest integer values.
  void SimpleRounding();

  // Loads the lp_data_.
  void InitializeWorkingLP();

  // Changes the LP objective and bounds of the norm constraints so the new
  // objective also tries to minimize the distance to the rounded solution.
  void L1DistanceMinimize();

  // Stores the solutions in the shared repository. Stores LP solution if it is
  // integer and stores the integer solution if it is feasible.
  void MaybePushToRepo();

  void PrintStats();

  // Shortcut for an integer linear expression type.
  using LinearExpression = std::vector<std::pair<glop::ColIndex, IntegerValue>>;

  // Gets or creates an LP variable that mirrors a model variable.
  // The variable should be a positive reference.
  glop::ColIndex GetOrCreateMirrorVariable(IntegerVariable positive_variable);

  // Updates the bounds of the LP variables from the CP bounds.
  void UpdateBoundsOfLpVariables();

  // This epsilon is related to the precision of the value returned by the LP
  // once they have been scaled back into the CP domain. So for large domain or
  // cost coefficient, we may have some issues.
  static const double kCpEpsilon;

  // Initial problem in integer form.
  // We always sort the inner vectors by increasing glop::ColIndex.
  struct LinearConstraintInternal {
    IntegerValue lb;
    IntegerValue ub;
    LinearExpression terms;
  };
  LinearExpression integer_objective_;
  IntegerValue objective_infinity_norm_ = IntegerValue(0);
  double objective_normalization_factor_ = 0.0;
  double mixing_factor_ = 1.0;

  gtl::ITIVector<glop::RowIndex, LinearConstraintInternal> integer_lp_;
  int model_vars_size_ = 0;

  // Underlying LP solver API.
  glop::LinearProgram lp_data_;
  glop::RevisedSimplex simplex_;

  glop::ColMapping norm_variables_;
  glop::ColToRowMapping norm_lhs_constraints_;
  glop::ColToRowMapping norm_rhs_constraints_;

  // Structures used for mirroring IntegerVariables inside the underlying LP
  // solver: an integer variable var is mirrored by mirror_lp_variable_[var].
  // Note that these indices are dense in [0, mirror_lp_variable_.size()] so
  // they can be used as vector indices.
  std::vector<IntegerVariable> integer_variables_;
  absl::flat_hash_map<IntegerVariable, glop::ColIndex> mirror_lp_variable_;

  // We need to remember what to optimize if an objective is given, because
  // then we will switch the objective between feasibility and optimization.
  bool objective_is_defined_ = false;

  // Singletons from Model.
  const SatParameters& sat_parameters_;
  TimeLimit* time_limit_;
  IntegerTrail* integer_trail_;
  Trail* trail_;
  IntegerEncoder* integer_encoder_;
  SharedIncompleteSolutionManager* incomplete_solutions_;
  const CpModelMapping* mapping_;

  // Last OPTIMAL/Feasible solution found by a call to the underlying LP solver.
  bool lp_solution_is_set_ = false;
  bool lp_solution_is_integer_ = false;
  double lp_objective_;
  std::vector<double> lp_solution_;
  std::vector<double> best_lp_solution_;
  // We use max fractionality of all variables.
  double lp_solution_fractionality_;

  // Rounded Integer solution. This might not be feasible.
  bool integer_solution_is_set_ = false;
  bool integer_solution_is_feasible_ = false;
  int64 integer_solution_objective_;
  std::vector<int64> integer_solution_;
  std::vector<int64> best_integer_solution_;
  int num_infeasible_constraints_;
  // We use max infeasibility of all constraints.
  int64 integer_solution_infeasibility_;

  // Sum of all simplex iterations performed by this class. This is useful to
  // test the incrementality and compare to other solvers.
  int64 total_num_simplex_iterations_ = 0;

  // TODO(user): Tune default value. Expose as parameter.
  int max_fp_iterations_ = 20;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_FEASIBILITY_PUMP_H_

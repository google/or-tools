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

#ifndef ORTOOLS_LINEAR_SOLVER_PROTO_SOLVER_PREPROCESSOR_H_
#define ORTOOLS_LINEAR_SOLVER_PROTO_SOLVER_PREPROCESSOR_H_

#include <cmath>
#include <functional>
#include <vector>

#include "absl/types/span.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/preprocessor.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/mp_model_wrapper.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/logging.h"

namespace operations_research {

// --------------------------------------------------------
// ImpliedIntegerPreprocessor
// --------------------------------------------------------

// In this preprocessor, we find continuous variables which can only take
// integer values and mark them as integer variables.
//
// There are two methods for detecting implied integer variables: 1) primal
// and 2) dual detection. If the variable appears in at least one equality
// constraint then we use primal detection otherwise we use dual detection.
class ImpliedIntegerPreprocessor : public glop::Preprocessor {
 public:
  explicit ImpliedIntegerPreprocessor(
      const glop::GlopParameters* parameters,
      glop::Fractional integer_solution_tolerance)
      : glop::Preprocessor(parameters),
        integer_solution_tolerance_(integer_solution_tolerance) {}

  ImpliedIntegerPreprocessor(const ImpliedIntegerPreprocessor&) = delete;
  ImpliedIntegerPreprocessor& operator=(const ImpliedIntegerPreprocessor&) =
      delete;
  ~ImpliedIntegerPreprocessor() override = default;

  // TODO(user): When some variable are detected to be implied integer, other
  // can in turn be detected as such. Change the code to reach a fixed point.
  // Calling this multiple time has a similar effect, but is a lot less
  // efficient and can require O(num_variables) calls to reach the fix point.
  Result Run(glop::LinearProgram* linear_program) override;

 private:
  void RecoverSolutionImpl(glop::SolveStatus&,
                           glop::ProblemSolution*) const final {}

  // Returns true if the given variable is implied integer. This method is used
  // for continuous variables appearing in at least one equality constraint.
  // This is sometimes called "primal" detection in the literature.
  //
  // For each equality constraint s in which the given continuous variable x_j
  // appears, this method checks the primal detection criteria by using
  // ConstraintSupportsImpliedIntegrality() method.
  bool AnyEqualityConstraintImpliesIntegrality(
      const glop::LinearProgram& linear_program, glop::ColIndex variable);

  // Returns true if given variable is implied integer variable. This method is
  // used for continuous variables for which primal detection is not applicable
  // i.e. all constraints containing the given variable are inequalities. This
  // is sometimes called "dual" detection in the literature.
  //
  // This method checks the following for the givan continuous variable x_j.
  // a) The lower and upper bound of x_j are integers or the variable has no
  //    cost and its domain contains an integer point.
  // b) For all constraint containing x_j, when treated as equality under primal
  //    detection, implies x_j as integer variable.
  // If both conditions are satisfied then the variable x_j is implied integer
  // variable.
  bool AllInequalityConstraintsImplyIntegrality(
      const glop::LinearProgram& linear_program, glop::ColIndex variable);

  // Returns true if the following conditions are satisfied.
  //
  // Let the constraint be lb_s <= \sum_{i=1..n}(a_si*x_i) + a_sj*x_j <= ub_s
  // a) lb_s / a_sj and ub_s / a_sj are integers.
  // b) a_si / a_sj is integer for all i.
  // c) x_i are all integer variables.
  bool ConstraintSupportsImpliedIntegrality(
      const glop::LinearProgram& linear_program, glop::ColIndex variable,
      glop::RowIndex row);

  // Returns true if the variable occurs in at least one equality constraint.
  bool VariableOccursInAtLeastOneEqualityConstraint(
      const glop::LinearProgram& linear_program, glop::ColIndex variable);

 private:
  const glop::Fractional integer_solution_tolerance_;
};

// --------------------------------------------------------
// ReduceCostOverExclusiveOrConstraintPreprocessor
// --------------------------------------------------------

// For an "exclusive or" constraint (sum Boolean = 1), if all the costs of the
// Boolean variables are positive, then we can subtract the cost of the one
// with minimum cost from the cost of all the others. We can do that for all
// such constraints one by one.
//
// ex: if x,y,z are Boolean variables with respective cost 1,2,1 and x+y+z=1,
// then we can change their costs to 0,1,0 and add 1 to the objective offset
// without changing the cost of any feasible solution.
//
// This seems pretty trivial, but can have a big impact depending on the
// technique we use to solve the MIP. It also makes the objective sparser which
// can only be a good thing.
//
// TODO(user): In more generality, in presence of an exclusive or constraint we
// can shift the cost by any value (even negative), so it may be good to
// maximize the number of coefficients at zero. To investigate.
class ReduceCostOverExclusiveOrConstraintPreprocessor
    : public glop::Preprocessor {
 public:
  explicit ReduceCostOverExclusiveOrConstraintPreprocessor(
      const glop::GlopParameters* mip_parameters)
      : glop::Preprocessor(mip_parameters) {}
  ReduceCostOverExclusiveOrConstraintPreprocessor(
      const ReduceCostOverExclusiveOrConstraintPreprocessor&) = delete;
  ReduceCostOverExclusiveOrConstraintPreprocessor& operator=(
      const ReduceCostOverExclusiveOrConstraintPreprocessor&) = delete;
  ~ReduceCostOverExclusiveOrConstraintPreprocessor() override = default;

  Result Run(glop::LinearProgram* linear_program) override;

 private:
  void RecoverSolutionImpl(glop::SolveStatus&,
                           glop::ProblemSolution*) const final {}
};

// --------------------------------------------------------
// MakeBoundsInteger (work on the MPModelProtoWrapper)
// --------------------------------------------------------

// Makes the bounds of integer variables integer. Makes the bounds of
// constraints involving only integer variables with integer coefficients
// integer.
//
// Returns false if the model is proven infeasible.
bool MakeBoundsInteger(double integer_solution_tolerance,
                       sat::MPModelProtoWrapper* model,
                       SolverLogger* logger = nullptr);

// --------------------------------------------------------
// PropagateBounds (work on the MPModelProtoWrapper)
// --------------------------------------------------------

// It is possible to compute "implied" bounds on a variable from the bounds of
// all the other variables and the constraints in which this variable take
// place. These "implied" bounds can be used to restrict the variable bounds.
// This preprocessor just does that until no more bounds can be propagated or
// a given limit on the number of propagations is reached.
//
// WARNING: this assumes tolerances have already been applied to constraint
// and/or variable bounds and propagates all bounds EXACTLY.
//
// TODO(user): We could also remove fixed enforcement, and if there is a single
// one, propagate it to false if the enforced constraint is infeasible. This is
// not really important at this stage for conversion to CP-SAT though.
//
// Returns false if the model is proven infeasible.
bool PropagateBounds(const sat::MPModelProtoWrapper& mp_model,
                     absl::Span<double> var_lbs, absl::Span<double> var_ubs,
                     SolverLogger* logger = nullptr);

// --------------------------------------------------------
// ExactPresolve (work on the MPModelProtoWrapper)
// --------------------------------------------------------

// Performs some basic presolving using only EXACT bound computation.
//
// This will relax variable/constraint bounds by params.mip_tight_precision()
// before propagation, so it will never cut solutions that were feasible up
// to that tolerance and not dominated via dual reasoning.
//
// Currently this just combines PropagateBounds() and
// RestrictBoundsWithDualReasoning().
//
// TODO(user): Add more like enforcement extraction, but this is a bit trickier
// to explain what happens to updated constraint bounds if the update is not
// exact.
bool ExactPresolve(const sat::SatParameters& params,
                   sat::MPModelProtoWrapper* mp_model_wrapper,
                   SolverLogger* logger = nullptr);

// --------------------------------------------------------
// ShiftVariableBounds (work on the MPModelProtoWrapper)
// --------------------------------------------------------

// For each variable, compute a "shift" and applies it.
// The logic we currently use are defined below.
//
// Returns a vector of offsets (empty if no variable was shifted) such that for
// each variable i:
//   new_bound[i] = initial_bound[i] - offsets[i]
//   initial_value[i] = new_value[i] + offsets[i]
//
// If a solution_hint is present in the model, it is also shifted accordingly.
// If shifting would cause a floating point overflow in a constraint or the
// objective, the model is left unchanged and an empty vector is returned.
std::vector<double> ShiftVariableBounds(
    const std::function<double(const MPVariableProto&)>& get_var_shift,
    sat::MPModelProtoWrapper* model, SolverLogger* logger = nullptr);

// Internal function used by ShiftVariableBounds(), return true if the given
// shift was applied without issue. Otherwise does nothing and return false.
bool ShiftVariableBounds(absl::Span<const double> offsets,
                         sat::MPModelProtoWrapper* model,
                         SolverLogger* logger = nullptr);

// Fixes unused variables (not appearing in any constraint) whose optimal bound
// in the objective direction is finite, shifts their domain to [0, 0], and
// marks them as integer. Returns the per-variable shift offsets applied so that
// initial_value[i] = new_value[i] + offsets[i].
std::vector<double> ShiftToZeroAndMarkAsIntegerUnusedVariables(
    const sat::SatParameters& params, sat::MPModelProtoWrapper* model,
    SolverLogger* logger = nullptr);

// This one moves domains that do not contain zero close to zero. It also only
// use integer shifts, so that when we round to integer in CP-SAT we are using
// integer in the original user space. This is also important for detected
// implied integers
inline std::function<double(const MPVariableProto& mp_var)> IntegerShifts() {
  return [](const MPVariableProto& var) {
    const double lb = var.lower_bound();
    const double ub = var.upper_bound();
    if (std::isfinite(lb) && lb > 0.0) return std::floor(lb);
    if (std::isfinite(ub) && ub < 0.0) return std::ceil(ub);
    return 0.0;
  };
}

// This one should be done just before the final conversion to CP-SAT. We want
// all domain to contain the integer 0! and to not shift more than necessary.
// At this stage, we want to:
// 1/ Shift small domain like [0.3, 0.4] to [0, 0.1] so at least the integer
//    zero could be a solution.
// 2/ Shift something like [1.5, 10] to [-0.5, 8] so that there is always the
//    integer zero inside.
//
// After this, all "fixed" variable should be fixed at exactly zero for
// instance. And our scaler correctly just skip these.
inline std::function<double(const MPVariableProto& mp_var)> FinalCpSatShift(
    double tolerance) {
  return [tolerance](const MPVariableProto& var) {
    const double lb = var.lower_bound();
    const double ub = var.upper_bound();
    if (!var.is_integer()) {
      // Case 1/
      if (std::floor(ub) < std::ceil(lb)) {
        if (std::isfinite(lb) && lb > 0.0) return lb;
        if (std::isfinite(ub) && ub < 0.0) return ub;
      }
    }

    // Case 2/
    if (std::isfinite(lb) && lb > tolerance) return std::ceil(lb);
    if (std::isfinite(ub) && ub < -tolerance) return std::floor(ub);

    return 0.0;
  };
}

// When we are going to scale floating point variables, it makes sense to shift
// one bound to 0.0 if the domain does not contain zero.
inline std::function<double(const MPVariableProto& mp_var)>
ShiftBeforeScaling() {
  return [](const MPVariableProto& var) {
    const double lb = var.lower_bound();
    const double ub = var.upper_bound();
    if (!var.is_integer()) {
      if (std::isfinite(lb) && lb > 0.0) return lb;
      if (std::isfinite(ub) && ub < 0.0) return ub;
    } else {
      if (std::isfinite(lb) && lb > 0.0) return std::ceil(lb);
      if (std::isfinite(ub) && ub < 0.0) return std::floor(ub);
    }

    return 0.0;
  };
}

}  // namespace operations_research

#endif  // ORTOOLS_LINEAR_SOLVER_PROTO_SOLVER_PREPROCESSOR_H_

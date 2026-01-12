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

#include "ortools/glop/preprocessor.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"

namespace operations_research {

// --------------------------------------------------------
// IntegerBoundsPreprocessor
// --------------------------------------------------------

// Makes the bounds of integer variables integer. Makes the bounds of
// constraints involving only integer variables with integer coefficients
// integer.
class IntegerBoundsPreprocessor : public glop::Preprocessor {
 public:
  IntegerBoundsPreprocessor(const glop::GlopParameters* parameters,
                            glop::Fractional integer_solution_tolerance)
      : glop::Preprocessor(parameters),
        integer_solution_tolerance_(integer_solution_tolerance) {}

  IntegerBoundsPreprocessor(const IntegerBoundsPreprocessor&) = delete;
  IntegerBoundsPreprocessor& operator=(const IntegerBoundsPreprocessor&) =
      delete;
  ~IntegerBoundsPreprocessor() override = default;

  bool Run(glop::LinearProgram* linear_program) override;
  void RecoverSolution(glop::ProblemSolution* /*solution*/) const override {}

 private:
  const glop::Fractional integer_solution_tolerance_;
};

// --------------------------------------------------------
// BoundPropagationPreprocessor
// --------------------------------------------------------

// It is possible to compute "implied" bounds on a variable from the bounds of
// all the other variables and the constraints in which this variable take
// place. These "implied" bounds can be used to restrict the variable bounds.
// This preprocessor just does that until no more bounds can be propagated or
// a given limit on the number of propagations is reached.
//
// Note(user): In particular, this preprocessor will remove any singleton row.
//
// Note(user): This seems like a general LP preprocessor but it is really
// difficult to postsolve it correctly in the LP context when one wants to have
// a basic optimal solution at the end. By contrast, in Mip context one is happy
// with any form of an optimal solution at the end, thus restoring the full
// solution is trivial. Consequently, bound propagation is implemented as a mip
// preprocessor.
class BoundPropagationPreprocessor : public glop::Preprocessor {
 public:
  BoundPropagationPreprocessor(const glop::GlopParameters* parameters,
                               glop::Fractional integer_solution_tolerance)
      : glop::Preprocessor(parameters),
        integer_solution_tolerance_(integer_solution_tolerance) {}

  BoundPropagationPreprocessor(const BoundPropagationPreprocessor&) = delete;
  BoundPropagationPreprocessor& operator=(const BoundPropagationPreprocessor&) =
      delete;
  ~BoundPropagationPreprocessor() override = default;

  bool Run(glop::LinearProgram* linear_program) override;
  void RecoverSolution(glop::ProblemSolution* /*solution*/) const override {}

 private:
  const glop::Fractional integer_solution_tolerance_;
};

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
  bool Run(glop::LinearProgram* linear_program) override;
  void RecoverSolution(glop::ProblemSolution* /*solution*/) const override {}

 private:
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

  bool Run(glop::LinearProgram* linear_program) override;
  void RecoverSolution(glop::ProblemSolution* /*solution*/) const override {}
};

}  // namespace operations_research

#endif  // ORTOOLS_LINEAR_SOLVER_PROTO_SOLVER_PREPROCESSOR_H_

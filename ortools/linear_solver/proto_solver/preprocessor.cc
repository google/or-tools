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

#include "ortools/linear_solver/proto_solver/preprocessor.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_utils.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/lp_data/sparse_column.h"
#include "ortools/util/fp_utils.h"
#include "ortools/util/return_macros.h"
#include "ortools/util/stats.h"

using ::operations_research::glop::ColIndex;
using ::operations_research::glop::ColToRowIndex;
using ::operations_research::glop::Fractional;
using ::operations_research::glop::kInfinity;
using ::operations_research::glop::LinearProgram;
using ::operations_research::glop::ProblemStatus;
using ::operations_research::glop::RowIndex;
using ::operations_research::glop::SparseColumn;
using ::operations_research::glop::SparseMatrix;
using ::operations_research::glop::StrictITIVector;
using ::operations_research::glop::SumWithNegativeInfiniteAndOneMissing;
using ::operations_research::glop::SumWithPositiveInfiniteAndOneMissing;

namespace operations_research {

// Helper function to check the bounds of the SetVariableBounds() and
// SetConstraintBounds() functions.
inline bool AreBoundsValid(Fractional lower_bound, Fractional upper_bound) {
  if (std::isnan(lower_bound)) return false;
  if (std::isnan(upper_bound)) return false;
  if (lower_bound == kInfinity && upper_bound == kInfinity) return false;
  if (lower_bound == -kInfinity && upper_bound == -kInfinity) return false;
  if (lower_bound > upper_bound) return false;
  return true;
}

// --------------------------------------------------------
// IntegerBoundsPreprocessor
// --------------------------------------------------------

bool IntegerBoundsPreprocessor::Run(LinearProgram* linear_program) {
  SCOPED_INSTRUCTION_COUNT(time_limit_);
  RETURN_VALUE_IF_NULL(linear_program, false);
  const Fractional tolerance = integer_solution_tolerance_;

  // Make integer the bounds of integer variables.
  // NOTE(user): it may happen that the new bound will be less strict (but at
  // most it will be off by integer_solution_tolerance).
  int64_t num_changed_bounds = 0;
  for (ColIndex col : linear_program->IntegerVariablesList()) {
    const Fractional lb =
        ceil(linear_program->variable_lower_bounds()[col] - tolerance);
    const Fractional ub =
        floor(linear_program->variable_upper_bounds()[col] + tolerance);
    if (!AreBoundsValid(lb, ub)) {
      status_ = glop::ProblemStatus::PRIMAL_INFEASIBLE;
      return false;
    }
    if (lb != linear_program->variable_lower_bounds()[col] ||
        ub != linear_program->variable_upper_bounds()[col]) {
      num_changed_bounds++;
    }
    linear_program->SetVariableBounds(col, lb, ub);
  }
  VLOG(2) << "IntegerBoundsPreprocessor changed " << num_changed_bounds
          << " variable bounds.";

  // Make integer the bounds of integer constraints, if it makes them stricter.
  const SparseMatrix& transpose = linear_program->GetTransposeSparseMatrix();
  num_changed_bounds = 0;
  for (RowIndex row = RowIndex(0); row < linear_program->num_constraints();
       ++row) {
    bool integer_constraint = true;
    for (const SparseColumn::Entry var : transpose.column(RowToColIndex(row))) {
      // Don't affect the constraint if it has a non-integer variable.
      if (!linear_program->IsVariableInteger(RowToColIndex(var.row()))) {
        integer_constraint = false;
        break;
      }
      // Don't affect the constraint if it has a non-integer coefficient. Note
      // that we require each coefficient to be precisely an integer in order to
      // avoid floating point errors.
      //
      // TODO(user): checking integer constraints can go further, e.g.,
      // x + 2 * y = 4 for binary x and y can never be satisfied. But this
      // perhaps starts to resemble bound propagation, which might be too much
      // for a lightweighted preprocessor like this one.
      if (round(var.coefficient()) != var.coefficient()) {
        integer_constraint = false;
        break;
      }
    }
    if (integer_constraint) {
      const Fractional lb =
          std::ceil(linear_program->constraint_lower_bounds()[row] - tolerance);
      const Fractional ub = std::floor(
          linear_program->constraint_upper_bounds()[row] + tolerance);
      if (!AreBoundsValid(lb, ub)) {
        status_ = ProblemStatus::PRIMAL_INFEASIBLE;
        return false;
      }
      if (lb != linear_program->constraint_lower_bounds()[row] ||
          ub != linear_program->constraint_upper_bounds()[row]) {
        num_changed_bounds++;
      }
      linear_program->SetConstraintBounds(row, lb, ub);
    }
  }
  VLOG(2) << "IntegerBoundsPreprocessor changed " << num_changed_bounds
          << " constraint bounds.";
  DCHECK(linear_program->BoundsOfIntegerVariablesAreInteger(tolerance));
  DCHECK(linear_program->BoundsOfIntegerConstraintsAreInteger(tolerance));
  return false;
}

// --------------------------------------------------------
// BoundPropagationPreprocessor
// --------------------------------------------------------
// TODO(user): This preprocessor is not as efficient as it could be because each
// time we process a constraint, we rescan all the involved variables. Make it
// more efficient if it becomes needed. Note that this kind of propagation is
// probably something we want to do each time we take a branch in the mip
// search, so probably an efficient class for this will be created at some
// point.
bool BoundPropagationPreprocessor::Run(LinearProgram* linear_program) {
  SCOPED_INSTRUCTION_COUNT(time_limit_);
  RETURN_VALUE_IF_NULL(linear_program, false);
  const Fractional tolerance = integer_solution_tolerance_;

  // Starts by adding all the row in the 'to_process' queue.
  StrictITIVector<RowIndex, bool> in_queue(linear_program->num_constraints(),
                                           false);
  std::deque<RowIndex> to_process;
  for (RowIndex row(0); row < linear_program->num_constraints(); ++row) {
    to_process.push_back(row);
    in_queue[row] = true;
  }

  // This preprocessor will need to access the constraints row by row.
  const SparseMatrix& transpose = linear_program->GetTransposeSparseMatrix();

  // Now process all the rows until none are left, or a limit on the number of
  // processed rows is reached. The limit is mainly here to prevent infinite
  // loops on corner cases problems. It should not be reached often in practice.
  const int kMaxNumberOfProcessedRows =
      linear_program->num_constraints().value() * 10;
  for (int i = 0; i < kMaxNumberOfProcessedRows && !to_process.empty(); ++i) {
    const RowIndex row = to_process.front();
    in_queue[row] = false;
    to_process.pop_front();

    // For each variable of a constraint on n variables, we want the bound
    // implied by the (n - 1) other variables and the constraint bounds. We use
    // two handy utility classes that allow us to do that efficiently while
    // dealing properly with infinite bounds.
    SumWithNegativeInfiniteAndOneMissing lb_sum;
    SumWithPositiveInfiniteAndOneMissing ub_sum;

    // Initialize the sums.
    bool skip = false;
    for (const SparseColumn::Entry e : transpose.column(RowToColIndex(row))) {
      const ColIndex col = RowToColIndex(e.row());
      Fractional entry_lb =
          e.coefficient() * linear_program->variable_lower_bounds()[col];
      Fractional entry_ub =
          e.coefficient() * linear_program->variable_upper_bounds()[col];
      if (e.coefficient() < 0.0) std::swap(entry_lb, entry_ub);
      if (entry_lb == kInfinity || entry_ub == -kInfinity) {
        // TODO(user): our SumWithOneMissing does not deal well with infinity of
        // the wrong sign. For now when this happen we skip this constraint.
        // Note however than the other implied bounds could still be used.
        skip = true;
        break;
      }
      lb_sum.Add(entry_lb);
      ub_sum.Add(entry_ub);
    }
    if (skip) continue;

    // The inequality
    //    constraint_lb <= sum(entries) <= constraint_ub
    // can be rewritten as:
    //    sum(entries) + (-activity) = 0,
    // where (-activity) has bounds [-constraint_ub, -constraint_lb].
    // We use this latter convention to simplify our code.
    lb_sum.Add(-linear_program->constraint_upper_bounds()[row]);
    ub_sum.Add(-linear_program->constraint_lower_bounds()[row]);

    // Process the variables one by one and check if the implied bounds are
    // more restrictive.
    for (const SparseColumn::Entry e : transpose.column(RowToColIndex(row))) {
      const ColIndex col = RowToColIndex(e.row());
      const Fractional coeff = e.coefficient();
      const Fractional var_lb = linear_program->variable_lower_bounds()[col];
      const Fractional var_ub = linear_program->variable_upper_bounds()[col];
      Fractional entry_lb = coeff * var_lb;
      Fractional entry_ub = coeff * var_ub;
      if (coeff < 0.0) std::swap(entry_lb, entry_ub);

      // If X is the variable with index col and Y the sum of all the other
      // variables and of (-activity), then coeff * X + Y = 0. Since Y's bounds
      // are [lb_sum without X, ub_sum without X], it is easy to derive the
      // implied bounds on X.
      Fractional implied_lb = -ub_sum.SumWithout(entry_ub) / coeff;
      Fractional implied_ub = -lb_sum.SumWithout(entry_lb) / coeff;
      if (coeff < 0.0) std::swap(implied_lb, implied_ub);

      // If the variable is integer, make the implied bounds integer.
      if (linear_program->IsVariableInteger(col)) {
        implied_lb = std::ceil(implied_lb - tolerance);
        implied_ub = std::floor(implied_ub + tolerance);
      }

      // more restrictive? If yes, sets the bounds, and add all the impacted
      // row back into to_process if they are not already there.
      if (implied_lb > var_lb || implied_ub < var_ub) {
        Fractional new_lb = std::max(implied_lb, var_lb);
        Fractional new_ub = std::min(implied_ub, var_ub);
        if (new_lb > new_ub) {
          // TODO(user): Investigate what tolerance we should use here.
          if (new_lb - tolerance > new_ub) {
            status_ = ProblemStatus::PRIMAL_INFEASIBLE;
            return false;
          } else {
            // We choose the nearest integer for an integer variable, or the
            // middle value for a non-integer one.
            if (linear_program->IsVariableInteger(col)) {
              new_lb = new_ub = round(new_lb);
            } else {
              new_lb = new_ub = (new_lb + new_ub) / 2.0;
            }
          }
        }

        // This extra test avoids reprocessing many rows for no reason.
        // It can be false if we run into the case new_lb > new_ub above.
        if (new_ub != var_ub || new_lb != var_lb) {
          linear_program->SetVariableBounds(col, new_lb, new_ub);
          for (SparseColumn::Entry e : linear_program->GetSparseColumn(col)) {
            if (!in_queue[e.row()]) {
              to_process.push_back(e.row());
              in_queue[e.row()] = true;
            }
          }
        }
      }
    }
  }
  if (!to_process.empty()) {
    LOG_FIRST_N(WARNING, 10)
        << "Propagation limit reached in the BoundPropagationPreprocessor, "
        << "maybe the limit should be increased.";
  }
  DCHECK(linear_program->BoundsOfIntegerVariablesAreInteger(
      integer_solution_tolerance_));
  DCHECK(linear_program->BoundsOfIntegerConstraintsAreInteger(
      integer_solution_tolerance_));
  return false;
}

// --------------------------------------------------------
// ImpliedIntegerPreprocessor
// --------------------------------------------------------
bool ImpliedIntegerPreprocessor::Run(LinearProgram* linear_program) {
  SCOPED_INSTRUCTION_COUNT(time_limit_);
  RETURN_VALUE_IF_NULL(linear_program, false);
  int64_t num_implied_integer_variables = 0;
  const Fractional tolerance = integer_solution_tolerance_;
  for (ColIndex col(0); col < linear_program->num_variables(); ++col) {
    DCHECK_EQ(linear_program->GetFirstSlackVariable(), glop::kInvalidCol);

    // Skip the integer variables.
    if (linear_program->GetVariableType(col) !=
        LinearProgram::VariableType::CONTINUOUS) {
      continue;
    }

    const bool is_implied_integer =
        VariableOccursInAtLeastOneEqualityConstraint(*linear_program, col)
            ? AnyEqualityConstraintImpliesIntegrality(*linear_program, col)
            : AllInequalityConstraintsImplyIntegrality(*linear_program, col);

    if (is_implied_integer) {
      linear_program->SetVariableType(
          col, LinearProgram::VariableType::IMPLIED_INTEGER);
      num_implied_integer_variables++;
      VLOG(2) << "Marked col " << col << " implied integer.";

      // We need to tighten its bounds if they are not integer, otherwise
      // other preprocessor complains.
      const Fractional lb =
          std::ceil(linear_program->variable_lower_bounds()[col] - tolerance);
      const Fractional ub =
          std::floor(linear_program->variable_upper_bounds()[col] + tolerance);
      if (!AreBoundsValid(lb, ub)) {
        status_ = ProblemStatus::PRIMAL_INFEASIBLE;
        return false;
      }
      linear_program->SetVariableBounds(col, lb, ub);
    }
  }
  VLOG(2) << "ImpliedIntegerPreprocessor detected "
          << num_implied_integer_variables << " implied integer variables.";

  DCHECK(linear_program->BoundsOfIntegerVariablesAreInteger(
      integer_solution_tolerance_));

  // TODO(user): Because this presolve step detects new integer variables and
  // does not tighten the bounds of a constraint if all its variables become
  // integer, this invariant is currently not enforced:
  // DCHECK(linear_program->BoundsOfIntegerConstraintsAreInteger(
  //    integer_solution_tolerance_));

  return false;  // Does not require postsolve.
}

bool ImpliedIntegerPreprocessor::AnyEqualityConstraintImpliesIntegrality(
    const LinearProgram& linear_program, ColIndex variable) {
  for (const SparseColumn::Entry entry :
       linear_program.GetSparseColumn(variable)) {
    // Process only equality constraints.
    if (linear_program.constraint_upper_bounds()[entry.row()] ==
        linear_program.constraint_lower_bounds()[entry.row()]) {
      if (ConstraintSupportsImpliedIntegrality(linear_program, variable,
                                               entry.row())) {
        return true;
      }
    }
  }
  return false;
}

bool ImpliedIntegerPreprocessor::AllInequalityConstraintsImplyIntegrality(
    const LinearProgram& linear_program, ColIndex variable) {
  // Check variable bounds.
  Fractional lower_bound = linear_program.variable_lower_bounds()[variable];
  Fractional upper_bound = linear_program.variable_upper_bounds()[variable];
  if (!IsIntegerWithinTolerance(lower_bound, integer_solution_tolerance_) ||
      !IsIntegerWithinTolerance(upper_bound, integer_solution_tolerance_)) {
    // The bounds are not integer.
    // We cannot deduce anything if the variable as an objective.
    //
    // TODO(user): Actually we can if the bound that minimize the cost is
    // integer but not the other. Improve the code.
    if (linear_program.objective_coefficients()[variable] != 0.0) return false;

    // No objective. If the variable domain contains an integer point, then
    // there is a chance for this variable to be integer. This is because if the
    // condition on the constraints below is true, then the constraints will
    // always imply the variable to be inside a [integer_lb, integer_ub] domain.
    // And if the intersection of this domain with the variable domain is
    // non-empty, then it contains one or more integer points and we can always
    // set the variable to one of these integer values.
    if (std::ceil(lower_bound) > std::floor(upper_bound)) return false;
  }

  // Primal detection for each constraint containing variable.
  for (const SparseColumn::Entry entry :
       linear_program.GetSparseColumn(variable)) {
    if (!ConstraintSupportsImpliedIntegrality(linear_program, variable,
                                              entry.row())) {
      return false;
    }
  }
  return true;
}

bool ImpliedIntegerPreprocessor::ConstraintSupportsImpliedIntegrality(
    const LinearProgram& linear_program, ColIndex variable, RowIndex row) {
  const SparseMatrix& coefficients_transpose =
      linear_program.GetTransposeSparseMatrix();
  const Fractional variable_coefficient = coefficients_transpose.LookUpValue(
      ColToRowIndex(variable), RowToColIndex(row));

  for (const SparseColumn::Entry entry :
       coefficients_transpose.column(RowToColIndex(row))) {
    const ColIndex col = RowToColIndex(entry.row());
    if (col == variable) continue;

    // Check if the variables in the row are all integers.
    if (!linear_program.IsVariableInteger(col)) {
      return false;
    }

    // Check if the coefficient ratios are all integers.
    const Fractional coefficient_ratio =
        entry.coefficient() / variable_coefficient;
    if (!IsIntegerWithinTolerance(coefficient_ratio,
                                  integer_solution_tolerance_)) {
      return false;
    }
  }

  // Check if the constraint bound ratios are integers.
  // Note that we ignore infinities.
  if (linear_program.constraint_lower_bounds()[row] != -kInfinity) {
    const Fractional constraint_lower_bound_ratio =
        linear_program.constraint_lower_bounds()[row] / variable_coefficient;
    if (!IsIntegerWithinTolerance(constraint_lower_bound_ratio,
                                  integer_solution_tolerance_)) {
      return false;
    }
  }
  if (linear_program.constraint_upper_bounds()[row] != kInfinity) {
    const Fractional constraint_upper_bound_ratio =
        linear_program.constraint_upper_bounds()[row] / variable_coefficient;
    if (!IsIntegerWithinTolerance(constraint_upper_bound_ratio,
                                  integer_solution_tolerance_)) {
      return false;
    }
  }
  return true;
}

bool ImpliedIntegerPreprocessor::VariableOccursInAtLeastOneEqualityConstraint(
    const LinearProgram& linear_program, ColIndex variable) {
  for (const SparseColumn::Entry entry :
       linear_program.GetSparseColumn(variable)) {
    // Check if the constraint is an equality.
    if (linear_program.constraint_upper_bounds()[entry.row()] ==
        linear_program.constraint_lower_bounds()[entry.row()]) {
      return true;
    }
  }
  return false;
}

// --------------------------------------------------------
// ReduceCostOverExclusiveOrConstraintPreprocessor
// --------------------------------------------------------

bool ReduceCostOverExclusiveOrConstraintPreprocessor::Run(
    LinearProgram* linear_program) {
  SCOPED_INSTRUCTION_COUNT(time_limit_);
  RETURN_VALUE_IF_NULL(linear_program, false);
  const RowIndex num_constraints = linear_program->num_constraints();
  const SparseMatrix& transpose = linear_program->GetTransposeSparseMatrix();
  for (RowIndex row(0); row < num_constraints; ++row) {
    if (linear_program->constraint_lower_bounds()[row] != 1.0) continue;
    if (linear_program->constraint_upper_bounds()[row] != 1.0) continue;
    Fractional min_cost = std::numeric_limits<Fractional>::infinity();
    bool constraint_is_exclusive_or = true;
    for (const SparseColumn::Entry e : transpose.column(RowToColIndex(row))) {
      const ColIndex var = RowToColIndex(e.row());
      if (!linear_program->IsVariableInteger(var) ||
          linear_program->variable_lower_bounds()[var] != 0.0 ||
          linear_program->variable_upper_bounds()[var] != 1.0 ||
          e.coefficient() != 1.0) {
        constraint_is_exclusive_or = false;
        break;
      }
      min_cost =
          std::min(min_cost, linear_program->objective_coefficients()[var]);
    }
    if (constraint_is_exclusive_or && min_cost > 0.0 &&
        glop::IsFinite(min_cost)) {
      for (const SparseColumn::Entry e : transpose.column(RowToColIndex(row))) {
        const ColIndex var = RowToColIndex(e.row());
        const Fractional cost = linear_program->objective_coefficients()[var];
        linear_program->SetObjectiveCoefficient(var, cost - min_cost);
      }
      linear_program->SetObjectiveOffset(linear_program->objective_offset() +
                                         min_cost);
    }
  }
  return false;
}

}  // namespace operations_research

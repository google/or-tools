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
#include <functional>
#include <limits>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "ortools/base/types.h"
#include "ortools/glop/preprocessor.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_utils.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/lp_data/sparse_column.h"
#include "ortools/sat/mp_model_wrapper.h"
#include "ortools/sat/util.h"
#include "ortools/util/fp_utils.h"
#include "ortools/util/full_precision_inequalities.h"
#include "ortools/util/logging.h"
#include "ortools/util/return_macros.h"
#include "ortools/util/stats.h"

using ::operations_research::glop::ColIndex;
using ::operations_research::glop::ColToRowIndex;
using ::operations_research::glop::Fractional;
using ::operations_research::glop::kInfinity;
using ::operations_research::glop::LinearProgram;
using ::operations_research::glop::RowIndex;
using ::operations_research::glop::SparseColumn;
using ::operations_research::glop::SparseMatrix;
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
// MakeBoundsInteger
// --------------------------------------------------------

bool MakeBoundsInteger(double tolerance, sat::MPModelProtoWrapper* model,
                       SolverLogger* logger) {
  // Make integer the bounds of integer variables.
  // NOTE(user): it may happen that the new bound will be less strict (but at
  // most it will be off by tolerance).
  int64_t num_changed_var_bounds = 0;
  const int num_variables = model->num_variables();
  for (int col = 0; col < num_variables; ++col) {
    const MPVariableProto& var = model->variables(col);
    if (!var.is_integer()) continue;
    const Fractional lb = std::ceil(var.lower_bound() - tolerance);
    const Fractional ub = std::floor(var.upper_bound() + tolerance);
    if (!AreBoundsValid(lb, ub)) {
      return false;
    }
    if (lb != var.lower_bound() || ub != var.upper_bound()) {
      num_changed_var_bounds++;
      model->mutable_variables(col)->set_lower_bound(lb);
      model->mutable_variables(col)->set_upper_bound(ub);
    }
  }

  // Make integer the bounds of EXACT integer constraints.
  int64_t num_changed_ct_bounds = 0;
  const int num_constraints = model->num_constraints();
  for (int row = 0; row < num_constraints; ++row) {
    const sat::EnforcedLinearConstraint& ct = model->constraints(row);
    const int num_terms = ct.num_terms();
    bool integer_constraint = true;
    for (int i = 0; i < num_terms; ++i) {
      const Fractional coeff = ct.coeffs(i);
      if (coeff == 0.0) continue;

      // Don't affect the constraint if it has a non-integer variable.
      if (!model->variables(ct.vars(i)).is_integer()) {
        integer_constraint = false;
        break;
      }

      // Don't affect the constraint if it has a non-integer coefficient. Note
      // that we require each coefficient to be precisely an integer in order to
      // avoid floating point errors.
      if (std::round(coeff) != coeff) {
        integer_constraint = false;
        break;
      }
    }
    if (integer_constraint) {
      const Fractional lb = std::ceil(ct.lower_bound() - tolerance);
      const Fractional ub = std::floor(ct.upper_bound() + tolerance);
      if (!AreBoundsValid(lb, ub)) {
        if (ct.IsEnforced()) continue;
        return false;
      }
      if (lb != ct.lower_bound() || ub != ct.upper_bound()) {
        num_changed_ct_bounds++;
        model->mutable_constraints(row)->set_lower_bound(lb);
        model->mutable_constraints(row)->set_upper_bound(ub);
      }
    }
  }
  if (logger != nullptr &&
      (num_changed_var_bounds > 0 || num_changed_ct_bounds > 0)) {
    SOLVER_LOG(
        logger, "[IntegerBounds] Tightened ",
        FormatCounter(num_changed_var_bounds), " integer variable bounds and ",
        FormatCounter(num_changed_ct_bounds), " integer constraint bounds.");
  }
  return true;
}

// --------------------------------------------------------
// PropagateBounds
// --------------------------------------------------------

// TODO(user): This preprocessor is not as efficient as it could be because each
// time we process a constraint, we rescan all the involved variables. Make it
// more efficient if it becomes needed. Note that this kind of propagation is
// probably something we want to do each time we take a branch in the mip
// search, so probably an efficient class for this will be created at some
// point.
bool PropagateBounds(double constraint_tolerance, bool relax_to_integer,
                     sat::MPModelProtoWrapper* model, SolverLogger* logger) {
  // Starts by adding all the row in the 'to_process' queue.
  const int num_constraints = model->num_constraints();
  const int num_variables = model->num_variables();
  std::vector<bool> in_queue(num_constraints, false);
  std::deque<int> to_process;
  sat::CompactVectorVectorBuilder<int, int> var_to_constraints_builder;
  for (int row = 0; row < num_constraints; ++row) {
    if (model->constraints(row).IsEnforced()) continue;
    in_queue[row] = true;
    to_process.push_back(row);
    for (const int col : model->constraints(row).vars()) {
      var_to_constraints_builder.Add(col, row);
    }
  }
  const sat::CompactVectorVector<int, int> var_to_constraints(
      var_to_constraints_builder, num_variables);

  std::vector<double> alpha;
  std::vector<double> beta;

  // Now process all the rows until none are left, or a limit on the number of
  // processed rows is reached. The limit is mainly here to prevent infinite
  // loops on corner cases problems. It should not be reached often in practice.
  const int kMaxNumberOfProcessedRows = num_constraints * 10;
  int num_pushes = 0;
  for (int i = 0; i < kMaxNumberOfProcessedRows && !to_process.empty(); ++i) {
    const int row = to_process.front();
    in_queue[row] = false;
    to_process.pop_front();

    const sat::EnforcedLinearConstraint& ct = model->constraints(row);
    const int num_terms = ct.num_terms();

    // For each variable of a constraint on n variables, we want the bound
    // implied by the (n - 1) other variables and the constraint bounds. We use
    // two handy utility classes that allow us to do that efficiently while
    // dealing properly with infinite bounds.
    SumWithNegativeInfiniteAndOneMissing lb_sum;
    SumWithPositiveInfiniteAndOneMissing ub_sum;

    // Initialize the sums.
    bool skip = false;
    for (int term_idx = 0; term_idx < num_terms; ++term_idx) {
      const int col = ct.vars(term_idx);
      const Fractional coeff = ct.coeffs(term_idx);
      if (coeff == 0.0) continue;
      Fractional entry_lb = coeff * model->variables(col).lower_bound();
      Fractional entry_ub = coeff * model->variables(col).upper_bound();
      if (coeff < 0.0) std::swap(entry_lb, entry_ub);
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
    lb_sum.Add(-ct.upper_bound());
    ub_sum.Add(-ct.lower_bound());

    auto compute_exact_ub_other = [&](int target_col) -> Fractional {
      const Fractional ct_lb = ct.lower_bound();
      DCHECK(std::isfinite(ct_lb));
      alpha.clear();
      beta.clear();
      if (ct_lb != 0.0) {
        alpha.push_back(-1.0);
        beta.push_back(ct_lb);
      }
      if (constraint_tolerance != 0.0) {
        alpha.push_back(1.0);
        beta.push_back(constraint_tolerance);
      }
      for (int term_idx = 0; term_idx < num_terms; ++term_idx) {
        const int col_other = ct.vars(term_idx);
        if (col_other == target_col) continue;
        const Fractional c = ct.coeffs(term_idx);
        const Fractional b = (c > 0.0)
                                 ? model->variables(col_other).upper_bound()
                                 : model->variables(col_other).lower_bound();
        DCHECK(std::isfinite(b));
        if (c != 0.0 && b != 0.0) {
          alpha.push_back(c);
          beta.push_back(b);
        }
      }
      return GetLooseDotProductBounds(alpha, beta).second;
    };

    auto compute_exact_lb_other = [&](int target_col) -> Fractional {
      const Fractional ct_ub = ct.upper_bound();
      DCHECK(std::isfinite(ct_ub));
      alpha.clear();
      beta.clear();
      if (ct_ub != 0.0) {
        alpha.push_back(-1.0);
        beta.push_back(ct_ub);
      }
      if (constraint_tolerance != 0.0) {
        alpha.push_back(-1.0);
        beta.push_back(constraint_tolerance);
      }
      for (int term_idx = 0; term_idx < num_terms; ++term_idx) {
        const int col_other = ct.vars(term_idx);
        if (col_other == target_col) continue;
        const Fractional c = ct.coeffs(term_idx);
        const Fractional b = (c > 0.0)
                                 ? model->variables(col_other).lower_bound()
                                 : model->variables(col_other).upper_bound();
        DCHECK(std::isfinite(b));
        if (c != 0.0 && b != 0.0) {
          alpha.push_back(c);
          beta.push_back(b);
        }
      }
      return GetLooseDotProductBounds(alpha, beta).first;
    };

    // Process the variables one by one and check if the implied bounds are
    // more restrictive.
    for (int term_idx = 0; term_idx < num_terms; ++term_idx) {
      const int col = ct.vars(term_idx);
      const Fractional coeff = ct.coeffs(term_idx);
      if (coeff == 0.0) continue;
      const Fractional var_lb = model->variables(col).lower_bound();
      const Fractional var_ub = model->variables(col).upper_bound();
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
      const bool is_integer = model->variables(col).is_integer();
      if (is_integer) {
        implied_lb = std::ceil(implied_lb - constraint_tolerance);
        implied_ub = std::floor(implied_ub + constraint_tolerance);
      }

      // more restrictive? If yes, re-check with exact dot product bounds
      // (GetLooseDotProductBounds) before setting the bounds. This is
      // relatively cheap if done only when the heuristic triggers, and avoiding
      // errors compounding.
      if (implied_lb > var_lb || implied_ub < var_ub) {
        Fractional exact_implied_lb = var_lb;
        Fractional exact_implied_ub = var_ub;
        if (implied_lb > var_lb) {
          const Fractional other_bound = (coeff > 0.0)
                                             ? compute_exact_ub_other(col)
                                             : compute_exact_lb_other(col);
          exact_implied_lb = std::nextafter(-other_bound / coeff, -kInfinity);
          if (is_integer) {
            // Note that there is no need to use tolerance here. If we are past
            // the exact bound, the activity should be more than tolerance away
            // from the constraint bound.
            exact_implied_lb = std::ceil(exact_implied_lb);
          } else if (relax_to_integer) {
            exact_implied_lb = std::floor(exact_implied_lb);
          }
        }
        if (implied_ub < var_ub) {
          const Fractional other_bound = (coeff > 0.0)
                                             ? compute_exact_lb_other(col)
                                             : compute_exact_ub_other(col);
          exact_implied_ub = std::nextafter(-other_bound / coeff, kInfinity);
          if (is_integer) {
            // Same as above, no tolerance needed here.
            exact_implied_ub = std::floor(exact_implied_ub);
          } else if (relax_to_integer) {
            exact_implied_ub = std::ceil(exact_implied_ub);
          }
        }

        Fractional new_lb = std::max(exact_implied_lb, var_lb);
        Fractional new_ub = std::min(exact_implied_ub, var_ub);
        if (new_lb > new_ub) {
          if (is_integer && new_lb - constraint_tolerance <= new_ub) {
            new_lb = new_ub = std::round(new_lb);
          } else {
            return false;
          }
        }

        // This extra test avoids reprocessing many rows for no reason.
        // It can be false if we run into the case new_lb > new_ub above.
        if (new_ub != var_ub || new_lb != var_lb) {
          ++num_pushes;
          model->mutable_variables(col)->set_lower_bound(new_lb);
          model->mutable_variables(col)->set_upper_bound(new_ub);
          for (const int c : var_to_constraints[col]) {
            if (!in_queue[c]) {
              to_process.push_back(c);
              in_queue[c] = true;
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
  if (logger != nullptr && num_pushes > 0) {
    SOLVER_LOG(logger, "[BoundPropagation] Pushed ", FormatCounter(num_pushes),
               " bounds.");
  }

  return true;
}

// --------------------------------------------------------
// ImpliedIntegerPreprocessor
// --------------------------------------------------------

glop::Preprocessor::Result ImpliedIntegerPreprocessor::Run(
    LinearProgram* linear_program) {
  SCOPED_INSTRUCTION_COUNT(time_limit_);
  RETURN_VALUE_IF_NULL(linear_program, {.postsolve_is_needed = false});
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
        return {.postsolve_is_needed = false,
                .solve_status = glop::PrimalInfeasibleSolveStatus()};
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

  return {.postsolve_is_needed = false};  // Does not require postsolve.
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

glop::Preprocessor::Result ReduceCostOverExclusiveOrConstraintPreprocessor::Run(
    LinearProgram* linear_program) {
  SCOPED_INSTRUCTION_COUNT(time_limit_);
  RETURN_VALUE_IF_NULL(linear_program, {.postsolve_is_needed = false});
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
  return {.postsolve_is_needed = false};
}

// --------------------------------------------------------
// ShiftVariableBounds
// --------------------------------------------------------

std::vector<double> ShiftVariableBounds(
    const std::function<double(const MPVariableProto&)>& get_var_shift,
    sat::MPModelProtoWrapper* model, SolverLogger* logger) {
  const int num_cols = model->num_variables();
  std::vector<double> offsets(num_cols, 0.0);
  int num_shifted = 0;
  double max_shift = 0.0;
  glop::KahanSum objective_offset;
  for (int col = 0; col < num_cols; ++col) {
    const MPVariableProto& var = model->variables(col);
    const double offset = get_var_shift(var);
    if (offset != 0.0) {
      // We should never shift an integer variable by a non-integer value, this
      // is not correct.
      CHECK(std::isfinite(offset));
      if (var.is_integer()) CHECK_EQ(offset, std::round(offset));

      ++num_shifted;
      offsets[col] = offset;
      objective_offset.Add(var.objective_coefficient() * offset);
      max_shift = std::max(max_shift, std::abs(offset));
    }
  }

  if (num_shifted == 0) return {};
  if (!std::isfinite(objective_offset.Value())) return {};

  const int num_rows = model->num_constraints();
  std::vector<double> row_offsets(num_rows, 0.0);
  for (int row = 0; row < num_rows; ++row) {
    const sat::EnforcedLinearConstraint& ct = model->constraints(row);
    const int num_terms = ct.num_terms();
    glop::KahanSum sum;
    for (int k = 0; k < num_terms; ++k) {
      const int col = ct.vars(k);
      const Fractional offset = offsets[col];
      if (offset != 0.0) {
        sum.Add(ct.coeffs(k) * offset);
      }
    }
    if (!std::isfinite(sum.Value())) return {};
    row_offsets[row] = sum.Value();
  }

  // Do the actual shifting.
  for (int col = 0; col < num_cols; ++col) {
    const Fractional offset = offsets[col];
    if (offset != 0.0) {
      MPVariableProto* var = model->mutable_variables(col);
      var->set_lower_bound(var->lower_bound() - offset);
      var->set_upper_bound(var->upper_bound() - offset);
    }
  }
  for (int row = 0; row < num_rows; ++row) {
    sat::EnforcedLinearConstraint* ct = model->mutable_constraints(row);
    for (int i = 0; i < ct->enforcement_vars().size(); ++i) {
      const int col = ct->enforcement_vars()[i];
      if (offsets[col] != 0.0) {
        // Note that we should only have integer enforcement with a domain
        // containing integer value expressible as int.
        DCHECK_LE(offsets[col], static_cast<double>(kint64max));
        DCHECK_GE(offsets[col], static_cast<double>(kint64min));
        ct->set_enforcement_values(i, ct->enforcement_values()[i] -
                                          static_cast<int64_t>(offsets[col]));
      }
    }
    const Fractional row_offset = row_offsets[row];
    if (row_offset != 0.0) {
      ct->set_lower_bound(ct->lower_bound() - row_offset);
      ct->set_upper_bound(ct->upper_bound() - row_offset);
    }
  }
  if (objective_offset.Value() != 0.0) {
    model->set_objective_offset(model->objective_offset() +
                                objective_offset.Value());
  }

  // Update the hint if any.
  if (model->mutable_underlying_model()->has_solution_hint()) {
    PartialVariableAssignment* hint =
        model->mutable_underlying_model()->mutable_solution_hint();
    for (int i = 0; i < hint->var_index_size(); ++i) {
      const int col = hint->var_index(i);
      if (col >= 0 && col < num_cols) {
        hint->set_var_value(i, hint->var_value(i) - offsets[col]);
      }
    }
  }

  if (logger != nullptr && num_shifted > 0) {
    SOLVER_LOG(logger, "[Shifting] Shifted ", FormatCounter(num_shifted),
               " variable towards zero with max shift magnitude ", max_shift,
               ".");
  }

  return offsets;
}

}  // namespace operations_research

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
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/types/span.h"
#include "ortools/base/types.h"
#include "ortools/glop/preprocessor.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_utils.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/lp_data/sparse_column.h"
#include "ortools/sat/lp_utils.h"
#include "ortools/sat/mip_model.pb.h"
#include "ortools/sat/mp_model_wrapper.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/util.h"
#include "ortools/util/fp_roundtrip_conv.h"
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
    const sat::MipVariableProto& var = model->variables(col);
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
    const sat::EnforcedLinearConstraintProto& ct = model->constraints(row);
    const int num_terms = ct.vars().size();
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
        if (!ct.enforcement_vars().empty()) continue;
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
bool PropagateBounds(const sat::MPModelProtoWrapper& mp_model,
                     absl::Span<double> var_lbs, absl::Span<double> var_ubs,
                     SolverLogger* logger) {
  // Starts by adding all the row in the 'to_process' queue.
  const int num_constraints = mp_model.num_constraints();
  const int num_variables = mp_model.num_variables();
  std::vector<bool> in_queue(num_constraints, false);
  std::deque<int> to_process;
  sat::CompactVectorVectorBuilder<int, int> var_to_constraints_builder;
  for (int row = 0; row < num_constraints; ++row) {
    if (!mp_model.constraints(row).enforcement_vars().empty()) continue;
    in_queue[row] = true;
    to_process.push_back(row);
    for (const int col : mp_model.constraints(row).vars()) {
      var_to_constraints_builder.Add(col, row);
    }
  }
  const sat::CompactVectorVector<int, int> var_to_constraints(
      var_to_constraints_builder, num_variables);

  sat::LeftRightActivityBounds bounds;

  // Now process all the rows until none are left, or a limit on the number of
  // processed rows is reached. The limit is mainly here to prevent infinite
  // loops on corner cases problems. It should not be reached often in practice.
  const int kMaxNumberOfProcessedRows = num_constraints * 10;
  int num_pushes = 0;
  double max_change = 0.0;
  for (int i = 0; i < kMaxNumberOfProcessedRows && !to_process.empty(); ++i) {
    const int row = to_process.front();
    in_queue[row] = false;
    to_process.pop_front();

    const sat::EnforcedLinearConstraintProto& ct = mp_model.constraints(row);
    const int num_terms = ct.vars().size();
    if (num_terms == 0) continue;

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
      Fractional entry_lb = coeff * var_lbs[col];
      Fractional entry_ub = coeff * var_ubs[col];
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
    const double ct_lb = ct.lower_bound();
    const double ct_ub = ct.upper_bound();
    lb_sum.Add(-ct_ub);
    ub_sum.Add(-ct_lb);

    // First, detect using fast double arithmetic if any propagation looks
    // possible.
    bool propagation_is_possible = false;
    for (int term_idx = 0; term_idx < num_terms; ++term_idx) {
      const int col = ct.vars(term_idx);
      const Fractional coeff = ct.coeffs(term_idx);
      if (coeff == 0.0) continue;
      const Fractional var_lb = var_lbs[col];
      const Fractional var_ub = var_ubs[col];
      Fractional entry_lb = coeff * var_lb;
      Fractional entry_ub = coeff * var_ub;
      if (coeff < 0.0) std::swap(entry_lb, entry_ub);

      // If X is the variable with index col and Y the sum of all the other
      // variables and of (-activity), then coeff * X + Y = 0. Since Y's bounds
      // are [lb_sum without X, ub_sum without X], it is easy to derive the
      // implied bounds on X.
      Fractional inexact_implied_lb = -ub_sum.SumWithout(entry_ub) / coeff;
      Fractional inexact_implied_ub = -lb_sum.SumWithout(entry_lb) / coeff;
      if (coeff < 0.0) std::swap(inexact_implied_lb, inexact_implied_ub);

      if (inexact_implied_lb > var_lb || inexact_implied_ub < var_ub) {
        propagation_is_possible = true;
        break;
      }
    }
    if (!propagation_is_possible) continue;

    // Propagate bounds exactly using LeftRightActivityBounds.
    bounds.FillFromConstraint(var_lbs, var_ubs, ct);
    for (int term_idx = 0; term_idx < num_terms; ++term_idx) {
      const int var = ct.vars(term_idx);
      const double coeff = ct.coeffs(term_idx);
      if (coeff == 0.0) continue;

      double primal_lb = ExactScaledSumLb(
          ct_lb, -bounds.max_activity_of_others[term_idx], coeff);
      double primal_ub = ExactScaledSumUb(
          ct_ub, -bounds.min_activity_of_others[term_idx], coeff);
      if (coeff < 0.0) {
        std::swap(primal_lb, primal_ub);
      }
      if (mp_model.variables(var).is_integer()) {
        primal_lb = std::ceil(primal_lb);
        primal_ub = std::floor(primal_ub);
      }

      bool changed = false;
      if (primal_ub < var_ubs[var]) {
        changed = true;
        max_change = std::max(max_change, var_ubs[var] - primal_ub);
        var_ubs[var] = primal_ub;
      }
      if (primal_lb > var_lbs[var]) {
        changed = true;
        max_change = std::max(max_change, primal_lb - var_lbs[var]);
        var_lbs[var] = primal_lb;
      }
      if (var_lbs[var] > var_ubs[var]) {
        return false;  // Infeasible.
      }

      // This extra test avoids reprocessing many rows for no reason.
      if (changed) {
        ++num_pushes;
        for (const int c : var_to_constraints[var]) {
          if (!in_queue[c]) {
            to_process.push_back(c);
            in_queue[c] = true;
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
               " bounds. Max change ", max_change, ".");
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

bool ShiftVariableBounds(absl::Span<const double> offsets,
                         sat::MPModelProtoWrapper* model,
                         SolverLogger* logger) {
  const int num_cols = model->num_variables();
  CHECK_EQ(offsets.size(), num_cols);
  int num_shifted = 0;
  double max_shift = 0.0;
  glop::KahanSum objective_offset;
  for (int col = 0; col < num_cols; ++col) {
    const double offset = offsets[col];
    if (offset != 0.0) {
      const sat::MipVariableProto& var = model->variables(col);
      // We should never shift an integer variable by a non-integer value, this
      // is not correct.
      CHECK(std::isfinite(offset));
      if (var.is_integer()) CHECK_EQ(offset, std::round(offset));

      ++num_shifted;
      objective_offset.Add(var.objective_coefficient() * offset);
      max_shift = std::max(max_shift, std::abs(offset));
    }
  }

  if (num_shifted == 0) return false;
  if (!std::isfinite(objective_offset.Value())) return false;

  const int num_rows = model->num_constraints();
  std::vector<double> row_offsets(num_rows, 0.0);
  for (int row = 0; row < num_rows; ++row) {
    const sat::EnforcedLinearConstraintProto& ct = model->constraints(row);
    const int num_terms = ct.vars().size();
    glop::KahanSum sum;
    for (int k = 0; k < num_terms; ++k) {
      const int col = ct.vars(k);
      const Fractional offset = offsets[col];
      if (offset != 0.0) {
        sum.Add(ct.coeffs(k) * offset);
      }
    }
    if (!std::isfinite(sum.Value())) return false;
    row_offsets[row] = sum.Value();
  }

  // Do the actual shifting.
  for (int col = 0; col < num_cols; ++col) {
    const Fractional offset = offsets[col];
    if (offset != 0.0) {
      sat::MipVariableProto* var = model->mutable_variables(col);
      var->set_lower_bound(var->lower_bound() - offset);
      var->set_upper_bound(var->upper_bound() - offset);
    }
  }
  for (int row = 0; row < num_rows; ++row) {
    sat::EnforcedLinearConstraintProto* ct = model->mutable_constraints(row);
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
  if (model->has_solution_hint()) {
    sat::MipPartialVariableAssignment* hint = model->mutable_solution_hint();
    for (int i = 0; i < hint->vars_size(); ++i) {
      const int col = hint->vars(i);
      if (col >= 0 && col < num_cols) {
        hint->set_values(i, hint->values(i) - offsets[col]);
      }
    }
  }

  if (logger != nullptr && num_shifted > 0) {
    SOLVER_LOG(logger, "[Shifting] Shifted ", FormatCounter(num_shifted),
               " variable towards zero with max shift magnitude ", max_shift,
               ".");
  }

  return true;
}

std::vector<double> ShiftVariableBounds(
    const std::function<double(const sat::MipVariableProto&)>& get_var_shift,
    sat::MPModelProtoWrapper* model, SolverLogger* logger) {
  const int num_cols = model->num_variables();
  std::vector<double> offsets(num_cols, 0.0);
  for (int col = 0; col < num_cols; ++col) {
    offsets[col] = get_var_shift(model->variables(col));
  }
  if (!ShiftVariableBounds(offsets, model, logger)) return {};
  return offsets;
}

std::vector<double> ShiftToZeroAndMarkAsIntegerUnusedVariables(
    const sat::SatParameters& params, sat::MPModelProtoWrapper* model,
    SolverLogger* logger) {
  if (params.keep_all_feasible_solutions_in_presolve()) return {};

  const int num_variables = model->num_variables();
  std::vector<bool> is_used(num_variables, false);
  const int num_constraints = model->num_constraints();
  for (int c = 0; c < num_constraints; ++c) {
    const sat::EnforcedLinearConstraintProto& ct = model->constraints(c);
    if (ct.lower_bound() == -kInfinity && ct.upper_bound() == kInfinity) {
      continue;
    }
    for (int i = 0; i < ct.vars().size(); ++i) {
      if (ct.coeffs(i) != 0.0) {
        is_used[ct.vars(i)] = true;
      }
    }
    for (const int var : ct.enforcement_vars()) {
      is_used[var] = true;
    }
  }

  double max_shift = 0.0;
  std::vector<int> vars_to_fix;
  std::vector<double> offsets(num_variables, 0.0);
  for (int v = 0; v < num_variables; ++v) {
    if (is_used[v]) continue;
    const sat::MipVariableProto& var = model->variables(v);
    if (var.lower_bound() == 0.0 && var.upper_bound() == 0.0 &&
        var.is_integer() && var.objective_coefficient() == 0.0) {
      continue;
    }

    double target = 0.0;
    if (var.objective_coefficient() > 0.0) {
      target = var.lower_bound();
    } else if (var.objective_coefficient() < 0.0) {
      target = var.upper_bound();
    } else if (var.lower_bound() > 0.0) {
      target = var.lower_bound();
    } else if (var.upper_bound() < 0.0) {
      target = var.upper_bound();
    }
    if (!std::isfinite(target)) continue;

    vars_to_fix.push_back(v);
    if (target != 0.0) {
      offsets[v] = target;
      max_shift = std::max(max_shift, std::abs(target));
    }
  }
  if (vars_to_fix.empty()) return {};

  if (max_shift > 0.0) {
    if (!ShiftVariableBounds(offsets, model, /*logger=*/nullptr)) return {};
  } else {
    offsets.clear();
  }

  for (const int v : vars_to_fix) {
    sat::MipVariableProto* mutable_var = model->mutable_variables(v);
    mutable_var->set_lower_bound(0.0);
    mutable_var->set_upper_bound(0.0);
    mutable_var->set_is_integer(true);
    mutable_var->clear_objective_coefficient();
  }
  if (logger != nullptr) {
    SOLVER_LOG(logger, "[UnusedVariables] Fixed to zero and marked as integer ",
               FormatCounter(vars_to_fix.size()),
               " unused variables with max shift magnitude ", max_shift, ".");
  }
  return offsets;
}

// --------------------------------------------------------
// ClearTrivialConstraints
// --------------------------------------------------------

void ClearTrivialConstraints(sat::MPModelProtoWrapper* model,
                             SolverLogger* logger) {
  int64_t num_relaxed_bounds = 0;
  int64_t num_trivial_constraints = 0;
  int64_t num_removed_enforcements = 0;
  TightSum min_activity;
  TightSum max_activity;
  const int num_constraints = model->num_constraints();
  for (int c = 0; c < num_constraints; ++c) {
    sat::EnforcedLinearConstraintProto* ct = model->mutable_constraints(c);
    const double ct_lb = ct->lower_bound();
    const double ct_ub = ct->upper_bound();
    bool check_lb = ct_lb > -kInfinity;
    bool check_ub = ct_ub < kInfinity;
    if (!check_lb && !check_ub) {
      if (!ct->vars().empty() || !ct->enforcement_vars().empty()) {
        sat::ClearConstraintToAlwaysTrue(ct);
        ++num_trivial_constraints;
      }
      continue;
    }

    // Remove the enforcement literals that always hold, and clear the
    // constraint if one of them can never hold. Note that the int64 to double
    // conversion is monotonic, so the strict comparisons below are exact.
    const int num_enforcements = ct->enforcement_vars().size();
    CHECK_EQ(num_enforcements, ct->enforcement_values().size());
    bool never_enforced = false;
    int new_num_enforcements = 0;
    for (int i = 0; i < num_enforcements; ++i) {
      const int var_index = ct->enforcement_vars(i);
      const sat::MipVariableProto& var = model->variables(var_index);
      const int64_t value = ct->enforcement_values(i);
      const double value_as_double = static_cast<double>(value);
      if (value_as_double < var.lower_bound() ||
          value_as_double > var.upper_bound()) {
        never_enforced = true;
        break;
      }
      if (var.lower_bound() == var.upper_bound() &&
          value_as_double == var.lower_bound() &&
          value_as_double == SafeDoubleToInt64(value_as_double)) {
        continue;
      }
      ct->set_enforcement_vars(new_num_enforcements, var_index);
      ct->set_enforcement_values(new_num_enforcements, value);
      ++new_num_enforcements;
    }
    if (never_enforced) {
      sat::ClearConstraintToAlwaysTrue(ct);
      ++num_trivial_constraints;
      continue;
    }
    num_removed_enforcements += num_enforcements - new_num_enforcements;
    ct->mutable_enforcement_vars()->Truncate(new_num_enforcements);
    ct->mutable_enforcement_values()->Truncate(new_num_enforcements);

    if (check_lb) min_activity.Clear();
    if (check_ub) max_activity.Clear();
    const int num_terms = ct->vars().size();
    bool invalid_var_bounds = false;
    for (int i = 0; i < num_terms; ++i) {
      const double coeff = ct->coeffs(i);
      if (coeff == 0.0) continue;
      const sat::MipVariableProto& var = model->variables(ct->vars(i));
      double lb = var.lower_bound();
      double ub = var.upper_bound();
      if (lb > ub) {
        invalid_var_bounds = true;
        break;
      }
      if (coeff < 0.0) std::swap(lb, ub);
      if (check_lb) {
        if (!std::isfinite(lb)) {
          check_lb = false;
        } else if (lb != 0.0) {
          min_activity.AddProd(coeff, lb);
        }
      }
      if (check_ub) {
        if (!std::isfinite(ub)) {
          check_ub = false;
        } else if (ub != 0.0) {
          max_activity.AddProd(coeff, ub);
        }
      }
      if (!check_lb && !check_ub) break;
    }
    if (invalid_var_bounds) continue;

    const bool relaxed_lb = check_lb && min_activity.TightLowerBound() >= ct_lb;
    const bool relaxed_ub = check_ub && max_activity.TightUpperBound() <= ct_ub;
    if (relaxed_lb) ct->set_lower_bound(-kInfinity);
    if (relaxed_ub) ct->set_upper_bound(kInfinity);
    if (ct->lower_bound() == -kInfinity && ct->upper_bound() == kInfinity) {
      sat::ClearConstraintToAlwaysTrue(ct);
      ++num_trivial_constraints;
    } else if (relaxed_lb || relaxed_ub) {
      ++num_relaxed_bounds;
    }
  }
  if (logger != nullptr &&
      (num_trivial_constraints > 0 || num_relaxed_bounds > 0)) {
    SOLVER_LOG(logger, "[TrivialConstraints] Cleared ",
               FormatCounter(num_trivial_constraints),
               " trivial constraints and relaxed ",
               FormatCounter(num_relaxed_bounds), " constraint bounds.");
  }
  if (logger != nullptr && num_removed_enforcements > 0) {
    SOLVER_LOG(logger, "[TrivialConstraints] Removed ",
               FormatCounter(num_removed_enforcements),
               " enforcement literals on fixed variables.");
  }
}

// --------------------------------------------------------
// ExactPresolve
// --------------------------------------------------------

bool ExactPresolve(const sat::SatParameters& params,
                   sat::MPModelProtoWrapper* mp_model_wrapper,
                   SolverLogger* logger) {
  // For now we use the same variable/constraint tolerance and use the tight
  // one.
  const double tolerance = params.mip_tight_precision();

  // Enlarge the variables by tolerance. The algorithm will directly work on
  // these vectors. We will push the computed bounds at the end.
  //
  // Note that using the tolerance initially allows chaining propagation in
  // an exact way (without re-adding tolerance in the middle).
  const int num_variables = mp_model_wrapper->num_variables();
  std::vector<double> var_lbs(num_variables);
  std::vector<double> var_ubs(num_variables);
  for (int v = 0; v < num_variables; ++v) {
    const sat::MipVariableProto& var_proto = mp_model_wrapper->variables(v);
    var_lbs[v] = ExactSumLb(var_proto.lower_bound(), -tolerance);
    var_ubs[v] = ExactSumUb(var_proto.upper_bound(), tolerance);
    if (var_proto.is_integer()) {
      var_lbs[v] = std::ceil(var_lbs[v]);
      var_ubs[v] = std::floor(var_ubs[v]);
    }
    if (var_lbs[v] > var_ubs[v]) {
      if (logger != nullptr) {
        SOLVER_LOG(logger, "Variable ", v,
                   " has infeasible bound at the beginning of ExactPresolve: [",
                   var_proto.lower_bound(), ",", var_proto.upper_bound(), "]");
      }
      return false;
    }
  }

  ClearTrivialConstraints(mp_model_wrapper, logger);

  // Save the constraint bounds, and enlarge the current ones by tolerance. We
  // will restore them at the end. This allows not worrying about tolerance in
  // the algorithms. But we still keep the old bounds at the end because some
  // reasoning does need them.
  const int num_constraints = mp_model_wrapper->num_constraints();
  std::vector<double> saved_constraint_lb(num_constraints);
  std::vector<double> saved_constraint_ub(num_constraints);
  for (int c = 0; c < num_constraints; ++c) {
    sat::EnforcedLinearConstraintProto* ct =
        mp_model_wrapper->mutable_constraints(c);
    saved_constraint_lb[c] = ct->lower_bound();
    saved_constraint_ub[c] = ct->upper_bound();
  }

  // PropagateBounds() reach a fix point by itself, but this is not the case of
  // RestrictBoundsWithDualReasoning(). And also, if we restricted bounds with
  // dual reasoning, we might in turn propagate more in PropagateBounds(). We do
  // a few passes and abort early if nothing changed.
  for (int iter = 0; iter < 3; ++iter) {
    for (int c = 0; c < num_constraints; ++c) {
      sat::EnforcedLinearConstraintProto* ct =
          mp_model_wrapper->mutable_constraints(c);
      ct->set_lower_bound(ExactSumLb(saved_constraint_lb[c], -tolerance));
      ct->set_upper_bound(ExactSumUb(saved_constraint_ub[c], tolerance));
    }

    const bool feasible =
        PropagateBounds(*mp_model_wrapper, absl::MakeSpan(var_lbs),
                        absl::MakeSpan(var_ubs), logger);

    // Restore constraint bounds.
    for (int c = 0; c < num_constraints; ++c) {
      sat::EnforcedLinearConstraintProto* ct =
          mp_model_wrapper->mutable_constraints(c);
      ct->set_lower_bound(saved_constraint_lb[c]);
      ct->set_upper_bound(saved_constraint_ub[c]);
    }
    if (!feasible) return false;

    // Note that we remove the constraint tolerance before this, so that we
    // don't exclude points allowing to satisfy the initial constraint
    // exactly. For instance if we just have X >= 1.5, this will propagate
    // X <= 1.5, not X <= 1.5 - tolerance!
    if (!sat::RestrictBoundsWithDualReasoning(
            params, *mp_model_wrapper, absl::MakeSpan(var_lbs),
            absl::MakeSpan(var_ubs), logger)) {
      break;
    }
  }

  // Integrate the propagated bounds. Note that to keep things simple for
  // things like implied variable detection, we always enlarge them to the
  // nearest integer. So we always propagate INTEGER bounds.
  int num_changed = 0;
  double min_of_all_domains = kInfinity;
  double max_of_all_domains = -kInfinity;
  double max_change = 0.0;
  for (int v = 0; v < num_variables; ++v) {
    sat::MipVariableProto* var_proto = mp_model_wrapper->mutable_variables(v);
    var_lbs[v] = std::floor(var_lbs[v]);
    var_ubs[v] = std::ceil(var_ubs[v]);

    if (var_lbs[v] > var_proto->lower_bound()) {
      ++num_changed;
      max_change = std::max(max_change, var_lbs[v] - var_proto->lower_bound());
      var_proto->set_lower_bound(var_lbs[v]);
    }
    if (var_ubs[v] < var_proto->upper_bound()) {
      ++num_changed;
      max_change = std::max(max_change, var_proto->upper_bound() - var_ubs[v]);
      var_proto->set_upper_bound(var_ubs[v]);
    }
    min_of_all_domains = std::min(min_of_all_domains, var_proto->lower_bound());
    max_of_all_domains = std::max(max_of_all_domains, var_proto->upper_bound());
  }
  if (num_changed > 0) {
    if (logger != nullptr) {
      SOLVER_LOG(logger, "[ExactPresolve] ", FormatCounter(num_changed),
                 " bounds were tightened. Max change ",
                 RoundTripDoubleFormat(max_change), ". All variables in [",
                 min_of_all_domains, ",", max_of_all_domains, "].");
    }
    ClearTrivialConstraints(mp_model_wrapper, logger);
  }

  return true;
}

// --------------------------------------------------------
// MergeExactDuplicateColumns
// --------------------------------------------------------

namespace {

// Anchor point in [lb, ub] closest to 0.0 (always finite when lb < ub).
double BaseValue(double lb, double ub) {
  if (lb <= 0.0 && 0.0 <= ub) return 0.0;
  return lb > 0.0 ? lb : ub;
}

// Only merge non-fixed continuous variables; duplicate integer/Boolean columns
// are already merged during CP-SAT's integer presolve
// (`CpModelPresolver::DetectDuplicateColumns`), where [0, 1] `at_most_one`
// structure is preserved.
bool IsEligibleForDuplicateMerge(const sat::MipVariableProto& var) {
  return !var.is_integer() && var.lower_bound() < var.upper_bound();
}

bool FiniteSumWouldOverflowDouble(const TightSum& sum, double x) {
  if (!std::isfinite(x) || !sum.IsFinite()) return false;
  const auto [cur_lb, cur_ub] = sum.TightBounds();
  return !std::isfinite(SumBounds(cur_lb, x).first) ||
         !std::isfinite(SumBounds(cur_ub, x).second);
}

struct ColTerm {
  int constraint;
  double coeff;

  bool operator==(const ColTerm& other) const {
    return constraint == other.constraint && coeff == other.coeff;
  }

  template <typename H>
  friend H AbslHashValue(H h, const ColTerm& t) {
    return H::combine(std::move(h), t.constraint, t.coeff);
  }
};

struct ColumnKey {
  double obj_coeff;
  // Points into `columns`, which outlives the hash map.
  absl::Span<const ColTerm> terms;

  bool operator==(const ColumnKey& other) const {
    return obj_coeff == other.obj_coeff && terms == other.terms;
  }

  template <typename H>
  friend H AbslHashValue(H h, const ColumnKey& key) {
    return H::combine(std::move(h), key.obj_coeff, key.terms);
  }
};

void UpdateSolutionHintForMergedColumns(
    absl::Span<const std::vector<DuplicateColumnsPostsolve::MergedVar>> classes,
    sat::MPModelProtoWrapper* model) {
  if (!model->has_solution_hint()) return;
  const int num_variables = model->num_variables();
  sat::MipPartialVariableAssignment* hint = model->mutable_solution_hint();
  const int hint_size = std::min(hint->vars_size(), hint->values_size());
  std::vector<std::optional<double>> hint_value(num_variables);
  for (int i = 0; i < hint_size; ++i) {
    const int var = hint->vars(i);
    if (var >= 0 && var < num_variables) {
      hint_value[var] = hint->values(i);
    }
  }
  std::vector<int> extra_hint_vars;
  for (const std::vector<DuplicateColumnsPostsolve::MergedVar>& members :
       classes) {
    bool any_hinted = false;
    double sum = 0.0;
    for (const DuplicateColumnsPostsolve::MergedVar& m : members) {
      if (hint_value[m.var].has_value()) any_hinted = true;
      sum += hint_value[m.var].value_or(BaseValue(m.lb, m.ub));
    }
    if (!any_hinted) continue;
    const int rep = members[0].var;
    const sat::MipVariableProto& rep_var = model->variables(rep);
    const double clamped =
        std::clamp(sum, rep_var.lower_bound(), rep_var.upper_bound());
    if (!hint_value[rep].has_value()) {
      extra_hint_vars.push_back(rep);
    }
    hint_value[rep] = clamped;
    for (size_t k = 1; k < members.size(); ++k) {
      if (hint_value[members[k].var].has_value()) {
        hint_value[members[k].var] = 0.0;
      }
    }
  }
  for (int i = 0; i < hint_size; ++i) {
    const int var = hint->vars(i);
    if (var >= 0 && var < num_variables && hint_value[var].has_value()) {
      hint->set_values(i, *hint_value[var]);
    }
  }
  for (const int var : extra_hint_vars) {
    hint->add_vars(var);
    hint->add_values(*hint_value[var]);
  }
}

}  // namespace

void DuplicateColumnsPostsolve::Postsolve(absl::Span<double> solution) const {
  const int solution_size = solution.size();
  std::vector<int> order;
  TightSum rem_sum;
  for (const std::vector<MergedVar>& members : classes_) {
    if (members.size() < 2) continue;
    const int rep = members[0].var;
    CHECK_GE(rep, 0);
    CHECK_LT(rep, solution_size);

    const double total = solution[rep];
    rem_sum.Clear();
    rem_sum.Add(total);
    order.resize(members.size());
    for (int i = 0; i < static_cast<int>(members.size()); ++i) {
      const MergedVar& m = members[i];
      CHECK_GE(m.var, 0);
      CHECK_LT(m.var, solution_size);
      const double base = BaseValue(m.lb, m.ub);
      solution[m.var] = base;
      rem_sum.Add(-base);
      order[i] = i;
    }

    if (rem_sum > 0.0) {
      absl::c_stable_sort(order, [&](int a, int b) {
        const double cap_a =
            members[a].ub - BaseValue(members[a].lb, members[a].ub);
        const double cap_b =
            members[b].ub - BaseValue(members[b].lb, members[b].ub);
        return cap_a > cap_b;
      });
      for (const int idx : order) {
        const MergedVar& m = members[idx];
        const double base = BaseValue(m.lb, m.ub);
        if (m.ub <= base) continue;
        rem_sum.Add(base);
        if (!std::isfinite(m.ub) || rem_sum <= m.ub) {
          solution[m.var] =
              std::clamp(rem_sum.TightLowerBound(), m.lb,
                         std::min(m.ub, std::numeric_limits<double>::max()));
          break;
        }
        solution[m.var] = m.ub;
        rem_sum.Add(-m.ub);
      }
    } else if (rem_sum < 0.0) {
      absl::c_stable_sort(order, [&](int a, int b) {
        const double cap_a =
            BaseValue(members[a].lb, members[a].ub) - members[a].lb;
        const double cap_b =
            BaseValue(members[b].lb, members[b].ub) - members[b].lb;
        return cap_a > cap_b;
      });
      for (const int idx : order) {
        const MergedVar& m = members[idx];
        const double base = BaseValue(m.lb, m.ub);
        if (m.lb >= base) continue;
        rem_sum.Add(base);
        if (!std::isfinite(m.lb) || rem_sum >= m.lb) {
          solution[m.var] = std::clamp(
              rem_sum.TightUpperBound(),
              std::max(m.lb, -std::numeric_limits<double>::max()), m.ub);
          break;
        }
        solution[m.var] = m.lb;
        rem_sum.Add(-m.lb);
      }
    }
  }
}

DuplicateColumnsPostsolve MergeExactDuplicateColumns(
    const sat::SatParameters& params, sat::MPModelProtoWrapper* model,
    SolverLogger* logger) {
  if (params.keep_all_feasible_solutions_in_presolve() ||
      params.enumerate_all_solutions()) {
    return {};
  }

  const int num_variables = model->num_variables();
  std::vector<bool> eligible(num_variables, false);
  int num_potentially_eligible = 0;
  for (int v = 0; v < num_variables; ++v) {
    if (IsEligibleForDuplicateMerge(model->variables(v))) {
      eligible[v] = true;
      ++num_potentially_eligible;
    }
  }
  if (num_potentially_eligible < 2) return {};

  const int num_constraints = model->num_constraints();
  sat::CompactVectorVectorBuilder<int, ColTerm> column_builder;
  for (int c = 0; c < num_constraints; ++c) {
    const sat::EnforcedLinearConstraintProto& ct = model->constraints(c);
    if (ct.lower_bound() == -kInfinity && ct.upper_bound() == kInfinity) {
      continue;
    }
    for (const int enf_var : ct.enforcement_vars()) {
      eligible[enf_var] = false;
    }
    const int num_terms = ct.vars().size();
    for (int i = 0; i < num_terms; ++i) {
      const int var = ct.vars(i);
      if (!eligible[var]) continue;
      const double coeff = ct.coeffs(i);
      if (coeff == 0.0) continue;
      column_builder.Add(var, ColTerm{.constraint = c, .coeff = coeff});
    }
  }
  const sat::CompactVectorVector<int, ColTerm> columns(
      std::move(column_builder), num_variables);
  if (columns.num_entries() == 0) return {};

  // Note that constraint enforcement belongs to rows (`ct.enforcement_vars()`),
  // and variables used as enforcement literals are excluded via `eligible`.
  // Two variables with equal `ColumnKey` appear with identical coefficients in
  // the exact same set of constraints (enforced or not), so they are governed
  // by the exact same enforcement conditions in every row.
  std::vector<std::vector<DuplicateColumnsPostsolve::MergedVar>> raw_groups;
  absl::flat_hash_map<ColumnKey, int> group_by_column;
  for (int v = 0; v < num_variables; ++v) {
    const absl::Span<const ColTerm> terms = columns[v];
    if (!eligible[v] || terms.empty()) continue;

    const sat::MipVariableProto& var = model->variables(v);
    const double lb = var.lower_bound();
    const double ub = var.upper_bound();
    // Normalize -0.0 to +0.0 so both representations share the same key.
    const double obj_coeff =
        var.objective_coefficient() == 0.0 ? 0.0 : var.objective_coefficient();
    const ColumnKey key{.obj_coeff = obj_coeff, .terms = terms};

    const auto [it, inserted] =
        group_by_column.try_emplace(key, raw_groups.size());
    if (inserted) {
      raw_groups.push_back({{.var = v, .lb = lb, .ub = ub}});
    } else {
      raw_groups[it->second].push_back({.var = v, .lb = lb, .ub = ub});
    }
  }

  std::vector<bool> is_absorbed(num_variables, false);
  int64_t num_absorbed = 0;
  std::vector<std::vector<DuplicateColumnsPostsolve::MergedVar>> classes;
  TightSum lb_sum;
  TightSum ub_sum;
  TightSum base_sum;
  const auto finalize_class =
      [&](std::vector<DuplicateColumnsPostsolve::MergedVar>& current_members) {
        if (current_members.size() < 2) return;
        sat::MipVariableProto* rep_var =
            model->mutable_variables(current_members[0].var);
        rep_var->set_lower_bound(lb_sum.TightLowerBound());
        rep_var->set_upper_bound(ub_sum.TightUpperBound());
        for (size_t k = 1; k < current_members.size(); ++k) {
          const int absorbed = current_members[k].var;
          is_absorbed[absorbed] = true;
          ++num_absorbed;
          sat::MipVariableProto* dup_var = model->mutable_variables(absorbed);
          dup_var->set_lower_bound(0.0);
          dup_var->set_upper_bound(0.0);
          dup_var->set_is_integer(true);
          dup_var->clear_objective_coefficient();
        }
        classes.push_back(std::move(current_members));
      };

  for (const std::vector<DuplicateColumnsPostsolve::MergedVar>& group :
       raw_groups) {
    if (group.size() < 2) continue;
    lb_sum.Clear();
    ub_sum.Clear();
    base_sum.Clear();
    lb_sum.Add(group[0].lb);
    ub_sum.Add(group[0].ub);
    base_sum.Add(BaseValue(group[0].lb, group[0].ub));
    std::vector<DuplicateColumnsPostsolve::MergedVar> current_members = {
        group[0]};
    for (size_t i = 1; i < group.size(); ++i) {
      const DuplicateColumnsPostsolve::MergedVar& m = group[i];
      const double base = BaseValue(m.lb, m.ub);
      if (FiniteSumWouldOverflowDouble(lb_sum, m.lb) ||
          FiniteSumWouldOverflowDouble(ub_sum, m.ub) ||
          FiniteSumWouldOverflowDouble(base_sum, base)) {
        finalize_class(current_members);
        lb_sum.Clear();
        ub_sum.Clear();
        base_sum.Clear();
        lb_sum.Add(m.lb);
        ub_sum.Add(m.ub);
        base_sum.Add(base);
        current_members = {m};
      } else {
        lb_sum.Add(m.lb);
        ub_sum.Add(m.ub);
        base_sum.Add(base);
        current_members.push_back(m);
      }
    }
    finalize_class(current_members);
  }
  if (classes.empty()) return {};
  absl::c_sort(classes,
               [](const std::vector<DuplicateColumnsPostsolve::MergedVar>& a,
                  const std::vector<DuplicateColumnsPostsolve::MergedVar>& b) {
                 return a[0].var < b[0].var;
               });

  for (int c = 0; c < num_constraints; ++c) {
    sat::EnforcedLinearConstraintProto* ct = model->mutable_constraints(c);
    const int num_terms = ct->vars().size();
    int new_size = 0;
    for (int i = 0; i < num_terms; ++i) {
      const int var = ct->vars(i);
      if (is_absorbed[var]) continue;
      if (new_size != i) {
        ct->set_vars(new_size, var);
        ct->set_coeffs(new_size, ct->coeffs(i));
      }
      ++new_size;
    }
    if (new_size < num_terms) {
      sat::ResizeConstraintTerms(new_size, ct);
    }
  }

  UpdateSolutionHintForMergedColumns(classes, model);

  if (logger != nullptr) {
    SOLVER_LOG(logger, "[DuplicateColumns] Merged ",
               FormatCounter(num_absorbed), " duplicate variables across ",
               FormatCounter(classes.size()), " equivalence classes.");
  }

  return DuplicateColumnsPostsolve(std::move(classes));
}

}  // namespace operations_research

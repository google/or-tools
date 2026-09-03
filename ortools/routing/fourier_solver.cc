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

// Implementation of local search filters for routing models.

#include "ortools/routing/fourier_solver.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <numeric>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/numeric/bits.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/types.h"

namespace operations_research::routing {

MaxLinearExpressionEvaluator::MaxLinearExpressionEvaluator(
    const std::vector<std::vector<double>>& rows)
    : num_variables_(rows.empty() ? 0 : rows[0].size()),
      num_rows_(rows.size()) {
  if (num_variables_ == 0) return;
  // Map rows to blocks, see class documentation.
  const int64_t num_block_rows = (num_rows_ + kBlockSize - 1) / kBlockSize;
  blocks_.resize(num_block_rows * num_variables_);
  const int64_t num_full_block_rows = num_rows_ / kBlockSize;
  for (int64_t br = 0; br < num_full_block_rows; ++br) {
    for (int64_t v = 0; v < num_variables_; ++v) {
      Block& block = blocks_[br * num_variables_ + v];
      for (int c = 0; c < kBlockSize; ++c) {
        block.coefficients[c] = rows[br * kBlockSize + c][v];
      }
    }
  }
  // If the block representation represents more rows than the original matrix,
  // i.e. if num_rows_ % kBlockSize != 0, we need to initialize it and to pad
  // non-existent rows.
  if (num_full_block_rows == num_block_rows) return;  // No padding needed.
  DCHECK_EQ(num_full_block_rows + 1, num_block_rows);
  Block* last_block_row = &blocks_[num_full_block_rows * num_variables_];
  const int first_coefficient_to_pad =
      num_rows_ - num_full_block_rows * kBlockSize;
  for (int64_t v = 0; v < num_variables_; ++v) {
    Block& block = last_block_row[v];
    // Copy original coefficients.
    for (int c = 0; c < first_coefficient_to_pad; ++c) {
      const int64_t r = num_full_block_rows * kBlockSize + c;
      block.coefficients[c] = rows[r][v];
    }
    // Pad coefficients with a copy of the first coefficient.
    for (int c = first_coefficient_to_pad; c < kBlockSize; ++c) {
      block.coefficients[c] = block.coefficients[0];
    }
  }
}

double MaxLinearExpressionEvaluator::Evaluate(
    absl::Span<const double> values) const {
  DCHECK_EQ(values.size(), num_variables_);
  constexpr double kInfinity = std::numeric_limits<double>::infinity();
  if (num_rows_ == 0) return -kInfinity;
  if (num_variables_ == 0) return 0.0;
  DCHECK_EQ(blocks_.size() % num_variables_, 0);
  Block maximums;
  absl::c_fill(maximums.coefficients, -kInfinity);
  for (auto row = blocks_.begin(); row != blocks_.end();
       row += num_variables_) {
    Block evaluations;
    absl::c_fill(evaluations.coefficients, 0.0);
    for (int64_t v = 0; v < num_variables_; ++v) {
      evaluations.BlockMultiplyAdd(row[v], values[v]);
    }
    maximums.MaximumWith(evaluations);
  }
  return maximums.Maximum();
}

FourierSolver::FourierSolver() {
  status_ = Status::kModeling;
  num_variables_ = 0;
  // The first variable is the objective variable z.
  // The first constraint is the objective constraint
  // z - sum(objective_coefficient * variable) >= 0.
  // The objective coefficients are set by the user before Solve(), but to allow
  // easier scaling, the coefficient of z is 0 until it is set to 1 at Solve().
  AddVariable(-kInfinity, kInfinity, /*is_symbolic=*/true);
  constraints_.push_back(
      {.coefs = {}, .lb = 0, .ub = kInfinity, .combination = 0});
}

FourierSolver::ColIndex FourierSolver::AddVariable(double lb, double ub,
                                                   bool is_symbolic) {
  DCHECK(status_ == Status::kModeling);
  variable_is_symbolic_.push_back(is_symbolic);
  variable_bounds_.push_back({.lb = lb, .ub = ub});
  return ColIndex(num_variables_++);
}

FourierSolver::RowIndex FourierSolver::AddConstraint(
    double lb, double ub, std::vector<CoefficientVariable> coefficients) {
  DCHECK(status_ == Status::kModeling);
  const RowIndex row(constraints_.size());
  constraints_.push_back({
      .coefs = util_intops::StrongVector<ColIndex, double>(num_variables_, 0.0),
      .lb = lb,
      .ub = ub,
      .combination = 0,
  });
  Constraint& constraint = constraints_.back();
  for (const auto& [coef, col] : coefficients) {
    DCHECK(std::isfinite(coef));
    DCHECK_LT(col.value(), num_variables_);
    constraint.coefs[col] = coef;
  }
  return row;
}

FourierSolver::RowIndex FourierSolver::AddConstraint(
    double lb, double ub,
    std::initializer_list<CoefficientVariable> coefficients) {
  return AddConstraint(lb, ub, std::vector<CoefficientVariable>(coefficients));
}

void FourierSolver::SetObjectiveCoefficient(ColIndex col, double coef) {
  DCHECK(status_ == Status::kModeling);
  DCHECK_LT(col.value(), num_variables_);
  DCHECK(std::isfinite(coef));
  if (coef == 0.0) return;
  Constraint& constraint = constraints_.front();
  if (constraint.coefs.size() <= col.value()) {
    constraint.coefs.resize(num_variables_, 0.0);
  }
  constraint.coefs[col] = -coef;
}

void FourierSolver::RemoveDominatedConstraints() {
  absl::flat_hash_map<util_intops::StrongVector<ColIndex, double>, double>
      constraint_to_lb;
  const RowIndex num_constraints(constraints_.size());
  for (RowIndex r(0); r < num_constraints; ++r) {
    const auto& ct = constraints_[r];
    const bool exists = constraint_to_lb.contains(ct.coefs);
    if (!exists || (exists && constraint_to_lb[ct.coefs] < ct.lb)) {
      constraint_to_lb[ct.coefs] = ct.lb;
    }
  }
  constraints_.clear();
  for (auto& [ct, lb] : constraint_to_lb) {
    constraints_.push_back({
        .coefs = ct,
        .lb = lb,
        .ub = kInfinity,
    });
  }
}

double FourierSolver::RescaleConstraint(FourierSolver::Constraint& ct) {
  auto& [coefs, lb, ub, _combination] = ct;
  int64_t scale = 0;
  for (const double coef : coefs) {
    // Reminder: std::gcd(0, x) = abs(x).
    const int64_t i64_coef = static_cast<int64_t>(coef);
    if (static_cast<double>(i64_coef) != coef) return 1.0;
    scale = std::gcd(scale, i64_coef);
  }
  if (scale == 0 || scale == 1) return 1.0;
  for (const double bound : {lb, ub}) {
    if (!std::isfinite(bound)) continue;
    const int64_t i64_bound = static_cast<int64_t>(bound);
    if (static_cast<double>(i64_bound) != bound) return 1.0;
    scale = std::gcd(scale, static_cast<int64_t>(bound));
  }
  for (double& coef : coefs) coef /= scale;
  lb /= scale;
  ub /= scale;
  return static_cast<double>(scale);
}

bool FourierSolver::PreprocessConstraints() {
  // Normalize all constraints to have num_variables_ coefficients.
  const ColIndex num_vars(num_variables_);
  for (Constraint& ct : constraints_) {
    if (ct.coefs.size() < num_vars.value()) ct.coefs.resize(num_vars, 0.0);
  }

  // Rescale objective constraint, set objective variable ColIndex(0).
  objective_scale_ = RescaleConstraint(constraints_.front());
  constraints_.front().coefs[ColIndex(0)] = 1.0;

  // Normalize into lb <= linear expr, do not keep trivial constraints.
  // We use zero_constraints_ as temporary storage for normalized constraints.
  auto change_ub_constraint_to_lb = [](Constraint& ct) {
    ct.lb = -ct.ub;
    ct.ub = kInfinity;
    for (double& coef : ct.coefs) coef = -coef;
  };
  zero_constraints_.clear();
  for (auto& ct : constraints_) {
    // TODO(b/492476073): this seems specific to integer values. Why do we want
    // to normalize coefficients?
    // Normalize coefficients near 0 to 0.0.
    int num_nonzeros = 0;
    for (double& coef : ct.coefs) {
      if (std::abs(coef) < 1e-9) {
        coef = 0.0;
      } else {
        ++num_nonzeros;
      }
    }
    // If constraint is infeasible, return false.
    bool is_feasible = true;
    if (num_nonzeros == 0) {
      is_feasible &= ct.lb <= 0.0;
      is_feasible &= ct.ub >= 0.0;
    }
    is_feasible &= ct.lb <= ct.ub;
    if (!is_feasible) {
      status_ = Status::kInfeasible;
      return false;
    }
    // If constraint is trivial, remove it.
    // TODO(b/492476073): compute the activity interval of the constraint,
    // the constraint is trivial if that interval is inside ct.bounds.
    if (num_nonzeros == 0 || (!std::isfinite(ct.lb) && !std::isfinite(ct.ub))) {
      continue;
    }
    // Turn general lb/ub constraint into lb-only.
    if (!std::isfinite(ct.lb)) {
      DCHECK(std::isfinite(ct.ub));
      // Add -ub <= -linear_expression.
      zero_constraints_.push_back(std::move(ct));
      change_ub_constraint_to_lb(zero_constraints_.back());
    } else if (std::isfinite(ct.ub)) {
      // Add lb <= linear_expression.
      zero_constraints_.push_back(ct);
      zero_constraints_.back().ub = kInfinity;
      // Add -ub <= -linear_expression.
      zero_constraints_.push_back(std::move(ct));
      change_ub_constraint_to_lb(zero_constraints_.back());
    } else {
      // Add lb <= linear_expression.
      zero_constraints_.push_back(std::move(ct));
      zero_constraints_.back().ub = kInfinity;
    }
  }
  std::swap(constraints_, zero_constraints_);

  // Rescale all user-passed constraints. This is done after the normalization
  // to lb <= linear expr to allow for more rescaling opportunities: maybe
  // gcd(lb, ub) is smaller (worse) than gcd(lb, coefs) or gcd(ub, coefs).
  for (Constraint& ct : constraints_) RescaleConstraint(ct);

  // Add variable bounds as constraints.
  for (ColIndex v{1}; v < num_vars; ++v) {
    const auto& [lb, ub] = variable_bounds_[v];
    if (std::isfinite(lb)) AddConstraint(lb, kInfinity, {{1.0, v}});
    if (std::isfinite(ub)) AddConstraint(-ub, kInfinity, {{-1.0, v}});
  }
  return true;
}

bool FourierSolver::Solve() {
  DCHECK(status_ == Status::kModeling);

  if (!PreprocessConstraints()) return false;

  DCHECK_LE(constraints_.size(), 64);
  for (RowIndex r(0); r < constraints_.size(); ++r) {
    constraints_[r].combination = uint64_t{1} << r.value();
  }

  int num_eliminations = 0;
  // Main Fourier elimination loop: each non-symbolic variable v is eliminated
  // using positive linear combinations of all pairs of constraints where v
  // appears with a different sign coefficient.
  for (int num_remaining_vars = absl::c_count(variable_is_symbolic_, false);
       num_remaining_vars > 0; --num_remaining_vars) {
    // Heuristic: find non-symbolic variable that will cause the smallest amount
    // of new combinations of constraints.
    // TODO(b/492476073): compare other scores, could p * q - p - q be better?
    // TODO(b/492476073): count constraint-wise instead of variable-wise.
    ColIndex min_var{0};
    int64_t min_score = kint64max;
    const int num_vars = num_variables_;
    for (ColIndex v{1}; v < num_vars; ++v) {
      if (variable_is_symbolic_[v]) continue;
      int64_t num_pos = 0;
      int64_t num_neg = 0;
      for (const auto& ct : constraints_) {
        if (ct.coefs[v] > 0.0) ++num_pos;
        if (ct.coefs[v] < 0.0) ++num_neg;
      }
      if (num_pos == 0 && num_neg == 0) continue;
      const int64_t score = num_pos * num_neg;
      if (score < min_score) {
        min_score = score;
        min_var = v;
      }
    }
    if (min_var.value() == 0) break;  // Dependent variables cause early stops.
    if (min_score > 0) ++num_eliminations;

    // Separate constraints by sign of min_var coefficient.
    pos_constraints_.clear();
    neg_constraints_.clear();
    zero_constraints_.clear();
    for (Constraint& ct : constraints_) {
      if (ct.coefs[min_var] > 0.0) {
        pos_constraints_.push_back(std::move(ct));
      } else if (ct.coefs[min_var] < 0.0) {
        neg_constraints_.push_back(std::move(ct));
      } else {
        zero_constraints_.push_back(std::move(ct));
      }
    }
    // Zero constraints are kept in the new system as they are.
    std::swap(constraints_, zero_constraints_);

    // Append all combinations of positive and negative constraints.
    for (const Constraint& pos_ct : pos_constraints_) {
      for (const Constraint& neg_ct : neg_constraints_) {
        // Kohler's rule: if popcount of combination is larger than number
        // of eliminated variables, it is redundant, skip it.
        const uint64_t combination = pos_ct.combination | neg_ct.combination;
        if (absl::popcount(combination) > num_eliminations + 1) continue;

        const int64_t scale =
            std::gcd(static_cast<int64_t>(pos_ct.coefs[min_var]),
                     static_cast<int64_t>(neg_ct.coefs[min_var]));
        const double pos_mult = pos_ct.coefs[min_var] / scale;
        const double neg_mult = -neg_ct.coefs[min_var] / scale;
        // Compute new constraint p_coef * nrow + n_coef * prow. This particular
        // combination should eliminate min_var in the new constraint.
        util_intops::StrongVector<ColIndex, double> new_coefs(num_vars);
        int num_nonzeros = 0;
        for (ColIndex v{0}; v < num_vars; ++v) {
          const double coef =
              pos_mult * neg_ct.coefs[v] + neg_mult * pos_ct.coefs[v];
          if (std::abs(coef) > 1e-9) {
            new_coefs[v] = coef;
            ++num_nonzeros;
          } else {
            new_coefs[v] = 0.0;
          }
        }
        const double new_lb = neg_mult * pos_ct.lb + pos_mult * neg_ct.lb;
        if (num_nonzeros == 0) {
          if (new_lb > 1e-9) {
            status_ = Status::kInfeasible;
            return false;
          }
          continue;  // Constraint is trivial and feasible, skip it.
        }
        // Add and rescale the new constraint.
        DCHECK_EQ(new_coefs[min_var], 0.0);
        constraints_.push_back({
            .coefs = std::move(new_coefs),
            .lb = new_lb,
            .ub = kInfinity,
            .combination = combination,
        });
        RescaleConstraint(constraints_.back());
      }
    }
    // TODO(b/492476073): find why RemoveDominatedConstraints() is wrong at the
    // end of this elimination loop. Incompatible with Kohler heuristic?
  }
  RemoveDominatedConstraints();
  status_ = Status::kFeasible;

  // Copy final expressions to feasibility_evaluator_ and objective_evaluator_.
  // Constraints that have a zero coefficient for the objective are the
  // feasibility constraints, the other are objective constraints.
  //
  // Given values for the symbolic variables, feasibility of the system can be
  // evaluated by computing the maximum violation of feasibility constraints,
  // given by lb - sum c_i x_i.
  //
  // The objective evaluator computes the minimum value the objective can take
  // without violating an objective constraint lb <= C z + sum c_i x_i.
  // This is done by taking the maximum over all (lb - sum c_i x_i) / C.
  //
  // In both cases, the offset is encoded by adding a variable x_0,
  // with coefficient -lb for feasibility constraints, and coefficient
  // lb / C for objective constraints.
  // Calling the evaluators with x_0 = 1 offsets the linear expressions by the
  // correct amount.
  //
  // First, map symbolic variables to a dense interval. Keep 0 for the offset.
  evaluator_index_of_symbolic_variable_.assign(num_variables_, -1);
  int num_symbolic_variables = 1;  // reserve offset.
  for (ColIndex v{1}; v < num_variables_; ++v) {
    if (variable_is_symbolic_[v]) {
      evaluator_index_of_symbolic_variable_[v] = num_symbolic_variables++;
    }
  }
  evaluator_values_.assign(num_symbolic_variables, 0);
  evaluator_values_[0] = 1.0;

  std::vector<std::vector<double>> fea_rows;
  std::vector<std::vector<double>> obj_rows;
  for (const Constraint& ct : constraints_) {
    const bool ct_is_feasibility = ct.coefs[ColIndex(0)] == 0.0;
    const double big_c = ct_is_feasibility ? 1.0 : ct.coefs[ColIndex(0)];
    std::vector<double> new_row(num_symbolic_variables);
    new_row[0] = ct.lb / big_c;
    for (ColIndex v{1}; v < num_variables_; ++v) {
      if (!variable_is_symbolic_[v]) continue;
      const int index = evaluator_index_of_symbolic_variable_[v];
      new_row[index] = -ct.coefs[v] / big_c;
    }
    if (ct_is_feasibility) {
      fea_rows.push_back(std::move(new_row));
    } else {
      obj_rows.push_back(std::move(new_row));
    }
  }
  fea_evaluator_ = std::make_unique<MaxLinearExpressionEvaluator>(fea_rows);
  obj_evaluator_ = std::make_unique<MaxLinearExpressionEvaluator>(obj_rows);
  return true;
}

void FourierSolver::SetSymbolicVariableValue(ColIndex var, double value) {
  DCHECK(status_ == Status::kFeasible);
  DCHECK_LT(var.value(), num_variables_);
  DCHECK(variable_is_symbolic_[var]);
  const int index = evaluator_index_of_symbolic_variable_[var];
  evaluator_values_[index] = value;
}

bool FourierSolver::EvaluateFeasibility() const {
  DCHECK(status_ == Status::kFeasible);
  return fea_evaluator_->Evaluate(evaluator_values_) <= 0.0;
}

double FourierSolver::EvaluateObjective() const {
  DCHECK(status_ == Status::kFeasible);
  return objective_scale_ * obj_evaluator_->Evaluate(evaluator_values_);
}

}  // namespace operations_research::routing

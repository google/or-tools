// Copyright 2010-2021 Google LLC
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

#include "ortools/sat/feasibility_pump.h"

#include <cstdint>
#include <limits>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {
namespace sat {

using glop::ColIndex;
using glop::ConstraintStatus;
using glop::Fractional;
using glop::RowIndex;

const double FeasibilityPump::kCpEpsilon = 1e-4;

FeasibilityPump::FeasibilityPump(Model* model)
    : sat_parameters_(*(model->GetOrCreate<SatParameters>())),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      trail_(model->GetOrCreate<Trail>()),
      integer_encoder_(model->GetOrCreate<IntegerEncoder>()),
      incomplete_solutions_(model->Mutable<SharedIncompleteSolutionManager>()),
      sat_solver_(model->GetOrCreate<SatSolver>()),
      domains_(model->GetOrCreate<IntegerDomains>()),
      mapping_(model->Get<CpModelMapping>()) {
  // Tweak the default parameters to make the solve incremental.
  glop::GlopParameters parameters;
  // Note(user): Primal simplex does better here since we have a limit on
  // simplex iterations. So dual simplex sometimes fails to find a LP feasible
  // solution.
  parameters.set_use_dual_simplex(false);
  parameters.set_max_number_of_iterations(2000);
  simplex_.SetParameters(parameters);
  lp_data_.Clear();
  integer_lp_.clear();
}

FeasibilityPump::~FeasibilityPump() {
  VLOG(1) << "Feasibility Pump Total number of simplex iterations: "
          << total_num_simplex_iterations_;
}

void FeasibilityPump::AddLinearConstraint(const LinearConstraint& ct) {
  // We still create the mirror variable right away though.
  for (const IntegerVariable var : ct.vars) {
    GetOrCreateMirrorVariable(PositiveVariable(var));
  }

  integer_lp_.push_back(LinearConstraintInternal());
  LinearConstraintInternal& new_ct = integer_lp_.back();
  new_ct.lb = ct.lb;
  new_ct.ub = ct.ub;
  const int size = ct.vars.size();
  CHECK_LE(ct.lb, ct.ub);
  for (int i = 0; i < size; ++i) {
    // We only use positive variable inside this class.
    IntegerVariable var = ct.vars[i];
    IntegerValue coeff = ct.coeffs[i];
    if (!VariableIsPositive(var)) {
      var = NegationOf(var);
      coeff = -coeff;
    }
    new_ct.terms.push_back({GetOrCreateMirrorVariable(var), coeff});
  }
  // Important to keep lp_data_ "clean".
  std::sort(new_ct.terms.begin(), new_ct.terms.end());
}

void FeasibilityPump::SetObjectiveCoefficient(IntegerVariable ivar,
                                              IntegerValue coeff) {
  objective_is_defined_ = true;
  const IntegerVariable pos_var =
      VariableIsPositive(ivar) ? ivar : NegationOf(ivar);
  if (ivar != pos_var) coeff = -coeff;

  const auto it = mirror_lp_variable_.find(pos_var);
  if (it == mirror_lp_variable_.end()) return;
  const ColIndex col = it->second;
  integer_objective_.push_back({col, coeff});
  objective_infinity_norm_ =
      std::max(objective_infinity_norm_, IntTypeAbs(coeff));
}

ColIndex FeasibilityPump::GetOrCreateMirrorVariable(
    IntegerVariable positive_variable) {
  DCHECK(VariableIsPositive(positive_variable));

  const auto it = mirror_lp_variable_.find(positive_variable);
  if (it == mirror_lp_variable_.end()) {
    const int model_var =
        mapping_->GetProtoVariableFromIntegerVariable(positive_variable);
    model_vars_size_ = std::max(model_vars_size_, model_var + 1);

    const ColIndex col(integer_variables_.size());
    mirror_lp_variable_[positive_variable] = col;
    integer_variables_.push_back(positive_variable);
    var_is_binary_.push_back(false);
    lp_solution_.push_back(std::numeric_limits<double>::infinity());
    integer_solution_.push_back(0);

    return col;
  }
  return it->second;
}

void FeasibilityPump::PrintStats() {
  if (lp_solution_is_set_) {
    VLOG(2) << "Fractionality: " << lp_solution_fractionality_;
  } else {
    VLOG(2) << "Fractionality: NA";
    VLOG(2) << "simplex status: " << simplex_.GetProblemStatus();
  }

  if (integer_solution_is_set_) {
    VLOG(2) << "#Infeasible const: " << num_infeasible_constraints_;
    VLOG(2) << "Infeasibility: " << integer_solution_infeasibility_;
  } else {
    VLOG(2) << "Infeasibility: NA";
  }
}

bool FeasibilityPump::Solve() {
  if (lp_data_.num_variables() == 0) {
    InitializeWorkingLP();
  }
  UpdateBoundsOfLpVariables();
  lp_solution_is_set_ = false;
  integer_solution_is_set_ = false;

  // Restore the original objective
  for (ColIndex col(0); col < lp_data_.num_variables(); ++col) {
    lp_data_.SetObjectiveCoefficient(col, 0.0);
  }
  for (const auto& term : integer_objective_) {
    lp_data_.SetObjectiveCoefficient(term.first, ToDouble(term.second));
  }

  mixing_factor_ = 1.0;
  for (int i = 0; i < max_fp_iterations_; ++i) {
    if (time_limit_->LimitReached()) break;
    L1DistanceMinimize();
    if (!SolveLp()) break;
    if (lp_solution_is_integer_) break;
    if (!Round()) break;
    // We don't end this loop if the integer solutions is feasible in hope to
    // get better solution.
    if (integer_solution_is_feasible_) MaybePushToRepo();
  }

  if (model_is_unsat_) return false;

  PrintStats();
  MaybePushToRepo();
  return true;
}

void FeasibilityPump::MaybePushToRepo() {
  if (incomplete_solutions_ == nullptr) return;

  std::vector<double> lp_solution(model_vars_size_,
                                  std::numeric_limits<double>::infinity());
  // TODO(user): Consider adding solutions that have low fractionality.
  if (lp_solution_is_integer_) {
    // Fill the solution using LP solution values.
    for (const IntegerVariable positive_var : integer_variables_) {
      const int model_var =
          mapping_->GetProtoVariableFromIntegerVariable(positive_var);
      if (model_var >= 0 && model_var < model_vars_size_) {
        lp_solution[model_var] = GetLPSolutionValue(positive_var);
      }
    }
    incomplete_solutions_->AddNewSolution(lp_solution);
  }

  if (integer_solution_is_feasible_) {
    // Fill the solution using Integer solution values.
    for (const IntegerVariable positive_var : integer_variables_) {
      const int model_var =
          mapping_->GetProtoVariableFromIntegerVariable(positive_var);
      if (model_var >= 0 && model_var < model_vars_size_) {
        lp_solution[model_var] = GetIntegerSolutionValue(positive_var);
      }
    }
    incomplete_solutions_->AddNewSolution(lp_solution);
  }
}

// ----------------------------------------------------------------
// -------------------LPSolving------------------------------------
// ----------------------------------------------------------------

void FeasibilityPump::InitializeWorkingLP() {
  lp_data_.Clear();
  // Create variables.
  for (int i = 0; i < integer_variables_.size(); ++i) {
    CHECK_EQ(ColIndex(i), lp_data_.CreateNewVariable());
    lp_data_.SetVariableType(ColIndex(i),
                             glop::LinearProgram::VariableType::INTEGER);
  }

  // Add constraints.
  for (const LinearConstraintInternal& ct : integer_lp_) {
    const ConstraintIndex row = lp_data_.CreateNewConstraint();
    lp_data_.SetConstraintBounds(row, ToDouble(ct.lb), ToDouble(ct.ub));
    for (const auto& term : ct.terms) {
      lp_data_.SetCoefficient(row, term.first, ToDouble(term.second));
    }
  }

  // Add objective.
  for (const auto& term : integer_objective_) {
    lp_data_.SetObjectiveCoefficient(term.first, ToDouble(term.second));
  }

  const int num_vars = integer_variables_.size();
  for (int i = 0; i < num_vars; i++) {
    const IntegerVariable cp_var = integer_variables_[i];
    const double lb = ToDouble(integer_trail_->LevelZeroLowerBound(cp_var));
    const double ub = ToDouble(integer_trail_->LevelZeroUpperBound(cp_var));
    lp_data_.SetVariableBounds(ColIndex(i), lb, ub);
  }

  objective_normalization_factor_ = 0.0;
  glop::ColIndexVector integer_variables;
  const ColIndex num_cols = lp_data_.num_variables();
  for (ColIndex col : lp_data_.IntegerVariablesList()) {
    var_is_binary_[col.value()] = lp_data_.IsVariableBinary(col);
    if (!var_is_binary_[col.value()]) {
      integer_variables.push_back(col);
    }

    // The aim of this normalization value is to compute a coefficient of the
    // d_i variables that should be minimized.
    objective_normalization_factor_ +=
        std::abs(lp_data_.GetObjectiveCoefficientForMinimizationVersion(col));
  }
  CHECK_GT(lp_data_.IntegerVariablesList().size(), 0);
  objective_normalization_factor_ =
      objective_normalization_factor_ / lp_data_.IntegerVariablesList().size();

  if (!integer_variables.empty()) {
    // Update the LpProblem with norm variables and constraints.
    norm_variables_.assign(num_cols, ColIndex(-1));
    norm_lhs_constraints_.assign(num_cols, RowIndex(-1));
    norm_rhs_constraints_.assign(num_cols, RowIndex(-1));
    // For each integer non-binary variable x_i we introduce one new variable
    // d_i subject to two new constraints:
    //   d_i - x_i >= -round(x'_i)
    //   d_i + x_i >= +round(x'_i)
    // That's round(x'_i) - d_i <= x_i <= round(x'_i) + d_i, where d_i is an
    // unbounded non-negative, and x'_i is the value of variable i from the
    // previous solution obtained during the projection step. Consequently
    // coefficients of the constraints are set here, but bounds of the
    // constraints are updated at each iteration of the feasibility pump. Also
    // coefficients of the objective are set here: x_i's are not present in the
    // objective (i.e., coefficients set to 0.0), and d_i's are present in the
    // objective with coefficients set to 1.0.
    // Note that the treatment of integer non-binary variables is different
    // from the treatment of binary variables. Binary variables do not impose
    // any extra variables, nor extra constraints, but their objective
    // coefficients are changed in the linear projection steps.
    for (const ColIndex col : integer_variables) {
      const ColIndex norm_variable = lp_data_.CreateNewVariable();
      norm_variables_[col] = norm_variable;
      lp_data_.SetVariableBounds(norm_variable, 0.0, glop::kInfinity);
      const RowIndex row_a = lp_data_.CreateNewConstraint();
      norm_lhs_constraints_[col] = row_a;
      lp_data_.SetCoefficient(row_a, norm_variable, 1.0);
      lp_data_.SetCoefficient(row_a, col, -1.0);
      const RowIndex row_b = lp_data_.CreateNewConstraint();
      norm_rhs_constraints_[col] = row_b;
      lp_data_.SetCoefficient(row_b, norm_variable, 1.0);
      lp_data_.SetCoefficient(row_b, col, 1.0);
    }
  }

  scaler_.Scale(&lp_data_);
  lp_data_.AddSlackVariablesWhereNecessary(
      /*detect_integer_constraints=*/false);
}

void FeasibilityPump::L1DistanceMinimize() {
  std::vector<double> new_obj_coeffs(lp_data_.num_variables().value(), 0.0);

  // Set the original subobjective. The coefficients are scaled by mixing factor
  // and the offset remains at 0 (because it does not affect the solution).
  const ColIndex num_cols(lp_data_.objective_coefficients().size());
  for (ColIndex col(0); col < num_cols; ++col) {
    new_obj_coeffs[col.value()] =
        mixing_factor_ * lp_data_.objective_coefficients()[col];
  }

  // Set the norm subobjective. The coefficients are scaled by 1 - mixing factor
  // and the offset remains at 0 (because it does not affect the solution).
  for (const ColIndex col : lp_data_.IntegerVariablesList()) {
    if (var_is_binary_[col.value()]) {
      const Fractional objective_coefficient =
          mixing_factor_ * lp_data_.objective_coefficients()[col] +
          (1 - mixing_factor_) * objective_normalization_factor_ *
              (1 - 2 * integer_solution_[col.value()]);
      new_obj_coeffs[col.value()] = objective_coefficient;
    } else {  // The variable is integer.
      // Update the bounds of the constraints added in
      // InitializeIntegerVariables() (see there for more details):
      //   d_i - x_i >= -round(x'_i)
      //   d_i + x_i >= +round(x'_i)

      // TODO(user): We change both the objective and the bounds, thus
      // breaking the incrementality. Handle integer variables differently,
      // e.g., intensify rounding, or use soft fixing from: Fischetti, Lodi,
      // "Local Branching", Math Program Ser B 98:23-47 (2003).
      const Fractional objective_coefficient =
          (1 - mixing_factor_) * objective_normalization_factor_;
      new_obj_coeffs[norm_variables_[col].value()] = objective_coefficient;
      // At this point, constraint bounds have already been transformed into
      // bounds of slack variables. Instead of updating the constraints, we need
      // to update the slack variables corresponding to them.
      const ColIndex norm_lhs_slack_variable =
          lp_data_.GetSlackVariable(norm_lhs_constraints_[col]);
      const double lhs_scaling_factor =
          scaler_.VariableScalingFactor(norm_lhs_slack_variable);
      lp_data_.SetVariableBounds(
          norm_lhs_slack_variable, -glop::kInfinity,
          lhs_scaling_factor * integer_solution_[col.value()]);
      const ColIndex norm_rhs_slack_variable =
          lp_data_.GetSlackVariable(norm_rhs_constraints_[col]);
      const double rhs_scaling_factor =
          scaler_.VariableScalingFactor(norm_rhs_slack_variable);
      lp_data_.SetVariableBounds(
          norm_rhs_slack_variable, -glop::kInfinity,
          -rhs_scaling_factor * integer_solution_[col.value()]);
    }
  }
  for (ColIndex col(0); col < lp_data_.num_variables(); ++col) {
    lp_data_.SetObjectiveCoefficient(col, new_obj_coeffs[col.value()]);
  }
  // TODO(user): Tune this or expose as parameter.
  mixing_factor_ *= 0.8;
}

bool FeasibilityPump::SolveLp() {
  const int num_vars = integer_variables_.size();
  VLOG(3) << "LP relaxation: " << lp_data_.GetDimensionString() << ".";

  const auto status = simplex_.Solve(lp_data_, time_limit_);
  total_num_simplex_iterations_ += simplex_.GetNumberOfIterations();
  if (!status.ok()) {
    VLOG(1) << "The LP solver encountered an error: " << status.error_message();
    simplex_.ClearStateForNextSolve();
    return false;
  }

  // TODO(user): This shouldn't really happen except if the problem is UNSAT.
  // But we can't just rely on a potentially imprecise LP to close the problem.
  // The rest of the solver should do that with exact precision.
  VLOG(3) << "simplex status: " << simplex_.GetProblemStatus();
  if (simplex_.GetProblemStatus() == glop::ProblemStatus::PRIMAL_INFEASIBLE) {
    return false;
  }

  lp_solution_fractionality_ = 0.0;
  if (simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL ||
      simplex_.GetProblemStatus() == glop::ProblemStatus::DUAL_FEASIBLE ||
      simplex_.GetProblemStatus() == glop::ProblemStatus::PRIMAL_FEASIBLE ||
      simplex_.GetProblemStatus() == glop::ProblemStatus::IMPRECISE) {
    lp_solution_is_set_ = true;
    for (int i = 0; i < num_vars; i++) {
      const double value = GetVariableValueAtCpScale(ColIndex(i));
      lp_solution_[i] = value;
      lp_solution_fractionality_ = std::max(
          lp_solution_fractionality_, std::abs(value - std::round(value)));
    }

    // Compute the objective value.
    lp_objective_ = 0;
    for (const auto& term : integer_objective_) {
      lp_objective_ += lp_solution_[term.first.value()] * term.second.value();
    }
    lp_solution_is_integer_ = lp_solution_fractionality_ < kCpEpsilon;
  }
  return true;
}

void FeasibilityPump::UpdateBoundsOfLpVariables() {
  const int num_vars = integer_variables_.size();
  for (int i = 0; i < num_vars; i++) {
    const IntegerVariable cp_var = integer_variables_[i];
    const double lb = ToDouble(integer_trail_->LevelZeroLowerBound(cp_var));
    const double ub = ToDouble(integer_trail_->LevelZeroUpperBound(cp_var));
    const double factor = scaler_.VariableScalingFactor(ColIndex(i));
    lp_data_.SetVariableBounds(ColIndex(i), lb * factor, ub * factor);
  }
}

double FeasibilityPump::GetLPSolutionValue(IntegerVariable variable) const {
  return lp_solution_[gtl::FindOrDie(mirror_lp_variable_, variable).value()];
}

double FeasibilityPump::GetVariableValueAtCpScale(ColIndex var) {
  return scaler_.UnscaleVariableValue(var, simplex_.GetVariableValue(var));
}

// ----------------------------------------------------------------
// -------------------Rounding-------------------------------------
// ----------------------------------------------------------------

int64_t FeasibilityPump::GetIntegerSolutionValue(
    IntegerVariable variable) const {
  return integer_solution_[gtl::FindOrDie(mirror_lp_variable_, variable)
                               .value()];
}

bool FeasibilityPump::Round() {
  bool rounding_successful = true;
  if (sat_parameters_.fp_rounding() == SatParameters::NEAREST_INTEGER) {
    rounding_successful = NearestIntegerRounding();
  } else if (sat_parameters_.fp_rounding() == SatParameters::LOCK_BASED) {
    rounding_successful = LockBasedRounding();
  } else if (sat_parameters_.fp_rounding() ==
             SatParameters::ACTIVE_LOCK_BASED) {
    rounding_successful = ActiveLockBasedRounding();
  } else if (sat_parameters_.fp_rounding() ==
             SatParameters::PROPAGATION_ASSISTED) {
    rounding_successful = PropagationRounding();
  }
  if (!rounding_successful) return false;
  FillIntegerSolutionStats();
  return true;
}

bool FeasibilityPump::NearestIntegerRounding() {
  if (!lp_solution_is_set_) return false;
  for (int i = 0; i < lp_solution_.size(); ++i) {
    integer_solution_[i] = static_cast<int64_t>(std::round(lp_solution_[i]));
  }
  integer_solution_is_set_ = true;
  return true;
}

bool FeasibilityPump::LockBasedRounding() {
  if (!lp_solution_is_set_) return false;
  const int num_vars = integer_variables_.size();

  // We compute the number of locks based on variable coefficient in constraints
  // and constraint bounds. This doesn't change over time so we cache it.
  if (var_up_locks_.empty()) {
    var_up_locks_.resize(num_vars, 0);
    var_down_locks_.resize(num_vars, 0);
    for (int i = 0; i < num_vars; ++i) {
      for (const auto entry : lp_data_.GetSparseColumn(ColIndex(i))) {
        ColIndex slack = lp_data_.GetSlackVariable(entry.row());
        const bool constraint_upper_bounded =
            lp_data_.variable_lower_bounds()[slack] > -glop::kInfinity;

        const bool constraint_lower_bounded =
            lp_data_.variable_upper_bounds()[slack] < glop::kInfinity;

        if (entry.coefficient() > 0) {
          var_up_locks_[i] += constraint_upper_bounded;
          var_down_locks_[i] += constraint_lower_bounded;
        } else {
          var_up_locks_[i] += constraint_lower_bounded;
          var_down_locks_[i] += constraint_upper_bounded;
        }
      }
    }
  }

  for (int i = 0; i < lp_solution_.size(); ++i) {
    if (std::abs(lp_solution_[i] - std::round(lp_solution_[i])) < 0.1 ||
        var_up_locks_[i] == var_down_locks_[i]) {
      integer_solution_[i] = static_cast<int64_t>(std::round(lp_solution_[i]));
    } else if (var_up_locks_[i] > var_down_locks_[i]) {
      integer_solution_[i] = static_cast<int64_t>(std::floor(lp_solution_[i]));
    } else {
      integer_solution_[i] = static_cast<int64_t>(std::ceil(lp_solution_[i]));
    }
  }
  integer_solution_is_set_ = true;
  return true;
}

bool FeasibilityPump::ActiveLockBasedRounding() {
  if (!lp_solution_is_set_) return false;
  const int num_vars = integer_variables_.size();

  // We compute the number of locks based on variable coefficient in constraints
  // and constraint bounds of active constraints. We consider the bound of the
  // constraint that is tight for the current lp solution.
  for (int i = 0; i < num_vars; ++i) {
    if (std::abs(lp_solution_[i] - std::round(lp_solution_[i])) < 0.1) {
      integer_solution_[i] = static_cast<int64_t>(std::round(lp_solution_[i]));
    }

    int up_locks = 0;
    int down_locks = 0;
    for (const auto entry : lp_data_.GetSparseColumn(ColIndex(i))) {
      const ConstraintStatus row_status =
          simplex_.GetConstraintStatus(entry.row());
      if (row_status == ConstraintStatus::AT_LOWER_BOUND) {
        if (entry.coefficient() > 0) {
          down_locks++;
        } else {
          up_locks++;
        }
      } else if (row_status == ConstraintStatus::AT_UPPER_BOUND) {
        if (entry.coefficient() > 0) {
          up_locks++;
        } else {
          down_locks++;
        }
      }
    }
    if (up_locks == down_locks) {
      integer_solution_[i] = static_cast<int64_t>(std::round(lp_solution_[i]));
    } else if (up_locks > down_locks) {
      integer_solution_[i] = static_cast<int64_t>(std::floor(lp_solution_[i]));
    } else {
      integer_solution_[i] = static_cast<int64_t>(std::ceil(lp_solution_[i]));
    }
  }

  integer_solution_is_set_ = true;
  return true;
}

bool FeasibilityPump::PropagationRounding() {
  if (!lp_solution_is_set_) return false;
  sat_solver_->ResetToLevelZero();

  // Compute an order in which we will fix variables and do the propagation.
  std::vector<int> rounding_order;
  {
    std::vector<std::pair<double, int>> binary_fractionality_vars;
    std::vector<std::pair<double, int>> general_fractionality_vars;
    for (int i = 0; i < lp_solution_.size(); ++i) {
      const double fractionality =
          std::abs(std::round(lp_solution_[i]) - lp_solution_[i]);
      if (var_is_binary_[i]) {
        binary_fractionality_vars.push_back({fractionality, i});
      } else {
        general_fractionality_vars.push_back({fractionality, i});
      }
    }
    std::sort(binary_fractionality_vars.begin(),
              binary_fractionality_vars.end());
    std::sort(general_fractionality_vars.begin(),
              general_fractionality_vars.end());

    for (int i = 0; i < binary_fractionality_vars.size(); ++i) {
      rounding_order.push_back(binary_fractionality_vars[i].second);
    }
    for (int i = 0; i < general_fractionality_vars.size(); ++i) {
      rounding_order.push_back(general_fractionality_vars[i].second);
    }
  }

  for (const int var_index : rounding_order) {
    if (time_limit_->LimitReached()) return false;
    // Get the bounds of the variable.
    const IntegerVariable var = integer_variables_[var_index];
    const Domain& domain = (*domains_)[var];

    const IntegerValue lb = integer_trail_->LowerBound(var);
    const IntegerValue ub = integer_trail_->UpperBound(var);
    if (lb == ub) {
      integer_solution_[var_index] = lb.value();
      continue;
    }

    const int64_t rounded_value =
        static_cast<int64_t>(std::round(lp_solution_[var_index]));
    const int64_t floor_value =
        static_cast<int64_t>(std::floor(lp_solution_[var_index]));
    const int64_t ceil_value =
        static_cast<int64_t>(std::ceil(lp_solution_[var_index]));

    const bool floor_is_in_domain =
        (domain.Contains(floor_value) && lb.value() <= floor_value);
    const bool ceil_is_in_domain =
        (domain.Contains(ceil_value) && ub.value() >= ceil_value);
    if (domain.IsEmpty()) {
      integer_solution_[var_index] = rounded_value;
      model_is_unsat_ = true;
      return false;
    }

    if (ceil_value < lb.value()) {
      integer_solution_[var_index] = lb.value();
    } else if (floor_value > ub.value()) {
      integer_solution_[var_index] = ub.value();
    } else if (ceil_is_in_domain && floor_is_in_domain) {
      DCHECK(domain.Contains(rounded_value));
      integer_solution_[var_index] = rounded_value;
    } else if (ceil_is_in_domain) {
      integer_solution_[var_index] = ceil_value;
    } else if (floor_is_in_domain) {
      integer_solution_[var_index] = floor_value;
    } else {
      const std::pair<IntegerLiteral, IntegerLiteral> values_in_domain =
          integer_encoder_->Canonicalize(
              IntegerLiteral::GreaterOrEqual(var, IntegerValue(rounded_value)));
      const int64_t lower_value = values_in_domain.first.bound.value();
      const int64_t higher_value = -values_in_domain.second.bound.value();
      const int64_t distance_from_lower_value =
          std::abs(lower_value - rounded_value);
      const int64_t distance_from_higher_value =
          std::abs(higher_value - rounded_value);

      integer_solution_[var_index] =
          (distance_from_lower_value < distance_from_higher_value)
              ? lower_value
              : higher_value;
    }

    CHECK(domain.Contains(integer_solution_[var_index]));
    CHECK_GE(integer_solution_[var_index], lb);
    CHECK_LE(integer_solution_[var_index], ub);

    // Propagate the value.
    //
    // When we want to fix the variable at its lb or ub, we do not create an
    // equality literal to minimize the number of new literal we create. This
    // is because creating an "== value" literal will implicitly also create
    // a ">= value" and a "<= value" literals.
    Literal to_enqueue;
    const IntegerValue value(integer_solution_[var_index]);
    if (value == lb) {
      to_enqueue = integer_encoder_->GetOrCreateAssociatedLiteral(
          IntegerLiteral::LowerOrEqual(var, value));
    } else if (value == ub) {
      to_enqueue = integer_encoder_->GetOrCreateAssociatedLiteral(
          IntegerLiteral::GreaterOrEqual(var, value));
    } else {
      to_enqueue =
          integer_encoder_->GetOrCreateLiteralAssociatedToEquality(var, value);
    }

    if (!sat_solver_->FinishPropagation()) {
      model_is_unsat_ = true;
      return false;
    }
    sat_solver_->EnqueueDecisionAndBacktrackOnConflict(to_enqueue);

    if (sat_solver_->IsModelUnsat()) {
      model_is_unsat_ = true;
      return false;
    }
  }
  sat_solver_->ResetToLevelZero();
  integer_solution_is_set_ = true;
  return true;
}

void FeasibilityPump::FillIntegerSolutionStats() {
  // Compute the objective value.
  integer_solution_objective_ = 0;
  for (const auto& term : integer_objective_) {
    integer_solution_objective_ +=
        integer_solution_[term.first.value()] * term.second.value();
  }

  integer_solution_is_feasible_ = true;
  num_infeasible_constraints_ = 0;
  integer_solution_infeasibility_ = 0;
  for (RowIndex i(0); i < integer_lp_.size(); ++i) {
    int64_t activity = 0;
    for (const auto& term : integer_lp_[i].terms) {
      const int64_t prod =
          CapProd(integer_solution_[term.first.value()], term.second.value());
      if (prod <= std::numeric_limits<int64_t>::min() ||
          prod >= std::numeric_limits<int64_t>::max()) {
        activity = prod;
        break;
      }
      activity = CapAdd(activity, prod);
      if (activity <= std::numeric_limits<int64_t>::min() ||
          activity >= std::numeric_limits<int64_t>::max())
        break;
    }
    if (activity > integer_lp_[i].ub || activity < integer_lp_[i].lb) {
      integer_solution_is_feasible_ = false;
      num_infeasible_constraints_++;
      const int64_t ub_infeasibility =
          activity > integer_lp_[i].ub.value()
              ? activity - integer_lp_[i].ub.value()
              : 0;
      const int64_t lb_infeasibility =
          activity < integer_lp_[i].lb.value()
              ? integer_lp_[i].lb.value() - activity
              : 0;
      integer_solution_infeasibility_ =
          std::max(integer_solution_infeasibility_,
                   std::max(ub_infeasibility, lb_infeasibility));
    }
  }
}

}  // namespace sat
}  // namespace operations_research

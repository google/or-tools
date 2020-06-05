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

#include "ortools/sat/feasibility_pump.h"

#include <limits>

#include "ortools/base/integral_types.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {
namespace sat {

using glop::ColIndex;
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
      mapping_(model->Get<CpModelMapping>()) {
  // Tweak the default parameters to make the solve incremental.
  glop::GlopParameters parameters;
  // TODO(user): Determine which simplex is better for this, dual or primal.
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
  const glop::ColIndex col = it->second;
  integer_objective_.push_back({col, coeff});
  objective_infinity_norm_ =
      std::max(objective_infinity_norm_, IntTypeAbs(coeff));
}

glop::ColIndex FeasibilityPump::GetOrCreateMirrorVariable(
    IntegerVariable positive_variable) {
  DCHECK(VariableIsPositive(positive_variable));

  const auto it = mirror_lp_variable_.find(positive_variable);
  if (it == mirror_lp_variable_.end()) {
    const int model_var =
        mapping_->GetProtoVariableFromIntegerVariable(positive_variable);
    model_vars_size_ = std::max(model_vars_size_, model_var + 1);

    const glop::ColIndex col(integer_variables_.size());
    mirror_lp_variable_[positive_variable] = col;
    integer_variables_.push_back(positive_variable);
    lp_solution_.push_back(std::numeric_limits<double>::infinity());
    integer_solution_.push_back(kMaxIntegerValue.value());

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

void FeasibilityPump::Solve() {
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

  // TODO(user): Add cycle detection.
  mixing_factor_ = 1.0;
  for (int i = 0; i < max_fp_iterations_; ++i) {
    L1DistanceMinimize();
    if (!SolveLp()) break;
    if (lp_solution_is_integer_) break;
    SimpleRounding();
    // We don't end this loop if the integer solutions is feasible in hope to
    // get better solution.
    if (integer_solution_is_feasible_) MaybePushToRepo();
  }

  PrintStats();
  MaybePushToRepo();
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
    CHECK_EQ(glop::ColIndex(i), lp_data_.CreateNewVariable());
    lp_data_.SetVariableType(glop::ColIndex(i),
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
    lp_data_.SetVariableBounds(glop::ColIndex(i), lb, ub);
  }

  objective_normalization_factor_ = 0.0;
  glop::ColIndexVector integer_variables;
  const ColIndex num_cols = lp_data_.num_variables();
  for (ColIndex col : lp_data_.IntegerVariablesList()) {
    if (!lp_data_.IsVariableBinary(col)) {
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

  // TODO(user): Try to add scaling.
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
    if (lp_data_.IsVariableBinary(col)) {
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
      const ColIndex norm_a_slack_variable =
          lp_data_.GetSlackVariable(norm_lhs_constraints_[col]);
      lp_data_.SetVariableBounds(norm_a_slack_variable, -glop::kInfinity,
                                 integer_solution_[col.value()]);
      const ColIndex norm_b_slack_variable =
          lp_data_.GetSlackVariable(norm_rhs_constraints_[col]);
      lp_data_.SetVariableBounds(norm_b_slack_variable, -glop::kInfinity,
                                 -integer_solution_[col.value()]);
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

  VLOG(3) << "simplex status: " << simplex_.GetProblemStatus();
  lp_solution_fractionality_ = 0.0;
  if (simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL ||
      simplex_.GetProblemStatus() == glop::ProblemStatus::DUAL_FEASIBLE ||
      simplex_.GetProblemStatus() == glop::ProblemStatus::PRIMAL_FEASIBLE ||
      simplex_.GetProblemStatus() == glop::ProblemStatus::IMPRECISE) {
    lp_solution_is_set_ = true;
    for (int i = 0; i < num_vars; i++) {
      const double value = simplex_.GetVariableValue(glop::ColIndex(i));
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
    const double lb = ToDouble(integer_trail_->LowerBound(cp_var));
    const double ub = ToDouble(integer_trail_->UpperBound(cp_var));
    lp_data_.SetVariableBounds(glop::ColIndex(i), lb, ub);
  }
}

double FeasibilityPump::GetLPSolutionValue(IntegerVariable variable) const {
  return lp_solution_[gtl::FindOrDie(mirror_lp_variable_, variable).value()];
}

// ----------------------------------------------------------------
// -------------------Rounding-------------------------------------
// ----------------------------------------------------------------

int64 FeasibilityPump::GetIntegerSolutionValue(IntegerVariable variable) const {
  return integer_solution_[gtl::FindOrDie(mirror_lp_variable_, variable)
                               .value()];
}

void FeasibilityPump::SimpleRounding() {
  if (!lp_solution_is_set_) return;
  integer_solution_is_set_ = true;
  for (int i = 0; i < lp_solution_.size(); ++i) {
    integer_solution_[i] = static_cast<int64>(std::round(lp_solution_[i]));
  }
  // Compute the objective value.
  integer_solution_objective_ = 0;
  for (const auto& term : integer_objective_) {
    integer_solution_objective_ +=
        integer_solution_[term.first.value()] * term.second.value();
  }

  integer_solution_is_feasible_ = true;
  num_infeasible_constraints_ = 0;
  integer_solution_infeasibility_ = 0;
  for (glop::RowIndex i(0); i < integer_lp_.size(); ++i) {
    int64 activity = 0;
    for (const auto& term : integer_lp_[i].terms) {
      const int64 prod =
          CapProd(integer_solution_[term.first.value()], term.second.value());
      if (prod <= kint64min || prod >= kint64max) {
        activity = prod;
        break;
      }
      activity = CapAdd(activity, prod);
      if (activity <= kint64min || activity >= kint64max) break;
    }
    if (activity > integer_lp_[i].ub || activity < integer_lp_[i].lb) {
      integer_solution_is_feasible_ = false;
      num_infeasible_constraints_++;
      integer_solution_infeasibility_ =
          std::max(integer_solution_infeasibility_,
                   std::max(activity - integer_lp_[i].ub.value(),
                            integer_lp_[i].lb.value() - activity));
    }
  }
}

}  // namespace sat
}  // namespace operations_research

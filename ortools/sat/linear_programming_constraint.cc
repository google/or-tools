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

#include "ortools/sat/linear_programming_constraint.h"

#include <cmath>
#include <limits>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/preprocessor.h"
#include "ortools/glop/status.h"
#include "ortools/graph/strongly_connected_components.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {
namespace sat {

using glop::ColIndex;
using glop::Fractional;
using glop::RowIndex;

const double LinearProgrammingConstraint::kCpEpsilon = 1e-4;
const double LinearProgrammingConstraint::kLpEpsilon = 1e-6;

namespace {

double ToDouble(IntegerValue value) {
  const double kInfinity = std::numeric_limits<double>::infinity();
  if (value >= kMaxIntegerValue) return kInfinity;
  if (value <= kMinIntegerValue) return -kInfinity;
  return static_cast<double>(value.value());
}

// TODO(user): Also used in sorted_interval_lists.h remove duplication.
int64 CeilRatio(int64 value, int64 positive_coeff) {
  CHECK_GT(positive_coeff, 0);
  const int64 result = value / positive_coeff;
  const int64 adjust = static_cast<int64>(result * positive_coeff < value);
  return result + adjust;
}
int64 FloorRatio(int64 value, int64 positive_coeff) {
  CHECK_GT(positive_coeff, 0);
  const int64 result = value / positive_coeff;
  const int64 adjust = static_cast<int64>(result * positive_coeff > value);
  return result - adjust;
}

}  // namespace

// TODO(user): make SatParameters singleton too, otherwise changing them after
// a constraint was added will have no effect on this class.
LinearProgrammingConstraint::LinearProgrammingConstraint(Model* model)
    : sat_parameters_(*(model->GetOrCreate<SatParameters>())),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      trail_(model->GetOrCreate<Trail>()),
      model_heuristics_(model->GetOrCreate<SearchHeuristicsVector>()),
      integer_encoder_(model->GetOrCreate<IntegerEncoder>()),
      dispatcher_(model->GetOrCreate<LinearProgrammingDispatcher>()) {
  // Tweak the default parameters to make the solve incremental.
  glop::GlopParameters parameters;
  parameters.set_use_dual_simplex(true);
  simplex_.SetParameters(parameters);
}

LinearProgrammingConstraint::ConstraintIndex
LinearProgrammingConstraint::CreateNewConstraint(IntegerValue lb,
                                                 IntegerValue ub) {
  DCHECK(!lp_constraint_is_registered_);
  const int index = integer_lp_.size();
  integer_lp_.push_back(LinearConstraintInternal());
  integer_lp_.back().lb = lb;
  integer_lp_.back().ub = ub;
  return ConstraintIndex(index);
}

glop::ColIndex LinearProgrammingConstraint::GetOrCreateMirrorVariable(
    IntegerVariable positive_variable) {
  DCHECK(VariableIsPositive(positive_variable));
  if (!gtl::ContainsKey(mirror_lp_variable_, positive_variable)) {
    const glop::ColIndex col = lp_data_.CreateNewVariable();
    DCHECK_EQ(col, integer_variables_.size());
    mirror_lp_variable_[positive_variable] = col;
    integer_variables_.push_back(positive_variable);
    lp_solution_.push_back(std::numeric_limits<double>::infinity());
    lp_reduced_cost_.push_back(0.0);
    (*dispatcher_)[positive_variable] = this;
    return col;
  }
  return mirror_lp_variable_[positive_variable];
}

void LinearProgrammingConstraint::SetCoefficient(ConstraintIndex ct,
                                                 IntegerVariable ivar,
                                                 IntegerValue coefficient) {
  CHECK(!lp_constraint_is_registered_);
  IntegerVariable pos_var = VariableIsPositive(ivar) ? ivar : NegationOf(ivar);
  if (ivar != pos_var) coefficient = -coefficient;
  const glop::ColIndex col = GetOrCreateMirrorVariable(pos_var);
  integer_lp_[ct.value()].terms.push_back({col, coefficient});
}

void LinearProgrammingConstraint::SetObjectiveCoefficient(IntegerVariable ivar,
                                                          IntegerValue coeff) {
  CHECK(!lp_constraint_is_registered_);
  objective_is_defined_ = true;
  IntegerVariable pos_var = VariableIsPositive(ivar) ? ivar : NegationOf(ivar);
  if (ivar != pos_var) coeff = -coeff;

  const glop::ColIndex col = GetOrCreateMirrorVariable(pos_var);
  lp_data_.SetObjectiveCoefficient(col, ToDouble(coeff));
  integer_objective_.push_back({col, coeff});
}

void LinearProgrammingConstraint::RegisterWith(Model* model) {
  DCHECK(!lp_constraint_is_registered_);
  lp_constraint_is_registered_ = true;
  model->GetOrCreate<LinearProgrammingConstraintCollection>()->push_back(this);

  std::sort(integer_objective_.begin(), integer_objective_.end());

  // Because sometimes we split a == constraint in two (>= and <=), it makes
  // sense to detect duplicate constraints and merge bounds.
  {
    int new_size = 0;
    absl::flat_hash_map<LinearConstraintInternal, int, TermsHash, TermsEquiv>
        equiv_constraint;
    for (LinearConstraintInternal& constraint : integer_lp_) {
      std::sort(constraint.terms.begin(), constraint.terms.end());
      if (gtl::ContainsKey(equiv_constraint, constraint)) {
        const int index = equiv_constraint[constraint];
        integer_lp_[index].lb = std::max(integer_lp_[index].lb, constraint.lb);
        integer_lp_[index].ub = std::min(integer_lp_[index].ub, constraint.ub);
        continue;
      }
      equiv_constraint[constraint] = new_size;
      integer_lp_[new_size++] = constraint;
    }
    if (new_size < integer_lp_.size()) {
      VLOG(1) << "Merged " << integer_lp_.size() - new_size << " constraints.";
    }
    integer_lp_.resize(new_size);
  }

  // Copy the integer_lp_ into lp_data_. Note that the objective is already
  // copied.
  for (const LinearConstraintInternal& ct : integer_lp_) {
    const ConstraintIndex row = lp_data_.CreateNewConstraint();
    lp_data_.SetConstraintBounds(row, ToDouble(ct.lb), ToDouble(ct.ub));
    for (const auto& term : ct.terms) {
      lp_data_.SetCoefficient(row, term.first, ToDouble(term.second));
    }
  }

  // Scale lp_data_.
  Scale(&lp_data_, &scaler_, glop::GlopParameters::DEFAULT);
  lp_data_.ScaleObjective();

  // ScaleBounds() looks at both the constraints and variable bounds, so we
  // initialize the LP variable bounds before scaling them.
  //
  // TODO(user): As part of the scaling, we may also want to shift the initial
  // variable bounds so that each variable contain the value zero in their
  // domain. Maybe just once and for all at the beginning.
  bound_scaling_factor_ = 1.0;
  UpdateBoundsOfLpVariables();
  bound_scaling_factor_ = lp_data_.ScaleBounds();

  lp_data_.AddSlackVariablesWhereNecessary(false);

  GenericLiteralWatcher* watcher = model->GetOrCreate<GenericLiteralWatcher>();
  const int watcher_id = watcher->Register(this);
  const int num_vars = integer_variables_.size();
  for (int i = 0; i < num_vars; i++) {
    watcher->WatchIntegerVariable(integer_variables_[i], watcher_id, i);
  }
  if (objective_is_defined_) {
    watcher->WatchUpperBound(objective_cp_, watcher_id);
  }
  watcher->SetPropagatorPriority(watcher_id, 2);

  if (integer_variables_.size() >= 20) {  // Do not use on small subparts.
    auto* container = model->GetOrCreate<SearchHeuristicsVector>();
    container->push_back(HeuristicLPPseudoCostBinary(model));
    container->push_back(HeuristicLPMostInfeasibleBinary(model));
  }

  // Registering it with the trail make sure this class is always in sync when
  // it is used in the decision heuristics.
  integer_trail_->RegisterReversibleClass(this);
}

void LinearProgrammingConstraint::SetLevel(int level) {
  if (lp_solution_is_set_ && level < lp_solution_level_) {
    lp_solution_is_set_ = false;
  }
}

void LinearProgrammingConstraint::AddCutGenerator(CutGenerator generator) {
  for (const IntegerVariable var : generator.vars) {
    GetOrCreateMirrorVariable(VariableIsPositive(var) ? var : NegationOf(var));
  }
  cut_generators_.push_back(std::move(generator));
}

// Check whether the change breaks the current LP solution.
// Call Propagate() only if it does.
bool LinearProgrammingConstraint::IncrementalPropagate(
    const std::vector<int>& watch_indices) {
  if (!lp_solution_is_set_) return Propagate();
  for (const int index : watch_indices) {
    const double lb =
        ToDouble(integer_trail_->LowerBound(integer_variables_[index]));
    const double ub =
        ToDouble(integer_trail_->UpperBound(integer_variables_[index]));
    const double value = lp_solution_[index];
    if (value < lb - kCpEpsilon || value > ub + kCpEpsilon) return Propagate();
  }
  return true;
}

glop::Fractional LinearProgrammingConstraint::CpToLpScalingFactor(
    glop::ColIndex col) const {
  return scaler_.col_scale(col) / bound_scaling_factor_;
}
glop::Fractional LinearProgrammingConstraint::LpToCpScalingFactor(
    glop::ColIndex col) const {
  return bound_scaling_factor_ / scaler_.col_scale(col);
}

glop::Fractional LinearProgrammingConstraint::GetVariableValueAtCpScale(
    glop::ColIndex var) {
  return simplex_.GetVariableValue(var) * LpToCpScalingFactor(var);
}

double LinearProgrammingConstraint::GetSolutionValue(
    IntegerVariable variable) const {
  return lp_solution_[gtl::FindOrDie(mirror_lp_variable_, variable).value()];
}

double LinearProgrammingConstraint::GetSolutionReducedCost(
    IntegerVariable variable) const {
  return lp_reduced_cost_[gtl::FindOrDie(mirror_lp_variable_, variable)
                              .value()];
}

void LinearProgrammingConstraint::UpdateBoundsOfLpVariables() {
  const int num_vars = integer_variables_.size();
  for (int i = 0; i < num_vars; i++) {
    const IntegerVariable cp_var = integer_variables_[i];
    const double lb = ToDouble(integer_trail_->LowerBound(cp_var));
    const double ub = ToDouble(integer_trail_->UpperBound(cp_var));
    const double factor = CpToLpScalingFactor(glop::ColIndex(i));
    lp_data_.SetVariableBounds(glop::ColIndex(i), lb * factor, ub * factor);
  }
}

bool LinearProgrammingConstraint::Propagate() {
  UpdateBoundsOfLpVariables();

  // TODO(user): It seems the time we loose by not stopping early might be worth
  // it because we end up with a better explanation at optimality.
  glop::GlopParameters parameters = simplex_.GetParameters();
  if (/* DISABLES CODE */ (false) && objective_is_defined_) {
    // We put a limit on the dual objective since there is no point increasing
    // it past our current objective upper-bound (we will already fail as soon
    // as we pass it). Note that this limit is properly transformed using the
    // objective scaling factor and offset stored in lp_data_.
    //
    // Note that we use a bigger epsilon here to be sure that if we abort
    // because of this, we will report a conflict.
    parameters.set_objective_upper_limit(
        static_cast<double>(integer_trail_->UpperBound(objective_cp_).value() +
                            100.0 * kCpEpsilon));
  }

  // Put an iteration limit on the work we do in the simplex for this call. Note
  // that because we are "incremental", even if we don't solve it this time we
  // will make progress towards a solve in the lower node of the tree search.
  //
  // TODO(user): Put more at the root, and less afterwards?
  parameters.set_max_number_of_iterations(500);
  if (sat_parameters_.use_exact_lp_reason()) {
    parameters.set_change_status_to_imprecise(false);
    parameters.set_primal_feasibility_tolerance(1e-7);
    parameters.set_dual_feasibility_tolerance(1e-7);
  }

  simplex_.SetParameters(parameters);
  simplex_.NotifyThatMatrixIsUnchangedForNextSolve();
  const auto status = simplex_.Solve(lp_data_, time_limit_);
  if (!status.ok()) {
    LOG(WARNING) << "The LP solver encountered an error: "
                 << status.error_message();
    simplex_.ClearStateForNextSolve();
    return true;
  }

  // Add cuts and resolve.
  // TODO(user): for the cuts, we scale back and forth, is this really needed?
  if (!cut_generators_.empty() && num_cuts_ < sat_parameters_.max_num_cuts() &&
      (trail_->CurrentDecisionLevel() == 0 ||
       !sat_parameters_.only_add_cuts_at_level_zero()) &&
      (simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL ||
       simplex_.GetProblemStatus() == glop::ProblemStatus::DUAL_FEASIBLE)) {
    int num_new_cuts = 0;
    for (const CutGenerator& generator : cut_generators_) {
      std::vector<double> local_solution;
      for (const IntegerVariable var : generator.vars) {
        if (VariableIsPositive(var)) {
          const auto index = gtl::FindOrDie(mirror_lp_variable_, var);
          local_solution.push_back(GetVariableValueAtCpScale(index));
        } else {
          const auto index =
              gtl::FindOrDie(mirror_lp_variable_, NegationOf(var));
          local_solution.push_back(-GetVariableValueAtCpScale(index));
        }
      }
      std::vector<LinearConstraint> cuts =
          generator.generate_cuts(local_solution);
      if (cuts.empty()) continue;

      // Add the cuts to the LP!
      if (num_new_cuts == 0) lp_data_.DeleteSlackVariables();
      for (const LinearConstraint& cut : cuts) {
        ++num_new_cuts;
        const glop::RowIndex row = lp_data_.CreateNewConstraint();
        lp_data_.SetConstraintBounds(row, ToDouble(cut.lb), ToDouble(cut.ub));
        integer_lp_.push_back(LinearConstraintInternal());
        integer_lp_.back().lb = cut.lb;
        integer_lp_.back().ub = cut.ub;
        for (int i = 0; i < cut.vars.size(); ++i) {
          const glop::ColIndex col = GetOrCreateMirrorVariable(cut.vars[i]);

          // The returned coefficients correspond to variables at the CP scale,
          // so we need to divide them by CpToLpScalingFactor() which is the
          // same as multiplying by LpToCpScalingFactor().
          //
          // TODO(user): we should still multiply this row by a row_scale so
          // that its maximum magnitude is one.
          lp_data_.SetCoefficient(
              row, col, ToDouble(cut.coeffs[i]) * LpToCpScalingFactor(col));
          integer_lp_.back().terms.push_back({col, cut.coeffs[i]});
        }
        std::sort(integer_lp_.back().terms.begin(),
                  integer_lp_.back().terms.end());
      }
    }

    // Resolve if we added some cuts.
    if (num_new_cuts > 0) {
      num_cuts_ += num_new_cuts;
      VLOG(1) << "#cuts " << num_cuts_;
      lp_data_.NotifyThatColumnsAreClean();
      lp_data_.AddSlackVariablesWhereNecessary(false);
      const auto status = simplex_.Solve(lp_data_, time_limit_);
      if (!status.ok()) {
        LOG(WARNING) << "The LP solver encountered an error: "
                     << status.error_message();
        simplex_.ClearStateForNextSolve();
        return true;
      }
    }
  }

  // A dual-unbounded problem is infeasible. We use the dual ray reason.
  if (simplex_.GetProblemStatus() == glop::ProblemStatus::DUAL_UNBOUNDED) {
    if (sat_parameters_.use_exact_lp_reason()) {
      if (!FillExactDualRayReason()) return true;
    } else {
      FillDualRayReason();
    }
    return integer_trail_->ReportConflict(integer_reason_);
  }

  // Optimality deductions if problem has an objective.
  if (objective_is_defined_ &&
      (simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL ||
       simplex_.GetProblemStatus() == glop::ProblemStatus::DUAL_FEASIBLE)) {
    // Try to filter optimal objective value. Note that GetObjectiveValue()
    // already take care of the scaling so that it returns an objective in the
    // CP world.
    const double relaxed_optimal_objective = simplex_.GetObjectiveValue();
    const IntegerValue approximate_new_lb(
        static_cast<int64>(std::ceil(relaxed_optimal_objective - kCpEpsilon)));

    // TODO(user): Maybe do a bit less computation when we cannot propagate
    // anything.
    const IntegerValue old_lb = integer_trail_->LowerBound(objective_cp_);
    IntegerValue new_lb;
    if (sat_parameters_.use_exact_lp_reason()) {
      new_lb = ExactLpReasonning();

      // A difference of 1 happens relatively often, so we just display when
      // there is more. Note that when we are over the objective upper bound,
      // we relax new_lb for a better reason, so we ignore this case.
      if (new_lb <= integer_trail_->UpperBound(objective_cp_) &&
          std::abs((approximate_new_lb - new_lb).value()) > 1) {
        VLOG(1) << "LP exact objective diff " << approximate_new_lb - new_lb;
      }
    } else {
      FillReducedCostsReason();
      new_lb = approximate_new_lb;
      const double objective_cp_ub =
          ToDouble(integer_trail_->UpperBound(objective_cp_));
      ReducedCostStrengtheningDeductions(objective_cp_ub -
                                         relaxed_optimal_objective);
      if (!deductions_.empty()) {
        deductions_reason_ = integer_reason_;
        deductions_reason_.push_back(
            integer_trail_->UpperBoundAsLiteral(objective_cp_));
      }
    }

    // Push new objective lb.
    if (old_lb < new_lb) {
      const IntegerLiteral deduction =
          IntegerLiteral::GreaterOrEqual(objective_cp_, new_lb);
      if (!integer_trail_->Enqueue(deduction, {}, integer_reason_)) {
        return false;
      }
    }

    // Push reduced cost strengthening bounds.
    if (!deductions_.empty()) {
      const int trail_index_with_same_reason = integer_trail_->Index();
      for (const IntegerLiteral deduction : deductions_) {
        if (!integer_trail_->Enqueue(deduction, {}, deductions_reason_,
                                     trail_index_with_same_reason)) {
          return false;
        }
      }
    }
  }

  // Copy current LP solution.
  if (simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL) {
    const double objective_scale = lp_data_.objective_scaling_factor();
    lp_solution_is_set_ = true;
    lp_solution_level_ = trail_->CurrentDecisionLevel();
    lp_objective_ = simplex_.GetObjectiveValue();
    lp_solution_is_integer_ = true;
    const int num_vars = integer_variables_.size();
    for (int i = 0; i < num_vars; i++) {
      lp_solution_[i] = GetVariableValueAtCpScale(glop::ColIndex(i));

      // The reduced cost need to be divided by LpToCpScalingFactor().
      lp_reduced_cost_[i] = simplex_.GetReducedCost(glop::ColIndex(i)) *
                            CpToLpScalingFactor(glop::ColIndex(i)) *
                            objective_scale;
      if (std::abs(lp_solution_[i] - std::round(lp_solution_[i])) >
          kCpEpsilon) {
        lp_solution_is_integer_ = false;
      }
    }

    if (compute_reduced_cost_averages_) {
      // Decay averages.
      num_calls_since_reduced_cost_averages_reset_++;
      if (num_calls_since_reduced_cost_averages_reset_ == 10000) {
        for (int i = 0; i < num_vars; i++) {
          sum_cost_up_[i] /= 2;
          num_cost_up_[i] /= 2;
          sum_cost_down_[i] /= 2;
          num_cost_down_[i] /= 2;
        }
        num_calls_since_reduced_cost_averages_reset_ = 0;
      }

      // Accumulate pseudo-costs of all unassigned variables.
      for (int i = 0; i < num_vars; i++) {
        const IntegerVariable var = this->integer_variables_[i];

        // Skip ignored and fixed variables.
        if (integer_trail_->IsCurrentlyIgnored(var)) continue;
        const IntegerValue lb = integer_trail_->LowerBound(var);
        const IntegerValue ub = integer_trail_->UpperBound(var);
        if (lb == ub) continue;

        // Skip reduced costs that are zero or close.
        const double rc = this->GetSolutionReducedCost(var);
        if (std::abs(rc) < kCpEpsilon) continue;

        if (rc < 0.0) {
          sum_cost_down_[i] -= rc;
          num_cost_down_[i]++;
        } else {
          sum_cost_up_[i] += rc;
          num_cost_up_[i]++;
        }
      }
    }
  }
  return true;
}

namespace {

std::vector<std::pair<ColIndex, IntegerValue>> GetSparseRepresentation(
    const gtl::ITIVector<ColIndex, IntegerValue>& dense_vector) {
  std::vector<std::pair<ColIndex, IntegerValue>> result;
  for (ColIndex col(0); col < dense_vector.size(); ++col) {
    if (dense_vector[col] != 0) {
      result.push_back({col, dense_vector[col]});
    }
  }
  return result;
}

// Returns false in case of overflow
bool AddLinearExpressionMultiple(
    IntegerValue multiplier,
    const std::vector<std::pair<ColIndex, IntegerValue>>& terms,
    gtl::ITIVector<ColIndex, IntegerValue>* dense_vector) {
  for (const std::pair<ColIndex, IntegerValue> term : terms) {
    const int64 prod = CapProd(multiplier.value(), term.second.value());
    if (prod == kint64min || prod == kint64max) return false;
    const int64 result = CapAdd((*dense_vector)[term.first].value(), prod);
    if (result == kint64min || result == kint64max) return false;
    (*dense_vector)[term.first] = IntegerValue(result);
  }
  return true;
}

}  // namespace

// Returns kMinIntegerValue in case of overflow.
//
// TODO(user): To avoid overflow, we could relax the constraint Sum term <= ub
// with Sum floor(term/divisor) <= floor(ub/divisor). It will be less precise,
// but we should be able to avoid overlow.
IntegerValue LinearProgrammingConstraint::GetImpliedLowerBound(
    const LinearExpression& terms) const {
  IntegerValue lower_bound(0);
  for (const auto term : terms) {
    const IntegerVariable var = integer_variables_[term.first.value()];
    const IntegerValue coeff = term.second;
    CHECK_NE(coeff, 0);
    const IntegerValue bound = coeff > 0 ? integer_trail_->LowerBound(var)
                                         : integer_trail_->UpperBound(var);
    const int64 prod = CapProd(bound.value(), coeff.value());
    if (prod == kint64min || prod == kint64max) return kMinIntegerValue;
    const int64 new_lb = CapAdd(lower_bound.value(), prod);
    if (new_lb == kint64min || new_lb == kint64max) return kMinIntegerValue;
    lower_bound = new_lb;
  }
  return lower_bound;
}

// TODO(user): combine this with RelaxLinearReason() to avoid the extra
// magnitude vector and the weird precondition of RelaxLinearReason().
void LinearProgrammingConstraint::SetImpliedLowerBoundReason(
    const LinearExpression& terms, IntegerValue slack) {
  integer_reason_.clear();
  std::vector<IntegerValue> magnitudes;
  for (const auto term : terms) {
    const IntegerVariable var = integer_variables_[term.first.value()];
    const IntegerValue coeff = term.second;
    CHECK_NE(coeff, 0);
    if (coeff > 0) {
      magnitudes.push_back(coeff);
      integer_reason_.push_back(integer_trail_->LowerBoundAsLiteral(var));
    } else {
      magnitudes.push_back(-coeff);
      integer_reason_.push_back(integer_trail_->UpperBoundAsLiteral(var));
    }
  }
  CHECK_GE(slack, 0);
  if (slack > 0) {
    integer_trail_->RelaxLinearReason(slack, magnitudes, &integer_reason_);
  }
  integer_trail_->RemoveLevelZeroBounds(&integer_reason_);
}

// TODO(user): Provide a sparse interface.
bool LinearProgrammingConstraint::ComputeNewLinearConstraint(
    bool take_objective_into_account,
    const glop::DenseColumn& dense_lp_multipliers, Fractional* scaling,
    gtl::ITIVector<ColIndex, IntegerValue>* dense_terms,
    IntegerValue* upper_bound) const {
  // Process the dense_lp_multipliers and compute their infinity norm.
  std::vector<std::pair<RowIndex, Fractional>> lp_multipliers;
  Fractional lp_multipliers_norm = take_objective_into_account ? 1.0 : 0.0;
  for (RowIndex row(0); row < dense_lp_multipliers.size(); ++row) {
    const Fractional lp_multi = dense_lp_multipliers[row];
    if (lp_multi == 0.0) continue;

    // Remove trivial bad cases.
    if (lp_multi > 0.0 && integer_lp_[row.value()].ub >= kMaxIntegerValue) {
      continue;
    }
    if (lp_multi < 0.0 && integer_lp_[row.value()].lb <= kMinIntegerValue) {
      continue;
    }
    lp_multipliers_norm = std::max(lp_multipliers_norm, std::abs(lp_multi));
    lp_multipliers.push_back({row, lp_multi});
  }

  // This scaling will be responsible to keep the wanted number of precision
  // digit when we round a floating point value to integer. We will want
  // around 6 digits for the larger lp_multi and less for the smaller ones.
  *scaling = 1.0;

  // Scale the lp_multipliers to the CP world (still Fractional though).
  std::vector<std::pair<RowIndex, Fractional>> cp_multipliers;
  Fractional min_cp_multi = glop::kInfinity;
  const Fractional global_scaling =
      bound_scaling_factor_ / lp_data_.objective_scaling_factor();
  for (const auto entry : lp_multipliers) {
    const RowIndex row = entry.first;
    const Fractional lp_multi = entry.second;

    // The LP guarantee about 6 digits of precision, so we ignore anything
    // smaller that lp_multipliers_norm * 1e-6.
    const Fractional magnitude_diff = lp_multipliers_norm / std::abs(lp_multi);
    if (magnitude_diff > 1e6) continue;

    // Scale back in the cp world.
    const Fractional cp_multi =
        lp_multi / scaler_.row_scale(row) / global_scaling;

    // We want std::round(cp_multi * scaling) to have the same number of
    // digits of relative precision as lp_multi.
    const Fractional wanted_scaling =
        (1e6 / magnitude_diff) / std::abs(cp_multi);
    *scaling = std::max(*scaling, wanted_scaling);

    min_cp_multi = std::min(std::abs(cp_multi), min_cp_multi);
    cp_multipliers.push_back({row, cp_multi});
  }

  // This behave exactly like if we had another "objective" constraint with
  // an lp_multi of 1.0 and a cp_multi of 1.0.
  if (take_objective_into_account) {
    *scaling = std::max(*scaling, 1e6 / lp_multipliers_norm);
  }

  // Scale the multipliers by *scaling.
  //
  // TODO(user): Maybe use int128 to avoid overflow?
  // TODO(user): Divide dual by gcd to limit overflow?
  // TODO(user): To avoid overflow, we could lower scaling at the cost of
  // loosing precision.
  std::vector<std::pair<RowIndex, IntegerValue>> integer_multipliers;
  for (const auto entry : cp_multipliers) {
    const IntegerValue coeff(std::round(entry.second * (*scaling)));
    if (coeff != 0) integer_multipliers.push_back({entry.first, coeff});
  }

  // Initialize the new constraint.
  *upper_bound = 0;
  dense_terms->assign(integer_variables_.size(), IntegerValue(0));

  // Compute the new constraint by taking the linear combination given by
  // integer_multipliers of the integer constraints in integer_lp_.
  const ColIndex num_cols(integer_variables_.size());
  for (const std::pair<RowIndex, IntegerValue> term : integer_multipliers) {
    const RowIndex row = term.first;
    const IntegerValue multiplier = term.second;
    CHECK_LT(row, integer_lp_.size());

    // Update the constraint.
    if (!AddLinearExpressionMultiple(multiplier, integer_lp_[row.value()].terms,
                                     dense_terms)) {
      return false;
    }

    // Update the upper bound.
    const int64 bound = multiplier > 0 ? integer_lp_[row.value()].ub.value()
                                       : integer_lp_[row.value()].lb.value();
    const int64 prod = CapProd(multiplier.value(), bound);
    if (prod == kint64min || prod == kint64max) return false;
    const int64 result = CapAdd((*upper_bound).value(), prod);
    if (result == kint64min || result == kint64max) return false;
    (*upper_bound) = IntegerValue(result);
  }

  return true;
}

// The "exact" computation go as follow:
//
// Given any INTEGER linear combination of the LP constraints, we can create a
// new integer constraint that is valid (its computation must not overflow
// though). Lets call this new_constraint. We can then always write for the
// objective linear expression:
//   objective = (objective - new_constraint) + new_constraint
// And we can compute a lower bound as folllow:
//   objective >= ImpliedLB(objective - new_constraint) + new_constraint_lb
// where ImpliedLB() is computed from the variable current bounds.
//
// Now, if we use for the linear combination and approximation of the optimal
// dual LP values (by scaling them and rounding them to integer), we will get an
// EXACT objective lower bound that is more or less the same as the inexact
// bound given by the LP relaxation. This allows to derive exact reasons for any
// propagation done by this constraint.
IntegerValue LinearProgrammingConstraint::ExactLpReasonning() {
  // Clear old reason and deductions.
  integer_reason_.clear();
  deductions_.clear();
  deductions_reason_.clear();

  // The row multipliers will be the negation of the LP duals.
  //
  // TODO(user): Provide and use a sparse API in Glop to get the duals.
  const RowIndex num_rows = simplex_.GetProblemNumRows();
  glop::DenseColumn lp_multipliers(num_rows);
  for (RowIndex row(0); row < num_rows; ++row) {
    lp_multipliers[row] = -simplex_.GetDualValue(row);
  }

  Fractional scaling;
  gtl::ITIVector<ColIndex, IntegerValue> reduced_costs;
  IntegerValue negative_lb;
  if (!ComputeNewLinearConstraint(/*take_objective_into_account=*/true,
                                  lp_multipliers, &scaling, &reduced_costs,
                                  &negative_lb)) {
    return kMinIntegerValue;  // Overflow.
  }

  // The "objective constraint" behave like if the unscaled cp multiplier was
  // 1.0, so we will multiply it by this number and add it to reduced_costs.
  const IntegerValue obj_scale(std::round(scaling));
  if (!AddLinearExpressionMultiple(obj_scale, integer_objective_,
                                   &reduced_costs)) {
    return kMinIntegerValue;  // Overflow.
  }

  // TODO(user): We could correct little imprecision by heuristically computing
  // for each row the best multiple to improve the scaled_objective_lb below
  // while keeping the reduced_costs of the same sign. This should improve the
  // objective lower bound.

  // Compute the objective lower bound, and the reason for it.
  const LinearExpression new_constraint =
      GetSparseRepresentation(reduced_costs);
  const IntegerValue lb = GetImpliedLowerBound(new_constraint);
  if (lb == kMinIntegerValue) return kMinIntegerValue;  // Overflow.
  const IntegerValue scaled_objective_lb(
      CapAdd(lb.value(), -negative_lb.value()));
  if (scaled_objective_lb == kint64min || scaled_objective_lb == kint64max) {
    return kMinIntegerValue;
  }

  const IntegerValue objective_ub = integer_trail_->UpperBound(objective_cp_);
  const IntegerValue scaled_objective_ub(
      CapProd(objective_ub.value(), obj_scale.value()));
  if (scaled_objective_ub == kint64min || scaled_objective_ub == kint64max) {
    return kMinIntegerValue;  // Overflow.
  }

  IntegerValue exact_objective_lb(
      CeilRatio(scaled_objective_lb.value(), obj_scale.value()));
  if (exact_objective_lb > objective_ub) {
    // We will have a conflict, so we can can relax more!
    exact_objective_lb = objective_ub + 1;
  } else {
    // Reduced cost strenghtening.
    //
    // Remark: This is nothing else than basic bound propagation of the
    // new_constraint with the given feasibility_slack between its implied lower
    // bound and its upper bound.
    IntegerValue explanation_slack = kMaxIntegerValue;
    const IntegerValue feasibility_slack = IntegerValue(
        CapSub(scaled_objective_ub.value(), scaled_objective_lb.value()));
    CHECK_GE(feasibility_slack, 0);
    if (feasibility_slack != kint64max) {
      for (const auto& term : new_constraint) {
        const IntegerVariable var = integer_variables_[term.first.value()];
        const IntegerValue coeff = term.second;
        CHECK_NE(coeff, 0);

        // Any change by more than this will make scaled_objective_lb go past
        // the objective upper bound
        const IntegerValue allowed_change(
            FloorRatio(feasibility_slack.value(), std::abs(coeff.value())));
        CHECK_GE(allowed_change, 0);
        if (coeff > 0) {
          const IntegerValue new_ub =
              integer_trail_->LowerBound(var) + allowed_change;
          if (new_ub < integer_trail_->UpperBound(var)) {
            explanation_slack =
                std::min(explanation_slack,
                         (allowed_change + 1) * coeff - feasibility_slack - 1);
            deductions_.push_back(IntegerLiteral::LowerOrEqual(var, new_ub));
          }
        } else {  // coeff < 0
          const IntegerValue new_lb =
              integer_trail_->UpperBound(var) - allowed_change;
          if (new_lb > integer_trail_->LowerBound(var)) {
            explanation_slack =
                std::min(explanation_slack,
                         (allowed_change + 1) * -coeff - feasibility_slack - 1);
            deductions_.push_back(IntegerLiteral::GreaterOrEqual(var, new_lb));
          }
        }
      }
    }

    if (!deductions_.empty()) {
      // TODO(user): Instead of taking explanation_slack as the min of the slack
      // of all deductions, we could use different reason for each push instead.
      // Experiment! Maybe there is some tradeoff depending on the number of
      // push.
      //
      // TODO(user): The individual reason are even smaller because we can
      // ignore the term corresponding to the variable we push.
      //
      // TODO(user): The proper fix might be to add a lazy reason code
      // that can reconstruct the relaxed reason on demand from a base one.
      // So we have better reason, and not more work at propagation time.
      // Also, this code should be shared with the one in IntegerSumLE since
      // they are the same, and it will facilitate unit-testing.
      SetImpliedLowerBoundReason(new_constraint, explanation_slack);
      deductions_reason_ = integer_reason_;
      deductions_reason_.push_back(
          integer_trail_->UpperBoundAsLiteral(objective_cp_));
    }
  }

  // Relax the lower bound reason.
  const IntegerValue min_value = (exact_objective_lb - 1) * obj_scale + 1;
  const IntegerValue slack = scaled_objective_lb - min_value;
  SetImpliedLowerBoundReason(new_constraint, slack);
  return exact_objective_lb;
}

bool LinearProgrammingConstraint::FillExactDualRayReason() {
  Fractional scaling;
  gtl::ITIVector<ColIndex, IntegerValue> dense_new_constraint;
  IntegerValue new_constraint_ub;
  if (!ComputeNewLinearConstraint(/*take_objective_into_account=*/false,
                                  simplex_.GetDualRay(), &scaling,
                                  &dense_new_constraint, &new_constraint_ub)) {
    return false;
  }
  const LinearExpression new_constraint =
      GetSparseRepresentation(dense_new_constraint);
  const IntegerValue implied_lb = GetImpliedLowerBound(new_constraint);
  if (implied_lb <= new_constraint_ub) {
    VLOG(1) << "LP exact dual ray not infeasible by "
            << (new_constraint_ub - implied_lb).value() / scaling;
    return false;
  }
  const IntegerValue slack = (implied_lb - new_constraint_ub) - 1;
  SetImpliedLowerBoundReason(new_constraint, slack);
  return true;
}

void LinearProgrammingConstraint::FillReducedCostsReason() {
  integer_reason_.clear();
  const int num_vars = integer_variables_.size();
  for (int i = 0; i < num_vars; i++) {
    const double rc = simplex_.GetReducedCost(glop::ColIndex(i));
    if (rc > kLpEpsilon) {
      integer_reason_.push_back(
          integer_trail_->LowerBoundAsLiteral(integer_variables_[i]));
    } else if (rc < -kLpEpsilon) {
      integer_reason_.push_back(
          integer_trail_->UpperBoundAsLiteral(integer_variables_[i]));
    }
  }

  integer_trail_->RemoveLevelZeroBounds(&integer_reason_);
}

void LinearProgrammingConstraint::FillDualRayReason() {
  integer_reason_.clear();
  const int num_vars = integer_variables_.size();
  for (int i = 0; i < num_vars; i++) {
    // TODO(user): Like for FillReducedCostsReason(), the bounds could be
    // extended here. Actually, the "dual ray cost updates" is the reduced cost
    // of an optimal solution if we were optimizing one direction of one basic
    // variable. The simplex_ interface would need to be slightly extended to
    // retrieve the basis column in question and the variable values though.
    const double rc = simplex_.GetDualRayRowCombination()[glop::ColIndex(i)];
    if (rc > kLpEpsilon) {
      integer_reason_.push_back(
          integer_trail_->LowerBoundAsLiteral(integer_variables_[i]));
    } else if (rc < -kLpEpsilon) {
      integer_reason_.push_back(
          integer_trail_->UpperBoundAsLiteral(integer_variables_[i]));
    }
  }
  integer_trail_->RemoveLevelZeroBounds(&integer_reason_);
}

void LinearProgrammingConstraint::ReducedCostStrengtheningDeductions(
    double cp_objective_delta) {
  deductions_.clear();

  // TRICKY: while simplex_.GetObjectiveValue() use the objective scaling factor
  // stored in the lp_data_, all the other functions like GetReducedCost() or
  // GetVariableValue() do not.
  const double lp_objective_delta =
      cp_objective_delta / lp_data_.objective_scaling_factor();
  const int num_vars = integer_variables_.size();
  for (int i = 0; i < num_vars; i++) {
    const IntegerVariable cp_var = integer_variables_[i];
    const glop::ColIndex lp_var = glop::ColIndex(i);
    const double rc = simplex_.GetReducedCost(lp_var);
    const double value = simplex_.GetVariableValue(lp_var);

    if (rc == 0.0) continue;
    const double lp_other_bound = value + lp_objective_delta / rc;
    const double cp_other_bound = lp_other_bound * LpToCpScalingFactor(lp_var);

    if (rc > kLpEpsilon) {
      const double ub = ToDouble(integer_trail_->UpperBound(cp_var));
      const double new_ub = std::floor(cp_other_bound + kCpEpsilon);
      if (new_ub < ub) {
        // TODO(user): Because rc > kLpEpsilon, the lower_bound of cp_var
        // will be part of the reason returned by FillReducedCostsReason(), but
        // we actually do not need it here. Same below.
        const IntegerValue new_ub_int(static_cast<IntegerValue>(new_ub));
        deductions_.push_back(IntegerLiteral::LowerOrEqual(cp_var, new_ub_int));
      }
    } else if (rc < -kLpEpsilon) {
      const double lb = ToDouble(integer_trail_->LowerBound(cp_var));
      const double new_lb = std::ceil(cp_other_bound - kCpEpsilon);
      if (new_lb > lb) {
        const IntegerValue new_lb_int(static_cast<IntegerValue>(new_lb));
        deductions_.push_back(
            IntegerLiteral::GreaterOrEqual(cp_var, new_lb_int));
      }
    }
  }
}

namespace {

// TODO(user): we could use a sparser algorithm, even if this do not seems to
// matter for now.
void AddIncomingAndOutgoingCutsIfNeeded(
    int num_nodes, const std::vector<int>& s, const std::vector<int>& tails,
    const std::vector<int>& heads, const std::vector<IntegerVariable>& vars,
    const std::vector<double>& lp_solution, int64 rhs_lower_bound,
    std::vector<LinearConstraint>* cuts) {
  LinearConstraint incoming;
  LinearConstraint outgoing;
  double sum_incoming = 0.0;
  double sum_outgoing = 0.0;
  incoming.lb = outgoing.lb = IntegerValue(rhs_lower_bound);
  incoming.ub = outgoing.ub = kMaxIntegerValue;
  const std::set<int> subset(s.begin(), s.end());

  // Add incoming/outgoing cut arcs, compute flow through cuts.
  for (int i = 0; i < tails.size(); ++i) {
    const bool out = gtl::ContainsKey(subset, tails[i]);
    const bool in = gtl::ContainsKey(subset, heads[i]);
    if (out && in) continue;
    if (out) {
      sum_outgoing += lp_solution[i];
      outgoing.vars.push_back(vars[i]);
      outgoing.coeffs.push_back(IntegerValue(1));
    }
    if (in) {
      sum_incoming += lp_solution[i];
      incoming.vars.push_back(vars[i]);
      incoming.coeffs.push_back(IntegerValue(1));
    }
  }

  // A node is said to be optional if it can be excluded from the subcircuit,
  // in which case there is a self-loop on that node.
  // If there are optional nodes, use extended formula:
  // sum(cut) >= 1 - optional_loop_in - optional_loop_out
  // where optional_loop_in's node is in subset, optional_loop_out's is out.
  // TODO(user): Favor optional loops fixed to zero at root.
  int num_optional_nodes_in = 0;
  int num_optional_nodes_out = 0;
  int optional_loop_in = -1;
  int optional_loop_out = -1;
  for (int i = 0; i < tails.size(); ++i) {
    if (tails[i] != heads[i]) continue;
    if (gtl::ContainsKey(subset, tails[i])) {
      num_optional_nodes_in++;
      if (optional_loop_in == -1 ||
          lp_solution[i] < lp_solution[optional_loop_in]) {
        optional_loop_in = i;
      }
    } else {
      num_optional_nodes_out++;
      if (optional_loop_out == -1 ||
          lp_solution[i] < lp_solution[optional_loop_out]) {
        optional_loop_out = i;
      }
    }
  }
  if (num_optional_nodes_in + num_optional_nodes_out > 0) {
    CHECK_EQ(rhs_lower_bound, 1);
    // When all optionals of one side are excluded in lp solution, no cut.
    if (num_optional_nodes_in == subset.size() &&
        (optional_loop_in == -1 ||
         lp_solution[optional_loop_in] > 1.0 - 1e-6)) {
      return;
    }
    if (num_optional_nodes_out == num_nodes - subset.size() &&
        (optional_loop_out == -1 ||
         lp_solution[optional_loop_out] > 1.0 - 1e-6)) {
      return;
    }

    // There is no mandatory node in subset, add optional_loop_in.
    if (num_optional_nodes_in == subset.size()) {
      incoming.vars.push_back(vars[optional_loop_in]);
      incoming.coeffs.push_back(IntegerValue(1));
      sum_incoming += lp_solution[optional_loop_in];

      outgoing.vars.push_back(vars[optional_loop_in]);
      outgoing.coeffs.push_back(IntegerValue(1));
      sum_outgoing += lp_solution[optional_loop_in];
    }

    // There is no mandatory node out of subset, add optional_loop_out.
    if (num_optional_nodes_out == num_nodes - subset.size()) {
      incoming.vars.push_back(vars[optional_loop_out]);
      incoming.coeffs.push_back(IntegerValue(1));
      sum_incoming += lp_solution[optional_loop_out];

      outgoing.vars.push_back(vars[optional_loop_out]);
      outgoing.coeffs.push_back(IntegerValue(1));
      sum_outgoing += lp_solution[optional_loop_out];
    }
  }

  if (sum_incoming < rhs_lower_bound - 1e-6) {
    cuts->push_back(std::move(incoming));
  }
  if (sum_outgoing < rhs_lower_bound - 1e-6) {
    cuts->push_back(std::move(outgoing));
  }
}

}  // namespace

// We use a basic algorithm to detect components that are not connected to the
// rest of the graph in the LP solution, and add cuts to force some arcs to
// enter and leave this component from outside.
CutGenerator CreateStronglyConnectedGraphCutGenerator(
    int num_nodes, const std::vector<int>& tails, const std::vector<int>& heads,
    const std::vector<IntegerVariable>& vars) {
  CutGenerator result;
  result.vars = vars;
  result.generate_cuts = [num_nodes, tails, heads,
                          vars](const std::vector<double>& lp_solution) {
    int num_arcs_in_lp_solution = 0;
    std::vector<std::vector<int>> graph(num_nodes);
    for (int i = 0; i < lp_solution.size(); ++i) {
      // TODO(user): a more advanced algorithm consist of adding the arcs
      // in the decreasing order of their lp_solution, and for each strongly
      // connected components S along the way, try to add the corresponding
      // cuts. We can stop as soon as there is only two components left, after
      // adding the corresponding cut.
      if (lp_solution[i] > 1e-6) {
        ++num_arcs_in_lp_solution;
        graph[tails[i]].push_back(heads[i]);
      }
    }
    std::vector<LinearConstraint> cuts;
    std::vector<std::vector<int>> components;
    FindStronglyConnectedComponents(num_nodes, graph, &components);
    if (components.size() == 1) return cuts;

    VLOG(1) << "num_arcs_in_lp_solution:" << num_arcs_in_lp_solution
            << " sccs:" << components.size();
    for (const std::vector<int>& component : components) {
      if (component.size() == 1) continue;
      AddIncomingAndOutgoingCutsIfNeeded(num_nodes, component, tails, heads,
                                         vars, lp_solution,
                                         /*rhs_lower_bound=*/1, &cuts);

      // In this case, the cuts for each component are the same.
      if (components.size() == 2) break;
    }
    return cuts;
  };
  return result;
}

CutGenerator CreateCVRPCutGenerator(int num_nodes,
                                    const std::vector<int>& tails,
                                    const std::vector<int>& heads,
                                    const std::vector<IntegerVariable>& vars,
                                    const std::vector<int64>& demands,
                                    int64 capacity) {
  CHECK_GT(capacity, 0);
  int64 total_demands = 0;
  for (const int64 demand : demands) total_demands += demand;

  CutGenerator result;
  result.vars = vars;
  result.generate_cuts = [num_nodes, tails, heads, total_demands, demands,
                          capacity,
                          vars](const std::vector<double>& lp_solution) {
    int num_arcs_in_lp_solution = 0;
    std::vector<std::vector<int>> graph(num_nodes);
    for (int i = 0; i < lp_solution.size(); ++i) {
      if (lp_solution[i] > 1e-6) {
        ++num_arcs_in_lp_solution;
        graph[tails[i]].push_back(heads[i]);
      }
    }
    std::vector<LinearConstraint> cuts;
    std::vector<std::vector<int>> components;
    FindStronglyConnectedComponents(num_nodes, graph, &components);
    if (components.size() == 1) return cuts;

    VLOG(1) << "num_arcs_in_lp_solution:" << num_arcs_in_lp_solution
            << " sccs:" << components.size();
    for (const std::vector<int>& component : components) {
      if (component.size() == 1) continue;

      bool contain_depot = false;
      int64 component_demand = 0;
      for (const int node : component) {
        if (node == 0) contain_depot = true;
        component_demand += demands[node];
      }
      const int min_num_vehicles =
          contain_depot
              ? (total_demands - component_demand + capacity - 1) / capacity
              : (component_demand + capacity - 1) / capacity;
      CHECK_GE(min_num_vehicles, 1);

      AddIncomingAndOutgoingCutsIfNeeded(
          num_nodes, component, tails, heads, vars, lp_solution,
          /*rhs_lower_bound=*/min_num_vehicles, &cuts);

      // In this case, the cuts for each component are the same.
      if (components.size() == 2) break;
    }
    return cuts;
  };
  return result;
}

std::function<LiteralIndex()>
LinearProgrammingConstraint::HeuristicLPMostInfeasibleBinary(Model* model) {
  IntegerTrail* integer_trail = integer_trail_;
  IntegerEncoder* integer_encoder = model->GetOrCreate<IntegerEncoder>();
  // Gather all 0-1 variables that appear in some LP.
  std::vector<IntegerVariable> variables;
  for (IntegerVariable var : integer_variables_) {
    if (integer_trail_->LowerBound(var) == 0 &&
        integer_trail_->UpperBound(var) == 1) {
      variables.push_back(var);
    }
  }
  VLOG(1) << "HeuristicLPMostInfeasibleBinary has " << variables.size()
          << " variables.";

  return [this, variables, integer_trail, integer_encoder]() {
    const double kEpsilon = 1e-6;
    // Find most fractional value.
    IntegerVariable fractional_var = kNoIntegerVariable;
    double fractional_distance_best = -1.0;
    for (const IntegerVariable var : variables) {
      // Skip ignored and fixed variables.
      if (integer_trail_->IsCurrentlyIgnored(var)) continue;
      const IntegerValue lb = integer_trail_->LowerBound(var);
      const IntegerValue ub = integer_trail_->UpperBound(var);
      if (lb == ub) continue;

      // Check variable's support is fractional.
      const double lp_value = this->GetSolutionValue(var);
      const double fractional_distance =
          std::min(std::ceil(lp_value - kEpsilon) - lp_value,
                   lp_value - std::floor(lp_value + kEpsilon));
      if (fractional_distance < kEpsilon) continue;

      // Keep variable if it is farther from integrality than the previous.
      if (fractional_distance > fractional_distance_best) {
        fractional_var = var;
        fractional_distance_best = fractional_distance;
      }
    }

    if (fractional_var != kNoIntegerVariable) {
      return integer_encoder
          ->GetOrCreateAssociatedLiteral(
              IntegerLiteral::GreaterOrEqual(fractional_var, IntegerValue(1)))
          .Index();
    }
    return kNoLiteralIndex;
  };
}

std::function<LiteralIndex()>
LinearProgrammingConstraint::HeuristicLPPseudoCostBinary(Model* model) {
  // Gather all 0-1 variables that appear in this LP.
  std::vector<IntegerVariable> variables;
  for (IntegerVariable var : integer_variables_) {
    if (integer_trail_->LowerBound(var) == 0 &&
        integer_trail_->UpperBound(var) == 1) {
      variables.push_back(var);
    }
  }
  VLOG(1) << "HeuristicLPPseudoCostBinary has " << variables.size()
          << " variables.";

  // Store average of reduced cost from 1 to 0. The best heuristic only sets
  // variables to one and cares about cost to zero, even though classic
  // pseudocost will use max_var min(cost_to_one[var], cost_to_zero[var]).
  const int num_vars = variables.size();
  std::vector<double> cost_to_zero(num_vars, 0.0);
  std::vector<int> num_cost_to_zero(num_vars);
  int num_calls = 0;

  IntegerEncoder* integer_encoder = model->GetOrCreate<IntegerEncoder>();
  return [=]() mutable {
    const double kEpsilon = 1e-6;

    // Every 10000 calls, decay pseudocosts.
    num_calls++;
    if (num_calls == 10000) {
      for (int i = 0; i < num_vars; i++) {
        cost_to_zero[i] /= 2;
        num_cost_to_zero[i] /= 2;
      }
      num_calls = 0;
    }

    // Accumulate pseudo-costs of all unassigned variables.
    for (int i = 0; i < num_vars; i++) {
      const IntegerVariable var = variables[i];
      // Skip ignored and fixed variables.
      if (integer_trail_->IsCurrentlyIgnored(var)) continue;
      const IntegerValue lb = integer_trail_->LowerBound(var);
      const IntegerValue ub = integer_trail_->UpperBound(var);
      if (lb == ub) continue;

      const double rc = this->GetSolutionReducedCost(var);
      // Skip reduced costs that are nonzero because of numerical issues.
      if (std::abs(rc) < kEpsilon) continue;

      const double value = std::round(this->GetSolutionValue(var));
      if (value == 1.0 && rc < 0.0) {
        cost_to_zero[i] -= rc;
        num_cost_to_zero[i]++;
      }
    }

    // Select noninstantiated variable with highest pseudo-cost.
    int selected_index = -1;
    double best_cost = 0.0;
    for (int i = 0; i < num_vars; i++) {
      const IntegerVariable var = variables[i];
      // Skip ignored and fixed variables.
      if (integer_trail_->IsCurrentlyIgnored(var)) continue;
      const IntegerValue lb = integer_trail_->LowerBound(var);
      const IntegerValue ub = integer_trail_->UpperBound(var);
      if (lb == ub) continue;

      if (num_cost_to_zero[i] > 0 &&
          best_cost < cost_to_zero[i] / num_cost_to_zero[i]) {
        best_cost = cost_to_zero[i] / num_cost_to_zero[i];
        selected_index = i;
      }
    }

    if (selected_index >= 0) {
      const Literal decision = integer_encoder->GetOrCreateAssociatedLiteral(
          IntegerLiteral::GreaterOrEqual(variables[selected_index],
                                         IntegerValue(1)));
      return decision.Index();
    }

    return kNoLiteralIndex;
  };
}

std::function<LiteralIndex()>
LinearProgrammingConstraint::LPReducedCostAverageBranching() {
  if (!compute_reduced_cost_averages_) {
    compute_reduced_cost_averages_ = true;
    const int num_vars = integer_variables_.size();
    VLOG(1) << " LPReducedCostAverageBranching has #variables: " << num_vars;
    sum_cost_down_.resize(num_vars, 0.0);
    num_cost_down_.resize(num_vars, 0);
    sum_cost_up_.resize(num_vars, 0.0);
    num_cost_up_.resize(num_vars, 0);
  }

  return [this]() { return this->LPReducedCostAverageDecision(); };
}

LiteralIndex LinearProgrammingConstraint::LPReducedCostAverageDecision() {
  const int num_vars = integer_variables_.size();
  // Select noninstantiated variable with highest pseudo-cost.
  int selected_index = -1;
  double best_cost = 0.0;
  for (int i = 0; i < num_vars; i++) {
    const IntegerVariable var = this->integer_variables_[i];
    // Skip ignored and fixed variables.
    if (integer_trail_->IsCurrentlyIgnored(var)) continue;
    const IntegerValue lb = integer_trail_->LowerBound(var);
    const IntegerValue ub = integer_trail_->UpperBound(var);
    if (lb == ub) continue;

    // If only one direction exist, we takes its value divided by 2, so that
    // such variable should have a smaller cost than the min of the two side
    // except if one direction have a really high reduced costs.
    double cost_i = 0.0;
    if (num_cost_down_[i] > 0 && num_cost_up_[i] > 0) {
      cost_i = std::min(sum_cost_down_[i] / num_cost_down_[i],
                        sum_cost_up_[i] / num_cost_up_[i]);
    } else {
      const double divisor = num_cost_down_[i] + num_cost_up_[i];
      if (divisor != 0) {
        cost_i = 0.5 * (sum_cost_down_[i] + sum_cost_up_[i]) / divisor;
      }
    }

    if (selected_index == -1 || cost_i > best_cost) {
      best_cost = cost_i;
      selected_index = i;
    }
  }

  if (selected_index == -1) return kNoLiteralIndex;
  const IntegerVariable var = this->integer_variables_[selected_index];

  // If ceil(value) is current upper bound, try var == upper bound first.
  // Guarding with >= prevents numerical problems.
  // With 0/1 variables, this will tend to try setting to 1 first,
  // which produces more shallow trees.
  const IntegerValue ub = integer_trail_->UpperBound(var);
  const IntegerValue value_ceil(
      std::ceil(this->GetSolutionValue(var) - kCpEpsilon));
  if (value_ceil >= ub) {
    return integer_encoder_
        ->GetOrCreateAssociatedLiteral(IntegerLiteral::GreaterOrEqual(var, ub))
        .Index();
  }

  // If floor(value) is current lower bound, try var == lower bound first.
  // Guarding with <= prevents numerical problems.
  const IntegerValue lb = integer_trail_->LowerBound(var);
  const IntegerValue value_floor(
      std::floor(this->GetSolutionValue(var) + kCpEpsilon));
  if (value_floor <= lb) {
    return integer_encoder_
        ->GetOrCreateAssociatedLiteral(IntegerLiteral::LowerOrEqual(var, lb))
        .Index();
  }

  // Here lb < value_floor <= value_ceil < ub.
  // Try the most promising split between var <= floor or var >= ceil.
  if (sum_cost_down_[selected_index] / num_cost_down_[selected_index] <
      sum_cost_up_[selected_index] / num_cost_up_[selected_index]) {
    return integer_encoder_
        ->GetOrCreateAssociatedLiteral(
            IntegerLiteral::LowerOrEqual(var, value_floor))
        .Index();
  } else {
    return integer_encoder_
        ->GetOrCreateAssociatedLiteral(
            IntegerLiteral::GreaterOrEqual(var, value_ceil))
        .Index();
  }
}

}  // namespace sat
}  // namespace operations_research

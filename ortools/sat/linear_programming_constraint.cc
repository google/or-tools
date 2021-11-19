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

#include "ortools/sat/linear_programming_constraint.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/numeric/int128.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/preprocessor.h"
#include "ortools/glop/status.h"
#include "ortools/graph/strongly_connected_components.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/zero_half_cuts.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {
namespace sat {

using glop::ColIndex;
using glop::Fractional;
using glop::RowIndex;

void ScatteredIntegerVector::ClearAndResize(int size) {
  if (is_sparse_) {
    for (const glop::ColIndex col : non_zeros_) {
      dense_vector_[col] = IntegerValue(0);
    }
    dense_vector_.resize(size, IntegerValue(0));
  } else {
    dense_vector_.assign(size, IntegerValue(0));
  }
  for (const glop::ColIndex col : non_zeros_) {
    is_zeros_[col] = true;
  }
  is_zeros_.resize(size, true);
  non_zeros_.clear();
  is_sparse_ = true;
}

bool ScatteredIntegerVector::Add(glop::ColIndex col, IntegerValue value) {
  const int64_t add = CapAdd(value.value(), dense_vector_[col].value());
  if (add == std::numeric_limits<int64_t>::min() ||
      add == std::numeric_limits<int64_t>::max())
    return false;
  dense_vector_[col] = IntegerValue(add);
  if (is_sparse_ && is_zeros_[col]) {
    is_zeros_[col] = false;
    non_zeros_.push_back(col);
  }
  return true;
}

bool ScatteredIntegerVector::AddLinearExpressionMultiple(
    IntegerValue multiplier,
    const std::vector<std::pair<glop::ColIndex, IntegerValue>>& terms) {
  const double threshold = 0.1 * static_cast<double>(dense_vector_.size());
  if (is_sparse_ && static_cast<double>(terms.size()) < threshold) {
    for (const std::pair<glop::ColIndex, IntegerValue> term : terms) {
      if (is_zeros_[term.first]) {
        is_zeros_[term.first] = false;
        non_zeros_.push_back(term.first);
      }
      if (!AddProductTo(multiplier, term.second, &dense_vector_[term.first])) {
        return false;
      }
    }
    if (static_cast<double>(non_zeros_.size()) > threshold) {
      is_sparse_ = false;
    }
  } else {
    is_sparse_ = false;
    for (const std::pair<glop::ColIndex, IntegerValue> term : terms) {
      if (!AddProductTo(multiplier, term.second, &dense_vector_[term.first])) {
        return false;
      }
    }
  }
  return true;
}

void ScatteredIntegerVector::ConvertToLinearConstraint(
    const std::vector<IntegerVariable>& integer_variables,
    IntegerValue upper_bound, LinearConstraint* result) {
  result->vars.clear();
  result->coeffs.clear();
  if (is_sparse_) {
    std::sort(non_zeros_.begin(), non_zeros_.end());
    for (const glop::ColIndex col : non_zeros_) {
      const IntegerValue coeff = dense_vector_[col];
      if (coeff == 0) continue;
      result->vars.push_back(integer_variables[col.value()]);
      result->coeffs.push_back(coeff);
    }
  } else {
    const int size = dense_vector_.size();
    for (glop::ColIndex col(0); col < size; ++col) {
      const IntegerValue coeff = dense_vector_[col];
      if (coeff == 0) continue;
      result->vars.push_back(integer_variables[col.value()]);
      result->coeffs.push_back(coeff);
    }
  }
  result->lb = kMinIntegerValue;
  result->ub = upper_bound;
}

std::vector<std::pair<glop::ColIndex, IntegerValue>>
ScatteredIntegerVector::GetTerms() {
  std::vector<std::pair<glop::ColIndex, IntegerValue>> result;
  if (is_sparse_) {
    std::sort(non_zeros_.begin(), non_zeros_.end());
    for (const glop::ColIndex col : non_zeros_) {
      const IntegerValue coeff = dense_vector_[col];
      if (coeff != 0) result.push_back({col, coeff});
    }
  } else {
    const int size = dense_vector_.size();
    for (glop::ColIndex col(0); col < size; ++col) {
      const IntegerValue coeff = dense_vector_[col];
      if (coeff != 0) result.push_back({col, coeff});
    }
  }
  return result;
}

// TODO(user): make SatParameters singleton too, otherwise changing them after
// a constraint was added will have no effect on this class.
LinearProgrammingConstraint::LinearProgrammingConstraint(Model* model)
    : constraint_manager_(model),
      parameters_(*(model->GetOrCreate<SatParameters>())),
      model_(model),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      trail_(model->GetOrCreate<Trail>()),
      integer_encoder_(model->GetOrCreate<IntegerEncoder>()),
      random_(model->GetOrCreate<ModelRandomGenerator>()),
      implied_bounds_processor_({}, integer_trail_,
                                model->GetOrCreate<ImpliedBounds>()),
      dispatcher_(model->GetOrCreate<LinearProgrammingDispatcher>()),
      expanded_lp_solution_(
          *model->GetOrCreate<LinearProgrammingConstraintLpSolution>()) {
  // Tweak the default parameters to make the solve incremental.
  glop::GlopParameters parameters;
  parameters.set_use_dual_simplex(true);
  simplex_.SetParameters(parameters);
  if (parameters_.use_branching_in_lp() ||
      parameters_.search_branching() == SatParameters::LP_SEARCH) {
    compute_reduced_cost_averages_ = true;
  }

  // Register our local rev int repository.
  integer_trail_->RegisterReversibleClass(&rc_rev_int_repository_);
}

void LinearProgrammingConstraint::AddLinearConstraint(
    const LinearConstraint& ct) {
  DCHECK(!lp_constraint_is_registered_);
  constraint_manager_.Add(ct);

  // We still create the mirror variable right away though.
  //
  // TODO(user): clean this up? Note that it is important that the variable
  // in lp_data_ never changes though, so we can restart from the current
  // lp solution and be incremental (even if the constraints changed).
  for (const IntegerVariable var : ct.vars) {
    GetOrCreateMirrorVariable(PositiveVariable(var));
  }
}

glop::ColIndex LinearProgrammingConstraint::GetOrCreateMirrorVariable(
    IntegerVariable positive_variable) {
  DCHECK(VariableIsPositive(positive_variable));
  const auto it = mirror_lp_variable_.find(positive_variable);
  if (it == mirror_lp_variable_.end()) {
    const glop::ColIndex col(integer_variables_.size());
    implied_bounds_processor_.AddLpVariable(positive_variable);
    mirror_lp_variable_[positive_variable] = col;
    integer_variables_.push_back(positive_variable);
    lp_solution_.push_back(std::numeric_limits<double>::infinity());
    lp_reduced_cost_.push_back(0.0);
    (*dispatcher_)[positive_variable] = this;

    const int index = std::max(positive_variable.value(),
                               NegationOf(positive_variable).value());
    if (index >= expanded_lp_solution_.size()) {
      expanded_lp_solution_.resize(index + 1, 0.0);
    }
    return col;
  }
  return it->second;
}

void LinearProgrammingConstraint::SetObjectiveCoefficient(IntegerVariable ivar,
                                                          IntegerValue coeff) {
  CHECK(!lp_constraint_is_registered_);
  objective_is_defined_ = true;
  IntegerVariable pos_var = VariableIsPositive(ivar) ? ivar : NegationOf(ivar);
  if (ivar != pos_var) coeff = -coeff;

  constraint_manager_.SetObjectiveCoefficient(pos_var, coeff);
  const glop::ColIndex col = GetOrCreateMirrorVariable(pos_var);
  integer_objective_.push_back({col, coeff});
  objective_infinity_norm_ =
      std::max(objective_infinity_norm_, IntTypeAbs(coeff));
}

// TODO(user): As the search progress, some variables might get fixed. Exploit
// this to reduce the number of variables in the LP and in the
// ConstraintManager? We might also detect during the search that two variable
// are equivalent.
//
// TODO(user): On TSP/VRP with a lot of cuts, this can take 20% of the overall
// running time. We should be able to almost remove most of this from the
// profile by being more incremental (modulo LP scaling).
//
// TODO(user): A longer term idea for LP with a lot of variables is to not
// add all variables to each LP solve and do some "sifting". That can be useful
// for TSP for instance where the number of edges is large, but only a small
// fraction will be used in the optimal solution.
bool LinearProgrammingConstraint::CreateLpFromConstraintManager() {
  // Fill integer_lp_.
  integer_lp_.clear();
  infinity_norms_.clear();
  const auto& all_constraints = constraint_manager_.AllConstraints();
  for (const auto index : constraint_manager_.LpConstraints()) {
    const LinearConstraint& ct = all_constraints[index].constraint;

    integer_lp_.push_back(LinearConstraintInternal());
    LinearConstraintInternal& new_ct = integer_lp_.back();
    new_ct.lb = ct.lb;
    new_ct.ub = ct.ub;
    const int size = ct.vars.size();
    IntegerValue infinity_norm(0);
    if (ct.lb > ct.ub) {
      VLOG(1) << "Trivial infeasible bound in an LP constraint";
      return false;
    }
    if (ct.lb > kMinIntegerValue) {
      infinity_norm = std::max(infinity_norm, IntTypeAbs(ct.lb));
    }
    if (ct.ub < kMaxIntegerValue) {
      infinity_norm = std::max(infinity_norm, IntTypeAbs(ct.ub));
    }
    for (int i = 0; i < size; ++i) {
      // We only use positive variable inside this class.
      IntegerVariable var = ct.vars[i];
      IntegerValue coeff = ct.coeffs[i];
      if (!VariableIsPositive(var)) {
        var = NegationOf(var);
        coeff = -coeff;
      }
      infinity_norm = std::max(infinity_norm, IntTypeAbs(coeff));
      new_ct.terms.push_back({GetOrCreateMirrorVariable(var), coeff});
    }
    infinity_norms_.push_back(infinity_norm);

    // Important to keep lp_data_ "clean".
    std::sort(new_ct.terms.begin(), new_ct.terms.end());
  }

  // Copy the integer_lp_ into lp_data_.
  lp_data_.Clear();
  for (int i = 0; i < integer_variables_.size(); ++i) {
    CHECK_EQ(glop::ColIndex(i), lp_data_.CreateNewVariable());
  }

  // We remove fixed variables from the objective. This should help the LP
  // scaling, but also our integer reason computation.
  int new_size = 0;
  objective_infinity_norm_ = 0;
  for (const auto entry : integer_objective_) {
    const IntegerVariable var = integer_variables_[entry.first.value()];
    if (integer_trail_->IsFixedAtLevelZero(var)) {
      integer_objective_offset_ +=
          entry.second * integer_trail_->LevelZeroLowerBound(var);
      continue;
    }
    objective_infinity_norm_ =
        std::max(objective_infinity_norm_, IntTypeAbs(entry.second));
    integer_objective_[new_size++] = entry;
    lp_data_.SetObjectiveCoefficient(entry.first, ToDouble(entry.second));
  }
  objective_infinity_norm_ =
      std::max(objective_infinity_norm_, IntTypeAbs(integer_objective_offset_));
  integer_objective_.resize(new_size);
  lp_data_.SetObjectiveOffset(ToDouble(integer_objective_offset_));

  for (const LinearConstraintInternal& ct : integer_lp_) {
    const ConstraintIndex row = lp_data_.CreateNewConstraint();
    lp_data_.SetConstraintBounds(row, ToDouble(ct.lb), ToDouble(ct.ub));
    for (const auto& term : ct.terms) {
      lp_data_.SetCoefficient(row, term.first, ToDouble(term.second));
    }
  }
  lp_data_.NotifyThatColumnsAreClean();

  // We scale the LP using the level zero bounds that we later override
  // with the current ones.
  //
  // TODO(user): As part of the scaling, we may also want to shift the initial
  // variable bounds so that each variable contain the value zero in their
  // domain. Maybe just once and for all at the beginning.
  const int num_vars = integer_variables_.size();
  for (int i = 0; i < num_vars; i++) {
    const IntegerVariable cp_var = integer_variables_[i];
    const double lb = ToDouble(integer_trail_->LevelZeroLowerBound(cp_var));
    const double ub = ToDouble(integer_trail_->LevelZeroUpperBound(cp_var));
    lp_data_.SetVariableBounds(glop::ColIndex(i), lb, ub);
  }

  // TODO(user): As we have an idea of the LP optimal after the first solves,
  // maybe we can adapt the scaling accordingly.
  glop::GlopParameters params;
  params.set_cost_scaling(glop::GlopParameters::MEAN_COST_SCALING);
  scaler_.Scale(params, &lp_data_);
  UpdateBoundsOfLpVariables();

  // Set the information for the step to polish the LP basis. All our variables
  // are integer, but for now, we just try to minimize the fractionality of the
  // binary variables.
  if (parameters_.polish_lp_solution()) {
    simplex_.ClearIntegralityScales();
    for (int i = 0; i < num_vars; ++i) {
      const IntegerVariable cp_var = integer_variables_[i];
      const IntegerValue lb = integer_trail_->LevelZeroLowerBound(cp_var);
      const IntegerValue ub = integer_trail_->LevelZeroUpperBound(cp_var);
      if (lb != 0 || ub != 1) continue;
      simplex_.SetIntegralityScale(
          glop::ColIndex(i),
          1.0 / scaler_.VariableScalingFactor(glop::ColIndex(i)));
    }
  }

  lp_data_.NotifyThatColumnsAreClean();
  VLOG(1) << "LP relaxation: " << lp_data_.GetDimensionString() << ". "
          << constraint_manager_.AllConstraints().size()
          << " Managed constraints.";
  return true;
}

LPSolveInfo LinearProgrammingConstraint::SolveLpForBranching() {
  LPSolveInfo info;
  glop::BasisState basis_state = simplex_.GetState();

  const glop::Status status = simplex_.Solve(lp_data_, time_limit_);
  total_num_simplex_iterations_ += simplex_.GetNumberOfIterations();
  simplex_.LoadStateForNextSolve(basis_state);
  if (!status.ok()) {
    VLOG(1) << "The LP solver encountered an error: " << status.error_message();
    info.status = glop::ProblemStatus::ABNORMAL;
    return info;
  }
  info.status = simplex_.GetProblemStatus();
  if (info.status == glop::ProblemStatus::OPTIMAL ||
      info.status == glop::ProblemStatus::DUAL_FEASIBLE) {
    // Record the objective bound.
    info.lp_objective = simplex_.GetObjectiveValue();
    info.new_obj_bound = IntegerValue(
        static_cast<int64_t>(std::ceil(info.lp_objective - kCpEpsilon)));
  }
  return info;
}

void LinearProgrammingConstraint::FillReducedCostReasonIn(
    const glop::DenseRow& reduced_costs,
    std::vector<IntegerLiteral>* integer_reason) {
  integer_reason->clear();
  const int num_vars = integer_variables_.size();
  for (int i = 0; i < num_vars; i++) {
    const double rc = reduced_costs[glop::ColIndex(i)];
    if (rc > kLpEpsilon) {
      integer_reason->push_back(
          integer_trail_->LowerBoundAsLiteral(integer_variables_[i]));
    } else if (rc < -kLpEpsilon) {
      integer_reason->push_back(
          integer_trail_->UpperBoundAsLiteral(integer_variables_[i]));
    }
  }

  integer_trail_->RemoveLevelZeroBounds(integer_reason);
}

bool LinearProgrammingConstraint::BranchOnVar(IntegerVariable positive_var) {
  // From the current LP solution, branch on the given var if fractional.
  DCHECK(lp_solution_is_set_);
  const double current_value = GetSolutionValue(positive_var);
  DCHECK_GT(std::abs(current_value - std::round(current_value)), kCpEpsilon);

  // Used as empty reason in this method.
  integer_reason_.clear();

  bool deductions_were_made = false;

  UpdateBoundsOfLpVariables();

  const IntegerValue current_obj_lb = integer_trail_->LowerBound(objective_cp_);
  // This will try to branch in both direction around the LP value of the
  // given variable and push any deduction done this way.

  const glop::ColIndex lp_var = GetOrCreateMirrorVariable(positive_var);
  const double current_lb = ToDouble(integer_trail_->LowerBound(positive_var));
  const double current_ub = ToDouble(integer_trail_->UpperBound(positive_var));
  const double factor = scaler_.VariableScalingFactor(lp_var);
  if (current_value < current_lb || current_value > current_ub) {
    return false;
  }

  // Form LP1 var <= floor(current_value)
  const double new_ub = std::floor(current_value);
  lp_data_.SetVariableBounds(lp_var, current_lb * factor, new_ub * factor);

  LPSolveInfo lower_branch_info = SolveLpForBranching();
  if (lower_branch_info.status != glop::ProblemStatus::OPTIMAL &&
      lower_branch_info.status != glop::ProblemStatus::DUAL_FEASIBLE &&
      lower_branch_info.status != glop::ProblemStatus::DUAL_UNBOUNDED) {
    return false;
  }

  if (lower_branch_info.status == glop::ProblemStatus::DUAL_UNBOUNDED) {
    // Push the other branch.
    const IntegerLiteral deduction = IntegerLiteral::GreaterOrEqual(
        positive_var, IntegerValue(std::ceil(current_value)));
    if (!integer_trail_->Enqueue(deduction, {}, integer_reason_)) {
      return false;
    }
    deductions_were_made = true;
  } else if (lower_branch_info.new_obj_bound <= current_obj_lb) {
    return false;
  }

  // Form LP2 var >= ceil(current_value)
  const double new_lb = std::ceil(current_value);
  lp_data_.SetVariableBounds(lp_var, new_lb * factor, current_ub * factor);

  LPSolveInfo upper_branch_info = SolveLpForBranching();
  if (upper_branch_info.status != glop::ProblemStatus::OPTIMAL &&
      upper_branch_info.status != glop::ProblemStatus::DUAL_FEASIBLE &&
      upper_branch_info.status != glop::ProblemStatus::DUAL_UNBOUNDED) {
    return deductions_were_made;
  }

  if (upper_branch_info.status == glop::ProblemStatus::DUAL_UNBOUNDED) {
    // Push the other branch if not infeasible.
    if (lower_branch_info.status != glop::ProblemStatus::DUAL_UNBOUNDED) {
      const IntegerLiteral deduction = IntegerLiteral::LowerOrEqual(
          positive_var, IntegerValue(std::floor(current_value)));
      if (!integer_trail_->Enqueue(deduction, {}, integer_reason_)) {
        return deductions_were_made;
      }
      deductions_were_made = true;
    }
  } else if (upper_branch_info.new_obj_bound <= current_obj_lb) {
    return deductions_were_made;
  }

  IntegerValue approximate_obj_lb = kMinIntegerValue;

  if (lower_branch_info.status == glop::ProblemStatus::DUAL_UNBOUNDED &&
      upper_branch_info.status == glop::ProblemStatus::DUAL_UNBOUNDED) {
    return integer_trail_->ReportConflict(integer_reason_);
  } else if (lower_branch_info.status == glop::ProblemStatus::DUAL_UNBOUNDED) {
    approximate_obj_lb = upper_branch_info.new_obj_bound;
  } else if (upper_branch_info.status == glop::ProblemStatus::DUAL_UNBOUNDED) {
    approximate_obj_lb = lower_branch_info.new_obj_bound;
  } else {
    approximate_obj_lb = std::min(lower_branch_info.new_obj_bound,
                                  upper_branch_info.new_obj_bound);
  }

  // NOTE: On some problems, the approximate_obj_lb could be inexact which add
  // some tolerance to CP-SAT where currently there is none.
  if (approximate_obj_lb <= current_obj_lb) return deductions_were_made;

  // Push the bound to the trail.
  const IntegerLiteral deduction =
      IntegerLiteral::GreaterOrEqual(objective_cp_, approximate_obj_lb);
  if (!integer_trail_->Enqueue(deduction, {}, integer_reason_)) {
    return deductions_were_made;
  }

  return true;
}

void LinearProgrammingConstraint::RegisterWith(Model* model) {
  DCHECK(!lp_constraint_is_registered_);
  lp_constraint_is_registered_ = true;
  model->GetOrCreate<LinearProgrammingConstraintCollection>()->push_back(this);

  // Note fdid, this is not really needed by should lead to better cache
  // locality.
  std::sort(integer_objective_.begin(), integer_objective_.end());

  // Set the LP to its initial content.
  if (!parameters_.add_lp_constraints_lazily()) {
    constraint_manager_.AddAllConstraintsToLp();
  }
  if (!CreateLpFromConstraintManager()) {
    model->GetOrCreate<SatSolver>()->NotifyThatModelIsUnsat();
    return;
  }

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
  watcher->AlwaysCallAtLevelZero(watcher_id);

  // Registering it with the trail make sure this class is always in sync when
  // it is used in the decision heuristics.
  integer_trail_->RegisterReversibleClass(this);
  watcher->RegisterReversibleInt(watcher_id, &rev_optimal_constraints_size_);
}

void LinearProgrammingConstraint::SetLevel(int level) {
  optimal_constraints_.resize(rev_optimal_constraints_size_);
  if (lp_solution_is_set_ && level < lp_solution_level_) {
    lp_solution_is_set_ = false;
  }

  // Special case for level zero, we "reload" any previously known optimal
  // solution from that level.
  //
  // TODO(user): Keep all optimal solution in the current branch?
  // TODO(user): Still try to add cuts/constraints though!
  if (level == 0 && !level_zero_lp_solution_.empty()) {
    lp_solution_is_set_ = true;
    lp_solution_ = level_zero_lp_solution_;
    lp_solution_level_ = 0;
    for (int i = 0; i < lp_solution_.size(); i++) {
      expanded_lp_solution_[integer_variables_[i]] = lp_solution_[i];
      expanded_lp_solution_[NegationOf(integer_variables_[i])] =
          -lp_solution_[i];
    }
  }
}

void LinearProgrammingConstraint::AddCutGenerator(CutGenerator generator) {
  for (const IntegerVariable var : generator.vars) {
    GetOrCreateMirrorVariable(VariableIsPositive(var) ? var : NegationOf(var));
  }
  cut_generators_.push_back(std::move(generator));
}

bool LinearProgrammingConstraint::IncrementalPropagate(
    const std::vector<int>& watch_indices) {
  if (!lp_solution_is_set_) return Propagate();

  // At level zero, if there is still a chance to add cuts or lazy constraints,
  // we re-run the LP.
  if (trail_->CurrentDecisionLevel() == 0 && !lp_at_level_zero_is_final_) {
    return Propagate();
  }

  // Check whether the change breaks the current LP solution. If it does, call
  // Propagate() on the current LP.
  for (const int index : watch_indices) {
    const double lb =
        ToDouble(integer_trail_->LowerBound(integer_variables_[index]));
    const double ub =
        ToDouble(integer_trail_->UpperBound(integer_variables_[index]));
    const double value = lp_solution_[index];
    if (value < lb - kCpEpsilon || value > ub + kCpEpsilon) return Propagate();
  }

  // TODO(user): The saved lp solution is still valid given the current variable
  // bounds, so the LP optimal didn't change. However we might still want to add
  // new cuts or new lazy constraints?
  //
  // TODO(user): Propagate the last optimal_constraint? Note that we need
  // to be careful since the reversible int in IntegerSumLE are not registered.
  // However, because we delete "optimalconstraints" on backtrack, we might not
  // care.
  return true;
}

glop::Fractional LinearProgrammingConstraint::GetVariableValueAtCpScale(
    glop::ColIndex var) {
  return scaler_.UnscaleVariableValue(var, simplex_.GetVariableValue(var));
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
    const double factor = scaler_.VariableScalingFactor(glop::ColIndex(i));
    lp_data_.SetVariableBounds(glop::ColIndex(i), lb * factor, ub * factor);
  }
}

bool LinearProgrammingConstraint::SolveLp() {
  if (trail_->CurrentDecisionLevel() == 0) {
    lp_at_level_zero_is_final_ = false;
  }

  const auto status = simplex_.Solve(lp_data_, time_limit_);
  total_num_simplex_iterations_ += simplex_.GetNumberOfIterations();
  if (!status.ok()) {
    VLOG(1) << "The LP solver encountered an error: " << status.error_message();
    simplex_.ClearStateForNextSolve();
    return false;
  }
  average_degeneracy_.AddData(CalculateDegeneracy());
  if (average_degeneracy_.CurrentAverage() >= 1000.0) {
    VLOG(2) << "High average degeneracy: "
            << average_degeneracy_.CurrentAverage();
  }

  const int status_as_int = static_cast<int>(simplex_.GetProblemStatus());
  if (status_as_int >= num_solves_by_status_.size()) {
    num_solves_by_status_.resize(status_as_int + 1);
  }
  num_solves_++;
  num_solves_by_status_[status_as_int]++;
  VLOG(2) << "lvl:" << trail_->CurrentDecisionLevel() << " "
          << simplex_.GetProblemStatus()
          << " iter:" << simplex_.GetNumberOfIterations()
          << " obj:" << simplex_.GetObjectiveValue();

  if (simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL) {
    lp_solution_is_set_ = true;
    lp_solution_level_ = trail_->CurrentDecisionLevel();
    const int num_vars = integer_variables_.size();
    for (int i = 0; i < num_vars; i++) {
      const glop::Fractional value =
          GetVariableValueAtCpScale(glop::ColIndex(i));
      lp_solution_[i] = value;
      expanded_lp_solution_[integer_variables_[i]] = value;
      expanded_lp_solution_[NegationOf(integer_variables_[i])] = -value;
    }

    if (lp_solution_level_ == 0) {
      level_zero_lp_solution_ = lp_solution_;
    }
  }
  return true;
}

bool LinearProgrammingConstraint::AddCutFromConstraints(
    const std::string& name,
    const std::vector<std::pair<RowIndex, IntegerValue>>& integer_multipliers) {
  // This is initialized to a valid linear constraint (by taking linear
  // combination of the LP rows) and will be transformed into a cut if
  // possible.
  //
  // TODO(user): For CG cuts, Ideally this linear combination should have only
  // one fractional variable (basis_col). But because of imprecision, we get a
  // bunch of fractional entry with small coefficient (relative to the one of
  // basis_col). We try to handle that in IntegerRoundingCut(), but it might be
  // better to add small multiple of the involved rows to get rid of them.
  IntegerValue cut_ub;
  if (!ComputeNewLinearConstraint(integer_multipliers, &tmp_scattered_vector_,
                                  &cut_ub)) {
    VLOG(1) << "Issue, overflow!";
    return false;
  }

  // Important: because we use integer_multipliers below, we cannot just
  // divide by GCD or call PreventOverflow() here.
  //
  // TODO(user): the conversion col_index -> IntegerVariable is slow and could
  // in principle be removed. Easy for cuts, but not so much for
  // implied_bounds_processor_. Note that in theory this could allow us to
  // use Literal directly without the need to have an IntegerVariable for them.
  tmp_scattered_vector_.ConvertToLinearConstraint(integer_variables_, cut_ub,
                                                  &cut_);

  // Note that the base constraint we use are currently always tight.
  // It is not a requirement though.
  if (DEBUG_MODE) {
    const double norm = ToDouble(ComputeInfinityNorm(cut_));
    const double activity = ComputeActivity(cut_, expanded_lp_solution_);
    if (std::abs(activity - ToDouble(cut_.ub)) / norm > 1e-4) {
      VLOG(1) << "Cut not tight " << activity << " <= " << ToDouble(cut_.ub);
      return false;
    }
  }
  CHECK(constraint_manager_.DebugCheckConstraint(cut_));

  // We will create "artificial" variables after this index that will be
  // substitued back into LP variables afterwards. Also not that we only use
  // positive variable indices for these new variables, so that algorithm that
  // take their negation will not mess up the indexing.
  const IntegerVariable first_new_var(expanded_lp_solution_.size());
  CHECK_EQ(first_new_var.value() % 2, 0);

  LinearConstraint copy_in_debug;
  if (DEBUG_MODE) {
    copy_in_debug = cut_;
  }

  // Unlike for the knapsack cuts, it might not be always beneficial to
  // process the implied bounds even though it seems to be better in average.
  //
  // TODO(user): Perform more experiments, in particular with which bound we use
  // and if we complement or not before the MIR rounding. Other solvers seems
  // to try different complementation strategies in a "potprocessing" and we
  // don't. Try this too.
  std::vector<ImpliedBoundsProcessor::SlackInfo> ib_slack_infos;
  implied_bounds_processor_.ProcessUpperBoundedConstraintWithSlackCreation(
      /*substitute_only_inner_variables=*/false, first_new_var,
      expanded_lp_solution_, &cut_, &ib_slack_infos);
  DCHECK(implied_bounds_processor_.DebugSlack(first_new_var, copy_in_debug,
                                              cut_, ib_slack_infos));

  // Fills data for IntegerRoundingCut().
  //
  // Note(user): we use the current bound here, so the reasonement will only
  // produce locally valid cut if we call this at a non-root node. We could
  // use the level zero bounds if we wanted to generate a globally valid cut
  // at another level. For now this is only called at level zero anyway.
  tmp_lp_values_.clear();
  tmp_var_lbs_.clear();
  tmp_var_ubs_.clear();
  for (const IntegerVariable var : cut_.vars) {
    if (var >= first_new_var) {
      CHECK(VariableIsPositive(var));
      const auto& info =
          ib_slack_infos[(var.value() - first_new_var.value()) / 2];
      tmp_lp_values_.push_back(info.lp_value);
      tmp_var_lbs_.push_back(info.lb);
      tmp_var_ubs_.push_back(info.ub);
    } else {
      tmp_lp_values_.push_back(expanded_lp_solution_[var]);
      tmp_var_lbs_.push_back(integer_trail_->LevelZeroLowerBound(var));
      tmp_var_ubs_.push_back(integer_trail_->LevelZeroUpperBound(var));
    }
  }

  // Add slack.
  // definition: integer_lp_[row] + slack_row == bound;
  const IntegerVariable first_slack(first_new_var +
                                    IntegerVariable(2 * ib_slack_infos.size()));
  tmp_slack_rows_.clear();
  tmp_slack_bounds_.clear();
  for (const auto pair : integer_multipliers) {
    const RowIndex row = pair.first;
    const IntegerValue coeff = pair.second;
    const auto status = simplex_.GetConstraintStatus(row);
    if (status == glop::ConstraintStatus::FIXED_VALUE) continue;

    tmp_lp_values_.push_back(0.0);
    cut_.vars.push_back(first_slack +
                        2 * IntegerVariable(tmp_slack_rows_.size()));
    tmp_slack_rows_.push_back(row);
    cut_.coeffs.push_back(coeff);

    const IntegerValue diff(
        CapSub(integer_lp_[row].ub.value(), integer_lp_[row].lb.value()));
    if (coeff > 0) {
      tmp_slack_bounds_.push_back(integer_lp_[row].ub);
      tmp_var_lbs_.push_back(IntegerValue(0));
      tmp_var_ubs_.push_back(diff);
    } else {
      tmp_slack_bounds_.push_back(integer_lp_[row].lb);
      tmp_var_lbs_.push_back(-diff);
      tmp_var_ubs_.push_back(IntegerValue(0));
    }
  }

  bool at_least_one_added = false;

  // Try cover approach to find cut.
  {
    if (cover_cut_helper_.TrySimpleKnapsack(cut_, tmp_lp_values_, tmp_var_lbs_,
                                            tmp_var_ubs_)) {
      at_least_one_added |= PostprocessAndAddCut(
          absl::StrCat(name, "_K"), cover_cut_helper_.Info(), first_new_var,
          first_slack, ib_slack_infos, cover_cut_helper_.mutable_cut());
    }
  }

  // Try integer rounding heuristic to find cut.
  {
    RoundingOptions options;
    options.max_scaling = parameters_.max_integer_rounding_scaling();
    integer_rounding_cut_helper_.ComputeCut(options, tmp_lp_values_,
                                            tmp_var_lbs_, tmp_var_ubs_,
                                            &implied_bounds_processor_, &cut_);
    at_least_one_added |= PostprocessAndAddCut(
        name,
        absl::StrCat("num_lifted_booleans=",
                     integer_rounding_cut_helper_.NumLiftedBooleans()),
        first_new_var, first_slack, ib_slack_infos, &cut_);
  }
  return at_least_one_added;
}

bool LinearProgrammingConstraint::PostprocessAndAddCut(
    const std::string& name, const std::string& info,
    IntegerVariable first_new_var, IntegerVariable first_slack,
    const std::vector<ImpliedBoundsProcessor::SlackInfo>& ib_slack_infos,
    LinearConstraint* cut) {
  // Compute the activity. Warning: the cut no longer have the same size so we
  // cannot use tmp_lp_values_. Note that the substitution below shouldn't
  // change the activity by definition.
  double activity = 0.0;
  for (int i = 0; i < cut->vars.size(); ++i) {
    if (cut->vars[i] < first_new_var) {
      activity +=
          ToDouble(cut->coeffs[i]) * expanded_lp_solution_[cut->vars[i]];
    }
  }
  const double kMinViolation = 1e-4;
  const double violation = activity - ToDouble(cut->ub);
  if (violation < kMinViolation) {
    VLOG(3) << "Bad cut " << activity << " <= " << ToDouble(cut->ub);
    return false;
  }

  // Substitute any slack left.
  {
    int num_slack = 0;
    tmp_scattered_vector_.ClearAndResize(integer_variables_.size());
    IntegerValue cut_ub = cut->ub;
    bool overflow = false;
    for (int i = 0; i < cut->vars.size(); ++i) {
      const IntegerVariable var = cut->vars[i];

      // Simple copy for non-slack variables.
      if (var < first_new_var) {
        const glop::ColIndex col =
            gtl::FindOrDie(mirror_lp_variable_, PositiveVariable(var));
        if (VariableIsPositive(var)) {
          tmp_scattered_vector_.Add(col, cut->coeffs[i]);
        } else {
          tmp_scattered_vector_.Add(col, -cut->coeffs[i]);
        }
        continue;
      }

      // Replace slack from bound substitution.
      if (var < first_slack) {
        const IntegerValue multiplier = cut->coeffs[i];
        const int index = (var.value() - first_new_var.value()) / 2;
        CHECK_LT(index, ib_slack_infos.size());

        std::vector<std::pair<ColIndex, IntegerValue>> terms;
        for (const std::pair<IntegerVariable, IntegerValue>& term :
             ib_slack_infos[index].terms) {
          terms.push_back(
              {gtl::FindOrDie(mirror_lp_variable_,
                              PositiveVariable(term.first)),
               VariableIsPositive(term.first) ? term.second : -term.second});
        }
        if (!tmp_scattered_vector_.AddLinearExpressionMultiple(multiplier,
                                                               terms)) {
          overflow = true;
          break;
        }
        if (!AddProductTo(multiplier, -ib_slack_infos[index].offset, &cut_ub)) {
          overflow = true;
          break;
        }
        continue;
      }

      // Replace slack from LP constraints.
      ++num_slack;
      const int slack_index = (var.value() - first_slack.value()) / 2;
      const glop::RowIndex row = tmp_slack_rows_[slack_index];
      const IntegerValue multiplier = -cut->coeffs[i];
      if (!tmp_scattered_vector_.AddLinearExpressionMultiple(
              multiplier, integer_lp_[row].terms)) {
        overflow = true;
        break;
      }

      // Update rhs.
      if (!AddProductTo(multiplier, tmp_slack_bounds_[slack_index], &cut_ub)) {
        overflow = true;
        break;
      }
    }

    if (overflow) {
      VLOG(1) << "Overflow in slack removal.";
      return false;
    }

    VLOG(3) << " num_slack: " << num_slack;
    tmp_scattered_vector_.ConvertToLinearConstraint(integer_variables_, cut_ub,
                                                    cut);
  }

  // Display some stats used for investigation of cut generation.
  const std::string extra_info =
      absl::StrCat(info, " num_ib_substitutions=", ib_slack_infos.size());

  const double new_violation =
      ComputeActivity(*cut, expanded_lp_solution_) - ToDouble(cut_.ub);
  if (std::abs(violation - new_violation) >= 1e-4) {
    VLOG(1) << "Violation discrepancy after slack removal. "
            << " before = " << violation << " after = " << new_violation;
  }

  DivideByGCD(cut);
  return constraint_manager_.AddCut(*cut, name, expanded_lp_solution_,
                                    extra_info);
}

// TODO(user): This can be still too slow on some problems like
// 30_70_45_05_100.mps.gz. Not this actual function, but the set of computation
// it triggers. We should add heuristics to abort earlier if a cut is not
// promising. Or only test a few positions and not all rows.
void LinearProgrammingConstraint::AddCGCuts() {
  const RowIndex num_rows = lp_data_.num_constraints();
  std::vector<std::pair<RowIndex, double>> lp_multipliers;
  std::vector<std::pair<RowIndex, IntegerValue>> integer_multipliers;
  for (RowIndex row(0); row < num_rows; ++row) {
    ColIndex basis_col = simplex_.GetBasis(row);
    const Fractional lp_value = GetVariableValueAtCpScale(basis_col);

    // Only consider fractional basis element. We ignore element that are close
    // to an integer to reduce the amount of positions we try.
    //
    // TODO(user): We could just look at the diff with std::floor() in the hope
    // that when we are just under an integer, the exact computation below will
    // also be just under it.
    if (std::abs(lp_value - std::round(lp_value)) < 0.01) continue;

    // If this variable is a slack, we ignore it. This is because the
    // corresponding row is not tight under the given lp values.
    if (basis_col >= integer_variables_.size()) continue;

    if (time_limit_->LimitReached()) break;

    // TODO(user): Avoid code duplication between the sparse/dense path.
    double magnitude = 0.0;
    lp_multipliers.clear();
    const glop::ScatteredRow& lambda = simplex_.GetUnitRowLeftInverse(row);
    if (lambda.non_zeros.empty()) {
      for (RowIndex row(0); row < num_rows; ++row) {
        const double value = lambda.values[glop::RowToColIndex(row)];
        if (std::abs(value) < kZeroTolerance) continue;

        // There should be no BASIC status, but they could be imprecision
        // in the GetUnitRowLeftInverse() code? not sure, so better be safe.
        const auto status = simplex_.GetConstraintStatus(row);
        if (status == glop::ConstraintStatus::BASIC) {
          VLOG(1) << "BASIC row not expected! " << value;
          continue;
        }

        magnitude = std::max(magnitude, std::abs(value));
        lp_multipliers.push_back({row, value});
      }
    } else {
      for (const ColIndex col : lambda.non_zeros) {
        const RowIndex row = glop::ColToRowIndex(col);
        const double value = lambda.values[col];
        if (std::abs(value) < kZeroTolerance) continue;

        const auto status = simplex_.GetConstraintStatus(row);
        if (status == glop::ConstraintStatus::BASIC) {
          VLOG(1) << "BASIC row not expected! " << value;
          continue;
        }

        magnitude = std::max(magnitude, std::abs(value));
        lp_multipliers.push_back({row, value});
      }
    }
    if (lp_multipliers.empty()) continue;

    Fractional scaling;
    for (int i = 0; i < 2; ++i) {
      if (i == 1) {
        // Try other sign.
        //
        // TODO(user): Maybe add an heuristic to know beforehand which sign to
        // use?
        for (std::pair<RowIndex, double>& p : lp_multipliers) {
          p.second = -p.second;
        }
      }

      // TODO(user): We use a lower value here otherwise we might run into
      // overflow while computing the cut. This should be fixable.
      integer_multipliers =
          ScaleLpMultiplier(/*take_objective_into_account=*/false,
                            lp_multipliers, &scaling, /*max_pow=*/52);
      AddCutFromConstraints("CG", integer_multipliers);
    }
  }
}

namespace {

// For each element of a, adds a random one in b and append the pair to output.
void RandomPick(const std::vector<RowIndex>& a, const std::vector<RowIndex>& b,
                ModelRandomGenerator* random,
                std::vector<std::pair<RowIndex, RowIndex>>* output) {
  if (a.empty() || b.empty()) return;
  for (const RowIndex row : a) {
    const RowIndex other = b[absl::Uniform<int>(*random, 0, b.size())];
    if (other != row) {
      output->push_back({row, other});
    }
  }
}

template <class ListOfTerms>
IntegerValue GetCoeff(ColIndex col, const ListOfTerms& terms) {
  for (const auto& term : terms) {
    if (term.first == col) return term.second;
  }
  return IntegerValue(0);
}

}  // namespace

// Because we know the objective is integer, the constraint objective >= lb can
// sometime cut the current lp optimal, and it can make a big difference to add
// it. Or at least use it when constructing more advanced cuts. See
// 'multisetcover_batch_0_case_115_instance_0_small_subset_elements_3_sumreqs
//  _1295_candidates_41.fzn'
//
// TODO(user): It might be better to just integrate this with the MIR code so
// that we not only consider MIR1 involving the objective but we also consider
// combining it with other constraints.
void LinearProgrammingConstraint::AddObjectiveCut() {
  if (integer_objective_.size() <= 1) return;

  // Clear temp data.
  tmp_lp_values_.clear();
  tmp_var_lbs_.clear();
  tmp_var_ubs_.clear();
  cut_.Clear();

  // We negate everything to have a <= base constraint.
  cut_.lb = kMinIntegerValue;
  cut_.ub = integer_objective_offset_ -
            integer_trail_->LevelZeroLowerBound(objective_cp_);
  for (const auto& [col, coeff] : integer_objective_) {
    const IntegerVariable var = integer_variables_[col.value()];
    cut_.vars.push_back(var);
    tmp_lp_values_.push_back(expanded_lp_solution_[var]);
    tmp_var_lbs_.push_back(integer_trail_->LevelZeroLowerBound(var));
    tmp_var_ubs_.push_back(integer_trail_->LevelZeroUpperBound(var));
    cut_.coeffs.push_back(-coeff);
  }

  // Because the objective has often large coefficient, we always try a MIR1
  // like heuristic to round it to reasonable values.
  RoundingOptions options;
  options.max_scaling = parameters_.max_integer_rounding_scaling();
  integer_rounding_cut_helper_.ComputeCut(options, tmp_lp_values_, tmp_var_lbs_,
                                          tmp_var_ubs_,
                                          &implied_bounds_processor_, &cut_);

  // Note that the cut will not be added if it is not good enough.
  constraint_manager_.AddCut(cut_, "Objective", expanded_lp_solution_);
}

void LinearProgrammingConstraint::AddMirCuts() {
  // Heuristic to generate MIR_n cuts by combining a small number of rows. This
  // works greedily and follow more or less the MIR cut description in the
  // literature. We have a current cut, and we add one more row to it while
  // eliminating a variable of the current cut whose LP value is far from its
  // bound.
  //
  // A notable difference is that we randomize the variable we eliminate and
  // the row we use to do so. We still have weights to indicate our preferred
  // choices. This allows to generate different cuts when called again and
  // again.
  //
  // TODO(user): We could combine n rows to make sure we eliminate n variables
  // far away from their bounds by solving exactly in integer small linear
  // system.
  absl::StrongVector<ColIndex, IntegerValue> dense_cut(
      integer_variables_.size(), IntegerValue(0));
  SparseBitset<ColIndex> non_zeros_(ColIndex(integer_variables_.size()));

  // We compute all the rows that are tight, these will be used as the base row
  // for the MIR_n procedure below.
  const RowIndex num_rows = lp_data_.num_constraints();
  std::vector<std::pair<RowIndex, IntegerValue>> base_rows;
  absl::StrongVector<RowIndex, double> row_weights(num_rows.value(), 0.0);
  for (RowIndex row(0); row < num_rows; ++row) {
    const auto status = simplex_.GetConstraintStatus(row);
    if (status == glop::ConstraintStatus::BASIC) continue;
    if (status == glop::ConstraintStatus::FREE) continue;

    if (status == glop::ConstraintStatus::AT_UPPER_BOUND ||
        status == glop::ConstraintStatus::FIXED_VALUE) {
      base_rows.push_back({row, IntegerValue(1)});
    }
    if (status == glop::ConstraintStatus::AT_LOWER_BOUND ||
        status == glop::ConstraintStatus::FIXED_VALUE) {
      base_rows.push_back({row, IntegerValue(-1)});
    }

    // For now, we use the dual values for the row "weights".
    //
    // Note that we use the dual at LP scale so that it make more sense when we
    // compare different rows since the LP has been scaled.
    //
    // TODO(user): In Kati Wolter PhD "Implementation of Cutting Plane
    // Separators for Mixed Integer Programs" which describe SCIP's MIR cuts
    // implementation (or at least an early version of it), a more complex score
    // is used.
    //
    // Note(user): Because we only consider tight rows under the current lp
    // solution (i.e. non-basic rows), most should have a non-zero dual values.
    // But there is some degenerate problem where these rows have a really low
    // weight (or even zero), and having only weight of exactly zero in
    // std::discrete_distribution will result in a crash.
    row_weights[row] = std::max(1e-8, std::abs(simplex_.GetDualValue(row)));
  }

  std::vector<double> weights;
  absl::StrongVector<RowIndex, bool> used_rows;
  std::vector<std::pair<RowIndex, IntegerValue>> integer_multipliers;
  for (const std::pair<RowIndex, IntegerValue>& entry : base_rows) {
    if (time_limit_->LimitReached()) break;

    // First try to generate a cut directly from this base row (MIR1).
    //
    // Note(user): We abort on success like it seems to be done in the
    // literature. Note that we don't succeed that often in generating an
    // efficient cut, so I am not sure aborting will make a big difference
    // speedwise. We might generate similar cuts though, but hopefully the cut
    // management can deal with that.
    integer_multipliers = {entry};
    if (AddCutFromConstraints("MIR_1", integer_multipliers)) {
      continue;
    }

    // Cleanup.
    for (const ColIndex col : non_zeros_.PositionsSetAtLeastOnce()) {
      dense_cut[col] = IntegerValue(0);
    }
    non_zeros_.SparseClearAll();

    // Copy cut.
    const IntegerValue multiplier = entry.second;
    for (const std::pair<ColIndex, IntegerValue> term :
         integer_lp_[entry.first].terms) {
      const ColIndex col = term.first;
      const IntegerValue coeff = term.second;
      non_zeros_.Set(col);
      dense_cut[col] += coeff * multiplier;
    }

    used_rows.assign(num_rows.value(), false);
    used_rows[entry.first] = true;

    // We will aggregate at most kMaxAggregation more rows.
    //
    // TODO(user): optim + tune.
    const int kMaxAggregation = 5;
    for (int i = 0; i < kMaxAggregation; ++i) {
      // First pick a variable to eliminate. We currently pick a random one with
      // a weight that depend on how far it is from its closest bound.
      IntegerValue max_magnitude(0);
      weights.clear();
      std::vector<ColIndex> col_candidates;
      for (const ColIndex col : non_zeros_.PositionsSetAtLeastOnce()) {
        if (dense_cut[col] == 0) continue;

        max_magnitude = std::max(max_magnitude, IntTypeAbs(dense_cut[col]));
        const int col_degree =
            lp_data_.GetSparseColumn(col).num_entries().value();
        if (col_degree <= 1) continue;
        if (simplex_.GetVariableStatus(col) != glop::VariableStatus::BASIC) {
          continue;
        }

        const IntegerVariable var = integer_variables_[col.value()];
        const double lp_value = expanded_lp_solution_[var];
        const double lb = ToDouble(integer_trail_->LevelZeroLowerBound(var));
        const double ub = ToDouble(integer_trail_->LevelZeroUpperBound(var));
        const double bound_distance = std::min(ub - lp_value, lp_value - lb);
        if (bound_distance > 1e-2) {
          weights.push_back(bound_distance);
          col_candidates.push_back(col);
        }
      }
      if (col_candidates.empty()) break;

      const ColIndex var_to_eliminate =
          col_candidates[std::discrete_distribution<>(weights.begin(),
                                                      weights.end())(*random_)];

      // What rows can we add to eliminate var_to_eliminate?
      std::vector<RowIndex> possible_rows;
      weights.clear();
      for (const auto entry : lp_data_.GetSparseColumn(var_to_eliminate)) {
        const RowIndex row = entry.row();
        const auto status = simplex_.GetConstraintStatus(row);
        if (status == glop::ConstraintStatus::BASIC) continue;
        if (status == glop::ConstraintStatus::FREE) continue;

        // We disallow all the rows that contain a variable that we already
        // eliminated (or are about to). This mean that we choose rows that
        // form a "triangular" matrix on the position we choose to eliminate.
        if (used_rows[row]) continue;
        used_rows[row] = true;

        // TODO(user): Instead of using FIXED_VALUE consider also both direction
        // when we almost have an equality? that is if the LP constraints bounds
        // are close from each others (<1e-6 ?). Initial experiments shows it
        // doesn't change much, so I kept this version for now. Note that it
        // might just be better to use the side that constrain the current lp
        // optimal solution (that we get from the status).
        bool add_row = false;
        if (status == glop::ConstraintStatus::FIXED_VALUE ||
            status == glop::ConstraintStatus::AT_UPPER_BOUND) {
          if (entry.coefficient() > 0.0) {
            if (dense_cut[var_to_eliminate] < 0) add_row = true;
          } else {
            if (dense_cut[var_to_eliminate] > 0) add_row = true;
          }
        }
        if (status == glop::ConstraintStatus::FIXED_VALUE ||
            status == glop::ConstraintStatus::AT_LOWER_BOUND) {
          if (entry.coefficient() > 0.0) {
            if (dense_cut[var_to_eliminate] > 0) add_row = true;
          } else {
            if (dense_cut[var_to_eliminate] < 0) add_row = true;
          }
        }
        if (add_row) {
          possible_rows.push_back(row);
          weights.push_back(row_weights[row]);
        }
      }
      if (possible_rows.empty()) break;

      const RowIndex row_to_combine =
          possible_rows[std::discrete_distribution<>(weights.begin(),
                                                     weights.end())(*random_)];
      const IntegerValue to_combine_coeff =
          GetCoeff(var_to_eliminate, integer_lp_[row_to_combine].terms);
      CHECK_NE(to_combine_coeff, 0);

      IntegerValue mult1 = -to_combine_coeff;
      IntegerValue mult2 = dense_cut[var_to_eliminate];
      CHECK_NE(mult2, 0);
      if (mult1 < 0) {
        mult1 = -mult1;
        mult2 = -mult2;
      }

      const IntegerValue gcd = IntegerValue(
          MathUtil::GCD64(std::abs(mult1.value()), std::abs(mult2.value())));
      CHECK_NE(gcd, 0);
      mult1 /= gcd;
      mult2 /= gcd;

      // Overflow detection.
      //
      // TODO(user): do that in the possible_rows selection? only problem is
      // that we do not have the integer coefficient there...
      for (std::pair<RowIndex, IntegerValue>& entry : integer_multipliers) {
        max_magnitude = std::max(max_magnitude, IntTypeAbs(entry.second));
      }
      if (CapAdd(CapProd(max_magnitude.value(), std::abs(mult1.value())),
                 CapProd(infinity_norms_[row_to_combine].value(),
                         std::abs(mult2.value()))) ==
          std::numeric_limits<int64_t>::max()) {
        break;
      }

      for (std::pair<RowIndex, IntegerValue>& entry : integer_multipliers) {
        entry.second *= mult1;
      }
      integer_multipliers.push_back({row_to_combine, mult2});

      // TODO(user): Not supper efficient to recombine the rows.
      if (AddCutFromConstraints(absl::StrCat("MIR_", i + 2),
                                integer_multipliers)) {
        break;
      }

      // Minor optim: the computation below is only needed if we do one more
      // iteration.
      if (i + 1 == kMaxAggregation) break;

      for (ColIndex col : non_zeros_.PositionsSetAtLeastOnce()) {
        dense_cut[col] *= mult1;
      }
      for (const std::pair<ColIndex, IntegerValue> term :
           integer_lp_[row_to_combine].terms) {
        const ColIndex col = term.first;
        const IntegerValue coeff = term.second;
        non_zeros_.Set(col);
        dense_cut[col] += coeff * mult2;
      }
    }
  }
}

void LinearProgrammingConstraint::AddZeroHalfCuts() {
  if (time_limit_->LimitReached()) return;

  tmp_lp_values_.clear();
  tmp_var_lbs_.clear();
  tmp_var_ubs_.clear();
  for (const IntegerVariable var : integer_variables_) {
    tmp_lp_values_.push_back(expanded_lp_solution_[var]);
    tmp_var_lbs_.push_back(integer_trail_->LevelZeroLowerBound(var));
    tmp_var_ubs_.push_back(integer_trail_->LevelZeroUpperBound(var));
  }

  // TODO(user): See if it make sense to try to use implied bounds there.
  zero_half_cut_helper_.ProcessVariables(tmp_lp_values_, tmp_var_lbs_,
                                         tmp_var_ubs_);
  for (glop::RowIndex row(0); row < integer_lp_.size(); ++row) {
    // Even though we could use non-tight row, for now we prefer to use tight
    // ones.
    const auto status = simplex_.GetConstraintStatus(row);
    if (status == glop::ConstraintStatus::BASIC) continue;
    if (status == glop::ConstraintStatus::FREE) continue;

    zero_half_cut_helper_.AddOneConstraint(
        row, integer_lp_[row].terms, integer_lp_[row].lb, integer_lp_[row].ub);
  }
  for (const std::vector<std::pair<RowIndex, IntegerValue>>& multipliers :
       zero_half_cut_helper_.InterestingCandidates(random_)) {
    if (time_limit_->LimitReached()) break;

    // TODO(user): Make sure that if the resulting linear coefficients are not
    // too high, we do try a "divisor" of two and thus try a true zero-half cut
    // instead of just using our best MIR heuristic (which might still be better
    // though).
    AddCutFromConstraints("ZERO_HALF", multipliers);
  }
}

void LinearProgrammingConstraint::UpdateSimplexIterationLimit(
    const int64_t min_iter, const int64_t max_iter) {
  if (parameters_.linearization_level() < 2) return;
  const int64_t num_degenerate_columns = CalculateDegeneracy();
  const int64_t num_cols = simplex_.GetProblemNumCols().value();
  if (num_cols <= 0) {
    return;
  }
  CHECK_GT(num_cols, 0);
  const int64_t decrease_factor = (10 * num_degenerate_columns) / num_cols;
  if (simplex_.GetProblemStatus() == glop::ProblemStatus::DUAL_FEASIBLE) {
    // We reached here probably because we predicted wrong. We use this as a
    // signal to increase the iterations or punish less for degeneracy compare
    // to the other part.
    if (is_degenerate_) {
      next_simplex_iter_ /= std::max(int64_t{1}, decrease_factor);
    } else {
      next_simplex_iter_ *= 2;
    }
  } else if (simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL) {
    if (is_degenerate_) {
      next_simplex_iter_ /= std::max(int64_t{1}, 2 * decrease_factor);
    } else {
      // This is the most common case. We use the size of the problem to
      // determine the limit and ignore the previous limit.
      next_simplex_iter_ = num_cols / 40;
    }
  }
  next_simplex_iter_ =
      std::max(min_iter, std::min(max_iter, next_simplex_iter_));
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
  if (trail_->CurrentDecisionLevel() == 0) {
    // TODO(user): Dynamically change the iteration limit for root node as
    // well.
    parameters.set_max_number_of_iterations(2000);
  } else {
    parameters.set_max_number_of_iterations(next_simplex_iter_);
  }
  if (parameters_.use_exact_lp_reason()) {
    parameters.set_change_status_to_imprecise(false);
    parameters.set_primal_feasibility_tolerance(1e-7);
    parameters.set_dual_feasibility_tolerance(1e-7);
  }

  simplex_.SetParameters(parameters);
  simplex_.NotifyThatMatrixIsUnchangedForNextSolve();
  if (!SolveLp()) return true;

  // Add new constraints to the LP and resolve?
  const int max_cuts_rounds =
      parameters_.cut_level() <= 0
          ? 0
          : (trail_->CurrentDecisionLevel() == 0
                 ? parameters_.max_cut_rounds_at_level_zero()
                 : 1);
  int cuts_round = 0;
  while (simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL &&
         cuts_round < max_cuts_rounds) {
    // We wait for the first batch of problem constraints to be added before we
    // begin to generate cuts. Note that we rely on num_solves_ since on some
    // problems there is no other constriants than the cuts.
    cuts_round++;
    if (num_solves_ > 1) {
      // This must be called first.
      implied_bounds_processor_.RecomputeCacheAndSeparateSomeImpliedBoundCuts(
          expanded_lp_solution_);

      // The "generic" cuts are currently part of this class as they are using
      // data from the current LP.
      //
      // TODO(user): Refactor so that they are just normal cut generators?
      if (trail_->CurrentDecisionLevel() == 0) {
        if (parameters_.add_objective_cut()) AddObjectiveCut();
        if (parameters_.add_mir_cuts()) AddMirCuts();
        if (parameters_.add_cg_cuts()) AddCGCuts();
        if (parameters_.add_zero_half_cuts()) AddZeroHalfCuts();
      }

      // Try to add cuts.
      if (!cut_generators_.empty() &&
          (trail_->CurrentDecisionLevel() == 0 ||
           !parameters_.only_add_cuts_at_level_zero())) {
        for (const CutGenerator& generator : cut_generators_) {
          if (!generator.generate_cuts(expanded_lp_solution_,
                                       &constraint_manager_)) {
            return false;
          }
        }
      }

      implied_bounds_processor_.IbCutPool().TransferToManager(
          expanded_lp_solution_, &constraint_manager_);
    }

    glop::BasisState state = simplex_.GetState();
    if (constraint_manager_.ChangeLp(expanded_lp_solution_, &state)) {
      simplex_.LoadStateForNextSolve(state);
      if (!CreateLpFromConstraintManager()) {
        return integer_trail_->ReportConflict({});
      }
      const double old_obj = simplex_.GetObjectiveValue();
      if (!SolveLp()) return true;
      if (simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL) {
        VLOG(1) << "Relaxation improvement " << old_obj << " -> "
                << simplex_.GetObjectiveValue()
                << " diff: " << simplex_.GetObjectiveValue() - old_obj
                << " level: " << trail_->CurrentDecisionLevel();
      }
    } else {
      if (trail_->CurrentDecisionLevel() == 0) {
        lp_at_level_zero_is_final_ = true;
      }
      break;
    }
  }

  // A dual-unbounded problem is infeasible. We use the dual ray reason.
  if (simplex_.GetProblemStatus() == glop::ProblemStatus::DUAL_UNBOUNDED) {
    if (parameters_.use_exact_lp_reason()) {
      if (!FillExactDualRayReason()) return true;
    } else {
      FillReducedCostReasonIn(simplex_.GetDualRayRowCombination(),
                              &integer_reason_);
    }
    return integer_trail_->ReportConflict(integer_reason_);
  }

  // TODO(user): Update limits for DUAL_UNBOUNDED status as well.
  UpdateSimplexIterationLimit(/*min_iter=*/10, /*max_iter=*/1000);

  // Optimality deductions if problem has an objective.
  if (objective_is_defined_ &&
      (simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL ||
       simplex_.GetProblemStatus() == glop::ProblemStatus::DUAL_FEASIBLE)) {
    // TODO(user): Maybe do a bit less computation when we cannot propagate
    // anything.
    if (parameters_.use_exact_lp_reason()) {
      if (!ExactLpReasonning()) return false;

      // Display when the inexact bound would have propagated more.
      if (VLOG_IS_ON(2)) {
        const double relaxed_optimal_objective = simplex_.GetObjectiveValue();
        const IntegerValue approximate_new_lb(static_cast<int64_t>(
            std::ceil(relaxed_optimal_objective - kCpEpsilon)));
        const IntegerValue propagated_lb =
            integer_trail_->LowerBound(objective_cp_);
        if (approximate_new_lb > propagated_lb) {
          VLOG(2) << "LP objective [ " << ToDouble(propagated_lb) << ", "
                  << ToDouble(integer_trail_->UpperBound(objective_cp_))
                  << " ] approx_lb += "
                  << ToDouble(approximate_new_lb - propagated_lb) << " gap: "
                  << integer_trail_->UpperBound(objective_cp_) - propagated_lb;
        }
      }
    } else {
      // Try to filter optimal objective value. Note that GetObjectiveValue()
      // already take care of the scaling so that it returns an objective in the
      // CP world.
      FillReducedCostReasonIn(simplex_.GetReducedCosts(), &integer_reason_);
      const double objective_cp_ub =
          ToDouble(integer_trail_->UpperBound(objective_cp_));
      const double relaxed_optimal_objective = simplex_.GetObjectiveValue();
      ReducedCostStrengtheningDeductions(objective_cp_ub -
                                         relaxed_optimal_objective);
      if (!deductions_.empty()) {
        deductions_reason_ = integer_reason_;
        deductions_reason_.push_back(
            integer_trail_->UpperBoundAsLiteral(objective_cp_));
      }

      // Push new objective lb.
      const IntegerValue approximate_new_lb(static_cast<int64_t>(
          std::ceil(relaxed_optimal_objective - kCpEpsilon)));
      if (approximate_new_lb > integer_trail_->LowerBound(objective_cp_)) {
        const IntegerLiteral deduction =
            IntegerLiteral::GreaterOrEqual(objective_cp_, approximate_new_lb);
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
  }

  // Copy more info about the current solution.
  if (simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL) {
    CHECK(lp_solution_is_set_);

    lp_objective_ = simplex_.GetObjectiveValue();
    lp_solution_is_integer_ = true;
    const int num_vars = integer_variables_.size();
    for (int i = 0; i < num_vars; i++) {
      lp_reduced_cost_[i] = scaler_.UnscaleReducedCost(
          glop::ColIndex(i), simplex_.GetReducedCost(glop::ColIndex(i)));
      if (std::abs(lp_solution_[i] - std::round(lp_solution_[i])) >
          kCpEpsilon) {
        lp_solution_is_integer_ = false;
      }
    }

    if (compute_reduced_cost_averages_) {
      UpdateAverageReducedCosts();
    }
  }

  if (parameters_.use_branching_in_lp() && objective_is_defined_ &&
      trail_->CurrentDecisionLevel() == 0 && !is_degenerate_ &&
      lp_solution_is_set_ && !lp_solution_is_integer_ &&
      parameters_.linearization_level() >= 2 &&
      compute_reduced_cost_averages_ &&
      simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL) {
    count_since_last_branching_++;
    if (count_since_last_branching_ < branching_frequency_) {
      return true;
    }
    count_since_last_branching_ = 0;
    bool branching_successful = false;

    // Strong branching on top max_num_branches variable.
    const int max_num_branches = 3;
    const int num_vars = integer_variables_.size();
    std::vector<std::pair<double, IntegerVariable>> branching_vars;
    for (int i = 0; i < num_vars; ++i) {
      const IntegerVariable var = integer_variables_[i];
      const IntegerVariable positive_var = PositiveVariable(var);

      // Skip non fractional variables.
      const double current_value = GetSolutionValue(positive_var);
      if (std::abs(current_value - std::round(current_value)) <= kCpEpsilon) {
        continue;
      }

      // Skip ignored variables.
      if (integer_trail_->IsCurrentlyIgnored(var)) continue;

      // We can use any metric to select a variable to branch on. Reduced cost
      // average is one of the most promissing metric. It captures the history
      // of the objective bound improvement in LP due to changes in the given
      // variable bounds.
      //
      // NOTE: We also experimented using PseudoCosts and most recent reduced
      // cost as metrics but it doesn't give better results on benchmarks.
      const double cost_i = rc_scores_[i];
      std::pair<double, IntegerVariable> branching_var =
          std::make_pair(-cost_i, positive_var);
      auto iterator = std::lower_bound(branching_vars.begin(),
                                       branching_vars.end(), branching_var);

      branching_vars.insert(iterator, branching_var);
      if (branching_vars.size() > max_num_branches) {
        branching_vars.resize(max_num_branches);
      }
    }

    for (const std::pair<double, IntegerVariable>& branching_var :
         branching_vars) {
      const IntegerVariable positive_var = branching_var.second;
      VLOG(2) << "Branching on: " << positive_var;
      if (BranchOnVar(positive_var)) {
        VLOG(2) << "Branching successful.";
        branching_successful = true;
      } else {
        break;
      }
    }
    if (!branching_successful) {
      branching_frequency_ *= 2;
    }
  }
  return true;
}

// Returns kMinIntegerValue in case of overflow.
//
// TODO(user): Because of PreventOverflow(), this should actually never happen.
IntegerValue LinearProgrammingConstraint::GetImpliedLowerBound(
    const LinearConstraint& terms) const {
  IntegerValue lower_bound(0);
  const int size = terms.vars.size();
  for (int i = 0; i < size; ++i) {
    const IntegerVariable var = terms.vars[i];
    const IntegerValue coeff = terms.coeffs[i];
    CHECK_NE(coeff, 0);
    const IntegerValue bound = coeff > 0 ? integer_trail_->LowerBound(var)
                                         : integer_trail_->UpperBound(var);
    if (!AddProductTo(bound, coeff, &lower_bound)) return kMinIntegerValue;
  }
  return lower_bound;
}

bool LinearProgrammingConstraint::PossibleOverflow(
    const LinearConstraint& constraint) {
  IntegerValue lower_bound(0);
  const int size = constraint.vars.size();
  for (int i = 0; i < size; ++i) {
    const IntegerVariable var = constraint.vars[i];
    const IntegerValue coeff = constraint.coeffs[i];
    CHECK_NE(coeff, 0);
    const IntegerValue bound = coeff > 0
                                   ? integer_trail_->LevelZeroLowerBound(var)
                                   : integer_trail_->LevelZeroUpperBound(var);
    if (!AddProductTo(bound, coeff, &lower_bound)) {
      return true;
    }
  }
  const int64_t slack = CapAdd(lower_bound.value(), -constraint.ub.value());
  if (slack == std::numeric_limits<int64_t>::min() ||
      slack == std::numeric_limits<int64_t>::max()) {
    return true;
  }
  return false;
}

namespace {

absl::int128 FloorRatio128(absl::int128 x, IntegerValue positive_div) {
  absl::int128 div128(positive_div.value());
  absl::int128 result = x / div128;
  if (result * div128 > x) return result - 1;
  return result;
}

}  // namespace

void LinearProgrammingConstraint::PreventOverflow(LinearConstraint* constraint,
                                                  int max_pow) {
  // First, make all coefficient positive.
  MakeAllCoefficientsPositive(constraint);

  // Compute the min/max possible partial sum. Note that we need to use the
  // level zero bounds here since we might use this cut after backtrack.
  double sum_min = std::min(0.0, ToDouble(-constraint->ub));
  double sum_max = std::max(0.0, ToDouble(-constraint->ub));
  const int size = constraint->vars.size();
  for (int i = 0; i < size; ++i) {
    const IntegerVariable var = constraint->vars[i];
    const double coeff = ToDouble(constraint->coeffs[i]);
    sum_min +=
        coeff *
        std::min(0.0, ToDouble(integer_trail_->LevelZeroLowerBound(var)));
    sum_max +=
        coeff *
        std::max(0.0, ToDouble(integer_trail_->LevelZeroUpperBound(var)));
  }
  const double max_value = std::max({sum_max, -sum_min, sum_max - sum_min});

  const IntegerValue divisor(std::ceil(std::ldexp(max_value, -max_pow)));
  if (divisor <= 1) return;

  // To be correct, we need to shift all variable so that they are positive.
  //
  // Important: One might be tempted to think that using the current variable
  // bounds is okay here since we only use this to derive cut/constraint that
  // only needs to be locally valid. However, in some corner cases (like when
  // one term become zero), we might loose the fact that we used one of the
  // variable bound to derive the new constraint, so we will miss it in the
  // explanation !!
  //
  // TODO(user): This code is tricky and similar to the one to generate cuts.
  // Test and may reduce the duplication? note however that here we use int128
  // to deal with potential overflow.
  int new_size = 0;
  absl::int128 adjust = 0;
  for (int i = 0; i < size; ++i) {
    const IntegerValue old_coeff = constraint->coeffs[i];
    const IntegerValue new_coeff = FloorRatio(old_coeff, divisor);

    // Compute the rhs adjustement.
    const absl::int128 remainder =
        absl::int128(old_coeff.value()) -
        absl::int128(new_coeff.value()) * absl::int128(divisor.value());
    adjust +=
        remainder *
        absl::int128(
            integer_trail_->LevelZeroLowerBound(constraint->vars[i]).value());

    if (new_coeff == 0) continue;
    constraint->vars[new_size] = constraint->vars[i];
    constraint->coeffs[new_size] = new_coeff;
    ++new_size;
  }
  constraint->vars.resize(new_size);
  constraint->coeffs.resize(new_size);

  constraint->ub = IntegerValue(static_cast<int64_t>(
      FloorRatio128(absl::int128(constraint->ub.value()) - adjust, divisor)));
}

// TODO(user): combine this with RelaxLinearReason() to avoid the extra
// magnitude vector and the weird precondition of RelaxLinearReason().
void LinearProgrammingConstraint::SetImpliedLowerBoundReason(
    const LinearConstraint& terms, IntegerValue slack) {
  integer_reason_.clear();
  std::vector<IntegerValue> magnitudes;
  const int size = terms.vars.size();
  for (int i = 0; i < size; ++i) {
    const IntegerVariable var = terms.vars[i];
    const IntegerValue coeff = terms.coeffs[i];
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

std::vector<std::pair<RowIndex, IntegerValue>>
LinearProgrammingConstraint::ScaleLpMultiplier(
    bool take_objective_into_account,
    const std::vector<std::pair<RowIndex, double>>& lp_multipliers,
    Fractional* scaling, int max_pow) const {
  double max_sum = 0.0;
  tmp_cp_multipliers_.clear();
  for (const std::pair<RowIndex, double>& p : lp_multipliers) {
    const RowIndex row = p.first;
    const Fractional lp_multi = p.second;

    // We ignore small values since these are likely errors and will not
    // contribute much to the new lp constraint anyway.
    if (std::abs(lp_multi) < kZeroTolerance) continue;

    // Remove trivial bad cases.
    //
    // TODO(user): It might be better (when possible) to use the OPTIMAL row
    // status since in most situation we do want the constraint we add to be
    // tight under the current LP solution. Only for infeasible problem we might
    // not have access to the status.
    if (lp_multi > 0.0 && integer_lp_[row].ub >= kMaxIntegerValue) {
      continue;
    }
    if (lp_multi < 0.0 && integer_lp_[row].lb <= kMinIntegerValue) {
      continue;
    }

    const Fractional cp_multi = scaler_.UnscaleDualValue(row, lp_multi);
    tmp_cp_multipliers_.push_back({row, cp_multi});
    max_sum += ToDouble(infinity_norms_[row]) * std::abs(cp_multi);
  }

  // This behave exactly like if we had another "objective" constraint with
  // an lp_multi of 1.0 and a cp_multi of 1.0.
  if (take_objective_into_account) {
    max_sum += ToDouble(objective_infinity_norm_);
  }

  *scaling = 1.0;
  std::vector<std::pair<RowIndex, IntegerValue>> integer_multipliers;
  if (max_sum == 0.0) {
    // Empty linear combinaison.
    return integer_multipliers;
  }

  // We want max_sum * scaling to be <= 2 ^ max_pow and fit on an int64_t.
  // We use a power of 2 as this seems to work better.
  const double threshold = std::ldexp(1, max_pow) / max_sum;
  if (threshold < 1.0) {
    // TODO(user): we currently do not support scaling down, so we just abort
    // in this case.
    return integer_multipliers;
  }
  while (2 * *scaling <= threshold) *scaling *= 2;

  // Scale the multipliers by *scaling.
  //
  // TODO(user): Maybe use int128 to avoid overflow?
  for (const auto entry : tmp_cp_multipliers_) {
    const IntegerValue coeff(std::round(entry.second * (*scaling)));
    if (coeff != 0) integer_multipliers.push_back({entry.first, coeff});
  }
  return integer_multipliers;
}

bool LinearProgrammingConstraint::ComputeNewLinearConstraint(
    const std::vector<std::pair<RowIndex, IntegerValue>>& integer_multipliers,
    ScatteredIntegerVector* scattered_vector, IntegerValue* upper_bound) const {
  // Initialize the new constraint.
  *upper_bound = 0;
  scattered_vector->ClearAndResize(integer_variables_.size());

  // Compute the new constraint by taking the linear combination given by
  // integer_multipliers of the integer constraints in integer_lp_.
  for (const std::pair<RowIndex, IntegerValue> term : integer_multipliers) {
    const RowIndex row = term.first;
    const IntegerValue multiplier = term.second;
    CHECK_LT(row, integer_lp_.size());

    // Update the constraint.
    if (!scattered_vector->AddLinearExpressionMultiple(
            multiplier, integer_lp_[row].terms)) {
      return false;
    }

    // Update the upper bound.
    const IntegerValue bound =
        multiplier > 0 ? integer_lp_[row].ub : integer_lp_[row].lb;
    if (!AddProductTo(multiplier, bound, upper_bound)) return false;
  }

  return true;
}

// TODO(user): no need to update the multipliers.
void LinearProgrammingConstraint::AdjustNewLinearConstraint(
    std::vector<std::pair<glop::RowIndex, IntegerValue>>* integer_multipliers,
    ScatteredIntegerVector* scattered_vector, IntegerValue* upper_bound) const {
  const IntegerValue kMaxWantedCoeff(1e18);
  for (std::pair<RowIndex, IntegerValue>& term : *integer_multipliers) {
    const RowIndex row = term.first;
    const IntegerValue multiplier = term.second;
    if (multiplier == 0) continue;

    // We will only allow change of the form "multiplier += to_add" with to_add
    // in [-negative_limit, positive_limit].
    IntegerValue negative_limit = kMaxWantedCoeff;
    IntegerValue positive_limit = kMaxWantedCoeff;

    // Make sure we never change the sign of the multiplier, except if the
    // row is an equality in which case we don't care.
    if (integer_lp_[row].ub != integer_lp_[row].lb) {
      if (multiplier > 0) {
        negative_limit = std::min(negative_limit, multiplier);
      } else {
        positive_limit = std::min(positive_limit, -multiplier);
      }
    }

    // Make sure upper_bound + to_add * row_bound never overflow.
    const IntegerValue row_bound =
        multiplier > 0 ? integer_lp_[row].ub : integer_lp_[row].lb;
    if (row_bound != 0) {
      const IntegerValue limit1 = FloorRatio(
          std::max(IntegerValue(0), kMaxWantedCoeff - IntTypeAbs(*upper_bound)),
          IntTypeAbs(row_bound));
      const IntegerValue limit2 =
          FloorRatio(kMaxWantedCoeff, IntTypeAbs(row_bound));
      if ((*upper_bound > 0) == (row_bound > 0)) {  // Same sign.
        positive_limit = std::min(positive_limit, limit1);
        negative_limit = std::min(negative_limit, limit2);
      } else {
        negative_limit = std::min(negative_limit, limit1);
        positive_limit = std::min(positive_limit, limit2);
      }
    }

    // If we add the row to the scattered_vector, diff will indicate by how much
    // |upper_bound - ImpliedLB(scattered_vector)| will change. That correspond
    // to increasing the multiplier by 1.
    //
    // At this stage, we are not sure computing sum coeff * bound will not
    // overflow, so we use floating point numbers. It is fine to do so since
    // this is not directly involved in the actual exact constraint generation:
    // these variables are just used in an heuristic.
    double positive_diff = ToDouble(row_bound);
    double negative_diff = ToDouble(row_bound);

    // TODO(user): we could relax a bit some of the condition and allow a sign
    // change. It is just trickier to compute the diff when we allow such
    // changes.
    for (const auto entry : integer_lp_[row].terms) {
      const ColIndex col = entry.first;
      const IntegerValue coeff = entry.second;
      const IntegerValue abs_coef = IntTypeAbs(coeff);
      CHECK_NE(coeff, 0);

      const IntegerVariable var = integer_variables_[col.value()];
      const IntegerValue lb = integer_trail_->LowerBound(var);
      const IntegerValue ub = integer_trail_->UpperBound(var);

      // Moving a variable away from zero seems to improve the bound even
      // if it reduces the number of non-zero. Note that this is because of
      // this that positive_diff and negative_diff are not the same.
      const IntegerValue current = (*scattered_vector)[col];
      if (current == 0) {
        const IntegerValue overflow_limit(
            FloorRatio(kMaxWantedCoeff, abs_coef));
        positive_limit = std::min(positive_limit, overflow_limit);
        negative_limit = std::min(negative_limit, overflow_limit);
        if (coeff > 0) {
          positive_diff -= ToDouble(coeff) * ToDouble(lb);
          negative_diff -= ToDouble(coeff) * ToDouble(ub);
        } else {
          positive_diff -= ToDouble(coeff) * ToDouble(ub);
          negative_diff -= ToDouble(coeff) * ToDouble(lb);
        }
        continue;
      }

      // We don't want to change the sign of current (except if the variable is
      // fixed) or to have an overflow.
      //
      // Corner case:
      //  - IntTypeAbs(current) can be larger than kMaxWantedCoeff!
      //  - The code assumes that 2 * kMaxWantedCoeff do not overflow.
      const IntegerValue current_magnitude = IntTypeAbs(current);
      const IntegerValue other_direction_limit = FloorRatio(
          lb == ub
              ? kMaxWantedCoeff + std::min(current_magnitude,
                                           kMaxIntegerValue - kMaxWantedCoeff)
              : current_magnitude,
          abs_coef);
      const IntegerValue same_direction_limit(FloorRatio(
          std::max(IntegerValue(0), kMaxWantedCoeff - current_magnitude),
          abs_coef));
      if ((current > 0) == (coeff > 0)) {  // Same sign.
        negative_limit = std::min(negative_limit, other_direction_limit);
        positive_limit = std::min(positive_limit, same_direction_limit);
      } else {
        negative_limit = std::min(negative_limit, same_direction_limit);
        positive_limit = std::min(positive_limit, other_direction_limit);
      }

      // This is how diff change.
      const IntegerValue implied = current > 0 ? lb : ub;
      if (implied != 0) {
        positive_diff -= ToDouble(coeff) * ToDouble(implied);
        negative_diff -= ToDouble(coeff) * ToDouble(implied);
      }
    }

    // Only add a multiple of this row if it tighten the final constraint.
    // The positive_diff/negative_diff are supposed to be integer modulo the
    // double precision, so we only add a multiple if they seems far away from
    // zero.
    IntegerValue to_add(0);
    if (positive_diff <= -1.0 && positive_limit > 0) {
      to_add = positive_limit;
    }
    if (negative_diff >= 1.0 && negative_limit > 0) {
      // Pick this if it is better than the positive sign.
      if (to_add == 0 ||
          std::abs(ToDouble(negative_limit) * negative_diff) >
              std::abs(ToDouble(positive_limit) * positive_diff)) {
        to_add = -negative_limit;
      }
    }
    if (to_add != 0) {
      term.second += to_add;
      *upper_bound += to_add * row_bound;

      // TODO(user): we could avoid checking overflow here, but this is likely
      // not in the hot loop.
      CHECK(scattered_vector->AddLinearExpressionMultiple(
          to_add, integer_lp_[row].terms));
    }
  }
}

// The "exact" computation go as follow:
//
// Given any INTEGER linear combination of the LP constraints, we can create a
// new integer constraint that is valid (its computation must not overflow
// though). Lets call this "linear_combination <= ub". We can then always add to
// it the inequality "objective_terms <= objective_var", so we get:
// ImpliedLB(objective_terms + linear_combination) - ub <= objective_var.
// where ImpliedLB() is computed from the variable current bounds.
//
// Now, if we use for the linear combination and approximation of the optimal
// negated dual LP values (by scaling them and rounding them to integer), we
// will get an EXACT objective lower bound that is more or less the same as the
// inexact bound given by the LP relaxation. This allows to derive exact reasons
// for any propagation done by this constraint.
bool LinearProgrammingConstraint::ExactLpReasonning() {
  // Clear old reason and deductions.
  integer_reason_.clear();
  deductions_.clear();
  deductions_reason_.clear();

  // The row multipliers will be the negation of the LP duals.
  //
  // TODO(user): Provide and use a sparse API in Glop to get the duals.
  const RowIndex num_rows = simplex_.GetProblemNumRows();
  std::vector<std::pair<RowIndex, double>> lp_multipliers;
  for (RowIndex row(0); row < num_rows; ++row) {
    const double value = -simplex_.GetDualValue(row);
    if (std::abs(value) < kZeroTolerance) continue;
    lp_multipliers.push_back({row, value});
  }

  Fractional scaling;
  std::vector<std::pair<RowIndex, IntegerValue>> integer_multipliers =
      ScaleLpMultiplier(/*take_objective_into_account=*/true, lp_multipliers,
                        &scaling);

  IntegerValue rc_ub;
  if (!ComputeNewLinearConstraint(integer_multipliers, &tmp_scattered_vector_,
                                  &rc_ub)) {
    VLOG(1) << "Issue while computing the exact LP reason. Aborting.";
    return true;
  }

  // The "objective constraint" behave like if the unscaled cp multiplier was
  // 1.0, so we will multiply it by this number and add it to reduced_costs.
  const IntegerValue obj_scale(std::round(scaling));
  if (obj_scale == 0) {
    VLOG(1) << "Overflow during exact LP reasoning. scaling=" << scaling;
    return true;
  }
  CHECK(tmp_scattered_vector_.AddLinearExpressionMultiple(obj_scale,
                                                          integer_objective_));
  CHECK(AddProductTo(-obj_scale, integer_objective_offset_, &rc_ub));
  AdjustNewLinearConstraint(&integer_multipliers, &tmp_scattered_vector_,
                            &rc_ub);

  // Create the IntegerSumLE that will allow to propagate the objective and more
  // generally do the reduced cost fixing.
  LinearConstraint new_constraint;
  tmp_scattered_vector_.ConvertToLinearConstraint(integer_variables_, rc_ub,
                                                  &new_constraint);
  new_constraint.vars.push_back(objective_cp_);
  new_constraint.coeffs.push_back(-obj_scale);
  DivideByGCD(&new_constraint);
  PreventOverflow(&new_constraint);
  DCHECK(!PossibleOverflow(new_constraint));
  DCHECK(constraint_manager_.DebugCheckConstraint(new_constraint));

  // Corner case where prevent overflow removed all terms.
  if (new_constraint.vars.empty()) {
    trail_->MutableConflict()->clear();
    return new_constraint.ub >= 0;
  }

  IntegerSumLE* cp_constraint =
      new IntegerSumLE({}, new_constraint.vars, new_constraint.coeffs,
                       new_constraint.ub, model_);
  if (trail_->CurrentDecisionLevel() == 0) {
    // Since we will never ask the reason for a constraint at level 0, we just
    // keep the last one.
    optimal_constraints_.clear();
  }
  optimal_constraints_.emplace_back(cp_constraint);
  rev_optimal_constraints_size_ = optimal_constraints_.size();
  if (!cp_constraint->PropagateAtLevelZero()) return false;
  return cp_constraint->Propagate();
}

bool LinearProgrammingConstraint::FillExactDualRayReason() {
  Fractional scaling;
  const glop::DenseColumn ray = simplex_.GetDualRay();
  std::vector<std::pair<RowIndex, double>> lp_multipliers;
  for (RowIndex row(0); row < ray.size(); ++row) {
    const double value = ray[row];
    if (std::abs(value) < kZeroTolerance) continue;
    lp_multipliers.push_back({row, value});
  }
  std::vector<std::pair<RowIndex, IntegerValue>> integer_multipliers =
      ScaleLpMultiplier(/*take_objective_into_account=*/false, lp_multipliers,
                        &scaling);

  IntegerValue new_constraint_ub;
  if (!ComputeNewLinearConstraint(integer_multipliers, &tmp_scattered_vector_,
                                  &new_constraint_ub)) {
    VLOG(1) << "Isse while computing the exact dual ray reason. Aborting.";
    return false;
  }

  AdjustNewLinearConstraint(&integer_multipliers, &tmp_scattered_vector_,
                            &new_constraint_ub);

  LinearConstraint new_constraint;
  tmp_scattered_vector_.ConvertToLinearConstraint(
      integer_variables_, new_constraint_ub, &new_constraint);
  DivideByGCD(&new_constraint);
  PreventOverflow(&new_constraint);
  DCHECK(!PossibleOverflow(new_constraint));
  DCHECK(constraint_manager_.DebugCheckConstraint(new_constraint));

  const IntegerValue implied_lb = GetImpliedLowerBound(new_constraint);
  if (implied_lb <= new_constraint.ub) {
    VLOG(1) << "LP exact dual ray not infeasible,"
            << " implied_lb: " << implied_lb.value() / scaling
            << " ub: " << new_constraint.ub.value() / scaling;
    return false;
  }
  const IntegerValue slack = (implied_lb - new_constraint.ub) - 1;
  SetImpliedLowerBoundReason(new_constraint, slack);
  return true;
}

int64_t LinearProgrammingConstraint::CalculateDegeneracy() {
  const glop::ColIndex num_vars = simplex_.GetProblemNumCols();
  int num_non_basic_with_zero_rc = 0;
  for (glop::ColIndex i(0); i < num_vars; ++i) {
    const double rc = simplex_.GetReducedCost(i);
    if (rc != 0.0) continue;
    if (simplex_.GetVariableStatus(i) == glop::VariableStatus::BASIC) {
      continue;
    }
    num_non_basic_with_zero_rc++;
  }
  const int64_t num_cols = simplex_.GetProblemNumCols().value();
  is_degenerate_ = num_non_basic_with_zero_rc >= 0.3 * num_cols;
  return num_non_basic_with_zero_rc;
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
    const double cp_other_bound =
        scaler_.UnscaleVariableValue(lp_var, lp_other_bound);

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

// Add a cut of the form Sum_{outgoing arcs from S} lp >= rhs_lower_bound.
//
// Note that we used to also add the same cut for the incoming arcs, but because
// of flow conservation on these problems, the outgoing flow is always the same
// as the incoming flow, so adding this extra cut doesn't seem relevant.
void AddOutgoingCut(
    int num_nodes, int subset_size, const std::vector<bool>& in_subset,
    const std::vector<int>& tails, const std::vector<int>& heads,
    const std::vector<Literal>& literals,
    const std::vector<double>& literal_lp_values, int64_t rhs_lower_bound,
    const absl::StrongVector<IntegerVariable, double>& lp_values,
    LinearConstraintManager* manager, Model* model) {
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
    if (in_subset[tails[i]]) {
      num_optional_nodes_in++;
      if (optional_loop_in == -1 ||
          literal_lp_values[i] < literal_lp_values[optional_loop_in]) {
        optional_loop_in = i;
      }
    } else {
      num_optional_nodes_out++;
      if (optional_loop_out == -1 ||
          literal_lp_values[i] < literal_lp_values[optional_loop_out]) {
        optional_loop_out = i;
      }
    }
  }

  // TODO(user): The lower bound for CVRP is computed assuming all nodes must be
  // served, if it is > 1 we lower it to one in the presence of optional nodes.
  if (num_optional_nodes_in + num_optional_nodes_out > 0) {
    CHECK_GE(rhs_lower_bound, 1);
    rhs_lower_bound = 1;
  }

  LinearConstraintBuilder outgoing(model, IntegerValue(rhs_lower_bound),
                                   kMaxIntegerValue);
  double sum_outgoing = 0.0;

  // Add outgoing arcs, compute outgoing flow.
  for (int i = 0; i < tails.size(); ++i) {
    if (in_subset[tails[i]] && !in_subset[heads[i]]) {
      sum_outgoing += literal_lp_values[i];
      CHECK(outgoing.AddLiteralTerm(literals[i], IntegerValue(1)));
    }
  }

  // Support optional nodes if any.
  if (num_optional_nodes_in + num_optional_nodes_out > 0) {
    // When all optionals of one side are excluded in lp solution, no cut.
    if (num_optional_nodes_in == subset_size &&
        (optional_loop_in == -1 ||
         literal_lp_values[optional_loop_in] > 1.0 - 1e-6)) {
      return;
    }
    if (num_optional_nodes_out == num_nodes - subset_size &&
        (optional_loop_out == -1 ||
         literal_lp_values[optional_loop_out] > 1.0 - 1e-6)) {
      return;
    }

    // There is no mandatory node in subset, add optional_loop_in.
    if (num_optional_nodes_in == subset_size) {
      CHECK(
          outgoing.AddLiteralTerm(literals[optional_loop_in], IntegerValue(1)));
      sum_outgoing += literal_lp_values[optional_loop_in];
    }

    // There is no mandatory node out of subset, add optional_loop_out.
    if (num_optional_nodes_out == num_nodes - subset_size) {
      CHECK(outgoing.AddLiteralTerm(literals[optional_loop_out],
                                    IntegerValue(1)));
      sum_outgoing += literal_lp_values[optional_loop_out];
    }
  }

  if (sum_outgoing < rhs_lower_bound - 1e-6) {
    manager->AddCut(outgoing.Build(), "Circuit", lp_values);
  }
}

}  // namespace

// We roughly follow the algorithm described in section 6 of "The Traveling
// Salesman Problem, A computational Study", David L. Applegate, Robert E.
// Bixby, Vasek Chvatal, William J. Cook.
//
// Note that this is mainly a "symmetric" case algo, but it does still work for
// the asymmetric case.
void SeparateSubtourInequalities(
    int num_nodes, const std::vector<int>& tails, const std::vector<int>& heads,
    const std::vector<Literal>& literals,
    const absl::StrongVector<IntegerVariable, double>& lp_values,
    absl::Span<const int64_t> demands, int64_t capacity,
    LinearConstraintManager* manager, Model* model) {
  if (num_nodes <= 2) return;

  // We will collect only the arcs with a positive lp_values to speed up some
  // computation below.
  struct Arc {
    int tail;
    int head;
    double lp_value;
  };
  std::vector<Arc> relevant_arcs;

  // Sort the arcs by non-increasing lp_values.
  std::vector<double> literal_lp_values(literals.size());
  std::vector<std::pair<double, int>> arc_by_decreasing_lp_values;
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  for (int i = 0; i < literals.size(); ++i) {
    double lp_value;
    const IntegerVariable direct_view = encoder->GetLiteralView(literals[i]);
    if (direct_view != kNoIntegerVariable) {
      lp_value = lp_values[direct_view];
    } else {
      lp_value =
          1.0 - lp_values[encoder->GetLiteralView(literals[i].Negated())];
    }
    literal_lp_values[i] = lp_value;

    if (lp_value < 1e-6) continue;
    relevant_arcs.push_back({tails[i], heads[i], lp_value});
    arc_by_decreasing_lp_values.push_back({lp_value, i});
  }
  std::sort(arc_by_decreasing_lp_values.begin(),
            arc_by_decreasing_lp_values.end(),
            std::greater<std::pair<double, int>>());

  // We will do a union-find by adding one by one the arc of the lp solution
  // in the order above. Every intermediate set during this construction will
  // be a candidate for a cut.
  //
  // In parallel to the union-find, to efficiently reconstruct these sets (at
  // most num_nodes), we construct a "decomposition forest" of the different
  // connected components. Note that we don't exploit any asymmetric nature of
  // the graph here. This is exactly the algo 6.3 in the book above.
  int num_components = num_nodes;
  std::vector<int> parent(num_nodes);
  std::vector<int> root(num_nodes);
  for (int i = 0; i < num_nodes; ++i) {
    parent[i] = i;
    root[i] = i;
  }
  auto get_root_and_compress_path = [&root](int node) {
    int r = node;
    while (root[r] != r) r = root[r];
    while (root[node] != r) {
      const int next = root[node];
      root[node] = r;
      node = next;
    }
    return r;
  };
  for (const auto pair : arc_by_decreasing_lp_values) {
    if (num_components == 2) break;
    const int tail = get_root_and_compress_path(tails[pair.second]);
    const int head = get_root_and_compress_path(heads[pair.second]);
    if (tail != head) {
      // Update the decomposition forest, note that the number of nodes is
      // growing.
      const int new_node = parent.size();
      parent.push_back(new_node);
      parent[head] = new_node;
      parent[tail] = new_node;
      --num_components;

      // It is important that the union-find representative is the same node.
      root.push_back(new_node);
      root[head] = new_node;
      root[tail] = new_node;
    }
  }

  // For each node in the decomposition forest, try to add a cut for the set
  // formed by the nodes and its children. To do that efficiently, we first
  // order the nodes so that for each node in a tree, the set of children forms
  // a consecutive span in the pre_order vector. This vector just lists the
  // nodes in the "pre-order" graph traversal order. The Spans will point inside
  // the pre_order vector, it is why we initialize it once and for all.
  int new_size = 0;
  std::vector<int> pre_order(num_nodes);
  std::vector<absl::Span<const int>> subsets;
  {
    std::vector<absl::InlinedVector<int, 2>> graph(parent.size());
    for (int i = 0; i < parent.size(); ++i) {
      if (parent[i] != i) graph[parent[i]].push_back(i);
    }
    std::vector<int> queue;
    std::vector<bool> seen(graph.size(), false);
    std::vector<int> start_index(parent.size());
    for (int i = num_nodes; i < parent.size(); ++i) {
      // Note that because of the way we constructed 'parent', the graph is a
      // binary tree. This is not required for the correctness of the algorithm
      // here though.
      CHECK(graph[i].empty() || graph[i].size() == 2);
      if (parent[i] != i) continue;

      // Explore the subtree rooted at node i.
      CHECK(!seen[i]);
      queue.push_back(i);
      while (!queue.empty()) {
        const int node = queue.back();
        if (seen[node]) {
          queue.pop_back();
          // All the children of node are in the span [start, end) of the
          // pre_order vector.
          const int start = start_index[node];
          if (new_size - start > 1) {
            subsets.emplace_back(&pre_order[start], new_size - start);
          }
          continue;
        }
        seen[node] = true;
        start_index[node] = new_size;
        if (node < num_nodes) pre_order[new_size++] = node;
        for (const int child : graph[node]) {
          if (!seen[child]) queue.push_back(child);
        }
      }
    }
  }

  // Compute the total demands in order to know the minimum incoming/outgoing
  // flow.
  int64_t total_demands = 0;
  if (!demands.empty()) {
    for (const int64_t demand : demands) total_demands += demand;
  }

  // Process each subsets and add any violated cut.
  CHECK_EQ(pre_order.size(), num_nodes);
  std::vector<bool> in_subset(num_nodes, false);
  for (const absl::Span<const int> subset : subsets) {
    CHECK_GT(subset.size(), 1);
    CHECK_LT(subset.size(), num_nodes);

    // These fields will be left untouched if demands.empty().
    bool contain_depot = false;
    int64_t subset_demand = 0;

    // Initialize "in_subset" and the subset demands.
    for (const int n : subset) {
      in_subset[n] = true;
      if (!demands.empty()) {
        if (n == 0) contain_depot = true;
        subset_demand += demands[n];
      }
    }

    // Compute a lower bound on the outgoing flow.
    //
    // TODO(user): This lower bound assume all nodes in subset must be served,
    // which is not the case. For TSP we do the correct thing in
    // AddOutgoingCut() but not for CVRP... Fix!!
    //
    // TODO(user): It could be very interesting to see if this "min outgoing
    // flow" cannot be automatically infered from the constraint in the
    // precedence graph. This might work if we assume that any kind of path
    // cumul constraint is encoded with constraints:
    //   [edge => value_head >= value_tail + edge_weight].
    // We could take the minimum incoming edge weight per node in the set, and
    // use the cumul variable domain to infer some capacity.
    int64_t min_outgoing_flow = 1;
    if (!demands.empty()) {
      min_outgoing_flow =
          contain_depot
              ? (total_demands - subset_demand + capacity - 1) / capacity
              : (subset_demand + capacity - 1) / capacity;
    }

    // We still need to serve nodes with a demand of zero, and in the corner
    // case where all node in subset have a zero demand, the formula above
    // result in a min_outgoing_flow of zero.
    min_outgoing_flow = std::max(min_outgoing_flow, int64_t{1});

    // Compute the current outgoing flow out of the subset.
    //
    // This can take a significant portion of the running time, it is why it is
    // faster to do it only on arcs with non-zero lp values which should be in
    // linear number rather than the total number of arc which can be quadratic.
    //
    // TODO(user): For the symmetric case there is an even faster algo. See if
    // it can be generalized to the asymmetric one if become needed.
    // Reference is algo 6.4 of the "The Traveling Salesman Problem" book
    // mentionned above.
    double outgoing_flow = 0.0;
    for (const auto arc : relevant_arcs) {
      if (in_subset[arc.tail] && !in_subset[arc.head]) {
        outgoing_flow += arc.lp_value;
      }
    }

    // Add a cut if the current outgoing flow is not enough.
    if (outgoing_flow < min_outgoing_flow - 1e-6) {
      AddOutgoingCut(num_nodes, subset.size(), in_subset, tails, heads,
                     literals, literal_lp_values,
                     /*rhs_lower_bound=*/min_outgoing_flow, lp_values, manager,
                     model);
    }

    // Sparse clean up.
    for (const int n : subset) in_subset[n] = false;
  }
}

namespace {

// Returns for each literal its integer view, or the view of its negation.
std::vector<IntegerVariable> GetAssociatedVariables(
    const std::vector<Literal>& literals, Model* model) {
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  std::vector<IntegerVariable> result;
  for (const Literal l : literals) {
    const IntegerVariable direct_view = encoder->GetLiteralView(l);
    if (direct_view != kNoIntegerVariable) {
      result.push_back(direct_view);
    } else {
      result.push_back(encoder->GetLiteralView(l.Negated()));
      DCHECK_NE(result.back(), kNoIntegerVariable);
    }
  }
  return result;
}

}  // namespace

// We use a basic algorithm to detect components that are not connected to the
// rest of the graph in the LP solution, and add cuts to force some arcs to
// enter and leave this component from outside.
CutGenerator CreateStronglyConnectedGraphCutGenerator(
    int num_nodes, const std::vector<int>& tails, const std::vector<int>& heads,
    const std::vector<Literal>& literals, Model* model) {
  CutGenerator result;
  result.vars = GetAssociatedVariables(literals, model);
  result.generate_cuts =
      [num_nodes, tails, heads, literals, model](
          const absl::StrongVector<IntegerVariable, double>& lp_values,
          LinearConstraintManager* manager) {
        SeparateSubtourInequalities(
            num_nodes, tails, heads, literals, lp_values,
            /*demands=*/{}, /*capacity=*/0, manager, model);
        return true;
      };
  return result;
}

CutGenerator CreateCVRPCutGenerator(int num_nodes,
                                    const std::vector<int>& tails,
                                    const std::vector<int>& heads,
                                    const std::vector<Literal>& literals,
                                    const std::vector<int64_t>& demands,
                                    int64_t capacity, Model* model) {
  CutGenerator result;
  result.vars = GetAssociatedVariables(literals, model);
  result.generate_cuts =
      [num_nodes, tails, heads, demands, capacity, literals, model](
          const absl::StrongVector<IntegerVariable, double>& lp_values,
          LinearConstraintManager* manager) {
        SeparateSubtourInequalities(num_nodes, tails, heads, literals,
                                    lp_values, demands, capacity, manager,
                                    model);
        return true;
      };
  return result;
}

std::function<IntegerLiteral()>
LinearProgrammingConstraint::HeuristicLpMostInfeasibleBinary(Model* model) {
  // Gather all 0-1 variables that appear in this LP.
  std::vector<IntegerVariable> variables;
  for (IntegerVariable var : integer_variables_) {
    if (integer_trail_->LowerBound(var) == 0 &&
        integer_trail_->UpperBound(var) == 1) {
      variables.push_back(var);
    }
  }
  VLOG(1) << "HeuristicLPMostInfeasibleBinary has " << variables.size()
          << " variables.";

  return [this, variables]() {
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
      IntegerLiteral::GreaterOrEqual(fractional_var, IntegerValue(1));
    }
    return IntegerLiteral();
  };
}

std::function<IntegerLiteral()>
LinearProgrammingConstraint::HeuristicLpReducedCostBinary(Model* model) {
  // Gather all 0-1 variables that appear in this LP.
  std::vector<IntegerVariable> variables;
  for (IntegerVariable var : integer_variables_) {
    if (integer_trail_->LowerBound(var) == 0 &&
        integer_trail_->UpperBound(var) == 1) {
      variables.push_back(var);
    }
  }
  VLOG(1) << "HeuristicLpReducedCostBinary has " << variables.size()
          << " variables.";

  // Store average of reduced cost from 1 to 0. The best heuristic only sets
  // variables to one and cares about cost to zero, even though classic
  // pseudocost will use max_var min(cost_to_one[var], cost_to_zero[var]).
  const int num_vars = variables.size();
  std::vector<double> cost_to_zero(num_vars, 0.0);
  std::vector<int> num_cost_to_zero(num_vars);
  int num_calls = 0;

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
      if (integer_trail_->IsFixed(var)) continue;

      if (num_cost_to_zero[i] > 0 &&
          best_cost < cost_to_zero[i] / num_cost_to_zero[i]) {
        best_cost = cost_to_zero[i] / num_cost_to_zero[i];
        selected_index = i;
      }
    }

    if (selected_index >= 0) {
      return IntegerLiteral::GreaterOrEqual(variables[selected_index],
                                            IntegerValue(1));
    }
    return IntegerLiteral();
  };
}

void LinearProgrammingConstraint::UpdateAverageReducedCosts() {
  const int num_vars = integer_variables_.size();
  if (sum_cost_down_.size() < num_vars) {
    sum_cost_down_.resize(num_vars, 0.0);
    num_cost_down_.resize(num_vars, 0);
    sum_cost_up_.resize(num_vars, 0.0);
    num_cost_up_.resize(num_vars, 0);
    rc_scores_.resize(num_vars, 0.0);
  }

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

  // Accumulate reduced costs of all unassigned variables.
  for (int i = 0; i < num_vars; i++) {
    const IntegerVariable var = integer_variables_[i];

    // Skip ignored and fixed variables.
    if (integer_trail_->IsCurrentlyIgnored(var)) continue;
    if (integer_trail_->IsFixed(var)) continue;

    // Skip reduced costs that are zero or close.
    const double rc = lp_reduced_cost_[i];
    if (std::abs(rc) < kCpEpsilon) continue;

    if (rc < 0.0) {
      sum_cost_down_[i] -= rc;
      num_cost_down_[i]++;
    } else {
      sum_cost_up_[i] += rc;
      num_cost_up_[i]++;
    }
  }

  // Tricky, we artificially reset the rc_rev_int_repository_ to level zero
  // so that the rev_rc_start_ is zero.
  rc_rev_int_repository_.SetLevel(0);
  rc_rev_int_repository_.SetLevel(trail_->CurrentDecisionLevel());
  rev_rc_start_ = 0;

  // Cache the new score (higher is better) using the average reduced costs
  // as a signal.
  positions_by_decreasing_rc_score_.clear();
  for (int i = 0; i < num_vars; i++) {
    // If only one direction exist, we takes its value divided by 2, so that
    // such variable should have a smaller cost than the min of the two side
    // except if one direction have a really high reduced costs.
    const double a_up =
        num_cost_up_[i] > 0 ? sum_cost_up_[i] / num_cost_up_[i] : 0.0;
    const double a_down =
        num_cost_down_[i] > 0 ? sum_cost_down_[i] / num_cost_down_[i] : 0.0;
    if (num_cost_down_[i] > 0 && num_cost_up_[i] > 0) {
      rc_scores_[i] = std::min(a_up, a_down);
    } else {
      rc_scores_[i] = 0.5 * (a_down + a_up);
    }

    // We ignore scores of zero (i.e. no data) and will follow the default
    // search heuristic if all variables are like this.
    if (rc_scores_[i] > 0.0) {
      positions_by_decreasing_rc_score_.push_back({-rc_scores_[i], i});
    }
  }
  std::sort(positions_by_decreasing_rc_score_.begin(),
            positions_by_decreasing_rc_score_.end());
}

// TODO(user): Remove duplication with HeuristicLpReducedCostBinary().
std::function<IntegerLiteral()>
LinearProgrammingConstraint::HeuristicLpReducedCostAverageBranching() {
  return [this]() { return this->LPReducedCostAverageDecision(); };
}

IntegerLiteral LinearProgrammingConstraint::LPReducedCostAverageDecision() {
  // Select noninstantiated variable with highest positive average reduced cost.
  int selected_index = -1;
  const int size = positions_by_decreasing_rc_score_.size();
  rc_rev_int_repository_.SaveState(&rev_rc_start_);
  for (int i = rev_rc_start_; i < size; ++i) {
    const int index = positions_by_decreasing_rc_score_[i].second;
    const IntegerVariable var = integer_variables_[index];
    if (integer_trail_->IsCurrentlyIgnored(var)) continue;
    if (integer_trail_->IsFixed(var)) continue;
    selected_index = index;
    rev_rc_start_ = i;
    break;
  }

  if (selected_index == -1) return IntegerLiteral();
  const IntegerVariable var = integer_variables_[selected_index];

  // If ceil(value) is current upper bound, try var == upper bound first.
  // Guarding with >= prevents numerical problems.
  // With 0/1 variables, this will tend to try setting to 1 first,
  // which produces more shallow trees.
  const IntegerValue ub = integer_trail_->UpperBound(var);
  const IntegerValue value_ceil(
      std::ceil(this->GetSolutionValue(var) - kCpEpsilon));
  if (value_ceil >= ub) {
    return IntegerLiteral::GreaterOrEqual(var, ub);
  }

  // If floor(value) is current lower bound, try var == lower bound first.
  // Guarding with <= prevents numerical problems.
  const IntegerValue lb = integer_trail_->LowerBound(var);
  const IntegerValue value_floor(
      std::floor(this->GetSolutionValue(var) + kCpEpsilon));
  if (value_floor <= lb) {
    return IntegerLiteral::LowerOrEqual(var, lb);
  }

  // Here lb < value_floor <= value_ceil < ub.
  // Try the most promising split between var <= floor or var >= ceil.
  const double a_up =
      num_cost_up_[selected_index] > 0
          ? sum_cost_up_[selected_index] / num_cost_up_[selected_index]
          : 0.0;
  const double a_down =
      num_cost_down_[selected_index] > 0
          ? sum_cost_down_[selected_index] / num_cost_down_[selected_index]
          : 0.0;
  if (a_down < a_up) {
    return IntegerLiteral::LowerOrEqual(var, value_floor);
  } else {
    return IntegerLiteral::GreaterOrEqual(var, value_ceil);
  }
}

std::string LinearProgrammingConstraint::Statistics() const {
  std::string result = "LP statistics:\n";
  absl::StrAppend(&result, "  final dimension: ", DimensionString(), "\n");
  absl::StrAppend(&result, "  total number of simplex iterations: ",
                  total_num_simplex_iterations_, "\n");
  absl::StrAppend(&result, "  num solves: \n");
  for (int i = 0; i < num_solves_by_status_.size(); ++i) {
    if (num_solves_by_status_[i] == 0) continue;
    absl::StrAppend(&result, "    - #",
                    glop::GetProblemStatusString(glop::ProblemStatus(i)), ": ",
                    num_solves_by_status_[i], "\n");
  }
  absl::StrAppend(&result, constraint_manager_.Statistics());
  return result;
}

}  // namespace sat
}  // namespace operations_research

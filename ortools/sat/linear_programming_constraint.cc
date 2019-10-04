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

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/int128.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/preprocessor.h"
#include "ortools/glop/status.h"
#include "ortools/graph/strongly_connected_components.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {
namespace sat {

using glop::ColIndex;
using glop::Fractional;
using glop::RowIndex;

const double LinearProgrammingConstraint::kCpEpsilon = 1e-4;
const double LinearProgrammingConstraint::kLpEpsilon = 1e-6;

// TODO(user): make SatParameters singleton too, otherwise changing them after
// a constraint was added will have no effect on this class.
LinearProgrammingConstraint::LinearProgrammingConstraint(Model* model)
    : constraint_manager_(model),
      sat_parameters_(*(model->GetOrCreate<SatParameters>())),
      model_(model),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      trail_(model->GetOrCreate<Trail>()),
      model_heuristics_(model->GetOrCreate<SearchHeuristicsVector>()),
      integer_encoder_(model->GetOrCreate<IntegerEncoder>()),
      implied_bounds_processor_({}, integer_trail_,
                                model->GetOrCreate<ImpliedBounds>()),
      dispatcher_(model->GetOrCreate<LinearProgrammingDispatcher>()),
      expanded_lp_solution_(
          *model->GetOrCreate<LinearProgrammingConstraintLpSolution>()) {
  // Tweak the default parameters to make the solve incremental.
  glop::GlopParameters parameters;
  parameters.set_use_dual_simplex(true);
  simplex_.SetParameters(parameters);
  if (sat_parameters_.use_branching_in_lp() ||
      sat_parameters_.search_branching() == SatParameters::LP_SEARCH) {
    compute_reduced_cost_averages_ = true;
  }
}

LinearProgrammingConstraint::~LinearProgrammingConstraint() {
  VLOG(1) << "Total number of simplex iterations: "
          << total_num_simplex_iterations_;
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
void LinearProgrammingConstraint::CreateLpFromConstraintManager() {
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
  for (const auto entry : integer_objective_) {
    lp_data_.SetObjectiveCoefficient(entry.first, ToDouble(entry.second));
  }
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

  scaler_.Scale(&lp_data_);
  UpdateBoundsOfLpVariables();

  lp_data_.NotifyThatColumnsAreClean();
  lp_data_.AddSlackVariablesWhereNecessary(false);
  VLOG(1) << "LP relaxation: " << lp_data_.GetDimensionString() << ". "
          << constraint_manager_.AllConstraints().size()
          << " Managed constraints.";
}

LPSolveInfo LinearProgrammingConstraint::SolveLpForBranching() {
  LPSolveInfo info;
  glop::BasisState basis_state = simplex_.GetState();

  const auto status = simplex_.Solve(lp_data_, time_limit_);
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
        static_cast<int64>(std::ceil(info.lp_objective - kCpEpsilon)));
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
  if (!sat_parameters_.add_lp_constraints_lazily()) {
    constraint_manager_.AddAllConstraintsToLp();
  }
  CreateLpFromConstraintManager();

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

  if (integer_variables_.size() >= 20) {  // Do not use on small subparts.
    auto* container = model->GetOrCreate<SearchHeuristicsVector>();
    container->push_back(HeuristicLPPseudoCostBinary(model));
    container->push_back(HeuristicLPMostInfeasibleBinary(model));
  }

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

LinearConstraint LinearProgrammingConstraint::ConvertToLinearConstraint(
    const gtl::ITIVector<ColIndex, IntegerValue>& dense_vector,
    IntegerValue upper_bound) {
  LinearConstraint result;
  for (ColIndex col(0); col < dense_vector.size(); ++col) {
    const IntegerValue coeff = dense_vector[col];
    if (coeff == 0) continue;
    const IntegerVariable var = integer_variables_[col.value()];
    result.vars.push_back(var);
    result.coeffs.push_back(coeff);
  }
  result.lb = kMinIntegerValue;
  result.ub = upper_bound;
  return result;
}

namespace {

// Returns false in case of overflow
bool AddLinearExpressionMultiple(
    IntegerValue multiplier,
    const std::vector<std::pair<ColIndex, IntegerValue>>& terms,
    gtl::ITIVector<ColIndex, IntegerValue>* dense_vector) {
  for (const std::pair<ColIndex, IntegerValue> term : terms) {
    if (!AddProductTo(multiplier, term.second, &(*dense_vector)[term.first])) {
      return false;
    }
  }
  return true;
}

}  // namespace

void LinearProgrammingConstraint::AddCutFromConstraints(
    const std::string& name,
    const std::vector<std::pair<RowIndex, IntegerValue>>& integer_multipliers) {
  // This is initialized to a valid linear contraint (by taking linear
  // combination of the LP rows) and will be transformed into a cut if
  // possible.
  //
  // TODO(user): Ideally this linear combination should have only one
  // fractional variable (basis_col). But because of imprecision, we get a
  // bunch of fractional entry with small coefficient (relative to the one of
  // basis_col). We try to handle that in IntegerRoundingCut(), but it might
  // be better to add small multiple of the involved rows to get rid of them.
  LinearConstraint cut;
  {
    gtl::ITIVector<ColIndex, IntegerValue> dense_cut;
    IntegerValue cut_ub;
    if (!ComputeNewLinearConstraint(
            /*use_constraint_status=*/true, integer_multipliers, &dense_cut,
            &cut_ub)) {
      VLOG(1) << "Issue, overflow!";
      return;
    }

    // Important: because we use integer_multipliers below, we cannot just
    // divide by GCD or call PreventOverflow() here.
    cut = ConvertToLinearConstraint(dense_cut, cut_ub);
  }

  // This should be tight!
  if (std::abs(ComputeActivity(cut, expanded_lp_solution_) - ToDouble(cut.ub)) /
          std::max(1.0, std::abs(ToDouble(cut.ub))) >
      1e-2) {
    VLOG(1) << "Cut not tight " << ComputeActivity(cut, expanded_lp_solution_)
            << " " << ToDouble(cut.ub);
    return;
  }

  // Unlike for the knapsack cuts, it might not be always beneficial to
  // process the implied bounds even though it seems to be better in average.
  //
  // TODO(user): Understand & investigate more.
  implied_bounds_processor_.ProcessUpperBoundedConstraint(expanded_lp_solution_,
                                                          &cut);

  // TODO(user): Might be cleaner to only do that in the functions that needs
  // this precondition below.
  MakeAllVariablesPositive(&cut);

  // Fills data for IntegerRoundingCut().
  //
  // Note(user): we use the current bound here, so the reasonement will only
  // produce locally valid cut if we call this at a non-root node. We could
  // use the level zero bounds if we wanted to generate a globally valid cut
  // at another level, but we will likely not genereate a constraint violating
  // the current lp solution in that case.
  std::vector<double> lp_values;
  std::vector<IntegerValue> var_lbs;
  std::vector<IntegerValue> var_ubs;
  for (const IntegerVariable var : cut.vars) {
    lp_values.push_back(expanded_lp_solution_[var]);
    var_lbs.push_back(integer_trail_->LowerBound(var));
    var_ubs.push_back(integer_trail_->UpperBound(var));
  }

  // Add slack.
  // definition: integer_lp_[row] + slack_row == bound;
  const IntegerVariable first_slack(expanded_lp_solution_.size());
  for (const auto pair : integer_multipliers) {
    const RowIndex row = pair.first;
    const IntegerValue coeff = pair.second;
    const auto status = simplex_.GetConstraintStatus(row);
    if (status == glop::ConstraintStatus::FIXED_VALUE) continue;

    lp_values.push_back(0.0);
    cut.vars.push_back(first_slack + IntegerVariable(row.value()));
    cut.coeffs.push_back(coeff);

    const IntegerValue diff(
        CapSub(integer_lp_[row].ub.value(), integer_lp_[row].lb.value()));
    if (status == glop::ConstraintStatus::AT_UPPER_BOUND) {
      var_lbs.push_back(IntegerValue(0));
      var_ubs.push_back(diff);
    } else {
      CHECK_EQ(status, glop::ConstraintStatus::AT_LOWER_BOUND);
      var_lbs.push_back(-diff);
      var_ubs.push_back(IntegerValue(0));
    }
  }

  // Get the cut using some integer rounding heuristic.
  RoundingOptions options;
  options.use_mir = sat_parameters_.use_mir_rounding();
  options.max_scaling = sat_parameters_.max_integer_rounding_scaling();
  IntegerRoundingCut(options, lp_values, var_lbs, var_ubs, &cut);

  // Compute the activity. Warning: the cut no longer have the same size so we
  // cannot use lp_values. Note that the substitution below shouldn't change
  // the activity by definition.
  double activity = 0.0;
  for (int i = 0; i < cut.vars.size(); ++i) {
    if (cut.vars[i] < first_slack) {
      activity += ToDouble(cut.coeffs[i]) * expanded_lp_solution_[cut.vars[i]];
    }
  }
  const double kMinViolation = 1e-4;
  const double violation = activity - ToDouble(cut.ub);
  if (violation < kMinViolation) {
    VLOG(3) << "Bad cut " << activity << " <= " << ToDouble(cut.ub);
    return;
  }

  // Substitute any slack left.
  {
    int num_slack = 0;
    gtl::ITIVector<ColIndex, IntegerValue> dense_cut(integer_variables_.size(),
                                                     IntegerValue(0));
    IntegerValue cut_ub = cut.ub;
    bool overflow = false;
    for (int i = 0; i < cut.vars.size(); ++i) {
      if (cut.vars[i] < first_slack) {
        CHECK(VariableIsPositive(cut.vars[i]));
        const glop::ColIndex col =
            gtl::FindOrDie(mirror_lp_variable_, cut.vars[i]);
        dense_cut[col] = cut.coeffs[i];
      } else {
        ++num_slack;

        // Update the constraint.
        const glop::RowIndex row(cut.vars[i].value() - first_slack.value());
        const IntegerValue multiplier = -cut.coeffs[i];
        if (!AddLinearExpressionMultiple(multiplier, integer_lp_[row].terms,
                                         &dense_cut)) {
          overflow = true;
          break;
        }

        // Update rhs.
        const auto status = simplex_.GetConstraintStatus(row);
        if (status == glop::ConstraintStatus::AT_LOWER_BOUND) {
          if (!AddProductTo(multiplier, integer_lp_[row].lb, &cut_ub)) {
            overflow = true;
            break;
          }
        } else {
          CHECK_EQ(status, glop::ConstraintStatus::AT_UPPER_BOUND);
          if (!AddProductTo(multiplier, integer_lp_[row].ub, &cut_ub)) {
            overflow = true;
            break;
          }
        }
      }
    }

    if (overflow) {
      VLOG(1) << "Overflow in slack removal.";
      return;
    }

    VLOG(3) << " num_slack: " << num_slack;
    cut = ConvertToLinearConstraint(dense_cut, cut_ub);
  }

  const double new_violation =
      ComputeActivity(cut, expanded_lp_solution_) - ToDouble(cut.ub);
  if (std::abs(violation - new_violation) >= 1e-4) {
    VLOG(1) << "Violation discrepancy after slack removal. "
            << " before = " << violation << " after = " << new_violation;
  }

  DivideByGCD(&cut);
  constraint_manager_.AddCut(cut, name, expanded_lp_solution_);
}

void LinearProgrammingConstraint::AddCGCuts() {
  CHECK_EQ(trail_->CurrentDecisionLevel(), 0);
  const RowIndex num_rows = lp_data_.num_constraints();
  for (RowIndex row(0); row < num_rows; ++row) {
    ColIndex basis_col = simplex_.GetBasis(row);
    const Fractional lp_value = GetVariableValueAtCpScale(basis_col);

    // TODO(user): We could just look at the diff with std::floor() in the hope
    // that when we are just under an integer, the exact computation below will
    // also be just under it.
    if (std::abs(lp_value - std::round(lp_value)) < 0.01) continue;

    // This is optional, but taking the negation allow to change the
    // fractionality to 1 - fractionality. And having a fractionality close
    // to 1.0 result in smaller coefficients in IntegerRoundingCut().
    //
    // TODO(user): Perform more experiments. Provide an option?
    const bool take_negation = lp_value - std::floor(lp_value) < 0.5;

    // If this variable is a slack, we ignore it. This is because the
    // corresponding row is not tight under the given lp values.
    if (basis_col >= integer_variables_.size()) continue;

    const glop::ScatteredRow& lambda = simplex_.GetUnitRowLeftInverse(row);
    glop::DenseColumn lp_multipliers(num_rows, 0.0);
    double magnitude = 0.0;
    int num_non_zeros = 0;
    for (RowIndex row(0); row < num_rows; ++row) {
      lp_multipliers[row] = lambda.values[glop::RowToColIndex(row)];
      if (lp_multipliers[row] == 0.0) continue;

      if (take_negation) lp_multipliers[row] = -lp_multipliers[row];

      // There should be no BASIC status, but they could be imprecision
      // in the GetUnitRowLeftInverse() code? not sure, so better be safe.
      const auto status = simplex_.GetConstraintStatus(row);
      if (status == glop::ConstraintStatus::BASIC) {
        VLOG(1) << "BASIC row not expected! " << lp_multipliers[row];
        lp_multipliers[row] = 0.0;
      }

      magnitude = std::max(magnitude, std::abs(lp_multipliers[row]));
      if (lp_multipliers[row] != 0.0) ++num_non_zeros;
    }
    if (num_non_zeros == 0) continue;

    Fractional scaling;

    // TODO(user): We use a lower value here otherwise we might run into
    // overflow while computing the cut. This should be fixable.
    const std::vector<std::pair<RowIndex, IntegerValue>> integer_multipliers =
        ScaleLpMultiplier(/*take_objective_into_account=*/false,
                          /*use_constraint_status=*/true, lp_multipliers,
                          &scaling, /*max_pow=*/52);
    AddCutFromConstraints("CG", integer_multipliers);
  }
}

void LinearProgrammingConstraint::AddMirCuts() {
  CHECK_EQ(trail_->CurrentDecisionLevel(), 0);
  const RowIndex num_rows = lp_data_.num_constraints();
  for (RowIndex row(0); row < num_rows; ++row) {
    const auto status = simplex_.GetConstraintStatus(row);
    if (status == glop::ConstraintStatus::BASIC) continue;
    if (status == glop::ConstraintStatus::FREE) continue;

    // TODO(user): Do not consider just one constraint, but take linear
    // combination of a small number of constraints. There is a lot of
    // literature on the possible heuristics here.
    std::vector<std::pair<RowIndex, IntegerValue>> integer_multipliers;
    integer_multipliers.push_back({row, IntegerValue(1)});
    AddCutFromConstraints("MIR1", integer_multipliers);
  }
}

void LinearProgrammingConstraint::UpdateSimplexIterationLimit(
    const int64 min_iter, const int64 max_iter) {
  if (sat_parameters_.linearization_level() < 2) return;
  const int64 num_degenerate_columns = CalculateDegeneracy();
  const int64 num_cols = simplex_.GetProblemNumCols().value();
  if (num_cols <= 0) {
    return;
  }
  CHECK_GT(num_cols, 0);
  const int64 decrease_factor = (10 * num_degenerate_columns) / num_cols;
  if (simplex_.GetProblemStatus() == glop::ProblemStatus::DUAL_FEASIBLE) {
    // We reached here probably because we predicted wrong. We use this as a
    // signal to increase the iterations or punish less for degeneracy compare
    // to the other part.
    if (is_degenerate_) {
      next_simplex_iter_ /= std::max(int64{1}, decrease_factor);
    } else {
      next_simplex_iter_ *= 2;
    }
  } else if (simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL) {
    if (is_degenerate_) {
      next_simplex_iter_ /= std::max(int64{1}, 2 * decrease_factor);
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
  if (sat_parameters_.use_exact_lp_reason()) {
    parameters.set_change_status_to_imprecise(false);
    parameters.set_primal_feasibility_tolerance(1e-7);
    parameters.set_dual_feasibility_tolerance(1e-7);
  }

  simplex_.SetParameters(parameters);
  simplex_.NotifyThatMatrixIsUnchangedForNextSolve();
  if (!SolveLp()) return true;

  // Add new constraints to the LP and resolve?
  //
  // TODO(user): We might want to do that more than once. Currently we rely on
  // this beeing called again on the next IncrementalPropagate() call, but that
  // might not always happen at level zero.
  if (simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL) {
    // First add any new lazy constraints or cuts that where previsouly
    // generated and are now cutting the current solution.
    if (constraint_manager_.ChangeLp(expanded_lp_solution_)) {
      CreateLpFromConstraintManager();
      if (!SolveLp()) return true;
    } else if (constraint_manager_.num_cuts() <
               sat_parameters_.max_num_cuts()) {
      const int old_num_cuts = constraint_manager_.num_cuts();

      // The "generic" cuts are currently part of this class as they are using
      // data from the current LP.
      //
      // TODO(user): Refactor so that they are just normal cut generators?
      if (trail_->CurrentDecisionLevel() == 0) {
        if (sat_parameters_.add_mir_cuts()) AddMirCuts();
        if (sat_parameters_.add_cg_cuts()) AddCGCuts();
      }

      // Try to add cuts.
      if (!cut_generators_.empty() &&
          (trail_->CurrentDecisionLevel() == 0 ||
           !sat_parameters_.only_add_cuts_at_level_zero())) {
        for (const CutGenerator& generator : cut_generators_) {
          generator.generate_cuts(expanded_lp_solution_, &constraint_manager_);
        }
      }

      if (constraint_manager_.num_cuts() > old_num_cuts &&
          constraint_manager_.ChangeLp(expanded_lp_solution_)) {
        CreateLpFromConstraintManager();
        const double old_obj = simplex_.GetObjectiveValue();
        if (!SolveLp()) return true;
        if (simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL) {
          VLOG(1) << "Cuts relaxation improvement " << old_obj << " -> "
                  << simplex_.GetObjectiveValue()
                  << " diff: " << simplex_.GetObjectiveValue() - old_obj
                  << " level: " << trail_->CurrentDecisionLevel();
        }
      } else {
        if (trail_->CurrentDecisionLevel() == 0) {
          lp_at_level_zero_is_final_ = true;
        }
      }
    }
  }

  // A dual-unbounded problem is infeasible. We use the dual ray reason.
  if (simplex_.GetProblemStatus() == glop::ProblemStatus::DUAL_UNBOUNDED) {
    if (sat_parameters_.use_exact_lp_reason()) {
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
    // Try to filter optimal objective value. Note that GetObjectiveValue()
    // already take care of the scaling so that it returns an objective in the
    // CP world.
    const double relaxed_optimal_objective = simplex_.GetObjectiveValue();
    const IntegerValue approximate_new_lb(
        static_cast<int64>(std::ceil(relaxed_optimal_objective - kCpEpsilon)));

    // TODO(user): Maybe do a bit less computation when we cannot propagate
    // anything.
    if (sat_parameters_.use_exact_lp_reason()) {
      if (!ExactLpReasonning()) return false;

      // Display when the inexact bound would have propagated more.
      const IntegerValue propagated_lb =
          integer_trail_->LowerBound(objective_cp_);
      if (approximate_new_lb > propagated_lb) {
        VLOG(2) << "LP objective [ " << ToDouble(propagated_lb) << ", "
                << ToDouble(integer_trail_->UpperBound(objective_cp_))
                << " ] approx_lb += "
                << ToDouble(approximate_new_lb - propagated_lb);
      }
    } else {
      FillReducedCostReasonIn(simplex_.GetReducedCosts(), &integer_reason_);
      const double objective_cp_ub =
          ToDouble(integer_trail_->UpperBound(objective_cp_));
      ReducedCostStrengtheningDeductions(objective_cp_ub -
                                         relaxed_optimal_objective);
      if (!deductions_.empty()) {
        deductions_reason_ = integer_reason_;
        deductions_reason_.push_back(
            integer_trail_->UpperBoundAsLiteral(objective_cp_));
      }

      // Push new objective lb.
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
      const int num_vars = integer_variables_.size();
      if (sum_cost_down_.size() < num_vars) {
        VLOG(1) << " LPReducedCostAverageBranching has #variables: "
                << num_vars;
        sum_cost_down_.resize(num_vars, 0.0);
        num_cost_down_.resize(num_vars, 0);
        sum_cost_up_.resize(num_vars, 0.0);
        num_cost_up_.resize(num_vars, 0);
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

  if (sat_parameters_.use_branching_in_lp() && objective_is_defined_ &&
      trail_->CurrentDecisionLevel() == 0 && !is_degenerate_ &&
      lp_solution_is_set_ && !lp_solution_is_integer_ &&
      sat_parameters_.linearization_level() >= 2 &&
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
      const double cost_i = GetCostFromAverageReducedCosts(i);
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
    const IntegerValue bound = coeff > 0 ? integer_trail_->LowerBound(var)
                                         : integer_trail_->UpperBound(var);
    if (!AddProductTo(bound, coeff, &lower_bound)) {
      return true;
    }
  }
  const int64 slack = CapAdd(lower_bound.value(), -constraint.ub.value());
  if (slack == kint64min || slack == kint64max) {
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
  // Compute the min/max possible partial sum.
  double sum_min = std::min(0.0, ToDouble(-constraint->ub));
  double sum_max = std::max(0.0, ToDouble(-constraint->ub));
  const int size = constraint->vars.size();
  for (int i = 0; i < size; ++i) {
    const IntegerVariable var = constraint->vars[i];
    const double coeff = ToDouble(constraint->coeffs[i]);
    const double prod1 = coeff * ToDouble(integer_trail_->LowerBound(var));
    const double prod2 = coeff * ToDouble(integer_trail_->UpperBound(var));
    sum_min += std::min(0.0, std::min(prod1, prod2));
    sum_max += std::max(0.0, std::max(prod1, prod2));
  }
  const double max_value = std::max(sum_max, -sum_min);

  const IntegerValue divisor(std::ceil(std::ldexp(max_value, -max_pow)));
  if (divisor <= 1) return;

  // To be correct, we need to shift all variable so that they are positive.
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
        absl::int128(integer_trail_->LowerBound(constraint->vars[i]).value());

    if (new_coeff == 0) continue;
    constraint->vars[new_size] = constraint->vars[i];
    constraint->coeffs[new_size] = new_coeff;
    ++new_size;
  }
  constraint->vars.resize(new_size);
  constraint->coeffs.resize(new_size);

  constraint->ub = IntegerValue(static_cast<int64>(
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

// TODO(user): Provide a sparse interface.
std::vector<std::pair<RowIndex, IntegerValue>>
LinearProgrammingConstraint::ScaleLpMultiplier(
    bool take_objective_into_account, bool use_constraint_status,
    const glop::DenseColumn& dense_lp_multipliers, Fractional* scaling,
    int max_pow) const {
  double max_sum = 0.0;
  std::vector<std::pair<RowIndex, Fractional>> cp_multipliers;
  for (RowIndex row(0); row < dense_lp_multipliers.size(); ++row) {
    const Fractional lp_multi = dense_lp_multipliers[row];
    if (lp_multi == 0.0) continue;

    // Remove trivial bad cases.
    if (!use_constraint_status) {
      if (lp_multi > 0.0 && integer_lp_[row].ub >= kMaxIntegerValue) {
        continue;
      }
      if (lp_multi < 0.0 && integer_lp_[row].lb <= kMinIntegerValue) {
        continue;
      }
    }

    const Fractional cp_multi = scaler_.UnscaleDualValue(row, lp_multi);
    cp_multipliers.push_back({row, cp_multi});
    max_sum += ToDouble(infinity_norms_[row]) * std::abs(cp_multi);
  }

  // This behave exactly like if we had another "objective" constraint with
  // an lp_multi of 1.0 and a cp_multi of 1.0.
  if (take_objective_into_account) {
    max_sum += ToDouble(objective_infinity_norm_);
  }

  // We want max_sum * scaling to be <= 2 ^ max_pow and fit on an int64.
  *scaling = std::ldexp(1, max_pow) / max_sum;

  // Scale the multipliers by *scaling.
  //
  // TODO(user): Maybe use int128 to avoid overflow?
  std::vector<std::pair<RowIndex, IntegerValue>> integer_multipliers;
  for (const auto entry : cp_multipliers) {
    const IntegerValue coeff(std::round(entry.second * (*scaling)));
    if (coeff != 0) integer_multipliers.push_back({entry.first, coeff});
  }
  return integer_multipliers;
}

bool LinearProgrammingConstraint::ComputeNewLinearConstraint(
    bool use_constraint_status,
    const std::vector<std::pair<RowIndex, IntegerValue>>& integer_multipliers,
    gtl::ITIVector<ColIndex, IntegerValue>* dense_terms,
    IntegerValue* upper_bound) const {
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
    if (!AddLinearExpressionMultiple(multiplier, integer_lp_[row].terms,
                                     dense_terms)) {
      return false;
    }

    // Update the upper bound.
    IntegerValue bound;
    if (use_constraint_status) {
      const auto status = simplex_.GetConstraintStatus(row);
      if (status == glop::ConstraintStatus::FIXED_VALUE ||
          status == glop::ConstraintStatus::AT_LOWER_BOUND) {
        bound = integer_lp_[row].lb;
      } else {
        CHECK_EQ(status, glop::ConstraintStatus::AT_UPPER_BOUND);
        bound = integer_lp_[row].ub;
      }
    } else {
      bound = multiplier > 0 ? integer_lp_[row].ub : integer_lp_[row].lb;
    }
    if (!AddProductTo(multiplier, bound, upper_bound)) return false;
  }

  return true;
}

// TODO(user): no need to update the multipliers.
void LinearProgrammingConstraint::AdjustNewLinearConstraint(
    std::vector<std::pair<glop::RowIndex, IntegerValue>>* integer_multipliers,
    gtl::ITIVector<ColIndex, IntegerValue>* dense_terms,
    IntegerValue* upper_bound) const {
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
      if (*upper_bound > 0 == row_bound > 0) {  // Same sign.
        positive_limit = std::min(positive_limit, limit1);
        negative_limit = std::min(negative_limit, limit2);
      } else {
        negative_limit = std::min(negative_limit, limit1);
        positive_limit = std::min(positive_limit, limit2);
      }
    }

    // If we add the row to the dense_terms, diff will indicate by how much
    // |upper_bound - ImpliedLB(dense_terms)| will change. That correspond to
    // increasing the multiplier by 1.
    IntegerValue positive_diff = row_bound;
    IntegerValue negative_diff = row_bound;

    // TODO(user): we could relax a bit some of the condition and allow a sign
    // change. It is just trickier to compute the diff when we allow such
    // changes.
    for (const auto entry : integer_lp_[row].terms) {
      const ColIndex col = entry.first;
      const IntegerValue coeff = entry.second;
      CHECK_NE(coeff, 0);

      // Moving a variable away from zero seems to improve the bound even
      // if it reduces the number of non-zero. Note that this is because of
      // this that positive_diff and negative_diff are not the same.
      const IntegerValue current = (*dense_terms)[col];
      if (current == 0) {
        const IntegerValue overflow_limit(
            FloorRatio(kMaxWantedCoeff, IntTypeAbs(coeff)));
        positive_limit = std::min(positive_limit, overflow_limit);
        negative_limit = std::min(negative_limit, overflow_limit);
        const IntegerVariable var = integer_variables_[col.value()];
        if (coeff > 0) {
          positive_diff -= coeff * integer_trail_->LowerBound(var);
          negative_diff -= coeff * integer_trail_->UpperBound(var);
        } else {
          positive_diff -= coeff * integer_trail_->UpperBound(var);
          negative_diff -= coeff * integer_trail_->LowerBound(var);
        }
        continue;
      }

      // We don't want to change the sign of current or to have an overflow.
      IntegerValue before_sign_change(
          FloorRatio(IntTypeAbs(current), IntTypeAbs(coeff)));

      // If the variable is fixed, we don't actually care about changing the
      // sign.
      const IntegerVariable var = integer_variables_[col.value()];
      if (integer_trail_->LowerBound(var) == integer_trail_->UpperBound(var)) {
        before_sign_change = kMaxWantedCoeff;
      }

      const IntegerValue overflow_limit(
          FloorRatio(kMaxWantedCoeff - IntTypeAbs(current), IntTypeAbs(coeff)));
      if (current > 0 == coeff > 0) {  // Same sign.
        negative_limit = std::min(negative_limit, before_sign_change);
        positive_limit = std::min(positive_limit, overflow_limit);
      } else {
        negative_limit = std::min(negative_limit, overflow_limit);
        positive_limit = std::min(positive_limit, before_sign_change);
      }

      // This is how diff change.
      const IntegerValue implied = current > 0
                                       ? integer_trail_->LowerBound(var)
                                       : integer_trail_->UpperBound(var);

      positive_diff -= coeff * implied;
      negative_diff -= coeff * implied;
    }

    // Only add a multiple of this row if it tighten the final constraint.
    IntegerValue to_add(0);
    if (positive_diff < 0 && positive_limit > 0) {
      to_add = positive_limit;
    }
    if (negative_diff > 0 && negative_limit > 0) {
      // Pick this if it is better than the positive sign.
      if (to_add == 0 || IntTypeAbs(negative_limit * negative_diff) >
                             IntTypeAbs(positive_limit * positive_diff)) {
        to_add = -negative_limit;
      }
    }
    if (to_add != 0) {
      term.second += to_add;
      *upper_bound += to_add * row_bound;
      for (const auto entry : integer_lp_[row].terms) {
        const ColIndex col = entry.first;
        const IntegerValue coeff = entry.second;
        (*dense_terms)[col] += to_add * coeff;
      }
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
  glop::DenseColumn lp_multipliers(num_rows);
  for (RowIndex row(0); row < num_rows; ++row) {
    lp_multipliers[row] = -simplex_.GetDualValue(row);
  }

  Fractional scaling;
  std::vector<std::pair<RowIndex, IntegerValue>> integer_multipliers =
      ScaleLpMultiplier(/*take_objective_into_account=*/true,
                        /*use_constraint_status=*/false, lp_multipliers,
                        &scaling);

  gtl::ITIVector<ColIndex, IntegerValue> reduced_costs;
  IntegerValue rc_ub;
  if (!ComputeNewLinearConstraint(
          /*use_constraint_status=*/false, integer_multipliers, &reduced_costs,
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
  CHECK(AddLinearExpressionMultiple(obj_scale, integer_objective_,
                                    &reduced_costs));

  AdjustNewLinearConstraint(&integer_multipliers, &reduced_costs, &rc_ub);

  // Create the IntegerSumLE that will allow to propagate the objective and more
  // generally do the reduced cost fixing.
  LinearConstraint new_constraint =
      ConvertToLinearConstraint(reduced_costs, rc_ub);
  new_constraint.vars.push_back(objective_cp_);
  new_constraint.coeffs.push_back(-obj_scale);
  DivideByGCD(&new_constraint);
  PreventOverflow(&new_constraint);
  CHECK(!PossibleOverflow(new_constraint));

  IntegerSumLE* cp_constraint =
      new IntegerSumLE({}, new_constraint.vars, new_constraint.coeffs,
                       new_constraint.ub, model_);
  optimal_constraints_.emplace_back(cp_constraint);
  rev_optimal_constraints_size_ = optimal_constraints_.size();
  return cp_constraint->Propagate();
}

bool LinearProgrammingConstraint::FillExactDualRayReason() {
  Fractional scaling;
  std::vector<std::pair<RowIndex, IntegerValue>> integer_multipliers =
      ScaleLpMultiplier(/*take_objective_into_account=*/false,
                        /*use_constraint_status=*/false, simplex_.GetDualRay(),
                        &scaling);

  gtl::ITIVector<ColIndex, IntegerValue> dense_new_constraint;
  IntegerValue new_constraint_ub;
  if (!ComputeNewLinearConstraint(
          /*use_constraint_status=*/false, integer_multipliers,
          &dense_new_constraint, &new_constraint_ub)) {
    VLOG(1) << "Isse while computing the exact dual ray reason. Aborting.";
    return false;
  }

  AdjustNewLinearConstraint(&integer_multipliers, &dense_new_constraint,
                            &new_constraint_ub);

  LinearConstraint new_constraint =
      ConvertToLinearConstraint(dense_new_constraint, new_constraint_ub);
  DivideByGCD(&new_constraint);
  PreventOverflow(&new_constraint);
  CHECK(!PossibleOverflow(new_constraint));

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

int64 LinearProgrammingConstraint::CalculateDegeneracy() {
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
  const int64 num_cols = simplex_.GetProblemNumCols().value();
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
void AddOutgoingCut(int num_nodes, int subset_size,
                    const std::vector<bool>& in_subset,
                    const std::vector<int>& tails,
                    const std::vector<int>& heads,
                    const std::vector<Literal>& literals,
                    const std::vector<double>& literal_lp_values,
                    int64 rhs_lower_bound,
                    const gtl::ITIVector<IntegerVariable, double>& lp_values,
                    LinearConstraintManager* manager, Model* model) {
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
  if (num_optional_nodes_in + num_optional_nodes_out > 0) {
    CHECK_EQ(rhs_lower_bound, 1);
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
    const gtl::ITIVector<IntegerVariable, double>& lp_values,
    absl::Span<const int64> demands, int64 capacity,
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
  int64 total_demands = 0;
  if (!demands.empty()) {
    for (const int64 demand : demands) total_demands += demand;
  }

  // Process each subsets and add any violated cut.
  CHECK_EQ(pre_order.size(), num_nodes);
  std::vector<bool> in_subset(num_nodes, false);
  for (const absl::Span<const int> subset : subsets) {
    CHECK_GT(subset.size(), 1);
    CHECK_LT(subset.size(), num_nodes);

    // These fields will be left untouched if demands.empty().
    bool contain_depot = false;
    int64 subset_demand = 0;

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
    int64 min_outgoing_flow = 1;
    if (!demands.empty()) {
      min_outgoing_flow =
          contain_depot
              ? (total_demands - subset_demand + capacity - 1) / capacity
              : (subset_demand + capacity - 1) / capacity;
    }

    // We still need to serve nodes with a demand of zero, and in the corner
    // case where all node in subset have a zero demand, the formula above
    // result in a min_outgoing_flow of zero.
    min_outgoing_flow = std::max(min_outgoing_flow, int64{1});

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
          const gtl::ITIVector<IntegerVariable, double>& lp_values,
          LinearConstraintManager* manager) {
        SeparateSubtourInequalities(
            num_nodes, tails, heads, literals, lp_values,
            /*demands=*/{}, /*capacity=*/0, manager, model);
      };
  return result;
}

CutGenerator CreateCVRPCutGenerator(int num_nodes,
                                    const std::vector<int>& tails,
                                    const std::vector<int>& heads,
                                    const std::vector<Literal>& literals,
                                    const std::vector<int64>& demands,
                                    int64 capacity, Model* model) {
  CutGenerator result;
  result.vars = GetAssociatedVariables(literals, model);
  result.generate_cuts =
      [num_nodes, tails, heads, demands, capacity, literals, model](
          const gtl::ITIVector<IntegerVariable, double>& lp_values,
          LinearConstraintManager* manager) {
        SeparateSubtourInequalities(num_nodes, tails, heads, literals,
                                    lp_values, demands, capacity, manager,
                                    model);
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
  const int num_vars = integer_variables_.size();
  if (sum_cost_down_.size() < num_vars) {
    VLOG(1) << " LPReducedCostAverageBranching has #variables: " << num_vars;
    sum_cost_down_.resize(num_vars, 0.0);
    num_cost_down_.resize(num_vars, 0);
    sum_cost_up_.resize(num_vars, 0.0);
    num_cost_up_.resize(num_vars, 0);
  }

  return [this]() { return this->LPReducedCostAverageDecision(); };
}

double LinearProgrammingConstraint::GetCostFromAverageReducedCosts(
    int position) {
  // If only one direction exist, we takes its value divided by 2, so that
  // such variable should have a smaller cost than the min of the two side
  // except if one direction have a really high reduced costs.
  if (num_cost_down_[position] > 0 && num_cost_up_[position] > 0) {
    return std::min(sum_cost_down_[position] / num_cost_down_[position],
                    sum_cost_up_[position] / num_cost_up_[position]);
  } else {
    const double divisor = num_cost_down_[position] + num_cost_up_[position];
    if (divisor != 0) {
      return 0.5 * (sum_cost_down_[position] + sum_cost_up_[position]) /
             divisor;
    }
  }
  return 0.0;
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

    const double cost_i = GetCostFromAverageReducedCosts(i);

    if (selected_index == -1 || cost_i > best_cost) {
      best_cost = cost_i;
      selected_index = i;
    }
  }

  if (selected_index == -1) return kNoLiteralIndex;
  const IntegerVariable var = integer_variables_[selected_index];

  // If ceil(value) is current upper bound, try var == upper bound first.
  // Guarding with >= prevents numerical problems.
  // With 0/1 variables, this will tend to try setting to 1 first,
  // which produces more shallow trees.
  const IntegerValue ub = integer_trail_->UpperBound(var);
  const IntegerValue value_ceil(
      std::ceil(this->GetSolutionValue(var) - kCpEpsilon));
  if (value_ceil >= ub) {
    const Literal result = integer_encoder_->GetOrCreateAssociatedLiteral(
        IntegerLiteral::GreaterOrEqual(var, ub));
    CHECK(!trail_->Assignment().LiteralIsAssigned(result));
    return result.Index();
  }

  // If floor(value) is current lower bound, try var == lower bound first.
  // Guarding with <= prevents numerical problems.
  const IntegerValue lb = integer_trail_->LowerBound(var);
  const IntegerValue value_floor(
      std::floor(this->GetSolutionValue(var) + kCpEpsilon));
  if (value_floor <= lb) {
    const Literal result = integer_encoder_->GetOrCreateAssociatedLiteral(
        IntegerLiteral::LowerOrEqual(var, lb));
    CHECK(!trail_->Assignment().LiteralIsAssigned(result))
        << " " << lb << " " << ub;
    return result.Index();
  }

  // Here lb < value_floor <= value_ceil < ub.
  // Try the most promising split between var <= floor or var >= ceil.
  if (sum_cost_down_[selected_index] / num_cost_down_[selected_index] <
      sum_cost_up_[selected_index] / num_cost_up_[selected_index]) {
    const Literal result = integer_encoder_->GetOrCreateAssociatedLiteral(
        IntegerLiteral::LowerOrEqual(var, value_floor));
    CHECK(!trail_->Assignment().LiteralIsAssigned(result));
    return result.Index();
  } else {
    const Literal result = integer_encoder_->GetOrCreateAssociatedLiteral(
        IntegerLiteral::GreaterOrEqual(var, value_ceil));
    CHECK(!trail_->Assignment().LiteralIsAssigned(result));
    return result.Index();
  }
}

}  // namespace sat
}  // namespace operations_research

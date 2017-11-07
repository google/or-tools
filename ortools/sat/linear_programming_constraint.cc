// Copyright 2010-2017 Google
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

#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/graph/strongly_connected_components.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/base/map_util.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/status.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

const double LinearProgrammingConstraint::kEpsilon = 1e-6;

LinearProgrammingConstraint::LinearProgrammingConstraint(Model* model)
    : sat_parameters_(model->GetOrCreate<SatSolver>()->parameters()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      trail_(model->GetOrCreate<Trail>()),
      dispatcher_(model->GetOrCreate<LinearProgrammingDispatcher>()) {
  // TODO(user): Find a way to make GetOrCreate<TimeLimit>() construct it by
  // default.
  time_limit_ = model->Mutable<TimeLimit>();
  if (time_limit_ == nullptr) {
    model->SetSingleton(TimeLimit::Infinite());
    time_limit_ = model->Mutable<TimeLimit>();
  }

  // Tweak the default parameters to make the solve incremental.
  glop::GlopParameters parameters;
  parameters.set_use_dual_simplex(true);
  simplex_.SetParameters(parameters);
}

LinearProgrammingConstraint::ConstraintIndex
LinearProgrammingConstraint::CreateNewConstraint(double lb, double ub) {
  DCHECK(!lp_constraint_is_registered_);
  const ConstraintIndex ct = lp_data_.CreateNewConstraint();
  lp_data_.SetConstraintBounds(ct, lb, ub);
  return ct;
}

glop::ColIndex LinearProgrammingConstraint::GetOrCreateMirrorVariable(
    IntegerVariable positive_variable) {
  DCHECK(VariableIsPositive(positive_variable));
  if (!ContainsKey(integer_variable_to_index_, positive_variable)) {
    integer_variable_to_index_[positive_variable] = integer_variables_.size();
    integer_variables_.push_back(positive_variable);
    mirror_lp_variables_.push_back(lp_data_.CreateNewVariable());
    lp_solution_.push_back(std::numeric_limits<double>::infinity());
    lp_reduced_cost_.push_back(0.0);
    (*dispatcher_)[positive_variable] = this;
  }
  return mirror_lp_variables_[integer_variable_to_index_[positive_variable]];
}

void LinearProgrammingConstraint::SetCoefficient(ConstraintIndex ct,
                                                 IntegerVariable ivar,
                                                 double coefficient) {
  CHECK(!lp_constraint_is_registered_);
  IntegerVariable pos_var = VariableIsPositive(ivar) ? ivar : NegationOf(ivar);
  if (ivar != pos_var) coefficient *= -1.0;
  glop::ColIndex cvar = GetOrCreateMirrorVariable(pos_var);
  lp_data_.SetCoefficient(ct, cvar, coefficient);
}

void LinearProgrammingConstraint::SetObjectiveCoefficient(IntegerVariable ivar,
                                                          double coeff) {
  CHECK(!lp_constraint_is_registered_);
  objective_is_defined_ = true;
  IntegerVariable pos_var = VariableIsPositive(ivar) ? ivar : NegationOf(ivar);
  if (ivar != pos_var) coeff *= -1.0;
  objective_lp_.push_back(
      std::make_pair(GetOrCreateMirrorVariable(pos_var), coeff));
}

void LinearProgrammingConstraint::RegisterWith(GenericLiteralWatcher* watcher) {
  DCHECK(!lp_constraint_is_registered_);
  lp_constraint_is_registered_ = true;

  // Note that the order is important so that the lp objective is exactly the
  // same as the cp objective after scaling by the factor stored in lp_data_.
  if (objective_is_defined_) {
    for (const auto& var_coeff : objective_lp_) {
      lp_data_.SetObjectiveCoefficient(var_coeff.first, var_coeff.second);
    }
  }
  lp_data_.Scale(&scaler_);
  lp_data_.ScaleObjective();
  lp_data_.AddSlackVariablesWhereNecessary(false);

  const int watcher_id = watcher->Register(this);
  const int num_vars = integer_variables_.size();
  for (int i = 0; i < num_vars; i++) {
    watcher->WatchIntegerVariable(integer_variables_[i], watcher_id, i);
  }
  if (objective_is_defined_) {
    watcher->WatchUpperBound(objective_cp_, watcher_id);
  }
  watcher->SetPropagatorPriority(watcher_id, 2);
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
  for (const int index : watch_indices) {
    const double lb = static_cast<double>(
        integer_trail_->LowerBound(integer_variables_[index]).value());
    const double ub = static_cast<double>(
        integer_trail_->UpperBound(integer_variables_[index]).value());
    const double value = lp_solution_[index];
    if (value < lb - kEpsilon || value > ub + kEpsilon) return Propagate();
  }
  return true;
}

glop::Fractional LinearProgrammingConstraint::GetVariableValueAtCpScale(
    glop::ColIndex var) {
  return simplex_.GetVariableValue(var) / scaler_.col_scale(var);
}

double LinearProgrammingConstraint::GetSolutionValue(
    IntegerVariable variable) const {
  return lp_solution_[FindOrDie(integer_variable_to_index_, variable)];
}

double LinearProgrammingConstraint::GetSolutionReducedCost(
    IntegerVariable variable) const {
  return lp_reduced_cost_[FindOrDie(integer_variable_to_index_, variable)];
}

bool LinearProgrammingConstraint::Propagate() {
  // Copy CP state into LP state.
  const int num_vars = integer_variables_.size();
  for (int i = 0; i < num_vars; i++) {
    const IntegerVariable cp_var = integer_variables_[i];
    const double lb =
        static_cast<double>(integer_trail_->LowerBound(cp_var).value());
    const double ub =
        static_cast<double>(integer_trail_->UpperBound(cp_var).value());
    const double factor = scaler_.col_scale(mirror_lp_variables_[i]);
    lp_data_.SetVariableBounds(mirror_lp_variables_[i], lb * factor,
                               ub * factor);
  }

  glop::GlopParameters parameters = simplex_.GetParameters();

  if (objective_is_defined_) {
    // We put a limit on the dual objective since there is no point increasing
    // it past our current objective upper-bound (we will already fail as soon
    // as we pass it). Note that this limit is properly transformed using the
    // objective scaling factor and offset stored in lp_data_.
    //
    // Note that we use a bigger epsilon here to be sure that if we abort
    // because of this, we will report a conflict.
    parameters.set_objective_upper_limit(static_cast<double>(
        integer_trail_->UpperBound(objective_cp_).value() + 100.0 * kEpsilon));
  }

  // Put an iteration limit on the work we do in the simplex for this call. Note
  // that because we are "incremental", even if we don't solve it this time we
  // will make progress towards a solve in the lower node of the tree search.
  //
  // TODO(user): Put more at the root, and less afterwards?
  parameters.set_max_number_of_iterations(500);

  simplex_.SetParameters(parameters);
  simplex_.NotifyThatMatrixIsUnchangedForNextSolve();
  const auto status = simplex_.Solve(lp_data_, time_limit_);
  CHECK(status.ok()) << "LinearProgrammingConstraint encountered an error: "
                     << status.error_message();

  // Add cuts and resolve.
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
          const int index = integer_variable_to_index_[var];
          local_solution.push_back(
              GetVariableValueAtCpScale(mirror_lp_variables_[index]));
        } else {
          const int index = integer_variable_to_index_[NegationOf(var)];
          local_solution.push_back(
              -GetVariableValueAtCpScale(mirror_lp_variables_[index]));
        }
      }
      std::vector<LinearConstraint> cuts =
          generator.generate_cuts(local_solution);
      if (cuts.empty()) continue;

      // Add the cuts to the LP!
      lp_data_.DeleteSlackVariables();
      for (const LinearConstraint& cut : cuts) {
        ++num_new_cuts;
        const glop::RowIndex row = lp_data_.CreateNewConstraint();
        lp_data_.SetConstraintBounds(row, cut.lb, cut.ub);
        for (int i = 0; i < cut.vars.size(); ++i) {
          const glop::ColIndex col = GetOrCreateMirrorVariable(cut.vars[i]);
          lp_data_.SetCoefficient(row, col,
                                  cut.coeffs[i] / scaler_.col_scale(col));
        }
      }
    }

    // Resolve if we added some cuts.
    if (num_new_cuts > 0) {
      num_cuts_ += num_new_cuts;
      VLOG(1) << "#cuts " << num_cuts_;
      lp_data_.NotifyThatColumnsAreClean();
      lp_data_.AddSlackVariablesWhereNecessary(false);
      const auto status = simplex_.Solve(lp_data_, time_limit_);
      CHECK(status.ok()) << "LinearProgrammingConstraint encountered an error: "
                         << status.error_message();
    }
  }

  // A dual-unbounded problem is infeasible. We use the dual ray reason.
  if (simplex_.GetProblemStatus() == glop::ProblemStatus::DUAL_UNBOUNDED) {
    FillDualRayReason();
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
    const IntegerValue old_lb = integer_trail_->LowerBound(objective_cp_);
    const IntegerValue new_lb(
        static_cast<int64>(std::ceil(relaxed_optimal_objective - kEpsilon)));
    if (old_lb < new_lb) {
      FillReducedCostsReason();
      const IntegerLiteral deduction =
          IntegerLiteral::GreaterOrEqual(objective_cp_, new_lb);
      if (!integer_trail_->Enqueue(deduction, {}, integer_reason_)) {
        return false;
      }
    }

    // Reduced cost strengthening.
    const double objective_cp_ub =
        static_cast<double>(integer_trail_->UpperBound(objective_cp_).value());
    ReducedCostStrengtheningDeductions(objective_cp_ub -
                                       relaxed_optimal_objective);
    if (!deductions_.empty()) {
      FillReducedCostsReason();
      integer_reason_.push_back(
          integer_trail_->UpperBoundAsLiteral(objective_cp_));
      const int trail_index_with_same_reason = integer_trail_->Index();
      for (const IntegerLiteral deduction : deductions_) {
        if (!integer_trail_->Enqueue(deduction, {}, integer_reason_,
                                     trail_index_with_same_reason)) {
          return false;
        }
      }
    }
  }

  // Copy current LP solution.
  if (simplex_.GetProblemStatus() == glop::ProblemStatus::OPTIMAL) {
    const double objective_scale = lp_data_.objective_scaling_factor();
    for (int i = 0; i < num_vars; i++) {
      lp_solution_[i] = GetVariableValueAtCpScale(mirror_lp_variables_[i]);
      lp_reduced_cost_[i] = simplex_.GetReducedCost(mirror_lp_variables_[i]) *
                            scaler_.col_scale(mirror_lp_variables_[i]) *
                            objective_scale;
    }
  }
  return true;
}

void LinearProgrammingConstraint::FillReducedCostsReason() {
  integer_reason_.clear();
  const int num_vars = integer_variables_.size();
  for (int i = 0; i < num_vars; i++) {
    // TODO(user): try to extend the bounds that are put in the
    // explanation of feasibility: can we compute bounds of variables for which
    // the objective would still not be low/high enough for the problem to be
    // feasible? If the violation minimum is 10 and a variable has rc 1,
    // then decreasing it by 9 would still leave the problem infeasible.
    // Using this could allow to generalize some explanations.
    const double rc = simplex_.GetReducedCost(mirror_lp_variables_[i]);
    if (rc > kEpsilon) {
      integer_reason_.push_back(
          integer_trail_->LowerBoundAsLiteral(integer_variables_[i]));
    } else if (rc < -kEpsilon) {
      integer_reason_.push_back(
          integer_trail_->UpperBoundAsLiteral(integer_variables_[i]));
    }
  }
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
    const double rc =
        simplex_.GetDualRayRowCombination()[mirror_lp_variables_[i]];
    if (rc > kEpsilon) {
      integer_reason_.push_back(
          integer_trail_->LowerBoundAsLiteral(integer_variables_[i]));
    } else if (rc < -kEpsilon) {
      integer_reason_.push_back(
          integer_trail_->UpperBoundAsLiteral(integer_variables_[i]));
    }
  }
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
    const glop::ColIndex lp_var = mirror_lp_variables_[i];
    const double rc = simplex_.GetReducedCost(lp_var);
    const double value = simplex_.GetVariableValue(lp_var);

    if (rc == 0.0) continue;
    const double lp_other_bound = value + lp_objective_delta / rc;
    const double cp_other_bound = lp_other_bound / scaler_.col_scale(lp_var);

    if (rc > kEpsilon) {
      const double ub =
          static_cast<double>(integer_trail_->UpperBound(cp_var).value());
      const double new_ub = std::floor(cp_other_bound + kEpsilon);
      if (new_ub < ub) {
        const IntegerValue new_ub_int(static_cast<IntegerValue>(new_ub));
        deductions_.push_back(IntegerLiteral::LowerOrEqual(cp_var, new_ub_int));
      }
    } else if (rc < -kEpsilon) {
      const double lb =
          static_cast<double>(integer_trail_->LowerBound(cp_var).value());
      const double new_lb = std::ceil(cp_other_bound - kEpsilon);
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
    const std::vector<int>& s, const std::vector<int>& tails,
    const std::vector<int>& heads, const std::vector<IntegerVariable>& vars,
    const std::vector<double>& lp_solution, int64 rhs_lower_bound,
    std::vector<LinearConstraint>* cuts) {
  LinearConstraint incoming;
  LinearConstraint outgoing;
  double sum_incoming = 0.0;
  double sum_outgoing = 0.0;
  incoming.lb = outgoing.lb = rhs_lower_bound;
  incoming.ub = outgoing.ub = std::numeric_limits<double>::infinity();
  const std::set<int> subset(s.begin(), s.end());
  for (int i = 0; i < tails.size(); ++i) {
    const bool out = ContainsKey(subset, tails[i]);
    const bool in = ContainsKey(subset, heads[i]);
    if (out && in) continue;
    if (out) {
      sum_outgoing += lp_solution[i];
      outgoing.vars.push_back(vars[i]);
      outgoing.coeffs.push_back(1.0);
    }
    if (in) {
      sum_incoming += lp_solution[i];
      incoming.vars.push_back(vars[i]);
      incoming.coeffs.push_back(1.0);
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
      AddIncomingAndOutgoingCutsIfNeeded(component, tails, heads, vars,
                                         lp_solution, /*rhs_lower_bound=*/1,
                                         &cuts);

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
          component, tails, heads, vars, lp_solution,
          /*rhs_lower_bound=*/min_num_vehicles, &cuts);

      // In this case, the cuts for each component are the same.
      if (components.size() == 2) break;
    }
    return cuts;
  };
  return result;
}

std::function<LiteralIndex()> HeuristicLPMostInfeasibleBinary(Model* model) {
  // Gather all 0-1 variables that appear in some LP.
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  LinearProgrammingDispatcher* dispatcher =
      model->GetOrCreate<LinearProgrammingDispatcher>();
  std::vector<IntegerVariable> variables;
  for (const auto entry : *dispatcher) {
    IntegerVariable var = entry.first;
    if (integer_trail->LowerBound(var) == 0 &&
        integer_trail->UpperBound(var) == 1) {
      variables.push_back(var);
    }
  }
  std::sort(variables.begin(), variables.end());

  LOG(INFO) << "HeuristicLPMostInfeasibleBinary has " << variables.size()
            << " variables.";

  IntegerEncoder* integer_encoder = model->GetOrCreate<IntegerEncoder>();
  SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
  return [variables, dispatcher, integer_trail, integer_encoder, sat_solver]() {
    const double kEpsilon = 1e-6;
    // Find most fractional value.
    IntegerVariable fractional_var = kNoIntegerVariable;
    double fractional_distance_best = -1.0;
    for (const IntegerVariable var : variables) {
      // Check variable is not ignored and unfixed.
      if (integer_trail->IsCurrentlyIgnored(var)) continue;
      const IntegerValue lb = integer_trail->LowerBound(var);
      const IntegerValue ub = integer_trail->UpperBound(var);
      if (lb == ub) continue;

      // Check variable's support is fractional.
      LinearProgrammingConstraint* lp = (*dispatcher)[var];
      const double lp_value = lp->GetSolutionValue(var);
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

std::function<LiteralIndex()> HeuristicLPPseudoCostBinary(Model* model) {
  // Gather all 0-1 variables that appear in some LP.
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  LinearProgrammingDispatcher* dispatcher =
      model->GetOrCreate<LinearProgrammingDispatcher>();
  std::vector<IntegerVariable> variables;
  for (const auto entry : *dispatcher) {
    IntegerVariable var = entry.first;
    if (integer_trail->LowerBound(var) == 0 &&
        integer_trail->UpperBound(var) == 1) {
      variables.push_back(var);
    }
  }
  std::sort(variables.begin(), variables.end());

  LOG(INFO) << "HeuristicLPPseudoCostBinary has " << variables.size()
            << " variables.";

  // Store average of reduced cost from 1 to 0. The best heuristic only sets
  // variables to one and cares about cost to zero, even though classic
  // pseudocost will use max_var std::min(cost_to_one[var], cost_to_zero[var]).
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
      if (integer_trail->LowerBound(var) == integer_trail->UpperBound(var))
        continue;

      LinearProgrammingConstraint* lp = (*dispatcher)[var];
      const double rc = lp->GetSolutionReducedCost(var);
      // Skip reduced costs that are nonzero because of numerical issues.
      if (std::abs(rc) < kEpsilon) continue;

      const double value = std::round(lp->GetSolutionValue(var));
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
      if (integer_trail->LowerBound(var) == integer_trail->UpperBound(var)) {
        continue;
      }

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

}  // namespace sat
}  // namespace operations_research

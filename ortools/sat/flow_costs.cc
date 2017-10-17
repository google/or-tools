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

#include "ortools/sat/flow_costs.h"

#include <cmath>
#include <memory>

#include "ortools/base/logging.h"
#include "ortools/base/int_type.h"

namespace operations_research {
namespace sat {

const double FlowCosts::kEpsilon = 1e-6;

FlowCosts::FlowCosts(
    const std::vector<IntegerVariable>& demands,
    const std::vector<IntegerVariable>& flow, const std::vector<int>& tails,
    const std::vector<int>& heads,
    const std::vector<std::vector<int>>& arc_costs_per_cost_type,
    const std::vector<IntegerVariable>& total_costs_per_cost_type,
    IntegerTrail* integer_trail)
    : num_nodes_(demands.size()),
      num_arcs_(flow.size()),
      num_costs_(total_costs_per_cost_type.size()),
      num_vars_(num_nodes_ + num_arcs_ + num_costs_),
      demands_cp_(demands),
      flow_cp_(flow),
      total_costs_per_cost_type_cp_(total_costs_per_cost_type),
      integer_trail_(integer_trail),
      lp_solver_("LPRelaxation", MPSolver::GLOP_LINEAR_PROGRAMMING) {
  DCHECK_EQ(num_arcs_, tails.size());
  DCHECK_EQ(num_arcs_, heads.size());

  for (int arc = 0; arc < num_arcs_; arc++) {
    DCHECK_LE(0, tails[arc]);
    DCHECK_LT(tails[arc], num_nodes_);
    DCHECK_LE(0, heads[arc]);
    DCHECK_LT(heads[arc], num_nodes_);
  }

  DCHECK_EQ(num_costs_, arc_costs_per_cost_type.size());
  for (int i = 0; i < num_costs_; i++) {
    DCHECK_EQ(num_arcs_, arc_costs_per_cost_type[i].size());
  }

  const double infinity = MPSolver::infinity();

  // TODO(user): here we use violation_ as the maximum violation of all
  // constraints, try to introduce the sum of all violations instead.
  violation_ = lp_solver_.MakeNumVar(0.0, infinity, "violation");

  // Make LP variables for the flow, and set flow conservation constraints.
  // sum_{node, node'} flow[node->node'] + demand[node] =
  // sum_{node', node} flow[node'->node]
  lp_solver_.MakeNumVarArray(num_arcs_, 0.0, 0.0, "flow", &flow_lp_);
  lp_solver_.MakeNumVarArray(num_nodes_, 0.0, 0.0, "demand", &demands_lp_);

  std::vector<MPConstraint*> flow_conservation_pos;
  std::vector<MPConstraint*> flow_conservation_neg;

  for (int node = 0; node < num_nodes_; node++) {
    MPConstraint* ct_pos = lp_solver_.MakeRowConstraint(0.0, infinity);
    MPConstraint* ct_neg = lp_solver_.MakeRowConstraint(-infinity, 0.0);

    ct_pos->SetCoefficient(violation_, 1);
    ct_neg->SetCoefficient(violation_, -1);

    ct_pos->SetCoefficient(demands_lp_[node], 1);
    ct_neg->SetCoefficient(demands_lp_[node], 1);

    flow_conservation_pos.push_back(ct_pos);
    flow_conservation_neg.push_back(ct_neg);
  }

  for (int arc = 0; arc < num_arcs_; arc++) {
    flow_conservation_pos[tails[arc]]->SetCoefficient(flow_lp_[arc], -1);
    flow_conservation_neg[tails[arc]]->SetCoefficient(flow_lp_[arc], -1);

    flow_conservation_pos[heads[arc]]->SetCoefficient(flow_lp_[arc], 1);
    flow_conservation_neg[heads[arc]]->SetCoefficient(flow_lp_[arc], 1);
  }

  // Make cost variables, link them to arc_costs_per_cost_type and flow values.
  // total_costs_per_cost_type[c] = sum_{arc} flow[arc] *
  // arc_costs_per_cost_type[c][arc]
  lp_solver_.MakeNumVarArray(num_costs_, -infinity, infinity,
                             "total_costs_per_cost_type",
                             &total_costs_per_cost_type_lp_);

  for (int c = 0; c < num_costs_; c++) {
    MPConstraint* ct_pos = lp_solver_.MakeRowConstraint(0.0, infinity);
    MPConstraint* ct_neg = lp_solver_.MakeRowConstraint(-infinity, 0.0);

    ct_pos->SetCoefficient(violation_, 1);
    ct_neg->SetCoefficient(violation_, -1);

    for (int arc = 0; arc < num_arcs_; arc++) {
      ct_pos->SetCoefficient(flow_lp_[arc], arc_costs_per_cost_type[c][arc]);
      ct_neg->SetCoefficient(flow_lp_[arc], arc_costs_per_cost_type[c][arc]);
    }

    ct_pos->SetCoefficient(total_costs_per_cost_type_lp_[c], -1);
    ct_neg->SetCoefficient(total_costs_per_cost_type_lp_[c], -1);
  }

  // Objective is to minimize lb violation.
  objective_lp_ = lp_solver_.MutableObjective();

  // Put all variables in vectors,
  // more practical for registering, copying state from CP to LP,
  // generating explanations, and reduced cost strengthening.
  for (int node = 0; node < num_nodes_; node++) {
    all_cp_variables_.push_back(demands_cp_[node]);
    all_lp_variables_.push_back(demands_lp_[node]);
  }
  for (int arc = 0; arc < num_arcs_; arc++) {
    all_cp_variables_.push_back(flow_cp_[arc]);
    all_lp_variables_.push_back(flow_lp_[arc]);
  }
  for (int c = 0; c < num_costs_; c++) {
    all_cp_variables_.push_back(total_costs_per_cost_type_cp_[c]);
    all_lp_variables_.push_back(total_costs_per_cost_type_lp_[c]);
  }

  lp_solution_.resize(num_vars_);
  for (int i = 0; i < num_vars_; i++) {
    lp_solution_[i] = -infinity;
  }
}

void FlowCosts::RegisterWith(GenericLiteralWatcher* watcher) {
  const int watch_id = watcher->Register(this);
  for (int i = 0; i < num_vars_; i++) {
    watcher->WatchIntegerVariable(all_cp_variables_[i], watch_id, i);
  }
}

// Check whether the change breaks the current LP support.
// Call Propagate() only if it does.
bool FlowCosts::IncrementalPropagate(const std::vector<int>& watch_indices) {
  for (const int index : watch_indices) {
    // TODO(user): check that casting does not lose precision.
    const double lb = static_cast<double>(
        integer_trail_->LowerBound(all_cp_variables_[index]).value());
    const double ub = static_cast<double>(
        integer_trail_->UpperBound(all_cp_variables_[index]).value());
    const double value = lp_solution_[index];
    if (value < lb - kEpsilon || value > ub + kEpsilon) return Propagate();
  }
  return true;
}

bool FlowCosts::Propagate() {
  // Copy CP state into LP state.
  for (int i = 0; i < num_vars_; i++) {
    const IntegerVariable cp_var = all_cp_variables_[i];
    const double lb =
        static_cast<double>(integer_trail_->LowerBound(cp_var).value());
    const double ub =
        static_cast<double>(integer_trail_->UpperBound(cp_var).value());
    all_lp_variables_[i]->SetBounds(lb, ub);
  }

  // Solve LP state for feasibility, then try reduced cost stremghtening on
  // feasibility.
  objective_lp_->Clear();
  objective_lp_->SetCoefficient(violation_, 1);
  objective_lp_->SetMinimization();
  violation_->SetBounds(0.0, MPSolver::infinity());

  bool problem_is_feasible_ =
      lp_solver_.Solve() == MPSolver::ResultStatus::OPTIMAL;
  DCHECK(problem_is_feasible_)
      << "Bad encoding of flow constraint: problem should always be feasible";
  double violation_amount = violation_->solution_value();
  problem_is_feasible_ &= violation_amount < kEpsilon;

  if (!problem_is_feasible_) {
    FillIntegerReason(1.0);
    return integer_trail_->ReportConflict(integer_reason_);
  }

  ReducedCostStrengtheningDeductions(1.0, 0.0);
  if (!deductions_.empty()) {
    FillIntegerReason(1.0);
    for (IntegerLiteral deduction : deductions_) {
      if (!integer_trail_->Enqueue(deduction, {}, integer_reason_)) {
        return false;
      }
    }
  }

  violation_->SetBounds(0.0, 0.0);

  // For every cost variable, optimize both min/max value using LP,
  // and try reduced cost strengthening all other variables.
  // TODO(user): use a different lp solver for every cost type,
  // and for minimize/maximize.
  for (int c = 0; c < num_costs_; c++) {
    // If a cost variable is fixed, its reduced cost strengthening deductions
    // add nothing to that of feasibility, skip variable.
    const IntegerValue cost_cp_lb =
        integer_trail_->LowerBound(total_costs_per_cost_type_cp_[c]);
    const IntegerValue cost_cp_ub =
        integer_trail_->UpperBound(total_costs_per_cost_type_cp_[c]);

    if (cost_cp_lb == cost_cp_ub) continue;

    objective_lp_->Clear();
    objective_lp_->SetCoefficient(total_costs_per_cost_type_lp_[c], 1);

    // Minimize total_costs_per_cost_type[c], try to prune min, then use
    // reduced cost strengthening against max.
    objective_lp_->SetMinimization();
    const bool minimize_succeeded =
        lp_solver_.Solve() == MPSolver::ResultStatus::OPTIMAL;
    CHECK(minimize_succeeded);

    const double cost_lp_lb =
        total_costs_per_cost_type_lp_[c]->solution_value();

    for (int i = 0; i < num_vars_; i++) {
      lp_solution_[i] = all_lp_variables_[i]->solution_value();
    }

    if (cost_cp_lb < cost_lp_lb - kEpsilon) {
      FillIntegerReason(1.0);
      const IntegerValue new_lb =
          static_cast<IntegerValue>(ceil(cost_lp_lb - kEpsilon));
      const IntegerLiteral deduction = IntegerLiteral::GreaterOrEqual(
          total_costs_per_cost_type_cp_[c], new_lb);
      return integer_trail_->Enqueue(deduction, {}, integer_reason_);
    }

    ReducedCostStrengtheningDeductions(
        1.0, static_cast<double>(cost_cp_ub.value()) - cost_lp_lb);
    if (!deductions_.empty()) {
      FillIntegerReason(1.0);
      // Add the upper bound of the variable used for strenghening.
      integer_trail_->UpperBoundAsLiteral(total_costs_per_cost_type_cp_[c]);
      for (IntegerLiteral deduction : deductions_) {
        if (!integer_trail_->Enqueue(deduction, {}, integer_reason_)) {
          return false;
        }
      }
    }
  }

  return true;
}

void FlowCosts::ReducedCostStrengtheningDeductions(double direction,
                                                   double optimal_slack) {
  deductions_.clear();

  for (int i = 0; i < num_vars_; i++) {
    const double rc = all_lp_variables_[i]->reduced_cost() * direction;
    const double value = all_lp_variables_[i]->solution_value();

    if (rc > kEpsilon) {
      const IntegerValue ub = integer_trail_->UpperBound(all_cp_variables_[i]);
      const double new_ub = value + optimal_slack / rc;
      const IntegerValue new_ub_int =
          static_cast<IntegerValue>(floor(new_ub + kEpsilon));

      if (new_ub_int < ub) {
        deductions_.push_back(
            IntegerLiteral::LowerOrEqual(all_cp_variables_[i], new_ub_int));
      }
    } else if (rc < -kEpsilon) {
      const IntegerValue lb = integer_trail_->LowerBound(all_cp_variables_[i]);
      const double new_lb = value + optimal_slack / rc;
      const IntegerValue new_lb_int =
          static_cast<IntegerValue>(ceil(new_lb - kEpsilon));

      if (new_lb_int > lb) {
        deductions_.push_back(
            IntegerLiteral::GreaterOrEqual(all_cp_variables_[i], new_lb_int));
      }
    }
  }
}

void FlowCosts::FillIntegerReason(double direction) {
  integer_reason_.clear();

  for (int i = 0; i < num_vars_; i++) {
    // TODO(user): use the variable status instead of reduced cost.
    const double rc = all_lp_variables_[i]->reduced_cost() * direction;

    if (rc > kEpsilon) {
      integer_reason_.push_back(
          integer_trail_->LowerBoundAsLiteral(all_cp_variables_[i]));
    } else if (rc < -kEpsilon) {
      integer_reason_.push_back(
          integer_trail_->UpperBoundAsLiteral(all_cp_variables_[i]));
    }
  }
}

std::function<void(Model*)> FlowCostsConstraint(
    const std::vector<IntegerVariable>& demands,
    const std::vector<IntegerVariable>& flow, const std::vector<int>& tails,
    const std::vector<int>& heads,
    const std::vector<std::vector<int>>& arc_costs_per_cost_type,
    const std::vector<IntegerVariable>& total_costs_per_cost_type) {
  return [=](Model* model) {
    FlowCosts* constraint = new FlowCosts(
        demands, flow, tails, heads, arc_costs_per_cost_type,
        total_costs_per_cost_type, model->GetOrCreate<IntegerTrail>());
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

}  // namespace sat
}  // namespace operations_research

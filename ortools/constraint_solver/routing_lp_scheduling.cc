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

#include "ortools/constraint_solver/routing_lp_scheduling.h"

#include "ortools/constraint_solver/routing.h"
#include "ortools/glop/lp_solver.h"

namespace operations_research {
RouteDimensionCumulOptimizer::RouteDimensionCumulOptimizer(
    const RoutingDimension* dimension)
    : optimizer_core_(dimension) {
  // Using one solver and linear program per vehicle in the hope that if
  // routes don't change this will be faster.
  const int vehicles = dimension->model()->vehicles();
  lp_solver_.resize(vehicles);
  linear_program_.resize(vehicles);
  // The following parameters give the fastest response time without impacting
  // solutions found negatively.
  glop::GlopParameters parameters;
  parameters.set_use_dual_simplex(true);
  parameters.set_use_preprocessing(false);
  for (int vehicle = 0; vehicle < vehicles; ++vehicle) {
    lp_solver_[vehicle] = absl::make_unique<glop::LPSolver>();
    lp_solver_[vehicle]->SetParameters(parameters);
    linear_program_[vehicle] = absl::make_unique<glop::LinearProgram>();
  }
}

bool RouteDimensionCumulOptimizer::ComputeRouteCumulCost(
    int vehicle, const std::function<int64(int64)>& next_accessor,
    int64* optimal_cost) {
  return optimizer_core_.OptimizeSingleRoute(
      vehicle, next_accessor, linear_program_[vehicle].get(),
      lp_solver_[vehicle].get(), nullptr, optimal_cost, nullptr);
}

bool RouteDimensionCumulOptimizer::ComputeRouteCumulCostWithoutFixedTransits(
    int vehicle, const std::function<int64(int64)>& next_accessor,
    int64* optimal_cost_without_transits) {
  int64 cost = 0;
  int64 transit_cost = 0;
  if (optimizer_core_.OptimizeSingleRoute(
          vehicle, next_accessor, linear_program_[vehicle].get(),
          lp_solver_[vehicle].get(), nullptr, &cost, &transit_cost)) {
    if (optimal_cost_without_transits != nullptr) {
      *optimal_cost_without_transits = CapSub(cost, transit_cost);
    }
    return true;
  }
  return false;
}

bool RouteDimensionCumulOptimizer::ComputeRouteCumuls(
    int vehicle, const std::function<int64(int64)>& next_accessor,
    std::vector<int64>* optimal_cumuls) {
  return optimizer_core_.OptimizeSingleRoute(
      vehicle, next_accessor, linear_program_[vehicle].get(),
      lp_solver_[vehicle].get(), optimal_cumuls, nullptr, nullptr);
}

bool DimensionCumulOptimizerCore::OptimizeSingleRoute(
    int vehicle, const std::function<int64(int64)>& next_accessor,
    glop::LinearProgram* linear_program, glop::LPSolver* lp_solver,
    std::vector<int64>* cumul_values, int64* cost, int64* transit_cost) {
  linear_program->Clear();
  linear_program->SetMaximizationProblem(false);

  SetRouteCumulConstraints(vehicle, next_accessor, linear_program,
                           transit_cost);
  return FinalizeAndSolve(linear_program, lp_solver, cumul_values, cost);
}

void DimensionCumulOptimizerCore::SetRouteCumulConstraints(
    int vehicle, const std::function<int64(int64)>& next_accessor,
    glop::LinearProgram* linear_program, int64* route_transit_cost) {
  current_route_cumul_variables_.clear();
  std::vector<glop::ColIndex> slacks;
  int64 fixed_transit = 0;
  int64 total_fixed_transit = 0;

  const std::function<int64(int64, int64)>& transit_accessor =
      dimension_->transit_evaluator(vehicle);

  RoutingModel* const model = dimension_->model();
  int current = model->Start(vehicle);
  while (true) {
    glop::ColIndex cumul = linear_program->CreateNewVariable();
    IntVar* const cp_cumul = dimension_->CumulVar(current);
    // Handle potential precision errors due to infinite upper bound.
    const double cumul_max =
        (cp_cumul->Max() == kint64max) ? glop::kInfinity : cp_cumul->Max();
    linear_program->SetVariableBounds(cumul, cp_cumul->Min(), cumul_max);
    if (dimension_->HasCumulVarSoftUpperBound(current)) {
      // cumul - bound <= soft_ub_cost
      const int64 bound = dimension_->GetCumulVarSoftUpperBound(current);
      const int64 coef =
          dimension_->GetCumulVarSoftUpperBoundCoefficient(current);
      glop::ColIndex soft_ub_cost = linear_program->CreateNewVariable();
      glop::RowIndex ct = linear_program->CreateNewConstraint();
      linear_program->SetConstraintBounds(ct, -glop::kInfinity, bound);
      linear_program->SetCoefficient(ct, cumul, 1);
      linear_program->SetCoefficient(ct, soft_ub_cost, -1);
      linear_program->SetObjectiveCoefficient(soft_ub_cost, coef);
    }
    if (dimension_->HasCumulVarSoftLowerBound(current)) {
      // bound - cumul <= soft_lb_cost
      const int64 bound = dimension_->GetCumulVarSoftLowerBound(current);
      const int64 coef =
          dimension_->GetCumulVarSoftLowerBoundCoefficient(current);
      glop::ColIndex soft_lb_cost = linear_program->CreateNewVariable();
      glop::RowIndex ct = linear_program->CreateNewConstraint();
      linear_program->SetConstraintBounds(ct, bound, glop::kInfinity);
      linear_program->SetCoefficient(ct, cumul, 1);
      linear_program->SetCoefficient(ct, soft_lb_cost, 1);
      linear_program->SetObjectiveCoefficient(soft_lb_cost, coef);
    }
    if (!slacks.empty()) {
      // cumul = prev_cumul + prev_slack + transit
      glop::RowIndex ct = linear_program->CreateNewConstraint();
      linear_program->SetConstraintBounds(ct, fixed_transit, fixed_transit);
      linear_program->SetCoefficient(ct, cumul, 1);
      linear_program->SetCoefficient(ct, current_route_cumul_variables_.back(),
                                     -1);
      linear_program->SetCoefficient(ct, slacks.back(), -1);
    }
    current_route_cumul_variables_.push_back(cumul);
    if (!model->IsEnd(current)) {
      IntVar* const cp_slack = dimension_->SlackVar(current);
      glop::ColIndex slack = linear_program->CreateNewVariable();
      // Handle potential precision errors due to infinite upper bound.
      const double slack_max =
          (cp_slack->Max() == kint64max) ? glop::kInfinity : cp_slack->Max();
      linear_program->SetVariableBounds(slack, cp_slack->Min(), slack_max);
      slacks.push_back(slack);
      const int64 next_current = next_accessor(current);
      fixed_transit = transit_accessor(current, next_current);
      total_fixed_transit = CapAdd(total_fixed_transit, fixed_transit);
      current = next_current;
    } else {
      break;
    }
  }
  DCHECK_GE(current_route_cumul_variables_.size(), 2);

  const int64 span_bound = dimension_->GetSpanUpperBoundForVehicle(vehicle);
  if (span_bound < kint64max) {
    // end_cumul - start_cumul <= bound
    glop::RowIndex ct = linear_program->CreateNewConstraint();
    linear_program->SetConstraintBounds(ct, -glop::kInfinity, span_bound);
    linear_program->SetCoefficient(ct, current_route_cumul_variables_.back(),
                                   1);
    linear_program->SetCoefficient(ct, current_route_cumul_variables_.front(),
                                   -1);
  }
  const int64 span_cost = dimension_->GetSpanCostCoefficientForVehicle(vehicle);
  if (span_cost > 0) {
    linear_program->SetObjectiveCoefficient(
        current_route_cumul_variables_.back(), span_cost);
    linear_program->SetObjectiveCoefficient(
        current_route_cumul_variables_.front(), -span_cost);
  }
  if (route_transit_cost != nullptr) {
    if (span_cost > 0) {
      *route_transit_cost = CapProd(total_fixed_transit, span_cost);
    } else {
      *route_transit_cost = 0;
    }
  }
}

bool DimensionCumulOptimizerCore::FinalizeAndSolve(
    glop::LinearProgram* linear_program, glop::LPSolver* lp_solver,
    std::vector<int64>* cumul_values, int64* cost) {
  linear_program->CleanUp();
  const glop::ProblemStatus status = lp_solver->Solve(*linear_program);
  if (status != glop::ProblemStatus::OPTIMAL) {
    if (cost != nullptr) {
      *cost = kint64max;
    }
    return false;
  }
  if (cost != nullptr) {
    *cost = std::round(lp_solver->GetObjectiveValue());
  }
  if (cumul_values != nullptr) {
    cumul_values->clear();
    cumul_values->reserve(current_route_cumul_variables_.size());
    for (auto cumul : current_route_cumul_variables_) {
      cumul_values->push_back(std::round(lp_solver->variable_values()[cumul]));
    }
  }
  return true;
}
}  // namespace operations_research

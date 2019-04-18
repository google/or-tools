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

#include <numeric>

#include "absl/time/time.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {

namespace {

// The following sets of parameters give the fastest response time without
// impacting solutions found negatively.
glop::GlopParameters GetGlopParametersForLocalLP() {
  glop::GlopParameters parameters;
  parameters.set_use_dual_simplex(true);
  parameters.set_use_preprocessing(false);
  return parameters;
}

glop::GlopParameters GetGlopParametersForGlobalLP() {
  glop::GlopParameters parameters;
  parameters.set_use_dual_simplex(true);
  return parameters;
}

bool SetVariableBounds(glop::LinearProgram* linear_program,
                       const glop::ColIndex index, int64 min, int64 max) {
  // When variable upper bounds are greater than this threshold, precision
  // issues arise in GLOP. In this case we are just going to suppose that these
  // high bound values are infinite and not set the upper bound.
  const int64 kMaxValue = 1e10;
  const double lp_min = min;
  const double lp_max = (max > kMaxValue) ? glop::kInfinity : max;
  if (lp_min <= lp_max) {
    linear_program->SetVariableBounds(index, lp_min, lp_max);
    return true;
  }
  // The linear_program would not be feasible, and it cannot handle the
  // lp_min > lp_max case, so we must detect infeasibility here.
  return false;
}
}  // namespace

LocalDimensionCumulOptimizer::LocalDimensionCumulOptimizer(
    const RoutingDimension* dimension)
    : optimizer_core_(dimension) {
  // Using one solver and linear program per vehicle in the hope that if
  // routes don't change this will be faster.
  const int vehicles = dimension->model()->vehicles();
  lp_solver_.resize(vehicles);
  linear_program_.resize(vehicles);
  const glop::GlopParameters parameters = GetGlopParametersForLocalLP();
  for (int vehicle = 0; vehicle < vehicles; ++vehicle) {
    lp_solver_[vehicle] = absl::make_unique<glop::LPSolver>();
    lp_solver_[vehicle]->SetParameters(parameters);
    linear_program_[vehicle] = absl::make_unique<glop::LinearProgram>();
  }
}

bool LocalDimensionCumulOptimizer::ComputeRouteCumulCost(
    int vehicle, const std::function<int64(int64)>& next_accessor,
    int64* optimal_cost) {
  return optimizer_core_.OptimizeSingleRoute(
      vehicle, next_accessor, linear_program_[vehicle].get(),
      lp_solver_[vehicle].get(), nullptr, optimal_cost, nullptr);
}

bool LocalDimensionCumulOptimizer::ComputeRouteCumulCostWithoutFixedTransits(
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

bool LocalDimensionCumulOptimizer::ComputeRouteCumuls(
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
  InitOptimizer(linear_program);

  const RoutingModel* const model = dimension()->model();
  const bool optimize_vehicle_costs =
      (cumul_values != nullptr || cost != nullptr) &&
      (!model->IsEnd(next_accessor(model->Start(vehicle))) ||
       model->AreEmptyRouteCostsConsideredForVehicle(vehicle));
  const int64 cumul_offset =
      dimension_->GetLocalOptimizerOffsetForVehicle(vehicle);
  int64 cost_offset = 0;
  if (!SetRouteCumulConstraints(vehicle, next_accessor, cumul_offset,
                                optimize_vehicle_costs, linear_program,
                                transit_cost, &cost_offset) ||
      !FinalizeAndSolve(linear_program, lp_solver)) {
    return false;
  }

  SetCumulValuesFromLP(current_route_cumul_variables_, cumul_offset, *lp_solver,
                       cumul_values);
  if (cost != nullptr) {
    *cost = CapAdd(cost_offset, std::round(lp_solver->GetObjectiveValue()));
  }

  linear_program->Clear();
  return true;
}

bool DimensionCumulOptimizerCore::Optimize(
    const std::function<int64(int64)>& next_accessor,
    glop::LinearProgram* linear_program, glop::LPSolver* lp_solver,
    std::vector<int64>* cumul_values, int64* cost, int64* transit_cost) {
  InitOptimizer(linear_program);

  // If both "cumul_values" and "cost" parameters are null, we don't try to
  // optimize the cost and stop at the first feasible solution.
  const bool optimize_costs = (cumul_values != nullptr) || (cost != nullptr);
  bool has_vehicles_being_optimized = false;

  const int64 cumul_offset = dimension_->GetGlobalOptimizerOffset();
  int64 total_transit_cost = 0;
  int64 total_cost_offset = 0;
  const RoutingModel* model = dimension()->model();
  for (int vehicle = 0; vehicle < model->vehicles(); vehicle++) {
    int64 route_transit_cost = 0;
    int64 route_cost_offset = 0;
    const bool optimize_vehicle_costs =
        optimize_costs &&
        (!model->IsEnd(next_accessor(model->Start(vehicle))) ||
         model->AreEmptyRouteCostsConsideredForVehicle(vehicle));
    if (!SetRouteCumulConstraints(vehicle, next_accessor, cumul_offset,
                                  optimize_vehicle_costs, linear_program,
                                  &route_transit_cost, &route_cost_offset)) {
      return false;
    }
    total_transit_cost = CapAdd(total_transit_cost, route_transit_cost);
    total_cost_offset = CapAdd(total_cost_offset, route_cost_offset);
    has_vehicles_being_optimized |= optimize_vehicle_costs;
  }
  if (transit_cost != nullptr) {
    *transit_cost = total_transit_cost;
  }

  SetGlobalConstraints(has_vehicles_being_optimized, linear_program);

  if (!FinalizeAndSolve(linear_program, lp_solver)) {
    return false;
  }

  SetCumulValuesFromLP(index_to_cumul_variable_, cumul_offset, *lp_solver,
                       cumul_values);

  if (cost != nullptr) {
    *cost =
        CapAdd(std::round(lp_solver->GetObjectiveValue()), total_cost_offset);
  }

  linear_program->Clear();
  return true;
}

bool DimensionCumulOptimizerCore::OptimizeAndPack(
    const std::function<int64(int64)>& next_accessor,
    glop::LinearProgram* linear_program, glop::LPSolver* lp_solver,
    std::vector<int64>* cumul_values) {
  InitOptimizer(linear_program);

  const int64 cumul_offset = dimension_->GetGlobalOptimizerOffset();
  const RoutingModel* model = dimension()->model();
  bool has_vehicles_being_optimized = false;
  for (int vehicle = 0; vehicle < model->vehicles(); vehicle++) {
    const bool optimize_vehicle_costs =
        !model->IsEnd(next_accessor(model->Start(vehicle))) ||
        model->AreEmptyRouteCostsConsideredForVehicle(vehicle);
    if (!SetRouteCumulConstraints(vehicle, next_accessor, cumul_offset,
                                  optimize_vehicle_costs, linear_program,
                                  nullptr, nullptr)) {
      return false;
    }
    has_vehicles_being_optimized |= optimize_vehicle_costs;
  }
  SetGlobalConstraints(has_vehicles_being_optimized, linear_program);

  if (!FinalizeAndSolve(linear_program, lp_solver)) {
    return false;
  }

  // Minimize the route end times without increasing the cost.
  glop::RowIndex objective_ct = linear_program->CreateNewConstraint();
  linear_program->SetConstraintBounds(objective_ct, 0,
                                      lp_solver->GetObjectiveValue());

  const glop::DenseRow& objective_coefficients =
      linear_program->objective_coefficients();
  for (glop::ColIndex variable(0); variable < linear_program->num_variables();
       variable++) {
    const double coefficient = objective_coefficients[variable];
    if (coefficient != 0) {
      linear_program->SetCoefficient(objective_ct, variable, coefficient);
      linear_program->SetObjectiveCoefficient(variable, 0);
    }
  }
  for (int vehicle = 0; vehicle < model->vehicles(); vehicle++) {
    linear_program->SetObjectiveCoefficient(
        index_to_cumul_variable_[model->End(vehicle)], 1);
  }

  if (!FinalizeAndSolve(linear_program, lp_solver)) {
    return false;
  }

  // Maximize the route start times without increasing the cost or the route end
  // times.
  for (int vehicle = 0; vehicle < model->vehicles(); vehicle++) {
    const glop::ColIndex end_cumul_var =
        index_to_cumul_variable_[model->End(vehicle)];
    // end_cumul_var <= lp_solver.variable_values()[end_cumul_var]
    linear_program->SetVariableBounds(
        end_cumul_var, linear_program->variable_lower_bounds()[end_cumul_var],
        lp_solver->variable_values()[end_cumul_var]);
    linear_program->SetObjectiveCoefficient(end_cumul_var, 0);

    // Maximize the starts of the routes.
    linear_program->SetObjectiveCoefficient(
        index_to_cumul_variable_[model->Start(vehicle)], -1);
  }
  if (!FinalizeAndSolve(linear_program, lp_solver)) {
    return false;
  }

  SetCumulValuesFromLP(index_to_cumul_variable_, cumul_offset, *lp_solver,
                       cumul_values);
  linear_program->Clear();
  return true;
}

void DimensionCumulOptimizerCore::InitOptimizer(
    glop::LinearProgram* linear_program) {
  linear_program->Clear();
  linear_program->SetMaximizationProblem(false);
  index_to_cumul_variable_.clear();
  index_to_cumul_variable_.resize(dimension_->cumuls().size(),
                                  glop::ColIndex(-1));
  max_end_cumul_ = linear_program->CreateNewVariable();
  min_start_cumul_ = linear_program->CreateNewVariable();
}

bool DimensionCumulOptimizerCore::SetRouteCumulConstraints(
    int vehicle, const std::function<int64(int64)>& next_accessor,
    int64 cumul_offset, bool optimize_costs,
    glop::LinearProgram* linear_program, int64* route_transit_cost,
    int64* route_cost_offset) {
  RoutingModel* const model = dimension_->model();
  // Extract the vehicle's path from next_accessor.
  std::vector<int64> path;
  {
    int node = model->Start(vehicle);
    path.push_back(node);
    while (!model->IsEnd(node)) {
      node = next_accessor(node);
      path.push_back(node);
    }
    DCHECK_GE(path.size(), 2);
  }
  const int path_size = path.size();
  // Extract cumul min/max and fixed transits from CP.
  std::vector<int64> cumul_min(path_size);
  std::vector<int64> cumul_max(path_size);
  for (int pos = 0; pos < path_size; ++pos) {
    const IntVar* cumul = dimension_->CumulVar(path[pos]);
    cumul_min[pos] = cumul->Min();
    cumul_min[pos] = std::max<int64>(0, CapSub(cumul_min[pos], cumul_offset));
    cumul_max[pos] = cumul->Max();
    cumul_max[pos] =
        (cumul_max[pos] == kint64max)
            ? kint64max
            : std::max<int64>(0, CapSub(cumul_max[pos], cumul_offset));
  }
  std::vector<int64> fixed_transit(path_size - 1);
  {
    const std::function<int64(int64, int64)>& transit_accessor =
        dimension_->transit_evaluator(vehicle);
    for (int pos = 1; pos < path_size; ++pos) {
      fixed_transit[pos - 1] = transit_accessor(path[pos - 1], path[pos]);
    }
  }
  // Refine cumul bounds using cumul[i] + fixed_transit[i] <= cumul[i+1].
  for (int pos = 1; pos < path_size; ++pos) {
    cumul_min[pos] = std::max(
        cumul_min[pos], CapAdd(cumul_min[pos - 1], fixed_transit[pos - 1]));
  }
  for (int pos = path_size - 2; pos >= 0; --pos) {
    // If cumul_max[pos+1] is kint64max, it will be translated to
    // double +infinity, so it must not constrain cumul_max[pos].
    if (cumul_max[pos + 1] < kint64max) {
      cumul_max[pos] = std::min(cumul_max[pos],
                                CapSub(cumul_max[pos + 1], fixed_transit[pos]));
    }
  }

  // LP Model variables, current_route_cumul_variables_ and lp_slacks.
  // Create LP variables for cumuls.
  std::vector<glop::ColIndex>& lp_cumuls = current_route_cumul_variables_;
  lp_cumuls.assign(path_size, glop::kInvalidCol);
  for (int pos = 0; pos < path_size; ++pos) {
    const glop::ColIndex lp_cumul = linear_program->CreateNewVariable();
    index_to_cumul_variable_[path[pos]] = lp_cumul;
    lp_cumuls[pos] = lp_cumul;
    if (!SetVariableBounds(linear_program, lp_cumul, cumul_min[pos],
                           cumul_max[pos])) {
      return false;
    }
  }
  // Create LP variables for slacks.
  std::vector<glop::ColIndex> lp_slacks(path_size - 1, glop::kInvalidCol);
  for (int pos = 0; pos < path_size - 1; ++pos) {
    const IntVar* cp_slack = dimension_->SlackVar(path[pos]);
    lp_slacks[pos] = linear_program->CreateNewVariable();
    if (!SetVariableBounds(linear_program, lp_slacks[pos], cp_slack->Min(),
                           cp_slack->Max())) {
      return false;
    }
  }

  // LP Model constraints and costs.
  // Add all path constraints to LP:
  // cumul[i] + fixed_transit[i] + slack[i] == cumul[i+1]
  // <=> fixed_transit[i] == cumul[i+1] - cumul[i] - slack[i].
  for (int pos = 0; pos < path_size - 1; ++pos) {
    const glop::RowIndex ct = linear_program->CreateNewConstraint();
    linear_program->SetConstraintBounds(ct, fixed_transit[pos],
                                        fixed_transit[pos]);
    linear_program->SetCoefficient(ct, lp_cumuls[pos + 1], 1);
    linear_program->SetCoefficient(ct, lp_cumuls[pos], -1);
    linear_program->SetCoefficient(ct, lp_slacks[pos], -1);
  }
  if (route_cost_offset != nullptr) *route_cost_offset = 0;
  if (optimize_costs) {
    // Add soft upper bounds.
    for (int pos = 0; pos < path_size; ++pos) {
      if (!dimension_->HasCumulVarSoftUpperBound(path[pos])) continue;
      const int64 coef =
          dimension_->GetCumulVarSoftUpperBoundCoefficient(path[pos]);
      if (coef == 0) continue;
      int64 bound = dimension_->GetCumulVarSoftUpperBound(path[pos]);
      if (bound < cumul_offset && route_cost_offset != nullptr) {
        // Add coef * (cumul_offset - bound) to the cost offset.
        *route_cost_offset = CapAdd(*route_cost_offset,
                                    CapProd(CapSub(cumul_offset, bound), coef));
      }
      bound = std::max<int64>(0, CapSub(bound, cumul_offset));
      if (cumul_max[pos] <= bound) continue;  // constraint is never violated.
      const glop::ColIndex soft_ub_diff = linear_program->CreateNewVariable();
      linear_program->SetObjectiveCoefficient(soft_ub_diff, coef);
      // cumul - soft_ub_diff <= bound.
      const glop::RowIndex ct = linear_program->CreateNewConstraint();
      linear_program->SetConstraintBounds(ct, -glop::kInfinity, bound);
      linear_program->SetCoefficient(ct, lp_cumuls[pos], 1);
      linear_program->SetCoefficient(ct, soft_ub_diff, -1);
    }
    // Add soft lower bounds.
    for (int pos = 0; pos < path_size; ++pos) {
      if (!dimension_->HasCumulVarSoftLowerBound(path[pos])) continue;
      const int64 coef =
          dimension_->GetCumulVarSoftLowerBoundCoefficient(path[pos]);
      if (coef == 0) continue;
      const int64 bound = std::max<int64>(
          0, CapSub(dimension_->GetCumulVarSoftLowerBound(path[pos]),
                    cumul_offset));
      if (cumul_min[pos] >= bound) continue;  // constraint is never violated.
      const glop::ColIndex soft_lb_diff = linear_program->CreateNewVariable();
      linear_program->SetObjectiveCoefficient(soft_lb_diff, coef);
      // bound - cumul <= soft_lb_diff
      const glop::RowIndex ct = linear_program->CreateNewConstraint();
      linear_program->SetConstraintBounds(ct, bound, glop::kInfinity);
      linear_program->SetCoefficient(ct, lp_cumuls[pos], 1);
      linear_program->SetCoefficient(ct, soft_lb_diff, 1);
    }
  }
  // Add pickup and delivery limits.
  if (dimension_->HasPickupToDeliveryLimits()) {
    // visited_pickup_index_for_pair_ must be all -1.
    DCHECK(std::all_of(visited_pickup_index_for_pair_.begin(),
                       visited_pickup_index_for_pair_.end(),
                       [](int64 node) { return node == -1; }));
    std::vector<int64> visited_pairs;
    for (int pos = 0; pos < path_size - 1; ++pos) {
      const std::vector<std::pair<int, int>>& pickup_index_pairs =
          model->GetPickupIndexPairs(path[pos]);
      const std::vector<std::pair<int, int>>& delivery_index_pairs =
          model->GetDeliveryIndexPairs(path[pos]);
      if (!pickup_index_pairs.empty()) {
        // The current node is a pickup. We verify that it belongs to a single
        // pickup index pair and that it's not a delivery, and store the index.
        DCHECK(delivery_index_pairs.empty());
        DCHECK_EQ(pickup_index_pairs.size(), 1);
        visited_pickup_index_for_pair_[pickup_index_pairs[0].first] = path[pos];
        visited_pairs.push_back(pickup_index_pairs[0].first);
      } else if (!delivery_index_pairs.empty()) {
        // The node is a delivery. We verify that it belongs to a single
        // delivery pair, and set the limit with its pickup if one has been
        // visited for this pair.
        DCHECK_EQ(delivery_index_pairs.size(), 1);
        const int pair_index = delivery_index_pairs[0].first;
        const int64 pickup_index = visited_pickup_index_for_pair_[pair_index];
        if (pickup_index < 0) continue;
        const int64 limit = dimension_->GetPickupToDeliveryLimitForPair(
            pair_index, model->GetPickupIndexPairs(pickup_index)[0].second,
            delivery_index_pairs[0].second);
        if (limit < kint64max) {
          // delivery_cumul - pickup_cumul <= limit.
          glop::RowIndex ct = linear_program->CreateNewConstraint();
          linear_program->SetConstraintBounds(ct, -glop::kInfinity, limit);
          linear_program->SetCoefficient(ct, lp_cumuls[pos], 1);
          linear_program->SetCoefficient(
              ct, index_to_cumul_variable_[pickup_index], -1);
        }
      }
    }
    for (const int64 pair : visited_pairs) {
      visited_pickup_index_for_pair_[pair] = -1;
    }
  }
  // Add span bound constraint.
  const int64 span_bound = dimension_->GetSpanUpperBoundForVehicle(vehicle);
  if (span_bound < kint64max) {
    // end_cumul - start_cumul <= bound
    glop::RowIndex ct = linear_program->CreateNewConstraint();
    linear_program->SetConstraintBounds(ct, -glop::kInfinity, span_bound);
    linear_program->SetCoefficient(ct, lp_cumuls.back(), 1);
    linear_program->SetCoefficient(ct, lp_cumuls.front(), -1);
  }
  // Add span cost.
  const int64 span_cost_coef =
      dimension_->GetSpanCostCoefficientForVehicle(vehicle);
  if (optimize_costs && span_cost_coef > 0) {
    linear_program->SetObjectiveCoefficient(lp_cumuls.back(), span_cost_coef);
    linear_program->SetObjectiveCoefficient(lp_cumuls.front(), -span_cost_coef);
  }
  if (optimize_costs && dimension_->global_span_cost_coefficient() > 0) {
    // min_start_cumul_ <= cumuls[start]
    glop::RowIndex ct = linear_program->CreateNewConstraint();
    linear_program->SetConstraintBounds(ct, -glop::kInfinity, 0);
    linear_program->SetCoefficient(ct, min_start_cumul_, 1);
    linear_program->SetCoefficient(ct, lp_cumuls.front(), -1);
    // max_end_cumul_ >= cumuls[end]
    ct = linear_program->CreateNewConstraint();
    linear_program->SetConstraintBounds(ct, 0, glop::kInfinity);
    linear_program->SetCoefficient(ct, max_end_cumul_, 1);
    linear_program->SetCoefficient(ct, lp_cumuls.back(), -1);
  }
  if (route_transit_cost != nullptr) {
    if (optimize_costs && span_cost_coef > 0) {
      const int64 total_fixed_transit = std::accumulate(
          fixed_transit.begin(), fixed_transit.end(), 0, CapAdd);
      *route_transit_cost = CapProd(total_fixed_transit, span_cost_coef);
    } else {
      *route_transit_cost = 0;
    }
  }
  return true;
}

void DimensionCumulOptimizerCore::SetGlobalConstraints(
    bool optimize_costs, glop::LinearProgram* linear_program) {
  // Global span cost =
  //     global_span_cost_coefficient * (max_end_cumul - min_start_cumul).
  const int64 global_span_coeff = dimension_->global_span_cost_coefficient();
  if (optimize_costs && global_span_coeff > 0) {
    linear_program->SetObjectiveCoefficient(max_end_cumul_, global_span_coeff);
    linear_program->SetObjectiveCoefficient(min_start_cumul_,
                                            -global_span_coeff);
  }

  // Node precedence constraints, set when both nodes are visited.
  for (const RoutingDimension::NodePrecedence& precedence :
       dimension_->GetNodePrecedences()) {
    const glop::ColIndex first_cumul_var =
        index_to_cumul_variable_[precedence.first_node];
    const glop::ColIndex second_cumul_var =
        index_to_cumul_variable_[precedence.second_node];
    if (first_cumul_var < 0 || second_cumul_var < 0) {
      // At least one of the nodes is not on any route, skip this precedence
      // constraint.
      continue;
    }
    DCHECK_NE(first_cumul_var, second_cumul_var)
        << "Dimension " << dimension_->name()
        << " has a self-precedence on node " << precedence.first_node << ".";

    // cumul[second_node] - cumul[first_node] >= offset.
    const glop::RowIndex ct = linear_program->CreateNewConstraint();
    linear_program->SetConstraintBounds(ct, precedence.offset, glop::kInfinity);
    linear_program->SetCoefficient(ct, second_cumul_var, 1);
    linear_program->SetCoefficient(ct, first_cumul_var, -1);
  }
}

bool DimensionCumulOptimizerCore::FinalizeAndSolve(
    glop::LinearProgram* linear_program, glop::LPSolver* lp_solver) {
  // Set the time limit of the LP solver based on the model's remaining time.
  const absl::Duration duration_limit = dimension()->model()->RemainingTime();
  lp_solver->GetMutableParameters()->set_max_time_in_seconds(
      absl::ToDoubleSeconds(duration_limit));

  // Because we construct the lp one constraint at a time and we never call
  // SetCoefficient() on the same variable twice for a constraint, we know that
  // the columns do not contain duplicates and are already ordered by constraint
  // so we do not need to call linear_program->CleanUp() which can be costly.
  // Note that the assumptions are DCHECKed() in the call below.
  linear_program->NotifyThatColumnsAreClean();
  VLOG(2) << linear_program->Dump();
  const glop::ProblemStatus status = lp_solver->Solve(*linear_program);
  if (status != glop::ProblemStatus::OPTIMAL &&
      status != glop::ProblemStatus::IMPRECISE) {
    linear_program->Clear();
    return false;
  }
  return true;
}

void DimensionCumulOptimizerCore::SetCumulValuesFromLP(
    const std::vector<glop::ColIndex>& cumul_variables, int64 offset,
    const glop::LPSolver& lp_solver, std::vector<int64>* cumul_values) {
  if (cumul_values == nullptr) return;
  cumul_values->clear();
  cumul_values->resize(cumul_variables.size());

  for (int index = 0; index < cumul_variables.size(); index++) {
    const glop::ColIndex cumul_var = cumul_variables[index];
    if (cumul_var < 0) {
      // Node indices that do not appear on any route (i.e. unperformed nodes)
      // have a cumul_var of -1 when SetCumulValuesFromLP() is called with
      // cumul_variables == index_to_cumul_variable_.
      (*cumul_values)[index] = dimension_->CumulVar(index)->Min();
    } else {
      const double lp_value_double = lp_solver.variable_values()[cumul_var];
      const int64 lp_value_int64 =
          (lp_value_double >= kint64max)
              ? kint64max
              : static_cast<int64>(std::round(lp_value_double));
      (*cumul_values)[index] = CapAdd(lp_value_int64, offset);
    }
  }
}

GlobalDimensionCumulOptimizer::GlobalDimensionCumulOptimizer(
    const RoutingDimension* dimension)
    : optimizer_core_(dimension) {
  lp_solver_.SetParameters(GetGlopParametersForGlobalLP());
}

bool GlobalDimensionCumulOptimizer::ComputeCumulCostWithoutFixedTransits(
    const std::function<int64(int64)>& next_accessor,
    int64* optimal_cost_without_transits) {
  int64 cost = 0;
  int64 transit_cost = 0;
  if (optimizer_core_.Optimize(next_accessor, &linear_program_, &lp_solver_,
                               nullptr, &cost, &transit_cost)) {
    if (optimal_cost_without_transits != nullptr) {
      *optimal_cost_without_transits = CapSub(cost, transit_cost);
    }
    return true;
  }
  return false;
}

bool GlobalDimensionCumulOptimizer::ComputeCumuls(
    const std::function<int64(int64)>& next_accessor,
    std::vector<int64>* optimal_cumuls) {
  return optimizer_core_.Optimize(next_accessor, &linear_program_, &lp_solver_,
                                  optimal_cumuls, nullptr, nullptr);
}

bool GlobalDimensionCumulOptimizer::IsFeasible(
    const std::function<int64(int64)>& next_accessor) {
  return optimizer_core_.Optimize(next_accessor, &linear_program_, &lp_solver_,
                                  nullptr, nullptr, nullptr);
}

bool GlobalDimensionCumulOptimizer::ComputePackedCumuls(
    const std::function<int64(int64)>& next_accessor,
    std::vector<int64>* packed_cumuls) {
  return optimizer_core_.OptimizeAndPack(next_accessor, &linear_program_,
                                         &lp_solver_, packed_cumuls);
}
}  // namespace operations_research

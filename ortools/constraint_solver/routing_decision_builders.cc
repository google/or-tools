// Copyright 2010-2022 Google LLC
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

#include "ortools/constraint_solver/routing_decision_builders.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/map_util.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_lp_scheduling.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {

namespace {

// A decision builder which tries to assign values to variables as close as
// possible to target values first.
class SetValuesFromTargets : public DecisionBuilder {
 public:
  SetValuesFromTargets(std::vector<IntVar*> variables,
                       std::vector<int64_t> targets)
      : variables_(std::move(variables)),
        targets_(std::move(targets)),
        index_(0),
        steps_(variables_.size(), 0) {
    DCHECK_EQ(variables_.size(), targets_.size());
  }
  Decision* Next(Solver* solver) override {
    int index = index_.Value();
    while (index < variables_.size() && variables_[index]->Bound()) {
      ++index;
    }
    index_.SetValue(solver, index);
    if (index >= variables_.size()) return nullptr;
    const int64_t variable_min = variables_[index]->Min();
    const int64_t variable_max = variables_[index]->Max();
    // Target can be before, inside, or after the variable range.
    // We do a trichotomy on this for clarity.
    if (targets_[index] <= variable_min) {
      return solver->MakeAssignVariableValue(variables_[index], variable_min);
    } else if (targets_[index] >= variable_max) {
      return solver->MakeAssignVariableValue(variables_[index], variable_max);
    } else {
      int64_t step = steps_[index];
      int64_t value = CapAdd(targets_[index], step);
      // If value is out of variable's range, we can remove the interval of
      // values already explored (which can make the solver fail) and
      // recall Next() to get back into the trichotomy above.
      if (value < variable_min || variable_max < value) {
        step = GetNextStep(step);
        value = CapAdd(targets_[index], step);
        if (step > 0) {
          // Values in [variable_min, value) were already explored.
          variables_[index]->SetMin(value);
        } else {
          // Values in (value, variable_max] were already explored.
          variables_[index]->SetMax(value);
        }
        return Next(solver);
      }
      steps_.SetValue(solver, index, GetNextStep(step));
      return solver->MakeAssignVariableValueOrDoNothing(variables_[index],
                                                        value);
    }
  }

 private:
  int64_t GetNextStep(int64_t step) const {
    return (step > 0) ? -step : CapSub(1, step);
  }
  const std::vector<IntVar*> variables_;
  const std::vector<int64_t> targets_;
  Rev<int> index_;
  RevArray<int64_t> steps_;
};

}  // namespace

DecisionBuilder* MakeSetValuesFromTargets(Solver* solver,
                                          std::vector<IntVar*> variables,
                                          std::vector<int64_t> targets) {
  return solver->RevAlloc(
      new SetValuesFromTargets(std::move(variables), std::move(targets)));
}

namespace {

bool DimensionFixedTransitsEqualTransitEvaluatorForVehicle(
    const RoutingDimension& dimension, int vehicle) {
  const RoutingModel* const model = dimension.model();
  int node = model->Start(vehicle);
  while (!model->IsEnd(node)) {
    if (!model->NextVar(node)->Bound()) {
      return false;
    }
    const int next = model->NextVar(node)->Value();
    if (dimension.transit_evaluator(vehicle)(node, next) !=
        dimension.FixedTransitVar(node)->Value()) {
      return false;
    }
    node = next;
  }
  return true;
}

bool DimensionFixedTransitsEqualTransitEvaluators(
    const RoutingDimension& dimension) {
  for (int vehicle = 0; vehicle < dimension.model()->vehicles(); vehicle++) {
    if (!DimensionFixedTransitsEqualTransitEvaluatorForVehicle(dimension,
                                                               vehicle)) {
      return false;
    }
  }
  return true;
}

// Concatenates cumul_values and break_values into 'values', and generates the
// corresponding 'variables' vector.
void ConcatenateRouteCumulAndBreakVarAndValues(
    const RoutingDimension& dimension, int vehicle,
    const std::vector<int64_t>& cumul_values,
    absl::Span<const int64_t> break_values, std::vector<IntVar*>* variables,
    std::vector<int64_t>* values) {
  *values = cumul_values;
  variables->clear();
  const RoutingModel& model = *dimension.model();
  {
    int current = model.Start(vehicle);
    while (true) {
      variables->push_back(dimension.CumulVar(current));
      if (!model.IsEnd(current)) {
        current = model.NextVar(current)->Value();
      } else {
        break;
      }
    }
  }
  // Setting the cumuls of path start/end first is more efficient than
  // setting the cumuls in order of path appearance, because setting start
  // and end cumuls gives an opportunity to fix all cumuls with two
  // decisions instead of |path| decisions.
  // To this effect, we put end cumul just after the start cumul.
  std::swap(variables->at(1), variables->back());
  std::swap(values->at(1), values->back());
  if (dimension.HasBreakConstraints()) {
    for (IntervalVar* interval :
         dimension.GetBreakIntervalsOfVehicle(vehicle)) {
      variables->push_back(interval->SafeStartExpr(0)->Var());
      variables->push_back(interval->SafeEndExpr(0)->Var());
    }
    values->insert(values->end(), break_values.begin(), break_values.end());
  }
  // Value kint64min signals an unoptimized variable, set to min instead.
  for (int j = 0; j < values->size(); ++j) {
    if (values->at(j) == std::numeric_limits<int64_t>::min()) {
      values->at(j) = variables->at(j)->Min();
    }
  }
  DCHECK_EQ(variables->size(), values->size());
}

class SetCumulsFromLocalDimensionCosts : public DecisionBuilder {
 public:
  SetCumulsFromLocalDimensionCosts(
      LocalDimensionCumulOptimizer* local_optimizer,
      LocalDimensionCumulOptimizer* local_mp_optimizer, SearchMonitor* monitor,
      bool optimize_and_pack,
      std::vector<RoutingModel::RouteDimensionTravelInfo>
          dimension_travel_info_per_route)
      : local_optimizer_(local_optimizer),
        local_mp_optimizer_(local_mp_optimizer),
        monitor_(monitor),
        optimize_and_pack_(optimize_and_pack),
        dimension_travel_info_per_route_(
            std::move(dimension_travel_info_per_route)) {
    DCHECK(dimension_travel_info_per_route_.empty() ||
           dimension_travel_info_per_route_.size() ==
               local_optimizer_->dimension()->model()->vehicles());
    const RoutingDimension* const dimension = local_optimizer->dimension();
    const std::vector<int>& resource_groups =
        dimension->model()->GetDimensionResourceGroupIndices(dimension);
    DCHECK_LE(resource_groups.size(), optimize_and_pack ? 1 : 0);
    resource_group_index_ = resource_groups.empty() ? -1 : resource_groups[0];
  }

  Decision* Next(Solver* solver) override {
    const RoutingDimension& dimension = *local_optimizer_->dimension();
    RoutingModel* const model = dimension.model();
    // The following boolean variable indicates if the solver should fail, in
    // order to postpone the Fail() call until after the for loop, so there are
    // no memory leaks related to the cumul_values vector.
    bool should_fail = false;
    for (int vehicle = 0; vehicle < model->vehicles(); ++vehicle) {
      solver->TopPeriodicCheck();
      // TODO(user): Investigate if we should skip unused vehicles.
      DCHECK(DimensionFixedTransitsEqualTransitEvaluatorForVehicle(dimension,
                                                                   vehicle));
      const bool vehicle_has_break_constraint =
          dimension.HasBreakConstraints() &&
          !dimension.GetBreakIntervalsOfVehicle(vehicle).empty();
      LocalDimensionCumulOptimizer* const optimizer =
          vehicle_has_break_constraint ? local_mp_optimizer_ : local_optimizer_;
      DCHECK(optimizer != nullptr);
      std::vector<int64_t> cumul_values;
      std::vector<int64_t> break_start_end_values;
      const DimensionSchedulingStatus status =
          ComputeCumulAndBreakValuesForVehicle(
              optimizer, vehicle, &cumul_values, &break_start_end_values);
      if (status == DimensionSchedulingStatus::INFEASIBLE) {
        should_fail = true;
        break;
      }
      // If relaxation is not feasible, try the MILP optimizer.
      if (status == DimensionSchedulingStatus::RELAXED_OPTIMAL_ONLY) {
        DCHECK(local_mp_optimizer_ != nullptr);
        if (ComputeCumulAndBreakValuesForVehicle(local_mp_optimizer_, vehicle,
                                                 &cumul_values,
                                                 &break_start_end_values) ==
            DimensionSchedulingStatus::INFEASIBLE) {
          should_fail = true;
          break;
        }
      } else {
        DCHECK(status == DimensionSchedulingStatus::OPTIMAL);
      }
      // Concatenate cumul_values and break_start_end_values into cp_values,
      // generate corresponding cp_variables vector.
      std::vector<IntVar*> cp_variables;
      std::vector<int64_t> cp_values;
      ConcatenateRouteCumulAndBreakVarAndValues(
          dimension, vehicle, cumul_values, break_start_end_values,
          &cp_variables, &cp_values);
      if (!solver->SolveAndCommit(
              MakeSetValuesFromTargets(solver, std::move(cp_variables),
                                       std::move(cp_values)),
              monitor_)) {
        should_fail = true;
        break;
      }
    }
    if (should_fail) {
      solver->Fail();
    }
    return nullptr;
  }

 private:
  using Resource = RoutingModel::ResourceGroup::Resource;
  using RouteDimensionTravelInfo = RoutingModel::RouteDimensionTravelInfo;

  DimensionSchedulingStatus ComputeCumulAndBreakValuesForVehicle(
      LocalDimensionCumulOptimizer* optimizer, int vehicle,
      std::vector<int64_t>* cumul_values,
      std::vector<int64_t>* break_start_end_values) {
    cumul_values->clear();
    break_start_end_values->clear();
    RoutingModel* const model = optimizer->dimension()->model();
    const auto next = [model](int64_t n) { return model->NextVar(n)->Value(); };
    const RouteDimensionTravelInfo& dimension_travel_info =
        dimension_travel_info_per_route_.empty()
            ? RouteDimensionTravelInfo()
            : dimension_travel_info_per_route_[vehicle];
    const Resource* resource = nullptr;
    if (resource_group_index_ >= 0 &&
        model->ResourceVar(vehicle, resource_group_index_)->Bound()) {
      const int resource_index =
          model->ResourceVar(vehicle, resource_group_index_)->Value();
      if (resource_index >= 0) {
        resource = &model->GetResourceGroup(resource_group_index_)
                        ->GetResource(resource_index);
      }
    }
    return optimize_and_pack_
               ? optimizer->ComputePackedRouteCumuls(
                     vehicle, next, dimension_travel_info, resource,
                     cumul_values, break_start_end_values)
               : optimizer->ComputeRouteCumuls(
                     vehicle, next, dimension_travel_info, resource,
                     cumul_values, break_start_end_values);
  }

  LocalDimensionCumulOptimizer* const local_optimizer_;
  LocalDimensionCumulOptimizer* const local_mp_optimizer_;
  // Stores the resource group index of the local_[mp_]optimizer_'s dimension.
  int resource_group_index_;
  SearchMonitor* const monitor_;
  const bool optimize_and_pack_;
  const std::vector<RouteDimensionTravelInfo> dimension_travel_info_per_route_;
};

}  // namespace

DecisionBuilder* MakeSetCumulsFromLocalDimensionCosts(
    Solver* solver, LocalDimensionCumulOptimizer* local_optimizer,
    LocalDimensionCumulOptimizer* local_mp_optimizer, SearchMonitor* monitor,
    bool optimize_and_pack,
    std::vector<RoutingModel::RouteDimensionTravelInfo>
        dimension_travel_info_per_route) {
  return solver->RevAlloc(new SetCumulsFromLocalDimensionCosts(
      local_optimizer, local_mp_optimizer, monitor, optimize_and_pack,
      std::move(dimension_travel_info_per_route)));
}

namespace {

class SetCumulsFromGlobalDimensionCosts : public DecisionBuilder {
 public:
  SetCumulsFromGlobalDimensionCosts(
      GlobalDimensionCumulOptimizer* global_optimizer,
      GlobalDimensionCumulOptimizer* global_mp_optimizer,
      SearchMonitor* monitor, bool optimize_and_pack,
      std::vector<RoutingModel::RouteDimensionTravelInfo>
          dimension_travel_info_per_route)
      : global_optimizer_(global_optimizer),
        global_mp_optimizer_(global_mp_optimizer),
        monitor_(monitor),
        optimize_and_pack_(optimize_and_pack),
        dimension_travel_info_per_route_(
            std::move(dimension_travel_info_per_route)) {
    DCHECK(dimension_travel_info_per_route_.empty() ||
           dimension_travel_info_per_route_.size() ==
               global_optimizer_->dimension()->model()->vehicles());
  }

  Decision* Next(Solver* solver) override {
    // The following boolean variable indicates if the solver should fail, in
    // order to postpone the Fail() call until after the scope, so there are
    // no memory leaks related to the cumul_values vector.
    bool should_fail = false;
    {
      const RoutingDimension* dimension = global_optimizer_->dimension();
      DCHECK(DimensionFixedTransitsEqualTransitEvaluators(*dimension));
      RoutingModel* const model = dimension->model();

      GlobalDimensionCumulOptimizer* const optimizer =
          model->GetDimensionResourceGroupIndices(dimension).empty()
              ? global_optimizer_
              : global_mp_optimizer_;
      std::vector<int64_t> cumul_values;
      std::vector<int64_t> break_start_end_values;
      std::vector<std::vector<int>> resource_indices_per_group;
      const DimensionSchedulingStatus status =
          ComputeCumulBreakAndResourceValues(optimizer, &cumul_values,
                                             &break_start_end_values,
                                             &resource_indices_per_group);

      if (status == DimensionSchedulingStatus::INFEASIBLE) {
        should_fail = true;
      } else if (status == DimensionSchedulingStatus::RELAXED_OPTIMAL_ONLY) {
        // If relaxation is not feasible, try the MILP optimizer.
        const DimensionSchedulingStatus mp_status =
            ComputeCumulBreakAndResourceValues(
                global_mp_optimizer_, &cumul_values, &break_start_end_values,
                &resource_indices_per_group);
        if (mp_status != DimensionSchedulingStatus::OPTIMAL) {
          should_fail = true;
        }
      } else {
        DCHECK(status == DimensionSchedulingStatus::OPTIMAL);
      }
      if (!should_fail) {
        // Concatenate cumul_values and break_start_end_values into cp_values,
        // generate corresponding cp_variables vector.
        std::vector<IntVar*> cp_variables = dimension->cumuls();
        std::vector<int64_t> cp_values;
        std::swap(cp_values, cumul_values);
        if (dimension->HasBreakConstraints()) {
          const int num_vehicles = model->vehicles();
          for (int vehicle = 0; vehicle < num_vehicles; ++vehicle) {
            for (IntervalVar* interval :
                 dimension->GetBreakIntervalsOfVehicle(vehicle)) {
              cp_variables.push_back(interval->SafeStartExpr(0)->Var());
              cp_variables.push_back(interval->SafeEndExpr(0)->Var());
            }
          }
          cp_values.insert(cp_values.end(), break_start_end_values.begin(),
                           break_start_end_values.end());
        }
        for (int rg_index :
             model->GetDimensionResourceGroupIndices(dimension)) {
          const std::vector<int>& resource_values =
              resource_indices_per_group[rg_index];
          DCHECK(!resource_values.empty());
          cp_values.insert(cp_values.end(), resource_values.begin(),
                           resource_values.end());
          const std::vector<IntVar*>& resource_vars =
              model->ResourceVars(rg_index);
          DCHECK_EQ(resource_vars.size(), resource_values.size());
          cp_variables.insert(cp_variables.end(), resource_vars.begin(),
                              resource_vars.end());
        }
        // Value kint64min signals an unoptimized variable, set to min instead.
        for (int j = 0; j < cp_values.size(); ++j) {
          if (cp_values[j] == std::numeric_limits<int64_t>::min()) {
            cp_values[j] = cp_variables[j]->Min();
          }
        }
        if (!solver->SolveAndCommit(
                MakeSetValuesFromTargets(solver, std::move(cp_variables),
                                         std::move(cp_values)),
                monitor_)) {
          should_fail = true;
        }
      }
    }
    if (should_fail) {
      solver->Fail();
    }
    return nullptr;
  }

 private:
  DimensionSchedulingStatus ComputeCumulBreakAndResourceValues(
      GlobalDimensionCumulOptimizer* optimizer,
      std::vector<int64_t>* cumul_values,
      std::vector<int64_t>* break_start_end_values,
      std::vector<std::vector<int>>* resource_indices_per_group) {
    DCHECK_NE(optimizer, nullptr);
    cumul_values->clear();
    break_start_end_values->clear();
    resource_indices_per_group->clear();
    RoutingModel* const model = optimizer->dimension()->model();
    const auto next = [model](int64_t n) { return model->NextVar(n)->Value(); };
    return optimize_and_pack_
               ? optimizer->ComputePackedCumuls(
                     next, dimension_travel_info_per_route_, cumul_values,
                     break_start_end_values, resource_indices_per_group)
               : optimizer->ComputeCumuls(
                     next, dimension_travel_info_per_route_, cumul_values,
                     break_start_end_values, resource_indices_per_group);
  }

  GlobalDimensionCumulOptimizer* const global_optimizer_;
  GlobalDimensionCumulOptimizer* const global_mp_optimizer_;
  SearchMonitor* const monitor_;
  const bool optimize_and_pack_;
  const std::vector<RoutingModel::RouteDimensionTravelInfo>
      dimension_travel_info_per_route_;
};

}  // namespace

DecisionBuilder* MakeSetCumulsFromGlobalDimensionCosts(
    Solver* solver, GlobalDimensionCumulOptimizer* global_optimizer,
    GlobalDimensionCumulOptimizer* global_mp_optimizer, SearchMonitor* monitor,
    bool optimize_and_pack,
    std::vector<RoutingModel::RouteDimensionTravelInfo>
        dimension_travel_info_per_route) {
  return solver->RevAlloc(new SetCumulsFromGlobalDimensionCosts(
      global_optimizer, global_mp_optimizer, monitor, optimize_and_pack,
      std::move(dimension_travel_info_per_route)));
}

namespace {

class SetCumulsFromResourceAssignmentCosts : public DecisionBuilder {
 public:
  SetCumulsFromResourceAssignmentCosts(
      LocalDimensionCumulOptimizer* lp_optimizer,
      LocalDimensionCumulOptimizer* mp_optimizer, SearchMonitor* monitor)
      : model_(*lp_optimizer->dimension()->model()),
        dimension_(*lp_optimizer->dimension()),
        lp_optimizer_(lp_optimizer),
        mp_optimizer_(mp_optimizer),
        rg_index_(model_.GetDimensionResourceGroupIndex(&dimension_)),
        resource_group_(*model_.GetResourceGroup(rg_index_)),
        monitor_(monitor) {}

  Decision* Next(Solver* const solver) override {
    bool should_fail = false;
    {
      const int num_vehicles = model_.vehicles();
      std::vector<std::vector<int64_t>> assignment_costs(num_vehicles);
      std::vector<std::vector<std::vector<int64_t>>> cumul_values(num_vehicles);
      std::vector<std::vector<std::vector<int64_t>>> break_values(num_vehicles);

      const auto next = [&model = model_](int64_t n) {
        return model.NextVar(n)->Value();
      };
      DCHECK(DimensionFixedTransitsEqualTransitEvaluators(dimension_));

      for (int v : resource_group_.GetVehiclesRequiringAResource()) {
        if (!ComputeVehicleToResourcesAssignmentCosts(
                v, resource_group_, next, dimension_.transit_evaluator(v),
                /*optimize_vehicle_costs*/ true, lp_optimizer_, mp_optimizer_,
                &assignment_costs[v], &cumul_values[v], &break_values[v])) {
          should_fail = true;
          break;
        }
      }

      std::vector<int> resource_indices(num_vehicles);
      should_fail =
          should_fail ||
          ComputeBestVehicleToResourceAssignment(
              resource_group_.GetVehiclesRequiringAResource(),
              resource_group_.Size(),
              [&assignment_costs](int v) { return &assignment_costs[v]; },
              &resource_indices) < 0;

      if (!should_fail) {
        DCHECK_EQ(resource_indices.size(), num_vehicles);
        const int num_resources = resource_group_.Size();
        for (int v : resource_group_.GetVehiclesRequiringAResource()) {
          if (next(model_.Start(v)) == model_.End(v) &&
              !model_.IsVehicleUsedWhenEmpty(v)) {
            continue;
          }
          const int resource_index = resource_indices[v];
          DCHECK_GE(resource_index, 0);
          DCHECK_EQ(cumul_values[v].size(), num_resources);
          DCHECK_EQ(break_values[v].size(), num_resources);
          const std::vector<int64_t>& optimal_cumul_values =
              cumul_values[v][resource_index];
          const std::vector<int64_t>& optimal_break_values =
              break_values[v][resource_index];
          std::vector<IntVar*> cp_variables;
          std::vector<int64_t> cp_values;
          ConcatenateRouteCumulAndBreakVarAndValues(
              dimension_, v, optimal_cumul_values, optimal_break_values,
              &cp_variables, &cp_values);

          const std::vector<IntVar*>& resource_vars =
              model_.ResourceVars(rg_index_);
          DCHECK_EQ(resource_vars.size(), resource_indices.size());
          cp_variables.insert(cp_variables.end(), resource_vars.begin(),
                              resource_vars.end());
          cp_values.insert(cp_values.end(), resource_indices.begin(),
                           resource_indices.end());
          if (!solver->SolveAndCommit(
                  MakeSetValuesFromTargets(solver, std::move(cp_variables),
                                           std::move(cp_values)),
                  monitor_)) {
            should_fail = true;
            break;
          }
        }
      }
    }
    if (should_fail) {
      solver->Fail();
    }
    return nullptr;
  }

 private:
  const RoutingModel& model_;
  const RoutingDimension& dimension_;
  LocalDimensionCumulOptimizer* lp_optimizer_;
  LocalDimensionCumulOptimizer* mp_optimizer_;
  const int rg_index_;
  const RoutingModel::ResourceGroup& resource_group_;
  SearchMonitor* const monitor_;
};

}  // namespace

DecisionBuilder* MakeSetCumulsFromResourceAssignmentCosts(
    Solver* solver, LocalDimensionCumulOptimizer* lp_optimizer,
    LocalDimensionCumulOptimizer* mp_optimizer, SearchMonitor* monitor) {
  return solver->RevAlloc(new SetCumulsFromResourceAssignmentCosts(
      lp_optimizer, mp_optimizer, monitor));
}
namespace {
// A decision builder that tries to set variables to their value in the last
// solution, if their corresponding vehicle path has not changed.
// This tries to constrain all such variables in one shot in order to speed up
// instantiation.
// TODO(user): try to use Assignment instead of MakeAssignment(),
//   try to record and restore the min/max instead of a single value.
class RestoreDimensionValuesForUnchangedRoutes : public DecisionBuilder {
 public:
  explicit RestoreDimensionValuesForUnchangedRoutes(RoutingModel* model)
      : model_(model) {
    model_->AddAtSolutionCallback([this]() { AtSolution(); });
    next_last_value_.resize(model_->Nexts().size(), -1);
  }

  // In a given branch of a search tree, this decision builder only returns
  // a Decision once, the first time it is called in that branch.
  Decision* Next(Solver* const s) override {
    if (!must_return_decision_) return nullptr;
    s->SaveAndSetValue(&must_return_decision_, false);
    return MakeDecision(s);
  }

 private:
  // Initialize() is lazy to make sure all dimensions have been instantiated
  // when initialization is done.
  void Initialize() {
    is_initialized_ = true;
    const int num_nodes = model_->VehicleVars().size();
    node_to_integer_variable_indices_.resize(num_nodes);
    node_to_interval_variable_indices_.resize(num_nodes);
    // Search for dimension variables that correspond to input variables.
    for (const std::string& dimension_name : model_->GetAllDimensionNames()) {
      const RoutingDimension& dimension =
          model_->GetDimensionOrDie(dimension_name);
      // Search among cumuls and slacks, and attach them to corresponding nodes.
      for (const std::vector<IntVar*>& dimension_variables :
           {dimension.cumuls(), dimension.slacks()}) {
        const int num_dimension_variables = dimension_variables.size();
        DCHECK_LE(num_dimension_variables, num_nodes);
        for (int node = 0; node < num_dimension_variables; ++node) {
          node_to_integer_variable_indices_[node].push_back(
              integer_variables_.size());
          integer_variables_.push_back(dimension_variables[node]);
        }
      }
      // Search for break start/end variables, attach them to vehicle starts.
      for (int vehicle = 0; vehicle < model_->vehicles(); ++vehicle) {
        if (!dimension.HasBreakConstraints()) continue;
        const int vehicle_start = model_->Start(vehicle);
        for (IntervalVar* interval :
             dimension.GetBreakIntervalsOfVehicle(vehicle)) {
          node_to_interval_variable_indices_[vehicle_start].push_back(
              interval_variables_.size());
          interval_variables_.push_back(interval);
        }
      }
    }
    integer_variables_last_min_.resize(integer_variables_.size());
    interval_variables_last_start_min_.resize(interval_variables_.size());
    interval_variables_last_end_max_.resize(interval_variables_.size());
  }

  Decision* MakeDecision(Solver* const s) {
    if (!is_initialized_) return nullptr;
    // Collect vehicles that have not changed.
    std::vector<int> unchanged_vehicles;
    const int num_vehicles = model_->vehicles();
    for (int v = 0; v < num_vehicles; ++v) {
      bool unchanged = true;
      for (int current = model_->Start(v); !model_->IsEnd(current);
           current = next_last_value_[current]) {
        if (!model_->NextVar(current)->Bound() ||
            next_last_value_[current] != model_->NextVar(current)->Value()) {
          unchanged = false;
          break;
        }
      }
      if (unchanged) unchanged_vehicles.push_back(v);
    }
    // If all routes are unchanged, the solver might be trying to do a full
    // reschedule. Do nothing.
    if (unchanged_vehicles.size() == num_vehicles) return nullptr;

    // Collect cumuls and slacks of unchanged routes to be assigned a value.
    std::vector<IntVar*> vars;
    std::vector<int64_t> values;
    for (const int vehicle : unchanged_vehicles) {
      for (int current = model_->Start(vehicle); true;
           current = next_last_value_[current]) {
        for (const int index : node_to_integer_variable_indices_[current]) {
          vars.push_back(integer_variables_[index]);
          values.push_back(integer_variables_last_min_[index]);
        }
        for (const int index : node_to_interval_variable_indices_[current]) {
          const int64_t start_min = interval_variables_last_start_min_[index];
          const int64_t end_max = interval_variables_last_end_max_[index];
          if (start_min < end_max) {
            vars.push_back(interval_variables_[index]->SafeStartExpr(0)->Var());
            values.push_back(interval_variables_last_start_min_[index]);
            vars.push_back(interval_variables_[index]->SafeEndExpr(0)->Var());
            values.push_back(interval_variables_last_end_max_[index]);
          } else {
            vars.push_back(interval_variables_[index]->PerformedExpr()->Var());
            values.push_back(0);
          }
        }
        if (model_->IsEnd(current)) break;
      }
    }
    return s->MakeAssignVariablesValuesOrDoNothing(vars, values);
  }

  void AtSolution() {
    if (!is_initialized_) Initialize();
    const int num_integers = integer_variables_.size();
    // Variables may not be fixed at solution time,
    // the decision builder is fine with the Min() of the unfixed variables.
    for (int i = 0; i < num_integers; ++i) {
      integer_variables_last_min_[i] = integer_variables_[i]->Min();
    }
    const int num_intervals = interval_variables_.size();
    for (int i = 0; i < num_intervals; ++i) {
      const bool is_performed = interval_variables_[i]->MustBePerformed();
      interval_variables_last_start_min_[i] =
          is_performed ? interval_variables_[i]->StartMin() : 0;
      interval_variables_last_end_max_[i] =
          is_performed ? interval_variables_[i]->EndMax() : -1;
    }
    const int num_nodes = next_last_value_.size();
    for (int node = 0; node < num_nodes; ++node) {
      if (model_->IsEnd(node)) continue;
      next_last_value_[node] = model_->NextVar(node)->Value();
    }
  }

  // Input data.
  RoutingModel* const model_;

  // The valuation of the last solution.
  std::vector<int> next_last_value_;
  // For every node, the indices of integer_variables_ and interval_variables_
  // that correspond to that node.
  std::vector<std::vector<int>> node_to_integer_variable_indices_;
  std::vector<std::vector<int>> node_to_interval_variable_indices_;
  // Variables and the value they had in the previous solution.
  std::vector<IntVar*> integer_variables_;
  std::vector<int64_t> integer_variables_last_min_;
  std::vector<IntervalVar*> interval_variables_;
  std::vector<int64_t> interval_variables_last_start_min_;
  std::vector<int64_t> interval_variables_last_end_max_;

  bool is_initialized_ = false;
  bool must_return_decision_ = true;
};
}  // namespace

DecisionBuilder* MakeRestoreDimensionValuesForUnchangedRoutes(
    RoutingModel* model) {
  return model->solver()->RevAlloc(
      new RestoreDimensionValuesForUnchangedRoutes(model));
}

// FinalizerVariables

void FinalizerVariables::AddWeightedVariableTarget(IntVar* var, int64_t target,
                                                   int64_t cost) {
  CHECK(var != nullptr);
  const int index =
      gtl::LookupOrInsert(&weighted_finalizer_variable_index_, var,
                          weighted_finalizer_variable_targets_.size());
  if (index < weighted_finalizer_variable_targets_.size()) {
    auto& [var_target, total_cost] =
        weighted_finalizer_variable_targets_[index];
    DCHECK_EQ(var_target.var, var);
    DCHECK_EQ(var_target.target, target);
    total_cost = CapAdd(total_cost, cost);
  } else {
    DCHECK_EQ(index, weighted_finalizer_variable_targets_.size());
    weighted_finalizer_variable_targets_.push_back({{var, target}, cost});
  }
}

void FinalizerVariables::AddWeightedVariableToMinimize(IntVar* var,
                                                       int64_t cost) {
  AddWeightedVariableTarget(var, std::numeric_limits<int64_t>::min(), cost);
}

void FinalizerVariables::AddWeightedVariableToMaximize(IntVar* var,
                                                       int64_t cost) {
  AddWeightedVariableTarget(var, std::numeric_limits<int64_t>::max(), cost);
}

void FinalizerVariables::AddVariableTarget(IntVar* var, int64_t target) {
  CHECK(var != nullptr);
  if (finalizer_variable_target_set_.contains(var)) return;
  finalizer_variable_target_set_.insert(var);
  finalizer_variable_targets_.push_back({var, target});
}

void FinalizerVariables::AddVariableToMaximize(IntVar* var) {
  AddVariableTarget(var, std::numeric_limits<int64_t>::max());
}

void FinalizerVariables::AddVariableToMinimize(IntVar* var) {
  AddVariableTarget(var, std::numeric_limits<int64_t>::min());
}

DecisionBuilder* FinalizerVariables::CreateFinalizer() {
  std::stable_sort(weighted_finalizer_variable_targets_.begin(),
                   weighted_finalizer_variable_targets_.end(),
                   [](const std::pair<VarTarget, int64_t>& var_cost1,
                      const std::pair<VarTarget, int64_t>& var_cost2) {
                     return var_cost1.second > var_cost2.second;
                   });
  const int num_variables = weighted_finalizer_variable_targets_.size() +
                            finalizer_variable_targets_.size();
  std::vector<IntVar*> variables;
  std::vector<int64_t> targets;
  variables.reserve(num_variables);
  targets.reserve(num_variables);
  for (const auto& [var_target, cost] : weighted_finalizer_variable_targets_) {
    variables.push_back(var_target.var);
    targets.push_back(var_target.target);
  }
  for (const auto& [var, target] : finalizer_variable_targets_) {
    variables.push_back(var);
    targets.push_back(target);
  }
  return MakeSetValuesFromTargets(solver_, std::move(variables),
                                  std::move(targets));
}

}  // namespace operations_research

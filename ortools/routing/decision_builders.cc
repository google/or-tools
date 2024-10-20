// Copyright 2010-2024 Google LLC
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

#include "ortools/routing/decision_builders.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/map_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/routing/lp_scheduling.h"
#include "ortools/routing/routing.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research::routing {

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
void AppendRouteCumulAndBreakVarAndValues(
    const RoutingDimension& dimension, int vehicle,
    absl::Span<const int64_t> cumul_values,
    absl::Span<const int64_t> break_values, std::vector<IntVar*>* variables,
    std::vector<int64_t>* values) {
  auto& vars = *variables;
  auto& vals = *values;
  DCHECK_EQ(vars.size(), vals.size());
  const int old_num_values = vals.size();
  vals.insert(vals.end(), cumul_values.begin(), cumul_values.end());
  const RoutingModel& model = *dimension.model();
  {
    int current = model.Start(vehicle);
    while (true) {
      vars.push_back(dimension.CumulVar(current));
      if (!model.IsEnd(current)) {
        current = model.NextVar(current)->Value();
      } else {
        break;
      }
    }
  }
  if (dimension.HasBreakConstraints()) {
    for (IntervalVar* interval :
         dimension.GetBreakIntervalsOfVehicle(vehicle)) {
      vars.push_back(interval->SafeStartExpr(0)->Var());
      vars.push_back(interval->SafeEndExpr(0)->Var());
    }
    vals.insert(vals.end(), break_values.begin(), break_values.end());
  }
  DCHECK_EQ(vars.size(), vals.size());
  int new_num_values = old_num_values;
  for (int j = old_num_values; j < vals.size(); ++j) {
    // Value kint64min signals an unoptimized variable, skip setting those.
    if (vals[j] == std::numeric_limits<int64_t>::min()) continue;
    // Skip variables that are not bound.
    if (vars[j]->Bound()) continue;
    vals[new_num_values] = vals[j];
    vars[new_num_values] = vars[j];
    ++new_num_values;
  }
  vars.resize(new_num_values);
  vals.resize(new_num_values);
}

class SetCumulsFromLocalDimensionCosts : public DecisionBuilder {
 public:
  SetCumulsFromLocalDimensionCosts(
      LocalDimensionCumulOptimizer* lp_optimizer,
      LocalDimensionCumulOptimizer* mp_optimizer, bool optimize_and_pack,
      std::vector<RoutingModel::RouteDimensionTravelInfo>
          dimension_travel_info_per_route)
      : model_(*lp_optimizer->dimension()->model()),
        dimension_(*lp_optimizer->dimension()),
        lp_optimizer_(lp_optimizer),
        mp_optimizer_(mp_optimizer),
        rg_index_(model_.GetDimensionResourceGroupIndices(&dimension_).empty()
                      ? -1
                      : model_.GetDimensionResourceGroupIndex(&dimension_)),
        resource_group_(rg_index_ >= 0 ? model_.GetResourceGroup(rg_index_)
                                       : nullptr),
        vehicle_resource_class_values_(model_.vehicles()),
        optimize_and_pack_(optimize_and_pack),
        dimension_travel_info_per_route_(
            std::move(dimension_travel_info_per_route)),
        decision_level_(0) {
    if (!dimension_travel_info_per_route_.empty()) {
      DCHECK(optimize_and_pack_);
      DCHECK_EQ(dimension_travel_info_per_route_.size(), model_.vehicles());
    }
  }

  Decision* Next(Solver* solver) override {
    if (decision_level_.Value() == 2) return nullptr;
    if (decision_level_.Value() == 1) {
      Decision* d = set_values_from_targets_->Next(solver);
      if (d == nullptr) decision_level_.SetValue(solver, 2);
      return d;
    }
    decision_level_.SetValue(solver, 1);
    if (!FillCPVariablesAndValues(solver)) {
      solver->Fail();
    }
    set_values_from_targets_ =
        MakeSetValuesFromTargets(solver, cp_variables_, cp_values_);
    return solver->MakeAssignVariablesValuesOrDoNothing(cp_variables_,
                                                        cp_values_);
  }

 private:
  using Resource = RoutingModel::ResourceGroup::Resource;
  using RCIndex = RoutingModel::ResourceClassIndex;
  using RouteDimensionTravelInfo = RoutingModel::RouteDimensionTravelInfo;

  bool FillCPVariablesAndValues(Solver* solver) {
    DCHECK(DimensionFixedTransitsEqualTransitEvaluators(dimension_));
    cp_variables_.clear();
    cp_values_.clear();

    std::vector<int> vehicles_without_resource_assignment;
    std::vector<int> vehicles_with_resource_assignment;
    util_intops::StrongVector<RCIndex, absl::flat_hash_set<int>>
        used_resources_per_class;
    DetermineVehiclesRequiringResourceAssignment(
        &vehicles_without_resource_assignment,
        &vehicles_with_resource_assignment, &used_resources_per_class);

    const auto next = [&model = model_](int64_t n) {
      return model.NextVar(n)->Value();
    };

    // First look at vehicles that do not need resource assignment (fewer/faster
    // computations).
    for (int vehicle : vehicles_without_resource_assignment) {
      solver->TopPeriodicCheck();
      std::vector<int64_t> cumul_values;
      std::vector<int64_t> break_start_end_values;
      if (!ComputeCumulAndBreakValuesForVehicle(vehicle, next, &cumul_values,
                                                &break_start_end_values)) {
        return false;
      }
      AppendRouteCumulAndBreakVarAndValues(dimension_, vehicle, cumul_values,
                                           break_start_end_values,
                                           &cp_variables_, &cp_values_);
    }

    if (vehicles_with_resource_assignment.empty()) {
      return true;
    }

    // Do resource assignment for the vehicles requiring it and append the
    // corresponding var and values.
    std::vector<int> resource_indices;
    if (!ComputeVehicleResourceClassValuesAndIndices(
            vehicles_with_resource_assignment, used_resources_per_class, next,
            &resource_indices)) {
      return false;
    }
    DCHECK_EQ(resource_indices.size(), model_.vehicles());
    const int num_resource_classes = resource_group_->GetResourceClassesCount();
    for (int v : vehicles_with_resource_assignment) {
      DCHECK(next(model_.Start(v)) != model_.End(v) ||
             model_.IsVehicleUsedWhenEmpty(v));
      const auto& [unused, cumul_values, break_values] =
          vehicle_resource_class_values_[v];
      const int resource_index = resource_indices[v];
      DCHECK_GE(resource_index, 0);
      DCHECK_EQ(cumul_values.size(), num_resource_classes);
      DCHECK_EQ(break_values.size(), num_resource_classes);
      const int rc_index =
          resource_group_->GetResourceClassIndex(resource_index).value();
      const std::vector<int64_t>& optimal_cumul_values = cumul_values[rc_index];
      const std::vector<int64_t>& optimal_break_values = break_values[rc_index];
      AppendRouteCumulAndBreakVarAndValues(dimension_, v, optimal_cumul_values,
                                           optimal_break_values, &cp_variables_,
                                           &cp_values_);

      const std::vector<IntVar*>& resource_vars =
          model_.ResourceVars(rg_index_);
      DCHECK_EQ(resource_vars.size(), resource_indices.size());
      cp_variables_.insert(cp_variables_.end(), resource_vars.begin(),
                           resource_vars.end());
      cp_values_.insert(cp_values_.end(), resource_indices.begin(),
                        resource_indices.end());
    }
    return true;
  }

  void DetermineVehiclesRequiringResourceAssignment(
      std::vector<int>* vehicles_without_resource_assignment,
      std::vector<int>* vehicles_with_resource_assignment,
      util_intops::StrongVector<RCIndex, absl::flat_hash_set<int>>*
          used_resources_per_class) const {
    vehicles_without_resource_assignment->clear();
    vehicles_with_resource_assignment->clear();
    used_resources_per_class->clear();
    if (rg_index_ < 0) {
      vehicles_without_resource_assignment->reserve(model_.vehicles());
      for (int v = 0; v < model_.vehicles(); ++v) {
        vehicles_without_resource_assignment->push_back(v);
      }
      return;
    }
    DCHECK_NE(resource_group_, nullptr);
    const int num_vehicles_req_res =
        resource_group_->GetVehiclesRequiringAResource().size();
    vehicles_without_resource_assignment->reserve(model_.vehicles() -
                                                  num_vehicles_req_res);
    vehicles_with_resource_assignment->reserve(num_vehicles_req_res);
    used_resources_per_class->resize(
        resource_group_->GetResourceClassesCount());
    for (int v = 0; v < model_.vehicles(); ++v) {
      if (!resource_group_->VehicleRequiresAResource(v)) {
        vehicles_without_resource_assignment->push_back(v);
      } else if (model_.NextVar(model_.Start(v))->Value() == model_.End(v) &&
                 !model_.IsVehicleUsedWhenEmpty(v)) {
        // No resource assignment required for this unused vehicle.
        // TODO(user): Investigate if we should skip unused vehicles.
        vehicles_without_resource_assignment->push_back(v);
      } else if (model_.ResourceVar(v, rg_index_)->Bound()) {
        vehicles_without_resource_assignment->push_back(v);
        const int resource_idx = model_.ResourceVar(v, rg_index_)->Value();
        DCHECK_GE(resource_idx, 0);
        used_resources_per_class
            ->at(resource_group_->GetResourceClassIndex(resource_idx))
            .insert(resource_idx);
      } else {
        vehicles_with_resource_assignment->push_back(v);
      }
    }
  }

  bool ComputeCumulAndBreakValuesForVehicle(
      int vehicle, const std::function<int64_t(int64_t)>& next_accessor,
      std::vector<int64_t>* cumul_values,
      std::vector<int64_t>* break_start_end_values) {
    cumul_values->clear();
    break_start_end_values->clear();
    const RouteDimensionTravelInfo* const dimension_travel_info =
        dimension_travel_info_per_route_.empty()
            ? nullptr
            : &dimension_travel_info_per_route_[vehicle];
    const Resource* resource = nullptr;
    if (rg_index_ >= 0 && model_.ResourceVar(vehicle, rg_index_)->Bound()) {
      const int resource_index =
          model_.ResourceVar(vehicle, rg_index_)->Value();
      if (resource_index >= 0) {
        resource =
            &model_.GetResourceGroup(rg_index_)->GetResource(resource_index);
      }
    }
    const bool use_mp_optimizer =
        dimension_.HasQuadraticCostSoftSpanUpperBounds() ||
        (dimension_.HasBreakConstraints() &&
         !dimension_.GetBreakIntervalsOfVehicle(vehicle).empty());
    LocalDimensionCumulOptimizer* const optimizer =
        use_mp_optimizer ? mp_optimizer_ : lp_optimizer_;
    DCHECK_NE(optimizer, nullptr);
    DimensionSchedulingStatus status =
        optimize_and_pack_
            ? optimizer->ComputePackedRouteCumuls(
                  vehicle, next_accessor, dimension_travel_info, resource,
                  cumul_values, break_start_end_values)
            : optimizer->ComputeRouteCumuls(
                  vehicle, next_accessor, dimension_travel_info, resource,
                  cumul_values, break_start_end_values);
    if (status == DimensionSchedulingStatus::INFEASIBLE) {
      return false;
    }
    // If relaxation is not feasible, try the MP optimizer.
    if (status == DimensionSchedulingStatus::RELAXED_OPTIMAL_ONLY) {
      DCHECK(!use_mp_optimizer);
      DCHECK_NE(mp_optimizer_, nullptr);
      status = optimize_and_pack_
                   ? mp_optimizer_->ComputePackedRouteCumuls(
                         vehicle, next_accessor, dimension_travel_info,
                         resource, cumul_values, break_start_end_values)
                   : mp_optimizer_->ComputeRouteCumuls(
                         vehicle, next_accessor, dimension_travel_info,
                         resource, cumul_values, break_start_end_values);
      if (status == DimensionSchedulingStatus::INFEASIBLE) {
        return false;
      }
    } else {
      DCHECK(status == DimensionSchedulingStatus::OPTIMAL);
    }
    return true;
  }

  bool ComputeVehicleResourceClassValuesAndIndices(
      absl::Span<const int> vehicles_to_assign,
      const util_intops::StrongVector<RCIndex, absl::flat_hash_set<int>>&
          used_resources_per_class,
      const std::function<int64_t(int64_t)>& next_accessor,
      std::vector<int>* resource_indices) {
    resource_indices->assign(model_.vehicles(), -1);
    if (vehicles_to_assign.empty()) return true;
    DCHECK_NE(resource_group_, nullptr);

    for (int v : vehicles_to_assign) {
      DCHECK(resource_group_->VehicleRequiresAResource(v));
      auto& [assignment_costs, cumul_values, break_values] =
          vehicle_resource_class_values_[v];
      if (!ComputeVehicleToResourceClassAssignmentCosts(
              v, *resource_group_, used_resources_per_class, next_accessor,
              dimension_.transit_evaluator(v),
              /*optimize_vehicle_costs*/ true, lp_optimizer_, mp_optimizer_,
              &assignment_costs, &cumul_values, &break_values)) {
        return false;
      }
    }

    return ComputeBestVehicleToResourceAssignment(
               vehicles_to_assign,
               resource_group_->GetResourceIndicesPerClass(),
               used_resources_per_class,
               [&vehicle_rc_values = vehicle_resource_class_values_](int v) {
                 return &vehicle_rc_values[v].assignment_costs;
               },
               resource_indices) >= 0;
  }

  const RoutingModel& model_;
  const RoutingDimension& dimension_;
  LocalDimensionCumulOptimizer* lp_optimizer_;
  LocalDimensionCumulOptimizer* mp_optimizer_;
  // Stores the resource group index of the lp_/mp_optimizer_'s dimension, if
  // there is any.
  const int rg_index_;
  const RoutingModel::ResourceGroup* const resource_group_;
  // Stores the information related to assigning a given vehicle to resource
  // classes. We keep these as class members to avoid unnecessary memory
  // reallocations.
  struct VehicleResourceClassValues {
    std::vector<int64_t> assignment_costs;
    std::vector<std::vector<int64_t>> cumul_values;
    std::vector<std::vector<int64_t>> break_values;
  };
  std::vector<VehicleResourceClassValues> vehicle_resource_class_values_;
  const bool optimize_and_pack_;
  const std::vector<RouteDimensionTravelInfo> dimension_travel_info_per_route_;
  std::vector<IntVar*> cp_variables_;
  std::vector<int64_t> cp_values_;
  // Decision level of this decision builder:
  // - level 0: set remaining dimension values at once.
  // - level 1: set remaining dimension values one by one.
  Rev<int> decision_level_;
  DecisionBuilder* set_values_from_targets_ = nullptr;
};

}  // namespace

DecisionBuilder* MakeSetCumulsFromLocalDimensionCosts(
    Solver* solver, LocalDimensionCumulOptimizer* lp_optimizer,
    LocalDimensionCumulOptimizer* mp_optimizer, bool optimize_and_pack,
    std::vector<RoutingModel::RouteDimensionTravelInfo>
        dimension_travel_info_per_route) {
  return solver->RevAlloc(new SetCumulsFromLocalDimensionCosts(
      lp_optimizer, mp_optimizer, optimize_and_pack,
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
            std::move(dimension_travel_info_per_route)),
        decision_level_(0) {
    DCHECK(dimension_travel_info_per_route_.empty() ||
           dimension_travel_info_per_route_.size() ==
               global_optimizer_->dimension()->model()->vehicles());
    // Store the cp variables used to set values on in Next().
    // NOTE: The order is important as we use the same order to add values
    // in cp_values_.
    const RoutingDimension* dimension = global_optimizer_->dimension();
    const RoutingModel* model = dimension->model();
    cp_variables_ = dimension->cumuls();
    if (dimension->HasBreakConstraints()) {
      for (int vehicle = 0; vehicle < model->vehicles(); ++vehicle) {
        for (IntervalVar* interval :
             dimension->GetBreakIntervalsOfVehicle(vehicle)) {
          cp_variables_.push_back(interval->SafeStartExpr(0)->Var());
          cp_variables_.push_back(interval->SafeEndExpr(0)->Var());
        }
      }
    }
    // NOTE: When packing, the resource variables should already have a bound
    // value which is taken into account by the optimizer, so we don't set them
    // in MakeSetValuesFromTargets().
    if (!optimize_and_pack_) {
      for (int rg_index : model->GetDimensionResourceGroupIndices(dimension)) {
        const std::vector<IntVar*>& res_vars = model->ResourceVars(rg_index);
        cp_variables_.insert(cp_variables_.end(), res_vars.begin(),
                             res_vars.end());
      }
    }
  }

  Decision* Next(Solver* solver) override {
    if (decision_level_.Value() == 2) return nullptr;
    if (decision_level_.Value() == 1) {
      Decision* d = set_values_from_targets_->Next(solver);
      if (d == nullptr) decision_level_.SetValue(solver, 2);
      return d;
    }
    decision_level_.SetValue(solver, 1);
    if (!FillCPValues()) {
      solver->Fail();
    }
    set_values_from_targets_ =
        MakeSetValuesFromTargets(solver, cp_variables_, cp_values_);
    return solver->MakeAssignVariablesValuesOrDoNothing(cp_variables_,
                                                        cp_values_);
  }

 private:
  bool FillCPValues() {
    const RoutingDimension* dimension = global_optimizer_->dimension();
    DCHECK(DimensionFixedTransitsEqualTransitEvaluators(*dimension));
    RoutingModel* const model = dimension->model();

    GlobalDimensionCumulOptimizer* const optimizer =
        model->GetDimensionResourceGroupIndices(dimension).empty()
            ? global_optimizer_
            : global_mp_optimizer_;
    const DimensionSchedulingStatus status = ComputeCumulBreakAndResourceValues(
        optimizer, &cumul_values_, &break_start_end_values_,
        &resource_indices_per_group_);

    if (status == DimensionSchedulingStatus::INFEASIBLE) {
      return false;
    } else if (status == DimensionSchedulingStatus::RELAXED_OPTIMAL_ONLY) {
      // If relaxation is not feasible, try the MILP optimizer.
      const DimensionSchedulingStatus mp_status =
          ComputeCumulBreakAndResourceValues(
              global_mp_optimizer_, &cumul_values_, &break_start_end_values_,
              &resource_indices_per_group_);
      if (mp_status != DimensionSchedulingStatus::OPTIMAL) {
        return false;
      }
    } else {
      DCHECK(status == DimensionSchedulingStatus::OPTIMAL);
    }
    // Concatenate cumul_values_, break_start_end_values_ and all
    // resource_indices_per_group_ into cp_values_.
    // NOTE: The order is important as it corresponds to the order of
    // variables in cp_variables_.
    cp_values_ = std::move(cumul_values_);
    if (dimension->HasBreakConstraints()) {
      cp_values_.insert(cp_values_.end(), break_start_end_values_.begin(),
                        break_start_end_values_.end());
    }
    if (optimize_and_pack_) {
// Resource variables should be bound when packing, so we don't need
// to restore them again.
#ifndef NDEBUG
      for (int rg_index : model->GetDimensionResourceGroupIndices(dimension)) {
        for (IntVar* res_var : model->ResourceVars(rg_index)) {
          DCHECK(res_var->Bound());
        }
      }
#endif
    } else {
      // Add resource values to cp_values_.
      for (int rg_index : model->GetDimensionResourceGroupIndices(dimension)) {
        const std::vector<int>& resource_values =
            resource_indices_per_group_[rg_index];
        DCHECK(!resource_values.empty());
        cp_values_.insert(cp_values_.end(), resource_values.begin(),
                          resource_values.end());
      }
    }
    DCHECK_EQ(cp_variables_.size(), cp_values_.size());
    // Value kint64min signals an unoptimized variable, set to min instead.
    for (int j = 0; j < cp_values_.size(); ++j) {
      if (cp_values_[j] == std::numeric_limits<int64_t>::min()) {
        cp_values_[j] = cp_variables_[j]->Min();
      }
    }
    return true;
  }

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
                     break_start_end_values)
               : optimizer->ComputeCumuls(
                     next, dimension_travel_info_per_route_, cumul_values,
                     break_start_end_values, resource_indices_per_group);
  }

  GlobalDimensionCumulOptimizer* const global_optimizer_;
  GlobalDimensionCumulOptimizer* const global_mp_optimizer_;
  SearchMonitor* const monitor_;
  const bool optimize_and_pack_;
  std::vector<IntVar*> cp_variables_;
  std::vector<int64_t> cp_values_;
  // The following 3 members are stored internally to avoid unnecessary memory
  // reallocations.
  std::vector<int64_t> cumul_values_;
  std::vector<int64_t> break_start_end_values_;
  std::vector<std::vector<int>> resource_indices_per_group_;
  const std::vector<RoutingModel::RouteDimensionTravelInfo>
      dimension_travel_info_per_route_;
  // Decision level of this decision builder:
  // - level 0: set remaining dimension values at once.
  // - level 1: set remaining dimension values one by one.
  Rev<int> decision_level_;
  DecisionBuilder* set_values_from_targets_ = nullptr;
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
    model_->AddRestoreDimensionValuesResetCallback([this]() { Reset(); });
    next_last_value_.resize(model_->Nexts().size(), -1);
  }

  // In a given branch of a search tree, this decision builder only returns
  // a Decision once, the first time it is called in that branch.
  Decision* Next(Solver* const s) override {
    if (!must_return_decision_) return nullptr;
    s->SaveAndSetValue(&must_return_decision_, false);
    return MakeDecision(s);
  }

  void Reset() { next_last_value_.assign(model_->Nexts().size(), -1); }

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

}  // namespace operations_research::routing

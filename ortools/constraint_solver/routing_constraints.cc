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

#include "ortools/constraint_solver/routing_constraints.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "ortools/base/strong_vector.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_lp_scheduling.h"
#include "ortools/constraint_solver/routing_search.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {

namespace {
// Constraint which ensures that var != values.
class DifferentFromValues : public Constraint {
 public:
  DifferentFromValues(Solver* solver, IntVar* var, std::vector<int64_t> values)
      : Constraint(solver), var_(var), values_(std::move(values)) {}
  void Post() override {}
  void InitialPropagate() override { var_->RemoveValues(values_); }
  std::string DebugString() const override { return "DifferentFromValues"; }
  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(RoutingModelVisitor::kRemoveValues, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               {var_});
    visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument, values_);
    visitor->EndVisitConstraint(RoutingModelVisitor::kRemoveValues, this);
  }

 private:
  IntVar* const var_;
  const std::vector<int64_t> values_;
};
}  // namespace

Constraint* MakeDifferentFromValues(Solver* solver, IntVar* var,
                                    std::vector<int64_t> values) {
  return solver->RevAlloc(
      new DifferentFromValues(solver, var, std::move(values)));
}

namespace {
// For each vehicle, computes information on the partially fixed start/end
// chains (based on bound NextVar values):
// - For every 'end_node', the last node of a start chain of a vehicle,
//   vehicle_index_of_start_chain_end[end_node] contains the corresponding
//   vehicle index. Contains -1 for other nodes.
// - For every vehicle 'v', end_chain_starts[v] contains the first node of the
//   end chain of that vehicle.
void ComputeVehicleChainStartEndInfo(
    const RoutingModel& model, std::vector<int64_t>* end_chain_starts,
    std::vector<int>* vehicle_index_of_start_chain_end) {
  vehicle_index_of_start_chain_end->resize(model.Size() + model.vehicles(), -1);

  for (int vehicle = 0; vehicle < model.vehicles(); ++vehicle) {
    int64_t node = model.Start(vehicle);
    while (!model.IsEnd(node) && model.NextVar(node)->Bound()) {
      node = model.NextVar(node)->Value();
    }
    vehicle_index_of_start_chain_end->at(node) = vehicle;
  }

  *end_chain_starts = ComputeVehicleEndChainStarts(model);
}

class ResourceAssignmentConstraint : public Constraint {
 public:
  ResourceAssignmentConstraint(
      const RoutingModel::ResourceGroup* resource_group,
      const std::vector<IntVar*>* vehicle_resource_vars, RoutingModel* model)
      : Constraint(model->solver()),
        model_(*model),
        resource_group_(*resource_group),
        vehicle_resource_vars_(*vehicle_resource_vars) {
    DCHECK_EQ(vehicle_resource_vars_.size(), model_.vehicles());

    const std::vector<RoutingDimension*>& dimensions = model_.GetDimensions();
    for (int v = 0; v < model_.vehicles(); v++) {
      IntVar* const resource_var = vehicle_resource_vars_[v];
      model->AddToAssignment(resource_var);
      // The resource variable must be fixed by the search.
      model->AddVariableTargetToFinalizer(resource_var, -1);

      if (!resource_group_.VehicleRequiresAResource(v)) {
        continue;
      }
      for (const RoutingModel::DimensionIndex d :
           resource_group_.GetAffectedDimensionIndices()) {
        const RoutingDimension* const dim = dimensions[d.value()];
        // The vehicle's start/end cumuls must be fixed by the search.
        model->AddVariableMinimizedByFinalizer(dim->CumulVar(model_.End(v)));
        model->AddVariableMaximizedByFinalizer(dim->CumulVar(model_.Start(v)));
      }
    }
  }

  void Post() override {}

  void InitialPropagate() override {
    if (!AllResourceAssignmentsFeasible()) {
      solver()->Fail();
    }
    SetupResourceConstraints();
  }

 private:
  bool AllResourceAssignmentsFeasible() {
    DCHECK(!model_.GetResourceGroups().empty());

    std::vector<int64_t> end_chain_starts;
    std::vector<int> vehicle_index_of_start_chain_end;
    ComputeVehicleChainStartEndInfo(model_, &end_chain_starts,
                                    &vehicle_index_of_start_chain_end);
    const auto next = [&model = model_, &end_chain_starts,
                       &vehicle_index_of_start_chain_end](int64_t node) {
      if (model.NextVar(node)->Bound()) return model.NextVar(node)->Value();
      const int vehicle = vehicle_index_of_start_chain_end[node];
      if (vehicle < 0) {
        // The node isn't the last node of a route start chain and is considered
        // as unperformed and ignored when evaluating the feasibility of the
        // resource assignment.
        return node;
      }
      return end_chain_starts[vehicle];
    };

    const std::vector<RoutingDimension*>& dimensions = model_.GetDimensions();
    for (RoutingModel::DimensionIndex d :
         resource_group_.GetAffectedDimensionIndices()) {
      if (!ResourceAssignmentFeasibleForDimension(*dimensions[d.value()],
                                                  next)) {
        return false;
      }
    }
    return true;
  }

  bool ResourceAssignmentFeasibleForDimension(
      const RoutingDimension& dimension,
      const std::function<int64_t(int64_t)>& next) {
    LocalDimensionCumulOptimizer* const optimizer =
        model_.GetMutableLocalCumulLPOptimizer(dimension);

    if (optimizer == nullptr) return true;

    LocalDimensionCumulOptimizer* const mp_optimizer =
        model_.GetMutableLocalCumulMPOptimizer(dimension);
    DCHECK_NE(mp_optimizer, nullptr);
    const auto transit = [&dimension](int64_t node, int64_t /*next*/) {
      // TODO(user): Get rid of this max() by only allowing resources on
      // dimensions with positive transits (model.AreVehicleTransitsPositive()).
      // TODO(user): The transit lower bounds have not necessarily been
      // propagated at this point. Add demons to check the resource assignment
      // feasibility after the transit ranges have been propagated.
      return std::max<int64_t>(dimension.FixedTransitVar(node)->Min(), 0);
    };

    using RCIndex = RoutingModel::ResourceClassIndex;
    const util_intops::StrongVector<RCIndex, absl::flat_hash_set<int>>
        ignored_resources_per_class(resource_group_.GetResourceClassesCount());
    std::vector<std::vector<int64_t>> assignment_costs(model_.vehicles());
    for (int v : resource_group_.GetVehiclesRequiringAResource()) {
      if (!ComputeVehicleToResourceClassAssignmentCosts(
              v, resource_group_, ignored_resources_per_class, next, transit,
              /*optimize_vehicle_costs*/ false,
              model_.GetMutableLocalCumulLPOptimizer(dimension),
              model_.GetMutableLocalCumulMPOptimizer(dimension),
              &assignment_costs[v], nullptr, nullptr)) {
        return false;
      }
    }
    // TODO(user): Replace this call with a more efficient max-flow, instead
    // of running the full min-cost flow.
    return ComputeBestVehicleToResourceAssignment(
               resource_group_.GetVehiclesRequiringAResource(),
               resource_group_.GetResourceIndicesPerClass(),
               ignored_resources_per_class,
               [&assignment_costs](int v) { return &assignment_costs[v]; },
               nullptr) >= 0;
  }

  void SetupResourceConstraints() {
    Solver* const s = solver();
    // Resources cannot be shared, so assigned resources must all be different
    // (note that resource_var == -1 means no resource assigned).
    s->AddConstraint(s->MakeAllDifferentExcept(vehicle_resource_vars_, -1));
    for (int v = 0; v < model_.vehicles(); v++) {
      IntVar* const resource_var = vehicle_resource_vars_[v];
      if (!resource_group_.VehicleRequiresAResource(v)) {
        resource_var->SetValue(-1);
        continue;
      }
      // vehicle_route_considered_[v] <--> vehicle_res_vars[v] != -1.
      s->AddConstraint(
          s->MakeEquality(model_.VehicleRouteConsideredVar(v),
                          s->MakeIsDifferentCstVar(resource_var, -1)));

      // Reduce domain of resource_var.
      const absl::flat_hash_set<int>& resources_marked_allowed =
          resource_group_.GetResourcesMarkedAllowedForVehicle(v);
      if (!resources_marked_allowed.empty()) {
        std::vector<int> allowed_resources(resources_marked_allowed.begin(),
                                           resources_marked_allowed.end());
        allowed_resources.push_back(-1);
        s->AddConstraint(s->MakeMemberCt(resource_var, allowed_resources));
      }

      if (resource_var->Bound()) {
        ResourceBound(v);
      } else {
        Demon* const demon = MakeConstraintDemon1(
            s, this, &ResourceAssignmentConstraint::ResourceBound,
            "ResourceBound", v);
        resource_var->WhenBound(demon);
      }
    }
  }
  void ResourceBound(int vehicle) {
    const int64_t resource = vehicle_resource_vars_[vehicle]->Value();
    if (resource < 0) return;
    for (const RoutingModel::DimensionIndex d :
         resource_group_.GetAffectedDimensionIndices()) {
      const RoutingDimension* const dim = model_.GetDimensions()[d.value()];
      const RoutingModel::ResourceGroup::Attributes& attributes =
          resource_group_.GetResources()[resource].GetDimensionAttributes(dim);
      // resource_start_lb <= cumul[start(vehicle)] <= resource_start_ub
      // resource_end_lb <= cumul[end(vehicle)] <= resource_end_ub
      dim->CumulVar(model_.Start(vehicle))
          ->SetRange(attributes.start_domain().Min(),
                     attributes.start_domain().Max());
      dim->CumulVar(model_.End(vehicle))
          ->SetRange(attributes.end_domain().Min(),
                     attributes.end_domain().Max());
    }
  }

  const RoutingModel& model_;
  const RoutingModel::ResourceGroup& resource_group_;
  const std::vector<IntVar*>& vehicle_resource_vars_;
};
}  // namespace

Constraint* MakeResourceConstraint(
    const RoutingModel::ResourceGroup* resource_group,
    const std::vector<IntVar*>* vehicle_resource_vars, RoutingModel* model) {
  return model->solver()->RevAlloc(new ResourceAssignmentConstraint(
      resource_group, vehicle_resource_vars, model));
}

namespace {
class PathSpansAndTotalSlacks : public Constraint {
 public:
  PathSpansAndTotalSlacks(const RoutingModel* model,
                          const RoutingDimension* dimension,
                          std::vector<IntVar*> spans,
                          std::vector<IntVar*> total_slacks)
      : Constraint(model->solver()),
        model_(model),
        dimension_(dimension),
        spans_(std::move(spans)),
        total_slacks_(std::move(total_slacks)) {
    CHECK_EQ(spans_.size(), model_->vehicles());
    CHECK_EQ(total_slacks_.size(), model_->vehicles());
    vehicle_demons_.resize(model_->vehicles());
  }

  std::string DebugString() const override { return "PathSpansAndTotalSlacks"; }

  void Post() override {
    const int num_nodes = model_->VehicleVars().size();
    const int num_transits = model_->Nexts().size();
    for (int node = 0; node < num_nodes; ++node) {
      auto* demon = MakeConstraintDemon1(
          model_->solver(), this, &PathSpansAndTotalSlacks::PropagateNode,
          "PathSpansAndTotalSlacks::PropagateNode", node);
      dimension_->CumulVar(node)->WhenRange(demon);
      model_->VehicleVar(node)->WhenBound(demon);
      if (node < num_transits) {
        dimension_->TransitVar(node)->WhenRange(demon);
        dimension_->FixedTransitVar(node)->WhenBound(demon);
        model_->NextVar(node)->WhenBound(demon);
      }
    }
    for (int vehicle = 0; vehicle < spans_.size(); ++vehicle) {
      if (!spans_[vehicle] && !total_slacks_[vehicle]) continue;
      auto* demon = MakeDelayedConstraintDemon1(
          solver(), this, &PathSpansAndTotalSlacks::PropagateVehicle,
          "PathSpansAndTotalSlacks::PropagateVehicle", vehicle);
      vehicle_demons_[vehicle] = demon;
      if (spans_[vehicle]) spans_[vehicle]->WhenRange(demon);
      if (total_slacks_[vehicle]) total_slacks_[vehicle]->WhenRange(demon);
      if (dimension_->HasBreakConstraints()) {
        for (IntervalVar* b : dimension_->GetBreakIntervalsOfVehicle(vehicle)) {
          b->WhenAnything(demon);
        }
      }
    }
  }

  // Call propagator on all vehicles.
  void InitialPropagate() override {
    for (int vehicle = 0; vehicle < spans_.size(); ++vehicle) {
      if (!spans_[vehicle] && !total_slacks_[vehicle]) continue;
      PropagateVehicle(vehicle);
    }
  }

 private:
  // Called when a path/dimension variables of the node changes,
  // this delays propagator calls until path variables (Next and VehicleVar)
  // are instantiated, which saves fruitless and multiple identical calls.
  void PropagateNode(int node) {
    if (!model_->VehicleVar(node)->Bound()) return;
    const int vehicle = model_->VehicleVar(node)->Min();
    if (vehicle < 0 || vehicle_demons_[vehicle] == nullptr) return;
    EnqueueDelayedDemon(vehicle_demons_[vehicle]);
  }

  // In order to make reasoning on span and total_slack of a vehicle uniform,
  // we rely on the fact that span == sum_fixed_transits + total_slack
  // to present both span and total_slack in terms of span and fixed transit.
  // This allows to use the same code whether there actually are variables
  // for span and total_slack or not.
  int64_t SpanMin(int vehicle, int64_t sum_fixed_transits) {
    DCHECK_GE(sum_fixed_transits, 0);
    const int64_t span_min = spans_[vehicle]
                                 ? spans_[vehicle]->Min()
                                 : std::numeric_limits<int64_t>::max();
    const int64_t total_slack_min = total_slacks_[vehicle]
                                        ? total_slacks_[vehicle]->Min()
                                        : std::numeric_limits<int64_t>::max();
    return std::min(span_min, CapAdd(total_slack_min, sum_fixed_transits));
  }
  int64_t SpanMax(int vehicle, int64_t sum_fixed_transits) {
    DCHECK_GE(sum_fixed_transits, 0);
    const int64_t span_max = spans_[vehicle]
                                 ? spans_[vehicle]->Max()
                                 : std::numeric_limits<int64_t>::min();
    const int64_t total_slack_max = total_slacks_[vehicle]
                                        ? total_slacks_[vehicle]->Max()
                                        : std::numeric_limits<int64_t>::min();
    return std::max(span_max, CapAdd(total_slack_max, sum_fixed_transits));
  }
  void SetSpanMin(int vehicle, int64_t min, int64_t sum_fixed_transits) {
    DCHECK_GE(sum_fixed_transits, 0);
    if (spans_[vehicle]) {
      spans_[vehicle]->SetMin(min);
    }
    if (total_slacks_[vehicle]) {
      total_slacks_[vehicle]->SetMin(CapSub(min, sum_fixed_transits));
    }
  }
  void SetSpanMax(int vehicle, int64_t max, int64_t sum_fixed_transits) {
    DCHECK_GE(sum_fixed_transits, 0);
    if (spans_[vehicle]) {
      spans_[vehicle]->SetMax(max);
    }
    if (total_slacks_[vehicle]) {
      total_slacks_[vehicle]->SetMax(CapSub(max, sum_fixed_transits));
    }
  }
  // Propagates span == sum_fixed_transits + total_slack.
  // This should be called at least once during PropagateVehicle().
  void SynchronizeSpanAndTotalSlack(int vehicle, int64_t sum_fixed_transits) {
    DCHECK_GE(sum_fixed_transits, 0);
    IntVar* span = spans_[vehicle];
    IntVar* total_slack = total_slacks_[vehicle];
    if (!span || !total_slack) return;
    span->SetMin(CapAdd(total_slack->Min(), sum_fixed_transits));
    span->SetMax(CapAdd(total_slack->Max(), sum_fixed_transits));
    total_slack->SetMin(CapSub(span->Min(), sum_fixed_transits));
    total_slack->SetMax(CapSub(span->Max(), sum_fixed_transits));
  }

  void PropagateVehicle(int vehicle) {
    DCHECK(spans_[vehicle] || total_slacks_[vehicle]);
    const int start = model_->Start(vehicle);
    const int end = model_->End(vehicle);
    // If transits are positive, the domain of the span variable can be reduced
    // to cumul(end) - cumul(start).
    if (spans_[vehicle] != nullptr &&
        dimension_->AreVehicleTransitsPositive(vehicle)) {
      spans_[vehicle]->SetRange(CapSub(dimension_->CumulVar(end)->Min(),
                                       dimension_->CumulVar(start)->Max()),
                                CapSub(dimension_->CumulVar(end)->Max(),
                                       dimension_->CumulVar(start)->Min()));
    }
    // Record path, if it is not fixed from start to end, stop here.
    // TRICKY: do not put end node yet, we look only at transits in the next
    // reasonings, we will append the end when we look at cumuls.
    {
      path_.clear();
      int curr_node = start;
      while (!model_->IsEnd(curr_node)) {
        const IntVar* next_var = model_->NextVar(curr_node);
        if (!next_var->Bound()) return;
        path_.push_back(curr_node);
        curr_node = next_var->Value();
      }
    }
    // Compute the sum of fixed transits. Fixed transit variables should all be
    // fixed, otherwise we wait to get called later when propagation does it.
    int64_t sum_fixed_transits = 0;
    for (const int node : path_) {
      const IntVar* fixed_transit_var = dimension_->FixedTransitVar(node);
      if (!fixed_transit_var->Bound()) return;
      sum_fixed_transits =
          CapAdd(sum_fixed_transits, fixed_transit_var->Value());
    }

    SynchronizeSpanAndTotalSlack(vehicle, sum_fixed_transits);

    // The amount of break time that must occur during the route must be smaller
    // than span max - sum_fixed_transits. A break must occur on the route if it
    // must be after the route's start and before the route's end.
    // Propagate lower bound on span, then filter out values
    // that would force more breaks in route than possible.
    if (dimension_->HasBreakConstraints() &&
        !dimension_->GetBreakIntervalsOfVehicle(vehicle).empty()) {
      const int64_t vehicle_start_max = dimension_->CumulVar(start)->Max();
      const int64_t vehicle_end_min = dimension_->CumulVar(end)->Min();
      // Compute and propagate lower bound.
      int64_t min_break_duration = 0;
      for (IntervalVar* br : dimension_->GetBreakIntervalsOfVehicle(vehicle)) {
        if (!br->MustBePerformed()) continue;
        if (vehicle_start_max < br->EndMin() &&
            br->StartMax() < vehicle_end_min) {
          min_break_duration = CapAdd(min_break_duration, br->DurationMin());
        }
      }
      SetSpanMin(vehicle, CapAdd(min_break_duration, sum_fixed_transits),
                 sum_fixed_transits);
      // If a break that is not inside the route may violate slack_max,
      // we can propagate in some cases: when the break must be before or
      // must be after the route.
      // In the other cases, we cannot deduce a better bound on a CumulVar or
      // on a break, so we do nothing.
      const int64_t slack_max =
          CapSub(SpanMax(vehicle, sum_fixed_transits), sum_fixed_transits);
      const int64_t max_additional_slack =
          CapSub(slack_max, min_break_duration);
      for (IntervalVar* br : dimension_->GetBreakIntervalsOfVehicle(vehicle)) {
        if (!br->MustBePerformed()) continue;
        // Break must be before end, detect whether it must be before start.
        if (vehicle_start_max >= br->EndMin() &&
            br->StartMax() < vehicle_end_min) {
          if (br->DurationMin() > max_additional_slack) {
            // Having the break inside would violate max_additional_slack..
            // Thus, it must be outside the route, in this case, before.
            br->SetEndMax(vehicle_start_max);
            dimension_->CumulVar(start)->SetMin(br->EndMin());
          }
        }
        // Break must be after start, detect whether it must be after end.
        // Same reasoning, in the case where the break is after.
        if (vehicle_start_max < br->EndMin() &&
            br->StartMax() >= vehicle_end_min) {
          if (br->DurationMin() > max_additional_slack) {
            br->SetStartMin(vehicle_end_min);
            dimension_->CumulVar(end)->SetMax(br->StartMax());
          }
        }
      }
    }

    // Propagate span == cumul(end) - cumul(start).
    {
      IntVar* start_cumul = dimension_->CumulVar(start);
      IntVar* end_cumul = dimension_->CumulVar(end);
      const int64_t start_min = start_cumul->Min();
      const int64_t start_max = start_cumul->Max();
      const int64_t end_min = end_cumul->Min();
      const int64_t end_max = end_cumul->Max();
      // Propagate from cumuls to span.
      const int64_t span_lb = CapSub(end_min, start_max);
      SetSpanMin(vehicle, span_lb, sum_fixed_transits);
      const int64_t span_ub = CapSub(end_max, start_min);
      SetSpanMax(vehicle, span_ub, sum_fixed_transits);
      // Propagate from span to cumuls.
      const int64_t span_min = SpanMin(vehicle, sum_fixed_transits);
      const int64_t span_max = SpanMax(vehicle, sum_fixed_transits);
      const int64_t slack_from_lb = CapSub(span_max, span_lb);
      const int64_t slack_from_ub = CapSub(span_ub, span_min);
      // start >= start_max - (span_max - span_lb).
      start_cumul->SetMin(CapSub(start_max, slack_from_lb));
      // end <= end_min + (span_max - span_lb).
      end_cumul->SetMax(CapAdd(end_min, slack_from_lb));
      // // start <= start_min + (span_ub - span_min)
      start_cumul->SetMax(CapAdd(start_min, slack_from_ub));
      // // end >= end_max - (span_ub - span_min)
      end_cumul->SetMin(CapSub(end_max, slack_from_ub));
    }

    // Propagate sum transits == span.
    {
      // Propagate from transits to span.
      int64_t span_lb = 0;
      int64_t span_ub = 0;
      for (const int node : path_) {
        span_lb = CapAdd(span_lb, dimension_->TransitVar(node)->Min());
        span_ub = CapAdd(span_ub, dimension_->TransitVar(node)->Max());
      }
      SetSpanMin(vehicle, span_lb, sum_fixed_transits);
      SetSpanMax(vehicle, span_ub, sum_fixed_transits);
      // Propagate from span to transits.
      // transit[i] <= transit_i_min + (span_max - span_lb)
      // transit[i] >= transit_i_max - (span_ub - span_min)
      const int64_t span_min = SpanMin(vehicle, sum_fixed_transits);
      const int64_t span_max = SpanMax(vehicle, sum_fixed_transits);
      const int64_t slack_from_lb = CapSub(span_max, span_lb);
      const int64_t slack_from_ub =
          span_ub < std::numeric_limits<int64_t>::max()
              ? CapSub(span_ub, span_min)
              : std::numeric_limits<int64_t>::max();
      for (const int node : path_) {
        IntVar* transit_var = dimension_->TransitVar(node);
        const int64_t transit_i_min = transit_var->Min();
        const int64_t transit_i_max = transit_var->Max();
        // TRICKY: the first propagation might change transit_var->Max(),
        // but we must use the same value of transit_i_max in the computation
        // of transit[i]'s lower bound that was used for span_ub.
        transit_var->SetMax(CapAdd(transit_i_min, slack_from_lb));
        transit_var->SetMin(CapSub(transit_i_max, slack_from_ub));
      }
    }

    // TRICKY: add end node now, we will look at cumuls.
    path_.push_back(end);

    // A stronger bound: from start min of the route, go to node i+1 with time
    // max(cumul[i] + fixed_transit, cumul[i+1].Min()).
    // Record arrival time (should be the same as end cumul min).
    // Then do the reverse route, going to time
    // min(cumul[i+1] - fixed_transit, cumul[i].Max())
    // Record final time as departure time.
    // Then arrival time - departure time is a valid lower bound of span.
    // First reasoning: start - end - start
    {
      // At each iteration, arrival time is a lower bound of path[i]'s cumul,
      // so we opportunistically tighten the variable.
      // This helps reduce the amount of inter-constraint propagation.
      int64_t arrival_time = dimension_->CumulVar(start)->Min();
      for (int i = 1; i < path_.size(); ++i) {
        arrival_time =
            std::max(CapAdd(arrival_time,
                            dimension_->FixedTransitVar(path_[i - 1])->Min()),
                     dimension_->CumulVar(path_[i])->Min());
        dimension_->CumulVar(path_[i])->SetMin(arrival_time);
      }
      // At each iteration, departure_time is the latest time at each the
      // vehicle can leave to reach the earliest feasible vehicle end. Thus it
      // is not an upper bound of the cumul, we cannot tighten the variable.
      int64_t departure_time = arrival_time;
      for (int i = path_.size() - 2; i >= 0; --i) {
        departure_time =
            std::min(CapSub(departure_time,
                            dimension_->FixedTransitVar(path_[i])->Min()),
                     dimension_->CumulVar(path_[i])->Max());
      }
      const int64_t span_lb = CapSub(arrival_time, departure_time);
      SetSpanMin(vehicle, span_lb, sum_fixed_transits);
      const int64_t maximum_deviation =
          CapSub(SpanMax(vehicle, sum_fixed_transits), span_lb);
      const int64_t start_lb = CapSub(departure_time, maximum_deviation);
      dimension_->CumulVar(start)->SetMin(start_lb);
    }
    // Second reasoning: end - start - end
    {
      // At each iteration, use departure time to tighten opportunistically.
      int64_t departure_time = dimension_->CumulVar(end)->Max();
      for (int i = path_.size() - 2; i >= 0; --i) {
        departure_time =
            std::min(CapSub(departure_time,
                            dimension_->FixedTransitVar(path_[i])->Min()),
                     dimension_->CumulVar(path_[i])->Max());
        dimension_->CumulVar(path_[i])->SetMax(departure_time);
      }
      // Symmetrically to the first reasoning, arrival_time is the earliest
      // possible arrival for the latest departure of vehicle start.
      // It cannot be used to tighten the successive cumul variables.
      int arrival_time = departure_time;
      for (int i = 1; i < path_.size(); ++i) {
        arrival_time =
            std::max(CapAdd(arrival_time,
                            dimension_->FixedTransitVar(path_[i - 1])->Min()),
                     dimension_->CumulVar(path_[i])->Min());
      }
      const int64_t span_lb = CapSub(arrival_time, departure_time);
      SetSpanMin(vehicle, span_lb, sum_fixed_transits);
      const int64_t maximum_deviation =
          CapSub(SpanMax(vehicle, sum_fixed_transits), span_lb);
      dimension_->CumulVar(end)->SetMax(
          CapAdd(arrival_time, maximum_deviation));
    }
  }

  const RoutingModel* const model_;
  const RoutingDimension* const dimension_;
  std::vector<IntVar*> spans_;
  std::vector<IntVar*> total_slacks_;
  std::vector<int> path_;
  std::vector<Demon*> vehicle_demons_;
};
}  // namespace

Constraint* MakePathSpansAndTotalSlacks(const RoutingDimension* dimension,
                                        std::vector<IntVar*> spans,
                                        std::vector<IntVar*> total_slacks) {
  RoutingModel* const model = dimension->model();
  CHECK_EQ(model->vehicles(), spans.size());
  CHECK_EQ(model->vehicles(), total_slacks.size());
  return model->solver()->RevAlloc(new PathSpansAndTotalSlacks(
      model, dimension, std::move(spans), std::move(total_slacks)));
}

namespace {
// Very light version of the RangeLessOrEqual constraint (see ./range_cst.cc).
// Only performs initial propagation and then checks the compatibility of the
// variable domains without domain pruning.
// This is useful when to avoid ping-pong effects with costly constraints
// such as the PathCumul constraint.
// This constraint has not been added to the cp library (in range_cst.cc) given
// it only does checking and no propagation (except the initial propagation)
// and is only fit for local search, in particular in the context of vehicle
// routing.
class LightRangeLessOrEqual : public Constraint {
 public:
  LightRangeLessOrEqual(Solver* s, IntExpr* l, IntExpr* r);
  ~LightRangeLessOrEqual() override {}
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  IntVar* Var() override {
    return solver()->MakeIsLessOrEqualVar(left_, right_);
  }
  // TODO(user): introduce a kLightLessOrEqual tag.
  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kLessOrEqual, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitConstraint(ModelVisitor::kLessOrEqual, this);
  }

 private:
  void CheckRange();

  IntExpr* const left_;
  IntExpr* const right_;
  Demon* demon_;
};

LightRangeLessOrEqual::LightRangeLessOrEqual(Solver* const s, IntExpr* const l,
                                             IntExpr* const r)
    : Constraint(s), left_(l), right_(r), demon_(nullptr) {}

void LightRangeLessOrEqual::Post() {
  demon_ = MakeConstraintDemon0(
      solver(), this, &LightRangeLessOrEqual::CheckRange, "CheckRange");
  left_->WhenRange(demon_);
  right_->WhenRange(demon_);
}

void LightRangeLessOrEqual::InitialPropagate() {
  left_->SetMax(right_->Max());
  right_->SetMin(left_->Min());
  if (left_->Max() <= right_->Min()) {
    demon_->inhibit(solver());
  }
}

void LightRangeLessOrEqual::CheckRange() {
  if (left_->Min() > right_->Max()) {
    solver()->Fail();
  }
  if (left_->Max() <= right_->Min()) {
    demon_->inhibit(solver());
  }
}

std::string LightRangeLessOrEqual::DebugString() const {
  return left_->DebugString() + " < " + right_->DebugString();
}
}  // namespace

namespace {

class RouteConstraint : public Constraint {
 public:
  RouteConstraint(
      RoutingModel* model, std::vector<IntVar*> route_cost_vars,
      std::function<std::optional<int64_t>(const std::vector<int64_t>&)>
          route_evaluator)
      : Constraint(model->solver()),
        model_(model),
        route_cost_vars_(std::move(route_cost_vars)),
        route_evaluator_(std::move(route_evaluator)),
        starts_(model->Size() + model->vehicles(), -1),
        ends_(model->Size() + model->vehicles(), -1) {
    const int size = model_->Size() + model_->vehicles();
    for (int i = 0; i < size; ++i) {
      starts_.SetValue(solver(), i, i);
      ends_.SetValue(solver(), i, i);
    }
  }
  ~RouteConstraint() override {}
  void Post() override {
    const std::vector<IntVar*> nexts = model_->Nexts();
    for (int i = 0; i < nexts.size(); ++i) {
      if (!nexts[i]->Bound()) {
        auto* demon = MakeConstraintDemon2(
            model_->solver(), this, &RouteConstraint::AddLink,
            "RouteConstraint::AddLink", i, nexts[i]);
        nexts[i]->WhenBound(demon);
      }
    }
  }
  void InitialPropagate() override {
    const std::vector<IntVar*> nexts = model_->Nexts();
    for (int i = 0; i < nexts.size(); ++i) {
      if (nexts[i]->Bound()) {
        AddLink(i, nexts[i]);
      }
    }
  }
  std::string DebugString() const override { return "RouteConstraint"; }

 private:
  void AddLink(int index, IntVar* next) {
    DCHECK(next->Bound());
    const int64_t chain_start = starts_.Value(index);
    const int64_t index_next = next->Min();
    const int64_t chain_end = ends_.Value(index_next);
    starts_.SetValue(solver(), chain_end, chain_start);
    ends_.SetValue(solver(), chain_start, chain_end);
    if (model_->IsStart(chain_start) && model_->IsEnd(chain_end)) {
      CheckRoute(chain_start, chain_end);
    }
  }
  void CheckRoute(int64_t start, int64_t end) {
    route_.clear();
    for (int64_t node = start; node != end;
         node = model_->NextVar(node)->Min()) {
      route_.push_back(node);
    }
    route_.push_back(end);
    std::optional<int64_t> cost = route_evaluator_(route_);
    if (!cost.has_value()) {
      solver()->Fail();
    }
    route_cost_vars_[model_->VehicleIndex(start)]->SetValue(cost.value());
  }

  RoutingModel* const model_;
  std::vector<IntVar*> route_cost_vars_;
  std::function<std::optional<int64_t>(const std::vector<int64_t>&)>
      route_evaluator_;
  RevArray<int> starts_;
  RevArray<int> ends_;
  std::vector<int64_t> route_;
};
}  // namespace

Constraint* MakeRouteConstraint(
    RoutingModel* model, std::vector<IntVar*> route_cost_vars,
    std::function<std::optional<int64_t>(const std::vector<int64_t>&)>
        route_evaluator) {
  return model->solver()->RevAlloc(new RouteConstraint(
      model, std::move(route_cost_vars), std::move(route_evaluator)));
}

}  // namespace operations_research

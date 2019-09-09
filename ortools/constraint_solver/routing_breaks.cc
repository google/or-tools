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

#include <algorithm>

#include "ortools/constraint_solver/routing.h"

namespace operations_research {

bool DisjunctivePropagator::Propagate(Tasks* tasks) {
  DCHECK_LE(tasks->num_chain_tasks, tasks->start_min.size());
  DCHECK_EQ(tasks->start_min.size(), tasks->start_max.size());
  DCHECK_EQ(tasks->start_min.size(), tasks->duration_min.size());
  DCHECK_EQ(tasks->start_min.size(), tasks->duration_max.size());
  DCHECK_EQ(tasks->start_min.size(), tasks->end_min.size());
  DCHECK_EQ(tasks->start_min.size(), tasks->end_max.size());
  DCHECK_EQ(tasks->start_min.size(), tasks->is_preemptible.size());
  // Do forward deductions, then backward deductions.
  // Interleave Precedences() that is O(n).
  return Precedences(tasks) && EdgeFinding(tasks) && Precedences(tasks) &&
         DetectablePrecedencesWithChain(tasks) && ForbiddenIntervals(tasks) &&
         Precedences(tasks) && DistanceDuration(tasks) && Precedences(tasks) &&
         MirrorTasks(tasks) && EdgeFinding(tasks) && Precedences(tasks) &&
         DetectablePrecedencesWithChain(tasks) && MirrorTasks(tasks);
}

bool DisjunctivePropagator::Precedences(Tasks* tasks) {
  const int num_chain_tasks = tasks->num_chain_tasks;
  if (num_chain_tasks == 0) return true;
  // Propagate forwards.
  int64 time = tasks->start_min[0];
  for (int task = 0; task < num_chain_tasks; ++task) {
    time = std::max(tasks->start_min[task], time);
    tasks->start_min[task] = time;
    time = CapAdd(time, tasks->duration_min[task]);
    if (tasks->end_max[task] < time) return false;
  }
  // Propagate backwards.
  time = tasks->end_max[num_chain_tasks - 1];
  for (int task = num_chain_tasks - 1; task >= 0; --task) {
    time = std::min(tasks->end_max[task], time);
    tasks->end_max[task] = time;
    time = CapSub(time, tasks->duration_min[task]);
    if (time < tasks->start_min[task]) return false;
  }
  const int num_tasks = tasks->start_min.size();
  for (int task = 0; task < num_tasks; ++task) {
    // Enforce start + duration <= end.
    tasks->end_min[task] =
        std::max(tasks->end_min[task],
                 CapAdd(tasks->start_min[task], tasks->duration_min[task]));
    tasks->start_max[task] =
        std::min(tasks->start_max[task],
                 CapSub(tasks->end_max[task], tasks->duration_min[task]));
    tasks->duration_max[task] =
        std::min(tasks->duration_max[task],
                 CapSub(tasks->end_max[task], tasks->start_min[task]));
    if (!tasks->is_preemptible[task]) {
      // Enforce start + duration == end for nonpreemptibles.
      tasks->end_max[task] =
          std::min(tasks->end_max[task],
                   CapAdd(tasks->start_max[task], tasks->duration_max[task]));
      tasks->start_min[task] =
          std::max(tasks->start_min[task],
                   CapSub(tasks->end_min[task], tasks->duration_max[task]));
      tasks->duration_min[task] =
          std::max(tasks->duration_min[task],
                   CapSub(tasks->end_min[task], tasks->start_max[task]));
    }
    if (tasks->duration_min[task] > tasks->duration_max[task]) return false;
    if (tasks->end_min[task] > tasks->end_max[task]) return false;
    if (tasks->start_min[task] > tasks->start_max[task]) return false;
  }
  return true;
}

bool DisjunctivePropagator::MirrorTasks(Tasks* tasks) {
  const int num_tasks = tasks->start_min.size();
  // For all tasks, start_min := -end_max and end_max := -start_min.
  for (int task = 0; task < num_tasks; ++task) {
    const int64 t = -tasks->start_min[task];
    tasks->start_min[task] = -tasks->end_max[task];
    tasks->end_max[task] = t;
  }
  // For all tasks, start_max := -end_min and end_min := -start_max.
  for (int task = 0; task < num_tasks; ++task) {
    const int64 t = -tasks->start_max[task];
    tasks->start_max[task] = -tasks->end_min[task];
    tasks->end_min[task] = t;
  }
  // In the mirror problem, tasks linked by precedences are in reversed order.
  const int num_chain_tasks = tasks->num_chain_tasks;
  if (num_chain_tasks > 0) {
    std::reverse(tasks->start_min.begin(),
                 tasks->start_min.begin() + num_chain_tasks);
    std::reverse(tasks->start_max.begin(),
                 tasks->start_max.begin() + num_chain_tasks);
    std::reverse(tasks->duration_min.begin(),
                 tasks->duration_min.begin() + num_chain_tasks);
    std::reverse(tasks->duration_max.begin(),
                 tasks->duration_max.begin() + num_chain_tasks);
    std::reverse(tasks->end_min.begin(),
                 tasks->end_min.begin() + num_chain_tasks);
    std::reverse(tasks->end_max.begin(),
                 tasks->end_max.begin() + num_chain_tasks);
    std::reverse(tasks->is_preemptible.begin(),
                 tasks->is_preemptible.begin() + num_chain_tasks);
  }
  return true;
}

bool DisjunctivePropagator::EdgeFinding(Tasks* tasks) {
  const int num_tasks = tasks->start_min.size();
  // Prepare start_min events for tree.
  tasks_by_start_min_.resize(num_tasks);
  std::iota(tasks_by_start_min_.begin(), tasks_by_start_min_.end(), 0);
  std::sort(
      tasks_by_start_min_.begin(), tasks_by_start_min_.end(),
      [&](int i, int j) { return tasks->start_min[i] < tasks->start_min[j]; });
  event_of_task_.resize(num_tasks);
  for (int event = 0; event < num_tasks; ++event) {
    event_of_task_[tasks_by_start_min_[event]] = event;
  }
  // Tasks will be browsed according to end_max order.
  tasks_by_end_max_.resize(num_tasks);
  std::iota(tasks_by_end_max_.begin(), tasks_by_end_max_.end(), 0);
  std::sort(
      tasks_by_end_max_.begin(), tasks_by_end_max_.end(),
      [&](int i, int j) { return tasks->end_max[i] < tasks->end_max[j]; });

  // Generic overload checking: insert tasks by end_max,
  // fail if envelope > end_max.
  theta_lambda_tree_.Reset(num_tasks);
  for (const int task : tasks_by_end_max_) {
    theta_lambda_tree_.AddOrUpdateEvent(
        event_of_task_[task], tasks->start_min[task], tasks->duration_min[task],
        tasks->duration_min[task]);
    if (theta_lambda_tree_.GetEnvelope() > tasks->end_max[task]) {
      return false;
    }
  }

  // Generic edge finding: from full set of tasks, at each end_max event in
  // decreasing order, check lambda feasibility, then move end_max task from
  // theta to lambda.
  for (int i = num_tasks - 1; i >= 0; --i) {
    const int task = tasks_by_end_max_[i];
    const int64 envelope = theta_lambda_tree_.GetEnvelope();
    // If a nonpreemptible optional would overload end_max, push to envelope.
    while (theta_lambda_tree_.GetOptionalEnvelope() > tasks->end_max[task]) {
      int critical_event;  // Dummy value.
      int optional_event;
      int64 available_energy;  // Dummy value.
      theta_lambda_tree_.GetEventsWithOptionalEnvelopeGreaterThan(
          tasks->end_max[task], &critical_event, &optional_event,
          &available_energy);
      const int optional_task = tasks_by_start_min_[optional_event];
      tasks->start_min[optional_task] =
          std::max(tasks->start_min[optional_task], envelope);
      theta_lambda_tree_.RemoveEvent(optional_event);
    }
    if (!tasks->is_preemptible[task]) {
      theta_lambda_tree_.AddOrUpdateOptionalEvent(event_of_task_[task],
                                                  tasks->start_min[task],
                                                  tasks->duration_min[task]);
    } else {
      theta_lambda_tree_.RemoveEvent(event_of_task_[task]);
    }
  }
  return true;
}

bool DisjunctivePropagator::DetectablePrecedencesWithChain(Tasks* tasks) {
  const int num_tasks = tasks->start_min.size();
  // Prepare start_min events for tree.
  tasks_by_start_min_.resize(num_tasks);
  std::iota(tasks_by_start_min_.begin(), tasks_by_start_min_.end(), 0);
  std::sort(
      tasks_by_start_min_.begin(), tasks_by_start_min_.end(),
      [&](int i, int j) { return tasks->start_min[i] < tasks->start_min[j]; });
  event_of_task_.resize(num_tasks);
  for (int event = 0; event < num_tasks; ++event) {
    event_of_task_[tasks_by_start_min_[event]] = event;
  }
  theta_lambda_tree_.Reset(num_tasks);

  // Sort nonchain tasks by start max = end_max - duration_min.
  const int num_chain_tasks = tasks->num_chain_tasks;
  nonchain_tasks_by_start_max_.resize(num_tasks - num_chain_tasks);
  std::iota(nonchain_tasks_by_start_max_.begin(),
            nonchain_tasks_by_start_max_.end(), num_chain_tasks);
  std::sort(nonchain_tasks_by_start_max_.begin(),
            nonchain_tasks_by_start_max_.end(), [&tasks](int i, int j) {
              return tasks->end_max[i] - tasks->duration_min[i] <
                     tasks->end_max[j] - tasks->duration_min[j];
            });

  // Detectable precedences, specialized for routes: for every task on route,
  // put all tasks before it in the tree, then push with envelope.
  int index_nonchain = 0;
  for (int i = 0; i < num_chain_tasks; ++i) {
    if (!tasks->is_preemptible[i]) {
      // Add all nonchain tasks detected before i.
      while (index_nonchain < nonchain_tasks_by_start_max_.size()) {
        const int task = nonchain_tasks_by_start_max_[index_nonchain];
        if (tasks->end_max[task] - tasks->duration_min[task] >=
            tasks->start_min[i] + tasks->duration_min[i])
          break;
        theta_lambda_tree_.AddOrUpdateEvent(
            event_of_task_[task], tasks->start_min[task],
            tasks->duration_min[task], tasks->duration_min[task]);
        index_nonchain++;
      }
    }
    // All chain and nonchain tasks before i are now in the tree, push i.
    const int64 new_start_min = theta_lambda_tree_.GetEnvelope();
    // Add i to the tree before updating it.
    theta_lambda_tree_.AddOrUpdateEvent(event_of_task_[i], tasks->start_min[i],
                                        tasks->duration_min[i],
                                        tasks->duration_min[i]);
    tasks->start_min[i] = std::max(tasks->start_min[i], new_start_min);
  }
  return true;
}

bool DisjunctivePropagator::ForbiddenIntervals(Tasks* tasks) {
  if (tasks->forbidden_intervals.empty()) return true;
  const int num_tasks = tasks->start_min.size();
  for (int task = 0; task < num_tasks; ++task) {
    if (tasks->duration_min[task] == 0) continue;
    if (tasks->forbidden_intervals[task] == nullptr) continue;
    // If start_min forbidden, push to next feasible value.
    {
      const auto& interval =
          tasks->forbidden_intervals[task]->FirstIntervalGreaterOrEqual(
              tasks->start_min[task]);
      if (interval == tasks->forbidden_intervals[task]->end()) continue;
      if (interval->start <= tasks->start_min[task]) {
        tasks->start_min[task] = CapAdd(interval->end, 1);
      }
    }
    // If end_max forbidden, push to next feasible value.
    {
      const int64 start_max =
          CapSub(tasks->end_max[task], tasks->duration_min[task]);
      const auto& interval =
          tasks->forbidden_intervals[task]->LastIntervalLessOrEqual(start_max);
      if (interval == tasks->forbidden_intervals[task]->end()) continue;
      if (interval->end >= start_max) {
        tasks->end_max[task] =
            CapAdd(interval->start, tasks->duration_min[task] - 1);
      }
    }
    if (CapAdd(tasks->start_min[task], tasks->duration_min[task]) >
        tasks->end_max[task]) {
      return false;
    }
  }
  return true;
}

GlobalVehicleBreaksConstraint::GlobalVehicleBreaksConstraint(
    const RoutingDimension* dimension)
    : Constraint(dimension->model()->solver()),
      model_(dimension->model()),
      dimension_(dimension) {
  vehicle_demons_.resize(model_->vehicles());
}

void GlobalVehicleBreaksConstraint::Post() {
  for (int vehicle = 0; vehicle < model_->vehicles(); vehicle++) {
    if (dimension_->GetBreakIntervalsOfVehicle(vehicle).empty()) continue;
    vehicle_demons_[vehicle] = MakeDelayedConstraintDemon1(
        solver(), this, &GlobalVehicleBreaksConstraint::PropagateVehicle,
        "PropagateVehicle", vehicle);
    for (IntervalVar* interval :
         dimension_->GetBreakIntervalsOfVehicle(vehicle)) {
      interval->WhenAnything(vehicle_demons_[vehicle]);
    }
  }
  const int num_cumuls = dimension_->cumuls().size();
  const int num_nexts = model_->Nexts().size();
  for (int node = 0; node < num_cumuls; node++) {
    Demon* dimension_demon = MakeConstraintDemon1(
        solver(), this, &GlobalVehicleBreaksConstraint::PropagateNode,
        "PropagateNode", node);
    if (node < num_nexts) {
      model_->NextVar(node)->WhenBound(dimension_demon);
      dimension_->SlackVar(node)->WhenRange(dimension_demon);
    }
    model_->VehicleVar(node)->WhenBound(dimension_demon);
    dimension_->CumulVar(node)->WhenRange(dimension_demon);
  }
}

void GlobalVehicleBreaksConstraint::InitialPropagate() {
  for (int vehicle = 0; vehicle < model_->vehicles(); vehicle++) {
    if (!dimension_->GetBreakIntervalsOfVehicle(vehicle).empty() ||
        !dimension_->GetBreakDistanceDurationOfVehicle(vehicle).empty()) {
      PropagateVehicle(vehicle);
    }
  }
}

// This dispatches node events to the right vehicle propagator.
// It also filters out a part of uninteresting events, on which the vehicle
// propagator will not find anything new.
void GlobalVehicleBreaksConstraint::PropagateNode(int node) {
  if (!model_->VehicleVar(node)->Bound()) return;
  const int vehicle = model_->VehicleVar(node)->Min();
  if (vehicle < 0 || vehicle_demons_[vehicle] == nullptr) return;
  EnqueueDelayedDemon(vehicle_demons_[vehicle]);
}

void GlobalVehicleBreaksConstraint::FillPartialPathOfVehicle(int vehicle) {
  path_.clear();
  int current = model_->Start(vehicle);
  while (!model_->IsEnd(current)) {
    path_.push_back(current);
    current = model_->NextVar(current)->Bound()
                  ? model_->NextVar(current)->Min()
                  : model_->End(vehicle);
  }
  path_.push_back(current);
}

void GlobalVehicleBreaksConstraint::FillPathTravels(
    const std::vector<int64>& path) {
  const int num_travels = path.size() - 1;
  min_travel_.resize(num_travels);
  max_travel_.resize(num_travels);
  for (int i = 0; i < num_travels; ++i) {
    min_travel_[i] = dimension_->FixedTransitVar(path[i])->Min();
    max_travel_[i] = dimension_->FixedTransitVar(path[i])->Max();
  }
}

// First, perform energy-based reasoning on intervals and cumul variables.
// Then, perform reasoning on slack variables.
void GlobalVehicleBreaksConstraint::PropagateVehicle(int vehicle) {
  // Fill path and pre/post travel information.
  FillPartialPathOfVehicle(vehicle);
  const int num_nodes = path_.size();
  FillPathTravels(path_);
  {
    const int index = dimension_->GetPreTravelEvaluatorOfVehicle(vehicle);
    if (index == -1) {
      pre_travel_.assign(num_nodes - 1, 0);
    } else {
      FillPathEvaluation(path_, model_->TransitCallback(index), &pre_travel_);
    }
  }
  {
    const int index = dimension_->GetPostTravelEvaluatorOfVehicle(vehicle);
    if (index == -1) {
      post_travel_.assign(num_nodes - 1, 0);
    } else {
      FillPathEvaluation(path_, model_->TransitCallback(index), &post_travel_);
    }
  }
  // The last travel might not be fixed: in that case, relax its information.
  if (!model_->NextVar(path_[num_nodes - 2])->Bound()) {
    min_travel_.back() = 0;
    max_travel_.back() = kint64max;
    pre_travel_.back() = 0;
    post_travel_.back() = 0;
  }

  // Fill tasks from path, break intervals, and break constraints.
  tasks_.Clear();
  AppendTasksFromPath(path_, min_travel_, max_travel_, pre_travel_,
                      post_travel_, *dimension_, &tasks_);
  tasks_.num_chain_tasks = tasks_.start_min.size();
  AppendTasksFromIntervals(dimension_->GetBreakIntervalsOfVehicle(vehicle),
                           &tasks_);
  tasks_.distance_duration =
      dimension_->GetBreakDistanceDurationOfVehicle(vehicle);

  // Do the actual reasoning, no need to continue if infeasible.
  if (!disjunctive_propagator_.Propagate(&tasks_)) solver()->Fail();

  // Make task translators to help set new bounds of CP variables.
  task_translators_.clear();
  for (int i = 0; i < num_nodes; ++i) {
    const int64 before_visit = (i == 0) ? 0 : post_travel_[i - 1];
    const int64 after_visit = (i == num_nodes - 1) ? 0 : pre_travel_[i];
    task_translators_.emplace_back(dimension_->CumulVar(path_[i]), before_visit,
                                   after_visit);
    if (i == num_nodes - 1) break;
    task_translators_.emplace_back();  // Dummy translator for travel tasks.
  }
  for (IntervalVar* interval :
       dimension_->GetBreakIntervalsOfVehicle(vehicle)) {
    if (!interval->MustBePerformed()) continue;
    task_translators_.emplace_back(interval);
  }

  // Push new bounds to CP variables.
  const int num_tasks = tasks_.start_min.size();
  for (int task = 0; task < num_tasks; ++task) {
    task_translators_[task].SetStartMin(tasks_.start_min[task]);
    task_translators_[task].SetStartMax(tasks_.start_max[task]);
    task_translators_[task].SetDurationMin(tasks_.duration_min[task]);
    task_translators_[task].SetEndMin(tasks_.end_min[task]);
    task_translators_[task].SetEndMax(tasks_.end_max[task]);
  }

  // Reasoning on slack variables: when intervals must be inside an arc,
  // that arc's slack must be large enough to accommodate for those.
  // TODO(user): Make a version more efficient than O(n^2).
  if (dimension_->GetBreakIntervalsOfVehicle(vehicle).empty()) return;
  // If the last arc of the path was not bound, do not change slack.
  const int64 last_bound_arc =
      num_nodes - 2 - (model_->NextVar(path_[num_nodes - 2])->Bound() ? 0 : 1);
  for (int i = 0; i <= last_bound_arc; ++i) {
    const int64 arc_start_max = CapSub(dimension_->CumulVar(path_[i])->Max(),
                                       i > 0 ? post_travel_[i - 1] : 0);
    const int64 arc_end_min =
        CapAdd(dimension_->CumulVar(path_[i + 1])->Min(),
               i < num_nodes - 2 ? pre_travel_[i + 1] : 0);
    int64 total_break_inside_arc = 0;
    for (IntervalVar* interval :
         dimension_->GetBreakIntervalsOfVehicle(vehicle)) {
      if (!interval->MustBePerformed()) continue;
      const int64 interval_start_max = interval->StartMax();
      const int64 interval_end_min = interval->EndMin();
      const int64 interval_duration_min = interval->DurationMin();
      // If interval cannot end before the arc's from node and
      // cannot start after the 'to' node, then it must be inside the arc.
      if (arc_start_max < interval_end_min &&
          interval_start_max < arc_end_min) {
        total_break_inside_arc += interval_duration_min;
      }
    }
    dimension_->SlackVar(path_[i])->SetMin(total_break_inside_arc);
  }
  // Reasoning on optional intervals.
  // TODO(user): merge this with energy-based reasoning.
  // If there is no optional interval, skip the rest of this function.
  {
    bool has_optional = false;
    for (const IntervalVar* interval :
         dimension_->GetBreakIntervalsOfVehicle(vehicle)) {
      if (interval->MayBePerformed() && !interval->MustBePerformed()) {
        has_optional = true;
        break;
      }
    }
    if (!has_optional) return;
  }
  const std::vector<IntervalVar*>& break_intervals =
      dimension_->GetBreakIntervalsOfVehicle(vehicle);
  for (int pos = 0; pos < num_nodes - 1; ++pos) {
    const int64 current_slack_max = dimension_->SlackVar(path_[pos])->Max();
    const int64 visit_start_offset = pos > 0 ? post_travel_[pos - 1] : 0;
    const int64 visit_start_max =
        CapSub(dimension_->CumulVar(path_[pos])->Max(), visit_start_offset);
    const int64 visit_end_offset = (pos < num_nodes - 1) ? pre_travel_[pos] : 0;
    const int64 visit_end_min =
        CapAdd(dimension_->CumulVar(path_[pos])->Min(), visit_end_offset);

    for (IntervalVar* interval : break_intervals) {
      if (!interval->MayBePerformed()) continue;
      const bool interval_is_performed = interval->MustBePerformed();
      const int64 interval_start_max = interval->StartMax();
      const int64 interval_end_min = interval->EndMin();
      const int64 interval_duration_min = interval->DurationMin();
      // When interval cannot fit inside current arc,
      // do disjunctive reasoning on full arc.
      if (pos < num_nodes - 1 && interval_duration_min > current_slack_max) {
        // The arc lasts from CumulVar(path_[pos]) - post_travel_[pos] to
        // CumulVar(path_[pos+1]) + pre_travel_[pos+1].
        const int64 arc_start_offset = pos > 0 ? post_travel_[pos - 1] : 0;
        const int64 arc_start_max = visit_start_max;
        const int64 arc_end_offset =
            (pos < num_nodes - 2) ? pre_travel_[pos + 1] : 0;
        const int64 arc_end_min =
            CapAdd(dimension_->CumulVar(path_[pos + 1])->Min(), arc_end_offset);
        // Interval not before.
        if (arc_start_max < interval_end_min) {
          interval->SetStartMin(arc_end_min);
          if (interval_is_performed) {
            dimension_->CumulVar(path_[pos + 1])
                ->SetMax(CapSub(interval_start_max, arc_end_offset));
          }
        }
        // Interval not after.
        if (interval_start_max < arc_end_min) {
          interval->SetEndMax(arc_start_max);
          if (interval_is_performed) {
            dimension_->CumulVar(path_[pos])
                ->SetMin(CapSub(interval_end_min, arc_start_offset));
          }
        }
        continue;
      }
      // Interval could fit inside arc: do disjunctive reasoning between
      // interval and visit.
      // Interval not before.
      if (visit_start_max < interval_end_min) {
        interval->SetStartMin(visit_end_min);
        if (interval_is_performed) {
          dimension_->CumulVar(path_[pos])
              ->SetMax(CapSub(interval_start_max, visit_end_offset));
        }
      }
      // Interval not after.
      if (interval_start_max < visit_end_min) {
        interval->SetEndMax(visit_start_max);
        if (interval_is_performed) {
          dimension_->CumulVar(path_[pos])
              ->SetMin(CapAdd(interval_end_min, visit_start_offset));
        }
      }
    }
  }
}

namespace {
class VehicleBreaksFilter : public BasePathFilter {
 public:
  VehicleBreaksFilter(const RoutingModel& routing_model,
                      const RoutingDimension& dimension);
  std::string DebugString() const override { return "VehicleBreaksFilter"; }
  bool AcceptPath(int64 path_start, int64 chain_start,
                  int64 chain_end) override;

 private:
  // Fills path_ with the path of vehicle, start to end.
  void FillPathOfVehicle(int64 vehicle);
  std::vector<int64> path_;
  // Handles to model.
  const RoutingModel& model_;
  const RoutingDimension& dimension_;
  // Strong energy-based filtering algorithm.
  DisjunctivePropagator disjunctive_propagator_;
  DisjunctivePropagator::Tasks tasks_;
  // Used to check whether propagation changed a vector.
  std::vector<int64> old_start_min_;
  std::vector<int64> old_start_max_;
  std::vector<int64> old_end_min_;
  std::vector<int64> old_end_max_;

  std::vector<int> start_to_vehicle_;
  std::vector<int64> min_travel_;
  std::vector<int64> max_travel_;
  std::vector<int64> pre_travel_;
  std::vector<int64> post_travel_;
};

VehicleBreaksFilter::VehicleBreaksFilter(const RoutingModel& routing_model,
                                         const RoutingDimension& dimension)
    : BasePathFilter(routing_model.Nexts(),
                     routing_model.Size() + routing_model.vehicles()),
      model_(routing_model),
      dimension_(dimension) {
  DCHECK(dimension_.HasBreakConstraints());
  start_to_vehicle_.resize(Size(), -1);
  for (int i = 0; i < routing_model.vehicles(); ++i) {
    start_to_vehicle_[routing_model.Start(i)] = i;
  }
}

void VehicleBreaksFilter::FillPathOfVehicle(int64 vehicle) {
  path_.clear();
  int current = model_.Start(vehicle);
  while (!model_.IsEnd(current)) {
    path_.push_back(current);
    current = GetNext(current);
  }
  path_.push_back(current);
}

bool VehicleBreaksFilter::AcceptPath(int64 path_start, int64 chain_start,
                                     int64 chain_end) {
  const int vehicle = start_to_vehicle_[path_start];
  if (dimension_.GetBreakIntervalsOfVehicle(vehicle).empty() &&
      dimension_.GetBreakDistanceDurationOfVehicle(vehicle).empty()) {
    return true;
  }
  // Fill path and pre/post travel information.
  FillPathOfVehicle(vehicle);
  FillPathEvaluation(path_, dimension_.transit_evaluator(vehicle),
                     &min_travel_);
  max_travel_.assign(min_travel_.size(), kint64max);
  {
    const int index = dimension_.GetPreTravelEvaluatorOfVehicle(vehicle);
    if (index == -1) {
      pre_travel_.assign(min_travel_.size(), 0);
    } else {
      FillPathEvaluation(path_, model_.TransitCallback(index), &pre_travel_);
    }
  }
  {
    const int index = dimension_.GetPostTravelEvaluatorOfVehicle(vehicle);
    if (index == -1) {
      post_travel_.assign(min_travel_.size(), 0);
    } else {
      FillPathEvaluation(path_, model_.TransitCallback(index), &post_travel_);
    }
  }
  // Fill tasks from path, forbidden intervals, breaks and break constraints.
  tasks_.Clear();
  AppendTasksFromPath(path_, min_travel_, max_travel_, pre_travel_,
                      post_travel_, dimension_, &tasks_);
  tasks_.num_chain_tasks = tasks_.start_min.size();
  AppendTasksFromIntervals(dimension_.GetBreakIntervalsOfVehicle(vehicle),
                           &tasks_);
  // Add forbidden intervals only if a node has some.
  tasks_.forbidden_intervals.clear();
  if (std::any_of(path_.begin(), path_.end(), [this](int64 node) {
        return dimension_.forbidden_intervals()[node].NumIntervals() > 0;
      })) {
    tasks_.forbidden_intervals.assign(tasks_.start_min.size(), nullptr);
    for (int i = 0; i < path_.size(); ++i) {
      tasks_.forbidden_intervals[2 * i] =
          &(dimension_.forbidden_intervals()[path_[i]]);
    }
  }
  // Max distance duration constraint.
  tasks_.distance_duration =
      dimension_.GetBreakDistanceDurationOfVehicle(vehicle);

  // Reduce bounds until failure or fixed point is reached.
  // We set a maximum amount of iterations to avoid slow propagation.
  bool is_feasible = true;
  int maximum_num_iterations = 8;
  while (--maximum_num_iterations >= 0) {
    old_start_min_ = tasks_.start_min;
    old_start_max_ = tasks_.start_max;
    old_end_min_ = tasks_.end_min;
    old_end_max_ = tasks_.end_max;
    is_feasible = disjunctive_propagator_.Propagate(&tasks_);
    if (!is_feasible) break;
    // If fixed point reached, stop.
    if ((old_start_min_ == tasks_.start_min) &&
        (old_start_max_ == tasks_.start_max) &&
        (old_end_min_ == tasks_.end_min) && (old_end_max_ == tasks_.end_max)) {
      break;
    }
  }
  return is_feasible;
}

}  // namespace

IntVarLocalSearchFilter* MakeVehicleBreaksFilter(
    const RoutingModel& routing_model, const RoutingDimension& dimension) {
  return routing_model.solver()->RevAlloc(
      new VehicleBreaksFilter(routing_model, dimension));
}

}  // namespace operations_research

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

// Implementation of all classes related to routing and search.
// This includes decision builders, local search neighborhood operators
// and local search filters.
// TODO(user): Move all existing routing search code here.

#include "ortools/constraint_solver/routing_search.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <numeric>
#include <queue>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/memory/memory.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ortools/base/adjustable_priority_queue-inl.h"
#include "ortools/base/adjustable_priority_queue.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_index_manager.h"
#include "ortools/constraint_solver/routing_types.h"
#include "ortools/graph/christofides.h"
#include "ortools/util/bitset.h"
#include "ortools/util/range_query_function.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {
class LocalSearchPhaseParameters;
}  // namespace operations_research

ABSL_FLAG(bool, routing_shift_insertion_cost_by_penalty, true,
          "Shift insertion costs by the penalty of the inserted node(s).");

ABSL_FLAG(int64_t, sweep_sectors, 1,
          "The number of sectors the space is divided into before it is sweeped"
          " by the ray.");

namespace operations_research {

// --- VehicleTypeCurator ---

void VehicleTypeCurator::Reset(const std::function<bool(int)>& store_vehicle) {
  const std::vector<std::set<VehicleClassEntry>>& all_vehicle_classes_per_type =
      vehicle_type_container_->sorted_vehicle_classes_per_type;
  sorted_vehicle_classes_per_type_.resize(all_vehicle_classes_per_type.size());
  const std::vector<std::deque<int>>& all_vehicles_per_class =
      vehicle_type_container_->vehicles_per_vehicle_class;
  vehicles_per_vehicle_class_.resize(all_vehicles_per_class.size());

  for (int type = 0; type < all_vehicle_classes_per_type.size(); type++) {
    std::set<VehicleClassEntry>& stored_class_entries =
        sorted_vehicle_classes_per_type_[type];
    stored_class_entries.clear();
    for (VehicleClassEntry class_entry : all_vehicle_classes_per_type[type]) {
      const int vehicle_class = class_entry.vehicle_class;
      std::vector<int>& stored_vehicles =
          vehicles_per_vehicle_class_[vehicle_class];
      stored_vehicles.clear();
      for (int vehicle : all_vehicles_per_class[vehicle_class]) {
        if (store_vehicle(vehicle)) {
          stored_vehicles.push_back(vehicle);
        }
      }
      if (!stored_vehicles.empty()) {
        stored_class_entries.insert(class_entry);
      }
    }
  }
}

void VehicleTypeCurator::Update(
    const std::function<bool(int)>& remove_vehicle) {
  for (std::set<VehicleClassEntry>& class_entries :
       sorted_vehicle_classes_per_type_) {
    auto class_entry_it = class_entries.begin();
    while (class_entry_it != class_entries.end()) {
      const int vehicle_class = class_entry_it->vehicle_class;
      std::vector<int>& vehicles = vehicles_per_vehicle_class_[vehicle_class];
      vehicles.erase(std::remove_if(vehicles.begin(), vehicles.end(),
                                    [&remove_vehicle](int vehicle) {
                                      return remove_vehicle(vehicle);
                                    }),
                     vehicles.end());
      if (vehicles.empty()) {
        class_entry_it = class_entries.erase(class_entry_it);
      } else {
        class_entry_it++;
      }
    }
  }
}

bool VehicleTypeCurator::HasCompatibleVehicleOfType(
    int type, const std::function<bool(int)>& vehicle_is_compatible) const {
  for (const VehicleClassEntry& vehicle_class_entry :
       sorted_vehicle_classes_per_type_[type]) {
    for (int vehicle :
         vehicles_per_vehicle_class_[vehicle_class_entry.vehicle_class]) {
      if (vehicle_is_compatible(vehicle)) return true;
    }
  }
  return false;
}

std::pair<int, int> VehicleTypeCurator::GetCompatibleVehicleOfType(
    int type, std::function<bool(int)> vehicle_is_compatible,
    std::function<bool(int)> stop_and_return_vehicle) {
  std::set<VehicleTypeCurator::VehicleClassEntry>& sorted_classes =
      sorted_vehicle_classes_per_type_[type];
  auto vehicle_class_it = sorted_classes.begin();

  while (vehicle_class_it != sorted_classes.end()) {
    const int vehicle_class = vehicle_class_it->vehicle_class;
    std::vector<int>& vehicles = vehicles_per_vehicle_class_[vehicle_class];
    DCHECK(!vehicles.empty());

    for (auto vehicle_it = vehicles.begin(); vehicle_it != vehicles.end();
         vehicle_it++) {
      const int vehicle = *vehicle_it;
      if (vehicle_is_compatible(vehicle)) {
        vehicles.erase(vehicle_it);
        if (vehicles.empty()) {
          sorted_classes.erase(vehicle_class_it);
        }
        return {vehicle, -1};
      }
      if (stop_and_return_vehicle(vehicle)) {
        return {-1, vehicle};
      }
    }
    // If no compatible vehicle was found in this class, move on to the next
    // vehicle class.
    vehicle_class_it++;
  }
  // No compatible vehicle of the given type was found and the stopping
  // condition wasn't met.
  return {-1, -1};
}

// - Models with pickup/deliveries or node precedences are best handled by
//   PARALLEL_CHEAPEST_INSERTION.
// - As of January 2018, models with single nodes and at least one node with
//   only one allowed vehicle are better solved by PATH_MOST_CONSTRAINED_ARC.
// - In all other cases, PATH_CHEAPEST_ARC is used.
// TODO(user): Make this smarter.
FirstSolutionStrategy::Value AutomaticFirstSolutionStrategy(
    bool has_pickup_deliveries, bool has_node_precedences,
    bool has_single_vehicle_node) {
  if (has_pickup_deliveries || has_node_precedences) {
    return FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION;
  }
  if (has_single_vehicle_node) {
    return FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC;
  }
  return FirstSolutionStrategy::PATH_CHEAPEST_ARC;
}

// --- First solution decision builder ---

// IntVarFilteredDecisionBuilder

IntVarFilteredDecisionBuilder::IntVarFilteredDecisionBuilder(
    std::unique_ptr<IntVarFilteredHeuristic> heuristic)
    : heuristic_(std::move(heuristic)) {}

Decision* IntVarFilteredDecisionBuilder::Next(Solver* solver) {
  Assignment* const assignment = heuristic_->BuildSolution();
  if (assignment != nullptr) {
    VLOG(2) << "Number of decisions: " << heuristic_->number_of_decisions();
    VLOG(2) << "Number of rejected decisions: "
            << heuristic_->number_of_rejects();
    assignment->Restore();
  } else {
    solver->Fail();
  }
  return nullptr;
}

int64_t IntVarFilteredDecisionBuilder::number_of_decisions() const {
  return heuristic_->number_of_decisions();
}

int64_t IntVarFilteredDecisionBuilder::number_of_rejects() const {
  return heuristic_->number_of_rejects();
}

std::string IntVarFilteredDecisionBuilder::DebugString() const {
  return absl::StrCat("IntVarFilteredDecisionBuilder(",
                      heuristic_->DebugString(), ")");
}

// --- First solution heuristics ---

// IntVarFilteredHeuristic

IntVarFilteredHeuristic::IntVarFilteredHeuristic(
    Solver* solver, const std::vector<IntVar*>& vars,
    LocalSearchFilterManager* filter_manager)
    : assignment_(solver->MakeAssignment()),
      solver_(solver),
      vars_(vars),
      delta_(solver->MakeAssignment()),
      is_in_delta_(vars_.size(), false),
      empty_(solver->MakeAssignment()),
      filter_manager_(filter_manager),
      number_of_decisions_(0),
      number_of_rejects_(0) {
  assignment_->MutableIntVarContainer()->Resize(vars_.size());
  delta_indices_.reserve(vars_.size());
}

void IntVarFilteredHeuristic::ResetSolution() {
  number_of_decisions_ = 0;
  number_of_rejects_ = 0;
  // Wiping assignment when starting a new search.
  assignment_->MutableIntVarContainer()->Clear();
  assignment_->MutableIntVarContainer()->Resize(vars_.size());
  delta_->MutableIntVarContainer()->Clear();
  SynchronizeFilters();
}

Assignment* const IntVarFilteredHeuristic::BuildSolution() {
  ResetSolution();
  if (!InitializeSolution()) {
    return nullptr;
  }
  SynchronizeFilters();
  if (BuildSolutionInternal()) {
    return assignment_;
  }
  return nullptr;
}

const Assignment* RoutingFilteredHeuristic::BuildSolutionFromRoutes(
    const std::function<int64_t(int64_t)>& next_accessor) {
  ResetSolution();
  ResetVehicleIndices();
  // NOTE: We don't need to clear or pre-set the two following vectors as the
  // for loop below will set all elements.
  start_chain_ends_.resize(model()->vehicles());
  end_chain_starts_.resize(model()->vehicles());

  for (int v = 0; v < model_->vehicles(); v++) {
    int64_t node = model_->Start(v);
    while (!model_->IsEnd(node)) {
      const int64_t next = next_accessor(node);
      DCHECK_NE(next, node);
      SetValue(node, next);
      SetVehicleIndex(node, v);
      node = next;
    }
    // We relax all routes from start to end, so routes can now be extended
    // by inserting nodes between the start and end.
    start_chain_ends_[v] = model()->Start(v);
    end_chain_starts_[v] = model()->End(v);
  }
  if (!Commit()) {
    return nullptr;
  }
  SynchronizeFilters();
  if (BuildSolutionInternal()) {
    return assignment_;
  }
  return nullptr;
}

bool IntVarFilteredHeuristic::Commit() {
  ++number_of_decisions_;
  const bool accept = FilterAccept();
  if (accept) {
    const Assignment::IntContainer& delta_container = delta_->IntVarContainer();
    const int delta_size = delta_container.Size();
    Assignment::IntContainer* const container =
        assignment_->MutableIntVarContainer();
    for (int i = 0; i < delta_size; ++i) {
      const IntVarElement& delta_element = delta_container.Element(i);
      IntVar* const var = delta_element.Var();
      DCHECK_EQ(var, vars_[delta_indices_[i]]);
      container->AddAtPosition(var, delta_indices_[i])
          ->SetValue(delta_element.Value());
    }
    SynchronizeFilters();
  } else {
    ++number_of_rejects_;
  }
  // Reset is_in_delta to all false.
  for (const int delta_index : delta_indices_) {
    is_in_delta_[delta_index] = false;
  }
  delta_->Clear();
  delta_indices_.clear();
  return accept;
}

void IntVarFilteredHeuristic::SynchronizeFilters() {
  if (filter_manager_) filter_manager_->Synchronize(assignment_, delta_);
}

bool IntVarFilteredHeuristic::FilterAccept() {
  if (!filter_manager_) return true;
  LocalSearchMonitor* const monitor = solver_->GetLocalSearchMonitor();
  return filter_manager_->Accept(monitor, delta_, empty_,
                                 std::numeric_limits<int64_t>::min(),
                                 std::numeric_limits<int64_t>::max());
}

// RoutingFilteredHeuristic

RoutingFilteredHeuristic::RoutingFilteredHeuristic(
    RoutingModel* model, LocalSearchFilterManager* filter_manager)
    : IntVarFilteredHeuristic(model->solver(), model->Nexts(), filter_manager),
      model_(model) {}

bool RoutingFilteredHeuristic::InitializeSolution() {
  // Find the chains of nodes (when nodes have their "Next" value bound in the
  // current solution, it forms a link in a chain). Eventually, starts[end]
  // will contain the index of the first node of the chain ending at node 'end'
  // and ends[start] will be the last node of the chain starting at node
  // 'start'. Values of starts[node] and ends[node] for other nodes is used
  // for intermediary computations and do not necessarily reflect actual chain
  // starts and ends.

  // Start by adding partial start chains to current assignment.
  start_chain_ends_.clear();
  start_chain_ends_.resize(model()->vehicles(), -1);
  end_chain_starts_.clear();
  end_chain_starts_.resize(model()->vehicles(), -1);

  ResetVehicleIndices();
  for (int vehicle = 0; vehicle < model()->vehicles(); ++vehicle) {
    int64_t node = model()->Start(vehicle);
    while (!model()->IsEnd(node) && Var(node)->Bound()) {
      const int64_t next = Var(node)->Min();
      SetValue(node, next);
      SetVehicleIndex(node, vehicle);
      node = next;
    }
    start_chain_ends_[vehicle] = node;
  }

  std::vector<int64_t> starts(Size() + model()->vehicles(), -1);
  std::vector<int64_t> ends(Size() + model()->vehicles(), -1);
  for (int node = 0; node < Size() + model()->vehicles(); ++node) {
    // Each node starts as a singleton chain.
    starts[node] = node;
    ends[node] = node;
  }
  std::vector<bool> touched(Size(), false);
  for (int node = 0; node < Size(); ++node) {
    int current = node;
    while (!model()->IsEnd(current) && !touched[current]) {
      touched[current] = true;
      IntVar* const next_var = Var(current);
      if (next_var->Bound()) {
        current = next_var->Value();
      }
    }
    // Merge the sub-chain starting from 'node' and ending at 'current' with
    // the existing sub-chain starting at 'current'.
    starts[ends[current]] = starts[node];
    ends[starts[node]] = ends[current];
  }

  // Set each route to be the concatenation of the chain at its starts and the
  // chain at its end, without nodes in between.
  for (int vehicle = 0; vehicle < model()->vehicles(); ++vehicle) {
    end_chain_starts_[vehicle] = starts[model()->End(vehicle)];
    int64_t node = start_chain_ends_[vehicle];
    if (!model()->IsEnd(node)) {
      int64_t next = starts[model()->End(vehicle)];
      SetValue(node, next);
      SetVehicleIndex(node, vehicle);
      node = next;
      while (!model()->IsEnd(node)) {
        next = Var(node)->Min();
        SetValue(node, next);
        SetVehicleIndex(node, vehicle);
        node = next;
      }
    }
  }

  if (!Commit()) {
    ResetVehicleIndices();
    return false;
  }
  return true;
}

void RoutingFilteredHeuristic::MakeDisjunctionNodesUnperformed(int64_t node) {
  model()->ForEachNodeInDisjunctionWithMaxCardinalityFromIndex(
      node, 1, [this, node](int alternate) {
        if (node != alternate && !Contains(alternate)) {
          SetValue(alternate, alternate);
        }
      });
}

void RoutingFilteredHeuristic::MakeUnassignedNodesUnperformed() {
  for (int index = 0; index < Size(); ++index) {
    if (!Contains(index)) {
      SetValue(index, index);
    }
  }
}

void RoutingFilteredHeuristic::MakePartiallyPerformedPairsUnperformed() {
  std::vector<bool> to_make_unperformed(Size(), false);
  for (const auto& [pickups, deliveries] :
       model()->GetPickupAndDeliveryPairs()) {
    int64_t performed_pickup = -1;
    for (int64_t pickup : pickups) {
      if (Contains(pickup) && Value(pickup) != pickup) {
        performed_pickup = pickup;
        break;
      }
    }
    int64_t performed_delivery = -1;
    for (int64_t delivery : deliveries) {
      if (Contains(delivery) && Value(delivery) != delivery) {
        performed_delivery = delivery;
        break;
      }
    }
    if ((performed_pickup == -1) != (performed_delivery == -1)) {
      if (performed_pickup != -1) {
        to_make_unperformed[performed_pickup] = true;
      }
      if (performed_delivery != -1) {
        to_make_unperformed[performed_delivery] = true;
      }
    }
  }
  for (int index = 0; index < Size(); ++index) {
    if (to_make_unperformed[index] || !Contains(index)) continue;
    int64_t next = Value(index);
    while (next < Size() && to_make_unperformed[next]) {
      const int64_t next_of_next = Value(next);
      SetValue(index, next_of_next);
      SetValue(next, next);
      next = next_of_next;
    }
  }
}

// CheapestInsertionFilteredHeuristic

CheapestInsertionFilteredHeuristic::CheapestInsertionFilteredHeuristic(
    RoutingModel* model,
    std::function<int64_t(int64_t, int64_t, int64_t)> evaluator,
    std::function<int64_t(int64_t)> penalty_evaluator,
    LocalSearchFilterManager* filter_manager)
    : RoutingFilteredHeuristic(model, filter_manager),
      evaluator_(std::move(evaluator)),
      penalty_evaluator_(std::move(penalty_evaluator)) {}

std::vector<std::vector<CheapestInsertionFilteredHeuristic::StartEndValue>>
CheapestInsertionFilteredHeuristic::ComputeStartEndDistanceForVehicles(
    const std::vector<int>& vehicles) {
  const absl::flat_hash_set<int> vehicle_set(vehicles.begin(), vehicles.end());
  std::vector<std::vector<StartEndValue>> start_end_distances_per_node(
      model()->Size());

  for (int node = 0; node < model()->Size(); node++) {
    if (Contains(node)) continue;
    std::vector<StartEndValue>& start_end_distances =
        start_end_distances_per_node[node];
    const IntVar* const vehicle_var = model()->VehicleVar(node);
    const int64_t num_allowed_vehicles = vehicle_var->Size();

    const auto add_distance = [this, node, num_allowed_vehicles,
                               &start_end_distances](int vehicle) {
      const int64_t start = model()->Start(vehicle);
      const int64_t end = model()->End(vehicle);

      // We compute the distance of node to the start/end nodes of the route.
      const int64_t distance =
          CapAdd(model()->GetArcCostForVehicle(start, node, vehicle),
                 model()->GetArcCostForVehicle(node, end, vehicle));
      start_end_distances.push_back({num_allowed_vehicles, distance, vehicle});
    };
    // Iterating over an IntVar domain is faster than calling Contains.
    // Therefore we iterate on 'vehicles' only if it's smaller than the domain
    // size of the VehicleVar.
    if (num_allowed_vehicles < vehicles.size()) {
      std::unique_ptr<IntVarIterator> it(
          vehicle_var->MakeDomainIterator(false));
      for (const int64_t vehicle : InitAndGetValues(it.get())) {
        if (vehicle < 0 || !vehicle_set.contains(vehicle)) continue;
        add_distance(vehicle);
      }
    } else {
      for (const int vehicle : vehicles) {
        if (!vehicle_var->Contains(vehicle)) continue;
        add_distance(vehicle);
      }
    }
    // Sort the distances for the node to all start/ends of available vehicles
    // in decreasing order.
    std::sort(start_end_distances.begin(), start_end_distances.end(),
              [](const StartEndValue& first, const StartEndValue& second) {
                return second < first;
              });
  }
  return start_end_distances_per_node;
}

template <class Queue>
void CheapestInsertionFilteredHeuristic::InitializePriorityQueue(
    std::vector<std::vector<StartEndValue>>* start_end_distances_per_node,
    Queue* priority_queue) {
  const int num_nodes = model()->Size();
  DCHECK_EQ(start_end_distances_per_node->size(), num_nodes);

  for (int node = 0; node < num_nodes; node++) {
    if (Contains(node)) continue;
    std::vector<StartEndValue>& start_end_distances =
        (*start_end_distances_per_node)[node];
    if (start_end_distances.empty()) {
      continue;
    }
    // Put the best StartEndValue for this node in the priority queue.
    const StartEndValue& start_end_value = start_end_distances.back();
    priority_queue->push(std::make_pair(start_end_value, node));
    start_end_distances.pop_back();
  }
}

void CheapestInsertionFilteredHeuristic::InsertBetween(int64_t node,
                                                       int64_t predecessor,
                                                       int64_t successor) {
  SetValue(predecessor, node);
  SetValue(node, successor);
  MakeDisjunctionNodesUnperformed(node);
}

void CheapestInsertionFilteredHeuristic::AppendInsertionPositionsAfter(
    int64_t node_to_insert, int64_t start, int64_t next_after_start,
    int vehicle, std::vector<NodeInsertion>* node_insertions) {
  DCHECK(node_insertions != nullptr);
  int64_t insert_after = start;
  while (!model()->IsEnd(insert_after)) {
    const int64_t insert_before =
        (insert_after == start) ? next_after_start : Value(insert_after);
    node_insertions->push_back(
        {insert_after, vehicle,
         GetInsertionCostForNodeAtPosition(node_to_insert, insert_after,
                                           insert_before, vehicle)});
    insert_after = insert_before;
  }
}

int64_t CheapestInsertionFilteredHeuristic::GetInsertionCostForNodeAtPosition(
    int64_t node_to_insert, int64_t insert_after, int64_t insert_before,
    int vehicle) const {
  return CapSub(CapAdd(evaluator_(insert_after, node_to_insert, vehicle),
                       evaluator_(node_to_insert, insert_before, vehicle)),
                evaluator_(insert_after, insert_before, vehicle));
}

int64_t CheapestInsertionFilteredHeuristic::GetUnperformedValue(
    int64_t node_to_insert) const {
  if (penalty_evaluator_ != nullptr) {
    return penalty_evaluator_(node_to_insert);
  }
  return std::numeric_limits<int64_t>::max();
}

namespace {
template <class T>
void SortAndExtractPairSeconds(std::vector<std::pair<int64_t, T>>* pairs,
                               std::vector<T>* sorted_seconds) {
  CHECK(pairs != nullptr);
  CHECK(sorted_seconds != nullptr);
  std::sort(pairs->begin(), pairs->end());
  sorted_seconds->reserve(pairs->size());
  for (const std::pair<int64_t, T>& p : *pairs) {
    sorted_seconds->push_back(p.second);
  }
}
}  // namespace

// Priority queue entries used by global cheapest insertion heuristic.

// Entry in priority queue containing the insertion positions of a node pair.
class GlobalCheapestInsertionFilteredHeuristic::PairEntry {
 public:
  PairEntry(int pickup_to_insert, int pickup_insert_after,
            int delivery_to_insert, int delivery_insert_after, int vehicle,
            int64_t bucket)
      : value_(std::numeric_limits<int64_t>::max()),
        heap_index_(-1),
        pickup_to_insert_(pickup_to_insert),
        pickup_insert_after_(pickup_insert_after),
        delivery_to_insert_(delivery_to_insert),
        delivery_insert_after_(delivery_insert_after),
        vehicle_(vehicle),
        bucket_(bucket) {}
  // Note: for compatibility reasons, comparator follows tie-breaking rules used
  // in the first version of GlobalCheapestInsertion.
  bool operator<(const PairEntry& other) const {
    // We give higher priority to insertions from lower buckets.
    if (bucket_ != other.bucket_) {
      return bucket_ > other.bucket_;
    }
    // We then compare by value, then we favor insertions (vehicle != -1).
    // The rest of the tie-breaking is done with std::tie.
    if (value_ != other.value_) {
      return value_ > other.value_;
    }
    if ((vehicle_ == -1) ^ (other.vehicle_ == -1)) {
      return vehicle_ == -1;
    }
    return std::tie(pickup_insert_after_, pickup_to_insert_,
                    delivery_insert_after_, delivery_to_insert_, vehicle_) >
           std::tie(other.pickup_insert_after_, other.pickup_to_insert_,
                    other.delivery_insert_after_, other.delivery_to_insert_,
                    other.vehicle_);
  }
  void SetHeapIndex(int h) { heap_index_ = h; }
  int GetHeapIndex() const { return heap_index_; }
  void set_value(int64_t value) { value_ = value; }
  int pickup_to_insert() const { return pickup_to_insert_; }
  int pickup_insert_after() const { return pickup_insert_after_; }
  void set_pickup_insert_after(int pickup_insert_after) {
    pickup_insert_after_ = pickup_insert_after;
  }
  int delivery_to_insert() const { return delivery_to_insert_; }
  int delivery_insert_after() const { return delivery_insert_after_; }
  int vehicle() const { return vehicle_; }
  void set_vehicle(int vehicle) { vehicle_ = vehicle; }

 private:
  int64_t value_;
  int heap_index_;
  const int pickup_to_insert_;
  int pickup_insert_after_;
  const int delivery_to_insert_;
  const int delivery_insert_after_;
  int vehicle_;
  const int64_t bucket_;
};

// Entry in priority queue containing the insertion position of a node.
class GlobalCheapestInsertionFilteredHeuristic::NodeEntry {
 public:
  NodeEntry(int node_to_insert, int insert_after, int vehicle, int64_t bucket)
      : value_(std::numeric_limits<int64_t>::max()),
        heap_index_(-1),
        node_to_insert_(node_to_insert),
        insert_after_(insert_after),
        vehicle_(vehicle),
        bucket_(bucket) {}
  bool operator<(const NodeEntry& other) const {
    // See PairEntry::operator<(), above. This one is similar.
    if (bucket_ != other.bucket_) {
      return bucket_ > other.bucket_;
    }
    if (value_ != other.value_) {
      return value_ > other.value_;
    }
    if ((vehicle_ == -1) ^ (other.vehicle_ == -1)) {
      return vehicle_ == -1;
    }
    return std::tie(insert_after_, node_to_insert_, vehicle_) >
           std::tie(other.insert_after_, other.node_to_insert_, other.vehicle_);
  }
  void SetHeapIndex(int h) { heap_index_ = h; }
  int GetHeapIndex() const { return heap_index_; }
  void set_value(int64_t value) { value_ = value; }
  int node_to_insert() const { return node_to_insert_; }
  int insert_after() const { return insert_after_; }
  void set_insert_after(int insert_after) { insert_after_ = insert_after; }
  int vehicle() const { return vehicle_; }
  void set_vehicle(int vehicle) { vehicle_ = vehicle; }

 private:
  int64_t value_;
  int heap_index_;
  const int node_to_insert_;
  int insert_after_;
  int vehicle_;
  const int64_t bucket_;
};

// GlobalCheapestInsertionFilteredHeuristic

GlobalCheapestInsertionFilteredHeuristic::
    GlobalCheapestInsertionFilteredHeuristic(
        RoutingModel* model,
        std::function<int64_t(int64_t, int64_t, int64_t)> evaluator,
        std::function<int64_t(int64_t)> penalty_evaluator,
        LocalSearchFilterManager* filter_manager,
        GlobalCheapestInsertionParameters parameters)
    : CheapestInsertionFilteredHeuristic(model, std::move(evaluator),
                                         std::move(penalty_evaluator),
                                         filter_manager),
      gci_params_(parameters),
      node_index_to_vehicle_(model->Size(), -1),
      empty_vehicle_type_curator_(nullptr) {
  CHECK_GT(gci_params_.neighbors_ratio, 0);
  CHECK_LE(gci_params_.neighbors_ratio, 1);
  CHECK_GE(gci_params_.min_neighbors, 1);

  if (NumNeighbors() >= NumNonStartEndNodes() - 1) {
    // All nodes are neighbors, so we set the neighbors_ratio to 1 to avoid
    // unnecessary computations in the code.
    gci_params_.neighbors_ratio = 1;
  }

  if (gci_params_.neighbors_ratio == 1) {
    gci_params_.use_neighbors_ratio_for_initialization = false;
    all_nodes_.resize(model->Size());
    std::iota(all_nodes_.begin(), all_nodes_.end(), 0);
  }
}

void GlobalCheapestInsertionFilteredHeuristic::ComputeNeighborhoods() {
  if (gci_params_.neighbors_ratio == 1 ||
      !node_index_to_neighbors_by_cost_class_.empty()) {
    // Neighborhood computations not needed or already done.
    return;
  }

  // TODO(user): Refactor the neighborhood computations in RoutingModel.
  const int64_t num_neighbors = NumNeighbors();
  // If num_neighbors was greater or equal num_non_start_end_nodes - 1,
  // gci_params_.neighbors_ratio should have been set to 1.
  DCHECK_LT(num_neighbors, NumNonStartEndNodes() - 1);

  const RoutingModel& routing_model = *model();
  const int64_t size = routing_model.Size();
  node_index_to_neighbors_by_cost_class_.resize(size);
  const int num_cost_classes = routing_model.GetCostClassesCount();
  for (int64_t node_index = 0; node_index < size; node_index++) {
    node_index_to_neighbors_by_cost_class_[node_index].resize(num_cost_classes);
    for (int cc = 0; cc < num_cost_classes; cc++) {
      node_index_to_neighbors_by_cost_class_[node_index][cc] =
          absl::make_unique<SparseBitset<int64_t>>(size);
    }
  }

  for (int64_t node_index = 0; node_index < size; ++node_index) {
    DCHECK(!routing_model.IsEnd(node_index));
    if (routing_model.IsStart(node_index)) {
      // We don't compute neighbors for vehicle starts: all nodes are considered
      // neighbors for a vehicle start.
      continue;
    }

    // TODO(user): Use the model's IndexNeighborFinder when available.
    for (int cost_class = 0; cost_class < num_cost_classes; cost_class++) {
      if (!routing_model.HasVehicleWithCostClassIndex(
              RoutingCostClassIndex(cost_class))) {
        // No vehicle with this cost class, avoid unnecessary computations.
        continue;
      }
      std::vector<std::pair</*cost*/ int64_t, /*node*/ int64_t>>
          costed_after_nodes;
      costed_after_nodes.reserve(size);
      for (int after_node = 0; after_node < size; ++after_node) {
        if (after_node != node_index && !routing_model.IsStart(after_node)) {
          costed_after_nodes.push_back(
              std::make_pair(routing_model.GetArcCostForClass(
                                 node_index, after_node, cost_class),
                             after_node));
        }
      }
      std::nth_element(costed_after_nodes.begin(),
                       costed_after_nodes.begin() + num_neighbors - 1,
                       costed_after_nodes.end());
      costed_after_nodes.resize(num_neighbors);

      for (auto [cost, neighbor] : costed_after_nodes) {
        node_index_to_neighbors_by_cost_class_[node_index][cost_class]->Set(
            neighbor);

        // Add reverse neighborhood.
        DCHECK(!routing_model.IsEnd(neighbor) &&
               !routing_model.IsStart(neighbor));
        node_index_to_neighbors_by_cost_class_[neighbor][cost_class]->Set(
            node_index);
      }
      // Add all vehicle starts as neighbors to this node and vice-versa.
      for (int vehicle = 0; vehicle < routing_model.vehicles(); vehicle++) {
        const int64_t vehicle_start = routing_model.Start(vehicle);
        node_index_to_neighbors_by_cost_class_[node_index][cost_class]->Set(
            vehicle_start);
        node_index_to_neighbors_by_cost_class_[vehicle_start][cost_class]->Set(
            node_index);
      }
    }
  }
}

bool GlobalCheapestInsertionFilteredHeuristic::IsNeighborForCostClass(
    int cost_class, int64_t node_index, int64_t neighbor_index) const {
  return gci_params_.neighbors_ratio == 1 ||
         (*node_index_to_neighbors_by_cost_class_[node_index]
                                                 [cost_class])[neighbor_index];
}

bool GlobalCheapestInsertionFilteredHeuristic::CheckVehicleIndices() const {
  std::vector<bool> node_is_visited(model()->Size(), -1);
  for (int v = 0; v < model()->vehicles(); v++) {
    for (int node = model()->Start(v); !model()->IsEnd(node);
         node = Value(node)) {
      if (node_index_to_vehicle_[node] != v) {
        return false;
      }
      node_is_visited[node] = true;
    }
  }

  for (int node = 0; node < model()->Size(); node++) {
    if (!node_is_visited[node] && node_index_to_vehicle_[node] != -1) {
      return false;
    }
  }

  return true;
}

bool GlobalCheapestInsertionFilteredHeuristic::BuildSolutionInternal() {
  ComputeNeighborhoods();
  if (empty_vehicle_type_curator_ == nullptr) {
    empty_vehicle_type_curator_ = absl::make_unique<VehicleTypeCurator>(
        model()->GetVehicleTypeContainer());
  }
  // Store all empty vehicles in the empty_vehicle_type_curator_.
  empty_vehicle_type_curator_->Reset(
      [this](int vehicle) { return VehicleIsEmpty(vehicle); });
  // Insert partially inserted pairs.
  const RoutingModel::IndexPairs& pickup_delivery_pairs =
      model()->GetPickupAndDeliveryPairs();
  std::map<int64_t, std::vector<int>> pairs_to_insert_by_bucket;
  absl::flat_hash_map<int, std::map<int64_t, std::vector<int>>>
      vehicle_to_pair_nodes;
  for (int index = 0; index < pickup_delivery_pairs.size(); index++) {
    const RoutingModel::IndexPair& index_pair = pickup_delivery_pairs[index];
    int pickup_vehicle = -1;
    for (int64_t pickup : index_pair.first) {
      if (Contains(pickup)) {
        pickup_vehicle = node_index_to_vehicle_[pickup];
        break;
      }
    }
    int delivery_vehicle = -1;
    for (int64_t delivery : index_pair.second) {
      if (Contains(delivery)) {
        delivery_vehicle = node_index_to_vehicle_[delivery];
        break;
      }
    }
    if (pickup_vehicle < 0 && delivery_vehicle < 0) {
      pairs_to_insert_by_bucket[GetBucketOfPair(index_pair)].push_back(index);
    }
    if (pickup_vehicle >= 0 && delivery_vehicle < 0) {
      std::vector<int>& pair_nodes = vehicle_to_pair_nodes[pickup_vehicle][1];
      for (int64_t delivery : index_pair.second) {
        pair_nodes.push_back(delivery);
      }
    }
    if (pickup_vehicle < 0 && delivery_vehicle >= 0) {
      std::vector<int>& pair_nodes = vehicle_to_pair_nodes[delivery_vehicle][1];
      for (int64_t pickup : index_pair.first) {
        pair_nodes.push_back(pickup);
      }
    }
  }
  for (const auto& [vehicle, nodes] : vehicle_to_pair_nodes) {
    if (!InsertNodesOnRoutes(nodes, {vehicle})) return false;
  }

  if (!InsertPairsAndNodesByRequirementTopologicalOrder()) return false;

  // TODO(user): Adapt the pair insertions to also support seed and
  // sequential insertion.
  if (!InsertPairs(pairs_to_insert_by_bucket)) return false;
  std::map<int64_t, std::vector<int>> nodes_by_bucket;
  for (int node = 0; node < model()->Size(); ++node) {
    if (!Contains(node) && model()->GetPickupIndexPairs(node).empty() &&
        model()->GetDeliveryIndexPairs(node).empty()) {
      nodes_by_bucket[GetBucketOfNode(node)].push_back(node);
    }
  }
  InsertFarthestNodesAsSeeds();
  if (gci_params_.is_sequential) {
    if (!SequentialInsertNodes(nodes_by_bucket)) return false;
  } else if (!InsertNodesOnRoutes(nodes_by_bucket, {})) {
    return false;
  }
  MakeUnassignedNodesUnperformed();
  DCHECK(CheckVehicleIndices());
  return Commit();
}

bool GlobalCheapestInsertionFilteredHeuristic::
    InsertPairsAndNodesByRequirementTopologicalOrder() {
  const RoutingModel::IndexPairs& pickup_delivery_pairs =
      model()->GetPickupAndDeliveryPairs();
  for (const std::vector<int>& types :
       model()->GetTopologicallySortedVisitTypes()) {
    for (int type : types) {
      std::map<int64_t, std::vector<int>> pairs_to_insert_by_bucket;
      for (int index : model()->GetPairIndicesOfType(type)) {
        pairs_to_insert_by_bucket[GetBucketOfPair(pickup_delivery_pairs[index])]
            .push_back(index);
      }
      if (!InsertPairs(pairs_to_insert_by_bucket)) return false;
      std::map<int64_t, std::vector<int>> nodes_by_bucket;
      for (int node : model()->GetSingleNodesOfType(type)) {
        nodes_by_bucket[GetBucketOfNode(node)].push_back(node);
      }
      if (!InsertNodesOnRoutes(nodes_by_bucket, {})) return false;
    }
  }
  return true;
}

bool GlobalCheapestInsertionFilteredHeuristic::InsertPairs(
    const std::map<int64_t, std::vector<int>>& pair_indices_by_bucket) {
  AdjustablePriorityQueue<PairEntry> priority_queue;
  std::vector<PairEntries> pickup_to_entries;
  std::vector<PairEntries> delivery_to_entries;
  std::vector<int> pair_indices_to_insert;
  for (const auto& [bucket, pair_indices] : pair_indices_by_bucket) {
    pair_indices_to_insert.insert(pair_indices_to_insert.end(),
                                  pair_indices.begin(), pair_indices.end());
    if (!InitializePairPositions(pair_indices_to_insert, &priority_queue,
                                 &pickup_to_entries, &delivery_to_entries)) {
      return false;
    }
    while (!priority_queue.IsEmpty()) {
      if (StopSearchAndCleanup(&priority_queue)) {
        return false;
      }
      PairEntry* const entry = priority_queue.Top();
      const int64_t pickup = entry->pickup_to_insert();
      const int64_t delivery = entry->delivery_to_insert();
      if (Contains(pickup) || Contains(delivery)) {
        DeletePairEntry(entry, &priority_queue, &pickup_to_entries,
                        &delivery_to_entries);
        continue;
      }

      const int entry_vehicle = entry->vehicle();
      if (entry_vehicle == -1) {
        // Pair is unperformed.
        SetValue(pickup, pickup);
        SetValue(delivery, delivery);
        if (!Commit()) {
          DeletePairEntry(entry, &priority_queue, &pickup_to_entries,
                          &delivery_to_entries);
        }
        continue;
      }

      // Pair is performed.
      if (UseEmptyVehicleTypeCuratorForVehicle(entry_vehicle)) {
        if (!InsertPairEntryUsingEmptyVehicleTypeCurator(
                pair_indices_to_insert, entry, &priority_queue,
                &pickup_to_entries, &delivery_to_entries)) {
          return false;
        }
        // The entry corresponded to an insertion on an empty vehicle, which was
        // handled by the call to InsertPairEntryUsingEmptyVehicleTypeCurator().
        continue;
      }

      const int64_t pickup_insert_after = entry->pickup_insert_after();
      const int64_t pickup_insert_before = Value(pickup_insert_after);
      InsertBetween(pickup, pickup_insert_after, pickup_insert_before);

      const int64_t delivery_insert_after = entry->delivery_insert_after();
      const int64_t delivery_insert_before = (delivery_insert_after == pickup)
                                                 ? pickup_insert_before
                                                 : Value(delivery_insert_after);
      InsertBetween(delivery, delivery_insert_after, delivery_insert_before);
      if (Commit()) {
        if (!UpdateAfterPairInsertion(
                pair_indices_to_insert, entry_vehicle, pickup,
                pickup_insert_after, delivery, delivery_insert_after,
                &priority_queue, &pickup_to_entries, &delivery_to_entries)) {
          return false;
        }
      } else {
        DeletePairEntry(entry, &priority_queue, &pickup_to_entries,
                        &delivery_to_entries);
      }
    }
    // In case all pairs could not be inserted, pushing uninserted ones to the
    // next bucket.
    const RoutingModel::IndexPairs& pickup_delivery_pairs =
        model()->GetPickupAndDeliveryPairs();
    pair_indices_to_insert.erase(
        std::remove_if(
            pair_indices_to_insert.begin(), pair_indices_to_insert.end(),
            [this, &pickup_delivery_pairs](int pair_index) {
              // Checking a pickup has been inserted is enough to make sure the
              // full pair has been inserted since a pickup cannot be inserted
              // without its delivery (and vice versa).
              for (int64_t pickup : pickup_delivery_pairs[pair_index].first) {
                if (Contains(pickup)) return true;
              }
              return false;
            }),
        pair_indices_to_insert.end());
  }
  return true;
}

bool GlobalCheapestInsertionFilteredHeuristic::
    InsertPairEntryUsingEmptyVehicleTypeCurator(
        const std::vector<int>& pair_indices,
        GlobalCheapestInsertionFilteredHeuristic::PairEntry* const pair_entry,
        AdjustablePriorityQueue<
            GlobalCheapestInsertionFilteredHeuristic::PairEntry>*
            priority_queue,
        std::vector<GlobalCheapestInsertionFilteredHeuristic::PairEntries>*
            pickup_to_entries,
        std::vector<GlobalCheapestInsertionFilteredHeuristic::PairEntries>*
            delivery_to_entries) {
  const int entry_vehicle = pair_entry->vehicle();
  DCHECK(UseEmptyVehicleTypeCuratorForVehicle(entry_vehicle));

  // Trying to insert on an empty vehicle.
  // As we only have one pair_entry per empty vehicle type, we try inserting on
  // all vehicles of this type with the same fixed cost, as they all have the
  // same insertion value.
  const int64_t pickup = pair_entry->pickup_to_insert();
  const int64_t delivery = pair_entry->delivery_to_insert();
  const int64_t entry_fixed_cost =
      model()->GetFixedCostOfVehicle(entry_vehicle);
  auto vehicle_is_compatible = [this, entry_fixed_cost, pickup,
                                delivery](int vehicle) {
    if (model()->GetFixedCostOfVehicle(vehicle) != entry_fixed_cost) {
      return false;
    }
    // NOTE: Only empty vehicles should be in the vehicle_curator_.
    DCHECK(VehicleIsEmpty(vehicle));
    const int64_t end = model()->End(vehicle);
    InsertBetween(pickup, model()->Start(vehicle), end);
    InsertBetween(delivery, pickup, end);
    return Commit();
  };
  // Since the vehicles of the same type are sorted by increasing fixed
  // cost by the curator, we can stop as soon as a vehicle with a fixed cost
  // higher than the entry_fixed_cost is found which is empty, and adapt the
  // pair entry with this new vehicle.
  auto stop_and_return_vehicle = [this, entry_fixed_cost](int vehicle) {
    return model()->GetFixedCostOfVehicle(vehicle) > entry_fixed_cost;
  };
  const auto [compatible_vehicle, next_fixed_cost_empty_vehicle] =
      empty_vehicle_type_curator_->GetCompatibleVehicleOfType(
          empty_vehicle_type_curator_->Type(entry_vehicle),
          vehicle_is_compatible, stop_and_return_vehicle);
  if (compatible_vehicle >= 0) {
    // The pair was inserted on this vehicle.
    const int64_t vehicle_start = model()->Start(compatible_vehicle);
    const int num_previous_vehicle_entries =
        pickup_to_entries->at(vehicle_start).size() +
        delivery_to_entries->at(vehicle_start).size();
    if (!UpdateAfterPairInsertion(
            pair_indices, compatible_vehicle, pickup, vehicle_start, delivery,
            pickup, priority_queue, pickup_to_entries, delivery_to_entries)) {
      return false;
    }
    if (compatible_vehicle != entry_vehicle) {
      // The pair was inserted on another empty vehicle of the same type
      // and same fixed cost as entry_vehicle.
      // Since this vehicle is empty and has the same fixed cost as the
      // entry_vehicle, it shouldn't be the representative of empty vehicles
      // for any pickup/delivery in the priority queue.
      DCHECK_EQ(num_previous_vehicle_entries, 0);
      return true;
    }
    // The previously unused entry_vehicle is now used, so we use the next
    // available vehicle of the same type to compute and store insertions on
    // empty vehicles.
    const int new_empty_vehicle =
        empty_vehicle_type_curator_->GetLowestFixedCostVehicleOfType(
            empty_vehicle_type_curator_->Type(compatible_vehicle));

    if (new_empty_vehicle >= 0) {
      DCHECK(VehicleIsEmpty(new_empty_vehicle));
      // Add node entries after this vehicle start for uninserted pairs which
      // don't have entries on this empty vehicle.
      UpdatePairPositions(pair_indices, new_empty_vehicle,
                          model()->Start(new_empty_vehicle), priority_queue,
                          pickup_to_entries, delivery_to_entries);
    }
  } else if (next_fixed_cost_empty_vehicle >= 0) {
    // Could not insert on this vehicle or any other vehicle of the same type
    // with the same fixed cost, but found an empty vehicle of this type with
    // higher fixed cost.
    DCHECK(VehicleIsEmpty(next_fixed_cost_empty_vehicle));
    // Update the pair entry to correspond to an insertion on this
    // next_fixed_cost_empty_vehicle instead of the previous entry_vehicle.
    pair_entry->set_vehicle(next_fixed_cost_empty_vehicle);
    pickup_to_entries->at(pair_entry->pickup_insert_after()).erase(pair_entry);
    pair_entry->set_pickup_insert_after(
        model()->Start(next_fixed_cost_empty_vehicle));
    pickup_to_entries->at(pair_entry->pickup_insert_after()).insert(pair_entry);
    DCHECK_EQ(pair_entry->delivery_insert_after(), pickup);
    UpdatePairEntry(pair_entry, priority_queue);
  } else {
    DeletePairEntry(pair_entry, priority_queue, pickup_to_entries,
                    delivery_to_entries);
  }

  return true;
}

bool GlobalCheapestInsertionFilteredHeuristic::InsertNodesOnRoutes(
    const std::map<int64_t, std::vector<int>>& nodes_by_bucket,
    const absl::flat_hash_set<int>& vehicles) {
  AdjustablePriorityQueue<NodeEntry> priority_queue;
  std::vector<NodeEntries> position_to_node_entries;
  std::vector<int> nodes_to_insert;
  for (const auto& [bucket, nodes] : nodes_by_bucket) {
    nodes_to_insert.insert(nodes_to_insert.end(), nodes.begin(), nodes.end());
    if (!InitializePositions(nodes_to_insert, vehicles, &priority_queue,
                             &position_to_node_entries)) {
      return false;
    }
    // The following boolean indicates whether or not all vehicles are being
    // considered for insertion of the nodes simultaneously.
    // In the sequential version of the heuristic, as well as when inserting
    // single pickup or deliveries from pickup/delivery pairs, this will be
    // false. In the general parallel version of the heuristic, all_vehicles is
    // true.
    const bool all_vehicles =
        vehicles.empty() || vehicles.size() == model()->vehicles();

    while (!priority_queue.IsEmpty()) {
      NodeEntry* const node_entry = priority_queue.Top();
      if (StopSearchAndCleanup(&priority_queue)) {
        return false;
      }
      const int64_t node_to_insert = node_entry->node_to_insert();
      if (Contains(node_to_insert)) {
        DeleteNodeEntry(node_entry, &priority_queue, &position_to_node_entries);
        continue;
      }

      const int entry_vehicle = node_entry->vehicle();
      if (entry_vehicle == -1) {
        DCHECK(all_vehicles);
        // Make node unperformed.
        SetValue(node_to_insert, node_to_insert);
        if (!Commit()) {
          DeleteNodeEntry(node_entry, &priority_queue,
                          &position_to_node_entries);
        }
        continue;
      }

      // Make node performed.
      if (UseEmptyVehicleTypeCuratorForVehicle(entry_vehicle, all_vehicles)) {
        DCHECK(all_vehicles);
        if (!InsertNodeEntryUsingEmptyVehicleTypeCurator(
                nodes_to_insert, all_vehicles, node_entry, &priority_queue,
                &position_to_node_entries)) {
          return false;
        }
        continue;
      }

      const int64_t insert_after = node_entry->insert_after();
      InsertBetween(node_to_insert, insert_after, Value(insert_after));
      if (Commit()) {
        if (!UpdatePositions(nodes_to_insert, entry_vehicle, node_to_insert,
                             all_vehicles, &priority_queue,
                             &position_to_node_entries) ||
            !UpdatePositions(nodes_to_insert, entry_vehicle, insert_after,
                             all_vehicles, &priority_queue,
                             &position_to_node_entries)) {
          return false;
        }
        SetVehicleIndex(node_to_insert, entry_vehicle);
      } else {
        DeleteNodeEntry(node_entry, &priority_queue, &position_to_node_entries);
      }
    }
    // In case all nodes could not be inserted, pushing uninserted ones to the
    // next bucket.
    nodes_to_insert.erase(
        std::remove_if(nodes_to_insert.begin(), nodes_to_insert.end(),
                       [this](int node) { return Contains(node); }),
        nodes_to_insert.end());
  }
  return true;
}

bool GlobalCheapestInsertionFilteredHeuristic::
    InsertNodeEntryUsingEmptyVehicleTypeCurator(
        const std::vector<int>& nodes, bool all_vehicles,
        GlobalCheapestInsertionFilteredHeuristic::NodeEntry* const node_entry,
        AdjustablePriorityQueue<
            GlobalCheapestInsertionFilteredHeuristic::NodeEntry>*
            priority_queue,
        std::vector<GlobalCheapestInsertionFilteredHeuristic::NodeEntries>*
            position_to_node_entries) {
  const int entry_vehicle = node_entry->vehicle();
  DCHECK(UseEmptyVehicleTypeCuratorForVehicle(entry_vehicle, all_vehicles));

  // Trying to insert on an empty vehicle, and all vehicles are being
  // considered simultaneously.
  // As we only have one node_entry per type, we try inserting on all vehicles
  // of this type with the same fixed cost as they all have the same insertion
  // value.
  const int64_t node_to_insert = node_entry->node_to_insert();
  const int64_t entry_fixed_cost =
      model()->GetFixedCostOfVehicle(entry_vehicle);
  auto vehicle_is_compatible = [this, entry_fixed_cost,
                                node_to_insert](int vehicle) {
    if (model()->GetFixedCostOfVehicle(vehicle) != entry_fixed_cost) {
      return false;
    }
    // NOTE: Only empty vehicles should be in the vehicle_curator_.
    DCHECK(VehicleIsEmpty(vehicle));
    InsertBetween(node_to_insert, model()->Start(vehicle),
                  model()->End(vehicle));
    return Commit();
  };
  // Since the vehicles of the same type are sorted by increasing fixed
  // cost by the curator, we can stop as soon as an empty vehicle with a fixed
  // cost higher than the entry_fixed_cost is found, and add new entries for
  // this new vehicle.
  auto stop_and_return_vehicle = [this, entry_fixed_cost](int vehicle) {
    return model()->GetFixedCostOfVehicle(vehicle) > entry_fixed_cost;
  };
  const auto [compatible_vehicle, next_fixed_cost_empty_vehicle] =
      empty_vehicle_type_curator_->GetCompatibleVehicleOfType(
          empty_vehicle_type_curator_->Type(entry_vehicle),
          vehicle_is_compatible, stop_and_return_vehicle);
  if (compatible_vehicle >= 0) {
    // The node was inserted on this vehicle.
    if (!UpdatePositions(nodes, compatible_vehicle, node_to_insert,
                         all_vehicles, priority_queue,
                         position_to_node_entries)) {
      return false;
    }
    const int64_t compatible_start = model()->Start(compatible_vehicle);
    const bool no_prior_entries_for_this_vehicle =
        position_to_node_entries->at(compatible_start).empty();
    if (!UpdatePositions(nodes, compatible_vehicle, compatible_start,
                         all_vehicles, priority_queue,
                         position_to_node_entries)) {
      return false;
    }
    SetVehicleIndex(node_to_insert, compatible_vehicle);
    if (compatible_vehicle != entry_vehicle) {
      // The node was inserted on another empty vehicle of the same type
      // and same fixed cost as entry_vehicle.
      // Since this vehicle is empty and has the same fixed cost as the
      // entry_vehicle, it shouldn't be the representative of empty vehicles
      // for any node in the priority queue.
      DCHECK(no_prior_entries_for_this_vehicle);
      return true;
    }
    // The previously unused entry_vehicle is now used, so we use the next
    // available vehicle of the same type to compute and store insertions on
    // empty vehicles.
    const int new_empty_vehicle =
        empty_vehicle_type_curator_->GetLowestFixedCostVehicleOfType(
            empty_vehicle_type_curator_->Type(compatible_vehicle));

    if (new_empty_vehicle >= 0) {
      DCHECK(VehicleIsEmpty(new_empty_vehicle));
      // Add node entries after this vehicle start for uninserted nodes which
      // don't have entries on this empty vehicle.
      if (!UpdatePositions(nodes, new_empty_vehicle,
                           model()->Start(new_empty_vehicle), all_vehicles,
                           priority_queue, position_to_node_entries)) {
        return false;
      }
    }
  } else if (next_fixed_cost_empty_vehicle >= 0) {
    // Could not insert on this vehicle or any other vehicle of the same
    // type with the same fixed cost, but found an empty vehicle of this type
    // with higher fixed cost.
    DCHECK(VehicleIsEmpty(next_fixed_cost_empty_vehicle));
    // Update the insertion entry to be on next_empty_vehicle instead of the
    // previous entry_vehicle.
    position_to_node_entries->at(node_entry->insert_after()).erase(node_entry);
    node_entry->set_insert_after(model()->Start(next_fixed_cost_empty_vehicle));
    position_to_node_entries->at(node_entry->insert_after()).insert(node_entry);
    node_entry->set_vehicle(next_fixed_cost_empty_vehicle);
    UpdateNodeEntry(node_entry, priority_queue);
  } else {
    DeleteNodeEntry(node_entry, priority_queue, position_to_node_entries);
  }

  return true;
}

bool GlobalCheapestInsertionFilteredHeuristic::SequentialInsertNodes(
    const std::map<int64_t, std::vector<int>>& nodes_by_bucket) {
  std::vector<bool> is_vehicle_used;
  absl::flat_hash_set<int> used_vehicles;
  std::vector<int> unused_vehicles;

  DetectUsedVehicles(&is_vehicle_used, &unused_vehicles, &used_vehicles);
  if (!used_vehicles.empty() &&
      !InsertNodesOnRoutes(nodes_by_bucket, used_vehicles)) {
    return false;
  }

  std::vector<std::vector<StartEndValue>> start_end_distances_per_node =
      ComputeStartEndDistanceForVehicles(unused_vehicles);
  std::priority_queue<Seed, std::vector<Seed>, std::greater<Seed>>
      first_node_queue;
  InitializePriorityQueue(&start_end_distances_per_node, &first_node_queue);

  int vehicle = InsertSeedNode(&start_end_distances_per_node, &first_node_queue,
                               &is_vehicle_used);

  while (vehicle >= 0) {
    if (!InsertNodesOnRoutes(nodes_by_bucket, {vehicle})) {
      return false;
    }
    vehicle = InsertSeedNode(&start_end_distances_per_node, &first_node_queue,
                             &is_vehicle_used);
  }
  return true;
}

void GlobalCheapestInsertionFilteredHeuristic::DetectUsedVehicles(
    std::vector<bool>* is_vehicle_used, std::vector<int>* unused_vehicles,
    absl::flat_hash_set<int>* used_vehicles) {
  is_vehicle_used->clear();
  is_vehicle_used->resize(model()->vehicles());

  used_vehicles->clear();
  used_vehicles->reserve(model()->vehicles());

  unused_vehicles->clear();
  unused_vehicles->reserve(model()->vehicles());

  for (int vehicle = 0; vehicle < model()->vehicles(); vehicle++) {
    if (!VehicleIsEmpty(vehicle)) {
      (*is_vehicle_used)[vehicle] = true;
      used_vehicles->insert(vehicle);
    } else {
      (*is_vehicle_used)[vehicle] = false;
      unused_vehicles->push_back(vehicle);
    }
  }
}

void GlobalCheapestInsertionFilteredHeuristic::InsertFarthestNodesAsSeeds() {
  if (gci_params_.farthest_seeds_ratio <= 0) return;
  // Insert at least 1 farthest Seed if the parameter is positive.
  const int num_seeds = static_cast<int>(
      std::ceil(gci_params_.farthest_seeds_ratio * model()->vehicles()));

  std::vector<bool> is_vehicle_used;
  absl::flat_hash_set<int> used_vehicles;
  std::vector<int> unused_vehicles;
  DetectUsedVehicles(&is_vehicle_used, &unused_vehicles, &used_vehicles);
  std::vector<std::vector<StartEndValue>> start_end_distances_per_node =
      ComputeStartEndDistanceForVehicles(unused_vehicles);

  // Priority queue where the Seeds with a larger distance are given higher
  // priority.
  std::priority_queue<Seed> farthest_node_queue;
  InitializePriorityQueue(&start_end_distances_per_node, &farthest_node_queue);

  int inserted_seeds = 0;
  while (inserted_seeds++ < num_seeds) {
    if (InsertSeedNode(&start_end_distances_per_node, &farthest_node_queue,
                       &is_vehicle_used) < 0) {
      break;
    }
  }

  // NOTE: As we don't use the empty_vehicle_type_curator_ when inserting seed
  // nodes on routes, some previously empty vehicles may now be used, so we
  // update the curator accordingly to ensure it still only stores empty
  // vehicles.
  DCHECK(empty_vehicle_type_curator_ != nullptr);
  empty_vehicle_type_curator_->Update(
      [this](int vehicle) { return !VehicleIsEmpty(vehicle); });
}

template <class Queue>
int GlobalCheapestInsertionFilteredHeuristic::InsertSeedNode(
    std::vector<std::vector<StartEndValue>>* start_end_distances_per_node,
    Queue* priority_queue, std::vector<bool>* is_vehicle_used) {
  while (!priority_queue->empty()) {
    if (StopSearch()) return -1;
    const Seed& seed = priority_queue->top();

    const int seed_node = seed.second;
    const int seed_vehicle = seed.first.vehicle;

    std::vector<StartEndValue>& other_start_end_values =
        (*start_end_distances_per_node)[seed_node];

    if (Contains(seed_node)) {
      // The node is already inserted, it is therefore no longer considered as
      // a potential seed.
      priority_queue->pop();
      other_start_end_values.clear();
      continue;
    }
    if (!(*is_vehicle_used)[seed_vehicle]) {
      // Try to insert this seed_node on this vehicle's route.
      const int64_t start = model()->Start(seed_vehicle);
      const int64_t end = model()->End(seed_vehicle);
      DCHECK_EQ(Value(start), end);
      InsertBetween(seed_node, start, end);
      if (Commit()) {
        priority_queue->pop();
        (*is_vehicle_used)[seed_vehicle] = true;
        other_start_end_values.clear();
        SetVehicleIndex(seed_node, seed_vehicle);
        return seed_vehicle;
      }
    }
    // Either the vehicle is already used, or the Commit() wasn't successful.
    // In both cases, we remove this Seed from the priority queue, and insert
    // the next StartEndValue from start_end_distances_per_node[seed_node]
    // in the priority queue.
    priority_queue->pop();
    if (!other_start_end_values.empty()) {
      const StartEndValue& next_seed_value = other_start_end_values.back();
      priority_queue->push(std::make_pair(next_seed_value, seed_node));
      other_start_end_values.pop_back();
    }
  }
  // No seed node was inserted.
  return -1;
}

bool GlobalCheapestInsertionFilteredHeuristic::InitializePairPositions(
    const std::vector<int>& pair_indices,
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredHeuristic::PairEntry>* priority_queue,
    std::vector<GlobalCheapestInsertionFilteredHeuristic::PairEntries>*
        pickup_to_entries,
    std::vector<GlobalCheapestInsertionFilteredHeuristic::PairEntries>*
        delivery_to_entries) {
  priority_queue->Clear();
  pickup_to_entries->clear();
  pickup_to_entries->resize(model()->Size());
  delivery_to_entries->clear();
  delivery_to_entries->resize(model()->Size());
  const RoutingModel::IndexPairs& pickup_delivery_pairs =
      model()->GetPickupAndDeliveryPairs();
  for (int index : pair_indices) {
    const RoutingModel::IndexPair& index_pair = pickup_delivery_pairs[index];
    for (int64_t pickup : index_pair.first) {
      if (Contains(pickup)) continue;
      for (int64_t delivery : index_pair.second) {
        if (Contains(delivery)) continue;
        if (StopSearchAndCleanup(priority_queue)) return false;
        // Add insertion entry making pair unperformed. When the pair is part
        // of a disjunction we do not try to make any of its pairs unperformed
        // as it requires having an entry with all pairs being unperformed.
        // TODO(user): Adapt the code to make pair disjunctions unperformed.
        if (gci_params_.add_unperformed_entries &&
            index_pair.first.size() == 1 && index_pair.second.size() == 1 &&
            GetUnperformedValue(pickup) !=
                std::numeric_limits<int64_t>::max() &&
            GetUnperformedValue(delivery) !=
                std::numeric_limits<int64_t>::max()) {
          AddPairEntry(pickup, -1, delivery, -1, -1, priority_queue, nullptr,
                       nullptr);
        }
        // Add all other insertion entries with pair performed.
        InitializeInsertionEntriesPerformingPair(
            pickup, delivery, priority_queue, pickup_to_entries,
            delivery_to_entries);
      }
    }
  }
  return true;
}

void GlobalCheapestInsertionFilteredHeuristic::
    InitializeInsertionEntriesPerformingPair(
        int64_t pickup, int64_t delivery,
        AdjustablePriorityQueue<
            GlobalCheapestInsertionFilteredHeuristic::PairEntry>*
            priority_queue,
        std::vector<GlobalCheapestInsertionFilteredHeuristic::PairEntries>*
            pickup_to_entries,
        std::vector<GlobalCheapestInsertionFilteredHeuristic::PairEntries>*
            delivery_to_entries) {
  if (!gci_params_.use_neighbors_ratio_for_initialization) {
    struct PairInsertion {
      int64_t insert_pickup_after;
      int64_t insert_delivery_after;
      int vehicle;
    };
    std::vector<PairInsertion> pair_insertions;
    for (int vehicle = 0; vehicle < model()->vehicles(); ++vehicle) {
      if (VehicleIsEmpty(vehicle) &&
          empty_vehicle_type_curator_->GetLowestFixedCostVehicleOfType(
              empty_vehicle_type_curator_->Type(vehicle)) != vehicle) {
        // We only consider the least expensive empty vehicle of each type for
        // entries.
        continue;
      }
      const int64_t start = model()->Start(vehicle);
      std::vector<NodeInsertion> pickup_insertions;
      AppendInsertionPositionsAfter(pickup, start, Value(start), vehicle,
                                    &pickup_insertions);
      for (const auto [insert_pickup_after, pickup_vehicle,
                       unused_pickup_value] : pickup_insertions) {
        CHECK(!model()->IsEnd(insert_pickup_after));
        std::vector<NodeInsertion> delivery_insertions;
        AppendInsertionPositionsAfter(delivery, pickup,
                                      Value(insert_pickup_after), vehicle,
                                      &delivery_insertions);
        for (const auto [insert_delivery_after, delivery_vehicle,
                         unused_delivery_value] : delivery_insertions) {
          pair_insertions.push_back(
              {insert_pickup_after, insert_delivery_after, vehicle});
        }
      }
    }
    for (const auto& [insert_pickup_after, insert_delivery_after, vehicle] :
         pair_insertions) {
      DCHECK_NE(insert_pickup_after, insert_delivery_after);
      AddPairEntry(pickup, insert_pickup_after, delivery, insert_delivery_after,
                   vehicle, priority_queue, pickup_to_entries,
                   delivery_to_entries);
    }
    return;
  }

  // We're only considering the closest neighbors as insertion positions for
  // the pickup/delivery pair.
  for (int cost_class = 0; cost_class < model()->GetCostClassesCount();
       cost_class++) {
    absl::flat_hash_set<std::pair<int64_t, int64_t>>
        existing_insertion_positions;
    // Explore the neighborhood of the pickup.
    for (const int64_t pickup_insert_after :
         GetNeighborsOfNodeForCostClass(cost_class, pickup)) {
      if (!Contains(pickup_insert_after)) {
        continue;
      }
      const int vehicle = node_index_to_vehicle_[pickup_insert_after];
      if (vehicle < 0 ||
          model()->GetCostClassIndexOfVehicle(vehicle).value() != cost_class) {
        continue;
      }

      if (VehicleIsEmpty(vehicle) &&
          empty_vehicle_type_curator_->GetLowestFixedCostVehicleOfType(
              empty_vehicle_type_curator_->Type(vehicle)) != vehicle) {
        // We only consider the least expensive empty vehicle of each type for
        // entries.
        continue;
      }

      int64_t delivery_insert_after = pickup;
      while (!model()->IsEnd(delivery_insert_after)) {
        const std::pair<int64_t, int64_t> insertion_position = {
            pickup_insert_after, delivery_insert_after};
        DCHECK(!existing_insertion_positions.contains(insertion_position));
        existing_insertion_positions.insert(insertion_position);

        AddPairEntry(pickup, pickup_insert_after, delivery,
                     delivery_insert_after, vehicle, priority_queue,
                     pickup_to_entries, delivery_to_entries);
        delivery_insert_after = (delivery_insert_after == pickup)
                                    ? Value(pickup_insert_after)
                                    : Value(delivery_insert_after);
      }
    }

    // Explore the neighborhood of the delivery.
    for (const int64_t delivery_insert_after :
         GetNeighborsOfNodeForCostClass(cost_class, delivery)) {
      if (!Contains(delivery_insert_after)) {
        continue;
      }
      const int vehicle = node_index_to_vehicle_[delivery_insert_after];
      if (vehicle < 0 ||
          model()->GetCostClassIndexOfVehicle(vehicle).value() != cost_class) {
        continue;
      }

      if (VehicleIsEmpty(vehicle)) {
        // Vehicle is empty.
        DCHECK_EQ(delivery_insert_after, model()->Start(vehicle));
      }

      int64_t pickup_insert_after = model()->Start(vehicle);
      while (pickup_insert_after != delivery_insert_after) {
        if (!existing_insertion_positions.contains(
                std::make_pair(pickup_insert_after, delivery_insert_after))) {
          AddPairEntry(pickup, pickup_insert_after, delivery,
                       delivery_insert_after, vehicle, priority_queue,
                       pickup_to_entries, delivery_to_entries);
        }
        pickup_insert_after = Value(pickup_insert_after);
      }
    }
  }
}

bool GlobalCheapestInsertionFilteredHeuristic::UpdateAfterPairInsertion(
    const std::vector<int>& pair_indices, int vehicle, int64_t pickup,
    int64_t pickup_position, int64_t delivery, int64_t delivery_position,
    AdjustablePriorityQueue<PairEntry>* priority_queue,
    std::vector<PairEntries>* pickup_to_entries,
    std::vector<PairEntries>* delivery_to_entries) {
  if (!UpdatePairPositions(pair_indices, vehicle, pickup_position,
                           priority_queue, pickup_to_entries,
                           delivery_to_entries) ||
      !UpdatePairPositions(pair_indices, vehicle, pickup, priority_queue,
                           pickup_to_entries, delivery_to_entries) ||
      !UpdatePairPositions(pair_indices, vehicle, delivery, priority_queue,
                           pickup_to_entries, delivery_to_entries)) {
    return false;
  }
  if (delivery_position != pickup &&
      !UpdatePairPositions(pair_indices, vehicle, delivery_position,
                           priority_queue, pickup_to_entries,
                           delivery_to_entries)) {
    return false;
  }
  SetVehicleIndex(pickup, vehicle);
  SetVehicleIndex(delivery, vehicle);
  return true;
}

bool GlobalCheapestInsertionFilteredHeuristic::UpdatePickupPositions(
    const std::vector<int>& pair_indices, int vehicle,
    int64_t pickup_insert_after,
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredHeuristic::PairEntry>* priority_queue,
    std::vector<GlobalCheapestInsertionFilteredHeuristic::PairEntries>*
        pickup_to_entries,
    std::vector<GlobalCheapestInsertionFilteredHeuristic::PairEntries>*
        delivery_to_entries) {
  // First, remove entries which have already been inserted and keep track of
  // the entries which are being kept and must be updated.
  using Pair = std::pair<int64_t, int64_t>;
  using Insertion = std::pair<Pair, /*delivery_insert_after*/ int64_t>;
  absl::flat_hash_set<Insertion> existing_insertions;
  std::vector<PairEntry*> to_remove;
  for (PairEntry* const pair_entry :
       pickup_to_entries->at(pickup_insert_after)) {
    DCHECK(priority_queue->Contains(pair_entry));
    DCHECK_EQ(pair_entry->pickup_insert_after(), pickup_insert_after);
    if (Contains(pair_entry->pickup_to_insert()) ||
        Contains(pair_entry->delivery_to_insert())) {
      to_remove.push_back(pair_entry);
    } else {
      DCHECK(delivery_to_entries->at(pair_entry->delivery_insert_after())
                 .contains(pair_entry));
      UpdatePairEntry(pair_entry, priority_queue);
      existing_insertions.insert(
          {{pair_entry->pickup_to_insert(), pair_entry->delivery_to_insert()},
           pair_entry->delivery_insert_after()});
    }
  }
  for (PairEntry* const pair_entry : to_remove) {
    DeletePairEntry(pair_entry, priority_queue, pickup_to_entries,
                    delivery_to_entries);
  }
  // Create new entries for which the pickup is to be inserted after
  // pickup_insert_after.
  const int cost_class = model()->GetCostClassIndexOfVehicle(vehicle).value();
  const int64_t pickup_insert_before = Value(pickup_insert_after);
  const RoutingModel::IndexPairs& pickup_delivery_pairs =
      model()->GetPickupAndDeliveryPairs();
  for (int pair_index : pair_indices) {
    if (StopSearchAndCleanup(priority_queue)) return false;
    const RoutingModel::IndexPair& index_pair =
        pickup_delivery_pairs[pair_index];
    for (int64_t pickup : index_pair.first) {
      if (Contains(pickup) ||
          !IsNeighborForCostClass(cost_class, pickup_insert_after, pickup)) {
        continue;
      }
      for (int64_t delivery : index_pair.second) {
        if (Contains(delivery)) {
          continue;
        }
        int64_t delivery_insert_after = pickup;
        while (!model()->IsEnd(delivery_insert_after)) {
          const Insertion insertion = {{pickup, delivery},
                                       delivery_insert_after};
          if (!existing_insertions.contains(insertion)) {
            AddPairEntry(pickup, pickup_insert_after, delivery,
                         delivery_insert_after, vehicle, priority_queue,
                         pickup_to_entries, delivery_to_entries);
          }
          if (delivery_insert_after == pickup) {
            delivery_insert_after = pickup_insert_before;
          } else {
            delivery_insert_after = Value(delivery_insert_after);
          }
        }
      }
    }
  }
  return true;
}

bool GlobalCheapestInsertionFilteredHeuristic::UpdateDeliveryPositions(
    const std::vector<int>& pair_indices, int vehicle,
    int64_t delivery_insert_after,
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredHeuristic::PairEntry>* priority_queue,
    std::vector<GlobalCheapestInsertionFilteredHeuristic::PairEntries>*
        pickup_to_entries,
    std::vector<GlobalCheapestInsertionFilteredHeuristic::PairEntries>*
        delivery_to_entries) {
  // First, remove entries which have already been inserted and keep track of
  // the entries which are being kept and must be updated.
  using Pair = std::pair<int64_t, int64_t>;
  using Insertion = std::pair<Pair, /*pickup_insert_after*/ int64_t>;
  absl::flat_hash_set<Insertion> existing_insertions;
  std::vector<PairEntry*> to_remove;
  for (PairEntry* const pair_entry :
       delivery_to_entries->at(delivery_insert_after)) {
    DCHECK(priority_queue->Contains(pair_entry));
    DCHECK_EQ(pair_entry->delivery_insert_after(), delivery_insert_after);
    if (Contains(pair_entry->pickup_to_insert()) ||
        Contains(pair_entry->delivery_to_insert())) {
      to_remove.push_back(pair_entry);
    } else {
      DCHECK(pickup_to_entries->at(pair_entry->pickup_insert_after())
                 .contains(pair_entry));
      existing_insertions.insert(
          {{pair_entry->pickup_to_insert(), pair_entry->delivery_to_insert()},
           pair_entry->pickup_insert_after()});
      UpdatePairEntry(pair_entry, priority_queue);
    }
  }
  for (PairEntry* const pair_entry : to_remove) {
    DeletePairEntry(pair_entry, priority_queue, pickup_to_entries,
                    delivery_to_entries);
  }
  // Create new entries for which the delivery is to be inserted after
  // delivery_insert_after.
  const int cost_class = model()->GetCostClassIndexOfVehicle(vehicle).value();
  const RoutingModel::IndexPairs& pickup_delivery_pairs =
      model()->GetPickupAndDeliveryPairs();
  for (int pair_index : pair_indices) {
    if (StopSearchAndCleanup(priority_queue)) return false;
    const RoutingModel::IndexPair& index_pair =
        pickup_delivery_pairs[pair_index];
    for (int64_t delivery : index_pair.second) {
      if (Contains(delivery) ||
          !IsNeighborForCostClass(cost_class, delivery_insert_after,
                                  delivery)) {
        continue;
      }
      for (int64_t pickup : index_pair.first) {
        if (Contains(pickup)) {
          continue;
        }
        int64_t pickup_insert_after = model()->Start(vehicle);
        while (pickup_insert_after != delivery_insert_after) {
          const Insertion insertion = {{pickup, delivery}, pickup_insert_after};
          if (!existing_insertions.contains(insertion)) {
            AddPairEntry(pickup, pickup_insert_after, delivery,
                         delivery_insert_after, vehicle, priority_queue,
                         pickup_to_entries, delivery_to_entries);
          }
          pickup_insert_after = Value(pickup_insert_after);
        }
      }
    }
  }
  return true;
}

void GlobalCheapestInsertionFilteredHeuristic::DeletePairEntry(
    GlobalCheapestInsertionFilteredHeuristic::PairEntry* entry,
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredHeuristic::PairEntry>* priority_queue,
    std::vector<PairEntries>* pickup_to_entries,
    std::vector<PairEntries>* delivery_to_entries) {
  priority_queue->Remove(entry);
  if (entry->pickup_insert_after() != -1) {
    pickup_to_entries->at(entry->pickup_insert_after()).erase(entry);
  }
  if (entry->delivery_insert_after() != -1) {
    delivery_to_entries->at(entry->delivery_insert_after()).erase(entry);
  }
  delete entry;
}

void GlobalCheapestInsertionFilteredHeuristic::AddPairEntry(
    int64_t pickup, int64_t pickup_insert_after, int64_t delivery,
    int64_t delivery_insert_after, int vehicle,
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredHeuristic::PairEntry>* priority_queue,
    std::vector<GlobalCheapestInsertionFilteredHeuristic::PairEntries>*
        pickup_entries,
    std::vector<GlobalCheapestInsertionFilteredHeuristic::PairEntries>*
        delivery_entries) const {
  const IntVar* pickup_vehicle_var = model()->VehicleVar(pickup);
  const IntVar* delivery_vehicle_var = model()->VehicleVar(delivery);
  if (!pickup_vehicle_var->Contains(vehicle) ||
      !delivery_vehicle_var->Contains(vehicle)) {
    if (vehicle == -1 || !VehicleIsEmpty(vehicle)) return;
    // We need to check there is not an equivalent empty vehicle the pair
    // could fit on.
    const auto vehicle_is_compatible = [pickup_vehicle_var,
                                        delivery_vehicle_var](int vehicle) {
      return pickup_vehicle_var->Contains(vehicle) &&
             delivery_vehicle_var->Contains(vehicle);
    };
    if (!empty_vehicle_type_curator_->HasCompatibleVehicleOfType(
            empty_vehicle_type_curator_->Type(vehicle),
            vehicle_is_compatible)) {
      return;
    }
  }
  const int num_allowed_vehicles =
      std::min(pickup_vehicle_var->Size(), delivery_vehicle_var->Size());
  if (pickup_insert_after == -1) {
    DCHECK_EQ(delivery_insert_after, -1);
    DCHECK_EQ(vehicle, -1);
    PairEntry* pair_entry =
        new PairEntry(pickup, -1, delivery, -1, -1, num_allowed_vehicles);
    pair_entry->set_value(
        absl::GetFlag(FLAGS_routing_shift_insertion_cost_by_penalty)
            ? 0
            : CapAdd(GetUnperformedValue(pickup),
                     GetUnperformedValue(delivery)));
    priority_queue->Add(pair_entry);
    return;
  }

  PairEntry* const pair_entry =
      new PairEntry(pickup, pickup_insert_after, delivery,
                    delivery_insert_after, vehicle, num_allowed_vehicles);
  pair_entry->set_value(GetInsertionValueForPairAtPositions(
      pickup, pickup_insert_after, delivery, delivery_insert_after, vehicle));

  // Add entry to priority_queue and pickup_/delivery_entries.
  DCHECK(!priority_queue->Contains(pair_entry));
  pickup_entries->at(pickup_insert_after).insert(pair_entry);
  delivery_entries->at(delivery_insert_after).insert(pair_entry);
  priority_queue->Add(pair_entry);
}

void GlobalCheapestInsertionFilteredHeuristic::UpdatePairEntry(
    GlobalCheapestInsertionFilteredHeuristic::PairEntry* const pair_entry,
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredHeuristic::PairEntry>* priority_queue)
    const {
  pair_entry->set_value(GetInsertionValueForPairAtPositions(
      pair_entry->pickup_to_insert(), pair_entry->pickup_insert_after(),
      pair_entry->delivery_to_insert(), pair_entry->delivery_insert_after(),
      pair_entry->vehicle()));

  // Update the priority_queue.
  DCHECK(priority_queue->Contains(pair_entry));
  priority_queue->NoteChangedPriority(pair_entry);
}

int64_t
GlobalCheapestInsertionFilteredHeuristic::GetInsertionValueForPairAtPositions(
    int64_t pickup, int64_t pickup_insert_after, int64_t delivery,
    int64_t delivery_insert_after, int vehicle) const {
  DCHECK_GE(pickup_insert_after, 0);
  const int64_t pickup_insert_before = Value(pickup_insert_after);
  const int64_t pickup_value = GetInsertionCostForNodeAtPosition(
      pickup, pickup_insert_after, pickup_insert_before, vehicle);

  DCHECK_GE(delivery_insert_after, 0);
  const int64_t delivery_insert_before = (delivery_insert_after == pickup)
                                             ? pickup_insert_before
                                             : Value(delivery_insert_after);
  const int64_t delivery_value = GetInsertionCostForNodeAtPosition(
      delivery, delivery_insert_after, delivery_insert_before, vehicle);

  const int64_t penalty_shift =
      absl::GetFlag(FLAGS_routing_shift_insertion_cost_by_penalty)
          ? CapAdd(GetUnperformedValue(pickup), GetUnperformedValue(delivery))
          : 0;
  return CapSub(CapAdd(pickup_value, delivery_value), penalty_shift);
}

bool GlobalCheapestInsertionFilteredHeuristic::InitializePositions(
    const std::vector<int>& nodes, const absl::flat_hash_set<int>& vehicles,
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredHeuristic::NodeEntry>* priority_queue,
    std::vector<GlobalCheapestInsertionFilteredHeuristic::NodeEntries>*
        position_to_node_entries) {
  priority_queue->Clear();
  position_to_node_entries->clear();
  position_to_node_entries->resize(model()->Size());

  const int num_vehicles =
      vehicles.empty() ? model()->vehicles() : vehicles.size();
  const bool all_vehicles = (num_vehicles == model()->vehicles());

  for (int node : nodes) {
    if (Contains(node)) {
      continue;
    }
    if (StopSearchAndCleanup(priority_queue)) return false;
    // Add insertion entry making node unperformed.
    if (gci_params_.add_unperformed_entries &&
        GetUnperformedValue(node) != std::numeric_limits<int64_t>::max()) {
      AddNodeEntry(node, -1, -1, all_vehicles, priority_queue, nullptr);
    }
    // Add all insertion entries making node performed.
    InitializeInsertionEntriesPerformingNode(node, vehicles, priority_queue,
                                             position_to_node_entries);
  }
  return true;
}

void GlobalCheapestInsertionFilteredHeuristic::
    InitializeInsertionEntriesPerformingNode(
        int64_t node, const absl::flat_hash_set<int>& vehicles,
        AdjustablePriorityQueue<
            GlobalCheapestInsertionFilteredHeuristic::NodeEntry>*
            priority_queue,
        std::vector<GlobalCheapestInsertionFilteredHeuristic::NodeEntries>*
            position_to_node_entries) {
  const int num_vehicles =
      vehicles.empty() ? model()->vehicles() : vehicles.size();
  const bool all_vehicles = (num_vehicles == model()->vehicles());

  if (!gci_params_.use_neighbors_ratio_for_initialization) {
    auto vehicles_it = vehicles.begin();
    for (int v = 0; v < num_vehicles; v++) {
      const int vehicle = vehicles.empty() ? v : *vehicles_it++;

      const int64_t start = model()->Start(vehicle);
      if (all_vehicles && VehicleIsEmpty(vehicle) &&
          empty_vehicle_type_curator_->GetLowestFixedCostVehicleOfType(
              empty_vehicle_type_curator_->Type(vehicle)) != vehicle) {
        // We only consider the least expensive empty vehicle of each type for
        // entries.
        continue;
      }
      std::vector<NodeInsertion> insertions;
      AppendInsertionPositionsAfter(node, start, Value(start), vehicle,
                                    &insertions);
      for (const auto [insert_after, insertion_vehicle, value] : insertions) {
        DCHECK_EQ(insertion_vehicle, vehicle);
        AddNodeEntry(node, insert_after, vehicle, all_vehicles, priority_queue,
                     position_to_node_entries);
      }
    }
    return;
  }

  // We're only considering the closest neighbors as insertion positions for
  // the node.
  const auto insert_on_vehicle_for_cost_class = [this, &vehicles, all_vehicles](
                                                    int v, int cost_class) {
    return (model()->GetCostClassIndexOfVehicle(v).value() == cost_class) &&
           (all_vehicles || vehicles.contains(v));
  };
  for (int cost_class = 0; cost_class < model()->GetCostClassesCount();
       cost_class++) {
    for (const int64_t insert_after :
         GetNeighborsOfNodeForCostClass(cost_class, node)) {
      if (!Contains(insert_after)) {
        continue;
      }
      const int vehicle = node_index_to_vehicle_[insert_after];
      if (vehicle == -1 ||
          !insert_on_vehicle_for_cost_class(vehicle, cost_class)) {
        continue;
      }
      if (all_vehicles && VehicleIsEmpty(vehicle) &&
          empty_vehicle_type_curator_->GetLowestFixedCostVehicleOfType(
              empty_vehicle_type_curator_->Type(vehicle)) != vehicle) {
        // We only consider the least expensive empty vehicle of each type for
        // entries.
        continue;
      }
      AddNodeEntry(node, insert_after, vehicle, all_vehicles, priority_queue,
                   position_to_node_entries);
    }
  }
}

bool GlobalCheapestInsertionFilteredHeuristic::UpdatePositions(
    const std::vector<int>& nodes, int vehicle, int64_t insert_after,
    bool all_vehicles,
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredHeuristic::NodeEntry>* priority_queue,
    std::vector<GlobalCheapestInsertionFilteredHeuristic::NodeEntries>*
        node_entries) {
  // Either create new entries if we are inserting after a newly inserted node
  // or remove entries which have already been inserted.
  std::vector<NodeEntry*> to_remove;
  absl::flat_hash_set<int> existing_insertions;
  for (NodeEntry* const node_entry : node_entries->at(insert_after)) {
    DCHECK_EQ(node_entry->insert_after(), insert_after);
    const int64_t node_to_insert = node_entry->node_to_insert();
    if (Contains(node_to_insert)) {
      to_remove.push_back(node_entry);
    } else {
      UpdateNodeEntry(node_entry, priority_queue);
      existing_insertions.insert(node_to_insert);
    }
  }
  for (NodeEntry* const node_entry : to_remove) {
    DeleteNodeEntry(node_entry, priority_queue, node_entries);
  }
  const int cost_class = model()->GetCostClassIndexOfVehicle(vehicle).value();
  for (int node_to_insert : nodes) {
    if (StopSearchAndCleanup(priority_queue)) return false;
    if (!Contains(node_to_insert) &&
        !existing_insertions.contains(node_to_insert) &&
        IsNeighborForCostClass(cost_class, insert_after, node_to_insert)) {
      AddNodeEntry(node_to_insert, insert_after, vehicle, all_vehicles,
                   priority_queue, node_entries);
    }
  }
  return true;
}

void GlobalCheapestInsertionFilteredHeuristic::DeleteNodeEntry(
    GlobalCheapestInsertionFilteredHeuristic::NodeEntry* entry,
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredHeuristic::NodeEntry>* priority_queue,
    std::vector<NodeEntries>* node_entries) {
  priority_queue->Remove(entry);
  if (entry->insert_after() != -1) {
    node_entries->at(entry->insert_after()).erase(entry);
  }
  delete entry;
}

void GlobalCheapestInsertionFilteredHeuristic::AddNodeEntry(
    int64_t node, int64_t insert_after, int vehicle, bool all_vehicles,
    AdjustablePriorityQueue<NodeEntry>* priority_queue,
    std::vector<NodeEntries>* node_entries) const {
  const int64_t node_penalty = GetUnperformedValue(node);
  const int64_t penalty_shift =
      absl::GetFlag(FLAGS_routing_shift_insertion_cost_by_penalty)
          ? node_penalty
          : 0;
  const IntVar* const vehicle_var = model()->VehicleVar(node);
  if (!vehicle_var->Contains(vehicle)) {
    if (vehicle == -1 || !VehicleIsEmpty(vehicle)) return;
    // We need to check there is not an equivalent empty vehicle the node
    // could fit on.
    const auto vehicle_is_compatible = [vehicle_var](int vehicle) {
      return vehicle_var->Contains(vehicle);
    };
    if (!empty_vehicle_type_curator_->HasCompatibleVehicleOfType(
            empty_vehicle_type_curator_->Type(vehicle),
            vehicle_is_compatible)) {
      return;
    }
  }
  const int num_allowed_vehicles = vehicle_var->Size();
  if (insert_after == -1) {
    DCHECK_EQ(vehicle, -1);
    if (!all_vehicles) {
      // NOTE: In the case where we're not considering all routes
      // simultaneously, we don't add insertion entries making nodes
      // unperformed.
      return;
    }
    NodeEntry* const node_entry =
        new NodeEntry(node, -1, -1, num_allowed_vehicles);
    node_entry->set_value(CapSub(node_penalty, penalty_shift));
    priority_queue->Add(node_entry);
    return;
  }

  const int64_t insertion_cost = GetInsertionCostForNodeAtPosition(
      node, insert_after, Value(insert_after), vehicle);
  if (!all_vehicles && insertion_cost > node_penalty) {
    // NOTE: When all vehicles aren't considered for insertion, we don't
    // add entries making nodes unperformed, so we don't add insertions
    // which cost more than the node penalty either.
    return;
  }

  NodeEntry* const node_entry =
      new NodeEntry(node, insert_after, vehicle, num_allowed_vehicles);
  node_entry->set_value(CapSub(insertion_cost, penalty_shift));
  // Add entry to priority_queue and node_entries.
  DCHECK(!priority_queue->Contains(node_entry));
  node_entries->at(insert_after).insert(node_entry);
  priority_queue->Add(node_entry);
}

void GlobalCheapestInsertionFilteredHeuristic::UpdateNodeEntry(
    NodeEntry* const node_entry,
    AdjustablePriorityQueue<NodeEntry>* priority_queue) const {
  const int64_t node = node_entry->node_to_insert();
  const int64_t insert_after = node_entry->insert_after();
  DCHECK_GE(insert_after, 0);
  const int64_t insertion_cost = GetInsertionCostForNodeAtPosition(
      node, insert_after, Value(insert_after), node_entry->vehicle());
  const int64_t penalty_shift =
      absl::GetFlag(FLAGS_routing_shift_insertion_cost_by_penalty)
          ? GetUnperformedValue(node)
          : 0;

  node_entry->set_value(CapSub(insertion_cost, penalty_shift));
  // Update the priority_queue.
  DCHECK(priority_queue->Contains(node_entry));
  priority_queue->NoteChangedPriority(node_entry);
}

// LocalCheapestInsertionFilteredHeuristic
// TODO(user): Add support for penalty costs.
LocalCheapestInsertionFilteredHeuristic::
    LocalCheapestInsertionFilteredHeuristic(
        RoutingModel* model,
        std::function<int64_t(int64_t, int64_t, int64_t)> evaluator,
        LocalSearchFilterManager* filter_manager)
    : CheapestInsertionFilteredHeuristic(model, std::move(evaluator), nullptr,
                                         filter_manager) {
  std::vector<int> all_vehicles(model->vehicles());
  std::iota(std::begin(all_vehicles), std::end(all_vehicles), 0);

  start_end_distances_per_node_ =
      ComputeStartEndDistanceForVehicles(all_vehicles);
}

bool LocalCheapestInsertionFilteredHeuristic::BuildSolutionInternal() {
  // Marking if we've tried inserting a node.
  std::vector<bool> visited(model()->Size(), false);
  // Possible positions where the current pickup can be inserted.
  std::vector<NodeInsertion> pickup_insertion_positions;
  // Possible positions where its associated delivery node can be inserted (if
  // the current node has one).
  std::vector<NodeInsertion> delivery_insertion_positions;
  // Iterating on pickup and delivery pairs
  const RoutingModel::IndexPairs& index_pairs =
      model()->GetPickupAndDeliveryPairs();
  // Sort pairs according to number of possible vehicles.
  std::vector<std::pair<int, int>> domain_to_pair;
  for (int i = 0; i < index_pairs.size(); ++i) {
    uint64_t domain = kint64max;
    for (int64_t pickup : index_pairs[i].first) {
      domain = std::min(domain, model()->VehicleVar(pickup)->Size());
    }
    for (int64_t delivery : index_pairs[i].first) {
      domain = std::min(domain, model()->VehicleVar(delivery)->Size());
    }
    domain_to_pair.emplace_back(domain, i);
  }
  std::sort(domain_to_pair.begin(), domain_to_pair.end());
  for (const auto [domain, pair] : domain_to_pair) {
    const auto& index_pair = index_pairs[pair];
    for (int64_t pickup : index_pair.first) {
      if (Contains(pickup)) {
        continue;
      }
      for (int64_t delivery : index_pair.second) {
        // If either is already in the solution, let it be inserted in the
        // standard node insertion loop.
        if (Contains(delivery)) {
          continue;
        }
        if (StopSearch()) return false;
        visited[pickup] = true;
        visited[delivery] = true;
        ComputeEvaluatorSortedPositions(pickup, &pickup_insertion_positions);
        for (const auto [insert_pickup_after, pickup_vehicle,
                         unused_pickup_value] : pickup_insertion_positions) {
          const int insert_pickup_before = Value(insert_pickup_after);
          ComputeEvaluatorSortedPositionsOnRouteAfter(
              delivery, pickup, insert_pickup_before, pickup_vehicle,
              &delivery_insertion_positions);
          bool found = false;
          for (const auto [insert_delivery_after, unused_delivery_vehicle,
                           unused_delivery_value] :
               delivery_insertion_positions) {
            InsertBetween(pickup, insert_pickup_after, insert_pickup_before);
            const int64_t insert_delivery_before =
                (insert_delivery_after == insert_pickup_after) ? pickup
                : (insert_delivery_after == pickup)
                    ? insert_pickup_before
                    : Value(insert_delivery_after);
            InsertBetween(delivery, insert_delivery_after,
                          insert_delivery_before);
            if (Commit()) {
              found = true;
              break;
            }
          }
          if (found) {
            break;
          }
        }
      }
    }
  }

  std::priority_queue<Seed> node_queue;
  InitializePriorityQueue(&start_end_distances_per_node_, &node_queue);

  // Possible positions where the current node can be inserted.
  std::vector<NodeInsertion> insertion_positions;
  while (!node_queue.empty()) {
    const int node = node_queue.top().second;
    node_queue.pop();
    if (Contains(node) || visited[node]) continue;
    ComputeEvaluatorSortedPositions(node, &insertion_positions);
    for (const auto [insert_after, unused_vehicle, unused_value] :
         insertion_positions) {
      if (StopSearch()) return false;
      InsertBetween(node, insert_after, Value(insert_after));
      if (Commit()) {
        break;
      }
    }
  }
  MakeUnassignedNodesUnperformed();
  return Commit();
}

void LocalCheapestInsertionFilteredHeuristic::ComputeEvaluatorSortedPositions(
    int64_t node, std::vector<NodeInsertion>* sorted_insertions) {
  DCHECK(sorted_insertions != nullptr);
  DCHECK(!Contains(node));
  sorted_insertions->clear();
  const int size = model()->Size();
  if (node < size) {
    for (int vehicle = 0; vehicle < model()->vehicles(); ++vehicle) {
      const int64_t start = model()->Start(vehicle);
      AppendInsertionPositionsAfter(node, start, Value(start), vehicle,
                                    sorted_insertions);
    }
    std::sort(sorted_insertions->begin(), sorted_insertions->end());
  }
}

void LocalCheapestInsertionFilteredHeuristic::
    ComputeEvaluatorSortedPositionsOnRouteAfter(
        int64_t node, int64_t start, int64_t next_after_start, int vehicle,
        std::vector<NodeInsertion>* sorted_insertions) {
  DCHECK(sorted_insertions != nullptr);
  DCHECK(!Contains(node));
  sorted_insertions->clear();
  const int size = model()->Size();
  if (node < size) {
    AppendInsertionPositionsAfter(node, start, next_after_start, vehicle,
                                  sorted_insertions);
    std::sort(sorted_insertions->begin(), sorted_insertions->end());
  }
}

// CheapestAdditionFilteredHeuristic

CheapestAdditionFilteredHeuristic::CheapestAdditionFilteredHeuristic(
    RoutingModel* model, LocalSearchFilterManager* filter_manager)
    : RoutingFilteredHeuristic(model, filter_manager) {}

bool CheapestAdditionFilteredHeuristic::BuildSolutionInternal() {
  const int kUnassigned = -1;
  const RoutingModel::IndexPairs& pairs = model()->GetPickupAndDeliveryPairs();
  std::vector<std::vector<int64_t>> deliveries(Size());
  std::vector<std::vector<int64_t>> pickups(Size());
  for (const RoutingModel::IndexPair& pair : pairs) {
    for (int first : pair.first) {
      for (int second : pair.second) {
        deliveries[first].push_back(second);
        pickups[second].push_back(first);
      }
    }
  }
  // To mimic the behavior of PathSelector (cf. search.cc), iterating on
  // routes with partial route at their start first then on routes with largest
  // index.
  std::vector<int> sorted_vehicles(model()->vehicles(), 0);
  for (int vehicle = 0; vehicle < model()->vehicles(); ++vehicle) {
    sorted_vehicles[vehicle] = vehicle;
  }
  std::sort(sorted_vehicles.begin(), sorted_vehicles.end(),
            PartialRoutesAndLargeVehicleIndicesFirst(*this));
  // Neighbors of the node currently being extended.
  for (const int vehicle : sorted_vehicles) {
    int64_t last_node = GetStartChainEnd(vehicle);
    bool extend_route = true;
    // Extend the route of the current vehicle while it's possible. We can
    // iterate more than once if pickup and delivery pairs have been inserted
    // in the last iteration (see comment below); the new iteration will try to
    // extend the route after the last delivery on the route.
    while (extend_route) {
      extend_route = false;
      bool found = true;
      int64_t index = last_node;
      int64_t end = GetEndChainStart(vehicle);
      // Extend the route until either the end node of the vehicle is reached
      // or no node or node pair can be added. Deliveries in pickup and
      // delivery pairs are added at the same time as pickups, at the end of the
      // route, in reverse order of the pickups. Deliveries are never added
      // alone.
      while (found && !model()->IsEnd(index)) {
        found = false;
        std::vector<int64_t> neighbors;
        if (index < model()->Nexts().size()) {
          std::unique_ptr<IntVarIterator> it(
              model()->Nexts()[index]->MakeDomainIterator(false));
          auto next_values = InitAndGetValues(it.get());
          neighbors = GetPossibleNextsFromIterator(index, next_values.begin(),
                                                   next_values.end());
        }
        for (int i = 0; !found && i < neighbors.size(); ++i) {
          int64_t next = -1;
          switch (i) {
            case 0:
              next = FindTopSuccessor(index, neighbors);
              break;
            case 1:
              SortSuccessors(index, &neighbors);
              ABSL_FALLTHROUGH_INTENDED;
            default:
              next = neighbors[i];
          }
          if (model()->IsEnd(next) && next != end) {
            continue;
          }
          // Only add a delivery if one of its pickups has been added already.
          if (!model()->IsEnd(next) && !pickups[next].empty()) {
            bool contains_pickups = false;
            for (int64_t pickup : pickups[next]) {
              if (Contains(pickup)) {
                contains_pickups = true;
                break;
              }
            }
            if (!contains_pickups) {
              continue;
            }
          }
          std::vector<int64_t> next_deliveries;
          if (next < deliveries.size()) {
            next_deliveries = GetPossibleNextsFromIterator(
                next, deliveries[next].begin(), deliveries[next].end());
          }
          if (next_deliveries.empty()) next_deliveries = {kUnassigned};
          for (int j = 0; !found && j < next_deliveries.size(); ++j) {
            if (StopSearch()) return false;
            int delivery = -1;
            switch (j) {
              case 0:
                delivery = FindTopSuccessor(next, next_deliveries);
                break;
              case 1:
                SortSuccessors(next, &next_deliveries);
                ABSL_FALLTHROUGH_INTENDED;
              default:
                delivery = next_deliveries[j];
            }
            // Insert "next" after "index", and before "end" if it is not the
            // end already.
            SetValue(index, next);
            if (!model()->IsEnd(next)) {
              SetValue(next, end);
              MakeDisjunctionNodesUnperformed(next);
              if (delivery != kUnassigned) {
                SetValue(next, delivery);
                SetValue(delivery, end);
                MakeDisjunctionNodesUnperformed(delivery);
              }
            }
            if (Commit()) {
              index = next;
              found = true;
              if (delivery != kUnassigned) {
                if (model()->IsEnd(end) && last_node != delivery) {
                  last_node = delivery;
                  extend_route = true;
                }
                end = delivery;
              }
              break;
            }
          }
        }
      }
    }
  }
  MakeUnassignedNodesUnperformed();
  return Commit();
}

bool CheapestAdditionFilteredHeuristic::
    PartialRoutesAndLargeVehicleIndicesFirst::operator()(int vehicle1,
                                                         int vehicle2) const {
  const bool has_partial_route1 = (builder_.model()->Start(vehicle1) !=
                                   builder_.GetStartChainEnd(vehicle1));
  const bool has_partial_route2 = (builder_.model()->Start(vehicle2) !=
                                   builder_.GetStartChainEnd(vehicle2));
  if (has_partial_route1 == has_partial_route2) {
    return vehicle2 < vehicle1;
  } else {
    return has_partial_route2 < has_partial_route1;
  }
}

// EvaluatorCheapestAdditionFilteredHeuristic

EvaluatorCheapestAdditionFilteredHeuristic::
    EvaluatorCheapestAdditionFilteredHeuristic(
        RoutingModel* model, std::function<int64_t(int64_t, int64_t)> evaluator,
        LocalSearchFilterManager* filter_manager)
    : CheapestAdditionFilteredHeuristic(model, filter_manager),
      evaluator_(std::move(evaluator)) {}

int64_t EvaluatorCheapestAdditionFilteredHeuristic::FindTopSuccessor(
    int64_t node, const std::vector<int64_t>& successors) {
  int64_t best_evaluation = std::numeric_limits<int64_t>::max();
  int64_t best_successor = -1;
  for (int64_t successor : successors) {
    const int64_t evaluation = (successor >= 0)
                                   ? evaluator_(node, successor)
                                   : std::numeric_limits<int64_t>::max();
    if (evaluation < best_evaluation ||
        (evaluation == best_evaluation && successor > best_successor)) {
      best_evaluation = evaluation;
      best_successor = successor;
    }
  }
  return best_successor;
}

void EvaluatorCheapestAdditionFilteredHeuristic::SortSuccessors(
    int64_t node, std::vector<int64_t>* successors) {
  std::vector<std::pair<int64_t, int64_t>> values;
  values.reserve(successors->size());
  for (int64_t successor : *successors) {
    // Tie-breaking on largest node index to mimic the behavior of
    // CheapestValueSelector (search.cc).
    values.push_back({evaluator_(node, successor), -successor});
  }
  std::sort(values.begin(), values.end());
  successors->clear();
  for (auto value : values) {
    successors->push_back(-value.second);
  }
}

// ComparatorCheapestAdditionFilteredHeuristic

ComparatorCheapestAdditionFilteredHeuristic::
    ComparatorCheapestAdditionFilteredHeuristic(
        RoutingModel* model, Solver::VariableValueComparator comparator,
        LocalSearchFilterManager* filter_manager)
    : CheapestAdditionFilteredHeuristic(model, filter_manager),
      comparator_(std::move(comparator)) {}

int64_t ComparatorCheapestAdditionFilteredHeuristic::FindTopSuccessor(
    int64_t node, const std::vector<int64_t>& successors) {
  return *std::min_element(successors.begin(), successors.end(),
                           [this, node](int successor1, int successor2) {
                             return comparator_(node, successor1, successor2);
                           });
}

void ComparatorCheapestAdditionFilteredHeuristic::SortSuccessors(
    int64_t node, std::vector<int64_t>* successors) {
  std::sort(successors->begin(), successors->end(),
            [this, node](int successor1, int successor2) {
              return comparator_(node, successor1, successor2);
            });
}

// Class storing and allowing access to the savings according to the number of
// vehicle types.
// The savings are stored and sorted in sorted_savings_per_vehicle_type_.
// Furthermore, when there is more than one vehicle type, the savings for a same
// before-->after arc are sorted in costs_and_savings_per_arc_[arc] by
// increasing cost(s-->before-->after-->e), where s and e are the start and end
// of the route, in order to make sure the arc is served by the route with the
// closest depot (start/end) possible.
// When there is only one vehicle "type" (i.e. all vehicles have the same
// start/end and cost class), each arc has a single saving value associated to
// it, so we ignore this last step to avoid unnecessary computations, and only
// work with sorted_savings_per_vehicle_type_[0].
// In case of multiple vehicle types, the best savings for each arc, i.e. the
// savings corresponding to the closest vehicle type, are inserted and sorted in
// sorted_savings_.
//
// This class also handles skipped Savings:
// The vectors skipped_savings_starting/ending_at_ contain all the Savings that
// weren't added to the model, which we want to consider for later:
// 1) When a Saving before-->after with both nodes uncontained cannot be used to
//    start a new route (no more available vehicles or could not commit on any
//    of those available).
// 2) When only one of the nodes of the Saving is contained but on a different
//    vehicle type.
// In these cases, the Update() method is called with update_best_saving = true,
// which in turn calls SkipSavingForArc() (within
// UpdateNextAndSkippedSavingsForArcWithType()) to mark the Saving for this arc
// (with the correct type in the second case) as "skipped", by storing it in
// skipped_savings_starting_at_[before] and skipped_savings_ending_at_[after].
//
// UpdateNextAndSkippedSavingsForArcWithType() also updates the next_savings_
// vector, which stores the savings to go through once we've iterated through
// all sorted_savings_.
// In the first case above, where neither nodes are contained, we skip the
// current Saving (current_saving_), and add the next best Saving for this arc
// to next_savings_ (in case this skipped Saving is never considered).
// In the second case with a specific type, we search for the Saving with the
// correct type for this arc, and add it to both next_savings_ and the skipped
// Savings.
//
// The skipped Savings are then re-considered when one of their ends gets
// inserted:
// When another Saving other_node-->before (or after-->other_node) gets
// inserted, all skipped Savings in skipped_savings_starting_at_[before] (or
// skipped_savings_ending_at_[after]) are once again considered by calling
// ReinjectSkippedSavingsStartingAt() (or ReinjectSkippedSavingsEndingAt()).
// Then, when calling GetSaving(), we iterate through the reinjected Savings in
// order of insertion in the vectors while there are reinjected savings.
template <typename Saving>
class SavingsFilteredHeuristic::SavingsContainer {
 public:
  explicit SavingsContainer(const SavingsFilteredHeuristic* savings_db,
                            int vehicle_types)
      : savings_db_(savings_db),
        index_in_sorted_savings_(0),
        vehicle_types_(vehicle_types),
        single_vehicle_type_(vehicle_types == 1),
        using_incoming_reinjected_saving_(false),
        sorted_(false),
        to_update_(true) {}

  void InitializeContainer(int64_t size, int64_t saving_neighbors) {
    sorted_savings_per_vehicle_type_.clear();
    sorted_savings_per_vehicle_type_.resize(vehicle_types_);
    for (std::vector<Saving>& savings : sorted_savings_per_vehicle_type_) {
      savings.reserve(size * saving_neighbors);
    }

    sorted_savings_.clear();
    costs_and_savings_per_arc_.clear();
    arc_indices_per_before_node_.clear();

    if (!single_vehicle_type_) {
      costs_and_savings_per_arc_.reserve(size * saving_neighbors);
      arc_indices_per_before_node_.resize(size);
      for (int before_node = 0; before_node < size; before_node++) {
        arc_indices_per_before_node_[before_node].reserve(saving_neighbors);
      }
    }
    skipped_savings_starting_at_.clear();
    skipped_savings_starting_at_.resize(size);
    skipped_savings_ending_at_.clear();
    skipped_savings_ending_at_.resize(size);
    incoming_reinjected_savings_ = nullptr;
    outgoing_reinjected_savings_ = nullptr;
    incoming_new_reinjected_savings_ = nullptr;
    outgoing_new_reinjected_savings_ = nullptr;
  }

  void AddNewSaving(const Saving& saving, int64_t total_cost,
                    int64_t before_node, int64_t after_node, int vehicle_type) {
    CHECK(!sorted_savings_per_vehicle_type_.empty())
        << "Container not initialized!";
    sorted_savings_per_vehicle_type_[vehicle_type].push_back(saving);
    UpdateArcIndicesCostsAndSavings(before_node, after_node,
                                    {total_cost, saving});
  }

  void Sort() {
    CHECK(!sorted_) << "Container already sorted!";

    for (std::vector<Saving>& savings : sorted_savings_per_vehicle_type_) {
      std::sort(savings.begin(), savings.end());
    }

    if (single_vehicle_type_) {
      const auto& savings = sorted_savings_per_vehicle_type_[0];
      sorted_savings_.resize(savings.size());
      std::transform(savings.begin(), savings.end(), sorted_savings_.begin(),
                     [](const Saving& saving) {
                       return SavingAndArc({saving, /*arc_index*/ -1});
                     });
    } else {
      // For each arc, sort the savings by decreasing total cost
      // start-->a-->b-->end.
      // The best saving for each arc is therefore the last of its vector.
      sorted_savings_.reserve(vehicle_types_ *
                              costs_and_savings_per_arc_.size());

      for (int arc_index = 0; arc_index < costs_and_savings_per_arc_.size();
           arc_index++) {
        std::vector<std::pair<int64_t, Saving>>& costs_and_savings =
            costs_and_savings_per_arc_[arc_index];
        DCHECK(!costs_and_savings.empty());

        std::sort(
            costs_and_savings.begin(), costs_and_savings.end(),
            [](const std::pair<int64_t, Saving>& cs1,
               const std::pair<int64_t, Saving>& cs2) { return cs1 > cs2; });

        // Insert all Savings for this arc with the lowest cost into
        // sorted_savings_.
        // TODO(user): Also do this when reiterating on next_savings_.
        const int64_t cost = costs_and_savings.back().first;
        while (!costs_and_savings.empty() &&
               costs_and_savings.back().first == cost) {
          sorted_savings_.push_back(
              {costs_and_savings.back().second, arc_index});
          costs_and_savings.pop_back();
        }
      }
      std::sort(sorted_savings_.begin(), sorted_savings_.end());
      next_saving_type_and_index_for_arc_.clear();
      next_saving_type_and_index_for_arc_.resize(
          costs_and_savings_per_arc_.size(), {-1, -1});
    }
    sorted_ = true;
    index_in_sorted_savings_ = 0;
    to_update_ = false;
  }

  bool HasSaving() {
    return index_in_sorted_savings_ < sorted_savings_.size() ||
           HasReinjectedSavings();
  }

  Saving GetSaving() {
    CHECK(sorted_) << "Calling GetSaving() before Sort() !";
    CHECK(!to_update_)
        << "Update() should be called between two calls to GetSaving() !";

    to_update_ = true;

    if (HasReinjectedSavings()) {
      if (incoming_reinjected_savings_ != nullptr &&
          outgoing_reinjected_savings_ != nullptr) {
        // Get the best Saving among the two.
        SavingAndArc& incoming_saving = incoming_reinjected_savings_->front();
        SavingAndArc& outgoing_saving = outgoing_reinjected_savings_->front();
        if (incoming_saving < outgoing_saving) {
          current_saving_ = incoming_saving;
          using_incoming_reinjected_saving_ = true;
        } else {
          current_saving_ = outgoing_saving;
          using_incoming_reinjected_saving_ = false;
        }
      } else {
        if (incoming_reinjected_savings_ != nullptr) {
          current_saving_ = incoming_reinjected_savings_->front();
          using_incoming_reinjected_saving_ = true;
        }
        if (outgoing_reinjected_savings_ != nullptr) {
          current_saving_ = outgoing_reinjected_savings_->front();
          using_incoming_reinjected_saving_ = false;
        }
      }
    } else {
      current_saving_ = sorted_savings_[index_in_sorted_savings_];
    }
    return current_saving_.saving;
  }

  void Update(bool update_best_saving, int type = -1) {
    CHECK(to_update_) << "Container already up to date!";
    if (update_best_saving) {
      const int64_t arc_index = current_saving_.arc_index;
      UpdateNextAndSkippedSavingsForArcWithType(arc_index, type);
    }
    if (!HasReinjectedSavings()) {
      index_in_sorted_savings_++;

      if (index_in_sorted_savings_ == sorted_savings_.size()) {
        sorted_savings_.swap(next_savings_);
        gtl::STLClearObject(&next_savings_);
        index_in_sorted_savings_ = 0;

        std::sort(sorted_savings_.begin(), sorted_savings_.end());
        next_saving_type_and_index_for_arc_.clear();
        next_saving_type_and_index_for_arc_.resize(
            costs_and_savings_per_arc_.size(), {-1, -1});
      }
    }
    UpdateReinjectedSavings();
    to_update_ = false;
  }

  void UpdateWithType(int type) {
    CHECK(!single_vehicle_type_);
    Update(/*update_best_saving*/ true, type);
  }

  const std::vector<Saving>& GetSortedSavingsForVehicleType(int type) {
    CHECK(sorted_) << "Savings not sorted yet!";
    CHECK_LT(type, vehicle_types_);
    return sorted_savings_per_vehicle_type_[type];
  }

  void ReinjectSkippedSavingsStartingAt(int64_t node) {
    CHECK(outgoing_new_reinjected_savings_ == nullptr);
    outgoing_new_reinjected_savings_ = &(skipped_savings_starting_at_[node]);
  }

  void ReinjectSkippedSavingsEndingAt(int64_t node) {
    CHECK(incoming_new_reinjected_savings_ == nullptr);
    incoming_new_reinjected_savings_ = &(skipped_savings_ending_at_[node]);
  }

 private:
  struct SavingAndArc {
    Saving saving;
    int64_t arc_index;

    bool operator<(const SavingAndArc& other) const {
      return std::tie(saving, arc_index) <
             std::tie(other.saving, other.arc_index);
    }
  };

  // Skips the Saving for the arc before_node-->after_node, by adding it to the
  // skipped_savings_ vector of the nodes, if they're uncontained.
  void SkipSavingForArc(const SavingAndArc& saving_and_arc) {
    const Saving& saving = saving_and_arc.saving;
    const int64_t before_node = savings_db_->GetBeforeNodeFromSaving(saving);
    const int64_t after_node = savings_db_->GetAfterNodeFromSaving(saving);
    if (!savings_db_->Contains(before_node)) {
      skipped_savings_starting_at_[before_node].push_back(saving_and_arc);
    }
    if (!savings_db_->Contains(after_node)) {
      skipped_savings_ending_at_[after_node].push_back(saving_and_arc);
    }
  }

  // Called within Update() when update_best_saving is true, this method updates
  // the next_savings_ and skipped savings vectors for a given arc_index and
  // vehicle type.
  // When a Saving with the right type has already been added to next_savings_
  // for this arc, no action is needed on next_savings_.
  // Otherwise, if such a Saving exists, GetNextSavingForArcWithType() will find
  // and assign it to next_saving, which is then used to update next_savings_.
  // Finally, the right Saving is skipped for this arc: if looking for a
  // specific type (i.e. type != -1), next_saving (which has the correct type)
  // is skipped, otherwise the current_saving_ is.
  void UpdateNextAndSkippedSavingsForArcWithType(int64_t arc_index, int type) {
    if (single_vehicle_type_) {
      // No next Saving, skip the current Saving.
      CHECK_EQ(type, -1);
      SkipSavingForArc(current_saving_);
      return;
    }
    CHECK_GE(arc_index, 0);
    auto& type_and_index = next_saving_type_and_index_for_arc_[arc_index];
    const int previous_index = type_and_index.second;
    const int previous_type = type_and_index.first;
    bool next_saving_added = false;
    Saving next_saving;

    if (previous_index >= 0) {
      // Next Saving already added for this arc.
      DCHECK_GE(previous_type, 0);
      if (type == -1 || previous_type == type) {
        // Not looking for a specific type, or correct type already in
        // next_savings_.
        next_saving_added = true;
        next_saving = next_savings_[previous_index].saving;
      }
    }

    if (!next_saving_added &&
        GetNextSavingForArcWithType(arc_index, type, &next_saving)) {
      type_and_index.first = savings_db_->GetVehicleTypeFromSaving(next_saving);
      if (previous_index >= 0) {
        // Update the previous saving.
        next_savings_[previous_index] = {next_saving, arc_index};
      } else {
        // Insert the new next Saving for this arc.
        type_and_index.second = next_savings_.size();
        next_savings_.push_back({next_saving, arc_index});
      }
      next_saving_added = true;
    }

    // Skip the Saving based on the vehicle type.
    if (type == -1) {
      // Skip the current Saving.
      SkipSavingForArc(current_saving_);
    } else {
      // Skip the Saving with the correct type, already added to next_savings_
      // if it was found.
      if (next_saving_added) {
        SkipSavingForArc({next_saving, arc_index});
      }
    }
  }

  void UpdateReinjectedSavings() {
    UpdateGivenReinjectedSavings(incoming_new_reinjected_savings_,
                                 &incoming_reinjected_savings_,
                                 using_incoming_reinjected_saving_);
    UpdateGivenReinjectedSavings(outgoing_new_reinjected_savings_,
                                 &outgoing_reinjected_savings_,
                                 !using_incoming_reinjected_saving_);
    incoming_new_reinjected_savings_ = nullptr;
    outgoing_new_reinjected_savings_ = nullptr;
  }

  void UpdateGivenReinjectedSavings(
      std::deque<SavingAndArc>* new_reinjected_savings,
      std::deque<SavingAndArc>** reinjected_savings,
      bool using_reinjected_savings) {
    if (new_reinjected_savings == nullptr) {
      // No new reinjected savings, update the previous ones if needed.
      if (*reinjected_savings != nullptr && using_reinjected_savings) {
        CHECK(!(*reinjected_savings)->empty());
        (*reinjected_savings)->pop_front();
        if ((*reinjected_savings)->empty()) {
          *reinjected_savings = nullptr;
        }
      }
      return;
    }

    // New savings reinjected.
    // Forget about the previous reinjected savings and add the new ones if
    // there are any.
    if (*reinjected_savings != nullptr) {
      (*reinjected_savings)->clear();
    }
    *reinjected_savings = nullptr;
    if (!new_reinjected_savings->empty()) {
      *reinjected_savings = new_reinjected_savings;
    }
  }

  bool HasReinjectedSavings() {
    return outgoing_reinjected_savings_ != nullptr ||
           incoming_reinjected_savings_ != nullptr;
  }

  void UpdateArcIndicesCostsAndSavings(
      int64_t before_node, int64_t after_node,
      const std::pair<int64_t, Saving>& cost_and_saving) {
    if (single_vehicle_type_) {
      return;
    }
    absl::flat_hash_map<int, int>& arc_indices =
        arc_indices_per_before_node_[before_node];
    const auto& arc_inserted = arc_indices.insert(
        std::make_pair(after_node, costs_and_savings_per_arc_.size()));
    const int index = arc_inserted.first->second;
    if (arc_inserted.second) {
      costs_and_savings_per_arc_.push_back({cost_and_saving});
    } else {
      DCHECK_LT(index, costs_and_savings_per_arc_.size());
      costs_and_savings_per_arc_[index].push_back(cost_and_saving);
    }
  }

  bool GetNextSavingForArcWithType(int64_t arc_index, int type,
                                   Saving* next_saving) {
    std::vector<std::pair<int64_t, Saving>>& costs_and_savings =
        costs_and_savings_per_arc_[arc_index];

    bool found_saving = false;
    while (!costs_and_savings.empty() && !found_saving) {
      const Saving& saving = costs_and_savings.back().second;
      if (type == -1 || savings_db_->GetVehicleTypeFromSaving(saving) == type) {
        *next_saving = saving;
        found_saving = true;
      }
      costs_and_savings.pop_back();
    }
    return found_saving;
  }

  const SavingsFilteredHeuristic* const savings_db_;
  int64_t index_in_sorted_savings_;
  std::vector<std::vector<Saving>> sorted_savings_per_vehicle_type_;
  std::vector<SavingAndArc> sorted_savings_;
  std::vector<SavingAndArc> next_savings_;
  std::vector<std::pair</*type*/ int, /*index*/ int>>
      next_saving_type_and_index_for_arc_;
  SavingAndArc current_saving_;
  std::vector<std::vector<std::pair</*cost*/ int64_t, Saving>>>
      costs_and_savings_per_arc_;
  std::vector<absl::flat_hash_map</*after_node*/ int, /*arc_index*/ int>>
      arc_indices_per_before_node_;
  std::vector<std::deque<SavingAndArc>> skipped_savings_starting_at_;
  std::vector<std::deque<SavingAndArc>> skipped_savings_ending_at_;
  std::deque<SavingAndArc>* outgoing_reinjected_savings_;
  std::deque<SavingAndArc>* incoming_reinjected_savings_;
  std::deque<SavingAndArc>* outgoing_new_reinjected_savings_;
  std::deque<SavingAndArc>* incoming_new_reinjected_savings_;
  const int vehicle_types_;
  const bool single_vehicle_type_;
  bool using_incoming_reinjected_saving_;
  bool sorted_;
  bool to_update_;
};

// SavingsFilteredHeuristic

SavingsFilteredHeuristic::SavingsFilteredHeuristic(
    RoutingModel* model, SavingsParameters parameters,
    LocalSearchFilterManager* filter_manager)
    : RoutingFilteredHeuristic(model, filter_manager),
      vehicle_type_curator_(nullptr),
      savings_params_(parameters) {
  DCHECK_GT(savings_params_.neighbors_ratio, 0);
  DCHECK_LE(savings_params_.neighbors_ratio, 1);
  DCHECK_GT(savings_params_.max_memory_usage_bytes, 0);
  DCHECK_GT(savings_params_.arc_coefficient, 0);
  const int size = model->Size();
  size_squared_ = size * size;
}

SavingsFilteredHeuristic::~SavingsFilteredHeuristic() {}

bool SavingsFilteredHeuristic::BuildSolutionInternal() {
  if (vehicle_type_curator_ == nullptr) {
    vehicle_type_curator_ = absl::make_unique<VehicleTypeCurator>(
        model()->GetVehicleTypeContainer());
  }
  // Only store empty vehicles in the vehicle_type_curator_.
  vehicle_type_curator_->Reset(
      [this](int vehicle) { return VehicleIsEmpty(vehicle); });
  if (!ComputeSavings()) return false;
  BuildRoutesFromSavings();
  // Free all the space used to store the Savings in the container.
  savings_container_.reset();
  MakeUnassignedNodesUnperformed();
  if (!Commit()) return false;
  MakePartiallyPerformedPairsUnperformed();
  return Commit();
}

int SavingsFilteredHeuristic::StartNewRouteWithBestVehicleOfType(
    int type, int64_t before_node, int64_t after_node) {
  auto vehicle_is_compatible = [this, before_node, after_node](int vehicle) {
    if (!model()->VehicleVar(before_node)->Contains(vehicle) ||
        !model()->VehicleVar(after_node)->Contains(vehicle)) {
      return false;
    }
    // Try to commit the arc on this vehicle.
    DCHECK(VehicleIsEmpty(vehicle));
    const int64_t start = model()->Start(vehicle);
    const int64_t end = model()->End(vehicle);
    SetValue(start, before_node);
    SetValue(before_node, after_node);
    SetValue(after_node, end);
    return Commit();
  };

  return vehicle_type_curator_
      ->GetCompatibleVehicleOfType(
          type, vehicle_is_compatible,
          /*stop_and_return_vehicle*/ [](int) { return false; })
      .first;
}

void SavingsFilteredHeuristic::AddSymmetricArcsToAdjacencyLists(
    std::vector<std::vector<int64_t>>* adjacency_lists) {
  for (int64_t node = 0; node < adjacency_lists->size(); node++) {
    for (int64_t neighbor : (*adjacency_lists)[node]) {
      if (model()->IsStart(neighbor) || model()->IsEnd(neighbor)) {
        continue;
      }
      (*adjacency_lists)[neighbor].push_back(node);
    }
  }
  std::transform(adjacency_lists->begin(), adjacency_lists->end(),
                 adjacency_lists->begin(), [](std::vector<int64_t> vec) {
                   std::sort(vec.begin(), vec.end());
                   vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
                   return vec;
                 });
}

// Computes the savings related to each pair of non-start and non-end nodes.
// The savings value for an arc a-->b for a vehicle starting at node s and
// ending at node e is:
// saving = cost(s-->a-->e) + cost(s-->b-->e) - cost(s-->a-->b-->e), i.e.
// saving = cost(a-->e) + cost(s-->b) - cost(a-->b)
// The saving value also considers a coefficient for the cost of the arc
// a-->b, which results in:
// saving = cost(a-->e) + cost(s-->b) - [arc_coefficient_ * cost(a-->b)]
// The higher this saving value, the better the arc.
// Here, the value stored for the savings is -saving, which are therefore
// considered in decreasing order.
bool SavingsFilteredHeuristic::ComputeSavings() {
  const int num_vehicle_types = vehicle_type_curator_->NumTypes();
  const int size = model()->Size();

  std::vector<int64_t> uncontained_non_start_end_nodes;
  uncontained_non_start_end_nodes.reserve(size);
  for (int node = 0; node < size; node++) {
    if (!model()->IsStart(node) && !model()->IsEnd(node) && !Contains(node)) {
      uncontained_non_start_end_nodes.push_back(node);
    }
  }

  const int64_t saving_neighbors =
      std::min(MaxNumNeighborsPerNode(num_vehicle_types),
               static_cast<int64_t>(uncontained_non_start_end_nodes.size()));

  savings_container_ =
      absl::make_unique<SavingsContainer<Saving>>(this, num_vehicle_types);
  savings_container_->InitializeContainer(size, saving_neighbors);
  if (StopSearch()) return false;
  std::vector<std::vector<int64_t>> adjacency_lists(size);

  for (int type = 0; type < num_vehicle_types; ++type) {
    const int vehicle =
        vehicle_type_curator_->GetLowestFixedCostVehicleOfType(type);
    if (vehicle < 0) {
      continue;
    }

    const int64_t cost_class =
        model()->GetCostClassIndexOfVehicle(vehicle).value();
    const int64_t start = model()->Start(vehicle);
    const int64_t end = model()->End(vehicle);
    const int64_t fixed_cost = model()->GetFixedCostOfVehicle(vehicle);

    // Compute the neighbors for each non-start/end node not already inserted in
    // the model.
    for (int before_node : uncontained_non_start_end_nodes) {
      std::vector<std::pair</*cost*/ int64_t, /*node*/ int64_t>>
          costed_after_nodes;
      costed_after_nodes.reserve(uncontained_non_start_end_nodes.size());
      if (StopSearch()) return false;
      for (int after_node : uncontained_non_start_end_nodes) {
        if (after_node != before_node) {
          costed_after_nodes.push_back(std::make_pair(
              model()->GetArcCostForClass(before_node, after_node, cost_class),
              after_node));
        }
      }
      if (saving_neighbors < costed_after_nodes.size()) {
        std::nth_element(costed_after_nodes.begin(),
                         costed_after_nodes.begin() + saving_neighbors,
                         costed_after_nodes.end());
        costed_after_nodes.resize(saving_neighbors);
      }
      adjacency_lists[before_node].resize(costed_after_nodes.size());
      std::transform(costed_after_nodes.begin(), costed_after_nodes.end(),
                     adjacency_lists[before_node].begin(),
                     [](std::pair<int64_t, int64_t> cost_and_node) {
                       return cost_and_node.second;
                     });
    }
    if (savings_params_.add_reverse_arcs) {
      AddSymmetricArcsToAdjacencyLists(&adjacency_lists);
    }
    if (StopSearch()) return false;

    // Build the savings for this vehicle type given the adjacency_lists.
    for (int before_node : uncontained_non_start_end_nodes) {
      const int64_t before_to_end_cost =
          model()->GetArcCostForClass(before_node, end, cost_class);
      const int64_t start_to_before_cost =
          CapSub(model()->GetArcCostForClass(start, before_node, cost_class),
                 fixed_cost);
      if (StopSearch()) return false;
      for (int64_t after_node : adjacency_lists[before_node]) {
        if (model()->IsStart(after_node) || model()->IsEnd(after_node) ||
            before_node == after_node || Contains(after_node)) {
          continue;
        }
        const int64_t arc_cost =
            model()->GetArcCostForClass(before_node, after_node, cost_class);
        const int64_t start_to_after_cost =
            CapSub(model()->GetArcCostForClass(start, after_node, cost_class),
                   fixed_cost);
        const int64_t after_to_end_cost =
            model()->GetArcCostForClass(after_node, end, cost_class);

        const double weighted_arc_cost_fp =
            savings_params_.arc_coefficient * arc_cost;
        const int64_t weighted_arc_cost =
            weighted_arc_cost_fp < std::numeric_limits<int64_t>::max()
                ? static_cast<int64_t>(weighted_arc_cost_fp)
                : std::numeric_limits<int64_t>::max();
        const int64_t saving_value = CapSub(
            CapAdd(before_to_end_cost, start_to_after_cost), weighted_arc_cost);

        const Saving saving =
            BuildSaving(-saving_value, type, before_node, after_node);

        const int64_t total_cost =
            CapAdd(CapAdd(start_to_before_cost, arc_cost), after_to_end_cost);

        savings_container_->AddNewSaving(saving, total_cost, before_node,
                                         after_node, type);
      }
    }
  }
  savings_container_->Sort();
  return !StopSearch();
}

int64_t SavingsFilteredHeuristic::MaxNumNeighborsPerNode(
    int num_vehicle_types) const {
  const int64_t size = model()->Size();

  const int64_t num_neighbors_with_ratio =
      std::max(1.0, size * savings_params_.neighbors_ratio);

  // A single Saving takes 2*8 bytes of memory.
  // max_memory_usage_in_savings_unit = num_savings * multiplicative_factor,
  // Where multiplicative_factor is the memory taken (in Savings unit) for each
  // computed Saving.
  const double max_memory_usage_in_savings_unit =
      savings_params_.max_memory_usage_bytes / 16;

  // In the SavingsContainer, for each Saving, the Savings are stored:
  // - Once in "sorted_savings_per_vehicle_type", and (at most) once in
  //   "sorted_savings_" --> factor 2
  // - If num_vehicle_types > 1, they're also stored by arc_index in
  //   "costs_and_savings_per_arc", along with their int64_t cost --> factor 1.5
  //
  // On top of that,
  // - In the sequential version, the Saving* are also stored by in-coming and
  //   outgoing node (in in/out_savings_ptr), adding another 2*8 bytes per
  //   Saving --> factor 1.
  // - In the parallel version, skipped Savings are also stored in
  //   skipped_savings_starting/ending_at_, resulting in a maximum added factor
  //   of 2 for each Saving.
  // These extra factors are given by ExtraSavingsMemoryMultiplicativeFactor().
  double multiplicative_factor = 2.0 + ExtraSavingsMemoryMultiplicativeFactor();
  if (num_vehicle_types > 1) {
    multiplicative_factor += 1.5;
  }
  const double num_savings =
      max_memory_usage_in_savings_unit / multiplicative_factor;
  const int64_t num_neighbors_with_memory_restriction =
      std::max(1.0, num_savings / (num_vehicle_types * size));

  return std::min(num_neighbors_with_ratio,
                  num_neighbors_with_memory_restriction);
}

// SequentialSavingsFilteredHeuristic

void SequentialSavingsFilteredHeuristic::BuildRoutesFromSavings() {
  const int vehicle_types = vehicle_type_curator_->NumTypes();
  DCHECK_GT(vehicle_types, 0);
  const int size = model()->Size();
  // Store savings for each incoming and outgoing node and by vehicle type. This
  // is necessary to quickly extend partial chains without scanning all savings.
  std::vector<std::vector<const Saving*>> in_savings_ptr(size * vehicle_types);
  std::vector<std::vector<const Saving*>> out_savings_ptr(size * vehicle_types);
  for (int type = 0; type < vehicle_types; type++) {
    const int vehicle_type_offset = type * size;
    const std::vector<Saving>& sorted_savings_for_type =
        savings_container_->GetSortedSavingsForVehicleType(type);
    for (const Saving& saving : sorted_savings_for_type) {
      DCHECK_EQ(GetVehicleTypeFromSaving(saving), type);
      const int before_node = GetBeforeNodeFromSaving(saving);
      in_savings_ptr[vehicle_type_offset + before_node].push_back(&saving);
      const int after_node = GetAfterNodeFromSaving(saving);
      out_savings_ptr[vehicle_type_offset + after_node].push_back(&saving);
    }
  }

  // Build routes from savings.
  while (savings_container_->HasSaving()) {
    if (StopSearch()) return;
    // First find the best saving to start a new route.
    const Saving saving = savings_container_->GetSaving();
    int before_node = GetBeforeNodeFromSaving(saving);
    int after_node = GetAfterNodeFromSaving(saving);
    const bool nodes_not_contained =
        !Contains(before_node) && !Contains(after_node);

    bool committed = false;

    if (nodes_not_contained) {
      // Find the right vehicle to start the route with this Saving.
      const int type = GetVehicleTypeFromSaving(saving);
      const int vehicle =
          StartNewRouteWithBestVehicleOfType(type, before_node, after_node);

      if (vehicle >= 0) {
        committed = true;
        const int64_t start = model()->Start(vehicle);
        const int64_t end = model()->End(vehicle);
        // Then extend the route from both ends of the partial route.
        int in_index = 0;
        int out_index = 0;
        const int saving_offset = type * size;

        while (in_index < in_savings_ptr[saving_offset + after_node].size() ||
               out_index <
                   out_savings_ptr[saving_offset + before_node].size()) {
          if (StopSearch()) return;
          // First determine how to extend the route.
          int before_before_node = -1;
          int after_after_node = -1;
          if (in_index < in_savings_ptr[saving_offset + after_node].size()) {
            const Saving& in_saving =
                *(in_savings_ptr[saving_offset + after_node][in_index]);
            if (out_index <
                out_savings_ptr[saving_offset + before_node].size()) {
              const Saving& out_saving =
                  *(out_savings_ptr[saving_offset + before_node][out_index]);
              if (GetSavingValue(in_saving) < GetSavingValue(out_saving)) {
                // Should extend after after_node
                after_after_node = GetAfterNodeFromSaving(in_saving);
              } else {
                // Should extend before before_node
                before_before_node = GetBeforeNodeFromSaving(out_saving);
              }
            } else {
              // Should extend after after_node
              after_after_node = GetAfterNodeFromSaving(in_saving);
            }
          } else {
            // Should extend before before_node
            before_before_node = GetBeforeNodeFromSaving(
                *(out_savings_ptr[saving_offset + before_node][out_index]));
          }
          // Extend the route
          if (after_after_node != -1) {
            DCHECK_EQ(before_before_node, -1);
            // Extending after after_node
            if (!Contains(after_after_node)) {
              SetValue(after_node, after_after_node);
              SetValue(after_after_node, end);
              if (Commit()) {
                in_index = 0;
                after_node = after_after_node;
              } else {
                ++in_index;
              }
            } else {
              ++in_index;
            }
          } else {
            // Extending before before_node
            CHECK_GE(before_before_node, 0);
            if (!Contains(before_before_node)) {
              SetValue(start, before_before_node);
              SetValue(before_before_node, before_node);
              if (Commit()) {
                out_index = 0;
                before_node = before_before_node;
              } else {
                ++out_index;
              }
            } else {
              ++out_index;
            }
          }
        }
      }
    }
    savings_container_->Update(nodes_not_contained && !committed);
  }
}

// ParallelSavingsFilteredHeuristic

void ParallelSavingsFilteredHeuristic::BuildRoutesFromSavings() {
  // Initialize the vehicles of the first/last non start/end nodes served by
  // each route.
  const int64_t size = model()->Size();
  const int vehicles = model()->vehicles();

  first_node_on_route_.resize(vehicles, -1);
  last_node_on_route_.resize(vehicles, -1);
  vehicle_of_first_or_last_node_.resize(size, -1);

  for (int vehicle = 0; vehicle < vehicles; vehicle++) {
    const int64_t start = model()->Start(vehicle);
    const int64_t end = model()->End(vehicle);
    if (!Contains(start)) {
      continue;
    }
    int64_t node = Value(start);
    if (node != end) {
      vehicle_of_first_or_last_node_[node] = vehicle;
      first_node_on_route_[vehicle] = node;

      int64_t next = Value(node);
      while (next != end) {
        node = next;
        next = Value(node);
      }
      vehicle_of_first_or_last_node_[node] = vehicle;
      last_node_on_route_[vehicle] = node;
    }
  }

  while (savings_container_->HasSaving()) {
    if (StopSearch()) return;
    const Saving saving = savings_container_->GetSaving();
    const int64_t before_node = GetBeforeNodeFromSaving(saving);
    const int64_t after_node = GetAfterNodeFromSaving(saving);
    const int type = GetVehicleTypeFromSaving(saving);

    if (!Contains(before_node) && !Contains(after_node)) {
      // Neither nodes are contained, start a new route.
      bool committed = false;

      const int vehicle =
          StartNewRouteWithBestVehicleOfType(type, before_node, after_node);

      if (vehicle >= 0) {
        committed = true;
        // Store before_node and after_node as first and last nodes of the route
        vehicle_of_first_or_last_node_[before_node] = vehicle;
        vehicle_of_first_or_last_node_[after_node] = vehicle;
        first_node_on_route_[vehicle] = before_node;
        last_node_on_route_[vehicle] = after_node;
        savings_container_->ReinjectSkippedSavingsStartingAt(after_node);
        savings_container_->ReinjectSkippedSavingsEndingAt(before_node);
      }
      savings_container_->Update(!committed);
      continue;
    }

    if (Contains(before_node) && Contains(after_node)) {
      // Merge the two routes if before_node is last and after_node first of its
      // route, the two nodes aren't already on the same route, and the vehicle
      // types are compatible.
      const int v1 = vehicle_of_first_or_last_node_[before_node];
      const int64_t last_node = v1 == -1 ? -1 : last_node_on_route_[v1];

      const int v2 = vehicle_of_first_or_last_node_[after_node];
      const int64_t first_node = v2 == -1 ? -1 : first_node_on_route_[v2];

      if (before_node == last_node && after_node == first_node && v1 != v2 &&
          vehicle_type_curator_->Type(v1) == vehicle_type_curator_->Type(v2)) {
        CHECK_EQ(Value(before_node), model()->End(v1));
        CHECK_EQ(Value(model()->Start(v2)), after_node);

        // We try merging the two routes.
        // TODO(user): Try to use skipped savings to start new routes when
        // a vehicle becomes available after a merge (not trivial because it can
        // result in an infinite loop).
        MergeRoutes(v1, v2, before_node, after_node);
      }
    }

    if (Contains(before_node) && !Contains(after_node)) {
      const int vehicle = vehicle_of_first_or_last_node_[before_node];
      const int64_t last_node =
          vehicle == -1 ? -1 : last_node_on_route_[vehicle];

      if (before_node == last_node) {
        const int64_t end = model()->End(vehicle);
        CHECK_EQ(Value(before_node), end);

        const int route_type = vehicle_type_curator_->Type(vehicle);
        if (type != route_type) {
          // The saving doesn't correspond to the type of the vehicle serving
          // before_node. We update the container with the correct type.
          savings_container_->UpdateWithType(route_type);
          continue;
        }

        // Try adding after_node on route of before_node.
        SetValue(before_node, after_node);
        SetValue(after_node, end);
        if (Commit()) {
          if (first_node_on_route_[vehicle] != before_node) {
            // before_node is no longer the start or end of its route
            DCHECK_NE(Value(model()->Start(vehicle)), before_node);
            vehicle_of_first_or_last_node_[before_node] = -1;
          }
          vehicle_of_first_or_last_node_[after_node] = vehicle;
          last_node_on_route_[vehicle] = after_node;
          savings_container_->ReinjectSkippedSavingsStartingAt(after_node);
        }
      }
    }

    if (!Contains(before_node) && Contains(after_node)) {
      const int vehicle = vehicle_of_first_or_last_node_[after_node];
      const int64_t first_node =
          vehicle == -1 ? -1 : first_node_on_route_[vehicle];

      if (after_node == first_node) {
        const int64_t start = model()->Start(vehicle);
        CHECK_EQ(Value(start), after_node);

        const int route_type = vehicle_type_curator_->Type(vehicle);
        if (type != route_type) {
          // The saving doesn't correspond to the type of the vehicle serving
          // after_node. We update the container with the correct type.
          savings_container_->UpdateWithType(route_type);
          continue;
        }

        // Try adding before_node on route of after_node.
        SetValue(before_node, after_node);
        SetValue(start, before_node);
        if (Commit()) {
          if (last_node_on_route_[vehicle] != after_node) {
            // after_node is no longer the start or end of its route
            DCHECK_NE(Value(after_node), model()->End(vehicle));
            vehicle_of_first_or_last_node_[after_node] = -1;
          }
          vehicle_of_first_or_last_node_[before_node] = vehicle;
          first_node_on_route_[vehicle] = before_node;
          savings_container_->ReinjectSkippedSavingsEndingAt(before_node);
        }
      }
    }
    savings_container_->Update(/*update_best_saving*/ false);
  }
}

void ParallelSavingsFilteredHeuristic::MergeRoutes(int first_vehicle,
                                                   int second_vehicle,
                                                   int64_t before_node,
                                                   int64_t after_node) {
  if (StopSearch()) return;
  const int64_t new_first_node = first_node_on_route_[first_vehicle];
  DCHECK_EQ(vehicle_of_first_or_last_node_[new_first_node], first_vehicle);
  CHECK_EQ(Value(model()->Start(first_vehicle)), new_first_node);
  const int64_t new_last_node = last_node_on_route_[second_vehicle];
  DCHECK_EQ(vehicle_of_first_or_last_node_[new_last_node], second_vehicle);
  CHECK_EQ(Value(new_last_node), model()->End(second_vehicle));

  // Select the vehicle with lower fixed cost to merge the routes.
  int used_vehicle = first_vehicle;
  int unused_vehicle = second_vehicle;
  if (model()->GetFixedCostOfVehicle(first_vehicle) >
      model()->GetFixedCostOfVehicle(second_vehicle)) {
    used_vehicle = second_vehicle;
    unused_vehicle = first_vehicle;
  }

  SetValue(before_node, after_node);
  SetValue(model()->Start(unused_vehicle), model()->End(unused_vehicle));
  if (used_vehicle == first_vehicle) {
    SetValue(new_last_node, model()->End(used_vehicle));
  } else {
    SetValue(model()->Start(used_vehicle), new_first_node);
  }
  bool committed = Commit();
  if (!committed &&
      model()->GetVehicleClassIndexOfVehicle(first_vehicle).value() !=
          model()->GetVehicleClassIndexOfVehicle(second_vehicle).value()) {
    // Try committing on other vehicle instead.
    std::swap(used_vehicle, unused_vehicle);
    SetValue(before_node, after_node);
    SetValue(model()->Start(unused_vehicle), model()->End(unused_vehicle));
    if (used_vehicle == first_vehicle) {
      SetValue(new_last_node, model()->End(used_vehicle));
    } else {
      SetValue(model()->Start(used_vehicle), new_first_node);
    }
    committed = Commit();
  }
  if (committed) {
    // Make unused_vehicle available
    vehicle_type_curator_->ReinjectVehicleOfClass(
        unused_vehicle,
        model()->GetVehicleClassIndexOfVehicle(unused_vehicle).value(),
        model()->GetFixedCostOfVehicle(unused_vehicle));

    // Update the first and last nodes on vehicles.
    first_node_on_route_[unused_vehicle] = -1;
    last_node_on_route_[unused_vehicle] = -1;
    vehicle_of_first_or_last_node_[before_node] = -1;
    vehicle_of_first_or_last_node_[after_node] = -1;
    first_node_on_route_[used_vehicle] = new_first_node;
    last_node_on_route_[used_vehicle] = new_last_node;
    vehicle_of_first_or_last_node_[new_last_node] = used_vehicle;
    vehicle_of_first_or_last_node_[new_first_node] = used_vehicle;
  }
}

// ChristofidesFilteredHeuristic

ChristofidesFilteredHeuristic::ChristofidesFilteredHeuristic(
    RoutingModel* model, LocalSearchFilterManager* filter_manager,
    bool use_minimum_matching)
    : RoutingFilteredHeuristic(model, filter_manager),
      use_minimum_matching_(use_minimum_matching) {}

// TODO(user): Support pickup & delivery.
bool ChristofidesFilteredHeuristic::BuildSolutionInternal() {
  const int size = model()->Size() - model()->vehicles() + 1;
  // Node indices for Christofides solver.
  // 0: start/end node
  // >0: non start/end nodes
  // TODO(user): Add robustness to fixed arcs by collapsing them into meta-
  // nodes.
  std::vector<int> indices(1, 0);
  for (int i = 1; i < size; ++i) {
    if (!model()->IsStart(i) && !model()->IsEnd(i)) {
      indices.push_back(i);
    }
  }
  const int num_cost_classes = model()->GetCostClassesCount();
  std::vector<std::vector<int>> path_per_cost_class(num_cost_classes);
  std::vector<bool> class_covered(num_cost_classes, false);
  for (int vehicle = 0; vehicle < model()->vehicles(); ++vehicle) {
    const int64_t cost_class =
        model()->GetCostClassIndexOfVehicle(vehicle).value();
    if (!class_covered[cost_class]) {
      class_covered[cost_class] = true;
      const int64_t start = model()->Start(vehicle);
      const int64_t end = model()->End(vehicle);
      auto cost = [this, &indices, start, end, cost_class](int from, int to) {
        DCHECK_LT(from, indices.size());
        DCHECK_LT(to, indices.size());
        const int from_index = (from == 0) ? start : indices[from];
        const int to_index = (to == 0) ? end : indices[to];
        const int64_t cost =
            model()->GetArcCostForClass(from_index, to_index, cost_class);
        // To avoid overflow issues, capping costs at kint64max/2, the maximum
        // value supported by MinCostPerfectMatching.
        // TODO(user): Investigate if ChristofidesPathSolver should not
        // return a status to bail out fast in case of problem.
        return std::min(cost, std::numeric_limits<int64_t>::max() / 2);
      };
      using Cost = decltype(cost);
      ChristofidesPathSolver<int64_t, int64_t, int, Cost> christofides_solver(
          indices.size(), cost);
      if (use_minimum_matching_) {
        christofides_solver.SetMatchingAlgorithm(
            ChristofidesPathSolver<int64_t, int64_t, int, Cost>::
                MatchingAlgorithm::MINIMUM_WEIGHT_MATCHING);
      }
      if (christofides_solver.Solve()) {
        path_per_cost_class[cost_class] =
            christofides_solver.TravelingSalesmanPath();
      }
    }
  }
  // TODO(user): Investigate if sorting paths per cost improves solutions.
  for (int vehicle = 0; vehicle < model()->vehicles(); ++vehicle) {
    const int64_t cost_class =
        model()->GetCostClassIndexOfVehicle(vehicle).value();
    const std::vector<int>& path = path_per_cost_class[cost_class];
    if (path.empty()) continue;
    DCHECK_EQ(0, path[0]);
    DCHECK_EQ(0, path.back());
    // Extend route from start.
    int prev = GetStartChainEnd(vehicle);
    const int end = model()->End(vehicle);
    for (int i = 1; i < path.size() - 1 && prev != end; ++i) {
      if (StopSearch()) return false;
      int next = indices[path[i]];
      if (!Contains(next)) {
        SetValue(prev, next);
        SetValue(next, end);
        if (Commit()) {
          prev = next;
        }
      }
    }
  }
  MakeUnassignedNodesUnperformed();
  return Commit();
}

// Sweep heuristic
// TODO(user): Clean up to match other first solution strategies.

namespace {
struct SweepIndex {
  SweepIndex(const int64_t index, const double angle, const double distance)
      : index(index), angle(angle), distance(distance) {}
  ~SweepIndex() {}

  int64_t index;
  double angle;
  double distance;
};

struct SweepIndexSortAngle {
  bool operator()(const SweepIndex& node1, const SweepIndex& node2) const {
    return (node1.angle < node2.angle);
  }
} SweepIndexAngleComparator;

struct SweepIndexSortDistance {
  bool operator()(const SweepIndex& node1, const SweepIndex& node2) const {
    return (node1.distance < node2.distance);
  }
} SweepIndexDistanceComparator;
}  // namespace

SweepArranger::SweepArranger(
    const std::vector<std::pair<int64_t, int64_t>>& points)
    : coordinates_(2 * points.size(), 0), sectors_(1) {
  for (int64_t i = 0; i < points.size(); ++i) {
    coordinates_[2 * i] = points[i].first;
    coordinates_[2 * i + 1] = points[i].second;
  }
}

// Splits the space of the indices into sectors and sorts the indices of each
// sector with ascending angle from the depot.
void SweepArranger::ArrangeIndices(std::vector<int64_t>* indices) {
  const double pi_rad = 3.14159265;
  // Suppose that the center is at x0, y0.
  const int x0 = coordinates_[0];
  const int y0 = coordinates_[1];

  std::vector<SweepIndex> sweep_indices;
  for (int64_t index = 0; index < static_cast<int>(coordinates_.size()) / 2;
       ++index) {
    const int x = coordinates_[2 * index];
    const int y = coordinates_[2 * index + 1];
    const double x_delta = x - x0;
    const double y_delta = y - y0;
    double square_distance = x_delta * x_delta + y_delta * y_delta;
    double angle = square_distance == 0 ? 0 : std::atan2(y_delta, x_delta);
    angle = angle >= 0 ? angle : 2 * pi_rad + angle;
    SweepIndex sweep_index(index, angle, square_distance);
    sweep_indices.push_back(sweep_index);
  }
  std::sort(sweep_indices.begin(), sweep_indices.end(),
            SweepIndexDistanceComparator);

  const int size = static_cast<int>(sweep_indices.size()) / sectors_;
  for (int sector = 0; sector < sectors_; ++sector) {
    std::vector<SweepIndex> cluster;
    std::vector<SweepIndex>::iterator begin =
        sweep_indices.begin() + sector * size;
    std::vector<SweepIndex>::iterator end =
        sector == sectors_ - 1 ? sweep_indices.end()
                               : sweep_indices.begin() + (sector + 1) * size;
    std::sort(begin, end, SweepIndexAngleComparator);
  }
  for (const SweepIndex& sweep_index : sweep_indices) {
    indices->push_back(sweep_index.index);
  }
}

namespace {

struct Link {
  Link(std::pair<int, int> link, double value, int vehicle_class,
       int64_t start_depot, int64_t end_depot)
      : link(link),
        value(value),
        vehicle_class(vehicle_class),
        start_depot(start_depot),
        end_depot(end_depot) {}
  ~Link() {}

  std::pair<int, int> link;
  int64_t value;
  int vehicle_class;
  int64_t start_depot;
  int64_t end_depot;
};

// The RouteConstructor creates the routes of a VRP instance subject to its
// constraints by iterating on a list of arcs appearing in descending order
// of priority.
// TODO(user): Use the dimension class in this class.
// TODO(user): Add support for vehicle-dependent dimension transits.
class RouteConstructor {
 public:
  RouteConstructor(Assignment* const assignment, RoutingModel* const model,
                   bool check_assignment, int64_t num_indices,
                   const std::vector<Link>& links_list)
      : assignment_(assignment),
        model_(model),
        check_assignment_(check_assignment),
        solver_(model_->solver()),
        num_indices_(num_indices),
        links_list_(links_list),
        nexts_(model_->Nexts()),
        in_route_(num_indices_, -1),
        final_routes_(),
        index_to_chain_index_(num_indices, -1),
        index_to_vehicle_class_index_(num_indices, -1) {
    {
      const std::vector<std::string> dimension_names =
          model_->GetAllDimensionNames();
      dimensions_.assign(dimension_names.size(), nullptr);
      for (int i = 0; i < dimension_names.size(); ++i) {
        dimensions_[i] = &model_->GetDimensionOrDie(dimension_names[i]);
      }
    }
    cumuls_.resize(dimensions_.size());
    for (std::vector<int64_t>& cumuls : cumuls_) {
      cumuls.resize(num_indices_);
    }
    new_possible_cumuls_.resize(dimensions_.size());
  }

  ~RouteConstructor() {}

  void Construct() {
    model_->solver()->TopPeriodicCheck();
    // Initial State: Each order is served by its own vehicle.
    for (int index = 0; index < num_indices_; ++index) {
      if (!model_->IsStart(index) && !model_->IsEnd(index)) {
        std::vector<int> route(1, index);
        routes_.push_back(route);
        in_route_[index] = routes_.size() - 1;
      }
    }

    for (const Link& link : links_list_) {
      model_->solver()->TopPeriodicCheck();
      const int index1 = link.link.first;
      const int index2 = link.link.second;
      const int vehicle_class = link.vehicle_class;
      const int64_t start_depot = link.start_depot;
      const int64_t end_depot = link.end_depot;

      // Initialisation of cumuls_ if the indices are encountered for first time
      if (index_to_vehicle_class_index_[index1] < 0) {
        for (int dimension_index = 0; dimension_index < dimensions_.size();
             ++dimension_index) {
          cumuls_[dimension_index][index1] =
              std::max(dimensions_[dimension_index]->GetTransitValue(
                           start_depot, index1, 0),
                       dimensions_[dimension_index]->CumulVar(index1)->Min());
        }
      }
      if (index_to_vehicle_class_index_[index2] < 0) {
        for (int dimension_index = 0; dimension_index < dimensions_.size();
             ++dimension_index) {
          cumuls_[dimension_index][index2] =
              std::max(dimensions_[dimension_index]->GetTransitValue(
                           start_depot, index2, 0),
                       dimensions_[dimension_index]->CumulVar(index2)->Min());
        }
      }

      const int route_index1 = in_route_[index1];
      const int route_index2 = in_route_[index2];
      const bool merge =
          route_index1 >= 0 && route_index2 >= 0 &&
          FeasibleMerge(routes_[route_index1], routes_[route_index2], index1,
                        index2, route_index1, route_index2, vehicle_class,
                        start_depot, end_depot);
      if (Merge(merge, route_index1, route_index2)) {
        index_to_vehicle_class_index_[index1] = vehicle_class;
        index_to_vehicle_class_index_[index2] = vehicle_class;
      }
    }

    model_->solver()->TopPeriodicCheck();
    // Beyond this point not checking limits anymore as the rest of the code is
    // linear and that given we managed to build a solution would be stupid to
    // drop it now.
    for (int chain_index = 0; chain_index < chains_.size(); ++chain_index) {
      if (!deleted_chains_.contains(chain_index)) {
        final_chains_.push_back(chains_[chain_index]);
      }
    }
    std::sort(final_chains_.begin(), final_chains_.end(), ChainComparator);
    for (int route_index = 0; route_index < routes_.size(); ++route_index) {
      if (!deleted_routes_.contains(route_index)) {
        final_routes_.push_back(routes_[route_index]);
      }
    }
    std::sort(final_routes_.begin(), final_routes_.end(), RouteComparator);

    const int extra_vehicles = std::max(
        0, static_cast<int>(final_chains_.size()) - model_->vehicles());
    // Bind the Start and End of each chain
    int chain_index = 0;
    for (chain_index = extra_vehicles; chain_index < final_chains_.size();
         ++chain_index) {
      if (chain_index - extra_vehicles >= model_->vehicles()) {
        break;
      }
      const int start = final_chains_[chain_index].head;
      const int end = final_chains_[chain_index].tail;
      assignment_->Add(
          model_->NextVar(model_->Start(chain_index - extra_vehicles)));
      assignment_->SetValue(
          model_->NextVar(model_->Start(chain_index - extra_vehicles)), start);
      assignment_->Add(nexts_[end]);
      assignment_->SetValue(nexts_[end],
                            model_->End(chain_index - extra_vehicles));
    }

    // Create the single order routes
    for (int route_index = 0; route_index < final_routes_.size();
         ++route_index) {
      if (chain_index - extra_vehicles >= model_->vehicles()) {
        break;
      }
      DCHECK_LT(route_index, final_routes_.size());
      const int head = final_routes_[route_index].front();
      const int tail = final_routes_[route_index].back();
      if (head == tail && head < model_->Size()) {
        assignment_->Add(
            model_->NextVar(model_->Start(chain_index - extra_vehicles)));
        assignment_->SetValue(
            model_->NextVar(model_->Start(chain_index - extra_vehicles)), head);
        assignment_->Add(nexts_[tail]);
        assignment_->SetValue(nexts_[tail],
                              model_->End(chain_index - extra_vehicles));
        ++chain_index;
      }
    }

    // Unperformed
    for (int index = 0; index < model_->Size(); ++index) {
      IntVar* const next = nexts_[index];
      if (!assignment_->Contains(next)) {
        assignment_->Add(next);
        if (next->Contains(index)) {
          assignment_->SetValue(next, index);
        }
      }
    }
  }

 private:
  enum MergeStatus { FIRST_SECOND, SECOND_FIRST, NO_MERGE };

  struct RouteSort {
    bool operator()(const std::vector<int>& route1,
                    const std::vector<int>& route2) const {
      return (route1.size() < route2.size());
    }
  } RouteComparator;

  struct Chain {
    int head;
    int tail;
    int nodes;
  };

  struct ChainSort {
    bool operator()(const Chain& chain1, const Chain& chain2) const {
      return (chain1.nodes < chain2.nodes);
    }
  } ChainComparator;

  bool Head(int node) const {
    return (node == routes_[in_route_[node]].front());
  }

  bool Tail(int node) const {
    return (node == routes_[in_route_[node]].back());
  }

  bool FeasibleRoute(const std::vector<int>& route, int64_t route_cumul,
                     int dimension_index) {
    const RoutingDimension& dimension = *dimensions_[dimension_index];
    std::vector<int>::const_iterator it = route.begin();
    int64_t cumul = route_cumul;
    while (it != route.end()) {
      const int previous = *it;
      const int64_t cumul_previous = cumul;
      gtl::InsertOrDie(&(new_possible_cumuls_[dimension_index]), previous,
                       cumul_previous);
      ++it;
      if (it == route.end()) {
        return true;
      }
      const int next = *it;
      int64_t available_from_previous =
          cumul_previous + dimension.GetTransitValue(previous, next, 0);
      int64_t available_cumul_next =
          std::max(cumuls_[dimension_index][next], available_from_previous);

      const int64_t slack = available_cumul_next - available_from_previous;
      if (slack > dimension.SlackVar(previous)->Max()) {
        available_cumul_next =
            available_from_previous + dimension.SlackVar(previous)->Max();
      }

      if (available_cumul_next > dimension.CumulVar(next)->Max()) {
        return false;
      }
      if (available_cumul_next <= cumuls_[dimension_index][next]) {
        return true;
      }
      cumul = available_cumul_next;
    }
    return true;
  }

  bool CheckRouteConnection(const std::vector<int>& route1,
                            const std::vector<int>& route2, int dimension_index,
                            int64_t start_depot, int64_t end_depot) {
    const int tail1 = route1.back();
    const int head2 = route2.front();
    const int tail2 = route2.back();
    const RoutingDimension& dimension = *dimensions_[dimension_index];
    int non_depot_node = -1;
    for (int node = 0; node < num_indices_; ++node) {
      if (!model_->IsStart(node) && !model_->IsEnd(node)) {
        non_depot_node = node;
        break;
      }
    }
    CHECK_GE(non_depot_node, 0);
    const int64_t depot_threshold =
        std::max(dimension.SlackVar(non_depot_node)->Max(),
                 dimension.CumulVar(non_depot_node)->Max());

    int64_t available_from_tail1 = cumuls_[dimension_index][tail1] +
                                   dimension.GetTransitValue(tail1, head2, 0);
    int64_t new_available_cumul_head2 =
        std::max(cumuls_[dimension_index][head2], available_from_tail1);

    const int64_t slack = new_available_cumul_head2 - available_from_tail1;
    if (slack > dimension.SlackVar(tail1)->Max()) {
      new_available_cumul_head2 =
          available_from_tail1 + dimension.SlackVar(tail1)->Max();
    }

    bool feasible_route = true;
    if (new_available_cumul_head2 > dimension.CumulVar(head2)->Max()) {
      return false;
    }
    if (new_available_cumul_head2 <= cumuls_[dimension_index][head2]) {
      return true;
    }

    feasible_route =
        FeasibleRoute(route2, new_available_cumul_head2, dimension_index);
    const int64_t new_possible_cumul_tail2 =
        new_possible_cumuls_[dimension_index].contains(tail2)
            ? new_possible_cumuls_[dimension_index][tail2]
            : cumuls_[dimension_index][tail2];

    if (!feasible_route || (new_possible_cumul_tail2 +
                                dimension.GetTransitValue(tail2, end_depot, 0) >
                            depot_threshold)) {
      return false;
    }
    return true;
  }

  bool FeasibleMerge(const std::vector<int>& route1,
                     const std::vector<int>& route2, int node1, int node2,
                     int route_index1, int route_index2, int vehicle_class,
                     int64_t start_depot, int64_t end_depot) {
    if ((route_index1 == route_index2) || !(Tail(node1) && Head(node2))) {
      return false;
    }

    // Vehicle Class Check
    if (!((index_to_vehicle_class_index_[node1] == -1 &&
           index_to_vehicle_class_index_[node2] == -1) ||
          (index_to_vehicle_class_index_[node1] == vehicle_class &&
           index_to_vehicle_class_index_[node2] == -1) ||
          (index_to_vehicle_class_index_[node1] == -1 &&
           index_to_vehicle_class_index_[node2] == vehicle_class) ||
          (index_to_vehicle_class_index_[node1] == vehicle_class &&
           index_to_vehicle_class_index_[node2] == vehicle_class))) {
      return false;
    }

    // Check Route1 -> Route2 connection for every dimension
    bool merge = true;
    for (int dimension_index = 0; dimension_index < dimensions_.size();
         ++dimension_index) {
      new_possible_cumuls_[dimension_index].clear();
      merge = merge && CheckRouteConnection(route1, route2, dimension_index,
                                            start_depot, end_depot);
      if (!merge) {
        return false;
      }
    }
    return true;
  }

  bool CheckTempAssignment(Assignment* const temp_assignment,
                           int new_chain_index, int old_chain_index, int head1,
                           int tail1, int head2, int tail2) {
    // TODO(user): If the chain index is greater than the number of vehicles,
    // use another vehicle instead.
    if (new_chain_index >= model_->vehicles()) return false;
    const int start = head1;
    temp_assignment->Add(model_->NextVar(model_->Start(new_chain_index)));
    temp_assignment->SetValue(model_->NextVar(model_->Start(new_chain_index)),
                              start);
    temp_assignment->Add(nexts_[tail1]);
    temp_assignment->SetValue(nexts_[tail1], head2);
    temp_assignment->Add(nexts_[tail2]);
    temp_assignment->SetValue(nexts_[tail2], model_->End(new_chain_index));
    for (int chain_index = 0; chain_index < chains_.size(); ++chain_index) {
      if ((chain_index != new_chain_index) &&
          (chain_index != old_chain_index) &&
          (!deleted_chains_.contains(chain_index))) {
        const int start = chains_[chain_index].head;
        const int end = chains_[chain_index].tail;
        temp_assignment->Add(model_->NextVar(model_->Start(chain_index)));
        temp_assignment->SetValue(model_->NextVar(model_->Start(chain_index)),
                                  start);
        temp_assignment->Add(nexts_[end]);
        temp_assignment->SetValue(nexts_[end], model_->End(chain_index));
      }
    }
    return solver_->Solve(solver_->MakeRestoreAssignment(temp_assignment));
  }

  bool UpdateAssignment(const std::vector<int>& route1,
                        const std::vector<int>& route2) {
    bool feasible = true;
    const int head1 = route1.front();
    const int tail1 = route1.back();
    const int head2 = route2.front();
    const int tail2 = route2.back();
    const int chain_index1 = index_to_chain_index_[head1];
    const int chain_index2 = index_to_chain_index_[head2];
    if (chain_index1 < 0 && chain_index2 < 0) {
      const int chain_index = chains_.size();
      if (check_assignment_) {
        Assignment* const temp_assignment =
            solver_->MakeAssignment(assignment_);
        feasible = CheckTempAssignment(temp_assignment, chain_index, -1, head1,
                                       tail1, head2, tail2);
      }
      if (feasible) {
        Chain chain;
        chain.head = head1;
        chain.tail = tail2;
        chain.nodes = 2;
        index_to_chain_index_[head1] = chain_index;
        index_to_chain_index_[tail2] = chain_index;
        chains_.push_back(chain);
      }
    } else if (chain_index1 >= 0 && chain_index2 < 0) {
      if (check_assignment_) {
        Assignment* const temp_assignment =
            solver_->MakeAssignment(assignment_);
        feasible =
            CheckTempAssignment(temp_assignment, chain_index1, chain_index2,
                                head1, tail1, head2, tail2);
      }
      if (feasible) {
        index_to_chain_index_[tail2] = chain_index1;
        chains_[chain_index1].head = head1;
        chains_[chain_index1].tail = tail2;
        ++chains_[chain_index1].nodes;
      }
    } else if (chain_index1 < 0 && chain_index2 >= 0) {
      if (check_assignment_) {
        Assignment* const temp_assignment =
            solver_->MakeAssignment(assignment_);
        feasible =
            CheckTempAssignment(temp_assignment, chain_index2, chain_index1,
                                head1, tail1, head2, tail2);
      }
      if (feasible) {
        index_to_chain_index_[head1] = chain_index2;
        chains_[chain_index2].head = head1;
        chains_[chain_index2].tail = tail2;
        ++chains_[chain_index2].nodes;
      }
    } else {
      if (check_assignment_) {
        Assignment* const temp_assignment =
            solver_->MakeAssignment(assignment_);
        feasible =
            CheckTempAssignment(temp_assignment, chain_index1, chain_index2,
                                head1, tail1, head2, tail2);
      }
      if (feasible) {
        index_to_chain_index_[tail2] = chain_index1;
        chains_[chain_index1].head = head1;
        chains_[chain_index1].tail = tail2;
        chains_[chain_index1].nodes += chains_[chain_index2].nodes;
        deleted_chains_.insert(chain_index2);
      }
    }
    if (feasible) {
      assignment_->Add(nexts_[tail1]);
      assignment_->SetValue(nexts_[tail1], head2);
    }
    return feasible;
  }

  bool Merge(bool merge, int index1, int index2) {
    if (merge) {
      if (UpdateAssignment(routes_[index1], routes_[index2])) {
        // Connection Route1 -> Route2
        for (const int node : routes_[index2]) {
          in_route_[node] = index1;
          routes_[index1].push_back(node);
        }
        for (int dimension_index = 0; dimension_index < dimensions_.size();
             ++dimension_index) {
          for (const std::pair<int, int64_t> new_possible_cumul :
               new_possible_cumuls_[dimension_index]) {
            cumuls_[dimension_index][new_possible_cumul.first] =
                new_possible_cumul.second;
          }
        }
        deleted_routes_.insert(index2);
        return true;
      }
    }
    return false;
  }

  Assignment* const assignment_;
  RoutingModel* const model_;
  const bool check_assignment_;
  Solver* const solver_;
  const int64_t num_indices_;
  const std::vector<Link> links_list_;
  std::vector<IntVar*> nexts_;
  std::vector<const RoutingDimension*> dimensions_;  // Not owned.
  std::vector<std::vector<int64_t>> cumuls_;
  std::vector<absl::flat_hash_map<int, int64_t>> new_possible_cumuls_;
  std::vector<std::vector<int>> routes_;
  std::vector<int> in_route_;
  absl::flat_hash_set<int> deleted_routes_;
  std::vector<std::vector<int>> final_routes_;
  std::vector<Chain> chains_;
  absl::flat_hash_set<int> deleted_chains_;
  std::vector<Chain> final_chains_;
  std::vector<int> index_to_chain_index_;
  std::vector<int> index_to_vehicle_class_index_;
};

// Decision Builder building a first solution based on Sweep heuristic for
// Vehicle Routing Problem.
// Suitable only when distance is considered as the cost.
class SweepBuilder : public DecisionBuilder {
 public:
  SweepBuilder(RoutingModel* const model, bool check_assignment)
      : model_(model), check_assignment_(check_assignment) {}
  ~SweepBuilder() override {}

  Decision* Next(Solver* const solver) override {
    // Setup the model of the instance for the Sweep Algorithm
    ModelSetup();

    // Build the assignment routes for the model
    Assignment* const assignment = solver->MakeAssignment();
    route_constructor_ = absl::make_unique<RouteConstructor>(
        assignment, model_, check_assignment_, num_indices_, links_);
    // This call might cause backtracking if the search limit is reached.
    route_constructor_->Construct();
    route_constructor_.reset(nullptr);
    // This call might cause backtracking if the solution is not feasible.
    assignment->Restore();

    return nullptr;
  }

 private:
  void ModelSetup() {
    const int depot = model_->GetDepot();
    num_indices_ = model_->Size() + model_->vehicles();
    if (absl::GetFlag(FLAGS_sweep_sectors) > 0 &&
        absl::GetFlag(FLAGS_sweep_sectors) < num_indices_) {
      model_->sweep_arranger()->SetSectors(absl::GetFlag(FLAGS_sweep_sectors));
    }
    std::vector<int64_t> indices;
    model_->sweep_arranger()->ArrangeIndices(&indices);
    for (int i = 0; i < indices.size() - 1; ++i) {
      const int64_t first = indices[i];
      const int64_t second = indices[i + 1];
      if ((model_->IsStart(first) || !model_->IsEnd(first)) &&
          (model_->IsStart(second) || !model_->IsEnd(second))) {
        if (first != depot && second != depot) {
          Link link(std::make_pair(first, second), 0, 0, depot, depot);
          links_.push_back(link);
        }
      }
    }
  }

  RoutingModel* const model_;
  std::unique_ptr<RouteConstructor> route_constructor_;
  const bool check_assignment_;
  int64_t num_indices_;
  std::vector<Link> links_;
};
}  // namespace

DecisionBuilder* MakeSweepDecisionBuilder(RoutingModel* model,
                                          bool check_assignment) {
  return model->solver()->RevAlloc(new SweepBuilder(model, check_assignment));
}

// AllUnperformed

namespace {
// Decision builder to build a solution with all nodes inactive. It does no
// branching and may fail if some nodes cannot be made inactive.

class AllUnperformed : public DecisionBuilder {
 public:
  // Does not take ownership of model.
  explicit AllUnperformed(RoutingModel* const model) : model_(model) {}
  ~AllUnperformed() override {}
  Decision* Next(Solver* const solver) override {
    // Solver::(Un)FreezeQueue is private, passing through the public API
    // on PropagationBaseObject.
    model_->CostVar()->FreezeQueue();
    for (int i = 0; i < model_->Size(); ++i) {
      if (!model_->IsStart(i)) {
        model_->ActiveVar(i)->SetValue(0);
      }
    }
    model_->CostVar()->UnfreezeQueue();
    return nullptr;
  }

 private:
  RoutingModel* const model_;
};
}  // namespace

DecisionBuilder* MakeAllUnperformed(RoutingModel* model) {
  return model->solver()->RevAlloc(new AllUnperformed(model));
}

namespace {
// The description is in routing.h:MakeGuidedSlackFinalizer
class GuidedSlackFinalizer : public DecisionBuilder {
 public:
  GuidedSlackFinalizer(const RoutingDimension* dimension, RoutingModel* model,
                       std::function<int64_t(int64_t)> initializer);
  Decision* Next(Solver* solver) override;

 private:
  int64_t SelectValue(int64_t index);
  int64_t ChooseVariable();

  const RoutingDimension* const dimension_;
  RoutingModel* const model_;
  const std::function<int64_t(int64_t)> initializer_;
  RevArray<bool> is_initialized_;
  std::vector<int64_t> initial_values_;
  Rev<int64_t> current_index_;
  Rev<int64_t> current_route_;
  RevArray<int64_t> last_delta_used_;

  DISALLOW_COPY_AND_ASSIGN(GuidedSlackFinalizer);
};

GuidedSlackFinalizer::GuidedSlackFinalizer(
    const RoutingDimension* dimension, RoutingModel* model,
    std::function<int64_t(int64_t)> initializer)
    : dimension_(ABSL_DIE_IF_NULL(dimension)),
      model_(ABSL_DIE_IF_NULL(model)),
      initializer_(std::move(initializer)),
      is_initialized_(dimension->slacks().size(), false),
      initial_values_(dimension->slacks().size(),
                      std::numeric_limits<int64_t>::min()),
      current_index_(model_->Start(0)),
      current_route_(0),
      last_delta_used_(dimension->slacks().size(), 0) {}

Decision* GuidedSlackFinalizer::Next(Solver* solver) {
  CHECK_EQ(solver, model_->solver());
  const int node_idx = ChooseVariable();
  CHECK(node_idx == -1 ||
        (node_idx >= 0 && node_idx < dimension_->slacks().size()));
  if (node_idx != -1) {
    if (!is_initialized_[node_idx]) {
      initial_values_[node_idx] = initializer_(node_idx);
      is_initialized_.SetValue(solver, node_idx, true);
    }
    const int64_t value = SelectValue(node_idx);
    IntVar* const slack_variable = dimension_->SlackVar(node_idx);
    return solver->MakeAssignVariableValue(slack_variable, value);
  }
  return nullptr;
}

int64_t GuidedSlackFinalizer::SelectValue(int64_t index) {
  const IntVar* const slack_variable = dimension_->SlackVar(index);
  const int64_t center = initial_values_[index];
  const int64_t max_delta =
      std::max(center - slack_variable->Min(), slack_variable->Max() - center) +
      1;
  int64_t delta = last_delta_used_[index];

  // The sequence of deltas is 0, 1, -1, 2, -2 ...
  // Only the values inside the domain of variable are returned.
  while (std::abs(delta) < max_delta &&
         !slack_variable->Contains(center + delta)) {
    if (delta > 0) {
      delta = -delta;
    } else {
      delta = -delta + 1;
    }
  }
  last_delta_used_.SetValue(model_->solver(), index, delta);
  return center + delta;
}

int64_t GuidedSlackFinalizer::ChooseVariable() {
  int64_t int_current_node = current_index_.Value();
  int64_t int_current_route = current_route_.Value();

  while (int_current_route < model_->vehicles()) {
    while (!model_->IsEnd(int_current_node) &&
           dimension_->SlackVar(int_current_node)->Bound()) {
      int_current_node = model_->NextVar(int_current_node)->Value();
    }
    if (!model_->IsEnd(int_current_node)) {
      break;
    }
    int_current_route += 1;
    if (int_current_route < model_->vehicles()) {
      int_current_node = model_->Start(int_current_route);
    }
  }

  CHECK(int_current_route == model_->vehicles() ||
        !dimension_->SlackVar(int_current_node)->Bound());
  current_index_.SetValue(model_->solver(), int_current_node);
  current_route_.SetValue(model_->solver(), int_current_route);
  if (int_current_route < model_->vehicles()) {
    return int_current_node;
  } else {
    return -1;
  }
}
}  // namespace

DecisionBuilder* RoutingModel::MakeGuidedSlackFinalizer(
    const RoutingDimension* dimension,
    std::function<int64_t(int64_t)> initializer) {
  return solver_->RevAlloc(
      new GuidedSlackFinalizer(dimension, this, std::move(initializer)));
}

int64_t RoutingDimension::ShortestTransitionSlack(int64_t node) const {
  CHECK_EQ(base_dimension_, this);
  CHECK(!model_->IsEnd(node));
  // Recall that the model is cumul[i+1] = cumul[i] + transit[i] + slack[i]. Our
  // aim is to find a value for slack[i] such that cumul[i+1] + transit[i+1] is
  // minimized.
  const int64_t next = model_->NextVar(node)->Value();
  if (model_->IsEnd(next)) {
    return SlackVar(node)->Min();
  }
  const int64_t next_next = model_->NextVar(next)->Value();
  const int64_t serving_vehicle = model_->VehicleVar(node)->Value();
  CHECK_EQ(serving_vehicle, model_->VehicleVar(next)->Value());
  const RoutingModel::StateDependentTransit transit_from_next =
      model_->StateDependentTransitCallback(
          state_dependent_class_evaluators_
              [state_dependent_vehicle_to_class_[serving_vehicle]])(next,
                                                                    next_next);
  // We have that transit[i+1] is a function of cumul[i+1].
  const int64_t next_cumul_min = CumulVar(next)->Min();
  const int64_t next_cumul_max = CumulVar(next)->Max();
  const int64_t optimal_next_cumul =
      transit_from_next.transit_plus_identity->RangeMinArgument(
          next_cumul_min, next_cumul_max + 1);
  // A few checks to make sure we're on the same page.
  DCHECK_LE(next_cumul_min, optimal_next_cumul);
  DCHECK_LE(optimal_next_cumul, next_cumul_max);
  // optimal_next_cumul = cumul + transit + optimal_slack, so
  // optimal_slack = optimal_next_cumul - cumul - transit.
  // In the current implementation TransitVar(i) = transit[i] + slack[i], so we
  // have to find the transit from the evaluators.
  const int64_t current_cumul = CumulVar(node)->Value();
  const int64_t current_state_independent_transit = model_->TransitCallback(
      class_evaluators_[vehicle_to_class_[serving_vehicle]])(node, next);
  const int64_t current_state_dependent_transit =
      model_
          ->StateDependentTransitCallback(
              state_dependent_class_evaluators_
                  [state_dependent_vehicle_to_class_[serving_vehicle]])(node,
                                                                        next)
          .transit->Query(current_cumul);
  const int64_t optimal_slack = optimal_next_cumul - current_cumul -
                                current_state_independent_transit -
                                current_state_dependent_transit;
  CHECK_LE(SlackVar(node)->Min(), optimal_slack);
  CHECK_LE(optimal_slack, SlackVar(node)->Max());
  return optimal_slack;
}

namespace {
class GreedyDescentLSOperator : public LocalSearchOperator {
 public:
  explicit GreedyDescentLSOperator(std::vector<IntVar*> variables);

  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override;
  void Start(const Assignment* assignment) override;

 private:
  int64_t FindMaxDistanceToDomain(const Assignment* assignment);

  const std::vector<IntVar*> variables_;
  const Assignment* center_;
  int64_t current_step_;
  // The deltas are returned in this order:
  // (current_step_, 0, ... 0), (-current_step_, 0, ... 0),
  // (0, current_step_, ... 0), (0, -current_step_, ... 0),
  // ...
  // (0, ... 0, current_step_), (0, ... 0, -current_step_).
  // current_direction_ keeps track what was the last returned delta.
  int64_t current_direction_;

  DISALLOW_COPY_AND_ASSIGN(GreedyDescentLSOperator);
};

GreedyDescentLSOperator::GreedyDescentLSOperator(std::vector<IntVar*> variables)
    : variables_(std::move(variables)),
      center_(nullptr),
      current_step_(0),
      current_direction_(0) {}

bool GreedyDescentLSOperator::MakeNextNeighbor(Assignment* delta,
                                               Assignment* /*deltadelta*/) {
  static const int64_t sings[] = {1, -1};
  for (; 1 <= current_step_; current_step_ /= 2) {
    for (; current_direction_ < 2 * variables_.size();) {
      const int64_t variable_idx = current_direction_ / 2;
      IntVar* const variable = variables_[variable_idx];
      const int64_t sign_index = current_direction_ % 2;
      const int64_t sign = sings[sign_index];
      const int64_t offset = sign * current_step_;
      const int64_t new_value = center_->Value(variable) + offset;
      ++current_direction_;
      if (variable->Contains(new_value)) {
        delta->Add(variable);
        delta->SetValue(variable, new_value);
        return true;
      }
    }
    current_direction_ = 0;
  }
  return false;
}

void GreedyDescentLSOperator::Start(const Assignment* assignment) {
  CHECK(assignment != nullptr);
  current_step_ = FindMaxDistanceToDomain(assignment);
  center_ = assignment;
}

int64_t GreedyDescentLSOperator::FindMaxDistanceToDomain(
    const Assignment* assignment) {
  int64_t result = std::numeric_limits<int64_t>::min();
  for (const IntVar* const var : variables_) {
    result = std::max(result, std::abs(var->Max() - assignment->Value(var)));
    result = std::max(result, std::abs(var->Min() - assignment->Value(var)));
  }
  return result;
}
}  // namespace

std::unique_ptr<LocalSearchOperator> RoutingModel::MakeGreedyDescentLSOperator(
    std::vector<IntVar*> variables) {
  return std::unique_ptr<LocalSearchOperator>(
      new GreedyDescentLSOperator(std::move(variables)));
}

DecisionBuilder* RoutingModel::MakeSelfDependentDimensionFinalizer(
    const RoutingDimension* dimension) {
  CHECK(dimension != nullptr);
  CHECK(dimension->base_dimension() == dimension);
  std::function<int64_t(int64_t)> slack_guide = [dimension](int64_t index) {
    return dimension->ShortestTransitionSlack(index);
  };
  DecisionBuilder* const guided_finalizer =
      MakeGuidedSlackFinalizer(dimension, slack_guide);
  DecisionBuilder* const slacks_finalizer =
      solver_->MakeSolveOnce(guided_finalizer);
  std::vector<IntVar*> start_cumuls(vehicles_, nullptr);
  for (int64_t vehicle_idx = 0; vehicle_idx < vehicles_; ++vehicle_idx) {
    start_cumuls[vehicle_idx] = dimension->CumulVar(starts_[vehicle_idx]);
  }
  LocalSearchOperator* const hill_climber =
      solver_->RevAlloc(new GreedyDescentLSOperator(start_cumuls));
  LocalSearchPhaseParameters* const parameters =
      solver_->MakeLocalSearchPhaseParameters(CostVar(), hill_climber,
                                              slacks_finalizer);
  Assignment* const first_solution = solver_->MakeAssignment();
  first_solution->Add(start_cumuls);
  for (IntVar* const cumul : start_cumuls) {
    first_solution->SetValue(cumul, cumul->Min());
  }
  DecisionBuilder* const finalizer =
      solver_->MakeLocalSearchPhase(first_solution, parameters);
  return finalizer;
}
}  // namespace operations_research

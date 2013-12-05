// Copyright 2010-2013 Google
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

#include "constraint_solver/routing.h"

#include "constraint_solver/constraint_solveri.h"

namespace operations_research {

// IntVarFilteredDecisionBuilder

IntVarFilteredDecisionBuilder::IntVarFilteredDecisionBuilder(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<LocalSearchFilter*>& filters)
    : vars_(vars),
      assignment_(solver->MakeAssignment()),
      delta_(solver->MakeAssignment()),
      empty_(solver->MakeAssignment()),
      filters_(filters) {
  assignment_->MutableIntVarContainer()->Resize(vars_.size());
  delta_indices_.reserve(vars_.size());
}

Decision* IntVarFilteredDecisionBuilder::Next(Solver* solver) {
  SetValuesFromDomains();
  if (BuildSolution()) {
    assignment_->Restore();
  } else {
    solver->Fail();
  }
  return nullptr;
}

bool IntVarFilteredDecisionBuilder::Commit() {
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
  }
  delta_->Clear();
  delta_indices_.clear();
  return accept;
}

void IntVarFilteredDecisionBuilder::SetValuesFromDomains() {
  Assignment::IntContainer* const container =
      assignment_->MutableIntVarContainer();
  for (int index = 0; index < vars_.size(); ++index) {
    IntVar* const var = vars_[index];
    if (var->Bound()) {
      container->AddAtPosition(var, index)->SetValue(var->Min());
    }
  }
}

void IntVarFilteredDecisionBuilder::SynchronizeFilters() {
  for (int i = 0; i < filters_.size(); ++i) {
    filters_[i]->Synchronize(assignment_);
  }
}

bool IntVarFilteredDecisionBuilder::FilterAccept() {
  // All incremental filters must be called.
  bool ok = true;
  for (int i = 0; i < filters_.size(); ++i) {
    if (filters_[i]->IsIncremental() || ok) {
      ok = filters_[i]->Accept(delta_, empty_) && ok;
    }
  }
  return ok;
}

// RoutingFilteredDecisionBuilder

RoutingFilteredDecisionBuilder::RoutingFilteredDecisionBuilder(
    RoutingModel* model, const std::vector<LocalSearchFilter*>& filters)
    : IntVarFilteredDecisionBuilder(model->solver(), model->Nexts(), filters),
      model_(model) {}

bool RoutingFilteredDecisionBuilder::InitializeRoutes() {
  // TODO(user): Handle chains to end nodes properly.
  start_chain_ends_.clear();
  start_chain_ends_.reserve(model()->vehicles());
  for (int vehicle = 0; vehicle < model()->vehicles(); ++vehicle) {
    int64 start = model()->Start(vehicle);
    const int64 end = model()->End(vehicle);
    while (Contains(start)) {
      start = Value(start);
      if (model()->IsEnd(start)) {
        if (start != end) {
          return false;
        }
        break;
      }
    }
    start_chain_ends_.push_back(start);
    if (start != end) {
      SetValue(start, end);
    }
  }
  return Commit();
}

void RoutingFilteredDecisionBuilder::MakeDisjunctionNodesUnperformed(int node) {
  std::vector<int> alternates;
  model()->GetDisjunctionIndicesFromIndex(node, &alternates);
  for (int alternate_index = 0; alternate_index < alternates.size();
       ++alternate_index) {
    const int alternate = alternates[alternate_index];
    if (node != alternate) {
      SetValue(alternate, alternate);
    }
  }
}

void RoutingFilteredDecisionBuilder::MakeUnassignedNodesUnperformed() {
  for (int index = 0; index < Size(); ++index) {
    if (!Contains(index)) {
      SetValue(index, index);
    }
  }
}

// GlobalCheapestInsertionFilteredDecisionBuilder

GlobalCheapestInsertionFilteredDecisionBuilder::
    GlobalCheapestInsertionFilteredDecisionBuilder(
        RoutingModel* model, ResultCallback2<int64, int64, int64>* evaluator,
        const std::vector<LocalSearchFilter*>& filters)
    : RoutingFilteredDecisionBuilder(model, filters), evaluator_(evaluator) {
  evaluator_->CheckIsRepeatable();
}

bool GlobalCheapestInsertionFilteredDecisionBuilder::BuildSolution() {
  if (!InitializeRoutes()) {
    return false;
  }
  // Node insertions currently being considered.
  std::vector<std::pair<int, int>> insertions;
  bool found = true;
  while (found) {
    found = false;
    ComputeEvaluatorSortedInsertions(&insertions);
    for (int index = 0; index < insertions.size(); ++index) {
      const int node = insertions[index].first;
      const int insertion = insertions[index].second;
      const int insertion_next = Value(insertion);
      // Insert "node" after "insertion", and before "insertion_next".
      SetValue(insertion, node);
      SetValue(node, insertion_next);
      MakeDisjunctionNodesUnperformed(node);
      if (Commit()) {
        found = true;
        break;
      }
    }
  }
  MakeUnassignedNodesUnperformed();
  return Commit();
}

void GlobalCheapestInsertionFilteredDecisionBuilder::
    ComputeEvaluatorSortedInsertions(
        std::vector<std::pair<int, int>>* sorted_insertions) {
  CHECK(sorted_insertions != nullptr);
  sorted_insertions->clear();
  std::vector<std::pair<int64, std::pair<int, int>>> valued_insertions;
  for (int node = 0; node < model()->Size(); ++node) {
    if (Contains(node)) {
      continue;
    }
    for (int vehicle = 0; vehicle < model()->vehicles(); ++vehicle) {
      int64 insert_after = model()->Start(vehicle);
      while (!model()->IsEnd(insert_after)) {
        const int64 insert_before = Value(insert_after);
        valued_insertions.push_back(
            std::make_pair(evaluator_->Run(insert_after, node) +
                               evaluator_->Run(node, insert_before),
                           std::make_pair(node, insert_after)));
        insert_after = insert_before;
      }
    }
  }
  std::sort(valued_insertions.begin(), valued_insertions.end());
  sorted_insertions->reserve(valued_insertions.size());
  for (int i = 0; i < valued_insertions.size(); ++i) {
    sorted_insertions->push_back(valued_insertions[i].second);
  }
}

// LocalCheapestInsertionFilteredDecisionBuilder

LocalCheapestInsertionFilteredDecisionBuilder::
    LocalCheapestInsertionFilteredDecisionBuilder(
        RoutingModel* model, ResultCallback2<int64, int64, int64>* evaluator,
        const std::vector<LocalSearchFilter*>& filters)
    : RoutingFilteredDecisionBuilder(model, filters), evaluator_(evaluator) {
  evaluator_->CheckIsRepeatable();
}

bool LocalCheapestInsertionFilteredDecisionBuilder::BuildSolution() {
  if (!InitializeRoutes()) {
    return false;
  }
  // Neighbors of the node currently being inserted.
  std::vector<int> neighbors;
  for (int node = 0; node < model()->Size(); ++node) {
    if (Contains(node)) {
      continue;
    }
    ComputeEvaluatorSortedNeighbors(node, &neighbors);
    for (int index = 0; index < neighbors.size(); ++index) {
      const int insertion = neighbors[index];
      const int insertion_next = Value(insertion);
      // Insert "node" after "insertion", and before "insertion_next".
      SetValue(insertion, node);
      SetValue(node, insertion_next);
      MakeDisjunctionNodesUnperformed(node);
      if (Commit()) {
        break;
      }
    }
  }
  MakeUnassignedNodesUnperformed();
  return Commit();
}

void
LocalCheapestInsertionFilteredDecisionBuilder::ComputeEvaluatorSortedNeighbors(
    int index, std::vector<int>* sorted_neighbors) {
  CHECK(sorted_neighbors != nullptr);
  CHECK(!Contains(index));
  sorted_neighbors->clear();
  const int size = model()->Size();
  if (index < size) {
    std::vector<std::pair<int64, int>> valued_neighbors;
    for (int vehicle = 0; vehicle < model()->vehicles(); ++vehicle) {
      int64 insert_after = model()->Start(vehicle);
      while (!model()->IsEnd(insert_after)) {
        const int64 insert_before = Value(insert_after);
        valued_neighbors.push_back(
            std::make_pair(evaluator_->Run(insert_after, index) +
                               evaluator_->Run(index, insert_before),
                           insert_after));
        insert_after = insert_before;
      }
    }
    std::sort(valued_neighbors.begin(), valued_neighbors.end());
    sorted_neighbors->reserve(valued_neighbors.size());
    for (int i = 0; i < valued_neighbors.size(); ++i) {
      sorted_neighbors->push_back(valued_neighbors[i].second);
    }
  }
}

// CheapestAdditionFilteredDecisionBuilder

CheapestAdditionFilteredDecisionBuilder::
    CheapestAdditionFilteredDecisionBuilder(
        RoutingModel* model, ResultCallback2<int64, int64, int64>* evaluator,
        const std::vector<LocalSearchFilter*>& filters)
    : RoutingFilteredDecisionBuilder(model, filters), evaluator_(evaluator) {
  evaluator_->CheckIsRepeatable();
}

bool CheapestAdditionFilteredDecisionBuilder::BuildSolution() {
  if (!InitializeRoutes()) {
    return false;
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
  std::vector<int> neighbors;
  for (int vehicle_index = 0; vehicle_index < sorted_vehicles.size();
       ++vehicle_index) {
    const int vehicle = sorted_vehicles[vehicle_index];
    int64 index = GetStartChainEnd(vehicle);
    const int64 end = model()->End(vehicle);
    bool found = true;
    // Extend the route of the current vehicle while it's possible.
    while (found && !model()->IsEnd(index)) {
      found = false;
      ComputeEvaluatorSortedNeighbors(index, &neighbors);
      for (int next_index = 0; next_index < neighbors.size(); ++next_index) {
        const int next = neighbors[next_index];
        if (model()->IsEnd(next) && next != end) {
          continue;
        }
        // Insert "next" after "index", and before "end" if it is not the end
        // already.
        SetValue(index, next);
        if (!model()->IsEnd(next)) {
          SetValue(next, end);
          MakeDisjunctionNodesUnperformed(next);
        }
        if (Commit()) {
          index = next;
          found = true;
          break;
        }
      }
    }
  }
  MakeUnassignedNodesUnperformed();
  return Commit();
}

void CheapestAdditionFilteredDecisionBuilder::ComputeEvaluatorSortedNeighbors(
    int index, std::vector<int>* sorted_neighbors) {
  CHECK(sorted_neighbors != nullptr);
  const std::vector<IntVar*>& nexts = model()->Nexts();
  sorted_neighbors->clear();
  const int size = model()->Size();
  if (index < size) {
    std::vector<std::pair<int64, int>> valued_neighbors;
    IntVar* const next = nexts[index];
    std::unique_ptr<IntVarIterator> it(next->MakeDomainIterator(false));
    for (it->Init(); it->Ok(); it->Next()) {
      const int value = it->Value();
      if (value != index && (value >= size || !Contains(value))) {
        // Tie-breaking on largest node index to mimic the behavior of
        // CheapestValueSelector (search.cc).
        valued_neighbors.push_back(
            std::make_pair(evaluator_->Run(index, value), -value));
      }
    }
    std::sort(valued_neighbors.begin(), valued_neighbors.end());
    sorted_neighbors->reserve(valued_neighbors.size());
    for (int i = 0; i < valued_neighbors.size(); ++i) {
      sorted_neighbors->push_back(-valued_neighbors[i].second);
    }
  }
}

bool CheapestAdditionFilteredDecisionBuilder::
    PartialRoutesAndLargeVehicleIndicesFirst::
    operator()(int vehicle1, int vehicle2) {
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
}  // namespace operations_research

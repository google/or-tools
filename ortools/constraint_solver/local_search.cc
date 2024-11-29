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

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/iterator_adaptors.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/strong_int.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/timer.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/graph/hamiltonian_path.h"
#include "ortools/util/bitset.h"
#include "ortools/util/saturated_arithmetic.h"

ABSL_FLAG(int, cp_local_search_sync_frequency, 16,
          "Frequency of checks for better solutions in the solution pool.");

ABSL_FLAG(int, cp_local_search_tsp_opt_size, 13,
          "Size of TSPs solved in the TSPOpt operator.");

ABSL_FLAG(int, cp_local_search_tsp_lns_size, 10,
          "Size of TSPs solved in the TSPLns operator.");

ABSL_FLAG(bool, cp_use_empty_path_symmetry_breaker, true,
          "If true, equivalent empty paths are removed from the neighborhood "
          "of PathOperators");

namespace operations_research {

// Utility methods to ensure the communication between local search and the
// search.

// Returns true if a local optimum has been reached and cannot be improved.
bool LocalOptimumReached(Search* search);

// Returns true if the search accepts the delta (actually checking this by
// calling AcceptDelta on the monitors of the search).
bool AcceptDelta(Search* search, Assignment* delta, Assignment* deltadelta);

// Notifies the search that a neighbor has been accepted by local search.
void AcceptNeighbor(Search* search);
void AcceptUncheckedNeighbor(Search* search);

// ----- Base operator class for operators manipulating IntVars -----

bool IntVarLocalSearchOperator::MakeNextNeighbor(Assignment* delta,
                                                 Assignment* deltadelta) {
  CHECK(delta != nullptr);
  VLOG(2) << DebugString() << "::MakeNextNeighbor(delta=("
          << delta->DebugString() << "), deltadelta=("
          << (deltadelta ? deltadelta->DebugString() : std::string("nullptr"))
          << "))";
  while (true) {
    RevertChanges(true);

    if (!MakeOneNeighbor()) {
      return false;
    }

    if (ApplyChanges(delta, deltadelta)) {
      VLOG(2) << "Delta (" << DebugString() << ") = " << delta->DebugString();
      return true;
    }
  }
  return false;
}
// TODO(user): Make this a pure virtual.
bool IntVarLocalSearchOperator::MakeOneNeighbor() { return true; }

// ----- Base Large Neighborhood Search operator -----

BaseLns::BaseLns(const std::vector<IntVar*>& vars)
    : IntVarLocalSearchOperator(vars) {}

BaseLns::~BaseLns() {}

bool BaseLns::MakeOneNeighbor() {
  fragment_.clear();
  if (NextFragment()) {
    for (int candidate : fragment_) {
      Deactivate(candidate);
    }
    return true;
  }
  return false;
}

void BaseLns::OnStart() { InitFragments(); }

void BaseLns::InitFragments() {}

void BaseLns::AppendToFragment(int index) {
  if (index >= 0 && index < Size()) {
    fragment_.push_back(index);
  }
}

int BaseLns::FragmentSize() const { return fragment_.size(); }

// ----- Simple Large Neighborhood Search operator -----

// Frees number_of_variables (contiguous in vars) variables.

namespace {
class SimpleLns : public BaseLns {
 public:
  SimpleLns(const std::vector<IntVar*>& vars, int number_of_variables)
      : BaseLns(vars), index_(0), number_of_variables_(number_of_variables) {
    CHECK_GT(number_of_variables_, 0);
  }
  ~SimpleLns() override {}
  void InitFragments() override { index_ = 0; }
  bool NextFragment() override;
  std::string DebugString() const override { return "SimpleLns"; }

 private:
  int index_;
  const int number_of_variables_;
};

bool SimpleLns::NextFragment() {
  const int size = Size();
  if (index_ < size) {
    for (int i = index_; i < index_ + number_of_variables_; ++i) {
      AppendToFragment(i % size);
    }
    ++index_;
    return true;
  }
  return false;
}

// ----- Random Large Neighborhood Search operator -----

// Frees up to number_of_variables random variables.

class RandomLns : public BaseLns {
 public:
  RandomLns(const std::vector<IntVar*>& vars, int number_of_variables,
            int32_t seed)
      : BaseLns(vars), rand_(seed), number_of_variables_(number_of_variables) {
    CHECK_GT(number_of_variables_, 0);
    CHECK_LE(number_of_variables_, Size());
  }
  ~RandomLns() override {}
  bool NextFragment() override;

  std::string DebugString() const override { return "RandomLns"; }

 private:
  std::mt19937 rand_;
  const int number_of_variables_;
};

bool RandomLns::NextFragment() {
  DCHECK_GT(Size(), 0);
  for (int i = 0; i < number_of_variables_; ++i) {
    AppendToFragment(absl::Uniform<int>(rand_, 0, Size()));
  }
  return true;
}
}  // namespace

LocalSearchOperator* Solver::MakeRandomLnsOperator(
    const std::vector<IntVar*>& vars, int number_of_variables) {
  return MakeRandomLnsOperator(vars, number_of_variables, CpRandomSeed());
}

LocalSearchOperator* Solver::MakeRandomLnsOperator(
    const std::vector<IntVar*>& vars, int number_of_variables, int32_t seed) {
  return RevAlloc(new RandomLns(vars, number_of_variables, seed));
}

// ----- Move Toward Target Local Search operator -----

// A local search operator that compares the current assignment with a target
// one, and that generates neighbors corresponding to a single variable being
// changed from its current value to its target value.
namespace {
class MoveTowardTargetLS : public IntVarLocalSearchOperator {
 public:
  MoveTowardTargetLS(const std::vector<IntVar*>& variables,
                     const std::vector<int64_t>& target_values)
      : IntVarLocalSearchOperator(variables),
        target_(target_values),
        // Initialize variable_index_ at the number of the of variables minus
        // one, so that the first to be tried (after one increment) is the one
        // of index 0.
        variable_index_(Size() - 1) {
    CHECK_EQ(target_values.size(), variables.size()) << "Illegal arguments.";
  }

  ~MoveTowardTargetLS() override {}

  std::string DebugString() const override { return "MoveTowardTargetLS"; }

 protected:
  // Make a neighbor assigning one variable to its target value.
  bool MakeOneNeighbor() override {
    while (num_var_since_last_start_ < Size()) {
      ++num_var_since_last_start_;
      variable_index_ = (variable_index_ + 1) % Size();
      const int64_t target_value = target_.at(variable_index_);
      const int64_t current_value = OldValue(variable_index_);
      if (current_value != target_value) {
        SetValue(variable_index_, target_value);
        return true;
      }
    }
    return false;
  }

 private:
  void OnStart() override {
    // Do not change the value of variable_index_: this way, we keep going from
    // where we last modified something. This is because we expect that most
    // often, the variables we have just checked are less likely to be able
    // to be changed to their target values than the ones we have not yet
    // checked.
    //
    // Consider the case where oddly indexed variables can be assigned to their
    // target values (no matter in what order they are considered), while even
    // indexed ones cannot. Restarting at index 0 each time an odd-indexed
    // variable is modified will cause a total of Theta(n^2) neighbors to be
    // generated, while not restarting will produce only Theta(n) neighbors.
    CHECK_GE(variable_index_, 0);
    CHECK_LT(variable_index_, Size());
    num_var_since_last_start_ = 0;
  }

  // Target values
  const std::vector<int64_t> target_;

  // Index of the next variable to try to restore
  int64_t variable_index_;

  // Number of variables checked since the last call to OnStart().
  int64_t num_var_since_last_start_;
};
}  // namespace

LocalSearchOperator* Solver::MakeMoveTowardTargetOperator(
    const Assignment& target) {
  typedef std::vector<IntVarElement> Elements;
  const Elements& elements = target.IntVarContainer().elements();
  // Copy target values and construct the vector of variables
  std::vector<IntVar*> vars;
  std::vector<int64_t> values;
  vars.reserve(target.NumIntVars());
  values.reserve(target.NumIntVars());
  for (const auto& it : elements) {
    vars.push_back(it.Var());
    values.push_back(it.Value());
  }
  return MakeMoveTowardTargetOperator(vars, values);
}

LocalSearchOperator* Solver::MakeMoveTowardTargetOperator(
    const std::vector<IntVar*>& variables,
    const std::vector<int64_t>& target_values) {
  return RevAlloc(new MoveTowardTargetLS(variables, target_values));
}

// ----- ChangeValue Operators -----

ChangeValue::ChangeValue(const std::vector<IntVar*>& vars)
    : IntVarLocalSearchOperator(vars), index_(0) {}

ChangeValue::~ChangeValue() {}

bool ChangeValue::MakeOneNeighbor() {
  const int size = Size();
  while (index_ < size) {
    const int64_t value = ModifyValue(index_, Value(index_));
    SetValue(index_, value);
    ++index_;
    return true;
  }
  return false;
}

void ChangeValue::OnStart() { index_ = 0; }

// Increments the current value of variables.

namespace {
class IncrementValue : public ChangeValue {
 public:
  explicit IncrementValue(const std::vector<IntVar*>& vars)
      : ChangeValue(vars) {}
  ~IncrementValue() override {}
  int64_t ModifyValue(int64_t, int64_t value) override { return value + 1; }

  std::string DebugString() const override { return "IncrementValue"; }
};

// Decrements the current value of variables.

class DecrementValue : public ChangeValue {
 public:
  explicit DecrementValue(const std::vector<IntVar*>& vars)
      : ChangeValue(vars) {}
  ~DecrementValue() override {}
  int64_t ModifyValue(int64_t, int64_t value) override { return value - 1; }

  std::string DebugString() const override { return "DecrementValue"; }
};
}  // namespace

// ----- Path-based Operators -----

PathOperator::PathOperator(const std::vector<IntVar*>& next_vars,
                           const std::vector<IntVar*>& path_vars,
                           IterationParameters iteration_parameters)
    : IntVarLocalSearchOperator(next_vars, true),
      number_of_nexts_(next_vars.size()),
      ignore_path_vars_(path_vars.empty()),
      base_nodes_(iteration_parameters.number_of_base_nodes),
      base_alternatives_(iteration_parameters.number_of_base_nodes),
      base_sibling_alternatives_(iteration_parameters.number_of_base_nodes),
      end_nodes_(iteration_parameters.number_of_base_nodes),
      base_paths_(iteration_parameters.number_of_base_nodes),
      node_path_starts_(number_of_nexts_, -1),
      node_path_ends_(number_of_nexts_, -1),
      calls_per_base_node_(iteration_parameters.number_of_base_nodes, 0),
      just_started_(false),
      first_start_(true),
      next_base_to_increment_(iteration_parameters.number_of_base_nodes),
      iteration_parameters_(std::move(iteration_parameters)),
      optimal_paths_enabled_(false),
      active_paths_(number_of_nexts_),
      alternative_index_(next_vars.size(), -1) {
  DCHECK_GT(iteration_parameters_.number_of_base_nodes, 0);
  if (!ignore_path_vars_) {
    AddVars(path_vars);
  }
  path_basis_.push_back(0);
  for (int i = 1; i < base_nodes_.size(); ++i) {
    if (!OnSamePathAsPreviousBase(i)) path_basis_.push_back(i);
  }
  if ((path_basis_.size() > 2) ||
      (!next_vars.empty() && !next_vars.back()
                                  ->solver()
                                  ->parameters()
                                  .skip_locally_optimal_paths())) {
    iteration_parameters_.skip_locally_optimal_paths = false;
  }
}

void PathOperator::EnterSearch() {
  first_start_ = true;
  ResetIncrementalism();
}

void PathOperator::Reset() {
  active_paths_.Clear();
  ResetIncrementalism();
}

void PathOperator::OnStart() {
  optimal_paths_enabled_ = false;
  InitializeBaseNodes();
  InitializeAlternatives();
  OnNodeInitialization();
}

bool PathOperator::MakeOneNeighbor() {
  while (IncrementPosition()) {
    // Need to revert changes here since MakeNeighbor might have returned false
    // and have done changes in the previous iteration.
    RevertChanges(true);
    if (MakeNeighbor()) {
      return true;
    }
  }
  return false;
}

bool PathOperator::SkipUnchanged(int index) const {
  if (ignore_path_vars_) {
    return true;
  }
  if (index < number_of_nexts_) {
    int path_index = index + number_of_nexts_;
    return Value(path_index) == OldValue(path_index);
  }
  int next_index = index - number_of_nexts_;
  return Value(next_index) == OldValue(next_index);
}

bool PathOperator::MoveChain(int64_t before_chain, int64_t chain_end,
                             int64_t destination) {
  if (destination == before_chain || destination == chain_end) return false;
  DCHECK(CheckChainValidity(before_chain, chain_end, destination) &&
         !IsPathEnd(chain_end) && !IsPathEnd(destination));
  const int64_t destination_path = Path(destination);
  const int64_t after_chain = Next(chain_end);
  SetNext(chain_end, Next(destination), destination_path);
  if (!ignore_path_vars_) {
    int current = destination;
    int next = Next(before_chain);
    while (current != chain_end) {
      SetNext(current, next, destination_path);
      current = next;
      next = Next(next);
    }
  } else {
    SetNext(destination, Next(before_chain), destination_path);
  }
  SetNext(before_chain, after_chain, Path(before_chain));
  return true;
}

bool PathOperator::ReverseChain(int64_t before_chain, int64_t after_chain,
                                int64_t* chain_last) {
  if (CheckChainValidity(before_chain, after_chain, -1)) {
    int64_t path = Path(before_chain);
    int64_t current = Next(before_chain);
    if (current == after_chain) {
      return false;
    }
    int64_t current_next = Next(current);
    SetNext(current, after_chain, path);
    while (current_next != after_chain) {
      const int64_t next = Next(current_next);
      SetNext(current_next, current, path);
      current = current_next;
      current_next = next;
    }
    SetNext(before_chain, current, path);
    *chain_last = current;
    return true;
  }
  return false;
}

bool PathOperator::MakeActive(int64_t node, int64_t destination) {
  if (!IsPathEnd(destination)) {
    int64_t destination_path = Path(destination);
    SetNext(node, Next(destination), destination_path);
    SetNext(destination, node, destination_path);
    return true;
  }
  return false;
}

bool PathOperator::MakeChainInactive(int64_t before_chain, int64_t chain_end) {
  const int64_t kNoPath = -1;
  if (CheckChainValidity(before_chain, chain_end, -1) &&
      !IsPathEnd(chain_end)) {
    const int64_t after_chain = Next(chain_end);
    int64_t current = Next(before_chain);
    while (current != after_chain) {
      const int64_t next = Next(current);
      SetNext(current, current, kNoPath);
      current = next;
    }
    SetNext(before_chain, after_chain, Path(before_chain));
    return true;
  }
  return false;
}

bool PathOperator::SwapActiveAndInactive(int64_t active, int64_t inactive) {
  if (active == inactive) return false;
  const int64_t prev = Prev(active);
  return MakeChainInactive(prev, active) && MakeActive(inactive, prev);
}

bool PathOperator::SwapActiveAndInactiveChains(
    absl::Span<const int64_t> active_chain,
    absl::Span<const int64_t> inactive_chain) {
  if (active_chain.empty()) return false;
  if (active_chain == inactive_chain) return false;
  const int before_active_chain = Prev(active_chain.front());
  if (!MakeChainInactive(before_active_chain, active_chain.back())) {
    return false;
  }
  for (auto it = inactive_chain.crbegin(); it != inactive_chain.crend(); ++it) {
    if (!MakeActive(*it, before_active_chain)) return false;
  }
  return true;
}

bool PathOperator::IncrementPosition() {
  const int base_node_size = iteration_parameters_.number_of_base_nodes;

  if (just_started_) {
    just_started_ = false;
    return true;
  }
  const int number_of_paths = path_starts_.size();
  // Finding next base node positions.
  // Increment the position of inner base nodes first (higher index nodes);
  // if a base node is at the end of a path, reposition it at the start
  // of the path and increment the position of the preceding base node (this
  // action is called a restart).
  int last_restarted = base_node_size;
  for (int i = base_node_size - 1; i >= 0; --i) {
    if (base_nodes_[i] < number_of_nexts_ && i <= next_base_to_increment_) {
      if (ConsiderAlternatives(i)) {
        // Iterate on sibling alternatives.
        const int sibling_alternative_index =
            GetSiblingAlternativeIndex(base_nodes_[i]);
        if (sibling_alternative_index >= 0) {
          if (base_sibling_alternatives_[i] <
              alternative_sets_[sibling_alternative_index].size() - 1) {
            ++base_sibling_alternatives_[i];
            break;
          }
          base_sibling_alternatives_[i] = 0;
        }
        // Iterate on base alternatives.
        const int alternative_index = alternative_index_[base_nodes_[i]];
        if (alternative_index >= 0) {
          if (base_alternatives_[i] <
              alternative_sets_[alternative_index].size() - 1) {
            ++base_alternatives_[i];
            break;
          }
          base_alternatives_[i] = 0;
          base_sibling_alternatives_[i] = 0;
        }
      }
      if (HasNeighbors()) {
        const int64_t base_node = BaseNode(i);
        const int64_t start_node = StartNode(i);
        const int num_incoming_neighbors =
            IsPathStart(base_node) ||
                    iteration_parameters_.get_incoming_neighbors == nullptr
                ? 0
                : iteration_parameters_
                      .get_incoming_neighbors(base_node, start_node)
                      .size();
        const int num_outgoing_neighbors =
            IsPathEnd(base_node) ||
                    iteration_parameters_.get_outgoing_neighbors == nullptr
                ? 0
                : iteration_parameters_
                      .get_outgoing_neighbors(base_node, start_node)
                      .size();
        if (++calls_per_base_node_[i] <
            num_incoming_neighbors + num_outgoing_neighbors) {
          break;
        }
      }
      calls_per_base_node_[i] = 0;
      base_alternatives_[i] = 0;
      base_sibling_alternatives_[i] = 0;
      base_nodes_[i] = OldNext(base_nodes_[i]);
      if (iteration_parameters_.accept_path_end_base ||
          !IsPathEnd(base_nodes_[i])) {
        break;
      }
    }
    calls_per_base_node_[i] = 0;
    base_alternatives_[i] = 0;
    base_sibling_alternatives_[i] = 0;
    base_nodes_[i] = StartNode(i);
    last_restarted = i;
  }
  next_base_to_increment_ = base_node_size;
  // At the end of the loop, base nodes with indexes in
  // [last_restarted, base_node_size[ have been restarted.
  // Restarted base nodes are then repositioned by the virtual
  // GetBaseNodeRestartPosition to reflect position constraints between
  // base nodes (by default GetBaseNodeRestartPosition leaves the nodes
  // at the start of the path).
  // Base nodes are repositioned in ascending order to ensure that all
  // base nodes "below" the node being repositioned have their final
  // position.
  for (int i = last_restarted; i < base_node_size; ++i) {
    calls_per_base_node_[i] = 0;
    base_alternatives_[i] = 0;
    base_sibling_alternatives_[i] = 0;
    base_nodes_[i] = GetBaseNodeRestartPosition(i);
  }
  if (last_restarted > 0) {
    return CheckEnds();
  }
  // If all base nodes have been restarted, base nodes are moved to new paths.
  // First we mark the current paths as locally optimal if they have been
  // completely explored.
  if (optimal_paths_enabled_ &&
      iteration_parameters_.skip_locally_optimal_paths) {
    if (path_basis_.size() > 1) {
      for (int i = 1; i < path_basis_.size(); ++i) {
        active_paths_.DeactivatePathPair(StartNode(path_basis_[i - 1]),
                                         StartNode(path_basis_[i]));
      }
    } else {
      active_paths_.DeactivatePathPair(StartNode(path_basis_[0]),
                                       StartNode(path_basis_[0]));
    }
  }
  std::vector<int> current_starts(base_node_size);
  for (int i = 0; i < base_node_size; ++i) {
    current_starts[i] = StartNode(i);
  }
  // Exploration of next paths can lead to locally optimal paths since we are
  // exploring them from scratch.
  optimal_paths_enabled_ = true;
  while (true) {
    for (int i = base_node_size - 1; i >= 0; --i) {
      const int next_path_index = base_paths_[i] + 1;
      if (next_path_index < number_of_paths) {
        base_paths_[i] = next_path_index;
        calls_per_base_node_[i] = 0;
        base_alternatives_[i] = 0;
        base_sibling_alternatives_[i] = 0;
        base_nodes_[i] = path_starts_[next_path_index];
        if (i == 0 || !OnSamePathAsPreviousBase(i)) {
          break;
        }
      } else {
        base_paths_[i] = 0;
        calls_per_base_node_[i] = 0;
        base_alternatives_[i] = 0;
        base_sibling_alternatives_[i] = 0;
        base_nodes_[i] = path_starts_[0];
      }
    }
    if (!iteration_parameters_.skip_locally_optimal_paths) return CheckEnds();
    // If the new paths have already been completely explored, we can
    // skip them from now on.
    if (path_basis_.size() > 1) {
      for (int j = 1; j < path_basis_.size(); ++j) {
        if (active_paths_.IsPathPairActive(StartNode(path_basis_[j - 1]),
                                           StartNode(path_basis_[j]))) {
          return CheckEnds();
        }
      }
    } else {
      if (active_paths_.IsPathPairActive(StartNode(path_basis_[0]),
                                         StartNode(path_basis_[0]))) {
        return CheckEnds();
      }
    }
    // If we are back to paths we just iterated on or have reached the end
    // of the neighborhood search space, we can stop.
    if (!CheckEnds()) return false;
    bool stop = true;
    for (int i = 0; i < base_node_size; ++i) {
      if (StartNode(i) != current_starts[i]) {
        stop = false;
        break;
      }
    }
    if (stop) return false;
  }
  return CheckEnds();
}

void PathOperator::InitializePathStarts() {
  // Detect nodes which do not have any possible predecessor in a path; these
  // nodes are path starts.
  int max_next = -1;
  std::vector<bool> has_prevs(number_of_nexts_, false);
  for (int i = 0; i < number_of_nexts_; ++i) {
    const int next = OldNext(i);
    if (next < number_of_nexts_) {
      has_prevs[next] = true;
    }
    max_next = std::max(max_next, next);
  }
  // Update locally optimal paths.
  if (iteration_parameters_.skip_locally_optimal_paths) {
    active_paths_.Initialize(
        /*is_start=*/[&has_prevs](int node) { return !has_prevs[node]; });
    for (int i = 0; i < number_of_nexts_; ++i) {
      if (!has_prevs[i]) {
        int current = i;
        while (!IsPathEnd(current)) {
          if ((OldNext(current) != PrevNext(current))) {
            active_paths_.ActivatePath(i);
            break;
          }
          current = OldNext(current);
        }
      }
    }
  }
  // Create a list of path starts, dropping equivalent path starts of
  // currently empty paths.
  std::vector<bool> empty_found(number_of_nexts_, false);
  std::vector<int64_t> new_path_starts;
  const bool use_empty_path_symmetry_breaker =
      absl::GetFlag(FLAGS_cp_use_empty_path_symmetry_breaker);
  for (int i = 0; i < number_of_nexts_; ++i) {
    if (!has_prevs[i]) {
      if (use_empty_path_symmetry_breaker && IsPathEnd(OldNext(i))) {
        if (iteration_parameters_.start_empty_path_class != nullptr) {
          if (empty_found[iteration_parameters_.start_empty_path_class(i)])
            continue;
          empty_found[iteration_parameters_.start_empty_path_class(i)] = true;
        }
      }
      new_path_starts.push_back(i);
    }
  }
  if (!first_start_) {
    // Synchronizing base_paths_ with base node positions. When the last move
    // was performed a base node could have been moved to a new route in which
    // case base_paths_ needs to be updated. This needs to be done on the path
    // starts before we re-adjust base nodes for new path starts.
    std::vector<int> node_paths(max_next + 1, -1);
    for (int i = 0; i < path_starts_.size(); ++i) {
      int node = path_starts_[i];
      while (!IsPathEnd(node)) {
        node_paths[node] = i;
        node = OldNext(node);
      }
      node_paths[node] = i;
    }
    for (int j = 0; j < iteration_parameters_.number_of_base_nodes; ++j) {
      // Always restart from first alternative.
      calls_per_base_node_[j] = 0;
      base_alternatives_[j] = 0;
      base_sibling_alternatives_[j] = 0;
      if (IsInactive(base_nodes_[j]) || node_paths[base_nodes_[j]] == -1) {
        // Base node was made inactive or was moved to a new path, reposition
        // the base node to its restart position.
        base_nodes_[j] = GetBaseNodeRestartPosition(j);
        base_paths_[j] = node_paths[base_nodes_[j]];
      } else {
        base_paths_[j] = node_paths[base_nodes_[j]];
      }
    }
    // Re-adjust current base_nodes and base_paths to take into account new
    // path starts (there could be fewer if a new path was made empty, or more
    // if nodes were added to a formerly empty path).
    int new_index = 0;
    absl::flat_hash_set<int> found_bases;
    for (int i = 0; i < path_starts_.size(); ++i) {
      int index = new_index;
      // Note: old and new path starts are sorted by construction.
      while (index < new_path_starts.size() &&
             new_path_starts[index] < path_starts_[i]) {
        ++index;
      }
      const bool found = (index < new_path_starts.size() &&
                          new_path_starts[index] == path_starts_[i]);
      if (found) {
        new_index = index;
      }
      for (int j = 0; j < iteration_parameters_.number_of_base_nodes; ++j) {
        if (base_paths_[j] == i && !found_bases.contains(j)) {
          found_bases.insert(j);
          base_paths_[j] = new_index;
          // If the current position of the base node is a removed empty path,
          // readjusting it to the last visited path start.
          if (!found) {
            base_nodes_[j] = new_path_starts[new_index];
          }
        }
      }
    }
  }
  path_starts_.swap(new_path_starts);
  // For every base path, store the end corresponding to the path start.
  // TODO(user): make this faster, maybe by pairing starts with ends.
  path_ends_.clear();
  path_ends_.reserve(path_starts_.size());
  int64_t max_node_index = number_of_nexts_ - 1;
  for (const int start_node : path_starts_) {
    int64_t node = start_node;
    while (!IsPathEnd(node)) node = OldNext(node);
    path_ends_.push_back(node);
    max_node_index = std::max(max_node_index, node);
  }
  node_path_starts_.assign(max_node_index + 1, -1);
  node_path_ends_.assign(max_node_index + 1, -1);
  for (int i = 0; i < path_starts_.size(); ++i) {
    const int64_t start_node = path_starts_[i];
    const int64_t end_node = path_ends_[i];
    int64_t node = start_node;
    while (!IsPathEnd(node)) {
      node_path_starts_[node] = start_node;
      node_path_ends_[node] = end_node;
      node = OldNext(node);
    }
    node_path_starts_[node] = start_node;
    node_path_ends_[node] = end_node;
  }
}

void PathOperator::InitializeInactives() {
  inactives_.clear();
  for (int i = 0; i < number_of_nexts_; ++i) {
    inactives_.push_back(OldNext(i) == i);
  }
}

void PathOperator::InitializeBaseNodes() {
  // Inactive nodes must be detected before determining new path starts.
  InitializeInactives();
  InitializePathStarts();
  if (first_start_ || InitPosition()) {
    // Only do this once since the following starts will continue from the
    // preceding position
    for (int i = 0; i < iteration_parameters_.number_of_base_nodes; ++i) {
      base_paths_[i] = 0;
      base_nodes_[i] = path_starts_[0];
    }
    first_start_ = false;
  }
  for (int i = 0; i < iteration_parameters_.number_of_base_nodes; ++i) {
    // If base node has been made inactive, restart from path start.
    int64_t base_node = base_nodes_[i];
    if (RestartAtPathStartOnSynchronize() || IsInactive(base_node)) {
      base_node = path_starts_[base_paths_[i]];
      base_nodes_[i] = base_node;
    }
    end_nodes_[i] = base_node;
  }
  // Repair end_nodes_ in case some must be on the same path and are not anymore
  // (due to other operators moving these nodes).
  for (int i = 1; i < iteration_parameters_.number_of_base_nodes; ++i) {
    if (OnSamePathAsPreviousBase(i) &&
        !OnSamePath(base_nodes_[i - 1], base_nodes_[i])) {
      const int64_t base_node = base_nodes_[i - 1];
      base_nodes_[i] = base_node;
      end_nodes_[i] = base_node;
      base_paths_[i] = base_paths_[i - 1];
    }
  }
  for (int i = 0; i < iteration_parameters_.number_of_base_nodes; ++i) {
    base_alternatives_[i] = 0;
    base_sibling_alternatives_[i] = 0;
    calls_per_base_node_[i] = 0;
  }
  just_started_ = true;
}

void PathOperator::InitializeAlternatives() {
  active_in_alternative_set_.resize(alternative_sets_.size(), -1);
  for (int i = 0; i < alternative_sets_.size(); ++i) {
    const int64_t current_active = active_in_alternative_set_[i];
    if (current_active >= 0 && !IsInactive(current_active)) continue;
    for (int64_t index : alternative_sets_[i]) {
      if (!IsInactive(index)) {
        active_in_alternative_set_[i] = index;
        break;
      }
    }
  }
}

bool PathOperator::OnSamePath(int64_t node1, int64_t node2) const {
  if (IsInactive(node1) != IsInactive(node2)) {
    return false;
  }
  for (int node = node1; !IsPathEnd(node); node = OldNext(node)) {
    if (node == node2) {
      return true;
    }
  }
  for (int node = node2; !IsPathEnd(node); node = OldNext(node)) {
    if (node == node1) {
      return true;
    }
  }
  return false;
}

// Rejects chain if chain_end is not after before_chain on the path or if
// the chain contains exclude. Given before_chain is the node before the
// chain, if before_chain and chain_end are the same the chain is rejected too.
// Also rejects cycles (cycle detection is detected through chain length
//  overflow).
bool PathOperator::CheckChainValidity(int64_t before_chain, int64_t chain_end,
                                      int64_t exclude) const {
  if (before_chain == chain_end || before_chain == exclude) return false;
  int64_t current = before_chain;
  int chain_size = 0;
  while (current != chain_end) {
    if (chain_size > number_of_nexts_) {
      return false;
    }
    if (IsPathEnd(current)) {
      return false;
    }
    current = Next(current);
    ++chain_size;
    if (current == exclude) {
      return false;
    }
  }
  return true;
}

// ----- 2Opt -----

// Reverses a sub-chain of a path. It is called 2Opt because it breaks
// 2 arcs on the path; resulting paths are called 2-optimal.
// Possible neighbors for the path 1 -> 2 -> 3 -> 4 -> 5
// (where (1, 5) are first and last nodes of the path and can therefore not be
// moved):
// 1 -> 3 -> 2 -> 4 -> 5
// 1 -> 4 -> 3 -> 2 -> 5
// 1 -> 2 -> 4 -> 3 -> 5

using NeighborAccessor =
    std::function<const std::vector<int>&(/*node=*/int, /*start_node=*/int)>;
class TwoOpt : public PathOperator {
 public:
  TwoOpt(const std::vector<IntVar*>& vars,
         const std::vector<IntVar*>& secondary_vars,
         std::function<int(int64_t)> start_empty_path_class,
         NeighborAccessor get_incoming_neighbors = nullptr,
         NeighborAccessor get_outgoing_neighbors = nullptr)
      : PathOperator(vars, secondary_vars,
                     (get_incoming_neighbors == nullptr &&
                      get_outgoing_neighbors == nullptr)
                         ? 2
                         : 1,
                     /*skip_locally_optimal_paths=*/true,
                     /*accept_path_end_base=*/true,
                     std::move(start_empty_path_class),
                     std::move(get_incoming_neighbors),
                     std::move(get_outgoing_neighbors)),
        last_base_(-1),
        last_(-1) {}
  ~TwoOpt() override {}
  bool MakeNeighbor() override;
  bool IsIncremental() const override { return true; }

  std::string DebugString() const override { return "TwoOpt"; }

 protected:
  void ResetIncrementalism() override { last_ = -1; }
  bool OnSamePathAsPreviousBase(int64_t /*base_index*/) override {
    // Both base nodes have to be on the same path.
    return true;
  }
  int64_t GetBaseNodeRestartPosition(int base_index) override {
    return (base_index == 0) ? StartNode(0) : BaseNode(0);
  }

 private:
  void OnNodeInitialization() override { last_ = -1; }

  int64_t last_base_;
  int64_t last_;
};

bool TwoOpt::MakeNeighbor() {
  const int64_t node0 = BaseNode(0);
  int64_t before_chain = node0;
  int64_t after_chain = -1;
  if (HasNeighbors()) {
    const auto [neighbor, outgoing] = GetNeighborForBaseNode(0);
    if (neighbor < 0 || IsInactive(neighbor)) return false;
    if (CurrentNodePathStart(node0) != CurrentNodePathStart(neighbor)) {
      return false;
    }
    if (outgoing) {
      if (IsPathEnd(neighbor)) return false;
      // Reverse the chain starting *after" node0 and ending with 'neighbor'.
      after_chain = Next(neighbor);
    } else {
      if (IsPathStart(neighbor)) return false;
      // Reverse the chain starting with 'neighbor' and ending before node0.
      before_chain = Prev(neighbor);
      after_chain = node0;
    }
  } else {
    DCHECK_EQ(StartNode(0), StartNode(1));
    after_chain = BaseNode(1);
  }
  // Incrementality is disabled with neighbors.
  if (last_base_ != node0 || last_ == -1 || HasNeighbors()) {
    RevertChanges(false);
    if (IsPathEnd(node0)) {
      last_ = -1;
      return false;
    }
    last_base_ = node0;
    last_ = Next(before_chain);
    int64_t chain_last;
    if (ReverseChain(before_chain, after_chain, &chain_last)
        // Check there are more than one node in the chain (reversing a
        // single node is a NOP).
        && last_ != chain_last) {
      return true;
    }
    last_ = -1;
    return false;
  }
  DCHECK_EQ(before_chain, node0);
  const int64_t to_move = Next(last_);
  DCHECK_EQ(Next(to_move), after_chain);
  return MoveChain(last_, to_move, before_chain);
}

// ----- Relocate -----

// Moves a sub-chain of a path to another position; the specified chain length
// is the fixed length of the chains being moved. When this length is 1 the
// operator simply moves a node to another position.
// Possible neighbors for the path 1 -> 2 -> 3 -> 4 -> 5, for a chain length
// of 2 (where (1, 5) are first and last nodes of the path and can
// therefore not be moved):
// 1 -> 4 -> 2 -> 3 -> 5
// 1 -> 3 -> 4 -> 2 -> 5
//
// Using Relocate with chain lengths of 1, 2 and 3 together is equivalent to
// the OrOpt operator on a path. The OrOpt operator is a limited version of
// 3Opt (breaks 3 arcs on a path).

class Relocate : public PathOperator {
 public:
  Relocate(const std::vector<IntVar*>& vars,
           const std::vector<IntVar*>& secondary_vars, const std::string& name,
           std::function<int(int64_t)> start_empty_path_class,
           NeighborAccessor get_incoming_neighbors,
           NeighborAccessor get_outgoing_neighbors, int64_t chain_length = 1LL,
           bool single_path = false)
      : PathOperator(
            vars, secondary_vars,
            (get_incoming_neighbors == nullptr &&
             get_outgoing_neighbors == nullptr)
                ? 2
                : 1,
            /*skip_locally_optimal_paths=*/true,
            /*accept_path_end_base=*/false, std::move(start_empty_path_class),
            chain_length == 1 ? std::move(get_incoming_neighbors) : nullptr,
            std::move(get_outgoing_neighbors)),
        chain_length_(chain_length),
        single_path_(single_path),
        name_(name) {
    CHECK_GT(chain_length_, 0);
  }
  Relocate(const std::vector<IntVar*>& vars,
           const std::vector<IntVar*>& secondary_vars,
           std::function<int(int64_t)> start_empty_path_class,
           NeighborAccessor get_incoming_neighbors,
           NeighborAccessor get_outgoing_neighbors, int64_t chain_length = 1LL,
           bool single_path = false)
      : Relocate(vars, secondary_vars,
                 absl::StrCat("Relocate<", chain_length, ">"),
                 std::move(start_empty_path_class),
                 std::move(get_incoming_neighbors),
                 std::move(get_outgoing_neighbors), chain_length, single_path) {
  }
  Relocate(const std::vector<IntVar*>& vars,
           const std::vector<IntVar*>& secondary_vars,
           std::function<int(int64_t)> start_empty_path_class,
           int64_t chain_length = 1LL, bool single_path = false)
      : Relocate(vars, secondary_vars,
                 absl::StrCat("Relocate<", chain_length, ">"),
                 std::move(start_empty_path_class), nullptr, nullptr,
                 chain_length, single_path) {}
  ~Relocate() override {}
  bool MakeNeighbor() override;

  std::string DebugString() const override { return name_; }

 protected:
  bool OnSamePathAsPreviousBase(int64_t /*base_index*/) override {
    // Both base nodes have to be on the same path when it's the single path
    // version.
    return single_path_;
  }

 private:
  const int64_t chain_length_;
  const bool single_path_;
  const std::string name_;
};

bool Relocate::MakeNeighbor() {
  const auto do_move = [this](int64_t before_chain, int64_t destination) {
    DCHECK(!IsPathEnd(destination));
    int64_t chain_end = before_chain;
    for (int i = 0; i < chain_length_; ++i) {
      if (IsPathEnd(chain_end) || chain_end == destination) return false;
      chain_end = Next(chain_end);
    }
    return !IsPathEnd(chain_end) &&
           MoveChain(before_chain, chain_end, destination);
  };
  const int64_t node0 = BaseNode(0);
  if (HasNeighbors()) {
    const auto [neighbor, outgoing] = GetNeighborForBaseNode(0);
    if (neighbor < 0 || IsInactive(neighbor)) return false;
    if (outgoing) {
      return do_move(/*before_chain=*/Prev(neighbor),
                     /*destination=*/node0);
    }
    DCHECK_EQ(chain_length_, 1);
    // TODO(user): Handle chain_length_ > 1 for incoming neighbors by going
    // backwards on the chain. NOTE: In this setting it makes sense to have path
    // ends as base nodes as we move the chain "before" the base node.
    DCHECK(!IsPathStart(node0)) << "Path starts have no incoming neighbors.";
    return do_move(/*before_chain=*/Prev(neighbor),
                   /*destination=*/Prev(node0));
  }
  DCHECK(!single_path_ || StartNode(0) == StartNode(1));
  return do_move(/*before_chain=*/node0, /*destination=*/BaseNode(1));
}

// ----- Exchange -----

// Exchanges the positions of two nodes.
// Possible neighbors for the path 1 -> 2 -> 3 -> 4 -> 5
// (where (1, 5) are first and last nodes of the path and can therefore not
// be moved):
// 1 -> 3 -> 2 -> 4 -> 5
// 1 -> 4 -> 3 -> 2 -> 5
// 1 -> 2 -> 4 -> 3 -> 5

class Exchange : public PathOperator {
 public:
  Exchange(const std::vector<IntVar*>& vars,
           const std::vector<IntVar*>& secondary_vars,
           std::function<int(int64_t)> start_empty_path_class,
           NeighborAccessor get_incoming_neighbors = nullptr,
           NeighborAccessor get_outgoing_neighbors = nullptr)
      : PathOperator(vars, secondary_vars,
                     (get_incoming_neighbors == nullptr &&
                      get_outgoing_neighbors == nullptr)
                         ? 2
                         : 1,
                     /*skip_locally_optimal_paths=*/true,
                     /*accept_path_end_base=*/false,
                     std::move(start_empty_path_class),
                     std::move(get_incoming_neighbors),
                     std::move(get_outgoing_neighbors)) {}
  ~Exchange() override {}
  bool MakeNeighbor() override;

  std::string DebugString() const override { return "Exchange"; }
};

bool Exchange::MakeNeighbor() {
  const auto do_move = [this](int64_t node1, int64_t node2) {
    if (IsPathEnd(node1) || IsPathEnd(node2) || IsPathStart(node1) ||
        IsPathStart(node2)) {
      return false;
    }
    if (node1 == node2) return false;
    const int64_t prev_node1 = Prev(node1);
    const bool ok = MoveChain(prev_node1, node1, Prev(node2));
    return MoveChain(Prev(node2), node2, prev_node1) || ok;
  };
  const int64_t node0 = BaseNode(0);
  if (HasNeighbors()) {
    const auto [neighbor, outgoing] = GetNeighborForBaseNode(0);
    if (neighbor < 0 || IsInactive(neighbor)) return false;
    if (outgoing) {
      // Exchange node0's next with 'neighbor'.
      return do_move(Next(node0), neighbor);
    }
    DCHECK(!IsPathStart(node0)) << "Path starts have no incoming neighbors.";
    // Exchange node0's prev with 'neighbor'.
    return do_move(Prev(node0), neighbor);
  }
  return do_move(Next(node0), Next(BaseNode(1)));
}

// ----- Cross -----

// Cross echanges the starting chains of 2 paths, including exchanging the
// whole paths.
// First and last nodes are not moved.
// Possible neighbors for the paths 1 -> 2 -> 3 -> 4 -> 5 and 6 -> 7 -> 8
// (where (1, 5) and (6, 8) are first and last nodes of the paths and can
// therefore not be moved):
// 1 -> 7 -> 3 -> 4 -> 5  6 -> 2 -> 8
// 1 -> 7 -> 4 -> 5       6 -> 2 -> 3 -> 8
// 1 -> 7 -> 5            6 -> 2 -> 3 -> 4 -> 8

class Cross : public PathOperator {
 public:
  Cross(const std::vector<IntVar*>& vars,
        const std::vector<IntVar*>& secondary_vars,
        std::function<int(int64_t)> start_empty_path_class,
        NeighborAccessor get_incoming_neighbors = nullptr,
        NeighborAccessor get_outgoing_neighbors = nullptr)
      : PathOperator(
            vars, secondary_vars, get_outgoing_neighbors == nullptr ? 2 : 1,
            /*skip_locally_optimal_paths=*/true,
            /*accept_path_end_base=*/true, std::move(start_empty_path_class),
            std::move(get_incoming_neighbors),
            std::move(get_outgoing_neighbors)) {}
  ~Cross() override {}
  bool MakeNeighbor() override;

  std::string DebugString() const override { return "Cross"; }
};

bool Cross::MakeNeighbor() {
  const int64_t start0 = StartNode(0);
  int64_t start1 = -1;
  const int64_t node0 = BaseNode(0);
  int64_t node1 = -1;
  if (node0 == start0) return false;
  bool cross_path_starts = false;
  if (HasNeighbors()) {
    const auto [neighbor, outgoing] = GetNeighborForBaseNode(0);
    if (neighbor < 0) return false;
    cross_path_starts = outgoing;
    DCHECK(!IsPathStart(neighbor));
    if (IsInactive(neighbor)) return false;
    start1 = CurrentNodePathStart(neighbor);
    // Tricky: In all cases we want to connect node0 to neighbor. If we are
    // crossing path starts, node0 is the end of a chain and neighbor is the
    // the first node after the other chain ending at node1, therefore
    // node1 = prev(neighbor).
    // If we are crossing path ends, node0 is the start of a chain and neighbor
    // is the last node before the other chain starting at node1, therefore
    // node1 = next(neighbor).
    node1 = cross_path_starts ? Prev(neighbor) : Next(neighbor);
  } else {
    start1 = StartNode(1);
    node1 = BaseNode(1);
    cross_path_starts = start0 < start1;
  }
  if (start1 == start0 || node1 == start1) return false;

  bool moved = false;
  if (cross_path_starts) {
    // Cross path starts.
    // If two paths are equivalent don't exchange the full paths.
    if (PathClassFromStartNode(start0) == PathClassFromStartNode(start1) &&
        !IsPathEnd(node0) && IsPathEnd(Next(node0)) && !IsPathEnd(node1) &&
        IsPathEnd(Next(node1))) {
      return false;
    }
    const int first1 = Next(start1);
    if (!IsPathEnd(node0)) moved |= MoveChain(start0, node0, start1);
    if (!IsPathEnd(node1)) moved |= MoveChain(Prev(first1), node1, start0);
  } else {
    // Cross path ends.
    // If paths are equivalent, every end crossing has a corresponding start
    // crossing, we don't generate those symmetric neighbors.
    if (PathClassFromStartNode(start0) == PathClassFromStartNode(start1) &&
        !HasNeighbors()) {
      return false;
    }
    // Never exchange full paths, equivalent or not.
    // Full path exchange is only performed when start0 < start1.
    if (IsPathStart(Prev(node0)) && IsPathStart(Prev(node1)) &&
        !HasNeighbors()) {
      return false;
    }

    const int prev_end_node1 = Prev(CurrentNodePathEnd(node1));
    if (!IsPathEnd(node0)) {
      moved |= MoveChain(Prev(node0), Prev(EndNode(0)), prev_end_node1);
    }
    if (!IsPathEnd(node1)) {
      moved |= MoveChain(Prev(node1), prev_end_node1, Prev(EndNode(0)));
    }
  }
  return moved;
}

// ----- BaseInactiveNodeToPathOperator -----
// Base class of path operators which make inactive nodes active.

class BaseInactiveNodeToPathOperator : public PathOperator {
 public:
  BaseInactiveNodeToPathOperator(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars, int number_of_base_nodes,
      std::function<int(int64_t)> start_empty_path_class,
      NeighborAccessor get_incoming_neighbors = nullptr,
      NeighborAccessor get_outgoing_neighbors = nullptr)
      : PathOperator(vars, secondary_vars, number_of_base_nodes, false, false,
                     std::move(start_empty_path_class),
                     std::move(get_incoming_neighbors),
                     std::move(get_outgoing_neighbors)),
        inactive_node_(0) {
    // TODO(user): Activate skipping optimal paths.
  }
  ~BaseInactiveNodeToPathOperator() override {}

 protected:
  bool MakeOneNeighbor() override;
  int64_t GetInactiveNode() const { return inactive_node_; }

 private:
  void OnNodeInitialization() override;

  int inactive_node_;
};

void BaseInactiveNodeToPathOperator::OnNodeInitialization() {
  for (int i = 0; i < Size(); ++i) {
    if (IsInactive(i)) {
      inactive_node_ = i;
      return;
    }
  }
  inactive_node_ = Size();
}

bool BaseInactiveNodeToPathOperator::MakeOneNeighbor() {
  while (inactive_node_ < Size()) {
    if (!IsInactive(inactive_node_) || !PathOperator::MakeOneNeighbor()) {
      ResetPosition();
      ++inactive_node_;
    } else {
      return true;
    }
  }
  return false;
}

// ----- MakeActiveOperator -----

// MakeActiveOperator inserts an inactive node into a path.
// Possible neighbors for the path 1 -> 2 -> 3 -> 4 with 5 inactive (where 1 and
// 4 are first and last nodes of the path) are:
// 1 -> 5 -> 2 -> 3 -> 4
// 1 -> 2 -> 5 -> 3 -> 4
// 1 -> 2 -> 3 -> 5 -> 4

class MakeActiveOperator : public BaseInactiveNodeToPathOperator {
 public:
  MakeActiveOperator(const std::vector<IntVar*>& vars,
                     const std::vector<IntVar*>& secondary_vars,
                     std::function<int(int64_t)> start_empty_path_class,
                     NeighborAccessor get_incoming_neighbors = nullptr,
                     NeighborAccessor get_outgoing_neighbors = nullptr)
      : BaseInactiveNodeToPathOperator(vars, secondary_vars, 1,
                                       std::move(start_empty_path_class),
                                       std::move(get_incoming_neighbors),
                                       std::move(get_outgoing_neighbors)) {}
  ~MakeActiveOperator() override {}
  bool MakeNeighbor() override;

  std::string DebugString() const override { return "MakeActiveOperator"; }
};

bool MakeActiveOperator::MakeNeighbor() {
  // TODO(user): Add support for neighbors of inactive nodes; would require
  // having a version without base nodes (probably not a PathOperator).
  return MakeActive(GetInactiveNode(), BaseNode(0));
}

// ---- RelocateAndMakeActiveOperator -----

// RelocateAndMakeActiveOperator relocates a node and replaces it by an inactive
// node.
// The idea is to make room for inactive nodes.
// Possible neighbor for paths 0 -> 4, 1 -> 2 -> 5 and 3 inactive is:
// 0 -> 2 -> 4, 1 -> 3 -> 5.
// TODO(user): Naming is close to MakeActiveAndRelocate but this one is
// correct; rename MakeActiveAndRelocate if it is actually used.
class RelocateAndMakeActiveOperator : public BaseInactiveNodeToPathOperator {
 public:
  RelocateAndMakeActiveOperator(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars,
      std::function<int(int64_t)> start_empty_path_class)
      : BaseInactiveNodeToPathOperator(vars, secondary_vars, 2,
                                       std::move(start_empty_path_class)) {}
  ~RelocateAndMakeActiveOperator() override {}
  bool MakeNeighbor() override {
    const int64_t before_node_to_move = BaseNode(1);
    const int64_t node = Next(before_node_to_move);
    return !IsPathEnd(node) &&
           MoveChain(before_node_to_move, node, BaseNode(0)) &&
           MakeActive(GetInactiveNode(), before_node_to_move);
  }

  std::string DebugString() const override {
    return "RelocateAndMakeActiveOperator";
  }
};

// ----- MakeActiveAndRelocate -----

// MakeActiveAndRelocate makes a node active next to a node being relocated.
// Possible neighbor for paths 0 -> 4, 1 -> 2 -> 5 and 3 inactive is:
// 0 -> 3 -> 2 -> 4, 1 -> 5.

class MakeActiveAndRelocate : public BaseInactiveNodeToPathOperator {
 public:
  MakeActiveAndRelocate(const std::vector<IntVar*>& vars,
                        const std::vector<IntVar*>& secondary_vars,
                        std::function<int(int64_t)> start_empty_path_class)
      : BaseInactiveNodeToPathOperator(vars, secondary_vars, 2,
                                       std::move(start_empty_path_class)) {}
  ~MakeActiveAndRelocate() override {}
  bool MakeNeighbor() override;

  std::string DebugString() const override {
    return "MakeActiveAndRelocateOperator";
  }
};

bool MakeActiveAndRelocate::MakeNeighbor() {
  const int64_t before_chain = BaseNode(1);
  const int64_t chain_end = Next(before_chain);
  const int64_t destination = BaseNode(0);
  return !IsPathEnd(chain_end) &&
         MoveChain(before_chain, chain_end, destination) &&
         MakeActive(GetInactiveNode(), destination);
}

// ----- MakeInactiveOperator -----

// MakeInactiveOperator makes path nodes inactive.
// Possible neighbors for the path 1 -> 2 -> 3 -> 4 (where 1 and 4 are first
// and last nodes of the path) are:
// 1 -> 3 -> 4 & 2 inactive
// 1 -> 2 -> 4 & 3 inactive

class MakeInactiveOperator : public PathOperator {
 public:
  MakeInactiveOperator(const std::vector<IntVar*>& vars,
                       const std::vector<IntVar*>& secondary_vars,
                       std::function<int(int64_t)> start_empty_path_class)
      : PathOperator(vars, secondary_vars, 1, true, false,
                     std::move(start_empty_path_class), nullptr, nullptr) {}
  ~MakeInactiveOperator() override {}
  bool MakeNeighbor() override {
    const int64_t base = BaseNode(0);
    return MakeChainInactive(base, Next(base));
  }

  std::string DebugString() const override { return "MakeInactiveOperator"; }
};

// ----- RelocateAndMakeInactiveOperator -----

// RelocateAndMakeInactiveOperator relocates a node to a new position and makes
// the node which was at that position inactive.
// Possible neighbors for paths 0 -> 2 -> 4, 1 -> 3 -> 5 are:
// 0 -> 3 -> 4, 1 -> 5 & 2 inactive
// 0 -> 4, 1 -> 2 -> 5 & 3 inactive

class RelocateAndMakeInactiveOperator : public PathOperator {
 public:
  RelocateAndMakeInactiveOperator(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars,
      std::function<int(int64_t)> start_empty_path_class)
      : PathOperator(vars, secondary_vars, 2, true, false,
                     std::move(start_empty_path_class), nullptr, nullptr) {}
  ~RelocateAndMakeInactiveOperator() override {}
  bool MakeNeighbor() override {
    const int64_t destination = BaseNode(1);
    const int64_t before_to_move = BaseNode(0);
    const int64_t node_to_inactivate = Next(destination);
    if (node_to_inactivate == before_to_move || IsPathEnd(node_to_inactivate) ||
        !MakeChainInactive(destination, node_to_inactivate)) {
      return false;
    }
    const int64_t node = Next(before_to_move);
    return !IsPathEnd(node) && MoveChain(before_to_move, node, destination);
  }

  std::string DebugString() const override {
    return "RelocateAndMakeInactiveOperator";
  }
};

// ----- MakeChainInactiveOperator -----

// Operator which makes a "chain" of path nodes inactive.
// Possible neighbors for the path 1 -> 2 -> 3 -> 4 (where 1 and 4 are first
// and last nodes of the path) are:
//   1 -> 3 -> 4 with 2 inactive
//   1 -> 2 -> 4 with 3 inactive
//   1 -> 4 with 2 and 3 inactive

class MakeChainInactiveOperator : public PathOperator {
 public:
  MakeChainInactiveOperator(const std::vector<IntVar*>& vars,
                            const std::vector<IntVar*>& secondary_vars,
                            std::function<int(int64_t)> start_empty_path_class)
      : PathOperator(vars, secondary_vars, 2,
                     /*skip_locally_optimal_paths=*/true,
                     /*accept_path_end_base=*/false,
                     std::move(start_empty_path_class), nullptr, nullptr) {}
  ~MakeChainInactiveOperator() override {}
  bool MakeNeighbor() override {
    const int64_t chain_end = BaseNode(1);
    if (!IsPathEnd(chain_end) && chain_end != BaseNode(0) &&
        !Var(chain_end)->Contains(chain_end)) {
      // Move to the next before_chain since an unskippable node has been
      // encountered.
      SetNextBaseToIncrement(0);
      return false;
    }
    return MakeChainInactive(BaseNode(0), chain_end);
  }

  std::string DebugString() const override {
    return "MakeChainInactiveOperator";
  }

 protected:
  bool OnSamePathAsPreviousBase(int64_t) override {
    // Start and end of chain (defined by both base nodes) must be on the same
    // path.
    return true;
  }

  int64_t GetBaseNodeRestartPosition(int base_index) override {
    // Base node 1 must be after base node 0.
    return (base_index == 0) ? StartNode(base_index) : BaseNode(base_index - 1);
  }
};

// ----- SwapActiveOperator -----

// SwapActiveOperator replaces an active node by an inactive one.
// Possible neighbors for the path 1 -> 2 -> 3 -> 4 with 5 inactive (where 1 and
// 4 are first and last nodes of the path) are:
// 1 -> 5 -> 3 -> 4 & 2 inactive
// 1 -> 2 -> 5 -> 4 & 3 inactive

class SwapActiveOperator : public BaseInactiveNodeToPathOperator {
 public:
  SwapActiveOperator(const std::vector<IntVar*>& vars,
                     const std::vector<IntVar*>& secondary_vars,
                     std::function<int(int64_t)> start_empty_path_class)
      : BaseInactiveNodeToPathOperator(vars, secondary_vars, 1,
                                       std::move(start_empty_path_class)) {}
  ~SwapActiveOperator() override {}
  bool MakeNeighbor() override;

  std::string DebugString() const override { return "SwapActiveOperator"; }
};

bool SwapActiveOperator::MakeNeighbor() {
  const int64_t base = BaseNode(0);
  return MakeChainInactive(base, Next(base)) &&
         MakeActive(GetInactiveNode(), base);
}

class SwapActiveChainOperator : public BaseInactiveNodeToPathOperator {
 public:
  SwapActiveChainOperator(const std::vector<IntVar*>& vars,
                          const std::vector<IntVar*>& secondary_vars,
                          std::function<int(int64_t)> start_empty_path_class,
                          int max_chain_size)
      : BaseInactiveNodeToPathOperator(vars, secondary_vars, 2,
                                       std::move(start_empty_path_class)),
        last_before_chain_(-1),
        last_chain_end_(-1),
        current_chain_size_(0),
        max_chain_size_(max_chain_size) {
    DCHECK_GE(max_chain_size_, 1);
  }
  ~SwapActiveChainOperator() override {}
  bool MakeNeighbor() override;
  bool IsIncremental() const override { return true; }

 protected:
  void ResetIncrementalism() override {
    last_chain_end_ = -1;
    current_chain_size_ = 0;
  }
  bool OnSamePathAsPreviousBase(int64_t /*base_index*/) override {
    return true;
  }
  // TODO(user): Skip unfeasible chains by forcing the first base node to be
  // before the second one. Ideally this should be done as follows:
  // int64_t GetBaseNodeRestartPosition(int base_index) override {
  //   return (base_index == 0) ? StartNode(base_index) : BaseNode(base_index -
  //   1);
  // }
  // However due to the fact we are iterating over the chains multiple times
  // (once for each unperformed node), this breaks the ending position of the
  // neighborhood (causing an infinite iteration).

  std::string DebugString() const override { return "SwapActiveChainOperator"; }

 private:
  void OnNodeInitialization() override {
    last_chain_end_ = -1;
    current_chain_size_ = 0;
  }

  int64_t last_before_chain_;
  int64_t last_chain_end_;
  int current_chain_size_;
  const int max_chain_size_;
};

bool SwapActiveChainOperator::MakeNeighbor() {
  const int64_t before_chain = BaseNode(0);
  const int64_t chain_end = BaseNode(1);
  if (last_before_chain_ != before_chain || last_chain_end_ == -1) {
    RevertChanges(/*change_was_incremental=*/false);
    last_before_chain_ = before_chain;
    last_chain_end_ = GetInactiveNode();
    if (!IsPathEnd(chain_end) && before_chain != chain_end &&
        MakeChainInactive(before_chain, chain_end) &&
        MakeActive(GetInactiveNode(), before_chain)) {
      ++current_chain_size_;
      return true;
    } else {
      last_chain_end_ = -1;
      current_chain_size_ = 0;
      return false;
    }
  }
  if (current_chain_size_ >= max_chain_size_) {
    // Move to the next before_chain.
    SetNextBaseToIncrement(0);
    current_chain_size_ = 0;
    return false;
  }
  if (!IsPathEnd(last_chain_end_) &&
      MakeChainInactive(last_chain_end_, Next(last_chain_end_))) {
    ++current_chain_size_;
    return true;
  }
  last_chain_end_ = -1;
  current_chain_size_ = 0;
  return false;
}

// ----- ExtendedSwapActiveOperator -----

// ExtendedSwapActiveOperator makes an inactive node active and an active one
// inactive. It is similar to SwapActiveOperator excepts that it tries to
// insert the inactive node in all possible positions instead of just the
// position of the node made inactive.
// Possible neighbors for the path 1 -> 2 -> 3 -> 4 with 5 inactive (where 1 and
// 4 are first and last nodes of the path) are:
// 1 -> 5 -> 3 -> 4 & 2 inactive
// 1 -> 3 -> 5 -> 4 & 2 inactive
// 1 -> 5 -> 2 -> 4 & 3 inactive
// 1 -> 2 -> 5 -> 4 & 3 inactive

class ExtendedSwapActiveOperator : public BaseInactiveNodeToPathOperator {
 public:
  ExtendedSwapActiveOperator(const std::vector<IntVar*>& vars,
                             const std::vector<IntVar*>& secondary_vars,
                             std::function<int(int64_t)> start_empty_path_class)
      : BaseInactiveNodeToPathOperator(vars, secondary_vars, 2,
                                       std::move(start_empty_path_class)) {}
  ~ExtendedSwapActiveOperator() override {}
  bool MakeNeighbor() override;

  std::string DebugString() const override {
    return "ExtendedSwapActiveOperator";
  }
};

bool ExtendedSwapActiveOperator::MakeNeighbor() {
  const int64_t base0 = BaseNode(0);
  const int64_t base1 = BaseNode(1);
  if (Next(base0) == base1) {
    return false;
  }
  return MakeChainInactive(base0, Next(base0)) &&
         MakeActive(GetInactiveNode(), base1);
}

// ----- TSP-based operators -----

// Sliding TSP operator
// Uses an exact dynamic programming algorithm to solve the TSP corresponding
// to path sub-chains.
// For a subchain 1 -> 2 -> 3 -> 4 -> 5 -> 6, solves the TSP on nodes A, 2, 3,
// 4, 5, where A is a merger of nodes 1 and 6 such that cost(A,i) = cost(1,i)
// and cost(i,A) = cost(i,6).

class TSPOpt : public PathOperator {
 public:
  TSPOpt(const std::vector<IntVar*>& vars,
         const std::vector<IntVar*>& secondary_vars,
         Solver::IndexEvaluator3 evaluator, int chain_length);
  ~TSPOpt() override {}
  bool MakeNeighbor() override;

  std::string DebugString() const override { return "TSPOpt"; }

 private:
  std::vector<std::vector<int64_t>> cost_;
  HamiltonianPathSolver<int64_t, std::vector<std::vector<int64_t>>>
      hamiltonian_path_solver_;
  Solver::IndexEvaluator3 evaluator_;
  const int chain_length_;
};

TSPOpt::TSPOpt(const std::vector<IntVar*>& vars,
               const std::vector<IntVar*>& secondary_vars,
               Solver::IndexEvaluator3 evaluator, int chain_length)
    : PathOperator(vars, secondary_vars, 1, true, false, nullptr, nullptr,
                   nullptr),
      hamiltonian_path_solver_(cost_),
      evaluator_(std::move(evaluator)),
      chain_length_(chain_length) {}

bool TSPOpt::MakeNeighbor() {
  std::vector<int64_t> nodes;
  int64_t chain_end = BaseNode(0);
  for (int i = 0; i < chain_length_ + 1; ++i) {
    nodes.push_back(chain_end);
    if (IsPathEnd(chain_end)) {
      break;
    }
    chain_end = Next(chain_end);
  }
  if (nodes.size() <= 3) {
    return false;
  }
  int64_t chain_path = Path(BaseNode(0));
  const int size = nodes.size() - 1;
  cost_.resize(size);
  for (int i = 0; i < size; ++i) {
    cost_[i].resize(size);
    cost_[i][0] = evaluator_(nodes[i], nodes[size], chain_path);
    for (int j = 1; j < size; ++j) {
      cost_[i][j] = evaluator_(nodes[i], nodes[j], chain_path);
    }
  }
  hamiltonian_path_solver_.ChangeCostMatrix(cost_);
  std::vector<PathNodeIndex> path;
  hamiltonian_path_solver_.TravelingSalesmanPath(&path);
  CHECK_EQ(size + 1, path.size());
  for (int i = 0; i < size - 1; ++i) {
    SetNext(nodes[path[i]], nodes[path[i + 1]], chain_path);
  }
  SetNext(nodes[path[size - 1]], nodes[size], chain_path);
  return true;
}

// TSP-base lns
// Randomly merge consecutive nodes until n "meta"-nodes remain and solve the
// corresponding TSP. This can be seen as a large neighborhood search operator
// although decisions are taken with the operator.
// This is an "unlimited" neighborhood which must be stopped by search limits.
// To force diversification, the operator iteratively forces each node to serve
// as base of a meta-node.

class TSPLns : public PathOperator {
 public:
  TSPLns(const std::vector<IntVar*>& vars,
         const std::vector<IntVar*>& secondary_vars,
         Solver::IndexEvaluator3 evaluator, int tsp_size);
  ~TSPLns() override {}
  bool MakeNeighbor() override;

  std::string DebugString() const override { return "TSPLns"; }

 protected:
  bool MakeOneNeighbor() override;

 private:
  void OnNodeInitialization() override {
    // NOTE: Avoid any computations if there are no vars added.
    has_long_enough_paths_ = Size() != 0;
  }

  std::vector<std::vector<int64_t>> cost_;
  HamiltonianPathSolver<int64_t, std::vector<std::vector<int64_t>>>
      hamiltonian_path_solver_;
  Solver::IndexEvaluator3 evaluator_;
  const int tsp_size_;
  std::mt19937 rand_;
  bool has_long_enough_paths_;
};

TSPLns::TSPLns(const std::vector<IntVar*>& vars,
               const std::vector<IntVar*>& secondary_vars,
               Solver::IndexEvaluator3 evaluator, int tsp_size)
    : PathOperator(vars, secondary_vars, 1, true, false, nullptr, nullptr,
                   nullptr),
      hamiltonian_path_solver_(cost_),
      evaluator_(std::move(evaluator)),
      tsp_size_(tsp_size),
      rand_(CpRandomSeed()),
      has_long_enough_paths_(true) {
  CHECK_GE(tsp_size_, 0);
  cost_.resize(tsp_size_);
  for (int i = 0; i < tsp_size_; ++i) {
    cost_[i].resize(tsp_size_);
  }
}

bool TSPLns::MakeOneNeighbor() {
  while (has_long_enough_paths_) {
    has_long_enough_paths_ = false;
    if (PathOperator::MakeOneNeighbor()) {
      return true;
    }
    Var(0)->solver()->TopPeriodicCheck();
  }
  return false;
}

bool TSPLns::MakeNeighbor() {
  const int64_t base_node = BaseNode(0);
  std::vector<int64_t> nodes;
  for (int64_t node = StartNode(0); !IsPathEnd(node); node = Next(node)) {
    nodes.push_back(node);
  }
  if (nodes.size() <= tsp_size_) {
    return false;
  }
  has_long_enough_paths_ = true;
  // Randomly select break nodes (final nodes of a meta-node, after which
  // an arc is relaxed.
  absl::flat_hash_set<int64_t> breaks_set;
  // Always add base node to break nodes (diversification)
  breaks_set.insert(base_node);
  CHECK(!nodes.empty());  // Should have been caught earlier.
  while (breaks_set.size() < tsp_size_) {
    breaks_set.insert(nodes[absl::Uniform<int>(rand_, 0, nodes.size())]);
  }
  CHECK_EQ(breaks_set.size(), tsp_size_);
  // Setup break node indexing and internal meta-node cost (cost of partial
  // route starting at first node of the meta-node and ending at its last node);
  // this cost has to be added to the TSP matrix cost in order to respect the
  // triangle inequality.
  std::vector<int> breaks;
  std::vector<int64_t> meta_node_costs;
  int64_t cost = 0;
  int64_t node = StartNode(0);
  int64_t node_path = Path(node);
  while (!IsPathEnd(node)) {
    int64_t next = Next(node);
    if (breaks_set.contains(node)) {
      breaks.push_back(node);
      meta_node_costs.push_back(cost);
      cost = 0;
    } else {
      cost = CapAdd(cost, evaluator_(node, next, node_path));
    }
    node = next;
  }
  meta_node_costs[0] += cost;
  CHECK_EQ(breaks.size(), tsp_size_);
  // Setup TSP cost matrix
  CHECK_EQ(meta_node_costs.size(), tsp_size_);
  for (int i = 0; i < tsp_size_; ++i) {
    cost_[i][0] =
        CapAdd(meta_node_costs[i],
               evaluator_(breaks[i], Next(breaks[tsp_size_ - 1]), node_path));
    for (int j = 1; j < tsp_size_; ++j) {
      cost_[i][j] =
          CapAdd(meta_node_costs[i],
                 evaluator_(breaks[i], Next(breaks[j - 1]), node_path));
    }
    cost_[i][i] = 0;
  }
  // Solve TSP and inject solution in delta (only if it leads to a new solution)
  hamiltonian_path_solver_.ChangeCostMatrix(cost_);
  std::vector<PathNodeIndex> path;
  hamiltonian_path_solver_.TravelingSalesmanPath(&path);
  bool nochange = true;
  for (int i = 0; i < path.size() - 1; ++i) {
    if (path[i] != i) {
      nochange = false;
      break;
    }
  }
  if (nochange) {
    return false;
  }
  CHECK_EQ(0, path[path.size() - 1]);
  for (int i = 0; i < tsp_size_ - 1; ++i) {
    SetNext(breaks[path[i]], OldNext(breaks[path[i + 1] - 1]), node_path);
  }
  SetNext(breaks[path[tsp_size_ - 1]], OldNext(breaks[tsp_size_ - 1]),
          node_path);
  return true;
}

// ----- Lin Kernighan -----

// For each variable in vars, stores the 'size' pairs(i,j) with the smallest
// value according to evaluator, where i is the index of the variable in vars
// and j is in the domain of the variable.
// Note that the resulting pairs are sorted.
// Works in O(size) per variable on average (same approach as qsort)

class NearestNeighbors {
 public:
  NearestNeighbors(Solver::IndexEvaluator3 evaluator,
                   const PathOperator& path_operator, int size);

  // This type is neither copyable nor movable.
  NearestNeighbors(const NearestNeighbors&) = delete;
  NearestNeighbors& operator=(const NearestNeighbors&) = delete;

  virtual ~NearestNeighbors() {}
  void Initialize(const std::vector<int>& path);
  const std::vector<int>& Neighbors(int index) const;

  virtual std::string DebugString() const { return "NearestNeighbors"; }

 private:
  void ComputeNearest(int row, absl::Span<const int> path);

  std::vector<std::vector<int>> neighbors_;
  Solver::IndexEvaluator3 evaluator_;
  const PathOperator& path_operator_;
  const int size_;
};

NearestNeighbors::NearestNeighbors(Solver::IndexEvaluator3 evaluator,
                                   const PathOperator& path_operator, int size)
    : neighbors_(path_operator.number_of_nexts()),
      evaluator_(std::move(evaluator)),
      path_operator_(path_operator),
      size_(size) {}

void NearestNeighbors::Initialize(const std::vector<int>& path) {
  for (int node : path) {
    if (node < path_operator_.number_of_nexts()) ComputeNearest(node, path);
  }
}

const std::vector<int>& NearestNeighbors::Neighbors(int index) const {
  return neighbors_[index];
}

void NearestNeighbors::ComputeNearest(int row,
                                      absl::Span<const int> path_nodes) {
  // Find size_ nearest neighbors for row of index 'row'.
  const int path = path_operator_.Path(row);
  const IntVar* var = path_operator_.Var(row);
  using ValuedIndex = std::pair<int64_t /*value*/, int /*index*/>;
  std::vector<ValuedIndex> neighbors;
  for (int index : path_nodes) {
    if (!var->Contains(index)) continue;
    neighbors.push_back({evaluator_(row, index, path), index});
  }
  const int neighbors_size = neighbors.size();
  if (neighbors_size > size_) {
    std::nth_element(neighbors.begin(), neighbors.begin() + size_ - 1,
                     neighbors.end());
  }

  // Setup global neighbor matrix for row row_index
  neighbors_[row].clear();
  for (int i = 0; i < std::min(size_, neighbors_size); ++i) {
    neighbors_[row].push_back(neighbors[i].second);
  }
  std::sort(neighbors_[row].begin(), neighbors_[row].end());
}

class LinKernighan : public PathOperator {
 public:
  LinKernighan(const std::vector<IntVar*>& vars,
               const std::vector<IntVar*>& secondary_vars,
               const Solver::IndexEvaluator3& evaluator, bool topt);
  ~LinKernighan() override;
  bool MakeNeighbor() override;

  std::string DebugString() const override { return "LinKernighan"; }

 private:
  void OnNodeInitialization() override;

  static const int kNeighbors;

  bool GetBestOut(int64_t in_i, int64_t in_j, int64_t* out, int64_t* gain);

  Solver::IndexEvaluator3 const evaluator_;
  NearestNeighbors neighbors_;
  absl::flat_hash_set<int64_t> marked_;
  const bool topt_;
  std::vector<int> old_path_starts_;
};

// While the accumulated local gain is positive, perform a 2opt or a 3opt move
// followed by a series of 2opt moves. Return a neighbor for which the global
// gain is positive.

LinKernighan::LinKernighan(const std::vector<IntVar*>& vars,
                           const std::vector<IntVar*>& secondary_vars,
                           const Solver::IndexEvaluator3& evaluator, bool topt)
    : PathOperator(vars, secondary_vars, 1, true, false, nullptr, nullptr,
                   nullptr),
      evaluator_(evaluator),
      neighbors_(evaluator, *this, kNeighbors),
      topt_(topt) {
  old_path_starts_.resize(number_of_nexts(), -1);
}

LinKernighan::~LinKernighan() {}

void LinKernighan::OnNodeInitialization() {
  absl::flat_hash_set<int> touched_paths;
  for (int i = 0; i < number_of_nexts(); ++i) {
    if (IsPathStart(i)) {
      for (int node = i; !IsPathEnd(node); node = Next(node)) {
        if (i != old_path_starts_[node]) {
          touched_paths.insert(old_path_starts_[node]);
          touched_paths.insert(i);
          old_path_starts_[node] = i;
        }
      }
    } else if (Next(i) == i) {
      touched_paths.insert(old_path_starts_[i]);
      old_path_starts_[i] = -1;
    }
  }
  for (int touched_path_start : touched_paths) {
    if (touched_path_start == -1) continue;
    std::vector<int> path;
    int node = touched_path_start;
    while (!IsPathEnd(node)) {
      path.push_back(node);
      node = Next(node);
    }
    path.push_back(node);
    neighbors_.Initialize(path);
  }
}

bool LinKernighan::MakeNeighbor() {
  marked_.clear();
  int64_t node = BaseNode(0);
  int64_t path = Path(node);
  int64_t base = node;
  int64_t next = Next(node);
  if (IsPathEnd(next)) return false;
  int64_t out = -1;
  int64_t gain = 0;
  marked_.insert(node);
  if (topt_) {  // Try a 3opt first
    if (!GetBestOut(node, next, &out, &gain)) return false;
    marked_.insert(next);
    marked_.insert(out);
    const int64_t node1 = out;
    if (IsPathEnd(node1)) return false;
    const int64_t next1 = Next(node1);
    if (IsPathEnd(next1)) return false;
    if (!GetBestOut(node1, next1, &out, &gain)) return false;
    marked_.insert(next1);
    marked_.insert(out);
    if (!CheckChainValidity(out, node1, node) || !MoveChain(out, node1, node)) {
      return false;
    }
    const int64_t next_out = Next(out);
    const int64_t in_cost = evaluator_(node, next_out, path);
    const int64_t out_cost = evaluator_(out, next_out, path);
    if (CapAdd(CapSub(gain, in_cost), out_cost) > 0) return true;
    node = out;
    if (IsPathEnd(node)) return false;
    next = next_out;
    if (IsPathEnd(next)) return false;
  }
  // Try 2opts
  while (GetBestOut(node, next, &out, &gain)) {
    marked_.insert(next);
    marked_.insert(out);
    int64_t chain_last;
    if (Next(base) == out || (!IsPathEnd(out) && Next(out) == base)) {
      return false;
    }
    const bool success = ReverseChain(base, out, &chain_last) ||
                         ReverseChain(out, base, &chain_last);
    if (!success) {
#ifndef NDEBUG
      LOG(ERROR) << "ReverseChain failed: " << base << " " << out;
      for (int node = StartNode(0); !IsPathEnd(node); node = Next(node)) {
        LOG(ERROR) << "node: " << node;
      }
      LOG(ERROR) << "node: " << node;
      DCHECK(false);
#endif
    }
    const int64_t in_cost = evaluator_(base, chain_last, path);
    const int64_t out_cost = evaluator_(chain_last, out, path);
    if (CapAdd(CapSub(gain, in_cost), out_cost) > 0) {
      return true;
    }
    node = out;
    if (IsPathEnd(node)) {
      return false;
    }
    next = chain_last;
    if (IsPathEnd(next)) {
      return false;
    }
  }
  return false;
}

const int LinKernighan::kNeighbors = 5 + 1;

bool LinKernighan::GetBestOut(int64_t in_i, int64_t in_j, int64_t* out,
                              int64_t* gain) {
  int64_t best_gain = std::numeric_limits<int64_t>::min();
  const int64_t path = Path(in_i);
  const int64_t out_cost = evaluator_(in_i, in_j, path);
  const int64_t current_gain = CapAdd(*gain, out_cost);
  for (int next : neighbors_.Neighbors(in_j)) {
    if (next != in_j && next != Next(in_j) && !marked_.contains(in_j) &&
        !marked_.contains(next)) {
      const int64_t in_cost = evaluator_(in_j, next, path);
      const int64_t new_gain = CapSub(current_gain, in_cost);
      if (new_gain > 0 && best_gain < new_gain) {
        *out = next;
        best_gain = new_gain;
      }
    }
  }
  *gain = best_gain;
  return (best_gain > std::numeric_limits<int64_t>::min());
}

// ----- Path-based Large Neighborhood Search -----

// Breaks "number_of_chunks" chains of "chunk_size" arcs, and deactivate all
// inactive nodes if "unactive_fragments" is true.
// As a special case, if chunk_size=0, then we break full paths.

class PathLns : public PathOperator {
 public:
  PathLns(const std::vector<IntVar*>& vars,
          const std::vector<IntVar*>& secondary_vars, int number_of_chunks,
          int chunk_size, bool unactive_fragments)
      : PathOperator(vars, secondary_vars, number_of_chunks, true, true,
                     nullptr, nullptr, nullptr),
        number_of_chunks_(number_of_chunks),
        chunk_size_(chunk_size),
        unactive_fragments_(unactive_fragments) {
    CHECK_GE(chunk_size_, 0);
  }
  ~PathLns() override {}
  bool MakeNeighbor() override;

  std::string DebugString() const override { return "PathLns"; }
  bool HasFragments() const override { return true; }

 private:
  inline bool ChainsAreFullPaths() const { return chunk_size_ == 0; }
  void DeactivateChain(int64_t node);
  void DeactivateUnactives();

  const int number_of_chunks_;
  const int chunk_size_;
  const bool unactive_fragments_;
};

bool PathLns::MakeNeighbor() {
  if (ChainsAreFullPaths()) {
    // Reject the current position as a neighbor if any of its base node
    // isn't at the start of a path.
    // TODO(user): make this more efficient.
    for (int i = 0; i < number_of_chunks_; ++i) {
      if (BaseNode(i) != StartNode(i)) return false;
    }
  }
  for (int i = 0; i < number_of_chunks_; ++i) {
    DeactivateChain(BaseNode(i));
  }
  DeactivateUnactives();
  return true;
}

void PathLns::DeactivateChain(int64_t node) {
  for (int i = 0, current = node;
       (ChainsAreFullPaths() || i < chunk_size_) && !IsPathEnd(current);
       ++i, current = Next(current)) {
    Deactivate(current);
    if (!ignore_path_vars_) {
      Deactivate(number_of_nexts_ + current);
    }
  }
}

void PathLns::DeactivateUnactives() {
  if (unactive_fragments_) {
    for (int i = 0; i < Size(); ++i) {
      if (IsInactive(i)) {
        Deactivate(i);
        if (!ignore_path_vars_) {
          Deactivate(number_of_nexts_ + i);
        }
      }
    }
  }
}

// ----- Limit the number of neighborhoods explored -----

class NeighborhoodLimit : public LocalSearchOperator {
 public:
  NeighborhoodLimit(LocalSearchOperator* const op, int64_t limit)
      : operator_(op), limit_(limit), next_neighborhood_calls_(0) {
    CHECK(op != nullptr);
    CHECK_GT(limit, 0);
  }

  void Start(const Assignment* assignment) override {
    next_neighborhood_calls_ = 0;
    operator_->Start(assignment);
  }

  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override {
    if (next_neighborhood_calls_ >= limit_) {
      return false;
    }
    ++next_neighborhood_calls_;
    return operator_->MakeNextNeighbor(delta, deltadelta);
  }

  bool HoldsDelta() const override { return operator_->HoldsDelta(); }

  std::string DebugString() const override { return "NeighborhoodLimit"; }

 private:
  LocalSearchOperator* const operator_;
  const int64_t limit_;
  int64_t next_neighborhood_calls_;
};

LocalSearchOperator* Solver::MakeNeighborhoodLimit(
    LocalSearchOperator* const op, int64_t limit) {
  return RevAlloc(new NeighborhoodLimit(op, limit));
}

// ----- Concatenation of operators -----

namespace {
class CompoundOperator : public LocalSearchOperator {
 public:
  CompoundOperator(std::vector<LocalSearchOperator*> operators,
                   std::function<int64_t(int, int)> evaluator);
  ~CompoundOperator() override {}
  void EnterSearch() override;
  void Reset() override;
  void Start(const Assignment* assignment) override;
  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override;
  bool HasFragments() const override { return has_fragments_; }
  bool HoldsDelta() const override { return true; }

  std::string DebugString() const override {
    return operators_.empty()
               ? ""
               : operators_[operator_indices_[index_]]->DebugString();
  }
  const LocalSearchOperator* Self() const override {
    return operators_.empty() ? this
                              : operators_[operator_indices_[index_]]->Self();
  }

 private:
  class OperatorComparator {
   public:
    OperatorComparator(std::function<int64_t(int, int)> evaluator,
                       int active_operator)
        : evaluator_(std::move(evaluator)), active_operator_(active_operator) {}
    bool operator()(int lhs, int rhs) const {
      const int64_t lhs_value = Evaluate(lhs);
      const int64_t rhs_value = Evaluate(rhs);
      return lhs_value < rhs_value || (lhs_value == rhs_value && lhs < rhs);
    }

   private:
    int64_t Evaluate(int operator_index) const {
      return evaluator_(active_operator_, operator_index);
    }

    std::function<int64_t(int, int)> evaluator_;
    const int active_operator_;
  };

  int64_t index_;
  std::vector<LocalSearchOperator*> operators_;
  std::vector<int> operator_indices_;
  std::function<int64_t(int, int)> evaluator_;
  Bitset64<> started_;
  const Assignment* start_assignment_;
  bool has_fragments_;
};

CompoundOperator::CompoundOperator(std::vector<LocalSearchOperator*> operators,
                                   std::function<int64_t(int, int)> evaluator)
    : index_(0),
      operators_(std::move(operators)),
      evaluator_(std::move(evaluator)),
      started_(operators_.size()),
      start_assignment_(nullptr),
      has_fragments_(false) {
  operators_.erase(std::remove(operators_.begin(), operators_.end(), nullptr),
                   operators_.end());
  operator_indices_.resize(operators_.size());
  absl::c_iota(operator_indices_, 0);
  for (LocalSearchOperator* const op : operators_) {
    if (op->HasFragments()) {
      has_fragments_ = true;
      break;
    }
  }
}

void CompoundOperator::EnterSearch() {
  absl::c_iota(operator_indices_, 0);
  index_ = 0;
  for (LocalSearchOperator* const op : operators_) {
    op->EnterSearch();
  }
}

void CompoundOperator::Reset() {
  for (LocalSearchOperator* const op : operators_) {
    op->Reset();
  }
}

void CompoundOperator::Start(const Assignment* assignment) {
  start_assignment_ = assignment;
  started_.ClearAll();
  if (!operators_.empty()) {
    std::sort(operator_indices_.begin(), operator_indices_.end(),
              OperatorComparator(evaluator_, operator_indices_[index_]));
    index_ = 0;
  }
}

bool CompoundOperator::MakeNextNeighbor(Assignment* delta,
                                        Assignment* deltadelta) {
  const auto is_leaf = [](const LocalSearchOperator* op) {
    return op == op->Self();
  };
  if (!operators_.empty()) {
    Solver* solver = delta->solver();
    do {
      // TODO(user): keep copy of delta in case MakeNextNeighbor
      // pollutes delta on a fail.
      const int64_t operator_index = operator_indices_[index_];
      LocalSearchOperator* op = operators_[operator_index];
      if (!started_[operator_index]) {
        op->Start(start_assignment_);
        started_.Set(operator_index);
      }
      if (!op->HoldsDelta()) {
        delta->Clear();
      }
      if (is_leaf(op)) {
        solver->GetLocalSearchMonitor()->BeginMakeNextNeighbor(op);
      }
      if (op->MakeNextNeighbor(delta, deltadelta)) return true;
      if (is_leaf(op)) {
        solver->GetLocalSearchMonitor()->EndMakeNextNeighbor(
            op, /*has_neighbor*/ false, delta, deltadelta);
      }
      ++index_;
      delta->Clear();
      if (index_ == operators_.size()) {
        index_ = 0;
      }
    } while (index_ != 0);
  }
  return false;
}

int64_t CompoundOperatorNoRestart(int size, int active_index,
                                  int operator_index) {
  return (operator_index < active_index) ? size + operator_index - active_index
                                         : operator_index - active_index;
}
}  // namespace

LocalSearchOperator* Solver::ConcatenateOperators(
    const std::vector<LocalSearchOperator*>& ops) {
  return ConcatenateOperators(ops, false);
}

LocalSearchOperator* Solver::ConcatenateOperators(
    const std::vector<LocalSearchOperator*>& ops, bool restart) {
  if (restart) {
    return ConcatenateOperators(
        ops, std::function<int64_t(int, int)>([](int, int) { return 0; }));
  }
  const int size = ops.size();
  return ConcatenateOperators(ops, [size](int i, int j) {
    return CompoundOperatorNoRestart(size, i, j);
  });
}

LocalSearchOperator* Solver::ConcatenateOperators(
    const std::vector<LocalSearchOperator*>& ops,
    std::function<int64_t(int, int)> evaluator) {
  return RevAlloc(new CompoundOperator(ops, std::move(evaluator)));
}

namespace {
class RandomCompoundOperator : public LocalSearchOperator {
 public:
  explicit RandomCompoundOperator(std::vector<LocalSearchOperator*> operators);
  RandomCompoundOperator(std::vector<LocalSearchOperator*> operators,
                         int32_t seed);
  ~RandomCompoundOperator() override {}
  void EnterSearch() override;
  void Reset() override;
  void Start(const Assignment* assignment) override;
  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override;
  bool HoldsDelta() const override { return true; }

  std::string DebugString() const override { return "RandomCompoundOperator"; }
  // TODO(user): define Self method.

 private:
  std::mt19937 rand_;
  const std::vector<LocalSearchOperator*> operators_;
  bool has_fragments_;
};

void RandomCompoundOperator::Start(const Assignment* assignment) {
  for (LocalSearchOperator* const op : operators_) {
    op->Start(assignment);
  }
}

RandomCompoundOperator::RandomCompoundOperator(
    std::vector<LocalSearchOperator*> operators)
    : RandomCompoundOperator(std::move(operators), CpRandomSeed()) {}

RandomCompoundOperator::RandomCompoundOperator(
    std::vector<LocalSearchOperator*> operators, int32_t seed)
    : rand_(seed), operators_(std::move(operators)), has_fragments_(false) {
  for (LocalSearchOperator* const op : operators_) {
    if (op->HasFragments()) {
      has_fragments_ = true;
      break;
    }
  }
}

void RandomCompoundOperator::EnterSearch() {
  for (LocalSearchOperator* const op : operators_) {
    op->EnterSearch();
  }
}

void RandomCompoundOperator::Reset() {
  for (LocalSearchOperator* const op : operators_) {
    op->Reset();
  }
}

bool RandomCompoundOperator::MakeNextNeighbor(Assignment* delta,
                                              Assignment* deltadelta) {
  const int size = operators_.size();
  std::vector<int> indices(size);
  absl::c_iota(indices, 0);
  std::shuffle(indices.begin(), indices.end(), rand_);
  for (int index : indices) {
    if (!operators_[index]->HoldsDelta()) {
      delta->Clear();
    }
    if (operators_[index]->MakeNextNeighbor(delta, deltadelta)) {
      return true;
    }
    delta->Clear();
  }
  return false;
}
}  // namespace

LocalSearchOperator* Solver::RandomConcatenateOperators(
    const std::vector<LocalSearchOperator*>& ops) {
  return RevAlloc(new RandomCompoundOperator(ops));
}

LocalSearchOperator* Solver::RandomConcatenateOperators(
    const std::vector<LocalSearchOperator*>& ops, int32_t seed) {
  return RevAlloc(new RandomCompoundOperator(ops, seed));
}

namespace {
class MultiArmedBanditCompoundOperator : public LocalSearchOperator {
 public:
  explicit MultiArmedBanditCompoundOperator(
      std::vector<LocalSearchOperator*> operators, double memory_coefficient,
      double exploration_coefficient, bool maximize);
  ~MultiArmedBanditCompoundOperator() override {}
  void EnterSearch() override;
  void Reset() override;
  void Start(const Assignment* assignment) override;
  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override;
  bool HoldsDelta() const override { return true; }

  std::string DebugString() const override {
    return operators_.empty()
               ? ""
               : operators_[operator_indices_[index_]]->DebugString();
  }
  const LocalSearchOperator* Self() const override {
    return operators_.empty() ? this
                              : operators_[operator_indices_[index_]]->Self();
  }

 private:
  double Score(int index);
  int index_;
  std::vector<LocalSearchOperator*> operators_;
  Bitset64<> started_;
  const Assignment* start_assignment_;
  bool has_fragments_;
  std::vector<int> operator_indices_;
  int64_t last_objective_;
  std::vector<double> avg_improvement_;
  int num_neighbors_;
  std::vector<double> num_neighbors_per_operator_;
  const bool maximize_;
  // Sets how much the objective improvement of previous accepted neighbors
  // influence the current average improvement. The formula is
  //  avg_improvement +=
  //   memory_coefficient * (current_improvement - avg_improvement).
  const double memory_coefficient_;
  // Sets how often we explore rarely used and unsuccessful in the past
  // operators. Operators are sorted by
  //  avg_improvement_[i] + exploration_coefficient_ *
  //   sqrt(2 * log(1 + num_neighbors_) / (1 + num_neighbors_per_operator_[i])).
  // This definition uses the UCB1 exploration bonus for unstructured
  // multi-armed bandits.
  const double exploration_coefficient_;
};

MultiArmedBanditCompoundOperator::MultiArmedBanditCompoundOperator(
    std::vector<LocalSearchOperator*> operators, double memory_coefficient,
    double exploration_coefficient, bool maximize)
    : index_(0),
      operators_(std::move(operators)),
      started_(operators_.size()),
      start_assignment_(nullptr),
      has_fragments_(false),
      last_objective_(std::numeric_limits<int64_t>::max()),
      num_neighbors_(0),
      maximize_(maximize),
      memory_coefficient_(memory_coefficient),
      exploration_coefficient_(exploration_coefficient) {
  DCHECK_GE(memory_coefficient_, 0);
  DCHECK_LE(memory_coefficient_, 1);
  DCHECK_GE(exploration_coefficient_, 0);
  operators_.erase(std::remove(operators_.begin(), operators_.end(), nullptr),
                   operators_.end());
  operator_indices_.resize(operators_.size());
  absl::c_iota(operator_indices_, 0);
  num_neighbors_per_operator_.resize(operators_.size(), 0);
  avg_improvement_.resize(operators_.size(), 0);
  for (LocalSearchOperator* const op : operators_) {
    if (op->HasFragments()) {
      has_fragments_ = true;
      break;
    }
  }
}

void MultiArmedBanditCompoundOperator::EnterSearch() {
  last_objective_ = std::numeric_limits<int64_t>::max();
  num_neighbors_ = 0;
  absl::c_iota(operator_indices_, 0);
  index_ = 0;
  num_neighbors_per_operator_.resize(operators_.size(), 0);
  avg_improvement_.resize(operators_.size(), 0);
  for (LocalSearchOperator* const op : operators_) {
    op->EnterSearch();
  }
}

void MultiArmedBanditCompoundOperator::Reset() {
  for (LocalSearchOperator* const op : operators_) {
    op->Reset();
  }
}

double MultiArmedBanditCompoundOperator::Score(int index) {
  return avg_improvement_[index] +
         exploration_coefficient_ *
             sqrt(2 * log(1 + num_neighbors_) /
                  (1 + num_neighbors_per_operator_[index]));
}

void MultiArmedBanditCompoundOperator::Start(const Assignment* assignment) {
  start_assignment_ = assignment;
  started_.ClearAll();
  if (operators_.empty()) return;

  const double objective = assignment->ObjectiveValue();

  if (objective == last_objective_) return;
  // Skip a neighbor evaluation if last_objective_ hasn't been set yet.
  if (last_objective_ == std::numeric_limits<int64_t>::max()) {
    last_objective_ = objective;
    return;
  }

  const double improvement =
      maximize_ ? objective - last_objective_ : last_objective_ - objective;
  if (improvement < 0) {
    return;
  }
  last_objective_ = objective;
  avg_improvement_[operator_indices_[index_]] +=
      memory_coefficient_ *
      (improvement - avg_improvement_[operator_indices_[index_]]);

  std::sort(operator_indices_.begin(), operator_indices_.end(),
            [this](int lhs, int rhs) {
              const double lhs_score = Score(lhs);
              const double rhs_score = Score(rhs);
              return lhs_score > rhs_score ||
                     (lhs_score == rhs_score && lhs < rhs);
            });

  index_ = 0;
}

bool MultiArmedBanditCompoundOperator::MakeNextNeighbor(
    Assignment* delta, Assignment* deltadelta) {
  if (operators_.empty()) return false;
  do {
    const int operator_index = operator_indices_[index_];
    if (!started_[operator_index]) {
      operators_[operator_index]->Start(start_assignment_);
      started_.Set(operator_index);
    }
    if (!operators_[operator_index]->HoldsDelta()) {
      delta->Clear();
    }
    if (operators_[operator_index]->MakeNextNeighbor(delta, deltadelta)) {
      ++num_neighbors_;
      ++num_neighbors_per_operator_[operator_index];
      return true;
    }
    ++index_;
    delta->Clear();
    if (index_ == operators_.size()) {
      index_ = 0;
    }
  } while (index_ != 0);
  return false;
}
}  // namespace

LocalSearchOperator* Solver::MultiArmedBanditConcatenateOperators(
    const std::vector<LocalSearchOperator*>& ops, double memory_coefficient,
    double exploration_coefficient, bool maximize) {
  return RevAlloc(new MultiArmedBanditCompoundOperator(
      ops, memory_coefficient, exploration_coefficient, maximize));
}

// ----- Operator factory -----

template <class T>
LocalSearchOperator* MakeLocalSearchOperator(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class) {
  return solver->RevAlloc(
      new T(vars, secondary_vars, std::move(start_empty_path_class), nullptr));
}

template <class T, typename ArgType>
LocalSearchOperator* MakeLocalSearchOperatorWithArg(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class, ArgType arg) {
  return solver->RevAlloc(new T(
      vars, secondary_vars, std::move(start_empty_path_class), std::move(arg)));
}

template <class T>
LocalSearchOperator* MakeLocalSearchOperatorWithNeighbors(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    NeighborAccessor get_incoming_neighbors,
    NeighborAccessor get_outgoing_neighbors) {
  return solver->RevAlloc(new T(
      vars, secondary_vars, std::move(start_empty_path_class),
      std::move(get_incoming_neighbors), std::move(get_outgoing_neighbors)));
}

#define MAKE_LOCAL_SEARCH_OPERATOR(OperatorClass)                  \
  template <>                                                      \
  LocalSearchOperator* MakeLocalSearchOperator<OperatorClass>(     \
      Solver * solver, const std::vector<IntVar*>& vars,           \
      const std::vector<IntVar*>& secondary_vars,                  \
      std::function<int(int64_t)> start_empty_path_class) {        \
    return solver->RevAlloc(new OperatorClass(                     \
        vars, secondary_vars, std::move(start_empty_path_class))); \
  }

#define MAKE_LOCAL_SEARCH_OPERATOR_WITH_ARG(OperatorClass, ArgType)            \
  template <>                                                                  \
  LocalSearchOperator* MakeLocalSearchOperatorWithArg<OperatorClass, ArgType>( \
      Solver * solver, const std::vector<IntVar*>& vars,                       \
      const std::vector<IntVar*>& secondary_vars,                              \
      std::function<int(int64_t)> start_empty_path_class, ArgType arg) {       \
    return solver->RevAlloc(                                                   \
        new OperatorClass(vars, secondary_vars,                                \
                          std::move(start_empty_path_class), std::move(arg))); \
  }

#define MAKE_LOCAL_SEARCH_OPERATOR_WITH_NEIGHBORS(OperatorClass)            \
  template <>                                                               \
  LocalSearchOperator* MakeLocalSearchOperatorWithNeighbors<OperatorClass>( \
      Solver * solver, const std::vector<IntVar*>& vars,                    \
      const std::vector<IntVar*>& secondary_vars,                           \
      std::function<int(int64_t)> start_empty_path_class,                   \
      NeighborAccessor get_incoming_neighbors,                              \
      NeighborAccessor get_outgoing_neighbors) {                            \
    return solver->RevAlloc(new OperatorClass(                              \
        vars, secondary_vars, std::move(start_empty_path_class),            \
        std::move(get_incoming_neighbors),                                  \
        std::move(get_outgoing_neighbors)));                                \
  }

MAKE_LOCAL_SEARCH_OPERATOR(TwoOpt)
MAKE_LOCAL_SEARCH_OPERATOR_WITH_NEIGHBORS(TwoOpt)
MAKE_LOCAL_SEARCH_OPERATOR(Relocate)
MAKE_LOCAL_SEARCH_OPERATOR_WITH_NEIGHBORS(Relocate)
MAKE_LOCAL_SEARCH_OPERATOR(Exchange)
MAKE_LOCAL_SEARCH_OPERATOR_WITH_NEIGHBORS(Exchange)
MAKE_LOCAL_SEARCH_OPERATOR(Cross)
MAKE_LOCAL_SEARCH_OPERATOR_WITH_NEIGHBORS(Cross)
MAKE_LOCAL_SEARCH_OPERATOR(MakeActiveOperator)
MAKE_LOCAL_SEARCH_OPERATOR(MakeInactiveOperator)
MAKE_LOCAL_SEARCH_OPERATOR(MakeChainInactiveOperator)
MAKE_LOCAL_SEARCH_OPERATOR(SwapActiveOperator)
MAKE_LOCAL_SEARCH_OPERATOR_WITH_ARG(SwapActiveChainOperator, int)
MAKE_LOCAL_SEARCH_OPERATOR(ExtendedSwapActiveOperator)
MAKE_LOCAL_SEARCH_OPERATOR(MakeActiveAndRelocate)
MAKE_LOCAL_SEARCH_OPERATOR(RelocateAndMakeActiveOperator)
MAKE_LOCAL_SEARCH_OPERATOR(RelocateAndMakeInactiveOperator)

#undef MAKE_LOCAL_SEARCH_OPERATOR

// TODO(user): Remove (parts of) the following methods as they are mostly
// redundant with the MakeLocalSearchOperatorWithNeighbors and
// MakeLocalSearchOperator functions.
LocalSearchOperator* Solver::MakeOperator(
    const std::vector<IntVar*>& vars, Solver::LocalSearchOperators op,
    NeighborAccessor get_incoming_neighbors,
    NeighborAccessor get_outgoing_neighbors) {
  return MakeOperator(vars, std::vector<IntVar*>(), op,
                      std::move(get_incoming_neighbors),
                      std::move(get_outgoing_neighbors));
}

LocalSearchOperator* Solver::MakeOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars, Solver::LocalSearchOperators op,
    NeighborAccessor get_incoming_neighbors,
    NeighborAccessor get_outgoing_neighbors) {
  switch (op) {
    case Solver::TWOOPT: {
      return MakeLocalSearchOperatorWithNeighbors<TwoOpt>(
          this, vars, secondary_vars, nullptr,
          std::move(get_incoming_neighbors), std::move(get_outgoing_neighbors));
    }
    case Solver::OROPT: {
      std::vector<LocalSearchOperator*> operators;
      for (int i = 1; i < 4; ++i) {
        operators.push_back(
            RevAlloc(new Relocate(vars, secondary_vars,
                                  /*name=*/absl::StrCat("OrOpt<", i, ">"),
                                  /*start_empty_path_class=*/nullptr,
                                  /*get_incoming_neighbors=*/nullptr,
                                  /*get_outgoing_neighbors=*/nullptr,
                                  /*chain_length=*/i, /*single_path=*/true)));
      }
      return ConcatenateOperators(operators);
    }
    case Solver::RELOCATE: {
      return MakeLocalSearchOperatorWithNeighbors<Relocate>(
          this, vars, secondary_vars, nullptr,
          std::move(get_incoming_neighbors), std::move(get_outgoing_neighbors));
    }
    case Solver::EXCHANGE: {
      return MakeLocalSearchOperatorWithNeighbors<Exchange>(
          this, vars, secondary_vars, nullptr,
          std::move(get_incoming_neighbors), std::move(get_outgoing_neighbors));
    }
    case Solver::CROSS: {
      return MakeLocalSearchOperatorWithNeighbors<Cross>(
          this, vars, secondary_vars, nullptr,
          std::move(get_incoming_neighbors), std::move(get_outgoing_neighbors));
    }
    case Solver::MAKEACTIVE: {
      return MakeLocalSearchOperator<MakeActiveOperator>(
          this, vars, secondary_vars, nullptr);
    }
    case Solver::MAKEINACTIVE: {
      return MakeLocalSearchOperator<MakeInactiveOperator>(
          this, vars, secondary_vars, nullptr);
    }
    case Solver::MAKECHAININACTIVE: {
      return MakeLocalSearchOperator<MakeChainInactiveOperator>(
          this, vars, secondary_vars, nullptr);
    }
    case Solver::SWAPACTIVE: {
      return MakeLocalSearchOperator<SwapActiveOperator>(
          this, vars, secondary_vars, nullptr);
    }
    case Solver::SWAPACTIVECHAIN: {
      return MakeLocalSearchOperatorWithArg<SwapActiveChainOperator, int>(
          this, vars, secondary_vars, nullptr, kint32max);
    }
    case Solver::EXTENDEDSWAPACTIVE: {
      return MakeLocalSearchOperator<ExtendedSwapActiveOperator>(
          this, vars, secondary_vars, nullptr);
    }
    case Solver::PATHLNS: {
      return RevAlloc(new PathLns(vars, secondary_vars, 2, 3, false));
    }
    case Solver::FULLPATHLNS: {
      return RevAlloc(new PathLns(vars, secondary_vars,
                                  /*number_of_chunks=*/1,
                                  /*chunk_size=*/0,
                                  /*unactive_fragments=*/true));
    }
    case Solver::UNACTIVELNS: {
      return RevAlloc(new PathLns(vars, secondary_vars, 1, 6, true));
    }
    case Solver::INCREMENT: {
      if (!secondary_vars.empty()) {
        LOG(FATAL) << "Operator " << op
                   << " does not support secondary variables";
      }
      return RevAlloc(new IncrementValue(vars));
    }
    case Solver::DECREMENT: {
      if (!secondary_vars.empty()) {
        LOG(FATAL) << "Operator " << op
                   << " does not support secondary variables";
      }
      return RevAlloc(new DecrementValue(vars));
    }
    case Solver::SIMPLELNS: {
      if (!secondary_vars.empty()) {
        LOG(FATAL) << "Operator " << op
                   << " does not support secondary variables";
      }
      return RevAlloc(new SimpleLns(vars, 1));
    }
    default:
      LOG(FATAL) << "Unknown operator " << op;
  }
  return nullptr;
}

LocalSearchOperator* Solver::MakeOperator(
    const std::vector<IntVar*>& vars, Solver::IndexEvaluator3 evaluator,
    Solver::EvaluatorLocalSearchOperators op) {
  return MakeOperator(vars, std::vector<IntVar*>(), std::move(evaluator), op);
}

LocalSearchOperator* Solver::MakeOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    Solver::IndexEvaluator3 evaluator,
    Solver::EvaluatorLocalSearchOperators op) {
  switch (op) {
    case Solver::LK: {
      std::vector<LocalSearchOperator*> operators;
      operators.push_back(RevAlloc(
          new LinKernighan(vars, secondary_vars, evaluator, /*topt=*/false)));
      operators.push_back(RevAlloc(
          new LinKernighan(vars, secondary_vars, evaluator, /*topt=*/true)));
      return ConcatenateOperators(operators);
    }
    case Solver::TSPOPT: {
      return RevAlloc(
          new TSPOpt(vars, secondary_vars, std::move(evaluator),
                     absl::GetFlag(FLAGS_cp_local_search_tsp_opt_size)));
    }
    case Solver::TSPLNS: {
      return RevAlloc(
          new TSPLns(vars, secondary_vars, std::move(evaluator),
                     absl::GetFlag(FLAGS_cp_local_search_tsp_lns_size)));
    }
    default:
      LOG(FATAL) << "Unknown operator " << op;
  }
  return nullptr;
}

namespace {
// Always accepts deltas, cost 0.
class AcceptFilter : public LocalSearchFilter {
 public:
  std::string DebugString() const override { return "AcceptFilter"; }
  bool Accept(const Assignment*, const Assignment*, int64_t, int64_t) override {
    return true;
  }
  void Synchronize(const Assignment*, const Assignment*) override {}
};
}  // namespace

LocalSearchFilter* Solver::MakeAcceptFilter() {
  return RevAlloc(new AcceptFilter());
}

namespace {
// Never accepts deltas, cost 0.
class RejectFilter : public LocalSearchFilter {
 public:
  std::string DebugString() const override { return "RejectFilter"; }
  bool Accept(const Assignment*, const Assignment*, int64_t, int64_t) override {
    return false;
  }
  void Synchronize(const Assignment*, const Assignment*) override {}
};
}  // namespace

LocalSearchFilter* Solver::MakeRejectFilter() {
  return RevAlloc(new RejectFilter());
}

namespace {
// ----- Variable domain filter -----
// Rejects assignments to values outside the domain of variables

class VariableDomainFilter : public LocalSearchFilter {
 public:
  VariableDomainFilter() {}
  ~VariableDomainFilter() override {}
  bool Accept(const Assignment* delta, const Assignment* deltadelta,
              int64_t objective_min, int64_t objective_max) override;
  void Synchronize(const Assignment*, const Assignment*) override {}

  std::string DebugString() const override { return "VariableDomainFilter"; }
};

bool VariableDomainFilter::Accept(const Assignment* delta, const Assignment*,
                                  int64_t, int64_t) {
  const Assignment::IntContainer& container = delta->IntVarContainer();
  const int size = container.Size();
  for (int i = 0; i < size; ++i) {
    const IntVarElement& element = container.Element(i);
    if (element.Activated() && !element.Var()->Contains(element.Value())) {
      return false;
    }
  }
  return true;
}
}  // namespace

LocalSearchFilter* Solver::MakeVariableDomainFilter() {
  return RevAlloc(new VariableDomainFilter());
}

// ----- IntVarLocalSearchFilter -----

const int IntVarLocalSearchFilter::kUnassigned = -1;

IntVarLocalSearchFilter::IntVarLocalSearchFilter(
    const std::vector<IntVar*>& vars) {
  AddVars(vars);
}

void IntVarLocalSearchFilter::AddVars(const std::vector<IntVar*>& vars) {
  if (!vars.empty()) {
    for (int i = 0; i < vars.size(); ++i) {
      const int index = vars[i]->index();
      if (index >= var_index_to_index_.size()) {
        var_index_to_index_.resize(index + 1, kUnassigned);
      }
      var_index_to_index_[index] = i + vars_.size();
    }
    vars_.insert(vars_.end(), vars.begin(), vars.end());
    values_.resize(vars_.size(), /*junk*/ 0);
    var_synced_.resize(vars_.size(), false);
  }
}

IntVarLocalSearchFilter::~IntVarLocalSearchFilter() {}

void IntVarLocalSearchFilter::Synchronize(const Assignment* assignment,
                                          const Assignment* delta) {
  if (delta == nullptr || delta->Empty()) {
    var_synced_.assign(var_synced_.size(), false);
    SynchronizeOnAssignment(assignment);
  } else {
    SynchronizeOnAssignment(delta);
  }
  OnSynchronize(delta);
}

void IntVarLocalSearchFilter::SynchronizeOnAssignment(
    const Assignment* assignment) {
  const Assignment::IntContainer& container = assignment->IntVarContainer();
  const int size = container.Size();
  for (int i = 0; i < size; ++i) {
    const IntVarElement& element = container.Element(i);
    IntVar* const var = element.Var();
    if (var != nullptr) {
      if (i < vars_.size() && vars_[i] == var) {
        values_[i] = element.Value();
        var_synced_[i] = true;
      } else {
        const int64_t kUnallocated = -1;
        int64_t index = kUnallocated;
        if (FindIndex(var, &index)) {
          values_[index] = element.Value();
          var_synced_[index] = true;
        }
      }
    }
  }
}

// ----- Sum Objective filter ------
// Maintains the sum of costs of variables, where the subclass implements
// CostOfSynchronizedVariable() and FillCostOfBoundDeltaVariable() to compute
// the cost of a variable depending on its value.
// An assignment is accepted by this filter if the total cost is allowed
// depending on the relation defined by filter_enum:
// - Solver::LE -> total_cost <= objective_max.
// - Solver::GE -> total_cost >= objective_min.
// - Solver::EQ -> the conjunction of LE and GE.
namespace {
template <typename Filter>
class SumObjectiveFilter : public IntVarLocalSearchFilter {
 public:
  SumObjectiveFilter(const std::vector<IntVar*>& vars, Filter filter)
      : IntVarLocalSearchFilter(vars),
        primary_vars_size_(vars.size()),
        synchronized_costs_(vars.size()),
        delta_costs_(vars.size()),
        filter_(std::move(filter)),
        synchronized_sum_(std::numeric_limits<int64_t>::min()),
        delta_sum_(std::numeric_limits<int64_t>::min()),
        incremental_(false) {}
  ~SumObjectiveFilter() override {}
  bool Accept(const Assignment* delta, const Assignment* deltadelta,
              int64_t objective_min, int64_t objective_max) override {
    if (delta == nullptr) return false;
    if (deltadelta->Empty()) {
      if (incremental_) {
        for (int i = 0; i < primary_vars_size_; ++i) {
          delta_costs_[i] = synchronized_costs_[i];
        }
      }
      incremental_ = false;
      delta_sum_ = CapAdd(synchronized_sum_, CostOfChanges(delta, false));
    } else {
      if (incremental_) {
        delta_sum_ = CapAdd(delta_sum_, CostOfChanges(deltadelta, true));
      } else {
        delta_sum_ = CapAdd(synchronized_sum_, CostOfChanges(delta, true));
      }
      incremental_ = true;
    }
    return filter_(delta_sum_, objective_min, objective_max);
  }
  // If the variable is synchronized, returns its associated cost, otherwise
  // returns 0.
  virtual int64_t CostOfSynchronizedVariable(int64_t index) = 0;
  // Returns the cost of applying changes to the current solution.
  virtual int64_t CostOfChanges(const Assignment* changes,
                                bool incremental) = 0;
  bool IsIncremental() const override { return true; }

  std::string DebugString() const override { return "SumObjectiveFilter"; }

  int64_t GetSynchronizedObjectiveValue() const override {
    return synchronized_sum_;
  }
  int64_t GetAcceptedObjectiveValue() const override { return delta_sum_; }

 protected:
  const int primary_vars_size_;
  std::vector<int64_t> synchronized_costs_;
  std::vector<int64_t> delta_costs_;
  Filter filter_;
  int64_t synchronized_sum_;
  int64_t delta_sum_;
  bool incremental_;

 private:
  void OnSynchronize(const Assignment*) override {
    synchronized_sum_ = 0;
    for (int i = 0; i < primary_vars_size_; ++i) {
      const int64_t cost = CostOfSynchronizedVariable(i);
      synchronized_costs_[i] = cost;
      delta_costs_[i] = cost;
      synchronized_sum_ = CapAdd(synchronized_sum_, cost);
    }
    delta_sum_ = synchronized_sum_;
    incremental_ = false;
  }
};

template <typename Filter>
class BinaryObjectiveFilter : public SumObjectiveFilter<Filter> {
 public:
  BinaryObjectiveFilter(const std::vector<IntVar*>& vars,
                        Solver::IndexEvaluator2 value_evaluator, Filter filter)
      : SumObjectiveFilter<Filter>(vars, std::move(filter)),
        value_evaluator_(std::move(value_evaluator)) {}
  ~BinaryObjectiveFilter() override {}
  int64_t CostOfSynchronizedVariable(int64_t index) override {
    return IntVarLocalSearchFilter::IsVarSynced(index)
               ? value_evaluator_(index, IntVarLocalSearchFilter::Value(index))
               : 0;
  }
  int64_t CostOfChanges(const Assignment* changes, bool incremental) override {
    int64_t total_cost = 0;
    const Assignment::IntContainer& container = changes->IntVarContainer();
    for (const IntVarElement& new_element : container.elements()) {
      IntVar* const var = new_element.Var();
      int64_t index = -1;
      if (this->FindIndex(var, &index)) {
        total_cost = CapSub(total_cost, this->delta_costs_[index]);
        int64_t new_cost = 0LL;
        if (new_element.Activated()) {
          new_cost = value_evaluator_(index, new_element.Value());
        } else if (var->Bound()) {
          new_cost = value_evaluator_(index, var->Min());
        }
        total_cost = CapAdd(total_cost, new_cost);
        if (incremental) {
          this->delta_costs_[index] = new_cost;
        }
      }
    }
    return total_cost;
  }

 private:
  Solver::IndexEvaluator2 value_evaluator_;
};

template <typename Filter>
class TernaryObjectiveFilter : public SumObjectiveFilter<Filter> {
 public:
  TernaryObjectiveFilter(const std::vector<IntVar*>& vars,
                         const std::vector<IntVar*>& secondary_vars,
                         Solver::IndexEvaluator3 value_evaluator, Filter filter)
      : SumObjectiveFilter<Filter>(vars, std::move(filter)),
        secondary_vars_offset_(vars.size()),
        secondary_values_(vars.size(), -1),
        value_evaluator_(std::move(value_evaluator)) {
    IntVarLocalSearchFilter::AddVars(secondary_vars);
    CHECK_GE(IntVarLocalSearchFilter::Size(), 0);
  }
  ~TernaryObjectiveFilter() override {}
  int64_t CostOfSynchronizedVariable(int64_t index) override {
    DCHECK_LT(index, secondary_vars_offset_);
    return IntVarLocalSearchFilter::IsVarSynced(index)
               ? value_evaluator_(index, IntVarLocalSearchFilter::Value(index),
                                  IntVarLocalSearchFilter::Value(
                                      index + secondary_vars_offset_))
               : 0;
  }
  int64_t CostOfChanges(const Assignment* changes, bool incremental) override {
    int64_t total_cost = 0;
    const Assignment::IntContainer& container = changes->IntVarContainer();
    for (const IntVarElement& new_element : container.elements()) {
      IntVar* const var = new_element.Var();
      int64_t index = -1;
      if (this->FindIndex(var, &index) && index < secondary_vars_offset_) {
        secondary_values_[index] = -1;
      }
    }
    // Primary variable indices range from 0 to secondary_vars_offset_ - 1,
    // matching secondary indices from secondary_vars_offset_ to
    // 2 * secondary_vars_offset_ - 1.
    const int max_secondary_index = 2 * secondary_vars_offset_;
    for (const IntVarElement& new_element : container.elements()) {
      IntVar* const var = new_element.Var();
      int64_t index = -1;
      if (new_element.Activated() && this->FindIndex(var, &index) &&
          index >= secondary_vars_offset_ &&
          // Only consider secondary_variables linked to primary ones.
          index < max_secondary_index) {
        secondary_values_[index - secondary_vars_offset_] = new_element.Value();
      }
    }
    for (const IntVarElement& new_element : container.elements()) {
      IntVar* const var = new_element.Var();
      int64_t index = -1;
      if (this->FindIndex(var, &index) && index < secondary_vars_offset_) {
        total_cost = CapSub(total_cost, this->delta_costs_[index]);
        int64_t new_cost = 0LL;
        if (new_element.Activated()) {
          new_cost = value_evaluator_(index, new_element.Value(),
                                      secondary_values_[index]);
        } else if (var->Bound() &&
                   IntVarLocalSearchFilter::Var(index + secondary_vars_offset_)
                       ->Bound()) {
          new_cost = value_evaluator_(
              index, var->Min(),
              IntVarLocalSearchFilter::Var(index + secondary_vars_offset_)
                  ->Min());
        }
        total_cost = CapAdd(total_cost, new_cost);
        if (incremental) {
          this->delta_costs_[index] = new_cost;
        }
      }
    }
    return total_cost;
  }

 private:
  int secondary_vars_offset_;
  std::vector<int64_t> secondary_values_;
  Solver::IndexEvaluator3 value_evaluator_;
};
}  // namespace

IntVarLocalSearchFilter* Solver::MakeSumObjectiveFilter(
    const std::vector<IntVar*>& vars, Solver::IndexEvaluator2 values,
    Solver::LocalSearchFilterBound filter_enum) {
  switch (filter_enum) {
    case Solver::LE: {
      auto filter = [](int64_t value, int64_t, int64_t max_value) {
        return value <= max_value;
      };
      return RevAlloc(new BinaryObjectiveFilter<decltype(filter)>(
          vars, std::move(values), std::move(filter)));
    }
    case Solver::GE: {
      auto filter = [](int64_t value, int64_t min_value, int64_t) {
        return value >= min_value;
      };
      return RevAlloc(new BinaryObjectiveFilter<decltype(filter)>(
          vars, std::move(values), std::move(filter)));
    }
    case Solver::EQ: {
      auto filter = [](int64_t value, int64_t min_value, int64_t max_value) {
        return min_value <= value && value <= max_value;
      };
      return RevAlloc(new BinaryObjectiveFilter<decltype(filter)>(
          vars, std::move(values), std::move(filter)));
    }
    default: {
      LOG(ERROR) << "Unknown local search filter enum value";
      return nullptr;
    }
  }
}

IntVarLocalSearchFilter* Solver::MakeSumObjectiveFilter(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars, Solver::IndexEvaluator3 values,
    Solver::LocalSearchFilterBound filter_enum) {
  switch (filter_enum) {
    case Solver::LE: {
      auto filter = [](int64_t value, int64_t, int64_t max_value) {
        return value <= max_value;
      };
      return RevAlloc(new TernaryObjectiveFilter<decltype(filter)>(
          vars, secondary_vars, std::move(values), std::move(filter)));
    }
    case Solver::GE: {
      auto filter = [](int64_t value, int64_t min_value, int64_t) {
        return value >= min_value;
      };
      return RevAlloc(new TernaryObjectiveFilter<decltype(filter)>(
          vars, secondary_vars, std::move(values), std::move(filter)));
    }
    case Solver::EQ: {
      auto filter = [](int64_t value, int64_t min_value, int64_t max_value) {
        return min_value <= value && value <= max_value;
      };
      return RevAlloc(new TernaryObjectiveFilter<decltype(filter)>(
          vars, secondary_vars, std::move(values), std::move(filter)));
    }
    default: {
      LOG(ERROR) << "Unknown local search filter enum value";
      return nullptr;
    }
  }
}

void SubDagComputer::BuildGraph(int num_nodes) {
  DCHECK_GE(num_nodes, num_nodes_);
  DCHECK(!graph_was_built_);
  graph_was_built_ = true;
  std::sort(arcs_.begin(), arcs_.end());
  arcs_of_node_.clear();
  NodeId prev_tail(-1);
  for (int a = 0; a < arcs_.size(); ++a) {
    const NodeId tail = arcs_[a].tail;
    if (tail != prev_tail) {
      prev_tail = tail;
      arcs_of_node_.resize(tail.value() + 1, a);
    }
  }
  num_nodes_ = std::max(num_nodes_, num_nodes);
  arcs_of_node_.resize(num_nodes_ + 1, arcs_.size());
  DCHECK(!HasDirectedCycle()) << "Graph has a directed cycle";
}

bool SubDagComputer::HasDirectedCycle() const {
  DCHECK(graph_was_built_);
  util_intops::StrongVector<NodeId, bool> node_is_open(num_nodes_, false);
  util_intops::StrongVector<NodeId, bool> node_was_visited(num_nodes_, false);
  // Depth first search event: a node and a boolean indicating whether
  // to open or to close it.
  struct DFSEvent {
    NodeId node;
    bool to_open;
  };
  std::vector<DFSEvent> event_stack;

  for (NodeId node(0); node.value() < num_nodes_; ++node) {
    if (node_was_visited[node]) continue;
    event_stack.push_back({node, true});
    while (!event_stack.empty()) {
      const auto [node, to_open] = event_stack.back();
      event_stack.pop_back();
      if (node_was_visited[node]) continue;
      if (to_open) {
        if (node_is_open[node]) return true;
        node_is_open[node] = true;
        event_stack.push_back({node, false});
        for (int a = arcs_of_node_[node];
             a < arcs_of_node_[NodeId(node.value() + 1)]; ++a) {
          const NodeId head = arcs_[a].head;
          if (node_was_visited[head]) continue;  // Optim, keeps stack smaller.
          event_stack.push_back({head, true});
        }
      } else {
        node_was_visited[node] = true;
        node_is_open[node] = false;
      }
    }
  }
  return false;
}

const std::vector<SubDagComputer::ArcId>&
SubDagComputer::ComputeSortedSubDagArcs(NodeId node) {
  DCHECK_LT(node.value(), num_nodes_);
  DCHECK(graph_was_built_);
  if (indegree_of_node_.size() < num_nodes_) {
    indegree_of_node_.resize(num_nodes_, 0);
  }
  // Compute indegrees of nodes of the sub-DAG reachable from node.
  nodes_to_visit_.clear();
  nodes_to_visit_.push_back(node);
  while (!nodes_to_visit_.empty()) {
    const NodeId node = nodes_to_visit_.back();
    nodes_to_visit_.pop_back();
    const NodeId next(node.value() + 1);
    for (int a = arcs_of_node_[node]; a < arcs_of_node_[next]; ++a) {
      const NodeId head = arcs_[a].head;
      if (indegree_of_node_[head] == 0) {
        nodes_to_visit_.push_back(head);
      }
      ++indegree_of_node_[head];
    }
  }
  // Generate arc ordering by iteratively removing zero-indegree nodes.
  sorted_arcs_.clear();
  nodes_to_visit_.push_back(node);
  while (!nodes_to_visit_.empty()) {
    const NodeId node = nodes_to_visit_.back();
    nodes_to_visit_.pop_back();
    const NodeId next(node.value() + 1);
    for (int a = arcs_of_node_[node]; a < arcs_of_node_[next]; ++a) {
      const NodeId head = arcs_[a].head;
      --indegree_of_node_[head];
      if (indegree_of_node_[head] == 0) {
        nodes_to_visit_.push_back(head);
      }
      sorted_arcs_.push_back(arcs_[a].arc_id);
    }
  }
  // Invariant: indegree_of_node_ must be all 0, or the graph has a cycle.
  DCHECK(absl::c_all_of(indegree_of_node_, [](int x) { return x == 0; }));
  return sorted_arcs_;
}

using VariableDomainId = LocalSearchState::VariableDomainId;
VariableDomainId LocalSearchState::AddVariableDomain(int64_t relaxed_min,
                                                     int64_t relaxed_max) {
  DCHECK(state_domains_are_all_nonempty_);
  DCHECK_LE(relaxed_min, relaxed_max);
  relaxed_domains_.push_back({relaxed_min, relaxed_max});
  current_domains_.push_back({relaxed_min, relaxed_max});
  domain_is_trailed_.push_back(false);
  return VariableDomainId(current_domains_.size() - 1);
}

LocalSearchState::Variable LocalSearchState::MakeVariable(
    VariableDomainId domain_id) {
  return {this, domain_id};
}

LocalSearchState::Variable LocalSearchState::MakeVariableWithRelaxedDomain(
    int64_t min, int64_t max) {
  const VariableDomainId domain_id = AddVariableDomain(min, max);
  return {this, domain_id};
}

LocalSearchState::Variable LocalSearchState::DummyVariable() {
  return {nullptr, VariableDomainId(-1)};
}

bool LocalSearchState::RelaxVariableDomain(VariableDomainId domain_id) {
  DCHECK(state_domains_are_all_nonempty_);
  if (!state_has_relaxed_domains_) {
    trailed_num_committed_empty_domains_ = num_committed_empty_domains_;
  }
  state_has_relaxed_domains_ = true;
  if (!domain_is_trailed_[domain_id]) {
    domain_is_trailed_[domain_id] = true;
    trailed_domains_.push_back({current_domains_[domain_id], domain_id});
    if (IntersectionIsEmpty(relaxed_domains_[domain_id],
                            current_domains_[domain_id])) {
      DCHECK_GT(num_committed_empty_domains_, 0);
      --num_committed_empty_domains_;
    }
    current_domains_[domain_id] = relaxed_domains_[domain_id];
    return true;
  }
  return false;
}

int64_t LocalSearchState::VariableDomainMin(VariableDomainId domain_id) const {
  DCHECK(state_domains_are_all_nonempty_);
  return current_domains_[domain_id].min;
}

int64_t LocalSearchState::VariableDomainMax(VariableDomainId domain_id) const {
  DCHECK(state_domains_are_all_nonempty_);
  return current_domains_[domain_id].max;
}

bool LocalSearchState::TightenVariableDomainMin(VariableDomainId domain_id,
                                                int64_t min_value) {
  DCHECK(state_domains_are_all_nonempty_);
  DCHECK(domain_is_trailed_[domain_id]);
  VariableDomain& domain = current_domains_[domain_id];
  if (domain.max < min_value) {
    state_domains_are_all_nonempty_ = false;
  }
  domain.min = std::max(domain.min, min_value);
  return state_domains_are_all_nonempty_;
}

bool LocalSearchState::TightenVariableDomainMax(VariableDomainId domain_id,
                                                int64_t max_value) {
  DCHECK(state_domains_are_all_nonempty_);
  DCHECK(domain_is_trailed_[domain_id]);
  VariableDomain& domain = current_domains_[domain_id];
  if (domain.min > max_value) {
    state_domains_are_all_nonempty_ = false;
  }
  domain.max = std::min(domain.max, max_value);
  return state_domains_are_all_nonempty_;
}

void LocalSearchState::ChangeRelaxedVariableDomain(VariableDomainId domain_id,
                                                   int64_t min, int64_t max) {
  DCHECK(state_domains_are_all_nonempty_);
  DCHECK(!domain_is_trailed_[domain_id]);
  const bool domain_was_empty = IntersectionIsEmpty(
      relaxed_domains_[domain_id], current_domains_[domain_id]);
  relaxed_domains_[domain_id] = {min, max};
  const bool domain_is_empty =
      IntersectionIsEmpty({min, max}, current_domains_[domain_id]);

  if (!domain_was_empty && domain_is_empty) {
    num_committed_empty_domains_++;
  } else if (domain_was_empty && !domain_is_empty) {
    num_committed_empty_domains_--;
  }
}

// TODO(user): When the class has more users, find a threshold ratio of
// saved/total domains under which a sparse clear would be more efficient
// for both Commit() and Revert().
void LocalSearchState::Commit() {
  DCHECK(StateIsFeasible());
  // Clear domains trail.
  state_has_relaxed_domains_ = false;
  trailed_domains_.clear();
  domain_is_trailed_.assign(domain_is_trailed_.size(), false);
  // Clear constraint trail.
  for (Constraint* constraint : trailed_constraints_) {
    constraint->Commit();
  }
  trailed_constraints_.clear();
}

void LocalSearchState::Revert() {
  // Revert trailed domains.
  for (const auto& [domain, id] : trailed_domains_) {
    DCHECK(domain_is_trailed_[id]);
    current_domains_[id] = domain;
  }
  trailed_domains_.clear();
  state_has_relaxed_domains_ = false;
  domain_is_trailed_.assign(domain_is_trailed_.size(), false);
  state_domains_are_all_nonempty_ = true;
  num_committed_empty_domains_ = trailed_num_committed_empty_domains_;
  // Revert trailed constraints.
  for (Constraint* constraint : trailed_constraints_) {
    constraint->Revert();
  }
  trailed_constraints_.clear();
}

using NodeId = SubDagComputer::NodeId;
NodeId LocalSearchState::DependencyGraph::GetOrCreateNodeOfDomainId(
    VariableDomainId domain_id) {
  if (domain_id.value() >= dag_node_of_domain_.size()) {
    dag_node_of_domain_.resize(domain_id.value() + 1, NodeId(0));
  }
  if (dag_node_of_domain_[domain_id] == NodeId(0)) {
    dag_node_of_domain_[domain_id] = NodeId(num_dag_nodes_++);
  }
  return dag_node_of_domain_[domain_id];
}

NodeId LocalSearchState::DependencyGraph::GetOrCreateNodeOfConstraintId(
    ConstraintId constraint_id) {
  if (constraint_id.value() >= dag_node_of_constraint_.size()) {
    dag_node_of_constraint_.resize(constraint_id.value() + 1, NodeId(0));
  }
  if (dag_node_of_constraint_[constraint_id] == NodeId(0)) {
    dag_node_of_constraint_[constraint_id] = NodeId(num_dag_nodes_++);
  }
  return dag_node_of_constraint_[constraint_id];
}

void LocalSearchState::DependencyGraph::AddDomainsConstraintDependencies(
    const std::vector<VariableDomainId>& domain_ids,
    ConstraintId constraint_id) {
  const NodeId cnode = GetOrCreateNodeOfConstraintId(constraint_id);
  for (int i = 0; i < domain_ids.size(); ++i) {
    const NodeId dnode = GetOrCreateNodeOfDomainId(domain_ids[i]);
    dag_.AddArc(dnode, cnode);
    dependency_of_dag_arc_.push_back({.domain_id = domain_ids[i],
                                      .input_index = i,
                                      .constraint_id = constraint_id});
  }
}

void LocalSearchState::DependencyGraph::AddConstraintDomainDependency(
    ConstraintId constraint_id, VariableDomainId domain_id) {
  const NodeId cnode = GetOrCreateNodeOfConstraintId(constraint_id);
  const NodeId dnode = GetOrCreateNodeOfDomainId(domain_id);
  dag_.AddArc(cnode, dnode);
  dependency_of_dag_arc_.push_back({.domain_id = domain_id,
                                    .input_index = -1,
                                    .constraint_id = constraint_id});
}

using ArcId = SubDagComputer::ArcId;
const std::vector<LocalSearchState::DependencyGraph::Dependency>&
LocalSearchState::DependencyGraph::ComputeSortedDependencies(
    VariableDomainId domain_id) {
  sorted_dependencies_.clear();
  const NodeId node = dag_node_of_domain_[domain_id];
  for (const ArcId a : dag_.ComputeSortedSubDagArcs(node)) {
    if (dependency_of_dag_arc_[a].input_index == -1) continue;
    sorted_dependencies_.push_back(dependency_of_dag_arc_[a]);
  }
  return sorted_dependencies_;
}

void LocalSearchState::AddWeightedSumConstraint(
    const std::vector<VariableDomainId>& input_domain_ids,
    const std::vector<int64_t>& input_weights, int64_t input_offset,
    VariableDomainId output_domain_id) {
  DCHECK_EQ(input_domain_ids.size(), input_weights.size());
  // Store domain/constraint dependencies.
  const ConstraintId constraint_id(constraints_.size());
  dependency_graph_.AddDomainsConstraintDependencies(input_domain_ids,
                                                     constraint_id);
  dependency_graph_.AddConstraintDomainDependency(constraint_id,
                                                  output_domain_id);
  // Store constraint.
  auto constraint = std::make_unique<WeightedSum>(
      this, input_domain_ids, input_weights, input_offset, output_domain_id);
  constraints_.push_back(std::move(constraint));
}

void LocalSearchState::DependencyGraph::BuildDependencyDAG(int num_domains) {
  dag_.BuildGraph(num_dag_nodes_);
  // Assign all unassigned nodes to dummy node 0.
  const int num_assigned_nodes = dag_node_of_domain_.size();
  DCHECK_GE(num_domains, num_assigned_nodes);
  num_domains = std::max(num_domains, num_assigned_nodes);
  dag_node_of_domain_.resize(num_domains, NodeId(0));
}

void LocalSearchState::CompileConstraints() {
  triggers_.clear();
  triggers_of_domain_.clear();
  const int num_domains = current_domains_.size();
  dependency_graph_.BuildDependencyDAG(num_domains);
  for (int vid = 0; vid < num_domains; ++vid) {
    triggers_of_domain_.push_back(triggers_.size());
    for (const auto& [domain_id, input_index, constraint_id] :
         dependency_graph_.ComputeSortedDependencies(VariableDomainId(vid))) {
      triggers_.push_back({.constraint = constraints_[constraint_id].get(),
                           .input_index = input_index});
    }
  }
  triggers_of_domain_.push_back(triggers_.size());
}

LocalSearchState::WeightedSum::WeightedSum(
    LocalSearchState* state,
    const std::vector<VariableDomainId>& input_domain_ids,
    const std::vector<int64_t>& input_weights, int64_t input_offset,
    VariableDomainId output)
    : output_(output), state_(state) {
  // Make trailable values that mirror last known value of inputs,
  // and others to represent internal counts and sums of inputs.
  invariants_.num_neg_inf = 0;
  invariants_.num_pos_inf = 0;
  invariants_.wsum_mins = input_offset;
  invariants_.wsum_maxs = input_offset;
  for (int i = 0; i < input_domain_ids.size(); ++i) {
    const VariableDomainId domain_id = input_domain_ids[i];
    const int64_t weight = input_weights[i];
    const int64_t min = state->VariableDomainMin(domain_id);
    const int64_t max = state->VariableDomainMax(domain_id);
    inputs_.push_back({.min = min,
                       .max = max,
                       .committed_min = min,
                       .committed_max = max,
                       .weight = weight,
                       .domain = domain_id,
                       .is_trailed = false});

    if (weight > 0) {
      if (min == kint64min) {
        ++invariants_.num_neg_inf;
      } else {
        invariants_.wsum_mins =
            CapAdd(invariants_.wsum_mins, CapProd(weight, min));
      }
      if (max == kint64max) {
        ++invariants_.num_pos_inf;
      } else {
        invariants_.wsum_maxs =
            CapAdd(invariants_.wsum_maxs, CapProd(weight, max));
      }
    } else {
      if (min == kint64min) {
        ++invariants_.num_pos_inf;
      } else {
        invariants_.wsum_maxs =
            CapAdd(invariants_.wsum_maxs, CapProd(weight, min));
      }
      if (max == kint64max) {
        ++invariants_.num_neg_inf;
      } else {
        invariants_.wsum_mins =
            CapAdd(invariants_.wsum_mins, CapProd(weight, max));
      }
    }
  }
  committed_invariants_ = invariants_;
}

LocalSearchState::VariableDomain LocalSearchState::WeightedSum::Propagate(
    int input_index) {
  if (!constraint_is_trailed_) {
    constraint_is_trailed_ = true;
    state_->TrailConstraint(this);
  }
  WeightedVariable& input = inputs_[input_index];
  if (!input.is_trailed) {
    input.is_trailed = true;
    trailed_inputs_.push_back(&input);
  }
  const int64_t new_min = state_->VariableDomainMin(input.domain);
  const int64_t new_max = state_->VariableDomainMax(input.domain);
  const int64_t weight = input.weight;
  if (weight > 0) {
    if (input.min != new_min) {
      // Remove contribution of last known min.
      if (input.min == kint64min) {
        --invariants_.num_neg_inf;
      } else {
        invariants_.wsum_mins =
            CapSub(invariants_.wsum_mins, CapProd(weight, input.min));
      }
      // Add contribution of new min.
      if (new_min == kint64min) {
        ++invariants_.num_neg_inf;
      } else {
        invariants_.wsum_mins =
            CapAdd(invariants_.wsum_mins, CapProd(weight, new_min));
      }
      input.min = new_min;
    }
    if (input.max != new_max) {
      // Remove contribution of last known max.
      if (input.max == kint64max) {
        --invariants_.num_pos_inf;
      } else {
        invariants_.wsum_maxs =
            CapSub(invariants_.wsum_maxs, CapProd(weight, input.max));
      }
      // Add contribution of new max.
      if (new_max == kint64max) {
        ++invariants_.num_pos_inf;
      } else {
        invariants_.wsum_maxs =
            CapAdd(invariants_.wsum_maxs, CapProd(weight, new_max));
      }
      input.max = new_max;
    }
  } else {  // weight <= 0
    if (input.min != new_min) {
      // Remove contribution of last known min.
      if (input.min == kint64min) {
        --invariants_.num_pos_inf;
      } else {
        invariants_.wsum_maxs =
            CapSub(invariants_.wsum_maxs, CapProd(weight, input.min));
      }
      // Add contribution of new min.
      if (new_min == kint64min) {
        ++invariants_.num_pos_inf;
      } else {
        invariants_.wsum_maxs =
            CapAdd(invariants_.wsum_maxs, CapProd(weight, new_min));
      }
      input.min = new_min;
    }
    if (input.max != new_max) {
      // Remove contribution of last known max.
      if (input.max == kint64max) {
        --invariants_.num_neg_inf;
      } else {
        invariants_.wsum_mins =
            CapSub(invariants_.wsum_mins, CapProd(weight, input.max));
      }
      // Add contribution of new max.
      if (new_max == kint64max) {
        ++invariants_.num_neg_inf;
      } else {
        invariants_.wsum_mins =
            CapAdd(invariants_.wsum_mins, CapProd(weight, new_max));
      }
      input.max = new_max;
    }
  }
  return {invariants_.num_neg_inf == 0 ? invariants_.wsum_mins : kint64min,
          invariants_.num_pos_inf == 0 ? invariants_.wsum_maxs : kint64max};
}

void LocalSearchState::WeightedSum::Commit() {
  committed_invariants_ = invariants_;
  constraint_is_trailed_ = false;
  for (WeightedVariable* wv : trailed_inputs_) wv->Commit();
  trailed_inputs_.clear();
}

void LocalSearchState::WeightedSum::Revert() {
  invariants_ = committed_invariants_;
  constraint_is_trailed_ = false;
  for (WeightedVariable* wv : trailed_inputs_) wv->Revert();
  trailed_inputs_.clear();
}

void LocalSearchState::PropagateRelax(VariableDomainId domain_id) {
  const VariableDomainId next_id = VariableDomainId(domain_id.value() + 1);
  for (int t = triggers_of_domain_[domain_id]; t < triggers_of_domain_[next_id];
       ++t) {
    const auto& [constraint, input_index] = triggers_[t];
    constraint->Propagate(input_index);
    RelaxVariableDomain(constraint->Output());
  }
}

bool LocalSearchState::PropagateTighten(VariableDomainId domain_id) {
  const VariableDomainId next_id = VariableDomainId(domain_id.value() + 1);
  for (int t = triggers_of_domain_[domain_id]; t < triggers_of_domain_[next_id];
       ++t) {
    const auto& [constraint, input_index] = triggers_[t];
    const auto [output_min, output_max] = constraint->Propagate(input_index);
    if (output_min != kint64min &&
        !TightenVariableDomainMin(constraint->Output(), output_min)) {
      return false;
    }
    if (output_max != kint64max &&
        !TightenVariableDomainMax(constraint->Output(), output_max)) {
      return false;
    }
  }
  return true;
}

// ----- LocalSearchProfiler -----

class LocalSearchProfiler : public LocalSearchMonitor {
 public:
  explicit LocalSearchProfiler(Solver* solver) : LocalSearchMonitor(solver) {}
  std::string DebugString() const override { return "LocalSearchProfiler"; }
  void RestartSearch() override {
    operator_stats_.clear();
    filter_stats_per_context_.clear();
    last_operator_ = nullptr;
  }
  void ExitSearch() override {
    // Update times for current operator when the search ends.
    UpdateTime();
    last_operator_ = nullptr;
  }
  template <typename Callback>
  void ParseFirstSolutionStatistics(const Callback& callback) const {
    for (ProfiledDecisionBuilder* db : profiled_decision_builders_) {
      if (db->seconds() == 0) continue;
      callback(db->name(), db->seconds());
    }
  }

  template <typename Callback>
  void ParseLocalSearchOperatorStatistics(const Callback& callback) const {
    std::vector<const LocalSearchOperator*> operators;
    for (const auto& stat : operator_stats_) {
      operators.push_back(stat.first);
    }
    std::sort(
        operators.begin(), operators.end(),
        [this](const LocalSearchOperator* op1, const LocalSearchOperator* op2) {
          return gtl::FindOrDie(operator_stats_, op1).neighbors >
                 gtl::FindOrDie(operator_stats_, op2).neighbors;
        });
    for (const LocalSearchOperator* const op : operators) {
      const OperatorStats& stats = gtl::FindOrDie(operator_stats_, op);
      const std::string& label = op->DebugString();
      // Skip operators with no name: these come from empty compound operators.
      if (label.empty() &&
          dynamic_cast<const CompoundOperator*>(op) != nullptr) {
        continue;
      }
      callback(label, stats.neighbors, stats.filtered_neighbors,
               stats.accepted_neighbors, stats.seconds,
               stats.make_next_neighbor_seconds, stats.accept_neighbor_seconds);
    }
  }

  template <typename Callback>
  void ParseLocalSearchFilterStatistics(const Callback& callback) const {
    for (const auto& [context, filter_stats] : filter_stats_per_context_) {
      std::vector<const LocalSearchFilter*> filters;
      for (const auto& [filter, stats] : filter_stats) {
        filters.push_back(filter);
      }
      std::sort(
          filters.begin(), filters.end(),
          [filter_stats_ptr = &filter_stats](const LocalSearchFilter* filter1,
                                             const LocalSearchFilter* filter2) {
            return gtl::FindOrDie(*filter_stats_ptr, filter1).calls >
                   gtl::FindOrDie(*filter_stats_ptr, filter2).calls;
          });
      for (const LocalSearchFilter* const filter : filters) {
        const FilterStats& stats = gtl::FindOrDie(filter_stats, filter);
        callback(context, filter->DebugString(), stats.calls, stats.rejects,
                 stats.seconds);
      }
    }
  }
  LocalSearchStatistics ExportToLocalSearchStatistics() const {
    LocalSearchStatistics statistics_proto;
    ParseFirstSolutionStatistics(
        [&statistics_proto](absl::string_view name, double duration_seconds) {
          LocalSearchStatistics::FirstSolutionStatistics* const
              first_solution_statistics =
                  statistics_proto.add_first_solution_statistics();
          first_solution_statistics->set_strategy(name);
          first_solution_statistics->set_duration_seconds(duration_seconds);
        });
    ParseLocalSearchOperatorStatistics(
        [&statistics_proto](
            absl::string_view name, int64_t num_neighbors,
            int64_t num_filtered_neighbors, int64_t num_accepted_neighbors,
            double duration_seconds, double make_next_neighbor_duration_seconds,
            double accept_neighbor_duration_seconds) {
          LocalSearchStatistics::LocalSearchOperatorStatistics* const
              local_search_operator_statistics =
                  statistics_proto.add_local_search_operator_statistics();
          local_search_operator_statistics->set_local_search_operator(name);
          local_search_operator_statistics->set_num_neighbors(num_neighbors);
          local_search_operator_statistics->set_num_filtered_neighbors(
              num_filtered_neighbors);
          local_search_operator_statistics->set_num_accepted_neighbors(
              num_accepted_neighbors);
          local_search_operator_statistics->set_duration_seconds(
              duration_seconds);
          local_search_operator_statistics
              ->set_make_next_neighbor_duration_seconds(
                  make_next_neighbor_duration_seconds);
          local_search_operator_statistics
              ->set_accept_neighbor_duration_seconds(
                  accept_neighbor_duration_seconds);
        });
    ParseLocalSearchFilterStatistics([&statistics_proto](
                                         absl::string_view context,
                                         absl::string_view name,
                                         int64_t num_calls, int64_t num_rejects,
                                         double duration_seconds) {
      LocalSearchStatistics::LocalSearchFilterStatistics* const
          local_search_filter_statistics =
              statistics_proto.add_local_search_filter_statistics();
      local_search_filter_statistics->set_local_search_filter(name);
      local_search_filter_statistics->set_num_calls(num_calls);
      local_search_filter_statistics->set_num_rejects(num_rejects);
      local_search_filter_statistics->set_duration_seconds(duration_seconds);
      local_search_filter_statistics->set_num_rejects_per_second(
          num_rejects / duration_seconds);
      local_search_filter_statistics->set_context(context);
    });
    statistics_proto.set_total_num_neighbors(solver()->neighbors());
    statistics_proto.set_total_num_filtered_neighbors(
        solver()->filtered_neighbors());
    statistics_proto.set_total_num_accepted_neighbors(
        solver()->accepted_neighbors());
    return statistics_proto;
  }
  std::string PrintOverview() const {
    std::string overview;
    size_t max_name_size = 0;
    ParseFirstSolutionStatistics(
        [&max_name_size](absl::string_view name, double) {
          max_name_size = std::max(max_name_size, name.length());
        });
    if (max_name_size > 0) {
      absl::StrAppendFormat(&overview,
                            "First solution statistics:\n%*s | Time (s)\n",
                            max_name_size, "");
      ParseFirstSolutionStatistics(
          [&overview, max_name_size](absl::string_view name,
                                     double duration_seconds) {
            absl::StrAppendFormat(&overview, "%*s | %7.2g\n", max_name_size,
                                  name, duration_seconds);
          });
    }
    max_name_size = 0;
    ParseLocalSearchOperatorStatistics(
        [&max_name_size](absl::string_view name, int64_t, int64_t, int64_t,
                         double, double, double) {
          max_name_size = std::max(max_name_size, name.length());
        });
    if (max_name_size > 0) {
      absl::StrAppendFormat(
          &overview,
          "Local search operator statistics:\n%*s | Neighbors | Filtered "
          "| Accepted | Time (s)\n",
          max_name_size, "");
      OperatorStats total_stats;
      ParseLocalSearchOperatorStatistics(
          [&overview, &total_stats, max_name_size](
              absl::string_view name, int64_t num_neighbors,
              int64_t num_filtered_neighbors, int64_t num_accepted_neighbors,
              double duration_seconds,
              double /*make_next_neighbor_duration_seconds*/,
              double /*accept_neighbor_duration_seconds*/) {
            // TODO(user): Add make_next_neighbor_duration_seconds and
            // accept_neighbor_duration_seconds to stats.
            absl::StrAppendFormat(
                &overview, "%*s | %9ld | %8ld | %8ld | %7.2g\n", max_name_size,
                name, num_neighbors, num_filtered_neighbors,
                num_accepted_neighbors, duration_seconds);
            total_stats.neighbors += num_neighbors;
            total_stats.filtered_neighbors += num_filtered_neighbors;
            total_stats.accepted_neighbors += num_accepted_neighbors;
            total_stats.seconds += duration_seconds;
          });
      absl::StrAppendFormat(
          &overview, "%*s | %9ld | %8ld | %8ld | %7.2g\n", max_name_size,
          "Total", total_stats.neighbors, total_stats.filtered_neighbors,
          total_stats.accepted_neighbors, total_stats.seconds);
    }
    max_name_size = 0;
    ParseLocalSearchFilterStatistics(
        [&max_name_size](absl::string_view, absl::string_view name, int64_t,
                         int64_t, double) {
          max_name_size = std::max(max_name_size, name.length());
        });
    if (max_name_size > 0) {
      std::optional<std::string> filter_context;
      FilterStats total_filter_stats;
      ParseLocalSearchFilterStatistics(
          [&overview, &filter_context, &total_filter_stats, max_name_size](
              const std::string& context, const std::string& name,
              int64_t num_calls, int64_t num_rejects, double duration_seconds) {
            if (!filter_context.has_value() ||
                filter_context.value() != context) {
              if (filter_context.has_value()) {
                absl::StrAppendFormat(
                    &overview, "%*s | %9ld | %9ld | %7.2g  | %7.2g\n",
                    max_name_size, "Total", total_filter_stats.calls,
                    total_filter_stats.rejects, total_filter_stats.seconds,
                    total_filter_stats.rejects / total_filter_stats.seconds);
                total_filter_stats = {};
              }
              filter_context = context;
              absl::StrAppendFormat(
                  &overview,
                  "Local search filter statistics%s:\n%*s |     Calls |  "
                  " Rejects | Time (s) | Rejects/s\n",
                  context.empty() ? "" : " (" + context + ")", max_name_size,
                  "");
            }
            absl::StrAppendFormat(
                &overview, "%*s | %9ld | %9ld | %7.2g  | %7.2g\n",
                max_name_size, name, num_calls, num_rejects, duration_seconds,
                num_rejects / duration_seconds);
            total_filter_stats.calls += num_calls;
            total_filter_stats.rejects += num_rejects;
            total_filter_stats.seconds += duration_seconds;
          });
      absl::StrAppendFormat(
          &overview, "%*s | %9ld | %9ld | %7.2g  | %7.2g\n", max_name_size,
          "Total", total_filter_stats.calls, total_filter_stats.rejects,
          total_filter_stats.seconds,
          total_filter_stats.rejects / total_filter_stats.seconds);
    }
    return overview;
  }
  void BeginOperatorStart() override {}
  void EndOperatorStart() override {}
  void BeginMakeNextNeighbor(const LocalSearchOperator* op) override {
    if (last_operator_ != op->Self()) {
      UpdateTime();
      last_operator_ = op->Self();
    }
    make_next_neighbor_timer_.Start();
  }
  void EndMakeNextNeighbor(const LocalSearchOperator* op, bool neighbor_found,
                           const Assignment*, const Assignment*) override {
    // To be robust to multiple calls to EndMakeNextNeighbor, we only collect
    // data if the timer was not stopped.
    if (!make_next_neighbor_timer_.IsRunning()) return;
    make_next_neighbor_timer_.Stop();
    operator_stats_[op->Self()].make_next_neighbor_seconds +=
        make_next_neighbor_timer_.Get();
    if (neighbor_found) {
      operator_stats_[op->Self()].neighbors++;
    }
  }
  void BeginFilterNeighbor(const LocalSearchOperator*) override {}
  void EndFilterNeighbor(const LocalSearchOperator* op,
                         bool neighbor_found) override {
    if (neighbor_found) {
      operator_stats_[op->Self()].filtered_neighbors++;
    }
  }
  void BeginAcceptNeighbor(const LocalSearchOperator*) override {
    accept_neighbor_timer_.Start();
  }
  void EndAcceptNeighbor(const LocalSearchOperator* op,
                         bool neighbor_found) override {
    accept_neighbor_timer_.Stop();
    operator_stats_[op->Self()].accept_neighbor_seconds +=
        accept_neighbor_timer_.Get();
    if (neighbor_found) {
      operator_stats_[op->Self()].accepted_neighbors++;
    }
  }
  void BeginFiltering(const LocalSearchFilter* filter) override {
    FilterStats& filter_stats =
        filter_stats_per_context_[solver()->context()][filter];
    filter_stats.calls++;
    filter_timer_.Start();
  }
  void EndFiltering(const LocalSearchFilter* filter, bool reject) override {
    filter_timer_.Stop();
    auto& stats = filter_stats_per_context_[solver()->context()][filter];
    stats.seconds += filter_timer_.Get();
    if (reject) {
      stats.rejects++;
    }
  }
  void AddFirstSolutionProfiledDecisionBuilder(
      ProfiledDecisionBuilder* profiled_db) {
    profiled_decision_builders_.push_back(profiled_db);
  }
  bool IsActive() const override { return true; }
  void Install() override { SearchMonitor::Install(); }

 private:
  void UpdateTime() {
    if (last_operator_ != nullptr) {
      timer_.Stop();
      operator_stats_[last_operator_].seconds += timer_.Get();
    }
    timer_.Start();
  }

  struct OperatorStats {
    int64_t neighbors = 0;
    int64_t filtered_neighbors = 0;
    int64_t accepted_neighbors = 0;
    double seconds = 0;
    double make_next_neighbor_seconds = 0;
    double accept_neighbor_seconds = 0;
  };

  struct FilterStats {
    int64_t calls = 0;
    int64_t rejects = 0;
    double seconds = 0;
  };
  WallTimer timer_;
  WallTimer make_next_neighbor_timer_;
  WallTimer accept_neighbor_timer_;
  WallTimer filter_timer_;
  const LocalSearchOperator* last_operator_ = nullptr;
  absl::flat_hash_map<const LocalSearchOperator*, OperatorStats>
      operator_stats_;
  absl::flat_hash_map<
      std::string, absl::flat_hash_map<const LocalSearchFilter*, FilterStats>>
      filter_stats_per_context_;
  // Profiled decision builders.
  std::vector<ProfiledDecisionBuilder*> profiled_decision_builders_;
};

DecisionBuilder* Solver::MakeProfiledDecisionBuilderWrapper(
    DecisionBuilder* db) {
  if (IsLocalSearchProfilingEnabled()) {
    ProfiledDecisionBuilder* profiled_db =
        RevAlloc(new ProfiledDecisionBuilder(db));
    local_search_profiler_->AddFirstSolutionProfiledDecisionBuilder(
        profiled_db);
    return profiled_db;
  }
  return db;
}

void InstallLocalSearchProfiler(LocalSearchProfiler* monitor) {
  monitor->Install();
}

LocalSearchProfiler* BuildLocalSearchProfiler(Solver* solver) {
  if (solver->IsLocalSearchProfilingEnabled()) {
    return new LocalSearchProfiler(solver);
  }
  return nullptr;
}

void DeleteLocalSearchProfiler(LocalSearchProfiler* monitor) { delete monitor; }

std::string Solver::LocalSearchProfile() const {
  if (local_search_profiler_ != nullptr) {
    return local_search_profiler_->PrintOverview();
  }
  return "";
}

LocalSearchStatistics Solver::GetLocalSearchStatistics() const {
  if (local_search_profiler_ != nullptr) {
    return local_search_profiler_->ExportToLocalSearchStatistics();
  }
  return LocalSearchStatistics();
}

void LocalSearchFilterManager::FindIncrementalEventEnd() {
  const int num_events = events_.size();
  incremental_events_end_ = num_events;
  int last_priority = -1;
  for (int e = num_events - 1; e >= 0; --e) {
    const auto& [filter, event_type, priority] = events_[e];
    if (priority != last_priority) {
      incremental_events_end_ = e + 1;
      last_priority = priority;
    }
    if (filter->IsIncremental()) break;
  }
}

LocalSearchFilterManager::LocalSearchFilterManager(
    std::vector<LocalSearchFilter*> filters)
    : synchronized_value_(std::numeric_limits<int64_t>::min()),
      accepted_value_(std::numeric_limits<int64_t>::min()) {
  events_.reserve(2 * filters.size());
  int priority = 0;
  for (LocalSearchFilter* filter : filters) {
    events_.push_back({filter, FilterEventType::kRelax, priority++});
  }
  for (LocalSearchFilter* filter : filters) {
    events_.push_back({filter, FilterEventType::kAccept, priority++});
  }
  FindIncrementalEventEnd();
}

LocalSearchFilterManager::LocalSearchFilterManager(
    std::vector<FilterEvent> filter_events)
    : events_(std::move(filter_events)),
      synchronized_value_(std::numeric_limits<int64_t>::min()),
      accepted_value_(std::numeric_limits<int64_t>::min()) {
  std::sort(events_.begin(), events_.end(),
            [](const FilterEvent& e1, const FilterEvent& e2) {
              return e1.priority < e2.priority;
            });
  FindIncrementalEventEnd();
}

// Filters' Revert() must be called in the reverse order in which their
// Relax() was called.
void LocalSearchFilterManager::Revert() {
  for (int e = last_event_called_; e >= 0; --e) {
    const auto [filter, event_type, _priority] = events_[e];
    if (event_type == FilterEventType::kRelax) filter->Revert();
  }
  last_event_called_ = -1;
}

// TODO(user): the behaviour of Accept relies on the initial order of
// filters having at most one filter with negative objective values,
// this could be fixed by having filters return their general bounds.
bool LocalSearchFilterManager::Accept(LocalSearchMonitor* const monitor,
                                      const Assignment* delta,
                                      const Assignment* deltadelta,
                                      int64_t objective_min,
                                      int64_t objective_max) {
  Revert();
  accepted_value_ = 0;
  bool feasible = true;
  bool reordered = false;
  int events_end = events_.size();
  for (int e = 0; e < events_end; ++e) {
    last_event_called_ = e;
    const auto [filter, event_type, priority] = events_[e];
    switch (event_type) {
      case FilterEventType::kAccept: {
        if (!feasible && !filter->IsIncremental()) continue;
        if (monitor != nullptr) monitor->BeginFiltering(filter);
        const bool accept = filter->Accept(
            delta, deltadelta, CapSub(objective_min, accepted_value_),
            CapSub(objective_max, accepted_value_));
        feasible &= accept;
        if (monitor != nullptr) monitor->EndFiltering(filter, !accept);
        if (feasible) {
          accepted_value_ =
              CapAdd(accepted_value_, filter->GetAcceptedObjectiveValue());
          // TODO(user): handle objective min.
          feasible = accepted_value_ <= objective_max;
        }
        if (!feasible) {
          events_end = incremental_events_end_;
          if (!reordered) {
            // Bump up rejected event, together with its kRelax event,
            // unless it is already first in its priority layer.
            reordered = true;
            int to_move = e - 1;
            if (to_move >= 0 && events_[to_move].filter == filter) --to_move;
            if (to_move >= 0 && events_[to_move].priority == priority) {
              std::rotate(events_.begin() + to_move,
                          events_.begin() + to_move + 1,
                          events_.begin() + e + 1);
            }
          }
        }
        break;
      }
      case FilterEventType::kRelax: {
        filter->Relax(delta, deltadelta);
        break;
      }
      default:
        LOG(FATAL) << "Unknown filter event type.";
    }
  }
  return feasible;
}

void LocalSearchFilterManager::Synchronize(const Assignment* assignment,
                                           const Assignment* delta) {
  // If delta is nullptr or empty, then assignment may be a partial solution.
  // Send a signal to Relaxing filters to inform them,
  // so they can show the partial solution as a change from the empty solution.
  const bool reset_to_assignment = delta == nullptr || delta->Empty();
  // Relax in the forward direction.
  for (auto [filter, event_type, unused_priority] : events_) {
    switch (event_type) {
      case FilterEventType::kAccept: {
        break;
      }
      case FilterEventType::kRelax: {
        if (reset_to_assignment) {
          filter->Reset();
          filter->Relax(assignment, nullptr);
        } else {
          filter->Relax(delta, nullptr);
        }
        break;
      }
      default:
        LOG(FATAL) << "Unknown filter event type.";
    }
  }
  // Synchronize/Commit backwards, so filters can read changes from their
  // dependencies before those are synchronized/committed.
  synchronized_value_ = 0;
  for (auto [filter, event_type, _priority] : ::gtl::reversed_view(events_)) {
    switch (event_type) {
      case FilterEventType::kAccept: {
        filter->Synchronize(assignment, delta);
        synchronized_value_ = CapAdd(synchronized_value_,
                                     filter->GetSynchronizedObjectiveValue());
        break;
      }
      case FilterEventType::kRelax: {
        filter->Commit(assignment, delta);
        break;
      }
      default:
        LOG(FATAL) << "Unknown filter event type.";
    }
  }
}

// ----- Finds a neighbor of the assignment passed -----

class FindOneNeighbor : public DecisionBuilder {
 public:
  FindOneNeighbor(Assignment* assignment, IntVar* objective, SolutionPool* pool,
                  LocalSearchOperator* ls_operator,
                  DecisionBuilder* sub_decision_builder,
                  const RegularLimit* limit,
                  LocalSearchFilterManager* filter_manager);
  ~FindOneNeighbor() override {}
  void EnterSearch();
  Decision* Next(Solver* solver) override;
  std::string DebugString() const override { return "FindOneNeighbor"; }

 private:
  bool FilterAccept(Solver* solver, Assignment* delta, Assignment* deltadelta,
                    int64_t objective_min, int64_t objective_max);
  void SynchronizeAll(Solver* solver);

  Assignment* const assignment_;
  IntVar* const objective_;
  std::unique_ptr<Assignment> reference_assignment_;
  std::unique_ptr<Assignment> last_synchronized_assignment_;
  Assignment* const filter_assignment_delta_;
  SolutionPool* const pool_;
  LocalSearchOperator* const ls_operator_;
  DecisionBuilder* const sub_decision_builder_;
  RegularLimit* limit_;
  const RegularLimit* const original_limit_;
  bool neighbor_found_;
  LocalSearchFilterManager* const filter_manager_;
  int64_t solutions_since_last_check_;
  int64_t check_period_;
  Assignment last_checked_assignment_;
  bool has_checked_assignment_ = false;
};

// reference_assignment_ is used to keep track of the last assignment on which
// operators were started, assignment_ corresponding to the last successful
// neighbor.
// last_synchronized_assignment_ keeps track of the last assignment on which
// filters were synchronized and is used to compute the filter_assignment_delta_
// when synchronizing again.
FindOneNeighbor::FindOneNeighbor(Assignment* const assignment,
                                 IntVar* objective, SolutionPool* const pool,
                                 LocalSearchOperator* const ls_operator,
                                 DecisionBuilder* const sub_decision_builder,
                                 const RegularLimit* const limit,
                                 LocalSearchFilterManager* filter_manager)
    : assignment_(assignment),
      objective_(objective),
      reference_assignment_(new Assignment(assignment_)),
      filter_assignment_delta_(assignment->solver()->MakeAssignment()),
      pool_(pool),
      ls_operator_(ls_operator),
      sub_decision_builder_(sub_decision_builder),
      limit_(nullptr),
      original_limit_(limit),
      neighbor_found_(false),
      filter_manager_(filter_manager),
      solutions_since_last_check_(0),
      check_period_(
          assignment_->solver()->parameters().check_solution_period()),
      last_checked_assignment_(assignment) {
  CHECK(nullptr != assignment);
  CHECK(nullptr != ls_operator);

  Solver* const solver = assignment_->solver();
  // If limit is nullptr, default limit is 1 solution
  if (nullptr == limit) {
    limit_ = solver->MakeSolutionsLimit(1);
  } else {
    limit_ = limit->MakeIdenticalClone();
    // TODO(user): Support skipping neighborhood checks for limits accepting
    // more than one solution (e.g. best accept). For now re-enabling systematic
    // checks.
    if (limit_->solutions() != 1) {
      VLOG(1) << "Disabling neighbor-check skipping outside of first accept.";
      check_period_ = 1;
    }
  }
  // TODO(user): Support skipping neighborhood checks with LNS (at least on
  // the non-LNS operators).
  if (ls_operator->HasFragments()) {
    VLOG(1) << "Disabling neighbor-check skipping for LNS.";
    check_period_ = 1;
  }

  if (!reference_assignment_->HasObjective()) {
    reference_assignment_->AddObjective(objective_);
  }
}

void FindOneNeighbor::EnterSearch() {
  // Reset neighbor_found_ to false to ensure everything is properly
  // synchronized at the beginning of the search.
  neighbor_found_ = false;
  last_synchronized_assignment_.reset();
}

Decision* FindOneNeighbor::Next(Solver* const solver) {
  CHECK(nullptr != solver);

  if (original_limit_ != nullptr) {
    limit_->Copy(original_limit_);
  }

  if (!last_checked_assignment_.HasObjective()) {
    last_checked_assignment_.AddObjective(assignment_->Objective());
  }

  if (!neighbor_found_) {
    // Only called on the first call to Next(), reference_assignment_ has not
    // been synced with assignment_ yet

    // Keeping the code in case a performance problem forces us to
    // use the old code with a zero test on pool_.
    // reference_assignment_->CopyIntersection(assignment_);
    pool_->Initialize(assignment_);
    SynchronizeAll(solver);
  }

  {
    // Another assignment is needed to apply the delta
    Assignment* assignment_copy =
        solver->MakeAssignment(reference_assignment_.get());
    int counter = 0;

    DecisionBuilder* restore = solver->MakeRestoreAssignment(assignment_copy);
    if (sub_decision_builder_) {
      restore = solver->Compose(restore, sub_decision_builder_);
    }
    Assignment* delta = solver->MakeAssignment();
    Assignment* deltadelta = solver->MakeAssignment();
    while (true) {
      if (!ls_operator_->HoldsDelta()) {
        delta->Clear();
      }
      delta->ClearObjective();
      deltadelta->Clear();
      solver->TopPeriodicCheck();
      if (++counter >= absl::GetFlag(FLAGS_cp_local_search_sync_frequency) &&
          pool_->SyncNeeded(reference_assignment_.get())) {
        // TODO(user) : SyncNeed(assignment_) ?
        counter = 0;
        SynchronizeAll(solver);
      }

      bool has_neighbor = false;
      if (!limit_->Check()) {
        solver->GetLocalSearchMonitor()->BeginMakeNextNeighbor(ls_operator_);
        has_neighbor = ls_operator_->MakeNextNeighbor(delta, deltadelta);
        solver->GetLocalSearchMonitor()->EndMakeNextNeighbor(
            ls_operator_, has_neighbor, delta, deltadelta);
      }

      if (has_neighbor && !solver->IsUncheckedSolutionLimitReached()) {
        solver->neighbors_ += 1;
        // All filters must be called for incrementality reasons.
        // Empty deltas must also be sent to incremental filters; can be needed
        // to resync filters on non-incremental (empty) moves.
        // TODO(user): Don't call both if no filter is incremental and one
        // of them returned false.
        solver->GetLocalSearchMonitor()->BeginFilterNeighbor(ls_operator_);
        const bool mh_filter =
            AcceptDelta(solver->ParentSearch(), delta, deltadelta);
        int64_t objective_min = std::numeric_limits<int64_t>::min();
        int64_t objective_max = std::numeric_limits<int64_t>::max();
        if (objective_) {
          objective_min = objective_->Min();
          objective_max = objective_->Max();
        }
        if (delta->HasObjective() && delta->Objective() == objective_) {
          objective_min = std::max(objective_min, delta->ObjectiveMin());
          objective_max = std::min(objective_max, delta->ObjectiveMax());
        }
        const bool move_filter = FilterAccept(solver, delta, deltadelta,
                                              objective_min, objective_max);
        solver->GetLocalSearchMonitor()->EndFilterNeighbor(
            ls_operator_, mh_filter && move_filter);
        if (!mh_filter || !move_filter) {
          if (filter_manager_ != nullptr) filter_manager_->Revert();
          continue;
        }
        solver->filtered_neighbors_ += 1;
        if (delta->HasObjective()) {
          if (!assignment_copy->HasObjective()) {
            assignment_copy->AddObjective(delta->Objective());
          }
          if (!assignment_->HasObjective()) {
            assignment_->AddObjective(delta->Objective());
            last_checked_assignment_.AddObjective(delta->Objective());
          }
        }
        assignment_copy->CopyIntersection(reference_assignment_.get());
        assignment_copy->CopyIntersection(delta);
        solver->GetLocalSearchMonitor()->BeginAcceptNeighbor(ls_operator_);
        const bool check_solution = (solutions_since_last_check_ == 0) ||
                                    !solver->UseFastLocalSearch() ||
                                    // LNS deltas need to be restored
                                    !delta->AreAllElementsBound();
        if (has_checked_assignment_) solutions_since_last_check_++;
        if (solutions_since_last_check_ >= check_period_) {
          solutions_since_last_check_ = 0;
        }
        const bool accept = !check_solution || solver->SolveAndCommit(restore);
        solver->GetLocalSearchMonitor()->EndAcceptNeighbor(ls_operator_,
                                                           accept);
        if (accept) {
          solver->accepted_neighbors_ += 1;
          if (check_solution) {
            solver->SetSearchContext(solver->ParentSearch(),
                                     ls_operator_->DebugString());
            assignment_->Store();
            last_checked_assignment_.CopyIntersection(assignment_);
            neighbor_found_ = true;
            has_checked_assignment_ = true;
            return nullptr;
          }
          solver->SetSearchContext(solver->ActiveSearch(),
                                   ls_operator_->DebugString());
          assignment_->CopyIntersection(assignment_copy);
          assignment_->SetObjectiveValue(
              filter_manager_ ? filter_manager_->GetAcceptedObjectiveValue()
                              : 0);
          // Advancing local search to the current solution without
          // checking.
          // TODO(user): support the case were limit_ accepts more than
          // one solution (e.g. best accept).
          AcceptUncheckedNeighbor(solver->ParentSearch());
          solver->IncrementUncheckedSolutionCounter();
          pool_->RegisterNewSolution(assignment_);
          SynchronizeAll(solver);
          // NOTE: SynchronizeAll() sets neighbor_found_ to false, force it
          // back to true when skipping checks.
          neighbor_found_ = true;
        } else {
          if (filter_manager_ != nullptr) filter_manager_->Revert();
          if (check_period_ > 1 && has_checked_assignment_) {
            // Filtering is not perfect, disabling fast local search and
            // resynchronizing with the last checked solution.
            // TODO(user): Restore state of local search operators to
            // make sure we are exploring neighbors in the same order. This can
            // affect the local optimum found.
            VLOG(1) << "Imperfect filtering detected, backtracking to last "
                       "checked solution and checking all solutions.";
            check_period_ = 1;
            solutions_since_last_check_ = 0;
            pool_->RegisterNewSolution(&last_checked_assignment_);
            SynchronizeAll(solver);
            assignment_->CopyIntersection(&last_checked_assignment_);
          }
        }
      } else {
        // Reset the last synchronized assignment in case it's no longer up to
        // date or we fail below.
        last_synchronized_assignment_.reset();
        if (neighbor_found_) {
          // In case the last checked assignment isn't the current one, restore
          // it to make sure the solver knows about it, especially if this is
          // the end of the search.
          // TODO(user): Compare assignments in addition to their cost.
          if (last_checked_assignment_.ObjectiveValue() !=
              assignment_->ObjectiveValue()) {
            // If restoring fails this means filtering is not perfect and the
            // solver will consider the last checked assignment.
            assignment_copy->CopyIntersection(assignment_);
            if (!solver->SolveAndCommit(restore)) solver->Fail();
            last_checked_assignment_.CopyIntersection(assignment_);
            has_checked_assignment_ = true;
            return nullptr;
          }
          AcceptNeighbor(solver->ParentSearch());
          // Keeping the code in case a performance problem forces us to
          // use the old code with a zero test on pool_.
          //          reference_assignment_->CopyIntersection(assignment_);
          pool_->RegisterNewSolution(assignment_);
          SynchronizeAll(solver);
        } else {
          break;
        }
      }
    }
  }
  // NOTE(user): The last synchronized assignment must be reset here to
  // guarantee filters will be properly synched in case we re-solve using an
  // assignment that wasn't the last accepted and synchronized assignment.
  last_synchronized_assignment_.reset();
  solver->Fail();
  return nullptr;
}

bool FindOneNeighbor::FilterAccept(Solver* solver, Assignment* delta,
                                   Assignment* deltadelta,
                                   int64_t objective_min,
                                   int64_t objective_max) {
  if (filter_manager_ == nullptr) return true;
  LocalSearchMonitor* const monitor = solver->GetLocalSearchMonitor();
  return filter_manager_->Accept(monitor, delta, deltadelta, objective_min,
                                 objective_max);
}

namespace {

template <typename Container>
void AddDeltaElements(const Container& old_container,
                      const Container& new_container, Assignment* delta) {
  for (const auto& new_element : new_container.elements()) {
    const auto var = new_element.Var();
    const auto old_element_ptr = old_container.ElementPtrOrNull(var);
    if (old_element_ptr == nullptr || *old_element_ptr != new_element) {
      delta->FastAdd(var)->Copy(new_element);
    }
  }
}

void MakeDelta(const Assignment* old_assignment,
               const Assignment* new_assignment, Assignment* delta) {
  DCHECK_NE(delta, nullptr);
  delta->Clear();
  AddDeltaElements(old_assignment->IntVarContainer(),
                   new_assignment->IntVarContainer(), delta);
  AddDeltaElements(old_assignment->IntervalVarContainer(),
                   new_assignment->IntervalVarContainer(), delta);
  AddDeltaElements(old_assignment->SequenceVarContainer(),
                   new_assignment->SequenceVarContainer(), delta);
}
}  // namespace

void FindOneNeighbor::SynchronizeAll(Solver* solver) {
  Assignment* const reference_assignment = reference_assignment_.get();
  pool_->GetNextSolution(reference_assignment);
  neighbor_found_ = false;
  limit_->Init();
  solver->GetLocalSearchMonitor()->BeginOperatorStart();
  ls_operator_->Start(reference_assignment);
  if (filter_manager_ != nullptr) {
    Assignment* delta = nullptr;
    if (last_synchronized_assignment_ == nullptr) {
      last_synchronized_assignment_ =
          std::make_unique<Assignment>(reference_assignment);
    } else {
      MakeDelta(last_synchronized_assignment_.get(), reference_assignment,
                filter_assignment_delta_);
      delta = filter_assignment_delta_;
      last_synchronized_assignment_->Copy(reference_assignment);
    }
    filter_manager_->Synchronize(reference_assignment_.get(), delta);
  }
  solver->GetLocalSearchMonitor()->EndOperatorStart();
}

// ---------- Local Search Phase Parameters ----------

class LocalSearchPhaseParameters : public BaseObject {
 public:
  LocalSearchPhaseParameters(IntVar* objective, SolutionPool* const pool,
                             LocalSearchOperator* ls_operator,
                             DecisionBuilder* sub_decision_builder,
                             RegularLimit* const limit,
                             LocalSearchFilterManager* filter_manager)
      : objective_(objective),
        solution_pool_(pool),
        ls_operator_(ls_operator),
        sub_decision_builder_(sub_decision_builder),
        limit_(limit),
        filter_manager_(filter_manager) {}
  ~LocalSearchPhaseParameters() override {}
  std::string DebugString() const override {
    return "LocalSearchPhaseParameters";
  }

  IntVar* objective() const { return objective_; }
  SolutionPool* solution_pool() const { return solution_pool_; }
  LocalSearchOperator* ls_operator() const { return ls_operator_; }
  DecisionBuilder* sub_decision_builder() const {
    return sub_decision_builder_;
  }
  RegularLimit* limit() const { return limit_; }
  LocalSearchFilterManager* filter_manager() const { return filter_manager_; }

 private:
  IntVar* const objective_;
  SolutionPool* const solution_pool_;
  LocalSearchOperator* const ls_operator_;
  DecisionBuilder* const sub_decision_builder_;
  RegularLimit* const limit_;
  LocalSearchFilterManager* const filter_manager_;
};

LocalSearchPhaseParameters* Solver::MakeLocalSearchPhaseParameters(
    IntVar* objective, LocalSearchOperator* const ls_operator,
    DecisionBuilder* const sub_decision_builder) {
  return MakeLocalSearchPhaseParameters(objective, MakeDefaultSolutionPool(),
                                        ls_operator, sub_decision_builder,
                                        nullptr, nullptr);
}

LocalSearchPhaseParameters* Solver::MakeLocalSearchPhaseParameters(
    IntVar* objective, LocalSearchOperator* const ls_operator,
    DecisionBuilder* const sub_decision_builder, RegularLimit* const limit) {
  return MakeLocalSearchPhaseParameters(objective, MakeDefaultSolutionPool(),
                                        ls_operator, sub_decision_builder,
                                        limit, nullptr);
}

LocalSearchPhaseParameters* Solver::MakeLocalSearchPhaseParameters(
    IntVar* objective, LocalSearchOperator* const ls_operator,
    DecisionBuilder* const sub_decision_builder, RegularLimit* const limit,
    LocalSearchFilterManager* filter_manager) {
  return MakeLocalSearchPhaseParameters(objective, MakeDefaultSolutionPool(),
                                        ls_operator, sub_decision_builder,
                                        limit, filter_manager);
}

LocalSearchPhaseParameters* Solver::MakeLocalSearchPhaseParameters(
    IntVar* objective, SolutionPool* const pool,
    LocalSearchOperator* const ls_operator,
    DecisionBuilder* const sub_decision_builder) {
  return MakeLocalSearchPhaseParameters(objective, pool, ls_operator,
                                        sub_decision_builder, nullptr, nullptr);
}

LocalSearchPhaseParameters* Solver::MakeLocalSearchPhaseParameters(
    IntVar* objective, SolutionPool* const pool,
    LocalSearchOperator* const ls_operator,
    DecisionBuilder* const sub_decision_builder, RegularLimit* const limit) {
  return MakeLocalSearchPhaseParameters(objective, pool, ls_operator,
                                        sub_decision_builder, limit, nullptr);
}

LocalSearchPhaseParameters* Solver::MakeLocalSearchPhaseParameters(
    IntVar* objective, SolutionPool* const pool,
    LocalSearchOperator* const ls_operator,
    DecisionBuilder* const sub_decision_builder, RegularLimit* const limit,
    LocalSearchFilterManager* filter_manager) {
  return RevAlloc(new LocalSearchPhaseParameters(objective, pool, ls_operator,
                                                 sub_decision_builder, limit,
                                                 filter_manager));
}

namespace {
// ----- NestedSolve decision wrapper -----

// This decision calls a nested Solve on the given DecisionBuilder in its
// left branch; does nothing in the left branch.
// The state of the decision corresponds to the result of the nested Solve:
// DECISION_PENDING - Nested Solve not called yet
// DECISION_FAILED  - Nested Solve failed
// DECISION_FOUND   - Nested Solve succeeded

class NestedSolveDecision : public Decision {
 public:
  // This enum is used internally to tag states in the local search tree.
  enum StateType { DECISION_PENDING, DECISION_FAILED, DECISION_FOUND };

  NestedSolveDecision(DecisionBuilder* db, bool restore,
                      const std::vector<SearchMonitor*>& monitors);
  NestedSolveDecision(DecisionBuilder* db, bool restore);
  ~NestedSolveDecision() override {}
  void Apply(Solver* solver) override;
  void Refute(Solver* solver) override;
  std::string DebugString() const override { return "NestedSolveDecision"; }
  int state() const { return state_; }

 private:
  DecisionBuilder* const db_;
  bool restore_;
  std::vector<SearchMonitor*> monitors_;
  int state_;
};

NestedSolveDecision::NestedSolveDecision(
    DecisionBuilder* const db, bool restore,
    const std::vector<SearchMonitor*>& monitors)
    : db_(db),
      restore_(restore),
      monitors_(monitors),
      state_(DECISION_PENDING) {
  CHECK(nullptr != db);
}

NestedSolveDecision::NestedSolveDecision(DecisionBuilder* const db,
                                         bool restore)
    : db_(db), restore_(restore), state_(DECISION_PENDING) {
  CHECK(nullptr != db);
}

void NestedSolveDecision::Apply(Solver* const solver) {
  CHECK(nullptr != solver);
  if (restore_) {
    if (solver->Solve(db_, monitors_)) {
      solver->SaveAndSetValue(&state_, static_cast<int>(DECISION_FOUND));
    } else {
      solver->SaveAndSetValue(&state_, static_cast<int>(DECISION_FAILED));
    }
  } else {
    if (solver->SolveAndCommit(db_, monitors_)) {
      solver->SaveAndSetValue(&state_, static_cast<int>(DECISION_FOUND));
    } else {
      solver->SaveAndSetValue(&state_, static_cast<int>(DECISION_FAILED));
    }
  }
}

void NestedSolveDecision::Refute(Solver* const) {}
}  // namespace

// ----- Local search decision builder -----

// Given a first solution (resulting from either an initial assignment or the
// result of a decision builder), it searches for neighbors using a local
// search operator. The first solution corresponds to the first leaf of the
// search.
// The local search applies to the variables contained either in the assignment
// or the vector of variables passed.

class LocalSearch : public DecisionBuilder {
 public:
  LocalSearch(Assignment* assignment, IntVar* objective, SolutionPool* pool,
              LocalSearchOperator* ls_operator,
              DecisionBuilder* sub_decision_builder, RegularLimit* limit,
              LocalSearchFilterManager* filter_manager);
  // TODO(user): find a way to not have to pass vars here: redundant with
  // variables in operators
  LocalSearch(const std::vector<IntVar*>& vars, IntVar* objective,
              SolutionPool* pool, DecisionBuilder* first_solution,
              LocalSearchOperator* ls_operator,
              DecisionBuilder* sub_decision_builder, RegularLimit* limit,
              LocalSearchFilterManager* filter_manager);
  LocalSearch(const std::vector<IntVar*>& vars, IntVar* objective,
              SolutionPool* pool, DecisionBuilder* first_solution,
              DecisionBuilder* first_solution_sub_decision_builder,
              LocalSearchOperator* ls_operator,
              DecisionBuilder* sub_decision_builder, RegularLimit* limit,
              LocalSearchFilterManager* filter_manager);
  LocalSearch(const std::vector<SequenceVar*>& vars, IntVar* objective,
              SolutionPool* pool, DecisionBuilder* first_solution,
              LocalSearchOperator* ls_operator,
              DecisionBuilder* sub_decision_builder, RegularLimit* limit,
              LocalSearchFilterManager* filter_manager);
  ~LocalSearch() override;
  Decision* Next(Solver* solver) override;
  std::string DebugString() const override { return "LocalSearch"; }
  void Accept(ModelVisitor* visitor) const override;

 protected:
  void PushFirstSolutionDecision(DecisionBuilder* first_solution);
  void PushLocalSearchDecision();

 private:
  Assignment* assignment_;
  IntVar* const objective_ = nullptr;
  SolutionPool* const pool_;
  LocalSearchOperator* const ls_operator_;
  DecisionBuilder* const first_solution_sub_decision_builder_;
  DecisionBuilder* const sub_decision_builder_;
  FindOneNeighbor* find_neighbors_db_;
  std::vector<NestedSolveDecision*> nested_decisions_;
  int nested_decision_index_;
  RegularLimit* const limit_;
  LocalSearchFilterManager* const filter_manager_;
  bool has_started_;
};

LocalSearch::LocalSearch(Assignment* const assignment, IntVar* objective,
                         SolutionPool* const pool,
                         LocalSearchOperator* const ls_operator,
                         DecisionBuilder* const sub_decision_builder,
                         RegularLimit* const limit,
                         LocalSearchFilterManager* filter_manager)
    : assignment_(nullptr),
      objective_(objective),
      pool_(pool),
      ls_operator_(ls_operator),
      first_solution_sub_decision_builder_(sub_decision_builder),
      sub_decision_builder_(sub_decision_builder),
      nested_decision_index_(0),
      limit_(limit),
      filter_manager_(filter_manager),
      has_started_(false) {
  CHECK(nullptr != assignment);
  CHECK(nullptr != ls_operator);
  Solver* const solver = assignment->solver();
  assignment_ = solver->GetOrCreateLocalSearchState();
  assignment_->Copy(assignment);
  DecisionBuilder* restore = solver->MakeRestoreAssignment(assignment);
  PushFirstSolutionDecision(restore);
  PushLocalSearchDecision();
}

LocalSearch::LocalSearch(const std::vector<IntVar*>& vars, IntVar* objective,
                         SolutionPool* const pool,
                         DecisionBuilder* const first_solution,
                         LocalSearchOperator* const ls_operator,
                         DecisionBuilder* const sub_decision_builder,
                         RegularLimit* const limit,
                         LocalSearchFilterManager* filter_manager)
    : assignment_(nullptr),
      objective_(objective),
      pool_(pool),
      ls_operator_(ls_operator),
      first_solution_sub_decision_builder_(sub_decision_builder),
      sub_decision_builder_(sub_decision_builder),
      nested_decision_index_(0),
      limit_(limit),
      filter_manager_(filter_manager),
      has_started_(false) {
  CHECK(nullptr != first_solution);
  CHECK(nullptr != ls_operator);
  CHECK(!vars.empty());
  Solver* const solver = vars[0]->solver();
  assignment_ = solver->GetOrCreateLocalSearchState();
  assignment_->Add(vars);
  PushFirstSolutionDecision(first_solution);
  PushLocalSearchDecision();
}

LocalSearch::LocalSearch(
    const std::vector<IntVar*>& vars, IntVar* objective,
    SolutionPool* const pool, DecisionBuilder* const first_solution,
    DecisionBuilder* const first_solution_sub_decision_builder,
    LocalSearchOperator* const ls_operator,
    DecisionBuilder* const sub_decision_builder, RegularLimit* const limit,
    LocalSearchFilterManager* filter_manager)
    : assignment_(nullptr),
      objective_(objective),
      pool_(pool),
      ls_operator_(ls_operator),
      first_solution_sub_decision_builder_(first_solution_sub_decision_builder),
      sub_decision_builder_(sub_decision_builder),
      nested_decision_index_(0),
      limit_(limit),
      filter_manager_(filter_manager),
      has_started_(false) {
  CHECK(nullptr != first_solution);
  CHECK(nullptr != ls_operator);
  CHECK(!vars.empty());
  Solver* const solver = vars[0]->solver();
  assignment_ = solver->GetOrCreateLocalSearchState();
  assignment_->Add(vars);
  PushFirstSolutionDecision(first_solution);
  PushLocalSearchDecision();
}

LocalSearch::LocalSearch(const std::vector<SequenceVar*>& vars,
                         IntVar* objective, SolutionPool* const pool,
                         DecisionBuilder* const first_solution,
                         LocalSearchOperator* const ls_operator,
                         DecisionBuilder* const sub_decision_builder,
                         RegularLimit* const limit,
                         LocalSearchFilterManager* filter_manager)
    : assignment_(nullptr),
      objective_(objective),
      pool_(pool),
      ls_operator_(ls_operator),
      first_solution_sub_decision_builder_(sub_decision_builder),
      sub_decision_builder_(sub_decision_builder),
      nested_decision_index_(0),
      limit_(limit),
      filter_manager_(filter_manager),
      has_started_(false) {
  CHECK(nullptr != first_solution);
  CHECK(nullptr != ls_operator);
  CHECK(!vars.empty());
  Solver* const solver = vars[0]->solver();
  assignment_ = solver->GetOrCreateLocalSearchState();
  assignment_->Add(vars);
  PushFirstSolutionDecision(first_solution);
  PushLocalSearchDecision();
}

LocalSearch::~LocalSearch() {}

// Model Visitor support.
void LocalSearch::Accept(ModelVisitor* const visitor) const {
  DCHECK(assignment_ != nullptr);
  visitor->BeginVisitExtension(ModelVisitor::kVariableGroupExtension);
  // We collect decision variables from the assignment.
  const std::vector<IntVarElement>& elements =
      assignment_->IntVarContainer().elements();
  if (!elements.empty()) {
    std::vector<IntVar*> vars;
    for (const IntVarElement& elem : elements) {
      vars.push_back(elem.Var());
    }
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars);
  }
  const std::vector<IntervalVarElement>& interval_elements =
      assignment_->IntervalVarContainer().elements();
  if (!interval_elements.empty()) {
    std::vector<IntervalVar*> interval_vars;
    for (const IntervalVarElement& elem : interval_elements) {
      interval_vars.push_back(elem.Var());
    }
    visitor->VisitIntervalArrayArgument(ModelVisitor::kIntervalsArgument,
                                        interval_vars);
  }
  visitor->EndVisitExtension(ModelVisitor::kVariableGroupExtension);
}

// This is equivalent to a multi-restart decision builder
// TODO(user): abstract this from the local search part
// TODO(user): handle the case where the tree depth is not enough to hold
//                all solutions.

Decision* LocalSearch::Next(Solver* const solver) {
  CHECK(nullptr != solver);
  CHECK_LT(0, nested_decisions_.size());
  if (!has_started_) {
    nested_decision_index_ = 0;
    solver->SaveAndSetValue(&has_started_, true);
    find_neighbors_db_->EnterSearch();
    ls_operator_->EnterSearch();
  } else if (nested_decision_index_ < 0) {
    solver->Fail();
  }
  NestedSolveDecision* decision = nested_decisions_[nested_decision_index_];
  const int state = decision->state();
  switch (state) {
    case NestedSolveDecision::DECISION_FAILED: {
      const bool local_optimum_reached =
          LocalOptimumReached(solver->ActiveSearch());
      if (local_optimum_reached) {
        // A local optimum has been reached. The search will continue only if we
        // accept up-hill moves (due to metaheuristics). In this case we need to
        // reset neighborhood optimal routes.
        ls_operator_->Reset();
      }
      if (!local_optimum_reached || solver->IsUncheckedSolutionLimitReached()) {
        nested_decision_index_ = -1;  // Stop the search
      }
      solver->Fail();
      return nullptr;
    }
    case NestedSolveDecision::DECISION_PENDING: {
      // TODO(user): Find a way to make this balancing invisible to the
      // user (no increase in branch or fail counts for instance).
      const int32_t kLocalSearchBalancedTreeDepth = 32;
      const int depth = solver->SearchDepth();
      if (depth < kLocalSearchBalancedTreeDepth) {
        return solver->balancing_decision();
      }
      if (depth > kLocalSearchBalancedTreeDepth) {
        solver->Fail();
      }
      return decision;
    }
    case NestedSolveDecision::DECISION_FOUND: {
      // Next time go to next decision
      if (nested_decision_index_ + 1 < nested_decisions_.size()) {
        ++nested_decision_index_;
      }
      return nullptr;
    }
    default: {
      LOG(ERROR) << "Unknown local search state";
      return nullptr;
    }
  }
  return nullptr;
}

void LocalSearch::PushFirstSolutionDecision(DecisionBuilder* first_solution) {
  CHECK(first_solution);
  Solver* const solver = assignment_->solver();
  DecisionBuilder* store = solver->MakeStoreAssignment(assignment_);
  DecisionBuilder* first_solution_and_store = solver->Compose(
      solver->MakeProfiledDecisionBuilderWrapper(first_solution),
      first_solution_sub_decision_builder_, store);
  std::vector<SearchMonitor*> monitor;
  monitor.push_back(limit_);
  nested_decisions_.push_back(solver->RevAlloc(
      new NestedSolveDecision(first_solution_and_store, false, monitor)));
}

void LocalSearch::PushLocalSearchDecision() {
  Solver* const solver = assignment_->solver();
  find_neighbors_db_ = solver->RevAlloc(
      new FindOneNeighbor(assignment_, objective_, pool_, ls_operator_,
                          sub_decision_builder_, limit_, filter_manager_));
  nested_decisions_.push_back(
      solver->RevAlloc(new NestedSolveDecision(find_neighbors_db_, false)));
}

class DefaultSolutionPool : public SolutionPool {
 public:
  DefaultSolutionPool() {}

  ~DefaultSolutionPool() override {}

  void Initialize(Assignment* const assignment) override {
    reference_assignment_ = std::make_unique<Assignment>(assignment);
  }

  void RegisterNewSolution(Assignment* const assignment) override {
    reference_assignment_->CopyIntersection(assignment);
  }

  void GetNextSolution(Assignment* const assignment) override {
    assignment->CopyIntersection(reference_assignment_.get());
  }

  bool SyncNeeded(Assignment* const) override { return false; }

  std::string DebugString() const override { return "DefaultSolutionPool"; }

 private:
  std::unique_ptr<Assignment> reference_assignment_;
};

SolutionPool* Solver::MakeDefaultSolutionPool() {
  return RevAlloc(new DefaultSolutionPool());
}

DecisionBuilder* Solver::MakeLocalSearchPhase(
    Assignment* assignment, LocalSearchPhaseParameters* parameters) {
  return RevAlloc(new LocalSearch(
      assignment, parameters->objective(), parameters->solution_pool(),
      parameters->ls_operator(), parameters->sub_decision_builder(),
      parameters->limit(), parameters->filter_manager()));
}

DecisionBuilder* Solver::MakeLocalSearchPhase(
    const std::vector<IntVar*>& vars, DecisionBuilder* first_solution,
    LocalSearchPhaseParameters* parameters) {
  return RevAlloc(new LocalSearch(
      vars, parameters->objective(), parameters->solution_pool(),
      first_solution, parameters->ls_operator(),
      parameters->sub_decision_builder(), parameters->limit(),
      parameters->filter_manager()));
}

DecisionBuilder* Solver::MakeLocalSearchPhase(
    const std::vector<IntVar*>& vars, DecisionBuilder* first_solution,
    DecisionBuilder* first_solution_sub_decision_builder,
    LocalSearchPhaseParameters* parameters) {
  return RevAlloc(new LocalSearch(
      vars, parameters->objective(), parameters->solution_pool(),
      first_solution, first_solution_sub_decision_builder,
      parameters->ls_operator(), parameters->sub_decision_builder(),
      parameters->limit(), parameters->filter_manager()));
}

DecisionBuilder* Solver::MakeLocalSearchPhase(
    const std::vector<SequenceVar*>& vars, DecisionBuilder* first_solution,
    LocalSearchPhaseParameters* parameters) {
  return RevAlloc(new LocalSearch(
      vars, parameters->objective(), parameters->solution_pool(),
      first_solution, parameters->ls_operator(),
      parameters->sub_decision_builder(), parameters->limit(),
      parameters->filter_manager()));
}

template <bool is_profile_active>
Assignment* Solver::RunUncheckedLocalSearchInternal(
    const Assignment* initial_solution,
    LocalSearchFilterManager* filter_manager, LocalSearchOperator* ls_operator,
    const std::vector<SearchMonitor*>& monitors, RegularLimit* limit,
    absl::flat_hash_set<IntVar*>* touched) {
  DCHECK(initial_solution != nullptr);
  DCHECK(initial_solution->Objective() != nullptr);
  DCHECK(filter_manager != nullptr);
  if (limit != nullptr) limit->Init();
  Assignment delta(this);
  Assignment deltadelta(this);
  Assignment* const current_solution = GetOrCreateLocalSearchState();
  current_solution->Copy(initial_solution);
  std::function<bool(Assignment*, Assignment*)> make_next_neighbor;
  std::function<bool(Assignment*, Assignment*)> filter_neighbor;
  LocalSearchMonitor* const ls_monitor =
      is_profile_active ? GetLocalSearchMonitor() : nullptr;
  const auto sync_with_solution =
      [this, ls_monitor, ls_operator,  // NOLINT: ls_monitor is used when
                                       // is_profile_active is true
       filter_manager, current_solution](Assignment* delta) {
        IncrementUncheckedSolutionCounter();
        if constexpr (is_profile_active) {
          ls_monitor->BeginOperatorStart();
        }
        ls_operator->Start(current_solution);
        filter_manager->Synchronize(current_solution, delta);
        if constexpr (is_profile_active) {
          ls_monitor->EndOperatorStart();
        }
      };
  sync_with_solution(/*delta=*/nullptr);
  while (true) {
    if (!ls_operator->HoldsDelta()) {
      delta.Clear();
    }
    delta.ClearObjective();
    deltadelta.Clear();
    if (limit != nullptr && (limit->CheckWithOffset(absl::ZeroDuration()) ||
                             limit->IsUncheckedSolutionLimitReached())) {
      break;
    }
    if constexpr (is_profile_active) {
      ls_monitor->BeginMakeNextNeighbor(ls_operator);
    }
    const bool has_neighbor =
        ls_operator->MakeNextNeighbor(&delta, &deltadelta);
    if constexpr (is_profile_active) {
      ls_monitor->EndMakeNextNeighbor(ls_operator, has_neighbor, &delta,
                                      &deltadelta);
    }
    if (!has_neighbor) {
      break;
    }
    if constexpr (is_profile_active) {
      ls_monitor->BeginFilterNeighbor(ls_operator);
    }
    for (SearchMonitor* monitor : monitors) {
      const bool mh_accept = monitor->AcceptDelta(&delta, &deltadelta);
      DCHECK(mh_accept);
    }
    const bool filter_accept =
        filter_manager->Accept(ls_monitor, &delta, &deltadelta,
                               delta.ObjectiveMin(), delta.ObjectiveMax());
    if constexpr (is_profile_active) {
      ls_monitor->EndFilterNeighbor(ls_operator, filter_accept);
      ls_monitor->BeginAcceptNeighbor(ls_operator);
      ls_monitor->EndAcceptNeighbor(ls_operator, filter_accept);
    }
    if (!filter_accept) {
      filter_manager->Revert();
      continue;
    }
    filtered_neighbors_ += 1;
    current_solution->CopyIntersection(&delta);
    current_solution->SetObjectiveValue(
        filter_manager->GetAcceptedObjectiveValue());
    DCHECK(delta.AreAllElementsBound());
    accepted_neighbors_ += 1;
    SetSearchContext(ActiveSearch(), ls_operator->DebugString());
    for (SearchMonitor* monitor : monitors) {
      monitor->AtSolution();
    }
    if (touched != nullptr) {
      for (const auto& element : delta.IntVarContainer().elements()) {
        touched->insert(element.Var());
      }
    }
    // Syncing here to avoid resyncing when filtering fails.
    sync_with_solution(/*delta=*/&delta);
  }
  if (parameters_.print_local_search_profile()) {
    const std::string profile = LocalSearchProfile();
    if (!profile.empty()) LOG(INFO) << profile;
  }
  return MakeAssignment(current_solution);
}

Assignment* Solver::RunUncheckedLocalSearch(
    const Assignment* initial_solution,
    LocalSearchFilterManager* filter_manager, LocalSearchOperator* ls_operator,
    const std::vector<SearchMonitor*>& monitors, RegularLimit* limit,
    absl::flat_hash_set<IntVar*>* touched) {
  if (GetLocalSearchMonitor()->IsActive()) {
    return RunUncheckedLocalSearchInternal<true>(initial_solution,
                                                 filter_manager, ls_operator,
                                                 monitors, limit, touched);
  } else {
    return RunUncheckedLocalSearchInternal<false>(initial_solution,
                                                  filter_manager, ls_operator,
                                                  monitors, limit, touched);
  }
}

}  // namespace operations_research

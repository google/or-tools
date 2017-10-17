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


#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <iterator>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/callback.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/join.h"
#include "ortools/base/map_util.h"
#include "ortools/base/hash.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/graph/hamiltonian_path.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/base/random.h"

DEFINE_int32(cp_local_search_sync_frequency, 16,
             "Frequency of checks for better solutions in the solution pool.");

DEFINE_int32(cp_local_search_tsp_opt_size, 13,
             "Size of TSPs solved in the TSPOpt operator.");

DEFINE_int32(cp_local_search_tsp_lns_size, 10,
             "Size of TSPs solved in the TSPLns operator.");

DEFINE_bool(cp_use_empty_path_symmetry_breaker, true,
            "If true, equivalent empty paths are removed from the neighborhood "
            "of PathOperators");

namespace operations_research {

// Utility methods to ensure the communication between local search and the
// search.

// Returns true if a local optimum has been reached and cannot be improved.
bool LocalOptimumReached(Search* const search);

// Returns true if the search accepts the delta (actually checking this by
// calling AcceptDelta on the monitors of the search).
bool AcceptDelta(Search* const search, Assignment* delta,
                 Assignment* deltadelta);

// Notifies the search that a neighbor has been accepted by local search.
void AcceptNeighbor(Search* const search);

// ----- Base operator class for operators manipulating IntVars -----

bool IntVarLocalSearchOperator::MakeNextNeighbor(Assignment* delta,
                                                 Assignment* deltadelta) {
  CHECK(delta != nullptr);
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
  } else {
    return false;
  }
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
  } else {
    return false;
  }
}

// ----- Random Large Neighborhood Search operator -----

// Frees up to number_of_variables random variables.

class RandomLns : public BaseLns {
 public:
  RandomLns(const std::vector<IntVar*>& vars, int number_of_variables,
            int32 seed)
      : BaseLns(vars), rand_(seed), number_of_variables_(number_of_variables) {
    CHECK_GT(number_of_variables_, 0);
    CHECK_LE(number_of_variables_, Size());
  }
  ~RandomLns() override {}
  bool NextFragment() override;

  std::string DebugString() const override { return "RandomLns"; }

 private:
  ACMRandom rand_;
  const int number_of_variables_;
};

bool RandomLns::NextFragment() {
  for (int i = 0; i < number_of_variables_; ++i) {
    AppendToFragment(rand_.Uniform(Size()));
  }
  return true;
}
}  // namespace

LocalSearchOperator* Solver::MakeRandomLnsOperator(
    const std::vector<IntVar*>& vars, int number_of_variables) {
  return MakeRandomLnsOperator(vars, number_of_variables,
                               ACMRandom::HostnamePidTimeSeed());
}

LocalSearchOperator* Solver::MakeRandomLnsOperator(
    const std::vector<IntVar*>& vars, int number_of_variables, int32 seed) {
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
                     const std::vector<int64>& target_values)
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
      const int64 target_value = target_.at(variable_index_);
      const int64 current_value = OldValue(variable_index_);
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
  const std::vector<int64> target_;

  // Index of the next variable to try to restore
  int64 variable_index_;

  // Number of variables checked since the last call to OnStart().
  int64 num_var_since_last_start_;
};
}  // namespace

LocalSearchOperator* Solver::MakeMoveTowardTargetOperator(
    const Assignment& target) {
  typedef std::vector<IntVarElement> Elements;
  const Elements& elements = target.IntVarContainer().elements();
  // Copy target values and construct the vector of variables
  std::vector<IntVar*> vars;
  std::vector<int64> values;
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
    const std::vector<int64>& target_values) {
  return RevAlloc(new MoveTowardTargetLS(variables, target_values));
}

// ----- ChangeValue Operators -----

ChangeValue::ChangeValue(const std::vector<IntVar*>& vars)
    : IntVarLocalSearchOperator(vars), index_(0) {}

ChangeValue::~ChangeValue() {}

bool ChangeValue::MakeOneNeighbor() {
  const int size = Size();
  while (index_ < size) {
    const int64 value = ModifyValue(index_, Value(index_));
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
  int64 ModifyValue(int64 index, int64 value) override { return value + 1; }

  std::string DebugString() const override { return "IncrementValue"; }
};

// Decrements the current value of variables.

class DecrementValue : public ChangeValue {
 public:
  explicit DecrementValue(const std::vector<IntVar*>& vars)
      : ChangeValue(vars) {}
  ~DecrementValue() override {}
  int64 ModifyValue(int64 index, int64 value) override { return value - 1; }

  std::string DebugString() const override { return "DecrementValue"; }
};
}  // namespace

// ----- Path-based Operators -----

PathOperator::PathOperator(const std::vector<IntVar*>& next_vars,
                           const std::vector<IntVar*>& path_vars,
                           int number_of_base_nodes,
                           std::function<int(int64)> start_empty_path_class)
    : IntVarLocalSearchOperator(next_vars),
      number_of_nexts_(next_vars.size()),
      ignore_path_vars_(path_vars.empty()),
      base_nodes_(number_of_base_nodes),
      end_nodes_(number_of_base_nodes),
      base_paths_(number_of_base_nodes),
      just_started_(false),
      first_start_(true),
      start_empty_path_class_(std::move(start_empty_path_class)) {
  if (!ignore_path_vars_) {
    AddVars(path_vars);
  }
}

void PathOperator::OnStart() {
  InitializeBaseNodes();
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
  } else {
    int next_index = index - number_of_nexts_;
    return Value(next_index) == OldValue(next_index);
  }
}

bool PathOperator::MoveChain(int64 before_chain, int64 chain_end,
                             int64 destination) {
  if (CheckChainValidity(before_chain, chain_end, destination) &&
      !IsPathEnd(chain_end) && !IsPathEnd(destination)) {
    const int64 destination_path = Path(destination);
    const int64 after_chain = Next(chain_end);
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
  return false;
}

bool PathOperator::ReverseChain(int64 before_chain, int64 after_chain,
                                int64* chain_last) {
  if (CheckChainValidity(before_chain, after_chain, -1)) {
    int64 path = Path(before_chain);
    int64 current = Next(before_chain);
    if (current == after_chain) {
      return false;
    }
    int64 current_next = Next(current);
    SetNext(current, after_chain, path);
    while (current_next != after_chain) {
      const int64 next = Next(current_next);
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

bool PathOperator::MakeActive(int64 node, int64 destination) {
  if (!IsPathEnd(destination)) {
    int64 destination_path = Path(destination);
    SetNext(node, Next(destination), destination_path);
    SetNext(destination, node, destination_path);
    return true;
  } else {
    return false;
  }
}

bool PathOperator::MakeChainInactive(int64 before_chain, int64 chain_end) {
  const int64 kNoPath = -1;
  if (CheckChainValidity(before_chain, chain_end, -1) &&
      !IsPathEnd(chain_end)) {
    const int64 after_chain = Next(chain_end);
    int64 current = Next(before_chain);
    while (current != after_chain) {
      const int64 next = Next(current);
      SetNext(current, current, kNoPath);
      current = next;
    }
    SetNext(before_chain, after_chain, Path(before_chain));
    return true;
  }
  return false;
}

bool PathOperator::IncrementPosition() {
  const int base_node_size = base_nodes_.size();
  if (!just_started_) {
    const int number_of_paths = path_starts_.size();
    // Finding next base node positions.
    // Increment the position of inner base nodes first (higher index nodes);
    // if a base node is at the end of a path, reposition it at the start
    // of the path and increment the position of the preceding base node (this
    // action is called a restart).
    int last_restarted = base_node_size;
    for (int i = base_node_size - 1; i >= 0; --i) {
      if (base_nodes_[i] < number_of_nexts_) {
        base_nodes_[i] = OldNext(base_nodes_[i]);
        break;
      }
      base_nodes_[i] = StartNode(i);
      last_restarted = i;
    }
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
      base_nodes_[i] = GetBaseNodeRestartPosition(i);
    }
    if (last_restarted > 0) {
      return CheckEnds();
    }
    // If all base nodes have been restarted, base nodes are moved to new paths.
    for (int i = base_node_size - 1; i >= 0; --i) {
      const int next_path_index = base_paths_[i] + 1;
      if (next_path_index < number_of_paths) {
        base_paths_[i] = next_path_index;
        base_nodes_[i] = path_starts_[next_path_index];
        if (i == 0 || !OnSamePathAsPreviousBase(i)) {
          return CheckEnds();
        }
      } else {
        base_paths_[i] = 0;
        base_nodes_[i] = path_starts_[0];
      }
    }
  } else {
    just_started_ = false;
    return true;
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
  // Create a list of path starts, dropping equivalent path starts of
  // currently empty paths.
  std::vector<bool> empty_found(number_of_nexts_, false);
  std::vector<int64> new_path_starts;
  const bool use_empty_path_symmetry_breaker =
      FLAGS_cp_use_empty_path_symmetry_breaker;
  for (int i = 0; i < number_of_nexts_; ++i) {
    if (!has_prevs[i]) {
      if (use_empty_path_symmetry_breaker && IsPathEnd(OldNext(i))) {
        if (start_empty_path_class_ != nullptr) {
          if (empty_found[start_empty_path_class_(i)]) {
            continue;
          } else {
            empty_found[start_empty_path_class_(i)] = true;
          }
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
    for (int j = 0; j < base_nodes_.size(); ++j) {
      if (IsInactive(base_nodes_[j]) || node_paths[base_nodes_[j]] == -1) {
        // Base node was made inactive or was moved to a new path, reposition
        // the base node to the start of the path on which it was.
        base_nodes_[j] = path_starts_[base_paths_[j]];
      } else {
        base_paths_[j] = node_paths[base_nodes_[j]];
      }
    }
    // Re-adjust current base_nodes and base_paths to take into account new
    // path starts (there could be fewer if a new path was made empty, or more
    // if nodes were added to a formerly empty path).
    int new_index = 0;
    std::unordered_set<int> found_bases;
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
      for (int j = 0; j < base_nodes_.size(); ++j) {
        if (base_paths_[j] == i && !ContainsKey(found_bases, j)) {
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
    for (int i = 0; i < base_nodes_.size(); ++i) {
      base_paths_[i] = 0;
      base_nodes_[i] = path_starts_[0];
    }
    first_start_ = false;
  }
  for (int i = 0; i < base_nodes_.size(); ++i) {
    // If base node has been made inactive, restart from path start.
    int64 base_node = base_nodes_[i];
    if (RestartAtPathStartOnSynchronize() || IsInactive(base_node)) {
      base_node = path_starts_[base_paths_[i]];
      base_nodes_[i] = base_node;
    }
    end_nodes_[i] = base_node;
  }
  // Repair end_nodes_ in case some must be on the same path and are not anymore
  // (due to other operators moving these nodes).
  for (int i = 1; i < base_nodes_.size(); ++i) {
    if (OnSamePathAsPreviousBase(i) &&
        !OnSamePath(base_nodes_[i - 1], base_nodes_[i])) {
      const int64 base_node = base_nodes_[i - 1];
      base_nodes_[i] = base_node;
      end_nodes_[i] = base_node;
      base_paths_[i] = base_paths_[i - 1];
    }
  }
  just_started_ = true;
}

bool PathOperator::OnSamePath(int64 node1, int64 node2) const {
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
bool PathOperator::CheckChainValidity(int64 before_chain, int64 chain_end,
                                      int64 exclude) const {
  if (before_chain == chain_end || before_chain == exclude) return false;
  int64 current = before_chain;
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

PathWithPreviousNodesOperator::PathWithPreviousNodesOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars, int number_of_base_nodes,
    std::function<int(int64)> start_empty_path_class)
    : PathOperator(vars, secondary_vars, number_of_base_nodes,
                   std::move(start_empty_path_class)) {
  int64 max_next = -1;
  for (const IntVar* const var : vars) {
    max_next = std::max(max_next, var->Max());
  }
  prevs_.resize(max_next + 1, -1);
}

void PathWithPreviousNodesOperator::OnNodeInitialization() {
  for (int node_index = 0; node_index < number_of_nexts(); ++node_index) {
    prevs_[Next(node_index)] = node_index;
  }
}

// ----- 2Opt -----

// Reverves a sub-chain of a path. It is called 2Opt because it breaks
// 2 arcs on the path; resulting paths are called 2-optimal.
// Possible neighbors for the path 1 -> 2 -> 3 -> 4 -> 5
// (where (1, 5) are first and last nodes of the path and can therefore not be
// moved):
// 1 -> 3 -> 2 -> 4 -> 5
// 1 -> 4 -> 3 -> 2 -> 5
// 1 -> 2 -> 4 -> 3 -> 5
class TwoOpt : public PathOperator {
 public:
  TwoOpt(const std::vector<IntVar*>& vars,
         const std::vector<IntVar*>& secondary_vars,
         std::function<int(int64)> start_empty_path_class)
      : PathOperator(vars, secondary_vars, 2,
                     std::move(start_empty_path_class)),
        last_base_(-1),
        last_(-1) {}
  ~TwoOpt() override {}
  bool MakeNeighbor() override;
  bool IsIncremental() const override { return true; }

  std::string DebugString() const override { return "TwoOpt"; }

 protected:
  bool OnSamePathAsPreviousBase(int64 base_index) override {
    // Both base nodes have to be on the same path.
    return true;
  }

 private:
  void OnNodeInitialization() override { last_ = -1; }

  int64 last_base_;
  int64 last_;
};

bool TwoOpt::MakeNeighbor() {
  DCHECK_EQ(StartNode(0), StartNode(1));
  if (last_base_ != BaseNode(0) || last_ == -1) {
    RevertChanges(false);
    if (IsPathEnd(BaseNode(0))) {
      last_ = -1;
      return false;
    }
    last_base_ = BaseNode(0);
    last_ = Next(BaseNode(0));
    int64 chain_last;
    if (ReverseChain(BaseNode(0), BaseNode(1), &chain_last)
        // Check there are more than one node in the chain (reversing a
        // single node is a NOP).
        && last_ != chain_last) {
      return true;
    } else {
      last_ = -1;
      return false;
    }
  } else {
    const int64 to_move = Next(last_);
    DCHECK_EQ(Next(to_move), BaseNode(1));
    return MoveChain(last_, to_move, BaseNode(0));
  }
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
           std::function<int(int64)> start_empty_path_class,
           int64 chain_length = 1LL, bool single_path = false)
      : PathOperator(vars, secondary_vars, 2,
                     std::move(start_empty_path_class)),
        chain_length_(chain_length),
        single_path_(single_path),
        name_(name) {
    CHECK_GT(chain_length_, 0);
  }
  Relocate(const std::vector<IntVar*>& vars,
           const std::vector<IntVar*>& secondary_vars,
           std::function<int(int64)> start_empty_path_class,
           int64 chain_length = 1LL, bool single_path = false)
      : Relocate(vars, secondary_vars,
                 StrCat("Relocate<", chain_length, ">"),
                 std::move(start_empty_path_class), chain_length, single_path) {
  }
  ~Relocate() override {}
  bool MakeNeighbor() override;

  std::string DebugString() const override { return name_; }

 protected:
  bool OnSamePathAsPreviousBase(int64 base_index) override {
    // Both base nodes have to be on the same path when it's the single path
    // version.
    return single_path_;
  }

 private:
  const int64 chain_length_;
  const bool single_path_;
  const std::string name_;
};

bool Relocate::MakeNeighbor() {
  DCHECK(!single_path_ || StartNode(0) == StartNode(1));
  const int64 before_chain = BaseNode(0);
  int64 chain_end = before_chain;
  for (int i = 0; i < chain_length_; ++i) {
    if (IsPathEnd(chain_end)) {
      return false;
    }
    chain_end = Next(chain_end);
  }
  const int64 destination = BaseNode(1);
  return MoveChain(before_chain, chain_end, destination);
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
           std::function<int(int64)> start_empty_path_class)
      : PathOperator(vars, secondary_vars, 2,
                     std::move(start_empty_path_class)) {}
  ~Exchange() override {}
  bool MakeNeighbor() override;

  std::string DebugString() const override { return "Exchange"; }
};

bool Exchange::MakeNeighbor() {
  const int64 prev_node0 = BaseNode(0);
  if (IsPathEnd(prev_node0)) return false;
  const int64 node0 = Next(prev_node0);
  const int64 prev_node1 = BaseNode(1);
  if (IsPathEnd(prev_node1)) return false;
  const int64 node1 = Next(prev_node1);
  if (node0 == prev_node1) {
    return MoveChain(prev_node1, node1, prev_node0);
  } else if (node1 == prev_node0) {
    return MoveChain(prev_node0, node0, prev_node1);
  } else {
    return MoveChain(prev_node0, node0, prev_node1) &&
           MoveChain(node0, Next(node0), prev_node0);
  }
  return false;
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
        std::function<int(int64)> start_empty_path_class)
      : PathOperator(vars, secondary_vars, 2,
                     std::move(start_empty_path_class)) {}
  ~Cross() override {}
  bool MakeNeighbor() override;

  std::string DebugString() const override { return "Cross"; }
};

bool Cross::MakeNeighbor() {
  const int64 node0 = BaseNode(0);
  const int64 start0 = StartNode(0);
  const int64 node1 = BaseNode(1);
  const int64 start1 = StartNode(1);
  if (start1 == start0) {
    return false;
  }
  if (!IsPathEnd(node0) && !IsPathEnd(node1)) {
    // If two paths are equivalent don't exchange them.
    if (PathClass(0) == PathClass(1) && IsPathEnd(Next(node0)) &&
        IsPathEnd(Next(node1))) {
      return false;
    }
    return MoveChain(start0, node0, start1) && MoveChain(node0, node1, start0);
  } else if (!IsPathEnd(node0)) {
    return MoveChain(start0, node0, start1);
  } else if (!IsPathEnd(node1)) {
    return MoveChain(start1, node1, start0);
  }
  return false;
}

// ----- BaseInactiveNodeToPathOperator -----
// Base class of path operators which make inactive nodes active.

class BaseInactiveNodeToPathOperator : public PathOperator {
 public:
  BaseInactiveNodeToPathOperator(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars, int number_of_base_nodes,
      std::function<int(int64)> start_empty_path_class)
      : PathOperator(vars, secondary_vars, number_of_base_nodes,
                     std::move(start_empty_path_class)),
        inactive_node_(0) {}
  ~BaseInactiveNodeToPathOperator() override {}

 protected:
  bool MakeOneNeighbor() override;
  int64 GetInactiveNode() const { return inactive_node_; }

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
                     std::function<int(int64)> start_empty_path_class)
      : BaseInactiveNodeToPathOperator(vars, secondary_vars, 1,
                                       std::move(start_empty_path_class)) {}
  ~MakeActiveOperator() override {}
  bool MakeNeighbor() override;

  std::string DebugString() const override { return "MakeActiveOperator"; }
};

bool MakeActiveOperator::MakeNeighbor() {
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
      std::function<int(int64)> start_empty_path_class)
      : BaseInactiveNodeToPathOperator(vars, secondary_vars, 2,
                                       std::move(start_empty_path_class)) {}
  ~RelocateAndMakeActiveOperator() override {}
  bool MakeNeighbor() override {
    const int64 before_node_to_move = BaseNode(1);
    if (IsPathEnd(before_node_to_move)) {
      return false;
    }
    return MoveChain(before_node_to_move, Next(before_node_to_move),
                     BaseNode(0)) &&
           MakeActive(GetInactiveNode(), before_node_to_move);
  }

  std::string DebugString() const override { return "RelocateAndMakeActiveOpertor"; }
};

// ----- MakeActiveAndRelocate -----

// MakeActiveAndRelocate makes a node active next to a node being relocated.
// Possible neighbor for paths 0 -> 4, 1 -> 2 -> 5 and 3 inactive is:
// 0 -> 3 -> 2 -> 4, 1 -> 5.

class MakeActiveAndRelocate : public BaseInactiveNodeToPathOperator {
 public:
  MakeActiveAndRelocate(const std::vector<IntVar*>& vars,
                        const std::vector<IntVar*>& secondary_vars,
                        std::function<int(int64)> start_empty_path_class)
      : BaseInactiveNodeToPathOperator(vars, secondary_vars, 2,
                                       std::move(start_empty_path_class)) {}
  ~MakeActiveAndRelocate() override {}
  bool MakeNeighbor() override;

  std::string DebugString() const override {
    return "MakeActiveAndRelocateOperator";
  }
};

bool MakeActiveAndRelocate::MakeNeighbor() {
  const int64 before_chain = BaseNode(1);
  if (IsPathEnd(before_chain)) {
    return false;
  }
  const int64 chain_end = Next(before_chain);
  const int64 destination = BaseNode(0);
  return MoveChain(before_chain, chain_end, destination) &&
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
                       std::function<int(int64)> start_empty_path_class)
      : PathOperator(vars, secondary_vars, 1,
                     std::move(start_empty_path_class)) {}
  ~MakeInactiveOperator() override {}
  bool MakeNeighbor() override {
    const int64 base = BaseNode(0);
    if (IsPathEnd(base)) {
      return false;
    }
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
      std::function<int(int64)> start_empty_path_class)
      : PathOperator(vars, secondary_vars, 2,
                     std::move(start_empty_path_class)) {}
  ~RelocateAndMakeInactiveOperator() override {}
  bool MakeNeighbor() override {
    const int64 destination = BaseNode(1);
    const int64 before_to_move = BaseNode(0);
    if (IsPathEnd(destination) || IsPathEnd(before_to_move)) {
      return false;
    }
    return MakeChainInactive(destination, Next(destination)) &&
           MoveChain(before_to_move, Next(before_to_move), destination);
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
                            std::function<int(int64)> start_empty_path_class)
      : PathOperator(vars, secondary_vars, 2,
                     std::move(start_empty_path_class)) {}
  ~MakeChainInactiveOperator() override {}
  bool MakeNeighbor() override {
    return MakeChainInactive(BaseNode(0), BaseNode(1));
  }

  std::string DebugString() const override { return "MakeChainInactiveOperator"; }

 protected:
  bool OnSamePathAsPreviousBase(int64 base_index) override {
    // Start and end of chain (defined by both base nodes) must be on the same
    // path.
    return true;
  }

  int64 GetBaseNodeRestartPosition(int base_index) override {
    // Base node 1 must be after base node 0.
    if (base_index == 0) {
      return StartNode(base_index);
    } else {
      return BaseNode(base_index - 1);
    }
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
                     std::function<int(int64)> start_empty_path_class)
      : BaseInactiveNodeToPathOperator(vars, secondary_vars, 1,
                                       std::move(start_empty_path_class)) {}
  ~SwapActiveOperator() override {}
  bool MakeNeighbor() override;

  std::string DebugString() const override { return "SwapActiveOperator"; }
};

bool SwapActiveOperator::MakeNeighbor() {
  const int64 base = BaseNode(0);
  if (IsPathEnd(base)) {
    return false;
  }
  return MakeChainInactive(base, Next(base)) &&
         MakeActive(GetInactiveNode(), base);
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
                             std::function<int(int64)> start_empty_path_class)
      : BaseInactiveNodeToPathOperator(vars, secondary_vars, 2,
                                       std::move(start_empty_path_class)) {}
  ~ExtendedSwapActiveOperator() override {}
  bool MakeNeighbor() override;

  std::string DebugString() const override { return "ExtendedSwapActiveOperator"; }
};

bool ExtendedSwapActiveOperator::MakeNeighbor() {
  const int64 base0 = BaseNode(0);
  if (IsPathEnd(base0)) {
    return false;
  }
  const int64 base1 = BaseNode(1);
  if (IsPathEnd(base1)) {
    return false;
  }
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
  std::vector<std::vector<int64>> cost_;
  HamiltonianPathSolver<int64, std::vector<std::vector<int64>>>
      hamiltonian_path_solver_;
  Solver::IndexEvaluator3 evaluator_;
  const int chain_length_;
};

TSPOpt::TSPOpt(const std::vector<IntVar*>& vars,
               const std::vector<IntVar*>& secondary_vars,
               Solver::IndexEvaluator3 evaluator, int chain_length)
    : PathOperator(vars, secondary_vars, 1, nullptr),
      hamiltonian_path_solver_(cost_),
      evaluator_(std::move(evaluator)),
      chain_length_(chain_length) {}

bool TSPOpt::MakeNeighbor() {
  std::vector<int64> nodes;
  int64 chain_end = BaseNode(0);
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
  int64 chain_path = Path(BaseNode(0));
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
  std::vector<std::vector<int64>> cost_;
  HamiltonianPathSolver<int64, std::vector<std::vector<int64>>>
      hamiltonian_path_solver_;
  Solver::IndexEvaluator3 evaluator_;
  const int tsp_size_;
  ACMRandom rand_;
};

TSPLns::TSPLns(const std::vector<IntVar*>& vars,
               const std::vector<IntVar*>& secondary_vars,
               Solver::IndexEvaluator3 evaluator, int tsp_size)
    : PathOperator(vars, secondary_vars, 1, nullptr),
      hamiltonian_path_solver_(cost_),
      evaluator_(std::move(evaluator)),
      tsp_size_(tsp_size),
      rand_(ACMRandom::HostnamePidTimeSeed()) {
  cost_.resize(tsp_size_);
  for (int i = 0; i < tsp_size_; ++i) {
    cost_[i].resize(tsp_size_);
  }
}

bool TSPLns::MakeOneNeighbor() {
  while (true) {
    if (PathOperator::MakeOneNeighbor()) {
      return true;
    }
  }
  return false;
}

bool TSPLns::MakeNeighbor() {
  const int64 base_node = BaseNode(0);
  if (IsPathEnd(base_node)) {
    return false;
  }
  std::vector<int64> nodes;
  for (int64 node = StartNode(0); !IsPathEnd(node); node = Next(node)) {
    nodes.push_back(node);
  }
  if (nodes.size() <= tsp_size_) {
    return false;
  }
  // Randomly select break nodes (final nodes of a meta-node, after which
  // an arc is relaxed.
  std::unordered_set<int64> breaks_set;
  // Always add base node to break nodes (diversification)
  breaks_set.insert(base_node);
  while (breaks_set.size() < tsp_size_) {
    const int64 one_break = nodes[rand_.Uniform(nodes.size())];
    if (!ContainsKey(breaks_set, one_break)) {
      breaks_set.insert(one_break);
    }
  }
  CHECK_EQ(breaks_set.size(), tsp_size_);
  // Setup break node indexing and internal meta-node cost (cost of partial
  // route starting at first node of the meta-node and ending at its last node);
  // this cost has to be added to the TSP matrix cost in order to respect the
  // triangle inequality.
  std::vector<int> breaks;
  std::vector<int64> meta_node_costs;
  int64 cost = 0;
  int64 node = StartNode(0);
  int64 node_path = Path(node);
  while (!IsPathEnd(node)) {
    int64 next = Next(node);
    if (ContainsKey(breaks_set, node)) {
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
  virtual ~NearestNeighbors() {}
  void Initialize();
  const std::vector<int>& Neighbors(int index) const;

  virtual std::string DebugString() const { return "NearestNeighbors"; }

 private:
  void ComputeNearest(int row);

  std::vector<std::vector<int>> neighbors_;
  Solver::IndexEvaluator3 evaluator_;
  const PathOperator& path_operator_;
  const int size_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(NearestNeighbors);
};

NearestNeighbors::NearestNeighbors(Solver::IndexEvaluator3 evaluator,
                                   const PathOperator& path_operator, int size)
    : evaluator_(std::move(evaluator)),
      path_operator_(path_operator),
      size_(size),
      initialized_(false) {}

void NearestNeighbors::Initialize() {
  // TODO(user): recompute if node changes path ?
  if (!initialized_) {
    initialized_ = true;
    for (int i = 0; i < path_operator_.number_of_nexts(); ++i) {
      neighbors_.push_back(std::vector<int>());
      ComputeNearest(i);
    }
  }
}

const std::vector<int>& NearestNeighbors::Neighbors(int index) const {
  return neighbors_[index];
}

void NearestNeighbors::ComputeNearest(int row) {
  // Find size_ nearest neighbors for row of index 'row'.
  const int path = path_operator_.Path(row);
  const IntVar* var = path_operator_.Var(row);
  const int64 var_min = var->Min();
  const int var_size = var->Max() - var_min + 1;
  using ValuedIndex = std::pair<int /*index*/, int64 /*value*/>;
  std::vector<ValuedIndex> neighbors(var_size);
  for (int i = 0; i < var_size; ++i) {
    const int index = i + var_min;
    neighbors[i] = std::make_pair(index, evaluator_(row, index, path));
  }
  if (var_size > size_) {
    std::nth_element(neighbors.begin(), neighbors.begin() + size_ - 1,
                     neighbors.end(),
                     [](const ValuedIndex& a, const ValuedIndex& b) {
                       return a.second < b.second;
                     });
  }

  // Setup global neighbor matrix for row row_index
  for (int i = 0; i < std::min(size_, var_size); ++i) {
    neighbors_[row].push_back(neighbors[i].first);
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

  bool InFromOut(int64 in_i, int64 in_j, int64* out, int64* gain);

  Solver::IndexEvaluator3 const evaluator_;
  NearestNeighbors neighbors_;
  std::unordered_set<int64> marked_;
  const bool topt_;
};

// While the accumulated local gain is positive, perform a 2opt or a 3opt move
// followed by a series of 2opt moves. Return a neighbor for which the global
// gain is positive.

LinKernighan::LinKernighan(const std::vector<IntVar*>& vars,
                           const std::vector<IntVar*>& secondary_vars,
                           const Solver::IndexEvaluator3& evaluator, bool topt)
    : PathOperator(vars, secondary_vars, 1, nullptr),
      evaluator_(evaluator),
      neighbors_(evaluator, *this, kNeighbors),
      topt_(topt) {}

LinKernighan::~LinKernighan() {}

void LinKernighan::OnNodeInitialization() { neighbors_.Initialize(); }

bool LinKernighan::MakeNeighbor() {
  marked_.clear();
  int64 node = BaseNode(0);
  if (IsPathEnd(node)) return false;
  int64 path = Path(node);
  int64 base = node;
  int64 next = Next(node);
  if (IsPathEnd(next)) return false;
  int64 out = -1;
  int64 gain = 0;
  marked_.insert(node);
  if (topt_) {  // Try a 3opt first
    if (InFromOut(node, next, &out, &gain)) {
      marked_.insert(next);
      marked_.insert(out);
      const int64 node1 = out;
      if (IsPathEnd(node1)) return false;
      const int64 next1 = Next(node1);
      if (IsPathEnd(next1)) return false;
      if (InFromOut(node1, next1, &out, &gain)) {
        marked_.insert(next1);
        marked_.insert(out);
        if (MoveChain(out, node1, node)) {
          const int64 next_out = Next(out);
          int64 in_cost = evaluator_(node, next_out, path);
          int64 out_cost = evaluator_(out, next_out, path);
          if (CapAdd(CapSub(gain, in_cost), out_cost) > 0) return true;
          node = out;
          if (IsPathEnd(node)) {
            return false;
          }
          next = next_out;
          if (IsPathEnd(next)) {
            return false;
          }
        } else {
          return false;
        }
      } else {
        return false;
      }
    } else {
      return false;
    }
  }
  // Try 2opts
  while (InFromOut(node, next, &out, &gain)) {
    marked_.insert(next);
    marked_.insert(out);
    int64 chain_last;
    if (!ReverseChain(node, out, &chain_last)) {
      return false;
    }
    int64 in_cost = evaluator_(base, chain_last, path);
    int64 out_cost = evaluator_(chain_last, out, path);
    if (CapAdd(CapSub(gain, in_cost), out_cost) > 0) {
      return true;
    }
    node = chain_last;
    if (IsPathEnd(node)) {
      return false;
    }
    next = out;
    if (IsPathEnd(next)) {
      return false;
    }
  }
  return false;
}

const int LinKernighan::kNeighbors = 5 + 1;

bool LinKernighan::InFromOut(int64 in_i, int64 in_j, int64* out, int64* gain) {
  const std::vector<int>& nexts = neighbors_.Neighbors(in_j);
  int64 best_gain = kint64min;
  int64 path = Path(in_i);
  int64 out_cost = evaluator_(in_i, in_j, path);
  const int64 current_gain = CapAdd(*gain, out_cost);
  for (int k = 0; k < nexts.size(); ++k) {
    const int64 next = nexts[k];
    if (next != in_j) {
      int64 in_cost = evaluator_(in_j, next, path);
      int64 new_gain = CapSub(current_gain, in_cost);
      if (new_gain > 0 && next != Next(in_j) && marked_.count(in_j) == 0 &&
          marked_.count(next) == 0) {
        if (best_gain < new_gain) {
          *out = next;
          best_gain = new_gain;
        }
      }
    }
  }
  *gain = best_gain;
  return (best_gain > kint64min);
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
      : PathOperator(vars, secondary_vars, number_of_chunks, nullptr),
        number_of_chunks_(number_of_chunks),
        chunk_size_(chunk_size),
        unactive_fragments_(unactive_fragments) {
    CHECK_GE(chunk_size_, 0);
  }
  ~PathLns() override {}
  bool MakeNeighbor() override;

  std::string DebugString() const override { return "PathLns"; }

 private:
  inline bool ChainsAreFullPaths() const { return chunk_size_ == 0; }
  void DeactivateChain(int64 node0);
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

void PathLns::DeactivateChain(int64 node) {
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
  NeighborhoodLimit(LocalSearchOperator* const op, int64 limit)
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

  std::string DebugString() const override { return "NeighborhoodLimit"; }

 private:
  LocalSearchOperator* const operator_;
  const int64 limit_;
  int64 next_neighborhood_calls_;
};

LocalSearchOperator* Solver::MakeNeighborhoodLimit(
    LocalSearchOperator* const op, int64 limit) {
  return RevAlloc(new NeighborhoodLimit(op, limit));
}

// ----- Concatenation of operators -----

namespace {
class CompoundOperator : public LocalSearchOperator {
 public:
  CompoundOperator(std::vector<LocalSearchOperator*> operators,
                   std::function<int64(int, int)> evaluator);
  ~CompoundOperator() override {}
  void Start(const Assignment* assignment) override;
  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override;

  std::string DebugString() const override {
    return operators_[operator_indices_[index_]]->DebugString();
  }

 private:
  class OperatorComparator {
   public:
    OperatorComparator(std::function<int64(int, int)> evaluator,
                       int active_operator)
        : evaluator_(std::move(evaluator)), active_operator_(active_operator) {}
    bool operator()(int lhs, int rhs) const {
      const int64 lhs_value = Evaluate(lhs);
      const int64 rhs_value = Evaluate(rhs);
      return lhs_value < rhs_value || (lhs_value == rhs_value && lhs < rhs);
    }

   private:
    int64 Evaluate(int operator_index) const {
      return evaluator_(active_operator_, operator_index);
    }

    std::function<int64(int, int)> evaluator_;
    const int active_operator_;
  };

  int64 index_;
  std::vector<LocalSearchOperator*> operators_;
  std::vector<int> operator_indices_;
  std::function<int64(int, int)> evaluator_;
  Bitset64<> started_;
  const Assignment* start_assignment_;
};

CompoundOperator::CompoundOperator(std::vector<LocalSearchOperator*> operators,
                                   std::function<int64(int, int)> evaluator)
    : index_(0),
      operators_(std::move(operators)),
      evaluator_(std::move(evaluator)),
      started_(operators_.size()),
      start_assignment_(nullptr) {
  operators_.erase(std::remove(operators_.begin(), operators_.end(), nullptr),
                   operators_.end());
  operator_indices_.resize(operators_.size());
  std::iota(operator_indices_.begin(), operator_indices_.end(), 0);
}

void CompoundOperator::Start(const Assignment* assignment) {
  start_assignment_ = assignment;
  started_.ClearAll();
  if (!operators_.empty()) {
    OperatorComparator comparator(evaluator_, operator_indices_[index_]);
    std::sort(operator_indices_.begin(), operator_indices_.end(), comparator);
    index_ = 0;
  }
}

bool CompoundOperator::MakeNextNeighbor(Assignment* delta,
                                        Assignment* deltadelta) {
  if (!operators_.empty()) {
    do {
      // TODO(user): keep copy of delta in case MakeNextNeighbor
      // pollutes delta on a fail.
      const int64 operator_index = operator_indices_[index_];
      if (!started_[operator_index]) {
        operators_[operator_index]->Start(start_assignment_);
        started_.Set(operator_index);
      }
      if (operators_[operator_index]->MakeNextNeighbor(delta, deltadelta)) {
        return true;
      }
      ++index_;
      if (index_ == operators_.size()) {
        index_ = 0;
      }
    } while (index_ != 0);
  }
  return false;
}

int64 CompoundOperatorNoRestart(int size, int active_index,
                                int operator_index) {
  if (operator_index < active_index) {
    return size + operator_index - active_index;
  } else {
    return operator_index - active_index;
  }
}

int64 CompoundOperatorRestart(int active_index, int operator_index) {
  return 0;
}
}  // namespace

LocalSearchOperator* Solver::ConcatenateOperators(
    const std::vector<LocalSearchOperator*>& ops) {
  return ConcatenateOperators(ops, false);
}

LocalSearchOperator* Solver::ConcatenateOperators(
    const std::vector<LocalSearchOperator*>& ops, bool restart) {
  if (restart) {
    std::function<int64(int, int)> eval = CompoundOperatorRestart;
    return ConcatenateOperators(ops, eval);
  } else {
    const int size = ops.size();
    return ConcatenateOperators(ops, [size](int i, int j) {
      return CompoundOperatorNoRestart(size, i, j);
    });
  }
}

LocalSearchOperator* Solver::ConcatenateOperators(
    const std::vector<LocalSearchOperator*>& ops,
    std::function<int64(int, int)> evaluator) {
  return RevAlloc(new CompoundOperator(ops, std::move(evaluator)));
}

namespace {
class RandomCompoundOperator : public LocalSearchOperator {
 public:
  explicit RandomCompoundOperator(std::vector<LocalSearchOperator*> operators);
  RandomCompoundOperator(std::vector<LocalSearchOperator*> operators,
                         int32 seed);
  ~RandomCompoundOperator() override {}
  void Start(const Assignment* assignment) override;
  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override;

  std::string DebugString() const override { return "RandomCompoundOperator"; }

 private:
  ACMRandom rand_;
  const std::vector<LocalSearchOperator*> operators_;
};

void RandomCompoundOperator::Start(const Assignment* assignment) {
  for (LocalSearchOperator* const op : operators_) {
    op->Start(assignment);
  }
}

RandomCompoundOperator::RandomCompoundOperator(
    std::vector<LocalSearchOperator*> operators)
    : RandomCompoundOperator(std::move(operators),
                             ACMRandom::HostnamePidTimeSeed()) {}

RandomCompoundOperator::RandomCompoundOperator(
    std::vector<LocalSearchOperator*> operators, int32 seed)
    : rand_(seed), operators_(std::move(operators)) {}

bool RandomCompoundOperator::MakeNextNeighbor(Assignment* delta,
                                              Assignment* deltadelta) {
  const int size = operators_.size();
  std::vector<int> indices(size);
  std::iota(indices.begin(), indices.end(), 0);
  std::random_shuffle(indices.begin(), indices.end(), rand_);
  for (int i = 0; i < size; ++i) {
    if (operators_[indices[i]]->MakeNextNeighbor(delta, deltadelta)) {
      return true;
    }
  }
  return false;
}
}  // namespace

LocalSearchOperator* Solver::RandomConcatenateOperators(
    const std::vector<LocalSearchOperator*>& ops) {
  return RevAlloc(new RandomCompoundOperator(ops));
}

LocalSearchOperator* Solver::RandomConcatenateOperators(
    const std::vector<LocalSearchOperator*>& ops, int32 seed) {
  return RevAlloc(new RandomCompoundOperator(ops, seed));
}

// ----- Operator factory -----

template <class T>
LocalSearchOperator* MakeLocalSearchOperator(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class) {
  return solver->RevAlloc(
      new T(vars, secondary_vars, std::move(start_empty_path_class)));
}

#define MAKE_LOCAL_SEARCH_OPERATOR(OperatorClass)                  \
  template <>                                                      \
  LocalSearchOperator* MakeLocalSearchOperator<OperatorClass>(     \
      Solver * solver, const std::vector<IntVar*>& vars,                \
      const std::vector<IntVar*>& secondary_vars,                       \
      std::function<int(int64)> start_empty_path_class) {          \
    return solver->RevAlloc(new OperatorClass(                     \
        vars, secondary_vars, std::move(start_empty_path_class))); \
  }

MAKE_LOCAL_SEARCH_OPERATOR(TwoOpt)
MAKE_LOCAL_SEARCH_OPERATOR(Relocate)
MAKE_LOCAL_SEARCH_OPERATOR(Exchange)
MAKE_LOCAL_SEARCH_OPERATOR(Cross)
MAKE_LOCAL_SEARCH_OPERATOR(MakeActiveOperator)
MAKE_LOCAL_SEARCH_OPERATOR(MakeInactiveOperator)
MAKE_LOCAL_SEARCH_OPERATOR(MakeChainInactiveOperator)
MAKE_LOCAL_SEARCH_OPERATOR(SwapActiveOperator)
MAKE_LOCAL_SEARCH_OPERATOR(ExtendedSwapActiveOperator)
MAKE_LOCAL_SEARCH_OPERATOR(MakeActiveAndRelocate)
MAKE_LOCAL_SEARCH_OPERATOR(RelocateAndMakeActiveOperator)
MAKE_LOCAL_SEARCH_OPERATOR(RelocateAndMakeInactiveOperator)

#undef MAKE_LOCAL_SEARCH_OPERATOR

LocalSearchOperator* Solver::MakeOperator(const std::vector<IntVar*>& vars,
                                          Solver::LocalSearchOperators op) {
  return MakeOperator(vars, std::vector<IntVar*>(), op);
}

LocalSearchOperator* Solver::MakeOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    Solver::LocalSearchOperators op) {
  LocalSearchOperator* result = nullptr;
  switch (op) {
    case Solver::TWOOPT: {
      result = RevAlloc(new TwoOpt(vars, secondary_vars, nullptr));
      break;
    }
    case Solver::OROPT: {
      std::vector<LocalSearchOperator*> operators;
      for (int i = 1; i < 4; ++i) {
        operators.push_back(RevAlloc(
            new Relocate(vars, secondary_vars, "OrOpt", nullptr, i, true)));
      }
      result = ConcatenateOperators(operators);
      break;
    }
    case Solver::RELOCATE: {
      result = MakeLocalSearchOperator<Relocate>(this, vars, secondary_vars,
                                                 nullptr);
      break;
    }
    case Solver::EXCHANGE: {
      result = MakeLocalSearchOperator<Exchange>(this, vars, secondary_vars,
                                                 nullptr);
      break;
    }
    case Solver::CROSS: {
      result =
          MakeLocalSearchOperator<Cross>(this, vars, secondary_vars, nullptr);
      break;
    }
    case Solver::MAKEACTIVE: {
      result = MakeLocalSearchOperator<MakeActiveOperator>(
          this, vars, secondary_vars, nullptr);
      break;
    }
    case Solver::MAKEINACTIVE: {
      result = MakeLocalSearchOperator<MakeInactiveOperator>(
          this, vars, secondary_vars, nullptr);
      break;
    }
    case Solver::MAKECHAININACTIVE: {
      result = MakeLocalSearchOperator<MakeChainInactiveOperator>(
          this, vars, secondary_vars, nullptr);
      break;
    }
    case Solver::SWAPACTIVE: {
      result = MakeLocalSearchOperator<SwapActiveOperator>(
          this, vars, secondary_vars, nullptr);
      break;
    }
    case Solver::EXTENDEDSWAPACTIVE: {
      result = MakeLocalSearchOperator<ExtendedSwapActiveOperator>(
          this, vars, secondary_vars, nullptr);
      break;
    }
    case Solver::PATHLNS: {
      result = RevAlloc(new PathLns(vars, secondary_vars, 2, 3, false));
      break;
    }
    case Solver::FULLPATHLNS: {
      result = RevAlloc(new PathLns(vars, secondary_vars,
                                    /*number_of_chunks=*/1,
                                    /*chunk_size=*/0,
                                    /*unactive_fragments=*/true));
      break;
    }
    case Solver::UNACTIVELNS: {
      result = RevAlloc(new PathLns(vars, secondary_vars, 1, 6, true));
      break;
    }
    case Solver::INCREMENT: {
      if (secondary_vars.empty()) {
        result = RevAlloc(new IncrementValue(vars));
      } else {
        LOG(FATAL) << "Operator " << op
                   << " does not support secondary variables";
      }
      break;
    }
    case Solver::DECREMENT: {
      if (secondary_vars.empty()) {
        result = RevAlloc(new DecrementValue(vars));
      } else {
        LOG(FATAL) << "Operator " << op
                   << " does not support secondary variables";
      }
      break;
    }
    case Solver::SIMPLELNS: {
      if (secondary_vars.empty()) {
        result = RevAlloc(new SimpleLns(vars, 1));
      } else {
        LOG(FATAL) << "Operator " << op
                   << " does not support secondary variables";
      }
      break;
    }
    default:
      LOG(FATAL) << "Unknown operator " << op;
  }
  return result;
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
  LocalSearchOperator* result = nullptr;
  switch (op) {
    case Solver::LK: {
      std::vector<LocalSearchOperator*> operators;
      operators.push_back(RevAlloc(
          new LinKernighan(vars, secondary_vars, evaluator, /*topt=*/false)));
      operators.push_back(RevAlloc(
          new LinKernighan(vars, secondary_vars, evaluator, /*topt=*/true)));
      result = ConcatenateOperators(operators);
      break;
    }
    case Solver::TSPOPT: {
      result = RevAlloc(new TSPOpt(vars, secondary_vars, evaluator,
                                   FLAGS_cp_local_search_tsp_opt_size));
      break;
    }
    case Solver::TSPLNS: {
      result = RevAlloc(new TSPLns(vars, secondary_vars, evaluator,
                                   FLAGS_cp_local_search_tsp_lns_size));
      break;
    }
    default:
      LOG(FATAL) << "Unknown operator " << op;
  }
  return result;
}

namespace {
// Classes for Local Search Operation used in Local Search filters.

class SumOperation {
 public:
  SumOperation() : value_(0) {}
  void Init() { value_ = 0; }
  void Update(int64 update) { value_ = CapAdd(value_, update); }
  void Remove(int64 remove) { value_ = CapSub(value_, remove); }
  int64 value() const { return value_; }
  void set_value(int64 new_value) { value_ = new_value; }

 private:
  int64 value_;
};

class ProductOperation {
 public:
  ProductOperation() : value_(1) {}
  void Init() { value_ = 1; }
  void Update(int64 update) { value_ *= update; }
  void Remove(int64 remove) {
    if (remove != 0) {
      value_ /= remove;
    }
  }
  int64 value() const { return value_; }
  void set_value(int64 new_value) { value_ = new_value; }

 private:
  int64 value_;
};

class MinOperation {
 public:
  void Init() { values_set_.clear(); }
  void Update(int64 update) { values_set_.insert(update); }
  void Remove(int64 remove) { values_set_.erase(remove); }
  int64 value() const {
    return (!values_set_.empty()) ? *values_set_.begin() : 0;
  }
  void set_value(int64 new_value) {}

 private:
  std::set<int64> values_set_;
};

class MaxOperation {
 public:
  void Init() { values_set_.clear(); }
  void Update(int64 update) { values_set_.insert(update); }
  void Remove(int64 remove) { values_set_.erase(remove); }
  int64 value() const {
    return (!values_set_.empty()) ? *values_set_.rbegin() : 0;
  }
  void set_value(int64 new_value) {}

 private:
  std::set<int64> values_set_;
};

// ----- Variable domain filter -----
// Rejects assignments to values outside the domain of variables

class VariableDomainFilter : public LocalSearchFilter {
 public:
  VariableDomainFilter() {}
  ~VariableDomainFilter() override {}
  bool Accept(const Assignment* delta, const Assignment* deltadelta) override;
  void Synchronize(const Assignment* assignment,
                   const Assignment* delta) override {}

  std::string DebugString() const override { return "VariableDomainFilter"; }
};

bool VariableDomainFilter::Accept(const Assignment* delta,
                                  const Assignment* deltadelta) {
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
        const int64 kUnallocated = -1;
        int64 index = kUnallocated;
        if (FindIndex(var, &index)) {
          values_[index] = element.Value();
          var_synced_[index] = true;
        }
      }
    }
  }
}

// ----- Objective filter ------
// Assignment is accepted if it improves the best objective value found so far.
// 'Values' callback takes an index of a variable and its value and returns the
// contribution into the objective value. The type of objective function
// is determined by LocalSearchOperation enum. Conditions on neighbor
// acceptance are presented in LocalSearchFilterBound enum. Objective function
// can be represented by any variable.

namespace {
template <typename Operator>
class ObjectiveFilter : public IntVarLocalSearchFilter {
 public:
  ObjectiveFilter(const std::vector<IntVar*>& vars,
                  Solver::ObjectiveWatcher delta_objective_callback,
                  const IntVar* const objective,
                  Solver::LocalSearchFilterBound filter_enum)
      : IntVarLocalSearchFilter(vars),
        primary_vars_size_(vars.size()),
        cache_(new int64[vars.size()]),
        delta_cache_(new int64[vars.size()]),
        delta_objective_callback_(std::move(delta_objective_callback)),
        objective_(objective),
        filter_enum_(filter_enum),
        op_(),
        old_value_(0),
        old_delta_value_(0),
        incremental_(false) {
    for (int i = 0; i < Size(); ++i) {
      cache_[i] = 0;
      delta_cache_[i] = 0;
    }
    op_.Init();
    old_value_ = op_.value();
  }
  ~ObjectiveFilter() override {
    delete[] cache_;
    delete[] delta_cache_;
  }
  bool Accept(const Assignment* delta, const Assignment* deltadelta) override {
    if (delta == nullptr) {
      return false;
    }
    int64 value = 0;
    if (!deltadelta->Empty()) {
      if (!incremental_) {
        value = Evaluate(delta, old_value_, cache_, true);
      } else {
        value = Evaluate(deltadelta, old_delta_value_, delta_cache_, true);
      }
      incremental_ = true;
    } else {
      if (incremental_) {
        for (int i = 0; i < primary_vars_size_; ++i) {
          delta_cache_[i] = cache_[i];
        }
        old_delta_value_ = old_value_;
      }
      incremental_ = false;
      value = Evaluate(delta, old_value_, cache_, false);
    }
    old_delta_value_ = value;
    int64 var_min = objective_->Min();
    int64 var_max = objective_->Max();
    if (delta->Objective() == objective_) {
      var_min = std::max(var_min, delta->ObjectiveMin());
      var_max = std::min(var_max, delta->ObjectiveMax());
    }
    if (delta_objective_callback_ != nullptr) {
      delta_objective_callback_(value);
    }
    switch (filter_enum_) {
      case Solver::LE: {
        return value <= var_max;
      }
      case Solver::GE: {
        return value >= var_min;
      }
      case Solver::EQ: {
        return value <= var_max && value >= var_min;
      }
      default: {
        LOG(ERROR) << "Unknown local search filter enum value";
        return false;
      }
    }
  }
  virtual int64 SynchronizedElementValue(int64 index) = 0;
  virtual bool EvaluateElementValue(const Assignment::IntContainer& container,
                                    int index, int* container_index,
                                    int64* obj_value) = 0;
  bool IsIncremental() const override { return true; }

  std::string DebugString() const override { return "ObjectiveFilter"; }

 protected:
  const int primary_vars_size_;
  int64* const cache_;
  int64* const delta_cache_;
  Solver::ObjectiveWatcher delta_objective_callback_;
  const IntVar* const objective_;
  Solver::LocalSearchFilterBound filter_enum_;
  Operator op_;
  int64 old_value_;
  int64 old_delta_value_;
  bool incremental_;

 private:
  void OnSynchronize(const Assignment* delta) override {
    op_.Init();
    for (int i = 0; i < primary_vars_size_; ++i) {
      const int64 obj_value = SynchronizedElementValue(i);
      cache_[i] = obj_value;
      delta_cache_[i] = obj_value;
      op_.Update(obj_value);
    }
    old_value_ = op_.value();
    old_delta_value_ = old_value_;
    incremental_ = false;
    if (delta_objective_callback_ != nullptr) {
      delta_objective_callback_(op_.value());
    }
  }
  int64 Evaluate(const Assignment* delta, int64 current_value,
                 const int64* const out_values, bool cache_delta_values) {
    if (current_value == kint64max) return current_value;
    op_.set_value(current_value);
    const Assignment::IntContainer& container = delta->IntVarContainer();
    const int size = container.Size();
    for (int i = 0; i < size; ++i) {
      const IntVarElement& new_element = container.Element(i);
      IntVar* const var = new_element.Var();
      int64 index = -1;
      if (FindIndex(var, &index) && index < primary_vars_size_) {
        op_.Remove(out_values[index]);
        int64 obj_value = 0LL;
        if (EvaluateElementValue(container, index, &i, &obj_value)) {
          op_.Update(obj_value);
          if (cache_delta_values) {
            delta_cache_[index] = obj_value;
          }
        }
      }
    }
    return op_.value();
  }
};

template <typename Operator>
class BinaryObjectiveFilter : public ObjectiveFilter<Operator> {
 public:
  BinaryObjectiveFilter(const std::vector<IntVar*>& vars,
                        Solver::IndexEvaluator2 value_evaluator,
                        Solver::ObjectiveWatcher delta_objective_callback,
                        const IntVar* const objective,
                        Solver::LocalSearchFilterBound filter_enum)
      : ObjectiveFilter<Operator>(vars, delta_objective_callback, objective,
                                  filter_enum),
        value_evaluator_(std::move(value_evaluator)) {}
  ~BinaryObjectiveFilter() override {}
  int64 SynchronizedElementValue(int64 index) override {
    return IntVarLocalSearchFilter::IsVarSynced(index)
               ? value_evaluator_(index, IntVarLocalSearchFilter::Value(index))
               : 0;
  }
  bool EvaluateElementValue(const Assignment::IntContainer& container,
                            int index, int* container_index,
                            int64* obj_value) override {
    const IntVarElement& element = container.Element(*container_index);
    if (element.Activated()) {
      *obj_value = value_evaluator_(index, element.Value());
      return true;
    } else {
      const IntVar* var = element.Var();
      if (var->Bound()) {
        *obj_value = value_evaluator_(index, var->Min());
        return true;
      }
    }
    return false;
  }

 private:
  Solver::IndexEvaluator2 value_evaluator_;
};

template <typename Operator>
class TernaryObjectiveFilter : public ObjectiveFilter<Operator> {
 public:
  TernaryObjectiveFilter(const std::vector<IntVar*>& vars,
                         const std::vector<IntVar*>& secondary_vars,
                         Solver::IndexEvaluator3 value_evaluator,
                         Solver::ObjectiveWatcher delta_objective_callback,
                         const IntVar* const objective,
                         Solver::LocalSearchFilterBound filter_enum)
      : ObjectiveFilter<Operator>(vars, delta_objective_callback, objective,
                                  filter_enum),
        secondary_vars_offset_(vars.size()),
        value_evaluator_(std::move(value_evaluator)) {
    IntVarLocalSearchFilter::AddVars(secondary_vars);
    CHECK_GE(IntVarLocalSearchFilter::Size(), 0);
  }
  ~TernaryObjectiveFilter() override {}
  int64 SynchronizedElementValue(int64 index) override {
    DCHECK_LT(index, secondary_vars_offset_);
    return IntVarLocalSearchFilter::IsVarSynced(index)
               ? value_evaluator_(index, IntVarLocalSearchFilter::Value(index),
                                  IntVarLocalSearchFilter::Value(
                                      index + secondary_vars_offset_))
               : 0;
  }
  bool EvaluateElementValue(const Assignment::IntContainer& container,
                            int index, int* container_index,
                            int64* obj_value) override {
    DCHECK_LT(index, secondary_vars_offset_);
    *obj_value = 0LL;
    const IntVarElement& element = container.Element(*container_index);
    const IntVar* secondary_var =
        IntVarLocalSearchFilter::Var(index + secondary_vars_offset_);
    if (element.Activated()) {
      const int64 value = element.Value();
      int hint_index = *container_index + 1;
      if (hint_index < container.Size() &&
          secondary_var == container.Element(hint_index).Var()) {
        *obj_value = value_evaluator_(index, value,
                                      container.Element(hint_index).Value());
        *container_index = hint_index;
      } else {
        *obj_value = value_evaluator_(index, value,
                                      container.Element(secondary_var).Value());
      }
      return true;
    } else {
      const IntVar* var = element.Var();
      if (var->Bound() && secondary_var->Bound()) {
        *obj_value = value_evaluator_(index, var->Min(), secondary_var->Min());
        return true;
      }
    }
    return false;
  }

 private:
  int secondary_vars_offset_;
  Solver::IndexEvaluator3 value_evaluator_;
};
}  // namespace

// ---- Local search filter factory ----

#define ReturnObjectiveFilter5(Filter, op_enum, arg0, arg1, arg2, arg3, arg4)  \
  switch (op_enum) {                                                           \
    case Solver::SUM: {                                                        \
      return RevAlloc(new Filter<SumOperation>(arg0, arg1, arg2, arg3, arg4)); \
    }                                                                          \
    case Solver::PROD: {                                                       \
      return RevAlloc(                                                         \
          new Filter<ProductOperation>(arg0, arg1, arg2, arg3, arg4));         \
    }                                                                          \
    case Solver::MAX: {                                                        \
      return RevAlloc(new Filter<MaxOperation>(arg0, arg1, arg2, arg3, arg4)); \
    }                                                                          \
    case Solver::MIN: {                                                        \
      return RevAlloc(new Filter<MinOperation>(arg0, arg1, arg2, arg3, arg4)); \
    }                                                                          \
    default:                                                                   \
      LOG(FATAL) << "Unknown operator " << op_enum;                            \
  }                                                                            \
  return nullptr;

#define ReturnObjectiveFilter6(Filter, op_enum, arg0, arg1, arg2, arg3, arg4, \
                               arg5)                                          \
  switch (op_enum) {                                                          \
    case Solver::SUM: {                                                       \
      return RevAlloc(                                                        \
          new Filter<SumOperation>(arg0, arg1, arg2, arg3, arg4, arg5));      \
    }                                                                         \
    case Solver::PROD: {                                                      \
      return RevAlloc(                                                        \
          new Filter<ProductOperation>(arg0, arg1, arg2, arg3, arg4, arg5));  \
    }                                                                         \
    case Solver::MAX: {                                                       \
      return RevAlloc(                                                        \
          new Filter<MaxOperation>(arg0, arg1, arg2, arg3, arg4, arg5));      \
    }                                                                         \
    case Solver::MIN: {                                                       \
      return RevAlloc(                                                        \
          new Filter<MinOperation>(arg0, arg1, arg2, arg3, arg4, arg5));      \
    }                                                                         \
    default:                                                                  \
      LOG(FATAL) << "Unknown operator " << op_enum;                           \
  }                                                                           \
  return nullptr;

LocalSearchFilter* Solver::MakeLocalSearchObjectiveFilter(
    const std::vector<IntVar*>& vars, Solver::IndexEvaluator2 values,
    IntVar* const objective, Solver::LocalSearchFilterBound filter_enum,
    Solver::LocalSearchOperation op_enum) {
  ReturnObjectiveFilter5(BinaryObjectiveFilter, op_enum, vars, values, nullptr,
                         objective, filter_enum);
}

LocalSearchFilter* Solver::MakeLocalSearchObjectiveFilter(
    const std::vector<IntVar*>& vars, Solver::IndexEvaluator2 values,
    ObjectiveWatcher delta_objective_callback, IntVar* const objective,
    Solver::LocalSearchFilterBound filter_enum,
    Solver::LocalSearchOperation op_enum) {
  ReturnObjectiveFilter5(BinaryObjectiveFilter, op_enum, vars, values,
                         delta_objective_callback, objective, filter_enum);
}

LocalSearchFilter* Solver::MakeLocalSearchObjectiveFilter(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars, Solver::IndexEvaluator3 values,
    IntVar* const objective, Solver::LocalSearchFilterBound filter_enum,
    Solver::LocalSearchOperation op_enum) {
  ReturnObjectiveFilter6(TernaryObjectiveFilter, op_enum, vars, secondary_vars,
                         values, nullptr, objective, filter_enum);
}

LocalSearchFilter* Solver::MakeLocalSearchObjectiveFilter(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars, Solver::IndexEvaluator3 values,
    ObjectiveWatcher delta_objective_callback, IntVar* const objective,
    Solver::LocalSearchFilterBound filter_enum,
    Solver::LocalSearchOperation op_enum) {
  ReturnObjectiveFilter6(TernaryObjectiveFilter, op_enum, vars, secondary_vars,
                         values, delta_objective_callback, objective,
                         filter_enum);
}
#undef ReturnObjectiveFilter6
#undef ReturnObjectiveFilter5

// ----- LocalSearchProfiler -----

class LocalSearchProfiler : public LocalSearchMonitor {
 public:
  explicit LocalSearchProfiler(Solver* solver) : LocalSearchMonitor(solver) {}
  void RestartSearch() override {
    operator_stats_.clear();
    filter_stats_.clear();
  }
  void ExitSearch() override {
    // Update times for current operator when the search ends.
    if (solver()->TopLevelSearch() == solver()->ActiveSearch()) {
      UpdateTime();
    }
  }
  std::string PrintOverview() const {
    size_t op_name_size = 0;
    for (const auto& stat : operator_stats_) {
      op_name_size = std::max(op_name_size, stat.first.length());
    }
    std::string overview = "Local search operator statistics:\n";
    StringAppendF(&overview,
                  StrCat("%", op_name_size,
                               "s | Neighbors | Filtered | "
                               "Accepted | Time (s)\n")
                      .c_str(),
                  "");
    OperatorStats total_stats;
    const std::string row_format =
        StrCat("%", op_name_size, "s | %9d | %8d | %8d | %7.2g\n");
    for (const auto& stat : operator_stats_) {
      StringAppendF(&overview, row_format.c_str(), stat.first.c_str(),
                    stat.second.neighbors, stat.second.filtered_neighbors,
                    stat.second.accepted_neighbors, stat.second.seconds);
      total_stats.neighbors += stat.second.neighbors;
      total_stats.filtered_neighbors += stat.second.filtered_neighbors;
      total_stats.accepted_neighbors += stat.second.accepted_neighbors;
      total_stats.seconds += stat.second.seconds;
    }
    StringAppendF(&overview, row_format.c_str(), "Total", total_stats.neighbors,
                  total_stats.filtered_neighbors,
                  total_stats.accepted_neighbors, total_stats.seconds);
    op_name_size = 0;
    for (const auto& stat : filter_stats_) {
      op_name_size = std::max(op_name_size, stat.first.length());
    }
    StringAppendF(
        &overview,
        StrCat("Local search filter statistics:\n%", op_name_size,
                     "s |     Calls |   Rejects | Time (s) "
                     "| Rejects/s\n")
            .c_str(),
        "");
    FilterStats total_filter_stats;
    const std::string filter_row_format =
        StrCat("%", op_name_size, "s | %9d | %9d | %7.2g  | %7.2g\n");
    for (const auto& stat : filter_stats_) {
      StringAppendF(&overview, filter_row_format.c_str(), stat.first.c_str(),
                    stat.second.calls, stat.second.rejects, stat.second.seconds,
                    stat.second.rejects / stat.second.seconds);
      total_filter_stats.calls += stat.second.calls;
      total_filter_stats.rejects += stat.second.rejects;
      total_filter_stats.seconds += stat.second.seconds;
    }
    StringAppendF(&overview, filter_row_format.c_str(), "Total",
                  total_filter_stats.calls, total_filter_stats.rejects,
                  total_filter_stats.seconds,
                  total_filter_stats.rejects / total_filter_stats.seconds);
    return overview;
  }
  void BeginOperatorStart() override {}
  void EndOperatorStart() override {}
  void BeginMakeNextNeighbor(const LocalSearchOperator* op) override {
    if (last_operator_ != op->DebugString()) {
      UpdateTime();
      last_operator_ = op->DebugString();
    }
  }
  void EndMakeNextNeighbor(const LocalSearchOperator* op, bool neighbor_found,
                           const Assignment* delta,
                           const Assignment* deltadelta) override {
    if (neighbor_found) {
      operator_stats_[op->DebugString()].neighbors++;
    }
  }
  void BeginFilterNeighbor(const LocalSearchOperator* op) override {}
  void EndFilterNeighbor(const LocalSearchOperator* op,
                         bool neighbor_found) override {
    if (neighbor_found) {
      operator_stats_[op->DebugString()].filtered_neighbors++;
    }
  }
  void BeginAcceptNeighbor(const LocalSearchOperator* op) override {}
  void EndAcceptNeighbor(const LocalSearchOperator* op,
                         bool neighbor_found) override {
    if (neighbor_found) {
      operator_stats_[op->DebugString()].accepted_neighbors++;
    }
  }
  void BeginFiltering(const LocalSearchFilter* filter) override {
    filter_stats_[filter->DebugString()].calls++;
    filter_timer_.Start();
  }
  void EndFiltering(const LocalSearchFilter* filter, bool reject) override {
    filter_timer_.Stop();
    auto& stats = filter_stats_[filter->DebugString()];
    stats.seconds += filter_timer_.Get();
    if (reject) {
      stats.rejects++;
    }
  }
  void Install() override { SearchMonitor::Install(); }

 private:
  void UpdateTime() {
    if (!last_operator_.empty()) {
      timer_.Stop();
      operator_stats_[last_operator_].seconds += timer_.Get();
    }
    timer_.Start();
  }

  struct OperatorStats {
    int neighbors = 0;
    int filtered_neighbors = 0;
    int accepted_neighbors = 0;
    double seconds = 0;
  };

  struct FilterStats {
    int calls = 0;
    int rejects = 0;
    double seconds = 0;
  };
  WallTimer timer_;
  WallTimer filter_timer_;
  std::string last_operator_;
  std::map<std::string, OperatorStats> operator_stats_;
  std::map<std::string, FilterStats> filter_stats_;
};

void InstallLocalSearchProfiler(LocalSearchProfiler* monitor) {
  monitor->Install();
}

LocalSearchProfiler* BuildLocalSearchProfiler(Solver* solver) {
  if (solver->IsLocalSearchProfilingEnabled()) {
    return new LocalSearchProfiler(solver);
  } else {
    return nullptr;
  }
}

void DeleteLocalSearchProfiler(LocalSearchProfiler* monitor) { delete monitor; }

std::string Solver::LocalSearchProfile() const {
  if (local_search_profiler_ != nullptr) {
    return local_search_profiler_->PrintOverview();
  }
  return "";
}

// ----- Finds a neighbor of the assignment passed -----

class FindOneNeighbor : public DecisionBuilder {
 public:
  FindOneNeighbor(Assignment* const assignment, SolutionPool* const pool,
                  LocalSearchOperator* const ls_operator,
                  DecisionBuilder* const sub_decision_builder,
                  const SearchLimit* const limit,
                  const std::vector<LocalSearchFilter*>& filters);
  ~FindOneNeighbor() override {}
  Decision* Next(Solver* const solver) override;
  std::string DebugString() const override { return "FindOneNeighbor"; }

 private:
  bool FilterAccept(Solver* solver, const Assignment* delta,
                    const Assignment* deltadelta);
  void SynchronizeAll(Solver* solver);
  void SynchronizeFilters(const Assignment* assignment);

  Assignment* const assignment_;
  std::unique_ptr<Assignment> reference_assignment_;
  SolutionPool* const pool_;
  LocalSearchOperator* const ls_operator_;
  DecisionBuilder* const sub_decision_builder_;
  SearchLimit* limit_;
  const SearchLimit* const original_limit_;
  bool neighbor_found_;
  std::vector<LocalSearchFilter*> filters_;
};

// reference_assignment_ is used to keep track of the last assignment on which
// operators were started, assignment_ corresponding to the last successful
// neighbor.
FindOneNeighbor::FindOneNeighbor(Assignment* const assignment,
                                 SolutionPool* const pool,
                                 LocalSearchOperator* const ls_operator,
                                 DecisionBuilder* const sub_decision_builder,
                                 const SearchLimit* const limit,
                                 const std::vector<LocalSearchFilter*>& filters)
    : assignment_(assignment),
      reference_assignment_(new Assignment(assignment_)),
      pool_(pool),
      ls_operator_(ls_operator),
      sub_decision_builder_(sub_decision_builder),
      limit_(nullptr),
      original_limit_(limit),
      neighbor_found_(false),
      filters_(filters) {
  CHECK(nullptr != assignment);
  CHECK(nullptr != ls_operator);

  // If limit is nullptr, default limit is 1 solution
  if (nullptr == limit) {
    Solver* const solver = assignment_->solver();
    limit_ = solver->MakeLimit(kint64max, kint64max, kint64max, 1);
  } else {
    limit_ = limit->MakeClone();
  }
}

Decision* FindOneNeighbor::Next(Solver* const solver) {
  CHECK(nullptr != solver);

  if (original_limit_ != nullptr) {
    limit_->Copy(original_limit_);
  }

  if (!neighbor_found_) {
    // Only called on the first call to Next(), reference_assignment_ has not
    // been synced with assignment_ yet

    // Keeping the code in case a performance problem forces us to
    // use the old code with a zero test on pool_.
    // reference_assignment_->Copy(assignment_);
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
      delta->Clear();
      deltadelta->Clear();
      solver->TopPeriodicCheck();
      if (++counter >= FLAGS_cp_local_search_sync_frequency &&
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
      if (has_neighbor) {
        solver->neighbors_ += 1;
        // All filters must be called for incrementality reasons.
        // Empty deltas must also be sent to incremental filters; can be needed
        // to resync filters on non-incremental (empty) moves.
        // TODO(user): Don't call both if no filter is incremental and one
        // of them returned false.
        solver->GetLocalSearchMonitor()->BeginFilterNeighbor(ls_operator_);
        const bool mh_filter =
            AcceptDelta(solver->ParentSearch(), delta, deltadelta);
        const bool move_filter = FilterAccept(solver, delta, deltadelta);
        solver->GetLocalSearchMonitor()->EndFilterNeighbor(
            ls_operator_, mh_filter && move_filter);
        if (mh_filter && move_filter) {
          solver->filtered_neighbors_ += 1;
          assignment_copy->Copy(reference_assignment_.get());
          assignment_copy->Copy(delta);
          solver->GetLocalSearchMonitor()->BeginAcceptNeighbor(ls_operator_);
          const bool accept = solver->SolveAndCommit(restore);
          solver->GetLocalSearchMonitor()->EndAcceptNeighbor(ls_operator_,
                                                             accept);
          if (accept) {
            solver->accepted_neighbors_ += 1;
            assignment_->Store();
            neighbor_found_ = true;
            return nullptr;
          }
        }
      } else {
        if (neighbor_found_) {
          AcceptNeighbor(solver->ParentSearch());
          // Keeping the code in case a performance problem forces us to
          // use the old code with a zero test on pool_.
          //          reference_assignment_->Copy(assignment_);
          pool_->RegisterNewSolution(assignment_);
          SynchronizeAll(solver);
        } else {
          break;
        }
      }
    }
  }
  solver->Fail();
  return nullptr;
}

bool FindOneNeighbor::FilterAccept(Solver* solver, const Assignment* delta,
                                   const Assignment* deltadelta) {
  bool ok = true;
  LocalSearchMonitor* const monitor = solver->GetLocalSearchMonitor();
  for (int i = 0; i < filters_.size(); ++i) {
    if (ok || filters_[i]->IsIncremental()) {
      monitor->BeginFiltering(filters_[i]);
      const bool accept = filters_[i]->Accept(delta, deltadelta);
      monitor->EndFiltering(filters_[i], !accept);
      ok = accept && ok;
    }
  }
  return ok;
}

void FindOneNeighbor::SynchronizeAll(Solver* solver) {
  pool_->GetNextSolution(reference_assignment_.get());
  neighbor_found_ = false;
  limit_->Init();
  solver->GetLocalSearchMonitor()->BeginOperatorStart();
  ls_operator_->Start(reference_assignment_.get());
  SynchronizeFilters(reference_assignment_.get());
  solver->GetLocalSearchMonitor()->EndOperatorStart();
}

void FindOneNeighbor::SynchronizeFilters(const Assignment* assignment) {
  for (int i = 0; i < filters_.size(); ++i) {
    filters_[i]->Synchronize(assignment, nullptr);
  }
}

// ---------- Local Search Phase Parameters ----------

class LocalSearchPhaseParameters : public BaseObject {
 public:
  LocalSearchPhaseParameters(SolutionPool* const pool,
                             LocalSearchOperator* ls_operator,
                             DecisionBuilder* sub_decision_builder,
                             SearchLimit* const limit,
                             const std::vector<LocalSearchFilter*>& filters)
      : solution_pool_(pool),
        ls_operator_(ls_operator),
        sub_decision_builder_(sub_decision_builder),
        limit_(limit),
        filters_(filters) {}
  ~LocalSearchPhaseParameters() override {}
  std::string DebugString() const override { return "LocalSearchPhaseParameters"; }

  SolutionPool* solution_pool() const { return solution_pool_; }
  LocalSearchOperator* ls_operator() const { return ls_operator_; }
  DecisionBuilder* sub_decision_builder() const {
    return sub_decision_builder_;
  }
  SearchLimit* limit() const { return limit_; }
  const std::vector<LocalSearchFilter*>& filters() const { return filters_; }

 private:
  SolutionPool* const solution_pool_;
  LocalSearchOperator* const ls_operator_;
  DecisionBuilder* const sub_decision_builder_;
  SearchLimit* const limit_;
  std::vector<LocalSearchFilter*> filters_;
};

LocalSearchPhaseParameters* Solver::MakeLocalSearchPhaseParameters(
    LocalSearchOperator* const ls_operator,
    DecisionBuilder* const sub_decision_builder) {
  return MakeLocalSearchPhaseParameters(MakeDefaultSolutionPool(), ls_operator,
                                        sub_decision_builder, nullptr,
                                        std::vector<LocalSearchFilter*>());
}

LocalSearchPhaseParameters* Solver::MakeLocalSearchPhaseParameters(
    LocalSearchOperator* const ls_operator,
    DecisionBuilder* const sub_decision_builder, SearchLimit* const limit) {
  return MakeLocalSearchPhaseParameters(MakeDefaultSolutionPool(), ls_operator,
                                        sub_decision_builder, limit,
                                        std::vector<LocalSearchFilter*>());
}

LocalSearchPhaseParameters* Solver::MakeLocalSearchPhaseParameters(
    LocalSearchOperator* const ls_operator,
    DecisionBuilder* const sub_decision_builder, SearchLimit* const limit,
    const std::vector<LocalSearchFilter*>& filters) {
  return MakeLocalSearchPhaseParameters(MakeDefaultSolutionPool(), ls_operator,
                                        sub_decision_builder, limit, filters);
}

LocalSearchPhaseParameters* Solver::MakeLocalSearchPhaseParameters(
    SolutionPool* const pool, LocalSearchOperator* const ls_operator,
    DecisionBuilder* const sub_decision_builder) {
  return MakeLocalSearchPhaseParameters(pool, ls_operator, sub_decision_builder,
                                        nullptr,
                                        std::vector<LocalSearchFilter*>());
}

LocalSearchPhaseParameters* Solver::MakeLocalSearchPhaseParameters(
    SolutionPool* const pool, LocalSearchOperator* const ls_operator,
    DecisionBuilder* const sub_decision_builder, SearchLimit* const limit) {
  return MakeLocalSearchPhaseParameters(pool, ls_operator, sub_decision_builder,
                                        limit,
                                        std::vector<LocalSearchFilter*>());
}

LocalSearchPhaseParameters* Solver::MakeLocalSearchPhaseParameters(
    SolutionPool* const pool, LocalSearchOperator* const ls_operator,
    DecisionBuilder* const sub_decision_builder, SearchLimit* const limit,
    const std::vector<LocalSearchFilter*>& filters) {
  return RevAlloc(new LocalSearchPhaseParameters(
      pool, ls_operator, sub_decision_builder, limit, filters));
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

  NestedSolveDecision(DecisionBuilder* const db, bool restore,
                      const std::vector<SearchMonitor*>& monitors);
  NestedSolveDecision(DecisionBuilder* const db, bool restore);
  ~NestedSolveDecision() override {}
  void Apply(Solver* const solver) override;
  void Refute(Solver* const solver) override;
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

void NestedSolveDecision::Refute(Solver* const solver) {}

// ----- Local search decision builder -----

// Given a first solution (resulting from either an initial assignment or the
// result of a decision builder), it searches for neighbors using a local
// search operator. The first solution corresponds to the first leaf of the
// search.
// The local search applies to the variables contained either in the assignment
// or the vector of variables passed.

class LocalSearch : public DecisionBuilder {
 public:
  LocalSearch(Assignment* const assignment, SolutionPool* const pool,
              LocalSearchOperator* const ls_operator,
              DecisionBuilder* const sub_decision_builder,
              SearchLimit* const limit,
              const std::vector<LocalSearchFilter*>& filters);
  // TODO(user): find a way to not have to pass vars here: redundant with
  // variables in operators
  LocalSearch(const std::vector<IntVar*>& vars, SolutionPool* const pool,
              DecisionBuilder* const first_solution,
              LocalSearchOperator* const ls_operator,
              DecisionBuilder* const sub_decision_builder,
              SearchLimit* const limit,
              const std::vector<LocalSearchFilter*>& filters);
  LocalSearch(const std::vector<SequenceVar*>& vars, SolutionPool* const pool,
              DecisionBuilder* const first_solution,
              LocalSearchOperator* const ls_operator,
              DecisionBuilder* const sub_decision_builder,
              SearchLimit* const limit,
              const std::vector<LocalSearchFilter*>& filters);
  ~LocalSearch() override;
  Decision* Next(Solver* const solver) override;
  std::string DebugString() const override { return "LocalSearch"; }
  void Accept(ModelVisitor* const visitor) const override;

 protected:
  void PushFirstSolutionDecision(DecisionBuilder* first_solution);
  void PushLocalSearchDecision();

 private:
  Assignment* assignment_;
  SolutionPool* const pool_;
  LocalSearchOperator* const ls_operator_;
  DecisionBuilder* const sub_decision_builder_;
  std::vector<NestedSolveDecision*> nested_decisions_;
  int nested_decision_index_;
  SearchLimit* const limit_;
  const std::vector<LocalSearchFilter*> filters_;
  bool has_started_;
};

LocalSearch::LocalSearch(Assignment* const assignment, SolutionPool* const pool,
                         LocalSearchOperator* const ls_operator,
                         DecisionBuilder* const sub_decision_builder,
                         SearchLimit* const limit,
                         const std::vector<LocalSearchFilter*>& filters)
    : assignment_(assignment),
      pool_(pool),
      ls_operator_(ls_operator),
      sub_decision_builder_(sub_decision_builder),
      nested_decision_index_(0),
      limit_(limit),
      filters_(filters),
      has_started_(false) {
  CHECK(nullptr != assignment);
  CHECK(nullptr != ls_operator);
  Solver* const solver = assignment_->solver();
  DecisionBuilder* restore = solver->MakeRestoreAssignment(assignment_);
  PushFirstSolutionDecision(restore);
  PushLocalSearchDecision();
}

LocalSearch::LocalSearch(const std::vector<IntVar*>& vars,
                         SolutionPool* const pool,
                         DecisionBuilder* const first_solution,
                         LocalSearchOperator* const ls_operator,
                         DecisionBuilder* const sub_decision_builder,
                         SearchLimit* const limit,
                         const std::vector<LocalSearchFilter*>& filters)
    : assignment_(nullptr),
      pool_(pool),
      ls_operator_(ls_operator),
      sub_decision_builder_(sub_decision_builder),
      nested_decision_index_(0),
      limit_(limit),
      filters_(filters),
      has_started_(false) {
  CHECK(nullptr != first_solution);
  CHECK(nullptr != ls_operator);
  CHECK(!vars.empty());
  Solver* const solver = vars[0]->solver();
  assignment_ = solver->MakeAssignment();
  assignment_->Add(vars);
  PushFirstSolutionDecision(first_solution);
  PushLocalSearchDecision();
}

LocalSearch::LocalSearch(const std::vector<SequenceVar*>& vars,
                         SolutionPool* const pool,
                         DecisionBuilder* const first_solution,
                         LocalSearchOperator* const ls_operator,
                         DecisionBuilder* const sub_decision_builder,
                         SearchLimit* const limit,
                         const std::vector<LocalSearchFilter*>& filters)
    : assignment_(nullptr),
      pool_(pool),
      ls_operator_(ls_operator),
      sub_decision_builder_(sub_decision_builder),
      nested_decision_index_(0),
      limit_(limit),
      filters_(filters),
      has_started_(false) {
  CHECK(nullptr != first_solution);
  CHECK(nullptr != ls_operator);
  CHECK(!vars.empty());
  Solver* const solver = vars[0]->solver();
  assignment_ = solver->MakeAssignment();
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
  } else if (nested_decision_index_ < 0) {
    solver->Fail();
  }
  NestedSolveDecision* decision = nested_decisions_[nested_decision_index_];
  const int state = decision->state();
  switch (state) {
    case NestedSolveDecision::DECISION_FAILED: {
      if (!LocalOptimumReached(solver->ActiveSearch())) {
        nested_decision_index_ = -1;  // Stop the search
      }
      solver->Fail();
      return nullptr;
    }
    case NestedSolveDecision::DECISION_PENDING: {
      // TODO(user): Find a way to make this balancing invisible to the
      // user (no increase in branch or fail counts for instance).
      const int32 kLocalSearchBalancedTreeDepth = 32;
      const int depth = solver->SearchDepth();
      if (depth < kLocalSearchBalancedTreeDepth) {
        return solver->balancing_decision();
      } else if (depth > kLocalSearchBalancedTreeDepth) {
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
  DecisionBuilder* first_solution_and_store =
      solver->Compose(first_solution, sub_decision_builder_, store);
  std::vector<SearchMonitor*> monitor;
  monitor.push_back(limit_);
  nested_decisions_.push_back(solver->RevAlloc(
      new NestedSolveDecision(first_solution_and_store, false, monitor)));
}

void LocalSearch::PushLocalSearchDecision() {
  Solver* const solver = assignment_->solver();
  DecisionBuilder* find_neighbors = solver->RevAlloc(
      new FindOneNeighbor(assignment_, pool_, ls_operator_,
                          sub_decision_builder_, limit_, filters_));
  nested_decisions_.push_back(
      solver->RevAlloc(new NestedSolveDecision(find_neighbors, false)));
}

class DefaultSolutionPool : public SolutionPool {
 public:
  DefaultSolutionPool() {}

  ~DefaultSolutionPool() override {}

  void Initialize(Assignment* const assignment) override {
    reference_assignment_.reset(new Assignment(assignment));
  }

  void RegisterNewSolution(Assignment* const assignment) override {
    reference_assignment_->Copy(assignment);
  }

  void GetNextSolution(Assignment* const assignment) override {
    assignment->Copy(reference_assignment_.get());
  }

  bool SyncNeeded(Assignment* const local_assignment) override { return false; }

  std::string DebugString() const override { return "DefaultSolutionPool"; }

 private:
  std::unique_ptr<Assignment> reference_assignment_;
};
}  // namespace

SolutionPool* Solver::MakeDefaultSolutionPool() {
  return RevAlloc(new DefaultSolutionPool());
}

DecisionBuilder* Solver::MakeLocalSearchPhase(
    Assignment* assignment, LocalSearchPhaseParameters* parameters) {
  return RevAlloc(new LocalSearch(assignment, parameters->solution_pool(),
                                  parameters->ls_operator(),
                                  parameters->sub_decision_builder(),
                                  parameters->limit(), parameters->filters()));
}

DecisionBuilder* Solver::MakeLocalSearchPhase(
    const std::vector<IntVar*>& vars, DecisionBuilder* first_solution,
    LocalSearchPhaseParameters* parameters) {
  return RevAlloc(new LocalSearch(vars, parameters->solution_pool(),
                                  first_solution, parameters->ls_operator(),
                                  parameters->sub_decision_builder(),
                                  parameters->limit(), parameters->filters()));
}

DecisionBuilder* Solver::MakeLocalSearchPhase(
    const std::vector<SequenceVar*>& vars, DecisionBuilder* first_solution,
    LocalSearchPhaseParameters* parameters) {
  return RevAlloc(new LocalSearch(vars, parameters->solution_pool(),
                                  first_solution, parameters->ls_operator(),
                                  parameters->sub_decision_builder(),
                                  parameters->limit(), parameters->filters()));
}
}  // namespace operations_research

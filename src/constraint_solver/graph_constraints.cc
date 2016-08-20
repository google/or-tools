// Copyright 2010-2014 Google
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
#include <memory>
#include <string>
#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/saturated_arithmetic.h"
#include "util/string_array.h"

namespace operations_research {
// ---------- No cycle ----------

// This constraint ensures there are no cycles in the variable/value graph.
// "Sink" values are values outside the range of the array of variables; they
// are used to end paths.
// The constraint does essentially two things:
// - forbid partial paths from looping back to themselves
// - ensure each variable/node can be connected to a "sink".
// If assume_paths is true, the constraint assumes the 'next' variables
// represent paths (and performs a faster propagation); otherwise the
// constraint assumes the 'next' variables represent a forest.
// TODO(user): improve code when assume_paths is false (currently does an
// expensive n^2 loop).

namespace {
class NoCycle : public Constraint {
 public:
  NoCycle(Solver* const s, const std::vector<IntVar*>& nexts,
          const std::vector<IntVar*>& active, Solver::IndexFilter1 sink_handler,
          bool assume_paths);
  ~NoCycle() override {}
  void Post() override;
  void InitialPropagate() override;
  void NextChange(int index);
  void ActiveBound(int index);
  void NextBound(int index);
  void ComputeSupports();
  void ComputeSupport(int index);
  std::string DebugString() const override;

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kNoCycle, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kNextsArgument,
                                               nexts_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kActiveArgument,
                                               active_);
    visitor->VisitIntegerArgument("assume_paths", assume_paths_);
    visitor->VisitInt64ToBoolExtension(sink_handler_, -size(), size());
    visitor->EndVisitConstraint(ModelVisitor::kNoCycle, this);
  }

 private:
  int64 size() const { return nexts_.size(); }

  const std::vector<IntVar*> nexts_;
  const std::vector<IntVar*> active_;
  std::vector<IntVarIterator*> iterators_;
  std::vector<int64> starts_;
  std::vector<int64> ends_;
  bool all_nexts_bound_;
  std::vector<int64> outbound_supports_;
  std::vector<int64> support_leaves_;
  std::vector<int64> unsupported_;
  Solver::IndexFilter1 sink_handler_;
  std::vector<int64> sinks_;
  bool assume_paths_;
};

NoCycle::NoCycle(Solver* const s, const std::vector<IntVar*>& nexts,
                 const std::vector<IntVar*>& active,
                 Solver::IndexFilter1 sink_handler, bool assume_paths)
    : Constraint(s),
      nexts_(nexts),
      active_(active),
      iterators_(nexts.size(), nullptr),
      starts_(nexts.size()),
      ends_(nexts.size()),
      all_nexts_bound_(false),
      outbound_supports_(nexts.size(), -1),
      sink_handler_(sink_handler),
      assume_paths_(assume_paths) {
  support_leaves_.reserve(size());
  unsupported_.reserve(size());
  for (int i = 0; i < size(); ++i) {
    starts_[i] = i;
    ends_[i] = i;
    iterators_[i] = nexts_[i]->MakeDomainIterator(true);
  }
}

void NoCycle::InitialPropagate() {
  // Reduce next domains to sinks + range of nexts
  for (int i = 0; i < size(); ++i) {
    outbound_supports_[i] = -1;
    IntVar* next = nexts_[i];
    for (int j = next->Min(); j < 0; ++j) {
      if (!sink_handler_(j)) {
        next->RemoveValue(j);
      }
    }
    for (int j = next->Max(); j >= size(); --j) {
      if (!sink_handler_(j)) {
        next->RemoveValue(j);
      }
    }
  }
  solver()->SaveAndSetValue(&all_nexts_bound_, true);
  for (int i = 0; i < size(); ++i) {
    if (nexts_[i]->Bound()) {
      NextBound(i);
    } else {
      solver()->SaveAndSetValue(&all_nexts_bound_, false);
    }
  }
  ComputeSupports();
}

void NoCycle::Post() {
  if (size() == 0) return;
  for (int i = 0; i < size(); ++i) {
    IntVar* next = nexts_[i];
    Demon* support_demon = MakeConstraintDemon1(
        solver(), this, &NoCycle::NextChange, "NextChange", i);
    next->WhenDomain(support_demon);
    Demon* active_demon = MakeConstraintDemon1(
        solver(), this, &NoCycle::ActiveBound, "ActiveBound", i);
    active_[i]->WhenBound(active_demon);
  }
  // Setting up sinks
  int64 min_min = nexts_[0]->Min();
  int64 max_max = nexts_[0]->Max();
  for (int i = 1; i < size(); ++i) {
    const IntVar* next = nexts_[i];
    min_min = std::min(min_min, next->Min());
    max_max = std::max(max_max, next->Max());
  }
  sinks_.clear();
  for (int i = min_min; i <= max_max; ++i) {
    if (sink_handler_(i)) {
      sinks_.push_back(i);
    }
  }
}

void NoCycle::NextChange(int index) {
  IntVar* const next_var = nexts_[index];
  if (next_var->Bound()) {
    NextBound(index);
  }
  if (!all_nexts_bound_) {
    bool all_nexts_bound = true;
    for (int i = 0; i < size(); ++i) {
      if (!nexts_[i]->Bound()) {
        all_nexts_bound = false;
        break;
      }
    }
    solver()->SaveAndSetValue(&all_nexts_bound_, all_nexts_bound);
  }
  if (all_nexts_bound_) {
    return;
  }
  if (!next_var->Contains(outbound_supports_[index])) {
    ComputeSupport(index);
  }
}

void NoCycle::ActiveBound(int index) {
  if (nexts_[index]->Bound()) {
    NextBound(index);
  }
}

void NoCycle::NextBound(int index) {
  if (active_[index]->Min() == 0) return;
  const int64 next = nexts_[index]->Value();
  const int64 chain_start = starts_[index];
  const int64 chain_end = !sink_handler_(next) ? ends_[next] : next;
  Solver* const s = solver();
  if (!sink_handler_(chain_start)) {
    s->SaveAndSetValue(&ends_[chain_start], chain_end);
    if (!sink_handler_(chain_end)) {
      s->SaveAndSetValue(&starts_[chain_end], chain_start);
      nexts_[chain_end]->RemoveValue(chain_start);
      if (!assume_paths_) {
        for (int i = 0; i < size(); ++i) {
          int64 current = i;
          bool found = (current == chain_end);
          // Counter to detect implicit cycles.
          int count = 0;
          while (!found && count < size() && !sink_handler_(current) &&
                 nexts_[current]->Bound()) {
            current = nexts_[current]->Value();
            found = (current == chain_end);
            ++count;
          }
          if (found) {
            nexts_[chain_end]->RemoveValue(i);
          }
        }
      }
    }
  }
}

// Compute the support tree. For each variable, find a path connecting to a
// sink. Starts partial paths from the sinks down to all unconnected variables.
// If some variables remain unconnected, make the corresponding active_
// variable false.
// Resulting tree is used as supports for next variables.
// TODO(user): Try to see if we can find an algorithm which is less than
// quadratic to do this (note that if the tree is flat we are already linear
// for a given number of sinks).
void NoCycle::ComputeSupports() {
  // unsupported_ contains nodes not connected to sinks.
  unsupported_.clear();
  // supported_leaves_ contains the current frontier containing nodes surely
  // connected to sinks.
  support_leaves_.clear();
  // Initial phase: find direct connections to sinks and initialize
  // support_leaves_ and unsupported_ accordingly.
  const int sink_size = sinks_.size();
  for (int i = 0; i < size(); ++i) {
    const IntVar* next = nexts_[i];
    // If node is not active, no need to try to connect it to a sink.
    if (active_[i]->Max() != 0) {
      const int64 current_support = outbound_supports_[i];
      // Optimization: if this node was already supported by a sink, check if
      // it's still a valid support.
      if (current_support >= 0 && sink_handler_(current_support) &&
          next->Contains(current_support)) {
        support_leaves_.push_back(i);
      } else {
        // Optimization: iterate on sinks or next domain depending on which is
        // smaller.
        outbound_supports_[i] = -1;
        if (sink_size < next->Size()) {
          for (int j = 0; j < sink_size; ++j) {
            if (next->Contains(sinks_[j])) {
              outbound_supports_[i] = sinks_[j];
              support_leaves_.push_back(i);
              break;
            }
          }
        } else {
          for (const int64 value : InitAndGetValues(iterators_[i])) {
            if (sink_handler_(value)) {
              outbound_supports_[i] = value;
              support_leaves_.push_back(i);
              break;
            }
          }
        }
      }
      if (outbound_supports_[i] == -1) {
        unsupported_.push_back(i);
      }
    }
  }
  // No need to iterate on all nodes connected to sinks but just on the ones
  // added in the last iteration; leaves_begin and leaves_end mark the block
  // in support_leaves_ corresponding to such nodes.
  size_t leaves_begin = 0;
  size_t leaves_end = support_leaves_.size();
  while (!unsupported_.empty()) {
    // Try to connected unsupported nodes to nodes connected to sinks.
    for (int64 unsupported_index = 0; unsupported_index < unsupported_.size();
         ++unsupported_index) {
      const int64 unsupported = unsupported_[unsupported_index];
      const IntVar* const next = nexts_[unsupported];
      for (int i = leaves_begin; i < leaves_end; ++i) {
        if (next->Contains(support_leaves_[i])) {
          outbound_supports_[unsupported] = support_leaves_[i];
          support_leaves_.push_back(unsupported);
          // Remove current node from unsupported vector.
          unsupported_[unsupported_index] = unsupported_.back();
          unsupported_.pop_back();
          --unsupported_index;
          break;
        }
        // TODO(user): evaluate same trick as with the sinks by keeping a
        // bitmap with supported nodes.
      }
    }
    // No new leaves were added, we can bail out.
    if (leaves_end == support_leaves_.size()) {
      break;
    }
    leaves_begin = leaves_end;
    leaves_end = support_leaves_.size();
  }
  // Mark as inactive any unsupported node.
  for (int64 unsupported_index = 0; unsupported_index < unsupported_.size();
       ++unsupported_index) {
    active_[unsupported_[unsupported_index]]->SetMax(0);
  }
}

void NoCycle::ComputeSupport(int index) {
  // Try to reconnect the node to the support tree by finding a next node
  // which is both supported and was not a descendant of the node in the tree.
  if (active_[index]->Max() != 0) {
    for (const int64 next : InitAndGetValues(iterators_[index])) {
      if (sink_handler_(next)) {
        outbound_supports_[index] = next;
        return;
      }
      if (next != index && next < outbound_supports_.size()) {
        int64 next_support = outbound_supports_[next];
        if (next_support >= 0) {
          // Check if next is not already a descendant of index.
          bool ancestor_found = false;
          while (next_support < outbound_supports_.size() &&
                 !sink_handler_(next_support)) {
            if (next_support == index) {
              ancestor_found = true;
              break;
            }
            next_support = outbound_supports_[next_support];
          }
          if (!ancestor_found) {
            outbound_supports_[index] = next;
            return;
          }
        }
      }
    }
  }
  // No support was found, rebuild the support tree.
  ComputeSupports();
}

std::string NoCycle::DebugString() const {
  return StringPrintf("NoCycle(%s)", JoinDebugStringPtr(nexts_, ", ").c_str());
}

// ----- Circuit constraint -----

class Circuit : public Constraint {
 public:
  Circuit(Solver* const s, const std::vector<IntVar*>& nexts, bool sub_circuit)
      : Constraint(s),
        nexts_(nexts),
        size_(nexts_.size()),
        starts_(size_, -1),
        ends_(size_, -1),
        lengths_(size_, 1),
        domains_(size_),
        outbound_support_(size_, -1),
        inbound_support_(size_, -1),
        temp_support_(size_, -1),
        inbound_demon_(nullptr),
        outbound_demon_(nullptr),
        root_(-1),
        num_inactives_(0),
        sub_circuit_(sub_circuit) {
    for (int i = 0; i < size_; ++i) {
      domains_[i] = nexts_[i]->MakeDomainIterator(true);
    }
  }

  ~Circuit() override {}

  void Post() override {
    inbound_demon_ = MakeDelayedConstraintDemon0(
        solver(), this, &Circuit::CheckReachabilityToRoot,
        "CheckReachabilityToRoot");
    outbound_demon_ = MakeDelayedConstraintDemon0(
        solver(), this, &Circuit::CheckReachabilityFromRoot,
        "CheckReachabilityFromRoot");
    for (int i = 0; i < size_; ++i) {
      if (!nexts_[i]->Bound()) {
        Demon* const bound_demon = MakeConstraintDemon1(
            solver(), this, &Circuit::NextBound, "NextBound", i);
        nexts_[i]->WhenBound(bound_demon);
        Demon* const domain_demon = MakeConstraintDemon1(
            solver(), this, &Circuit::NextDomain, "NextDomain", i);
        nexts_[i]->WhenDomain(domain_demon);
      }
    }
    solver()->AddConstraint(solver()->MakeAllDifferent(nexts_));
  }

  void InitialPropagate() override {
    Solver* const s = solver();
    if (!sub_circuit_) {
      root_.SetValue(solver(), 0);
    }
    for (int i = 0; i < size_; ++i) {
      nexts_[i]->SetRange(0, size_ - 1);
      if (!sub_circuit_) {
        nexts_[i]->RemoveValue(i);
      }
    }
    for (int i = 0; i < size_; ++i) {
      starts_.SetValue(s, i, i);
      ends_.SetValue(s, i, i);
      lengths_.SetValue(s, i, 1);
    }
    for (int i = 0; i < size_; ++i) {
      if (nexts_[i]->Bound()) {
        NextBound(i);
      }
    }
    CheckReachabilityFromRoot();
    CheckReachabilityToRoot();
  }

  std::string DebugString() const override {
    return StringPrintf("%sCircuit(%s)", sub_circuit_ ? "Sub" : "",
                        JoinDebugStringPtr(nexts_, " ").c_str());
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kCircuit, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kNextsArgument,
                                               nexts_);
    visitor->VisitIntegerArgument(ModelVisitor::kPartialArgument, sub_circuit_);
    visitor->EndVisitConstraint(ModelVisitor::kCircuit, this);
  }

 private:
  bool Inactive(int index) const {
    return nexts_[index]->Bound() && nexts_[index]->Min() == index;
  }

  void NextBound(int index) {
    Solver* const s = solver();
    const int destination = nexts_[index]->Value();
    const int root = root_.Value();
    if (destination != index) {
      if (root == -1) {
        root_.SetValue(s, index);
      }
      const int new_end = ends_.Value(destination);
      const int new_start = starts_.Value(index);
      starts_.SetValue(s, new_end, new_start);
      ends_.SetValue(s, new_start, new_end);
      lengths_.SetValue(s, new_start, lengths_.Value(new_start) +
                                          lengths_.Value(destination));
      if (sub_circuit_) {
        // You are creating the only path. Nexts can no longer loop upon itself.
        nexts_[destination]->RemoveValue(destination);
      } else {
        if (lengths_.Value(new_start) < size_ - 1 - num_inactives_.Value()) {
          nexts_[new_end]->RemoveValue(new_start);
        }
      }
    } else {
      num_inactives_.Incr(solver());
    }
    // TODO(user): You might get more propagation if you maintain
    //     num_undecided_actives_;
    // then
    // if (num_undecided_actives_ == 0 &&
    //     lengths_.Value(new_start) < size_ - 1 - num_inactives_.Value()) {
    //   nexts_[new_end]->RemoveValue(new_start);
    // }
    // for both complete == true and false.
  }

  void NextDomain(int index) {
    if (root_.Value() == -1) {
      return;
    }
    if (!nexts_[index]->Contains(outbound_support_[index])) {
      EnqueueDelayedDemon(outbound_demon_);
    }
    if (!nexts_[index]->Contains(inbound_support_[index])) {
      EnqueueDelayedDemon(inbound_demon_);
    }
  }

  void TryInsertReached(int candidate, int64 after) {
    if (!reached_[after]) {
      reached_[after] = true;
      insertion_queue_.push_back(after);
      temp_support_[candidate] = after;
    }
  }

  void CheckReachabilityFromRoot() {
    if (root_.Value() == -1) {  // Root is not yet defined. Nothing to deduce.
      return;
    }

    // Assign temp_support_ to a dummy value.
    temp_support_.assign(size_, -1);
    // Clear the spanning tree.
    int processed = 0;
    reached_.assign(size_, false);
    insertion_queue_.clear();
    // Add the root node.
    const int root_value = root_.Value();
    reached_[root_value] = true;
    insertion_queue_.push_back(root_value);
    // Compute reachable nodes.
    while (processed < insertion_queue_.size() &&
           insertion_queue_.size() + num_inactives_.Value() < size_) {
      const int candidate = insertion_queue_[processed++];
      IntVar* const var = nexts_[candidate];
      switch (var->Size()) {
        case 1: {
          TryInsertReached(candidate, var->Min());
          break;
        }
        case 2: {
          TryInsertReached(candidate, var->Min());
          TryInsertReached(candidate, var->Max());
          break;
        }
        default: {
          IntVarIterator* const domain = domains_[candidate];
          for (const int64 value : InitAndGetValues(domain)) {
            TryInsertReached(candidate, value);
          }
        }
      }
    }
    // All non reachable nodes should point to themselves in the incomplete
    // case
    for (int i = 0; i < size_; ++i) {
      if (!reached_[i]) {
        nexts_[i]->SetValue(i);
      }
    }
    // Update the outbound_support_ vector.
    outbound_support_.swap(temp_support_);
  }

  void CheckReachabilityToRoot() {
    // TODO(user): Improve with prev_ data structure.
    if (root_.Value() == -1) {
      return;
    }

    insertion_queue_.clear();
    insertion_queue_.push_back(root_.Value());
    temp_support_[root_.Value()] = nexts_[root_.Value()]->Min();
    int processed = 0;
    to_visit_.clear();
    for (int i = 0; i < size_; ++i) {
      if (!Inactive(i) && i != root_.Value()) {
        to_visit_.push_back(i);
      }
    }
    const int inactive = num_inactives_.Value();
    while (processed < insertion_queue_.size() &&
           insertion_queue_.size() + inactive < size_) {
      const int inserted = insertion_queue_[processed++];
      std::vector<int> rejected;
      for (int index = 0; index < to_visit_.size(); ++index) {
        const int candidate = to_visit_[index];
        if (nexts_[candidate]->Contains(inserted)) {
          insertion_queue_.push_back(candidate);
          temp_support_[candidate] = inserted;
        } else {
          rejected.push_back(candidate);
        }
      }
      to_visit_.swap(rejected);
    }
    for (int i = 0; i < to_visit_.size(); ++i) {
      const int node = to_visit_[i];
      nexts_[node]->SetValue(node);
    }
    temp_support_.swap(inbound_support_);
  }

  const std::vector<IntVar*> nexts_;
  const int size_;
  std::vector<int> insertion_queue_;
  std::vector<int> to_visit_;
  std::vector<bool> reached_;
  RevArray<int> starts_;
  RevArray<int> ends_;
  RevArray<int> lengths_;
  std::vector<IntVarIterator*> domains_;
  std::vector<int> outbound_support_;
  std::vector<int> inbound_support_;
  std::vector<int> temp_support_;
  Demon* inbound_demon_;
  Demon* outbound_demon_;
  Rev<int> root_;
  NumericalRev<int> num_inactives_;
  const bool sub_circuit_;
};
}  // namespace

Constraint* Solver::MakeNoCycle(const std::vector<IntVar*>& nexts,
                                const std::vector<IntVar*>& active,
                                Solver::IndexFilter1 sink_handler,
                                bool assume_paths) {
  CHECK_EQ(nexts.size(), active.size());
  if (sink_handler == nullptr) {
    const int64 size = nexts.size();
    sink_handler = [size](int64 index) { return index >= size; };
  }
  return RevAlloc(new NoCycle(this, nexts, active, sink_handler, assume_paths));
}

Constraint* Solver::MakeNoCycle(const std::vector<IntVar*>& nexts,
                                const std::vector<IntVar*>& active,
                                Solver::IndexFilter1 sink_handler) {
  return MakeNoCycle(nexts, active, sink_handler, true);
}

// TODO(user): Merge NoCycle and Circuit.
Constraint* Solver::MakeCircuit(const std::vector<IntVar*>& nexts) {
  return RevAlloc(new Circuit(this, nexts, false));
}

Constraint* Solver::MakeSubCircuit(const std::vector<IntVar*>& nexts) {
  return RevAlloc(new Circuit(this, nexts, true));
}

// ----- Path cumul constraints -----

namespace {
class BasePathCumul : public Constraint {
 public:
  BasePathCumul(Solver* const s, const std::vector<IntVar*>& nexts,
                const std::vector<IntVar*>& active, const std::vector<IntVar*>& cumuls);
  ~BasePathCumul() override {}
  void Post() override;
  void InitialPropagate() override;
  void ActiveBound(int index);
  virtual void NextBound(int index) = 0;
  virtual bool AcceptLink(int i, int j) const = 0;
  void UpdateSupport(int index);
  void CumulRange(int index);
  std::string DebugString() const override;

 protected:
  int64 size() const { return nexts_.size(); }
  int cumul_size() const { return cumuls_.size(); }

  const std::vector<IntVar*> nexts_;
  const std::vector<IntVar*> active_;
  const std::vector<IntVar*> cumuls_;
  RevArray<int> prevs_;
  std::vector<int> supports_;
};

BasePathCumul::BasePathCumul(Solver* const s, const std::vector<IntVar*>& nexts,
                             const std::vector<IntVar*>& active,
                             const std::vector<IntVar*>& cumuls)
    : Constraint(s),
      nexts_(nexts),
      active_(active),
      cumuls_(cumuls),
      prevs_(cumuls.size(), -1),
      supports_(nexts.size()) {
  CHECK_GE(cumul_size(), size());
  for (int i = 0; i < size(); ++i) {
    supports_[i] = -1;
  }
}

void BasePathCumul::InitialPropagate() {
  for (int i = 0; i < size(); ++i) {
    if (nexts_[i]->Bound()) {
      NextBound(i);
    } else {
      UpdateSupport(i);
    }
  }
}

void BasePathCumul::Post() {
  for (int i = 0; i < size(); ++i) {
    IntVar* var = nexts_[i];
    Demon* d = MakeConstraintDemon1(solver(), this, &BasePathCumul::NextBound,
                                    "NextBound", i);
    var->WhenBound(d);
    Demon* ds = MakeConstraintDemon1(
        solver(), this, &BasePathCumul::UpdateSupport, "UpdateSupport", i);
    var->WhenDomain(ds);
    Demon* active_demon = MakeConstraintDemon1(
        solver(), this, &BasePathCumul::ActiveBound, "ActiveBound", i);
    active_[i]->WhenBound(active_demon);
  }
  for (int i = 0; i < cumul_size(); ++i) {
    IntVar* cumul = cumuls_[i];
    Demon* d = MakeConstraintDemon1(solver(), this, &BasePathCumul::CumulRange,
                                    "CumulRange", i);
    cumul->WhenRange(d);
  }
}

void BasePathCumul::ActiveBound(int index) {
  if (nexts_[index]->Bound()) {
    NextBound(index);
  }
}

void BasePathCumul::CumulRange(int index) {
  if (index < size()) {
    if (nexts_[index]->Bound()) {
      NextBound(index);
    } else {
      UpdateSupport(index);
    }
  }
  if (prevs_[index] >= 0) {
    NextBound(prevs_[index]);
  } else {
    for (int i = 0; i < size(); ++i) {
      if (index == supports_[i]) {
        UpdateSupport(i);
      }
    }
  }
}

void BasePathCumul::UpdateSupport(int index) {
  int support = supports_[index];
  if (support < 0 || !AcceptLink(index, support)) {
    IntVar* var = nexts_[index];
    for (int i = var->Min(); i <= var->Max(); ++i) {
      if (i != support && AcceptLink(index, i)) {
        supports_[index] = i;
        return;
      }
    }
    active_[index]->SetMax(0);
  }
}

std::string BasePathCumul::DebugString() const {
  std::string out = "PathCumul(";
  for (int i = 0; i < size(); ++i) {
    out += nexts_[i]->DebugString() + " " + cumuls_[i]->DebugString();
  }
  out += ")";
  return out;
}

// cumuls[next[i]] = cumuls[i] + transits[i]

class PathCumul : public BasePathCumul {
 public:
  PathCumul(Solver* const s, const std::vector<IntVar*>& nexts,
            const std::vector<IntVar*>& active, const std::vector<IntVar*>& cumuls,
            const std::vector<IntVar*>& transits)
      : BasePathCumul(s, nexts, active, cumuls), transits_(transits) {}
  ~PathCumul() override {}
  void Post() override;
  void NextBound(int index) override;
  bool AcceptLink(int i, int j) const override;
  void TransitRange(int index);

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kPathCumul, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kNextsArgument,
                                               nexts_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kActiveArgument,
                                               active_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kCumulsArgument,
                                               cumuls_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kTransitsArgument,
                                               transits_);
    visitor->EndVisitConstraint(ModelVisitor::kPathCumul, this);
  }

 private:
  const std::vector<IntVar*> transits_;
};

void PathCumul::Post() {
  BasePathCumul::Post();
  for (int i = 0; i < size(); ++i) {
    Demon* transit_demon = MakeConstraintDemon1(
        solver(), this, &PathCumul::TransitRange, "TransitRange", i);
    transits_[i]->WhenRange(transit_demon);
  }
}

void PathCumul::NextBound(int index) {
  if (active_[index]->Min() == 0) return;
  const int64 next = nexts_[index]->Value();
  IntVar* cumul = cumuls_[index];
  IntVar* cumul_next = cumuls_[next];
  IntVar* transit = transits_[index];
  cumul_next->SetMin(cumul->Min() + transit->Min());
  cumul_next->SetMax(CapAdd(cumul->Max(), transit->Max()));
  cumul->SetMin(CapSub(cumul_next->Min(), transit->Max()));
  cumul->SetMax(CapSub(cumul_next->Max(), transit->Min()));
  transit->SetMin(CapSub(cumul_next->Min(), cumul->Max()));
  transit->SetMax(CapSub(cumul_next->Max(), cumul->Min()));
  if (prevs_[next] < 0) {
    prevs_.SetValue(solver(), next, index);
  }
}

void PathCumul::TransitRange(int index) {
  if (nexts_[index]->Bound()) {
    NextBound(index);
  } else {
    UpdateSupport(index);
  }
  if (prevs_[index] >= 0) {
    NextBound(prevs_[index]);
  } else {
    for (int i = 0; i < size(); ++i) {
      if (index == supports_[i]) {
        UpdateSupport(i);
      }
    }
  }
}

bool PathCumul::AcceptLink(int i, int j) const {
  const IntVar* const cumul_i = cumuls_[i];
  const IntVar* const cumul_j = cumuls_[j];
  const IntVar* const transit_i = transits_[i];
  return transit_i->Min() <= CapSub(cumul_j->Max(), cumul_i->Min()) &&
         CapSub(cumul_j->Min(), cumul_i->Max()) <= transit_i->Max();
}

namespace {
template <class T>
class StampedVector {
 public:
  StampedVector() : stamp_(0) {}
  const std::vector<T>& Values(Solver* solver) {
    CheckStamp(solver);
    return values_;
  }
  void PushBack(Solver* solver, const T& value) {
    CheckStamp(solver);
    values_.push_back(value);
  }
  void Clear(Solver* solver) {
    values_.clear();
    stamp_ = solver->fail_stamp();
  }

 private:
  void CheckStamp(Solver* solver) {
    if (solver->fail_stamp() > stamp_) {
      Clear(solver);
    }
  }

  std::vector<T> values_;
  uint64 stamp_;
};
}  // namespace

class DelayedPathCumul : public Constraint {
 public:
  DelayedPathCumul(Solver* const solver, const std::vector<IntVar*>& nexts,
                   const std::vector<IntVar*>& active, const std::vector<IntVar*>& cumuls,
                   const std::vector<IntVar*>& transits)
      : Constraint(solver),
        nexts_(nexts),
        active_(active),
        cumuls_(cumuls),
        transits_(transits),
        cumul_transit_demons_(cumuls.size(), nullptr),
        path_demon_(nullptr),
        touched_(),
        chain_starts_(cumuls.size(), -1),
        chain_ends_(cumuls.size(), -1),
        is_chain_start_(cumuls.size(), false),
        prevs_(cumuls.size(), -1),
        supports_(nexts.size()),
        was_bound_(nexts.size(), false),
        has_cumul_demon_(cumuls.size(), false) {
    for (int64 i = 0; i < cumuls_.size(); ++i) {
      cumul_transit_demons_[i] = MakeDelayedConstraintDemon1(
          solver, this, &DelayedPathCumul::CumulRange, "CumulRange", i);
      chain_starts_[i] = i;
      chain_ends_[i] = i;
    }
    path_demon_ = MakeDelayedConstraintDemon0(
        solver, this, &DelayedPathCumul::PropagatePaths, "PropagatePaths");
    for (int i = 0; i < nexts_.size(); ++i) {
      supports_[i] = -1;
    }
  }
  ~DelayedPathCumul() override {}
  void Post() override {
    solver()->RegisterDemon(path_demon_);
    for (int i = 0; i < nexts_.size(); ++i) {
      if (!nexts_[i]->Bound()) {
        Demon* const demon = MakeConstraintDemon1(
            solver(), this, &DelayedPathCumul::NextBound, "NextBound", i);
        nexts_[i]->WhenBound(demon);
      }
    }
    for (int i = 0; i < active_.size(); ++i) {
      if (!active_[i]->Bound()) {
        Demon* const demon = MakeConstraintDemon1(
            solver(), this, &DelayedPathCumul::ActiveBound, "ActiveBound", i);
        active_[i]->WhenBound(demon);
      }
    }
  }
  void InitialPropagate() override {
    touched_.Clear(solver());
    for (int i = 0; i < nexts_.size(); ++i) {
      if (nexts_[i]->Bound()) {
        NextBound(i);
      }
    }
    for (int i = 0; i < active_.size(); ++i) {
      if (active_[i]->Bound()) {
        ActiveBound(i);
      }
    }
  }
  // TODO(user): Merge NextBound and ActiveBound to re-use the same demon
  // for next and active variables.
  void NextBound(int index) {
    if (active_[index]->Min() > 0) {
      const int next = nexts_[index]->Min();
      PropagateLink(index, next);
      touched_.PushBack(solver(), index);
      EnqueueDelayedDemon(path_demon_);
    }
  }
  void ActiveBound(int index) {
    if (nexts_[index]->Bound()) {
      NextBound(index);
    }
  }
  void PropagatePaths() {
    // Detecting new chains.
    const std::vector<int>& touched_values = touched_.Values(solver());
    for (const int touched : touched_values) {
      chain_starts_[touched] = touched;
      chain_ends_[touched] = touched;
      is_chain_start_[touched] = false;
      const int next = nexts_[touched]->Min();
      chain_starts_[next] = next;
      chain_ends_[next] = next;
      is_chain_start_[next] = false;
    }
    for (const int touched : touched_values) {
      if (touched >= nexts_.size()) continue;
      IntVar* const next_var = nexts_[touched];
      if (!was_bound_[touched] && next_var->Bound() &&
          active_[touched]->Min() > 0) {
        const int64 next = next_var->Min();
        was_bound_.SetValue(solver(), touched, true);
        chain_starts_[chain_ends_[next]] = chain_starts_[touched];
        chain_ends_[chain_starts_[touched]] = chain_ends_[next];
        is_chain_start_[next] = false;
        is_chain_start_[chain_starts_[touched]] = true;
      }
    }
    // Propagating new chains.
    for (const int touched : touched_values) {
      // Is touched the start of a chain ?
      if (is_chain_start_[touched]) {
        // Propagate min cumuls from chain_starts[touch] to chain_ends_[touch].
        int64 current = touched;
        int64 next = nexts_[current]->Min();
        while (current != chain_ends_[touched]) {
          prevs_.SetValue(solver(), next, current);
          PropagateLink(current, next);
          current = next;
          if (current != chain_ends_[touched]) {
            next = nexts_[current]->Min();
          }
        }
        // Propagate max cumuls from chain_ends_[i] to chain_starts_[i].
        int64 prev = prevs_[current];
        while (current != touched) {
          PropagateLink(prev, current);
          current = prev;
          if (current != touched) {
            prev = prevs_[current];
          }
        }
        // Now that the chain has been propagated in both directions, adding
        // demons for the corresponding cumul and transit variables for
        // future changes in their range.
        current = touched;
        while (current != chain_ends_[touched]) {
          if (!has_cumul_demon_[current]) {
            Demon* const demon = cumul_transit_demons_[current];
            cumuls_[current]->WhenRange(demon);
            transits_[current]->WhenRange(demon);
            has_cumul_demon_.SetValue(solver(), current, true);
          }
          current = nexts_[current]->Min();
        }
        if (!has_cumul_demon_[current]) {
          Demon* const demon = cumul_transit_demons_[current];
          cumuls_[current]->WhenRange(demon);
          if (current < transits_.size()) {
            transits_[current]->WhenRange(demon);
            UpdateSupport(current);
          }
          has_cumul_demon_.SetValue(solver(), current, true);
        }
      }
    }
    touched_.Clear(solver());
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kDelayedPathCumul, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kNextsArgument,
                                               nexts_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kActiveArgument,
                                               active_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kCumulsArgument,
                                               cumuls_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kTransitsArgument,
                                               transits_);
    visitor->EndVisitConstraint(ModelVisitor::kDelayedPathCumul, this);
  }

  std::string DebugString() const override {
    std::string out = "DelayedPathCumul(";
    for (int i = 0; i < nexts_.size(); ++i) {
      out += nexts_[i]->DebugString() + " " + cumuls_[i]->DebugString();
    }
    out += ")";
    return out;
  }

 private:
  void CumulRange(int64 index) {
    if (index < nexts_.size()) {
      if (nexts_[index]->Bound()) {
        if (active_[index]->Min() > 0) {
          PropagateLink(index, nexts_[index]->Min());
        }
      } else {
        UpdateSupport(index);
      }
    }
    if (prevs_[index] >= 0) {
      PropagateLink(prevs_[index], index);
    } else {
      for (int i = 0; i < nexts_.size(); ++i) {
        if (index == supports_[i]) {
          UpdateSupport(i);
        }
      }
    }
  }
  void UpdateSupport(int index) {
    int support = supports_[index];
    if (support < 0 || !AcceptLink(index, support)) {
      IntVar* const next = nexts_[index];
      for (int i = next->Min(); i <= next->Max(); ++i) {
        if (i != support && AcceptLink(index, i)) {
          supports_[index] = i;
          return;
        }
      }
      active_[index]->SetMax(0);
    }
  }
  void PropagateLink(int64 index, int64 next) {
    IntVar* const cumul_var = cumuls_[index];
    IntVar* const next_cumul_var = cumuls_[next];
    IntVar* const transit = transits_[index];
    const int64 transit_min = transit->Min();
    const int64 transit_max = transit->Max();
    next_cumul_var->SetMin(CapAdd(cumul_var->Min(), transit_min));
    next_cumul_var->SetMax(CapAdd(cumul_var->Max(), transit_max));
    const int64 next_cumul_min = next_cumul_var->Min();
    const int64 next_cumul_max = next_cumul_var->Max();
    cumul_var->SetMin(CapSub(next_cumul_min, transit_max));
    cumul_var->SetMax(CapSub(next_cumul_max, transit_min));
    transit->SetMin(CapSub(next_cumul_min, cumul_var->Max()));
    transit->SetMax(CapSub(next_cumul_max, cumul_var->Min()));
  }
  bool AcceptLink(int index, int next) const {
    IntVar* const cumul_var = cumuls_[index];
    IntVar* const next_cumul_var = cumuls_[next];
    IntVar* const transit = transits_[index];
    return transit->Min() <= CapSub(next_cumul_var->Max(), cumul_var->Min()) &&
           CapSub(next_cumul_var->Min(), cumul_var->Max()) <= transit->Max();
  }

  const std::vector<IntVar*> nexts_;
  const std::vector<IntVar*> active_;
  const std::vector<IntVar*> cumuls_;
  const std::vector<IntVar*> transits_;
  std::vector<Demon*> cumul_transit_demons_;
  Demon* path_demon_;
  StampedVector<int> touched_;
  std::vector<int64> chain_starts_;
  std::vector<int64> chain_ends_;
  std::vector<bool> is_chain_start_;
  RevArray<int> prevs_;
  std::vector<int> supports_;
  RevArray<bool> was_bound_;
  RevArray<bool> has_cumul_demon_;
};

// cumuls[next[i]] = cumuls[i] + transit_evaluator(i, next[i])

class IndexEvaluator2PathCumul : public BasePathCumul {
 public:
  IndexEvaluator2PathCumul(Solver* const s, const std::vector<IntVar*>& nexts,
                           const std::vector<IntVar*>& active,
                           const std::vector<IntVar*>& cumuls,
                           Solver::IndexEvaluator2 transit_evaluator);
  ~IndexEvaluator2PathCumul() override {}
  void NextBound(int index) override;
  bool AcceptLink(int i, int j) const override;

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kPathCumul, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kNextsArgument,
                                               nexts_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kActiveArgument,
                                               active_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kCumulsArgument,
                                               cumuls_);
    // TODO(user): Visit transit correctly.
    // visitor->VisitIntegerVariableArrayArgument(
    //     ModelVisitor::kTransitsArgument,
    //     transit_evaluator);
    visitor->EndVisitConstraint(ModelVisitor::kPathCumul, this);
  }

 private:
  Solver::IndexEvaluator2 transits_evaluator_;
};

IndexEvaluator2PathCumul::IndexEvaluator2PathCumul(
    Solver* const s, const std::vector<IntVar*>& nexts,
    const std::vector<IntVar*>& active, const std::vector<IntVar*>& cumuls,
    Solver::IndexEvaluator2 transit_evaluator)
    : BasePathCumul(s, nexts, active, cumuls),
      transits_evaluator_(std::move(transit_evaluator)) {}

void IndexEvaluator2PathCumul::NextBound(int index) {
  if (active_[index]->Min() == 0) return;
  const int64 next = nexts_[index]->Value();
  IntVar* cumul = cumuls_[index];
  IntVar* cumul_next = cumuls_[next];
  const int64 transit = transits_evaluator_(index, next);
  cumul_next->SetMin(cumul->Min() + transit);
  cumul_next->SetMax(CapAdd(cumul->Max(), transit));
  cumul->SetMin(CapSub(cumul_next->Min(), transit));
  cumul->SetMax(CapSub(cumul_next->Max(), transit));
  if (prevs_[next] < 0) {
    prevs_.SetValue(solver(), next, index);
  }
}

bool IndexEvaluator2PathCumul::AcceptLink(int i, int j) const {
  const IntVar* const cumul_i = cumuls_[i];
  const IntVar* const cumul_j = cumuls_[j];
  const int64 transit = transits_evaluator_(i, j);
  return transit <= CapSub(cumul_j->Max(), cumul_i->Min()) &&
         CapSub(cumul_j->Min(), cumul_i->Max()) <= transit;
}

// ----- ResulatCallback2SlackPathCumul -----

class IndexEvaluator2SlackPathCumul : public BasePathCumul {
 public:
  IndexEvaluator2SlackPathCumul(Solver* const s, const std::vector<IntVar*>& nexts,
                                const std::vector<IntVar*>& active,
                                const std::vector<IntVar*>& cumuls,
                                const std::vector<IntVar*>& slacks,
                                Solver::IndexEvaluator2 transit_evaluator);
  ~IndexEvaluator2SlackPathCumul() override {}
  void Post() override;
  void NextBound(int index) override;
  bool AcceptLink(int i, int j) const override;
  void SlackRange(int index);

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kPathCumul, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kNextsArgument,
                                               nexts_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kActiveArgument,
                                               active_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kCumulsArgument,
                                               cumuls_);
    // TODO(user): Visit transit correctly.
    // visitor->VisitIntegerVariableArrayArgument(
    //     ModelVisitor::kTransitsArgument,
    //     transit_evaluator);
    visitor->EndVisitConstraint(ModelVisitor::kPathCumul, this);
  }

 private:
  const std::vector<IntVar*> slacks_;
  Solver::IndexEvaluator2 transits_evaluator_;
};

IndexEvaluator2SlackPathCumul::IndexEvaluator2SlackPathCumul(
    Solver* const s, const std::vector<IntVar*>& nexts,
    const std::vector<IntVar*>& active, const std::vector<IntVar*>& cumuls,
    const std::vector<IntVar*>& slacks, Solver::IndexEvaluator2 transit_evaluator)
    : BasePathCumul(s, nexts, active, cumuls),
      slacks_(slacks),
      transits_evaluator_(std::move(transit_evaluator)) {}

void IndexEvaluator2SlackPathCumul::Post() {
  BasePathCumul::Post();
  for (int i = 0; i < size(); ++i) {
    Demon* slack_demon = MakeConstraintDemon1(
        solver(), this, &IndexEvaluator2SlackPathCumul::SlackRange,
        "SlackRange", i);
    slacks_[i]->WhenRange(slack_demon);
  }
}

void IndexEvaluator2SlackPathCumul::SlackRange(int index) {
  if (nexts_[index]->Bound()) {
    NextBound(index);
  } else {
    UpdateSupport(index);
  }
  if (prevs_[index] >= 0) {
    NextBound(prevs_[index]);
  } else {
    for (int i = 0; i < size(); ++i) {
      if (index == supports_[i]) {
        UpdateSupport(i);
      }
    }
  }
}

void IndexEvaluator2SlackPathCumul::NextBound(int index) {
  if (active_[index]->Min() == 0) return;
  const int64 next = nexts_[index]->Value();
  IntVar* const cumul = cumuls_[index];
  IntVar* const cumul_next = cumuls_[next];
  IntVar* const slack = slacks_[index];
  const int64 transit = transits_evaluator_(index, next);
  const int64 cumul_next_minus_transit_min = CapSub(cumul_next->Min(), transit);
  const int64 cumul_next_minus_transit_max = CapSub(cumul_next->Max(), transit);
  cumul_next->SetMin(CapAdd(CapAdd(cumul->Min(), transit), slack->Min()));
  cumul_next->SetMax(CapAdd(CapAdd(cumul->Max(), transit), slack->Max()));
  cumul->SetMin(CapSub(cumul_next_minus_transit_min, slack->Max()));
  cumul->SetMax(CapSub(cumul_next_minus_transit_max, slack->Min()));
  slack->SetMin(CapSub(cumul_next_minus_transit_min, cumul->Max()));
  slack->SetMax(CapSub(cumul_next_minus_transit_max, cumul->Min()));
  if (prevs_[next] < 0) {
    prevs_.SetValue(solver(), next, index);
  }
}

bool IndexEvaluator2SlackPathCumul::AcceptLink(int i, int j) const {
  const IntVar* const cumul_i = cumuls_[i];
  const IntVar* const cumul_j = cumuls_[j];
  const IntVar* const slack = slacks_[i];
  const int64 transit = transits_evaluator_(i, j);
  return CapAdd(transit, slack->Min()) <=
             CapSub(cumul_j->Max(), cumul_i->Min()) &&
         CapSub(cumul_j->Min(), cumul_i->Max()) <=
             CapAdd(slack->Max(), transit);
}
}  // namespace

Constraint* Solver::MakePathCumul(const std::vector<IntVar*>& nexts,
                                  const std::vector<IntVar*>& active,
                                  const std::vector<IntVar*>& cumuls,
                                  const std::vector<IntVar*>& transits) {
  CHECK_EQ(nexts.size(), active.size());
  CHECK_EQ(transits.size(), nexts.size());
  return RevAlloc(new PathCumul(this, nexts, active, cumuls, transits));
}

Constraint* Solver::MakePathCumul(const std::vector<IntVar*>& nexts,
                                  const std::vector<IntVar*>& active,
                                  const std::vector<IntVar*>& cumuls,
                                  Solver::IndexEvaluator2 transit_evaluator) {
  CHECK_EQ(nexts.size(), active.size());
  return RevAlloc(new IndexEvaluator2PathCumul(this, nexts, active, cumuls,
                                               transit_evaluator));
}

Constraint* Solver::MakePathCumul(const std::vector<IntVar*>& nexts,
                                  const std::vector<IntVar*>& active,
                                  const std::vector<IntVar*>& cumuls,
                                  const std::vector<IntVar*>& slacks,
                                  Solver::IndexEvaluator2 transit_evaluator) {
  CHECK_EQ(nexts.size(), active.size());
  return RevAlloc(new IndexEvaluator2SlackPathCumul(this, nexts, active, cumuls,
                                                    slacks, transit_evaluator));
}

Constraint* Solver::MakeDelayedPathCumul(const std::vector<IntVar*>& nexts,
                                         const std::vector<IntVar*>& active,
                                         const std::vector<IntVar*>& cumuls,
                                         const std::vector<IntVar*>& transits) {
  CHECK_EQ(nexts.size(), active.size());
  CHECK_EQ(transits.size(), nexts.size());
  return RevAlloc(new DelayedPathCumul(this, nexts, active, cumuls, transits));
}
}  // namespace operations_research

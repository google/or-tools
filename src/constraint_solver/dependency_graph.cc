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


#include <deque>
#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/map_util.h"
#include "base/stl_util.h"
#include "base/hash.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/bitset.h"

namespace operations_research {
// Outgoing dependency from a node (destination, offset).
struct Arc {
  Arc(DependencyGraphNode* const n, int64 o) : node(n), offset(o) {}
  DependencyGraphNode* node;
  int64 offset;
};

typedef std::vector<Arc> Arcs;

// A node in the dependency graph.
class DependencyGraphNode {
 public:
  enum PerformedState {
    UNPERFORMED,
    PERFORMED,
    UNDECIDED
  };
  explicit DependencyGraphNode(DependencyGraph* const graph) : graph_(graph) {
    CHECK(graph_ != nullptr);
  }
  virtual ~DependencyGraphNode() {}
  virtual int64 Min() const = 0;
  virtual int64 Max() const = 0;
  virtual PerformedState State() = 0;
  void SetMin(int64 new_min);
  virtual void SetMinInternal(int64 new_min) = 0;
  void SetMax(int64 new_max);
  virtual void SetMaxInternal(int64 new_max) = 0;
  virtual void SetState(PerformedState state) = 0;
  virtual std::string DebugString() const = 0;

  void AddMinDependency(DependencyGraphNode* const node, int64 offset);
  void AddMaxDependency(DependencyGraphNode* const node, int64 offset);
  const Arcs& min_dependencies() const { return min_dependencies_; }
  const Arcs& max_dependencies() const { return max_dependencies_; }
  void PropagateMin();
  void PropagateMax();
  DependencyGraph* graph() const { return graph_; }

 private:
  Arcs min_dependencies_;
  Arcs max_dependencies_;
  DependencyGraph* const graph_;
};

namespace {
class IntervalVarStartAdapter : public DependencyGraphNode {
 public:
  IntervalVarStartAdapter(DependencyGraph* const graph, IntervalVar* const var)
      : DependencyGraphNode(graph), interval_var_(var) {
    CHECK(graph != nullptr);
    CHECK(var != nullptr);
    Demon* const demon =
        interval_var_->solver()->MakeCallbackDemon(NewPermanentCallback(
            this, &IntervalVarStartAdapter::WhenIntervalChanged));
    interval_var_->WhenAnything(demon);
  }

  virtual ~IntervalVarStartAdapter() {}

  virtual int64 Min() const { return interval_var_->StartMin(); }

  virtual int64 Max() const { return interval_var_->StartMax(); }

  virtual void SetMinInternal(int64 new_min) {
    interval_var_->SetStartMin(new_min);
  }

  virtual void SetMaxInternal(int64 new_max) {
    interval_var_->SetStartMax(new_max);
  }

  virtual PerformedState State() {
    if (interval_var_->MustBePerformed()) {
      return PERFORMED;
    } else if (interval_var_->MayBePerformed()) {
      return UNPERFORMED;
    } else {
      return UNDECIDED;
    }
  }

  virtual void SetState(PerformedState state) {
    CHECK_NE(state, UNDECIDED);
    interval_var_->SetPerformed(state == PERFORMED);
  }

  virtual std::string DebugString() const {
    return StringPrintf("Node-Start(%s)", interval_var_->DebugString().c_str());
  }

  virtual void WhenIntervalChanged() {
    graph()->Enqueue(this, true);   // min may have changed.
    graph()->Enqueue(this, false);  // max may have changed.
  }

 private:
  IntervalVar* const interval_var_;
};

class NonReversibleDependencyGraph : public DependencyGraph {
 public:
  // Represents node(i) has min/max changed.
  struct QueueElem {
    QueueElem(DependencyGraphNode* const n, bool c) : node(n), changed_min(c) {}
    DependencyGraphNode* node;
    bool changed_min;
  };

  explicit NonReversibleDependencyGraph(Solver* const solver)
      : solver_(solver), in_process_(0) {}

  virtual ~NonReversibleDependencyGraph() {}

  virtual void AddEquality(DependencyGraphNode* const left,
                           DependencyGraphNode* const right, int64 offset) {
    AddInequality(left, right, offset);
    AddInequality(right, left, -offset);
  }

  virtual void AddInequality(DependencyGraphNode* const left,
                             DependencyGraphNode* const right, int64 offset) {
    right->AddMinDependency(left, offset);
    left->AddMaxDependency(right, offset);
    Freeze();
    Enqueue(right, true);
    Enqueue(left, false);
    UnFreeze();
  }

  virtual void PropagatePerformed(DependencyGraphNode* const node) {
    Freeze();
    Enqueue(node, true);
    Enqueue(node, false);
    UnFreeze();
  }

  virtual void Enqueue(DependencyGraphNode* const node, bool changed_min) {
    CheckStamp();
    actives_.push_back(QueueElem(node, changed_min));
    ProcessQueue();
  }

  virtual std::string DebugString() const { return "NonReversibleDependencyGraph"; }

 private:
  bool Dequeue(DependencyGraphNode** const node, bool* const changed_min) {
    DCHECK(node != nullptr);
    DCHECK(changed_min != nullptr);
    if (actives_.empty()) {
      return false;
    }

    *node = actives_.front().node;
    *changed_min = actives_.front().changed_min;
    actives_.pop_front();
    return true;
  }

  void ProcessQueue() {
    if (in_process_ == 0) {
      ++in_process_;
      DependencyGraphNode* node = nullptr;
      bool changed_min = false;
      while (Dequeue(&node, &changed_min)) {
        if (changed_min) {
          node->PropagateMin();
        } else {
          node->PropagateMax();
        }
      }
      --in_process_;
    }
  }

  void CheckStamp() {
    if (in_process_ == 0 && solver_->fail_stamp() != fail_stamp_) {
      Clear();
      fail_stamp_ = solver_->fail_stamp();
    }
  }

  void Freeze() {
    CheckStamp();
    ++in_process_;
  }

  void UnFreeze() {
    --in_process_;
    ProcessQueue();
  }

  void Clear() {
    actives_.clear();
    in_process_ = 0;
  }

  Solver* const solver_;
  std::deque<QueueElem> actives_;
  int in_process_;
  uint64 fail_stamp_;
};
}  // namespace

// DependencyGraphNode.

void DependencyGraphNode::AddMinDependency(DependencyGraphNode* const node,
                                           int64 offset) {
  min_dependencies_.push_back(Arc(node, offset));
}

void DependencyGraphNode::SetMin(int64 new_min) {
  if (Min() < new_min) {
    SetMinInternal(new_min);
    graph_->Enqueue(this, true);
  }
}

void DependencyGraphNode::SetMax(int64 new_max) {
  if (Max() > new_max) {
    SetMaxInternal(new_max);
    graph_->Enqueue(this, false);
  }
}

void DependencyGraphNode::PropagateMin() {
  if (State() == PERFORMED) {
    const int64 current_min = Min();
    for (const Arc& arc : min_dependencies_) {
      arc.node->SetMin(current_min + arc.offset);
    }
  }
}

void DependencyGraphNode::AddMaxDependency(DependencyGraphNode* const node,
                                           int64 offset) {
  max_dependencies_.push_back(Arc(node, offset));
}

void DependencyGraphNode::PropagateMax() {
  if (State() == PERFORMED) {
    const int64 current_max = Max();
    for (const Arc& arc : max_dependencies_) {
      arc.node->SetMax(current_max - arc.offset);
    }
  }
}

// Dependency Graph internal API.

DependencyGraph::~DependencyGraph() { STLDeleteElements(&managed_nodes_); }

DependencyGraphNode* DependencyGraph::BuildStartNode(IntervalVar* const var) {
  DependencyGraphNode* const already_there =
      FindPtrOrNull(start_node_map_, var);
  if (already_there != nullptr) {
    return already_there;
  }
  DependencyGraphNode* const newly_created =
      new IntervalVarStartAdapter(this, var);
  start_node_map_[var] = newly_created;
  managed_nodes_.push_back(newly_created);
  return newly_created;
}

// External API.

void DependencyGraph::AddStartsAfterEndWithDelay(IntervalVar* const var1,
                                                 IntervalVar* const var2,
                                                 int64 delay) {
  // Only interval with fixed durations are supported.
  CHECK_EQ(var2->DurationMin(), var2->DurationMax());
  DependencyGraphNode* const node1 = BuildStartNode(var1);
  DependencyGraphNode* const node2 = BuildStartNode(var2);
  AddInequality(node1, node2, delay + var2->DurationMin());
}

void DependencyGraph::AddStartsAtEndWithDelay(IntervalVar* const var1,
                                              IntervalVar* const var2,
                                              int64 delay) {
  // Only interval with fixed durations are supported.
  CHECK_EQ(var2->DurationMin(), var2->DurationMax());
  DependencyGraphNode* const node1 = BuildStartNode(var1);
  DependencyGraphNode* const node2 = BuildStartNode(var2);
  AddEquality(node1, node2, delay + var2->DurationMin());
}

void DependencyGraph::AddStartsAfterStartWithDelay(IntervalVar* const var1,
                                                   IntervalVar* const var2,
                                                   int64 delay) {
  DependencyGraphNode* const node1 = BuildStartNode(var1);
  DependencyGraphNode* const node2 = BuildStartNode(var2);
  AddInequality(node1, node2, delay);
}

void DependencyGraph::AddStartsAtStartWithDelay(IntervalVar* const var1,
                                                IntervalVar* const var2,
                                                int64 delay) {
  DependencyGraphNode* const node1 = BuildStartNode(var1);
  DependencyGraphNode* const node2 = BuildStartNode(var2);
  AddEquality(node1, node2, delay);
}

DependencyGraph* BuildDependencyGraph(Solver* const solver) {
  return new NonReversibleDependencyGraph(solver);
}

DependencyGraph* Solver::Graph() const { return dependency_graph_.get(); }
}  // namespace operations_research

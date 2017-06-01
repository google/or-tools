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

#include "ortools/sat/no_cycle.h"

#include <numeric>

#include "ortools/base/stl_util.h"

namespace operations_research {
namespace sat {

bool NoCyclePropagator::Propagate(Trail* trail) {
  if (problem_is_unsat_) {
    trail->MutableConflict()->clear();
    return false;
  }

  if (!initialization_is_done_) {
    CHECK_EQ(0, trail->CurrentDecisionLevel());
    initialization_is_done_ = true;

    // Propagate all that can be propagated using the fixed arcs.
    for (int node = 0; node < graph_.size(); ++node) {
      FillNodeIsReachedWithAntecedentOf(node);
      for (const Arc arc : potential_graph_[node]) {
        if (!node_is_reached_[arc.head]) continue;

        // We know that l must be false.
        const Literal l(arc.literal_index);
        if (trail->Assignment().VariableIsAssigned(l.Variable())) {
          if (trail->Assignment().LiteralIsTrue(l)) {
            // The problem is UNSAT.
            trail->MutableConflict()->clear();
            return false;
          }
        } else {
          trail->EnqueueWithUnitReason(l.Negated());
        }
      }
    }
  }

  while (propagation_trail_index_ < trail->Index()) {
    const Literal literal = (*trail)[propagation_trail_index_++];
    if (literal.Index() >= potential_arcs_.size()) continue;
    for (const std::pair<int, int>& p : potential_arcs_[literal.Index()]) {
      {
        // Remove this arc from the potential_graph_.
        //
        // TODO(user): this is not super efficient, but it helps speeding up
        // the propagation here and in the makespan constraint.
        int new_size = 0;
        std::vector<Arc>& ref = potential_graph_[p.first];
        const int size = ref.size();
        while (new_size < size &&
               ref[new_size].literal_index != literal.Index()) {
          ++new_size;
        }
        for (int i = new_size; i < size; ++i) {
          if (ref[i].literal_index != literal.Index()) {
            ref[new_size++] = ref[i];
          }
        }
        ref.resize(new_size);
      }

      if (!include_propagated_arcs_in_graph_ &&
          trail->AssignmentType(literal.Variable()) == propagator_id_) {
        continue;
      }

      // Warning: The order of the following 3 lines matter!
      if (IsReachable(p.first, p.second)) continue;
      newly_reachable_ = NewlyReachable(p.second, &node_is_reached_);
      FillNodeIsReachedWithAntecedentOf(p.first);

      // Do nothing if we reached the threshold on the number of arcs.
      if (num_arcs_ == num_arcs_threshold_) continue;

      if (node_is_reached_[p.second]) {  // Conflict.
        // Note that this modifies node_is_reached_ and reached_nodes_, but
        // since we abort aftewards, it is fine.
        FindReasonForPath(*trail, p.second, p.first, propagation_trail_index_,
                          trail->MutableConflict());
        trail->MutableConflict()->push_back(literal.Negated());
        return false;
      }

      ++num_arcs_;
      graph_[p.first].push_back({p.second, literal.Index()});
      reverse_graph_[p.second].push_back({p.first, literal.Index()});

      for (const int node : newly_reachable_) {
        for (const Arc arc : potential_graph_[node]) {
          CHECK_NE(arc.literal_index, kNoLiteralIndex);
          if (node_is_reached_[arc.head]) {
            const Literal l(arc.literal_index);
            if (trail->Assignment().VariableIsAssigned(l.Variable())) {
              // TODO(user): we could detect a conflict earlier if the literal
              // l is already assigned to true.
              continue;
            }

            // Save the information needed for the lazy-explanation and enqueue
            // the fact that "arc" cannot be in the graph.
            const int trail_index = trail->Index();
            if (trail_index >= reason_arc_.size()) {
              reason_arc_.resize(trail_index + 1);
              reason_trail_limit_.resize(trail_index + 1);
            }
            reason_arc_[trail_index] = {node, arc.head};
            reason_trail_limit_[trail_index] = propagation_trail_index_;
            trail->Enqueue(l.Negated(), propagator_id_);
          }
        }
      }
    }
  }
  return true;
}

void NoCyclePropagator::Untrail(const Trail& trail, int trail_index) {
  while (propagation_trail_index_ > trail_index) {
    const Literal literal = trail[--propagation_trail_index_];
    if (literal.Index() >= potential_arcs_.size()) continue;
    for (const std::pair<int, int>& p : potential_arcs_[literal.Index()]) {
      DCHECK_LT(p.first, graph_.size());
      DCHECK_GE(p.first, 0);

      potential_graph_[p.first].push_back({p.second, literal.Index()});

      // We only remove this arc if it was added. That is if it is the last arc
      // in graph_[p.first].
      if (graph_[p.first].empty()) continue;
      if (graph_[p.first].back().literal_index != literal.Index()) continue;

      --num_arcs_;
      graph_[p.first].pop_back();
      reverse_graph_[p.second].pop_back();
    }
  }
}

// TODO(user): If one literal propagate many arcs, and more than one is needed
// to form a cycle, this will not work properly.
gtl::Span<Literal> NoCyclePropagator::Reason(const Trail& trail,
                                                    int trail_index) const {
  const int source = reason_arc_[trail_index].second;
  const int target = reason_arc_[trail_index].first;
  const int trail_limit = reason_trail_limit_[trail_index];
  std::vector<Literal>* const reason =
      trail.GetVectorToStoreReason(trail_index);

  // Note that this modify node_is_reached_ and reached_nodes_.
  FindReasonForPath(trail, source, target, trail_limit, reason);
  return *reason;
}

namespace {

// This sets the given vector of Booleans to all false using a vector of its
// positions at true in order to exploit sparsity.
//
// TODO(user): Add a test depending on position.size() or use directly
// util::operations_research::SparseBitset.
void ResetBitsetWithPosition(int new_size, std::vector<bool>* bitset,
                             std::vector<int>* true_positions) {
  bitset->resize(new_size, false);
  for (const int i : *true_positions) {
    DCHECK((*bitset)[i]);
    (*bitset)[i] = false;
  }
  true_positions->clear();
  DCHECK(
      std::none_of(bitset->begin(), bitset->end(), [](bool v) { return v; }));
}

}  // namespace

// We use a BFS to try to minimize the reason.
void NoCyclePropagator::FindReasonForPath(const Trail& trail, int source,
                                          int target, int trail_limit,
                                          std::vector<Literal>* reason) const {
  CHECK_NE(source, target);
  ResetBitsetWithPosition(graph_.size(), &node_is_reached_, &reached_nodes_);

  // This is the same code as IsReachable() below, except that we need to
  // remember the path taken to the target and we work on a subgraph.
  reached_nodes_.push_back(source);
  parent_index_with_literal_.clear();
  parent_index_with_literal_.push_back({0, LiteralIndex(-1)});
  node_is_reached_[source] = true;

  int i = 0;
  for (; i < reached_nodes_.size(); ++i) {
    const int node = reached_nodes_[i];
    if (node == target) break;

    // Only consider arc whose literal was assigned before trail_limit. The arcs
    // in graph_[node] are ordered by increasing trail index, so it is okay to
    // abort as soon as an arc was added after trail_limit.
    for (const Arc& arc : graph_[node]) {
      if (arc.literal_index != kNoLiteralIndex) {
        const BooleanVariable var = Literal(arc.literal_index).Variable();
        if (trail.Info(var).trail_index >= trail_limit) break;
      }
      if (node_is_reached_[arc.head]) continue;
      node_is_reached_[arc.head] = true;
      reached_nodes_.push_back(arc.head);
      parent_index_with_literal_.push_back({i, arc.literal_index});
    }
  }

  // Follow the path backward and fill the reason.
  CHECK_LT(i, reached_nodes_.size()) << "The target is not reachable!";
  reason->clear();
  while (i != 0) {
    const LiteralIndex literal_index = parent_index_with_literal_[i].second;
    if (literal_index != kNoLiteralIndex) {
      reason->push_back(Literal(literal_index).Negated());
    }
    i = parent_index_with_literal_[i].first;
  }
}

bool NoCyclePropagator::IsReachable(int source, int destination) const {
  if (source == destination) return true;
  ResetBitsetWithPosition(graph_.size(), &node_is_reached_, &reached_nodes_);
  reached_nodes_.push_back(source);
  node_is_reached_[source] = true;
  for (int i = 0; i < reached_nodes_.size(); ++i) {
    for (const Arc& arc : graph_[reached_nodes_[i]]) {
      if (arc.head == destination) return true;
      if (node_is_reached_[arc.head]) continue;
      node_is_reached_[arc.head] = true;
      reached_nodes_.push_back(arc.head);
    }
  }
  return false;
}

void NoCyclePropagator::FillNodeIsReachedWithAntecedentOf(int source) const {
  ResetBitsetWithPosition(graph_.size(), &node_is_reached_, &reached_nodes_);
  reached_nodes_.push_back(source);
  node_is_reached_[source] = true;
  for (int i = 0; i < reached_nodes_.size(); ++i) {
    for (const Arc& arc : reverse_graph_[reached_nodes_[i]]) {
      if (node_is_reached_[arc.head]) continue;
      node_is_reached_[arc.head] = true;
      reached_nodes_.push_back(arc.head);
    }
  }
}

std::vector<int> NoCyclePropagator::NewlyReachable(
    int source, std::vector<bool>* already_reached) const {
  if ((*already_reached)[source]) return std::vector<int>();
  std::vector<int> result;
  result.push_back(source);
  (*already_reached)[source] = true;
  for (int i = 0; i < result.size(); ++i) {
    for (const Arc& arc : graph_[result[i]]) {
      if ((*already_reached)[arc.head]) continue;
      (*already_reached)[arc.head] = true;
      result.push_back(arc.head);
    }
  }

  // Restore already_reached to its original value.
  for (const int node : result) (*already_reached)[node] = false;
  return result;
}

void NoCyclePropagator::AdjustSizes(int tail, int head, int literal_index) {
  CHECK_NE(tail, head);
  CHECK(!initialization_is_done_);
  CHECK_EQ(0, propagation_trail_index_);

  const int m = std::max(tail, head);
  DCHECK_GE(m, 0);
  if (m >= graph_.size()) {
    graph_.resize(m + 1);
    potential_graph_.resize(m + 1);
    reverse_graph_.resize(m + 1);
  }
  DCHECK_GE(literal_index, 0);
  if (literal_index >= potential_arcs_.size()) {
    potential_arcs_.resize(literal_index + 1);
  }
}

void NoCyclePropagator::AddArc(int tail, int head) {
  AdjustSizes(tail, head, 0);

  // Deal with the corner case of a cycle with the fixed arcs.
  if (problem_is_unsat_ || IsReachable(head, tail)) {
    problem_is_unsat_ = true;
    return;
  }

  // TODO(user): test IsReachable(tail, head) and do not add the arc if true?
  ++num_arcs_;
  graph_[tail].push_back({head, LiteralIndex(-1)});
  reverse_graph_[head].push_back({tail, LiteralIndex(-1)});
}

void NoCyclePropagator::AddPotentialArc(int tail, int head, Literal literal) {
  AdjustSizes(tail, head, literal.Index().value());
  potential_arcs_[literal.Index()].push_back({tail, head});
  potential_graph_[tail].push_back({head, literal.Index()});
  CHECK_EQ(1, potential_arcs_[literal.Index()].size())
      << "We don't support multiple arcs associated to the same literal. "
      << "However, it should be fairly easy to support this case.";
}

}  // namespace sat
}  // namespace operations_research

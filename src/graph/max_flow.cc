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


#include "graph/max_flow.h"

#include <algorithm>

#include "base/stringprintf.h"
#include "graph/graphs.h"

namespace operations_research {

SimpleMaxFlow::SimpleMaxFlow() : num_nodes_(0) {}

ArcIndex SimpleMaxFlow::AddArcWithCapacity(NodeIndex tail, NodeIndex head,
                                           FlowQuantity capacity) {
  const ArcIndex num_arcs = arc_tail_.size();
  num_nodes_ = std::max(num_nodes_, tail + 1);
  num_nodes_ = std::max(num_nodes_, head + 1);
  arc_tail_.push_back(tail);
  arc_head_.push_back(head);
  arc_capacity_.push_back(capacity);
  return num_arcs;
}

NodeIndex SimpleMaxFlow::NumNodes() const { return num_nodes_; }

ArcIndex SimpleMaxFlow::NumArcs() const { return arc_tail_.size(); }

NodeIndex SimpleMaxFlow::Tail(ArcIndex arc) const { return arc_tail_[arc]; }

NodeIndex SimpleMaxFlow::Head(ArcIndex arc) const { return arc_head_[arc]; }

FlowQuantity SimpleMaxFlow::Capacity(ArcIndex arc) const {
  return arc_capacity_[arc];
}

SimpleMaxFlow::Status SimpleMaxFlow::Solve(NodeIndex source, NodeIndex sink) {
  const ArcIndex num_arcs = arc_capacity_.size();
  arc_flow_.assign(num_arcs, 0);
  underlying_max_flow_.reset();
  underlying_graph_.reset();
  optimal_flow_ = 0;
  if (source == sink || source < 0 || sink < 0) {
    return BAD_INPUT;
  }
  if (source >= num_nodes_ || sink >= num_nodes_) {
    return OPTIMAL;
  }
  underlying_graph_.reset(new Graph(num_nodes_, num_arcs));
  underlying_graph_->AddNode(source);
  underlying_graph_->AddNode(sink);
  for (int arc = 0; arc < num_arcs; ++arc) {
    underlying_graph_->AddArc(arc_tail_[arc], arc_head_[arc]);
  }
  underlying_graph_->Build(&arc_permutation_);
  underlying_max_flow_.reset(
      new GenericMaxFlow<Graph>(underlying_graph_.get(), source, sink));
  for (ArcIndex arc = 0; arc < num_arcs; ++arc) {
    ArcIndex permuted_arc =
        arc < arc_permutation_.size() ? arc_permutation_[arc] : arc;
    underlying_max_flow_->SetArcCapacity(permuted_arc, arc_capacity_[arc]);
  }
  if (underlying_max_flow_->Solve()) {
    optimal_flow_ = underlying_max_flow_->GetOptimalFlow();
    for (ArcIndex arc = 0; arc < num_arcs; ++arc) {
      ArcIndex permuted_arc =
          arc < arc_permutation_.size() ? arc_permutation_[arc] : arc;
      arc_flow_[arc] = underlying_max_flow_->Flow(permuted_arc);
    }
  }
  // Translate the GenericMaxFlow::Status. It is different because NOT_SOLVED
  // does not make sense in the simple api.
  switch (underlying_max_flow_->status()) {
    case GenericMaxFlow<Graph>::NOT_SOLVED:
      return BAD_RESULT;
    case GenericMaxFlow<Graph>::OPTIMAL:
      return OPTIMAL;
    case GenericMaxFlow<Graph>::INT_OVERFLOW:
      return POSSIBLE_OVERFLOW;
    case GenericMaxFlow<Graph>::BAD_INPUT:
      return BAD_INPUT;
    case GenericMaxFlow<Graph>::BAD_RESULT:
      return BAD_RESULT;
  }
  return BAD_RESULT;
}

FlowQuantity SimpleMaxFlow::OptimalFlow() const { return optimal_flow_; }

FlowQuantity SimpleMaxFlow::Flow(ArcIndex arc) const { return arc_flow_[arc]; }

void SimpleMaxFlow::GetSourceSideMinCut(std::vector<NodeIndex>* result) {
  if (underlying_max_flow_.get() == NULL) return;
  underlying_max_flow_->GetSourceSideMinCut(result);
}

void SimpleMaxFlow::GetSinkSideMinCut(std::vector<NodeIndex>* result) {
  if (underlying_max_flow_.get() == NULL) return;
  underlying_max_flow_->GetSinkSideMinCut(result);
}

FlowModel SimpleMaxFlow::CreateFlowModelOfLastSolve() {
  if (underlying_max_flow_.get() == NULL) return FlowModel();
  return underlying_max_flow_->CreateFlowModel();
}

template <typename Graph>
GenericMaxFlow<Graph>::GenericMaxFlow(const Graph* graph, NodeIndex source,
                                      NodeIndex sink)
    : graph_(graph),
      node_excess_(),
      node_potential_(),
      residual_arc_capacity_(),
      first_admissible_arc_(),
      active_nodes_(),
      source_(source),
      sink_(sink),
      use_global_update_(true),
      use_two_phase_algorithm_(true),
      process_node_by_height_(true),
      check_input_(true),
      check_result_(true),
      stats_("MaxFlow") {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(graph->IsNodeValid(source));
  DCHECK(graph->IsNodeValid(sink));
  const NodeIndex max_num_nodes = Graphs<Graph>::NodeReservation(*graph_);
  if (max_num_nodes > 0) {
    node_excess_.Reserve(0, max_num_nodes - 1);
    node_excess_.SetAll(0);
    node_potential_.Reserve(0, max_num_nodes - 1);
    node_potential_.SetAll(0);
    first_admissible_arc_.Reserve(0, max_num_nodes - 1);
    first_admissible_arc_.SetAll(Graph::kNilArc);
    bfs_queue_.reserve(max_num_nodes);
    active_nodes_.reserve(max_num_nodes);
  }
  const ArcIndex max_num_arcs = Graphs<Graph>::ArcReservation(*graph_);
  if (max_num_arcs > 0) {
    residual_arc_capacity_.Reserve(-max_num_arcs, max_num_arcs - 1);
    residual_arc_capacity_.SetAll(0);
  }
}

template <typename Graph>
bool GenericMaxFlow<Graph>::CheckInputConsistency() const {
  SCOPED_TIME_STAT(&stats_);
  bool ok = true;
  for (ArcIndex arc = 0; arc < graph_->num_arcs(); ++arc) {
    if (residual_arc_capacity_[arc] < 0) {
      ok = false;
    }
  }
  return ok;
}

template <typename Graph>
void GenericMaxFlow<Graph>::SetArcCapacity(ArcIndex arc,
                                           FlowQuantity new_capacity) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK_LE(0, new_capacity);
  DCHECK(IsArcDirect(arc));
  const FlowQuantity free_capacity = residual_arc_capacity_[arc];
  const FlowQuantity capacity_delta = new_capacity - Capacity(arc);
  if (capacity_delta == 0) {
    return;  // Nothing to do.
  }
  status_ = NOT_SOLVED;
  if (free_capacity + capacity_delta >= 0) {
    // The above condition is true if one of the two conditions is true:
    // 1/ (capacity_delta > 0), meaning we are increasing the capacity
    // 2/ (capacity_delta < 0 && free_capacity + capacity_delta >= 0)
    //    meaning we are reducing the capacity, but that the capacity
    //    reduction is not larger than the free capacity.
    DCHECK((capacity_delta > 0) ||
           (capacity_delta < 0 && free_capacity + capacity_delta >= 0));
    residual_arc_capacity_.Set(arc, free_capacity + capacity_delta);
    DCHECK_LE(0, residual_arc_capacity_[arc]);
  } else {
    // Note that this breaks the preflow invariants but it is currently not an
    // issue since we restart from scratch on each Solve() and we set the status
    // to NOT_SOLVED.
    //
    // TODO(user): The easiest is probably to allow negative node excess in
    // other places than the source, but the current implementation does not
    // deal with this.
    SetCapacityAndClearFlow(arc, new_capacity);
  }
}

template <typename Graph>
void GenericMaxFlow<Graph>::SetArcFlow(ArcIndex arc, FlowQuantity new_flow) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(IsArcValid(arc));
  DCHECK_GE(new_flow, 0);
  const FlowQuantity capacity = Capacity(arc);
  DCHECK_GE(capacity, new_flow);

  // Note that this breaks the preflow invariants but it is currently not an
  // issue since we restart from scratch on each Solve() and we set the status
  // to NOT_SOLVED.
  residual_arc_capacity_.Set(Opposite(arc), -new_flow);
  residual_arc_capacity_.Set(arc, capacity - new_flow);
  status_ = NOT_SOLVED;
}

template <typename Graph>
void GenericMaxFlow<Graph>::GetSourceSideMinCut(
    std::vector<NodeIndex>* result) {
  ComputeReachableNodes<false>(source_, result);
}

template <typename Graph>
void GenericMaxFlow<Graph>::GetSinkSideMinCut(std::vector<NodeIndex>* result) {
  ComputeReachableNodes<true>(sink_, result);
}

template <typename Graph>
bool GenericMaxFlow<Graph>::CheckResult() const {
  SCOPED_TIME_STAT(&stats_);
  bool ok = true;
  if (node_excess_[source_] != -node_excess_[sink_]) {
    LOG(DFATAL) << "-node_excess_[source_] = " << -node_excess_[source_]
                << " != node_excess_[sink_] = " << node_excess_[sink_];
    ok = false;
  }
  for (NodeIndex node = 0; node < graph_->num_nodes(); ++node) {
    if (node != source_ && node != sink_) {
      if (node_excess_[node] != 0) {
        LOG(DFATAL) << "node_excess_[" << node << "] = " << node_excess_[node]
                    << " != 0";
        ok = false;
      }
    }
  }
  for (ArcIndex arc = 0; arc < graph_->num_arcs(); ++arc) {
    const ArcIndex opposite = Opposite(arc);
    const FlowQuantity direct_capacity = residual_arc_capacity_[arc];
    const FlowQuantity opposite_capacity = residual_arc_capacity_[opposite];
    if (direct_capacity < 0) {
      LOG(DFATAL) << "residual_arc_capacity_[" << arc
                  << "] = " << direct_capacity << " < 0";
      ok = false;
    }
    if (opposite_capacity < 0) {
      LOG(DFATAL) << "residual_arc_capacity_[" << opposite
                  << "] = " << opposite_capacity << " < 0";
      ok = false;
    }
    // The initial capacity of the direct arcs is non-negative.
    if (direct_capacity + opposite_capacity < 0) {
      LOG(DFATAL) << "initial capacity [" << arc
                  << "] = " << direct_capacity + opposite_capacity << " < 0";
      ok = false;
    }
  }
  return ok;
}

template <typename Graph>
bool GenericMaxFlow<Graph>::AugmentingPathExists() const {
  SCOPED_TIME_STAT(&stats_);

  // We simply compute the reachability from the source in the residual graph.
  const NodeIndex num_nodes = graph_->num_nodes();
  std::vector<bool> is_reached(num_nodes, false);
  std::vector<NodeIndex> to_process;

  to_process.push_back(source_);
  is_reached[source_] = true;
  while (!to_process.empty()) {
    const NodeIndex node = to_process.back();
    to_process.pop_back();
    for (OutgoingOrOppositeIncomingArcIterator it(*graph_, node); it.Ok();
         it.Next()) {
      const ArcIndex arc = it.Index();
      if (residual_arc_capacity_[arc] > 0) {
        const NodeIndex head = graph_->Head(arc);
        if (!is_reached[head]) {
          is_reached[head] = true;
          to_process.push_back(head);
        }
      }
    }
  }
  return is_reached[sink_];
}

template <typename Graph>
bool GenericMaxFlow<Graph>::CheckRelabelPrecondition(NodeIndex node) const {
  DCHECK(IsActive(node));
  for (OutgoingOrOppositeIncomingArcIterator it(*graph_, node); it.Ok();
       it.Next()) {
    const ArcIndex arc = it.Index();
    DCHECK(!IsAdmissible(arc)) << DebugString("CheckRelabelPrecondition:", arc);
  }
  return true;
}

template <typename Graph>
std::string GenericMaxFlow<Graph>::DebugString(const std::string& context,
                                          ArcIndex arc) const {
  const NodeIndex tail = Tail(arc);
  const NodeIndex head = Head(arc);
  return StringPrintf(
      "%s Arc %d, from %d to %d, "
      "Capacity = %lld, Residual capacity = %lld, "
      "Flow = residual capacity for reverse arc = %lld, "
      "Height(tail) = %d, Height(head) = %d, "
      "Excess(tail) = %lld, Excess(head) = %lld",
      context.c_str(), arc, tail, head, Capacity(arc),
      residual_arc_capacity_[arc], Flow(arc), node_potential_[tail],
      node_potential_[head], node_excess_[tail], node_excess_[head]);
}

template <typename Graph>
bool GenericMaxFlow<Graph>::Solve() {
  status_ = NOT_SOLVED;
  if (check_input_ && !CheckInputConsistency()) {
    status_ = BAD_INPUT;
    return false;
  }
  InitializePreflow();

  // Deal with the case when source_ or sink_ is not inside graph_.
  // Since they are both specified independently of the graph, we do need to
  // take care of this corner case.
  const NodeIndex num_nodes = graph_->num_nodes();
  if (sink_ >= num_nodes || source_ >= num_nodes) {
    // Behave like a normal graph where source_ and sink_ are disconnected.
    // Note that the arc flow is set to 0 by InitializePreflow().
    status_ = OPTIMAL;
    return true;
  }
  if (use_global_update_) {
    RefineWithGlobalUpdate();
  } else {
    Refine();
  }
  if (check_result_) {
    if (!CheckResult()) {
      status_ = BAD_RESULT;
      return false;
    }
    if (GetOptimalFlow() < kMaxFlowQuantity && AugmentingPathExists()) {
      LOG(ERROR) << "The algorithm terminated, but the flow is not maximal!";
      status_ = BAD_RESULT;
      return false;
    }
  }
  DCHECK_EQ(node_excess_[sink_], -node_excess_[source_]);
  status_ = OPTIMAL;
  if (GetOptimalFlow() == kMaxFlowQuantity && AugmentingPathExists()) {
    // In this case, we are sure that the flow is > kMaxFlowQuantity.
    status_ = INT_OVERFLOW;
  }
  IF_STATS_ENABLED(VLOG(1) << stats_.StatString());
  return true;
}

template <typename Graph>
void GenericMaxFlow<Graph>::InitializePreflow() {
  SCOPED_TIME_STAT(&stats_);
  // InitializePreflow() clears the whole flow that could have been computed
  // by a previous Solve(). This is not optimal in terms of complexity.
  // TODO(user): find a way to make the re-solving incremental (not an obvious
  // task, and there has not been a lot of literature on the subject.)
  node_excess_.SetAll(0);
  const ArcIndex num_arcs = graph_->num_arcs();
  for (ArcIndex arc = 0; arc < num_arcs; ++arc) {
    SetCapacityAndClearFlow(arc, Capacity(arc));
  }

  // All the initial heights are zero except for the source whose height is
  // equal to the number of nodes and will never change during the algorithm.
  node_potential_.SetAll(0);
  node_potential_.Set(source_, graph_->num_nodes());

  // Initially no arcs are admissible except maybe the one leaving the source,
  // but we treat the source in a special way, see
  // SaturateOutgoingArcsFromSource().
  const NodeIndex num_nodes = graph_->num_nodes();
  for (NodeIndex node = 0; node < num_nodes; ++node) {
    first_admissible_arc_[node] = Graph::kNilArc;
  }
}

// Note(user): Calling this function will break the property on the node
// potentials because of the way we cancel flow on cycle. However, we only call
// that at the end of the algorithm, or just before a GlobalUpdate() that will
// restore the precondition on the node potentials.
template <typename Graph>
void GenericMaxFlow<Graph>::PushFlowExcessBackToSource() {
  SCOPED_TIME_STAT(&stats_);
  const NodeIndex num_nodes = graph_->num_nodes();

  // We implement a variation of Tarjan's strongly connected component algorithm
  // to detect cycles published in: Tarjan, R. E. (1972), "Depth-first search
  // and linear graph algorithms", SIAM Journal on Computing. A description can
  // also be found in wikipedia.

  // Stored nodes are settled nodes already stored in the
  // reverse_topological_order (except the sink_ that we do not actually store).
  std::vector<bool> stored(num_nodes, false);
  stored[sink_] = true;

  // The visited nodes that are not yet stored are all the nodes from the
  // source_ to the current node in the current dfs branch.
  std::vector<bool> visited(num_nodes, false);
  visited[sink_] = true;

  // Stack of arcs to explore in the dfs search.
  // The current node is Head(arc_stack.back()).
  std::vector<ArcIndex> arc_stack;

  // Increasing list of indices into the arc_stack that correspond to the list
  // of arcs in the current dfs branch from the source_ to the current node.
  std::vector<int> index_branch;

  // Node in reverse_topological_order in the final dfs tree.
  std::vector<NodeIndex> reverse_topological_order;

  // We start by pushing all the outgoing arcs from the source on the stack to
  // avoid special conditions in the code. As a result, source_ will not be
  // stored in reverse_topological_order, and this is what we want.
  for (OutgoingArcIterator it(*graph_, source_); it.Ok(); it.Next()) {
    const ArcIndex arc = it.Index();
    const FlowQuantity flow = Flow(arc);
    if (flow > 0) {
      arc_stack.push_back(arc);
    }
  }
  visited[source_] = true;

  // Start the dfs on the subgraph formed by the direct arcs with positive flow.
  while (!arc_stack.empty()) {
    const NodeIndex node = Head(arc_stack.back());

    // If the node is visited, it means we have explored all its arcs and we
    // have just backtracked in the dfs. Store it if it is not already stored
    // and process the next arc on the stack.
    if (visited[node]) {
      if (!stored[node]) {
        stored[node] = true;
        reverse_topological_order.push_back(node);
        DCHECK(!index_branch.empty());
        index_branch.pop_back();
      }
      arc_stack.pop_back();
      continue;
    }

    // The node is a new unexplored node, add all its outgoing arcs with
    // positive flow to the stack and go deeper in the dfs.
    DCHECK(!stored[node]);
    DCHECK(index_branch.empty() ||
           (arc_stack.size() - 1 > index_branch.back()));
    visited[node] = true;
    index_branch.push_back(arc_stack.size() - 1);

    for (OutgoingArcIterator it(*graph_, node); it.Ok(); it.Next()) {
      const ArcIndex arc = it.Index();
      const FlowQuantity flow = Flow(arc);
      const NodeIndex head = Head(arc);
      if (flow > 0 && !stored[head]) {
        if (!visited[head]) {
          arc_stack.push_back(arc);
        } else {
          // There is a cycle.
          // Find the first index to consider,
          // arc_stack[index_branch[cycle_begin]] will be the first arc on the
          // cycle.
          int cycle_begin = index_branch.size();
          while (cycle_begin > 0 &&
                 Head(arc_stack[index_branch[cycle_begin - 1]]) != head) {
            --cycle_begin;
          }

          // Compute the maximum flow that can be canceled on the cycle and the
          // min index such that arc_stack[index_branch[i]] will be saturated.
          FlowQuantity max_flow = flow;
          int first_saturated_index = index_branch.size();
          for (int i = index_branch.size() - 1; i >= cycle_begin; --i) {
            const ArcIndex arc_on_cycle = arc_stack[index_branch[i]];
            if (Flow(arc_on_cycle) <= max_flow) {
              max_flow = Flow(arc_on_cycle);
              first_saturated_index = i;
            }
          }

          // This is just here for a DCHECK() below.
          const FlowQuantity excess = node_excess_[head];

          // Cancel the flow on the cycle, and set visited[node] = false for
          // the node that will be backtracked over.
          PushFlow(-max_flow, arc);
          for (int i = index_branch.size() - 1; i >= cycle_begin; --i) {
            const ArcIndex arc_on_cycle = arc_stack[index_branch[i]];
            PushFlow(-max_flow, arc_on_cycle);
            if (i >= first_saturated_index) {
              DCHECK(visited[Head(arc_on_cycle)]);
              visited[Head(arc_on_cycle)] = false;
            } else {
              DCHECK_GT(Flow(arc_on_cycle), 0);
            }
          }

          // This is a simple check that the flow was pushed properly.
          DCHECK_EQ(excess, node_excess_[head]);

          // Backtrack the dfs just before index_branch[first_saturated_index].
          // If the current node is still active, there is nothing to do.
          if (first_saturated_index < index_branch.size()) {
            arc_stack.resize(index_branch[first_saturated_index]);
            index_branch.resize(first_saturated_index);

            // We backtracked over the current node, so there is no need to
            // continue looping over its arcs.
            break;
          }
        }
      }
    }
  }
  DCHECK(arc_stack.empty());
  DCHECK(index_branch.empty());

  // Return the flow to the sink. Note that the sink_ and the source_ are not
  // stored in reverse_topological_order.
  for (int i = 0; i < reverse_topological_order.size(); i++) {
    const NodeIndex node = reverse_topological_order[i];
    if (node_excess_[node] == 0) continue;
    for (IncomingArcIterator it(*graph_, node); it.Ok(); it.Next()) {
      const ArcIndex opposite_arc = Opposite(it.Index());
      if (residual_arc_capacity_[opposite_arc] > 0) {
        const FlowQuantity flow =
            std::min(node_excess_[node], residual_arc_capacity_[opposite_arc]);
        PushFlow(flow, opposite_arc);
        if (node_excess_[node] == 0) break;
      }
    }
    DCHECK_EQ(0, node_excess_[node]);
  }
  DCHECK_EQ(-node_excess_[source_], node_excess_[sink_]);
}

template <typename Graph>
void GenericMaxFlow<Graph>::GlobalUpdate() {
  SCOPED_TIME_STAT(&stats_);
  bfs_queue_.clear();
  int queue_index = 0;
  const NodeIndex num_nodes = graph_->num_nodes();
  node_in_bfs_queue_.assign(num_nodes, false);
  node_in_bfs_queue_[sink_] = true;
  node_in_bfs_queue_[source_] = true;

  // We do two BFS in the reverse residual graph, one from the sink and one from
  // the source. Because all the arcs from the source are saturated (except in
  // presence of integer overflow), the source cannot reach the sink in the
  // residual graph. However, we still want to relabel all the nodes that cannot
  // reach the sink but can reach the source (because if they have excess, we
  // need to push it back to the source).
  //
  // Note that the second pass is not needed here if we use a two-pass algorithm
  // to return the flow to the source after we found the min cut.
  const int num_passes = use_two_phase_algorithm_ ? 1 : 2;
  for (int pass = 0; pass < num_passes; ++pass) {
    if (pass == 0) {
      bfs_queue_.push_back(sink_);
    } else {
      bfs_queue_.push_back(source_);
    }

    while (queue_index != bfs_queue_.size()) {
      const NodeIndex node = bfs_queue_[queue_index];
      ++queue_index;
      const NodeIndex candidate_distance = node_potential_[node] + 1;
      for (OutgoingOrOppositeIncomingArcIterator it(*graph_, node); it.Ok();
           it.Next()) {
        const ArcIndex arc = it.Index();
        const NodeIndex head = Head(arc);

        // Skip the arc if the height of head was already set to the correct
        // value (Remember we are doing reverse BFS).
        if (node_in_bfs_queue_[head]) continue;

        // TODO(user): By using more memory we can speed this up quite a bit by
        // avoiding to take the opposite arc here, too options:
        // - if (residual_arc_capacity_[arc] != arc_capacity_[arc])
        // - if (opposite_arc_is_admissible_[arc])  // need updates.
        // Experiment with the first option shows more than 10% gain on this
        // function running time, which is the bottleneck on many instances.
        const ArcIndex opposite_arc = Opposite(arc);
        if (residual_arc_capacity_[opposite_arc] > 0) {
          // Note(user): We used to have a DCHECK_GE(candidate_distance,
          // node_potential_[head]); which is always true except in the case
          // where we can push more than kMaxFlowQuantity out of the source. The
          // problem comes from the fact that in this case, we call
          // PushFlowExcessBackToSource() in the middle of the algorithm. The
          // later call will break the properties of the node potential. Note
          // however, that this function will recompute a good node potential
          // for all the nodes and thus fix the issue.

          // If head is active, we can steal some or all of its excess.
          // This brings a huge gain on some problems.
          // Note(user): I haven't seen this anywhere in the literature.
          // TODO(user): Investigate more and maybe write a publication :)
          if (node_excess_[head] > 0) {
            const FlowQuantity flow = std::min(
                node_excess_[head], residual_arc_capacity_[opposite_arc]);
            PushFlow(flow, opposite_arc);

            // If the arc became saturated, it is no longer in the residual
            // graph, so we do not need to consider head at this time.
            if (residual_arc_capacity_[opposite_arc] == 0) continue;
          }

          // Note that there is no need to touch first_admissible_arc_[node]
          // because of the relaxed Relabel() we use.
          node_potential_[head] = candidate_distance;
          node_in_bfs_queue_[head] = true;
          bfs_queue_.push_back(head);
        }
      }
    }
  }

  // At the end of the search, some nodes may not be in the bfs_queue_. Such
  // nodes cannot reach the sink_ or source_ in the residual graph, so there is
  // no point trying to push flow toward them. We obtain this effect by setting
  // their height to something unreachable.
  //
  // Note that this also prevents cycling due to our anti-overflow procedure.
  // For instance, suppose there is an edge s -> n outgoing from the source. If
  // node n has no other connection and some excess, we will push the flow back
  // to the source, but if we don't update the height of n
  // SaturateOutgoingArcsFromSource() will push the flow to n again.
  // TODO(user): This is another argument for another anti-overflow algorithm.
  for (NodeIndex node = 0; node < num_nodes; ++node) {
    if (!node_in_bfs_queue_[node]) {
      node_potential_[node] = 2 * num_nodes - 1;
    }
  }

  // Reset the active nodes. Doing it like this pushes the nodes in increasing
  // order of height. Note that bfs_queue_[0] is the sink_ so we skip it.
  DCHECK(IsEmptyActiveNodeContainer());
  for (int i = 1; i < bfs_queue_.size(); ++i) {
    const NodeIndex node = bfs_queue_[i];
    if (node_excess_[node] > 0) {
      DCHECK(IsActive(node));
      PushActiveNode(node);
    }
  }
}

template <typename Graph>
bool GenericMaxFlow<Graph>::SaturateOutgoingArcsFromSource() {
  SCOPED_TIME_STAT(&stats_);
  const NodeIndex num_nodes = graph_->num_nodes();

  // If sink_ or source_ already have kMaxFlowQuantity, then there is no
  // point pushing more flow since it will cause an integer overflow.
  if (node_excess_[sink_] == kMaxFlowQuantity) return false;
  if (node_excess_[source_] == -kMaxFlowQuantity) return false;

  bool flow_pushed = false;
  for (OutgoingArcIterator it(*graph_, source_); it.Ok(); it.Next()) {
    const ArcIndex arc = it.Index();
    const FlowQuantity flow = residual_arc_capacity_[arc];

    // This is a special IsAdmissible() condition for the source.
    if (flow == 0 || node_potential_[Head(arc)] >= num_nodes) continue;

    // We are careful in case the sum of the flow out of the source is greater
    // than kMaxFlowQuantity to avoid overflow. Since we sum two positive
    // integers, we detect an integer overflow when their sum is negative.
    const FlowQuantity current_flow_out_of_source = -node_excess_[source_];
    if (flow + current_flow_out_of_source < 0) {
      // We push as much flow as we can so the current flow on the network will
      // be kMaxFlowQuantity.
      const FlowQuantity capped_flow =
          kMaxFlowQuantity - current_flow_out_of_source;

      // Since at the beginning of the function, current_flow_out_of_source
      // was different from kMaxFlowQuantity, we are sure to have pushed some
      // flow before if capped_flow is 0.
      if (capped_flow == 0) return true;
      PushFlow(capped_flow, arc);
      return true;
    }
    PushFlow(flow, arc);
    flow_pushed = true;
  }
  DCHECK_LE(node_excess_[source_], 0);
  return flow_pushed;
}

template <typename Graph>
void GenericMaxFlow<Graph>::PushFlow(FlowQuantity flow, ArcIndex arc) {
  SCOPED_TIME_STAT(&stats_);
  // TODO(user): Do not allow a zero flow after fixing the UniformMaxFlow code.
  DCHECK_GE(residual_arc_capacity_[Opposite(arc)] + flow, 0);
  DCHECK_GE(residual_arc_capacity_[arc] - flow, 0);

  // node_excess_ should be always greater than or equal to 0 except for the
  // source where it should always be smaller than or equal to 0. Note however
  // that we cannot check this because when we cancel the flow on a cycle in
  // PushFlowExcessBackToSource(), we may break this invariant during the
  // operation even if it is still valid at the end.

  // Update the residual capacity of the arc and its opposite arc.
  residual_arc_capacity_[arc] -= flow;
  residual_arc_capacity_[Opposite(arc)] += flow;

  // Update the excesses at the tail and head of the arc.
  node_excess_[Tail(arc)] -= flow;
  node_excess_[Head(arc)] += flow;
}

template <typename Graph>
void GenericMaxFlow<Graph>::InitializeActiveNodeContainer() {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(IsEmptyActiveNodeContainer());
  const NodeIndex num_nodes = graph_->num_nodes();
  for (NodeIndex node = 0; node < num_nodes; ++node) {
    if (IsActive(node)) {
      if (use_two_phase_algorithm_ && node_potential_[node] >= num_nodes) {
        continue;
      }
      PushActiveNode(node);
    }
  }
}

template <typename Graph>
void GenericMaxFlow<Graph>::Refine() {
  SCOPED_TIME_STAT(&stats_);
  // Usually SaturateOutgoingArcsFromSource() will saturate all the arcs from
  // the source in one go, and we will loop just once. But in case we can push
  // more than kMaxFlowQuantity out of the source the loop is as follow:
  // - Push up to kMaxFlowQuantity out of the source on the admissible outgoing
  //   arcs. Stop if no flow was pushed.
  // - Compute the current max-flow. This will push some flow back to the
  //   source and render more outgoing arcs from the source not admissible.
  //
  // TODO(user): This may not be the most efficient algorithm if we need to loop
  // many times. An alternative may be to handle the source like the other nodes
  // in the algorithm, initially putting an excess of kMaxFlowQuantity on it,
  // and making the source active like any other node with positive excess. To
  // investigate.
  //
  // TODO(user): The code below is buggy when more than kMaxFlowQuantity can be
  // pushed out of the source (i.e. when we loop more than once in the while()).
  // This is not critical, since this code is not used in the default algorithm
  // computation. The issue is twofold:
  // - InitializeActiveNodeContainer() doesn't push the nodes in
  //   the correct order.
  // - PushFlowExcessBackToSource() may break the node potential properties, and
  //   we will need a call to GlobalUpdate() to fix that.
  while (SaturateOutgoingArcsFromSource()) {
    DCHECK(IsEmptyActiveNodeContainer());
    InitializeActiveNodeContainer();
    while (!IsEmptyActiveNodeContainer()) {
      const NodeIndex node = GetAndRemoveFirstActiveNode();
      if (node == source_ || node == sink_) continue;
      Discharge(node);
    }
    if (use_two_phase_algorithm_) {
      PushFlowExcessBackToSource();
    }
  }
}

template <typename Graph>
void GenericMaxFlow<Graph>::RefineWithGlobalUpdate() {
  SCOPED_TIME_STAT(&stats_);

  // TODO(user): This should be graph_->num_nodes(), but ebert graph does not
  // have a correct size if the highest index nodes have no arcs.
  const NodeIndex num_nodes = Graphs<Graph>::NodeReservation(*graph_);
  std::vector<int> skip_active_node;

  while (SaturateOutgoingArcsFromSource()) {
    int num_skipped;
    do {
      num_skipped = 0;
      skip_active_node.assign(num_nodes, 0);
      skip_active_node[sink_] = 2;
      skip_active_node[source_] = 2;
      GlobalUpdate();
      while (!IsEmptyActiveNodeContainer()) {
        const NodeIndex node = GetAndRemoveFirstActiveNode();
        if (skip_active_node[node] > 1) {
          if (node != sink_ && node != source_) ++num_skipped;
          continue;
        }
        const NodeIndex old_height = node_potential_[node];
        Discharge(node);

        // The idea behind this is that if a node height augments by more than
        // one, then it is likely to push flow back the way it came. This can
        // lead to very costly loops. A bad case is: source -> n1 -> n2 and n2
        // just recently isolated from the sink. Then n2 will push flow back to
        // n1, and n1 to n2 and so on. The height of each node will increase by
        // steps of two until the height of the source is reached, which can
        // take a long time. If the chain is longer, the situation is even
        // worse. The behavior of this heuristic is related to the Gap
        // heuristic.
        //
        // Note that the global update will fix all such cases efficently. So
        // the idea is to discharge the active node as much as possible, and
        // then do a global update.
        //
        // We skip a node when this condition was true 2 times to avoid doing a
        // global update too frequently.
        if (node_potential_[node] > old_height + 1) {
          ++skip_active_node[node];
        }
      }
    } while (num_skipped > 0);
    if (use_two_phase_algorithm_) {
      PushFlowExcessBackToSource();
    }
  }
}

template <typename Graph>
void GenericMaxFlow<Graph>::Discharge(NodeIndex node) {
  SCOPED_TIME_STAT(&stats_);
  const NodeIndex num_nodes = graph_->num_nodes();
  while (true) {
    DCHECK(IsActive(node));
    for (OutgoingOrOppositeIncomingArcIterator it(*graph_, node,
                                                  first_admissible_arc_[node]);
         it.Ok(); it.Next()) {
      const ArcIndex arc = it.Index();
      if (IsAdmissible(arc)) {
        DCHECK(IsActive(node));
        const NodeIndex head = Head(arc);
        if (node_excess_[head] == 0) {
          // The push below will make the node active for sure. Note that we may
          // push the sink_, but that is handled properly in Refine().
          PushActiveNode(head);
        }
        const FlowQuantity delta =
            std::min(node_excess_[node], residual_arc_capacity_[arc]);
        PushFlow(delta, arc);
        if (node_excess_[node] == 0) {
          first_admissible_arc_[node] = arc;  // arc may still be admissible.
          return;
        }
      }
    }
    Relabel(node);
    if (use_two_phase_algorithm_ && node_potential_[node] >= num_nodes) break;
  }
}

template <typename Graph>
void GenericMaxFlow<Graph>::Relabel(NodeIndex node) {
  SCOPED_TIME_STAT(&stats_);
  // Because we use a relaxed version, this is no longer true if the
  // first_admissible_arc_[node] was not actually the first arc!
  // DCHECK(CheckRelabelPrecondition(node));
  NodeHeight min_height = std::numeric_limits<NodeHeight>::max();
  ArcIndex first_admissible_arc = Graph::kNilArc;
  for (OutgoingOrOppositeIncomingArcIterator it(*graph_, node); it.Ok();
       it.Next()) {
    const ArcIndex arc = it.Index();
    if (residual_arc_capacity_[arc] > 0) {
      // Update min_height only for arcs with available capacity.
      NodeHeight head_height = node_potential_[Head(arc)];
      if (head_height < min_height) {
        min_height = head_height;
        first_admissible_arc = arc;

        // We found an admissible arc at the current height, just stop there.
        // This is the true first_admissible_arc_[node].
        if (min_height + 1 == node_potential_[node]) break;
      }
    }
  }
  DCHECK_NE(first_admissible_arc, Graph::kNilArc);
  node_potential_[node] = min_height + 1;

  // Note that after a Relabel(), the loop will continue in Discharge(), and
  // we are sure that all the arcs before first_admissible_arc are not
  // admissible since their height is > min_height.
  first_admissible_arc_[node] = first_admissible_arc;
}

template <typename Graph>
typename Graph::ArcIndex GenericMaxFlow<Graph>::Opposite(ArcIndex arc) const {
  return Graphs<Graph>::OppositeArc(*graph_, arc);
}

template <typename Graph>
bool GenericMaxFlow<Graph>::IsArcDirect(ArcIndex arc) const {
  return IsArcValid(arc) && arc >= 0;
}

template <typename Graph>
bool GenericMaxFlow<Graph>::IsArcValid(ArcIndex arc) const {
  return Graphs<Graph>::IsArcValid(*graph_, arc);
}

template <typename Graph>
const FlowQuantity GenericMaxFlow<Graph>::kMaxFlowQuantity =
    std::numeric_limits<FlowQuantity>::max();

template <typename Graph>
template <bool reverse>
void GenericMaxFlow<Graph>::ComputeReachableNodes(
    NodeIndex start, std::vector<NodeIndex>* result) {
  // If start is not a valid node index, it can reach only itself.
  // Note(user): This is needed because source and sink are given independently
  // of the graph and sometimes before it is even constructed.
  const NodeIndex num_nodes = graph_->num_nodes();
  if (start >= num_nodes) {
    result->clear();
    result->push_back(start);
    return;
  }
  bfs_queue_.clear();
  node_in_bfs_queue_.assign(num_nodes, false);

  int queue_index = 0;
  bfs_queue_.push_back(start);
  node_in_bfs_queue_[start] = true;
  while (queue_index != bfs_queue_.size()) {
    const NodeIndex node = bfs_queue_[queue_index];
    ++queue_index;
    for (OutgoingOrOppositeIncomingArcIterator it(*graph_, node); it.Ok();
         it.Next()) {
      const ArcIndex arc = it.Index();
      const NodeIndex head = Head(arc);
      if (node_in_bfs_queue_[head]) continue;
      if (residual_arc_capacity_[reverse ? Opposite(arc) : arc] == 0) continue;
      node_in_bfs_queue_[head] = true;
      bfs_queue_.push_back(head);
    }
  }
  *result = bfs_queue_;
}

template <typename Graph>
FlowModel GenericMaxFlow<Graph>::CreateFlowModel() {
  FlowModel model;
  model.set_problem_type(FlowModel::MAX_FLOW);
  for (int n = 0; n < graph_->num_nodes(); ++n) {
    Node* node = model.add_node();
    node->set_id(n);
    if (n == source_) node->set_supply(1);
    if (n == sink_) node->set_supply(-1);
  }
  for (int a = 0; a < graph_->num_arcs(); ++a) {
    Arc* arc = model.add_arc();
    arc->set_tail_node_id(graph_->Tail(a));
    arc->set_head_node_id(graph_->Head(a));
    arc->set_capacity(Capacity(a));
  }
  return model;
}

// Explicit instanciations that can be used by a client.
//
// TODO(user): moves this code out of a .cc file and include it at the end of
// the header so it can work with any graph implementation ?
template class GenericMaxFlow<StarGraph>;
template class GenericMaxFlow<ReverseArcListGraph<> >;
template class GenericMaxFlow<ReverseArcStaticGraph<> >;
template class GenericMaxFlow<ReverseArcMixedGraph<> >;

}  // namespace operations_research

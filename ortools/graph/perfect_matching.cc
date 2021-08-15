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

#include "ortools/graph/perfect_matching.h"

#include <cstdint>
#include <limits>

#include "absl/memory/memory.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {

void MinCostPerfectMatching::Reset(int num_nodes) {
  graph_ = absl::make_unique<BlossomGraph>(num_nodes);
  optimal_cost_ = 0;
  matches_.assign(num_nodes, -1);
}

void MinCostPerfectMatching::AddEdgeWithCost(int tail, int head, int64_t cost) {
  CHECK_GE(cost, 0) << "Not supported for now, just shift your costs.";
  if (tail == head) {
    VLOG(1) << "Ignoring self-arc: " << tail << " <-> " << head
            << " cost: " << cost;
    return;
  }
  maximum_edge_cost_ = std::max(maximum_edge_cost_, cost);
  graph_->AddEdge(BlossomGraph::NodeIndex(tail), BlossomGraph::NodeIndex(head),
                  BlossomGraph::CostValue(cost));
}

MinCostPerfectMatching::Status MinCostPerfectMatching::Solve() {
  optimal_solution_found_ = false;

  // We want all dual and all slack value to never overflow. After Initialize()
  // they are both bounded by the 2 * maximum cost. And we track an upper bound
  // on these quantities. The factor two is because of the re-scaling we do
  // internally since all our dual values are actually multiple of 1/2.
  //
  // Note that since the whole code in BlossomGraph assumes that dual/slack have
  // a magnitude that is always lower than kMaxCostValue it is important to use
  // it here since there is no reason it cannot be smaller than kint64max.
  //
  // TODO(user): Improve the overflow detection if needed. The current one seems
  // ok though.
  int64_t overflow_detection = CapAdd(maximum_edge_cost_, maximum_edge_cost_);
  if (overflow_detection >= BlossomGraph::kMaxCostValue) {
    return Status::INTEGER_OVERFLOW;
  }

  const int num_nodes = matches_.size();
  if (!graph_->Initialize()) return Status::INFEASIBLE;
  VLOG(2) << graph_->DebugString();
  VLOG(1) << "num_unmatched: " << num_nodes - graph_->NumMatched()
          << " dual_objective: " << graph_->DualObjective();

  while (graph_->NumMatched() != num_nodes) {
    graph_->PrimalUpdates();
    if (DEBUG_MODE) {
      graph_->DebugCheckNoPossiblePrimalUpdates();
    }

    VLOG(1) << "num_unmatched: " << num_nodes - graph_->NumMatched()
            << " dual_objective: " << graph_->DualObjective();
    if (graph_->NumMatched() == num_nodes) break;

    const BlossomGraph::CostValue delta =
        graph_->ComputeMaxCommonTreeDualDeltaAndResetPrimalEdgeQueue();
    overflow_detection = CapAdd(overflow_detection, std::abs(delta.value()));
    if (overflow_detection >= BlossomGraph::kMaxCostValue) {
      return Status::INTEGER_OVERFLOW;
    }

    if (delta == 0) break;  // Infeasible!
    graph_->UpdateAllTrees(delta);
  }

  VLOG(1) << "End: " << graph_->NumMatched() << " / " << num_nodes;
  graph_->DisplayStats();
  if (graph_->NumMatched() < num_nodes) {
    return Status::INFEASIBLE;
  }
  VLOG(2) << graph_->DebugString();
  CHECK(graph_->DebugDualsAreFeasible());

  // TODO(user): Maybe there is a faster/better way to recover the mapping
  // in the presence of blossoms.
  graph_->ExpandAllBlossoms();
  for (int i = 0; i < num_nodes; ++i) {
    matches_[i] = graph_->Match(BlossomGraph::NodeIndex(i)).value();
  }

  optimal_solution_found_ = true;
  optimal_cost_ = graph_->DualObjective().value();
  if (optimal_cost_ == std::numeric_limits<int64_t>::max())
    return Status::COST_OVERFLOW;
  return Status::OPTIMAL;
}

using NodeIndex = BlossomGraph::NodeIndex;
using CostValue = BlossomGraph::CostValue;

const BlossomGraph::NodeIndex BlossomGraph::kNoNodeIndex =
    BlossomGraph::NodeIndex(-1);
const BlossomGraph::EdgeIndex BlossomGraph::kNoEdgeIndex =
    BlossomGraph::EdgeIndex(-1);
const BlossomGraph::CostValue BlossomGraph::kMaxCostValue =
    BlossomGraph::CostValue(std::numeric_limits<int64_t>::max());

BlossomGraph::BlossomGraph(int num_nodes) {
  graph_.resize(num_nodes);
  nodes_.reserve(num_nodes);
  root_blossom_node_.resize(num_nodes);
  for (NodeIndex n(0); n < num_nodes; ++n) {
    root_blossom_node_[n] = n;
    nodes_.push_back(Node(n));
  }
}

void BlossomGraph::AddEdge(NodeIndex tail, NodeIndex head, CostValue cost) {
  DCHECK_GE(tail, 0);
  DCHECK_LT(tail, nodes_.size());
  DCHECK_GE(head, 0);
  DCHECK_LT(head, nodes_.size());
  DCHECK_GE(cost, 0);
  DCHECK(!is_initialized_);
  const EdgeIndex index(edges_.size());
  edges_.push_back(Edge(tail, head, cost));
  graph_[tail].push_back(index);
  graph_[head].push_back(index);
}

// TODO(user): Code the more advanced "Fractional matching initialization"
// heuristic.
//
// TODO(user): Add a preprocessing step that performs the 'forced' matches?
bool BlossomGraph::Initialize() {
  CHECK(!is_initialized_);
  is_initialized_ = true;

  for (NodeIndex n(0); n < nodes_.size(); ++n) {
    if (graph_[n].empty()) return false;  // INFEASIBLE.
    CostValue min_cost = kMaxCostValue;

    // Initialize the dual of each nodes to min_cost / 2.
    //
    // TODO(user): We might be able to do better for odd min_cost, but then
    // we might need to scale by 4? think about it.
    for (const EdgeIndex e : graph_[n]) {
      min_cost = std::min(min_cost, edges_[e].pseudo_slack);
    }
    DCHECK_NE(min_cost, kMaxCostValue);
    nodes_[n].pseudo_dual = min_cost / 2;

    // Starts with all nodes as tree roots.
    nodes_[n].type = 1;
  }

  // Update the slack of each edges now that nodes might have non-zero duals.
  // Note that we made sure that all updated slacks are non-negative.
  for (EdgeIndex e(0); e < edges_.size(); ++e) {
    Edge& mutable_edge = edges_[e];
    mutable_edge.pseudo_slack -= nodes_[mutable_edge.tail].pseudo_dual +
                                 nodes_[mutable_edge.head].pseudo_dual;
    DCHECK_GE(mutable_edge.pseudo_slack, 0);
  }

  for (NodeIndex n(0); n < nodes_.size(); ++n) {
    if (NodeIsMatched(n)) continue;

    // After this greedy update, there will be at least an edge with a
    // slack of zero.
    CostValue min_slack = kMaxCostValue;
    for (const EdgeIndex e : graph_[n]) {
      min_slack = std::min(min_slack, edges_[e].pseudo_slack);
    }
    DCHECK_NE(min_slack, kMaxCostValue);
    if (min_slack > 0) {
      nodes_[n].pseudo_dual += min_slack;
      for (const EdgeIndex e : graph_[n]) {
        edges_[e].pseudo_slack -= min_slack;
      }
      DebugUpdateNodeDual(n, min_slack);
    }

    // Match this node if possible.
    //
    // TODO(user): Optimize by merging this loop with the one above?
    for (const EdgeIndex e : graph_[n]) {
      const Edge& edge = edges_[e];
      if (edge.pseudo_slack != 0) continue;
      if (!NodeIsMatched(edge.OtherEnd(n))) {
        nodes_[edge.tail].type = 0;
        nodes_[edge.tail].match = edge.head;
        nodes_[edge.head].type = 0;
        nodes_[edge.head].match = edge.tail;
        break;
      }
    }
  }

  // Initialize unmatched_nodes_.
  for (NodeIndex n(0); n < nodes_.size(); ++n) {
    if (NodeIsMatched(n)) continue;
    unmatched_nodes_.push_back(n);
  }

  // Scale everything by 2 and update the dual cost. Note that we made sure that
  // there cannot be an integer overflow at the beginning of Solve().
  //
  // This scaling allows to only have integer weights during the algorithm
  // because the slack of [+] -- [+] edges will always stay even.
  //
  // TODO(user): Reduce the number of loops we do in the initialization. We
  // could likely just scale the edge cost as we fill them.
  for (NodeIndex n(0); n < nodes_.size(); ++n) {
    DCHECK_LE(nodes_[n].pseudo_dual, kMaxCostValue / 2);
    nodes_[n].pseudo_dual *= 2;
    AddToDualObjective(nodes_[n].pseudo_dual);
#ifndef NDEBUG
    nodes_[n].dual = nodes_[n].pseudo_dual;
#endif
  }
  for (EdgeIndex e(0); e < edges_.size(); ++e) {
    DCHECK_LE(edges_[e].pseudo_slack, kMaxCostValue / 2);
    edges_[e].pseudo_slack *= 2;
#ifndef NDEBUG
    edges_[e].slack = edges_[e].pseudo_slack;
#endif
  }

  // Initialize the edge priority queues and the primal update queue.
  // We only need to do that if we have unmatched nodes.
  if (!unmatched_nodes_.empty()) {
    primal_update_edge_queue_.clear();
    for (EdgeIndex e(0); e < edges_.size(); ++e) {
      Edge& edge = edges_[e];
      const bool tail_is_plus = nodes_[edge.tail].IsPlus();
      const bool head_is_plus = nodes_[edge.head].IsPlus();
      if (tail_is_plus && head_is_plus) {
        plus_plus_pq_.Add(&edge);
        if (edge.pseudo_slack == 0) primal_update_edge_queue_.push_back(e);
      } else if (tail_is_plus || head_is_plus) {
        plus_free_pq_.Add(&edge);
        if (edge.pseudo_slack == 0) primal_update_edge_queue_.push_back(e);
      }
    }
  }

  return true;
}

CostValue BlossomGraph::ComputeMaxCommonTreeDualDeltaAndResetPrimalEdgeQueue() {
  // TODO(user): Avoid this linear loop.
  CostValue best_update = kMaxCostValue;
  for (NodeIndex n(0); n < nodes_.size(); ++n) {
    const Node& node = nodes_[n];
    if (node.IsBlossom() && node.IsMinus()) {
      best_update = std::min(best_update, Dual(node));
    }
  }

  // This code only works because all tree_dual_delta are the same.
  CHECK(!unmatched_nodes_.empty());
  const CostValue tree_delta = nodes_[unmatched_nodes_.front()].tree_dual_delta;
  CostValue plus_plus_slack = kMaxCostValue;
  if (!plus_plus_pq_.IsEmpty()) {
    DCHECK_EQ(plus_plus_pq_.Top()->pseudo_slack % 2, 0) << "Non integer bound!";
    plus_plus_slack = plus_plus_pq_.Top()->pseudo_slack / 2 - tree_delta;
    best_update = std::min(best_update, plus_plus_slack);
  }
  CostValue plus_free_slack = kMaxCostValue;
  if (!plus_free_pq_.IsEmpty()) {
    plus_free_slack = plus_free_pq_.Top()->pseudo_slack - tree_delta;
    best_update = std::min(best_update, plus_free_slack);
  }

  // This means infeasible, and returning zero will abort the search.
  if (best_update == kMaxCostValue) return CostValue(0);

  // Initialize primal_update_edge_queue_ with all the edges that will have a
  // slack of zero once we apply the update.
  //
  // NOTE(user): If we want more "determinism" and be independent on the PQ
  // algorithm, we could std::sort() the primal_update_edge_queue_ here.
  primal_update_edge_queue_.clear();
  if (plus_plus_slack == best_update) {
    plus_plus_pq_.AllTop(&tmp_all_tops_);
    for (const Edge* pt : tmp_all_tops_) {
      primal_update_edge_queue_.push_back(EdgeIndex(pt - &edges_.front()));
    }
  }
  if (plus_free_slack == best_update) {
    plus_free_pq_.AllTop(&tmp_all_tops_);
    for (const Edge* pt : tmp_all_tops_) {
      primal_update_edge_queue_.push_back(EdgeIndex(pt - &edges_.front()));
    }
  }

  return best_update;
}

void BlossomGraph::UpdateAllTrees(CostValue delta) {
  ++num_dual_updates_;

  // Reminder: the tree roots are exactly the unmatched nodes.
  CHECK_GE(delta, 0);
  for (const NodeIndex n : unmatched_nodes_) {
    CHECK(!NodeIsMatched(n));
    AddToDualObjective(delta);
    nodes_[n].tree_dual_delta += delta;
  }

  if (DEBUG_MODE) {
    for (NodeIndex n(0); n < nodes_.size(); ++n) {
      const Node& node = nodes_[n];
      if (node.IsPlus()) DebugUpdateNodeDual(n, delta);
      if (node.IsMinus()) DebugUpdateNodeDual(n, -delta);
    }
  }
}

bool BlossomGraph::NodeIsMatched(NodeIndex n) const {
  // An unmatched node must be a tree root.
  const Node& node = nodes_[n];
  CHECK(node.match != n || (node.root == n && node.IsPlus()));
  return node.match != n;
}

NodeIndex BlossomGraph::Match(NodeIndex n) const {
  const Node& node = nodes_[n];
  if (DEBUG_MODE) {
    if (node.IsMinus()) CHECK_EQ(node.parent, node.match);
    if (node.IsPlus()) CHECK_EQ(n, node.match);
  }
  return node.match;
}

// Meant to only be used in DEBUG to make sure our queue in PrimalUpdates()
// do not miss any potential edges.
void BlossomGraph::DebugCheckNoPossiblePrimalUpdates() {
  for (EdgeIndex e(0); e < edges_.size(); ++e) {
    const Edge& edge = edges_[e];
    if (Head(edge) == Tail(edge)) continue;

    CHECK(!nodes_[Tail(edge)].is_internal);
    CHECK(!nodes_[Head(edge)].is_internal);
    if (Slack(edge) != 0) continue;

    // Make sure tail is a plus node if possible.
    NodeIndex tail = Tail(edge);
    NodeIndex head = Head(edge);
    if (!nodes_[tail].IsPlus()) std::swap(tail, head);
    if (!nodes_[tail].IsPlus()) continue;

    if (nodes_[head].IsFree()) {
      VLOG(2) << DebugString();
      LOG(FATAL) << "Possible Grow! " << tail << " " << head;
    }
    if (nodes_[head].IsPlus()) {
      if (nodes_[tail].root == nodes_[head].root) {
        LOG(FATAL) << "Possible Shrink!";
      } else {
        LOG(FATAL) << "Possible augment!";
      }
    }
  }
  for (const Node& node : nodes_) {
    if (node.IsMinus() && node.IsBlossom() && Dual(node) == 0) {
      LOG(FATAL) << "Possible expand!";
    }
  }
}

void BlossomGraph::PrimalUpdates() {
  // Any Grow/Augment/Shrink/Expand operation can add new tight edges that need
  // to be explored again.
  //
  // TODO(user): avoid adding duplicates?
  while (true) {
    possible_shrink_.clear();

    // First, we Grow/Augment as much as possible.
    while (!primal_update_edge_queue_.empty()) {
      const EdgeIndex e = primal_update_edge_queue_.back();
      primal_update_edge_queue_.pop_back();

      // Because of the Expand() operation, the edge may have become un-tight
      // since it has been inserted in the tight edges queue. It's cheaper to
      // detect it here and skip it than it would be to dynamically update the
      // queue to only keep actually tight edges at all times.
      const Edge& edge = edges_[e];
      if (Slack(edge) != 0) continue;

      NodeIndex tail = Tail(edge);
      NodeIndex head = Head(edge);
      if (!nodes_[tail].IsPlus()) std::swap(tail, head);
      if (!nodes_[tail].IsPlus()) continue;

      if (nodes_[head].IsFree()) {
        Grow(e, tail, head);
      } else if (nodes_[head].IsPlus()) {
        if (nodes_[tail].root != nodes_[head].root) {
          Augment(e);
        } else {
          possible_shrink_.push_back(e);
        }
      }
    }

    // Shrink all potential Blossom.
    for (const EdgeIndex e : possible_shrink_) {
      const Edge& edge = edges_[e];
      const NodeIndex tail = Tail(edge);
      const NodeIndex head = Head(edge);
      const Node& tail_node = nodes_[tail];
      const Node& head_node = nodes_[head];
      if (tail_node.IsPlus() && head_node.IsPlus() &&
          tail_node.root == head_node.root && tail != head) {
        Shrink(e);
      }
    }

    // Delay expand if any blossom was created.
    if (!primal_update_edge_queue_.empty()) continue;

    // Expand Blossom if any.
    //
    // TODO(user): Avoid doing a O(num_nodes). Also expand all blossom
    // recursively? I am not sure it is a good heuristic to expand all possible
    // blossom before trying the other operations though.
    int num_expands = 0;
    for (NodeIndex n(0); n < nodes_.size(); ++n) {
      const Node& node = nodes_[n];
      if (node.IsMinus() && node.IsBlossom() && Dual(node) == 0) {
        ++num_expands;
        Expand(n);
      }
    }
    if (num_expands == 0) break;
  }
}

bool BlossomGraph::DebugDualsAreFeasible() const {
  // The slack of all edge must be non-negative.
  for (const Edge& edge : edges_) {
    if (Slack(edge) < 0) return false;
  }

  // The dual of all Blossom must be non-negative.
  for (const Node& node : nodes_) {
    if (node.IsBlossom() && Dual(node) < 0) return false;
  }
  return true;
}

bool BlossomGraph::DebugEdgeIsTightAndExternal(const Edge& edge) const {
  if (Tail(edge) == Head(edge)) return false;
  if (nodes_[Tail(edge)].IsInternal()) return false;
  if (nodes_[Head(edge)].IsInternal()) return false;
  return Slack(edge) == 0;
}

void BlossomGraph::Grow(EdgeIndex e, NodeIndex tail, NodeIndex head) {
  ++num_grows_;
  VLOG(2) << "Grow " << tail << " -> " << head << " === " << Match(head);

  DCHECK(DebugEdgeIsTightAndExternal(edges_[e]));
  DCHECK(nodes_[tail].IsPlus());
  DCHECK(nodes_[head].IsFree());
  DCHECK(NodeIsMatched(head));

  const NodeIndex root = nodes_[tail].root;
  const NodeIndex leaf = Match(head);

  Node& head_node = nodes_[head];
  head_node.root = root;
  head_node.parent = tail;
  head_node.type = -1;

  // head was free and is now a [-] node.
  const CostValue tree_dual = nodes_[root].tree_dual_delta;
  head_node.pseudo_dual += tree_dual;
  for (const NodeIndex subnode : SubNodes(head)) {
    for (const EdgeIndex e : graph_[subnode]) {
      Edge& edge = edges_[e];
      const NodeIndex other_end = OtherEnd(edge, subnode);
      if (other_end == head) continue;
      edge.pseudo_slack -= tree_dual;
      if (plus_free_pq_.Contains(&edge)) plus_free_pq_.Remove(&edge);
    }
  }

  Node& leaf_node = nodes_[leaf];
  leaf_node.root = root;
  leaf_node.parent = head;
  leaf_node.type = +1;

  // leaf was free and is now a [+] node.
  leaf_node.pseudo_dual -= tree_dual;
  for (const NodeIndex subnode : SubNodes(leaf)) {
    for (const EdgeIndex e : graph_[subnode]) {
      Edge& edge = edges_[e];
      const NodeIndex other_end = OtherEnd(edge, subnode);
      if (other_end == leaf) continue;
      edge.pseudo_slack += tree_dual;
      const Node& other_node = nodes_[other_end];
      if (other_node.IsPlus()) {
        // The edge switch from [+] -- [0] to [+] -- [+].
        DCHECK(plus_free_pq_.Contains(&edge));
        DCHECK(!plus_plus_pq_.Contains(&edge));
        plus_free_pq_.Remove(&edge);
        plus_plus_pq_.Add(&edge);
        if (edge.pseudo_slack == 2 * tree_dual) {
          DCHECK_EQ(Slack(edge), 0);
          primal_update_edge_queue_.push_back(e);
        }
      } else if (other_node.IsFree()) {
        // We have a new [+] -- [0] edge.
        DCHECK(!plus_free_pq_.Contains(&edge));
        DCHECK(!plus_plus_pq_.Contains(&edge));
        plus_free_pq_.Add(&edge);
        if (edge.pseudo_slack == tree_dual) {
          DCHECK_EQ(Slack(edge), 0);
          primal_update_edge_queue_.push_back(e);
        }
      }
    }
  }
}

void BlossomGraph::AppendNodePathToRoot(NodeIndex n,
                                        std::vector<NodeIndex>* path) const {
  while (true) {
    path->push_back(n);
    n = nodes_[n].parent;
    if (n == path->back()) break;
  }
}

void BlossomGraph::Augment(EdgeIndex e) {
  ++num_augments_;

  const Edge& edge = edges_[e];
  VLOG(2) << "Augment " << Tail(edge) << " -> " << Head(edge);
  DCHECK(DebugEdgeIsTightAndExternal(edge));
  DCHECK(nodes_[Tail(edge)].IsPlus());
  DCHECK(nodes_[Head(edge)].IsPlus());

  const NodeIndex root_a = nodes_[Tail(edge)].root;
  const NodeIndex root_b = nodes_[Head(edge)].root;
  DCHECK_NE(root_a, root_b);

  // Compute the path from root_a to root_b.
  std::vector<NodeIndex> node_path;
  AppendNodePathToRoot(Tail(edge), &node_path);
  std::reverse(node_path.begin(), node_path.end());
  AppendNodePathToRoot(Head(edge), &node_path);

  // TODO(user): Check all dual/slack same after primal op?
  const CostValue delta_a = nodes_[root_a].tree_dual_delta;
  const CostValue delta_b = nodes_[root_b].tree_dual_delta;
  nodes_[root_a].tree_dual_delta = 0;
  nodes_[root_b].tree_dual_delta = 0;

  // Make all the nodes from both trees free while keeping the
  // current matching.
  //
  // TODO(user): It seems that we may waste some computation since the part of
  // the tree not in the path between roots can lead to the same Grow()
  // operations later when one of its node is ratched to a new root.
  //
  // TODO(user): Reduce this O(num_nodes) complexity. We might be able to
  // even do O(num_node_in_path) with lazy updates. Note that this operation
  // will only be performed at most num_initial_unmatched_nodes / 2 times
  // though.
  for (NodeIndex n(0); n < nodes_.size(); ++n) {
    Node& node = nodes_[n];
    if (node.IsInternal()) continue;
    const NodeIndex root = node.root;
    if (root != root_a && root != root_b) continue;

    const CostValue delta = node.type * (root == root_a ? delta_a : delta_b);
    node.pseudo_dual += delta;
    for (const NodeIndex subnode : SubNodes(n)) {
      for (const EdgeIndex e : graph_[subnode]) {
        Edge& edge = edges_[e];
        const NodeIndex other_end = OtherEnd(edge, subnode);
        if (other_end == n) continue;
        edge.pseudo_slack -= delta;

        // If the other end is not in one of the two trees, and it is a plus
        // node, we add it the plus_free queue. All previous [+]--[0] and
        // [+]--[+] edges need to be removed from the queues.
        const Node& other_node = nodes_[other_end];
        if (other_node.root != root_a && other_node.root != root_b &&
            other_node.IsPlus()) {
          if (plus_plus_pq_.Contains(&edge)) plus_plus_pq_.Remove(&edge);
          DCHECK(!plus_free_pq_.Contains(&edge));
          plus_free_pq_.Add(&edge);
          if (Slack(edge) == 0) primal_update_edge_queue_.push_back(e);
        } else {
          if (plus_plus_pq_.Contains(&edge)) plus_plus_pq_.Remove(&edge);
          if (plus_free_pq_.Contains(&edge)) plus_free_pq_.Remove(&edge);
        }
      }
    }

    node.type = 0;
    node.parent = node.root = n;
  }

  // Change the matching of nodes along node_path.
  CHECK_EQ(node_path.size() % 2, 0);
  for (int i = 0; i < node_path.size(); i += 2) {
    nodes_[node_path[i]].match = node_path[i + 1];
    nodes_[node_path[i + 1]].match = node_path[i];
  }

  // Update unmatched_nodes_.
  //
  // TODO(user): This could probably be optimized if needed. But we do usually
  // iterate a lot more over it than we update it. Note that as long as we use
  // the same delta for all trees, this is not even needed.
  int new_size = 0;
  for (const NodeIndex n : unmatched_nodes_) {
    if (!NodeIsMatched(n)) unmatched_nodes_[new_size++] = n;
  }
  CHECK_EQ(unmatched_nodes_.size(), new_size + 2);
  unmatched_nodes_.resize(new_size);
}

int BlossomGraph::GetDepth(NodeIndex n) const {
  int depth = 0;
  while (true) {
    const NodeIndex parent = nodes_[n].parent;
    if (parent == n) break;
    ++depth;
    n = parent;
  }
  return depth;
}

void BlossomGraph::Shrink(EdgeIndex e) {
  ++num_shrinks_;

  const Edge& edge = edges_[e];
  DCHECK(DebugEdgeIsTightAndExternal(edge));
  DCHECK(nodes_[Tail(edge)].IsPlus());
  DCHECK(nodes_[Head(edge)].IsPlus());
  DCHECK_EQ(nodes_[Tail(edge)].root, nodes_[Head(edge)].root);

  CHECK_NE(Tail(edge), Head(edge)) << e;

  // Find lowest common ancestor and the two node paths to reach it. Note that
  // we do not add it to the paths.
  NodeIndex lca_index = kNoNodeIndex;
  std::vector<NodeIndex> tail_path;
  std::vector<NodeIndex> head_path;
  {
    NodeIndex tail = Tail(edge);
    NodeIndex head = Head(edge);
    int tail_depth = GetDepth(tail);
    int head_depth = GetDepth(head);
    if (tail_depth > head_depth) {
      std::swap(tail, head);
      std::swap(tail_depth, head_depth);
    }
    VLOG(2) << "Shrink " << tail << " <-> " << head;

    while (head_depth > tail_depth) {
      head_path.push_back(head);
      head = nodes_[head].parent;
      --head_depth;
    }
    while (tail != head) {
      DCHECK_EQ(tail_depth, head_depth);
      DCHECK_GE(tail_depth, 0);
      if (DEBUG_MODE) {
        --tail_depth;
        --head_depth;
      }

      tail_path.push_back(tail);
      tail = nodes_[tail].parent;

      head_path.push_back(head);
      head = nodes_[head].parent;
    }
    lca_index = tail;
    VLOG(2) << "LCA " << lca_index;
  }
  Node& lca = nodes_[lca_index];
  DCHECK(lca.IsPlus());

  // Fill the cycle.
  std::vector<NodeIndex> blossom = {lca_index};
  std::reverse(head_path.begin(), head_path.end());
  blossom.insert(blossom.end(), head_path.begin(), head_path.end());
  blossom.insert(blossom.end(), tail_path.begin(), tail_path.end());
  CHECK_EQ(blossom.size() % 2, 1);

  const CostValue tree_dual = nodes_[lca.root].tree_dual_delta;

  // Save all values that will be needed if we expand this Blossom later.
  CHECK_GT(blossom.size(), 1);
  Node& backup_node = nodes_[blossom[1]];
#ifndef NDEBUG
  backup_node.saved_dual = lca.dual;
#endif
  backup_node.saved_pseudo_dual = lca.pseudo_dual + tree_dual;

  // Set the new dual of the node to zero.
#ifndef NDEBUG
  lca.dual = 0;
#endif
  lca.pseudo_dual = -tree_dual;
  CHECK_EQ(Dual(lca), 0);

  // Mark node as internal, but do not change their type to zero yet.
  // We need to do that first to properly detect edges between two internal
  // nodes in the second loop below.
  for (const NodeIndex n : blossom) {
    VLOG(2) << "blossom-node: " << NodeDebugString(n);
    if (n != lca_index) {
      nodes_[n].is_internal = true;
    }
  }

  // Update the dual of all edges and the priority queueus.
  for (const NodeIndex n : blossom) {
    Node& mutable_node = nodes_[n];
    const bool was_minus = mutable_node.IsMinus();
    const CostValue slack_adjust =
        mutable_node.IsMinus() ? tree_dual : -tree_dual;
    if (n != lca_index) {
      mutable_node.pseudo_dual -= slack_adjust;
#ifndef NDEBUG
      DCHECK_EQ(mutable_node.dual, mutable_node.pseudo_dual);
#endif
      mutable_node.type = 0;
    }
    for (const NodeIndex subnode : SubNodes(n)) {
      // Subtle: We update root_blossom_node_ while we loop, so for new internal
      // edges, depending if an edge "other end" appear after or before, it will
      // not be updated. We use this to only process internal edges once.
      root_blossom_node_[subnode] = lca_index;

      for (const EdgeIndex e : graph_[subnode]) {
        Edge& edge = edges_[e];
        const NodeIndex other_end = OtherEnd(edge, subnode);

        // Skip edge that are already internal.
        if (other_end == n) continue;

        // This internal edge was already processed from its other end, so we
        // can just skip it.
        if (other_end == lca_index) {
#ifndef NDEBUG
          DCHECK_EQ(edge.slack, Slack(edge));
#endif
          continue;
        }

        // This is a new-internal edge that we didn't process yet.
        //
        // TODO(user): It would be nicer to not to have to read the memory of
        // the other node at all. It might be possible once we store the
        // parent edge instead of the parent node since then we will only need
        // to know if this edges point to a new-internal node or not.
        Node& mutable_other_node = nodes_[other_end];
        if (mutable_other_node.is_internal) {
          DCHECK(!plus_free_pq_.Contains(&edge));
          if (plus_plus_pq_.Contains(&edge)) plus_plus_pq_.Remove(&edge);
          edge.pseudo_slack += slack_adjust;
          edge.pseudo_slack +=
              mutable_other_node.IsMinus() ? tree_dual : -tree_dual;
          continue;
        }

        // Replace the parent of any child of n by lca_index.
        if (mutable_other_node.parent == n) {
          mutable_other_node.parent = lca_index;
        }

        // Adjust when the edge used to be connected to a [-] node now that we
        // attach it to a [+] node. Note that if the node was [+] then the
        // non-internal incident edges slack and type do not change.
        if (was_minus) {
          edge.pseudo_slack += 2 * tree_dual;

          // Add it to the correct PQ.
          DCHECK(!plus_plus_pq_.Contains(&edge));
          DCHECK(!plus_free_pq_.Contains(&edge));
          if (mutable_other_node.IsPlus()) {
            plus_plus_pq_.Add(&edge);
            if (edge.pseudo_slack == 2 * tree_dual) {
              primal_update_edge_queue_.push_back(e);
            }
          } else if (mutable_other_node.IsFree()) {
            plus_free_pq_.Add(&edge);
            if (edge.pseudo_slack == tree_dual) {
              primal_update_edge_queue_.push_back(e);
            }
          }
        }

#ifndef NDEBUG
        DCHECK_EQ(edge.slack, Slack(edge));
#endif
      }
    }
  }

  DCHECK(backup_node.saved_blossom.empty());
  backup_node.saved_blossom = std::move(lca.blossom);
  lca.blossom = std::move(blossom);

  VLOG(2) << "S result " << NodeDebugString(lca_index);
}

BlossomGraph::EdgeIndex BlossomGraph::FindTightExternalEdgeBetweenNodes(
    NodeIndex tail, NodeIndex head) {
  DCHECK_NE(tail, head);
  DCHECK_EQ(tail, root_blossom_node_[tail]);
  DCHECK_EQ(head, root_blossom_node_[head]);
  for (const NodeIndex subnode : SubNodes(tail)) {
    for (const EdgeIndex e : graph_[subnode]) {
      const Edge& edge = edges_[e];
      const NodeIndex other_end = OtherEnd(edge, subnode);
      if (other_end == head && Slack(edge) == 0) {
        return e;
      }
    }
  }
  return kNoEdgeIndex;
}

void BlossomGraph::Expand(NodeIndex to_expand) {
  ++num_expands_;
  VLOG(2) << "Expand " << to_expand;

  Node& node_to_expand = nodes_[to_expand];
  DCHECK(node_to_expand.IsBlossom());
  DCHECK(node_to_expand.IsMinus());
  DCHECK_EQ(Dual(node_to_expand), 0);

  const EdgeIndex match_edge_index =
      FindTightExternalEdgeBetweenNodes(to_expand, node_to_expand.match);
  const EdgeIndex parent_edge_index =
      FindTightExternalEdgeBetweenNodes(to_expand, node_to_expand.parent);

  // First, restore the saved fields.
  Node& backup_node = nodes_[node_to_expand.blossom[1]];
#ifndef NDEBUG
  node_to_expand.dual = backup_node.saved_dual;
#endif
  node_to_expand.pseudo_dual = backup_node.saved_pseudo_dual;
  std::vector<NodeIndex> blossom = std::move(node_to_expand.blossom);
  node_to_expand.blossom = std::move(backup_node.saved_blossom);
  backup_node.saved_blossom.clear();

  // Restore the edges Head()/Tail().
  for (const NodeIndex n : blossom) {
    for (const NodeIndex subnode : SubNodes(n)) {
      root_blossom_node_[subnode] = n;
    }
  }

  // Now we try to find a 'blossom path' that will replace the blossom node in
  // the alternating tree: the blossom's parent [+] node in the tree will be
  // attached to a blossom subnode (the "path start"), the blossom's child in
  // the tree will be attached to a blossom subnode (the "path end", which could
  // be the same subnode or a different one), and, using the blossom cycle,
  // we'll get a path with an odd number of blossom subnodes to connect the two
  // (since the cycle is odd, one of the two paths will be odd too). The other
  // subnodes of the blossom will then be made free nodes matched pairwise.
  int blossom_path_start = -1;
  int blossom_path_end = -1;
  const NodeIndex start_node = OtherEndFromExternalNode(
      edges_[parent_edge_index], node_to_expand.parent);
  const NodeIndex end_node =
      OtherEndFromExternalNode(edges_[match_edge_index], node_to_expand.match);
  for (int i = 0; i < blossom.size(); ++i) {
    if (blossom[i] == start_node) blossom_path_start = i;
    if (blossom[i] == end_node) blossom_path_end = i;
  }

  // Split the cycle in two halves: nodes in [start..end] in path1, and
  // nodes in [end..start] in path2. Note the inclusive intervals.
  const std::vector<NodeIndex>& cycle = blossom;
  std::vector<NodeIndex> path1;
  std::vector<NodeIndex> path2;
  {
    const int end_offset =
        (blossom_path_end + cycle.size() - blossom_path_start) % cycle.size();
    for (int offset = 0; offset <= /*or equal*/ cycle.size(); ++offset) {
      const NodeIndex node =
          cycle[(blossom_path_start + offset) % cycle.size()];
      if (offset <= end_offset) path1.push_back(node);
      if (offset >= end_offset) path2.push_back(node);
    }
  }

  // Reverse path2 to also make it go from start to end.
  std::reverse(path2.begin(), path2.end());

  // Swap if necessary so that path1 is the odd-length one.
  if (path1.size() % 2 == 0) path1.swap(path2);

  // Use better aliases than 'path1' and 'path2' in the code below.
  std::vector<NodeIndex>& path_in_tree = path1;
  const std::vector<NodeIndex>& free_pairs = path2;

  // Strip path2 from the start and end, which aren't needed.
  path2.erase(path2.begin());
  path2.pop_back();

  const NodeIndex blossom_matched_node = node_to_expand.match;
  VLOG(2) << "Path ["
          << absl::StrJoin(path_in_tree, ", ", absl::StreamFormatter())
          << "] === " << blossom_matched_node;
  VLOG(2) << "Pairs ["
          << absl::StrJoin(free_pairs, ", ", absl::StreamFormatter()) << "]";

  // Restore the path in the tree, note that we append the blossom_matched_node
  // to simplify the code:
  // <---- Blossom ---->
  // [-] === [+] --- [-] === [+]
  path_in_tree.push_back(blossom_matched_node);
  CHECK_EQ(path_in_tree.size() % 2, 0);
  const CostValue tree_dual = nodes_[node_to_expand.root].tree_dual_delta;
  for (int i = 0; i < path_in_tree.size(); ++i) {
    const NodeIndex n = path_in_tree[i];
    const bool node_is_plus = i % 2;

    // Update the parent.
    if (i == 0) {
      // This is the path start and its parent is either itself or the parent of
      // to_expand if there was one.
      DCHECK(node_to_expand.parent != to_expand || n == to_expand);
      nodes_[n].parent = node_to_expand.parent;
    } else {
      nodes_[n].parent = path_in_tree[i - 1];
    }

    // Update the types and matches.
    nodes_[n].root = node_to_expand.root;
    nodes_[n].type = node_is_plus ? 1 : -1;
    nodes_[n].match = path_in_tree[node_is_plus ? i - 1 : i + 1];

    // Ignore the blossom_matched_node for the code below.
    if (i + 1 == path_in_tree.size()) continue;

    // Update the duals, depending on whether we have a new [+] or [-] node.
    // Note that this is also needed for the 'root' blossom node (i=0), because
    // we've restored its pseudo-dual from its old saved value above.
    const CostValue adjust = node_is_plus ? -tree_dual : tree_dual;
    nodes_[n].pseudo_dual += adjust;
    for (const NodeIndex subnode : SubNodes(n)) {
      for (const EdgeIndex e : graph_[subnode]) {
        Edge& edge = edges_[e];
        const NodeIndex other_end = OtherEnd(edge, subnode);
        if (other_end == n) continue;

        edge.pseudo_slack -= adjust;

        // non-internal edges used to be attached to the [-] node_to_expand,
        // so we adjust their dual.
        if (other_end != to_expand && !nodes_[other_end].is_internal) {
          edge.pseudo_slack += tree_dual;
        } else {
          // This was an internal edges. For the PQ code below to be correct, we
          // wait for its other end to have been processed by this loop already.
          // We detect that using the fact that the type of unprocessed internal
          // node is still zero.
          if (nodes_[other_end].type == 0) continue;
        }

        // Update edge queues.
        if (node_is_plus) {
          const Node& other_node = nodes_[other_end];
          DCHECK(!plus_plus_pq_.Contains(&edge));
          DCHECK(!plus_free_pq_.Contains(&edge));
          if (other_node.IsPlus()) {
            plus_plus_pq_.Add(&edge);
            if (edge.pseudo_slack == 2 * tree_dual) {
              primal_update_edge_queue_.push_back(e);
            }
          } else if (other_node.IsFree()) {
            plus_free_pq_.Add(&edge);
            if (edge.pseudo_slack == tree_dual) {
              primal_update_edge_queue_.push_back(e);
            }
          }
        }
      }
    }
  }

  // Update free nodes.
  for (const NodeIndex n : free_pairs) {
    nodes_[n].type = 0;
    nodes_[n].parent = n;
    nodes_[n].root = n;

    // Update edges slack and priority queue for the adjacent edges.
    for (const NodeIndex subnode : SubNodes(n)) {
      for (const EdgeIndex e : graph_[subnode]) {
        Edge& edge = edges_[e];
        const NodeIndex other_end = OtherEnd(edge, subnode);
        if (other_end == n) continue;

        // non-internal edges used to be attached to the [-] node_to_expand,
        // so we adjust their dual.
        if (other_end != to_expand && !nodes_[other_end].is_internal) {
          edge.pseudo_slack += tree_dual;
        }

        // Update PQ. Note that since this was attached to a [-] node it cannot
        // be in any queue. We will also never process twice the same edge here.
        DCHECK(!plus_plus_pq_.Contains(&edge));
        DCHECK(!plus_free_pq_.Contains(&edge));
        if (nodes_[other_end].IsPlus()) {
          plus_free_pq_.Add(&edge);
          if (edge.pseudo_slack == tree_dual) {
            primal_update_edge_queue_.push_back(e);
          }
        }
      }
    }
  }

  // Matches the free pair together.
  CHECK_EQ(free_pairs.size() % 2, 0);
  for (int i = 0; i < free_pairs.size(); i += 2) {
    nodes_[free_pairs[i]].match = free_pairs[i + 1];
    nodes_[free_pairs[i + 1]].match = free_pairs[i];
  }

  // Mark all node as external. We do that last so we could easily detect old
  // internal edges that are now external.
  for (const NodeIndex n : blossom) {
    nodes_[n].is_internal = false;
  }
}

void BlossomGraph::ExpandAllBlossoms() {
  // Queue of blossoms to expand.
  std::vector<NodeIndex> queue;
  for (NodeIndex n(0); n < nodes_.size(); ++n) {
    Node& node = nodes_[n];
    if (node.IsInternal()) continue;

    // When this is called, there should be no more trees.
    CHECK(node.IsFree());

    if (node.IsBlossom()) queue.push_back(n);
  }

  // TODO(user): remove duplication with expand?
  while (!queue.empty()) {
    const NodeIndex to_expand = queue.back();
    queue.pop_back();

    Node& node_to_expand = nodes_[to_expand];
    DCHECK(node_to_expand.IsBlossom());

    // Find the edge used to match to_expand with Match(to_expand).
    const EdgeIndex match_edge_index =
        FindTightExternalEdgeBetweenNodes(to_expand, node_to_expand.match);

    // Restore the saved data.
    Node& backup_node = nodes_[node_to_expand.blossom[1]];
#ifndef NDEBUG
    node_to_expand.dual = backup_node.saved_dual;
#endif
    node_to_expand.pseudo_dual = backup_node.saved_pseudo_dual;

    std::vector<NodeIndex> blossom = std::move(node_to_expand.blossom);
    node_to_expand.blossom = std::move(backup_node.saved_blossom);
    backup_node.saved_blossom.clear();

    // Restore the edges Head()/Tail().
    for (const NodeIndex n : blossom) {
      for (const NodeIndex subnode : SubNodes(n)) {
        root_blossom_node_[subnode] = n;
      }
    }

    // Find the index of matched_node in the blossom list.
    int internal_matched_index = -1;
    const NodeIndex matched_node = OtherEndFromExternalNode(
        edges_[match_edge_index], node_to_expand.match);
    const int size = blossom.size();
    for (int i = 0; i < size; ++i) {
      if (blossom[i] == matched_node) {
        internal_matched_index = i;
        break;
      }
    }
    CHECK_NE(internal_matched_index, -1);

    // Amongst the node_to_expand.blossom nodes, internal_matched_index is
    // matched with external_matched_node and the other are matched together.
    std::vector<NodeIndex> free_pairs;
    for (int i = (internal_matched_index + 1) % size;
         i != internal_matched_index; i = (i + 1) % size) {
      free_pairs.push_back(blossom[i]);
    }

    // Clear root/parent/type of all internal nodes.
    for (const NodeIndex to_clear : blossom) {
      nodes_[to_clear].type = 0;
      nodes_[to_clear].is_internal = false;
      nodes_[to_clear].parent = to_clear;
      nodes_[to_clear].root = to_clear;
    }

    // Matches the internal node with external one.
    const NodeIndex external_matched_node = node_to_expand.match;
    const NodeIndex internal_matched_node = blossom[internal_matched_index];
    nodes_[internal_matched_node].match = external_matched_node;
    nodes_[external_matched_node].match = internal_matched_node;

    // Matches the free pair together.
    CHECK_EQ(free_pairs.size() % 2, 0);
    for (int i = 0; i < free_pairs.size(); i += 2) {
      nodes_[free_pairs[i]].match = free_pairs[i + 1];
      nodes_[free_pairs[i + 1]].match = free_pairs[i];
    }

    // Now that the expansion is done, add to the queue any sub-blossoms.
    for (const NodeIndex n : blossom) {
      if (nodes_[n].IsBlossom()) queue.push_back(n);
    }
  }
}

const std::vector<NodeIndex>& BlossomGraph::SubNodes(NodeIndex n) {
  // This should be only called on an external node. However, in Shrink() we
  // mark the node as internal early, so we just make sure the node as no saved
  // blossom field here.
  DCHECK(nodes_[n].saved_blossom.empty());

  // Expand all the inner nodes under the node n. This will not be n iff node is
  // is in fact a blossom.
  subnodes_ = {n};
  for (int i = 0; i < subnodes_.size(); ++i) {
    const Node& node = nodes_[subnodes_[i]];

    // Since the first node in each list is always the node above, we just
    // skip it to avoid listing twice the nodes.
    if (!node.blossom.empty()) {
      subnodes_.insert(subnodes_.end(), node.blossom.begin() + 1,
                       node.blossom.end());
    }

    // We also need to recursively expand the sub-blossom nodes, which are (if
    // any) in the "saved_blossom" field of the first internal node of each
    // blossom. Since we iterate on all internal nodes here, we simply consult
    // the "saved_blossom" field of all subnodes, and it works the same.
    if (!node.saved_blossom.empty()) {
      subnodes_.insert(subnodes_.end(), node.saved_blossom.begin() + 1,
                       node.saved_blossom.end());
    }
  }
  return subnodes_;
}

std::string BlossomGraph::NodeDebugString(NodeIndex n) const {
  const Node& node = nodes_[n];
  if (node.is_internal) {
    return absl::StrCat("[I] #", n.value());
  }
  const std::string type = !NodeIsMatched(n) ? "[*]"
                           : node.type == 1  ? "[+]"
                           : node.type == -1 ? "[-]"
                                             : "[0]";
  return absl::StrCat(
      type, " #", n.value(), " dual: ", Dual(node).value(),
      " parent: ", node.parent.value(), " match: ", node.match.value(),
      " blossom: [", absl::StrJoin(node.blossom, ", ", absl::StreamFormatter()),
      "]");
}

std::string BlossomGraph::EdgeDebugString(EdgeIndex e) const {
  const Edge& edge = edges_[e];
  if (nodes_[Tail(edge)].is_internal || nodes_[Head(edge)].is_internal) {
    return absl::StrCat(Tail(edge).value(), "<->", Head(edge).value(),
                        " internal ");
  }
  return absl::StrCat(Tail(edge).value(), "<->", Head(edge).value(),
                      " slack: ", Slack(edge).value());
}

std::string BlossomGraph::DebugString() const {
  std::string result = "Graph:\n";
  for (NodeIndex n(0); n < nodes_.size(); ++n) {
    absl::StrAppend(&result, NodeDebugString(n), "\n");
  }
  for (EdgeIndex e(0); e < edges_.size(); ++e) {
    absl::StrAppend(&result, EdgeDebugString(e), "\n");
  }
  return result;
}

void BlossomGraph::DebugUpdateNodeDual(NodeIndex n, CostValue delta) {
#ifndef NDEBUG
  nodes_[n].dual += delta;
  for (const NodeIndex subnode : SubNodes(n)) {
    for (const EdgeIndex e : graph_[subnode]) {
      Edge& edge = edges_[e];
      const NodeIndex other_end = OtherEnd(edge, subnode);
      if (other_end == n) continue;
      edges_[e].slack -= delta;
    }
  }
#endif
}

CostValue BlossomGraph::Slack(const Edge& edge) const {
  const Node& tail_node = nodes_[Tail(edge)];
  const Node& head_node = nodes_[Head(edge)];
  CostValue slack = edge.pseudo_slack;
  if (Tail(edge) == Head(edge)) return slack;  // Internal...

  if (!tail_node.is_internal && !head_node.is_internal) {
    slack -= tail_node.type * nodes_[tail_node.root].tree_dual_delta +
             head_node.type * nodes_[head_node.root].tree_dual_delta;
  }
#ifndef NDEBUG
  DCHECK_EQ(slack, edge.slack) << tail_node.type << " " << head_node.type
                               << "  " << Tail(edge) << "<->" << Head(edge);
#endif
  return slack;
}

// Returns the dual value of the given node (which might be a pseudo-node).
CostValue BlossomGraph::Dual(const Node& node) const {
  const CostValue dual =
      node.pseudo_dual + node.type * nodes_[node.root].tree_dual_delta;
#ifndef NDEBUG
  DCHECK_EQ(dual, node.dual);
#endif
  return dual;
}

CostValue BlossomGraph::DualObjective() const {
  if (dual_objective_ == std::numeric_limits<int64_t>::max())
    return CostValue(std::numeric_limits<int64_t>::max());
  CHECK_EQ(dual_objective_ % 2, 0);
  return dual_objective_ / 2;
}

void BlossomGraph::AddToDualObjective(CostValue delta) {
  CHECK_GE(delta, 0);
  dual_objective_ = CostValue(CapAdd(dual_objective_.value(), delta.value()));
}

void BlossomGraph::DisplayStats() const {
  VLOG(1) << "num_grows: " << num_grows_;
  VLOG(1) << "num_augments: " << num_augments_;
  VLOG(1) << "num_shrinks: " << num_shrinks_;
  VLOG(1) << "num_expands: " << num_expands_;
  VLOG(1) << "num_dual_updates: " << num_dual_updates_;
}

}  // namespace operations_research

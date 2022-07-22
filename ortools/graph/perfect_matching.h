// Copyright 2010-2022 Google LLC
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

// Implementation of the Blossom V min-cost perfect matching algorithm. The
// main source for the algo is the paper: "Blossom V: A new implementation
// of a minimum cost perfect matching algorithm", Vladimir Kolmogorov.
//
// The Algorithm is a primal-dual algorithm. It always maintains a dual-feasible
// solution. We recall some notations here, but see the paper for more details
// as it is well written.
//
// TODO(user): This is a work in progress. The algo is not fully implemented
// yet. The initial version is closer to Blossom IV since we update the dual
// values for all trees at once with the same delta.

#ifndef OR_TOOLS_GRAPH_PERFECT_MATCHING_H_
#define OR_TOOLS_GRAPH_PERFECT_MATCHING_H_

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "ortools/base/adjustable_priority_queue-inl.h"
#include "ortools/base/adjustable_priority_queue.h"
#include "ortools/base/int_type.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/strong_vector.h"

namespace operations_research {

class BlossomGraph;

// Given an undirected graph with costs on each edges, this class allows to
// compute a perfect matching with minimum cost. A matching is a set of disjoint
// pairs of nodes connected by an edge. The matching is perfect if all nodes are
// matched to each others.
class MinCostPerfectMatching {
 public:
  // TODO(user): For now we ask the number of nodes at construction, but we
  // could automatically infer it from the added edges if needed.
  MinCostPerfectMatching() {}
  explicit MinCostPerfectMatching(int num_nodes) { Reset(num_nodes); }

  // Resets the class for a new graph.
  //
  // TODO(user): Eventually, we may support incremental Solves(). Or at least
  // memory reuse if one wants to solve many problems in a row.
  void Reset(int num_nodes);

  // Adds an undirected edges between the two given nodes.
  //
  // For now we only accept non-negative cost.
  // TODO(user): We can easily shift all costs if negative costs are needed.
  //
  // Important: The algorithm supports multi-edges, but it will be slower. So it
  // is better to only add one edge with a minimum cost between two nodes. In
  // particular, do not add both AddEdge(a, b, cost) and AddEdge(b, a, cost).
  // TODO(user): We could just presolve them away.
  void AddEdgeWithCost(int tail, int head, int64_t cost);

  // Solves the min-cost perfect matching problem on the given graph.
  //
  // NOTE(user): If needed we could support a time limit. Aborting early will
  // not provide a perfect matching, but the algorithm does maintain a valid
  // lower bound on the optimal cost that gets better and better during
  // execution until it reaches the optimal value. Similarly, it is easy to
  // support an early stop if this bound crosses a preset threshold.
  enum Status {
    // A perfect matching with min-cost has been found.
    OPTIMAL = 0,

    // There is no perfect matching in this graph.
    INFEASIBLE = 1,

    // The costs are too large and caused an overflow during the algorithm
    // execution.
    INTEGER_OVERFLOW = 2,

    // Advanced usage: the matching is OPTIMAL and was computed without
    // overflow, but its OptimalCost() does not fit on an int64_t. Note that
    // Match() still work and you can re-compute the cost in double for
    // instance.
    COST_OVERFLOW = 3,
  };
  ABSL_MUST_USE_RESULT Status Solve();

  // Returns the cost of the perfect matching. Only valid when the last solve
  // status was OPTIMAL.
  int64_t OptimalCost() const {
    DCHECK(optimal_solution_found_);
    return optimal_cost_;
  }

  // Returns the node matched to the given node. In a perfect matching all nodes
  // have a match. Only valid when the last solve status was OPTIMAL.
  int Match(int node) const {
    DCHECK(optimal_solution_found_);
    return matches_[node];
  }
  const std::vector<int>& Matches() const {
    DCHECK(optimal_solution_found_);
    return matches_;
  }

 private:
  std::unique_ptr<BlossomGraph> graph_;

  // Fields used to report the optimal solution. Most of it could be read on
  // the fly from BlossomGraph, but we prefer to copy them here. This allows to
  // reclaim the memory of graph_ early or allows to still query the last
  // solution if we later allow re-solve with incremental changes to the graph.
  bool optimal_solution_found_ = false;
  int64_t optimal_cost_ = 0;
  int64_t maximum_edge_cost_ = 0;
  std::vector<int> matches_;
};

// Class containing the main data structure used by the Blossom algorithm.
//
// At the core is the original undirected graph. During the algorithm execution
// we might collapse nodes into so-called Blossoms. A Blossom is a cycle of
// external nodes (which can be blossom nodes) of odd length (>= 3). The edges
// of the cycle are called blossom-forming eges and will always be tight
// (i.e. have a slack of zero). Once a Blossom is created, its nodes become
// "internal" and are basically considered merged into the blossom node for the
// rest of the algorithm (except if we later re-expand the blossom).
//
// Moreover, external nodes of the graph will have 3 possible types ([+], [-]
// and free [0]). Free nodes will always be matched together in pairs. Nodes of
// type [+] and [-] are arranged in a forest of alternating [+]/[-] disjoint
// trees. Each unmatched node is the root of a tree, and of type [+]. Nodes [-]
// will always have exactly one child to witch they are matched. [+] nodes can
// have any number of [-] children, to which they are not matched. All the edges
// of the trees will always be tight. Some examples below, double edges are used
// for matched nodes:
//
// A matched pair of free nodes:  [0] === [0]
//
// A possible rooted tree:  [+] -- [-] ==== [+]
//                            \
//                            [-] ==== [+] ---- [-] === [+]
//                                       \
//                                       [-] === [+]
//
// A single unmatched node is also a tree:  [+]
//
// TODO(user): For now this class does not maintain a second graph of edges
// between the trees nor does it maintains priority queue of edges.
//
// TODO(user): For now we use CHECKs in many places to facilitate development.
// Switch them to DCHECKs for speed once the code is more stable.
class BlossomGraph {
 public:
  // Typed index used by this class.
  DEFINE_INT_TYPE(NodeIndex, int);
  DEFINE_INT_TYPE(EdgeIndex, int);
  DEFINE_INT_TYPE(CostValue, int64_t);

  // Basic constants.
  // NOTE(user): Those can't be constexpr because of the or-tools export,
  // which complains for constexpr DEFINE_INT_TYPE.
  static const NodeIndex kNoNodeIndex;
  static const EdgeIndex kNoEdgeIndex;
  static const CostValue kMaxCostValue;

  // Node related data.
  // We store the edges incident to a node separately in the graph_ member.
  struct Node {
    explicit Node(NodeIndex n) : parent(n), match(n), root(n) {}

    // A node can be in one of these 4 exclusive states. Internal nodes are part
    // of a Blossom and should be ignored until this Blossom is expanded. All
    // the other nodes are "external". A free node is always matched to another
    // free node. All the other external node are in alternating [+]/[-] trees
    // rooted at the only unmatched node of the tree (always of type [+]).
    bool IsInternal() const {
      DCHECK(!is_internal || type == 0);
      return is_internal;
    }
    bool IsFree() const { return type == 0 && !is_internal; }
    bool IsPlus() const { return type == 1; }
    bool IsMinus() const { return type == -1; }

    // Is this node a blossom? if yes, it was formed by merging the node.blossom
    // nodes together. Note that we reuse the index of node.blossom[0] for this
    // blossom node. A blossom node can be of any type.
    bool IsBlossom() const { return !blossom.empty(); }

    // The type of this node. We use an int for convenience in the update
    // formulas. This is 1 for [+] nodes, -1 for [-] nodes and 0 for all the
    // others.
    //
    // Internal node also have a type of zero so the dual formula are correct.
    int type = 0;

    // Whether this node is part of a blossom.
    bool is_internal = false;

    // The parent of this node in its tree or itself otherwise.
    // Unused for internal nodes.
    NodeIndex parent;

    // Itself if not matched, or this node match otherwise.
    // Unused for internal nodes.
    NodeIndex match;

    // The root of this tree which never changes until a tree is disassambled by
    // an Augment(). Unused for internal nodes.
    NodeIndex root;

    // The "delta" to apply to get the dual for nodes of this tree.
    // This is only filled for root nodes (i.e unmatched nodes).
    CostValue tree_dual_delta = CostValue(0);

    // See the formula in Dual() used to derive the true dual of this node.
    // This is the equal to the "true" dual for free exterior node and internal
    // node.
    CostValue pseudo_dual = CostValue(0);

#ifndef NDEBUG
    // The true dual of this node. We only maintain this in debug mode.
    CostValue dual = CostValue(0);
#endif

    // Non-empty for Blossom only. The odd-cycle of blossom nodes that form this
    // blossom. The first element should always be the current blossom node, and
    // all the other nodes are internal nodes.
    std::vector<NodeIndex> blossom;

    // This allows to store information about a new blossom node created by
    // Shrink() so that we can properly restore it on Expand(). Note that we
    // store the saved information on the second node of a blossom cycle (and
    // not the blossom node itself) because that node will be "hidden" until the
    // blossom is expanded so this way, we do not need more than one set of
    // saved information per node.
#ifndef NDEBUG
    CostValue saved_dual;
#endif
    CostValue saved_pseudo_dual;
    std::vector<NodeIndex> saved_blossom;
  };

  // An undirected edge between two nodes: tail <-> head.
  struct Edge {
    Edge(NodeIndex t, NodeIndex h, CostValue c)
        : pseudo_slack(c),
#ifndef NDEBUG
          slack(c),
#endif
          tail(t),
          head(h) {
    }

    // Returns the "other" end of this edge.
    NodeIndex OtherEnd(NodeIndex n) const {
      DCHECK(n == tail || n == head);
      return NodeIndex(tail.value() ^ head.value() ^ n.value());
    }

    // AdjustablePriorityQueue interface. Note that we use std::greater<> in
    // our queues since we want the lowest pseudo_slack first.
    void SetHeapIndex(int index) { pq_position = index; }
    int GetHeapIndex() const { return pq_position; }
    bool operator>(const Edge& other) const {
      return pseudo_slack > other.pseudo_slack;
    }

    // See the formula is Slack() used to derive the true slack of this edge.
    CostValue pseudo_slack;

#ifndef NDEBUG
    // We only maintain this in debug mode.
    CostValue slack;
#endif

    // These are the current tail/head of this edges. These are changed when
    // creating or expanding blossoms. The order do not matter.
    //
    // TODO(user): Consider using node_a/node_b instead to remove the "directed"
    // meaning. I do need to think a bit more about it though.
    NodeIndex tail;
    NodeIndex head;

    // Position of this Edge in the underlying std::vector<> used to encode the
    // heap of one priority queue. An edge can be in at most one priority queue
    // which allow us to share this amongst queues.
    int pq_position = -1;
  };

  // Creates a BlossomGraph on the given number of nodes.
  explicit BlossomGraph(int num_nodes);

  // Same comment as MinCostPerfectMatching::AddEdgeWithCost() applies.
  void AddEdge(NodeIndex tail, NodeIndex head, CostValue cost);

  // Heuristic to start with a dual-feasible solution and some matched edges.
  // To be called once all edges are added. Returns false if the problem is
  // detected to be INFEASIBLE.
  ABSL_MUST_USE_RESULT bool Initialize();

  // Enters a loop that perform one of Grow()/Augment()/Shrink()/Expand() until
  // a fixed point is reached.
  void PrimalUpdates();

  // Computes the maximum possible delta for UpdateAllTrees() that keeps the
  // dual feasibility. Dual update approach (2) from the paper. This also fills
  // primal_update_edge_queue_.
  CostValue ComputeMaxCommonTreeDualDeltaAndResetPrimalEdgeQueue();

  // Applies the same dual delta to all trees. Dual update approach (2) from the
  // paper.
  void UpdateAllTrees(CostValue delta);

  // Returns true iff this node is matched and is thus not a tree root.
  // This cannot live in the Node class because we need to know the NodeIndex.
  bool NodeIsMatched(NodeIndex n) const;

  // Returns the node matched to the given one, or n if this node is not
  // currently matched.
  NodeIndex Match(NodeIndex n) const;

  // Adds to the tree of tail the free matched pair(head, Match(head)).
  // The edge is only used in DCHECKs. We duplicate tail/head because the
  // order matter here.
  void Grow(EdgeIndex e, NodeIndex tail, NodeIndex head);

  // Merges two tree and augment the number of matched nodes by 1. This is
  // the only functions that change the current matching.
  void Augment(EdgeIndex e);

  // Creates a Blossom using the given [+] -- [+] edge between two nodes of the
  // same tree.
  void Shrink(EdgeIndex e);

  // Expands a Blossom into its component.
  void Expand(NodeIndex to_expand);

  // Returns the current number of matched nodes.
  int NumMatched() const { return nodes_.size() - unmatched_nodes_.size(); }

  // Returns the current dual objective which is always a valid lower-bound on
  // the min-cost matching. Note that this is capped to kint64max in case of
  // overflow. Because all of our cost are positive, this starts at zero.
  CostValue DualObjective() const;

  // This must be called at the end of the algorithm to recover the matching.
  void ExpandAllBlossoms();

  // Return the "slack" of the given edge.
  CostValue Slack(const Edge& edge) const;

  // Returns the dual value of the given node (which might be a pseudo-node).
  CostValue Dual(const Node& node) const;

  // Display to VLOG(1) some statistic about the solve.
  void DisplayStats() const;

  // Checks that there is no possible primal update in the current
  // configuration.
  void DebugCheckNoPossiblePrimalUpdates();

  // Tests that the dual values are currently feasible.
  // This should ALWAYS be the case.
  bool DebugDualsAreFeasible() const;

  // In debug mode, we maintain the real slack of each edges and the real dual
  // of each node via this function. Both Slack() and Dual() checks in debug
  // mode that the value computed is the correct one.
  void DebugUpdateNodeDual(NodeIndex n, CostValue delta);

  // Returns true iff this is an external edge with a slack of zero.
  // An external edge is an edge between two external nodes.
  bool DebugEdgeIsTightAndExternal(const Edge& edge) const;

  // Getters to access node/edges from outside the class.
  // Only used in tests.
  const Edge& GetEdge(int e) const { return edges_[EdgeIndex(e)]; }
  const Node& GetNode(int n) const { return nodes_[NodeIndex(n)]; }

  // Display information for debugging.
  std::string NodeDebugString(NodeIndex n) const;
  std::string EdgeDebugString(EdgeIndex e) const;
  std::string DebugString() const;

 private:
  // Returns the index of a tight edge between the two given external nodes.
  // Returns kNoEdgeIndex if none could be found.
  //
  // TODO(user): Store edges for match/parent/blossom instead and remove the
  // need for this function that can take around 10% of the running time on
  // some problems.
  EdgeIndex FindTightExternalEdgeBetweenNodes(NodeIndex tail, NodeIndex head);

  // Appends the path from n to the root of its tree. Used by Augment().
  void AppendNodePathToRoot(NodeIndex n, std::vector<NodeIndex>* path) const;

  // Returns the depth of a node in its tree. Used by Shrink().
  int GetDepth(NodeIndex n) const;

  // Adds positive delta to dual_objective_ and cap at kint64max on overflow.
  void AddToDualObjective(CostValue delta);

  // In the presence of blossoms, the original tail/head of an arc might not be
  // up to date anymore. It is important to use these functions instead in all
  // the places where this can happen. That is basically everywhere except in
  // the initialization.
  NodeIndex Tail(const Edge& edge) const {
    return root_blossom_node_[edge.tail];
  }
  NodeIndex Head(const Edge& edge) const {
    return root_blossom_node_[edge.head];
  }

  // Returns the Head() or Tail() that does not correspond to node. Node that
  // node must be one of the original index in the given edge, this is DCHECKed
  // by edge.OtherEnd().
  NodeIndex OtherEnd(const Edge& edge, NodeIndex node) const {
    return root_blossom_node_[edge.OtherEnd(node)];
  }

  // Same as OtherEnd() but the given node should either be Tail(edge) or
  // Head(edge) and do not need to be one of the original node of this edge.
  NodeIndex OtherEndFromExternalNode(const Edge& edge, NodeIndex node) const {
    const NodeIndex head = Head(edge);
    if (head != node) {
      DCHECK_EQ(node, Tail(edge));
      return head;
    }
    return Tail(edge);
  }

  // Returns the given node and if this node is a blossom, all its internal
  // nodes (recursively). Note that any call to SubNodes() invalidate the
  // previously returned reference.
  const std::vector<NodeIndex>& SubNodes(NodeIndex n);

  // Just used to check that initialized is called exactly once.
  bool is_initialized_ = false;

  // The set of all edges/nodes of the graph.
  absl::StrongVector<EdgeIndex, Edge> edges_;
  absl::StrongVector<NodeIndex, Node> nodes_;

  // Identity for a non-blossom node, and its top blossom node (in case of many
  // nested blossom) for an internal node.
  absl::StrongVector<NodeIndex, NodeIndex> root_blossom_node_;

  // The current graph incidence. Note that one EdgeIndex should appear in
  // exactly two places (on its tail and head incidence list).
  absl::StrongVector<NodeIndex, std::vector<EdgeIndex>> graph_;

  // Used by SubNodes().
  std::vector<NodeIndex> subnodes_;

  // The unmatched nodes are exactly the root of the trees. After
  // initialization, this is only modified by Augment() which removes two nodes
  // from this list each time. Note that during Shrink()/Expand() we never
  // change the indexing of the root nodes.
  std::vector<NodeIndex> unmatched_nodes_;

  // List of tight_edges and possible shrink to check in PrimalUpdates().
  std::vector<EdgeIndex> primal_update_edge_queue_;
  std::vector<EdgeIndex> possible_shrink_;

  // Priority queues of edges of a certain types.
  AdjustablePriorityQueue<Edge, std::greater<Edge>> plus_plus_pq_;
  AdjustablePriorityQueue<Edge, std::greater<Edge>> plus_free_pq_;
  std::vector<Edge*> tmp_all_tops_;

  // The dual objective. Increase as the algorithm progress. This is a lower
  // bound on the min-cost of a perfect matching.
  CostValue dual_objective_ = CostValue(0);

  // Statistics on the main operations.
  int64_t num_grows_ = 0;
  int64_t num_augments_ = 0;
  int64_t num_shrinks_ = 0;
  int64_t num_expands_ = 0;
  int64_t num_dual_updates_ = 0;
};

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_PERFECT_MATCHING_H_

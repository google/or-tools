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


// An implementation of a push-relabel algorithm for the max flow problem.
//
// In the following, we consider a graph G = (V,E,s,t) where V denotes the set
// of nodes (vertices) in the graph, E denotes the set of arcs (edges). s and t
// denote distinguished nodes in G called source and target. n = |V| denotes the
// number of nodes in the graph, and m = |E| denotes the number of arcs in the
// graph.
//
// Each arc (v,w) is associated a capacity c(v,w).
//
// A flow is a function from E to R such that:
//
//  a) f(v,w) <= c(v,w) for all (v,w) in E (capacity constraint.)
//
//  b) f(v,w) = -f(w,v) for all (v,w) in E (flow antisymmetry constraint.)
//
//  c) sum on v f(v,w) = 0  (flow conservation.)
//
// The goal of this algorithm is to find the maximum flow from s to t, i.e.
// for example to maximize sum v f(s,v).
//
// The starting reference for this class of algorithms is:
// A.V. Goldberg and R.E. Tarjan. A new approach to the maximum flow problem.
// ACM Symposium on Theory of Computing, pp. 136-146.
// http://portal.acm.org/citation.cfm?id=12144.
//
// The basic idea of the algorithm is to handle preflows instead of flows,
// and to refine preflows until a maximum flow is obtained.
// A preflow is like a flow, except that the inflow can be larger than the
// outflow. If it is the case at a given node v, it is said that there is an
// excess at node v, and inflow = outflow + excess.
//
// More formally, a preflow is a function f such that:
//
// 1) f(v,w) <= c(v,w) for all (v,w) in E  (capacity constraint). c(v,w)  is a
//    value representing the maximum capacity for arc (v,w).
//
// 2) f(v,w) = -f(w,v) for all (v,w) in E (flow antisymmetry constraint)
//
// 3) excess(v) = sum on u f(u,v) >= 0 is the excess at node v, the
//    algebraic sum of all the incoming preflows at this node.
//
// Each node has an associated "height", in addition to its excess. The
// height of the source is defined to be equal to n, and cannot change. The
// height of the target is defined to be zero, and cannot change either. The
// height of all the other nodes is initialized at zero and is updated during
// the algorithm (see below). For those who want to know the details, the height
// of a node, corresponds to a reduced cost, and this enables one to prove that
// the algorithm actually computes the max flow. Note that the height of a node
// can be initialized to the distance to the target node in terms of number of
// nodes. This has not been tried in this implementation.
//
// A node v is said to be *active* if excess(v) > 0.
//
// In this case the following operations can be applied to it:
//
// - if there are *admissible* incident arcs, i.e. arcs which are not saturated,
//   and whose tail's height is lower than the height of the active node
//   considered, a PushFlow operation can be applied. It consists in sending as
//   much flow as both the excess at the node and the capacity of the arc
//   permit.
// - if there are no admissible arcs, the active node considered is relabeled,
//   i.e. its height is increased to 1 + the minimum height of its neighboring
//   nodes on admissible arcs.
// This is implemented in Discharge, which itself calls PushFlow and Relabel.
//
// Before running Discharge, it is necessary to initialize the algorithm with a
// preflow. This is done in InitializePreflow, which saturates all the arcs
// leaving the source node, and sets the excess at the heads of those arcs
// accordingly.
//
// The algorithm terminates when there are no remaining active nodes, i.e. all
// the excesses at all nodes are equal to zero. In this case, a maximum flow is
// obtained.
//
// The complexity of this algorithm depends amongst other things on the choice
// of the next active node. It has been shown, for example in:
// L. Tuncel, "On the Complexity of Preflow-Push Algorithms for Maximum-Flow
// Problems", Algorithmica 11(4): 353-359 (1994).
// and
// J. Cheriyan and K. Mehlhorn, "An analysis of the highest-level selection rule
// in the preflow-push max-flow algorithm", Information processing letters,
// 69(5):239-242 (1999).
// http://www.math.uwaterloo.ca/~jcheriya/PS_files/me3.0.ps
//
// ...that choosing the active node with the highest level yields a
// complexity of O(n^2 * sqrt(m)).
//
// TODO(user): implement the above active node choice rule.
//
// This has been validated experimentally in:
// R.K. Ahuja, M. Kodialam, A.K. Mishra, and J.B. Orlin, "Computational
// Investigations of Maximum Flow Algorithms", EJOR 97:509-542(1997).
// http://jorlin.scripts.mit.edu/docs/publications/58-comput%20investigations%20of.pdf.
//
//
// TODO(user): an alternative would be to evaluate:
// A.V. Goldberg, "The Partial Augment-Relabel Algorithm for the Maximum Flow
// Problem.” In Proceedings of Algorithms ESA, LNCS 5193:466-477, Springer 2008.
// http://www.springerlink.com/index/5535k2j1mt646338.pdf
//
// An interesting general reference on network flows is:
// R. K. Ahuja, T. L. Magnanti, J. B. Orlin, "Network Flows: Theory, Algorithms,
// and Applications," Prentice Hall, 1993, ISBN: 978-0136175490,
// http://www.amazon.com/dp/013617549X
//
// Keywords: Push-relabel, max-flow, network, graph, Goldberg, Tarjan, Dinic,
//           Dinitz.

#ifndef OR_TOOLS_GRAPH_MAX_FLOW_H_
#define OR_TOOLS_GRAPH_MAX_FLOW_H_

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/ebert_graph.h"
#include "ortools/graph/flow_problem.pb.h"
#include "ortools/util/stats.h"
#include "ortools/util/zvector.h"

namespace operations_research {

// Forward declaration.
template <typename Graph>
class GenericMaxFlow;

// A simple and efficient max-cost flow interface. This is as fast as
// GenericMaxFlow<ReverseArcStaticGraph>, which is the fastest, but uses
// more memory in order to hide the somewhat involved construction of the
// static graph.
//
// TODO(user): If the need arises, extend this interface to support warm start
// and incrementality between solves.
class SimpleMaxFlow {
 public:
  // The constructor takes no size.
  // New node indices will be created lazily by AddArcWithCapacity().
  SimpleMaxFlow();

  // Adds a directed arc with the given capacity from tail to head.
  // * Node indices and capacity must be non-negative (>= 0).
  // * Self-looping and duplicate arcs are supported.
  // * After the method finishes, NumArcs() == the returned ArcIndex + 1.
  ArcIndex AddArcWithCapacity(NodeIndex tail, NodeIndex head,
                              FlowQuantity capacity);

  // Returns the current number of nodes. This is one more than the largest
  // node index seen so far in AddArcWithCapacity().
  NodeIndex NumNodes() const;

  // Returns the current number of arcs in the graph.
  ArcIndex NumArcs() const;

  // Returns user-provided data.
  // The implementation will crash if "arc" is not in [0, NumArcs()).
  NodeIndex Tail(ArcIndex arc) const;
  NodeIndex Head(ArcIndex arc) const;
  FlowQuantity Capacity(ArcIndex arc) const;

  // Solves the problem (finds the maximum flow from the given source to the
  // given sink), and returns the problem status.
  enum Status {
    // Solve() was called and found an optimal solution. Note that OptimalFlow()
    // may be 0 which means that the sink is not reachable from the source.
    OPTIMAL,
    // There is a flow > std::numeric_limits<FlowQuantity>::max(). Note that in
    // this case, the class will contain a solution with a flow reaching that
    // bound.
    //
    // TODO(user): rename POSSIBLE_OVERFLOW to INT_OVERFLOW and modify our
    // clients.
    POSSIBLE_OVERFLOW,
    // The input is inconsistent (bad tail/head/capacity values).
    BAD_INPUT,
    // This should not happen. There was an error in our code (i.e. file a bug).
    BAD_RESULT
  };
  Status Solve(NodeIndex source, NodeIndex sink);

  // Returns the maximum flow we can send from the source to the sink in the
  // last OPTIMAL Solve() context.
  FlowQuantity OptimalFlow() const;

  // Returns the flow on the given arc in the last OPTIMAL Solve() context.
  //
  // Note: It is possible that there is more than one optimal solution. The
  // algorithm is deterministic so it will always return the same solution for
  // a given problem. However, there is no guarantee of this from one code
  // version to the next (but the code does not change often).
  FlowQuantity Flow(ArcIndex arc) const;

  // Returns the nodes reachable from the source by non-saturated arcs (.i.e.
  // arc with Flow(arc) < Capacity(arc)), the outgoing arcs of this set form a
  // minimum cut. This works only if Solve() returned OPTIMAL.
  void GetSourceSideMinCut(std::vector<NodeIndex>* result);

  // Returns the nodes that can reach the sink by non-saturated arcs, the
  // outgoing arcs of this set form a minimun cut. Note that if this is the
  // complement set of GetNodeReachableFromSource(), then the min-cut is unique.
  // This works only if Solve() returned OPTIMAL.
  void GetSinkSideMinCut(std::vector<NodeIndex>* result);

  // Creates the protocol buffer representation of the problem used by the last
  // Solve() call. This is mainly useful for debugging.
  FlowModel CreateFlowModelOfLastSolve();

 private:
  NodeIndex num_nodes_;
  std::vector<NodeIndex> arc_tail_;
  std::vector<NodeIndex> arc_head_;
  std::vector<FlowQuantity> arc_capacity_;
  std::vector<ArcIndex> arc_permutation_;
  std::vector<FlowQuantity> arc_flow_;
  FlowQuantity optimal_flow_;

  // Note that we cannot free the graph before we stop using the max-flow
  // instance that uses it.
  typedef ::util::ReverseArcStaticGraph<NodeIndex, ArcIndex> Graph;
  std::unique_ptr<Graph> underlying_graph_;
  std::unique_ptr<GenericMaxFlow<Graph> > underlying_max_flow_;

  DISALLOW_COPY_AND_ASSIGN(SimpleMaxFlow);
};

// Specific but efficient priority queue implementation. The priority type must
// be an integer. The queue allows to retrieve the element with highest priority
// but only allows pushes with a priority greater or equal to the highest
// priority in the queue minus one. All operations are in O(1) and the memory is
// in O(num elements in the queue). Elements with the same priority are
// retrieved with LIFO order.
//
// Note(user): As far as I know, this is an original idea and is the only code
// that use this in the Maximum Flow context. Papers usually refer to an
// height-indexed array of simple linked lists of active node with the same
// height. Even worse, sometimes they use double-linked list to allow arbitrary
// height update in order to detect missing height (used for the Gap heuristic).
// But this can actually be implemented a lot more efficiently by just
// maintaining the height distribution of all the node in the graph.
template <typename Element, typename IntegerPriority>
class PriorityQueueWithRestrictedPush {
 public:
  PriorityQueueWithRestrictedPush() : even_queue_(), odd_queue_() {}

  // Is the queue empty?
  bool IsEmpty() const;

  // Clears the queue.
  void Clear();

  // Push a new element in the queue. Its priority must be greater or equal to
  // the highest priority present in the queue, minus one. This condition is
  // DCHECKed, and violating it yields erroneous queue behavior in NDEBUG mode.
  void Push(Element index, IntegerPriority priority);

  // Returns the element with highest priority and remove it from the queue.
  // IsEmpty() must be false, this condition is DCHECKed.
  Element Pop();

 private:
  // Helper function to get the last element of a vector and pop it.
  Element PopBack(std::vector<std::pair<Element, IntegerPriority> >* queue);

  // This is the heart of the algorithm. basically we split the elements by
  // parity of their priority and the precondition on the Push() ensures that
  // both vectors are always sorted by increasing priority.
  std::vector<std::pair<Element, IntegerPriority> > even_queue_;
  std::vector<std::pair<Element, IntegerPriority> > odd_queue_;

  DISALLOW_COPY_AND_ASSIGN(PriorityQueueWithRestrictedPush);
};

// We want an enum for the Status of a max flow run, and we want this
// enum to be scoped under GenericMaxFlow<>. Unfortunately, swig
// doesn't handle templated enums very well, so we need a base,
// untemplated class to hold it.
class MaxFlowStatusClass {
 public:
  enum Status {
    NOT_SOLVED,    // The problem was not solved, or its data were edited.
    OPTIMAL,       // Solve() was called and found an optimal solution.
    INT_OVERFLOW,  // There is a feasible flow > max possible flow.
    BAD_INPUT,     // The input is inconsistent.
    BAD_RESULT     // There was an error.
  };
};

// Generic MaxFlow (there is a default MaxFlow specialization defined below)
// that works with StarGraph and all the reverse arc graphs from graph.h, see
// the end of max_flow.cc for the exact types this class is compiled for.
template <typename Graph>
class GenericMaxFlow : public MaxFlowStatusClass {
 public:
  typedef typename Graph::NodeIndex NodeIndex;
  typedef typename Graph::ArcIndex ArcIndex;
  typedef typename Graph::OutgoingArcIterator OutgoingArcIterator;
  typedef typename Graph::OutgoingOrOppositeIncomingArcIterator
      OutgoingOrOppositeIncomingArcIterator;
  typedef typename Graph::IncomingArcIterator IncomingArcIterator;
  typedef ZVector<ArcIndex> ArcIndexArray;

  // The height of a node never excess 2 times the number of node, so we
  // use the same type as a Node index.
  typedef NodeIndex NodeHeight;
  typedef ZVector<NodeHeight> NodeHeightArray;

  // Initialize a MaxFlow instance on the given graph. The graph does not need
  // to be fully built yet, but its capacity reservation are used to initialize
  // the memory of this class. source and target must also be valid node of
  // graph.
  GenericMaxFlow(const Graph* graph, NodeIndex source, NodeIndex target);
  virtual ~GenericMaxFlow() {}

  // Returns the graph associated to the current object.
  const Graph* graph() const { return graph_; }

  // Returns the status of last call to Solve(). NOT_SOLVED is returned if
  // Solve() has never been called or if the problem has been modified in such a
  // way that the previous solution becomes invalid.
  Status status() const { return status_; }

  // Returns the index of the node corresponding to the source of the network.
  NodeIndex GetSourceNodeIndex() const { return source_; }

  // Returns the index of the node corresponding to the sink of the network.
  NodeIndex GetSinkNodeIndex() const { return sink_; }

  // Sets the capacity for arc to new_capacity.
  void SetArcCapacity(ArcIndex arc, FlowQuantity new_capacity);

  // Sets the flow for arc.
  void SetArcFlow(ArcIndex arc, FlowQuantity new_flow);

  // Returns true if a maximum flow was solved.
  bool Solve();

  // Returns the total flow found by the algorithm.
  FlowQuantity GetOptimalFlow() const { return node_excess_[sink_]; }

  // Returns the flow on arc using the equations given in the comment on
  // residual_arc_capacity_.
  FlowQuantity Flow(ArcIndex arc) const {
    if (IsArcDirect(arc)) {
      return residual_arc_capacity_[Opposite(arc)];
    } else {
      return -residual_arc_capacity_[arc];
    }
  }

  // Returns the capacity of arc using the equations given in the comment on
  // residual_arc_capacity_.
  FlowQuantity Capacity(ArcIndex arc) const {
    if (IsArcDirect(arc)) {
      return residual_arc_capacity_[arc] +
             residual_arc_capacity_[Opposite(arc)];
    } else {
      return 0;
    }
  }

  // Returns the nodes reachable from the source in the residual graph, the
  // outgoing arcs of this set form a minimum cut.
  void GetSourceSideMinCut(std::vector<NodeIndex>* result);

  // Returns the nodes that can reach the sink in the residual graph, the
  // outgoing arcs of this set form a minimun cut. Note that if this is the
  // complement of GetNodeReachableFromSource(), then the min-cut is unique.
  //
  // TODO(user): In the two-phases algorithm, we can get this minimum cut
  // without doing the second phase. Add an option for this if there is a need
  // to, note that the second phase is pretty fast so the gain will be small.
  void GetSinkSideMinCut(std::vector<NodeIndex>* result);

  // Checks the consistency of the input, i.e. that capacities on the arcs are
  // non-negative or null.
  bool CheckInputConsistency() const;

  // Checks whether the result is valid, i.e. that node excesses are all equal
  // to zero (we have a flow) and that residual capacities are all non-negative
  // or zero.
  bool CheckResult() const;

  // Returns true if there exists a path from the source to the sink with
  // remaining capacity. This allows us to easily check at the end that the flow
  // we computed is indeed optimal (provided that all the conditions tested by
  // CheckResult() also hold).
  bool AugmentingPathExists() const;

  // Sets the different algorithm options. All default to true.
  // See the corresponding variable declaration below for more details.
  void SetUseGlobalUpdate(bool value) {
    use_global_update_ = value;
    if (!use_global_update_) process_node_by_height_ = false;
  }
  void SetUseTwoPhaseAlgorithm(bool value) { use_two_phase_algorithm_ = value; }
  void SetCheckInput(bool value) { check_input_ = value; }
  void SetCheckResult(bool value) { check_result_ = value; }
  void ProcessNodeByHeight(bool value) {
    process_node_by_height_ = value && use_global_update_;
  }

  // Returns the protocol buffer representation of the current problem.
  FlowModel CreateFlowModel();

 protected:
  // Returns true if arc is admissible.
  bool IsAdmissible(ArcIndex arc) const {
    return residual_arc_capacity_[arc] > 0 &&
           node_potential_[Tail(arc)] == node_potential_[Head(arc)] + 1;
  }

  // Returns true if node is active, i.e. if its excess is positive and it
  // is neither the source or the sink of the graph.
  bool IsActive(NodeIndex node) const {
    return (node != source_) && (node != sink_) && (node_excess_[node] > 0);
  }

  // Sets the capacity of arc to 'capacity' and clears the flow on arc.
  void SetCapacityAndClearFlow(ArcIndex arc, FlowQuantity capacity) {
    residual_arc_capacity_.Set(arc, capacity);
    residual_arc_capacity_.Set(Opposite(arc), 0);
  }

  // Returns true if a precondition for Relabel is met, i.e. the outgoing arcs
  // of node are all either saturated or the heights of their heads are greater
  // or equal to the height of node.
  bool CheckRelabelPrecondition(NodeIndex node) const;

  // Returns context concatenated with information about arc
  // in a human-friendly way.
  std::string DebugString(const std::string& context, ArcIndex arc) const;

  // Initializes the container active_nodes_.
  void InitializeActiveNodeContainer();

  // Get the first element from the active node container.
  NodeIndex GetAndRemoveFirstActiveNode() {
    if (process_node_by_height_) return active_node_by_height_.Pop();
    const NodeIndex node = active_nodes_.back();
    active_nodes_.pop_back();
    return node;
  }

  // Push element to the active node container.
  void PushActiveNode(const NodeIndex& node) {
    if (process_node_by_height_) {
      active_node_by_height_.Push(node, node_potential_[node]);
    } else {
      active_nodes_.push_back(node);
    }
  }

  // Check the emptiness of the container.
  bool IsEmptyActiveNodeContainer() {
    if (process_node_by_height_) {
      return active_node_by_height_.IsEmpty();
    } else {
      return active_nodes_.empty();
    }
  }

  // Performs optimization step.
  void Refine();
  void RefineWithGlobalUpdate();

  // Discharges an active node node by saturating its admissible adjacent arcs,
  // if any, and by relabelling it when it becomes inactive.
  void Discharge(NodeIndex node);

  // Initializes the preflow to a state that enables to run Refine.
  void InitializePreflow();

  // Clears the flow excess at each node by pushing the flow back to the source:
  // - Do a depth-first search from the source in the direct graph to cancel
  //   flow cycles.
  // - Then, return flow excess along the depth-first search tree (by pushing
  //   the flow in the reverse dfs topological order).
  // The theoretical complexity is O(mn), but it is a lot faster in practice.
  void PushFlowExcessBackToSource();

  // Computes the best possible node potential given the current flow using a
  // reverse breadth-first search from the sink in the reverse residual graph.
  // This is an implementation of the global update heuristic mentioned in many
  // max-flow papers. See for instance: B.V. Cherkassky, A.V. Goldberg, "On
  // implementing push-relabel methods for the maximum flow problem",
  // Algorithmica, 19:390-410, 1997.
  // ftp://reports.stanford.edu/pub/cstr/reports/cs/tr/94/1523/CS-TR-94-1523.pdf
  void GlobalUpdate();

  // Tries to saturate all the outgoing arcs from the source that can reach the
  // sink. Most of the time, we can do that in one go, except when more flow
  // than kMaxFlowQuantity can be pushed out of the source in which case we
  // have to be careful. Returns true if some flow was pushed.
  bool SaturateOutgoingArcsFromSource();

  // Pushes flow on arc,  i.e. consumes flow on residual_arc_capacity_[arc],
  // and consumes -flow on residual_arc_capacity_[Opposite(arc)]. Updates
  // node_excess_ at the tail and head of arc accordingly.
  void PushFlow(FlowQuantity flow, ArcIndex arc);

  // Relabels a node, i.e. increases its height by the minimum necessary amount.
  // This version of Relabel is relaxed in a way such that if an admissible arc
  // exists at the current node height, then the node is not relabeled. This
  // enables us to deal with wrong values of first_admissible_arc_[node] when
  // updating it is too costly.
  void Relabel(NodeIndex node);

  // Handy member functions to make the code more compact.
  NodeIndex Head(ArcIndex arc) const { return graph_->Head(arc); }
  NodeIndex Tail(ArcIndex arc) const { return graph_->Tail(arc); }
  ArcIndex Opposite(ArcIndex arc) const;
  bool IsArcDirect(ArcIndex arc) const;
  bool IsArcValid(ArcIndex arc) const;

  // Returns the set of nodes reachable from start in the residual graph or in
  // the reverse residual graph (if reverse is true).
  template <bool reverse>
  void ComputeReachableNodes(NodeIndex start, std::vector<NodeIndex>* result);

  // Maximum manageable flow.
  static const FlowQuantity kMaxFlowQuantity;

  // A pointer to the graph passed as argument.
  const Graph* graph_;

  // An array representing the excess for each node in graph_.
  QuantityArray node_excess_;

  // An array representing the height function for each node in graph_. For a
  // given node, this is a lower bound on the shortest path length from this
  // node to the sink in the residual network. The height of a node always goes
  // up during the course of a Solve().
  //
  // Since initially we saturate all the outgoing arcs of the source, we can
  // never reach the sink from the source in the residual graph. Initially we
  // set the height of the source to n (the number of node of the graph) and it
  // never changes. If a node as an height >= n, then this node can't reach the
  // sink and its height minus n is a lower bound on the shortest path length
  // from this node to the source in the residual graph.
  NodeHeightArray node_potential_;

  // An array representing the residual_capacity for each arc in graph_.
  // Residual capacities enable one to represent the capacity and flow for all
  // arcs in the graph in the following manner.
  // For all arc, residual_arc_capacity_[arc] = capacity[arc] - flow[arc]
  // Moreover, for reverse arcs, capacity[arc] = 0 by definition,
  // Also flow[Opposite(arc)] = -flow[arc] by definition.
  // Therefore:
  // - for a direct arc:
  //    flow[arc] = 0 - flow[Opposite(arc)]
  //              = capacity[Opposite(arc)] - flow[Opposite(arc)]
  //              = residual_arc_capacity_[Opposite(arc)]
  // - for a reverse arc:
  //    flow[arc] = -residual_arc_capacity_[arc]
  // Using these facts enables one to only maintain residual_arc_capacity_,
  // instead of both capacity and flow, for each direct and indirect arc. This
  // reduces the amount of memory for this information by a factor 2.
  QuantityArray residual_arc_capacity_;

  // An array representing the first admissible arc for each node in graph_.
  ArcIndexArray first_admissible_arc_;

  // A stack used for managing active nodes in the algorithm.
  // Note that the papers cited above recommend the use of a queue, but
  // benchmarking so far has not proved it is better. In particular, processing
  // nodes in LIFO order has better cache locality.
  std::vector<NodeIndex> active_nodes_;

  // A priority queue used for managing active nodes in the algorithm. It allows
  // to select the active node with highest height before each Discharge().
  // Moreover, since all pushes from this node will be to nodes with height
  // greater or equal to the initial discharged node height minus one, the
  // PriorityQueueWithRestrictedPush is a perfect fit.
  PriorityQueueWithRestrictedPush<NodeIndex, NodeHeight> active_node_by_height_;

  // The index of the source node in graph_.
  NodeIndex source_;

  // The index of the sink node in graph_.
  NodeIndex sink_;

  // The status of the problem.
  Status status_;

  // BFS queue used by the GlobalUpdate() function. We do not use a C++ queue
  // because we need access to the vector for different optimizations.
  std::vector<bool> node_in_bfs_queue_;
  std::vector<NodeIndex> bfs_queue_;

  // Whether or not to use GlobalUpdate().
  bool use_global_update_;

  // Whether or not we use a two-phase algorithm:
  // 1/ Only deal with nodes that can reach the sink. At the end we know the
  //    value of the maximum flow and we have a min-cut.
  // 2/ Call PushFlowExcessBackToSource() to obtain a max-flow. This is usually
  //    a lot faster than the first phase.
  bool use_two_phase_algorithm_;

  // Whether or not we use the PriorityQueueWithRestrictedPush to process the
  // active nodes rather than a simple queue. This can only be true if
  // use_global_update_ is true.
  //
  // Note(user): using a template will be slightly faster, but since we test
  // this in a non-critical path, this only has a minor impact.
  bool process_node_by_height_;

  // Whether or not we check the input, this is a small price to pay for
  // robustness. Disable only if you know the input is valid because an invalid
  // input can cause the algorithm to run into an infinite loop!
  bool check_input_;

  // Whether or not we check the result.
  // TODO(user): Make the check more exhaustive by checking the optimality?
  bool check_result_;

  // Statistics about this class.
  mutable StatsGroup stats_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GenericMaxFlow);
};

#if !SWIG

// Default instance MaxFlow that uses StarGraph. Note that we cannot just use a
// typedef because of dependent code expecting MaxFlow to be a real class.
// TODO(user): Modify this code and remove it.
class MaxFlow : public GenericMaxFlow<StarGraph> {
 public:
  MaxFlow(const StarGraph* graph, NodeIndex source, NodeIndex target)
      : GenericMaxFlow(graph, source, target) {}
};

#endif  // SWIG

template <typename Element, typename IntegerPriority>
bool PriorityQueueWithRestrictedPush<Element, IntegerPriority>::IsEmpty()
    const {
  return even_queue_.empty() && odd_queue_.empty();
}

template <typename Element, typename IntegerPriority>
void PriorityQueueWithRestrictedPush<Element, IntegerPriority>::Clear() {
  even_queue_.clear();
  odd_queue_.clear();
}

template <typename Element, typename IntegerPriority>
void PriorityQueueWithRestrictedPush<Element, IntegerPriority>::Push(
    Element element, IntegerPriority priority) {
  // Since users may rely on it, we DCHECK the exact condition.
  DCHECK(even_queue_.empty() || priority >= even_queue_.back().second - 1);
  DCHECK(odd_queue_.empty() || priority >= odd_queue_.back().second - 1);

  // Note that the DCHECK() below are less restrictive than the ones above but
  // check a necessary and sufficient condition for the priority queue to behave
  // as expected.
  if (priority & 1) {
    DCHECK(odd_queue_.empty() || priority >= odd_queue_.back().second);
    odd_queue_.push_back(std::make_pair(element, priority));
  } else {
    DCHECK(even_queue_.empty() || priority >= even_queue_.back().second);
    even_queue_.push_back(std::make_pair(element, priority));
  }
}

template <typename Element, typename IntegerPriority>
Element PriorityQueueWithRestrictedPush<Element, IntegerPriority>::Pop() {
  DCHECK(!IsEmpty());
  if (even_queue_.empty()) return PopBack(&odd_queue_);
  if (odd_queue_.empty()) return PopBack(&even_queue_);
  if (odd_queue_.back().second > even_queue_.back().second) {
    return PopBack(&odd_queue_);
  } else {
    return PopBack(&even_queue_);
  }
}

template <typename Element, typename IntegerPriority>
Element PriorityQueueWithRestrictedPush<Element, IntegerPriority>::PopBack(
    std::vector<std::pair<Element, IntegerPriority> >* queue) {
  DCHECK(!queue->empty());
  Element element = queue->back().first;
  queue->pop_back();
  return element;
}

}  // namespace operations_research
#endif  // OR_TOOLS_GRAPH_MAX_FLOW_H_

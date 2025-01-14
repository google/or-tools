// Copyright 2010-2025 Google LLC
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

#ifndef OR_TOOLS_GRAPH_MAX_FLOW_H_
#define OR_TOOLS_GRAPH_MAX_FLOW_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "ortools/graph/flow_problem.pb.h"
#include "ortools/graph/generic_max_flow.h"
#include "ortools/graph/graph.h"

namespace operations_research {

// A simple and efficient max-cost flow interface. This is as fast as
// GenericMaxFlow<ReverseArcStaticGraph>, which is the fastest, but uses
// more memory in order to hide the somewhat involved construction of the
// static graph.
//
// TODO(user): If the need arises, extend this interface to support warm start.
class SimpleMaxFlow {
 public:
  typedef int32_t NodeIndex;
  typedef int32_t ArcIndex;
  typedef int64_t FlowQuantity;

  // The constructor takes no size.
  // New node indices will be created lazily by AddArcWithCapacity().
  SimpleMaxFlow();

#ifndef SWIG
  // This type is neither copyable nor movable.
  SimpleMaxFlow(const SimpleMaxFlow&) = delete;
  SimpleMaxFlow& operator=(const SimpleMaxFlow&) = delete;
#endif

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
  // outgoing arcs of this set form a minimum cut. Note that if this is the
  // complement set of GetNodeReachableFromSource(), then the min-cut is unique.
  // This works only if Solve() returned OPTIMAL.
  void GetSinkSideMinCut(std::vector<NodeIndex>* result);

  // Change the capacity of an arc.
  //
  // WARNING: This looks like it enables incremental solves, but as of 2018-02,
  // the next Solve() will restart from scratch anyway.
  // TODO(user): Support incrementality in the max flow implementation.
  void SetArcCapacity(ArcIndex arc, FlowQuantity capacity);

  // Creates the protocol buffer representation of the current problem.
  FlowModelProto CreateFlowModelProto(NodeIndex source, NodeIndex sink) const;

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
};

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_MAX_FLOW_H_

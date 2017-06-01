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

#ifndef OR_TOOLS_SAT_NO_CYCLE_H_
#define OR_TOOLS_SAT_NO_CYCLE_H_

#include <vector>

#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {

// The "no-cycle" constraint.
//
// Each arc will be associated to a literal and this propagator will make sure
// that there is no cycle in the graph with only the arcs whose associated
// literal is set to true.
class NoCyclePropagator : public SatPropagator {
 public:
  NoCyclePropagator()
      : SatPropagator("NoCyclePropagator"),
        num_arcs_(0),
        problem_is_unsat_(false),
        initialization_is_done_(false),
        num_arcs_threshold_(std::numeric_limits<int64>::max()),
        include_propagated_arcs_in_graph_(true) {}
  ~NoCyclePropagator() final {}

  static NoCyclePropagator* CreateInModel(Model* model) {
    NoCyclePropagator* no_cycle = new NoCyclePropagator();
    model->GetOrCreate<SatSolver>()->AddPropagator(
        std::unique_ptr<NoCyclePropagator>(no_cycle));
    return no_cycle;
  }

  bool Propagate(Trail* trail) final;
  void Untrail(const Trail& trail, int trail_index) final;
  gtl::Span<Literal> Reason(const Trail& trail,
                                   int trail_index) const final;

  // Stops doing anything when the number of arcs in the graph becomes greater
  // that the given value. This allows to use this class to model a circuit
  // constraint on n nodes: we don't want any cycle, but it is okay to have one
  // when we add the n-th arc. Of course we also need to make sure that each
  // node as an unique successor using at-most-one constraints.
  void AllowCycleWhenNumArcsIsGreaterThan(int64 value) {
    num_arcs_threshold_ = value;
  }

  // If this is false, then we don't track inside our graphs the arcs that we
  // propagated. This is meant to be turned on if an arc and its reverse are
  // controlled by a literal and its negation. When this is the case, then we
  // know that all the arcs propagated by this class don't change the
  // reachability of the graph.
  void IncludePropagatedArcsInGraph(bool value) {
    include_propagated_arcs_in_graph_ = value;
  }

  // Adds a "constant" arc to the graph.
  // Self-arc are not allowed (it would create a trivial cycle).
  void AddArc(int tail, int head);

  // Registers an arc that will be present in the graph iff 'literal' is true.
  // Self-arc are not allowed (it would fix the given literal to false).
  //
  // TODO(user): support more than one arc associated to the same literal.
  void AddPotentialArc(int tail, int head, Literal literal);

  // Getters for the current graph. This is only in sync with the trail iff
  // SatPropagator::PropagationIsDone() is true.
  //
  // Note that these graphs will NOT contain all the arcs but will correctly
  // encode the reachability of every node. More specifically, when an arc (tail
  // -> head) is about to be added but a path from tail to head already exists
  // in the graph, this arc will not be added.
  struct Arc {
    int head;
    LiteralIndex literal_index;
  };
  const std::vector<std::vector<Arc>>& Graph() const { return graph_; }
  const std::vector<std::vector<Arc>>& ReverseGraph() const {
    return reverse_graph_;
  }

  // Getters for the "potential" arcs. That is the arcs that could be added to
  // the graph or not depending on their associated literal value. Note that
  // some already added arcs may not appear here for optimization purposes.
  const std::vector<std::vector<Arc>>& PotentialGraph() const {
    return potential_graph_;
  }
  const ITIVector<LiteralIndex, std::vector<std::pair<int, int>>>&
  PotentialArcs() const {
    return potential_arcs_;
  }

 private:
  // Adjust the internal data structures when a new arc is added.
  void AdjustSizes(int tail, int head, int literal_index);

  // Returns true if destination is reachable from source in graph_.
  // Warning: this modifies node_is_reached_ and reached_nodes_.
  bool IsReachable(int source, int destination) const;

  // Returns the set of node from which source can be reached (included).
  // Warning: this modifies node_is_reached_ and reached_nodes_.
  void FillNodeIsReachedWithAntecedentOf(int source) const;

  // Returns the vector of node that are reachable from source (included), but
  // not in the given already_reached. The already_reached vector is not const
  // because this function temporarily modifies it before restoring it to its
  // original value for performance reason.
  std::vector<int> NewlyReachable(int source,
                                  std::vector<bool>* already_reached) const;

  // Finds a path from source to target and output its reason.
  // Only the arcs whose associated literal is assigned before the given
  // trail_limit are considered.
  //
  // Warning: this modifies node_is_reached_ and reached_nodes_.
  void FindReasonForPath(const Trail& trail, int source, int target,
                         int trail_limit, std::vector<Literal>* reason) const;

  // The number of arcs in graph_ and reverse_graph_.
  int64 num_arcs_;

  // Just used to detect the corner case of a cycle with fixed arcs.
  bool problem_is_unsat_;
  bool initialization_is_done_;

  // Control the options of this class.
  int64 num_arcs_threshold_;
  bool include_propagated_arcs_in_graph_;

  // The current graph wich is kept in sync with the literal trail. For each
  // node, graph_[node] list the pair (head, literal_index) of the outgoing
  // arcs.
  //
  // Important: this will always be kept acyclic.
  std::vector<std::vector<Arc>> graph_;
  std::vector<std::vector<Arc>> reverse_graph_;

  // The graph formed by all the potential arcs in the same format as graph_.
  std::vector<std::vector<Arc>> potential_graph_;

  // The set of potential arc (tail, head) indexed by literal_index.
  //
  // TODO(user): Introduce a struct with .tail and .head to make the code more
  // readable.
  ITIVector<LiteralIndex, std::vector<std::pair<int, int>>> potential_arcs_;

  // Temporary vectors used by the various BFS computations. We always have:
  // node_is_reached_[node] is true iff reached_nodes_ contains node.
  mutable std::vector<int> reached_nodes_;
  mutable std::vector<bool> node_is_reached_;

  // Temporary vector to hold the result of NewlyReachable().
  std::vector<int> newly_reachable_;

  // Temporary vector used by FindReasonForPath().
  mutable std::vector<std::pair<int, LiteralIndex>> parent_index_with_literal_;

  // The arc that caused the literal at a given trail_index to be propagated.
  std::vector<std::pair<int, int>> reason_arc_;
  std::vector<int> reason_trail_limit_;

  DISALLOW_COPY_AND_ASSIGN(NoCyclePropagator);
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_NO_CYCLE_H_

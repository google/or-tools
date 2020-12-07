// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_SAT_CIRCUIT_H_
#define OR_TOOLS_SAT_CIRCUIT_H_

#include <functional>
#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ortools/base/int_type.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/rev.h"

namespace operations_research {
namespace sat {

// Circuit/sub-circuit constraint.
//
// Nodes that are not in the unique allowed sub-circuit must point to themseves.
// A nodes that has no self-arc must thus be inside the sub-circuit. If there is
// no self-arc at all, then this constaint forces the circuit to go through all
// the nodes. Multi-arcs are NOT supported.
//
// Important: for correctness, this constraint requires that "exactly one"
// constraints have been added for all the incoming (resp. outgoing) arcs of
// each node. Also, such constraint must propagate before this one.
class CircuitPropagator : PropagatorInterface, ReversibleInterface {
 public:
  struct Options {
    // Hack for the VRP to allow for more than one sub-circuit and forces all
    // the subcircuits to go through the node zero.
    bool multiple_subcircuit_through_zero = false;
  };

  // The constraints take a sparse representation of a graph on [0, n). Each arc
  // being present when the given literal is true.
  CircuitPropagator(int num_nodes, const std::vector<int>& tails,
                    const std::vector<int>& heads,
                    const std::vector<Literal>& literals, Options options,
                    Model* model);

  void SetLevel(int level) final;
  bool Propagate() final;
  bool IncrementalPropagate(const std::vector<int>& watch_indices) final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  // Updates the structures when the given arc is added to the paths.
  void AddArc(int tail, int head, LiteralIndex literal_index);

  // Clears and fills reason with the literals of the arcs that form a path from
  // the given node. The path can be a cycle, but in this case it must end at
  // start (not like a rho shape).
  void FillReasonForPath(int start_node, std::vector<Literal>* reason) const;

  const int num_nodes_;
  const Options options_;
  Trail* trail_;
  const VariablesAssignment& assignment_;

  // We use this to query in O(1) for an arc existence. The self-arcs are
  // accessed often, so we use a more efficient std::vector<> for them. Note
  // that we do not add self-arcs to graph_.
  //
  // TODO(user): for large dense graph, using a matrix is faster and uses less
  // memory. If the need arise we can have the two implementations.
  std::vector<Literal> self_arcs_;
  absl::flat_hash_map<std::pair<int, int>, Literal> graph_;

  // Data used to interpret the watch indices passed to IncrementalPropagate().
  struct Arc {
    int tail;
    int head;
  };
  std::vector<Literal> watch_index_to_literal_;
  std::vector<std::vector<Arc>> watch_index_to_arcs_;

  // Index in trail_ up to which we propagated all the assigned Literals.
  int propagation_trail_index_ = 0;

  // Current partial chains of arc that are present.
  std::vector<int> next_;  // -1 if not assigned yet.
  std::vector<int> prev_;  // -1 if not assigned yet.
  std::vector<LiteralIndex> next_literal_;

  // Backtrack support for the partial chains of arcs, level_ends_[level] is an
  // index in added_arcs_;
  std::vector<int> level_ends_;
  std::vector<Arc> added_arcs_;

  // Reversible list of node that must be in a cycle. A node must be in a cycle
  // iff self_arcs_[node] is false. This graph entry can be used as a reason.
  int rev_must_be_in_cycle_size_ = 0;
  std::vector<int> must_be_in_cycle_;

  // Temporary vectors.
  std::vector<bool> processed_;
  std::vector<bool> in_current_path_;

  DISALLOW_COPY_AND_ASSIGN(CircuitPropagator);
};

// This constraint ensures that the graph is a covering of all nodes by
// circuits and loops, such that all circuits contain exactly one distinguished
// node. Those distinguished nodes are meant to be depots.
//
// This constraint does not need ExactlyOnePerRowAndPerColumn() to be correct,
// but it does not propagate degree deductions (only fails if a node has more
// than one outgoing arc or more than one incoming arc), so that adding
// ExactlyOnePerRowAndPerColumn() should work better.
//
// TODO(user): Make distinguished nodes an array of Boolean variables,
// so this can be used for facility location problems.
class CircuitCoveringPropagator : PropagatorInterface, ReversibleInterface {
 public:
  CircuitCoveringPropagator(std::vector<std::vector<Literal>> graph,
                            const std::vector<int>& distinguished_nodes,
                            Model* model);

  void SetLevel(int level) final;
  bool Propagate() final;
  bool IncrementalPropagate(const std::vector<int>& watch_indices) final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  // Adds all literals on the path/circuit from tail to head in the graph of
  // literals set to true.
  // next_[i] should be filled with a node j s.t. graph_[i][j] is true, or -1.
  void FillFixedPathInReason(int start, int end, std::vector<Literal>* reason);

  // Input data.
  const std::vector<std::vector<Literal>> graph_;
  const int num_nodes_;
  std::vector<bool> node_is_distinguished_;

  // SAT incremental state.
  Trail* trail_;
  std::vector<std::pair<int, int>> watch_index_to_arc_;
  std::vector<std::pair<int, int>> fixed_arcs_;
  std::vector<int> level_ends_;

  // Used in Propagate() to represent paths and circuits.
  std::vector<int> next_;
  std::vector<int> prev_;
  std::vector<bool> visited_;
};

// Changes the node indices so that we get a graph in [0, num_nodes) where every
// node has at least one incoming or outgoing arc. Returns the number of nodes.
template <class IntContainer>
int ReindexArcs(IntContainer* tails, IntContainer* heads) {
  const int num_arcs = tails->size();
  if (num_arcs == 0) return 0;

  // Put all nodes in a set.
  std::set<int> nodes;
  for (int arc = 0; arc < num_arcs; ++arc) {
    nodes.insert((*tails)[arc]);
    nodes.insert((*heads)[arc]);
  }

  // Compute the new indices while keeping a stable order.
  int new_index = 0;
  absl::flat_hash_map<int, int> mapping;
  for (const int node : nodes) {
    mapping[node] = new_index++;
  }

  // Remap the arcs.
  for (int arc = 0; arc < num_arcs; ++arc) {
    (*tails)[arc] = mapping[(*tails)[arc]];
    (*heads)[arc] = mapping[(*heads)[arc]];
  }
  return nodes.size();
}

// ============================================================================
// Model based functions.
// ============================================================================

// This just wraps CircuitPropagator. See the comment there to see what this
// does. Note that any nodes with no outoing or no incoming arc will cause the
// problem to be UNSAT. One can call ReindexArcs() first to ignore such nodes.
std::function<void(Model*)> SubcircuitConstraint(
    int num_nodes, const std::vector<int>& tails, const std::vector<int>& heads,
    const std::vector<Literal>& literals,
    bool multiple_subcircuit_through_zero = false);

// TODO(user): Change to a sparse API like for the function above.
std::function<void(Model*)> ExactlyOnePerRowAndPerColumn(
    const std::vector<std::vector<Literal>>& graph);
std::function<void(Model*)> CircuitCovering(
    const std::vector<std::vector<Literal>>& graph,
    const std::vector<int>& distinguished_nodes);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CIRCUIT_H_

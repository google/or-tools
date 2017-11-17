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

#ifndef OR_TOOLS_SAT_CIRCUIT_H_
#define OR_TOOLS_SAT_CIRCUIT_H_

#include <functional>
#include <memory>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/int_type.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/rev.h"

namespace operations_research {
namespace sat {

// Initial version of the circuit/sub-circuit constraint.
//
// Nodes that are not in the unique allowed sub-circuit must point to themseves.
// A nodes that has no self-arc must thus be inside the sub-circuit. If there is
// no self-arc at all, then this constaint forces the circuit to go through all
// the nodes.
//
// Important: for correctness, this constraint requires to call
// ExactlyOnePerRowAndPerColumn() on the same graph.
class CircuitPropagator : PropagatorInterface, ReversibleInterface {
 public:
  struct Options {
    // Hack for the VRP to allow for more than one sub-circuit and forces all
    // the subcircuits to go through the node zero.
    bool multiple_subcircuit_through_zero = false;
  };

  // The constraints take a dense representation of a graph on [0, n). Each arc
  // being present when the given literal is true. The special values
  // kTrueLiteralIndex and kFalseLiteralIndex can be used for arcs that are
  // either always there or never there.
  CircuitPropagator(std::vector<std::vector<LiteralIndex>> graph,
                    Options options, Trail* trail);

  void SetLevel(int level) final;
  bool Propagate() final;
  bool IncrementalPropagate(const std::vector<int>& watch_indices) final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  // Helper to deal with kTrueLiteralIndex and kFalseLiteralIndex.
  bool LiteralIndexIsTrue(LiteralIndex index) {
    if (index == kTrueLiteralIndex) return true;
    if (index == kFalseLiteralIndex) return false;
    return assignment_.LiteralIsTrue(Literal(index));
  }
  bool LiteralIndexIsFalse(LiteralIndex index) {
    if (index == kTrueLiteralIndex) return false;
    if (index == kFalseLiteralIndex) return true;
    return assignment_.LiteralIsFalse(Literal(index));
  }

  // Updates the structures when the given arc is added to the paths.
  void AddArc(int tail, int head, LiteralIndex literal_index);

  // Clears and fills reason with the literals of the arcs that form a path from
  // the given node. The path can be a cycle, but in this case it must end at
  // start (not like a rho shape).
  void FillReasonForPath(int start_node, std::vector<Literal>* reason) const;

  const int num_nodes_;
  const std::vector<std::vector<LiteralIndex>> graph_;
  const Options options_;
  Trail* trail_;
  const VariablesAssignment& assignment_;

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
  // iff graph_[node][node] is false. This graph entry can be used as a reason.
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
  std::vector<int> visited_;
};

// ============================================================================
// Model based functions.
// ============================================================================

// Enforces that exactly one literal per rows and per columns is true.
// This only work for a square matrix (but could easily be generalized).
//
// If ignore_row_and_column_zero is true, this adds two less constraints by
// skipping the one for the row zero and for the column zero. Note however that
// the other constraints are not changed, i.e. matrix[0][5] is still counted
// in column 5.
std::function<void(Model*)> ExactlyOnePerRowAndPerColumn(
    const std::vector<std::vector<LiteralIndex>>& square_matrix,
    bool ignore_row_and_column_zero = false);

inline std::function<void(Model*)> SubcircuitConstraint(
    const std::vector<std::vector<LiteralIndex>>& graph) {
  return [=](Model* model) {
    if (graph.empty()) return;
    model->Add(ExactlyOnePerRowAndPerColumn(graph));
    CircuitPropagator::Options options;
    CircuitPropagator* constraint =
        new CircuitPropagator(graph, options, model->GetOrCreate<Trail>());
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

inline std::function<void(Model*)> MultipleSubcircuitThroughZeroConstraint(
    const std::vector<std::vector<LiteralIndex>>& graph) {
  return [=](Model* model) {
    if (graph.empty()) return;
    model->Add(ExactlyOnePerRowAndPerColumn(
        graph, /*ignore_row_and_column_zero=*/true));
    CircuitPropagator::Options options;
    options.multiple_subcircuit_through_zero = true;
    CircuitPropagator* constraint =
        new CircuitPropagator(graph, options, model->GetOrCreate<Trail>());
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

std::function<void(Model*)> CircuitCovering(
    const std::vector<std::vector<LiteralIndex>>& next,
    const std::vector<int>& distinguished_nodes);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CIRCUIT_H_

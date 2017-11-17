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

#include "ortools/sat/circuit.h"

#include <algorithm>

#include <unordered_map>
#include "ortools/base/map_util.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {

CircuitPropagator::CircuitPropagator(
    std::vector<std::vector<LiteralIndex>> graph, Options options, Trail* trail)
    : num_nodes_(graph.size()),
      graph_(std::move(graph)),
      options_(options),
      trail_(trail),
      assignment_(trail->Assignment()) {
  // TODO(user): add a way to properly handle trivially UNSAT cases.
  // For now we just check that they don't occur at construction.
  CHECK_GT(num_nodes_, 1)
      << "Trivial or UNSAT constraint, shouldn't be constructed!";
  next_.resize(num_nodes_, -1);
  prev_.resize(num_nodes_, -1);
  next_literal_.resize(num_nodes_);
  must_be_in_cycle_.resize(num_nodes_);
  std::unordered_map<LiteralIndex, int> literal_to_watch_index;
  for (int tail = 0; tail < num_nodes_; ++tail) {
    if (LiteralIndexIsFalse(graph_[tail][tail])) {
      // For the multiple_subcircuit_through_zero case, must_be_in_cycle_ will
      // be const and only contains zero.
      if (tail == 0 || !options_.multiple_subcircuit_through_zero) {
        must_be_in_cycle_[rev_must_be_in_cycle_size_++] = tail;
      }
    }
    for (int head = 0; head < num_nodes_; ++head) {
      LiteralIndex index = graph_[tail][head];
      if (LiteralIndexIsFalse(index)) continue;
      if (LiteralIndexIsTrue(index)) {
        CHECK_EQ(next_[tail], -1)
            << "Trivially UNSAT or duplicate arcs while adding " << tail
            << " -> " << head;
        CHECK_EQ(prev_[head], -1)
            << "Trivially UNSAT or duplicate arcs while adding " << tail
            << " -> " << head;
        AddArc(tail, head, kNoLiteralIndex);
        continue;
      }

      // Tricky: For self-arc, we watch instead when the arc become false.
      if (tail == head) index = Literal(index).NegatedIndex();

      int watch_index = FindWithDefault(literal_to_watch_index, index, -1);
      if (watch_index == -1) {
        watch_index = watch_index_to_literal_.size();
        literal_to_watch_index[index] = watch_index;
        watch_index_to_literal_.push_back(Literal(index));
        watch_index_to_arcs_.push_back(std::vector<Arc>());
      }
      watch_index_to_arcs_[watch_index].push_back({tail, head});
    }
  }
}

void CircuitPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (int w = 0; w < watch_index_to_literal_.size(); ++w) {
    watcher->WatchLiteral(watch_index_to_literal_[w], id, w);
  }
  watcher->RegisterReversibleClass(id, this);
  watcher->RegisterReversibleInt(id, &propagation_trail_index_);
  watcher->RegisterReversibleInt(id, &rev_must_be_in_cycle_size_);
}

void CircuitPropagator::SetLevel(int level) {
  if (level == level_ends_.size()) return;
  if (level > level_ends_.size()) {
    while (level > level_ends_.size()) {
      level_ends_.push_back(added_arcs_.size());
    }
    return;
  }

  // Backtrack.
  for (int i = level_ends_[level]; i < added_arcs_.size(); ++i) {
    const Arc arc = added_arcs_[i];
    next_[arc.tail] = -1;
    prev_[arc.head] = -1;
  }
  added_arcs_.resize(level_ends_[level]);
  level_ends_.resize(level);
}

void CircuitPropagator::FillReasonForPath(int start_node,
                                          std::vector<Literal>* reason) const {
  CHECK_NE(start_node, -1);
  reason->clear();
  int node = start_node;
  while (next_[node] != -1) {
    if (next_literal_[node] != kNoLiteralIndex) {
      reason->push_back(Literal(next_literal_[node]).Negated());
    }
    node = next_[node];
    if (node == start_node) break;
  }
}

// If multiple_subcircuit_through_zero is true, we never fill next_[0] and
// prev_[0].
void CircuitPropagator::AddArc(int tail, int head, LiteralIndex literal_index) {
  if (tail != 0 || !options_.multiple_subcircuit_through_zero) {
    next_[tail] = head;
    next_literal_[tail] = literal_index;
  }
  if (head != 0 || !options_.multiple_subcircuit_through_zero) {
    prev_[head] = tail;
  }
}

bool CircuitPropagator::IncrementalPropagate(
    const std::vector<int>& watch_indices) {
  for (const int w : watch_indices) {
    const Literal literal = watch_index_to_literal_[w];
    for (const Arc arc : watch_index_to_arcs_[w]) {
      // Special case for self-arc.
      if (arc.tail == arc.head) {
        must_be_in_cycle_[rev_must_be_in_cycle_size_++] = arc.tail;
        continue;
      }

      // Get rid of the trivial conflicts: At most one incoming and one outgoing
      // arc for each nodes.
      if (next_[arc.tail] != -1) {
        std::vector<Literal>* conflict = trail_->MutableConflict();
        if (next_literal_[arc.tail] != kNoLiteralIndex) {
          *conflict = {Literal(next_literal_[arc.tail]).Negated(),
                       literal.Negated()};
        } else {
          *conflict = {literal.Negated()};
        }
        return false;
      }
      if (prev_[arc.head] != -1) {
        std::vector<Literal>* conflict = trail_->MutableConflict();
        if (next_literal_[prev_[arc.head]] != kNoLiteralIndex) {
          *conflict = {Literal(next_literal_[prev_[arc.head]]).Negated(),
                       literal.Negated()};
        } else {
          *conflict = {literal.Negated()};
        }
        return false;
      }

      // Add the arc.
      AddArc(arc.tail, arc.head, literal.Index());
      added_arcs_.push_back(arc);
    }
  }
  return Propagate();
}

// This function assumes that next_, prev_, next_literal_ and must_be_in_cycle_
// are all up to date.
bool CircuitPropagator::Propagate() {
  processed_.assign(num_nodes_, false);
  for (int n = 0; n < num_nodes_; ++n) {
    if (processed_[n]) continue;
    if (next_[n] == n) continue;
    if (next_[n] == -1 && prev_[n] == -1) continue;

    // TODO(user): both this and the loop on must_be_in_cycle_ might take some
    // time on large graph. Optimize if this become an issue.
    in_current_path_.assign(num_nodes_, false);

    // Find the start and end of the path containing node n. If this is a
    // circuit, we will have start_node == end_node.
    int start_node = n;
    int end_node = n;
    in_current_path_[n] = true;
    processed_[n] = true;
    while (next_[end_node] != -1) {
      end_node = next_[end_node];
      in_current_path_[end_node] = true;
      processed_[end_node] = true;
      if (end_node == n) break;
    }
    while (prev_[start_node] != -1) {
      start_node = prev_[start_node];
      in_current_path_[start_node] = true;
      processed_[start_node] = true;
      if (start_node == n) break;
    }

    // Check if we miss any node that must be in the circuit. Note that the ones
    // for which graph_[i][i] is kFalseLiteralIndex are first. This is good as
    // it will produce shorter reason. Otherwise we prefer the first that was
    // assigned in the trail.
    bool miss_some_nodes = false;
    LiteralIndex extra_reason = kFalseLiteralIndex;
    for (int i = 0; i < rev_must_be_in_cycle_size_; ++i) {
      const int node = must_be_in_cycle_[i];
      if (!in_current_path_[node]) {
        miss_some_nodes = true;
        extra_reason = graph_[node][node];
        break;
      }
    }

    if (miss_some_nodes) {
      // A circuit that miss a mandatory node is a conflict.
      if (start_node == end_node) {
        FillReasonForPath(start_node, trail_->MutableConflict());
        if (extra_reason != kFalseLiteralIndex) {
          trail_->MutableConflict()->push_back(Literal(extra_reason));
        }
        return false;
      }

      // We have an unclosed path. Propagate the fact that it cannot
      // be closed into a cycle, i.e. not(end_node -> start_node).
      if (start_node != end_node) {
        const LiteralIndex literal_index = graph_[end_node][start_node];
        if (LiteralIndexIsFalse(literal_index)) continue;
        CHECK_NE(literal_index, kTrueLiteralIndex);

        std::vector<Literal>* reason = trail_->GetEmptyVectorToStoreReason();
        FillReasonForPath(start_node, reason);
        if (extra_reason != kFalseLiteralIndex) {
          reason->push_back(Literal(extra_reason));
        }
        return trail_->EnqueueWithStoredReason(
            Literal(literal_index).Negated());
      }
    }

    // If we have a cycle, we can propagate all the other nodes to point to
    // themselves. Otherwise there is nothing else to do.
    if (start_node != end_node) continue;
    if (options_.multiple_subcircuit_through_zero) continue;
    BooleanVariable variable_with_same_reason = kNoBooleanVariable;
    for (int node = 0; node < num_nodes_; ++node) {
      if (in_current_path_[node]) continue;
      if (LiteralIndexIsTrue(graph_[node][node])) continue;

      // This shouldn't happen because ExactlyOnePerRowAndPerColumn() should
      // have executed first and propagated graph_[node][node] to false.
      CHECK_EQ(next_[node], -1);

      // We should have detected that above (miss_some_nodes == true). But we
      // still need this for corner cases where the same literal is used for
      // many arcs, and we just propagated it here.
      if (LiteralIndexIsFalse(graph_[node][node])) {
        CHECK_NE(graph_[node][node], kFalseLiteralIndex);
        FillReasonForPath(start_node, trail_->MutableConflict());
        trail_->MutableConflict()->push_back(Literal(graph_[node][node]));
        return false;
      }

      // Propagate.
      const Literal literal(graph_[node][node]);
      if (variable_with_same_reason == kNoBooleanVariable) {
        variable_with_same_reason = literal.Variable();
        FillReasonForPath(start_node, trail_->GetEmptyVectorToStoreReason());
        CHECK(trail_->EnqueueWithStoredReason(literal));
      } else {
        trail_->EnqueueWithSameReasonAs(literal, variable_with_same_reason);
      }
    }
  }
  return true;
}

std::function<void(Model*)> ExactlyOnePerRowAndPerColumn(
    const std::vector<std::vector<LiteralIndex>>& square_matrix,
    bool ignore_row_and_col_zero) {
  return [=](Model* model) {
    int n = square_matrix.size();
    int num_trivially_false = 0;
    Trail* trail = model->GetOrCreate<Trail>();
    std::vector<Literal> exactly_one_constraint;
    for (const bool transpose : {false, true}) {
      for (int i = ignore_row_and_col_zero ? 1 : 0; i < n; ++i) {
        int num_true = 0;
        exactly_one_constraint.clear();
        for (int j = 0; j < n; ++j) {
          CHECK_EQ(n, square_matrix[i].size());
          const LiteralIndex index =
              transpose ? square_matrix[j][i] : square_matrix[i][j];
          if (index == kFalseLiteralIndex) continue;
          if (index == kTrueLiteralIndex) {
            ++num_true;
            continue;
          }
          exactly_one_constraint.push_back(Literal(index));
        }
        if (num_true > 1) {
          LOG(WARNING) << "UNSAT in ExactlyOnePerRowAndPerColumn().";
          return model->GetOrCreate<SatSolver>()->NotifyThatModelIsUnsat();
        }
        CHECK_LE(num_true, 1);
        if (num_true == 1) {
          for (const Literal l : exactly_one_constraint) {
            if (!trail->Assignment().VariableIsAssigned(l.Variable())) {
              ++num_trivially_false;
              trail->EnqueueWithUnitReason(l.Negated());
            }
          }
        } else {
          model->Add(ExactlyOneConstraint(exactly_one_constraint));
        }
      }
    }
    if (num_trivially_false > 0) {
      LOG(INFO) << "Num extra fixed literal: " << num_trivially_false;
    }
  };
}

CircuitCoveringPropagator::CircuitCoveringPropagator(
    std::vector<std::vector<Literal>> graph,
    const std::vector<int>& distinguished_nodes, Model* model)
    : graph_(std::move(graph)),
      num_nodes_(graph_.size()),
      trail_(model->GetOrCreate<Trail>()) {
  node_is_distinguished_.resize(num_nodes_, false);
  for (const int node : distinguished_nodes) {
    node_is_distinguished_[node] = true;
  }
}

void CircuitCoveringPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int watcher_id = watcher->Register(this);

  // Fill fixed_arcs_ with arcs that are initially fixed to true,
  // assign arcs to watch indices.
  for (int node1 = 0; node1 < num_nodes_; node1++) {
    for (int node2 = 0; node2 < num_nodes_; node2++) {
      const Literal l = graph_[node1][node2];
      if (trail_->Assignment().LiteralIsFalse(l)) continue;
      if (trail_->Assignment().LiteralIsTrue(l)) {
        fixed_arcs_.emplace_back(node1, node2);
      } else {
        watcher->WatchLiteral(l, watcher_id, watch_index_to_arc_.size());
        watch_index_to_arc_.emplace_back(node1, node2);
      }
    }
  }
  watcher->RegisterReversibleClass(watcher_id, this);
}

void CircuitCoveringPropagator::SetLevel(int level) {
  if (level == level_ends_.size()) return;
  if (level > level_ends_.size()) {
    while (level > level_ends_.size()) {
      level_ends_.push_back(fixed_arcs_.size());
    }
  } else {
    // Backtrack.
    fixed_arcs_.resize(level_ends_[level]);
    level_ends_.resize(level);
  }
}

bool CircuitCoveringPropagator::IncrementalPropagate(
    const std::vector<int>& watch_indices) {
  for (const int w : watch_indices) {
    const auto& arc = watch_index_to_arc_[w];
    fixed_arcs_.push_back(arc);
  }
  return Propagate();
}

void CircuitCoveringPropagator::FillFixedPathInReason(
    int start, int end, std::vector<Literal>* reason) {
  reason->clear();

  int current = start;
  do {
    DCHECK_NE(next_[current], -1);
    DCHECK(trail_->Assignment().LiteralIsTrue(graph_[current][next_[current]]));
    reason->push_back(graph_[current][next_[current]].Negated());
    current = next_[current];
  } while (current != end);
}

bool CircuitCoveringPropagator::Propagate() {
  // Gather next_ and prev_ from fixed arcs.
  next_.assign(num_nodes_, -1);
  prev_.assign(num_nodes_, -1);
  for (const auto& arc : fixed_arcs_) {
    // Two arcs go out of arc.first, forbidden.
    if (next_[arc.first] != -1) {
      *trail_->MutableConflict() = {graph_[arc.first][next_[arc.first]],
                                    graph_[arc.first][arc.second]};
      return false;
    }
    next_[arc.first] = arc.second;
    // Two arcs come into arc.second, forbidden.
    if (prev_[arc.second] != -1) {
      *trail_->MutableConflict() = {graph_[prev_[arc.second]][arc.second],
                                    graph_[arc.first][arc.second]};
      return false;
    }
    prev_[arc.second] = arc.first;
  }

  // For every node, find partial path/circuit in which the node is.
  // Use visited_ to visit each path/circuit only once.
  visited_.assign(num_nodes_, false);
  for (int node = 0; node < num_nodes_; node++) {
    // Skip if already visited, isolated or loop.
    if (visited_[node]) continue;
    if (prev_[node] == -1 && next_[node] == -1) continue;
    if (prev_[node] == node) continue;

    // Find start of path/circuit.
    int start = node;
    for (int current = prev_[node]; current != -1 && current != node;
         current = prev_[current]) {
      start = current;
    }

    // Find distinguished node of path. Fail if there are several,
    // fail if this is a non loop circuit and there are none.
    int distinguished = node_is_distinguished_[start] ? start : -1;
    int current = next_[start];
    int end = start;
    visited_[start] = true;
    while (current != -1 && current != start) {
      if (node_is_distinguished_[current]) {
        if (distinguished != -1) {
          FillFixedPathInReason(distinguished, current,
                                trail_->MutableConflict());
          return false;
        }
        distinguished = current;
      }
      visited_[current] = true;
      end = current;
      current = next_[current];
    }

    // Circuit with no distinguished nodes, forbidden.
    if (start == current && distinguished == -1) {
      FillFixedPathInReason(start, start, trail_->MutableConflict());
      return false;
    }

    // Path with no distinguished node: forbid to close it.
    if (current == -1 && distinguished == -1 &&
        !trail_->Assignment().LiteralIsFalse(graph_[end][start])) {
      auto* reason = trail_->GetEmptyVectorToStoreReason();
      FillFixedPathInReason(start, end, reason);
      const bool ok =
          trail_->EnqueueWithStoredReason(graph_[end][start].Negated());
      if (!ok) return false;
    }
  }
  return true;
}

std::function<void(Model*)> CircuitCovering(
    const std::vector<std::vector<LiteralIndex>>& next,
    const std::vector<int>& distinguished_nodes) {
  return [&next, &distinguished_nodes](Model* model) {
    const int num_nodes = next.size();
    // Convert LiteralIndex to Literal.
    IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
    std::vector<std::vector<Literal>> graph(num_nodes,
                                            std::vector<Literal>(num_nodes));
    for (int row = 0; row < num_nodes; row++) {
      for (int col = 0; col < num_nodes; col++) {
        if (next[row][col] == kTrueLiteralIndex) {
          graph[row][col] = encoder->GetLiteralTrue();
        } else if (next[row][col] == kFalseLiteralIndex) {
          graph[row][col] = encoder->GetLiteralTrue().Negated();
        } else {
          graph[row][col] = Literal(next[row][col]);
        }
      }
    }

    // Register, pass ownership.
    CircuitCoveringPropagator* constraint =
        new CircuitCoveringPropagator(graph, distinguished_nodes, model);
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

}  // namespace sat
}  // namespace operations_research

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

#include "ortools/sat/circuit.h"

#include <functional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/graph/strongly_connected_components.h"
#include "ortools/sat/all_different.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/enforcement.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

CircuitPropagator::CircuitPropagator(
    const int num_nodes, absl::Span<const int> tails,
    absl::Span<const int> heads, absl::Span<const Literal> enforcement_literals,
    absl::Span<const Literal> literals, Options options, Model* model)
    : num_nodes_(num_nodes),
      options_(options),
      trail_(*model->GetOrCreate<Trail>()),
      enforcement_helper_(*model->GetOrCreate<EnforcementHelper>()),
      assignment_(trail_.Assignment()) {
  CHECK(!tails.empty()) << "Empty constraint, shouldn't be constructed!";
  next_.resize(num_nodes_, -1);
  prev_.resize(num_nodes_, -1);
  next_literal_.resize(num_nodes_);
  must_be_in_cycle_.resize(num_nodes_);
  absl::flat_hash_map<LiteralIndex, int> literal_to_watch_index;

  // Temporary data to fill watch_index_to_arcs_.
  const int num_arcs = tails.size();
  std::vector<int> keys;
  std::vector<Arc> values;
  keys.reserve(num_arcs);
  values.reserve(num_arcs);

  graph_.reserve(num_arcs);
  self_arcs_.resize(num_nodes_, kFalseLiteralIndex);
  enabled_ = true;
  for (int arc = 0; arc < num_arcs; ++arc) {
    const int head = heads[arc];
    const int tail = tails[arc];
    const Literal literal = literals[arc];
    if (assignment_.LiteralIsFalse(literal)) continue;

    if (tail == head) {
      self_arcs_[tail] = literal.Index();
    } else {
      graph_[{tail, head}] = literal;
    }

    if (assignment_.LiteralIsTrue(literal)) {
      if (next_[tail] != -1 || prev_[head] != -1) {
        SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
        if (enforcement_literals.empty()) {
          VLOG(1) << "Trivially UNSAT or duplicate arcs while adding " << tail
                  << " -> " << head;
          sat_solver->NotifyThatModelIsUnsat();
        } else {
          std::vector<Literal> negated_enforcement_literals;
          for (const Literal literal : enforcement_literals) {
            negated_enforcement_literals.push_back(literal.Negated());
          }
          sat_solver->AddProblemClause(negated_enforcement_literals);
          enabled_ = false;
        }
        return;
      }
      AddArc(tail, head, kNoLiteralIndex);
      continue;
    }

    // Tricky: For self-arc, we watch instead when the arc become false.
    const Literal watched_literal = tail == head ? literal.Negated() : literal;
    const auto& it = literal_to_watch_index.find(watched_literal.Index());
    int watch_index = it != literal_to_watch_index.end() ? it->second : -1;
    if (watch_index == -1) {
      watch_index = watch_index_to_literal_.size();
      literal_to_watch_index[watched_literal.Index()] = watch_index;
      watch_index_to_literal_.push_back(watched_literal);
    }

    keys.push_back(watch_index);
    values.push_back({tail, head});
  }
  watch_index_to_arcs_.ResetFromFlatMapping(keys, values);

  for (int node = 0; node < num_nodes_; ++node) {
    if (self_arcs_[node] == kFalseLiteralIndex ||
        assignment_.LiteralIsFalse(Literal(self_arcs_[node]))) {
      // For the multiple_subcircuit_through_zero case, must_be_in_cycle_ will
      // be const and only contains zero.
      if (node == 0 || !options_.multiple_subcircuit_through_zero) {
        must_be_in_cycle_[rev_must_be_in_cycle_size_++] = node;
      }
    }
  }

  GenericLiteralWatcher* watcher = model->GetOrCreate<GenericLiteralWatcher>();
  enforcement_id_ = enforcement_helper_.Register(enforcement_literals, watcher,
                                                 RegisterWith(watcher));
}

int CircuitPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (int w = 0; w < watch_index_to_literal_.size(); ++w) {
    watcher->WatchLiteral(watch_index_to_literal_[w], id, w);
  }
  watcher->RegisterReversibleClass(id, this);
  watcher->RegisterReversibleInt(id, &rev_must_be_in_cycle_size_);

  // This is needed in case a Literal is used for more than one arc, we may
  // propagate it to false/true here, and it might trigger more propagation.
  //
  // TODO(user): come up with a test that fail when this is not here.
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
  return id;
}

void CircuitPropagator::SetLevel(int level) {
  if (!enabled_) return;
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

bool CircuitPropagator::ReportConflictOrPropagateEnforcement(
    std::vector<Literal>* reason) {
  if (enforcement_helper_.Status(enforcement_id_) ==
      EnforcementStatus::IS_ENFORCED) {
    enforcement_helper_.AddEnforcementReason(enforcement_id_, reason);
    trail_.MutableConflict()->assign(reason->begin(), reason->end());
    return false;
  } else {
    return enforcement_helper_.PropagateWhenFalse(enforcement_id_, *reason,
                                                  /*integer_reason=*/{});
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
  if (!enabled_) return true;
  const EnforcementStatus status = enforcement_helper_.Status(enforcement_id_);
  if (status != EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT &&
      status != EnforcementStatus::IS_ENFORCED) {
    return true;
  }

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
        if (next_literal_[arc.tail] != kNoLiteralIndex) {
          temp_reason_ = {Literal(next_literal_[arc.tail]).Negated(),
                          literal.Negated()};
        } else {
          temp_reason_ = {literal.Negated()};
        }
        return ReportConflictOrPropagateEnforcement(&temp_reason_);
      }
      if (prev_[arc.head] != -1) {
        if (next_literal_[prev_[arc.head]] != kNoLiteralIndex) {
          temp_reason_ = {Literal(next_literal_[prev_[arc.head]]).Negated(),
                          literal.Negated()};
        } else {
          temp_reason_ = {literal.Negated()};
        }
        return ReportConflictOrPropagateEnforcement(&temp_reason_);
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
  if (!enabled_) return true;
  const EnforcementStatus status = enforcement_helper_.Status(enforcement_id_);
  if (status != EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT &&
      status != EnforcementStatus::IS_ENFORCED) {
    return true;
  }

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

    // TODO(user): we can fail early in more case, like no more possible path
    // to any of the mandatory node.
    if (options_.multiple_subcircuit_through_zero) {
      // Any cycle must contain zero.
      if (start_node == end_node && !in_current_path_[0]) {
        FillReasonForPath(start_node, &temp_reason_);
        return ReportConflictOrPropagateEnforcement(&temp_reason_);
      }

      // An incomplete path cannot be closed except if one of the end-points
      // is zero.
      if (start_node != end_node && start_node != 0 && end_node != 0 &&
          status == EnforcementStatus::IS_ENFORCED) {
        const auto it = graph_.find({end_node, start_node});
        if (it == graph_.end()) continue;
        const Literal literal = it->second;
        if (assignment_.LiteralIsFalse(literal)) continue;

        std::vector<Literal>* reason = trail_.GetEmptyVectorToStoreReason();
        FillReasonForPath(start_node, reason);
        enforcement_helper_.AddEnforcementReason(enforcement_id_, reason);
        if (!trail_.EnqueueWithStoredReason(kNoClauseId, literal.Negated())) {
          return false;
        }
      }

      // None of the other propagation below are valid in case of multiple
      // circuits.
      continue;
    }

    // Check if we miss any node that must be in the circuit. Note that the ones
    // for which self_arcs_[i] is kFalseLiteralIndex are first. This is good as
    // it will produce shorter reason. Otherwise we prefer the first that was
    // assigned in the trail.
    bool miss_some_nodes = false;
    LiteralIndex extra_reason = kFalseLiteralIndex;
    for (int i = 0; i < rev_must_be_in_cycle_size_; ++i) {
      const int node = must_be_in_cycle_[i];
      if (!in_current_path_[node]) {
        miss_some_nodes = true;
        extra_reason = self_arcs_[node];
        break;
      }
    }

    if (miss_some_nodes) {
      // A circuit that miss a mandatory node is a conflict.
      if (start_node == end_node) {
        FillReasonForPath(start_node, &temp_reason_);
        if (extra_reason != kFalseLiteralIndex) {
          temp_reason_.push_back(Literal(extra_reason));
        }
        return ReportConflictOrPropagateEnforcement(&temp_reason_);
      }

      // We have an unclosed path. Propagate the fact that it cannot
      // be closed into a cycle, i.e. not(end_node -> start_node).
      if (start_node != end_node && status == EnforcementStatus::IS_ENFORCED) {
        const auto it = graph_.find({end_node, start_node});
        if (it == graph_.end()) continue;
        const Literal literal = it->second;
        if (assignment_.LiteralIsFalse(literal)) continue;

        std::vector<Literal>* reason = trail_.GetEmptyVectorToStoreReason();
        FillReasonForPath(start_node, reason);
        enforcement_helper_.AddEnforcementReason(enforcement_id_, reason);
        if (extra_reason != kFalseLiteralIndex) {
          reason->push_back(Literal(extra_reason));
        }
        const bool ok =
            trail_.EnqueueWithStoredReason(kNoClauseId, literal.Negated());
        if (!ok) return false;
        continue;
      }
    }

    // If we have a cycle, we can propagate all the other nodes to point to
    // themselves. Otherwise there is nothing else to do.
    if (start_node != end_node) continue;
    BooleanVariable variable_with_same_reason = kNoBooleanVariable;
    for (int node = 0; node < num_nodes_; ++node) {
      if (in_current_path_[node]) continue;
      if (self_arcs_[node] >= 0 &&
          assignment_.LiteralIsTrue(Literal(self_arcs_[node]))) {
        continue;
      }

      // This shouldn't happen because ExactlyOnePerRowAndPerColumn() should
      // have executed first and propagated self_arcs_[node] to false.
      CHECK_EQ(next_[node], -1);

      // We should have detected that above (miss_some_nodes == true). But we
      // still need this for corner cases where the same literal is used for
      // many arcs, and we just propagated it here.
      if (self_arcs_[node] == kFalseLiteralIndex ||
          assignment_.LiteralIsFalse(Literal(self_arcs_[node]))) {
        FillReasonForPath(start_node, &temp_reason_);
        if (self_arcs_[node] != kFalseLiteralIndex) {
          temp_reason_.push_back(Literal(self_arcs_[node]));
        }
        return ReportConflictOrPropagateEnforcement(&temp_reason_);
      }

      // Propagate.
      if (status == EnforcementStatus::IS_ENFORCED) {
        const Literal literal(self_arcs_[node]);
        if (variable_with_same_reason == kNoBooleanVariable) {
          variable_with_same_reason = literal.Variable();
          std::vector<Literal>* reason = trail_.GetEmptyVectorToStoreReason();
          FillReasonForPath(start_node, reason);
          enforcement_helper_.AddEnforcementReason(enforcement_id_, reason);
          const bool ok = trail_.EnqueueWithStoredReason(kNoClauseId, literal);
          if (!ok) return false;
        } else {
          trail_.EnqueueWithSameReasonAs(literal, variable_with_same_reason);
        }
      }
    }
  }
  return true;
}

NoCyclePropagator::NoCyclePropagator(int num_nodes, absl::Span<const int> tails,
                                     absl::Span<const int> heads,
                                     absl::Span<const Literal> literals,
                                     Model* model)
    : num_nodes_(num_nodes),
      trail_(model->GetOrCreate<Trail>()),
      assignment_(trail_->Assignment()) {
  CHECK(!tails.empty()) << "Empty constraint, shouldn't be constructed!";

  graph_.resize(num_nodes);
  graph_literals_.resize(num_nodes);

  const int num_arcs = tails.size();
  absl::flat_hash_map<LiteralIndex, int> literal_to_watch_index;
  for (int arc = 0; arc < num_arcs; ++arc) {
    const int head = heads[arc];
    const int tail = tails[arc];
    const Literal literal = literals[arc];

    if (assignment_.LiteralIsFalse(literal)) continue;
    if (assignment_.LiteralIsTrue(literal)) {
      // Fixed arc. It will never be removed.
      graph_[tail].push_back(head);
      graph_literals_[tail].push_back(literal);
      continue;
    }

    // We have to deal with the same literal controlling more than one arc.
    const auto [it, inserted] = literal_to_watch_index.insert(
        {literal.Index(), watch_index_to_literal_.size()});
    if (inserted) {
      watch_index_to_literal_.push_back(literal);
      watch_index_to_arcs_.push_back({});
    }
    watch_index_to_arcs_[it->second].push_back({tail, head});
  }

  // We register at construction.
  //
  // TODO(user): Uniformize this across propagator. Sometimes it is nice not
  // to register them, but most of them can be registered right away.
  RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
}

void NoCyclePropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (int w = 0; w < watch_index_to_literal_.size(); ++w) {
    watcher->WatchLiteral(watch_index_to_literal_[w], id, w);
  }
  watcher->RegisterReversibleClass(id, this);

  // This class currently only test for conflict, so no need to call it twice.
  // watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
}

void NoCyclePropagator::SetLevel(int level) {
  if (level == level_ends_.size()) return;
  if (level > level_ends_.size()) {
    while (level > level_ends_.size()) {
      level_ends_.push_back(touched_nodes_.size());
    }
    return;
  }

  // Backtrack.
  for (int i = level_ends_[level]; i < touched_nodes_.size(); ++i) {
    graph_literals_[touched_nodes_[i]].pop_back();
    graph_[touched_nodes_[i]].pop_back();
  }
  touched_nodes_.resize(level_ends_[level]);
  level_ends_.resize(level);
}

bool NoCyclePropagator::IncrementalPropagate(
    const std::vector<int>& watch_indices) {
  for (const int w : watch_indices) {
    const Literal literal = watch_index_to_literal_[w];
    for (const auto& [tail, head] : watch_index_to_arcs_[w]) {
      graph_[tail].push_back(head);
      graph_literals_[tail].push_back(literal);
      touched_nodes_.push_back(tail);
    }
  }
  return Propagate();
}

// TODO(user): only explore node with newly added arcs.
//
// TODO(user): We could easily re-index the graph so that only nodes with arcs
// are used. Because right now we are in O(num_nodes) even if the graph is
// empty.
bool NoCyclePropagator::Propagate() {
  // The graph should be up to date when this is called thanks to
  // IncrementalPropagate(). We just do a SCC on the graph.
  components_.clear();
  FindStronglyConnectedComponents(num_nodes_, graph_, &components_);

  for (const std::vector<int>& compo : components_) {
    if (compo.size() <= 1) continue;

    // We collect all arc from this compo.
    //
    // TODO(user): We could be more efficient here, but this is only executed on
    // conflicts. We should at least make sure we return a single cycle even
    // though if this is called often enough, we shouldn't have a lot more than
    // this.
    absl::flat_hash_set<int> nodes(compo.begin(), compo.end());
    std::vector<Literal>* conflict = trail_->MutableConflict();
    conflict->clear();
    for (const int tail : compo) {
      const int degree = graph_[tail].size();
      CHECK_EQ(degree, graph_literals_[tail].size());
      for (int i = 0; i < degree; ++i) {
        if (nodes.contains(graph_[tail][i])) {
          conflict->push_back(graph_literals_[tail][i].Negated());
        }
      }
    }
    return false;
  }

  return true;
}

CircuitCoveringPropagator::CircuitCoveringPropagator(
    std::vector<std::vector<Literal>> graph,
    absl::Span<const int> distinguished_nodes, Model* model)
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
      *trail_->MutableConflict() = {
          graph_[arc.first][next_[arc.first]].Negated(),
          graph_[arc.first][arc.second].Negated()};
      return false;
    }
    next_[arc.first] = arc.second;
    // Two arcs come into arc.second, forbidden.
    if (prev_[arc.second] != -1) {
      *trail_->MutableConflict() = {
          graph_[prev_[arc.second]][arc.second].Negated(),
          graph_[arc.first][arc.second].Negated()};
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
      const bool ok = trail_->EnqueueWithStoredReason(
          kNoClauseId, graph_[end][start].Negated());
      if (!ok) return false;
    }
  }
  return true;
}

std::function<void(Model*)> ExactlyOnePerRowAndPerColumn(
    absl::Span<const std::vector<Literal>> graph) {
  return [=, graph = std::vector<std::vector<Literal>>(
                 graph.begin(), graph.end())](Model* model) {
    const int n = graph.size();
    std::vector<Literal> exactly_one_constraint;
    exactly_one_constraint.reserve(n);
    for (const bool transpose : {false, true}) {
      for (int i = 0; i < n; ++i) {
        exactly_one_constraint.clear();
        for (int j = 0; j < n; ++j) {
          exactly_one_constraint.push_back(transpose ? graph[j][i]
                                                     : graph[i][j]);
        }
        model->Add(ExactlyOneConstraint(exactly_one_constraint));
      }
    }
  };
}

namespace {
bool AddAtMostOne(absl::Span<const Literal> enforcement_literals,
                  absl::Span<const Literal> literals, Model* model) {
  if (enforcement_literals.empty()) {
    return model->GetOrCreate<BinaryImplicationGraph>()->AddAtMostOne(literals);
  }
  std::vector<Literal> enforcement(enforcement_literals.begin(),
                                   enforcement_literals.end());
  std::vector<LiteralWithCoeff> cst;
  cst.reserve(literals.size());
  for (const Literal l : literals) {
    cst.emplace_back(l, Coefficient(1));
  }
  return model->GetOrCreate<SatSolver>()->AddLinearConstraint(
      /*use_lower_bound=*/false, Coefficient(0),
      /*use_upper_bound=*/true, Coefficient(1), &enforcement, &cst);
}
}  // namespace

void LoadSubcircuitConstraint(int num_nodes, absl::Span<const int> tails,
                              absl::Span<const int> heads,
                              absl::Span<const Literal> enforcement_literals,
                              absl::Span<const Literal> literals, Model* model,
                              bool multiple_subcircuit_through_zero) {
  const int num_arcs = tails.size();
  CHECK_GT(num_arcs, 0);
  CHECK_EQ(heads.size(), num_arcs);
  CHECK_EQ(literals.size(), num_arcs);

  // If a node has no outgoing or no incoming arc, the model will be unsat
  // as soon as we add the corresponding ExactlyOneConstraint().
  auto sat_solver = model->GetOrCreate<SatSolver>();

  std::vector<std::vector<Literal>> exactly_one_incoming(num_nodes);
  std::vector<std::vector<Literal>> exactly_one_outgoing(num_nodes);
  for (int arc = 0; arc < num_arcs; arc++) {
    const int tail = tails[arc];
    const int head = heads[arc];
    exactly_one_outgoing[tail].push_back(literals[arc]);
    exactly_one_incoming[head].push_back(literals[arc]);
  }
  for (int i = 0; i < exactly_one_incoming.size(); ++i) {
    if (i == 0 && multiple_subcircuit_through_zero) continue;
    if (!AddAtMostOne(enforcement_literals, exactly_one_incoming[i], model)) {
      sat_solver->NotifyThatModelIsUnsat();
      return;
    }
    model->Add(EnforcedClause(enforcement_literals, exactly_one_incoming[i]));
    if (sat_solver->ModelIsUnsat()) return;
  }
  for (int i = 0; i < exactly_one_outgoing.size(); ++i) {
    if (i == 0 && multiple_subcircuit_through_zero) continue;
    if (!AddAtMostOne(enforcement_literals, exactly_one_outgoing[i], model)) {
      sat_solver->NotifyThatModelIsUnsat();
      return;
    }
    model->Add(EnforcedClause(enforcement_literals, exactly_one_outgoing[i]));
    if (sat_solver->ModelIsUnsat()) return;
  }

  CircuitPropagator::Options options;
  options.multiple_subcircuit_through_zero = multiple_subcircuit_through_zero;
  model->TakeOwnership(new CircuitPropagator(
      num_nodes, tails, heads, enforcement_literals, literals, options, model));

  // TODO(user): Just ignore node zero if multiple_subcircuit_through_zero is
  // true.
  // TODO(user): add support for enforcement literals in
  // AllDifferentConstraint?
  if (model->GetOrCreate<SatParameters>()->use_all_different_for_circuit() &&
      enforcement_literals.empty() && !multiple_subcircuit_through_zero) {
    AllDifferentConstraint* constraint =
        new AllDifferentConstraint(num_nodes, tails, heads, literals, model);
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  }
}

std::function<void(Model*)> CircuitCovering(
    absl::Span<const std::vector<Literal>> graph,
    absl::Span<const int> distinguished_nodes) {
  return [=,
          distinguished_nodes = std::vector<int>(distinguished_nodes.begin(),
                                                 distinguished_nodes.end()),
          graph = std::vector<std::vector<Literal>>(
              graph.begin(), graph.end())](Model* model) {
    CircuitCoveringPropagator* constraint =
        new CircuitCoveringPropagator(graph, distinguished_nodes, model);
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

}  // namespace sat
}  // namespace operations_research

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
    const std::vector<std::vector<LiteralIndex>>& graph, Options options,
    Trail* trail)
    : num_nodes_(graph.size()),
      options_(options),
      trail_(trail),
      propagation_trail_index_(0) {
  // TODO(user): add a way to properly handle trivially UNSAT cases.
  // For now we just check that they don't occur at construction.
  CHECK_GT(num_nodes_, 1)
      << "Trivial or UNSAT constraint, shouldn't be constructed!";
  next_.resize(num_nodes_, -1);
  prev_.resize(num_nodes_, -1);
  next_literal_.resize(num_nodes_);
  self_arcs_.resize(num_nodes_);
  std::unordered_map<LiteralIndex, int> literal_to_watch_index;
  const VariablesAssignment& assignment = trail->Assignment();
  for (int tail = 0; tail < num_nodes_; ++tail) {
    self_arcs_[tail] = graph[tail][tail];
    for (int head = 0; head < num_nodes_; ++head) {
      const LiteralIndex index = graph[tail][head];
      // Note that we need to test for both "special" cases before we can
      // call assignment.LiteralIsTrue() or LiteralIsFalse().
      if (index == kFalseLiteralIndex) continue;
      if (index == kTrueLiteralIndex ||
          assignment.LiteralIsTrue(Literal(index))) {
        CHECK_EQ(next_[tail], -1)
            << "Trivially UNSAT or duplicate arcs while adding " << tail
            << " -> " << head;
        CHECK_EQ(prev_[head], -1)
            << "Trivially UNSAT or duplicate arcs while adding " << tail
            << " -> " << head;
        AddArc(tail, head, kNoLiteralIndex);
        continue;
      }
      if (assignment.LiteralIsFalse(Literal(index))) continue;

      int watch_index_ = FindWithDefault(literal_to_watch_index, index, -1);
      if (watch_index_ == -1) {
        watch_index_ = watch_index_to_literal_.size();
        literal_to_watch_index[index] = watch_index_;
        watch_index_to_literal_.push_back(Literal(index));
        watch_index_to_arcs_.push_back(std::vector<Arc>());
      }
      watch_index_to_arcs_[watch_index_].push_back({tail, head});
    }
  }
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

void CircuitPropagator::FillConflictFromCircuitAt(int start) {
  std::vector<Literal>* conflict = trail_->MutableConflict();
  conflict->clear();
  int node = start;
  do {
    CHECK_NE(node, -1);
    if (next_literal_[node] != kNoLiteralIndex) {
      conflict->push_back(Literal(next_literal_[node]).Negated());
    }
    node = next_[node];
  } while (node != start);
}

bool CircuitPropagator::Propagate() { return true; }

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
      // Get rid of the trivial conflicts:
      // - At most one incoming and one ougtoing arc for each nodes.
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

      // Circuit?
      in_circuit_.assign(num_nodes_, false);
      in_circuit_[arc.tail] = true;
      int size = 1;
      int node = arc.head;
      while (node != arc.tail && node != -1) {
        in_circuit_[node] = true;
        node = next_[node];
        size++;
      }

      if (options_.multiple_subcircuit_through_zero) {
        // If we reached zero, this is a valid path provided that we can
        // reach the beginning of the path from zero. Note that we only check
        // the basic case that the beginning of the path must have "open" arcs
        // thanks to ExactlyOnePerRowAndPerColumn().
        if (node == 0 || node != arc.tail) continue;

        // We have a cycle not touching zero, this is a conflict.
        FillConflictFromCircuitAt(arc.tail);
        return false;
      }

      if (node != arc.tail) continue;

      // We have one circuit.
      if (size == num_nodes_) return true;
      if (size == 1) continue;  // self-arc.

      // HACK: we can reuse the conflict vector even though we don't have a
      // conflict.
      FillConflictFromCircuitAt(arc.tail);
      BooleanVariable variable_with_same_reason = kNoBooleanVariable;

      // We can propagate all the other nodes to point to themselves.
      // If this is not already the case, we have a conflict.
      for (int node = 0; node < num_nodes_; ++node) {
        if (in_circuit_[node] || next_[node] == node) continue;
        if (next_[node] != -1) {
          std::vector<Literal>* conflict = trail_->MutableConflict();
          if (next_literal_[node] != kNoLiteralIndex) {
            conflict->push_back(Literal(next_literal_[node]).Negated());
          }
          return false;
        } else if (self_arcs_[node] == kFalseLiteralIndex) {
          return false;
        } else {
          DCHECK_NE(self_arcs_[node], kTrueLiteralIndex);
          const Literal literal(self_arcs_[node]);

          // We may not have processed this literal yet.
          if (trail_->Assignment().LiteralIsTrue(literal)) continue;
          if (trail_->Assignment().LiteralIsFalse(literal)) {
            std::vector<Literal>* conflict = trail_->MutableConflict();
            conflict->push_back(literal);
            return false;
          }

          // Propagate.
          if (variable_with_same_reason == kNoBooleanVariable) {
            variable_with_same_reason = literal.Variable();
            const int index = trail_->Index();
            trail_->Enqueue(literal, AssignmentType::kCachedReason);
            *trail_->GetVectorToStoreReason(index) = *trail_->MutableConflict();
            trail_->NotifyThatReasonIsCached(literal.Variable());
          } else {
            trail_->EnqueueWithSameReasonAs(literal, variable_with_same_reason);
          }
        }
      }
    }
  }
  return true;
}

void CircuitPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (int w = 0; w < watch_index_to_literal_.size(); ++w) {
    watcher->WatchLiteral(watch_index_to_literal_[w], id, w);
  }
  watcher->RegisterReversibleClass(id, this);
  watcher->RegisterReversibleInt(id, &propagation_trail_index_);
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

}  // namespace sat
}  // namespace operations_research

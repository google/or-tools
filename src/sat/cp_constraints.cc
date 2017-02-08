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

#include "sat/cp_constraints.h"
#include "util/sort.h"

#include "base/map_util.h"

namespace operations_research {
namespace sat {

bool BooleanXorPropagator::Propagate() {
  bool sum = false;
  int unassigned_index = -1;
  for (int i = 0; i < literals_.size(); ++i) {
    const Literal l = literals_[i];
    if (trail_->Assignment().LiteralIsFalse(l)) {
      sum ^= false;
    } else if (trail_->Assignment().LiteralIsTrue(l)) {
      sum ^= true;
    } else {
      // If we have more than one unassigned literal, we can't deduce anything.
      if (unassigned_index != -1) return true;
      unassigned_index = i;
    }
  }

  // Propagates?
  if (unassigned_index != -1) {
    literal_reason_.clear();
    for (int i = 0; i < literals_.size(); ++i) {
      if (i == unassigned_index) continue;
      const Literal l = literals_[i];
      literal_reason_.push_back(
          trail_->Assignment().LiteralIsFalse(l) ? l : l.Negated());
    }
    const Literal u = literals_[unassigned_index];
    integer_trail_->EnqueueLiteral(sum == value_ ? u.Negated() : u,
                                   literal_reason_, {});
    return true;
  }

  // Ok.
  if (sum == value_) return true;

  // Conflict.
  std::vector<Literal>* conflict = trail_->MutableConflict();
  conflict->clear();
  for (int i = 0; i < literals_.size(); ++i) {
    const Literal l = literals_[i];
    conflict->push_back(trail_->Assignment().LiteralIsFalse(l) ? l
                                                               : l.Negated());
  }
  return false;
}

void BooleanXorPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (const Literal& l : literals_) {
    watcher->WatchLiteral(l, id);
    watcher->WatchLiteral(l.Negated(), id);
  }
}

AllDifferentBoundsPropagator::AllDifferentBoundsPropagator(
    const std::vector<IntegerVariable>& vars, IntegerTrail* integer_trail)
    : vars_(vars), integer_trail_(integer_trail), num_calls_(0) {
  for (int i = 0; i < vars.size(); ++i) {
    negated_vars_.push_back(NegationOf(vars_[i]));
  }
}

bool AllDifferentBoundsPropagator::Propagate() {
  if (vars_.empty()) return true;
  if (!PropagateLowerBounds()) return false;

  // Note that it is not required to swap back vars_ and negated_vars_.
  // TODO(user): investigate the impact.
  std::swap(vars_, negated_vars_);
  const bool result = PropagateLowerBounds();
  std::swap(vars_, negated_vars_);
  return result;
}

// TODO(user): we could gain by pushing all the new bound at the end, so that
// we just have to sort to_insert_ once.
void AllDifferentBoundsPropagator::FillHallReason(IntegerValue hall_lb,
                                                  IntegerValue hall_ub) {
  for (auto entry : to_insert_) {
    value_to_variable_[entry.first] = entry.second;
  }
  to_insert_.clear();
  integer_reason_.clear();
  for (int64 v = hall_lb.value(); v <= hall_ub; ++v) {
    const IntegerVariable var = FindOrDie(value_to_variable_, v);
    integer_reason_.push_back(IntegerLiteral::GreaterOrEqual(var, hall_lb));
    integer_reason_.push_back(IntegerLiteral::LowerOrEqual(var, hall_ub));
  }
}

bool AllDifferentBoundsPropagator::PropagateLowerBounds() {
  ++num_calls_;
  critical_intervals_.clear();
  hall_starts_.clear();
  hall_ends_.clear();

  to_insert_.clear();
  if (num_calls_ % 20 == 0) {
    // We don't really need to clear this, but we do from time to time to
    // save memory (in case the variable domains are huge). This optimization
    // helps a bit.
    value_to_variable_.clear();
  }

  // Loop over the variables by increasing ub.
  IncrementalSort(
      vars_.begin(), vars_.end(), [this](IntegerVariable a, IntegerVariable b) {
        return integer_trail_->UpperBound(a) < integer_trail_->UpperBound(b);
      });
  for (const IntegerVariable var : vars_) {
    const IntegerValue lb = integer_trail_->LowerBound(var);

    // Check if lb is in an Hall interval, and push it if this is the case.
    const int hall_index =
        std::lower_bound(hall_ends_.begin(), hall_ends_.end(), lb) -
        hall_ends_.begin();
    if (hall_index < hall_ends_.size() && hall_starts_[hall_index] <= lb) {
      const IntegerValue hs = hall_starts_[hall_index];
      const IntegerValue he = hall_ends_[hall_index];
      FillHallReason(hs, he);
      integer_reason_.push_back(IntegerLiteral::GreaterOrEqual(var, hs));
      if (!integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(var, he + 1),
                                   /*literal_reason=*/{}, integer_reason_)) {
        return false;
      }
    }

    // Updates critical_intervals_. Note that we use the old lb, but that
    // doesn't change the value of newly_covered. This block is what takes the
    // most time.
    int64 newly_covered;
    const auto it =
        critical_intervals_.GrowRightByOne(lb.value(), &newly_covered);
    to_insert_.push_back({newly_covered, var});
    const IntegerValue end(it->end);

    // We cannot have a conflict, because it should have beend detected before
    // by pushing an interval lower bound past its upper bound.
    DCHECK_LE(end, integer_trail_->UpperBound(var));

    // If we have a new Hall interval, add it to the set. Note that it will
    // always be last, and if it overlaps some previous Hall intervals, it
    // always overlaps them fully.
    if (end == integer_trail_->UpperBound(var)) {
      const IntegerValue start(it->start);
      while (!hall_starts_.empty() && start <= hall_starts_.back()) {
        hall_starts_.pop_back();
        hall_ends_.pop_back();
      }
      DCHECK(hall_ends_.empty() || hall_ends_.back() < start);
      hall_starts_.push_back(start);
      hall_ends_.push_back(end);
    }
  }
  return true;
}

void AllDifferentBoundsPropagator::RegisterWith(
    GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (const IntegerVariable& var : vars_) {
    watcher->WatchIntegerVariable(var, id);
  }
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
}

std::function<void(Model*)> AllDifferent(
    const std::vector<IntegerVariable>& vars) {
  return [=](Model* model) {
    hash_set<IntegerValue> fixed_values;

    // First, we fully encode all the given integer variables.
    IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
    for (const IntegerVariable var : vars) {
      if (!encoder->VariableIsFullyEncoded(var)) {
        const IntegerValue lb(model->Get(LowerBound(var)));
        const IntegerValue ub(model->Get(UpperBound(var)));
        if (lb == ub) {
          fixed_values.insert(lb);
        } else {
          encoder->FullyEncodeVariable(var, lb, ub);
        }
      }
    }

    // Then we construct a mapping value -> List of literal each indicating
    // that a given variable takes this value.
    hash_map<IntegerValue, std::vector<Literal>> value_to_literals;
    for (const IntegerVariable var : vars) {
      if (!encoder->VariableIsFullyEncoded(var)) continue;
      for (const auto& entry : encoder->FullDomainEncoding(var)) {
        value_to_literals[entry.value].push_back(entry.literal);
      }
    }

    // Finally, we add an at most one constraint for each value.
    for (const auto& entry : value_to_literals) {
      if (ContainsKey(fixed_values, entry.first)) {
        LOG(WARNING) << "Case could be presolved.";
        // Fix all the literal to false!
        SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
        for (const Literal l : entry.second) {
          sat_solver->AddUnitClause(l.Negated());
        }
      } else if (entry.second.size() > 1) {
        model->Add(AtMostOneConstraint(entry.second));
      }
    }
  };
}

CircuitPropagator::CircuitPropagator(
    const std::vector<std::vector<LiteralIndex>>& graph, bool allow_subcircuit,
    Trail* trail)
    : num_nodes_(graph.size()),
      allow_subcircuit_(allow_subcircuit),
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
  hash_map<LiteralIndex, int> literal_to_watch_index;
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
        if (!allow_subcircuit) {
          CHECK_NE(tail, head) << "Trivially UNSAT.";
        }
        CHECK_EQ(next_[tail], -1) << "Trivially UNSAT or duplicate arcs.";
        CHECK_EQ(prev_[tail], -1) << "Trivially UNSAT or duplicate arcs.";
        next_[tail] = head;
        prev_[head] = tail;
        next_literal_[tail] = kNoLiteralIndex;
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

bool CircuitPropagator::IncrementalPropagate(
    const std::vector<int>& watch_indices) {
  for (const int w : watch_indices) {
    const Literal literal = watch_index_to_literal_[w];
    for (const Arc arc : watch_index_to_arcs_[w]) {
      // Get rid of the trivial conflicts:
      // - At most one incoming and one ougtoing arc for each nodes.
      // - No self arc if allow_subcircuit_ is false
      std::vector<Literal>* conflict = trail_->MutableConflict();
      if (arc.tail == arc.head && !allow_subcircuit_) {
        *conflict = {literal.Negated()};
        return false;
      }
      if (next_[arc.tail] != -1) {
        if (next_literal_[arc.tail] != kNoLiteralIndex) {
          *conflict = {Literal(next_literal_[arc.tail]).Negated(),
                       literal.Negated()};
        } else {
          *conflict = {literal.Negated()};
        }
        return false;
      }
      if (prev_[arc.head] != -1) {
        if (next_literal_[prev_[arc.head]] != kNoLiteralIndex) {
          *conflict = {Literal(next_literal_[prev_[arc.head]]).Negated(),
                       literal.Negated()};
        } else {
          *conflict = {literal.Negated()};
        }
        return false;
      }

      // Add the arc.
      added_arcs_.push_back(arc);
      next_[arc.tail] = arc.head;
      prev_[arc.head] = arc.tail;
      next_literal_[arc.tail] = literal.Index();

      // Circuit?
      if (allow_subcircuit_) {
        in_circuit_.assign(num_nodes_, false);
        in_circuit_[arc.tail] = true;
      }
      int node = arc.head;
      int size = 1;
      while (node != arc.tail && node != -1) {
        if (allow_subcircuit_) in_circuit_[node] = true;
        node = next_[node];
        size++;
      }
      if (node != arc.tail) continue;

      // We have one circuit.
      if (size == num_nodes_) return true;
      if (!allow_subcircuit_) {
        FillConflictFromCircuitAt(arc.tail);
        return false;
      }

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

// ----- CpPropagator -----

CpPropagator::CpPropagator(IntegerTrail* integer_trail)
    : integer_trail_(integer_trail) {}

CpPropagator::~CpPropagator() {}

IntegerValue CpPropagator::Min(IntegerVariable v) const {
  return integer_trail_->LowerBound(v);
}

IntegerValue CpPropagator::Max(IntegerVariable v) const {
  return integer_trail_->UpperBound(v);
}

bool CpPropagator::SetMin(IntegerVariable v, IntegerValue val,
                          const std::vector<IntegerLiteral>& reason) {
  IntegerValue current_min = Min(v);
  if (val > current_min &&
      !integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(v, val),
                               /*literal_reason=*/{}, reason)) {
    return false;
  }
  return true;
}

bool CpPropagator::SetMax(IntegerVariable v, IntegerValue val,
                          const std::vector<IntegerLiteral>& reason) {
  const IntegerValue current_max = Max(v);
  if (val < current_max &&
      !integer_trail_->Enqueue(IntegerLiteral::LowerOrEqual(v, val),
                               /*literal_reason=*/{}, reason)) {
    return false;
  }
  return true;
}

bool CpPropagator::SetMin(IntegerValue v, IntegerValue val,
                          const std::vector<IntegerLiteral>& reason) {
  if (val > v) {
    return integer_trail_->ReportConflict(reason);
  }
  return true;
}

bool CpPropagator::SetMax(IntegerValue v, IntegerValue val,
                          const std::vector<IntegerLiteral>& reason) {
  if (val < v) {
    return integer_trail_->ReportConflict(reason);
  }
  return true;
}

void CpPropagator::AddLowerBoundReason(
    IntegerVariable v, std::vector<IntegerLiteral>* reason) const {
  reason->push_back(integer_trail_->LowerBoundAsLiteral(v));
}

void CpPropagator::AddUpperBoundReason(
    IntegerVariable v, std::vector<IntegerLiteral>* reason) const {
  reason->push_back(integer_trail_->UpperBoundAsLiteral(v));
}

void CpPropagator::AddBoundsReason(IntegerVariable v,
                                   std::vector<IntegerLiteral>* reason) const {
  reason->push_back(integer_trail_->LowerBoundAsLiteral(v));
  reason->push_back(integer_trail_->UpperBoundAsLiteral(v));
}

template <class S>
NonOverlappingRectanglesPropagator<S>::NonOverlappingRectanglesPropagator(
    const std::vector<IntegerVariable>& x,
    const std::vector<IntegerVariable>& y, const std::vector<S>& dx,
    const std::vector<S>& dy, bool strict, IntegerTrail* integer_trail)
    : CpPropagator(integer_trail),
      x_(x),
      y_(y),
      dx_(dx),
      dy_(dy),
      strict_(strict) {
  const int n = x.size();
  CHECK_GT(n, 0);
  neighbors_.resize(n * (n - 1));
  neighbors_begins_.resize(n);
  neighbors_ends_.resize(n);
  for (int i = 0; i < n; ++i) {
    const int begin = i * (n - 1);
    neighbors_begins_[i] = begin;
    neighbors_ends_[i] = begin + (n - 1);
    for (int j = 0; j < n; ++j) {
      if (j == i) continue;
      neighbors_[begin + (j > i ? j - 1 : j)] = j;
    }
  }
}

template <class S>
NonOverlappingRectanglesPropagator<S>::~NonOverlappingRectanglesPropagator() {}

template <class S>
bool NonOverlappingRectanglesPropagator<S>::Propagate() {
  const int n = x_.size();
  cached_areas_.resize(n);
  cached_min_end_x_.resize(n);
  cached_min_end_y_.resize(n);
  for (int box = 0; box < n; ++box) {
    // We never change the min-size of a box, so this stays valid.
    cached_areas_[box] = Min(dx_[box]) * Min(dy_[box]);

    // These will be update on each push.
    cached_min_end_x_[box] = Min(x_[box]) + Min(dx_[box]);
    cached_min_end_y_[box] = Min(y_[box]) + Min(dy_[box]);
  }

  while (true) {
    const int64 saved_stamp = integer_trail_->num_enqueues();
    for (int box = 0; box < n; ++box) {
      if (!strict_ && cached_areas_[box] == 0) continue;

      UpdateNeighbors(box);
      if (!FailWhenEnergyIsTooLarge(box)) return false;

      const int end = neighbors_ends_[box];
      for (int i = neighbors_begins_[box]; i < end; ++i) {
        if (!PushOneBox(box, neighbors_[i])) return false;
      }
    }
    if (saved_stamp == integer_trail_->num_enqueues()) break;
  }
  return true;
}

template <class S>
void NonOverlappingRectanglesPropagator<S>::RegisterWith(
    GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (const IntegerVariable& var : x_) watcher->WatchIntegerVariable(var, id);
  for (const IntegerVariable& var : y_) watcher->WatchIntegerVariable(var, id);
  for (const S& var : dx_) watcher->WatchLowerBound(var, id);
  for (const S& var : dy_) watcher->WatchLowerBound(var, id);
  for (int i = 0; i < x_.size(); ++i) {
    watcher->RegisterReversibleInt(id, &neighbors_ends_[i]);
  }
}

template <class S>
void NonOverlappingRectanglesPropagator<S>::AddBoxReason(int box) {
  AddBoundsReason(x_[box], &integer_reason_);
  AddBoundsReason(y_[box], &integer_reason_);
  AddLowerBoundReason(dx_[box], &integer_reason_);
  AddLowerBoundReason(dy_[box], &integer_reason_);
}

template <class S>
void NonOverlappingRectanglesPropagator<S>::AddBoxReason(int box,
                                                         IntegerValue xmin,
                                                         IntegerValue xmax,
                                                         IntegerValue ymin,
                                                         IntegerValue ymax) {
  integer_reason_.push_back(IntegerLiteral::GreaterOrEqual(x_[box], xmin));
  integer_reason_.push_back(
      IntegerLiteral::LowerOrEqual(x_[box], xmax - Min(dx_[box])));
  integer_reason_.push_back(IntegerLiteral::GreaterOrEqual(y_[box], ymin));
  integer_reason_.push_back(
      IntegerLiteral::LowerOrEqual(y_[box], ymax - Min(dy_[box])));
  AddLowerBoundReason(dx_[box], &integer_reason_);
  AddLowerBoundReason(dy_[box], &integer_reason_);
}

namespace {

// Returns true iff the 2 given intervals are disjoint. If their union is one
// point, this also returns true.
bool IntervalAreDisjointForSure(IntegerValue min_a, IntegerValue max_a,
                                IntegerValue min_b, IntegerValue max_b) {
  return min_a >= max_b || min_b >= max_a;
}

// Returns the distance from interval a to the "bounding interval" of a and b.
IntegerValue DistanceToBoundingInterval(IntegerValue min_a, IntegerValue max_a,
                                        IntegerValue min_b,
                                        IntegerValue max_b) {
  return std::max(min_a - min_b, max_b - max_a);
}

}  // namespace

template <class S>
void NonOverlappingRectanglesPropagator<S>::UpdateNeighbors(int box) {
  tmp_removed_.clear();
  cached_distance_to_bounding_box_.resize(x_.size());
  const IntegerValue box_x_min = Min(x_[box]);
  const IntegerValue box_x_max = Max(x_[box]) + Max(dx_[box]);
  const IntegerValue box_y_min = Min(y_[box]);
  const IntegerValue box_y_max = Max(y_[box]) + Max(dy_[box]);
  int new_index = neighbors_begins_[box];
  const int end = neighbors_ends_[box];
  for (int i = new_index; i < end; ++i) {
    const int other = neighbors_[i];

    const IntegerValue other_x_min = Min(x_[other]);
    const IntegerValue other_x_max = Max(x_[other]) + Max(dx_[other]);
    if (IntervalAreDisjointForSure(box_x_min, box_x_max, other_x_min,
                                   other_x_max)) {
      tmp_removed_.push_back(other);
      continue;
    }

    const IntegerValue other_y_min = Min(y_[other]);
    const IntegerValue other_y_max = Max(y_[other]) + Max(dy_[other]);
    if (IntervalAreDisjointForSure(box_y_min, box_y_max, other_y_min,
                                   other_y_max)) {
      tmp_removed_.push_back(other);
      continue;
    }

    neighbors_[new_index++] = other;
    cached_distance_to_bounding_box_[other] =
        std::max(DistanceToBoundingInterval(box_x_min, box_x_max, other_x_min,
                                            other_x_max),
                 DistanceToBoundingInterval(box_y_min, box_y_max, other_y_min,
                                            other_y_max));
  }
  neighbors_ends_[box] = new_index;
  for (int i = 0; i < tmp_removed_.size();) {
    neighbors_[new_index++] = tmp_removed_[i++];
  }
  IncrementalSort(neighbors_.begin() + neighbors_begins_[box],
                  neighbors_.begin() + neighbors_ends_[box],
                  [this](int i, int j) {
                    return cached_distance_to_bounding_box_[i] <
                           cached_distance_to_bounding_box_[j];
                  });
}

template <class S>
bool NonOverlappingRectanglesPropagator<S>::FailWhenEnergyIsTooLarge(int box) {
  // Note that we only consider the smallest dimension of each boxes here.
  IntegerValue area_min_x = Min(x_[box]);
  IntegerValue area_max_x = Max(x_[box]) + Min(dx_[box]);
  IntegerValue area_min_y = Min(y_[box]);
  IntegerValue area_max_y = Max(y_[box]) + Min(dy_[box]);
  IntegerValue sum_of_areas = cached_areas_[box];

  IntegerValue total_sum_of_areas = sum_of_areas;
  const int end = neighbors_ends_[box];
  for (int i = neighbors_begins_[box]; i < end; ++i) {
    const int other = neighbors_[i];
    total_sum_of_areas += cached_areas_[other];
  }

  // TODO(user): Is there a better order, maybe sort by distance
  // with the current box.
  for (int i = neighbors_begins_[box]; i < end; ++i) {
    const int other = neighbors_[i];
    if (cached_areas_[other] == 0) continue;

    // Update Bounding box.
    area_min_x = std::min(area_min_x, Min(x_[other]));
    area_max_x = std::max(area_max_x, Max(x_[other]) + Min(dx_[other]));
    area_min_y = std::min(area_min_y, Min(y_[other]));
    area_max_y = std::max(area_max_y, Max(y_[other]) + Min(dy_[other]));

    // Update sum of areas.
    sum_of_areas += cached_areas_[other];
    const IntegerValue bounding_area =
        (area_max_x - area_min_x) * (area_max_y - area_min_y);
    if (bounding_area >= total_sum_of_areas) {
      // Nothing will be deduced. Exiting.
      return true;
    }

    if (sum_of_areas > bounding_area) {
      integer_reason_.clear();
      AddBoxReason(box, area_min_x, area_max_x, area_min_y, area_max_y);
      for (int j = neighbors_begins_[box]; j <= i; ++j) {
        const int other = neighbors_[j];
        if (cached_areas_[other] == 0) continue;
        AddBoxReason(other, area_min_x, area_max_x, area_min_y, area_max_y);
      }
      return integer_trail_->ReportConflict(integer_reason_);
    }
  }
  return true;
}

template <class S>
bool NonOverlappingRectanglesPropagator<S>::FirstBoxIsBeforeSecondBox(
    const std::vector<IntegerVariable>& pos, const std::vector<S>& size,
    int box, int other, std::vector<IntegerValue>* min_end) {
  const IntegerValue other_min_pos = (*min_end)[box];
  if (other_min_pos > Min(pos[other])) {
    if (integer_reason_.empty()) {
      AddBoxReason(box);
      AddBoxReason(other);
    }
    (*min_end)[other] = other_min_pos + Min(size[other]);
    if (!SetMin(pos[other], other_min_pos, integer_reason_)) return false;
  }
  const IntegerValue box_max_pos = Max(pos[other]) - Min(size[box]);
  if (box_max_pos < Max(pos[box])) {
    if (integer_reason_.empty()) {
      AddBoxReason(box);
      AddBoxReason(other);
    }
    if (!SetMax(pos[box], box_max_pos, integer_reason_)) return false;
  }
  const IntegerValue box_max_size = Max(pos[other]) - Min(pos[box]);
  if (box_max_size < Max(size[box])) {
    if (integer_reason_.empty()) {
      AddBoxReason(box);
      AddBoxReason(other);
    }
    if (!SetMax(size[box], box_max_size, integer_reason_)) return false;
  }
  return true;
}

template <class S>
bool NonOverlappingRectanglesPropagator<S>::PushOneBox(int box, int other) {
  if (!strict_ && cached_areas_[other] == 0) return true;

  // For each direction and each order, we test if the boxes can be disjoint.
  const int state = (cached_min_end_x_[box] <= Max(x_[other])) +
                    2 * (cached_min_end_x_[other] <= Max(x_[box])) +
                    4 * (cached_min_end_y_[box] <= Max(y_[other])) +
                    8 * (cached_min_end_y_[other] <= Max(y_[box]));

  // This is an "hack" to be able to easily test for none or for one
  // and only one of the conditions below.
  integer_reason_.clear();
  switch (state) {
    case 0: {
      AddBoxReason(box);
      AddBoxReason(other);
      return integer_trail_->ReportConflict(integer_reason_);
    }
    case 1:
      return FirstBoxIsBeforeSecondBox(x_, dx_, box, other, &cached_min_end_x_);
    case 2:
      return FirstBoxIsBeforeSecondBox(x_, dx_, other, box, &cached_min_end_x_);
    case 4:
      return FirstBoxIsBeforeSecondBox(y_, dy_, box, other, &cached_min_end_y_);
    case 8:
      return FirstBoxIsBeforeSecondBox(y_, dy_, other, box, &cached_min_end_y_);
    default:
      break;
  }
  return true;
}

template class NonOverlappingRectanglesPropagator<IntegerVariable>;
template class NonOverlappingRectanglesPropagator<IntegerValue>;

OneOfVarMinPropagator::OneOfVarMinPropagator(
    IntegerVariable target_var, const std::vector<IntegerVariable>& vars,
    const std::vector<Literal>& selectors, Trail* trail,
    IntegerTrail* integer_trail)
    : target_var_(target_var),
      vars_(vars),
      selectors_(selectors),
      trail_(trail),
      integer_trail_(integer_trail) {}

bool OneOfVarMinPropagator::Propagate() {
  // Compute the min of the lower-bound for the still possible variables.
  IntegerValue target_min = kMaxIntegerValue;
  for (int i = 0; i < vars_.size(); ++i) {
    if (trail_->Assignment().LiteralIsTrue(selectors_[i])) return true;
    if (trail_->Assignment().LiteralIsFalse(selectors_[i])) continue;
    target_min = std::min(target_min, integer_trail_->LowerBound(vars_[i]));
  }
  if (target_min == kMaxIntegerValue) {
    // All false, conflit.
    *(trail_->MutableConflict()) = selectors_;
    return false;
  }

  // Increase the min of the target var if we got a better bound.
  if (target_min <= integer_trail_->LowerBound(target_var_)) return true;
  literal_reason_.clear();
  integer_reason_.clear();
  for (int i = 0; i < vars_.size(); ++i) {
    if (trail_->Assignment().LiteralIsFalse(selectors_[i])) {
      literal_reason_.push_back(selectors_[i]);
    } else {
      integer_reason_.push_back(
          IntegerLiteral::GreaterOrEqual(vars_[i], target_min));
    }
  }
  return integer_trail_->Enqueue(
      IntegerLiteral::GreaterOrEqual(target_var_, target_min), literal_reason_,
      integer_reason_);
}

void OneOfVarMinPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (const Literal l : selectors_) watcher->WatchLiteral(l.Negated(), id);
  for (const IntegerVariable v : vars_) watcher->WatchLowerBound(v, id);
}

std::function<void(Model*)> ExactlyOnePerRowAndPerColumn(
    const std::vector<std::vector<LiteralIndex>>& square_matrix) {
  return [=](Model* model) {
    int n = square_matrix.size();
    int num_trivially_false = 0;
    Trail* trail = model->GetOrCreate<Trail>();
    std::vector<Literal> exactly_one_constraint;
    for (const bool transpose : {false, true}) {
      for (int i = 0; i < n; ++i) {
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

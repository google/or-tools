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

  // Unfortunately, we may not reach a fixed-point in one pass.
  // TODO(user): can we change the code to achieve this?
  while (true) {
    const int64 old_timestamp = integer_trail_->num_enqueues();

    if (!PropagateLowerBounds()) return false;

    // Note that it is not required to swap back vars_ and negated_vars_.
    // TODO(user): investigate the impact.
    std::swap(vars_, negated_vars_);
    const bool result = PropagateLowerBounds();
    std::swap(vars_, negated_vars_);
    if (!result) return false;

    if (old_timestamp == integer_trail_->num_enqueues()) break;
  }
  return true;
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
  std::sort(
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

// NonOverlappingRectanglesPropagator

#define RETURN_IF_FALSE(x) \
  if (!(x)) return false;

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
      strict_(strict) {}

template <class S>
NonOverlappingRectanglesPropagator<S>::~NonOverlappingRectanglesPropagator() {}

template <class S>
bool NonOverlappingRectanglesPropagator<S>::Propagate() {
  for (int box = 0; box < x_.size(); ++box) {
    FillNeighbors(box);
    RETURN_IF_FALSE(FailWhenEnergyIsTooLarge(box));
    for (int i = 0; i < neighbors_.size(); ++i) {
      RETURN_IF_FALSE(PushOneBox(box, neighbors_[i]));
    }
  }
  return true;
}

template <class S>
void NonOverlappingRectanglesPropagator<S>::RegisterWith(
    GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (const IntegerVariable& var : x_) {
    watcher->WatchIntegerVariable(var, id);
  }
  for (const IntegerVariable& var : y_) {
    watcher->WatchIntegerVariable(var, id);
  }
  for (const S& var : dx_) {
    watcher->WatchIntegerVariable(var, id);
  }
  for (const S& var : dy_) {
    watcher->WatchIntegerVariable(var, id);
  }
}

template <class S>
void NonOverlappingRectanglesPropagator<S>::AddBoxReason(int box) {
  AddBoundsReason(x_[box], &integer_reason_);
  AddBoundsReason(y_[box], &integer_reason_);
  AddBoundsReason(dx_[box], &integer_reason_);
  AddBoundsReason(dy_[box], &integer_reason_);
}

template <class S>
bool NonOverlappingRectanglesPropagator<S>::CanBoxesOverlap(int i,
                                                            int j) const {
  if (AreBoxesDisjoingHorizontallyForSure(i, j) ||
      AreBoxesDisjoingVerticallyForSure(i, j)) {
    return false;
  }
  return true;
}

template <class S>
bool NonOverlappingRectanglesPropagator<S>::AreBoxesDisjoingHorizontallyForSure(
    int i, int j) const {
  return Min(x_[i]) >= Max(x_[j]) + Max(dx_[j]) ||
         Min(x_[j]) >= Max(x_[i]) + Max(dx_[i]) ||
         (!strict_ && (Min(dx_[i]) == 0 || Min(dx_[j]) == 0));
}

template <class S>
bool NonOverlappingRectanglesPropagator<S>::AreBoxesDisjoingVerticallyForSure(
    int i, int j) const {
  return Min(y_[i]) >= Max(y_[j]) + Max(dy_[j]) ||
         Min(y_[j]) >= Max(y_[i]) + Max(dy_[i]) ||
         (!strict_ && (Min(dy_[i]) == 0 || Min(dx_[j]) == 0));
}

template <class S>
IntegerValue NonOverlappingRectanglesPropagator<S>::DistanceToBoundingBox(
    int box, int other) {
  IntegerValue distance = Min(x_[box]) - Min(x_[other]);
  distance =
      std::max(distance, Max(x_[other]) + Max(dx_[other]) - Max(x_[box]));
  distance = std::max(distance, Min(y_[box]) - Min(y_[other]));
  distance =
      std::max(distance, Max(y_[other]) + Max(dy_[other]) - Max(y_[box]));
  return distance;
}

template <class S>
void NonOverlappingRectanglesPropagator<S>::FillNeighbors(int box) {
  // TODO(user): We could maintain a non reversible list of
  // neighbors and clean it after each failure.
  neighbors_.clear();
  for (int other = 0; other < x_.size(); ++other) {
    if (other != box && CanBoxesOverlap(other, box)) {
      neighbors_.push_back(other);
    }
  }
  std::sort(neighbors_.begin(), neighbors_.end(), [this, box](int i, int j) {
    return DistanceToBoundingBox(box, i) < DistanceToBoundingBox(box, j);
  });
}

template <class S>
bool NonOverlappingRectanglesPropagator<S>::FailWhenEnergyIsTooLarge(int box) {
  IntegerValue area_min_x = Min(x_[box]);
  IntegerValue area_max_x = Max(x_[box]) + Max(dx_[box]);
  IntegerValue area_min_y = Min(y_[box]);
  IntegerValue area_max_y = Max(y_[box]) + Max(dy_[box]);
  IntegerValue sum_of_areas = Min(dx_[box]) * Min(dy_[box]);

  IntegerValue total_sum_of_areas = sum_of_areas;
  for (int i = 0; i < neighbors_.size(); ++i) {
    const int other = neighbors_[i];
    total_sum_of_areas += Min(dx_[other]) * Min(dy_[other]);
  }

  // TODO(user): Is there a better order, maybe sort by distance
  // with the current box.
  for (int i = 0; i < neighbors_.size(); ++i) {
    const int other = neighbors_[i];
    if (Min(dx_[other]) == 0 || Min(dy_[other]) == 0) {
      // This box will not contribute. Skipping.
      continue;
    }
    // Update Bounding box.
    area_min_x = std::min(area_min_x, Min(x_[other]));
    area_max_x = std::max(area_max_x, Max(x_[other]) + Max(dx_[other]));
    area_min_y = std::min(area_min_y, Min(y_[other]));
    area_max_y = std::max(area_max_y, Max(y_[other]) + Max(dy_[other]));
    // Update sum of areas.
    sum_of_areas += Min(dx_[other]) * Min(dy_[other]);
    const IntegerValue bounding_area =
        (area_max_x - area_min_x) * (area_max_y - area_min_y);
    if (bounding_area >= total_sum_of_areas) {
      // Nothing will be deduced. Exiting.
      return true;
    }

    if (sum_of_areas > bounding_area) {
      // TODO(user): compute wider reason with relaxed bounds on boxes.
      integer_reason_.clear();
      AddBoxReason(box);
      for (int j = 0; j <= i; ++j) {
        const int other = neighbors_[j];
        if (Min(dx_[other]) == 0 || Min(dy_[other]) == 0) {
          // This box will not contribute. Skipping.
          continue;
        }
        AddBoxReason(other);
      }
      return integer_trail_->ReportConflict(integer_reason_);
    }
  }
  return true;
}

template <class S>
bool NonOverlappingRectanglesPropagator<S>::PushOneBox(int box, int other) {
  // For each direction, and each order, we test if the boxes can be disjoint.
  const int state = (Min(x_[box]) + Min(dx_[box]) <= Max(x_[other])) +
                    2 * (Min(x_[other]) + Min(dx_[other]) <= Max(x_[box])) +
                    4 * (Min(y_[box]) + Min(dy_[box]) <= Max(y_[other])) +
                    8 * (Min(y_[other]) + Min(dy_[other]) <= Max(y_[box]));
  if (state == 0 || state == 1 || state == 2 || state == 4 || state == 8) {
    // Fill reason.
    integer_reason_.clear();
    AddBoxReason(box);
    AddBoxReason(other);

    // This is an "hack" to be able to easily test for none or for one
    // and only one of the conditions below.
    //
    // TODO(user, lperron): Abstract what happen in each case in a function, it
    // is the same code with other and box swapped or with x and y swapped. Like
    // this, it is too easy to make a typo and harder to test all the cases.
    switch (state) {
      case 0: {
        return integer_trail_->ReportConflict(integer_reason_);
      }
      case 1: {  // We push other left (x increasing).
        RETURN_IF_FALSE(
            SetMin(x_[other], Min(x_[box]) + Min(dx_[box]), integer_reason_));
        RETURN_IF_FALSE(
            SetMax(x_[box], Max(x_[other]) - Min(dx_[box]), integer_reason_));
        RETURN_IF_FALSE(
            SetMax(dx_[box], Max(x_[other]) - Min(x_[box]), integer_reason_));
        break;
      }
      case 2: {  // We push other right (x decreasing).
        RETURN_IF_FALSE(
            SetMin(x_[box], Min(x_[other]) + Min(dx_[other]), integer_reason_));
        RETURN_IF_FALSE(
            SetMax(x_[other], Max(x_[box]) - Min(dx_[other]), integer_reason_));
        RETURN_IF_FALSE(
            SetMax(dx_[other], Max(x_[box]) - Min(x_[other]), integer_reason_));
        break;
      }
      case 4: {  // We push other up (y increasing).
        RETURN_IF_FALSE(
            SetMin(y_[other], Min(y_[box]) + Min(dy_[box]), integer_reason_));
        RETURN_IF_FALSE(
            SetMax(y_[box], Max(y_[other]) - Min(dy_[box]), integer_reason_));
        RETURN_IF_FALSE(
            SetMax(dy_[box], Max(y_[other]) - Min(y_[box]), integer_reason_));
        break;
      }
      case 8: {  // We push other down (y decreasing).
        RETURN_IF_FALSE(
            SetMin(y_[box], Min(y_[other]) + Min(dy_[other]), integer_reason_));
        RETURN_IF_FALSE(
            SetMax(y_[other], Max(y_[box]) - Min(dy_[other]), integer_reason_));
        RETURN_IF_FALSE(
            SetMax(dy_[other], Max(y_[box]) - Min(y_[other]), integer_reason_));
        break;
      }
      default: { break; }
    }
  }
  return true;
}
#undef RETURN_IF_FALSE

template class NonOverlappingRectanglesPropagator<IntegerVariable>;
template class NonOverlappingRectanglesPropagator<IntegerValue>;

}  // namespace sat
}  // namespace operations_research

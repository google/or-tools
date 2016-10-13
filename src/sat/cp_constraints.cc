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

bool BooleanXorPropagator::Propagate(Trail* trail) {
  bool sum = false;
  int unassigned_index = -1;
  for (int i = 0; i < literals_.size(); ++i) {
    const Literal l = literals_[i];
    if (trail->Assignment().LiteralIsFalse(l)) {
      sum ^= false;
    } else if (trail->Assignment().LiteralIsTrue(l)) {
      sum ^= true;
    } else {
      // If we have more than one unassigned literal, we can't deduce anything.
      if (unassigned_index != -1) return true;
      unassigned_index = i;
    }
  }

  // Propagates?
  if (unassigned_index != -1) {
    std::vector<Literal>* literal_reason;
    std::vector<IntegerLiteral>* integer_reason;
    const Literal u = literals_[unassigned_index];
    integer_trail_->EnqueueLiteral(sum == value_ ? u.Negated() : u,
                                   &literal_reason, &integer_reason);
    for (int i = 0; i < literals_.size(); ++i) {
      if (i == unassigned_index) continue;
      const Literal l = literals_[i];
      literal_reason->push_back(
          trail->Assignment().LiteralIsFalse(l) ? l : l.Negated());
    }
    return true;
  }

  // Ok.
  if (sum == value_) return true;

  // Conflict.
  std::vector<Literal>* conflict = trail->MutableConflict();
  conflict->clear();
  for (int i = 0; i < literals_.size(); ++i) {
    const Literal l = literals_[i];
    conflict->push_back(trail->Assignment().LiteralIsFalse(l) ? l
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

bool AllDifferentBoundsPropagator::Propagate(Trail* trail) {
  if (vars_.empty()) return true;
  if (!PropagateLowerBounds(trail)) return false;

  // Note that it is not required to swap back vars_ and negated_vars_.
  // TODO(user): investigate the impact.
  std::swap(vars_, negated_vars_);
  const bool result = PropagateLowerBounds(trail);
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

bool AllDifferentBoundsPropagator::PropagateLowerBounds(Trail* trail) {
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

std::function<void(Model*)> AllDifferent(const std::vector<IntegerVariable>& vars) {
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

}  // namespace sat
}  // namespace operations_research

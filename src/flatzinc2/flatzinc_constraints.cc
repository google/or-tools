// Copyright 2010-2013 Google
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
#include "flatzinc2/flatzinc_constraints.h"

#include "base/commandlineflags.h"
#include "constraint_solver/constraint_solveri.h"
#include "flatzinc2/model.h"
#include "flatzinc2/sat_constraint.h"
#include "util/string_array.h"

DECLARE_bool(cp_trace_search);
DECLARE_bool(cp_trace_propagation);
DECLARE_bool(use_sat);
DECLARE_bool(fz_verbose);

namespace operations_research {
namespace {
class BooleanSumOdd : public Constraint {
 public:
  BooleanSumOdd(Solver* const s, const std::vector<IntVar*>& vars)
      : Constraint(s),
        vars_(vars),
        num_possible_true_vars_(0),
        num_always_true_vars_(0) {}

  virtual ~BooleanSumOdd() {}

  virtual void Post() {
    for (int i = 0; i < vars_.size(); ++i) {
      if (!vars_[i]->Bound()) {
        Demon* const u = MakeConstraintDemon1(
            solver(), this, &BooleanSumOdd::Update, "Update", i);
        vars_[i]->WhenBound(u);
      }
    }
  }

  virtual void InitialPropagate() {
    int num_always_true = 0;
    int num_possible_true = 0;
    int possible_true_index = -1;
    for (int i = 0; i < vars_.size(); ++i) {
      const IntVar* const var = vars_[i];
      if (var->Min() == 1) {
        num_always_true++;
        num_possible_true++;
      } else if (var->Max() == 1) {
        num_possible_true++;
        possible_true_index = i;
      }
    }
    if (num_always_true == num_possible_true && num_possible_true % 2 == 0) {
      solver()->Fail();
    } else if (num_possible_true == num_always_true + 1) {
      DCHECK_NE(-1, possible_true_index);
      if (num_possible_true % 2 == 1) {
        vars_[possible_true_index]->SetMin(1);
      } else {
        vars_[possible_true_index]->SetMax(0);
      }
    }
    num_possible_true_vars_.SetValue(solver(), num_possible_true);
    num_always_true_vars_.SetValue(solver(), num_always_true);
  }

  void Update(int index) {
    DCHECK(vars_[index]->Bound());
    const int64 value = vars_[index]->Min();  // Faster than Value().
    if (value == 0) {
      num_possible_true_vars_.Decr(solver());
    } else {
      DCHECK_EQ(1, value);
      num_always_true_vars_.Incr(solver());
    }
    if (num_always_true_vars_.Value() == num_possible_true_vars_.Value() &&
        num_possible_true_vars_.Value() % 2 == 0) {
      solver()->Fail();
    } else if (num_possible_true_vars_.Value() ==
               num_always_true_vars_.Value() + 1) {
      int possible_true_index = -1;
      for (int i = 0; i < vars_.size(); ++i) {
        if (!vars_[i]->Bound()) {
          possible_true_index = i;
          break;
        }
      }
      if (possible_true_index != -1) {
        if (num_possible_true_vars_.Value() % 2 == 1) {
          vars_[possible_true_index]->SetMin(1);
        } else {
          vars_[possible_true_index]->SetMax(0);
        }
      }
    }
  }

  virtual std::string DebugString() const {
    return StringPrintf("BooleanSumOdd([%s])",
                        JoinDebugStringPtr(vars_, ", ").c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kSumEqual, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->EndVisitConstraint(ModelVisitor::kSumEqual, this);
  }

 private:
  const std::vector<IntVar*> vars_;
  NumericalRev<int> num_possible_true_vars_;
  NumericalRev<int> num_always_true_vars_;
};

class VariableParity : public Constraint {
 public:
  VariableParity(Solver* const s, IntVar* const var, bool odd)
      : Constraint(s), var_(var), odd_(odd) {}

  virtual ~VariableParity() {}

  virtual void Post() {
    if (!var_->Bound()) {
      Demon* const u = solver()->MakeConstraintInitialPropagateCallback(this);
      var_->WhenRange(u);
    }
  }

  virtual void InitialPropagate() {
    const int64 vmax = var_->Max();
    const int64 vmin = var_->Min();
    int64 new_vmax = vmax;
    int64 new_vmin = vmin;
    if (odd_) {
      if (vmax % 2 == 0) {
        new_vmax--;
      }
      if (vmin % 2 == 0) {
        new_vmin++;
      }
    } else {
      if (vmax % 2 == 1) {
        new_vmax--;
      }
      if (vmin % 2 == 1) {
        new_vmin++;
      }
    }
    var_->SetRange(new_vmin, new_vmax);
  }

  virtual std::string DebugString() const {
    return StringPrintf("VarParity(%s, %d)", var_->DebugString().c_str(), odd_);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint("VarParity", this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kVariableArgument,
                                            var_);
    visitor->VisitIntegerArgument(ModelVisitor::kValuesArgument, odd_);
    visitor->EndVisitConstraint("VarParity", this);
  }

 private:
  IntVar* const var_;
  const bool odd_;
};

class IsBooleanSumInRange : public Constraint {
 public:
  IsBooleanSumInRange(Solver* const s, const std::vector<IntVar*>& vars,
                      int64 range_min, int64 range_max, IntVar* const target)
      : Constraint(s),
        vars_(vars),
        range_min_(range_min),
        range_max_(range_max),
        target_(target),
        num_possible_true_vars_(0),
        num_always_true_vars_(0) {}

  virtual ~IsBooleanSumInRange() {}

  virtual void Post() {
    for (int i = 0; i < vars_.size(); ++i) {
      if (!vars_[i]->Bound()) {
        Demon* const u = MakeConstraintDemon1(
            solver(), this, &IsBooleanSumInRange::Update, "Update", i);
        vars_[i]->WhenBound(u);
      }
    }
    if (!target_->Bound()) {
      Demon* const u = MakeConstraintDemon0(
          solver(), this, &IsBooleanSumInRange::UpdateTarget, "UpdateTarget");
      target_->WhenBound(u);
    }
  }

  virtual void InitialPropagate() {
    int num_always_true = 0;
    int num_possible_true = 0;
    for (int i = 0; i < vars_.size(); ++i) {
      const IntVar* const var = vars_[i];
      if (var->Min() == 1) {
        num_always_true++;
        num_possible_true++;
      } else if (var->Max() == 1) {
        num_possible_true++;
      }
    }
    num_possible_true_vars_.SetValue(solver(), num_possible_true);
    num_always_true_vars_.SetValue(solver(), num_always_true);
    UpdateTarget();
  }

  void UpdateTarget() {
    if (num_always_true_vars_.Value() > range_max_ ||
        num_possible_true_vars_.Value() < range_min_) {
      inactive_.Switch(solver());
      target_->SetValue(0);
    } else if (num_always_true_vars_.Value() >= range_min_ &&
               num_possible_true_vars_.Value() <= range_max_) {
      inactive_.Switch(solver());
      target_->SetValue(1);
    } else if (target_->Min() == 1) {
      if (num_possible_true_vars_.Value() == range_min_) {
        PushAllUnboundToOne();
      } else if (num_always_true_vars_.Value() == range_max_) {
        PushAllUnboundToZero();
      }
    } else if (target_->Max() == 0) {
      if (num_possible_true_vars_.Value() == range_max_ + 1 &&
          num_always_true_vars_.Value() >= range_min_) {
        PushAllUnboundToOne();
      } else if (num_always_true_vars_.Value() == range_min_ - 1 &&
                 num_possible_true_vars_.Value() <= range_max_) {
        PushAllUnboundToZero();
      }
    }
  }

  void Update(int index) {
    if (!inactive_.Switched()) {
      DCHECK(vars_[index]->Bound());
      const int64 value = vars_[index]->Min();  // Faster than Value().
      if (value == 0) {
        num_possible_true_vars_.Decr(solver());
      } else {
        DCHECK_EQ(1, value);
        num_always_true_vars_.Incr(solver());
      }
      UpdateTarget();
    }
  }

  virtual std::string DebugString() const {
    return StringPrintf(
        "Sum([%s]) in [%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d] == %s",
        JoinDebugStringPtr(vars_, ", ").c_str(), range_min_, range_max_,
        target_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kSumEqual, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->EndVisitConstraint(ModelVisitor::kSumEqual, this);
  }

 private:
  void PushAllUnboundToZero() {
    inactive_.Switch(solver());
    int true_vars = 0;
    for (int i = 0; i < vars_.size(); ++i) {
      if (vars_[i]->Min() == 0) {
        vars_[i]->SetValue(0);
      } else {
        true_vars++;
      }
    }
    target_->SetValue(true_vars >= range_min_ && true_vars <= range_max_);
  }

  void PushAllUnboundToOne() {
    inactive_.Switch(solver());
    int true_vars = 0;
    for (int i = 0; i < vars_.size(); ++i) {
      if (vars_[i]->Max() == 1) {
        vars_[i]->SetValue(1);
        true_vars++;
      }
    }
    target_->SetValue(true_vars >= range_min_ && true_vars <= range_max_);
  }

  const std::vector<IntVar*> vars_;
  const int64 range_min_;
  const int64 range_max_;
  IntVar* const target_;
  NumericalRev<int> num_possible_true_vars_;
  NumericalRev<int> num_always_true_vars_;
  RevSwitch inactive_;
};

class BooleanSumInRange : public Constraint {
 public:
  BooleanSumInRange(Solver* const s, const std::vector<IntVar*>& vars,
                    int64 range_min, int64 range_max)
      : Constraint(s),
        vars_(vars),
        range_min_(range_min),
        range_max_(range_max),
        num_possible_true_vars_(0),
        num_always_true_vars_(0) {}

  virtual ~BooleanSumInRange() {}

  virtual void Post() {
    for (int i = 0; i < vars_.size(); ++i) {
      if (!vars_[i]->Bound()) {
        Demon* const u = MakeConstraintDemon1(
            solver(), this, &BooleanSumInRange::Update, "Update", i);
        vars_[i]->WhenBound(u);
      }
    }
  }

  virtual void InitialPropagate() {
    int num_always_true = 0;
    int num_possible_true = 0;
    int possible_true_index = -1;
    for (int i = 0; i < vars_.size(); ++i) {
      const IntVar* const var = vars_[i];
      if (var->Min() == 1) {
        num_always_true++;
        num_possible_true++;
      } else if (var->Max() == 1) {
        num_possible_true++;
        possible_true_index = i;
      }
    }
    num_possible_true_vars_.SetValue(solver(), num_possible_true);
    num_always_true_vars_.SetValue(solver(), num_always_true);
    Check();
  }

  void Check() {
    if (num_always_true_vars_.Value() > range_max_ ||
        num_possible_true_vars_.Value() < range_min_) {
      solver()->Fail();
    } else if (num_always_true_vars_.Value() >= range_min_ &&
               num_possible_true_vars_.Value() <= range_max_) {
      // Inhibit.
    } else {
      if (num_possible_true_vars_.Value() == range_min_) {
        PushAllUnboundToOne();
      } else if (num_always_true_vars_.Value() == range_max_) {
        PushAllUnboundToZero();
      }
    }
  }

  void Update(int index) {
    DCHECK(vars_[index]->Bound());
    const int64 value = vars_[index]->Min();  // Faster than Value().
    if (value == 0) {
      num_possible_true_vars_.Decr(solver());
    } else {
      DCHECK_EQ(1, value);
      num_always_true_vars_.Incr(solver());
    }
    Check();
  }

  virtual std::string DebugString() const {
    return StringPrintf("Sum([%s]) in [%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d]",
                        JoinDebugStringPtr(vars_, ", ").c_str(), range_min_,
                        range_max_);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kSumEqual, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->EndVisitConstraint(ModelVisitor::kSumEqual, this);
  }

 private:
  void PushAllUnboundToZero() {
    for (int i = 0; i < vars_.size(); ++i) {
      if (vars_[i]->Min() == 0) {
        vars_[i]->SetValue(0);
      } else {
      }
    }
  }

  void PushAllUnboundToOne() {
    for (int i = 0; i < vars_.size(); ++i) {
      if (vars_[i]->Max() == 1) {
        vars_[i]->SetValue(1);
      }
    }
  }

  const std::vector<IntVar*> vars_;
  const int64 range_min_;
  const int64 range_max_;
  NumericalRev<int> num_possible_true_vars_;
  NumericalRev<int> num_always_true_vars_;
};

// ----- Inverse constraint -----

// Variable demand cumulative time table

class VariableCumulativeTask : public BaseObject {
 public:
  VariableCumulativeTask(IntVar* const start, IntVar* const duration,
                         IntVar* const demand)
      : start_(start), duration_(duration), demand_(demand) {}
  // Copy constructor. Cannot be explicit, because we want to pass instances of
  // VariableCumulativeTask by copy.
  VariableCumulativeTask(const VariableCumulativeTask& task)
      : start_(task.start_), duration_(task.duration_), demand_(task.demand_) {}

  IntVar* start() const { return start_; }
  IntVar* duration() const { return duration_; }
  IntVar* demand() const { return demand_; }
  int64 StartMin() const { return start_->Min(); }
  int64 StartMax() const { return start_->Max(); }
  int64 EndMin() const { return start_->Min() + duration_->Min(); }
  virtual std::string DebugString() const {
    return StringPrintf("Task{ start: %s, duration: %s, demand: %s }",
                        start_->DebugString().c_str(),
                        duration_->DebugString().c_str(),
                        demand_->DebugString().c_str());
  }

 private:
  IntVar* const start_;
  IntVar* const duration_;
  IntVar* const demand_;
};

struct ProfileDelta {
  ProfileDelta(int64 _time, int64 _delta) : time(_time), delta(_delta) {}
  int64 time;
  int64 delta;
};

bool TimeLessThan(const ProfileDelta& delta1, const ProfileDelta& delta2) {
  return delta1.time < delta2.time;
}

bool TaskStartMinLessThan(const VariableCumulativeTask* const task1,
                          const VariableCumulativeTask* const task2) {
  return (task1->StartMin() < task2->StartMin());
}

class VariableCumulativeTimeTable : public Constraint {
 public:
  VariableCumulativeTimeTable(Solver* const solver,
                              const std::vector<VariableCumulativeTask*>& tasks,
                              IntVar* const capacity)
      : Constraint(solver), by_start_min_(tasks), capacity_(capacity) {
    // There may be up to 2 delta's per interval (one on each side),
    // plus two sentinels
    const int profile_max_size = 2 * NumTasks() + 2;
    profile_non_unique_time_.reserve(profile_max_size);
    profile_unique_time_.reserve(profile_max_size);
  }

  virtual void InitialPropagate() {
    BuildProfile();
    PushTasks();
  }

  virtual void Post() {
    Demon* const demon =
        solver()->MakeDelayedConstraintInitialPropagateCallback(this);
    for (int i = 0; i < NumTasks(); ++i) {
      by_start_min_[i]->start()->WhenRange(demon);
      by_start_min_[i]->duration()->WhenRange(demon);
      by_start_min_[i]->demand()->WhenRange(demon);
    }
    capacity_->WhenRange(demon);
  }

  int NumTasks() const { return by_start_min_.size(); }

  void Accept(ModelVisitor* const visitor) const {
    LOG(FATAL) << "Should not be visited";
  }

  virtual std::string DebugString() const {
    return StringPrintf("VariableCumulativeTimeTable([%s], capacity = %s)",
                        JoinDebugStringPtr(by_start_min_, ", ").c_str(),
                        capacity_->DebugString().c_str());
  }

 private:
  // Build the usage profile. Runs in O(n log n).
  void BuildProfile() {
    // Build profile with non unique time
    profile_non_unique_time_.clear();
    for (int i = 0; i < NumTasks(); ++i) {
      const VariableCumulativeTask* task = by_start_min_[i];
      const int64 start_max = task->StartMax();
      const int64 end_min = task->EndMin();
      const int64 demand_min = task->demand()->Min();
      if (start_max < end_min && demand_min > 0) {
        profile_non_unique_time_.push_back(
            ProfileDelta(start_max, +demand_min));
        profile_non_unique_time_.push_back(ProfileDelta(end_min, -demand_min));
      }
    }
    // Sort
    std::sort(profile_non_unique_time_.begin(), profile_non_unique_time_.end(),
         TimeLessThan);
    // Build profile with unique times
    profile_unique_time_.clear();
    profile_unique_time_.push_back(ProfileDelta(kint64min, 0));
    for (int i = 0; i < profile_non_unique_time_.size(); ++i) {
      const ProfileDelta& profile_delta = profile_non_unique_time_[i];
      if (profile_delta.time == profile_unique_time_.back().time) {
        profile_unique_time_.back().delta += profile_delta.delta;
      } else {
        profile_unique_time_.push_back(profile_delta);
      }
    }
    // Re-scan profile to compute min usage, max usage, and check
    // final usage to be 0.
    int64 usage = 0;
    int64 max_required_usage = 0;
    const int64 max_capacity = capacity_->Max();
    for (int i = 0; i < profile_unique_time_.size(); ++i) {
      const ProfileDelta& profile_delta = profile_unique_time_[i];
      usage += profile_delta.delta;
      max_required_usage = std::max(max_required_usage, usage);
      if (usage > max_capacity) {
        solver()->Fail();
      }
    }
    DCHECK_EQ(0, usage);
    profile_unique_time_.push_back(ProfileDelta(kint64max, 0));

    // Propagate on capacity.
    capacity_->SetMin(max_required_usage);
  }

  // Update the start min for all tasks. Runs in O(n^2) and Omega(n).
  void PushTasks() {
    std::sort(by_start_min_.begin(), by_start_min_.end(), TaskStartMinLessThan);
    int64 usage = 0;
    int profile_index = 0;
    for (int task_index = 0; task_index < NumTasks(); ++task_index) {
      VariableCumulativeTask* const task = by_start_min_[task_index];
      if (task->duration()->Min() > 0) {
        while (task->StartMin() > profile_unique_time_[profile_index].time) {
          DCHECK(profile_index < profile_unique_time_.size());
          ++profile_index;
          usage += profile_unique_time_[profile_index].delta;
        }
        PushTask(task, profile_index, usage);
      }
    }
  }

  // Push the given task to new_start_min, defined as the smallest integer such
  // that the profile usage for all tasks, excluding the current one, does not
  // exceed capacity_ - task->demand()->Min() on the interval
  // [new_start_min, new_start_min + task->interval()->DurationMin() ).
  void PushTask(VariableCumulativeTask* const task, int profile_index,
                int64 usage) {
    const int64 demand_max = task->demand()->Max();
    if (demand_max == 0) {
      return;
    }

    // Init
    const int64 demand_min = task->demand()->Min();
    const int64 adjusted_demand = demand_min == 0 ? 1 : demand_min;
    const bool is_adjusted = demand_min == 0;
    const int64 residual_capacity = capacity_->Max() - adjusted_demand;
    const int64 duration_min = task->duration()->Min();
    const ProfileDelta& first_prof_delta = profile_unique_time_[profile_index];

    int64 new_start_min = task->StartMin();

    DCHECK_GE(first_prof_delta.time, task->StartMin());
    // The check above is with a '>='. Let's first treat the '>' case
    if (first_prof_delta.time > task->StartMin()) {
      // There was no profile delta at a time between interval->StartMin()
      // (included) and the current one.
      // As we don't delete delta's of 0 value, this means the current task
      // does not contribute to the usage before:
      // TODO(user): Re-enable this check. Failing with jfsp easy1.
      // DCHECK(task->StartMax() >= first_prof_delta.time ||
      //        task->StartMax() >= task->EndMin() ||
      //        first_prof_delta.time == kint64maxp);
      // The 'usage' given in argument is valid at first_prof_delta.time. To
      // compute the usage at the start min, we need to remove the last delta.
      const int64 usage_at_start_min = usage - first_prof_delta.delta;
      if (usage_at_start_min > residual_capacity) {
        new_start_min = profile_unique_time_[profile_index].time;
      }
    }

    // Influence of current task
    const int64 start_max = task->StartMax();
    const int64 end_min = task->EndMin();
    // TODO(user): remove delta_start and delta_end.
    ProfileDelta delta_start(start_max, 0);
    ProfileDelta delta_end(end_min, 0);
    if (start_max < end_min) {
      delta_start.delta = +demand_min;
      delta_end.delta = -demand_min;
    }
    while (profile_unique_time_[profile_index].time <
           duration_min + new_start_min) {
      const ProfileDelta& profile_delta = profile_unique_time_[profile_index];
      DCHECK(profile_index < profile_unique_time_.size());
      // Compensate for current task
      if (profile_delta.time == delta_start.time) {
        usage -= delta_start.delta;
      }
      if (profile_delta.time == delta_end.time) {
        usage -= delta_end.delta;
      }
      // Increment time
      ++profile_index;
      DCHECK(profile_index < profile_unique_time_.size());
      // Does it fit?
      if (usage > residual_capacity) {
        new_start_min = profile_unique_time_[profile_index].time;
      }
      usage += profile_unique_time_[profile_index].delta;
    }
    if (is_adjusted) {
      if (new_start_min > task->StartMax()) {
        task->demand()->SetMax(0);
      }
    } else {
      task->start()->SetMin(new_start_min);
    }
  }

  typedef std::vector<ProfileDelta> Profile;

  Profile profile_unique_time_;
  Profile profile_non_unique_time_;
  std::vector<VariableCumulativeTask*> by_start_min_;
  IntVar* const capacity_;

  DISALLOW_COPY_AND_ASSIGN(VariableCumulativeTimeTable);
};

// ----- LinkIntervalStartPerformed -----

class LinkIntervalStartPerformed : public Constraint {
 public:
  LinkIntervalStartPerformed(Solver* solver, IntervalVar* interval,
                             IntVar* start, IntVar* performed)
      : Constraint(solver),
        interval_(interval),
        start_(start),
        performed_(performed) {}

  virtual ~LinkIntervalStartPerformed() {}

  virtual void Post() {
    Demon* const demon = solver()->MakeConstraintInitialPropagateCallback(this);
    interval_->WhenPerformedBound(demon);
    interval_->WhenStartRange(demon);
    start_->WhenRange(demon);
  }

  virtual void InitialPropagate() {
    if (performed_->Bound() && !interval_->IsPerformedBound()) {
      interval_->SetPerformed(performed_->Min());
    } else if (interval_->MustBePerformed()) {
      performed_->SetValue(1);
    } else if (!interval_->MayBePerformed()) {
      performed_->SetValue(0);
    }
    interval_->SetStartRange(start_->Min(), start_->Max());
    if (interval_->MustBePerformed()) {
      start_->SetRange(interval_->StartMin(), interval_->StartMax());
    }
  }

  virtual std::string DebugString() const {
    return "LinkIntervalStartPerformed";
  }

 private:
  IntervalVar* const interval_;
  IntVar* const start_;
  IntVar* const performed_;
};
}  // namespace

Constraint* MakeIsBooleanSumInRange(Solver* const solver,
                                    const std::vector<IntVar*>& variables,
                                    int64 range_min, int64 range_max,
                                    IntVar* const target) {
  return solver->RevAlloc(
      new IsBooleanSumInRange(solver, variables, range_min, range_max, target));
}

Constraint* MakeBooleanSumInRange(Solver* const solver,
                                  const std::vector<IntVar*>& variables,
                                  int64 range_min, int64 range_max) {
  return solver->RevAlloc(
      new BooleanSumInRange(solver, variables, range_min, range_max));
}
Constraint* MakeBooleanSumOdd(Solver* const solver,
                              const std::vector<IntVar*>& variables) {
  return solver->RevAlloc(new BooleanSumOdd(solver, variables));
}

Constraint* MakeStrongScalProdEquality(Solver* const solver,
                                       const std::vector<IntVar*>& variables,
                                       const std::vector<int64>& coefficients,
                                       int64 rhs) {
  const bool trace = FLAGS_cp_trace_search;
  const bool propag = FLAGS_cp_trace_propagation;
  FLAGS_cp_trace_search = false;
  FLAGS_cp_trace_propagation = false;
  const int size = variables.size();
  IntTupleSet tuples(size);
  Solver s("build");
  std::vector<IntVar*> copy_vars(size);
  for (int i = 0; i < size; ++i) {
    copy_vars[i] = s.MakeIntVar(variables[i]->Min(), variables[i]->Max());
  }
  s.AddConstraint(s.MakeScalProdEquality(copy_vars, coefficients, rhs));
  s.NewSearch(s.MakePhase(copy_vars, Solver::CHOOSE_FIRST_UNBOUND,
                          Solver::ASSIGN_MIN_VALUE));
  while (s.NextSolution()) {
    std::vector<int64> one_tuple(size);
    for (int i = 0; i < size; ++i) {
      one_tuple[i] = copy_vars[i]->Value();
    }
    tuples.Insert(one_tuple);
  }
  s.EndSearch();
  FLAGS_cp_trace_search = trace;
  FLAGS_cp_trace_propagation = propag;
  return solver->MakeAllowedAssignments(variables, tuples);
}

Constraint* MakeVariableCumulative(Solver* const solver,
                                   const std::vector<IntVar*>& starts,
                                   const std::vector<IntVar*>& durations,
                                   const std::vector<IntVar*>& usages,
                                   IntVar* const capacity) {
  std::vector<VariableCumulativeTask*> tasks;
  for (int i = 0; i < starts.size(); ++i) {
    if (usages[i]->Max() > 0) {
      tasks.push_back(solver->RevAlloc(
          new VariableCumulativeTask(starts[i], durations[i], usages[i])));
    }
  }
  return solver->RevAlloc(
      new VariableCumulativeTimeTable(solver, tasks, capacity));
}

Constraint* MakeVariableOdd(Solver* const s, IntVar* const var) {
  return s->RevAlloc(new VariableParity(s, var, true));
}

Constraint* MakeVariableEven(Solver* const s, IntVar* const var) {
  return s->RevAlloc(new VariableParity(s, var, false));
}

void PostBooleanSumInRange(SatPropagator* sat, Solver* solver,
                           const std::vector<IntVar*>& variables,
                           int64 range_min, int64 range_max) {
  const int64 size = variables.size();
  range_min = std::max(0LL, range_min);
  range_max = std::min(size, range_max);
  int true_vars = 0;
  std::vector<IntVar*> alt;
  for (int i = 0; i < size; ++i) {
    if (!variables[i]->Bound()) {
      alt.push_back(variables[i]);
    } else if (variables[i]->Min() == 1) {
      true_vars++;
    }
  }
  const int possible_vars = alt.size();
  range_min -= true_vars;
  range_max -= true_vars;

  if (range_max < 0 || range_min > possible_vars) {
    Constraint* const ct = solver->MakeFalseConstraint();
    FZVLOG << "  - posted " << ct->DebugString() << FZENDL;
    solver->AddConstraint(ct);
  } else if (range_min <= 0 && range_max >= possible_vars) {
    Constraint* const ct = solver->MakeTrueConstraint();
    FZVLOG << "  - posted " << ct->DebugString() << FZENDL;
    solver->AddConstraint(ct);
  } else if (FLAGS_use_sat && range_min == 0 && range_max == 1 &&
             AddAtMostOne(sat, alt)) {
    FZVLOG << "  - posted to sat" << FZENDL;
  } else if (FLAGS_use_sat && range_min == 0 && range_max == size - 1 &&
             AddAtMostNMinusOne(sat, alt)) {
    FZVLOG << "  - posted to sat" << FZENDL;
  } else if (FLAGS_use_sat && range_min == 1 && range_max == 1 &&
             AddBoolOrArrayEqualTrue(sat, alt) && AddAtMostOne(sat, alt)) {
    FZVLOG << "  - posted to sat" << FZENDL;
  } else if (FLAGS_use_sat && range_min == 1 && range_max == possible_vars &&
             AddBoolOrArrayEqualTrue(sat, alt)) {
    FZVLOG << "  - posted to sat" << FZENDL;
  } else {
    Constraint* const ct =
        MakeBooleanSumInRange(solver, alt, range_min, range_max);
    FZVLOG << "  - posted " << ct->DebugString() << FZENDL;
    solver->AddConstraint(ct);
  }
}

void PostIsBooleanSumInRange(SatPropagator* sat, Solver* solver,
                             const std::vector<IntVar*>& variables,
                             int64 range_min, int64 range_max, IntVar* target) {
  const int64 size = variables.size();
  range_min = std::max(0LL, range_min);
  range_max = std::min(size, range_max);
  int true_vars = 0;
  int possible_vars = 0;
  for (int i = 0; i < size; ++i) {
    if (variables[i]->Max() == 1) {
      possible_vars++;
      if (variables[i]->Min() == 1) {
        true_vars++;
      }
    }
  }
  if (true_vars > range_max || possible_vars < range_min) {
    target->SetValue(0);
    FZVLOG << "  - set target to 0" << FZENDL;
  } else if (true_vars >= range_min && possible_vars <= range_max) {
    target->SetValue(1);
    FZVLOG << "  - set target to 1" << FZENDL;
  } else if (FLAGS_use_sat && range_min == size &&
             AddBoolAndArrayEqVar(sat, variables, target)) {
    FZVLOG << "  - posted to sat" << FZENDL;
  } else if (FLAGS_use_sat && range_max == 0 &&
             AddBoolOrArrayEqVar(sat, variables,
                                 solver->MakeDifference(1, target)->Var())) {
    FZVLOG << "  - posted to sat" << FZENDL;
  } else if (FLAGS_use_sat && range_min == 1 && range_max == size &&
             AddBoolOrArrayEqVar(sat, variables, target)) {
    FZVLOG << "  - posted to sat" << FZENDL;
  } else {
    Constraint* const ct = MakeIsBooleanSumInRange(solver, variables, range_min,
                                                   range_max, target);
    FZVLOG << "  - posted " << ct->DebugString() << FZENDL;
    solver->AddConstraint(ct);
  }
}

void PostIsBooleanSumDifferent(SatPropagator* sat, Solver* solver,
                               const std::vector<IntVar*>& variables,
                               int64 value, IntVar* target) {
  const int64 size = variables.size();
  if (value == 0) {
    PostIsBooleanSumInRange(sat, solver, variables, 1, size, target);
  } else if (value == size) {
    PostIsBooleanSumInRange(sat, solver, variables, 0, size - 1, target);
  } else {
    Constraint* const ct =
        solver->MakeIsDifferentCstCt(solver->MakeSum(variables), value, target);
    FZVLOG << "  - posted " << ct->DebugString() << FZENDL;
    solver->AddConstraint(ct);
  }
}

IntervalVar* MakeIntervalStartPerformed(Solver* solver, IntVar* start,
                                        int64 duration, IntVar* performed) {
  const std::string& name = start->name();
  if (performed->Min() == 1) {
    return solver->MakeFixedDurationIntervalVar(start, duration, name);
  } else if (performed->Max() == 0) {
    IntervalVar* const interval = solver->MakeFixedDurationIntervalVar(
        start->Min(), start->Max(), duration, true, name);
    interval->SetPerformed(false);
    return interval;
    // TODO(user): Implement unperformed interval.
  } else {
    IntervalVar* const interval = solver->MakeFixedDurationIntervalVar(
        start->Min(), start->Max(), duration, true, name);
    solver->AddConstraint(solver->RevAlloc(
        new LinkIntervalStartPerformed(solver, interval, start, performed)));
    return interval;
  }
}

}  // namespace operations_research

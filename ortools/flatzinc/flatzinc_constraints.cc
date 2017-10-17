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

#include "ortools/flatzinc/flatzinc_constraints.h"

#include <unordered_set>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/stringprintf.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/flatzinc/logging.h"
#include "ortools/util/string_array.h"

DECLARE_bool(cp_trace_search);
DECLARE_bool(cp_trace_propagation);

namespace operations_research {
namespace {
class BooleanSumOdd : public Constraint {
 public:
  BooleanSumOdd(Solver* const s, const std::vector<IntVar*>& vars)
      : Constraint(s),
        vars_(vars),
        num_possible_true_vars_(0),
        num_always_true_vars_(0) {}

  ~BooleanSumOdd() override {}

  void Post() override {
    for (int i = 0; i < vars_.size(); ++i) {
      if (!vars_[i]->Bound()) {
        Demon* const u = MakeConstraintDemon1(
            solver(), this, &BooleanSumOdd::Update, "Update", i);
        vars_[i]->WhenBound(u);
      }
    }
  }

  void InitialPropagate() override {
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

  std::string DebugString() const override {
    return StringPrintf("BooleanSumOdd([%s])",
                        JoinDebugStringPtr(vars_, ", ").c_str());
  }

  void Accept(ModelVisitor* const visitor) const override {
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

class FixedModulo : public Constraint {
 public:
  FixedModulo(Solver* const s, IntVar* x, IntVar* const m, int64 r)
      : Constraint(s), var_(x), mod_(m), residual_(r) {}

  ~FixedModulo() override {}

  void Post() override {
    Demon* const d = solver()->MakeConstraintInitialPropagateCallback(this);
    var_->WhenRange(d);
    mod_->WhenBound(d);
  }

  void InitialPropagate() override {
    if (mod_->Bound()) {
      const int64 d = std::abs(mod_->Min());
      if (d == 0) {
        solver()->Fail();
      } else {
        const int64 emin = var_->Min();
        const int64 emax = var_->Max();
        const int64 new_min = PosIntDivUp(emin - residual_, d) * d + residual_;
        const int64 new_max =
            PosIntDivDown(emax - residual_, d) * d + residual_;
        var_->SetRange(new_min, new_max);
      }
    }
  }

  std::string DebugString() const override {
    return StringPrintf("(%s %% %s == %" GG_LL_FORMAT "d)",
                        var_->DebugString().c_str(),
                        mod_->DebugString().c_str(), residual_);
  }

 private:
  IntVar* const var_;
  IntVar* const mod_;
  const int64 residual_;
};

class VariableParity : public Constraint {
 public:
  VariableParity(Solver* const s, IntVar* const var, bool odd)
      : Constraint(s), var_(var), odd_(odd) {}

  ~VariableParity() override {}

  void Post() override {
    if (!var_->Bound()) {
      Demon* const u = solver()->MakeConstraintInitialPropagateCallback(this);
      var_->WhenRange(u);
    }
  }

  void InitialPropagate() override {
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

  std::string DebugString() const override {
    return StringPrintf("VarParity(%s, %d)", var_->DebugString().c_str(), odd_);
  }

  void Accept(ModelVisitor* const visitor) const override {
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

  ~IsBooleanSumInRange() override {}

  void Post() override {
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

  void InitialPropagate() override {
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

  std::string DebugString() const override {
    return StringPrintf("Sum([%s]) in [%" GG_LL_FORMAT "d..%" GG_LL_FORMAT
                        "d] == %s",
                        JoinDebugStringPtr(vars_, ", ").c_str(), range_min_,
                        range_max_, target_->DebugString().c_str());
  }

  void Accept(ModelVisitor* const visitor) const override {
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

  ~BooleanSumInRange() override {}

  void Post() override {
    for (int i = 0; i < vars_.size(); ++i) {
      if (!vars_[i]->Bound()) {
        Demon* const u = MakeConstraintDemon1(
            solver(), this, &BooleanSumInRange::Update, "Update", i);
        vars_[i]->WhenBound(u);
      }
    }
  }

  void InitialPropagate() override {
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

  std::string DebugString() const override {
    return StringPrintf("Sum([%s]) in [%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d]",
                        JoinDebugStringPtr(vars_, ", ").c_str(), range_min_,
                        range_max_);
  }

  void Accept(ModelVisitor* const visitor) const override {
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

// ----- Variable duration interval var -----

class StartVarDurationVarPerformedIntervalVar : public IntervalVar {
 public:
  StartVarDurationVarPerformedIntervalVar(Solver* const s, IntVar* const start,
                                          IntVar* const duration,
                                          const std::string& name);
  ~StartVarDurationVarPerformedIntervalVar() override {}

  int64 StartMin() const override;
  int64 StartMax() const override;
  void SetStartMin(int64 m) override;
  void SetStartMax(int64 m) override;
  void SetStartRange(int64 mi, int64 ma) override;
  int64 OldStartMin() const override { return start_->OldMin(); }
  int64 OldStartMax() const override { return start_->OldMax(); }
  void WhenStartRange(Demon* const d) override { start_->WhenRange(d); }
  void WhenStartBound(Demon* const d) override { start_->WhenBound(d); }

  int64 DurationMin() const override;
  int64 DurationMax() const override;
  void SetDurationMin(int64 m) override;
  void SetDurationMax(int64 m) override;
  void SetDurationRange(int64 mi, int64 ma) override;
  int64 OldDurationMin() const override { return duration_->Min(); }
  int64 OldDurationMax() const override { return duration_->Max(); }
  void WhenDurationRange(Demon* const d) override { duration_->WhenRange(d); }
  void WhenDurationBound(Demon* const d) override { duration_->WhenBound(d); }

  int64 EndMin() const override;
  int64 EndMax() const override;
  void SetEndMin(int64 m) override;
  void SetEndMax(int64 m) override;
  void SetEndRange(int64 mi, int64 ma) override;
  int64 OldEndMin() const override { return end_->OldMin(); }
  int64 OldEndMax() const override { return end_->OldMax(); }
  void WhenEndRange(Demon* const d) override { end_->WhenRange(d); }
  void WhenEndBound(Demon* const d) override { end_->WhenBound(d); }

  bool MustBePerformed() const override;
  bool MayBePerformed() const override;
  void SetPerformed(bool val) override;
  bool WasPerformedBound() const override { return true; }
  void WhenPerformedBound(Demon* const d) override {}
  std::string DebugString() const override;

  IntExpr* StartExpr() override { return start_; }
  IntExpr* DurationExpr() override { return duration_; }
  IntExpr* EndExpr() override { return end_; }
  IntExpr* PerformedExpr() override { return solver()->MakeIntConst(1); }
  IntExpr* SafeStartExpr(int64 unperformed_value) override {
    return StartExpr();
  }
  IntExpr* SafeDurationExpr(int64 unperformed_value) override {
    return DurationExpr();
  }
  IntExpr* SafeEndExpr(int64 unperformed_value) override { return EndExpr(); }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->VisitIntervalVariable(this, "", 0, nullptr);
  }

 private:
  IntVar* const start_;
  IntVar* const duration_;
  IntVar* const end_;
};

// TODO(user): Take care of overflows.
StartVarDurationVarPerformedIntervalVar::
    StartVarDurationVarPerformedIntervalVar(Solver* const s, IntVar* const var,
                                            IntVar* const duration,
                                            const std::string& name)
    : IntervalVar(s, name),
      start_(var),
      duration_(duration),
      end_(s->MakeSum(var, duration)->Var()) {}

int64 StartVarDurationVarPerformedIntervalVar::StartMin() const {
  return start_->Min();
}

int64 StartVarDurationVarPerformedIntervalVar::StartMax() const {
  return start_->Max();
}

void StartVarDurationVarPerformedIntervalVar::SetStartMin(int64 m) {
  start_->SetMin(m);
}

void StartVarDurationVarPerformedIntervalVar::SetStartMax(int64 m) {
  start_->SetMax(m);
}

void StartVarDurationVarPerformedIntervalVar::SetStartRange(int64 mi,
                                                            int64 ma) {
  start_->SetRange(mi, ma);
}

int64 StartVarDurationVarPerformedIntervalVar::DurationMin() const {
  return duration_->Min();
}

int64 StartVarDurationVarPerformedIntervalVar::DurationMax() const {
  return duration_->Max();
}

void StartVarDurationVarPerformedIntervalVar::SetDurationMin(int64 m) {
  duration_->SetMin(m);
}

void StartVarDurationVarPerformedIntervalVar::SetDurationMax(int64 m) {
  duration_->SetMax(m);
}

void StartVarDurationVarPerformedIntervalVar::SetDurationRange(int64 mi,
                                                               int64 ma) {
  duration_->SetRange(mi, ma);
}

int64 StartVarDurationVarPerformedIntervalVar::EndMin() const {
  return end_->Min();
}

int64 StartVarDurationVarPerformedIntervalVar::EndMax() const {
  return end_->Max();
}

void StartVarDurationVarPerformedIntervalVar::SetEndMin(int64 m) {
  end_->SetMin(m);
}

void StartVarDurationVarPerformedIntervalVar::SetEndMax(int64 m) {
  end_->SetMax(m);
}

void StartVarDurationVarPerformedIntervalVar::SetEndRange(int64 mi, int64 ma) {
  end_->SetRange(mi, ma);
}

bool StartVarDurationVarPerformedIntervalVar::MustBePerformed() const {
  return true;
}

bool StartVarDurationVarPerformedIntervalVar::MayBePerformed() const {
  return true;
}

void StartVarDurationVarPerformedIntervalVar::SetPerformed(bool val) {
  if (!val) {
    solver()->Fail();
  }
}

std::string StartVarDurationVarPerformedIntervalVar::DebugString() const {
  std::string out;
  const std::string& var_name = name();
  if (!var_name.empty()) {
    out = var_name + "(start = ";
  } else {
    out = "IntervalVar(start = ";
  }
  StringAppendF(&out, "%s, duration = %s, performed = true)",
                start_->DebugString().c_str(),
                duration_->DebugString().c_str());
  return out;
}

// k - Diffn

class KDiffn : public Constraint {
 public:
  KDiffn(Solver* const solver, const std::vector<std::vector<IntVar*>>& x,
         const std::vector<std::vector<IntVar*>>& dx, bool strict)
      : Constraint(solver),
        x_(x),
        dx_(dx),
        strict_(strict),
        num_boxes_(x.size()),
        num_dims_(x[0].size()),
        fail_stamp_(0) {}

  ~KDiffn() override {}

  void Post() override {
    Solver* const s = solver();
    for (int box = 0; box < num_boxes_; ++box) {
      Demon* const demon = MakeConstraintDemon1(
          s, this, &KDiffn::OnBoxRangeChange, "OnBoxRangeChange", box);
      for (int dim = 0; dim < num_dims_; ++dim) {
        x_[box][dim]->WhenRange(demon);
        dx_[box][dim]->WhenRange(demon);
      }
    }
    delayed_demon_ = MakeDelayedConstraintDemon0(s, this, &KDiffn::PropagateAll,
                                                 "PropagateAll");
  }

  void InitialPropagate() override {
    // All sizes should be > 0.
    for (int box = 0; box < num_boxes_; ++box) {
      for (int dim = 0; dim < num_dims_; ++dim) {
        dx_[box][dim]->SetMin(1);
      }
    }

    // Force propagation on all boxes.
    to_propagate_.clear();
    for (int i = 0; i < num_boxes_; i++) {
      to_propagate_.insert(i);
    }
    PropagateAll();
  }

  std::string DebugString() const override { return "KDiffn()"; }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kDisjunctive, this);
    visitor->EndVisitConstraint(ModelVisitor::kDisjunctive, this);
  }

 private:
  void PropagateAll() {
    for (const int box : to_propagate_) {
      FillNeighbors(box);
      FailWhenEnergyIsTooLarge(box);
      PushOverlappingBoxes(box);
    }
    to_propagate_.clear();
    fail_stamp_ = solver()->fail_stamp();
  }

  void OnBoxRangeChange(int box) {
    if (solver()->fail_stamp() > fail_stamp_ && !to_propagate_.empty()) {
      // We have failed in the last propagation and the to_propagate_
      // was not cleared.
      fail_stamp_ = solver()->fail_stamp();
      to_propagate_.clear();
    }
    to_propagate_.insert(box);
    EnqueueDelayedDemon(delayed_demon_);
  }

  bool CanBoxesOverlap(int box1, int box2) const {
    for (int dim = 0; dim < num_dims_; ++dim) {
      if (AreBoxedDisjointInOneDimensionForSure(dim, box1, box2)) {
        return false;
      }
    }
    return true;
  }

  bool AreBoxedDisjointInOneDimensionForSure(int dim, int i, int j) const {
    return (x_[i][dim]->Min() >= x_[j][dim]->Max() + dx_[j][dim]->Max()) ||
           (x_[j][dim]->Min() >= x_[i][dim]->Max() + dx_[i][dim]->Max()) ||
           (!strict_ && (dx_[i][dim]->Min() == 0 || dx_[j][dim]->Min() == 0));
  }

  // Fill neighbors_ with all boxes that can overlap the given box.
  void FillNeighbors(int box) {
    // TODO(user): We could maintain a non reversible list of
    // neighbors and clean it after each failure.
    neighbors_.clear();
    for (int other = 0; other < num_boxes_; ++other) {
      if (other != box && CanBoxesOverlap(other, box)) {
        neighbors_.push_back(other);
      }
    }
  }

  // Fails if the minimum area of the given box plus the area of its neighbors
  // (that must already be computed in neighbors_) is greater than the area of a
  // bounding box that necessarily contains all these boxes.
  void FailWhenEnergyIsTooLarge(int box) {
    std::vector<int64> starts(num_dims_);
    std::vector<int64> ends(num_dims_);

    int64 box_volume = 1;
    for (int dim = 0; dim < num_dims_; ++dim) {
      starts[dim] = x_[box][dim]->Min();
      ends[dim] = x_[box][dim]->Max() + dx_[box][dim]->Max();
      box_volume *= dx_[box][dim]->Min();
    }
    int64 sum_of_volumes = box_volume;

    // TODO(user): Is there a better order, maybe sort by distance
    // with the current box.
    for (int i = 0; i < neighbors_.size(); ++i) {
      const int other = neighbors_[i];
      int64 other_volume = 1;
      int64 bounding_volume = 1;
      for (int dim = 0; dim < num_dims_; ++dim) {
        IntVar* const x = x_[other][dim];
        IntVar* const dx = dx_[other][dim];
        starts[dim] = std::min(starts[dim], x->Min());
        ends[dim] = std::max(ends[dim], x->Max() + dx->Max());
        other_volume *= dx->Min();
        bounding_volume *= ends[dim] - starts[dim];
      }
      // Update sum of volumes.
      sum_of_volumes += other_volume;
      if (sum_of_volumes > bounding_volume) {
        solver()->Fail();
      }
    }
  }

  // Changes the domain of all the neighbors of a given box (that must
  // already be computed in neighbors_) so that they can't overlap the
  // mandatory part of the given box.
  void PushOverlappingBoxes(int box) {
    for (int i = 0; i < neighbors_.size(); ++i) {
      TryPushOneBox(box, neighbors_[i]);
    }
  }

  // Changes the domain of the two given box by excluding the value that
  // make them overlap for sure. Note that this function is symmetric in
  // the sense that its argument can be swapped for the same result.
  void TryPushOneBox(int b1, int b2) {
    int b1_after_b2 = -1;
    int b2_after_b1 = -1;
    bool already_inserted = false;
    for (int dim = 0; dim < num_dims_; ++dim) {
      IntVar* const x1 = x_[b1][dim];
      IntVar* const x2 = x_[b2][dim];
      IntVar* const dx1 = dx_[b1][dim];
      IntVar* const dx2 = dx_[b2][dim];
      DCHECK(strict_ || dx1->Min() > 0);
      DCHECK(strict_ || dx2->Min() > 0);
      if (x1->Min() + dx1->Min() <= x2->Max()) {
        if (already_inserted) {  // Too much freedom degrees, we can exit.
          return;
        } else {
          already_inserted = true;
        }
        b2_after_b1 = dim;
      }
      if (x2->Min() + dx2->Min() <= x1->Max()) {
        if (already_inserted) {  // Too much freedom degrees, we can exit.
          return;
        } else {
          already_inserted = true;
        }
        b1_after_b2 = dim;
      }
    }
    if (b1_after_b2 == -1 && b2_after_b1 == -1) {
      // Stuck in an overlapping position. We can fail.
      solver()->Fail();
    }
    // We verify the exclusion.
    CHECK((b2_after_b1 == -1 && b1_after_b2 != -1) ||
          (b1_after_b2 == -1 && b2_after_b1 != -1));

    if (b1_after_b2 != -1) {
      // We need to push b1 after b2, and restrict b2 to be before b1.
      IntVar* const x1 = x_[b1][b1_after_b2];
      IntVar* const x2 = x_[b2][b1_after_b2];
      IntVar* const dx2 = dx_[b2][b1_after_b2];
      x1->SetMin(x2->Min() + dx2->Min());
      x2->SetMax(x1->Max() - dx2->Min());
      dx2->SetMax(x1->Max() - x2->Min());
    }

    if (b2_after_b1 != -1) {
      // We need to push b2 after b1, and restrict b1 to be before b2.
      IntVar* const x1 = x_[b1][b2_after_b1];
      IntVar* const x2 = x_[b2][b2_after_b1];
      IntVar* const dx1 = dx_[b1][b2_after_b1];
      x2->SetMin(x1->Min() + dx1->Min());
      x1->SetMax(x2->Max() - dx1->Min());
      dx1->SetMax(x2->Max() - x1->Min());
    }
  }

  std::vector<std::vector<IntVar*>> x_;
  std::vector<std::vector<IntVar*>> dx_;
  const bool strict_;
  const int64 num_boxes_;
  const int64 num_dims_;
  Demon* delayed_demon_;
  std::unordered_set<int> to_propagate_;
  std::vector<int> neighbors_;
  uint64 fail_stamp_;
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

Constraint* MakeVariableOdd(Solver* const s, IntVar* const var) {
  return s->RevAlloc(new VariableParity(s, var, true));
}

Constraint* MakeVariableEven(Solver* const s, IntVar* const var) {
  return s->RevAlloc(new VariableParity(s, var, false));
}

Constraint* MakeFixedModulo(Solver* const s, IntVar* const var,
                            IntVar* const mod, int64 residual) {
  return s->RevAlloc(new FixedModulo(s, var, mod, residual));
}

IntervalVar* MakePerformedIntervalVar(Solver* const solver, IntVar* const start,
                                      IntVar* const duration, const std::string& n) {
  CHECK(start != nullptr);
  CHECK(duration != nullptr);
  return solver->RegisterIntervalVar(solver->RevAlloc(
      new StartVarDurationVarPerformedIntervalVar(solver, start, duration, n)));
}

Constraint* MakeKDiffn(Solver* solver,
                       const std::vector<std::vector<IntVar*>>& x,
                       const std::vector<std::vector<IntVar*>>& dx,
                       bool strict) {
  return solver->RevAlloc(new KDiffn(solver, x, dx, strict));
}

}  // namespace operations_research

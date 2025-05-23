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

#include <string.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "ortools/base/logging.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/string_array.h"

namespace operations_research {

Demon* Solver::MakeConstraintInitialPropagateCallback(Constraint* const ct) {
  return MakeConstraintDemon0(this, ct, &Constraint::InitialPropagate,
                              "InitialPropagate");
}

Demon* Solver::MakeDelayedConstraintInitialPropagateCallback(
    Constraint* const ct) {
  return MakeDelayedConstraintDemon0(this, ct, &Constraint::InitialPropagate,
                                     "InitialPropagate");
}

namespace {
class ActionDemon : public Demon {
 public:
  explicit ActionDemon(const Solver::Action& action) : action_(action) {
    CHECK(action != nullptr);
  }

  ~ActionDemon() override {}

  void Run(Solver* const solver) override { action_(solver); }

 private:
  Solver::Action action_;
};

class ClosureDemon : public Demon {
 public:
  explicit ClosureDemon(const Solver::Closure& closure) : closure_(closure) {
    CHECK(closure != nullptr);
  }

  ~ClosureDemon() override {}

  void Run(Solver* const solver) override { closure_(); }

 private:
  Solver::Closure closure_;
};

// ----- True and False Constraint -----

class TrueConstraint : public Constraint {
 public:
  explicit TrueConstraint(Solver* const s) : Constraint(s) {}
  ~TrueConstraint() override {}

  void Post() override {}
  void InitialPropagate() override {}
  std::string DebugString() const override { return "TrueConstraint()"; }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kTrueConstraint, this);
    visitor->EndVisitConstraint(ModelVisitor::kTrueConstraint, this);
  }
  IntVar* Var() override { return solver()->MakeIntConst(1); }
};

class FalseConstraint : public Constraint {
 public:
  explicit FalseConstraint(Solver* const s) : Constraint(s) {}
  FalseConstraint(Solver* const s, const std::string& explanation)
      : Constraint(s), explanation_(explanation) {}
  ~FalseConstraint() override {}

  void Post() override {}
  void InitialPropagate() override { solver()->Fail(); }
  std::string DebugString() const override {
    return absl::StrCat("FalseConstraint(", explanation_, ")");
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kFalseConstraint, this);
    visitor->EndVisitConstraint(ModelVisitor::kFalseConstraint, this);
  }
  IntVar* Var() override { return solver()->MakeIntConst(0); }

 private:
  const std::string explanation_;
};

// ----- Map Variable Domain to Boolean Var Array -----
// TODO(user) : optimize constraint to avoid ping pong.
// After a boolvar is set to 0, we remove the value from the var.
// There is no need to rescan the var to find the hole if the size at the end of
// UpdateActive() is the same as the size at the beginning of VarDomain().

class MapDomain : public Constraint {
 public:
  MapDomain(Solver* const s, IntVar* const var,
            const std::vector<IntVar*>& actives)
      : Constraint(s), var_(var), actives_(actives) {
    holes_ = var->MakeHoleIterator(true);
  }

  ~MapDomain() override {}

  void Post() override {
    Demon* vd = MakeConstraintDemon0(solver(), this, &MapDomain::VarDomain,
                                     "VarDomain");
    var_->WhenDomain(vd);
    Demon* vb =
        MakeConstraintDemon0(solver(), this, &MapDomain::VarBound, "VarBound");
    var_->WhenBound(vb);
    std::unique_ptr<IntVarIterator> domain_it(
        var_->MakeDomainIterator(/*reversible=*/false));
    for (const int64_t index : InitAndGetValues(domain_it.get())) {
      if (index >= 0 && index < actives_.size() && !actives_[index]->Bound()) {
        Demon* d = MakeConstraintDemon1(
            solver(), this, &MapDomain::UpdateActive, "UpdateActive", index);
        actives_[index]->WhenDomain(d);
      }
    }
  }

  void InitialPropagate() override {
    for (int i = 0; i < actives_.size(); ++i) {
      actives_[i]->SetRange(int64_t{0}, int64_t{1});
      if (!var_->Contains(i)) {
        actives_[i]->SetValue(0);
      } else if (actives_[i]->Max() == 0LL) {
        var_->RemoveValue(i);
      }
      if (actives_[i]->Min() == 1LL) {
        var_->SetValue(i);
      }
    }
    if (var_->Bound()) {
      VarBound();
    }
  }

  void UpdateActive(int64_t index) {
    IntVar* const act = actives_[index];
    if (act->Max() == 0) {
      var_->RemoveValue(index);
    } else if (act->Min() == 1) {
      var_->SetValue(index);
    }
  }

  void VarDomain() {
    const int64_t oldmin = var_->OldMin();
    const int64_t oldmax = var_->OldMax();
    const int64_t vmin = var_->Min();
    const int64_t vmax = var_->Max();
    const int64_t size = actives_.size();
    for (int64_t j = std::max(oldmin, int64_t{0}); j < std::min(vmin, size);
         ++j) {
      actives_[j]->SetValue(0);
    }
    for (const int64_t j : InitAndGetValues(holes_)) {
      if (j >= 0 && j < size) {
        actives_[j]->SetValue(0);
      }
    }
    for (int64_t j = std::max(vmax + int64_t{1}, int64_t{0});
         j <= std::min(oldmax, size - int64_t{1}); ++j) {
      actives_[j]->SetValue(int64_t{0});
    }
  }

  void VarBound() {
    const int64_t val = var_->Min();
    if (val >= 0 && val < actives_.size()) {
      actives_[val]->SetValue(1);
    }
  }
  std::string DebugString() const override {
    return absl::StrFormat("MapDomain(%s, [%s])", var_->DebugString(),
                           JoinDebugStringPtr(actives_, ", "));
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kMapDomain, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            var_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               actives_);
    visitor->EndVisitConstraint(ModelVisitor::kMapDomain, this);
  }

 private:
  IntVar* const var_;
  std::vector<IntVar*> actives_;
  IntVarIterator* holes_;
};

// ----- Lex constraint -----

class LexicalLessOrEqual : public Constraint {
 public:
  LexicalLessOrEqual(Solver* const s, std::vector<IntVar*> left,
                     std::vector<IntVar*> right, std::vector<int64_t> offsets)
      : Constraint(s),
        left_(std::move(left)),
        right_(std::move(right)),
        active_var_(0),
        offsets_(std::move(offsets)),
        demon_added_(offsets_.size(), false),
        demon_(nullptr) {
    CHECK_EQ(left_.size(), right_.size());
    CHECK_EQ(offsets_.size(), right_.size());
    CHECK(std::all_of(offsets_.begin(), offsets_.end(),
                      [](int step) { return step > 0; }));
  }

  ~LexicalLessOrEqual() override {}

  void Post() override {
    const int position = JumpEqualVariables(0);
    active_var_.SetValue(solver(), position);
    if (position < left_.size()) {
      demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
      AddDemon(position);
    }
  }

  void InitialPropagate() override {
    const int position = JumpEqualVariables(active_var_.Value());
    if (position >= left_.size()) return;
    if (position != active_var_.Value()) {
      AddDemon(position);
      active_var_.SetValue(solver(), position);
    }
    const int next_non_equal = JumpEqualVariables(position + 1);
    if (next_non_equal < left_.size() &&
        left_[next_non_equal]->Min() > right_[next_non_equal]->Max()) {
      // We need to be strict if at next_non_equal, left is above right.
      left_[position]->SetMax(
          CapSub(right_[position]->Max(), offsets_[position]));
      right_[position]->SetMin(
          CapAdd(left_[position]->Min(), offsets_[position]));
    } else {
      left_[position]->SetMax(right_[position]->Max());
      right_[position]->SetMin(left_[position]->Min());
    }
    // Adding demons for the next position as it may trigger changes at the
    // active position (if the next position becomes invalid for instance).
    if (next_non_equal < left_.size()) {
      AddDemon(next_non_equal);
    }
  }

  std::string DebugString() const override {
    return absl::StrFormat(
        "LexicalLessOrEqual([%s], [%s], [%s])", JoinDebugStringPtr(left_, ", "),
        JoinDebugStringPtr(right_, ", "), absl::StrJoin(offsets_, ", "));
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kLexLess, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kLeftArgument,
                                               left_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kRightArgument,
                                               right_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument, offsets_);
    visitor->EndVisitConstraint(ModelVisitor::kLexLess, this);
  }

 private:
  int JumpEqualVariables(int start_position) const {
    int position = start_position;
    while (position < left_.size() &&
           left_[position]->Max() <= right_[position]->Min() &&
           CapSub(right_[position]->Max(), CapSub(offsets_[position], 1)) <=
               left_[position]->Min()) {
      position++;
    }
    return position;
  }
  void AddDemon(int position) {
    if (demon_added_.Value(position)) return;
    left_[position]->WhenRange(demon_);
    right_[position]->WhenRange(demon_);
    demon_added_.SetValue(solver(), position, true);
  }

  std::vector<IntVar*> left_;
  std::vector<IntVar*> right_;
  NumericalRev<int> active_var_;
  std::vector<int64_t> offsets_;
  RevArray<bool> demon_added_;
  Demon* demon_;
};

// ----- Inverse permutation constraint -----

class InversePermutationConstraint : public Constraint {
 public:
  InversePermutationConstraint(Solver* const s,
                               const std::vector<IntVar*>& left,
                               const std::vector<IntVar*>& right)
      : Constraint(s),
        left_(left),
        right_(right),
        left_hole_iterators_(left.size()),
        left_domain_iterators_(left_.size()),
        right_hole_iterators_(right_.size()),
        right_domain_iterators_(right_.size()) {
    CHECK_EQ(left_.size(), right_.size());
    for (int i = 0; i < left_.size(); ++i) {
      left_hole_iterators_[i] = left_[i]->MakeHoleIterator(true);
      left_domain_iterators_[i] = left_[i]->MakeDomainIterator(true);
      right_hole_iterators_[i] = right_[i]->MakeHoleIterator(true);
      right_domain_iterators_[i] = right_[i]->MakeDomainIterator(true);
    }
  }

  ~InversePermutationConstraint() override {}

  void Post() override {
    for (int i = 0; i < left_.size(); ++i) {
      Demon* const left_demon = MakeConstraintDemon1(
          solver(), this,
          &InversePermutationConstraint::PropagateHolesOfLeftVarToRight,
          "PropagateHolesOfLeftVarToRight", i);
      left_[i]->WhenDomain(left_demon);
      Demon* const right_demon = MakeConstraintDemon1(
          solver(), this,
          &InversePermutationConstraint::PropagateHolesOfRightVarToLeft,
          "PropagateHolesOfRightVarToLeft", i);
      right_[i]->WhenDomain(right_demon);
    }
    solver()->AddConstraint(
        solver()->MakeAllDifferent(left_, /*stronger_propagation=*/false));
    solver()->AddConstraint(
        solver()->MakeAllDifferent(right_, /*stronger_propagation=*/false));
  }

  void InitialPropagate() override {
    const int size = left_.size();
    for (int i = 0; i < size; ++i) {
      left_[i]->SetRange(0, size - 1);
      right_[i]->SetRange(0, size - 1);
    }
    for (int i = 0; i < size; ++i) {
      PropagateDomain(i, left_[i], left_domain_iterators_[i], right_);
      PropagateDomain(i, right_[i], right_domain_iterators_[i], left_);
    }
  }

  void PropagateHolesOfLeftVarToRight(int index) {
    PropagateHoles(index, left_[index], left_hole_iterators_[index], right_);
  }

  void PropagateHolesOfRightVarToLeft(int index) {
    PropagateHoles(index, right_[index], right_hole_iterators_[index], left_);
  }

  std::string DebugString() const override {
    return absl::StrFormat("InversePermutationConstraint([%s], [%s])",
                           JoinDebugStringPtr(left_, ", "),
                           JoinDebugStringPtr(right_, ", "));
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kInversePermutation, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kLeftArgument,
                                               left_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kRightArgument,
                                               right_);
    visitor->EndVisitConstraint(ModelVisitor::kInversePermutation, this);
  }

 private:
  // See PropagateHolesOfLeftVarToRight() and PropagateHolesOfRightVarToLeft().
  void PropagateHoles(int index, IntVar* const var, IntVarIterator* const holes,
                      const std::vector<IntVar*>& inverse) {
    const int64_t oldmin = std::max(var->OldMin(), int64_t{0});
    const int64_t oldmax =
        std::min(var->OldMax(), static_cast<int64_t>(left_.size() - 1));
    const int64_t vmin = var->Min();
    const int64_t vmax = var->Max();
    for (int64_t value = oldmin; value < vmin; ++value) {
      inverse[value]->RemoveValue(index);
    }
    for (const int64_t hole : InitAndGetValues(holes)) {
      if (hole >= 0 && hole < left_.size()) {
        inverse[hole]->RemoveValue(index);
      }
    }
    for (int64_t value = vmax + 1; value <= oldmax; ++value) {
      inverse[value]->RemoveValue(index);
    }
  }

  void PropagateDomain(int index, IntVar* const var,
                       IntVarIterator* const domain,
                       const std::vector<IntVar*>& inverse) {
    // Iterators are not safe w.r.t. removal. Postponing deletions.
    tmp_removed_values_.clear();
    for (const int64_t value : InitAndGetValues(domain)) {
      if (!inverse[value]->Contains(index)) {
        tmp_removed_values_.push_back(value);
      }
    }
    // Once we've finished iterating over the domain, we may call
    // RemoveValues().
    if (!tmp_removed_values_.empty()) {
      var->RemoveValues(tmp_removed_values_);
    }
  }

  std::vector<IntVar*> left_;
  std::vector<IntVar*> right_;
  std::vector<IntVarIterator*> left_hole_iterators_;
  std::vector<IntVarIterator*> left_domain_iterators_;
  std::vector<IntVarIterator*> right_hole_iterators_;
  std::vector<IntVarIterator*> right_domain_iterators_;

  // used only in PropagateDomain().
  std::vector<int64_t> tmp_removed_values_;
};

// Index of first Max Value

class IndexOfFirstMaxValue : public Constraint {
 public:
  IndexOfFirstMaxValue(Solver* solver, IntVar* index,
                       const std::vector<IntVar*>& vars)
      : Constraint(solver), index_(index), vars_(vars) {}

  ~IndexOfFirstMaxValue() override {}

  void Post() override {
    Demon* const demon =
        solver()->MakeDelayedConstraintInitialPropagateCallback(this);
    index_->WhenRange(demon);
    for (IntVar* const var : vars_) {
      var->WhenRange(demon);
    }
  }

  void InitialPropagate() override {
    const int64_t vsize = vars_.size();
    const int64_t imin = std::max(int64_t{0}, index_->Min());
    const int64_t imax = std::min(vsize - 1, index_->Max());
    int64_t max_max = std::numeric_limits<int64_t>::min();
    int64_t max_min = std::numeric_limits<int64_t>::min();

    // Compute min and max value in the current interval covered by index_.
    for (int i = imin; i <= imax; ++i) {
      max_max = std::max(max_max, vars_[i]->Max());
      max_min = std::max(max_min, vars_[i]->Min());
    }

    // Propagate the fact that the first maximum value belongs to the
    // [imin..imax].
    for (int i = 0; i < imin; ++i) {
      vars_[i]->SetMax(max_max - 1);
    }
    for (int i = imax + 1; i < vsize; ++i) {
      vars_[i]->SetMax(max_max);
    }

    // Shave bounds for index_.
    int64_t min_index = imin;
    while (vars_[min_index]->Max() < max_min) {
      min_index++;
    }
    int64_t max_index = imax;
    while (vars_[max_index]->Max() < max_min) {
      max_index--;
    }
    index_->SetRange(min_index, max_index);
  }

  std::string DebugString() const override {
    return absl::StrFormat("IndexMax(%s, [%s])", index_->DebugString(),
                           JoinDebugStringPtr(vars_, ", "));
  }

  void Accept(ModelVisitor* const visitor) const override {
    // TODO(user): Implement me.
  }

 private:
  IntVar* const index_;
  const std::vector<IntVar*> vars_;
};
}  // namespace

// ----- API -----

Demon* Solver::MakeActionDemon(Solver::Action action) {
  return RevAlloc(new ActionDemon(action));
}

Demon* Solver::MakeClosureDemon(Solver::Closure closure) {
  return RevAlloc(new ClosureDemon(closure));
}

Constraint* Solver::MakeTrueConstraint() {
  DCHECK(true_constraint_ != nullptr);
  return true_constraint_;
}

Constraint* Solver::MakeFalseConstraint() {
  DCHECK(false_constraint_ != nullptr);
  return false_constraint_;
}
Constraint* Solver::MakeFalseConstraint(const std::string& explanation) {
  return RevAlloc(new FalseConstraint(this, explanation));
}

void Solver::InitCachedConstraint() {
  DCHECK(true_constraint_ == nullptr);
  true_constraint_ = RevAlloc(new TrueConstraint(this));
  DCHECK(false_constraint_ == nullptr);
  false_constraint_ = RevAlloc(new FalseConstraint(this));
}

Constraint* Solver::MakeMapDomain(IntVar* const var,
                                  const std::vector<IntVar*>& actives) {
  return RevAlloc(new MapDomain(this, var, actives));
}

Constraint* Solver::MakeLexicalLess(const std::vector<IntVar*>& left,
                                    const std::vector<IntVar*>& right) {
  std::vector<IntVar*> adjusted_left = left;
  adjusted_left.push_back(MakeIntConst(1));
  std::vector<IntVar*> adjusted_right = right;
  adjusted_right.push_back(MakeIntConst(0));
  return MakeLexicalLessOrEqualWithOffsets(
      std::move(adjusted_left), std::move(adjusted_right),
      std::vector<int64_t>(left.size() + 1, 1));
}

Constraint* Solver::MakeLexicalLessOrEqual(const std::vector<IntVar*>& left,
                                           const std::vector<IntVar*>& right) {
  return MakeLexicalLessOrEqualWithOffsets(
      left, right, std::vector<int64_t>(left.size(), 1));
}

Constraint* Solver::MakeLexicalLessOrEqualWithOffsets(
    std::vector<IntVar*> left, std::vector<IntVar*> right,
    std::vector<int64_t> offsets) {
  return RevAlloc(new LexicalLessOrEqual(this, std::move(left),
                                         std::move(right), std::move(offsets)));
}

Constraint* Solver::MakeIsLexicalLessOrEqualWithOffsetsCt(
    std::vector<IntVar*> left, std::vector<IntVar*> right,
    std::vector<int64_t> offsets, IntVar* boolvar) {
  std::vector<IntVar*> adjusted_left = std::move(left);
  adjusted_left.insert(adjusted_left.begin(), boolvar);
  std::vector<IntVar*> adjusted_right = std::move(right);
  adjusted_right.insert(adjusted_right.begin(), MakeIntConst(1));
  std::vector<int64_t> adjusted_offsets = std::move(offsets);
  adjusted_offsets.insert(adjusted_offsets.begin(), 1);
  return MakeLexicalLessOrEqualWithOffsets(std::move(adjusted_left),
                                           std::move(adjusted_right),
                                           std::move(adjusted_offsets));
}

Constraint* Solver::MakeInversePermutationConstraint(
    const std::vector<IntVar*>& left, const std::vector<IntVar*>& right) {
  return RevAlloc(new InversePermutationConstraint(this, left, right));
}

Constraint* Solver::MakeIndexOfFirstMaxValueConstraint(
    IntVar* index, const std::vector<IntVar*>& vars) {
  return RevAlloc(new IndexOfFirstMaxValue(this, index, vars));
}

Constraint* Solver::MakeIndexOfFirstMinValueConstraint(
    IntVar* index, const std::vector<IntVar*>& vars) {
  std::vector<IntVar*> opp_vars(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    opp_vars[i] = MakeOpposite(vars[i])->Var();
  }
  return RevAlloc(new IndexOfFirstMaxValue(this, index, opp_vars));
}
}  // namespace operations_research

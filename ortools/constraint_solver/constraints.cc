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

#include "ortools/constraint_solver/constraints.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/string_array.h"

namespace operations_research {

void ActionDemon::Run(Solver* solver) { action_(solver); }

void ClosureDemon::Run(Solver* solver) { closure_(); }

void TrueConstraint::Post() {}
void TrueConstraint::InitialPropagate() {}
std::string TrueConstraint::DebugString() const { return "TrueConstraint()"; }

void TrueConstraint::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kTrueConstraint, this);
  visitor->EndVisitConstraint(ModelVisitor::kTrueConstraint, this);
}
IntVar* TrueConstraint::Var() { return solver()->MakeIntConst(1); }

void FalseConstraint::Post() {}
void FalseConstraint::InitialPropagate() { solver()->Fail(); }
std::string FalseConstraint::DebugString() const {
  return absl::StrCat("FalseConstraint(", explanation_, ")");
}

void FalseConstraint::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kFalseConstraint, this);
  visitor->EndVisitConstraint(ModelVisitor::kFalseConstraint, this);
}
IntVar* FalseConstraint::Var() { return solver()->MakeIntConst(0); }

void MapDomain::Post() {
  Demon* vd =
      MakeConstraintDemon0(solver(), this, &MapDomain::VarDomain, "VarDomain");
  var_->WhenDomain(vd);
  Demon* vb =
      MakeConstraintDemon0(solver(), this, &MapDomain::VarBound, "VarBound");
  var_->WhenBound(vb);
  std::unique_ptr<IntVarIterator> domain_it(
      var_->MakeDomainIterator(/*reversible=*/false));
  for (const int64_t index : InitAndGetValues(domain_it.get())) {
    if (index >= 0 && index < actives_.size() && !actives_[index]->Bound()) {
      Demon* d = MakeConstraintDemon1(solver(), this, &MapDomain::UpdateActive,
                                      "UpdateActive", index);
      actives_[index]->WhenDomain(d);
    }
  }
}

void MapDomain::InitialPropagate() {
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

void MapDomain::UpdateActive(int64_t index) {
  IntVar* const act = actives_[index];
  if (act->Max() == 0) {
    var_->RemoveValue(index);
  } else if (act->Min() == 1) {
    var_->SetValue(index);
  }
}

void MapDomain::VarDomain() {
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

void MapDomain::VarBound() {
  const int64_t val = var_->Min();
  if (val >= 0 && val < actives_.size()) {
    actives_[val]->SetValue(1);
  }
}
std::string MapDomain::DebugString() const {
  return absl::StrFormat("MapDomain(%s, [%s])", var_->DebugString(),
                         JoinDebugStringPtr(actives_, ", "));
}

void MapDomain::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kMapDomain, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument, var_);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             actives_);
  visitor->EndVisitConstraint(ModelVisitor::kMapDomain, this);
}

void LexicalLessOrEqual::Post() {
  const int position = JumpEqualVariables(0);
  active_var_.SetValue(solver(), position);
  if (position < left_.size()) {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    AddDemon(position);
  }
}

void LexicalLessOrEqual::InitialPropagate() {
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

std::string LexicalLessOrEqual::DebugString() const {
  return absl::StrFormat(
      "LexicalLessOrEqual([%s], [%s], [%s])", JoinDebugStringPtr(left_, ", "),
      JoinDebugStringPtr(right_, ", "), absl::StrJoin(offsets_, ", "));
}

void LexicalLessOrEqual::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kLexLess, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kLeftArgument,
                                             left_);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kRightArgument,
                                             right_);
  visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument, offsets_);
  visitor->EndVisitConstraint(ModelVisitor::kLexLess, this);
}

int LexicalLessOrEqual::JumpEqualVariables(int start_position) const {
  int position = start_position;
  while (position < left_.size() &&
         left_[position]->Max() <= right_[position]->Min() &&
         CapSub(right_[position]->Max(), CapSub(offsets_[position], 1)) <=
             left_[position]->Min()) {
    position++;
  }
  return position;
}
void LexicalLessOrEqual::AddDemon(int position) {
  if (demon_added_.Value(position)) return;
  left_[position]->WhenRange(demon_);
  right_[position]->WhenRange(demon_);
  demon_added_.SetValue(solver(), position, true);
}

void InversePermutationConstraint::Post() {
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

void InversePermutationConstraint::InitialPropagate() {
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

void InversePermutationConstraint::PropagateHolesOfLeftVarToRight(int index) {
  PropagateHoles(index, left_[index], left_hole_iterators_[index], right_);
}

void InversePermutationConstraint::PropagateHolesOfRightVarToLeft(int index) {
  PropagateHoles(index, right_[index], right_hole_iterators_[index], left_);
}

std::string InversePermutationConstraint::DebugString() const {
  return absl::StrFormat("InversePermutationConstraint([%s], [%s])",
                         JoinDebugStringPtr(left_, ", "),
                         JoinDebugStringPtr(right_, ", "));
}

void InversePermutationConstraint::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kInversePermutation, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kLeftArgument,
                                             left_);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kRightArgument,
                                             right_);
  visitor->EndVisitConstraint(ModelVisitor::kInversePermutation, this);
}

void InversePermutationConstraint::PropagateHoles(
    int index, IntVar* const var, IntVarIterator* const holes,
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

void InversePermutationConstraint::PropagateDomain(
    int index, IntVar* const var, IntVarIterator* const domain,
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

void IndexOfFirstMaxValue::Post() {
  Demon* const demon =
      solver()->MakeDelayedConstraintInitialPropagateCallback(this);
  index_->WhenRange(demon);
  for (IntVar* const var : vars_) {
    var->WhenRange(demon);
  }
}

void IndexOfFirstMaxValue::InitialPropagate() {
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

std::string IndexOfFirstMaxValue::DebugString() const {
  return absl::StrFormat("IndexMax(%s, [%s])", index_->DebugString(),
                         JoinDebugStringPtr(vars_, ", "));
}

void IndexOfFirstMaxValue::Accept(ModelVisitor* visitor) const {
  // TODO(user): Implement me.
}

}  // namespace operations_research

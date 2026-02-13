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

//  Count constraints

#include "ortools/constraint_solver/count_cst.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraints.h"
#include "ortools/constraint_solver/utilities.h"
#include "ortools/util/string_array.h"

namespace operations_research {

void AtMost::Post() {
  for (IntVar* var : vars_) {
    if (!var->Bound() && var->Contains(value_)) {
      Demon* d = MakeConstraintDemon1(solver(), this, &AtMost::OneBound,
                                      "OneBound", var);
      var->WhenBound(d);
    }
  }
}

void AtMost::InitialPropagate() {
  for (IntVar* var : vars_) {
    if (var->Bound() && var->Min() == value_) {
      current_count_.Incr(solver());
    }
  }
  CheckCount();
}

void AtMost::OneBound(IntVar* var) {
  if (var->Min() == value_) {
    current_count_.Incr(solver());
    CheckCount();
  }
}

void AtMost::CheckCount() {
  if (current_count_.Value() < max_count_) {
    return;
  }

  // Remove all remaining values.
  int forced = 0;
  for (IntVar* var : vars_) {
    if (var->Bound()) {
      if (var->Min() == value_) {
        forced++;
      }
    } else {
      var->RemoveValue(value_);
    }
  }
  if (forced > max_count_) {
    solver()->Fail();
  }
}

std::string AtMost::DebugString() const {
  return absl::StrFormat("AtMost(%s, %d, %d)", JoinDebugStringPtr(vars_, ", "),
                         value_, max_count_);
}

void AtMost::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kAtMost, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
  visitor->VisitIntegerArgument(ModelVisitor::kCountArgument, max_count_);
  visitor->EndVisitConstraint(ModelVisitor::kAtMost, this);
}

void Distribute::Post() {
  for (int i = 0; i < var_size(); ++i) {
    IntVar* var = vars_[i];
    if (!var->Bound()) {
      Demon* d = MakeConstraintDemon1(solver(), this, &Distribute::OneBound,
                                      "OneBound", i);
      var->WhenBound(d);
      d = MakeConstraintDemon1(solver(), this, &Distribute::OneDomain,
                               "OneDomain", i);
      var->WhenDomain(d);
    }
  }
  for (int i = 0; i < card_size(); ++i) {
    if (!cards_[i]->Bound()) {
      Demon* d =
          MakeConstraintDemon1(solver(), this, &Distribute::CountVar, "Var", i);
      cards_[i]->WhenRange(d);
    }
  }
}

void Distribute::InitialPropagate() {
  Solver* s = solver();
  for (int j = 0; j < card_size(); ++j) {
    int min = 0;
    int max = 0;
    for (int i = 0; i < var_size(); ++i) {
      IntVar* var = vars_[i];
      if (var->Bound()) {
        if (var->Min() == values_[j]) {
          min++;
          max++;
        }
      } else {
        if (var->Contains(values_[j])) {
          max++;
          undecided_.SetToOne(s, i, j);
        }
      }
    }
    cards_[j]->SetRange(min, max);
    if (cards_[j]->Max() == min) {
      CardMin(j);
    } else if (cards_[j]->Min() == max) {
      CardMax(j);
    }
    min_.SetValue(s, j, min);
    max_.SetValue(s, j, max);
  }
}

void Distribute::OneBound(int index) {
  IntVar* var = vars_[index];
  Solver* s = solver();
  for (int j = 0; j < card_size(); ++j) {
    if (undecided_.IsSet(index, j)) {
      undecided_.SetToZero(s, index, j);
      if (var->Min() == values_[j]) {
        min_.Incr(s, j);
        cards_[j]->SetMin(min_[j]);
        if (min_[j] == cards_[j]->Max()) {
          CardMin(j);
        }
      } else {
        max_.Decr(s, j);
        cards_[j]->SetMax(max_[j]);
        if (max_[j] == cards_[j]->Min()) {
          CardMax(j);
        }
      }
    }
  }
}

void Distribute::OneDomain(int index) {
  IntVar* var = vars_[index];
  Solver* s = solver();
  for (int j = 0; j < card_size(); ++j) {
    if (undecided_.IsSet(index, j)) {
      if (!var->Contains(values_[j])) {
        undecided_.SetToZero(s, index, j);
        max_.Decr(s, j);
        cards_[j]->SetMax(max_[j]);
        if (max_[j] == cards_[j]->Min()) {
          CardMax(j);
        }
      }
    }
  }
}

void Distribute::CountVar(int cindex) {
  if (cards_[cindex]->Min() > max_[cindex] ||
      cards_[cindex]->Max() < min_[cindex]) {
    solver()->Fail();
  }
  if (cards_[cindex]->Min() == max_[cindex]) {
    CardMax(cindex);
  }
  if (cards_[cindex]->Max() == min_[cindex]) {
    CardMin(cindex);
  }
}

void Distribute::CardMin(int cindex) {
  for (int i = 0; i < var_size(); ++i) {
    if (undecided_.IsSet(i, cindex)) {
      vars_[i]->RemoveValue(values_[cindex]);
    }
  }
}

void Distribute::CardMax(int cindex) {
  for (int i = 0; i < var_size(); ++i) {
    if (undecided_.IsSet(i, cindex)) {
      vars_[i]->SetValue(values_[cindex]);
    }
  }
}

std::string Distribute::DebugString() const {
  return absl::StrFormat("Distribute(vars = [%s], values = [%s], cards = [%s])",
                         JoinDebugStringPtr(vars_, ", "),
                         absl::StrJoin(values_, ", "),
                         JoinDebugStringPtr(cards_, ", "));
}

void Distribute::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kDistribute, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument, values_);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kCardsArgument,
                                             cards_);
  visitor->EndVisitConstraint(ModelVisitor::kDistribute, this);
}

FastDistribute::FastDistribute(Solver* s, const std::vector<IntVar*>& vars,
                               const std::vector<IntVar*>& cards)
    : Constraint(s),
      vars_(vars),
      cards_(cards),
      undecided_(vars.size(), cards.size()),
      min_(cards.size(), 0),
      max_(cards.size(), 0),
      holes_(vars.size()) {
  for (int var_index = 0; var_index < var_size(); ++var_index) {
    holes_[var_index] = vars_[var_index]->MakeHoleIterator(true);
  }
}

std::string FastDistribute::DebugString() const {
  return absl::StrFormat("FastDistribute(vars = [%s], cards = [%s])",
                         JoinDebugStringPtr(vars_, ", "),
                         JoinDebugStringPtr(cards_, ", "));
}

void FastDistribute::Post() {
  for (int var_index = 0; var_index < var_size(); ++var_index) {
    IntVar* var = vars_[var_index];
    if (!var->Bound()) {
      Demon* d = MakeConstraintDemon1(solver(), this, &FastDistribute::OneBound,
                                      "OneBound", var_index);
      var->WhenBound(d);
      d = MakeConstraintDemon1(solver(), this, &FastDistribute::OneDomain,
                               "OneDomain", var_index);
      var->WhenDomain(d);
    }
  }
  for (int card_index = 0; card_index < card_size(); ++card_index) {
    if (!cards_[card_index]->Bound()) {
      Demon* d = MakeConstraintDemon1(solver(), this, &FastDistribute::CountVar,
                                      "Var", card_index);
      cards_[card_index]->WhenRange(d);
    }
  }
}

void FastDistribute::InitialPropagate() {
  Solver* s = solver();
  for (int card_index = 0; card_index < card_size(); ++card_index) {
    int min = 0;
    int max = 0;
    for (int var_index = 0; var_index < var_size(); ++var_index) {
      IntVar* var = vars_[var_index];
      if (var->Bound() && var->Min() == card_index) {
        min++;
        max++;
      } else if (var->Contains(card_index)) {
        max++;
        undecided_.SetToOne(s, var_index, card_index);
      }
    }
    min_.SetValue(s, card_index, min);
    max_.SetValue(s, card_index, max);
    CountVar(card_index);
  }
}

void FastDistribute::OneBound(int index) {
  IntVar* var = vars_[index];
  for (int card_index = 0; card_index < card_size(); ++card_index) {
    if (undecided_.IsSet(index, card_index)) {
      if (var->Min() == card_index) {
        SetRevDoContribute(index, card_index);
      } else {
        SetRevCannotContribute(index, card_index);
      }
    }
  }
}

void FastDistribute::OneDomain(int index) {
  IntVar* var = vars_[index];
  const int64_t oldmin = var->OldMin();
  const int64_t oldmax = var->OldMax();
  const int64_t vmin = var->Min();
  const int64_t vmax = var->Max();
  for (int64_t card_index = std::max(oldmin, int64_t{0});
       card_index < std::min(vmin, card_size()); ++card_index) {
    if (undecided_.IsSet(index, card_index)) {
      SetRevCannotContribute(index, card_index);
    }
  }
  for (const int64_t card_index : InitAndGetValues(holes_[index])) {
    if (card_index >= 0 && card_index < card_size() &&
        undecided_.IsSet(index, card_index)) {
      SetRevCannotContribute(index, card_index);
    }
  }
  for (int64_t card_index = std::max(vmax + 1, int64_t{0});
       card_index <= std::min(oldmax, card_size() - 1); ++card_index) {
    if (undecided_.IsSet(index, card_index)) {
      SetRevCannotContribute(index, card_index);
    }
  }
}

void FastDistribute::CountVar(int card_index) {
  const int64_t stored_min = min_[card_index];
  const int64_t stored_max = max_[card_index];
  cards_[card_index]->SetRange(min_[card_index], max_[card_index]);
  if (cards_[card_index]->Min() == stored_max) {
    CardMax(card_index);
  }
  if (cards_[card_index]->Max() == stored_min) {
    CardMin(card_index);
  }
}

void FastDistribute::CardMin(int card_index) {
  for (int var_index = 0; var_index < var_size(); ++var_index) {
    if (undecided_.IsSet(var_index, card_index)) {
      vars_[var_index]->RemoveValue(card_index);
    }
  }
}

void FastDistribute::CardMax(int card_index) {
  for (int var_index = 0; var_index < var_size(); ++var_index) {
    if (undecided_.IsSet(var_index, card_index)) {
      vars_[var_index]->SetValue(card_index);
    }
  }
}

void FastDistribute::SetRevCannotContribute(int64_t var_index,
                                            int64_t card_index) {
  Solver* s = solver();
  undecided_.SetToZero(s, var_index, card_index);
  max_.Decr(s, card_index);
  cards_[card_index]->SetMax(max_[card_index]);
  if (max_[card_index] == cards_[card_index]->Min()) {
    CardMax(card_index);
  }
}
void FastDistribute::SetRevDoContribute(int64_t var_index, int64_t card_index) {
  Solver* s = solver();
  undecided_.SetToZero(s, var_index, card_index);
  min_.Incr(s, card_index);
  cards_[card_index]->SetMin(min_[card_index]);
  if (min_[card_index] == cards_[card_index]->Max()) {
    CardMin(card_index);
  }
}

void FastDistribute::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kDistribute, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kCardsArgument,
                                             cards_);
  visitor->EndVisitConstraint(ModelVisitor::kDistribute, this);
}

BoundedDistribute::BoundedDistribute(Solver* s,
                                     const std::vector<IntVar*>& vars,
                                     const std::vector<int64_t>& values,
                                     const std::vector<int64_t>& card_min,
                                     const std::vector<int64_t>& card_max)
    : Constraint(s),
      vars_(vars),
      values_(values),
      card_min_(card_min),
      card_max_(card_max),
      undecided_(vars.size(), values.size()),
      min_(values.size(), 0),
      max_(values.size(), 0),
      holes_(vars.size()) {
  for (int var_index = 0; var_index < var_size(); ++var_index) {
    holes_[var_index] = vars_[var_index]->MakeHoleIterator(true);
  }
}

std::string BoundedDistribute::DebugString() const {
  return absl::StrFormat(
      "BoundedDistribute([%s], values = [%s], card_min = [%s], card_max = "
      "[%s]",
      JoinDebugStringPtr(vars_, ", "), absl::StrJoin(values_, ", "),
      absl::StrJoin(card_min_, ", "), absl::StrJoin(card_max_, ", "));
}

void BoundedDistribute::Post() {
  for (int var_index = 0; var_index < var_size(); ++var_index) {
    IntVar* var = vars_[var_index];
    if (!var->Bound()) {
      Demon* d = MakeConstraintDemon1(
          solver(), this, &BoundedDistribute::OneBound, "OneBound", var_index);
      var->WhenBound(d);
      d = MakeConstraintDemon1(solver(), this, &BoundedDistribute::OneDomain,
                               "OneDomain", var_index);
      var->WhenDomain(d);
    }
  }
}

void BoundedDistribute::InitialPropagate() {
  Solver* s = solver();

  int64_t sum_card_min = 0;
  for (int i = 0; i < card_size(); ++i) {
    if (card_max_[i] < card_min_[i]) {
      solver()->Fail();
    }
    sum_card_min += card_min_[i];
  }
  if (sum_card_min > var_size()) {
    s->Fail();
  }
  if (sum_card_min == var_size()) {
    for (int i = 0; i < var_size(); ++i) {
      vars_[i]->SetValues(values_);
    }
  }

  for (int card_index = 0; card_index < card_size(); ++card_index) {
    const int64_t value = values_[card_index];
    int min = 0;
    int max = 0;
    for (int i = 0; i < var_size(); ++i) {
      IntVar* var = vars_[i];
      if (var->Bound()) {
        if (var->Min() == value) {
          min++;
          max++;
        }
      } else if (var->Contains(value)) {
        max++;
        undecided_.SetToOne(s, i, card_index);
      }
    }
    min_.SetValue(s, card_index, min);
    max_.SetValue(s, card_index, max);
    CountVar(card_index);
  }
}

void BoundedDistribute::OneBound(int index) {
  IntVar* var = vars_[index];
  const int64_t var_min = var->Min();
  for (int card_index = 0; card_index < card_size(); ++card_index) {
    if (undecided_.IsSet(index, card_index)) {
      if (var_min == values_[card_index]) {
        SetRevDoContribute(index, card_index);
      } else {
        SetRevCannotContribute(index, card_index);
      }
    }
  }
}

void BoundedDistribute::OneDomain(int index) {
  IntVar* var = vars_[index];
  for (int card_index = 0; card_index < card_size(); ++card_index) {
    if (undecided_.IsSet(index, card_index)) {
      if (!var->Contains(values_[card_index])) {
        SetRevCannotContribute(index, card_index);
      }
    }
  }
}

void BoundedDistribute::CountVar(int card_index) {
  const int64_t stored_min = min_[card_index];
  const int64_t stored_max = max_[card_index];
  if (card_min_[card_index] > stored_max ||
      card_max_[card_index] < stored_min) {
    solver()->Fail();
  }
  if (card_min_[card_index] == stored_max) {
    CardMax(card_index);
  }
  if (card_max_[card_index] == stored_min) {
    CardMin(card_index);
  }
}

void BoundedDistribute::CardMin(int card_index) {
  for (int var_index = 0; var_index < var_size(); ++var_index) {
    if (undecided_.IsSet(var_index, card_index)) {
      vars_[var_index]->RemoveValue(values_[card_index]);
    }
  }
}

void BoundedDistribute::CardMax(int card_index) {
  for (int var_index = 0; var_index < var_size(); ++var_index) {
    if (undecided_.IsSet(var_index, card_index)) {
      vars_[var_index]->SetValue(values_[card_index]);
    }
  }
}

void BoundedDistribute::SetRevCannotContribute(int64_t var_index,
                                               int64_t card_index) {
  Solver* s = solver();
  undecided_.SetToZero(s, var_index, card_index);
  max_.Decr(s, card_index);
  if (max_[card_index] < card_min_[card_index]) {
    solver()->Fail();
  }
  if (max_[card_index] == card_min_[card_index]) {
    CardMax(card_index);
  }
}
void BoundedDistribute::SetRevDoContribute(int64_t var_index,
                                           int64_t card_index) {
  Solver* s = solver();
  undecided_.SetToZero(s, var_index, card_index);
  min_.Incr(s, card_index);
  if (min_[card_index] > card_max_[card_index]) {
    solver()->Fail();
  }
  if (min_[card_index] == card_max_[card_index]) {
    CardMin(card_index);
  }
}

void BoundedDistribute::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kDistribute, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument, values_);
  visitor->VisitIntegerArrayArgument(ModelVisitor::kMinArgument, card_min_);
  visitor->VisitIntegerArrayArgument(ModelVisitor::kMaxArgument, card_max_);
  visitor->EndVisitConstraint(ModelVisitor::kDistribute, this);
}

BoundedFastDistribute::BoundedFastDistribute(
    Solver* s, const std::vector<IntVar*>& vars,
    const std::vector<int64_t>& card_min, const std::vector<int64_t>& card_max)
    : Constraint(s),
      vars_(vars),
      card_min_(card_min),
      card_max_(card_max),
      undecided_(vars.size(), card_min.size()),
      min_(card_min.size(), 0),
      max_(card_max.size(), 0),
      holes_(vars.size()) {
  for (int var_index = 0; var_index < var_size(); ++var_index) {
    holes_[var_index] = vars_[var_index]->MakeHoleIterator(true);
  }
}

std::string BoundedFastDistribute::DebugString() const {
  return absl::StrFormat(
      "BoundedFastDistribute([%s], card_min = [%s], card_max = [%s]",
      JoinDebugStringPtr(vars_, ", "), absl::StrJoin(card_min_, ", "),
      absl::StrJoin(card_max_, ", "));
}

void BoundedFastDistribute::Post() {
  for (int var_index = 0; var_index < var_size(); ++var_index) {
    IntVar* var = vars_[var_index];
    if (!var->Bound()) {
      Demon* d =
          MakeConstraintDemon1(solver(), this, &BoundedFastDistribute::OneBound,
                               "OneBound", var_index);
      var->WhenBound(d);
      d = MakeConstraintDemon1(solver(), this,
                               &BoundedFastDistribute::OneDomain, "OneDomain",
                               var_index);
      var->WhenDomain(d);
    }
  }
}

void BoundedFastDistribute::InitialPropagate() {
  Solver* s = solver();

  int64_t sum_card_min = 0;
  for (int i = 0; i < card_size(); ++i) {
    if (card_max_[i] < card_min_[i]) {
      solver()->Fail();
    }
    sum_card_min += card_min_[i];
  }
  if (sum_card_min > var_size()) {
    s->Fail();
  }
  if (sum_card_min == var_size()) {
    for (int i = 0; i < var_size(); ++i) {
      vars_[i]->SetRange(0, card_size() - 1);
    }
  }

  for (int card_index = 0; card_index < card_size(); ++card_index) {
    int min = 0;
    int max = 0;
    for (int i = 0; i < var_size(); ++i) {
      IntVar* var = vars_[i];
      if (var->Bound()) {
        if (var->Min() == card_index) {
          min++;
          max++;
        }
      } else if (var->Contains(card_index)) {
        max++;
        undecided_.SetToOne(s, i, card_index);
      }
    }
    min_.SetValue(s, card_index, min);
    max_.SetValue(s, card_index, max);
    CountVar(card_index);
  }
}

void BoundedFastDistribute::OneBound(int index) {
  IntVar* var = vars_[index];
  const int64_t var_min = var->Min();
  for (int card_index = 0; card_index < card_size(); ++card_index) {
    if (undecided_.IsSet(index, card_index)) {
      if (var_min == card_index) {
        SetRevDoContribute(index, card_index);
      } else {
        SetRevCannotContribute(index, card_index);
      }
    }
  }
}

void BoundedFastDistribute::OneDomain(int index) {
  IntVar* var = vars_[index];
  const int64_t oldmin = var->OldMin();
  const int64_t oldmax = var->OldMax();
  const int64_t vmin = var->Min();
  const int64_t vmax = var->Max();
  for (int64_t card_index = std::max(oldmin, int64_t{0});
       card_index < std::min(vmin, card_size()); ++card_index) {
    if (undecided_.IsSet(index, card_index)) {
      SetRevCannotContribute(index, card_index);
    }
  }
  for (const int64_t card_index : InitAndGetValues(holes_[index])) {
    if (card_index >= 0 && card_index < card_size() &&
        undecided_.IsSet(index, card_index)) {
      SetRevCannotContribute(index, card_index);
    }
  }
  for (int64_t card_index = std::max(vmax + 1, int64_t{0});
       card_index <= std::min(oldmax, card_size() - 1); ++card_index) {
    if (undecided_.IsSet(index, card_index)) {
      SetRevCannotContribute(index, card_index);
    }
  }
}

void BoundedFastDistribute::CountVar(int card_index) {
  const int64_t stored_min = min_[card_index];
  const int64_t stored_max = max_[card_index];
  if (card_min_[card_index] > stored_max ||
      card_max_[card_index] < stored_min) {
    solver()->Fail();
  }
  if (card_min_[card_index] == stored_max) {
    CardMax(card_index);
  }
  if (card_max_[card_index] == stored_min) {
    CardMin(card_index);
  }
}

void BoundedFastDistribute::CardMin(int card_index) {
  for (int var_index = 0; var_index < var_size(); ++var_index) {
    if (undecided_.IsSet(var_index, card_index)) {
      vars_[var_index]->RemoveValue(card_index);
    }
  }
}

void BoundedFastDistribute::CardMax(int card_index) {
  for (int var_index = 0; var_index < var_size(); ++var_index) {
    if (undecided_.IsSet(var_index, card_index)) {
      vars_[var_index]->SetValue(card_index);
    }
  }
}

void BoundedFastDistribute::SetRevCannotContribute(int64_t var_index,
                                                   int64_t card_index) {
  Solver* s = solver();
  undecided_.SetToZero(s, var_index, card_index);
  max_.Decr(s, card_index);
  if (max_[card_index] < card_min_[card_index]) {
    solver()->Fail();
  }
  if (max_[card_index] == card_min_[card_index]) {
    CardMax(card_index);
  }
}
void BoundedFastDistribute::SetRevDoContribute(int64_t var_index,
                                               int64_t card_index) {
  Solver* s = solver();
  undecided_.SetToZero(s, var_index, card_index);
  min_.Incr(s, card_index);
  if (min_[card_index] > card_max_[card_index]) {
    solver()->Fail();
  }
  if (min_[card_index] == card_max_[card_index]) {
    CardMin(card_index);
  }
}

void BoundedFastDistribute::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kDistribute, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerArrayArgument(ModelVisitor::kMinArgument, card_min_);
  visitor->VisitIntegerArrayArgument(ModelVisitor::kMaxArgument, card_max_);
  visitor->EndVisitConstraint(ModelVisitor::kDistribute, this);
}

void SetAllToZero::Post() {}

void SetAllToZero::InitialPropagate() {
  for (int i = 0; i < vars_.size(); ++i) {
    vars_[i]->SetValue(0);
  }
}

std::string SetAllToZero::DebugString() const { return "SetAllToZero()"; }

void SetAllToZero::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kDistribute, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kCardsArgument,
                                             vars_);
  visitor->EndVisitConstraint(ModelVisitor::kDistribute, this);
}

}  // namespace operations_research

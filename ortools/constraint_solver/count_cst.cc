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

//
//  Count constraints

#include <algorithm>
#include <string>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/join.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/util/string_array.h"

namespace operations_research {
Constraint* Solver::MakeCount(const std::vector<IntVar*>& vars, int64 v,
                              int64 c) {
  std::vector<IntVar*> tmp_sum;
  for (int i = 0; i < vars.size(); ++i) {
    if (vars[i]->Contains(v)) {
      if (vars[i]->Bound()) {
        c--;
      } else {
        tmp_sum.push_back(MakeIsEqualCstVar(vars[i], v));
      }
    }
  }
  return MakeSumEquality(tmp_sum, c);
}

Constraint* Solver::MakeCount(const std::vector<IntVar*>& vars, int64 v,
                              IntVar* c) {
  if (c->Bound()) {
    return MakeCount(vars, v, c->Min());
  } else {
    std::vector<IntVar*> tmp_sum;
    int64 num_vars_bound_to_v = 0;
    for (int i = 0; i < vars.size(); ++i) {
      if (vars[i]->Contains(v)) {
        if (vars[i]->Bound()) {
          ++num_vars_bound_to_v;
        } else {
          tmp_sum.push_back(MakeIsEqualCstVar(vars[i], v));
        }
      }
    }
    return MakeSumEquality(tmp_sum, MakeSum(c, -num_vars_bound_to_v)->Var());
  }
}

// ---------- Distribute ----------

namespace {
class AtMost : public Constraint {
 public:
  AtMost(Solver* const s, std::vector<IntVar*> vars, int64 value,
         int64 max_count)
      : Constraint(s),
        vars_(std::move(vars)),
        value_(value),
        max_count_(max_count),
        current_count_(0) {}

  ~AtMost() override {}

  void Post() override {
    for (IntVar* var : vars_) {
      if (!var->Bound() && var->Contains(value_)) {
        Demon* const d = MakeConstraintDemon1(solver(), this, &AtMost::OneBound,
                                              "OneBound", var);
        var->WhenBound(d);
      }
    }
  }

  void InitialPropagate() override {
    for (IntVar* var : vars_) {
      if (var->Bound() && var->Min() == value_) {
        current_count_.Incr(solver());
      }
    }
    CheckCount();
  }

  void OneBound(IntVar* var) {
    if (var->Min() == value_) {
      current_count_.Incr(solver());
      CheckCount();
    }
  }

  void CheckCount() {
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

  std::string DebugString() const override {
    return StringPrintf("AtMost(%s, %" GG_LL_FORMAT "d, %" GG_LL_FORMAT "d)",
                        JoinDebugStringPtr(vars_, ", ").c_str(), value_,
                        max_count_);
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kAtMost, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
    visitor->VisitIntegerArgument(ModelVisitor::kCountArgument, max_count_);
    visitor->EndVisitConstraint(ModelVisitor::kAtMost, this);
  }

 private:
  const std::vector<IntVar*> vars_;
  const int64 value_;
  const int64 max_count_;
  NumericalRev<int> current_count_;
};

class Distribute : public Constraint {
 public:
  Distribute(Solver* const s, const std::vector<IntVar*>& vars,
             const std::vector<int64>& values,
             const std::vector<IntVar*>& cards)
      : Constraint(s),
        vars_(vars),
        values_(values),
        cards_(cards),
        undecided_(vars.size(), cards.size()),
        min_(cards.size(), 0),
        max_(cards.size(), 0) {}

  ~Distribute() override {}

  void Post() override;
  void InitialPropagate() override;
  void OneBound(int vindex);
  void OneDomain(int vindex);
  void CountVar(int cindex);
  void CardMin(int cindex);
  void CardMax(int cindex);
  std::string DebugString() const override {
    return StringPrintf("Distribute(vars = [%s], values = [%s], cards = [%s])",
                        JoinDebugStringPtr(vars_, ", ").c_str(),
                        strings::Join(values_, ", ").c_str(),
                        JoinDebugStringPtr(cards_, ", ").c_str());
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kDistribute, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument, values_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kCardsArgument,
                                               cards_);
    visitor->EndVisitConstraint(ModelVisitor::kDistribute, this);
  }

 private:
  int64 var_size() const { return vars_.size(); }
  int64 card_size() const { return cards_.size(); }

  const std::vector<IntVar*> vars_;
  const std::vector<int64> values_;
  const std::vector<IntVar*> cards_;
  RevBitMatrix undecided_;
  NumericalRevArray<int> min_;
  NumericalRevArray<int> max_;
};

void Distribute::Post() {
  for (int i = 0; i < var_size(); ++i) {
    IntVar* const var = vars_[i];
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
  Solver* const s = solver();
  for (int j = 0; j < card_size(); ++j) {
    int min = 0;
    int max = 0;
    for (int i = 0; i < var_size(); ++i) {
      IntVar* const var = vars_[i];
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
  IntVar* const var = vars_[index];
  Solver* const s = solver();
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
  IntVar* const var = vars_[index];
  Solver* const s = solver();
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

// ----- FastDistribute -----

class FastDistribute : public Constraint {
 public:
  FastDistribute(Solver* const s, const std::vector<IntVar*>& vars,
                 const std::vector<IntVar*>& cards);
  ~FastDistribute() override {}

  void Post() override;
  void InitialPropagate() override;
  void OneBound(int vindex);
  void OneDomain(int vindex);
  void CountVar(int card_index);
  void CardMin(int card_index);
  void CardMax(int card_index);
  std::string DebugString() const override;
  void SetRevCannotContribute(int64 var_index, int64 card_index) {
    Solver* const s = solver();
    undecided_.SetToZero(s, var_index, card_index);
    max_.Decr(s, card_index);
    cards_[card_index]->SetMax(max_[card_index]);
    if (max_[card_index] == cards_[card_index]->Min()) {
      CardMax(card_index);
    }
  }
  void SetRevDoContribute(int64 var_index, int64 card_index) {
    Solver* const s = solver();
    undecided_.SetToZero(s, var_index, card_index);
    min_.Incr(s, card_index);
    cards_[card_index]->SetMin(min_[card_index]);
    if (min_[card_index] == cards_[card_index]->Max()) {
      CardMin(card_index);
    }
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kDistribute, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kCardsArgument,
                                               cards_);
    visitor->EndVisitConstraint(ModelVisitor::kDistribute, this);
  }

 private:
  int64 var_size() const { return vars_.size(); }
  int64 card_size() const { return cards_.size(); }

  const std::vector<IntVar*> vars_;
  const std::vector<IntVar*> cards_;
  RevBitMatrix undecided_;
  NumericalRevArray<int> min_;
  NumericalRevArray<int> max_;
  std::vector<IntVarIterator*> holes_;
};

FastDistribute::FastDistribute(Solver* const s,
                               const std::vector<IntVar*>& vars,
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
  return StringPrintf("FastDistribute(vars = [%s], cards = [%s])",
                      JoinDebugStringPtr(vars_, ", ").c_str(),
                      JoinDebugStringPtr(cards_, ", ").c_str());
}

void FastDistribute::Post() {
  for (int var_index = 0; var_index < var_size(); ++var_index) {
    IntVar* const var = vars_[var_index];
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
  Solver* const s = solver();
  for (int card_index = 0; card_index < card_size(); ++card_index) {
    int min = 0;
    int max = 0;
    for (int var_index = 0; var_index < var_size(); ++var_index) {
      IntVar* const var = vars_[var_index];
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
  IntVar* const var = vars_[index];
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
  IntVar* const var = vars_[index];
  const int64 oldmin = var->OldMin();
  const int64 oldmax = var->OldMax();
  const int64 vmin = var->Min();
  const int64 vmax = var->Max();
  for (int64 card_index = std::max(oldmin, 0LL);
       card_index < std::min(vmin, card_size()); ++card_index) {
    if (undecided_.IsSet(index, card_index)) {
      SetRevCannotContribute(index, card_index);
    }
  }
  for (const int64 card_index : InitAndGetValues(holes_[index])) {
    if (card_index >= 0 && card_index < card_size() &&
        undecided_.IsSet(index, card_index)) {
      SetRevCannotContribute(index, card_index);
    }
  }
  for (int64 card_index = std::max(vmax + 1, 0LL);
       card_index <= std::min(oldmax, card_size() - 1); ++card_index) {
    if (undecided_.IsSet(index, card_index)) {
      SetRevCannotContribute(index, card_index);
    }
  }
}

void FastDistribute::CountVar(int card_index) {
  const int64 stored_min = min_[card_index];
  const int64 stored_max = max_[card_index];
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

// ----- BoundedDistribute -----

class BoundedDistribute : public Constraint {
 public:
  BoundedDistribute(Solver* const s, const std::vector<IntVar*>& vars,
                    const std::vector<int64>& values,
                    const std::vector<int64>& card_min,
                    const std::vector<int64>& card_max);
  ~BoundedDistribute() override {}

  void Post() override;
  void InitialPropagate() override;
  void OneBound(int vindex);
  void OneDomain(int vindex);
  void CountVar(int card_index);
  void CardMin(int card_index);
  void CardMax(int card_index);
  std::string DebugString() const override;
  void SetRevCannotContribute(int64 var_index, int64 card_index) {
    Solver* const s = solver();
    undecided_.SetToZero(s, var_index, card_index);
    max_.Decr(s, card_index);
    if (max_[card_index] < card_min_[card_index]) {
      solver()->Fail();
    }
    if (max_[card_index] == card_min_[card_index]) {
      CardMax(card_index);
    }
  }
  void SetRevDoContribute(int64 var_index, int64 card_index) {
    Solver* const s = solver();
    undecided_.SetToZero(s, var_index, card_index);
    min_.Incr(s, card_index);
    if (min_[card_index] > card_max_[card_index]) {
      solver()->Fail();
    }
    if (min_[card_index] == card_max_[card_index]) {
      CardMin(card_index);
    }
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kDistribute, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument, values_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kMinArgument, card_min_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kMaxArgument, card_max_);
    visitor->EndVisitConstraint(ModelVisitor::kDistribute, this);
  }

 private:
  int64 var_size() const { return vars_.size(); }
  int64 card_size() const { return values_.size(); }

  const std::vector<IntVar*> vars_;
  const std::vector<int64> values_;
  const std::vector<int64> card_min_;
  const std::vector<int64> card_max_;
  RevBitMatrix undecided_;
  NumericalRevArray<int> min_;
  NumericalRevArray<int> max_;
  std::vector<IntVarIterator*> holes_;
};

BoundedDistribute::BoundedDistribute(Solver* const s,
                                     const std::vector<IntVar*>& vars,
                                     const std::vector<int64>& values,
                                     const std::vector<int64>& card_min,
                                     const std::vector<int64>& card_max)
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
  return StringPrintf(
      "BoundedDistribute([%s], values = [%s], card_min = [%s], card_max = "
      "[%s]",
      JoinDebugStringPtr(vars_, ", ").c_str(),
      strings::Join(values_, ", ").c_str(),
      strings::Join(card_min_, ", ").c_str(),
      strings::Join(card_max_, ", ").c_str());
}

void BoundedDistribute::Post() {
  for (int var_index = 0; var_index < var_size(); ++var_index) {
    IntVar* const var = vars_[var_index];
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
  Solver* const s = solver();

  int64 sum_card_min = 0;
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
    const int64 value = values_[card_index];
    int min = 0;
    int max = 0;
    for (int i = 0; i < var_size(); ++i) {
      IntVar* const var = vars_[i];
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
  IntVar* const var = vars_[index];
  const int64 var_min = var->Min();
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
  IntVar* const var = vars_[index];
  for (int card_index = 0; card_index < card_size(); ++card_index) {
    if (undecided_.IsSet(index, card_index)) {
      if (!var->Contains(values_[card_index])) {
        SetRevCannotContribute(index, card_index);
      }
    }
  }
}

void BoundedDistribute::CountVar(int card_index) {
  const int64 stored_min = min_[card_index];
  const int64 stored_max = max_[card_index];
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

// ----- BoundedFastDistribute -----

class BoundedFastDistribute : public Constraint {
 public:
  BoundedFastDistribute(Solver* const s, const std::vector<IntVar*>& vars,
                        const std::vector<int64>& card_min,
                        const std::vector<int64>& card_max);
  ~BoundedFastDistribute() override {}

  void Post() override;
  void InitialPropagate() override;
  void OneBound(int vindex);
  void OneDomain(int vindex);
  void CountVar(int card_index);
  void CardMin(int card_index);
  void CardMax(int card_index);
  std::string DebugString() const override;
  void SetRevCannotContribute(int64 var_index, int64 card_index) {
    Solver* const s = solver();
    undecided_.SetToZero(s, var_index, card_index);
    max_.Decr(s, card_index);
    if (max_[card_index] < card_min_[card_index]) {
      solver()->Fail();
    }
    if (max_[card_index] == card_min_[card_index]) {
      CardMax(card_index);
    }
  }
  void SetRevDoContribute(int64 var_index, int64 card_index) {
    Solver* const s = solver();
    undecided_.SetToZero(s, var_index, card_index);
    min_.Incr(s, card_index);
    if (min_[card_index] > card_max_[card_index]) {
      solver()->Fail();
    }
    if (min_[card_index] == card_max_[card_index]) {
      CardMin(card_index);
    }
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kDistribute, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kMinArgument, card_min_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kMaxArgument, card_max_);
    visitor->EndVisitConstraint(ModelVisitor::kDistribute, this);
  }

 private:
  int64 var_size() const { return vars_.size(); }
  int64 card_size() const { return card_min_.size(); }

  const std::vector<IntVar*> vars_;
  const std::vector<int64> card_min_;
  const std::vector<int64> card_max_;
  RevBitMatrix undecided_;
  NumericalRevArray<int> min_;
  NumericalRevArray<int> max_;
  std::vector<IntVarIterator*> holes_;
};

BoundedFastDistribute::BoundedFastDistribute(Solver* const s,
                                             const std::vector<IntVar*>& vars,
                                             const std::vector<int64>& card_min,
                                             const std::vector<int64>& card_max)
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
  return StringPrintf(
      "BoundedFastDistribute([%s], card_min = [%s], card_max = [%s]",
      JoinDebugStringPtr(vars_, ", ").c_str(),
      strings::Join(card_min_, ", ").c_str(),
      strings::Join(card_max_, ", ").c_str());
}

void BoundedFastDistribute::Post() {
  for (int var_index = 0; var_index < var_size(); ++var_index) {
    IntVar* const var = vars_[var_index];
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
  Solver* const s = solver();

  int64 sum_card_min = 0;
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
      IntVar* const var = vars_[i];
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
  IntVar* const var = vars_[index];
  const int64 var_min = var->Min();
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
  IntVar* const var = vars_[index];
  const int64 oldmin = var->OldMin();
  const int64 oldmax = var->OldMax();
  const int64 vmin = var->Min();
  const int64 vmax = var->Max();
  for (int64 card_index = std::max(oldmin, 0LL);
       card_index < std::min(vmin, card_size()); ++card_index) {
    if (undecided_.IsSet(index, card_index)) {
      SetRevCannotContribute(index, card_index);
    }
  }
  for (const int64 card_index : InitAndGetValues(holes_[index])) {
    if (card_index >= 0 && card_index < card_size() &&
        undecided_.IsSet(index, card_index)) {
      SetRevCannotContribute(index, card_index);
    }
  }
  for (int64 card_index = std::max(vmax + 1, 0LL);
       card_index <= std::min(oldmax, card_size() - 1); ++card_index) {
    if (undecided_.IsSet(index, card_index)) {
      SetRevCannotContribute(index, card_index);
    }
  }
}

void BoundedFastDistribute::CountVar(int card_index) {
  const int64 stored_min = min_[card_index];
  const int64 stored_max = max_[card_index];
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

// ----- SetAllToZero -----

class SetAllToZero : public Constraint {
 public:
  SetAllToZero(Solver* const s, const std::vector<IntVar*>& vars)
      : Constraint(s), vars_(vars) {}

  ~SetAllToZero() override {}

  void Post() override {}

  void InitialPropagate() override {
    for (int i = 0; i < vars_.size(); ++i) {
      vars_[i]->SetValue(0);
    }
  }

  std::string DebugString() const override { return "SetAllToZero()"; }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kDistribute, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kCardsArgument,
                                               vars_);
    visitor->EndVisitConstraint(ModelVisitor::kDistribute, this);
  }

 private:
  const std::vector<IntVar*> vars_;
};
}  // namespace

// ----- Factory -----

Constraint* Solver::MakeAtMost(std::vector<IntVar*> vars, int64 value,
                               int64 max_count) {
  CHECK_GE(max_count, 0);
  if (max_count >= vars.size()) {
    return MakeTrueConstraint();
  }
  return RevAlloc(new AtMost(this, std::move(vars), value, max_count));
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<int64>& values,
                                   const std::vector<IntVar*>& cards) {
  if (vars.empty()) {
    return RevAlloc(new SetAllToZero(this, cards));
  }
  CHECK_EQ(values.size(), cards.size());
  for (IntVar* const var : vars) {
    CHECK_EQ(this, var->solver());
  }

  // TODO(user) : we can sort values (and cards) before doing the test.
  bool fast = true;
  for (int i = 0; i < values.size(); ++i) {
    if (values[i] != i) {
      fast = false;
      break;
    }
  }
  for (IntVar* const card : cards) {
    CHECK_EQ(this, card->solver());
  }
  if (fast) {
    return RevAlloc(new FastDistribute(this, vars, cards));
  } else {
    return RevAlloc(new Distribute(this, vars, values, cards));
  }
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<int>& values,
                                   const std::vector<IntVar*>& cards) {
  return MakeDistribute(vars, ToInt64Vector(values), cards);
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<IntVar*>& cards) {
  if (vars.empty()) {
    return RevAlloc(new SetAllToZero(this, cards));
  }
  for (IntVar* const var : vars) {
    CHECK_EQ(this, var->solver());
  }
  for (IntVar* const card : cards) {
    CHECK_EQ(this, card->solver());
  }
  return RevAlloc(new FastDistribute(this, vars, cards));
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   int64 card_min, int64 card_max,
                                   int64 card_size) {
  const int vsize = vars.size();
  CHECK_NE(vsize, 0);
  for (IntVar* const var : vars) {
    CHECK_EQ(this, var->solver());
  }
  if (card_min == 0 && card_max >= vsize) {
    return MakeTrueConstraint();
  } else if (card_min > vsize || card_max < 0 || card_max < card_min) {
    return MakeFalseConstraint();
  } else {
    std::vector<int64> mins(card_size, card_min);
    std::vector<int64> maxes(card_size, card_max);
    return RevAlloc(new BoundedFastDistribute(this, vars, mins, maxes));
  }
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<int64>& card_min,
                                   const std::vector<int64>& card_max) {
  const int vsize = vars.size();
  CHECK_NE(vsize, 0);
  int64 cmax = kint64max;
  int64 cmin = kint64min;
  for (int i = 0; i < card_max.size(); ++i) {
    cmax = std::min(cmax, card_max[i]);
    cmin = std::max(cmin, card_min[i]);
  }
  if (cmax < 0 || cmin > vsize) {
    return MakeFalseConstraint();
  } else if (cmax >= vsize && cmin == 0) {
    return MakeTrueConstraint();
  } else {
    return RevAlloc(new BoundedFastDistribute(this, vars, card_min, card_max));
  }
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<int>& card_min,
                                   const std::vector<int>& card_max) {
  return MakeDistribute(vars, ToInt64Vector(card_min), ToInt64Vector(card_max));
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<int64>& values,
                                   const std::vector<int64>& card_min,
                                   const std::vector<int64>& card_max) {
  CHECK_NE(vars.size(), 0);
  CHECK_EQ(card_min.size(), values.size());
  CHECK_EQ(card_min.size(), card_max.size());
  if (AreAllOnes(card_min) && AreAllOnes(card_max) &&
      values.size() == vars.size() && IsIncreasingContiguous(values) &&
      IsArrayInRange(vars, values.front(), values.back())) {
    return MakeAllDifferent(vars);
  } else {
    return RevAlloc(
        new BoundedDistribute(this, vars, values, card_min, card_max));
  }
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<int>& values,
                                   const std::vector<int>& card_min,
                                   const std::vector<int>& card_max) {
  return MakeDistribute(vars, ToInt64Vector(values), ToInt64Vector(card_min),
                        ToInt64Vector(card_max));
}
}  // namespace operations_research

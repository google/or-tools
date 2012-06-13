// Copyright 2010-2012 Google
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

#include <string.h>
#include <algorithm>
#include <string>
#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/concise_iterator.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/string_array.h"

namespace operations_research {

//-----------------------------------------------------------------------------
// CountValueEqCst

// |{i | var[i] == value}| == constant
namespace {
class CountValueEqCst : public Constraint {
 public:
  CountValueEqCst(Solver* const s,
                  const IntVar* const* vars,
                  int size,
                  int64 value,
                  int64 count);
  virtual ~CountValueEqCst() {}

  virtual void Post();
  virtual void InitialPropagate();
  void OneBound(int index);
  void OneDomain(int index);
  void CardMin();
  void CardMax();
  virtual string DebugString() const;

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kCountEqual, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_.get(),
                                               size_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
    visitor->VisitIntegerArgument(ModelVisitor::kCountArgument, count_);
    visitor->EndVisitConstraint(ModelVisitor::kCountEqual, this);
  }

 private:
  scoped_array<IntVar*> vars_;
  int size_;
  int64 value_;
  RevBitSet undecided_;
  int64 count_;
  NumericalRev<int> min_;
  NumericalRev<int> max_;
};

CountValueEqCst::CountValueEqCst(Solver* const s,
                                 const IntVar* const* vars,
                                 int size,
                                 int64 value,
                                 int64 count)
    : Constraint(s), vars_(new IntVar*[size]), size_(size),
      value_(value), undecided_(size), count_(count),
      min_(0), max_(0) {
  memcpy(vars_.get(), vars, size * sizeof(*vars));
}

string CountValueEqCst::DebugString() const {
  return StringPrintf("CountValueEqCst([%s], value=%" GG_LL_FORMAT
                      "d, count=%" GG_LL_FORMAT "d)",
                      DebugStringArray(vars_.get(), size_, ", ").c_str(),
                      value_,
                      count_);
}

void CountValueEqCst::Post() {
  for (int i = 0; i < size_; ++i) {
    IntVar* const var = vars_[i];
    if (!var->Bound()) {
      Demon* d = MakeConstraintDemon1(solver(),
                                      this,
                                      &CountValueEqCst::OneBound,
                                      "OneBound",
                                      i);
      var->WhenBound(d);
      if (var->Contains(value_)) {
        d = MakeConstraintDemon1(solver(),
                                 this,
                                 &CountValueEqCst::OneDomain,
                                 "OneDomain",
                                 i);
        var->WhenDomain(d);
      }
    }
  }
}

void CountValueEqCst::InitialPropagate() {
  int min = 0;
  int max = 0;
  int i;
  Solver* const s = solver();
  for (i = 0; i < size_; ++i) {
    IntVar* const var = vars_[i];
    if (var->Bound()) {
      if (var->Min() == value_) {
        min++;
        max++;
      }
    } else {
      if (var->Contains(value_)) {
        max++;
        undecided_.SetToOne(s, i);
      }
    }
  }
  if (count_ < min || count_ > max) {
    s->Fail();
  }
  if (count_ == min) {
    CardMin();
  } else if (count_ == max) {
    CardMax();
  }
  min_.SetValue(s, min);
  max_.SetValue(s, max);
}

void CountValueEqCst::OneBound(int index) {
  IntVar* const var = vars_[index];
  Solver* const s = solver();
  if (undecided_.IsSet(index)) {
    undecided_.SetToZero(s, index);
    if (var->Min() == value_) {
      min_.Incr(s);
      if (min_.Value() == count_) {
        CardMin();
      }
    } else {
      max_.Decr(s);
      if (max_.Value() == count_) {
        CardMax();
      }
    }
  }
}

void CountValueEqCst::OneDomain(int index) {
  Solver* const s = solver();
  if (undecided_.IsSet(index) && !vars_[index]->Contains(value_)) {
    max_.Decr(s);
    undecided_.SetToZero(s, index);
    if (max_.Value() == count_) {
      CardMax();
    }
  }
}

void CountValueEqCst::CardMin() {
  int i;
  for (i = 0; i < size_; ++i) {
    if (undecided_.IsSet(i)) {
      vars_[i]->RemoveValue(value_);
    }
  }
}

void CountValueEqCst::CardMax() {
  int i;
  for (i = 0; i < size_; ++i) {
    if (undecided_.IsSet(i)) {
      vars_[i]->SetValue(value_);
    }
  }
}

}  // namespace
Constraint* Solver::MakeCount(const std::vector<IntVar*>& vars, int64 v, int64 c) {
  for (ConstIter<std::vector<IntVar*> > it(vars); !it.at_end(); ++it) {
    CHECK_EQ(this, (*it)->solver());
  }
  return RevAlloc(new CountValueEqCst(this, vars.data(), vars.size(), v, c));
}

//-----------------------------------------------------------------------------
// CountValueEq

// |{i | var[i] == value}| == count
namespace {
class CountValueEq : public Constraint {
 public:
  CountValueEq(Solver* const s,
               const IntVar* const* vars,
               int size,
               int64 value,
               IntVar* count);
  virtual ~CountValueEq() {}

  virtual void Post();
  virtual void InitialPropagate();
  void OneBound(int index);
  void OneDomain(int index);
  void CountVar();
  void CardMin();
  void CardMax();
  virtual string DebugString() const;

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kCountEqual, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_.get(),
                                               size_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kCountArgument,
                                            count_);
    visitor->EndVisitConstraint(ModelVisitor::kCountEqual, this);
  }

 private:
  scoped_array<IntVar*> vars_;
  int size_;
  int64 value_;
  RevBitSet undecided_;
  IntVar* const count_;
  NumericalRev<int> min_;
  NumericalRev<int> max_;
};

CountValueEq::CountValueEq(Solver* const s, const IntVar* const* vars, int size,
                           int64 value, IntVar* count)
    : Constraint(s), vars_(new IntVar*[size]), size_(size),
      value_(value), undecided_(size), count_(count), min_(kint32min),
      max_(kint32max) {
  memcpy(vars_.get(), vars, size * sizeof(*vars));
}

string CountValueEq::DebugString() const {
  return StringPrintf("CountValueEq([%s], value = %" GG_LL_FORMAT
                      "d, count = %s)",
                      DebugStringArray(vars_.get(), size_, ", ").c_str(),
                      value_,
                      count_->DebugString().c_str());
}

void CountValueEq::Post() {
  for (int i = 0; i < size_; ++i) {
    IntVar* const var = vars_[i];
    if (!var->Bound()) {
      Demon* d = MakeConstraintDemon1(solver(),
                                          this,
                                          &CountValueEq::OneBound,
                                          "OneBound",
                                          i);
      var->WhenBound(d);
      if (var->Contains(value_)) {
        d = MakeConstraintDemon1(solver(),
                                 this,
                                 &CountValueEq::OneDomain,
                                 "OneDomain",
                                 i);
        var->WhenDomain(d);
      }
    }
  }
  if (!count_->Bound()) {
    Demon* d = MakeConstraintDemon0(solver(),
                                        this,
                                        &CountValueEq::CountVar,
                                        "Var");
    count_->WhenRange(d);
  }
}

void CountValueEq::InitialPropagate() {
  int min = 0;
  int max = 0;
  int i;
  Solver* const s = solver();
  for (i = 0; i < size_; ++i) {
    IntVar* const var = vars_[i];
    if (var->Bound()) {
      if (var->Min() == value_) {
        min++;
        max++;
      }
    } else {
      if (var->Contains(value_)) {
        max++;
        undecided_.SetToOne(s, i);
      }
    }
  }
  count_->SetRange(min, max);
  if (count_->Max() == min) {
    CardMin();
  } else if (count_->Min() == max) {
    CardMax();
  }
  min_.SetValue(s, min);
  max_.SetValue(s, max);
}

void CountValueEq::OneBound(int index) {
  IntVar* const var = vars_[index];
  Solver* const s = solver();
  if (undecided_.IsSet(index)) {
    undecided_.SetToZero(s, index);
    if (var->Min() == value_) {
      min_.Incr(s);
      count_->SetMin(min_.Value());
      if (min_.Value() == count_->Max()) {
        CardMin();
      }
    } else {
      max_.Decr(s);
      count_->SetMax(max_.Value());
      if (max_.Value() == count_->Min()) {
        CardMax();
      }
    }
  }
}

void CountValueEq::OneDomain(int index) {
  Solver* const s = solver();
  if (undecided_.IsSet(index) && !vars_[index]->Contains(value_)) {
    max_.Decr(s);
    undecided_.SetToZero(s, index);
    count_->SetMax(max_.Value());
    if (max_.Value() == count_->Min()) {
      CardMax();
    }
  }
}

void CountValueEq::CountVar() {
  if (count_->Min() > max_.Value()) {
    solver()->Fail();
  }
  if (count_->Min() == max_.Value()) {
    CardMax();
  }
  if (count_->Max() < min_.Value()) {
    solver()->Fail();
  }
  if (count_->Max() == min_.Value()) {
    CardMin();
  }
}

void CountValueEq::CardMin() {
  int i;
  for (i = 0; i < size_; ++i) {
    if (undecided_.IsSet(i)) {
      vars_[i]->RemoveValue(value_);
    }
  }
}

void CountValueEq::CardMax() {
  int i;
  for (i = 0; i < size_; ++i) {
    if (undecided_.IsSet(i)) {
      vars_[i]->SetValue(value_);
    }
  }
}

}  // namespace
Constraint* Solver::MakeCount(const std::vector<IntVar*>& vars, int64 v, IntVar* c) {
  for (ConstIter<std::vector<IntVar*> > it(vars); !it.at_end(); ++it) {
    CHECK_EQ(this, (*it)->solver());
  }
  CHECK_EQ(this, c->solver());
  return RevAlloc(new CountValueEq(this, vars.data(), vars.size(), v, c));
}

// ---------- Distribute ----------

namespace {
class Distribute : public Constraint {
 public:
  Distribute(Solver* const s,
             const IntVar* const * vars,
             int vsize,
             const int64* values,
             const IntVar* const * cards,
             int csize);
  Distribute(Solver* const s,
             const IntVar* const * vars,
             int vsize,
             const int* values,
             const IntVar* const * cards,
             int csize);
  virtual ~Distribute() {}

  virtual void Post();
  virtual void InitialPropagate();
  void OneBound(int vindex);
  void OneDomain(int vindex);
  void CountVar(int cindex);
  void CardMin(int cindex);
  void CardMax(int cindex);
  virtual string DebugString() const;

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kDistribute, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_.get(),
                                               var_size_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument,
                                       values_.get(),
                                       card_size_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kCardsArgument,
                                               cards_.get(),
                                               card_size_);
    visitor->EndVisitConstraint(ModelVisitor::kDistribute, this);
  }

 private:
  const scoped_array<IntVar*> vars_;
  const int var_size_;
  const scoped_array<int64> values_;
  const scoped_array<IntVar*> cards_;
  const int card_size_;
  RevBitMatrix undecided_;
  NumericalRevArray<int> min_;
  NumericalRevArray<int> max_;
};

Distribute::Distribute(Solver* const s,
                       const IntVar* const * vars,
                       int vsize,
                       const int64* values,
                       const IntVar* const * cards,
                       int csize)
    : Constraint(s),
      vars_(new IntVar*[vsize]),
      var_size_(vsize),
      values_(new int64[csize]),
      cards_(new IntVar*[csize]),
      card_size_(csize),
      undecided_(var_size_, card_size_),
      min_(card_size_, 0),
      max_(card_size_, 0) {
  memcpy(vars_.get(), vars, var_size_ * sizeof(*vars));
  memcpy(values_.get(), values, card_size_ * sizeof(*values));
  memcpy(cards_.get(), cards, card_size_ * sizeof(*cards));
}

Distribute::Distribute(Solver* const s,
                       const IntVar* const * vars,
                       int vsize,
                       const int* values,
                       const IntVar* const * cards,
                       int csize)
    : Constraint(s),
      vars_(new IntVar*[vsize]),
      var_size_(vsize),
      values_(new int64[csize]),
      cards_(new IntVar*[csize]),
      card_size_(csize),
      undecided_(var_size_, card_size_),
      min_(card_size_, 0),
      max_(card_size_, 0) {
  memcpy(vars_.get(), vars, var_size_ * sizeof(*vars));
  memcpy(cards_.get(), cards, card_size_ * sizeof(*cards));
  for (int i = 0; i < card_size_; ++i) {
    values_[i] = values[i];
  }
}

string Distribute::DebugString() const {
  return StringPrintf(
      "Distribute(vars = [%s], values = [%s], cards = [%s])",
      DebugStringArray(vars_.get(), var_size_, ", ").c_str(),
      Int64ArrayToString(values_.get(), card_size_, ", ").c_str(),
      DebugStringArray(cards_.get(), card_size_, ", ").c_str());
}

void Distribute::Post() {
  for (int i = 0; i < var_size_; ++i) {
    IntVar* const var = vars_[i];
    if (!var->Bound()) {
      Demon* d = MakeConstraintDemon1(solver(),
                                      this,
                                      &Distribute::OneBound,
                                      "OneBound",
                                      i);
      var->WhenBound(d);
      d = MakeConstraintDemon1(solver(),
                               this,
                               &Distribute::OneDomain,
                               "OneDomain",
                               i);
      var->WhenDomain(d);
    }
  }
  for (int i = 0; i < card_size_; ++i) {
    if (!cards_[i]->Bound()) {
      Demon* d = MakeConstraintDemon1(solver(),
                                      this,
                                      &Distribute::CountVar,
                                      "Var",
                                      i);
      cards_[i]->WhenRange(d);
    }
  }
}

void Distribute::InitialPropagate() {
  Solver* const s = solver();
  for (int j = 0; j < card_size_; ++j) {
    int min = 0;
    int max = 0;
    for (int i = 0; i < var_size_; ++i) {
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
  for (int j = 0; j < card_size_; ++j) {
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
  for (int j = 0; j < card_size_; ++j) {
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
  for (int i = 0; i < var_size_; ++i) {
    if (undecided_.IsSet(i, cindex)) {
      vars_[i]->RemoveValue(values_[cindex]);
    }
  }
}

void Distribute::CardMax(int cindex) {
  for (int i = 0; i < var_size_; ++i) {
    if (undecided_.IsSet(i, cindex)) {
      vars_[i]->SetValue(values_[cindex]);
    }
  }
}

// ----- FastDistribute -----

class FastDistribute : public Constraint {
 public:
  FastDistribute(Solver* const s,
                 const IntVar* const * vars,
                 int vsize,
                 const IntVar* const * cards,
                 int csize);
  virtual ~FastDistribute() {}

  virtual void Post();
  virtual void InitialPropagate();
  void OneBound(int vindex);
  void OneDomain(int vindex);
  void CountVar(int card_index);
  void CardMin(int card_index);
  void CardMax(int card_index);
  virtual string DebugString() const;
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

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kDistribute, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_.get(),
                                               var_size_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kCardsArgument,
                                               cards_.get(),
                                               card_size_);
    visitor->EndVisitConstraint(ModelVisitor::kDistribute, this);
  }

 private:
  const scoped_array<IntVar*> vars_;
  const int var_size_;
  const scoped_array<IntVar*> cards_;
  const int64 card_size_;
  RevBitMatrix undecided_;
  NumericalRevArray<int> min_;
  NumericalRevArray<int> max_;
  scoped_array<IntVarIterator*> holes_;
};

FastDistribute::FastDistribute(Solver* const s,
                               const IntVar* const * vars,
                               int vsize,
                               const IntVar* const * cards,
                               int csize)
    : Constraint(s),
      vars_(new IntVar*[vsize]),
      var_size_(vsize),
      cards_(new IntVar*[csize]),
      card_size_(csize),
      undecided_(vsize, csize),
      min_(card_size_, 0),
      max_(card_size_, 0),
      holes_(new IntVarIterator*[var_size_]) {
  memcpy(vars_.get(), vars, var_size_ * sizeof(*vars));
  memcpy(cards_.get(), cards, card_size_ * sizeof(*cards));
  for (int var_index = 0; var_index < var_size_; ++var_index) {
    holes_[var_index] = vars_[var_index]->MakeHoleIterator(true);
  }
}


string FastDistribute::DebugString() const {
  return StringPrintf("FastDistribute(vars = [%s], cards = [%s])",
                      DebugStringArray(vars_.get(), var_size_, ", ").c_str(),
                      DebugStringArray(cards_.get(), card_size_, ", ").c_str());
}

void FastDistribute::Post() {
  for (int var_index = 0; var_index < var_size_; ++var_index) {
    IntVar* const var = vars_[var_index];
    if (!var->Bound()) {
      Demon* d = MakeConstraintDemon1(solver(),
                                      this,
                                      &FastDistribute::OneBound,
                                      "OneBound",
                                      var_index);
      var->WhenBound(d);
      d = MakeConstraintDemon1(solver(),
                               this,
                               &FastDistribute::OneDomain,
                               "OneDomain",
                               var_index);
      var->WhenDomain(d);
    }
  }
  for (int card_index = 0; card_index < card_size_; ++card_index) {
    if (!cards_[card_index]->Bound()) {
      Demon* d = MakeConstraintDemon1(solver(),
                                      this,
                                      &FastDistribute::CountVar,
                                      "Var",
                                      card_index);
      cards_[card_index]->WhenRange(d);
    }
  }
}

void FastDistribute::InitialPropagate() {
  Solver* const s = solver();
  for (int card_index = 0; card_index < card_size_; ++card_index) {
    int min = 0;
    int max = 0;
    for (int var_index = 0; var_index < var_size_; ++var_index) {
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
  for (int card_index = 0; card_index < card_size_; ++card_index) {
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
       card_index < std::min(vmin, card_size_);
       ++card_index) {
    if (undecided_.IsSet(index, card_index)) {
      SetRevCannotContribute(index, card_index);
    }
  }
  IntVarIterator* const holes = holes_[index];
  for (holes->Init(); holes->Ok(); holes->Next()) {
    const int64 card_index = holes->Value();
    if (card_index >= 0 &&
        card_index < card_size_ &&
        undecided_.IsSet(index, card_index)) {
      SetRevCannotContribute(index, card_index);
    }
  }
  for (int64 card_index = std::max(vmax + 1, 0LL);
       card_index <= std::min(oldmax, card_size_ - 1);
       ++card_index) {
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
  for (int var_index = 0; var_index < var_size_; ++var_index) {
    if (undecided_.IsSet(var_index, card_index)) {
      vars_[var_index]->RemoveValue(card_index);
    }
  }
}

void FastDistribute::CardMax(int card_index) {
  for (int var_index = 0; var_index < var_size_; ++var_index) {
    if (undecided_.IsSet(var_index, card_index)) {
      vars_[var_index]->SetValue(card_index);
    }
  }
}

// ----- BoundedDistribute -----

class BoundedDistribute : public Constraint {
 public:
  BoundedDistribute(Solver* const s,
                    const IntVar* const * vars,
                    int vsize,
                    const std::vector<int64>& card_min,
                    const std::vector<int64>& card_max);
  BoundedDistribute(Solver* const s,
                    const IntVar* const * vars,
                    int vsize,
                    const std::vector<int>& card_min,
                    const std::vector<int>& card_max);
  virtual ~BoundedDistribute() {}

  virtual void Post();
  virtual void InitialPropagate();
  void OneBound(int vindex);
  void OneDomain(int vindex);
  void CountVar(int card_index);
  void CardMin(int card_index);
  void CardMax(int card_index);
  virtual string DebugString() const;
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

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kDistribute, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_.get(),
                                               var_size_);
    visitor->VisitConstIntArrayArgument(ModelVisitor::kMinArgument, card_min_);
    visitor->VisitConstIntArrayArgument(ModelVisitor::kMaxArgument, card_max_);
    visitor->EndVisitConstraint(ModelVisitor::kDistribute, this);
  }

 private:
  const scoped_array<IntVar*> vars_;
  const int var_size_;
  ConstIntArray card_min_;
  ConstIntArray card_max_;
  const int64 card_size_;
  RevBitMatrix undecided_;
  NumericalRevArray<int> min_;
  NumericalRevArray<int> max_;
  scoped_array<IntVarIterator*> holes_;
};

BoundedDistribute::BoundedDistribute(Solver* const s,
                                     const IntVar* const * vars,
                                     int vsize,
                                     const std::vector<int64>& card_min,
                                     const std::vector<int64>& card_max)
    : Constraint(s),
      vars_(new IntVar*[vsize]),
      var_size_(vsize),
      card_min_(card_min),
      card_max_(card_max),
      card_size_(card_min.size()),
      undecided_(var_size_, card_size_),
      min_(card_size_, 0),
      max_(card_size_, 0),
      holes_(new IntVarIterator*[var_size_]) {
  memcpy(vars_.get(), vars, var_size_ * sizeof(*vars));
  for (int var_index = 0; var_index < var_size_; ++var_index) {
    holes_[var_index] = vars_[var_index]->MakeHoleIterator(true);
  }
}

BoundedDistribute::BoundedDistribute(Solver* const s,
                                     const IntVar* const * vars,
                                     int vsize,
                                     const std::vector<int>& card_min,
                                     const std::vector<int>& card_max)
    : Constraint(s),
      vars_(new IntVar*[vsize]),
      var_size_(vsize),
      card_min_(card_min),
      card_max_(card_max),
      card_size_(card_min.size()),
      undecided_(var_size_, card_size_),
      min_(card_size_, 0),
      max_(card_size_, 0),
      holes_(new IntVarIterator*[var_size_]) {
  memcpy(vars_.get(), vars, var_size_ * sizeof(*vars));
  for (int var_index = 0; var_index < var_size_; ++var_index) {
    holes_[var_index] = vars_[var_index]->MakeHoleIterator(true);
  }
}

string BoundedDistribute::DebugString() const {
  return StringPrintf(
      "BoundedDistribute([%s], card_min = [%s], card_max = [%s]",
      DebugStringArray(vars_.get(), var_size_, ", ").c_str(),
      card_min_.DebugString().c_str(),
      card_max_.DebugString().c_str());
}

void BoundedDistribute::Post() {
  for (int var_index = 0; var_index < var_size_; ++var_index) {
    IntVar* const var = vars_[var_index];
    if (!var->Bound()) {
      Demon* d = MakeConstraintDemon1(solver(),
                                      this,
                                      &BoundedDistribute::OneBound,
                                      "OneBound",
                                      var_index);
      var->WhenBound(d);
      d = MakeConstraintDemon1(solver(),
                               this,
                               &BoundedDistribute::OneDomain,
                               "OneDomain",
                               var_index);
      var->WhenDomain(d);
    }
  }
}

void BoundedDistribute::InitialPropagate() {
  Solver* const s = solver();

  int64 sum_card_min = 0;
  for (int i = 0; i < card_size_; ++i) {
    if (card_max_[i] < card_min_[i]) {
      solver()->Fail();
    }
    sum_card_min += card_min_[i];
  }
  if (sum_card_min > var_size_) {
    s->Fail();
  }
  if (sum_card_min == var_size_) {
    for (int i = 0; i < var_size_; ++i) {
      vars_[i]->SetRange(0, card_size_ - 1);
    }
  }

  for (int card_index = 0; card_index < card_size_; ++card_index) {
    int min = 0;
    int max = 0;
    for (int i = 0; i < var_size_; ++i) {
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

void BoundedDistribute::OneBound(int index) {
  IntVar* const var = vars_[index];
  const int64 var_min = var->Min();
  for (int card_index = 0; card_index < card_size_; ++card_index) {
    if (undecided_.IsSet(index, card_index)) {
      if (var_min == card_index) {
        SetRevDoContribute(index, card_index);
      } else {
        SetRevCannotContribute(index, card_index);
      }
    }
  }
}

void BoundedDistribute::OneDomain(int index) {
  IntVar* const var = vars_[index];
  const int64 oldmin = var->OldMin();
  const int64 oldmax = var->OldMax();
  const int64 vmin = var->Min();
  const int64 vmax = var->Max();
  for (int64 card_index = std::max(oldmin, 0LL);
       card_index < std::min(vmin, card_size_);
       ++card_index) {
    if (undecided_.IsSet(index, card_index)) {
      SetRevCannotContribute(index, card_index);
    }
  }
  IntVarIterator* const holes = holes_[index];
  for (holes->Init(); holes->Ok(); holes->Next()) {
    const int64 card_index = holes->Value();
    if (card_index >= 0 &&
        card_index < card_size_ &&
        undecided_.IsSet(index, card_index)) {
      SetRevCannotContribute(index, card_index);
    }
  }
  for (int64 card_index = std::max(vmax + 1, 0LL);
       card_index <= std::min(oldmax, card_size_ - 1);
       ++card_index) {
    if (undecided_.IsSet(index, card_index)) {
      SetRevCannotContribute(index, card_index);
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
  for (int var_index = 0; var_index < var_size_; ++var_index) {
    if (undecided_.IsSet(var_index, card_index)) {
      vars_[var_index]->RemoveValue(card_index);
    }
  }
}

void BoundedDistribute::CardMax(int card_index) {
  for (int var_index = 0; var_index < var_size_; ++var_index) {
    if (undecided_.IsSet(var_index, card_index)) {
      vars_[var_index]->SetValue(card_index);
    }
  }
}

// ----- SetAllToZero -----

class SetAllToZero : public Constraint {
 public:
  SetAllToZero(Solver* const s, const IntVar* const * vars, int size)
      : Constraint(s), vars_(new IntVar*[size]), size_(size) {
    DCHECK_GE(size_, 0);
    memcpy(vars_.get(), vars, size * sizeof(*vars));
  }
  virtual ~SetAllToZero() {}

  virtual void Post() {}

  virtual void InitialPropagate() {
    for (int i = 0; i < size_; ++i) {
      vars_[i]->SetValue(0);
    }
  }

  virtual string DebugString() const {
    return "SetAllToZero()";
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kDistribute, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kCardsArgument,
                                               vars_.get(),
                                               size_);
    visitor->EndVisitConstraint(ModelVisitor::kDistribute, this);
  }

 private:
  scoped_array<IntVar*> vars_;
  const int size_;
};

}  // namespace

// ----- Factory -----

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<int64>& values,
                                   const std::vector<IntVar*>& cards) {
  if (vars.size() == 0) {
    return RevAlloc(new SetAllToZero(this, cards.data(), cards.size()));
  }
  CHECK_EQ(values.size(), cards.size());
  for (ConstIter<std::vector<IntVar*> > it(vars); !it.at_end(); ++it) {
    CHECK_EQ(this, (*it)->solver());
  }

  // TODO(user) : we can sort values (and cards) before doing the test.
  bool fast = true;
  for (int i = 0; i < values.size(); ++i) {
    if (values[i] != i) {
      fast = false;
      break;
    }
  }
  for (ConstIter<std::vector<IntVar*> > it(cards); !it.at_end(); ++it) {
    CHECK_EQ(this, (*it)->solver());
  }
  if (fast) {
    return RevAlloc(new FastDistribute(this, vars.data(), vars.size(),
                                       cards.data(), cards.size()));
  } else {
    return RevAlloc(new Distribute(this, vars.data(), vars.size(),
                                   values.data(), cards.data(), cards.size()));
  }
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<int>& values,
                                   const std::vector<IntVar*>& cards) {
  if (vars.size() == 0) {
    return RevAlloc(new SetAllToZero(this, cards.data(), cards.size()));
  }
  CHECK_EQ(values.size(), cards.size());
  for (ConstIter<std::vector<IntVar*> > it(vars); !it.at_end(); ++it) {
    CHECK_EQ(this, (*it)->solver());
  }

  // TODO(user) : we can sort values (and cards) before doing the test.
  bool fast = true;
  for (int i = 0; i < values.size(); ++i) {
    if (values[i] != i) {
      fast = false;
      break;
    }
  }
  for (ConstIter<std::vector<IntVar*> > it(cards); !it.at_end(); ++it) {
    CHECK_EQ(this, (*it)->solver());
  }
  if (fast) {
    return RevAlloc(new FastDistribute(this, vars.data(), vars.size(),
                                       cards.data(), cards.size()));
  } else {
    return RevAlloc(new Distribute(this, vars.data(), vars.size(),
                                   values.data(), cards.data(), cards.size()));
  }
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<IntVar*>& cards) {
  if (vars.size() == 0) {
    return RevAlloc(new SetAllToZero(this, cards.data(), cards.size()));
  }
  for (ConstIter<std::vector<IntVar*> > it(vars); !it.at_end(); ++it) {
    CHECK_EQ(this, (*it)->solver());
  }
  for (ConstIter<std::vector<IntVar*> > it(cards); !it.at_end(); ++it) {
    CHECK_EQ(this, (*it)->solver());
  }
  return RevAlloc(new FastDistribute(this, vars.data(), vars.size(),
                                     cards.data(), cards.size()));
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   int64 card_min,
                                   int64 card_max,
                                   int64 card_size) {
  CHECK_NE(vars.size(), 0);
  std::vector<int64> mins(card_size, card_min);
  std::vector<int64> maxes(card_size, card_max);
  for (ConstIter<std::vector<IntVar*> > it(vars); !it.at_end(); ++it) {
    CHECK_EQ(this, (*it)->solver());
  }
  return RevAlloc(
      new BoundedDistribute(this, vars.data(), vars.size(), mins, maxes));
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<int64>& card_min,
                                   const std::vector<int64>& card_max) {
  CHECK_NE(vars.size(), 0);
  return RevAlloc(new BoundedDistribute(this,
                                        vars.data(),
                                        vars.size(),
                                        card_min,
                                        card_max));
}

Constraint* Solver::MakeDistribute(const std::vector<IntVar*>& vars,
                                   const std::vector<int>& card_min,
                                   const std::vector<int>& card_max) {
  CHECK_NE(vars.size(), 0);
  return RevAlloc(new BoundedDistribute(this,
                                        vars.data(),
                                        vars.size(),
                                        card_min,
                                        card_max));
}
}  // namespace operations_research

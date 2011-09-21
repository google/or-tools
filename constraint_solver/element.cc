// Copyright 2010-2011 Google
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
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/const_int_array.h"
#include "util/string_array.h"

namespace operations_research {

// ----- IntExprElement -----
void LinkVarExpr(Solver* const s, IntExpr* const expr, IntVar* const var);

namespace {
// ----- BaseIntExprElement -----

class BaseIntExprElement : public BaseIntExpr {
 public:
  BaseIntExprElement(Solver* const s, IntVar* const expr);
  virtual ~BaseIntExprElement() {}
  virtual int64 Min() const;
  virtual int64 Max() const;
  virtual void Range(int64* mi, int64* ma);
  virtual void SetMin(int64 m);
  virtual void SetMax(int64 m);
  virtual void SetRange(int64 mi, int64 ma);
  virtual bool Bound() const { return (expr_->Bound()); }
  // TODO(user) : improve me, the previous test is not always true
  virtual void WhenRange(Demon* d) {
    expr_->WhenRange(d);
  }

 protected:
  virtual int64 ElementValue(int index) const = 0;
  virtual int64 ExprMin() const = 0;
  virtual int64 ExprMax() const = 0;

  IntVar* const expr_;

 private:
  void UpdateSupports() const;

  mutable int64 min_;
  mutable int min_support_;
  mutable int64 max_;
  mutable int max_support_;
  mutable bool initial_update_;
  IntVarIterator* const expr_iterator_;
};

BaseIntExprElement::BaseIntExprElement(Solver* const s, IntVar* const e)
  : BaseIntExpr(s),
    expr_(e),
    min_(0),
    min_support_(-1),
    max_(0),
    max_support_(-1),
    initial_update_(true),
    expr_iterator_(expr_->MakeDomainIterator(true)) {
  CHECK_NOTNULL(s);
  CHECK_NOTNULL(e);
}

int64 BaseIntExprElement::Min() const {
  UpdateSupports();
  return min_;
}

int64 BaseIntExprElement::Max() const {
  UpdateSupports();
  return max_;
}

void BaseIntExprElement::Range(int64* mi, int64* ma) {
  UpdateSupports();
  *mi = min_;
  *ma = max_;
}

#define UPDATE_BASE_ELEMENT_INDEX_BOUNDS(test)  \
  const int64 emin = ExprMin();                 \
  const int64 emax = ExprMax();                 \
  int64 nmin = emin;                            \
  int64 value = ElementValue(nmin);             \
  while (nmin < emax && test) {                 \
    nmin++;                                     \
    value = ElementValue(nmin);                 \
  }                                             \
  if (nmin == emax && test) {                   \
    solver()->Fail();                           \
  }                                             \
  int64 nmax = emax;                            \
  value = ElementValue(nmax);                   \
  while (nmax >= nmin && test) {                \
    nmax--;                                     \
    value = ElementValue(nmax);                 \
  }                                             \
  expr_->SetRange(nmin, nmax);

void BaseIntExprElement::SetMin(int64 m) {
  UPDATE_BASE_ELEMENT_INDEX_BOUNDS(value < m);
}

void BaseIntExprElement::SetMax(int64 m) {
  UPDATE_BASE_ELEMENT_INDEX_BOUNDS(value > m);
}

void BaseIntExprElement::SetRange(int64 mi, int64 ma) {
  if (mi > ma) {
    solver()->Fail();
  }
  UPDATE_BASE_ELEMENT_INDEX_BOUNDS((value < mi || value > ma));
}

#undef UPDATE_BASE_ELEMENT_INDEX_BOUNDS

void BaseIntExprElement::UpdateSupports() const {
  if (initial_update_
      || !expr_->Contains(min_support_)
      || !expr_->Contains(max_support_)) {
    const int64 emin = ExprMin();
    const int64 emax = ExprMax();
    int64 min_value = ElementValue(emax);
    int64 max_value = min_value;
    int min_support = emax;
    int max_support = emax;
    for (expr_iterator_->Init(); expr_iterator_->Ok(); expr_iterator_->Next()) {
      const int64 index = expr_iterator_->Value();
      if (index >= emin && index <= emax) {
        const int64 value = ElementValue(index);
        if (value > max_value) {
          max_value = value;
          max_support = index;
        } else if (value < min_value) {
          min_value = value;
          min_support = index;
        }
      }
    }
    Solver* s = solver();
    s->SaveAndSetValue(&min_, min_value);
    s->SaveAndSetValue(&min_support_, min_support);
    s->SaveAndSetValue(&max_, max_value);
    s->SaveAndSetValue(&max_support_, max_support);
    s->SaveAndSetValue(&initial_update_, false);
  }
}

// ----- IntElementConstraint -----

// This constraint implements 'elem' == 'values'['index'].
// It scans the bounds of 'elem' to propagate on the domain of 'index'.
// It scans the domain of 'index' to compute the new bounds of 'elem'.
class IntElementConstraint : public Constraint {
 public:
  IntElementConstraint(Solver* const s,
                       std::vector<int64>* const values,
                       IntVar* const index,
                       IntVar* const elem)
    : Constraint(s),
      values_(values),
      index_(index),
      elem_(elem),
      index_iterator_(index_->MakeDomainIterator(true)) {
    CHECK_NOTNULL(values);
    CHECK_NOTNULL(index);
    CHECK_NOTNULL(elem);
  }

  virtual void Post() {
    Demon* const d =
        solver()->MakeDelayedConstraintInitialPropagateCallback(this);
    index_->WhenDomain(d);
    elem_->WhenRange(d);
  }

  virtual void InitialPropagate() {
    index_->SetRange(0, values_.size() - 1);
    const int64 elem_min = elem_->Min();
    const int64 elem_max = elem_->Max();
    int64 new_min = elem_max;
    int64 new_max = elem_min;
    to_remove_.clear();
    for (index_iterator_->Init();
         index_iterator_->Ok();
         index_iterator_->Next()) {
      const int64 index = index_iterator_->Value();
      const int64 value = values_[index];
      if (value < elem_min || value > elem_max) {
        to_remove_.push_back(index);
      } else {
        if (value < new_min) {
          new_min = value;
        }
        if (value > new_max) {
          new_max = value;
        }
      }
    }
    elem_->SetRange(new_min, new_max);
    if (!to_remove_.empty()) {
      index_->RemoveValues(to_remove_);
    }
  }

  virtual string DebugString() const {
    return StringPrintf("IntElementConstraint(%s, %s, %s)",
                        values_.DebugString().c_str(),
                        index_->DebugString().c_str(),
                        elem_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kElementEqual, this);
    visitor->VisitConstIntArrayArgument(ModelVisitor::kValuesArgument,
                                        values_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            index_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            elem_);
    visitor->EndVisitConstraint(ModelVisitor::kElementEqual, this);
  }

 private:
  ConstIntArray values_;
  IntVar* const index_;
  IntVar* const elem_;
  IntVarIterator* const index_iterator_;
  std::vector<int64> to_remove_;
};

// ----- IntExprElement

IntVar* BuildDomainIntVar(Solver* const solver, std::vector<int64>* values);

class IntExprElement : public BaseIntExprElement {
 public:
  IntExprElement(Solver* const s,
                 std::vector<int64>* const vals,
                 IntVar* const expr);

  virtual ~IntExprElement();

  virtual string name() const {
    return StringPrintf("IntElement(%s, %s)",
                        values_.DebugString().c_str(),
                        expr_->name().c_str());
  }

  virtual string DebugString() const {
    return StringPrintf("IntElement(%s, %s)",
                        values_.DebugString().c_str(),
                        expr_->DebugString().c_str());
  }

  virtual IntVar* CastToVar() {
    Solver* const s = solver();
    std::vector<int64>* copied_data = values_.Copy();
    IntVar* const var = s->MakeIntVar(*copied_data);
    AddDelegateName("Var", var);
    // Ownership or copied_data is transferred to the constraint.
    s->AddDelegateConstraint(s->RevAlloc(new IntElementConstraint(s,
                                                                  copied_data,
                                                                  expr_,
                                                                  var)));
    return var;
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kElement, this);
    visitor->VisitConstIntArrayArgument(ModelVisitor::kValuesArgument,
                                        values_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            expr_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kElement, this);
  }

 protected:
  virtual int64 ElementValue(int index) const {
    DCHECK_LT(index, values_.size());
    return values_[index];
  }
  virtual int64 ExprMin() const {
    return std::max(0LL, expr_->Min());
  }
  virtual int64 ExprMax() const {
    return std::min(values_.size() - 1LL, expr_->Max());
  }

 private:
  ConstIntArray values_;
};

IntExprElement::IntExprElement(Solver* const solver,
                               std::vector<int64>* const values,
                               IntVar* const index)
  : BaseIntExprElement(solver, index), values_(values) {
  CHECK_NOTNULL(values);
}


IntExprElement::~IntExprElement() {}

// ----- Increasing Element -----

class IncreasingIntExprElement : public BaseIntExpr {
 public:
  IncreasingIntExprElement(Solver* const s,
                           std::vector<int64>* const values,
                           IntVar* const index);
  virtual ~IncreasingIntExprElement() {}

  virtual int64 Min() const;
  virtual void SetMin(int64 m);
  virtual int64 Max() const;
  virtual void SetMax(int64 m);
  virtual void SetRange(int64 mi, int64 ma);
  virtual bool Bound() const { return (index_->Bound()); }
  // TODO(user) : improve me, the previous test is not always true
  virtual string name() const {
    return StringPrintf("IntElement(%s, %s)",
                        values_.DebugString().c_str(),
                        index_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("IntElement(%s, %s)",
                        values_.DebugString().c_str(),
                        index_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kElement, this);
    visitor->VisitConstIntArrayArgument(ModelVisitor::kValuesArgument,
                                        values_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            index_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kElement, this);
  }

  virtual void WhenRange(Demon* d) {
    index_->WhenRange(d);
  }

  virtual IntVar* CastToVar() {
    Solver* const s = solver();
    std::vector<int64>* copied_data = values_.Copy();
    IntVar* const var = s->MakeIntVar(*copied_data);
    delete copied_data;
    AddDelegateName("Var", var);
    LinkVarExpr(s, this, var);
    return var;
  }

 private:
  ConstIntArray values_;
  IntVar* const index_;
};

IncreasingIntExprElement::IncreasingIntExprElement(Solver* const s,
                                                   std::vector<int64>* const values,
                                                   IntVar* const index)
  : BaseIntExpr(s), values_(values), index_(index) {
  DCHECK(values);
  DCHECK(index);
  DCHECK(s);
}

int64 IncreasingIntExprElement::Min() const {
  const int64 expression_min = std::max(0LL, index_->Min());
  return  (expression_min < values_.size() ?
           values_[expression_min] :
           kint64max);
}

void IncreasingIntExprElement::SetMin(int64 m) {
  const int64 expression_min = std::max(0LL, index_->Min());
  const int64 expression_max = std::min(values_.size() - 1LL, index_->Max());
  if (expression_min > expression_max || m > values_[expression_max]) {
    solver()->Fail();
  }
  int64 nmin = expression_min;
  while (nmin <= expression_max && values_[nmin] < m) {
    nmin++;
  }
  DCHECK_LE(nmin, expression_max);
  index_->SetMin(nmin);
}

int64 IncreasingIntExprElement::Max() const {
  const int64 expression_max = std::min(values_.size() - 1LL, index_->Max());
  return (expression_max >= 0 ? values_[expression_max] : kint64max);
}

void IncreasingIntExprElement::SetMax(int64 m) {
  const int64 expression_min = std::max(0LL, index_->Min());
  const int64 expression_max = std::min(values_.size() - 1LL, index_->Max());
  if (expression_min > expression_max || m < values_[expression_min]) {
    solver()->Fail();
  }
  int64 nmax = expression_max;
  while (nmax >= expression_min && values_[nmax] > m) {
    nmax--;
  }
  DCHECK_GE(nmax, expression_min);
  index_->SetRange(expression_min, nmax);
}

void IncreasingIntExprElement::SetRange(int64 mi, int64 ma) {
  if (mi > ma) {
    solver()->Fail();
  }
  const int64 expression_min = std::max(0LL, index_->Min());
  const int64 expression_max = std::min(values_.size() - 1LL, index_->Max());
  if (expression_min > expression_max ||
      mi > values_[expression_max] ||
      ma < values_[expression_min]) {
    solver()->Fail();
  }
  int64 nmin = expression_min;
  while (nmin <= expression_max && (values_[nmin] < mi || values_[nmin] > ma)) {
    nmin++;
  }
  DCHECK_LE(nmin, expression_max);
  int64 nmax = expression_max;
  while (nmax >= nmin && (values_[nmax] < mi || values_[nmax] > ma)) {
    nmax--;
  }
  DCHECK_GE(nmax, expression_min);
  index_->SetRange(nmin, nmax);
}

// ----- Solver::MakeElement(int array, int var) -----
IntExpr* BuildElement(Solver* const solver,
                      ConstIntArray* values,
                      IntVar* const index) {
  // Various checks.
  // Is array constant?
  if (values->HasProperty(ConstIntArray::IS_CONSTANT)) {
    solver->AddConstraint(solver->MakeBetweenCt(index, 0, values->size() - 1));
    return solver->MakeIntConst(values->get(0));
  }
  // Is array built with booleans only?
  // TODO(user): We could maintain the index of the first one.
  if (values->HasProperty(ConstIntArray::IS_BOOLEAN)) {
    std::vector<int64> ones;
    int first_zero = -1;
    for (int i = 0; i < values->size(); ++i) {
      if (values->get(i) == 1LL) {
        ones.push_back(i);
      } else {
        first_zero = i;
      }
    }
    if (ones.size() == 1) {
      DCHECK_EQ(1LL, values->get(ones.back()));
      solver->AddConstraint(
          solver->MakeBetweenCt(index, 0, values->size() - 1));
      return solver->MakeIsEqualCstVar(index, ones.back());
    } else if (ones.size() == values->size() - 1) {
      solver->AddConstraint(
          solver->MakeBetweenCt(index, 0, values->size() - 1));
      return solver->MakeIsDifferentCstVar(index, first_zero);
    } else if (ones.size() == ones.back() - ones.front() + 1) {  // contiguous.
      solver->AddConstraint(
          solver->MakeBetweenCt(index, 0, values->size() - 1));
      IntVar* const b = solver->MakeBoolVar("ContiguousBooleanElementVar");
      solver->AddConstraint(
          solver->MakeIsBetweenCt(index, ones.front(), ones.back(), b));
      return b;
    } else {
      IntVar* const b = solver->MakeBoolVar("NonContiguousBooleanElementVar");
      solver->AddConstraint(
          solver->MakeBetweenCt(index, 0, values->size() - 1));
      solver->AddConstraint(solver->MakeIsMemberCt(index, ones, b));
      return b;
    }
  }
  // Is Array increasing
  if (values->HasProperty(ConstIntArray::IS_INCREASING)) {
    return solver->RevAlloc(
        new IncreasingIntExprElement(solver, values->Release(), index));
  }
  return solver->RevAlloc(new IntExprElement(solver, values->Release(), index));
}
}  // namespace

IntExpr* Solver::MakeElement(const int* const vals,
                             int size,
                             IntVar* const index) {
  DCHECK(index);
  DCHECK_EQ(this, index->solver());
  DCHECK_GT(size, 0);
  DCHECK(vals);
  ConstIntArray values(vals, size);
  return BuildElement(this, &values, index);
}

IntExpr* Solver::MakeElement(const std::vector<int64>& vals, IntVar* const index) {
  return MakeElement(vals.data(), vals.size(), index);
}

IntExpr* Solver::MakeElement(const int64* const vals,
                             int size,
                             IntVar* const index) {
  DCHECK(index);
  DCHECK_EQ(this, index->solver());
  DCHECK_GT(size, 0);
  DCHECK(vals);
  ConstIntArray values(vals, size);
  return BuildElement(this, &values, index);
}

IntExpr* Solver::MakeElement(const std::vector<int>& vals, IntVar* const index) {
  return MakeElement(vals.data(), vals.size(), index);
}

// ----- IntExprFunctionElement -----

namespace {
class IntExprFunctionElement : public BaseIntExprElement {
 public:
  IntExprFunctionElement(Solver* const s,
                         ResultCallback1<int64, int64>* values,
                         IntVar* const expr,
                         bool del);
  virtual ~IntExprFunctionElement();

  virtual string name() const {
    return StringPrintf("IntFunctionElement(%s)", expr_->name().c_str());
  }

  virtual string DebugString() const {
    return StringPrintf("IntFunctionElement(%s)", expr_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    // Warning: This will expand all values into a vector.
    visitor->BeginVisitIntegerExpression(ModelVisitor::kElement, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            expr_);
    if (expr_->Min() == 0) {
      visitor->VisitInt64ToInt64AsArray(values_,
                                        ModelVisitor::kValuesArgument,
                                        expr_->Max());
    } else {
      visitor->VisitInt64ToInt64Extension(values_, expr_->Min(), expr_->Max());
    }
    visitor->EndVisitIntegerExpression(ModelVisitor::kElement, this);
  }

 protected:
  virtual int64 ElementValue(int index) const { return values_->Run(index); }
  virtual int64 ExprMin() const { return expr_->Min(); }
  virtual int64 ExprMax() const { return expr_->Max(); }

 private:
  ResultCallback1<int64, int64>* values_;
  const bool delete_;
};

IntExprFunctionElement::IntExprFunctionElement(
    Solver* const s,
    ResultCallback1<int64, int64>* values,
    IntVar* const e,
    bool del)
  : BaseIntExprElement(s, e),
    values_(values),
    delete_(del) {
  CHECK(values) << "null pointer";
  values->CheckIsRepeatable();
}

IntExprFunctionElement::~IntExprFunctionElement() {
  if (delete_) {
    delete values_;
  }
}

// ----- Increasing Element -----

class IncreasingIntExprFunctionElement : public BaseIntExpr {
 public:
  IncreasingIntExprFunctionElement(Solver* const s,
                                  ResultCallback1<int64, int64>* values,
                                  IntVar* const index)
      : BaseIntExpr(s),
        values_(values),
        index_(index) {
    DCHECK(values);
    DCHECK(index);
    DCHECK(s);
    values->CheckIsRepeatable();
  }

  virtual ~IncreasingIntExprFunctionElement() {
    delete values_;
  }

  virtual int64 Min() const {
    return values_->Run(index_->Min());
  }

  virtual void SetMin(int64 m) {
    const int64 expression_min = index_->Min();
    const int64 expression_max = index_->Max();
    if (m > values_->Run(expression_max)) {
      solver()->Fail();
    }
    int64 nmin = expression_min;
    while (nmin <= expression_max && values_->Run(nmin) < m) {
      nmin++;
    }
    DCHECK_LE(nmin, expression_max);
    index_->SetMin(nmin);
  }

  virtual int64 Max() const {
    return values_->Run(index_->Max());
  }

  virtual void SetMax(int64 m) {
    const int64 expression_min = index_->Min();
    const int64 expression_max = index_->Max();
    if (m < values_->Run(expression_min)) {
      solver()->Fail();
    }
    int64 nmax = expression_max;
    while (nmax >= expression_min && values_->Run(nmax) > m) {
      nmax--;
    }
    DCHECK_GE(nmax, expression_min);
    index_->SetMax(nmax);
  }

  virtual void SetRange(int64 mi, int64 ma) {
    const int64 expression_min = index_->Min();
    const int64 expression_max = index_->Max();
    if (mi > ma ||
        ma < values_->Run(expression_min) ||
        mi > values_->Run(expression_max)) {
      solver()->Fail();
    }
    int64 nmax = expression_max;
    while (nmax >= expression_min && values_->Run(nmax) > ma) {
      nmax--;
    }
    DCHECK_GE(nmax, expression_min);
    int64 nmin = expression_min;
    while (nmin <= nmax && values_->Run(nmin) < mi) {
      nmin++;
    }
    DCHECK_LE(nmin, nmax);
    index_->SetRange(nmin, nmax);
  }

  virtual string name() const {
    return StringPrintf("IncreasingIntExprFunctionElement(values, %s)",
                        index_->name().c_str());
  }

  virtual string DebugString() const {
    return StringPrintf("IncreasingIntExprFunctionElement(values, %s)",
                        index_->DebugString().c_str());
  }

  virtual void WhenRange(Demon* d) {
    index_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    // Warning: This will expand all values into a vector.
    visitor->BeginVisitIntegerExpression(ModelVisitor::kElement, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            index_);
    if (index_->Min() == 0) {
      visitor->VisitInt64ToInt64AsArray(values_,
                                        ModelVisitor::kValuesArgument,
                                        index_->Max());
    } else {
      visitor->VisitInt64ToInt64Extension(values_,
                                          index_->Min(),
                                          index_->Max());
    }
    visitor->EndVisitIntegerExpression(ModelVisitor::kElement, this);
  }

 private:
  ResultCallback1<int64, int64>* values_;
  IntVar* const index_;
};
}  // namespace

IntExpr* Solver::MakeElement(ResultCallback1<int64, int64>* values,
                             IntVar* const index) {
  CHECK_EQ(this, index->solver());
  return RevAlloc(new IntExprFunctionElement(this, values, index, true));
}

namespace {
class OppositeCallback : public BaseObject {
 public:
  OppositeCallback(ResultCallback1<int64, int64>* const values)
      : values_(values) {
    CHECK_NOTNULL(values_);
    values_->CheckIsRepeatable();
  }

  virtual ~OppositeCallback() {}

  int64 Run(int64 index) {
    return -values_->Run(index);
  }
 public:
  scoped_ptr<ResultCallback1<int64, int64> > values_;
};
}  // namespace

IntExpr* Solver::MakeMonotonicElement(ResultCallback1<int64, int64>* values,
                                      bool increasing,
                                      IntVar* const index) {
  CHECK_EQ(this, index->solver());
  if (increasing) {
    return RevAlloc(new IncreasingIntExprFunctionElement(this, values, index));
  } else {
    OppositeCallback* const opposite_callback =
        RevAlloc(new OppositeCallback(values));
    ResultCallback1<int64, int64>* opposite_values =
        NewPermanentCallback(opposite_callback, &OppositeCallback::Run);
    return MakeOpposite(RevAlloc(
        new IncreasingIntExprFunctionElement(this, opposite_values, index)));
  }
}

// ----- IntIntExprFunctionElement -----

namespace {
class IntIntExprFunctionElement : public BaseIntExpr {
 public:
  IntIntExprFunctionElement(Solver* const s,
                            ResultCallback2<int64, int64, int64>* values,
                            IntVar* const expr1,
                            IntVar* const expr2);
  virtual ~IntIntExprFunctionElement();
  virtual string DebugString() const {
    return StringPrintf("IntIntFunctionElement(%s,%s)",
                        expr1_->DebugString().c_str(),
                        expr2_->DebugString().c_str());
  }
  virtual int64 Min() const;
  virtual int64 Max() const;
  virtual void Range(int64* mi, int64* ma);
  virtual void SetMin(int64 m);
  virtual void SetMax(int64 m);
  virtual void SetRange(int64 mi, int64 ma);
  virtual bool Bound() const { return expr1_->Bound() && expr2_->Bound(); }
  // TODO(user) : improve me, the previous test is not always true
  virtual void WhenRange(Demon* d) {
    expr1_->WhenRange(d);
    expr2_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kElement, this);
    // TODO(user): Implement me.
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            expr1_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndex2Argument,
                                            expr2_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kElement, this);
  }

 private:
  int64 ElementValue(int index1, int index2) const {
    return values_->Run(index1, index2);
  }
  void UpdateSupports() const;

  IntVar* const expr1_;
  IntVar* const expr2_;
  mutable int64 min_;
  mutable int min_support1_;
  mutable int min_support2_;
  mutable int64 max_;
  mutable int max_support1_;
  mutable int max_support2_;
  mutable bool initial_update_;
  scoped_ptr<ResultCallback2<int64, int64, int64> > values_;
  IntVarIterator* const expr1_iterator_;
  IntVarIterator* const expr2_iterator_;
};

IntIntExprFunctionElement::IntIntExprFunctionElement(
    Solver* const s,
    ResultCallback2<int64, int64, int64>* values,
    IntVar* const expr1,
    IntVar* const expr2)
    : BaseIntExpr(s),
      expr1_(expr1),
      expr2_(expr2),
      min_(0),
      min_support1_(-1),
      min_support2_(-1),
      max_(0),
      max_support1_(-1),
      max_support2_(-1),
      initial_update_(true),
      values_(values),
      expr1_iterator_(expr1_->MakeDomainIterator(true)),
      expr2_iterator_(expr2_->MakeDomainIterator(true)) {
  CHECK(values) << "null pointer";
  values->CheckIsRepeatable();
}

IntIntExprFunctionElement::~IntIntExprFunctionElement() {}

int64 IntIntExprFunctionElement::Min() const {
  UpdateSupports();
  return min_;
}

int64 IntIntExprFunctionElement::Max() const {
  UpdateSupports();
  return max_;
}

void IntIntExprFunctionElement::Range(int64* lower_bound, int64* upper_bound) {
  UpdateSupports();
  *lower_bound = min_;
  *upper_bound = max_;
}

#define UPDATE_ELEMENT_INDEX_BOUNDS(test)       \
  const int64 emin1 = expr1_->Min();            \
  const int64 emax1 = expr1_->Max();            \
  const int64 emin2 = expr2_->Min();            \
  const int64 emax2 = expr2_->Max();            \
  int64 nmin1 = emin1;                          \
  bool found = false;                           \
  while (nmin1 <= emax1 && !found) {            \
    for (int i = emin2; i <= emax2; ++i) {      \
      int64 value = ElementValue(nmin1, i);     \
      if (test) {                               \
        found = true;                           \
        break;                                  \
      }                                         \
    }                                           \
    if (!found) {                               \
      nmin1++;                                  \
    }                                           \
  }                                             \
  if (nmin1 > emax1) {                          \
    solver()->Fail();                           \
  }                                             \
  int64 nmin2 = emin2;                          \
  found = false;                                \
  while (nmin2 <= emax2 && !found) {            \
    for (int i = emin1; i <= emax1; ++i) {      \
      int64 value = ElementValue(i, nmin2);     \
      if (test) {                               \
        found = true;                           \
        break;                                  \
      }                                         \
    }                                           \
    if (!found) {                               \
      nmin2++;                                  \
    }                                           \
  }                                             \
  if (nmin2 > emax2) {                          \
    solver()->Fail();                           \
  }                                             \
  int64 nmax1 = emax1;                          \
  found = false;                                \
  while (nmax1 >= nmin1 && !found) {            \
    for (int i = emin2; i <= emax2; ++i) {      \
      int64 value = ElementValue(nmax1, i);     \
      if (test) {                               \
        found = true;                           \
        break;                                  \
      }                                         \
    }                                           \
    if (!found) {                               \
      nmax1--;                                  \
    }                                           \
  }                                             \
  int64 nmax2 = emax2;                          \
  found = false;                                \
  while (nmax2 >= nmin2 && !found) {            \
    for (int i = emin1; i <= emax1; ++i) {      \
      int64 value = ElementValue(i, nmax2);     \
      if (test) {                               \
        found = true;                           \
        break;                                  \
      }                                         \
    }                                           \
    if (!found) {                               \
      nmax2--;                                  \
    }                                           \
  }                                             \
  expr1_->SetRange(nmin1, nmax1);               \
  expr2_->SetRange(nmin2, nmax2);

void IntIntExprFunctionElement::SetMin(int64 lower_bound) {
  UPDATE_ELEMENT_INDEX_BOUNDS(value >= lower_bound);
}

void IntIntExprFunctionElement::SetMax(int64 upper_bound) {
  UPDATE_ELEMENT_INDEX_BOUNDS(value <= upper_bound);
}

void IntIntExprFunctionElement::SetRange(int64 lower_bound, int64 upper_bound) {
  if (lower_bound > upper_bound) {
    solver()->Fail();
  }
  UPDATE_ELEMENT_INDEX_BOUNDS(value >= lower_bound && value <= upper_bound);
}

#undef UPDATE_ELEMENT_INDEX_BOUNDS

void IntIntExprFunctionElement::UpdateSupports() const {
  if (initial_update_
      || !expr1_->Contains(min_support1_)
      || !expr1_->Contains(max_support1_)
      || !expr2_->Contains(min_support2_)
      || !expr2_->Contains(max_support2_)) {
    const int64 emax1 = expr1_->Max();
    const int64 emax2 = expr2_->Max();
    int64 min_value = ElementValue(emax1, emax2);
    int64 max_value = min_value;
    int min_support1 = emax1;
    int max_support1 = emax1;
    int min_support2 = emax2;
    int max_support2 = emax2;
    for (expr1_iterator_->Init();
         expr1_iterator_->Ok();
         expr1_iterator_->Next()) {
      const int64 index1 = expr1_iterator_->Value();
      for (expr2_iterator_->Init();
           expr2_iterator_->Ok();
           expr2_iterator_->Next()) {
        const int64 index2 = expr2_iterator_->Value();
        const int64 value = ElementValue(index1, index2);
        if (value > max_value) {
          max_value = value;
          max_support1 = index1;
          max_support2 = index2;
        } else if (value < min_value) {
          min_value = value;
          min_support1 = index1;
          min_support2 = index2;
        }
      }
    }
    Solver* s = solver();
    s->SaveAndSetValue(&min_, min_value);
    s->SaveAndSetValue(&min_support1_, min_support1);
    s->SaveAndSetValue(&min_support2_, min_support2);
    s->SaveAndSetValue(&max_, max_value);
    s->SaveAndSetValue(&max_support1_, max_support1);
    s->SaveAndSetValue(&max_support2_, max_support2);
    s->SaveAndSetValue(&initial_update_, false);
  }
}
}  // namespace

IntExpr* Solver::MakeElement(ResultCallback2<int64, int64, int64>* values,
                             IntVar* const index1, IntVar* const index2) {
  CHECK_EQ(this, index1->solver());
  CHECK_EQ(this, index2->solver());
  return RevAlloc(
      new IntIntExprFunctionElement(this, values, index1, index2));
}

// ---------- Generalized element ----------

// ----- IntExprArrayElementCt -----

// This constraint implements vars[index] == var. It is delayed such
// that propagation only occurs when all variables have been touched.

namespace {
class IntExprArrayElementCt : public Constraint {
 public:
  IntExprArrayElementCt(Solver* const s,
                        const IntVar* const * vars,
                        int size,
                        IntVar* const expr,
                        IntVar* const var);
  virtual ~IntExprArrayElementCt() {}

    virtual void Post();
  virtual void InitialPropagate();

  void Propagate();
  void Update(int index);
  void UpdateExpr();

  virtual string DebugString() const;

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kElementEqual, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_.get(),
                                               size_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            expr_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            var_);
    visitor->EndVisitConstraint(ModelVisitor::kElementEqual, this);
  }

 private:
  scoped_array<IntVar*> vars_;
  int size_;
  IntVar* const expr_;
  IntVar* const var_;
  int min_support_;
  int max_support_;
};

IntExprArrayElementCt::IntExprArrayElementCt(Solver* const s,
                                             const IntVar* const * vars,
                                             int size,
                                             IntVar* const expr,
                                             IntVar* const var)
    : Constraint(s),
      vars_(new IntVar*[size]),
      size_(size),
      expr_(expr),
      var_(var),
      min_support_(-1),
      max_support_(-1) {
  memcpy(vars_.get(), vars, size_ * sizeof(*vars));
}

void IntExprArrayElementCt::Post() {
  Demon* d = MakeDelayedConstraintDemon0(solver(),
                                         this,
                                         &IntExprArrayElementCt::Propagate,
                                         "Propagate");
  for (int i = 0; i < size_; ++i) {
    vars_[i]->WhenRange(d);
    Demon* u = MakeConstraintDemon1(solver(),
                                    this,
                                    &IntExprArrayElementCt::Update,
                                    "Update",
                                    i);
    vars_[i]->WhenRange(u);
  }
  expr_->WhenRange(d);
  Demon* ue = MakeConstraintDemon0(solver(),
                                   this,
                                   &IntExprArrayElementCt::UpdateExpr,
                                   "UpdateExpr");
  expr_->WhenRange(ue);
  Demon* uv = MakeConstraintDemon0(solver(),
                                   this,
                                   &IntExprArrayElementCt::Propagate,
                                   "UpdateVar");

  var_->WhenRange(uv);
}

void IntExprArrayElementCt::InitialPropagate() {
  Propagate();
}

void IntExprArrayElementCt::Propagate() {
  const int64 emin = std::max(0LL, expr_->Min());
  const int64 emax = std::min(size_ - 1LL, expr_->Max());
  const int64 vmin = var_->Min();
  const int64 vmax = var_->Max();
  if (emin == emax) {
    expr_->SetValue(emin);  // in case it was reduced by the above min/max.
    vars_[emin]->SetRange(vmin, vmax);
  } else {
    int64 nmin = emin;
    while (nmin <= emax && (vars_[nmin]->Min() > vmax ||
                            vars_[nmin]->Max() < vmin)) {
      nmin++;
    }
    int64 nmax = emax;
    while (nmax >= nmin && (vars_[nmax]->Max() < vmin ||
                            vars_[nmax]->Min() > vmax)) {
      nmax--;
    }
    expr_->SetRange(nmin, nmax);
    if (nmin == nmax) {
      vars_[nmin]->SetRange(vmin, vmax);
    }
  }
  if (min_support_ == -1 || max_support_ == -1) {
    int min_support = -1;
    int max_support = -1;
    int64 gmin = kint64max;
    int64 gmax = kint64min;
    for (int i = expr_->Min(); i <= expr_->Max(); ++i) {
      const int64 vmin = vars_[i]->Min();
      if (vmin < gmin) {
        gmin = vmin;
      }
      const int64 vmax = vars_[i]->Max();
      if (vmax > gmax) {
        gmax = vmax;
      }
    }
    solver()->SaveAndSetValue(&min_support_, min_support);
    solver()->SaveAndSetValue(&max_support_, max_support);
    var_->SetRange(gmin, gmax);
  }
}

void IntExprArrayElementCt::Update(int index) {
  if (index == min_support_ || index == max_support_) {
    solver()->SaveAndSetValue(&min_support_, -1);
    solver()->SaveAndSetValue(&max_support_, -1);
  }
}

void IntExprArrayElementCt::UpdateExpr() {
  if (!expr_->Contains(min_support_) || !expr_->Contains(max_support_)) {
    solver()->SaveAndSetValue(&min_support_, -1);
    solver()->SaveAndSetValue(&max_support_, -1);
  }
}

string IntExprArrayElementCt::DebugString() const {
  return StringPrintf("IntExprArrayElement([%s], %s) == %s",
                      DebugStringArray(vars_.get(), size_, ", ").c_str(),
                      expr_->DebugString().c_str(),
                      var_->DebugString().c_str());
}

// ----- IntExprArrayElement -----

class IntExprArrayElement : public BaseIntExpr {
 public:
  IntExprArrayElement(Solver* const s,
                      const IntVar* const * vars,
                      int size,
                      IntVar* const expr);
  virtual ~IntExprArrayElement() {}

  virtual int64 Min() const;
  virtual void SetMin(int64 m);
  virtual int64 Max() const;
  virtual void SetMax(int64 m);
  virtual void SetRange(int64 mi, int64 ma);
  virtual bool Bound() const;
  virtual string name() const {
    return StringPrintf("IntArrayElement(%s, %s)",
                        NameArray(vars_.get(), size_, ", ").c_str(),
                        var_->name().c_str());
  }
  virtual string DebugString() const {
    return StringPrintf("IntArrayElement(%s, %s)",
                        DebugStringArray(vars_.get(), size_, ", ").c_str(),
                        var_->DebugString().c_str());
  }
  virtual void WhenRange(Demon* d) {
    var_->WhenRange(d);
    for (int i = 0; i < size_; ++i) {
      vars_[i]->WhenRange(d);
    }
  }
  virtual IntVar* CastToVar() {
    Solver* const s = solver();
    int64 vmin = 0LL;
    int64 vmax = 0LL;
    Range(&vmin, &vmax);
    IntVar* var = solver()->MakeIntVar(vmin, vmax);
    AddDelegateName("Var", var);
    s->AddDelegateConstraint(s->RevAlloc(new IntExprArrayElementCt(s,
                                                                   vars_.get(),
                                                                   size_,
                                                                   var_,
                                                                   var)));
    return var;
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kElement, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_.get(),
                                               size_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            var_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kElement, this);
  }

 private:
  scoped_array<IntVar*> vars_;
  int size_;
  IntVar* const var_;
};

IntExprArrayElement::IntExprArrayElement(Solver* const s,
                                         const IntVar* const * vars,
                                         int size,
                                         IntVar* const v)
    : BaseIntExpr(s), vars_(new IntVar*[size]),
      size_(size), var_(v) {
  CHECK(vars);
  memcpy(vars_.get(), vars, size_ * sizeof(*vars));
}

int64 IntExprArrayElement::Min() const {
  const int64 emin = std::max(0LL, var_->Min());
  const int64 emax = std::min(size_ - 1LL, var_->Max());
  int64 res = kint64max;
  for (int i = emin; i <= emax; ++i) {
    const int64 vmin = vars_[i]->Min();
    if (vmin < res && var_->Contains(i)) {
      res = vmin;
    }
  }
  return res;
}

void IntExprArrayElement::SetMin(int64 m) {
  const int64 emin = std::max(0LL, var_->Min());
  const int64 emax = std::min(size_ - 1LL, var_->Max());
  if (emin == emax) {
    var_->SetValue(emin);  // in case it was reduced by the above min/max.
    vars_[emin]->SetMin(m);
  } else {
    int64 nmin = emin;
    while (nmin <= emax && vars_[nmin]->Max() < m) {
      nmin++;
    }
    if (nmin > emax) {
      solver()->Fail();
    }
    int64 nmax = emax;
    while (nmax >= nmin && vars_[nmax]->Max() < m) {
      nmax--;
    }
    var_->SetRange(nmin, nmax);
    if (var_->Bound()) {
      vars_[var_->Min()]->SetMin(m);
    }
  }
}

int64 IntExprArrayElement::Max() const {
  const int64 emin = std::max(0LL, var_->Min());
  const int64 emax = std::min(size_ - 1LL, var_->Max());
  int64 res = kint64min;
  for (int i = emin; i <= emax; ++i) {
    const int64 vmax = vars_[i]->Max();
    if (vmax > res && var_->Contains(i)) {
      res = vmax;
    }
  }
  return res;
}

void IntExprArrayElement::SetMax(int64 m) {
  const int64 emin = std::max(0LL, var_->Min());
  const int64 emax = std::min(size_ - 1LL, var_->Max());
  if (emin == emax) {
    var_->SetValue(emin);  // in case it was reduced by the above min/max.
    vars_[emin]->SetMax(m);
  } else {
    int64 nmin = emin;
    while (nmin <= emax && vars_[nmin]->Min() > m) {
      nmin++;
    }
    if (nmin > emax) {
      solver()->Fail();
    }
    int64 nmax = emax;
    while (nmax >= nmin && vars_[nmax]->Min() > m) {
      nmax--;
    }
    var_->SetRange(nmin, nmax);
    if (var_->Bound()) {
      vars_[var_->Min()]->SetMax(m);
    }
  }
}

void IntExprArrayElement::SetRange(int64 mi, int64 ma) {
  if (mi > ma) {
    solver()->Fail();
  }
  const int64 emin = std::max(0LL, var_->Min());
  const int64 emax = std::min(size_ - 1LL, var_->Max());
  if (emin == emax) {
    var_->SetValue(emin);  // in case it was reduced by the above min/max.
    vars_[emin]->SetRange(mi, ma);
  } else {
    int64 nmin = emin;
    while (nmin <= emax && (vars_[nmin]->Min() > ma ||
                            vars_[nmin]->Max() < mi)) {
      nmin++;
    }
    if (nmin > emax) {
      solver()->Fail();
    }
    int64 nmax = emax;
    while (nmax >= nmin && (vars_[nmax]->Max() < mi ||
                            vars_[nmax]->Min() > ma)) {
      nmax--;
    }
    if (nmax < emin) {
      solver()->Fail();
    }
    var_->SetRange(nmin, nmax);
    if (var_->Bound()) {
      vars_[var_->Min()]->SetRange(mi, ma);
    }
  }
}

bool IntExprArrayElement::Bound() const {
  const int64 emin = std::max(0LL, var_->Min());
  const int64 emax = std::min(size_ - 1LL, var_->Max());
  const int64 v = vars_[emin]->Min();
  for (int i = emin; i <= emax; ++i) {
    if (var_->Contains(i) && (!vars_[i]->Bound() || vars_[i]->Value() != v)) {
      return false;
    }
  }
  return true;
}
}  // namespace

IntExpr* Solver::MakeElement(const IntVar* const * vars,
                             int size,
                             IntVar* const index) {
  CHECK_EQ(this, index->solver());
  return RevAlloc(new IntExprArrayElement(this, vars, size, index));
}

IntExpr* Solver::MakeElement(const std::vector<IntVar*>& vars, IntVar* const index) {
  CHECK_EQ(this, index->solver());
  return RevAlloc(new IntExprArrayElement(this, vars.data(),
                                          vars.size(), index));
}

}  // namespace operations_research

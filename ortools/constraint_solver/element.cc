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

#include "ortools/constraint_solver/element.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraints.h"
#include "ortools/constraint_solver/expressions.h"
#include "ortools/constraint_solver/model_cache.h"
#include "ortools/constraint_solver/utilities.h"
#include "ortools/util/range_minimum_query.h"
#include "ortools/util/string_array.h"

namespace operations_research {

BaseIntExprElement::BaseIntExprElement(Solver* s, IntVar* e)
    : BaseIntExpr(s),
      expr_(e),
      min_(0),
      min_support_(-1),
      max_(0),
      max_support_(-1),
      initial_update_(true),
      expr_iterator_(expr_->MakeDomainIterator(true)) {
  CHECK(s != nullptr);
  CHECK(e != nullptr);
}

int64_t BaseIntExprElement::Min() const {
  UpdateSupports();
  return min_;
}

int64_t BaseIntExprElement::Max() const {
  UpdateSupports();
  return max_;
}

void BaseIntExprElement::Range(int64_t* mi, int64_t* ma) {
  UpdateSupports();
  *mi = min_;
  *ma = max_;
}

void BaseIntExprElement::SetMin(int64_t m) {
  UpdateElementIndexBounds([m](int64_t value) { return value < m; });
}

void BaseIntExprElement::SetMax(int64_t m) {
  UpdateElementIndexBounds([m](int64_t value) { return value > m; });
}

void BaseIntExprElement::SetRange(int64_t mi, int64_t ma) {
  if (mi > ma) {
    solver()->Fail();
  }
  UpdateElementIndexBounds(
      [mi, ma](int64_t value) { return value < mi || value > ma; });
}

void BaseIntExprElement::UpdateSupports() const {
  if (initial_update_ || !expr_->Contains(min_support_) ||
      !expr_->Contains(max_support_)) {
    const int64_t emin = ExprMin();
    const int64_t emax = ExprMax();
    int64_t min_value = ElementValue(emax);
    int64_t max_value = min_value;
    int min_support = emax;
    int max_support = emax;
    const uint64_t expr_size = expr_->Size();
    if (expr_size > 1) {
      if (expr_size == emax - emin + 1) {
        // Value(emax) already stored in min_value, max_value.
        for (int64_t index = emin; index < emax; ++index) {
          const int64_t value = ElementValue(index);
          if (value > max_value) {
            max_value = value;
            max_support = index;
          } else if (value < min_value) {
            min_value = value;
            min_support = index;
          }
        }
      } else {
        for (const int64_t index : InitAndGetValues(expr_iterator_)) {
          if (index >= emin && index <= emax) {
            const int64_t value = ElementValue(index);
            if (value > max_value) {
              max_value = value;
              max_support = index;
            } else if (value < min_value) {
              min_value = value;
              min_support = index;
            }
          }
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

void BaseIntExprElement::WhenRange(Demon* d) { expr_->WhenRange(d); }

IntElementConstraint::IntElementConstraint(Solver* s,
                                           const std::vector<int64_t>& values,
                                           IntVar* index, IntVar* elem)
    : CastConstraint(s, elem),
      values_(values),
      index_(index),
      index_iterator_(index_->MakeDomainIterator(true)) {
  CHECK(index != nullptr);
}

void IntElementConstraint::Post() {
  Demon* d = solver()->MakeDelayedConstraintInitialPropagateCallback(this);
  index_->WhenDomain(d);
  target_var_->WhenRange(d);
}

void IntElementConstraint::InitialPropagate() {
  index_->SetRange(0, values_.size() - 1);
  const int64_t target_var_min = target_var_->Min();
  const int64_t target_var_max = target_var_->Max();
  int64_t new_min = target_var_max;
  int64_t new_max = target_var_min;
  to_remove_.clear();
  for (const int64_t index : InitAndGetValues(index_iterator_)) {
    const int64_t value = values_[index];
    if (value < target_var_min || value > target_var_max) {
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
  target_var_->SetRange(new_min, new_max);
  if (!to_remove_.empty()) {
    index_->RemoveValues(to_remove_);
  }
}

std::string IntElementConstraint::DebugString() const {
  return absl::StrFormat("IntElementConstraint(%s, %s, %s)",
                         absl::StrJoin(values_, ", "), index_->DebugString(),
                         target_var_->DebugString());
}

void IntElementConstraint::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kElementEqual, this);
  visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument, values_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument, index_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          target_var_);
  visitor->EndVisitConstraint(ModelVisitor::kElementEqual, this);
}

std::string IntExprElement::name() const {
  const int size = values_.size();
  if (size > 10) {
    return absl::StrFormat("IntElement(array of size %d, %s)", size,
                           expr_->name());
  } else {
    return absl::StrFormat("IntElement(%s, %s)", absl::StrJoin(values_, ", "),
                           expr_->name());
  }
}

std::string IntExprElement::DebugString() const {
  const int size = values_.size();
  if (size > 10) {
    return absl::StrFormat("IntElement(array of size %d, %s)", size,
                           expr_->DebugString());
  } else {
    return absl::StrFormat("IntElement(%s, %s)", absl::StrJoin(values_, ", "),
                           expr_->DebugString());
  }
}

IntVar* IntExprElement::CastToVar() {
  Solver* const s = solver();
  IntVar* const var = s->MakeIntVar(values_);
  s->AddCastConstraint(
      s->RevAlloc(new IntElementConstraint(s, values_, expr_, var)), var, this);
  return var;
}

void IntExprElement::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kElement, this);
  visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument, values_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument, expr_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kElement, this);
}

int64_t IntExprElement::ElementValue(int index) const {
  DCHECK_LT(index, values_.size());
  return values_[index];
}
int64_t IntExprElement::ExprMin() const {
  return std::max<int64_t>(0, expr_->Min());
}
int64_t IntExprElement::ExprMax() const {
  return values_.empty() ? 0
                         : std::min<int64_t>(values_.size() - 1, expr_->Max());
}

RangeMinimumQueryExprElement::RangeMinimumQueryExprElement(
    Solver* solver, const std::vector<int64_t>& values, IntVar* index)
    : BaseIntExpr(solver), index_(index), min_rmq_(values), max_rmq_(values) {
  CHECK(solver != nullptr);
  CHECK(index != nullptr);
}

int64_t RangeMinimumQueryExprElement::Min() const {
  return min_rmq_.RangeMinimum(IndexMin(), IndexMax() + 1);
}

int64_t RangeMinimumQueryExprElement::Max() const {
  return max_rmq_.RangeMinimum(IndexMin(), IndexMax() + 1);
}

void RangeMinimumQueryExprElement::Range(int64_t* mi, int64_t* ma) {
  const int64_t range_min = IndexMin();
  const int64_t range_max = IndexMax() + 1;
  *mi = min_rmq_.RangeMinimum(range_min, range_max);
  *ma = max_rmq_.RangeMinimum(range_min, range_max);
}

#define UPDATE_RMQ_BASE_ELEMENT_INDEX_BOUNDS(test)       \
  const std::vector<int64_t>& values = min_rmq_.array(); \
  int64_t index_min = IndexMin();                        \
  int64_t index_max = IndexMax();                        \
  int64_t value = values[index_min];                     \
  while (index_min < index_max && (test)) {              \
    index_min++;                                         \
    value = values[index_min];                           \
  }                                                      \
  if (index_min == index_max && (test)) {                \
    solver()->Fail();                                    \
  }                                                      \
  value = values[index_max];                             \
  while (index_max >= index_min && (test)) {             \
    index_max--;                                         \
    value = values[index_max];                           \
  }                                                      \
  index_->SetRange(index_min, index_max);

void RangeMinimumQueryExprElement::SetMin(int64_t m) {
  UPDATE_RMQ_BASE_ELEMENT_INDEX_BOUNDS(value < m);
}

void RangeMinimumQueryExprElement::SetMax(int64_t m) {
  UPDATE_RMQ_BASE_ELEMENT_INDEX_BOUNDS(value > m);
}

void RangeMinimumQueryExprElement::SetRange(int64_t mi, int64_t ma) {
  if (mi > ma) {
    solver()->Fail();
  }
  UPDATE_RMQ_BASE_ELEMENT_INDEX_BOUNDS(value < mi || value > ma);
}

#undef UPDATE_RMQ_BASE_ELEMENT_INDEX_BOUNDS

void RangeMinimumQueryExprElement::WhenRange(Demon* d) { index_->WhenRange(d); }
IntVar* RangeMinimumQueryExprElement::CastToVar() {
  IntVar* const var = solver()->MakeIntVar(min_rmq_.array());
  solver()->AddCastConstraint(solver()->RevAlloc(new IntElementConstraint(
                                  solver(), min_rmq_.array(), index_, var)),
                              var, this);
  return var;
}
void RangeMinimumQueryExprElement::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kElement, this);
  visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument,
                                     min_rmq_.array());
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument, index_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kElement, this);
}

IncreasingIntExprElement::IncreasingIntExprElement(
    Solver* s, const std::vector<int64_t>& values, IntVar* index)
    : BaseIntExpr(s), values_(values), index_(index) {
  DCHECK(index);
  DCHECK(s);
}

int64_t IncreasingIntExprElement::Min() const {
  const int64_t expression_min = std::max<int64_t>(0, index_->Min());
  return (expression_min < values_.size()
              ? values_[expression_min]
              : std::numeric_limits<int64_t>::max());
}

void IncreasingIntExprElement::SetMin(int64_t m) {
  const int64_t index_min = std::max<int64_t>(0, index_->Min());
  const int64_t index_max =
      std::min<int64_t>(values_.size() - 1, index_->Max());

  if (index_min > index_max || m > values_[index_max]) {
    solver()->Fail();
  }

  const std::vector<int64_t>::const_iterator first =
      std::lower_bound(values_.begin(), values_.end(), m);
  const int64_t new_index_min = first - values_.begin();
  index_->SetMin(new_index_min);
}

int64_t IncreasingIntExprElement::Max() const {
  const int64_t expression_max =
      std::min<int64_t>(values_.size() - 1, index_->Max());
  return (expression_max >= 0 ? values_[expression_max]
                              : std::numeric_limits<int64_t>::max());
}

void IncreasingIntExprElement::SetMax(int64_t m) {
  int64_t index_min = std::max<int64_t>(0, index_->Min());
  if (m < values_[index_min]) {
    solver()->Fail();
  }

  const std::vector<int64_t>::const_iterator last_after =
      std::upper_bound(values_.begin(), values_.end(), m);
  const int64_t new_index_max = (last_after - values_.begin()) - 1;
  index_->SetRange(0, new_index_max);
}

void IncreasingIntExprElement::SetRange(int64_t mi, int64_t ma) {
  if (mi > ma) {
    solver()->Fail();
  }
  const int64_t index_min = std::max<int64_t>(0, index_->Min());
  const int64_t index_max =
      std::min<int64_t>(values_.size() - 1, index_->Max());

  if (mi > ma || ma < values_[index_min] || mi > values_[index_max]) {
    solver()->Fail();
  }

  const std::vector<int64_t>::const_iterator first =
      std::lower_bound(values_.begin(), values_.end(), mi);
  const int64_t new_index_min = first - values_.begin();

  const std::vector<int64_t>::const_iterator last_after =
      std::upper_bound(first, values_.end(), ma);
  const int64_t new_index_max = (last_after - values_.begin()) - 1;

  // Assign.
  index_->SetRange(new_index_min, new_index_max);
}

std::string IncreasingIntExprElement::name() const {
  return absl::StrFormat("IntElement(%s, %s)", absl::StrJoin(values_, ", "),
                         index_->name());
}
std::string IncreasingIntExprElement::DebugString() const {
  return absl::StrFormat("IntElement(%s, %s)", absl::StrJoin(values_, ", "),
                         index_->DebugString());
}

void IncreasingIntExprElement::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kElement, this);
  visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument, values_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument, index_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kElement, this);
}

void IncreasingIntExprElement::WhenRange(Demon* d) { index_->WhenRange(d); }

IntVar* IncreasingIntExprElement::CastToVar() {
  Solver* s = solver();
  IntVar* var = s->MakeIntVar(values_);
  if (var->Bound()) {
    // If constructed with a constant range, it might be already bound.
    // In that case we need to propagate back to the index.
    // However, LinkVarExpr handles propagation both ways.
    // But LinkVarExpr signature is void(Solver*, IntExpr*, IntVar*).
    // Here we need to call it.
    // But LinkVarExpr is declared in element.h.
    // And defined in expressions.cc? No, the previous error said it was defined
    // in element.cc AND expressions.cc. So I removed it from element.cc. So I
    // can call it here.
    operations_research::LinkVarExpr(s, this, var);
  } else {
    operations_research::LinkVarExpr(s, this, var);
  }
  return var;
}

IntExprFunctionElement::IntExprFunctionElement(Solver* s,
                                               Solver::IndexEvaluator1 values,
                                               IntVar* e)
    : BaseIntExprElement(s, e), values_(std::move(values)) {
  CHECK(values_ != nullptr);
}

IntExprFunctionElement::~IntExprFunctionElement() {}

std::string IntExprFunctionElement::name() const {
  return absl::StrFormat("IntFunctionElement(%s)", expr_->name());
}

std::string IntExprFunctionElement::DebugString() const {
  return absl::StrFormat("IntFunctionElement(%s)", expr_->DebugString());
}

void IntExprFunctionElement::Accept(ModelVisitor* visitor) const {
  // Warning: This will expand all values into a vector.
  visitor->BeginVisitIntegerExpression(ModelVisitor::kElement, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument, expr_);
  visitor->VisitInt64ToInt64Extension(values_, expr_->Min(), expr_->Max());
  visitor->EndVisitIntegerExpression(ModelVisitor::kElement, this);
}

int64_t IntExprFunctionElement::ElementValue(int index) const {
  return values_(index);
}
int64_t IntExprFunctionElement::ExprMin() const { return expr_->Min(); }
int64_t IntExprFunctionElement::ExprMax() const { return expr_->Max(); }

IncreasingIntExprFunctionElement::IncreasingIntExprFunctionElement(
    Solver* s, Solver::IndexEvaluator1 values, IntVar* index)
    : BaseIntExpr(s), values_(std::move(values)), index_(index) {
  DCHECK(values_ != nullptr);
  DCHECK(index);
  DCHECK(s);
}

IncreasingIntExprFunctionElement::~IncreasingIntExprFunctionElement() {}

int64_t IncreasingIntExprFunctionElement::Min() const {
  return values_(index_->Min());
}

void IncreasingIntExprFunctionElement::SetMin(int64_t m) {
  const int64_t index_min = index_->Min();
  const int64_t index_max = index_->Max();
  if (m > values_(index_max)) {
    solver()->Fail();
  }
  const int64_t new_index_min = FindNewIndexMin(index_min, index_max, m);
  index_->SetMin(new_index_min);
}

int64_t IncreasingIntExprFunctionElement::Max() const {
  return values_(index_->Max());
}

void IncreasingIntExprFunctionElement::SetMax(int64_t m) {
  int64_t index_min = index_->Min();
  int64_t index_max = index_->Max();
  if (m < values_(index_min)) {
    solver()->Fail();
  }
  const int64_t new_index_max = FindNewIndexMax(index_min, index_max, m);
  index_->SetMax(new_index_max);
}

void IncreasingIntExprFunctionElement::SetRange(int64_t mi, int64_t ma) {
  const int64_t index_min = index_->Min();
  const int64_t index_max = index_->Max();
  const int64_t value_min = values_(index_min);
  const int64_t value_max = values_(index_max);
  if (mi > ma || ma < value_min || mi > value_max) {
    solver()->Fail();
  }
  if (mi <= value_min && ma >= value_max) {
    // Nothing to do.
    return;
  }

  const int64_t new_index_min = FindNewIndexMin(index_min, index_max, mi);
  const int64_t new_index_max = FindNewIndexMax(new_index_min, index_max, ma);
  // Assign.
  index_->SetRange(new_index_min, new_index_max);
}

std::string IncreasingIntExprFunctionElement::name() const {
  return absl::StrFormat("IncreasingIntExprFunctionElement(values, %s)",
                         index_->name());
}

std::string IncreasingIntExprFunctionElement::DebugString() const {
  return absl::StrFormat("IncreasingIntExprFunctionElement(values, %s)",
                         index_->DebugString());
}

void IncreasingIntExprFunctionElement::WhenRange(Demon* d) {
  index_->WhenRange(d);
}

void IncreasingIntExprFunctionElement::Accept(ModelVisitor* visitor) const {
  // Warning: This will expand all values into a vector.
  visitor->BeginVisitIntegerExpression(ModelVisitor::kElement, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument, index_);
  if (index_->Min() == 0) {
    visitor->VisitInt64ToInt64AsArray(values_, ModelVisitor::kValuesArgument,
                                      index_->Max());
  } else {
    visitor->VisitInt64ToInt64Extension(values_, index_->Min(), index_->Max());
  }
  visitor->EndVisitIntegerExpression(ModelVisitor::kElement, this);
}

int64_t IncreasingIntExprFunctionElement::FindNewIndexMin(int64_t index_min,
                                                          int64_t index_max,
                                                          int64_t m) {
  if (m <= values_(index_min)) {
    return index_min;
  }

  DCHECK_LT(values_(index_min), m);
  DCHECK_GE(values_(index_max), m);

  int64_t index_lower_bound = index_min;
  int64_t index_upper_bound = index_max;
  while (index_upper_bound - index_lower_bound > 1) {
    DCHECK_LT(values_(index_lower_bound), m);
    DCHECK_GE(values_(index_upper_bound), m);
    const int64_t pivot = (index_lower_bound + index_upper_bound) / 2;
    const int64_t pivot_value = values_(pivot);
    if (pivot_value < m) {
      index_lower_bound = pivot;
    } else {
      index_upper_bound = pivot;
    }
  }
  DCHECK(values_(index_upper_bound) >= m);
  return index_upper_bound;
}

int64_t IncreasingIntExprFunctionElement::FindNewIndexMax(int64_t index_min,
                                                          int64_t index_max,
                                                          int64_t m) {
  if (m >= values_(index_max)) {
    return index_max;
  }

  DCHECK_LE(values_(index_min), m);
  DCHECK_GT(values_(index_max), m);

  int64_t index_lower_bound = index_min;
  int64_t index_upper_bound = index_max;
  while (index_upper_bound - index_lower_bound > 1) {
    DCHECK_LE(values_(index_lower_bound), m);
    DCHECK_GT(values_(index_upper_bound), m);
    const int64_t pivot = (index_lower_bound + index_upper_bound) / 2;
    const int64_t pivot_value = values_(pivot);
    if (pivot_value > m) {
      index_upper_bound = pivot;
    } else {
      index_lower_bound = pivot;
    }
  }
  DCHECK(values_(index_lower_bound) <= m);
  return index_lower_bound;
}

IntIntExprFunctionElement::IntIntExprFunctionElement(
    Solver* s, Solver::IndexEvaluator2 values, IntVar* expr1, IntVar* expr2)
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
      values_(std::move(values)),
      expr1_iterator_(expr1_->MakeDomainIterator(true)),
      expr2_iterator_(expr2_->MakeDomainIterator(true)) {
  CHECK(values_ != nullptr);
}

IntIntExprFunctionElement::~IntIntExprFunctionElement() {}

std::string IntIntExprFunctionElement::DebugString() const {
  return absl::StrFormat("IntIntFunctionElement(%s,%s)", expr1_->DebugString(),
                         expr2_->DebugString());
}
int64_t IntIntExprFunctionElement::Min() const {
  UpdateSupports();
  return min_;
}

int64_t IntIntExprFunctionElement::Max() const {
  UpdateSupports();
  return max_;
}

void IntIntExprFunctionElement::Range(int64_t* lower_bound,
                                      int64_t* upper_bound) {
  UpdateSupports();
  *lower_bound = min_;
  *upper_bound = max_;
}

#define UPDATE_ELEMENT_INDEX_BOUNDS(test)     \
  const int64_t emin1 = expr1_->Min();        \
  const int64_t emax1 = expr1_->Max();        \
  const int64_t emin2 = expr2_->Min();        \
  const int64_t emax2 = expr2_->Max();        \
  int64_t nmin1 = emin1;                      \
  bool found = false;                         \
  while (nmin1 <= emax1 && !found) {          \
    for (int i = emin2; i <= emax2; ++i) {    \
      int64_t value = ElementValue(nmin1, i); \
      if (test) {                             \
        found = true;                         \
        break;                                \
      }                                       \
    }                                         \
    if (!found) {                             \
      nmin1++;                                \
    }                                         \
  }                                           \
  if (nmin1 > emax1) {                        \
    solver()->Fail();                         \
  }                                           \
  int64_t nmin2 = emin2;                      \
  found = false;                              \
  while (nmin2 <= emax2 && !found) {          \
    for (int i = emin1; i <= emax1; ++i) {    \
      int64_t value = ElementValue(i, nmin2); \
      if (test) {                             \
        found = true;                         \
        break;                                \
      }                                       \
    }                                         \
    if (!found) {                             \
      nmin2++;                                \
    }                                         \
  }                                           \
  if (nmin2 > emax2) {                        \
    solver()->Fail();                         \
  }                                           \
  int64_t nmax1 = emax1;                      \
  found = false;                              \
  while (nmax1 >= nmin1 && !found) {          \
    for (int i = emin2; i <= emax2; ++i) {    \
      int64_t value = ElementValue(nmax1, i); \
      if (test) {                             \
        found = true;                         \
        break;                                \
      }                                       \
    }                                         \
    if (!found) {                             \
      nmax1--;                                \
    }                                         \
  }                                           \
  int64_t nmax2 = emax2;                      \
  found = false;                              \
  while (nmax2 >= nmin2 && !found) {          \
    for (int i = emin1; i <= emax1; ++i) {    \
      int64_t value = ElementValue(i, nmax2); \
      if (test) {                             \
        found = true;                         \
        break;                                \
      }                                       \
    }                                         \
    if (!found) {                             \
      nmax2--;                                \
    }                                         \
  }                                           \
  expr1_->SetRange(nmin1, nmax1);             \
  expr2_->SetRange(nmin2, nmax2);

void IntIntExprFunctionElement::SetMin(int64_t lower_bound) {
  UPDATE_ELEMENT_INDEX_BOUNDS(value >= lower_bound);
}

void IntIntExprFunctionElement::SetMax(int64_t upper_bound) {
  UPDATE_ELEMENT_INDEX_BOUNDS(value <= upper_bound);
}

void IntIntExprFunctionElement::SetRange(int64_t lower_bound,
                                         int64_t upper_bound) {
  if (lower_bound > upper_bound) {
    solver()->Fail();
  }
  UPDATE_ELEMENT_INDEX_BOUNDS(value >= lower_bound && value <= upper_bound);
}

#undef UPDATE_ELEMENT_INDEX_BOUNDS

void IntIntExprFunctionElement::WhenRange(Demon* d) {
  expr1_->WhenRange(d);
  expr2_->WhenRange(d);
}

void IntIntExprFunctionElement::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kElement, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument, expr1_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndex2Argument,
                                          expr2_);
  // Warning: This will expand all values into a vector.
  const int64_t expr1_min = expr1_->Min();
  const int64_t expr1_max = expr1_->Max();
  visitor->VisitIntegerArgument(ModelVisitor::kMinArgument, expr1_min);
  visitor->VisitIntegerArgument(ModelVisitor::kMaxArgument, expr1_max);
  for (int i = expr1_min; i <= expr1_max; ++i) {
    visitor->VisitInt64ToInt64Extension(
        [this, i](int64_t j) { return values_(i, j); }, expr2_->Min(),
        expr2_->Max());
  }
  visitor->EndVisitIntegerExpression(ModelVisitor::kElement, this);
}

int64_t IntIntExprFunctionElement::ElementValue(int index1, int index2) const {
  return values_(index1, index2);
}
void IntIntExprFunctionElement::UpdateSupports() const {
  if (initial_update_ || !expr1_->Contains(min_support1_) ||
      !expr1_->Contains(max_support1_) || !expr2_->Contains(min_support2_) ||
      !expr2_->Contains(max_support2_)) {
    const int64_t emax1 = expr1_->Max();
    const int64_t emax2 = expr2_->Max();
    int64_t min_value = ElementValue(emax1, emax2);
    int64_t max_value = min_value;
    int min_support1 = emax1;
    int max_support1 = emax1;
    int min_support2 = emax2;
    int max_support2 = emax2;
    for (const int64_t index1 : InitAndGetValues(expr1_iterator_)) {
      for (const int64_t index2 : InitAndGetValues(expr2_iterator_)) {
        const int64_t value = ElementValue(index1, index2);
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

IfThenElseCt::~IfThenElseCt() {}

void IfThenElseCt::Post() {
  Demon* demon = solver()->MakeConstraintInitialPropagateCallback(this);
  condition_->WhenBound(demon);
  one_->WhenRange(demon);
  zero_->WhenRange(demon);
  target_var_->WhenRange(demon);
}

void IfThenElseCt::InitialPropagate() {
  condition_->SetRange(0, 1);
  const int64_t target_var_min = target_var_->Min();
  const int64_t target_var_max = target_var_->Max();
  int64_t new_min = std::numeric_limits<int64_t>::min();
  int64_t new_max = std::numeric_limits<int64_t>::max();
  if (condition_->Max() == 0) {
    zero_->SetRange(target_var_min, target_var_max);
    zero_->Range(&new_min, &new_max);
  } else if (condition_->Min() == 1) {
    one_->SetRange(target_var_min, target_var_max);
    one_->Range(&new_min, &new_max);
  } else {
    if (target_var_max < zero_->Min() || target_var_min > zero_->Max()) {
      condition_->SetValue(1);
      one_->SetRange(target_var_min, target_var_max);
      one_->Range(&new_min, &new_max);
    } else if (target_var_max < one_->Min() || target_var_min > one_->Max()) {
      condition_->SetValue(0);
      zero_->SetRange(target_var_min, target_var_max);
      zero_->Range(&new_min, &new_max);
    } else {
      int64_t zl = 0;
      int64_t zu = 0;
      int64_t ol = 0;
      int64_t ou = 0;
      zero_->Range(&zl, &zu);
      one_->Range(&ol, &ou);
      new_min = std::min(zl, ol);
      new_max = std::max(zu, ou);
    }
  }
  target_var_->SetRange(new_min, new_max);
}

std::string IfThenElseCt::DebugString() const {
  return absl::StrFormat("(%s ? %s : %s) == %s", condition_->DebugString(),
                         one_->DebugString(), zero_->DebugString(),
                         target_var_->DebugString());
}

void IfThenElseCt::Accept(ModelVisitor* visitor) const {}

IntExprEvaluatorElementCt::IntExprEvaluatorElementCt(
    Solver* s, Solver::Int64ToIntVar evaluator, int64_t range_start,
    int64_t range_end, IntVar* index, IntVar* target_var)
    : CastConstraint(s, target_var),
      index_(index),
      evaluator_(std::move(evaluator)),
      range_start_(range_start),
      range_end_(range_end),
      min_support_(-1),
      max_support_(-1) {}

void IntExprEvaluatorElementCt::Post() {
  Demon* delayed_propagate_demon = MakeDelayedConstraintDemon0(
      solver(), this, &IntExprEvaluatorElementCt::Propagate, "Propagate");
  for (int i = range_start_; i < range_end_; ++i) {
    IntVar* current_var = evaluator_(i);
    current_var->WhenRange(delayed_propagate_demon);
    Demon* update_demon = MakeConstraintDemon1(
        solver(), this, &IntExprEvaluatorElementCt::Update, "Update", i);
    current_var->WhenRange(update_demon);
  }
  index_->WhenRange(delayed_propagate_demon);
  Demon* update_expr_demon = MakeConstraintDemon0(
      solver(), this, &IntExprEvaluatorElementCt::UpdateExpr, "UpdateExpr");
  index_->WhenRange(update_expr_demon);
  Demon* update_var_demon = MakeConstraintDemon0(
      solver(), this, &IntExprEvaluatorElementCt::Propagate, "UpdateVar");

  target_var_->WhenRange(update_var_demon);
}

void IntExprEvaluatorElementCt::InitialPropagate() { Propagate(); }

void IntExprEvaluatorElementCt::Propagate() {
  const int64_t emin = std::max(range_start_, index_->Min());
  const int64_t emax = std::min<int64_t>(range_end_ - 1, index_->Max());
  const int64_t vmin = target_var_->Min();
  const int64_t vmax = target_var_->Max();
  if (emin == emax) {
    index_->SetValue(emin);  // in case it was reduced by the above min/max.
    evaluator_(emin)->SetRange(vmin, vmax);
  } else {
    int64_t nmin = emin;
    for (; nmin <= emax; nmin++) {
      // break if the intersection of
      // [evaluator_(nmin)->Min(), evaluator_(nmin)->Max()] and [vmin, vmax]
      // is non-empty.
      IntVar* nmin_var = evaluator_(nmin);
      if (nmin_var->Min() <= vmax && nmin_var->Max() >= vmin) break;
    }
    int64_t nmax = emax;
    for (; nmin <= nmax; nmax--) {
      // break if the intersection of
      // [evaluator_(nmin)->Min(), evaluator_(nmin)->Max()] and [vmin, vmax]
      // is non-empty.
      IntExpr* nmax_var = evaluator_(nmax);
      if (nmax_var->Min() <= vmax && nmax_var->Max() >= vmin) break;
    }
    index_->SetRange(nmin, nmax);
    if (nmin == nmax) {
      evaluator_(nmin)->SetRange(vmin, vmax);
    }
  }
  if (min_support_ == -1 || max_support_ == -1) {
    int min_support = -1;
    int max_support = -1;
    int64_t gmin = std::numeric_limits<int64_t>::max();
    int64_t gmax = std::numeric_limits<int64_t>::min();
    for (int i = index_->Min(); i <= index_->Max(); ++i) {
      IntExpr* var_i = evaluator_(i);
      const int64_t vmin = var_i->Min();
      if (vmin < gmin) {
        gmin = vmin;
      }
      const int64_t vmax = var_i->Max();
      if (vmax > gmax) {
        gmax = vmax;
      }
    }
    solver()->SaveAndSetValue(&min_support_, min_support);
    solver()->SaveAndSetValue(&max_support_, max_support);
    target_var_->SetRange(gmin, gmax);
  }
}

void IntExprEvaluatorElementCt::Update(int index) {
  if (index == min_support_ || index == max_support_) {
    solver()->SaveAndSetValue(&min_support_, -1);
    solver()->SaveAndSetValue(&max_support_, -1);
  }
}

void IntExprEvaluatorElementCt::UpdateExpr() {
  if (!index_->Contains(min_support_) || !index_->Contains(max_support_)) {
    solver()->SaveAndSetValue(&min_support_, -1);
    solver()->SaveAndSetValue(&max_support_, -1);
  }
}

namespace {
std::string StringifyEvaluatorBare(const Solver::Int64ToIntVar& evaluator,
                                   int64_t range_start, int64_t range_end) {
  std::string out;
  for (int64_t i = range_start; i < range_end; ++i) {
    if (i != range_start) {
      out += ", ";
    }
    out += absl::StrFormat("%d -> %s", i, evaluator(i)->DebugString());
  }
  return out;
}

std::string StringifyInt64ToIntVar(const Solver::Int64ToIntVar& evaluator,
                                   int64_t range_begin, int64_t range_end) {
  std::string out;
  if (range_end - range_begin > 10) {
    out = absl::StrFormat(
        "IntToIntVar(%s, ...%s)",
        StringifyEvaluatorBare(evaluator, range_begin, range_begin + 5),
        StringifyEvaluatorBare(evaluator, range_end - 5, range_end));
  } else {
    out = absl::StrFormat(
        "IntToIntVar(%s)",
        StringifyEvaluatorBare(evaluator, range_begin, range_end));
  }
  return out;
}
}  // namespace

std::string IntExprEvaluatorElementCt::DebugString() const {
  return StringifyInt64ToIntVar(evaluator_, range_start_, range_end_);
}

void IntExprEvaluatorElementCt::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kElementEqual, this);
  visitor->VisitIntegerVariableEvaluatorArgument(
      ModelVisitor::kEvaluatorArgument, evaluator_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument, index_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          target_var_);
  visitor->EndVisitConstraint(ModelVisitor::kElementEqual, this);
}

IntExprArrayElementCt::IntExprArrayElementCt(Solver* s,
                                             std::vector<IntVar*> vars,
                                             IntVar* index, IntVar* target_var)
    : IntExprEvaluatorElementCt(
          s, [this](int64_t idx) { return vars_[idx]; }, 0, vars.size(), index,
          target_var),
      vars_(std::move(vars)) {}

std::string IntExprArrayElementCt::DebugString() const {
  int64_t size = vars_.size();
  if (size > 10) {
    return absl::StrFormat(
        "IntExprArrayElement(var array of size %d, %s) == %s", size,
        index_->DebugString(), target_var_->DebugString());
  } else {
    return absl::StrFormat("IntExprArrayElement([%s], %s) == %s",
                           JoinDebugStringPtr(vars_, ", "),
                           index_->DebugString(), target_var_->DebugString());
  }
}

void IntExprArrayElementCt::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kElementEqual, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument, index_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          target_var_);
  visitor->EndVisitConstraint(ModelVisitor::kElementEqual, this);
}

IntExprArrayElementCstCt::IntExprArrayElementCstCt(
    Solver* s, const std::vector<IntVar*>& vars, IntVar* index, int64_t target)
    : Constraint(s),
      vars_(vars),
      index_(index),
      target_(target),
      demons_(vars.size()) {}

void IntExprArrayElementCstCt::Post() {
  for (int i = 0; i < vars_.size(); ++i) {
    demons_[i] = MakeConstraintDemon1(
        solver(), this, &IntExprArrayElementCstCt::Propagate, "Propagate", i);
    vars_[i]->WhenDomain(demons_[i]);
  }
  Demon* index_demon = MakeConstraintDemon0(
      solver(), this, &IntExprArrayElementCstCt::PropagateIndex,
      "PropagateIndex");
  index_->WhenBound(index_demon);
}

void IntExprArrayElementCstCt::InitialPropagate() {
  for (int i = 0; i < vars_.size(); ++i) {
    Propagate(i);
  }
  PropagateIndex();
}

void IntExprArrayElementCstCt::Propagate(int index) {
  if (!vars_[index]->Contains(target_)) {
    index_->RemoveValue(index);
    demons_[index]->inhibit(solver());
  }
}

void IntExprArrayElementCstCt::PropagateIndex() {
  if (index_->Bound()) {
    vars_[index_->Min()]->SetValue(target_);
  }
}

std::string IntExprArrayElementCstCt::DebugString() const {
  return absl::StrFormat("IntExprArrayElement([%s], %s) == %d",
                         JoinDebugStringPtr(vars_, ", "), index_->DebugString(),
                         target_);
}

void IntExprArrayElementCstCt::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kElementEqual, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument, index_);
  visitor->VisitIntegerArgument(ModelVisitor::kTargetArgument, target_);
  visitor->EndVisitConstraint(ModelVisitor::kElementEqual, this);
}

IntExprIndexOfCt::IntExprIndexOfCt(Solver* s, const std::vector<IntVar*>& vars,
                                   IntVar* index, int64_t target)
    : Constraint(s),
      vars_(vars),
      index_(index),
      target_(target),
      demons_(vars_.size()),
      index_iterator_(index->MakeHoleIterator(true)) {}

void IntExprIndexOfCt::Post() {
  for (int i = 0; i < vars_.size(); ++i) {
    demons_[i] = MakeConstraintDemon1(
        solver(), this, &IntExprIndexOfCt::Propagate, "Propagate", i);
    vars_[i]->WhenDomain(demons_[i]);
  }
  Demon* index_demon = MakeConstraintDemon0(
      solver(), this, &IntExprIndexOfCt::PropagateIndex, "PropagateIndex");
  index_->WhenDomain(index_demon);
}

void IntExprIndexOfCt::InitialPropagate() {
  for (int i = 0; i < vars_.size(); ++i) {
    if (!index_->Contains(i)) {
      vars_[i]->RemoveValue(target_);
    } else if (!vars_[i]->Contains(target_)) {
      index_->RemoveValue(i);
      demons_[i]->inhibit(solver());
    } else if (vars_[i]->Bound()) {
      index_->SetValue(i);
      demons_[i]->inhibit(solver());
    }
  }
}

void IntExprIndexOfCt::Propagate(int index) {
  if (!vars_[index]->Contains(target_)) {
    index_->RemoveValue(index);
    demons_[index]->inhibit(solver());
  } else if (vars_[index]->Bound()) {
    index_->SetValue(index);
  }
}

void IntExprIndexOfCt::PropagateIndex() {
  const int64_t oldmax = index_->OldMax();
  const int64_t vmin = index_->Min();
  const int64_t vmax = index_->Max();
  for (int64_t value = index_->OldMin(); value < vmin; ++value) {
    vars_[value]->RemoveValue(target_);
    demons_[value]->inhibit(solver());
  }
  for (const int64_t value : InitAndGetValues(index_iterator_)) {
    vars_[value]->RemoveValue(target_);
    demons_[value]->inhibit(solver());
  }
  for (int64_t value = vmax + 1; value <= oldmax; ++value) {
    vars_[value]->RemoveValue(target_);
    demons_[value]->inhibit(solver());
  }
  if (index_->Bound()) {
    vars_[index_->Min()]->SetValue(target_);
  }
}

std::string IntExprIndexOfCt::DebugString() const {
  return absl::StrFormat("IntExprIndexOf([%s], %s) == %d",
                         JoinDebugStringPtr(vars_, ", "), index_->DebugString(),
                         target_);
}

void IntExprIndexOfCt::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kIndexOf, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument, index_);
  visitor->VisitIntegerArgument(ModelVisitor::kTargetArgument, target_);
  visitor->EndVisitConstraint(ModelVisitor::kIndexOf, this);
}

}  // namespace operations_research

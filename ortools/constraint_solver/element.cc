// Copyright 2010-2022 Google LLC
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

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/util/range_minimum_query.h"
#include "ortools/util/string_array.h"

ABSL_FLAG(bool, cp_disable_element_cache, true,
          "If true, caching for IntElement is disabled.");

namespace operations_research {

// ----- IntExprElement -----
void LinkVarExpr(Solver* const s, IntExpr* const expr, IntVar* const var);

namespace {

template <class T>
class VectorLess {
 public:
  explicit VectorLess(const std::vector<T>* values) : values_(values) {}
  bool operator()(const T& x, const T& y) const {
    return (*values_)[x] < (*values_)[y];
  }

 private:
  const std::vector<T>* values_;
};

template <class T>
class VectorGreater {
 public:
  explicit VectorGreater(const std::vector<T>* values) : values_(values) {}
  bool operator()(const T& x, const T& y) const {
    return (*values_)[x] > (*values_)[y];
  }

 private:
  const std::vector<T>* values_;
};

// ----- BaseIntExprElement -----

class BaseIntExprElement : public BaseIntExpr {
 public:
  BaseIntExprElement(Solver* const s, IntVar* const e);
  ~BaseIntExprElement() override {}
  int64_t Min() const override;
  int64_t Max() const override;
  void Range(int64_t* mi, int64_t* ma) override;
  void SetMin(int64_t m) override;
  void SetMax(int64_t m) override;
  void SetRange(int64_t mi, int64_t ma) override;
  bool Bound() const override { return (expr_->Bound()); }
  // TODO(user) : improve me, the previous test is not always true
  void WhenRange(Demon* d) override { expr_->WhenRange(d); }

 protected:
  virtual int64_t ElementValue(int index) const = 0;
  virtual int64_t ExprMin() const = 0;
  virtual int64_t ExprMax() const = 0;

  IntVar* const expr_;

 private:
  void UpdateSupports() const;
  template <typename T>
  void UpdateElementIndexBounds(T check_value) {
    const int64_t emin = ExprMin();
    const int64_t emax = ExprMax();
    int64_t nmin = emin;
    int64_t value = ElementValue(nmin);
    while (nmin < emax && check_value(value)) {
      nmin++;
      value = ElementValue(nmin);
    }
    if (nmin == emax && check_value(value)) {
      solver()->Fail();
    }
    int64_t nmax = emax;
    value = ElementValue(nmax);
    while (nmax >= nmin && check_value(value)) {
      nmax--;
      value = ElementValue(nmax);
    }
    expr_->SetRange(nmin, nmax);
  }

  mutable int64_t min_;
  mutable int min_support_;
  mutable int64_t max_;
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

// ----- IntElementConstraint -----

// This constraint implements 'elem' == 'values'['index'].
// It scans the bounds of 'elem' to propagate on the domain of 'index'.
// It scans the domain of 'index' to compute the new bounds of 'elem'.
class IntElementConstraint : public CastConstraint {
 public:
  IntElementConstraint(Solver* const s, const std::vector<int64_t>& values,
                       IntVar* const index, IntVar* const elem)
      : CastConstraint(s, elem),
        values_(values),
        index_(index),
        index_iterator_(index_->MakeDomainIterator(true)) {
    CHECK(index != nullptr);
  }

  void Post() override {
    Demon* const d =
        solver()->MakeDelayedConstraintInitialPropagateCallback(this);
    index_->WhenDomain(d);
    target_var_->WhenRange(d);
  }

  void InitialPropagate() override {
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

  std::string DebugString() const override {
    return absl::StrFormat("IntElementConstraint(%s, %s, %s)",
                           absl::StrJoin(values_, ", "), index_->DebugString(),
                           target_var_->DebugString());
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kElementEqual, this);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument, values_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            index_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            target_var_);
    visitor->EndVisitConstraint(ModelVisitor::kElementEqual, this);
  }

 private:
  const std::vector<int64_t> values_;
  IntVar* const index_;
  IntVarIterator* const index_iterator_;
  std::vector<int64_t> to_remove_;
};

// ----- IntExprElement

IntVar* BuildDomainIntVar(Solver* const solver, std::vector<int64_t>* values);

class IntExprElement : public BaseIntExprElement {
 public:
  IntExprElement(Solver* const s, const std::vector<int64_t>& vals,
                 IntVar* const expr)
      : BaseIntExprElement(s, expr), values_(vals) {}

  ~IntExprElement() override {}

  std::string name() const override {
    const int size = values_.size();
    if (size > 10) {
      return absl::StrFormat("IntElement(array of size %d, %s)", size,
                             expr_->name());
    } else {
      return absl::StrFormat("IntElement(%s, %s)", absl::StrJoin(values_, ", "),
                             expr_->name());
    }
  }

  std::string DebugString() const override {
    const int size = values_.size();
    if (size > 10) {
      return absl::StrFormat("IntElement(array of size %d, %s)", size,
                             expr_->DebugString());
    } else {
      return absl::StrFormat("IntElement(%s, %s)", absl::StrJoin(values_, ", "),
                             expr_->DebugString());
    }
  }

  IntVar* CastToVar() override {
    Solver* const s = solver();
    IntVar* const var = s->MakeIntVar(values_);
    s->AddCastConstraint(
        s->RevAlloc(new IntElementConstraint(s, values_, expr_, var)), var,
        this);
    return var;
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kElement, this);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument, values_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            expr_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kElement, this);
  }

 protected:
  int64_t ElementValue(int index) const override {
    DCHECK_LT(index, values_.size());
    return values_[index];
  }
  int64_t ExprMin() const override {
    return std::max<int64_t>(0, expr_->Min());
  }
  int64_t ExprMax() const override {
    return values_.empty()
               ? 0
               : std::min<int64_t>(values_.size() - 1, expr_->Max());
  }

 private:
  const std::vector<int64_t> values_;
};

// ----- Range Minimum Query-based Element -----

class RangeMinimumQueryExprElement : public BaseIntExpr {
 public:
  RangeMinimumQueryExprElement(Solver* solver,
                               const std::vector<int64_t>& values,
                               IntVar* index);
  ~RangeMinimumQueryExprElement() override {}
  int64_t Min() const override;
  int64_t Max() const override;
  void Range(int64_t* mi, int64_t* ma) override;
  void SetMin(int64_t m) override;
  void SetMax(int64_t m) override;
  void SetRange(int64_t mi, int64_t ma) override;
  bool Bound() const override { return (index_->Bound()); }
  // TODO(user) : improve me, the previous test is not always true
  void WhenRange(Demon* d) override { index_->WhenRange(d); }
  IntVar* CastToVar() override {
    // TODO(user): Should we try to make holes in the domain of index_, as we
    // do here, or should we only propagate bounds as we do in
    // IncreasingIntExprElement ?
    IntVar* const var = solver()->MakeIntVar(min_rmq_.array());
    solver()->AddCastConstraint(solver()->RevAlloc(new IntElementConstraint(
                                    solver(), min_rmq_.array(), index_, var)),
                                var, this);
    return var;
  }
  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kElement, this);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument,
                                       min_rmq_.array());
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            index_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kElement, this);
  }

 private:
  int64_t IndexMin() const { return std::max<int64_t>(0, index_->Min()); }
  int64_t IndexMax() const {
    return std::min<int64_t>(min_rmq_.array().size() - 1, index_->Max());
  }

  IntVar* const index_;
  const RangeMinimumQuery<int64_t, std::less<int64_t>> min_rmq_;
  const RangeMinimumQuery<int64_t, std::greater<int64_t>> max_rmq_;
};

RangeMinimumQueryExprElement::RangeMinimumQueryExprElement(
    Solver* solver, const std::vector<int64_t>& values, IntVar* index)
    : BaseIntExpr(solver), index_(index), min_rmq_(values), max_rmq_(values) {
  CHECK(solver != nullptr);
  CHECK(index != nullptr);
}

int64_t RangeMinimumQueryExprElement::Min() const {
  return min_rmq_.GetMinimumFromRange(IndexMin(), IndexMax() + 1);
}

int64_t RangeMinimumQueryExprElement::Max() const {
  return max_rmq_.GetMinimumFromRange(IndexMin(), IndexMax() + 1);
}

void RangeMinimumQueryExprElement::Range(int64_t* mi, int64_t* ma) {
  const int64_t range_min = IndexMin();
  const int64_t range_max = IndexMax() + 1;
  *mi = min_rmq_.GetMinimumFromRange(range_min, range_max);
  *ma = max_rmq_.GetMinimumFromRange(range_min, range_max);
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

// ----- Increasing Element -----

class IncreasingIntExprElement : public BaseIntExpr {
 public:
  IncreasingIntExprElement(Solver* const s, const std::vector<int64_t>& values,
                           IntVar* const index);
  ~IncreasingIntExprElement() override {}

  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  void SetRange(int64_t mi, int64_t ma) override;
  bool Bound() const override { return (index_->Bound()); }
  // TODO(user) : improve me, the previous test is not always true
  std::string name() const override {
    return absl::StrFormat("IntElement(%s, %s)", absl::StrJoin(values_, ", "),
                           index_->name());
  }
  std::string DebugString() const override {
    return absl::StrFormat("IntElement(%s, %s)", absl::StrJoin(values_, ", "),
                           index_->DebugString());
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kElement, this);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument, values_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            index_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kElement, this);
  }

  void WhenRange(Demon* d) override { index_->WhenRange(d); }

  IntVar* CastToVar() override {
    Solver* const s = solver();
    IntVar* const var = s->MakeIntVar(values_);
    LinkVarExpr(s, this, var);
    return var;
  }

 private:
  const std::vector<int64_t> values_;
  IntVar* const index_;
};

IncreasingIntExprElement::IncreasingIntExprElement(
    Solver* const s, const std::vector<int64_t>& values, IntVar* const index)
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

// ----- Solver::MakeElement(int array, int var) -----
IntExpr* BuildElement(Solver* const solver, const std::vector<int64_t>& values,
                      IntVar* const index) {
  // Various checks.
  // Is array constant?
  if (IsArrayConstant(values, values[0])) {
    solver->AddConstraint(solver->MakeBetweenCt(index, 0, values.size() - 1));
    return solver->MakeIntConst(values[0]);
  }
  // Is array built with booleans only?
  // TODO(user): We could maintain the index of the first one.
  if (IsArrayBoolean(values)) {
    std::vector<int64_t> ones;
    int first_zero = -1;
    for (int i = 0; i < values.size(); ++i) {
      if (values[i] == 1) {
        ones.push_back(i);
      } else {
        first_zero = i;
      }
    }
    if (ones.size() == 1) {
      DCHECK_EQ(int64_t{1}, values[ones.back()]);
      solver->AddConstraint(solver->MakeBetweenCt(index, 0, values.size() - 1));
      return solver->MakeIsEqualCstVar(index, ones.back());
    } else if (ones.size() == values.size() - 1) {
      solver->AddConstraint(solver->MakeBetweenCt(index, 0, values.size() - 1));
      return solver->MakeIsDifferentCstVar(index, first_zero);
    } else if (ones.size() == ones.back() - ones.front() + 1) {  // contiguous.
      solver->AddConstraint(solver->MakeBetweenCt(index, 0, values.size() - 1));
      IntVar* const b = solver->MakeBoolVar("ContiguousBooleanElementVar");
      solver->AddConstraint(
          solver->MakeIsBetweenCt(index, ones.front(), ones.back(), b));
      return b;
    } else {
      IntVar* const b = solver->MakeBoolVar("NonContiguousBooleanElementVar");
      solver->AddConstraint(solver->MakeBetweenCt(index, 0, values.size() - 1));
      solver->AddConstraint(solver->MakeIsMemberCt(index, ones, b));
      return b;
    }
  }
  IntExpr* cache = nullptr;
  if (!absl::GetFlag(FLAGS_cp_disable_element_cache)) {
    cache = solver->Cache()->FindVarConstantArrayExpression(
        index, values, ModelCache::VAR_CONSTANT_ARRAY_ELEMENT);
  }
  if (cache != nullptr) {
    return cache;
  } else {
    IntExpr* result = nullptr;
    if (values.size() >= 2 && index->Min() == 0 && index->Max() == 1) {
      result = solver->MakeSum(solver->MakeProd(index, values[1] - values[0]),
                               values[0]);
    } else if (values.size() == 2 && index->Contains(0) && index->Contains(1)) {
      solver->AddConstraint(solver->MakeBetweenCt(index, 0, 1));
      result = solver->MakeSum(solver->MakeProd(index, values[1] - values[0]),
                               values[0]);
    } else if (IsIncreasingContiguous(values)) {
      result = solver->MakeSum(index, values[0]);
    } else if (IsIncreasing(values)) {
      result = solver->RegisterIntExpr(solver->RevAlloc(
          new IncreasingIntExprElement(solver, values, index)));
    } else {
      if (solver->parameters().use_element_rmq()) {
        result = solver->RegisterIntExpr(solver->RevAlloc(
            new RangeMinimumQueryExprElement(solver, values, index)));
      } else {
        result = solver->RegisterIntExpr(
            solver->RevAlloc(new IntExprElement(solver, values, index)));
      }
    }
    if (!absl::GetFlag(FLAGS_cp_disable_element_cache)) {
      solver->Cache()->InsertVarConstantArrayExpression(
          result, index, values, ModelCache::VAR_CONSTANT_ARRAY_ELEMENT);
    }
    return result;
  }
}
}  // namespace

IntExpr* Solver::MakeElement(const std::vector<int64_t>& values,
                             IntVar* const index) {
  DCHECK(index);
  DCHECK_EQ(this, index->solver());
  if (index->Bound()) {
    return MakeIntConst(values[index->Min()]);
  }
  return BuildElement(this, values, index);
}

IntExpr* Solver::MakeElement(const std::vector<int>& values,
                             IntVar* const index) {
  DCHECK(index);
  DCHECK_EQ(this, index->solver());
  if (index->Bound()) {
    return MakeIntConst(values[index->Min()]);
  }
  return BuildElement(this, ToInt64Vector(values), index);
}

// ----- IntExprFunctionElement -----

namespace {
class IntExprFunctionElement : public BaseIntExprElement {
 public:
  IntExprFunctionElement(Solver* const s, Solver::IndexEvaluator1 values,
                         IntVar* const e);
  ~IntExprFunctionElement() override;

  std::string name() const override {
    return absl::StrFormat("IntFunctionElement(%s)", expr_->name());
  }

  std::string DebugString() const override {
    return absl::StrFormat("IntFunctionElement(%s)", expr_->DebugString());
  }

  void Accept(ModelVisitor* const visitor) const override {
    // Warning: This will expand all values into a vector.
    visitor->BeginVisitIntegerExpression(ModelVisitor::kElement, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            expr_);
    visitor->VisitInt64ToInt64Extension(values_, expr_->Min(), expr_->Max());
    visitor->EndVisitIntegerExpression(ModelVisitor::kElement, this);
  }

 protected:
  int64_t ElementValue(int index) const override { return values_(index); }
  int64_t ExprMin() const override { return expr_->Min(); }
  int64_t ExprMax() const override { return expr_->Max(); }

 private:
  Solver::IndexEvaluator1 values_;
};

IntExprFunctionElement::IntExprFunctionElement(Solver* const s,
                                               Solver::IndexEvaluator1 values,
                                               IntVar* const e)
    : BaseIntExprElement(s, e), values_(std::move(values)) {
  CHECK(values_ != nullptr);
}

IntExprFunctionElement::~IntExprFunctionElement() {}

// ----- Increasing Element -----

class IncreasingIntExprFunctionElement : public BaseIntExpr {
 public:
  IncreasingIntExprFunctionElement(Solver* const s,
                                   Solver::IndexEvaluator1 values,
                                   IntVar* const index)
      : BaseIntExpr(s), values_(std::move(values)), index_(index) {
    DCHECK(values_ != nullptr);
    DCHECK(index);
    DCHECK(s);
  }

  ~IncreasingIntExprFunctionElement() override {}

  int64_t Min() const override { return values_(index_->Min()); }

  void SetMin(int64_t m) override {
    const int64_t index_min = index_->Min();
    const int64_t index_max = index_->Max();
    if (m > values_(index_max)) {
      solver()->Fail();
    }
    const int64_t new_index_min = FindNewIndexMin(index_min, index_max, m);
    index_->SetMin(new_index_min);
  }

  int64_t Max() const override { return values_(index_->Max()); }

  void SetMax(int64_t m) override {
    int64_t index_min = index_->Min();
    int64_t index_max = index_->Max();
    if (m < values_(index_min)) {
      solver()->Fail();
    }
    const int64_t new_index_max = FindNewIndexMax(index_min, index_max, m);
    index_->SetMax(new_index_max);
  }

  void SetRange(int64_t mi, int64_t ma) override {
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

  std::string name() const override {
    return absl::StrFormat("IncreasingIntExprFunctionElement(values, %s)",
                           index_->name());
  }

  std::string DebugString() const override {
    return absl::StrFormat("IncreasingIntExprFunctionElement(values, %s)",
                           index_->DebugString());
  }

  void WhenRange(Demon* d) override { index_->WhenRange(d); }

  void Accept(ModelVisitor* const visitor) const override {
    // Warning: This will expand all values into a vector.
    visitor->BeginVisitIntegerExpression(ModelVisitor::kElement, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            index_);
    if (index_->Min() == 0) {
      visitor->VisitInt64ToInt64AsArray(values_, ModelVisitor::kValuesArgument,
                                        index_->Max());
    } else {
      visitor->VisitInt64ToInt64Extension(values_, index_->Min(),
                                          index_->Max());
    }
    visitor->EndVisitIntegerExpression(ModelVisitor::kElement, this);
  }

 private:
  int64_t FindNewIndexMin(int64_t index_min, int64_t index_max, int64_t m) {
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

  int64_t FindNewIndexMax(int64_t index_min, int64_t index_max, int64_t m) {
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

  Solver::IndexEvaluator1 values_;
  IntVar* const index_;
};
}  // namespace

IntExpr* Solver::MakeElement(Solver::IndexEvaluator1 values,
                             IntVar* const index) {
  CHECK_EQ(this, index->solver());
  return RegisterIntExpr(
      RevAlloc(new IntExprFunctionElement(this, std::move(values), index)));
}

IntExpr* Solver::MakeMonotonicElement(Solver::IndexEvaluator1 values,
                                      bool increasing, IntVar* const index) {
  CHECK_EQ(this, index->solver());
  if (increasing) {
    return RegisterIntExpr(
        RevAlloc(new IncreasingIntExprFunctionElement(this, values, index)));
  } else {
    // You need to pass by copy such that opposite_value does not include a
    // dandling reference when leaving this scope.
    Solver::IndexEvaluator1 opposite_values = [values](int64_t i) {
      return -values(i);
    };
    return RegisterIntExpr(MakeOpposite(RevAlloc(
        new IncreasingIntExprFunctionElement(this, opposite_values, index))));
  }
}

// ----- IntIntExprFunctionElement -----

namespace {
class IntIntExprFunctionElement : public BaseIntExpr {
 public:
  IntIntExprFunctionElement(Solver* const s, Solver::IndexEvaluator2 values,
                            IntVar* const expr1, IntVar* const expr2);
  ~IntIntExprFunctionElement() override;
  std::string DebugString() const override {
    return absl::StrFormat("IntIntFunctionElement(%s,%s)",
                           expr1_->DebugString(), expr2_->DebugString());
  }
  int64_t Min() const override;
  int64_t Max() const override;
  void Range(int64_t* lower_bound, int64_t* upper_bound) override;
  void SetMin(int64_t lower_bound) override;
  void SetMax(int64_t upper_bound) override;
  void SetRange(int64_t lower_bound, int64_t upper_bound) override;
  bool Bound() const override { return expr1_->Bound() && expr2_->Bound(); }
  // TODO(user) : improve me, the previous test is not always true
  void WhenRange(Demon* d) override {
    expr1_->WhenRange(d);
    expr2_->WhenRange(d);
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kElement, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            expr1_);
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

 private:
  int64_t ElementValue(int index1, int index2) const {
    return values_(index1, index2);
  }
  void UpdateSupports() const;

  IntVar* const expr1_;
  IntVar* const expr2_;
  mutable int64_t min_;
  mutable int min_support1_;
  mutable int min_support2_;
  mutable int64_t max_;
  mutable int max_support1_;
  mutable int max_support2_;
  mutable bool initial_update_;
  Solver::IndexEvaluator2 values_;
  IntVarIterator* const expr1_iterator_;
  IntVarIterator* const expr2_iterator_;
};

IntIntExprFunctionElement::IntIntExprFunctionElement(
    Solver* const s, Solver::IndexEvaluator2 values, IntVar* const expr1,
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
      values_(std::move(values)),
      expr1_iterator_(expr1_->MakeDomainIterator(true)),
      expr2_iterator_(expr2_->MakeDomainIterator(true)) {
  CHECK(values_ != nullptr);
}

IntIntExprFunctionElement::~IntIntExprFunctionElement() {}

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
}  // namespace

IntExpr* Solver::MakeElement(Solver::IndexEvaluator2 values,
                             IntVar* const index1, IntVar* const index2) {
  CHECK_EQ(this, index1->solver());
  CHECK_EQ(this, index2->solver());
  return RegisterIntExpr(RevAlloc(
      new IntIntExprFunctionElement(this, std::move(values), index1, index2)));
}

// ---------- Generalized element ----------

// ----- IfThenElseCt -----

class IfThenElseCt : public CastConstraint {
 public:
  IfThenElseCt(Solver* const solver, IntVar* const condition,
               IntExpr* const one, IntExpr* const zero, IntVar* const target)
      : CastConstraint(solver, target),
        condition_(condition),
        zero_(zero),
        one_(one) {}

  ~IfThenElseCt() override {}

  void Post() override {
    Demon* const demon = solver()->MakeConstraintInitialPropagateCallback(this);
    condition_->WhenBound(demon);
    one_->WhenRange(demon);
    zero_->WhenRange(demon);
    target_var_->WhenRange(demon);
  }

  void InitialPropagate() override {
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

  std::string DebugString() const override {
    return absl::StrFormat("(%s ? %s : %s) == %s", condition_->DebugString(),
                           one_->DebugString(), zero_->DebugString(),
                           target_var_->DebugString());
  }

  void Accept(ModelVisitor* const visitor) const override {}

 private:
  IntVar* const condition_;
  IntExpr* const zero_;
  IntExpr* const one_;
};

// ----- IntExprEvaluatorElementCt -----

// This constraint implements evaluator(index) == var. It is delayed such
// that propagation only occurs when all variables have been touched.
// The range of the evaluator is [range_start, range_end).

namespace {
class IntExprEvaluatorElementCt : public CastConstraint {
 public:
  IntExprEvaluatorElementCt(Solver* const s, Solver::Int64ToIntVar evaluator,
                            int64_t range_start, int64_t range_end,
                            IntVar* const index, IntVar* const target_var);
  ~IntExprEvaluatorElementCt() override {}

  void Post() override;
  void InitialPropagate() override;

  void Propagate();
  void Update(int index);
  void UpdateExpr();

  std::string DebugString() const override;
  void Accept(ModelVisitor* const visitor) const override;

 protected:
  IntVar* const index_;

 private:
  const Solver::Int64ToIntVar evaluator_;
  const int64_t range_start_;
  const int64_t range_end_;
  int min_support_;
  int max_support_;
};

IntExprEvaluatorElementCt::IntExprEvaluatorElementCt(
    Solver* const s, Solver::Int64ToIntVar evaluator, int64_t range_start,
    int64_t range_end, IntVar* const index, IntVar* const target_var)
    : CastConstraint(s, target_var),
      index_(index),
      evaluator_(std::move(evaluator)),
      range_start_(range_start),
      range_end_(range_end),
      min_support_(-1),
      max_support_(-1) {}

void IntExprEvaluatorElementCt::Post() {
  Demon* const delayed_propagate_demon = MakeDelayedConstraintDemon0(
      solver(), this, &IntExprEvaluatorElementCt::Propagate, "Propagate");
  for (int i = range_start_; i < range_end_; ++i) {
    IntVar* const current_var = evaluator_(i);
    current_var->WhenRange(delayed_propagate_demon);
    Demon* const update_demon = MakeConstraintDemon1(
        solver(), this, &IntExprEvaluatorElementCt::Update, "Update", i);
    current_var->WhenRange(update_demon);
  }
  index_->WhenRange(delayed_propagate_demon);
  Demon* const update_expr_demon = MakeConstraintDemon0(
      solver(), this, &IntExprEvaluatorElementCt::UpdateExpr, "UpdateExpr");
  index_->WhenRange(update_expr_demon);
  Demon* const update_var_demon = MakeConstraintDemon0(
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
      IntVar* const nmin_var = evaluator_(nmin);
      if (nmin_var->Min() <= vmax && nmin_var->Max() >= vmin) break;
    }
    int64_t nmax = emax;
    for (; nmin <= nmax; nmax--) {
      // break if the intersection of
      // [evaluator_(nmin)->Min(), evaluator_(nmin)->Max()] and [vmin, vmax]
      // is non-empty.
      IntExpr* const nmax_var = evaluator_(nmax);
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
      IntExpr* const var_i = evaluator_(i);
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

void IntExprEvaluatorElementCt::Accept(ModelVisitor* const visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kElementEqual, this);
  visitor->VisitIntegerVariableEvaluatorArgument(
      ModelVisitor::kEvaluatorArgument, evaluator_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument, index_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          target_var_);
  visitor->EndVisitConstraint(ModelVisitor::kElementEqual, this);
}

// ----- IntExprArrayElementCt -----

// This constraint implements vars[index] == var. It is delayed such
// that propagation only occurs when all variables have been touched.

class IntExprArrayElementCt : public IntExprEvaluatorElementCt {
 public:
  IntExprArrayElementCt(Solver* const s, std::vector<IntVar*> vars,
                        IntVar* const index, IntVar* const target_var);

  std::string DebugString() const override;
  void Accept(ModelVisitor* const visitor) const override;

 private:
  const std::vector<IntVar*> vars_;
};

IntExprArrayElementCt::IntExprArrayElementCt(Solver* const s,
                                             std::vector<IntVar*> vars,
                                             IntVar* const index,
                                             IntVar* const target_var)
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

void IntExprArrayElementCt::Accept(ModelVisitor* const visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kElementEqual, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument, index_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          target_var_);
  visitor->EndVisitConstraint(ModelVisitor::kElementEqual, this);
}

// ----- IntExprArrayElementCstCt -----

// This constraint implements vars[index] == constant.

class IntExprArrayElementCstCt : public Constraint {
 public:
  IntExprArrayElementCstCt(Solver* const s, const std::vector<IntVar*>& vars,
                           IntVar* const index, int64_t target)
      : Constraint(s),
        vars_(vars),
        index_(index),
        target_(target),
        demons_(vars.size()) {}

  ~IntExprArrayElementCstCt() override {}

  void Post() override {
    for (int i = 0; i < vars_.size(); ++i) {
      demons_[i] = MakeConstraintDemon1(
          solver(), this, &IntExprArrayElementCstCt::Propagate, "Propagate", i);
      vars_[i]->WhenDomain(demons_[i]);
    }
    Demon* const index_demon = MakeConstraintDemon0(
        solver(), this, &IntExprArrayElementCstCt::PropagateIndex,
        "PropagateIndex");
    index_->WhenBound(index_demon);
  }

  void InitialPropagate() override {
    for (int i = 0; i < vars_.size(); ++i) {
      Propagate(i);
    }
    PropagateIndex();
  }

  void Propagate(int index) {
    if (!vars_[index]->Contains(target_)) {
      index_->RemoveValue(index);
      demons_[index]->inhibit(solver());
    }
  }

  void PropagateIndex() {
    if (index_->Bound()) {
      vars_[index_->Min()]->SetValue(target_);
    }
  }

  std::string DebugString() const override {
    return absl::StrFormat("IntExprArrayElement([%s], %s) == %d",
                           JoinDebugStringPtr(vars_, ", "),
                           index_->DebugString(), target_);
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kElementEqual, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            index_);
    visitor->VisitIntegerArgument(ModelVisitor::kTargetArgument, target_);
    visitor->EndVisitConstraint(ModelVisitor::kElementEqual, this);
  }

 private:
  const std::vector<IntVar*> vars_;
  IntVar* const index_;
  const int64_t target_;
  std::vector<Demon*> demons_;
};

// This constraint implements index == position(constant in vars).

class IntExprIndexOfCt : public Constraint {
 public:
  IntExprIndexOfCt(Solver* const s, const std::vector<IntVar*>& vars,
                   IntVar* const index, int64_t target)
      : Constraint(s),
        vars_(vars),
        index_(index),
        target_(target),
        demons_(vars_.size()),
        index_iterator_(index->MakeHoleIterator(true)) {}

  ~IntExprIndexOfCt() override {}

  void Post() override {
    for (int i = 0; i < vars_.size(); ++i) {
      demons_[i] = MakeConstraintDemon1(
          solver(), this, &IntExprIndexOfCt::Propagate, "Propagate", i);
      vars_[i]->WhenDomain(demons_[i]);
    }
    Demon* const index_demon = MakeConstraintDemon0(
        solver(), this, &IntExprIndexOfCt::PropagateIndex, "PropagateIndex");
    index_->WhenDomain(index_demon);
  }

  void InitialPropagate() override {
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

  void Propagate(int index) {
    if (!vars_[index]->Contains(target_)) {
      index_->RemoveValue(index);
      demons_[index]->inhibit(solver());
    } else if (vars_[index]->Bound()) {
      index_->SetValue(index);
    }
  }

  void PropagateIndex() {
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

  std::string DebugString() const override {
    return absl::StrFormat("IntExprIndexOf([%s], %s) == %d",
                           JoinDebugStringPtr(vars_, ", "),
                           index_->DebugString(), target_);
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kIndexOf, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            index_);
    visitor->VisitIntegerArgument(ModelVisitor::kTargetArgument, target_);
    visitor->EndVisitConstraint(ModelVisitor::kIndexOf, this);
  }

 private:
  const std::vector<IntVar*> vars_;
  IntVar* const index_;
  const int64_t target_;
  std::vector<Demon*> demons_;
  IntVarIterator* const index_iterator_;
};

// Factory helper.

Constraint* MakeElementEqualityFunc(Solver* const solver,
                                    const std::vector<int64_t>& vals,
                                    IntVar* const index, IntVar* const target) {
  if (index->Bound()) {
    const int64_t val = index->Min();
    if (val < 0 || val >= vals.size()) {
      return solver->MakeFalseConstraint();
    } else {
      return solver->MakeEquality(target, vals[val]);
    }
  } else {
    if (IsIncreasingContiguous(vals)) {
      return solver->MakeEquality(target, solver->MakeSum(index, vals[0]));
    } else {
      return solver->RevAlloc(
          new IntElementConstraint(solver, vals, index, target));
    }
  }
}
}  // namespace

Constraint* Solver::MakeIfThenElseCt(IntVar* const condition,
                                     IntExpr* const then_expr,
                                     IntExpr* const else_expr,
                                     IntVar* const target_var) {
  return RevAlloc(
      new IfThenElseCt(this, condition, then_expr, else_expr, target_var));
}

IntExpr* Solver::MakeElement(const std::vector<IntVar*>& vars,
                             IntVar* const index) {
  if (index->Bound()) {
    return vars[index->Min()];
  }
  const int size = vars.size();
  if (AreAllBound(vars)) {
    std::vector<int64_t> values(size);
    for (int i = 0; i < size; ++i) {
      values[i] = vars[i]->Value();
    }
    return MakeElement(values, index);
  }
  if (index->Size() == 2 && index->Min() + 1 == index->Max() &&
      index->Min() >= 0 && index->Max() < vars.size()) {
    // Let's get the index between 0 and 1.
    IntVar* const scaled_index = MakeSum(index, -index->Min())->Var();
    IntVar* const zero = vars[index->Min()];
    IntVar* const one = vars[index->Max()];
    const std::string name = absl::StrFormat(
        "ElementVar([%s], %s)", JoinNamePtr(vars, ", "), index->name());
    IntVar* const target = MakeIntVar(std::min(zero->Min(), one->Min()),
                                      std::max(zero->Max(), one->Max()), name);
    AddConstraint(
        RevAlloc(new IfThenElseCt(this, scaled_index, one, zero, target)));
    return target;
  }
  int64_t emin = std::numeric_limits<int64_t>::max();
  int64_t emax = std::numeric_limits<int64_t>::min();
  std::unique_ptr<IntVarIterator> iterator(index->MakeDomainIterator(false));
  for (const int64_t index_value : InitAndGetValues(iterator.get())) {
    if (index_value >= 0 && index_value < size) {
      emin = std::min(emin, vars[index_value]->Min());
      emax = std::max(emax, vars[index_value]->Max());
    }
  }
  const std::string vname =
      size > 10 ? absl::StrFormat("ElementVar(var array of size %d, %s)", size,
                                  index->DebugString())
                : absl::StrFormat("ElementVar([%s], %s)",
                                  JoinNamePtr(vars, ", "), index->name());
  IntVar* const element_var = MakeIntVar(emin, emax, vname);
  AddConstraint(
      RevAlloc(new IntExprArrayElementCt(this, vars, index, element_var)));
  return element_var;
}

IntExpr* Solver::MakeElement(Int64ToIntVar vars, int64_t range_start,
                             int64_t range_end, IntVar* argument) {
  const std::string index_name =
      !argument->name().empty() ? argument->name() : argument->DebugString();
  const std::string vname = absl::StrFormat(
      "ElementVar(%s, %s)",
      StringifyInt64ToIntVar(vars, range_start, range_end), index_name);
  IntVar* const element_var =
      MakeIntVar(std::numeric_limits<int64_t>::min(),
                 std::numeric_limits<int64_t>::max(), vname);
  IntExprEvaluatorElementCt* evaluation_ct = new IntExprEvaluatorElementCt(
      this, std::move(vars), range_start, range_end, argument, element_var);
  AddConstraint(RevAlloc(evaluation_ct));
  evaluation_ct->Propagate();
  return element_var;
}

Constraint* Solver::MakeElementEquality(const std::vector<int64_t>& vals,
                                        IntVar* const index,
                                        IntVar* const target) {
  return MakeElementEqualityFunc(this, vals, index, target);
}

Constraint* Solver::MakeElementEquality(const std::vector<int>& vals,
                                        IntVar* const index,
                                        IntVar* const target) {
  return MakeElementEqualityFunc(this, ToInt64Vector(vals), index, target);
}

Constraint* Solver::MakeElementEquality(const std::vector<IntVar*>& vars,
                                        IntVar* const index,
                                        IntVar* const target) {
  if (AreAllBound(vars)) {
    std::vector<int64_t> values(vars.size());
    for (int i = 0; i < vars.size(); ++i) {
      values[i] = vars[i]->Value();
    }
    return MakeElementEquality(values, index, target);
  }
  if (index->Bound()) {
    const int64_t val = index->Min();
    if (val < 0 || val >= vars.size()) {
      return MakeFalseConstraint();
    } else {
      return MakeEquality(target, vars[val]);
    }
  } else {
    if (target->Bound()) {
      return RevAlloc(
          new IntExprArrayElementCstCt(this, vars, index, target->Min()));
    } else {
      return RevAlloc(new IntExprArrayElementCt(this, vars, index, target));
    }
  }
}

Constraint* Solver::MakeElementEquality(const std::vector<IntVar*>& vars,
                                        IntVar* const index, int64_t target) {
  if (AreAllBound(vars)) {
    std::vector<int> valid_indices;
    for (int i = 0; i < vars.size(); ++i) {
      if (vars[i]->Value() == target) {
        valid_indices.push_back(i);
      }
    }
    return MakeMemberCt(index, valid_indices);
  }
  if (index->Bound()) {
    const int64_t pos = index->Min();
    if (pos >= 0 && pos < vars.size()) {
      IntVar* const var = vars[pos];
      return MakeEquality(var, target);
    } else {
      return MakeFalseConstraint();
    }
  } else {
    return RevAlloc(new IntExprArrayElementCstCt(this, vars, index, target));
  }
}

Constraint* Solver::MakeIndexOfConstraint(const std::vector<IntVar*>& vars,
                                          IntVar* const index, int64_t target) {
  if (index->Bound()) {
    const int64_t pos = index->Min();
    if (pos >= 0 && pos < vars.size()) {
      IntVar* const var = vars[pos];
      return MakeEquality(var, target);
    } else {
      return MakeFalseConstraint();
    }
  } else {
    return RevAlloc(new IntExprIndexOfCt(this, vars, index, target));
  }
}

IntExpr* Solver::MakeIndexExpression(const std::vector<IntVar*>& vars,
                                     int64_t value) {
  IntExpr* const cache = model_cache_->FindVarArrayConstantExpression(
      vars, value, ModelCache::VAR_ARRAY_CONSTANT_INDEX);
  if (cache != nullptr) {
    return cache->Var();
  } else {
    const std::string name =
        absl::StrFormat("Index(%s, %d)", JoinNamePtr(vars, ", "), value);
    IntVar* const index = MakeIntVar(0, vars.size() - 1, name);
    AddConstraint(MakeIndexOfConstraint(vars, index, value));
    model_cache_->InsertVarArrayConstantExpression(
        index, vars, value, ModelCache::VAR_ARRAY_CONSTANT_INDEX);
    return index;
  }
}
}  // namespace operations_research

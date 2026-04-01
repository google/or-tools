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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_ELEMENT_H_
#define ORTOOLS_CONSTRAINT_SOLVER_ELEMENT_H_

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/expressions.h"
#include "ortools/util/range_minimum_query.h"

namespace operations_research {

void LinkVarExpr(Solver* s, IntExpr* expr, IntVar* var);

class BaseIntExprElement : public BaseIntExpr {
 public:
  BaseIntExprElement(Solver* s, IntVar* e);
  ~BaseIntExprElement() override {}
  int64_t Min() const override;
  int64_t Max() const override;
  void Range(int64_t* mi, int64_t* ma) override;
  void SetMin(int64_t m) override;
  void SetMax(int64_t m) override;
  void SetRange(int64_t mi, int64_t ma) override;
  bool Bound() const override { return (expr_->Bound()); }
  void WhenRange(Demon* d) override;

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

class IntElementConstraint : public CastConstraint {
 public:
  IntElementConstraint(Solver* s, const std::vector<int64_t>& values,
                       IntVar* index, IntVar* elem);

  void Post() override;

  void InitialPropagate() override;

  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  const std::vector<int64_t> values_;
  IntVar* const index_;
  IntVarIterator* const index_iterator_;
  std::vector<int64_t> to_remove_;
};

class IntExprElement : public BaseIntExprElement {
 public:
  IntExprElement(Solver* s, const std::vector<int64_t>& vals, IntVar* expr)
      : BaseIntExprElement(s, expr), values_(vals) {}

  ~IntExprElement() override {}

  std::string name() const override;

  std::string DebugString() const override;

  IntVar* CastToVar() override;

  void Accept(ModelVisitor* visitor) const override;

 protected:
  int64_t ElementValue(int index) const override;
  int64_t ExprMin() const override;
  int64_t ExprMax() const override;

 private:
  const std::vector<int64_t> values_;
};

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
  void WhenRange(Demon* d) override;
  IntVar* CastToVar() override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  int64_t IndexMin() const { return std::max<int64_t>(0, index_->Min()); }
  int64_t IndexMax() const {
    return std::min<int64_t>(min_rmq_.array().size() - 1, index_->Max());
  }

  IntVar* const index_;
  const RangeMinimumQuery<int64_t, std::less<int64_t>> min_rmq_;
  const RangeMinimumQuery<int64_t, std::greater<int64_t>> max_rmq_;
};

class IncreasingIntExprElement : public BaseIntExpr {
 public:
  IncreasingIntExprElement(Solver* s, const std::vector<int64_t>& values,
                           IntVar* index);
  ~IncreasingIntExprElement() override {}

  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  void SetRange(int64_t mi, int64_t ma) override;
  bool Bound() const override { return (index_->Bound()); }
  std::string name() const override;
  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

  void WhenRange(Demon* d) override;

  IntVar* CastToVar() override;

 private:
  const std::vector<int64_t> values_;
  IntVar* const index_;
};

class IntExprFunctionElement : public BaseIntExprElement {
 public:
  IntExprFunctionElement(Solver* s, Solver::IndexEvaluator1 values, IntVar* e);
  ~IntExprFunctionElement() override;

  std::string name() const override;

  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 protected:
  int64_t ElementValue(int index) const override;
  int64_t ExprMin() const override;
  int64_t ExprMax() const override;

 private:
  Solver::IndexEvaluator1 values_;
};

class IncreasingIntExprFunctionElement : public BaseIntExpr {
 public:
  IncreasingIntExprFunctionElement(Solver* s, Solver::IndexEvaluator1 values,
                                   IntVar* index);

  ~IncreasingIntExprFunctionElement() override;

  int64_t Min() const override;

  void SetMin(int64_t m) override;

  int64_t Max() const override;

  void SetMax(int64_t m) override;

  void SetRange(int64_t mi, int64_t ma) override;

  std::string name() const override;

  std::string DebugString() const override;

  void WhenRange(Demon* d) override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  int64_t FindNewIndexMin(int64_t index_min, int64_t index_max, int64_t m);

  int64_t FindNewIndexMax(int64_t index_min, int64_t index_max, int64_t m);

  Solver::IndexEvaluator1 values_;
  IntVar* const index_;
};

class IntIntExprFunctionElement : public BaseIntExpr {
 public:
  IntIntExprFunctionElement(Solver* s, Solver::IndexEvaluator2 values,
                            IntVar* expr1, IntVar* expr2);
  ~IntIntExprFunctionElement() override;
  std::string DebugString() const override;
  int64_t Min() const override;
  int64_t Max() const override;
  void Range(int64_t* lower_bound, int64_t* upper_bound) override;
  void SetMin(int64_t lower_bound) override;
  void SetMax(int64_t upper_bound) override;
  void SetRange(int64_t lower_bound, int64_t upper_bound) override;
  bool Bound() const override { return expr1_->Bound() && expr2_->Bound(); }
  void WhenRange(Demon* d) override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  int64_t ElementValue(int index1, int index2) const;
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

class IfThenElseCt : public CastConstraint {
 public:
  IfThenElseCt(Solver* solver, IntVar* condition, IntExpr* one, IntExpr* zero,
               IntVar* target)
      : CastConstraint(solver, target),
        condition_(condition),
        zero_(zero),
        one_(one) {}

  ~IfThenElseCt() override;

  void Post() override;

  void InitialPropagate() override;

  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  IntVar* const condition_;
  IntExpr* const zero_;
  IntExpr* const one_;
};

class IntExprEvaluatorElementCt : public CastConstraint {
 public:
  IntExprEvaluatorElementCt(Solver* s, Solver::Int64ToIntVar evaluator,
                            int64_t range_start, int64_t range_end,
                            IntVar* index, IntVar* target_var);
  ~IntExprEvaluatorElementCt() override {}

  void Post() override;
  void InitialPropagate() override;

  void Propagate();
  void Update(int index);
  void UpdateExpr();

  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 protected:
  IntVar* const index_;

 private:
  const Solver::Int64ToIntVar evaluator_;
  const int64_t range_start_;
  const int64_t range_end_;
  int min_support_;
  int max_support_;
};

class IntExprArrayElementCt : public IntExprEvaluatorElementCt {
 public:
  IntExprArrayElementCt(Solver* s, std::vector<IntVar*> vars, IntVar* index,
                        IntVar* target_var);

  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  const std::vector<IntVar*> vars_;
};

class IntExprArrayElementCstCt : public Constraint {
 public:
  IntExprArrayElementCstCt(Solver* s, const std::vector<IntVar*>& vars,
                           IntVar* index, int64_t target);

  ~IntExprArrayElementCstCt() override {}

  void Post() override;

  void InitialPropagate() override;

  void Propagate(int index);

  void PropagateIndex();

  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  const std::vector<IntVar*> vars_;
  IntVar* const index_;
  const int64_t target_;
  std::vector<Demon*> demons_;
};

class IntExprIndexOfCt : public Constraint {
 public:
  IntExprIndexOfCt(Solver* s, const std::vector<IntVar*>& vars, IntVar* index,
                   int64_t target);

  ~IntExprIndexOfCt() override {}

  void Post() override;

  void InitialPropagate() override;

  void Propagate(int index);

  void PropagateIndex();

  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  const std::vector<IntVar*> vars_;
  IntVar* const index_;
  const int64_t target_;
  std::vector<Demon*> demons_;
  IntVarIterator* const index_iterator_;
};

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_ELEMENT_H_

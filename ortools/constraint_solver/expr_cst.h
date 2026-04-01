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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_EXPR_CST_H_
#define ORTOOLS_CONSTRAINT_SOLVER_EXPR_CST_H_

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/expressions.h"
#include "ortools/constraint_solver/variables.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {

class EqualityExprCst : public Constraint {
 public:
  EqualityExprCst(Solver* s, IntExpr* e, int64_t v);
  ~EqualityExprCst() override;
  void Post() override;
  void InitialPropagate() override;
  IntVar* Var() override;
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const expr_;
  int64_t value_;
};

class GreaterEqExprCst : public Constraint {
 public:
  GreaterEqExprCst(Solver* s, IntExpr* e, int64_t v);
  ~GreaterEqExprCst() override;
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  IntVar* Var() override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const expr_;
  int64_t value_;
  Demon* demon_;
};

class LessEqExprCst : public Constraint {
 public:
  LessEqExprCst(Solver* s, IntExpr* e, int64_t v);
  ~LessEqExprCst() override;
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  IntVar* Var() override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const expr_;
  int64_t value_;
  Demon* demon_;
};

class DiffCst : public Constraint {
 public:
  DiffCst(Solver* s, IntVar* var, int64_t value);
  ~DiffCst() override;
  void Post() override;
  void InitialPropagate() override;
  void BoundPropagate();
  std::string DebugString() const override;
  IntVar* Var() override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  bool HasLargeDomain(IntVar* var);

  IntVar* const var_;
  int64_t value_;
  Demon* demon_;
};

class IsEqualCstCt : public CastConstraint {
 public:
  IsEqualCstCt(Solver* s, IntVar* v, int64_t c, IntVar* b);
  ~IsEqualCstCt() override {}
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntVar* const var_;
  int64_t cst_;
  Demon* demon_;
};

class IsDiffCstCt : public CastConstraint {
 public:
  IsDiffCstCt(Solver* s, IntVar* v, int64_t c, IntVar* b);
  ~IsDiffCstCt() override {}
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntVar* const var_;
  int64_t cst_;
  Demon* demon_;
};

class IsGreaterEqualCstCt : public CastConstraint {
 public:
  IsGreaterEqualCstCt(Solver* s, IntExpr* v, int64_t c, IntVar* b);
  ~IsGreaterEqualCstCt() override {}
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const expr_;
  int64_t cst_;
  Demon* demon_;
};

class IsLessEqualCstCt : public CastConstraint {
 public:
  IsLessEqualCstCt(Solver* s, IntExpr* v, int64_t c, IntVar* b);
  ~IsLessEqualCstCt() override {}
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const expr_;
  int64_t cst_;
  Demon* demon_;
};

class BetweenCt : public Constraint {
 public:
  BetweenCt(Solver* s, IntExpr* v, int64_t l, int64_t u);
  ~BetweenCt() override {}
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const expr_;
  int64_t min_;
  int64_t max_;
  Demon* demon_;
};

class NotBetweenCt : public Constraint {
 public:
  NotBetweenCt(Solver* s, IntExpr* v, int64_t l, int64_t u);
  ~NotBetweenCt() override {}
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const expr_;
  int64_t min_;
  int64_t max_;
  Demon* demon_;
};

class IsBetweenCt : public Constraint {
 public:
  IsBetweenCt(Solver* s, IntExpr* e, int64_t l, int64_t u, IntVar* b);
  ~IsBetweenCt() override {}
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const expr_;
  int64_t min_;
  int64_t max_;
  IntVar* const boolvar_;
  Demon* demon_;
};

class MemberCt : public Constraint {
 public:
  MemberCt(Solver* s, IntVar* v, const std::vector<int64_t>& sorted_values);
  ~MemberCt() override {}
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntVar* const var_;
  const std::vector<int64_t> values_;
};

class NotMemberCt : public Constraint {
 public:
  NotMemberCt(Solver* s, IntVar* v, const std::vector<int64_t>& sorted_values);
  ~NotMemberCt() override {}
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntVar* const var_;
  const std::vector<int64_t> values_;
};

class IsMemberCt : public Constraint {
 public:
  IsMemberCt(Solver* s, IntVar* v, const std::vector<int64_t>& sorted_values,
             IntVar* b);
  ~IsMemberCt() override {}
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  void VarDomain();
  void TargetBound();

  IntVar* const var_;
  absl::flat_hash_set<int64_t> values_as_set_;
  std::vector<int64_t> values_;
  IntVar* const boolvar_;
  int support_;
  Demon* demon_;
  IntVarIterator* const domain_;
  int64_t neg_support_;
};

class SortedDisjointForbiddenIntervalsConstraint : public Constraint {
 public:
  SortedDisjointForbiddenIntervalsConstraint(
      Solver* solver, IntVar* var, SortedDisjointIntervalList intervals);
  ~SortedDisjointForbiddenIntervalsConstraint() override {}
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntVar* const var_;
  const SortedDisjointIntervalList intervals_;
};

int64_t ExtractExprProductCoeff(IntExpr** expr);

template <class T>
Constraint* BuildIsMemberCt(Solver* solver, IntExpr* expr,
                            const std::vector<T>& values, IntVar* boolvar) {
  if (expr->IsVar()) {
    IntVar* sub = nullptr;
    int64_t coef = 1;
    if (IsVarProduct(expr, &sub, &coef) && coef != 0 && coef != 1) {
      std::vector<int64_t> new_values;
      new_values.reserve(values.size());
      for (const int64_t value : values) {
        if (value % coef == 0) {
          new_values.push_back(value / coef);
        }
      }
      return BuildIsMemberCt(solver, sub, new_values, boolvar);
    }

  } else {
    IntExpr* sub = nullptr;
    int64_t coef = 1;
    if (IsExprProduct(expr, &sub, &coef) && coef != 0 && coef != 1) {
      std::vector<int64_t> new_values;
      new_values.reserve(values.size());
      for (const int64_t value : values) {
        if (value % coef == 0) {
          new_values.push_back(value / coef);
        }
      }
      return BuildIsMemberCt(solver, sub, new_values, boolvar);
    }
  }

  std::set<T> set_of_values(values.begin(), values.end());
  std::vector<int64_t> filtered_values;
  bool all_values = false;
  if (expr->IsVar()) {
    IntVar* const var = expr->Var();
    for (const T value : set_of_values) {
      if (var->Contains(value)) {
        filtered_values.push_back(value);
      }
    }
    all_values = (filtered_values.size() == var->Size());
  } else {
    int64_t emin = 0;
    int64_t emax = 0;
    expr->Range(&emin, &emax);
    for (const T value : set_of_values) {
      if (value >= emin && value <= emax) {
        filtered_values.push_back(value);
      }
    }
    all_values = (filtered_values.size() == emax - emin + 1);
  }
  if (filtered_values.empty()) {
    return solver->MakeEquality(boolvar, Zero());
  } else if (all_values) {
    return solver->MakeEquality(boolvar, 1);
  } else if (filtered_values.size() == 1) {
    return solver->MakeIsEqualCstCt(expr, filtered_values.back(), boolvar);
  } else if (filtered_values.back() ==
             filtered_values.front() + filtered_values.size() - 1) {
    // Contiguous
    return solver->MakeIsBetweenCt(expr, filtered_values.front(),
                                   filtered_values.back(), boolvar);
  } else {
    return solver->RevAlloc(
        new IsMemberCt(solver, expr->Var(), filtered_values, boolvar));
  }
}

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_EXPR_CST_H_

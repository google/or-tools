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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_RANGE_CST_H_
#define ORTOOLS_CONSTRAINT_SOLVER_RANGE_CST_H_

#include <string>

#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {

//-----------------------------------------------------------------------------
// RangeEquality

class RangeEquality : public Constraint {
 public:
  RangeEquality(Solver* s, IntExpr* l, IntExpr* r);
  ~RangeEquality() override = default;
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  IntVar* Var() override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const left_;
  IntExpr* const right_;
};

//-----------------------------------------------------------------------------
// RangeLessOrEqual

class RangeLessOrEqual : public Constraint {
 public:
  RangeLessOrEqual(Solver* s, IntExpr* l, IntExpr* r);
  ~RangeLessOrEqual() override = default;
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  IntVar* Var() override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const left_;
  IntExpr* const right_;
  Demon* demon_;
};

//-----------------------------------------------------------------------------
// RangeLess

class RangeLess : public Constraint {
 public:
  RangeLess(Solver* s, IntExpr* l, IntExpr* r);
  ~RangeLess() override = default;
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  IntVar* Var() override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const left_;
  IntExpr* const right_;
  Demon* demon_;
};

//-----------------------------------------------------------------------------
// DiffVar

class DiffVar : public Constraint {
 public:
  DiffVar(Solver* s, IntVar* l, IntVar* r);
  ~DiffVar() override = default;
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  IntVar* Var() override;
  void LeftBound();
  void RightBound();

  void Accept(ModelVisitor* visitor) const override;

 private:
  IntVar* const left_;
  IntVar* const right_;
};

// --------------------- Reified API -------------------
// A reified API transforms a constraint into a status variables.
// For example x == y is transformed into IsEqual(x, y, b) where
// b is a boolean variable which is true if and only if x is equal to b.

// IsEqualCt

class IsEqualCt : public CastConstraint {
 public:
  IsEqualCt(Solver* s, IntExpr* l, IntExpr* r, IntVar* b);
  ~IsEqualCt() override = default;
  void Post() override;
  void InitialPropagate() override;
  void PropagateTarget();
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const left_;
  IntExpr* const right_;
  Demon* range_demon_;
};

// IsDifferentCt

class IsDifferentCt : public CastConstraint {
 public:
  IsDifferentCt(Solver* s, IntExpr* l, IntExpr* r, IntVar* b);
  ~IsDifferentCt() override = default;
  void Post() override;
  void InitialPropagate() override;
  void PropagateTarget();
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const left_;
  IntExpr* const right_;
  Demon* range_demon_;
};

class IsLessOrEqualCt : public CastConstraint {
 public:
  IsLessOrEqualCt(Solver* s, IntExpr* l, IntExpr* r, IntVar* b);
  ~IsLessOrEqualCt() override = default;
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const left_;
  IntExpr* const right_;
  Demon* demon_;
};

class IsLessCt : public CastConstraint {
 public:
  IsLessCt(Solver* s, IntExpr* l, IntExpr* r, IntVar* b);
  ~IsLessCt() override = default;
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const left_;
  IntExpr* const right_;
  Demon* demon_;
};

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_RANGE_CST_H_

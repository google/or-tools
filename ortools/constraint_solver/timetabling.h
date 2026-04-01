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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_TIMETABLING_H_
#define ORTOOLS_CONSTRAINT_SOLVER_TIMETABLING_H_

#include <cstdint>
#include <string>

#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {

// ----- interval <unary relation> date -----

class IntervalUnaryRelation : public Constraint {
 public:
  IntervalUnaryRelation(Solver* s, IntervalVar* t, int64_t d,
                        Solver::UnaryIntervalRelation rel);
  ~IntervalUnaryRelation() override;

  void Post() override;

  void InitialPropagate() override;

  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  IntervalVar* const t_;
  const int64_t d_;
  const Solver::UnaryIntervalRelation rel_;
};

// ----- interval <binary relation> interval -----

class IntervalBinaryRelation : public Constraint {
 public:
  IntervalBinaryRelation(Solver* s, IntervalVar* t1, IntervalVar* t2,
                         Solver::BinaryIntervalRelation rel, int64_t delay);
  ~IntervalBinaryRelation() override;

  void Post() override;

  void InitialPropagate() override;

  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  IntervalVar* const t1_;
  IntervalVar* const t2_;
  const Solver::BinaryIntervalRelation rel_;
  const int64_t delay_;
};

// ----- activity a before activity b or activity b before activity a -----

class TemporalDisjunction : public Constraint {
 public:
  enum State { ONE_BEFORE_TWO, TWO_BEFORE_ONE, UNDECIDED };

  TemporalDisjunction(Solver* s, IntervalVar* t1, IntervalVar* t2, IntVar* alt);
  ~TemporalDisjunction() override;

  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;

  void RangeDemon1();
  void RangeDemon2();
  void RangeAlt();
  void Decide(State s);
  void TryToDecide();

  void Accept(ModelVisitor* visitor) const override;

 private:
  IntervalVar* const t1_;
  IntervalVar* const t2_;
  IntVar* const alt_;
  State state_;
};

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_TIMETABLING_H_

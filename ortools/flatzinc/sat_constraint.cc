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

#include "ortools/flatzinc/sat_constraint.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/int_type.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/base/map_util.h"
#include "ortools/base/hash.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/flatzinc/logging.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"

// #define SAT_DEBUG
#if defined(SAT_DEBUG)
#define SATDLOG FZDLOG
#else
#define SATDLOG \
  if (false) FZDLOG
#endif

namespace operations_research {
// Constraint that tight together boolean variables in the CP solver to sat
// variables and clauses.
class SatPropagator : public Constraint {
 public:
  explicit SatPropagator(Solver* solver)
      : Constraint(solver), sat_decision_level_(0) {}

  ~SatPropagator() override {}

  bool ExpressionIsBoolean(IntExpr* expr) const {
    IntVar* expr_var = nullptr;
    bool expr_negated = false;
    return solver()->IsBooleanVar(expr, &expr_var, &expr_negated);
  }

  bool AllVariablesAreBoolean(const std::vector<IntVar*>& vars) const {
    for (int i = 0; i < vars.size(); ++i) {
      if (!ExpressionIsBoolean(vars[i])) {
        return false;
      }
    }
    return true;
  }

  // Converts a constraint solver literal to the SatSolver representation.
  sat::Literal GetOrCreateLiteral(IntExpr* expr) {
    IntVar* expr_var = nullptr;
    bool expr_negated = false;
    CHECK(solver()->IsBooleanVar(expr, &expr_var, &expr_negated));
    SATDLOG << "  - SAT: Parse " << expr->DebugString() << " to "
            << expr_var->DebugString() << "/" << expr_negated << FZENDL;
    if (ContainsKey(indices_, expr_var)) {
      return sat::Literal(indices_[expr_var], !expr_negated);
    }
    const sat::BooleanVariable var = sat_.NewBooleanVariable();
    vars_.push_back(expr_var);
    indices_[expr_var] = var;
    const sat::Literal literal(var, !expr_negated);
    SATDLOG << "    - created var = " << var.value()
            << ", literal = " << literal.SignedValue() << FZENDL;
    return literal;
  }

  // Queries the sat solver for all newly assigned literals, and propagates
  // the values to the CP variables.
  void QueryAssignedSatLiterals(int from_index) {
    const int to_index = sat_.LiteralTrail().Index();
    for (int index = from_index; index < to_index; ++index) {
      const sat::Literal literal = sat_.LiteralTrail()[index];
      const sat::BooleanVariable var = literal.Variable();
      const bool assigned_bool = literal.IsPositive();
      SATDLOG << " - var " << var << " was assigned to " << assigned_bool
              << " from literal " << literal.SignedValue() << FZENDL;
      demons_[var.value()]->inhibit(solver());
      vars_[var.value()]->SetValue(assigned_bool);
    }
  }

  // This method is called during the processing of the CP solver queue when
  // a boolean variable is bound.
  void OnBooleanVariableFixed(int index) {
    if (sat_decision_level_.Value() < sat_.CurrentDecisionLevel()) {
      SATDLOG << "After failure, sat_decision_level = "
              << sat_decision_level_.Value()
              << ", sat decision level = " << sat_.CurrentDecisionLevel()
              << FZENDL;
      sat_.Backtrack(sat_decision_level_.Value());
      DCHECK_EQ(sat_decision_level_.Value(), sat_.CurrentDecisionLevel());
    }
    const sat::BooleanVariable var = sat::BooleanVariable(index);
    SATDLOG << "OnBooleanVariableFixed: " << vars_[index]->DebugString()
            << " with sat variable " << var << FZENDL;
    const bool new_value = vars_[index]->Value() != 0;
    const sat::Literal literal(var, new_value);
    if (sat_.Assignment().VariableIsAssigned(var)) {
      if (sat_.Assignment().LiteralIsTrue(literal)) {
        SATDLOG << " - literal = " << literal.SignedValue()
                << " already processed" << FZENDL;
        return;
      } else {
        SATDLOG << " - literal = " << literal.SignedValue()
                << " assign opposite value" << FZENDL;
        solver()->Fail();
      }
    }
    SATDLOG << " - enqueue literal = " << literal.SignedValue() << " at depth "
            << sat_decision_level_.Value() << FZENDL;
    const int trail_index = sat_.LiteralTrail().Index();
    if (!sat_.EnqueueDecisionIfNotConflicting(literal)) {
      SATDLOG << " - failure detected, should backtrack" << FZENDL;
      solver()->Fail();
    } else {
      sat_decision_level_.SetValue(solver(), sat_.CurrentDecisionLevel());
      QueryAssignedSatLiterals(trail_index);
    }
  }

  void Post() override {
    demons_.resize(vars_.size());
    for (int i = 0; i < vars_.size(); ++i) {
      demons_[i] = MakeConstraintDemon1(solver(), this,
                                        &SatPropagator::OnBooleanVariableFixed,
                                        "OnBooleanVariableFixed", i);
      vars_[i]->WhenDomain(demons_[i]);
    }
  }

  void InitialPropagate() override {
    SATDLOG << "Initial propagation on sat solver" << FZENDL;
    QueryAssignedSatLiterals(0);

    for (int i = 0; i < vars_.size(); ++i) {
      IntVar* const var = vars_[i];
      if (var->Bound()) {
        OnBooleanVariableFixed(i);
      }
    }
    SATDLOG << " - done" << FZENDL;
  }

  sat::SatSolver* sat() { return &sat_; }

  std::string DebugString() const override {
    return StringPrintf("SatConstraint(%d variables)", sat_.NumVariables());
  }

  void Accept(ModelVisitor* visitor) const override {
    VLOG(1) << "Should Not Be Visited";
  }

 private:
  sat::SatSolver sat_;
  std::vector<IntVar*> vars_;
  std::unordered_map<IntVar*, sat::BooleanVariable> indices_;
  std::vector<sat::Literal> bound_literals_;
  NumericalRev<int> sat_decision_level_;
  std::vector<Demon*> demons_;
  std::vector<sat::Literal> early_deductions_;
};

bool AddBoolEq(SatPropagator* sat, IntExpr* left, IntExpr* right) {
  if (!sat->ExpressionIsBoolean(left) || !sat->ExpressionIsBoolean(right)) {
    return false;
  }
  const sat::Literal left_literal = sat->GetOrCreateLiteral(left);
  const sat::Literal right_literal = sat->GetOrCreateLiteral(right);
  sat->sat()->AddBinaryClause(left_literal.Negated(), right_literal);
  sat->sat()->AddBinaryClause(left_literal, right_literal.Negated());
  return true;
}

bool AddBoolLe(SatPropagator* sat, IntExpr* left, IntExpr* right) {
  if (!sat->ExpressionIsBoolean(left) || !sat->ExpressionIsBoolean(right)) {
    return false;
  }
  const sat::Literal left_literal = sat->GetOrCreateLiteral(left);
  const sat::Literal right_literal = sat->GetOrCreateLiteral(right);
  sat->sat()->AddBinaryClause(left_literal.Negated(), right_literal);
  return true;
}

bool AddBoolNot(SatPropagator* sat, IntExpr* left, IntExpr* right) {
  if (!sat->ExpressionIsBoolean(left) || !sat->ExpressionIsBoolean(right)) {
    return false;
  }
  const sat::Literal left_literal = sat->GetOrCreateLiteral(left);
  const sat::Literal right_literal = sat->GetOrCreateLiteral(right);
  sat->sat()->AddBinaryClause(left_literal.Negated(), right_literal.Negated());
  sat->sat()->AddBinaryClause(left_literal, right_literal);
  return true;
}

bool AddBoolOrArrayEqVar(SatPropagator* sat, const std::vector<IntVar*>& vars,
                         IntExpr* target) {
  if (!sat->AllVariablesAreBoolean(vars) || !sat->ExpressionIsBoolean(target)) {
    return false;
  }
  const sat::Literal target_literal = sat->GetOrCreateLiteral(target);
  std::vector<sat::Literal> lits(vars.size() + 1);
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->GetOrCreateLiteral(vars[i]);
  }
  lits[vars.size()] = target_literal.Negated();
  sat->sat()->AddProblemClause(lits);
  for (int i = 0; i < vars.size(); ++i) {
    sat->sat()->AddBinaryClause(target_literal,
                                sat->GetOrCreateLiteral(vars[i]).Negated());
  }
  return true;
}

bool AddBoolAndArrayEqVar(SatPropagator* sat, const std::vector<IntVar*>& vars,
                          IntExpr* target) {
  if (!sat->AllVariablesAreBoolean(vars) || !sat->ExpressionIsBoolean(target)) {
    return false;
  }
  const sat::Literal target_literal = sat->GetOrCreateLiteral(target);
  std::vector<sat::Literal> lits(vars.size() + 1);
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->GetOrCreateLiteral(vars[i]).Negated();
  }
  lits[vars.size()] = target_literal;
  sat->sat()->AddProblemClause(lits);
  for (int i = 0; i < vars.size(); ++i) {
    sat->sat()->AddBinaryClause(target_literal.Negated(),
                                sat->GetOrCreateLiteral(vars[i]));
  }
  return true;
}

bool AddSumBoolArrayGreaterEqVar(SatPropagator* sat,
                                 const std::vector<IntVar*>& vars,
                                 IntExpr* target) {
  if (!sat->AllVariablesAreBoolean(vars) || !sat->ExpressionIsBoolean(target)) {
    return false;
  }
  const sat::Literal target_literal = sat->GetOrCreateLiteral(target);
  std::vector<sat::Literal> lits(vars.size() + 1);
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->GetOrCreateLiteral(vars[i]);
  }
  lits[vars.size()] = target_literal.Negated();
  sat->sat()->AddProblemClause(lits);
  return true;
}

bool AddMaxBoolArrayLessEqVar(SatPropagator* sat,
                              const std::vector<IntVar*>& vars,
                              IntExpr* target) {
  if (!sat->AllVariablesAreBoolean(vars) || !sat->ExpressionIsBoolean(target)) {
    return false;
  }
  const sat::Literal target_literal = sat->GetOrCreateLiteral(target);
  for (int i = 0; i < vars.size(); ++i) {
    const sat::Literal literal = sat->GetOrCreateLiteral(vars[i]).Negated();
    sat->sat()->AddBinaryClause(target_literal, literal);
  }
  return true;
}

bool AddBoolOrEqVar(SatPropagator* sat, IntExpr* left, IntExpr* right,
                    IntExpr* target) {
  if (!sat->ExpressionIsBoolean(left) || !sat->ExpressionIsBoolean(right) ||
      !sat->ExpressionIsBoolean(target)) {
    return false;
  }
  const sat::Literal left_literal = sat->GetOrCreateLiteral(left);
  const sat::Literal right_literal = sat->GetOrCreateLiteral(right);
  const sat::Literal target_literal = sat->GetOrCreateLiteral(target);
  sat->sat()->AddTernaryClause(left_literal, right_literal,
                               target_literal.Negated());
  sat->sat()->AddBinaryClause(left_literal.Negated(), target_literal);
  sat->sat()->AddBinaryClause(right_literal.Negated(), target_literal);
  return true;
}

bool AddBoolAndEqVar(SatPropagator* sat, IntExpr* left, IntExpr* right,
                     IntExpr* target) {
  if (!sat->ExpressionIsBoolean(left) || !sat->ExpressionIsBoolean(right) ||
      !sat->ExpressionIsBoolean(target)) {
    return false;
  }
  const sat::Literal left_literal = sat->GetOrCreateLiteral(left);
  const sat::Literal right_literal = sat->GetOrCreateLiteral(right);
  const sat::Literal target_literal = sat->GetOrCreateLiteral(target);
  sat->sat()->AddTernaryClause(left_literal.Negated(), right_literal.Negated(),
                               target_literal);
  sat->sat()->AddBinaryClause(left_literal, target_literal.Negated());
  sat->sat()->AddBinaryClause(right_literal, target_literal.Negated());
  return true;
}

bool AddBoolIsEqVar(SatPropagator* sat, IntExpr* left, IntExpr* right,
                    IntExpr* target) {
  if (!sat->ExpressionIsBoolean(left) || !sat->ExpressionIsBoolean(right) ||
      !sat->ExpressionIsBoolean(target)) {
    return false;
  }
  const sat::Literal left_literal = sat->GetOrCreateLiteral(left);
  const sat::Literal right_literal = sat->GetOrCreateLiteral(right);
  const sat::Literal target_literal = sat->GetOrCreateLiteral(target);
  sat->sat()->AddTernaryClause(left_literal.Negated(), right_literal,
                               target_literal.Negated());
  sat->sat()->AddTernaryClause(left_literal, right_literal.Negated(),
                               target_literal.Negated());
  sat->sat()->AddTernaryClause(left_literal, right_literal, target_literal);
  sat->sat()->AddTernaryClause(left_literal.Negated(), right_literal.Negated(),
                               target_literal);
  return true;
}

bool AddBoolIsNEqVar(SatPropagator* sat, IntExpr* left, IntExpr* right,
                     IntExpr* target) {
  if (!sat->ExpressionIsBoolean(left) || !sat->ExpressionIsBoolean(right) ||
      !sat->ExpressionIsBoolean(target)) {
    return false;
  }
  const sat::Literal left_literal = sat->GetOrCreateLiteral(left);
  const sat::Literal right_literal = sat->GetOrCreateLiteral(right);
  const sat::Literal target_literal = sat->GetOrCreateLiteral(target);
  sat->sat()->AddTernaryClause(left_literal.Negated(), right_literal,
                               target_literal);
  sat->sat()->AddTernaryClause(left_literal, right_literal.Negated(),
                               target_literal);
  sat->sat()->AddTernaryClause(left_literal, right_literal,
                               target_literal.Negated());
  sat->sat()->AddTernaryClause(left_literal.Negated(), right_literal.Negated(),
                               target_literal.Negated());
  return true;
}

bool AddBoolIsLeVar(SatPropagator* sat, IntExpr* left, IntExpr* right,
                    IntExpr* target) {
  if (!sat->ExpressionIsBoolean(left) || !sat->ExpressionIsBoolean(right) ||
      !sat->ExpressionIsBoolean(target)) {
    return false;
  }
  const sat::Literal left_literal = sat->GetOrCreateLiteral(left);
  const sat::Literal right_literal = sat->GetOrCreateLiteral(right);
  const sat::Literal target_literal = sat->GetOrCreateLiteral(target);
  sat->sat()->AddTernaryClause(left_literal.Negated(), right_literal,
                               target_literal.Negated());
  sat->sat()->AddBinaryClause(left_literal, target_literal);
  sat->sat()->AddBinaryClause(right_literal.Negated(), target_literal);
  return true;
}

bool AddBoolOrArrayEqualTrue(SatPropagator* sat,
                             const std::vector<IntVar*>& vars) {
  if (!sat->AllVariablesAreBoolean(vars)) {
    return false;
  }
  std::vector<sat::Literal> lits(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->GetOrCreateLiteral(vars[i]);
  }
  sat->sat()->AddProblemClause(lits);
  return true;
}

bool AddBoolAndArrayEqualFalse(SatPropagator* sat,
                               const std::vector<IntVar*>& vars) {
  if (!sat->AllVariablesAreBoolean(vars)) {
    return false;
  }
  std::vector<sat::Literal> lits(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->GetOrCreateLiteral(vars[i]).Negated();
  }
  sat->sat()->AddProblemClause(lits);
  return true;
}

bool AddAtMostOne(SatPropagator* sat, const std::vector<IntVar*>& vars) {
  if (!sat->AllVariablesAreBoolean(vars)) {
    return false;
  }
  std::vector<sat::Literal> lits(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->GetOrCreateLiteral(vars[i]).Negated();
  }
  for (int i = 0; i < lits.size() - 1; ++i) {
    for (int j = i + 1; j < lits.size(); ++j) {
      sat->sat()->AddBinaryClause(lits[i], lits[j]);
    }
  }
  return true;
}

bool AddAtMostNMinusOne(SatPropagator* sat, const std::vector<IntVar*>& vars) {
  if (!sat->AllVariablesAreBoolean(vars)) {
    return false;
  }
  std::vector<sat::Literal> lits(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->GetOrCreateLiteral(vars[i]).Negated();
  }
  sat->sat()->AddProblemClause(lits);
  return true;
}

bool AddArrayXor(SatPropagator* sat, const std::vector<IntVar*>& vars) {
  if (!sat->AllVariablesAreBoolean(vars)) {
    return false;
  }
  return false;
}

bool AddIntEqReif(SatPropagator* sat, IntExpr* left, IntExpr* right,
                  IntExpr* target) {
  if (!sat->ExpressionIsBoolean(left) || !sat->ExpressionIsBoolean(right) ||
      !sat->ExpressionIsBoolean(target)) {
    return false;
  }
  const sat::Literal left_literal = sat->GetOrCreateLiteral(left);
  const sat::Literal right_literal = sat->GetOrCreateLiteral(right);
  const sat::Literal target_literal = sat->GetOrCreateLiteral(target);
  sat->sat()->AddTernaryClause(left_literal, right_literal, target_literal);
  sat->sat()->AddTernaryClause(left_literal.Negated(), right_literal.Negated(),
                               target_literal);
  sat->sat()->AddTernaryClause(left_literal.Negated(), right_literal,
                               target_literal.Negated());
  sat->sat()->AddTernaryClause(left_literal, right_literal.Negated(),
                               target_literal.Negated());
  return true;
}

bool AddIntNeReif(SatPropagator* sat, IntExpr* left, IntExpr* right,
                  IntExpr* target) {
  if (!sat->ExpressionIsBoolean(left) || !sat->ExpressionIsBoolean(right) ||
      !sat->ExpressionIsBoolean(target)) {
    return false;
  }
  const sat::Literal left_literal = sat->GetOrCreateLiteral(left);
  const sat::Literal right_literal = sat->GetOrCreateLiteral(right);
  const sat::Literal target_literal = sat->GetOrCreateLiteral(target);
  sat->sat()->AddTernaryClause(left_literal, right_literal.Negated(),
                               target_literal);
  sat->sat()->AddTernaryClause(left_literal.Negated(), right_literal,
                               target_literal);
  sat->sat()->AddTernaryClause(left_literal.Negated(), right_literal.Negated(),
                               target_literal.Negated());
  sat->sat()->AddTernaryClause(left_literal, right_literal,
                               target_literal.Negated());
  return true;
}

bool AddSumInRange(SatPropagator* sat, const std::vector<IntVar*>& vars,
                   int64 range_min, int64 range_max) {
  std::vector<sat::LiteralWithCoeff> terms(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    const sat::Literal lit = sat->GetOrCreateLiteral(vars[i]);
    terms[i] = sat::LiteralWithCoeff(lit, 1);
  }
  sat->sat()->AddLinearConstraint(range_min > 0, sat::Coefficient(range_min),
                                  range_max < vars.size(),
                                  sat::Coefficient(range_max), &terms);
  return true;
}

SatPropagator* MakeSatPropagator(Solver* solver) {
  return solver->RevAlloc(new SatPropagator(solver));
}

#undef SATDLOG
}  // namespace operations_research

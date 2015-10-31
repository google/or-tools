// Copyright 2010-2014 Google
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

#include "flatzinc/sat_constraint.h"

#include <algorithm>
#include <string>
#include <vector>
#include <iostream>  // NOLINT

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/int_type_indexed_vector.h"
#include "base/int_type.h"
#include "base/map_util.h"
#include "base/hash.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"

#include "flatzinc/model.h"

#include "sat/pb_constraint.h"
#include "sat/sat_base.h"
#include "sat/sat_solver.h"

// #define SAT_DEBUG
DECLARE_bool(fz_verbose);
DECLARE_bool(fz_debug);

namespace operations_research {
// Constraint that tight together boolean variables in the CP solver to sat
// variables and clauses.
class SatPropagator : public Constraint {
 public:
  explicit SatPropagator(Solver* solver)
      : Constraint(solver), sat_decision_level_(0) {}

  ~SatPropagator() {}

  bool IsExpressionBoolean(IntExpr* expr) const {
    IntVar* expr_var = nullptr;
    bool expr_negated = false;
    return solver()->IsBooleanVar(expr, &expr_var, &expr_negated);
  }

  bool AllVariablesBoolean(const std::vector<IntVar*>& vars) const {
    for (int i = 0; i < vars.size(); ++i) {
      if (!IsExpressionBoolean(vars[i])) {
        return false;
      }
    }
    return true;
  }

  // Converts a constraint solver literal to the SatSolver representation.
  sat::Literal Literal(IntExpr* expr) {
    IntVar* expr_var = nullptr;
    bool expr_negated = false;
    CHECK(solver()->IsBooleanVar(expr, &expr_var, &expr_negated));
#ifdef SAT_DEBUG
    FZDLOG << "  - SAT: Parse " << expr->DebugString() << " to "
           << expr_var->DebugString() << "/" << expr_negated << FZENDL;
#endif
    if (ContainsKey(indices_, expr_var)) {
      return sat::Literal(indices_[expr_var], !expr_negated);
    } else {
      const int num_vars = sat_.NumVariables();
      sat_.SetNumVariables(num_vars + 1);
      const sat::VariableIndex var(num_vars);
      DCHECK_EQ(vars_.size(), var.value());
      vars_.push_back(expr_var);
      indices_[expr_var] = var;
      sat::Literal literal(var, !expr_negated);
#ifdef SAT_DEBUG
      FZDLOG << "    - created var = " << var.value()
             << ", literal = " << literal.SignedValue() << FZENDL;
#endif
      return literal;
    }
  }

  void PullSatAssignmentFrom(int from_index) {
    const int to_index = sat_.LiteralTrail().Index();
    for (int index = from_index; index < to_index; ++index) {
      const sat::Literal literal = sat_.LiteralTrail()[index];
      const sat::VariableIndex var = literal.Variable();
      const bool assigned_bool = literal.IsPositive();
#ifdef SAT_DEBUG
      FZDLOG << " - var " << var << " was assigned to " << assigned_bool
             << " from literal " << literal.SignedValue() << FZENDL;
#endif
      demons_[var.value()]->inhibit(solver());
      vars_[var.value()]->SetValue(assigned_bool);
    }
  }

  // This method is called during the processing of the CP solver queue when
  // a boolean variable is bound.
  void VariableIndexBound(int index) {
    if (sat_decision_level_.Value() < sat_.CurrentDecisionLevel()) {
#ifdef SAT_DEBUG
      FZDLOG << "After failure, sat_decision_level = "
             << sat_decision_level_.Value()
             << ", sat decision level = " << sat_.CurrentDecisionLevel()
             << FZENDL;
#endif
      sat_.Backtrack(sat_decision_level_.Value());
      DCHECK_EQ(sat_decision_level_.Value(), sat_.CurrentDecisionLevel());
    }
    const sat::VariableIndex var = sat::VariableIndex(index);
#ifdef SAT_DEBUG
    FZDLOG << "VariableIndexBound: " << vars_[index]->DebugString()
           << " with sat variable " << var << FZENDL;
#endif
    const bool new_value = vars_[index]->Value() != 0;
    sat::Literal literal(var, new_value);
    if (sat_.Assignment().VariableIsAssigned(var)) {
      if (sat_.Assignment().LiteralIsTrue(literal)) {
#ifdef SAT_DEBUG
        FZDLOG << " - literal = " << literal.SignedValue()
               << " already processed" << FZENDL;
#endif
        return;
      } else {
#ifdef SAT_DEBUG
        FZDLOG << " - literal = " << literal.SignedValue()
               << " assign opposite value" << FZENDL;
#endif
        solver()->Fail();
      }
    }
#ifdef SAT_DEBUG
    FZDLOG << " - enqueue literal = " << literal.SignedValue() << " at depth "
           << sat_decision_level_.Value() << FZENDL;
#endif
    const int trail_index = sat_.LiteralTrail().Index();
    if (!sat_.EnqueueDecisionIfNotConflicting(literal)) {
#ifdef SAT_DEBUG
      FZDLOG << " - failure detected, should backtrack" << FZENDL;
#endif
      solver()->Fail();
    } else {
      sat_decision_level_.SetValue(solver(), sat_.CurrentDecisionLevel());
      PullSatAssignmentFrom(trail_index);
    }
  }

  virtual void Post() {
    demons_.resize(vars_.size());
    for (int i = 0; i < vars_.size(); ++i) {
      demons_[i] = MakeConstraintDemon1(solver(), this,
                                        &SatPropagator::VariableIndexBound,
                                        "VariableIndexBound", i);
      vars_[i]->WhenDomain(demons_[i]);
    }
  }

  virtual void InitialPropagate() {
#ifdef SAT_DEBUG
    FZDLOG << "Initial propagation on sat solver" << FZENDL;
#endif
    PullSatAssignmentFrom(0);

    for (int i = 0; i < vars_.size(); ++i) {
      IntVar* const var = vars_[i];
      if (var->Bound()) {
        VariableIndexBound(i);
      }
    }
#ifdef SAT_DEBUG
    FZDLOG << " - done" << FZENDL;
#endif
  }

  sat::SatSolver* sat() { return &sat_; }

  virtual std::string DebugString() const {
    return StringPrintf("SatConstraint(%d variables, %d constraints)",
                        sat_.NumVariables(), sat_.NumAddedConstraints());
  }

  void Accept(ModelVisitor* visitor) const {
    VLOG(1) << "Should Not Be Visited";
  }

 private:
  sat::SatSolver sat_;
  std::vector<IntVar*> vars_;
  hash_map<IntVar*, sat::VariableIndex> indices_;
  std::vector<sat::Literal> bound_literals_;
  NumericalRev<int> sat_decision_level_;
  std::vector<Demon*> demons_;
  std::vector<sat::Literal> early_deductions_;
};

void DeclareVariableIndex(SatPropagator* sat, IntVar* var) {
  CHECK(sat->IsExpressionBoolean(var));
  sat->Literal(var);
}

bool AddBoolEq(SatPropagator* sat, IntExpr* left, IntExpr* right) {
  if (!sat->IsExpressionBoolean(left) || !sat->IsExpressionBoolean(right)) {
    return false;
  }
  sat::Literal left_literal = sat->Literal(left);
  sat::Literal right_literal = sat->Literal(right);
  sat->sat()->AddBinaryClause(left_literal.Negated(), right_literal);
  sat->sat()->AddBinaryClause(left_literal, right_literal.Negated());
  return true;
}

bool AddBoolLe(SatPropagator* sat, IntExpr* left, IntExpr* right) {
  if (!sat->IsExpressionBoolean(left) || !sat->IsExpressionBoolean(right)) {
    return false;
  }
  sat::Literal left_literal = sat->Literal(left);
  sat::Literal right_literal = sat->Literal(right);
  sat->sat()->AddBinaryClause(left_literal.Negated(), right_literal);
  return true;
}

bool AddBoolNot(SatPropagator* sat, IntExpr* left, IntExpr* right) {
  if (!sat->IsExpressionBoolean(left) || !sat->IsExpressionBoolean(right)) {
    return false;
  }
  sat::Literal left_literal = sat->Literal(left);
  sat::Literal right_literal = sat->Literal(right);
  sat->sat()->AddBinaryClause(left_literal.Negated(), right_literal.Negated());
  sat->sat()->AddBinaryClause(left_literal, right_literal);
  return true;
}

bool AddBoolOrArrayEqVar(SatPropagator* sat, const std::vector<IntVar*>& vars,
                         IntExpr* target) {
  if (!sat->AllVariablesBoolean(vars) || !sat->IsExpressionBoolean(target)) {
    return false;
  }
  sat::Literal target_literal = sat->Literal(target);
  std::vector<sat::Literal> lits(vars.size() + 1);
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->Literal(vars[i]);
  }
  lits[vars.size()] = target_literal.Negated();
  sat->sat()->AddProblemClause(lits);
  for (int i = 0; i < vars.size(); ++i) {
    sat->sat()->AddBinaryClause(target_literal,
                                sat->Literal(vars[i]).Negated());
  }
  return true;
}

bool AddBoolAndArrayEqVar(SatPropagator* sat, const std::vector<IntVar*>& vars,
                          IntExpr* target) {
  if (!sat->AllVariablesBoolean(vars) || !sat->IsExpressionBoolean(target)) {
    return false;
  }
  sat::Literal target_literal = sat->Literal(target);
  std::vector<sat::Literal> lits(vars.size() + 1);
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->Literal(vars[i]).Negated();
  }
  lits[vars.size()] = target_literal;
  sat->sat()->AddProblemClause(lits);
  for (int i = 0; i < vars.size(); ++i) {
    sat->sat()->AddBinaryClause(target_literal.Negated(),
                                sat->Literal(vars[i]));
  }
  return true;
}

bool AddSumBoolArrayGreaterEqVar(SatPropagator* sat,
                                 const std::vector<IntVar*>& vars, IntExpr* target) {
  if (!sat->AllVariablesBoolean(vars) || !sat->IsExpressionBoolean(target)) {
    return false;
  }
  sat::Literal target_literal = sat->Literal(target);
  std::vector<sat::Literal> lits(vars.size() + 1);
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->Literal(vars[i]);
  }
  lits[vars.size()] = target_literal.Negated();
  sat->sat()->AddProblemClause(lits);
  return true;
}

bool AddMaxBoolArrayLessEqVar(SatPropagator* sat, const std::vector<IntVar*>& vars,
                              IntExpr* target) {
  if (!sat->AllVariablesBoolean(vars) || !sat->IsExpressionBoolean(target)) {
    return false;
  }
  const sat::Literal target_literal = sat->Literal(target);
  for (int i = 0; i < vars.size(); ++i) {
    const sat::Literal literal = sat->Literal(vars[i]).Negated();
    sat->sat()->AddBinaryClause(target_literal, literal);
  }
  return true;
}

bool AddSumBoolArrayLessEqKVar(SatPropagator* sat, const std::vector<IntVar*>& vars,
                               IntExpr* target) {
  if (vars.size() == 1) {
    return AddBoolLe(sat, vars[0], target);
  }
  if (!sat->AllVariablesBoolean(vars) || !sat->IsExpressionBoolean(target)) {
    return false;
  }
  IntVar* const extra = target->solver()->MakeBoolVar();
  sat::Literal target_literal = sat->Literal(target);
  sat::Literal extra_literal = sat->Literal(extra);
  std::vector<sat::Literal> lits(vars.size() + 1);
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->Literal(vars[i]);
  }
  lits[vars.size()] = extra_literal.Negated();
  sat->sat()->AddProblemClause(lits);
  for (int i = 0; i < vars.size(); ++i) {
    sat->sat()->AddBinaryClause(extra_literal, sat->Literal(vars[i]).Negated());
  }
  sat->sat()->AddBinaryClause(extra_literal.Negated(), target_literal);
  return true;
}

bool AddBoolOrEqVar(SatPropagator* sat, IntExpr* left, IntExpr* right,
                    IntExpr* target) {
  if (!sat->IsExpressionBoolean(left) || !sat->IsExpressionBoolean(right) ||
      !sat->IsExpressionBoolean(target)) {
    return false;
  }
  sat::Literal left_literal = sat->Literal(left);
  sat::Literal right_literal = sat->Literal(right);
  sat::Literal target_literal = sat->Literal(target);
  sat->sat()->AddTernaryClause(left_literal, right_literal,
                               target_literal.Negated());
  sat->sat()->AddBinaryClause(left_literal.Negated(), target_literal);
  sat->sat()->AddBinaryClause(right_literal.Negated(), target_literal);
  return true;
}

bool AddBoolAndEqVar(SatPropagator* sat, IntExpr* left, IntExpr* right,
                     IntExpr* target) {
  if (!sat->IsExpressionBoolean(left) || !sat->IsExpressionBoolean(right) ||
      !sat->IsExpressionBoolean(target)) {
    return false;
  }
  sat::Literal left_literal = sat->Literal(left);
  sat::Literal right_literal = sat->Literal(right);
  sat::Literal target_literal = sat->Literal(target);
  sat->sat()->AddTernaryClause(left_literal.Negated(), right_literal.Negated(),
                               target_literal);
  sat->sat()->AddBinaryClause(left_literal, target_literal.Negated());
  sat->sat()->AddBinaryClause(right_literal, target_literal.Negated());
  return true;
}

bool AddBoolIsEqVar(SatPropagator* sat, IntExpr* left, IntExpr* right,
                    IntExpr* target) {
  if (!sat->IsExpressionBoolean(left) || !sat->IsExpressionBoolean(right) ||
      !sat->IsExpressionBoolean(target)) {
    return false;
  }
  sat::Literal left_literal = sat->Literal(left);
  sat::Literal right_literal = sat->Literal(right);
  sat::Literal target_literal = sat->Literal(target);
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
  if (!sat->IsExpressionBoolean(left) || !sat->IsExpressionBoolean(right) ||
      !sat->IsExpressionBoolean(target)) {
    return false;
  }
  sat::Literal left_literal = sat->Literal(left);
  sat::Literal right_literal = sat->Literal(right);
  sat::Literal target_literal = sat->Literal(target);
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
  if (!sat->IsExpressionBoolean(left) || !sat->IsExpressionBoolean(right) ||
      !sat->IsExpressionBoolean(target)) {
    return false;
  }
  sat::Literal left_literal = sat->Literal(left);
  sat::Literal right_literal = sat->Literal(right);
  sat::Literal target_literal = sat->Literal(target);
  sat->sat()->AddTernaryClause(left_literal.Negated(), right_literal,
                               target_literal.Negated());
  sat->sat()->AddBinaryClause(left_literal, target_literal);
  sat->sat()->AddBinaryClause(right_literal.Negated(), target_literal);
  return true;
}

bool AddBoolOrArrayEqualTrue(SatPropagator* sat, const std::vector<IntVar*>& vars) {
  if (!sat->AllVariablesBoolean(vars)) {
    return false;
  }
  std::vector<sat::Literal> lits(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->Literal(vars[i]);
  }
  sat->sat()->AddProblemClause(lits);
  return true;
}

bool AddBoolAndArrayEqualFalse(SatPropagator* sat,
                               const std::vector<IntVar*>& vars) {
  if (!sat->AllVariablesBoolean(vars)) {
    return false;
  }
  std::vector<sat::Literal> lits(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->Literal(vars[i]).Negated();
  }
  sat->sat()->AddProblemClause(lits);
  return true;
}

bool AddAtMostOne(SatPropagator* sat, const std::vector<IntVar*>& vars) {
  if (!sat->AllVariablesBoolean(vars)) {
    return false;
  }
  std::vector<sat::Literal> lits(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->Literal(vars[i]).Negated();
  }
  for (int i = 0; i < lits.size() - 1; ++i) {
    for (int j = i + 1; j < lits.size(); ++j) {
      sat->sat()->AddBinaryClause(lits[i], lits[j]);
    }
  }
  return true;
}

bool AddAtMostNMinusOne(SatPropagator* sat, const std::vector<IntVar*>& vars) {
  if (!sat->AllVariablesBoolean(vars)) {
    return false;
  }
  std::vector<sat::Literal> lits(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->Literal(vars[i]).Negated();
  }
  sat->sat()->AddProblemClause(lits);
  return true;
}

bool AddArrayXor(SatPropagator* sat, const std::vector<IntVar*>& vars) {
  if (!sat->AllVariablesBoolean(vars)) {
    return false;
  }
  return false;
}

bool AddIntEqReif(SatPropagator* sat, IntExpr* left, IntExpr* right,
                  IntExpr* target) {
  if (!sat->IsExpressionBoolean(left) || !sat->IsExpressionBoolean(right) ||
      !sat->IsExpressionBoolean(target)) {
    return false;
  }
  sat::Literal left_literal = sat->Literal(left);
  sat::Literal right_literal = sat->Literal(right);
  sat::Literal target_literal = sat->Literal(target);
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
  if (!sat->IsExpressionBoolean(left) || !sat->IsExpressionBoolean(right) ||
      !sat->IsExpressionBoolean(target)) {
    return false;
  }
  sat::Literal left_literal = sat->Literal(left);
  sat::Literal right_literal = sat->Literal(right).Negated();
  sat::Literal target_literal = sat->Literal(target);
  sat->sat()->AddTernaryClause(left_literal, right_literal, target_literal);
  sat->sat()->AddTernaryClause(left_literal.Negated(), right_literal.Negated(),
                               target_literal);
  sat->sat()->AddTernaryClause(left_literal.Negated(), right_literal,
                               target_literal.Negated());
  sat->sat()->AddTernaryClause(left_literal, right_literal.Negated(),
                               target_literal.Negated());
  return true;
}

bool AddSumInRange(SatPropagator* sat, const std::vector<IntVar*>& vars,
                   int64 range_min, int64 range_max) {
  std::vector<sat::LiteralWithCoeff> terms(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    const sat::Literal lit = sat->Literal(vars[i]);
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

int NumSatConstraints(SatPropagator* sat) {
  return sat->sat()->NumAddedConstraints();
}
}  // namespace operations_research

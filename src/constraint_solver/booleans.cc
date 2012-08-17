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

#include <math.h>
#include <string.h>
#include <algorithm>
#include "base/hash.h"
#include <string>
#include <utility>
#include <vector>

#include "base/commandlineflags.h"
#include "base/hash.h"
#include "base/int-type-indexed-vector.h"
#include "base/int-type.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/map-util.h"
#include "base/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/bitset.h"
#include "util/const_int_array.h"

#include "core/Solver.cc"

namespace operations_research {
class SatPropagator : public Constraint {
 public:
  SatPropagator(Solver* const solver,
                bool backjump)
  : Constraint(solver),
    minisat_trail_(0),
    backtrack_level_(-1),
    backjump_(backjump) {}

  ~SatPropagator() {}

  bool Check(IntExpr* const expr) const {
    IntVar* expr_var = NULL;
    bool expr_negated = false;
    return solver()->IsBooleanVar(expr, &expr_var, &expr_negated);
  }

  bool Check(const std::vector<IntVar*>& vars) const {
    for (int i = 0; i < vars.size(); ++i) {
      if (!Check(vars[i])) {
        return false;
      }
    }
    return true;
  }

  Minisat::Lit Literal(IntExpr* const expr) {
    IntVar* expr_var = NULL;
    bool expr_negated = false;
    if (!solver()->IsBooleanVar(expr, &expr_var, &expr_negated)) {
      return Minisat::lit_Error;
    }
    VLOG(2) << "SAT: Parse " << expr->DebugString() << " to "
            << expr_var->DebugString() << "/" << expr_negated;
    if (ContainsKey(indices_, expr_var)) {
      return Minisat::mkLit(indices_[expr_var], !expr_negated);
    } else {
      const Minisat::Var var = minisat_.newVar(true, true);
      vars_.push_back(expr_var);
      indices_[expr_var] = var;
      Minisat::Lit lit = Minisat::mkLit(var, !expr_negated);
      VLOG(2) << "  - created var = " << Minisat::toInt(var)
              << ", lit = " << Minisat::toInt(lit);
      return lit;
    }
  }

  void VariableBound(int index) {
    if (minisat_trail_.Value() < minisat_.decisionLevel()) {
      VLOG(2) << "After failure, minisat_trail = " << minisat_trail_.Value()
              << ", minisat decision level = " << minisat_.decisionLevel();
      minisat_.cancelUntil(minisat_trail_.Value());
      CHECK_EQ(minisat_trail_.Value(), minisat_.decisionLevel());
      if (backjump_ && backtrack_level_ != -1) {
        if (minisat_trail_.Value() > backtrack_level_) {
          VLOG(2) << "  - learnt backtrack: " << minisat_trail_.Value()
                  << "/" << backtrack_level_;
          solver()->Fail();
        } else {
          backtrack_level_ = -1;
        }
      }
    }
    VLOG(2) << "VariableBound: " << vars_[index]->DebugString();
    const Minisat::Var var(index);
    const int internal_value = toInt(minisat_.value(var));
    const int64 var_value = vars_[index]->Value();
    if (internal_value != 2 && var_value != internal_value) {
      VLOG(2) << "  - internal value = " << internal_value << ", failing";
      solver()->Fail();
    }
    Minisat::Lit lit = Minisat::mkLit(var, var_value);
    VLOG(2) << "  - enqueue lit = " << Minisat::toInt(lit)
            << " at depth " << minisat_trail_.Value();
    backtrack_level_ = minisat_.propagateOneLiteral(lit);
    if (backtrack_level_ >= 0) {
      VLOG(2) << "  - failure detected, should backtrack to "
              << backtrack_level_;
      solver()->Fail();
    } else {
      minisat_trail_.SetValue(solver(), minisat_.decisionLevel());
      for (int i = 0; i < minisat_.touched_variables_.size(); ++i) {
        const Minisat::Lit lit = minisat_.touched_variables_[i];
        const int var = Minisat::var(lit);
        const bool assigned_bool = Minisat::sign(lit);
        VLOG(2) << "  - var " << var << " was assigned to " << assigned_bool;
        demons_[var]->inhibit(solver());
        vars_[var]->SetValue(assigned_bool);
      }
    }
  }

  virtual void Post() {
    demons_.resize(vars_.size());
    for (int i = 0; i < vars_.size(); ++i) {
      demons_[i] = MakeConstraintDemon1(solver(),
                                        this,
                                        &SatPropagator::VariableBound,
                                        "VariableBound",
                                        indices_[vars_[i]]);
      vars_[i]->WhenDomain(demons_[i]);
    }
  }

  virtual void InitialPropagate() {
    VLOG(2) << "Initial propagation on sat solver";
    minisat_.initPropagator();
    for (int i = 0; i < vars_.size(); ++i) {
      IntVar* const var = vars_[i];
      if (var->Bound()) {
        VariableBound(i);
      }
    }
    VLOG(2) << "  - done";
  }

  // Add a clause to the solver.
  bool AddClause (const std::vector<Minisat::Lit>& lits) {
    return minisat_.addClause(lits);
  }

  // Add the empty clause, making the solver contradictory.
  bool AddEmptyClause() {
    return minisat_.addEmptyClause();
  }

  // Add a unit clause to the solver.
  bool AddClause (Minisat::Lit p) {
    return minisat_.addClause(p);
  }

  // Add a binary clause to the solver.
  bool AddClause (Minisat::Lit p, Minisat:: Lit q) {
    return minisat_.addClause(p, q);
  }

  // Add a ternary clause to the solver.
  bool AddClause (Minisat::Lit p, Minisat::Lit q, Minisat::Lit r) {
    return minisat_.addClause(p, q, r);
  }

  virtual string DebugString() const {
    return "MinisatConstraint";
  }

  void Accept(ModelVisitor* const visitor) const {
    VLOG(1) << "Should Not Be Visited";
  }

 private:
  Minisat::Solver minisat_;
  std::vector<IntVar*> vars_;
  hash_map<IntVar*, Minisat::Var> indices_;
  std::vector<Minisat::Lit> bound_literals_;
  NumericalRev<int> minisat_trail_;
  std::vector<Demon*> demons_;
  int backtrack_level_;
  const bool backjump_;
};

bool AddBoolEq(SatPropagator* const sat,
               IntExpr* const left,
               IntExpr* const right) {
  if (!sat->Check(left) || !sat->Check(right)) {
    return false;
  }
  Minisat::Lit left_lit = sat->Literal(left);
  Minisat::Lit right_lit = sat->Literal(right);
  sat->AddClause(~left_lit, right_lit);
  sat->AddClause(left_lit, ~right_lit);
  return true;
}

bool AddBoolLe(SatPropagator* const sat,
               IntExpr* const left,
               IntExpr* const right) {
  if (!sat->Check(left) || !sat->Check(right)) {
    return false;
  }
  Minisat::Lit left_lit = sat->Literal(left);
  Minisat::Lit right_lit = sat->Literal(right);
  sat->AddClause(~left_lit, right_lit);
  return true;
}

bool AddBoolNot(SatPropagator* const sat,
                IntExpr* const left,
                IntExpr* const right) {
  if (!sat->Check(left) || !sat->Check(right)) {
    return false;
  }
  Minisat::Lit left_lit = sat->Literal(left);
  Minisat::Lit right_lit = sat->Literal(right);
  sat->AddClause(~left_lit, ~right_lit);
  sat->AddClause(left_lit, right_lit);
  return true;
}

bool AddBoolOrArrayEqVar(SatPropagator* const sat,
                         const std::vector<IntVar*>& vars,
                         IntExpr* const target) {
  if (!sat->Check(vars) || !sat->Check(target)) {
    return false;
  }
  Minisat::Lit target_lit = sat->Literal(target);
  std::vector<Minisat::Lit> lits(vars.size() + 1);
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->Literal(vars[i]);
  }
  lits[vars.size()] = ~target_lit;
  sat->AddClause(lits);
  for (int i = 0; i < vars.size(); ++i) {
    sat->AddClause(target_lit, ~lits[i]);
  }
  return true;
}

bool AddBoolAndArrayEqVar(SatPropagator* const sat,
                          const std::vector<IntVar*>& vars,
                          IntExpr* const target) {
  if (!sat->Check(vars) || !sat->Check(target)) {
    return false;
  }
  Minisat::Lit target_lit = sat->Literal(target);
  std::vector<Minisat::Lit> lits(vars.size() + 1);
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = ~sat->Literal(vars[i]);
  }
  lits[vars.size()] = target_lit;
  sat->AddClause(lits);
  for (int i = 0; i < vars.size(); ++i) {
    sat->AddClause(~target_lit, ~lits[i]);
  }
  return true;
}

bool AddBoolOrEqVar(SatPropagator* const sat,
                    IntExpr* const left,
                    IntExpr* const right,
                    IntExpr* const target) {
  if (!sat->Check(left) || !sat->Check(right) || !sat->Check(target)) {
    return false;
  }
  Minisat::Lit left_lit = sat->Literal(left);
  Minisat::Lit right_lit = sat->Literal(right);
  Minisat::Lit target_lit = sat->Literal(target);
  sat->AddClause(left_lit, right_lit, ~target_lit);
  sat->AddClause(~left_lit, target_lit);
  sat->AddClause(~right_lit, target_lit);
  return true;
}

bool AddBoolAndEqVar(SatPropagator* const sat,
                     IntExpr* const left,
                     IntExpr* const right,
                     IntExpr* const target) {
  if (!sat->Check(left) || !sat->Check(right) || !sat->Check(target)) {
    return false;
  }
  Minisat::Lit left_lit = sat->Literal(left);
  Minisat::Lit right_lit = sat->Literal(right);
  Minisat::Lit target_lit = sat->Literal(target);
  sat->AddClause(~left_lit, ~right_lit, target_lit);
  sat->AddClause(left_lit, ~target_lit);
  sat->AddClause(right_lit, ~target_lit);
  return true;
}

bool AddBoolIsEqVar(SatPropagator* const sat,
                    IntExpr* const left,
                    IntExpr* const right,
                    IntExpr* const target) {
  if (!sat->Check(left) || !sat->Check(right) || !sat->Check(target)) {
    return false;
  }
  Minisat::Lit left_lit = sat->Literal(left);
  Minisat::Lit right_lit = sat->Literal(right);
  Minisat::Lit target_lit = sat->Literal(target);
  sat->AddClause(~left_lit, right_lit, ~target_lit);
  sat->AddClause(left_lit, ~right_lit, ~target_lit);
  sat->AddClause(left_lit, right_lit, target_lit);
  sat->AddClause(~left_lit, ~right_lit, target_lit);
  return true;
}

bool AddBoolIsNEqVar(SatPropagator* const sat,
                     IntExpr* const left,
                     IntExpr* const right,
                     IntExpr* const target) {
  if (!sat->Check(left) || !sat->Check(right) || !sat->Check(target)) {
    return false;
  }
  Minisat::Lit left_lit = sat->Literal(left);
  Minisat::Lit right_lit = sat->Literal(right);
  Minisat::Lit target_lit = sat->Literal(target);
  sat->AddClause(~left_lit, right_lit, target_lit);
  sat->AddClause(left_lit, ~right_lit, target_lit);
  sat->AddClause(left_lit, right_lit, ~target_lit);
  sat->AddClause(~left_lit, ~right_lit, ~target_lit);
  return true;
}

bool AddBoolIsLeVar(SatPropagator* const sat,
                    IntExpr* const left,
                    IntExpr* const right,
                    IntExpr* const target) {
  if (!sat->Check(left) || !sat->Check(right) || !sat->Check(target)) {
    return false;
  }
  Minisat::Lit left_lit = sat->Literal(left);
  Minisat::Lit right_lit = sat->Literal(right);
  Minisat::Lit target_lit = sat->Literal(target);
  sat->AddClause(~left_lit, right_lit, ~target_lit);
  sat->AddClause(left_lit, target_lit);
  sat->AddClause(~right_lit, target_lit);
  return true;
}

bool AddBoolOrArrayEqualTrue(SatPropagator* const sat,
                             const std::vector<IntVar*>& vars) {
  if (!sat->Check(vars)) {
    return false;
  }
  std::vector<Minisat::Lit> lits(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->Literal(vars[i]);
  }
  sat->AddClause(lits);
  return true;
}

bool AddBoolAndArrayEqualFalse(SatPropagator* const sat,
                               const std::vector<IntVar*>& vars) {
  if (!sat->Check(vars)) {
    return false;
  }
  std::vector<Minisat::Lit> lits(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = ~sat->Literal(vars[i]);
  }
  sat->AddClause(lits);
  return true;
}

SatPropagator* MakeSatPropagator(Solver* const solver, bool backjump) {
  return solver->RevAlloc(new SatPropagator(solver, backjump));
}
}  // namespace operations_research

// Copyright 2010-2013 Google
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

namespace operations_research {
namespace Sat {
// Index of a variable.
DEFINE_INT_TYPE(Variable, int);

// A literal (pair variable, boolean).
DEFINE_INT_TYPE(Literal, int);

inline Literal MakeLiteral(Variable var, bool sign) {
  return Literal(2 * var.value() + static_cast<int>(sign));
}

inline Literal Negated(Literal p) { return Literal(p.value() ^ 1); }
inline bool Sign(Literal p) { return p.value() & 1; }
inline Variable Var(Literal p) { return Variable(p.value() >> 1); }

static const Literal kUndefinedLiteral = Literal(-2);
static const Literal kErrorLiteral = Literal(-1);

// Lifted boolean class with undefined value.
DEFINE_INT_TYPE(Boolean, uint8);
static const Boolean kTrue = Boolean((uint8) 0);
static const Boolean kFalse = Boolean((uint8) 1);
static const Boolean kUndefined = Boolean((uint8) 2);

inline Boolean MakeBoolean(bool x) { return Boolean(!x); }
inline Boolean Xor(Boolean a, bool b) {
  return Boolean((uint8)(a.value() ^ (uint8) b));
}
inline std::string ToString(Boolean b) {
  switch (b.value()) {
    case 0:
      return "true";
    case 1:
      return "false";
    case 2:
      return "undefined";
    default:
      return "error";
  }
}

// Clause -- a simple class for representing a clause:
class Clause {
 public:
  explicit Clause(std::vector<Literal>* const ps) { literals_.swap(*ps); }

  int size() const { return literals_.size(); }

  Literal& operator[](int i) { return literals_[i]; }
  Literal operator[](int i) const { return literals_[i]; }

 private:
  std::vector<Literal> literals_;
};

// A watcher represent a clause attached to a literal.
struct Watcher {
  Watcher() : blocker(kUndefinedLiteral) {}
  Watcher(Clause* const cr, Literal p) : clause(cr), blocker(p) {}

  Clause* clause;
  Literal blocker;
};

// Solver
class Solver {
 public:
  Solver() : ok_(true), qhead_(0), num_vars_() {}
  virtual ~Solver() {}

  // Add a new variable.
  Variable NewVariable() {
    Variable v = IncrementVariableCounter();
    watches_.resize(2 * v.value() + 2);
    implies_.resize(2 * v.value() + 2);
    assignment_.push_back(kUndefined);
    return v;
  }

  // Add a clause to the solver.
  bool AddClause(std::vector<Literal>* const ps);
  // Add the empty clause, making the solver contradictory.
  bool AddEmptyClause() {
    temporary_add_vector_.clear();
    return AddClause(&temporary_add_vector_);
  }
  // Add a unit clause to the solver.
  bool AddClause(Literal p) {
    temporary_add_vector_.clear();
    temporary_add_vector_.push_back(p);
    return AddClause(&temporary_add_vector_);
  }
  // Add a binary clause to the solver.
  bool AddClause(Literal p, Literal q) {
    temporary_add_vector_.clear();
    temporary_add_vector_.push_back(p);
    temporary_add_vector_.push_back(q);
    return AddClause(&temporary_add_vector_);
  }
  // Add a ternary clause to the solver.
  bool AddClause(Literal p, Literal q, Literal r) {
    temporary_add_vector_.clear();
    temporary_add_vector_.push_back(p);
    temporary_add_vector_.push_back(q);
    temporary_add_vector_.push_back(r);
    return AddClause(&temporary_add_vector_);
  }

  // Incremental propagation.
  bool InitPropagator() {
    touched_variables_.clear();
    return !ok_;
  }

  // Backtrack until a certain level.
  void BacktrackTo(int level) {
    if (TrailMarker() > level) {
      for (int c = trail_.size() - 1; c >= trail_markers_[level]; c--) {
        Variable x = Var(trail_[c]);
        assignment_[x] = kUndefined;
      }
      qhead_ = trail_markers_[level];
      trail_.resize(trail_markers_[level]);
      trail_markers_.resize(level);
    }
  }

  // Gives the current decisionlevel.
  int TrailMarker() const { return trail_markers_.size(); }
  std::vector<Literal> touched_variables_;

  // The current value of a variable.
  Boolean Value(Variable x) const { return assignment_[x]; }
  // The current value of a literal.
  Boolean Value(Literal p) const {
    const Boolean b = assignment_[Var(p)];
    return b == kUndefined ? kUndefined : Xor(b, Sign(p));
  }
  // Number of clauses.
  int NumClauses() const { return clauses.size(); }
  // Number of sat variables.
  int NumVariables() const { return num_vars_; }
  // Propagates one literal, returns true if successful, false in case
  // of failure.
  bool PropagateOneLiteral(Literal lit);

 private:
  Variable IncrementVariableCounter() { return Variable(num_vars_++); }

  // Begins a new decision level.
  void PushTrailMarker() { trail_markers_.push_back(trail_.size()); }

  // Enqueue a literal. Assumes value of literal is undefined.
  void UncheckedEnqueue(Literal p) {
    DCHECK_EQ(Value(p), kUndefined);
    if (assignment_[Var(p)] == kUndefined) {
      touched_variables_.push_back(p);
    }
    assignment_[Var(p)] = Sign(p) ? kFalse : kTrue;
    trail_.push_back(p);
  }

  // Test if fact 'p' contradicts current state, Enqueue otherwise.
  bool Enqueue(Literal p) {
    return Value(p) != kUndefined ? Value(p) != kFalse
                                  : (UncheckedEnqueue(p), true);
  }

  // Attach a clause to watcher lists.
  void AttachClause(Clause* cr) {
    const Clause& c = *cr;
    DCHECK_GT(c.size(), 1);
    watches_[Negated(c[0])].push_back(Watcher(cr, c[1]));
    watches_[Negated(c[1])].push_back(Watcher(cr, c[0]));
  }

  // Perform unit propagation. returns true upon success.
  bool Propagate();

  // If false, the constraints are already unsatisfiable. No part of
  // the solver state may be used!
  bool ok_;
  // List of problem clauses.
  std::vector<Clause*> clauses;
  // 'watches_[lit]' is a list of constraints watching 'lit'(will go
  // there if literal becomes true).
  ITIVector<Literal, std::vector<Watcher>> watches_;

  // implies_[lit] is a list of literals to set to true if 'lit' becomes true.
  ITIVector<Literal, std::vector<Literal>> implies_;
  // The current assignments.
  ITIVector<Variable, Boolean> assignment_;
  // Assignment stack; stores all assigments made in the order they
  // were made.
  std::vector<Literal> trail_;
  // Separator indices for different decision levels in 'trail_'.
  std::vector<int> trail_markers_;
  // Head of queue(as index into the trail_.
  int qhead_;
  // Number of variables
  int num_vars_;

  std::vector<Literal> temporary_add_vector_;
};

// Creates a new SAT variable in the solver. If 'decision' is cleared,
// variable will not be used as a decision variable (NOTE! This has
// effects on the meaning of a SATISFIABLE result).
//
bool Solver::AddClause(std::vector<Literal>* const ps) {
  DCHECK_EQ(0, TrailMarker());
  if (!ok_) return false;

  // Check if clause is satisfied and remove false/duplicate literals:
  std::sort(ps->begin(), ps->end());
  Literal p = kUndefinedLiteral;
  int j = 0;
  for (int i = 0; i < ps->size(); ++i) {
    if (Value((*ps)[i]) == kTrue || (*ps)[i] == Negated(p)) {
      return true;
    } else if (Value((*ps)[i]) != kFalse && (*ps)[i] != p) {
      (*ps)[j++] = p = (*ps)[i];
    }
  }
  ps->resize(j);

  switch (ps->size()) {
    case 0: {
      return (ok_ = false);
    }
    case 1: {
      UncheckedEnqueue((*ps)[0]);
      return (ok_ = Propagate());
    }
    case 2: {
      Literal l0 = (*ps)[0];
      Literal l1 = (*ps)[1];
      implies_[Negated(l0)].push_back(l1);
      implies_[Negated(l1)].push_back(l0);
      break;
    }
    default: {
      Clause* const cr = new Clause(ps);
      clauses.push_back(cr);
      AttachClause(cr);
    }
  }
  return true;
}

bool Solver::Propagate() {
  bool result = true;
  while (qhead_ < trail_.size()) {
    const Literal p = trail_[qhead_++];
    // 'p' is enqueued fact to propagate.
    // Propagate the implies first.
    const std::vector<Literal>& to_add = implies_[p];
    for (int i = 0; i < to_add.size(); ++i) {
      if (!Enqueue(to_add[i])) {
        return false;
      }
    }

    // Propagate watched clauses.
    std::vector<Watcher>& ws = watches_[p];

    int i = 0;
    int j = 0;
    while (i < ws.size()) {
      // Try to avoid inspecting the clause:
      Literal blocker = ws[i].blocker;
      if (Value(blocker) == kTrue) {
        ws[j++] = ws[i++];
        continue;
      }

      // Make sure the false literal is data[1]:
      Clause* const cr = ws[i].clause;
      Clause& c = *cr;
      const Literal false_lit = Negated(p);
      if (c[0] == false_lit) {
        c[0] = c[1], c[1] = false_lit;
      }
      DCHECK_EQ(c[1], false_lit);
      i++;

      // If 0th watch is true, then clause is already satisfied.
      const Literal first = c[0];
      const Watcher w = Watcher(cr, first);
      if (first != blocker && Value(first) == kTrue) {
        ws[j++] = w;
        continue;
      }

      // Look for new watch:
      bool cont = false;
      for (int k = 2; k < c.size(); k++) {
        if (Value(c[k]) != kFalse) {
          c[1] = c[k];
          c[k] = false_lit;
          watches_[Negated(c[1])].push_back(w);
          cont = true;
          break;
        }
      }

      // Did not find watch -- clause is unit under assignment:
      if (!cont) {
        ws[j++] = w;
        if (Value(first) == kFalse) {
          result = false;
          qhead_ = trail_.size();
          // Copy the remaining watches_:
          while (i < ws.size()) {
            ws[j++] = ws[i++];
          }
        } else {
          UncheckedEnqueue(first);
        }
      }
    }
    ws.resize(j);
  }
  return result;
}

bool Solver::PropagateOneLiteral(Literal lit) {
  DCHECK(ok_);
  touched_variables_.clear();
  if (!Propagate()) {
    return false;
  }
  if (Value(lit) == kTrue) {
    // Dummy decision level:
    PushTrailMarker();
    return true;
  } else if (Value(lit) == kFalse) {
    return false;
  }
  PushTrailMarker();
  // Unchecked enqueue
  DCHECK_EQ(Value(lit), kUndefined);
  assignment_[Var(lit)] = MakeBoolean(!Sign(lit));
  trail_.push_back(lit);
  if (!Propagate()) {
    return false;
  }
  return true;
}
}  // namespace Sat

class SatPropagator : public Constraint {
 public:
  explicit SatPropagator(Solver* const solver)
      : Constraint(solver), sat_trail_(0) {}

  ~SatPropagator() {}

  bool Check(IntExpr* const expr) const {
    IntVar* expr_var = nullptr;
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

  Sat::Literal Literal(IntExpr* const expr) {
    IntVar* expr_var = nullptr;
    bool expr_negated = false;
    if (!solver()->IsBooleanVar(expr, &expr_var, &expr_negated)) {
      return Sat::kErrorLiteral;
    }
#ifdef SAT_DEBUG
    VLOG(2) << "  - SAT: Parse " << expr->DebugString() << " to "
            << expr_var->DebugString() << "/" << expr_negated;
#endif
    if (ContainsKey(indices_, expr_var)) {
      return Sat::MakeLiteral(indices_[expr_var], !expr_negated);
    } else {
      const Sat::Variable var = sat_.NewVariable();
      DCHECK_EQ(vars_.size(), var.value());
      vars_.push_back(expr_var);
      indices_[expr_var] = var;
      Sat::Literal lit = Sat::MakeLiteral(var, !expr_negated);
#ifdef SAT_DEBUG
      VLOG(2) << "    - created var = " << var.value()
              << ", lit = " << lit.value();
#endif
      return lit;
    }
  }

  void VariableBound(int index) {
    if (sat_trail_.Value() < sat_.TrailMarker()) {
#ifdef SAT_DEBUG
      VLOG(2) << "After failure, sat_trail = " << sat_trail_.Value()
              << ", sat decision level = " << sat_.TrailMarker();
#endif
      sat_.BacktrackTo(sat_trail_.Value());
      DCHECK_EQ(sat_trail_.Value(), sat_.TrailMarker());
    }
    const Sat::Variable var = Sat::Variable(index);
#ifdef SAT_DEBUG
    VLOG(2) << "VariableBound: " << vars_[index]->DebugString()
            << " with sat variable " << var;
#endif
    const bool new_value = vars_[index]->Value() != 0;
    Sat::Literal lit = Sat::MakeLiteral(var, new_value);
#ifdef SAT_DEBUG
    VLOG(2) << " - enqueue lit = " << lit.value() << " at depth "
            << sat_trail_.Value();
#endif
    if (!sat_.PropagateOneLiteral(lit)) {
#ifdef SAT_DEBUG
      VLOG(2) << " - failure detected, should backtrack";
#endif
      solver()->Fail();
    } else {
      sat_trail_.SetValue(solver(), sat_.TrailMarker());
      for (int i = 0; i < sat_.touched_variables_.size(); ++i) {
        const Sat::Literal lit = sat_.touched_variables_[i];
        const Sat::Variable var = Sat::Var(lit);
        const bool assigned_bool = Sat::Sign(lit);
#ifdef SAT_DEBUG
        VLOG(2) << " - var " << var << " was assigned to " << assigned_bool
                << " from literal " << lit.value();
#endif
        demons_[var.value()]->inhibit(solver());
        vars_[var.value()]->SetValue(assigned_bool);
      }
    }
  }

  virtual void Post() {
    demons_.resize(vars_.size());
    for (int i = 0; i < vars_.size(); ++i) {
      demons_[i] = MakeConstraintDemon1(
          solver(), this, &SatPropagator::VariableBound, "VariableBound", i);
      vars_[i]->WhenDomain(demons_[i]);
    }
  }

  virtual void InitialPropagate() {
#ifdef SAT_DEBUG
    VLOG(2) << "Initial propagation on sat solver";
#endif
    sat_.InitPropagator();
    ApplyEarlyDeductions();
    for (int i = 0; i < vars_.size(); ++i) {
      IntVar* const var = vars_[i];
      if (var->Bound()) {
        VariableBound(i);
      }
    }
#ifdef SAT_DEBUG
    VLOG(2) << " - done";
#endif
  }

  // Add a clause to the solver, clears the vector.
  bool AddClause(std::vector<Sat::Literal>* const lits) {
    bool result = sat_.AddClause(lits);
    StoreEarlyDeductions();
    return result;
  }

  // Add the empty clause, making the solver contradictory.
  bool AddEmptyClause() { return sat_.AddEmptyClause(); }

  // Add a unit clause to the solver.
  bool AddClause(Sat::Literal p) {
    bool result = sat_.AddClause(p);
    StoreEarlyDeductions();
    return result;
  }

  // Add a binary clause to the solver.
  bool AddClause(Sat::Literal p, Sat::Literal q) {
    bool result = sat_.AddClause(p, q);
    StoreEarlyDeductions();
    return result;
  }

  // Add a ternary clause to the solver.
  bool AddClause(Sat::Literal p, Sat::Literal q, Sat::Literal r) {
    bool result = sat_.AddClause(p, q, r);
    StoreEarlyDeductions();
    return result;
  }

  virtual std::string DebugString() const {
    return StringPrintf("SatConstraint(%d variables, %d clauses)",
                        sat_.NumVariables(), sat_.NumClauses());
  }

  void Accept(ModelVisitor* const visitor) const {
    VLOG(1) << "Should Not Be Visited";
  }

 private:
  void StoreEarlyDeductions() {
    for (int i = 0; i < sat_.touched_variables_.size(); ++i) {
      const Sat::Literal lit = sat_.touched_variables_[i];
#ifdef SAT_DEBUG
      VLOG(2) << "Postponing deduction " << lit.value();
#endif
      early_deductions_.push_back(lit);
    }
    sat_.touched_variables_.clear();
  }

  void ApplyEarlyDeductions() {
    for (int i = 0; i < early_deductions_.size(); ++i) {
      const Sat::Literal lit = early_deductions_[i];
      const Sat::Variable var = Sat::Var(lit);
      const bool assigned_bool = Sat::Sign(lit);
#ifdef SAT_DEBUG
      VLOG(2) << " - var " << var << " (" << vars_[var.value()]->DebugString()
              << ") was early assigned to " << assigned_bool
              << " from literal " << lit.value();
#endif
      demons_[var.value()]->inhibit(solver());
      vars_[var.value()]->SetValue(assigned_bool);
    }
  }

  Sat::Solver sat_;
  std::vector<IntVar*> vars_;
  hash_map<IntVar*, Sat::Variable> indices_;
  std::vector<Sat::Literal> bound_literals_;
  NumericalRev<int> sat_trail_;
  std::vector<Demon*> demons_;
  std::vector<Sat::Literal> early_deductions_;
};

void DeclareVariable(SatPropagator* const sat, IntVar* const var) {
  CHECK(sat->Check(var));
  sat->Literal(var);
}

bool AddBoolEq(SatPropagator* const sat, IntExpr* const left,
               IntExpr* const right) {
  if (!sat->Check(left) || !sat->Check(right)) {
    return false;
  }
  Sat::Literal left_lit = sat->Literal(left);
  Sat::Literal right_lit = sat->Literal(right);
  sat->AddClause(Negated(left_lit), right_lit);
  sat->AddClause(left_lit, Negated(right_lit));
  return true;
}

bool AddBoolLe(SatPropagator* const sat, IntExpr* const left,
               IntExpr* const right) {
  if (!sat->Check(left) || !sat->Check(right)) {
    return false;
  }
  Sat::Literal left_lit = sat->Literal(left);
  Sat::Literal right_lit = sat->Literal(right);
  sat->AddClause(Negated(left_lit), right_lit);
  return true;
}

bool AddBoolNot(SatPropagator* const sat, IntExpr* const left,
                IntExpr* const right) {
  if (!sat->Check(left) || !sat->Check(right)) {
    return false;
  }
  Sat::Literal left_lit = sat->Literal(left);
  Sat::Literal right_lit = sat->Literal(right);
  sat->AddClause(Negated(left_lit), Negated(right_lit));
  sat->AddClause(left_lit, right_lit);
  return true;
}

bool AddBoolOrArrayEqVar(SatPropagator* const sat, const std::vector<IntVar*>& vars,
                         IntExpr* const target) {
  if (!sat->Check(vars) || !sat->Check(target)) {
    return false;
  }
  Sat::Literal target_lit = sat->Literal(target);
  std::vector<Sat::Literal> lits(vars.size() + 1);
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->Literal(vars[i]);
  }
  lits[vars.size()] = Negated(target_lit);
  sat->AddClause(&lits);
  for (int i = 0; i < vars.size(); ++i) {
    sat->AddClause(target_lit, Negated(sat->Literal(vars[i])));
  }
  return true;
}

bool AddBoolAndArrayEqVar(SatPropagator* const sat, const std::vector<IntVar*>& vars,
                          IntExpr* const target) {
  if (!sat->Check(vars) || !sat->Check(target)) {
    return false;
  }
  Sat::Literal target_lit = sat->Literal(target);
  std::vector<Sat::Literal> lits(vars.size() + 1);
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = Negated(sat->Literal(vars[i]));
  }
  lits[vars.size()] = target_lit;
  sat->AddClause(&lits);
  for (int i = 0; i < vars.size(); ++i) {
    sat->AddClause(Negated(target_lit), sat->Literal(vars[i]));
  }
  return true;
}

bool AddSumBoolArrayGreaterEqVar(SatPropagator* const sat,
                                 const std::vector<IntVar*>& vars,
                                 IntExpr* const target) {
  if (!sat->Check(vars) || !sat->Check(target)) {
    return false;
  }
  Sat::Literal target_lit = sat->Literal(target);
  std::vector<Sat::Literal> lits(vars.size() + 1);
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->Literal(vars[i]);
  }
  lits[vars.size()] = Negated(target_lit);
  sat->AddClause(&lits);
  return true;
}

bool AddSumBoolArrayLessEqKVar(SatPropagator* const sat,
                               const std::vector<IntVar*>& vars,
                               IntExpr* const target) {
  if (vars.size() == 1) {
    return AddBoolLe(sat, vars[0], target);
  }
  if (!sat->Check(vars) || !sat->Check(target)) {
    return false;
  }
  IntVar* const extra = target->solver()->MakeBoolVar();
  Sat::Literal target_lit = sat->Literal(target);
  Sat::Literal extra_lit = sat->Literal(extra);
  std::vector<Sat::Literal> lits(vars.size() + 1);
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->Literal(vars[i]);
  }
  lits[vars.size()] = Negated(extra_lit);
  sat->AddClause(&lits);
  for (int i = 0; i < vars.size(); ++i) {
    sat->AddClause(extra_lit, Negated(sat->Literal(vars[i])));
  }
  sat->AddClause(Negated(extra_lit), target_lit);
  return true;
}

bool AddBoolOrEqVar(SatPropagator* const sat, IntExpr* const left,
                    IntExpr* const right, IntExpr* const target) {
  if (!sat->Check(left) || !sat->Check(right) || !sat->Check(target)) {
    return false;
  }
  Sat::Literal left_lit = sat->Literal(left);
  Sat::Literal right_lit = sat->Literal(right);
  Sat::Literal target_lit = sat->Literal(target);
  sat->AddClause(left_lit, right_lit, Negated(target_lit));
  sat->AddClause(Negated(left_lit), target_lit);
  sat->AddClause(Negated(right_lit), target_lit);
  return true;
}

bool AddBoolAndEqVar(SatPropagator* const sat, IntExpr* const left,
                     IntExpr* const right, IntExpr* const target) {
  if (!sat->Check(left) || !sat->Check(right) || !sat->Check(target)) {
    return false;
  }
  Sat::Literal left_lit = sat->Literal(left);
  Sat::Literal right_lit = sat->Literal(right);
  Sat::Literal target_lit = sat->Literal(target);
  sat->AddClause(Negated(left_lit), Negated(right_lit), target_lit);
  sat->AddClause(left_lit, Negated(target_lit));
  sat->AddClause(right_lit, Negated(target_lit));
  return true;
}

bool AddBoolIsEqVar(SatPropagator* const sat, IntExpr* const left,
                    IntExpr* const right, IntExpr* const target) {
  if (!sat->Check(left) || !sat->Check(right) || !sat->Check(target)) {
    return false;
  }
  Sat::Literal left_lit = sat->Literal(left);
  Sat::Literal right_lit = sat->Literal(right);
  Sat::Literal target_lit = sat->Literal(target);
  sat->AddClause(Negated(left_lit), right_lit, Negated(target_lit));
  sat->AddClause(left_lit, Negated(right_lit), Negated(target_lit));
  sat->AddClause(left_lit, right_lit, target_lit);
  sat->AddClause(Negated(left_lit), Negated(right_lit), target_lit);
  return true;
}

bool AddBoolIsNEqVar(SatPropagator* const sat, IntExpr* const left,
                     IntExpr* const right, IntExpr* const target) {
  if (!sat->Check(left) || !sat->Check(right) || !sat->Check(target)) {
    return false;
  }
  Sat::Literal left_lit = sat->Literal(left);
  Sat::Literal right_lit = sat->Literal(right);
  Sat::Literal target_lit = sat->Literal(target);
  sat->AddClause(Negated(left_lit), right_lit, target_lit);
  sat->AddClause(left_lit, Negated(right_lit), target_lit);
  sat->AddClause(left_lit, right_lit, Negated(target_lit));
  sat->AddClause(Negated(left_lit), Negated(right_lit), Negated(target_lit));
  return true;
}

bool AddBoolIsLeVar(SatPropagator* const sat, IntExpr* const left,
                    IntExpr* const right, IntExpr* const target) {
  if (!sat->Check(left) || !sat->Check(right) || !sat->Check(target)) {
    return false;
  }
  Sat::Literal left_lit = sat->Literal(left);
  Sat::Literal right_lit = sat->Literal(right);
  Sat::Literal target_lit = sat->Literal(target);
  sat->AddClause(Negated(left_lit), right_lit, Negated(target_lit));
  sat->AddClause(left_lit, target_lit);
  sat->AddClause(Negated(right_lit), target_lit);
  return true;
}

bool AddBoolOrArrayEqualTrue(SatPropagator* const sat,
                             const std::vector<IntVar*>& vars) {
  if (!sat->Check(vars)) {
    return false;
  }
  std::vector<Sat::Literal> lits(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->Literal(vars[i]);
  }
  sat->AddClause(&lits);
  return true;
}

bool AddBoolAndArrayEqualFalse(SatPropagator* const sat,
                               const std::vector<IntVar*>& vars) {
  if (!sat->Check(vars)) {
    return false;
  }
  std::vector<Sat::Literal> lits(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = Negated(sat->Literal(vars[i]));
  }
  sat->AddClause(&lits);
  return true;
}

bool AddAtMostOne(SatPropagator* const sat, const std::vector<IntVar*>& vars) {
  if (!sat->Check(vars)) {
    return false;
  }
  std::vector<Sat::Literal> lits(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = Negated(sat->Literal(vars[i]));
  }
  for (int i = 0; i < lits.size() - 1; ++i) {
    for (int j = i + 1; j < lits.size(); ++j) {
      sat->AddClause(lits[i], lits[j]);
    }
  }
  return true;
}

bool AddAtMostNMinusOne(SatPropagator* const sat, const std::vector<IntVar*>& vars) {
  if (!sat->Check(vars)) {
    return false;
  }
  std::vector<Sat::Literal> lits(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = Negated(sat->Literal(vars[i]));
  }
  sat->AddClause(&lits);
  return true;
}

bool AddArrayXor(SatPropagator* const sat, const std::vector<IntVar*>& vars) {
  if (!sat->Check(vars)) {
    return false;
  }
  return false;
}

SatPropagator* MakeSatPropagator(Solver* const solver) {
  return solver->RevAlloc(new SatPropagator(solver));
}
}  // namespace operations_research

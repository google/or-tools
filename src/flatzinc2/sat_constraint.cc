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
#include "flatzinc2/sat_constraint.h"

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

// A literal, which encode the pair (variable, boolean) as an int, see
// MakeLiteral().
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
static const Boolean kTrue = Boolean(0);
static const Boolean kFalse = Boolean(1);
static const Boolean kUndefined = Boolean(2);

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

// Clause -- a simple class for representing a list of literals.
class Clause {
 public:
  // Note the special 'swap' behavior of the constructor.
  explicit Clause(std::vector<Literal>* ps) { literals_.swap(*ps); }

  int size() const { return literals_.size(); }

  Literal& operator[](int i) { return literals_[i]; }
  Literal operator[](int i) const { return literals_[i]; }

 private:
  std::vector<Literal> literals_;
};

// A watcher represent a clause attached to a literal.
struct Watcher {
  Watcher() : blocker(kUndefinedLiteral) {}
  Watcher(Clause* cr, Literal p) : clause(cr), blocker(p) {}

  Clause* clause;
  Literal blocker;
};

// SAT Solver.
//
// This is not a full-fledged solver, it allows a client to
// enqueue/backtrack decisions and it just takes care of propagating
// them efficiently and deciding if the current decisions lead to an
// infeasible problem.
class Solver {
 public:
  Solver() : ok_(true), queue_head_(0), num_vars_() {}
  virtual ~Solver() {}

  // Adds a new variable to the solver and returns its index.  This
  // must be called before adding any clause with a literal referring
  // to this variable.
  Variable NewVariable() {
    Variable v = IncrementVariableCounter();
    watches_.resize(2 * v.value() + 2);
    implies_.resize(2 * v.value() + 2);
    assignment_.push_back(kUndefined);
    return v;
  }

  // Add a clause to the solver. Returns true if the problem not proven
  // contradictory after the addition.
  bool AddClause(std::vector<Literal>* clause);
  // Add the empty clause, making the solver contradictory.
  bool AddEmptyClause() {
    temp_clause_.clear();
    return AddClause(&temp_clause_);
  }
  // Add a unit clause to the solver.
  bool AddClause(Literal p) {
    temp_clause_.clear();
    temp_clause_.push_back(p);
    return AddClause(&temp_clause_);
  }
  // Add a binary clause to the solver.
  bool AddClause(Literal p, Literal q) {
    temp_clause_.clear();
    temp_clause_.push_back(p);
    temp_clause_.push_back(q);
    return AddClause(&temp_clause_);
  }
  // Add a ternary clause to the solver.
  bool AddClause(Literal p, Literal q, Literal r) {
    temp_clause_.clear();
    temp_clause_.push_back(p);
    temp_clause_.push_back(q);
    temp_clause_.push_back(r);
    return AddClause(&temp_clause_);
  }

  // Initialize the propagator before processing the queue.
  void ClearTouchedVariables() { touched_variables_.clear(); }

  // List of touched variables since last propagate.
  const std::vector<Literal>& TouchedVariables() const {
    return touched_variables_;
  }

  // Backtrack until a certain level.
  void BacktrackTo(int level) {
    if (CurrentDecisionLevel() > level) {
      for (int c = trail_.size() - 1; c >= trail_markers_[level]; c--) {
        Variable x = Var(trail_[c]);
        assignment_[x] = kUndefined;
      }
      queue_head_ = trail_markers_[level];
      trail_.resize(trail_markers_[level]);
      trail_markers_.resize(level);
    }
  }

  // Gives the current decisionlevel.
  int CurrentDecisionLevel() const { return trail_markers_.size(); }

  // The current value of a variable.
  Boolean Value(Variable x) const { return assignment_[x]; }
  // The current value of a literal.
  Boolean Value(Literal p) const {
    const Boolean b = assignment_[Var(p)];
    return b == kUndefined ? kUndefined : Xor(b, Sign(p));
  }
  // Number of clauses.
  int NumClauses() const { return clauses_.size(); }
  // Number of sat variables.
  int NumVariables() const { return num_vars_; }
  // Propagates one literal. Returns true if no conflict was detected,
  // false if the SAT problem can't be satisfied with this new
  // decision.
  bool PropagateOneLiteral(Literal literal);

 private:
  Variable IncrementVariableCounter() { return Variable(num_vars_++); }

  // Begins a new decision level.
  void PushCurrentDecisionLevel() { trail_markers_.push_back(trail_.size()); }

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
  // List of problem clauses. Ownership belong to the sat solver.
  std::vector<Clause*> clauses_;
  // 'watches_[literal]' is a list of clauses watching 'literal'(will go
  // there if literal becomes true).
  ITIVector<Literal, std::vector<Watcher>> watches_;

  // implies_[literal] is a list of literals to set to true if
  // 'literal' becomes true.
  ITIVector<Literal, std::vector<Literal>> implies_;
  // The current assignments.
  ITIVector<Variable, Boolean> assignment_;
  // Assignment stack; stores all assigments made in the order they
  // were made.
  std::vector<Literal> trail_;
  // Separator indices for different decision levels in 'trail_'.
  std::vector<int> trail_markers_;
  // Head of queue(as index into the trail_.
  int queue_head_;
  // Number of variables
  int num_vars_;
  // This is a temporary vector used by the AddClause() functions.
  std::vector<Literal> temp_clause_;
  // Touched variables since last propagate.
  std::vector<Literal> touched_variables_;
};

bool Solver::AddClause(std::vector<Literal>* clause) {
  DCHECK_EQ(0, CurrentDecisionLevel());
  if (!ok_) return false;

  // Check if clause is satisfied and remove false/duplicate literals:
  std::sort(clause->begin(), clause->end());
  Literal p = kUndefinedLiteral;
  int j = 0;
  for (int i = 0; i < clause->size(); ++i) {
    if (Value((*clause)[i]) == kTrue || (*clause)[i] == Negated(p)) {
      return true;
    } else if (Value((*clause)[i]) != kFalse && (*clause)[i] != p) {
      (*clause)[j++] = p = (*clause)[i];
    }
  }
  clause->resize(j);

  switch (clause->size()) {
    case 0: {
      ok_ = false;
      return false;
    }
    case 1: {
      UncheckedEnqueue((*clause)[0]);
      ok_ = Propagate();
      return ok_;
    }
    case 2: {
      Literal l0 = (*clause)[0];
      Literal l1 = (*clause)[1];
      implies_[Negated(l0)].push_back(l1);
      implies_[Negated(l1)].push_back(l0);
      break;
    }
    default: {
      Clause* const cr = new Clause(clause);
      clauses_.push_back(cr);
      AttachClause(cr);
    }
  }
  return true;
}

bool Solver::Propagate() {
  bool result = true;
  while (queue_head_ < trail_.size()) {
    const Literal propagated_fact = trail_[queue_head_++];
    // 'propagated_fact' is enqueued fact to propagate.
    // Propagate the implies first.
    const std::vector<Literal>& to_add = implies_[propagated_fact];
    for (int i = 0; i < to_add.size(); ++i) {
      if (!Enqueue(to_add[i])) {
        return false;
      }
    }

    // Propagate watched clauses.
    std::vector<Watcher>& watchers = watches_[propagated_fact];

    int current = 0;
    int filled = 0;
    while (current < watchers.size()) {
      // Try to avoid inspecting the clause:
      Literal blocker = watchers[current].blocker;
      if (Value(blocker) == kTrue) {
        watchers[filled++] = watchers[current++];
        continue;
      }

      // Make sure the false literal is clause[1]:
      Clause& clause = *watchers[current].clause;
      const Literal negated_propagated_fact = Negated(propagated_fact);
      if (clause[0] == negated_propagated_fact) {
        clause[0] = clause[1], clause[1] = negated_propagated_fact;
      }
      DCHECK_EQ(clause[1], negated_propagated_fact);
      current++;

      // If 0th watch is true, then clause is already satisfied.
      const Literal first = clause[0];
      const Watcher w = Watcher(&clause, first);
      if (first != blocker && Value(first) == kTrue) {
        watchers[filled++] = w;
        continue;
      }

      // Look for new watch:
      bool contradiction = false;
      for (int k = 2; k < clause.size(); k++) {
        if (Value(clause[k]) != kFalse) {
          clause[1] = clause[k];
          clause[k] = negated_propagated_fact;
          watches_[Negated(clause[1])].push_back(w);
          contradiction = true;
          break;
        }
      }

      // Did not find watch -- clause is unit under assignment:
      if (!contradiction) {
        watchers[filled++] = w;
        if (Value(first) == kFalse) {
          result = false;
          queue_head_ = trail_.size();
          // Copy the remaining watches_:
          while (current < watchers.size()) {
            watchers[filled++] = watchers[current++];
          }
        } else {
          UncheckedEnqueue(first);
        }
      }
    }
    watchers.resize(filled);
  }
  return result;
}

bool Solver::PropagateOneLiteral(Literal literal) {
  DCHECK(ok_);
  ClearTouchedVariables();
  if (!Propagate()) {
    return false;
  }
  if (Value(literal) == kTrue) {
    // Dummy decision level:
    PushCurrentDecisionLevel();
    return true;
  } else if (Value(literal) == kFalse) {
    return false;
  }
  PushCurrentDecisionLevel();
  // Unchecked enqueue
  DCHECK_EQ(Value(literal), kUndefined);
  assignment_[Var(literal)] = MakeBoolean(!Sign(literal));
  trail_.push_back(literal);
  if (!Propagate()) {
    return false;
  }
  return true;
}
}  // namespace Sat

// Constraint that tight together boolean variables in the CP solver to sat
// variables and clauses.
class SatPropagator : public Constraint {
 public:
  explicit SatPropagator(Solver* solver) : Constraint(solver), sat_trail_(0) {}

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
  Sat::Literal Literal(IntExpr* expr) {
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
      Sat::Literal literal = Sat::MakeLiteral(var, !expr_negated);
#ifdef SAT_DEBUG
      VLOG(2) << "    - created var = " << var.value()
              << ", literal = " << literal.value();
#endif
      return literal;
    }
  }

  // This method is called during the processing of the CP solver queue when
  // a bolean variable is bound.
  void VariableBound(int index) {
    if (sat_trail_.Value() < sat_.CurrentDecisionLevel()) {
#ifdef SAT_DEBUG
      VLOG(2) << "After failure, sat_trail = " << sat_trail_.Value()
              << ", sat decision level = " << sat_.CurrentDecisionLevel();
#endif
      sat_.BacktrackTo(sat_trail_.Value());
      DCHECK_EQ(sat_trail_.Value(), sat_.CurrentDecisionLevel());
    }
    const Sat::Variable var = Sat::Variable(index);
#ifdef SAT_DEBUG
    VLOG(2) << "VariableBound: " << vars_[index]->DebugString()
            << " with sat variable " << var;
#endif
    const bool new_value = vars_[index]->Value() != 0;
    Sat::Literal literal = Sat::MakeLiteral(var, new_value);
#ifdef SAT_DEBUG
    VLOG(2) << " - enqueue literal = " << literal.value() << " at depth "
            << sat_trail_.Value();
#endif
    if (!sat_.PropagateOneLiteral(literal)) {
#ifdef SAT_DEBUG
      VLOG(2) << " - failure detected, should backtrack";
#endif
      solver()->Fail();
    } else {
      sat_trail_.SetValue(solver(), sat_.CurrentDecisionLevel());
      for (const Sat::Literal literal : sat_.TouchedVariables()) {
        const Sat::Variable var = Sat::Var(literal);
        const bool assigned_bool = Sat::Sign(literal);
#ifdef SAT_DEBUG
        VLOG(2) << " - var " << var << " was assigned to " << assigned_bool
                << " from literal " << literal.value();
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
    sat_.ClearTouchedVariables();
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

  // Simple wrapping of the same methods in the underlying solver class.
  bool AddClause(std::vector<Sat::Literal>* literals) {
    bool result = sat_.AddClause(literals);
    StoreEarlyDeductions();
    return result;
  }

  bool AddEmptyClause() { return sat_.AddEmptyClause(); }

  bool AddClause(Sat::Literal p) {
    bool result = sat_.AddClause(p);
    StoreEarlyDeductions();
    return result;
  }

  bool AddClause(Sat::Literal p, Sat::Literal q) {
    bool result = sat_.AddClause(p, q);
    StoreEarlyDeductions();
    return result;
  }

  bool AddClause(Sat::Literal p, Sat::Literal q, Sat::Literal r) {
    bool result = sat_.AddClause(p, q, r);
    StoreEarlyDeductions();
    return result;
  }

  virtual std::string DebugString() const {
    return StringPrintf("SatConstraint(%d variables, %d clauses)",
                        sat_.NumVariables(), sat_.NumClauses());
  }

  void Accept(ModelVisitor* visitor) const {
    VLOG(1) << "Should Not Be Visited";
  }

 private:
  void StoreEarlyDeductions() {
    for (const Sat::Literal literal : sat_.TouchedVariables()) {
#ifdef SAT_DEBUG
      VLOG(2) << "Postponing deduction " << literal.value();
#endif
      early_deductions_.push_back(literal);
    }
    sat_.ClearTouchedVariables();
  }

  void ApplyEarlyDeductions() {
    for (int i = 0; i < early_deductions_.size(); ++i) {
      const Sat::Literal literal = early_deductions_[i];
      const Sat::Variable var = Sat::Var(literal);
      const bool assigned_bool = Sat::Sign(literal);
#ifdef SAT_DEBUG
      VLOG(2) << " - var " << var << " (" << vars_[var.value()]->DebugString()
              << ") was early assigned to " << assigned_bool << " from literal "
              << literal.value();
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

void DeclareVariable(SatPropagator* sat, IntVar* var) {
  CHECK(sat->IsExpressionBoolean(var));
  sat->Literal(var);
}

bool AddBoolEq(SatPropagator* sat, IntExpr* left, IntExpr* right) {
  if (!sat->IsExpressionBoolean(left) || !sat->IsExpressionBoolean(right)) {
    return false;
  }
  Sat::Literal left_literal = sat->Literal(left);
  Sat::Literal right_literal = sat->Literal(right);
  sat->AddClause(Negated(left_literal), right_literal);
  sat->AddClause(left_literal, Negated(right_literal));
  return true;
}

bool AddBoolLe(SatPropagator* sat, IntExpr* left, IntExpr* right) {
  if (!sat->IsExpressionBoolean(left) || !sat->IsExpressionBoolean(right)) {
    return false;
  }
  Sat::Literal left_literal = sat->Literal(left);
  Sat::Literal right_literal = sat->Literal(right);
  sat->AddClause(Negated(left_literal), right_literal);
  return true;
}

bool AddBoolNot(SatPropagator* sat, IntExpr* left, IntExpr* right) {
  if (!sat->IsExpressionBoolean(left) || !sat->IsExpressionBoolean(right)) {
    return false;
  }
  Sat::Literal left_literal = sat->Literal(left);
  Sat::Literal right_literal = sat->Literal(right);
  sat->AddClause(Negated(left_literal), Negated(right_literal));
  sat->AddClause(left_literal, right_literal);
  return true;
}

bool AddBoolOrArrayEqVar(SatPropagator* sat, const std::vector<IntVar*>& vars,
                         IntExpr* target) {
  if (!sat->AllVariablesBoolean(vars) || !sat->IsExpressionBoolean(target)) {
    return false;
  }
  Sat::Literal target_literal = sat->Literal(target);
  std::vector<Sat::Literal> lits(vars.size() + 1);
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->Literal(vars[i]);
  }
  lits[vars.size()] = Negated(target_literal);
  sat->AddClause(&lits);
  for (int i = 0; i < vars.size(); ++i) {
    sat->AddClause(target_literal, Negated(sat->Literal(vars[i])));
  }
  return true;
}

bool AddBoolAndArrayEqVar(SatPropagator* sat, const std::vector<IntVar*>& vars,
                          IntExpr* target) {
  if (!sat->AllVariablesBoolean(vars) || !sat->IsExpressionBoolean(target)) {
    return false;
  }
  Sat::Literal target_literal = sat->Literal(target);
  std::vector<Sat::Literal> lits(vars.size() + 1);
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = Negated(sat->Literal(vars[i]));
  }
  lits[vars.size()] = target_literal;
  sat->AddClause(&lits);
  for (int i = 0; i < vars.size(); ++i) {
    sat->AddClause(Negated(target_literal), sat->Literal(vars[i]));
  }
  return true;
}

bool AddSumBoolArrayGreaterEqVar(SatPropagator* sat,
                                 const std::vector<IntVar*>& vars,
                                 IntExpr* target) {
  if (!sat->AllVariablesBoolean(vars) || !sat->IsExpressionBoolean(target)) {
    return false;
  }
  Sat::Literal target_literal = sat->Literal(target);
  std::vector<Sat::Literal> lits(vars.size() + 1);
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->Literal(vars[i]);
  }
  lits[vars.size()] = Negated(target_literal);
  sat->AddClause(&lits);
  return true;
}

bool AddMaxBoolArrayLessEqVar(SatPropagator* sat,
                              const std::vector<IntVar*>& vars,
                              IntExpr* target) {
  if (!sat->AllVariablesBoolean(vars) || !sat->IsExpressionBoolean(target)) {
    return false;
  }
  const Sat::Literal target_literal = sat->Literal(target);
  for (int i = 0; i < vars.size(); ++i) {
    const Sat::Literal literal = Negated(sat->Literal(vars[i]));
    sat->AddClause(target_literal, literal);
  }
  return true;
}

bool AddSumBoolArrayLessEqKVar(SatPropagator* sat,
                               const std::vector<IntVar*>& vars,
                               IntExpr* target) {
  if (vars.size() == 1) {
    return AddBoolLe(sat, vars[0], target);
  }
  if (!sat->AllVariablesBoolean(vars) || !sat->IsExpressionBoolean(target)) {
    return false;
  }
  IntVar* const extra = target->solver()->MakeBoolVar();
  Sat::Literal target_literal = sat->Literal(target);
  Sat::Literal extra_literal = sat->Literal(extra);
  std::vector<Sat::Literal> lits(vars.size() + 1);
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->Literal(vars[i]);
  }
  lits[vars.size()] = Negated(extra_literal);
  sat->AddClause(&lits);
  for (int i = 0; i < vars.size(); ++i) {
    sat->AddClause(extra_literal, Negated(sat->Literal(vars[i])));
  }
  sat->AddClause(Negated(extra_literal), target_literal);
  return true;
}

bool AddBoolOrEqVar(SatPropagator* sat, IntExpr* left, IntExpr* right,
                    IntExpr* target) {
  if (!sat->IsExpressionBoolean(left) || !sat->IsExpressionBoolean(right) ||
      !sat->IsExpressionBoolean(target)) {
    return false;
  }
  Sat::Literal left_literal = sat->Literal(left);
  Sat::Literal right_literal = sat->Literal(right);
  Sat::Literal target_literal = sat->Literal(target);
  sat->AddClause(left_literal, right_literal, Negated(target_literal));
  sat->AddClause(Negated(left_literal), target_literal);
  sat->AddClause(Negated(right_literal), target_literal);
  return true;
}

bool AddBoolAndEqVar(SatPropagator* sat, IntExpr* left, IntExpr* right,
                     IntExpr* target) {
  if (!sat->IsExpressionBoolean(left) || !sat->IsExpressionBoolean(right) ||
      !sat->IsExpressionBoolean(target)) {
    return false;
  }
  Sat::Literal left_literal = sat->Literal(left);
  Sat::Literal right_literal = sat->Literal(right);
  Sat::Literal target_literal = sat->Literal(target);
  sat->AddClause(Negated(left_literal), Negated(right_literal), target_literal);
  sat->AddClause(left_literal, Negated(target_literal));
  sat->AddClause(right_literal, Negated(target_literal));
  return true;
}

bool AddBoolIsEqVar(SatPropagator* sat, IntExpr* left, IntExpr* right,
                    IntExpr* target) {
  if (!sat->IsExpressionBoolean(left) || !sat->IsExpressionBoolean(right) ||
      !sat->IsExpressionBoolean(target)) {
    return false;
  }
  Sat::Literal left_literal = sat->Literal(left);
  Sat::Literal right_literal = sat->Literal(right);
  Sat::Literal target_literal = sat->Literal(target);
  sat->AddClause(Negated(left_literal), right_literal, Negated(target_literal));
  sat->AddClause(left_literal, Negated(right_literal), Negated(target_literal));
  sat->AddClause(left_literal, right_literal, target_literal);
  sat->AddClause(Negated(left_literal), Negated(right_literal), target_literal);
  return true;
}

bool AddBoolIsNEqVar(SatPropagator* sat, IntExpr* left, IntExpr* right,
                     IntExpr* target) {
  if (!sat->IsExpressionBoolean(left) || !sat->IsExpressionBoolean(right) ||
      !sat->IsExpressionBoolean(target)) {
    return false;
  }
  Sat::Literal left_literal = sat->Literal(left);
  Sat::Literal right_literal = sat->Literal(right);
  Sat::Literal target_literal = sat->Literal(target);
  sat->AddClause(Negated(left_literal), right_literal, target_literal);
  sat->AddClause(left_literal, Negated(right_literal), target_literal);
  sat->AddClause(left_literal, right_literal, Negated(target_literal));
  sat->AddClause(Negated(left_literal), Negated(right_literal),
                 Negated(target_literal));
  return true;
}

bool AddBoolIsLeVar(SatPropagator* sat, IntExpr* left, IntExpr* right,
                    IntExpr* target) {
  if (!sat->IsExpressionBoolean(left) || !sat->IsExpressionBoolean(right) ||
      !sat->IsExpressionBoolean(target)) {
    return false;
  }
  Sat::Literal left_literal = sat->Literal(left);
  Sat::Literal right_literal = sat->Literal(right);
  Sat::Literal target_literal = sat->Literal(target);
  sat->AddClause(Negated(left_literal), right_literal, Negated(target_literal));
  sat->AddClause(left_literal, target_literal);
  sat->AddClause(Negated(right_literal), target_literal);
  return true;
}

bool AddBoolOrArrayEqualTrue(SatPropagator* sat,
                             const std::vector<IntVar*>& vars) {
  if (!sat->AllVariablesBoolean(vars)) {
    return false;
  }
  std::vector<Sat::Literal> lits(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = sat->Literal(vars[i]);
  }
  sat->AddClause(&lits);
  return true;
}

bool AddBoolAndArrayEqualFalse(SatPropagator* sat,
                               const std::vector<IntVar*>& vars) {
  if (!sat->AllVariablesBoolean(vars)) {
    return false;
  }
  std::vector<Sat::Literal> lits(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = Negated(sat->Literal(vars[i]));
  }
  sat->AddClause(&lits);
  return true;
}

bool AddAtMostOne(SatPropagator* sat, const std::vector<IntVar*>& vars) {
  if (!sat->AllVariablesBoolean(vars)) {
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

bool AddAtMostNMinusOne(SatPropagator* sat, const std::vector<IntVar*>& vars) {
  if (!sat->AllVariablesBoolean(vars)) {
    return false;
  }
  std::vector<Sat::Literal> lits(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    lits[i] = Negated(sat->Literal(vars[i]));
  }
  sat->AddClause(&lits);
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
  Sat::Literal left_literal = sat->Literal(left);
  Sat::Literal right_literal = sat->Literal(right);
  Sat::Literal target_literal = sat->Literal(target);
  sat->AddClause(left_literal, right_literal, target_literal);
  sat->AddClause(Negated(left_literal), Negated(right_literal), target_literal);
  sat->AddClause(Negated(left_literal), right_literal, Negated(target_literal));
  sat->AddClause(left_literal, Negated(right_literal), Negated(target_literal));
  return true;
}

bool AddIntNeReif(SatPropagator* sat, IntExpr* left, IntExpr* right,
                  IntExpr* target) {
  if (!sat->IsExpressionBoolean(left) || !sat->IsExpressionBoolean(right) ||
      !sat->IsExpressionBoolean(target)) {
    return false;
  }
  Sat::Literal left_literal = sat->Literal(left);
  Sat::Literal right_literal = Negated(sat->Literal(right));
  Sat::Literal target_literal = sat->Literal(target);
  sat->AddClause(left_literal, right_literal, target_literal);
  sat->AddClause(Negated(left_literal), Negated(right_literal), target_literal);
  sat->AddClause(Negated(left_literal), right_literal, Negated(target_literal));
  sat->AddClause(left_literal, Negated(right_literal), Negated(target_literal));
  return true;
}

SatPropagator* MakeSatPropagator(Solver* solver) {
  return solver->RevAlloc(new SatPropagator(solver));
}
}  // namespace operations_research

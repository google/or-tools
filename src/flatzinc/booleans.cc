// Copyright 2010-2013 Google
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/commandlineflags.h"
#include "base/hash.h"
#include "base/int-type.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/map-util.h"
#include "base/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"

namespace operations_research {
namespace Sat {
// Index of a variable.
DEFINE_INT_TYPE(Variable, int);

// A literal (pair variable, boolean).
DEFINE_INT_TYPE(Literal, int);

inline Literal MakeLiteral (Variable var, bool sign) {
  return Literal(2 * var.value() + (int)sign);
}

inline Literal Negated(Literal p) { return Literal(p.value() ^ 1); }
inline bool Sign(Literal p)   { return p.value() & 1; }
inline Variable Var(Literal p)   { return Variable(p.value() >> 1); }

static const Literal kUndefinedLiteral = Literal(-2);
static const Literal kErrorLiteral = Literal(-1);

DEFINE_INT_TYPE(Boolean, uint8);
static const Boolean kTrue = Boolean((uint8)0);
static const Boolean kFalse = Boolean((uint8)1);
static const Boolean kUndefined = Boolean((uint8)2);

inline Boolean MakeBoolean(bool x) { return Boolean(!x); }
inline Boolean Xor(Boolean a, bool b) {
  return Boolean((uint8)(a.value() ^ (uint8)b));
}

// Clause -- a simple class for representing a clause:
class Clause {
 public:
  Clause(std::vector<Literal>* const ps) {
    literals_.swap(*ps);
  }

  int size() const { return literals_.size(); }

  Literal& operator[] (int i)   { return literals_[i]; }
  Literal operator[] (int i) const  { return literals_[i]; }

 private:
  std::vector<Literal> literals_;
};

struct Watcher {
  Watcher(): blocker(kUndefinedLiteral) {}
  Watcher(Clause* cr, Literal p) : clause(cr), blocker(p) {}

  Clause* clause;
  Literal blocker;
};

// WatcherList -- a class for maintaining occurence lists with lazy deletion:
class WatcherList {
 public:
  void init(const Variable& v) {
    occs_.resize(2 * v.value() + 2);
    dirty_.resize(2 * v.value() + 2, 0);
  }

  std::vector<Watcher>& operator[](const Literal& idx) {
    return occs_[idx.value()];
  }

  void cleanAll() {
    for (int i = 0; i < dirties_.size(); i++) {
      // Dirties may contain duplicates so check here if a variable is
      // already cleaned:
      if (dirty_[dirties_[i].value()]) {
        clean(dirties_[i]);
      }
    }
    dirties_.clear();
  }

  void clean(const Literal& idx) {
    std::vector<Watcher>& vec = occs_[idx.value()];
    int i, j;
    for (i = j = 0; i < vec.size(); i++) {
      vec[j++] = vec[i];
    }
    vec.resize(j);
    dirty_[idx.value()] = 0;
  }

  void clear() {
    occs_.clear();
    dirty_.clear();
    dirties_.clear();
  }

 private:
  std::vector<std::vector<Watcher>> occs_;
  std::vector<char> dirty_;
  std::vector<Literal> dirties_;
};

// Solver
class Solver {
 public:
  Solver() : ok_(true), qhead_(0), num_vars_() {}
  virtual ~Solver() {}

  // Add a new variable.
  Variable NewVariable() {
    Variable v = IncrementVariableCounter();
    watches_.init(v);
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
  bool initPropagator() {
    touched_variables_.clear();
    return !ok_;
  }

  // Backtrack until a certain level.
  void cancelUntil(int level) {
    if (TrailMarker() > level) {
      for (int c = trail_.size() - 1; c >= trail_markers_[level]; c--) {
        Variable x = Var(trail_[c]);
        assignment_[x.value()] = kUndefined;
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
  Boolean Value(Variable x) const { return assignment_[x.value()]; }
  // The current value of a literal.
  Boolean Value(Literal p) const {
    const Boolean b = assignment_[Var(p).value()];
    return b == kUndefined ? kUndefined : Xor(b, Sign(p));
  }
  // The current number of original clauses.
  int nClauses() const { return clauses.size(); }
  // Propagates one literal, returns true if successful, false in case
  // of failure.
  bool PropagateOneLiteral(Literal lit);

  private:
  // Helper structures:
  //
  Variable IncrementVariableCounter() { return Variable(num_vars_++); };

  // Begins a new decision level.
  void PushTrailMarker() {
    trail_markers_.push_back(trail_.size());
  }
  // Enqueue a literal. Assumes value of literal is undefined.
  void UncheckedEnqueue(Literal p) {
    DCHECK_EQ(Value(p), kUndefined);
    if (assignment_[Var(p).value()] == kUndefined) {
      touched_variables_.push_back(p);
    }
    assignment_[Var(p).value()] = Sign(p) ? kFalse : kTrue;
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
  WatcherList watches_;
  // The current assignments.
  std::vector<Boolean> assignment_;
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
  Literal p;
  int i;
  int j;
  for (i = j = 0, p = kUndefinedLiteral; i < ps->size(); i++) {
    if (Value((*ps)[i]) == kTrue || (*ps)[i] == Negated(p)) {
      return true;
    } else if (Value((*ps)[i]) != kFalse && (*ps)[i] != p) {
      (*ps)[j++] = p = (*ps)[i];
    }
  }
  ps->resize(j);

  if (ps->size() == 0) {
    return ok_ = false;
  }
  else if (ps->size() == 1) {
    UncheckedEnqueue((*ps)[0]);
    return ok_ = Propagate();
  } else {
    Clause* const cr = new Clause(ps);
    clauses.push_back(cr);
    AttachClause(cr);
  }
  return true;
}

bool Solver::Propagate() {
  bool result = true;
  watches_.cleanAll();
  while (qhead_ < trail_.size()) {
    Literal p = trail_[qhead_++];
    // 'p' is enqueued fact to propagate.
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
      Clause* const cr  = ws[i].clause;
      Clause& c = *cr;
      Literal false_lit = Negated(p);
      if (c[0] == false_lit) {
        c[0] = c[1], c[1] = false_lit;
      }
      DCHECK_EQ(c[1], false_lit);
      i++;

      // If 0th watch is true, then clause is already satisfied.
      const Literal first = c[0];
      Watcher w = Watcher(cr, first);
      if (first != blocker && Value(first) == kTrue) {
        ws[j++] = w;
        continue;
      }

      // Look for new watch:
      bool cont = false;
      for (int k = 2; k < c.size(); k++) {
        if (Value(c[k]) != kFalse) {
          c[1] = c[k]; c[k] = false_lit;
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
  assignment_[Var(lit).value()] = MakeBoolean(!Sign(lit));
  trail_.push_back(lit);
  if (!Propagate()) {
    return false;
  }
  return true;
}
} // namespace Sat

class SatPropagator : public Constraint {
 public:
  SatPropagator(Solver* const solver) : Constraint(solver), minisat_trail_(0) {}

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

  Sat::Literal Literal(IntExpr* const expr) {
    IntVar* expr_var = NULL;
    bool expr_negated = false;
    if (!solver()->IsBooleanVar(expr, &expr_var, &expr_negated)) {
      return Sat::kErrorLiteral;
    }
    VLOG(2) << "SAT: Parse " << expr->DebugString() << " to "
            << expr_var->DebugString() << "/" << expr_negated;
    if (ContainsKey(indices_, expr_var)) {
      return Sat::MakeLiteral(indices_[expr_var], !expr_negated);
    } else {
      const Sat::Variable var = minisat_.NewVariable();
      vars_.push_back(expr_var);
      sat_variables_.push_back(var);
      indices_[expr_var] = var;
      Sat::Literal lit = Sat::MakeLiteral(var, !expr_negated);
      VLOG(2) << " - created var = " << var.value()
              << ", lit = " << lit.value();
      return lit;
    }
  }

  void VariableBound(int index) {
    if (minisat_trail_.Value() < minisat_.TrailMarker()) {
      VLOG(2) << "After failure, minisat_trail = " << minisat_trail_.Value()
              << ", minisat decision level = " << minisat_.TrailMarker();
      minisat_.cancelUntil(minisat_trail_.Value());
      CHECK_EQ(minisat_trail_.Value(), minisat_.TrailMarker());
    }
    const Sat::Variable var = sat_variables_[index];
    VLOG(2) << "VariableBound: " << vars_[index]->DebugString()
            << " with sat variable " << var;
    const Sat::Boolean internal_value = minisat_.Value(var);
    const bool new_value = vars_[index]->Value() != 0;
    // if (internal_value != Sat::kUndefined && new_value != internal_value) {
    //   VLOG(2) << " - internal value = " << internal_value << ", failing";
    //   solver()->Fail();
    // }
    Sat::Literal lit = Sat::MakeLiteral(var, new_value);
    VLOG(2) << " - enqueue lit = " << lit.value()
            << " at depth " << minisat_trail_.Value();
    if (!minisat_.PropagateOneLiteral(lit)) {
      VLOG(2) << " - failure detected, should backtrack";
      solver()->Fail();
    } else {
      minisat_trail_.SetValue(solver(), minisat_.TrailMarker());
      for (int i = 0; i < minisat_.touched_variables_.size(); ++i) {
        const Sat::Literal lit = minisat_.touched_variables_[i];
        const Sat::Variable var = Sat::Var(lit);
        const bool assigned_bool = Sat::Sign(lit);
        VLOG(2) << " - var " << var << " was assigned to " << assigned_bool
                << " from literal " << lit.value();
        demons_[var.value()]->inhibit(solver());
        vars_[var.value()]->SetValue(assigned_bool);
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
                                        i);
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
    VLOG(2) << " - done";
  }

  // Add a clause to the solver, clears the vector.
  bool AddClause (std::vector<Sat::Literal>* const lits) {
    return minisat_.AddClause(lits);
  }

  // Add the empty clause, making the solver contradictory.
  bool AddEmptyClause() {
    return minisat_.AddEmptyClause();
  }

  // Add a unit clause to the solver.
  bool AddClause (Sat::Literal p) {
    return minisat_.AddClause(p);
  }

  // Add a binary clause to the solver.
  bool AddClause (Sat::Literal p, Sat:: Literal q) {
    return minisat_.AddClause(p, q);
  }

  // Add a ternary clause to the solver.
  bool AddClause (Sat::Literal p, Sat::Literal q, Sat::Literal r) {
    return minisat_.AddClause(p, q, r);
  }

  virtual string DebugString() const {
    return "SatConstraint";
  }

  void Accept(ModelVisitor* const visitor) const {
    VLOG(1) << "Should Not Be Visited";
  }

 private:
  Sat::Solver minisat_;
  std::vector<IntVar*> vars_;
  hash_map<IntVar*, Sat::Variable> indices_;
  std::vector<Sat::Variable> sat_variables_;
  std::vector<Sat::Literal> bound_literals_;
  NumericalRev<int> minisat_trail_;
  std::vector<Demon*> demons_;
};

bool AddBoolEq(SatPropagator* const sat,
               IntExpr* const left,
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

bool AddBoolLe(SatPropagator* const sat,
               IntExpr* const left,
               IntExpr* const right) {
  if (!sat->Check(left) || !sat->Check(right)) {
    return false;
  }
  Sat::Literal left_lit = sat->Literal(left);
  Sat::Literal right_lit = sat->Literal(right);
  sat->AddClause(Negated(left_lit), right_lit);
  return true;
}

bool AddBoolNot(SatPropagator* const sat,
                IntExpr* const left,
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

bool AddBoolOrArrayEqVar(SatPropagator* const sat,
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
  for (int i = 0; i < vars.size(); ++i) {
    sat->AddClause(target_lit, Negated(sat->Literal(vars[i])));
  }
  return true;
}

bool AddBoolAndArrayEqVar(SatPropagator* const sat,
                          const std::vector<IntVar*>& vars,
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

bool AddBoolOrEqVar(SatPropagator* const sat,
                    IntExpr* const left,
                    IntExpr* const right,
                    IntExpr* const target) {
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

bool AddBoolAndEqVar(SatPropagator* const sat,
                     IntExpr* const left,
                     IntExpr* const right,
                     IntExpr* const target) {
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

bool AddBoolIsEqVar(SatPropagator* const sat,
                    IntExpr* const left,
                    IntExpr* const right,
                    IntExpr* const target) {
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

bool AddBoolIsNEqVar(SatPropagator* const sat,
                     IntExpr* const left,
                     IntExpr* const right,
                     IntExpr* const target) {
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

bool AddBoolIsLeVar(SatPropagator* const sat,
                    IntExpr* const left,
                    IntExpr* const right,
                    IntExpr* const target) {
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

SatPropagator* MakeSatPropagator(Solver* const solver) {
  return solver->RevAlloc(new SatPropagator(solver));
}
} // namespace operations_research

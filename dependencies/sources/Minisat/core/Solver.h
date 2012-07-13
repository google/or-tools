/********************************************************************[Solver.h]
Copyright(c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright(c) 2007-2010, Niklas Sorensson

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files(the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/

#ifndef Minisat_Solver_h
#define Minisat_Solver_h

#include "mtl/Vec.h"
#include "mtl/Heap.h"
#include "mtl/Alg.h"
#include "core/SolverTypes.h"

namespace Minisat {

//=============================================================================
// Solver -- the main class:

class Solver {
 public:

  // Constructor/Destructor:
  //
  Solver();
  virtual ~Solver();

  // Problem specification:
  //
  // Add a new variable with parameters specifying variable mode.
  Var newVar(bool polarity = true, bool dvar = true);

  // Add a clause to the solver.
  bool addClause(const std::vector<Lit>& ps);
  // Add the empty clause, making the solver contradictory.
  bool addEmptyClause();
  // Add a unit clause to the solver.
  bool addClause(Lit p);
  // Add a binary clause to the solver.
  bool addClause(Lit p, Lit q);
  // Add a ternary clause to the solver.
  bool addClause(Lit p, Lit q, Lit r);
  // Add a clause to the solver without making superflous internal copy. Will
  bool addClause_(vec<Lit>& ps);
  // change the passed vector 'ps'.

  // Solving:
  //
  // Removes already satisfied clauses.
  bool simplify();
  // Search for a model that respects a given set of assumptions.
  bool solve(const std::vector<Lit>& assumps);
  // Search for a model that respects a given set of assumptions(With
  // resource constraints).
  lbool solveLimited(const std::vector<Lit>& assumps);
  // Search without assumptions.
  bool solve();
  // Search for a model that respects a single assumption.
  bool solve(Lit p);
  // Search for a model that respects two assumptions.
  bool solve(Lit p, Lit q);
  // Search for a model that respects three assumptions.
  bool solve(Lit p, Lit q, Lit r);
  // FALSE means solver is in a conflicting state
  bool okay() const;

  // Incremental propagation.
  bool initPropagator();
  // propagates one literal, return value < 0 means success, >= 0 means
  // we should backtrack up to this level.
  int propagateOneLiteral(Lit lit);
  // Backtrack until a certain level.
  void cancelUntil(int level);
  // Gives the current decisionlevel.
  int decisionLevel() const;
  bool store_unit_propagation_;
  std::vector<Lit> touched_variables_;

  // Variable mode:
  //
  // Declare which polarity the decision heuristic should use for a
  // variable. Requires mode 'polarity_user'.
  void setPolarity(Var v, bool b);
  // Declare if a variable should be eligible for selection in the
  // decision heuristic.
  void setDecisionVar(Var v, bool b);

  // Read state:
  //
  // The current value of a variable.
  lbool value(Var x) const;
  // The current value of a literal.
  lbool value(Lit p) const;
  // The value of a variable in the last model. The last call to solve
  // must have been satisfiable.
  lbool modelValue(Var x) const;
  // The value of a literal in the last model. The last call to solve
  // must have been satisfiable.
  lbool modelValue(Lit p) const;
  // The current number of assigned literals.
  int nAssigns() const;
  // The current number of original clauses.
  int nClauses() const;
  // The current number of learnt clauses.
  int nLearnts() const;
  // The current number of variables.
  int nVars() const;
  int nFreeVars() const;

  // Resource contraints:
  //
  void setConfBudget(int64 x);
  void setPropBudget(int64 x);
  void budgetOff();
  // Trigger a(potentially asynchronous) interruption of the solver.
  void interrupt();
  // Clear interrupt indicator flag.
  void clearInterrupt();

  // Memory managment:
  //
  virtual void garbageCollect();
  void checkGarbage(double gf);
  void checkGarbage();

  // Extra results:(read-only member variable)
  //
  // If problem is satisfiable, this vector contains the model(if any).
  vec<lbool> model;
  // If problem is unsatisfiable(possibly under assumptions),
  vec<Lit> conflict;
  // this vector represent the final conflict clause expressed in the
  // assumptions.

  // Mode of operation:
  //
  int verbosity;
  double var_decay;
  double clause_decay;
  double random_var_freq;
  double random_seed;
  bool luby_restart;
  // Controls conflict clause minimization(0=none, 1=basic, 2=deep).
  int ccmin_mode;
  // Controls the level of phase saving(0=none, 1=limited, 2=full).
  int phase_saving;
  // Use random polarities for branching heuristics.
  bool rnd_pol;
  // Initialize variable activities with a small random value.
  bool rnd_init_act;
  // The fraction of wasted memory allowed before a garbage collection
  // is triggered.
  double garbage_frac;

  // The initial restart limit.(default 100)
  int restart_first;
  // The factor with which the restart limit is multiplied in each
  // restart.(default 1.5)
  double restart_inc;
  // The intitial limit for learnt clauses is a factor of the original
  // clauses.(default 1 / 3)
  double learntsize_factor;
  // The limit for learnt clauses is multiplied with this factor each
  // restart.(default 1.1)
  double learntsize_inc;

  int learntsize_adjust_start_confl;
  double learntsize_adjust_inc;

  // Statistics:(read-only member variable)
  //
  uint64 solves, starts, decisions, rnd_decisions, propagations, conflicts;
  uint64 dec_vars, clauses_literals, learnts_literals, max_literals, tot_literals;

 protected:
  // Helper structures:
  //
  struct VarData {
    CRef reason;
    int level;
  };

  static inline VarData mkVarData(CRef cr, int l) {
    VarData d = {cr, l};
    return d;
  }

  struct Watcher {
    CRef cref;
    Lit blocker;
    Watcher(CRef cr, Lit p) : cref(cr), blocker(p) {}
    bool operator==(const Watcher& w) const {
      return cref == w.cref;
    }
    bool operator!=(const Watcher& w) const {
      return cref != w.cref;
    }
  };

  struct WatcherDeleted {
    const ClauseAllocator& ca;
    WatcherDeleted(const ClauseAllocator& _ca) : ca(_ca) {}
    bool operator()(const Watcher& w) const {
      return ca[w.cref].mark() == 1;
    }
  };

  struct VarOrderLt {
    const vec<double>& activity;
    bool operator()(Var x, Var y) const {
      return activity[x] > activity[y];
    }
    VarOrderLt(const vec<double>& act) : activity(act) { }
  };

  // Solver state:
  //
  // If FALSE, the constraints are already unsatisfiable. No part of
  // the solver state may be used!
  bool ok;
  // List of problem clauses.
  vec<CRef> clauses;
  // List of learnt clauses.
  vec<CRef> learnts;
  // Amount to bump next clause with.
  double cla_inc;
  // A heuristic measurement of the activity of a variable.
  vec<double> activity;
  // Amount to bump next variable with.
  double var_inc;
  OccLists<Lit, vec<Watcher>, WatcherDeleted>
  // 'watches[lit]' is a list of constraints watching 'lit'(will go
  // there if literal becomes true).
  watches;
  // The current assignments.
  vec<lbool> assigns;
  // The preferred polarity of each variable.
  vec<char> polarity;
  // Declares if a variable is eligible for selection in the decision
  // heuristic.
  vec<char> decision;
  // Assignment stack; stores all assigments made in the order they
  // were made.
  vec<Lit> trail;
  // Separator indices for different decision levels in 'trail'.
  vec<int> trail_lim;
  // Stores reason and level for each variable.
  vec<VarData> vardata;
  // Head of queue(as index into the trail -- no more explicit
  // propagation queue in MiniSat).
  int qhead;
  // Number of top-level assignments since last execution of
  // 'simplify()'.
  int simpDB_assigns;
  // Remaining number of propagations that must be made before next
  // execution of 'simplify()'.
  int64 simpDB_props;
  // Current set of assumptions provided to solve by the user.
  std::vector<Lit> assumptions;
  // A priority queue of variables ordered with respect to the
  // variable activity.
  Heap<VarOrderLt> order_heap;
  // Set by 'search()'.
  double progress_estimate;
  // Indicates whether possibly inefficient linear scan for satisfied
  // clauses should be performed in 'simplify'.
  bool remove_satisfied;

  ClauseAllocator ca;

  // Temporaries(to reduce allocation overhead). Each variable is
  // prefixed by the method in which it is used, exept 'seen' wich is
  // used in several places.
  //
  vec<char> seen;
  vec<Lit> analyze_stack;
  vec<Lit> analyze_toclear;
  vec<Lit> add_tmp;

  double max_learnts;
  double learntsize_adjust_confl;
  int learntsize_adjust_cnt;

  // Resource contraints:
  //
  int64 conflict_budget; // -1 means no budget.
  int64 propagation_budget; // -1 means no budget.
  bool asynch_interrupt;

  // Main internal methods:
  //
  // Insert a variable in the decision order priority queue.
  void insertVarOrder(Var x);
  // Return the next decision variable.
  Lit pickBranchLit();
  // Begins a new decision level.
  void newDecisionLevel();
  // Enqueue a literal. Assumes value of literal is undefined.
  void uncheckedEnqueue(Lit p, CRef from = CRef_Undef);
  // Test if fact 'p' contradicts current state, enqueue otherwise.
  bool enqueue(Lit p, CRef from = CRef_Undef);
  // Perform unit propagation. Returns possibly conflicting clause.
  CRef propagate();
  //(bt = backtrack)
  void analyze(CRef confl, vec<Lit>& out_learnt, int& out_btlevel);
  // COULD THIS BE IMPLEMENTED BY THE ORDINARIY "analyze" BY SOME REASONABLE GENERALIZATION?
  void analyzeFinal(Lit p, vec<Lit>& out_conflict);
  //(helper method for 'analyze()')
  bool litRedundant(Lit p, uint32 abstract_levels);
  // Search for a given number of conflicts.
  lbool search(int nof_conflicts);
  // Main solve method(assumptions given in 'assumptions').
  lbool solve_();
  // Reduce the set of learnt clauses.
  void reduceDB();
  // Shrink 'cs' to contain only non-satisfied clauses.
  void removeSatisfied(vec<CRef>& cs);
  void rebuildOrderHeap();

  // Maintaining Variable/Clause activity:

  //

  // Decay all variables with the specified factor. Implemented by
  // increasing the 'bump' value instead.
  void varDecayActivity();
  // Increase a variable with the current 'bump' value.
  void varBumpActivity(Var v, double inc);
  // Increase a variable with the current 'bump' value.
  void varBumpActivity(Var v);
  // Decay all clauses with the specified factor. Implemented by
  // increasing the 'bump' value instead.
  void claDecayActivity();
  // Increase a clause with the current 'bump' value.
  void claBumpActivity(Clause& c);

  // Operations on clauses:
  //
  // Attach a clause to watcher lists.
  void attachClause(CRef cr);
  // Detach a clause to watcher lists.
  void detachClause(CRef cr, bool strict = false);
  // Detach and free a clause.
  void removeClause(CRef cr);
  // Returns TRUE if a clause is a reason for some implication in the
  // current state.
  bool locked(const Clause& c) const;
  // Returns TRUE if a clause is satisfied in the current state.
  bool satisfied(const Clause& c) const;

  void relocAll(ClauseAllocator& to);

  // Misc:
  //
  // Used to represent an abstraction of sets of decision levels.
  uint32 abstractLevel(Var x) const;
  CRef reason(Var x) const;
  int level(Var x) const;
  // DELETE THIS ?? IT'S NOT VERY USEFUL ...
  double progressEstimate() const;
  bool withinBudget() const;

  // Static helpers:
  //

  // Returns a random float 0 <= x < 1. Seed must never be 0.
  static inline double drand(double& seed) {
    seed *= 1389796;
    int q =(int)(seed / 2147483647);
    seed -=(double)q * 2147483647;
    return seed / 2147483647; }

  // Returns a random integer 0 <= x < size. Seed must never be 0.
  static inline int irand(double& seed, int size) {
    return(int)(drand(seed) * size); }
};


//=============================================================================
// Implementation of inline methods:

inline CRef Solver::reason(Var x) const { return vardata[x].reason; }
inline int Solver::level(Var x) const { return vardata[x].level; }

inline void Solver::insertVarOrder(Var x) {
  if(!order_heap.inHeap(x) && decision[x]) order_heap.insert(x); }

inline void Solver::varDecayActivity() { var_inc *=(1 / var_decay); }
inline void Solver::varBumpActivity(Var v) { varBumpActivity(v, var_inc); }
inline void Solver::varBumpActivity(Var v, double inc) {
  if((activity[v] += inc) > 1e100 ) {
    // Rescale:
    for(int i = 0; i < nVars(); i++)
      activity[i] *= 1e-100;
    var_inc *= 1e-100; }

  // Update order_heap with respect to new activity:
  if(order_heap.inHeap(v))
    order_heap.decrease(v); }

inline void Solver::claDecayActivity() { cla_inc *=(1 / clause_decay); }
inline void Solver::claBumpActivity(Clause& c) {
  if((c.activity() += cla_inc) > 1e20 ) {
    // Rescale:
    for(int i = 0; i < learnts.size(); i++)
      ca[learnts[i]].activity() *= 1e-20;
    cla_inc *= 1e-20; } }

inline void Solver::checkGarbage(void){ return checkGarbage(garbage_frac); }
inline void Solver::checkGarbage(double gf){
  if(ca.wasted() > ca.size() * gf)
    garbageCollect(); }

// NOTE: enqueue does not set the ok flag!(only public methods do)
inline bool Solver::enqueue(Lit p, CRef from) {
  return value(p) != l_Undef ?
      value(p) != l_False :
      (uncheckedEnqueue(p, from), true);
}

inline bool Solver::addClause(const std::vector<Lit>& ps) {
  add_tmp.clear();
  for(int i = 0; i < ps.size(); ++i) {
    add_tmp.push(ps[i]);
  }
  return addClause_(add_tmp);
}

inline bool Solver::addEmptyClause() {
  add_tmp.clear();
  return addClause_(add_tmp);
}

inline bool Solver::addClause(Lit p) {
  add_tmp.clear();
  add_tmp.push(p);
  return addClause_(add_tmp);
}

inline bool Solver::addClause(Lit p, Lit q) {
  add_tmp.clear();
  add_tmp.push(p);
  add_tmp.push(q);
  return addClause_(add_tmp);
}

inline bool Solver::addClause(Lit p, Lit q, Lit r) {
  add_tmp.clear();
  add_tmp.push(p);
  add_tmp.push(q);
  add_tmp.push(r);
  return addClause_(add_tmp);
}

inline bool Solver::locked(const Clause& c) const {
  return value(c[0]) == l_True &&
      reason(var(c[0])) != CRef_Undef &&
      ca.lea(reason(var(c[0]))) == &c;
}

inline void Solver::newDecisionLevel() {
  trail_lim.push(trail.size());
}

inline int Solver::decisionLevel() const {
  return trail_lim.size();
}
inline uint32 Solver::abstractLevel(Var x) const {
  return 1 <<(level(x) & 31);
}
inline lbool Solver::value(Var x) const {
  return assigns[x];
}
inline lbool Solver::value(Lit p) const {
  return assigns[var(p)] ^ sign(p);
}
inline lbool Solver::modelValue(Var x) const {
  return model[x];
}
inline lbool Solver::modelValue(Lit p) const {
  return model[var(p)] ^ sign(p);
}
inline int Solver::nAssigns() const {
  return trail.size();
}
inline int Solver::nClauses() const {
  return clauses.size();
}
inline int Solver::nLearnts() const {
  return learnts.size();
}
inline int Solver::nVars() const {
  return vardata.size();
}
inline int Solver::nFreeVars() const {
  return(int)dec_vars -(trail_lim.size() == 0 ? trail.size() : trail_lim[0]);
}
inline void Solver::setPolarity(Var v, bool b) {
  polarity[v] = b;
}
inline void Solver::setDecisionVar(Var v, bool b) {
  if( b && !decision[v]) dec_vars++;
  else if(!b && decision[v]) dec_vars--;

  decision[v] = b;
  insertVarOrder(v);
}
inline void Solver::setConfBudget(int64 x){
  conflict_budget = conflicts + x;
}
inline void Solver::setPropBudget(int64 x){
  propagation_budget = propagations + x;
}
inline void Solver::interrupt(){
  asynch_interrupt = true;
}
inline void Solver::clearInterrupt(){
  asynch_interrupt = false;
}
inline void Solver::budgetOff(){
 conflict_budget = propagation_budget = -1;
}
inline bool Solver::withinBudget() const {
  return !asynch_interrupt &&
      (conflict_budget < 0 || conflicts <(uint64)conflict_budget) &&
      (propagation_budget < 0 || propagations <(uint64)propagation_budget);
}

// FIXME: after the introduction of asynchronous interrruptions the
// solve-versions that return a pure bool do not give a safe
// interface. Either interrupts must be possible to turn off here, or
// all calls to solve must return an 'lbool'. I'm not yet sure which I
// prefer.
inline bool Solver::solve() {
  budgetOff();
  assumptions.clear();
  return solve_() == l_True;
}
inline bool Solver::solve(Lit p) {
  budgetOff();
  assumptions.clear();
  assumptions.push_back(p);
  return solve_() == l_True;
}
inline bool Solver::solve(Lit p, Lit q) {
  budgetOff();
  assumptions.clear();
  assumptions.push_back(p);
  assumptions.push_back(q);
  return solve_() == l_True;
}
inline bool Solver::solve(Lit p, Lit q, Lit r) {
  budgetOff();
  assumptions.clear();
  assumptions.push_back(p);
  assumptions.push_back(q);
  assumptions.push_back(r);
  return solve_() == l_True;
}
inline bool Solver::solve(const std::vector<Lit>& assumps){
  budgetOff();
  assumptions = assumps;
  return solve_() == l_True;
}
inline lbool Solver::solveLimited(const std::vector<Lit>& assumps){
  assumptions = assumps; return solve_();
}
inline bool Solver::okay() const {
  return ok;
}
}  // namespace Minisat

#endif

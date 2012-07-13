/********************************************************************[Solver.cc]
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007-2010, Niklas Sorensson

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
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
*******************************************************************************/

#include <math.h>
#include <stdio.h>

#include "base/commandlineflags.h"
#include "mtl/Sort.h"
#include "core/Solver.h"

DEFINE_double(var_decay, 0.95, "The variable activity decay factor");
DEFINE_double(clause_decay,0.999, "The clause activity decay factor");
DEFINE_double(random_var_freq, 0,
              "The frequency with which the decision heuristic tries "
              "to choose a random variable");
DEFINE_int32(random_seed, 91638253, "Used by the random variable selection");
DEFINE_int32(ccmin_mode, 2,
             "Controls conflict clause minimization (0=none, 1=basic, 2=deep)");
DEFINE_int32(phase_saving, 2,
             "Controls the level of phase saving (0=none, 1=limited, 2=full)");
DEFINE_bool(rnd_init_act, false, "Randomize the initial activity");
DEFINE_bool(luby_restart, true, "Use the Luby restart sequence");
DEFINE_int32(restart_first, 100, "The base restart interval");
DEFINE_double(restart_inc, 2, "Restart interval increase factor");
DEFINE_double(garbage_frac, 0.20, "The fraction of wasted memory allowed "
              "before a garbage collection is triggered");

namespace Minisat {
//=============================================================================
// Constructor/Destructor:

Solver::Solver() :

    // Parameters (user settable):
    //
    store_unit_propagation_(false)
    , verbosity        (0)
    , var_decay        (FLAGS_var_decay)
    , clause_decay     (FLAGS_clause_decay)
    , random_var_freq  (FLAGS_random_var_freq)
    , random_seed      (FLAGS_random_seed)
    , luby_restart     (FLAGS_luby_restart)
    , ccmin_mode       (FLAGS_ccmin_mode)
    , phase_saving     (FLAGS_phase_saving)
    , rnd_pol          (false)
    , rnd_init_act     (FLAGS_rnd_init_act)
    , garbage_frac     (FLAGS_garbage_frac)
    , restart_first    (FLAGS_restart_first)
    , restart_inc      (FLAGS_restart_inc)

    // Parameters (the rest):
    //
    , learntsize_factor((double)1/(double)3), learntsize_inc(1.1)

    // Parameters (experimental):
    //
    , learntsize_adjust_start_confl (100)
    , learntsize_adjust_inc         (1.5)

    // Statistics: (formerly in 'SolverStats')
    //
    , solves(0)
    , starts(0)
    , decisions(0)
    , rnd_decisions(0)
    , propagations(0)
    , conflicts(0)
    , dec_vars(0)
    , clauses_literals(0)
    , learnts_literals(0)
    , max_literals(0)
    , tot_literals(0)

    , ok                 (true)
    , cla_inc            (1)
    , var_inc            (1)
    , watches            (WatcherDeleted(ca))
    , qhead              (0)
    , simpDB_assigns     (-1)
    , simpDB_props       (0)
    , order_heap         (VarOrderLt(activity))
    , progress_estimate  (0)
    , remove_satisfied   (true)

    // Resource constraints:
    //
    , conflict_budget    (-1)
    , propagation_budget (-1)
    , asynch_interrupt   (false)
{}


Solver::~Solver() {}

//=============================================================================
// Minor methods:


// Creates a new SAT variable in the solver. If 'decision' is cleared,
// variable will not be used as a decision variable (NOTE! This has
// effects on the meaning of a SATISFIABLE result).
//
Var Solver::newVar(bool sign, bool dvar) {
  int v = nVars();
  watches  .init(mkLit(v, false));
  watches  .init(mkLit(v, true ));
  assigns  .push(l_Undef);
  vardata  .push(mkVarData(CRef_Undef, 0));
  //activity .push(0);
  activity .push(rnd_init_act ? drand(random_seed) * 0.00001 : 0);
  seen     .push(0);
  polarity .push(sign);
  decision .push();
  trail    .capacity(v+1);
  setDecisionVar(v, dvar);
  return v;
}

bool Solver::addClause_(vec<Lit>& ps) {
  assert(decisionLevel() == 0);
  if (!ok) return false;

  // Check if clause is satisfied and remove false/duplicate literals:
  sort(ps);
  Lit p; int i, j;
  for (i = j = 0, p = lit_Undef; i < ps.size(); i++)
    if (value(ps[i]) == l_True || ps[i] == ~p)
      return true;
    else if (value(ps[i]) != l_False && ps[i] != p)
      ps[j++] = p = ps[i];
  ps.shrink(i - j);

  if (ps.size() == 0)
    return ok = false;
  else if (ps.size() == 1) {
    uncheckedEnqueue(ps[0]);
    return ok = (propagate() == CRef_Undef);
  } else {
    CRef cr = ca.alloc(ps, false);
    clauses.push(cr);
    attachClause(cr);
  }

  return true;
}

void Solver::attachClause(CRef cr) {
  const Clause& c = ca[cr];
  assert(c.size() > 1);
  watches[~c[0]].push(Watcher(cr, c[1]));
  watches[~c[1]].push(Watcher(cr, c[0]));
  if (c.learnt()) learnts_literals += c.size();
  else            clauses_literals += c.size();
 }


void Solver::detachClause(CRef cr, bool strict) {
  const Clause& c = ca[cr];
  assert(c.size() > 1);

  if (strict) {
    remove(watches[~c[0]], Watcher(cr, c[1]));
    remove(watches[~c[1]], Watcher(cr, c[0]));
  } else {
    // Lazy detaching: (NOTE! Must clean all watcher lists before
    // garbage collecting this clause)
    watches.smudge(~c[0]);
    watches.smudge(~c[1]);
  }

  if (c.learnt()) learnts_literals -= c.size();
  else            clauses_literals -= c.size();
}

void Solver::removeClause(CRef cr) {
  Clause& c = ca[cr];
  detachClause(cr);
  // Don't leave pointers to free'd memory!
  if (locked(c)) vardata[var(c[0])].reason = CRef_Undef;
  c.mark(1);
  ca.free(cr);
}

bool Solver::satisfied(const Clause& c) const {
  for (int i = 0; i < c.size(); i++)
    if (value(c[i]) == l_True)
      return true;
  return false;
}

// Revert to the state at given level (keeping all assignment at
// 'level' but not beyond).
//
void Solver::cancelUntil(int level) {
  if (decisionLevel() > level) {
    for (int c = trail.size()-1; c >= trail_lim[level]; c--) {
      Var      x  = var(trail[c]);
      assigns [x] = l_Undef;
      if (phase_saving > 1 || (phase_saving == 1) && c > trail_lim.last())
        polarity[x] = sign(trail[c]);
      insertVarOrder(x); }
    qhead = trail_lim[level];
    trail.shrink(trail.size() - trail_lim[level]);
    trail_lim.shrink(trail_lim.size() - level);
  }
}

//==============================================================================
// Major methods:

Lit Solver::pickBranchLit() {
  Var next = var_Undef;

  // Random decision:
  if (drand(random_seed) < random_var_freq && !order_heap.empty()) {
    next = order_heap[irand(random_seed,order_heap.size())];
    if (value(next) == l_Undef && decision[next])
      rnd_decisions++; }

  // Activity based decision:
  while (next == var_Undef || value(next) != l_Undef || !decision[next])
    if (order_heap.empty()) {
      next = var_Undef;
      break;
    } else
      next = order_heap.removeMin();

  return next == var_Undef ?
      lit_Undef :
      mkLit(next, rnd_pol ? drand(random_seed) < 0.5 : polarity[next]);
}

/*______________________________________________________________________________
  |
  |  analyze : (confl : Clause*) (out_learnt : vec<Lit>&) (out_btlevel : int&)
  |      ->  [void]
  |
  |  Description:
  |    Analyze conflict and produce a reason clause.
  |
  |    Pre-conditions:
  |      * 'out_learnt' is assumed to be cleared.
  |      * Current decision level must be greater than root level.
  |
  |    Post-conditions:
  |      * 'out_learnt[0]' is the asserting literal at level 'out_btlevel'.
  |      * If out_learnt.size() > 1 then 'out_learnt[1]' has
  |        the greatest decision level of the  rest of literals. There
  |        may be others from the same level though.
  |
  |__________________________________________________________________________@*/
void Solver::analyze(CRef confl, vec<Lit>& out_learnt, int& out_btlevel) {
  int pathC = 0;
  Lit p     = lit_Undef;

  // Generate conflict clause:
  //
  out_learnt.push();      // (leave room for the asserting literal)
  int index   = trail.size() - 1;

  do{
    assert(confl != CRef_Undef); // (otherwise should be UIP)
    Clause& c = ca[confl];

    if (c.learnt())
      claBumpActivity(c);

    for (int j = (p == lit_Undef) ? 0 : 1; j < c.size(); j++) {
      Lit q = c[j];

      if (!seen[var(q)] && level(var(q)) > 0) {
        varBumpActivity(var(q));
        seen[var(q)] = 1;
        if (level(var(q)) >= decisionLevel())
          pathC++;
        else
          out_learnt.push(q);
      }
    }

    // Select next clause to look at:
    while (!seen[var(trail[index--])]);
    p     = trail[index+1];
    confl = reason(var(p));
    seen[var(p)] = 0;
    pathC--;

  } while (pathC > 0);
  out_learnt[0] = ~p;

  // Simplify conflict clause:
  //
  int i, j;
  out_learnt.copyTo(analyze_toclear);
  if (ccmin_mode == 2) {
    uint32 abstract_level = 0;
    for (i = 1; i < out_learnt.size(); i++)
      abstract_level |= abstractLevel(var(out_learnt[i]));
    // (maintain an abstraction of levels involved in conflict)

    for (i = j = 1; i < out_learnt.size(); i++)
      if (reason(var(out_learnt[i])) == CRef_Undef ||
          !litRedundant(out_learnt[i], abstract_level))
        out_learnt[j++] = out_learnt[i];

  } else if (ccmin_mode == 1) {
    for (i = j = 1; i < out_learnt.size(); i++) {
      Var x = var(out_learnt[i]);

      if (reason(x) == CRef_Undef)
        out_learnt[j++] = out_learnt[i];
      else {
        Clause& c = ca[reason(var(out_learnt[i]))];
        for (int k = 1; k < c.size(); k++)
          if (!seen[var(c[k])] && level(var(c[k])) > 0) {
            out_learnt[j++] = out_learnt[i];
            break; }
      }
    }
  } else
    i = j = out_learnt.size();

  max_literals += out_learnt.size();
  out_learnt.shrink(i - j);
  tot_literals += out_learnt.size();

  // Find correct backtrack level:
  //
  if (out_learnt.size() == 1)
    out_btlevel = 0;
  else {
    int max_i = 1;
    // Find the first literal assigned at the next-highest level:
    for (int i = 2; i < out_learnt.size(); i++)
      if (level(var(out_learnt[i])) > level(var(out_learnt[max_i])))
        max_i = i;
    // Swap-in this literal at index 1:
    Lit p             = out_learnt[max_i];
    out_learnt[max_i] = out_learnt[1];
    out_learnt[1]     = p;
    out_btlevel       = level(var(p));
  }

  for (int j = 0; j < analyze_toclear.size(); j++)
    seen[var(analyze_toclear[j])] = 0;    // ('seen[]' is now cleared)
}


// Check if 'p' can be removed. 'abstract_levels' is used to abort
// early if the algorithm is visiting literals at levels that cannot
// be removed later.
bool Solver::litRedundant(Lit p, uint32_t abstract_levels) {
  analyze_stack.clear(); analyze_stack.push(p);
  int top = analyze_toclear.size();
  while (analyze_stack.size() > 0) {
    assert(reason(var(analyze_stack.last())) != CRef_Undef);
    Clause& c = ca[reason(var(analyze_stack.last()))]; analyze_stack.pop();

    for (int i = 1; i < c.size(); i++) {
      Lit p  = c[i];
      if (!seen[var(p)] && level(var(p)) > 0) {
        if (reason(var(p)) != CRef_Undef &&
            (abstractLevel(var(p)) & abstract_levels) != 0) {
          seen[var(p)] = 1;
          analyze_stack.push(p);
          analyze_toclear.push(p);
        } else {
          for (int j = top; j < analyze_toclear.size(); j++)
            seen[var(analyze_toclear[j])] = 0;
          analyze_toclear.shrink(analyze_toclear.size() - top);
          return false;
        }
      }
    }
  }

  return true;
}


/*_____________________________________________________________________________
  |
  |  analyzeFinal : (p : Lit)  ->  [void]
  |
  |  Description:
  |    Specialized analysis procedure to express the final conflict in terms
  |       of assumptions.
  |    Calculates the (possibly empty) set of assumptions that led to the
  |       assignment of 'p', and stores the result in 'out_conflict'.
  |__________________________________________________________________________@*/
void Solver::analyzeFinal(Lit p, vec<Lit>& out_conflict) {
  out_conflict.clear();
  out_conflict.push(p);

  if (decisionLevel() == 0)
    return;

  seen[var(p)] = 1;

  for (int i = trail.size()-1; i >= trail_lim[0]; i--) {
    Var x = var(trail[i]);
    if (seen[x]) {
      if (reason(x) == CRef_Undef) {
        assert(level(x) > 0);
        out_conflict.push(~trail[i]);
      } else {
        Clause& c = ca[reason(x)];
        for (int j = 1; j < c.size(); j++)
          if (level(var(c[j])) > 0)
            seen[var(c[j])] = 1;
      }
      seen[x] = 0;
    }
  }

  seen[var(p)] = 0;
}

void Solver::uncheckedEnqueue(Lit p, CRef from) {
  assert(value(p) == l_Undef);
  assigns[var(p)] = lbool(!sign(p));
  vardata[var(p)] = mkVarData(from, decisionLevel());
  trail.push_(p);
  if (store_unit_propagation_) {
    touched_variables_.push_back(var(p));
  }
}

/* _____________________________________________________________________________
  |
  |  propagate : [void]  ->  [Clause*]
  |
  |  Description:
  |    Propagates all enqueued facts. If a conflict arises, the conflicting
  |       clause is returned,
  |    otherwise CRef_Undef.
  |
  |    Post-conditions:
  |      * the propagation queue is empty, even if there was a conflict.
  |__________________________________________________________________________@*/
CRef Solver::propagate() {
  CRef    confl     = CRef_Undef;
  int     num_props = 0;
  watches.cleanAll();

  while (qhead < trail.size()) {
    Lit            p   = trail[qhead++];
    // 'p' is enqueued fact to propagate.
    vec<Watcher>&  ws  = watches[p];
    Watcher        *i, *j, *end;
    num_props++;

    for (i = j = (Watcher*)ws, end = i + ws.size();  i != end;) {
      // Try to avoid inspecting the clause:
      Lit blocker = i->blocker;
      if (value(blocker) == l_True) {
        *j++ = *i++; continue; }

      // Make sure the false literal is data[1]:
      CRef     cr        = i->cref;
      Clause&  c         = ca[cr];
      Lit      false_lit = ~p;
      if (c[0] == false_lit)
        c[0] = c[1], c[1] = false_lit;
      assert(c[1] == false_lit);
      i++;

      // If 0th watch is true, then clause is already satisfied.
      Lit     first = c[0];
      Watcher w     = Watcher(cr, first);
      if (first != blocker && value(first) == l_True) {
        *j++ = w; continue; }

      // Look for new watch:
      for (int k = 2; k < c.size(); k++)
        if (value(c[k]) != l_False) {
          c[1] = c[k]; c[k] = false_lit;
          watches[~c[1]].push(w);
          goto NextClause; }

      // Did not find watch -- clause is unit under assignment:
      *j++ = w;
      if (value(first) == l_False) {
        confl = cr;
        qhead = trail.size();
        // Copy the remaining watches:
        while (i < end)
          *j++ = *i++;
      } else
        uncheckedEnqueue(first, cr);

   NextClause:;
    }
    ws.shrink(i - j);
  }
  propagations += num_props;
  simpDB_props -= num_props;

  return confl;
}

/* ___________________________________________________________________________
  |
  |  reduceDB : ()  ->  [void]
  |
  |  Description:
  |    Remove half of the learnt clauses, minus the clauses locked by
  |        the current assignment. Locked clauses are clauses that are
  |        reason to some assignment. Binary clauses are never removed.
  |_________________________________________________________________________@*/
struct reduceDB_lt {
  ClauseAllocator& ca;
  reduceDB_lt(ClauseAllocator& ca_) : ca(ca_) {}
  bool operator () (CRef x, CRef y) {
    return ca[x].size() > 2 && (ca[y].size() == 2 ||
                                ca[x].activity() < ca[y].activity());
  }
};
void Solver::reduceDB() {
  int     i, j;
  double  extra_lim = cla_inc / learnts.size();    // Remove any clause below this activity

  sort(learnts, reduceDB_lt(ca));
  // Don't delete binary or locked clauses. From the rest, delete
  // clauses from the first half and clauses with activity smaller
  // than 'extra_lim':
  for (i = j = 0; i < learnts.size(); i++) {
    Clause& c = ca[learnts[i]];
    if (c.size() > 2 && !locked(c) &&
        (i < learnts.size() / 2 || c.activity() < extra_lim))
      removeClause(learnts[i]);
    else
      learnts[j++] = learnts[i];
  }
  learnts.shrink(i - j);
  checkGarbage();
}

void Solver::removeSatisfied(vec<CRef>& cs) {
  int i, j;
  for (i = j = 0; i < cs.size(); i++) {
    Clause& c = ca[cs[i]];
    if (satisfied(c))
      removeClause(cs[i]);
    else
      cs[j++] = cs[i];
  }
  cs.shrink(i - j);
}

void Solver::rebuildOrderHeap() {
  vec<Var> vs;
  for (Var v = 0; v < nVars(); v++)
    if (decision[v] && value(v) == l_Undef)
      vs.push(v);
  order_heap.build(vs);
}

/*_____________________________________________________________________________
  |
  |  simplify : [void]  ->  [bool]
  |
  |  Description:
  |    Simplify the clause database according to the current top-level
  |     assigment. Currently, the only thing done here is the removal
  |     of satisfied clauses, but more things can be put here.
  |__________________________________________________________________________@*/
bool Solver::simplify() {
  assert(decisionLevel() == 0);

  if (!ok || propagate() != CRef_Undef)
    return ok = false;

  if (nAssigns() == simpDB_assigns || (simpDB_props > 0))
    return true;

  // Remove satisfied clauses:
  removeSatisfied(learnts);
  if (remove_satisfied)        // Can be turned off.
    removeSatisfied(clauses);
  checkGarbage();
  rebuildOrderHeap();

  simpDB_assigns = nAssigns();
  // (shouldn't depend on stats really, but it will do for now)
  simpDB_props   = clauses_literals + learnts_literals;

  return true;
}


/*_____________________________________________________________________________
  |
  |  search : (nof_conflicts : int) (params : const SearchParams&)  ->  [lbool]
  |
  |  Description:
  |    Search for a model the specified number of conflicts.
  |    NOTE! Use negative value for 'nof_conflicts' indicate infinity.
  |
  |  Output:
  |    'l_True' if a partial assigment that is consistent with respect to
  |        the clauseset is found. If all variables are decision variables,
  |        this means that the clause set is satisfiable. 'l_False' if the
  |        clause set is unsatisfiable. 'l_Undef' if the bound on number
  |        of conflicts is reached.
  |_________________________________________________________________________@*/
lbool Solver::search(int nof_conflicts) {
  assert(ok);
  int         backtrack_level;
  int         conflictC = 0;
  vec<Lit>    learnt_clause;
  starts++;

  for (;;) {
    CRef confl = propagate();
    if (confl != CRef_Undef) {
      // CONFLICT
      conflicts++; conflictC++;
      if (decisionLevel() == 0) return l_False;

      learnt_clause.clear();
      analyze(confl, learnt_clause, backtrack_level);
      cancelUntil(backtrack_level);

      if (learnt_clause.size() == 1) {
        uncheckedEnqueue(learnt_clause[0]);
      } else {
        CRef cr = ca.alloc(learnt_clause, true);
        learnts.push(cr);
        attachClause(cr);
        claBumpActivity(ca[cr]);
        uncheckedEnqueue(learnt_clause[0], cr);
      }

      varDecayActivity();
      claDecayActivity();

      if (--learntsize_adjust_cnt == 0) {
        learntsize_adjust_confl *= learntsize_adjust_inc;
        learntsize_adjust_cnt    = (int)learntsize_adjust_confl;
        max_learnts             *= learntsize_inc;

        if (verbosity >= 1)
          printf("| %9d | %7d %8d %8d | %8d %8d %6.0f | %6.3f %% |\n",
                 (int)conflicts,
                 (int)dec_vars - (trail_lim.size() == 0 ?
                                  trail.size() :
                                  trail_lim[0]),
                 nClauses(),
                 (int)clauses_literals,
                 (int)max_learnts,
                 nLearnts(),
                 (double)learnts_literals/nLearnts(),
                 progressEstimate()*100);
      }
    } else {
      // NO CONFLICT
      if (nof_conflicts >= 0 && conflictC >= nof_conflicts || !withinBudget()) {
        // Reached bound on number of conflicts:
        progress_estimate = progressEstimate();
        cancelUntil(0);
        return l_Undef; }

      // Simplify the set of problem clauses:
      if (decisionLevel() == 0 && !simplify())
        return l_False;

      if (learnts.size()-nAssigns() >= max_learnts)
        // Reduce the set of learnt clauses:
        reduceDB();

      Lit next = lit_Undef;
      while (decisionLevel() < assumptions.size()) {
        // Perform user provided assumption:
        Lit p = assumptions[decisionLevel()];
        if (value(p) == l_True) {
          // Dummy decision level:
          newDecisionLevel();
        } else if (value(p) == l_False) {
          analyzeFinal(~p, conflict);
          return l_False;
        } else {
          next = p;
          break;
        }
      }

      if (next == lit_Undef) {
        // New variable decision:
        decisions++;
        next = pickBranchLit();

        if (next == lit_Undef)
          // Model found:
          return l_True;
      }

      // Increase decision level and enqueue 'next'
      newDecisionLevel();
      uncheckedEnqueue(next);
    }
  }
}


double Solver::progressEstimate() const {
  double  progress = 0;
  double  F = 1.0 / nVars();

  for (int i = 0; i <= decisionLevel(); i++) {
    int beg = i == 0 ? 0 : trail_lim[i - 1];
    int end = i == decisionLevel() ? trail.size() : trail_lim[i];
    progress += pow(F, i) * (end - beg);
  }
  return progress / nVars();
}

/*
  Finite subsequences of the Luby-sequence:

  0: 1
  1: 1 1 2
  2: 1 1 2 1 1 2 4
  3: 1 1 2 1 1 2 4 1 1 2 1 1 2 4 8
  ...


*/

static double luby(double y, int x) {
  // Find the finite subsequence that contains index 'x', and the
  // size of that subsequence:
  int size, seq;
  for (size = 1, seq = 0; size < x + 1; seq++, size = 2 * size + 1);

  while (size - 1 != x) {
    size = (size-1) >> 1;
    seq--;
    x = x % size;
  }

  return pow(y, seq);
}

bool Solver::initPropagator() {
  store_unit_propagation_ = true;
  model.clear();
  conflict.clear();
  touched_variables_.clear();
  if (!ok) return false;
  return true;
}

bool Solver::propagateOneLiteral(Lit lit) {
  assert(ok);
  touched_variables_.clear();
  CRef confl = propagate();
  if (confl != CRef_Undef) {
    return false;
  }
  if (value(lit) == l_True) {
    // Dummy decision level:
    newDecisionLevel();
    return true;
  } else if (value(lit) == l_False) {
    return false;
  }
  newDecisionLevel();
  // Unchecked enqueue
  assert(value(lit) == l_Undef);
  assigns[var(lit)] = lbool(!sign(lit));
  vardata[var(lit)] = mkVarData(CRef_Undef, decisionLevel());
  trail.push_(lit);
  confl = propagate();
  return confl == CRef_Undef;
}

// NOTE: assumptions passed in member-variable 'assumptions'.
lbool Solver::solve_() {
  model.clear();
  conflict.clear();
  if (!ok) return l_False;

  solves++;

  max_learnts               = nClauses() * learntsize_factor;
  learntsize_adjust_confl   = learntsize_adjust_start_confl;
  learntsize_adjust_cnt     = (int)learntsize_adjust_confl;
  lbool   status            = l_Undef;

  if (verbosity >= 1) {
    printf("============================[ Search Statistics ]==============================\n");
    printf("| Conflicts |          ORIGINAL         |          LEARNT          | Progress |\n");
    printf("|           |    Vars  Clauses Literals |    Limit  Clauses Lit/Cl |          |\n");
    printf("===============================================================================\n");
  }

  // Search:
  int curr_restarts = 0;
  while (status == l_Undef) {
    double rest_base = luby_restart ? luby(restart_inc, curr_restarts) : pow(restart_inc, curr_restarts);
    status = search(rest_base * restart_first);
    if (!withinBudget()) break;
    curr_restarts++;
  }

  if (verbosity >= 1)
    printf("===============================================================================\n");


  if (status == l_True) {
    // Extend & copy model:
    model.growTo(nVars());
    for (int i = 0; i < nVars(); i++) model[i] = value(i);
  } else if (status == l_False && conflict.size() == 0) {
    ok = false;
  }
  cancelUntil(0);
  return status;
}

//==============================================================================
// Garbage Collection methods:

void Solver::relocAll(ClauseAllocator& to) {
  // All watchers:
  //
  // for (int i = 0; i < watches.size(); i++)
  watches.cleanAll();
  for (int v = 0; v < nVars(); v++)
    for (int s = 0; s < 2; s++) {
      Lit p = mkLit(v, s);
      // printf(" >>> RELOCING: %s%d\n", sign(p)?"-":"", var(p)+1);
      vec<Watcher>& ws = watches[p];
      for (int j = 0; j < ws.size(); j++)
        ca.reloc(ws[j].cref, to);
    }

  // All reasons:
  //
  for (int i = 0; i < trail.size(); i++) {
    Var v = var(trail[i]);

    if (reason(v) != CRef_Undef &&
        (ca[reason(v)].reloced() || locked(ca[reason(v)])))
      ca.reloc(vardata[v].reason, to);
  }

  // All learnt:
  //
  for (int i = 0; i < learnts.size(); i++)
    ca.reloc(learnts[i], to);

  // All original:
  //
  for (int i = 0; i < clauses.size(); i++)
    ca.reloc(clauses[i], to);
}

void Solver::garbageCollect() {
  // Initialize the next region to a size corresponding to the
  // estimated utilization degree. This is not precise but should
  // avoid some unnecessary reallocations for the new region:
  ClauseAllocator to(ca.size() - ca.wasted());

  relocAll(to);
  if (verbosity >= 2)
    printf("|  Garbage collection:   %12d bytes => %12d bytes             |\n",
           ca.size() * ClauseAllocator::Unit_Size, to.size() *
           ClauseAllocator::Unit_Size);
  to.moveTo(ca);
}
}  // namespace Minisat

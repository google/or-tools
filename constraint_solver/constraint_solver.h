// Copyright 2010 Google
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
//
//  Declaration of the core objects for the constraint solver.
//
//
// Here is a very simple Constraint Programming problem:
//  Knowing that we see 56 legs and 20 heads, how many pheasants and rabbits
//  are we looking at?
//
// Here is a simple cp code to find out:
// void pheasant() {
//   Solver s("pheasant");
//   IntVar* const p = s.MakeIntVar(0, 20, "pheasant"));
//   IntVar* const r = s.MakeIntVar(0, 20, "rabbit"));
//   IntExpr* const legs = s.MakeSum(s.MakeProd(p, 2), s.MakeProd(r, 4));
//   IntExpr* const heads = s.MakeSum(p, r);
//   Constraint* const ct_legs = s.MakeEquality(legs, 56);
//   Constraint* const ct_heads = s.MakeEquality(heads, 20);
//   s.AddConstraint(ct_legs);
//   s.AddConstraint(ct_heads);
//   DecisionBuilder* const db = s.Phase(p, r, Solver::CHOOSE_FIRST_UNBOUND,
//                                       Solver::ASSIGN_MIN_VALUE);
//   s.NewSearch(db);
//   CHECK(s.NextSolution());
//   LG << "rabbits -> " << r->Value() << ", pheasants -> " << p->Value();
//   LG << s.DebugString();
//   s.EndSearch();
// }
//
// which outputs:
// rabbits -> 8, pheasants -> 12
// Solver(name = "pheasant",
//        state = OUTSIDE_SEARCH,
//        branches = 0,
//        fails = 0,
//        decisions = 0
//        propagation loops = 11,
//        demons Run = 25,
//        Run time = 0 ms)
//
// More infos: go/operations_research
//
// Global remark: many functions and methods in this file can take as argument
// either a const vector<IntVar>& or a IntVar* const* and a size; the two
// signatures are equivalent, size defining the number of variables.

#ifndef CONSTRAINT_SOLVER_CONSTRAINT_SOLVER_H_
#define CONSTRAINT_SOLVER_CONSTRAINT_SOLVER_H_

#include <vector>
#include <string>

#include "base/callback.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/strutil.h"
#include "base/map-util.h"
#include "base/random.h"

class File;
using operations_research::WallTimer;
#define ClockTimer WallTimer

namespace operations_research {

class Action;
class Assignment;
class AssignmentProto;
class BaseObject;
class BooleanVar;
class ClockTimer;
class Constraint;
class Decision;
class DecisionBuilder;
class DecisionVisitor;
class Demon;
class DemonMonitor;
class DemonProfiler;
class Dimension;
class DomainIntVar;
class EqualityVarCstCache;
class ExpressionCache;
class GreaterEqualCstCache;
class IntervalVar;
class IntervalVarAssignmentProto;
class IntervalVarElement;
class IntExpr;
class IntVar;
class IntVarAssignmentProto;
class IntVarElement;
class LessEqualCstCache;
class LocalSearchFilter;
class LocalSearchOperator;
class LocalSearchPhaseParameters;
class MPSolver;
class OptimizeVar;
class Pack;
class PropagationBaseObject;
class Queue;
class Search;
class SearchLimit;
class SearchMonitor;
class Sequence;
class SolutionCollector;
class SolutionPool;
class Solver;
class SymmetryBreaker;
struct StateInfo;
struct Trail;
class UnequalityVarCstCache;
class VariableQueueCleaner;
template <class T> class SimpleRevFIFO;

// This enum is used internally to tag states in the search tree.
enum MarkerType {
  SENTINEL,
  SIMPLE_MARKER,
  CHOICE_POINT,
  REVERSIBLE_ACTION
};

// This struct holds all parameters for the Solver object.
struct SolverParameters {
 public:
  enum TrailCompression {
    NO_COMPRESSION,
    COMPRESS_WITH_ZLIB
  };

  enum ProfileLevel {
    NO_PROFILING,
    NORMAL_PROFILING
  };

  static const TrailCompression kDefaultTrailCompression;
  static const int kDefaultTrailBlockSize;
  static const int kDefaultArraySplitSize;
  static const bool kDefaultNameStoring;
  static const ProfileLevel kDefaultProfileLevel;

  SolverParameters()
      : compress_trail(kDefaultTrailCompression),
        trail_block_size(kDefaultTrailBlockSize),
        array_split_size(kDefaultArraySplitSize),
        store_names(kDefaultNameStoring),
        profile_level(kDefaultProfileLevel) {}

  // This parameter indicates if the solver should compress the trail
  // during the search. No compression means the solver will be faster,
  // but will use more memory.
  TrailCompression compress_trail;
  // This parameter indicates the default size of a block of the trail.
  // Compression applies at the block level.
  int trail_block_size;
  // When a sum/min/max operations is applied on a large array, this
  // array is recursively split into blocks of size 'array_split_size'.
  int array_split_size;
  // This parameters indicates if the solver should store the names of
  // the objets it manages.
  bool store_names;
  // Support for profiling propagation. LIGHT supports only a reduced
  // version of the summary. COMPLETE supports the full version of the
  // summary, as well as the csv export.
  ProfileLevel profile_level;
};

// This struct holds all parameters for the default search.
struct DefaultPhaseParameters {
 public:
  static const int kDefaultNumberOfSplits;
  static const int kDefaultHeuristicPeriod;
  static const int kDefaultHeuristicNumFailuresLimit;
  static const int kDefaultSeed;
  enum VariableSelection {
    CHOOSE_MAX_SUM_IMPACT = 0,
    CHOOSE_MAX_AVERAGE_IMPACT = 1,
    CHOOSE_MAX_VALUE_IMPACT = 2,
  };

  enum ValueSelection {
    SELECT_MIN_IMPACT = 0,
    SELECT_MAX_IMPACT = 1,
  };

  DefaultPhaseParameters()
      : var_selection_schema(CHOOSE_MAX_SUM_IMPACT),
        value_selection_schema(SELECT_MIN_IMPACT),
        initialization_splits(kDefaultNumberOfSplits),
        run_all_heuristics(true),
        heuristic_period(kDefaultHeuristicPeriod),
        heuristic_num_failures_limit(kDefaultHeuristicNumFailuresLimit),
        random_seed(kDefaultSeed) {}

  // This parameter describes how the next variable to instantiate
  // will be chosen.
  VariableSelection var_selection_schema;
  // This parameter describes which value to select for a given var.
  ValueSelection value_selection_schema;
  // Maximum number of intervals the initialization of impacts will scan
  // per variable.
  int initialization_splits;
  // The default phase will run heuristic periodically. This parameter
  // indicates if we should run all heuristics, or a randomly selected
  // one.
  bool run_all_heuristics;
  // The distance in nodes between each run of the heuristics.
  int heuristic_period;
  // The failure limit for each heuristic that we run.
  int heuristic_num_failures_limit;
  // Seed used to initialize the random part in some heuristics.
  int random_seed;
};

/////////////////////////////////////////////////////////////////////
//
// Solver Class
//
// A solver represent the main computation engine. It implements the whole
// range of Constraint Programming protocol:
//   - Reversibility
//   - Propagation
//   - Search
//
// Usually, a Constraint Programming model consists of
//   The creation of the Solver
//   The creation of the decision variables of the model
//   The creation of the constraints of the model and their addition to the
//     solver() through the AddConstraint() method
//   The creation of the main DecisionBuilder class
//   The launch of the solve method with the above-created decision builder
//
// At the time being, Solver is not MT_SAFE, nor MT_HOT.
/////////////////////////////////////////////////////////////////////

class Solver {
 public:
  // Callback typedefs
  typedef ResultCallback1<int64, int64> IndexEvaluator1;
  typedef ResultCallback2<int64, int64, int64> IndexEvaluator2;
  typedef ResultCallback3<int64, int64, int64, int64> IndexEvaluator3;

  // Number of priorities for demons.
  static const int kNumPriorities = 3;

  // This enum represents the state of the solver w.r.t. the search.
  enum SolverState {
    OUTSIDE_SEARCH,     // Before search, after search.
    IN_SEARCH,          // Executing the search code.
    AT_SOLUTION,        // After successful NextSolution and before EndSearch.
    NO_MORE_SOLUTIONS,  // After failed NextSolution and before EndSearch.
    PROBLEM_INFEASIBLE  // After search, the model is infeasible.
  };

  enum IntVarStrategy {
    INT_VAR_DEFAULT,
    INT_VAR_SIMPLE,
    CHOOSE_FIRST_UNBOUND,
    CHOOSE_RANDOM,
    CHOOSE_MIN_SIZE_LOWEST_MIN,
    CHOOSE_MIN_SIZE_HIGHEST_MIN,
    CHOOSE_MIN_SIZE_LOWEST_MAX,
    CHOOSE_MIN_SIZE_HIGHEST_MAX,
    CHOOSE_PATH,
  };

  enum IntValueStrategy {
    INT_VALUE_DEFAULT,
    INT_VALUE_SIMPLE,
    ASSIGN_MIN_VALUE,
    ASSIGN_MAX_VALUE,
    ASSIGN_RANDOM_VALUE,
    ASSIGN_CENTER_VALUE
  };

  enum EvaluatorStrategy {
    CHOOSE_STATIC_GLOBAL_BEST,
    CHOOSE_DYNAMIC_GLOBAL_BEST,
  };

  enum SequenceStrategy {
    SEQUENCE_DEFAULT,
    SEQUENCE_SIMPLE,
    CHOOSE_MIN_SLACK_RANK_FORWARD
  };

  enum IntervalStrategy {
    INTERVAL_DEFAULT,
    INTERVAL_SIMPLE,
    INTERVAL_SET_TIMES_FORWARD
  };

  enum LocalSearchOperators {
    TWOOPT,
    OROPT,
    RELOCATE,
    EXCHANGE,
    CROSS,
    MAKEACTIVE,
    MAKEINACTIVE,
    SWAPACTIVE,
    EXTENDEDSWAPACTIVE,
    PATHLNS,
    UNACTIVELNS,
    INCREMENT,
    DECREMENT,
    SIMPLELNS
  };

  enum EvaluatorLocalSearchOperators {
    LK,
    TSPOPT,
    TSPLNS
  };

  enum LocalSearchFilterBound {
    GE,
    LE,
    EQ
  };

  enum LocalSearchOperation {
    SUM,
    PROD,
    MAX,
    MIN
  };

  enum DemonPriority {
    DELAYED_PRIORITY = 0,
    VAR_PRIORITY = 1,
    NORMAL_PRIORITY = 2,
  };

  enum BinaryIntervalRelation {
    ENDS_AFTER_END,
    ENDS_AFTER_START,
    ENDS_AT_END,
    ENDS_AT_START,
    STARTS_AFTER_END,
    STARTS_AFTER_START,
    STARTS_AT_END,
    STARTS_AT_START
  };

  enum UnaryIntervalRelation {
    ENDS_AFTER,
    ENDS_AT,
    ENDS_BEFORE,
    STARTS_AFTER,
    STARTS_AT,
    STARTS_BEFORE,
    CROSS_DATE,
    AVOID_DATE
  };

  enum DecisionModification {
    NO_CHANGE,
    KEEP_LEFT,
    KEEP_RIGHT,
    KILL_BOTH,
    SWITCH_BRANCHES
  };

  explicit Solver(const string& modelname);
  Solver(const string& modelname, const SolverParameters& parameters);
  ~Solver();

  // Init.
  void Init();

  // Read-only Parameters.
  const SolverParameters& parameters() const { return parameters_; }

  // reversibility

  // SaveValue() will save the value of the corresponding object. It must be
  //   called before modifying the object. The value will be restored upon
  //   backtrack.
  template <class T> void SaveValue(T* o) {
    InternalSaveValue(o);
  }

  // The RevAlloc method stores the corresponding object in a stack.
  // When the solver will backtrack, it will loop through all stored
  // objects and call delete on them.
  // Supported object types are arrays of integers (32 and 64 bits),
  // BaseObjects and arrays of BaseObjects.
  // After a call to RevAlloc, solver takes ownership of the object memory and
  // will delete the object itself. The user must not delete the object
  // after it has be passed to the RevAlloc method.
  template <class T> T* RevAlloc(T* o) {
    return reinterpret_cast<T*>(SafeRevAlloc(o));
  }

  // propagation

  // This method adds the constraint "c" to the solver.
  void AddConstraint(Constraint* const c);

  // search

  // Top level solve using a decision builder and up to four search
  // monitors, usually one for the objective, one for the limits and
  // one to collect solutions. Please note that the search is
  // backtracked when the Solve() exits. Thus, solutions and values
  // should be exported either using a decision builder, or a solution
  // collector. You can also decompose with a NewSearch(db),
  // NextSolution(), EndSearch() and access each feasible solution
  // after each successful call to NextSolution().  Solve() returns
  // true if the search has found solutions, false otherwise.
  bool Solve(DecisionBuilder* const db, const vector<SearchMonitor*>& monitors);
  bool Solve(DecisionBuilder* const db,
             SearchMonitor* const * monitors, int size);
  bool Solve(DecisionBuilder* const db);
  bool Solve(DecisionBuilder* const db, SearchMonitor* const m1);
  bool Solve(DecisionBuilder* const db,
             SearchMonitor* const m1,
             SearchMonitor* const m2);
  bool Solve(DecisionBuilder* const db,
             SearchMonitor* const m1,
             SearchMonitor* const m2,
             SearchMonitor* const m3);
  bool Solve(DecisionBuilder* const db,
             SearchMonitor* const m1,
             SearchMonitor* const m2,
             SearchMonitor* const m3,
             SearchMonitor* const m4);

  // Decomposed top level search.
  // The code should look like
  // solver->NewSearch(db);
  // while (solver->NextSolution()) {
  //   .. use the current solution
  // }
  // solver()->EndSearch();
  void NewSearch(DecisionBuilder* const db,
                 const vector<SearchMonitor*>& monitors);
  void NewSearch(DecisionBuilder* const db,
                 SearchMonitor* const * monitors, int size);
  void NewSearch(DecisionBuilder* const db);
  void NewSearch(DecisionBuilder* const db, SearchMonitor* const m1);
  void NewSearch(DecisionBuilder* const db,
                 SearchMonitor* const m1,
                 SearchMonitor* const m2);
  void NewSearch(DecisionBuilder* const db,
                 SearchMonitor* const m1,
                 SearchMonitor* const m2,
                 SearchMonitor* const m3);
  void NewSearch(DecisionBuilder* const db,
                 SearchMonitor* const m1,
                 SearchMonitor* const m2,
                 SearchMonitor* const m3,
                 SearchMonitor* const m4);

  bool NextSolution();
  void RestartSearch();
  void EndSearch();

  // Nested solve using a decision builder and up to three
  //   search monitors, usually one for the objective, one for the limits
  //   and one to collect solutions.
  // The restore parameter indicates if the search should backtrack completely
  // after completion, even in case of success.
  bool NestedSolve(DecisionBuilder* const db,
                   bool restore,
                   const vector<SearchMonitor*>& monitors);
  bool NestedSolve(DecisionBuilder* const db,
                   bool restore,
                   SearchMonitor* const * monitors,
                   int size);
  bool NestedSolve(DecisionBuilder* const db, bool restore);
  bool NestedSolve(DecisionBuilder* const db,
                   bool restore,
                   SearchMonitor* const m1);
  bool NestedSolve(DecisionBuilder* const db,
                   bool restore,
                   SearchMonitor* const m1, SearchMonitor* const m2);
  bool NestedSolve(DecisionBuilder* const db,
                   bool restore,
                   SearchMonitor* const m1,
                   SearchMonitor* const m2,
                   SearchMonitor* const m3);

  // This method returns the validity of the given assignment against the
  // current model.
  bool CheckAssignment(Assignment* const assignment);

  // state of the solver.
  SolverState state() const { return state_; }

  // Abandon the current branch in the search tree. A backtrack will follow.
  void Fail();

  // When SaveValue() is not the best way to go, one can create a reversible
  // action that will be called upon backtrack. The "fast" parameter
  // indicates whether we need restore all values saved through SaveValue()
  // before calling this method.
  void AddBacktrackAction(Action* a, bool fast);

  // misc debug string.
  string DebugString() const;

  // Current memory usage in bytes
  static int64 MemoryUsage();


  // wall_time() in ms since the creation of the solver.
  int64 wall_time() const;

  // number of branches explored since the creation of the solver.
  int64 branches() const { return branches_; }

  // number of solutions found since the start of the search.
  int64 solutions() const;

  // number of demons executed during search for a given priority.
  int64 demon_runs(DemonPriority p) const { return demon_runs_[p]; }

  // number of failures encountered since the creation of the solver.
  int64 failures() const { return fails_; }

  // number of neighbors created
  int64 neighbors() const { return neighbors_; }

  // number of filtered neighbors (neighbors accepted by filters)
  int64 filtered_neighbors() const { return filtered_neighbors_; }

  // number of accepted neighbors
  int64 accepted_neighbors() const { return accepted_neighbors_; }

  // The stamp indicates how many moves in the search tree we have performed.
  // It is useful to detect if we need to update same lazy structures.
  uint64 stamp() const;

  // The fail_stamp() is incremented after each backtrack.
  uint64 fail_stamp() const;

  // ---------- Make Factory ----------

  // All factories (MakeXXX methods) encapsulate creation of objects
  // through RevAlloc(). Hence, the Solver used for allocating the
  // returned object will retain ownership of the allocated memory.
  // Destructors are called upon backtrack, or when the Solver is
  // itself destructed.

  // ----- Int Variables and Constants -----

  // MakeIntVar will create the best range based int var for the bounds given.
  IntVar* MakeIntVar(int64 vmin, int64 vmax, const string& name);

  // MakeIntVar will create a variable with the given sparse domain.
  IntVar* MakeIntVar(const vector<int64>& values, const string& name);

  // MakeIntVar will create the best range based int var for the bounds given.
  IntVar* MakeIntVar(int64 vmin, int64 vmax);

  // MakeIntVar will create a variable with the given sparse domain.
  IntVar* MakeIntVar(const vector<int64>& values);

  // MakeBoolVar will create a variable with a {0, 1} domain.
  IntVar* MakeBoolVar(const string& name);

  // MakeBoolVar will create a variable with a {0, 1} domain.
  IntVar* MakeBoolVar();

  // IntConst will create a constant expression.
  IntVar* MakeIntConst(int64 val, const string& name);

  // IntConst will create a constant expression.
  IntVar* MakeIntConst(int64 val);

  // This method will append the vector vars with 'var_count' variables
  // having bounds vmin and vmax and having name "name<i>" where <i> is
  // the index of the variable.
  void MakeIntVarArray(int var_count,
                       int64 vmin,
                       int64 vmax,
                       const string& name,
                       vector<IntVar*>* vars);
  // This method will append the vector vars with 'var_count' variables
  // having bounds vmin and vmax and having no names.
  void MakeIntVarArray(int var_count,
                       int64 vmin,
                       int64 vmax,
                       vector<IntVar*>* vars);
  // Same but allocates an array and returns it.
  IntVar** MakeIntVarArray(int var_count,
                           int64 vmin,
                           int64 vmax,
                           const string& name);


  // This method will append the vector vars with 'var_count' boolean
  // variables having name "name<i>" where <i> is the index of the
  // variable.
  void MakeBoolVarArray(int var_count,
                        const string& name,
                        vector<IntVar*>* vars);
  // This method will append the vector vars with 'var_count' boolean
  // variables having no names.
  void MakeBoolVarArray(int var_count,
                        vector<IntVar*>* vars);
  // Same but allocates an array and returns it.
  IntVar** MakeBoolVarArray(int var_count, const string& name);

  // ----- Integer Expressions -----

  // left + right.
  IntExpr* MakeSum(IntExpr* const left, IntExpr* const right);
  // expr + value.
  IntExpr* MakeSum(IntExpr* const expr, int64 value);
  // sum of all vars.
  IntExpr* MakeSum(IntVar* const* vars, int size);
  // sum of all vars.
  IntExpr* MakeSum(const vector<IntVar*>& vars);

  // scalar product
  IntExpr* MakeScalProd(const vector<IntVar*>& vars,
                        const vector<int64>& coefs);
  // scalar product
  IntExpr* MakeScalProd(IntVar* const* vars,
                        const int64* const coefs,
                        int size);
  // scalar product
  IntExpr* MakeScalProd(const vector<IntVar*>& vars,
                        const vector<int>& coefs);
  // scalar product
  IntExpr* MakeScalProd(IntVar* const* vars,
                        const int* const coefs,
                        int size);

  // left - right
  IntExpr* MakeDifference(IntExpr* const left, IntExpr* const right);
  // value - expr
  IntExpr* MakeDifference(int64 value, IntExpr* const expr);
  // -expr
  IntExpr* MakeOpposite(IntExpr* const expr);

  // left * right
  IntExpr* MakeProd(IntExpr* const left, IntExpr* const right);
  // expr * value
  IntExpr* MakeProd(IntExpr* const expr, int64 value);

  // expr / value (integer division)
  IntExpr* MakeDiv(IntExpr* const expr, int64 value);
  // numerator / denominator (integer division). Terms need to be positive.
  IntExpr* MakeDiv(IntExpr* const numerator, IntExpr* const denominator);

  // |expr|
  IntExpr* MakeAbs(IntExpr* const expr);
  // expr * expr
  IntExpr* MakeSquare(IntExpr* const expr);

  // vals[expr]
  IntExpr* MakeElement(const int64* const vals, int size, IntVar* const index);
  // vals[expr]
  IntExpr* MakeElement(const vector<int64>& vals, IntVar* const index);

  // Function-based, constraint takes ownership of callback
  // The callback must be able to cope with any possible value in the
  // domain of 'index' (potentially negative ones too).
  // TODO(user): Add typedef for callbacks below.
  IntExpr* MakeElement(IndexEvaluator1* values, IntVar* const index);
  // 2D version of function-based element expression, values(expr1, expr2).
  IntExpr* MakeElement(IndexEvaluator2* values,
                       IntVar* const index1, IntVar* const index2);

  // vars[expr]
  IntExpr* MakeElement(const IntVar* const * vars, int size,
                       IntVar* const index);
  // vars[expr]
  IntExpr* MakeElement(const vector<IntVar*>& vars, IntVar* const index);

  // min(vars)
  IntExpr* MakeMin(const vector<IntVar*>& vars);
  // min(vars)
  IntExpr* MakeMin(IntVar* const* vars, int size);
  // min (left, right)
  IntExpr* MakeMin(IntExpr* const left, IntExpr* const right);
  // min(expr, val)
  IntExpr* MakeMin(IntExpr* const expr, int64 val);
  // min(expr, val)
  IntExpr* MakeMin(IntExpr* const expr, int val);

  // max(vars)
  IntExpr* MakeMax(const vector<IntVar*>& vars);
  // max(vars)
  IntExpr* MakeMax(IntVar* const* vars, int size);
  // max(left, right)
  IntExpr* MakeMax(IntExpr* const left, IntExpr* const right);
  // max(expr, val)
  IntExpr* MakeMax(IntExpr* const expr, int64 val);
  // max(expr, val)
  IntExpr* MakeMax(IntExpr* const expr, int val);

  // convex piecewise function.
  IntExpr* MakeConvexPiecewiseExpr(IntVar* e,
                                   int64 early_cost, int64 early_date,
                                   int64 late_date, int64 late_cost);

  // Semi continuous Expression (x <= 0 -> f(x) = 0; x > 0 -> f(x) = ax + b)
  // a >= 0 and b >= 0
  IntExpr* MakeSemiContinuousExpr(IntExpr* e, int64 fixed_charge, int64 step);

  // ----- Constraints -----
  // This constraint always succeeds.
  Constraint* MakeTrueConstraint();
  // This constraint always fails.
  Constraint* MakeFalseConstraint();

  // b == (v == c)
  Constraint* MakeIsEqualCstCt(IntVar* const v, int64 c, IntVar* const b);
  // status var of (v == c)
  IntVar* MakeIsEqualCstVar(IntVar* const var, int64 value);
  // b == (v1 == v2)
  Constraint* MakeIsEqualCt(IntExpr* const v1, IntExpr* v2, IntVar* const b);
  // status var of (v1 == v2)
  IntVar* MakeIsEqualVar(IntExpr* const var, IntExpr* v2);
  // left == right
  Constraint* MakeEquality(IntVar* const left, IntVar* const right);
  // expr == value
  Constraint* MakeEquality(IntExpr* const expr, int64 value);
  // expr == value
  Constraint* MakeEquality(IntExpr* const expr, int value);

  // b == (v != c)
  Constraint* MakeIsDifferentCstCt(IntVar* const v, int64 c, IntVar* const b);
  // status var of (v != c)
  IntVar* MakeIsDifferentCstVar(IntVar* const v, int64 c);
  // status var of (v1 != v2)
  IntVar* MakeIsDifferentVar(IntExpr* const v1, IntExpr* const v2);
  // b == (v1 != v2)
  Constraint* MakeIsDifferentCt(IntExpr* const v1, IntExpr* const v2,
                                IntVar* const b);
  // left != right
  Constraint* MakeNonEquality(IntVar* const left, IntVar* const right);
  // expr != value
  Constraint* MakeNonEquality(IntVar* const expr, int64 value);
  // expr != value
  Constraint* MakeNonEquality(IntVar* const expr, int value);

  // b == (v <= c)
  Constraint* MakeIsLessOrEqualCstCt(IntVar* const v, int64 c,
                                     IntVar* const b);
  // status var of (v <= c)
  IntVar* MakeIsLessOrEqualCstVar(IntVar* const v, int64 c);
  // status var of (left <= right)
  IntVar* MakeIsLessOrEqualVar(IntExpr* const left, IntExpr* const right);
  // b == (left <= right)
  Constraint* MakeIsLessOrEqualCt(IntExpr* const left, IntExpr* const right,
                                  IntVar* const b);
  // left <= right
  Constraint* MakeLessOrEqual(IntVar* const left, IntVar* const right);
  // expr <= value
  Constraint* MakeLessOrEqual(IntExpr* const expr, int64 value);
  // expr <= value
  Constraint* MakeLessOrEqual(IntExpr* const expr, int value);

  // b == (v >= c)
  Constraint* MakeIsGreaterOrEqualCstCt(IntVar* const v, int64 c,
                                        IntVar* const b);
  // status var of (v >= c)
  IntVar* MakeIsGreaterOrEqualCstVar(IntVar* const v, int64 c);
  // status var of (left >= right)
  IntVar* MakeIsGreaterOrEqualVar(IntExpr* const left, IntExpr* const right);
  // b == (left >= right)
  Constraint* MakeIsGreaterOrEqualCt(IntExpr* const left, IntExpr* const right,
                                     IntVar* const b);
  // left >= right
  Constraint* MakeGreaterOrEqual(IntVar* const left, IntVar* const right);
  // expr >= value
  Constraint* MakeGreaterOrEqual(IntExpr* const expr, int64 value);
  // expr >= value
  Constraint* MakeGreaterOrEqual(IntExpr* const expr, int value);

  // b == (v > c)
  Constraint* MakeIsGreaterCstCt(IntVar* const v, int64 c, IntVar* const b);
  // status var of (v > c)
  IntVar* MakeIsGreaterCstVar(IntVar* const v, int64 c);
  // status var of (left > right)
  IntVar* MakeIsGreaterVar(IntExpr* const left, IntExpr* const right);
  // b == (left > right)
  Constraint* MakeIsGreaterCt(IntExpr* const left, IntExpr* const right,
                              IntVar* const b);
  // left > right
  Constraint* MakeGreater(IntVar* const left, IntVar* const right);
  // expr > value
  Constraint* MakeGreater(IntExpr* const expr, int64 value);
  // expr > value
  Constraint* MakeGreater(IntExpr* const expr, int value);

  // b == (v < c)
  Constraint* MakeIsLessCstCt(IntVar* const v, int64 c, IntVar* const b);
  // status var of (v < c)
  IntVar* MakeIsLessCstVar(IntVar* const v, int64 c);
  // status var of (left < right)
  IntVar* MakeIsLessVar(IntExpr* const left, IntExpr* const right);
  // b == (left < right)
  Constraint* MakeIsLessCt(IntExpr* const left, IntExpr* const right,
                           IntVar* const b);
  // left < right
  Constraint* MakeLess(IntVar* const left, IntVar* const right);
  // expr < value
  Constraint* MakeLess(IntExpr* const expr, int64 value);
  // expr < value
  Constraint* MakeLess(IntExpr* const expr, int value);

  // Variation on arrays.
  Constraint* MakeSumLessOrEqual(const vector<IntVar*>& vars, int64 cst);
  Constraint* MakeSumLessOrEqual(IntVar* const* vars, int size, int64 cst);

  Constraint* MakeSumGreaterOrEqual(const vector<IntVar*>& vars, int64 cst);
  Constraint* MakeSumGreaterOrEqual(IntVar* const* vars, int size, int64 cst);

  Constraint* MakeSumEquality(const vector<IntVar*>& vars, int64 cst);
  Constraint* MakeSumEquality(IntVar* const* vars, int size, int64 cst);

  Constraint* MakeScalProdEquality(const vector<IntVar*>& vars,
                                   const vector<int64>& coefficients,
                                   int64 cst);
  Constraint* MakeScalProdEquality(IntVar* const* vars,
                                   int size,
                                   int64 const * coefficients,
                                   int64 cst);
  Constraint* MakeScalProdEquality(IntVar* const* vars,
                                   int size,
                                   int const * coefficients,
                                   int64 cst);
  Constraint* MakeScalProdEquality(const vector<IntVar*>& vars,
                                   const vector<int>& coefficients,
                                   int64 cst);
  Constraint* MakeScalProdGreaterOrEqual(const vector<IntVar*>& vars,
                                         const vector<int64>& coefficients,
                                         int64 cst);
  Constraint* MakeScalProdGreaterOrEqual(IntVar* const* vars,
                                         int size,
                                         int64 const * coefficients,
                                         int64 cst);
  Constraint* MakeScalProdGreaterOrEqual(const vector<IntVar*>& vars,
                                         const vector<int>& coefficients,
                                         int64 cst);
  Constraint* MakeScalProdGreaterOrEqual(IntVar* const* vars,
                                         int size,
                                         int const * coefficients,
                                         int64 cst);
  Constraint* MakeScalProdLessOrEqual(const vector<IntVar*>& vars,
                                      const vector<int64>& coefficients,
                                      int64 cst);
  Constraint* MakeScalProdLessOrEqual(IntVar* const* vars,
                                      int size,
                                      int64 const * coefficients,
                                      int64 cst);
  Constraint* MakeScalProdLessOrEqual(const vector<IntVar*>& vars,
                                      const vector<int>& coefficients,
                                      int64 cst);
  Constraint* MakeScalProdLessOrEqual(IntVar* const* vars,
                                      int size,
                                      int const * coefficients,
                                      int64 cst);

  // This method is a specialized case of the MakeConstraintDemon
  // method to call the InitiatePropagate of the constraint 'ct'.
  Demon* MakeConstraintInitialPropagateCallback(Constraint* const ct);

  Demon* MakeDelayedConstraintInitialPropagateCallback(Constraint* const ct);

  // (l <= b <= u)
  Constraint* MakeBetweenCt(IntVar* const v, int64 l, int64 u);

  // b == (l <= v <= u)
  Constraint* MakeIsBetweenCt(IntVar* const v,  int64 l, int64 u,
                              IntVar* const b);

  // b == (v in set)
  Constraint* MakeIsMemberCt(IntVar* const v, const int64* const values,
                             int size, IntVar* const b);
  Constraint* MakeIsMemberCt(IntVar* const v, const vector<int64>& values,
                             IntVar* const b);
  IntVar* MakeIsMemberVar(IntVar* const v, const int64* const values, int size);
  IntVar* MakeIsMemberVar(IntVar* const v, const vector<int64>& values);
  // v in set. Propagation is lazy, i.e. this constraint does not
  // creates holes in the domain of the variable.
  Constraint* MakeMemberCt(IntVar* const v, const int64* const values,
                           int size);
  Constraint* MakeMemberCt(IntVar* const v, const vector<int64>& values);


  // |{i | v[i] == value}| == count
  Constraint* MakeCount(const vector<IntVar*>& v, int64 value, int64 count);
  // |{i | v[i] == value}| == count
  Constraint* MakeCount(const vector<IntVar*>& v, int64 value,
                        IntVar* const count);

  // Aggregated version of count:  |{i | v[i] == values[j]}| == cards[j]
  Constraint* MakeDistribute(const vector<IntVar*>& vars,
                             const vector<int64>& values,
                             const vector<IntVar*>& cards);
  // Aggregated version of count:  |{i | v[i] == j}| == cards[j]
  Constraint* MakeDistribute(const vector<IntVar*>& vars,
                             const vector<IntVar*>& cards);
  // Aggregated version of count with bounded cardinalities:
  // forall j in 0 .. card_size - 1: card_min <= |{i | v[i] == j}| <= card_max
  Constraint* MakeDistribute(const vector<IntVar*>& vars,
                             int64 card_min,
                             int64 card_max,
                             int64 card_size);

  // All variables are pairwise different.
  Constraint* MakeAllDifferent(const vector<IntVar*>& vars, bool range);
  // All variables are pairwise different.
  Constraint* MakeAllDifferent(const IntVar* const* vars,
                               int size, bool range);

  // Prevent cycles, nexts variables representing the next in the chain.
  // Active variables indicate if the corresponding next variable is active;
  // this could be useful to model unperformed nodes in a routing problem.
  // A callback can be added to specify sink values (by default sink values
  // are values >= vars.size()). Ownership of the callback is passed to the
  // constraint.
  // If assume_paths is either not specified or true, the constraint assumes the
  // 'next' variables represent paths (and performs a faster propagation);
  // otherwise the constraint assumes the 'next' variables represent a forest.
  Constraint* MakeNoCycle(const vector<IntVar*>& nexts,
                          const vector<IntVar*>& active,
                          ResultCallback1<bool, int64>* sink_handler = NULL);
  Constraint* MakeNoCycle(const IntVar* const* nexts,
                          const IntVar* const* active,
                          int size,
                          ResultCallback1<bool, int64>* sink_handler = NULL);
  Constraint* MakeNoCycle(const vector<IntVar*>& nexts,
                          const vector<IntVar*>& active,
                          ResultCallback1<bool, int64>* sink_handler,
                          bool assume_paths);
  Constraint* MakeNoCycle(const IntVar* const* nexts,
                          const IntVar* const* active,
                          int size,
                          ResultCallback1<bool, int64>* sink_handler,
                          bool assume_paths);

  // Accumulate values along a path such that:
  // cumuls[next[i]] = cumuls[i] + transits[i].
  // Active variables indicate if the corresponding next variable is active;
  // this could be useful to model unperformed nodes in a routing problem.
  Constraint* MakePathCumul(const vector<IntVar*>& nexts,
                            const vector<IntVar*>& active,
                            const vector<IntVar*>& cumuls,
                            const vector<IntVar*>& transits);
  Constraint* MakePathCumul(const IntVar* const* nexts,
                            const IntVar* const* active,
                            const IntVar* const* cumuls,
                            const IntVar* const* transits,
                            int next_size,
                            int cumul_size);

  // This constraint maps the domain of 'var' onto the array of
  // variables 'vars'. That is
  // for all i in [0 .. size - 1]: vars[i] == 1 <=> var->Contains(i);
  Constraint* MakeMapDomain(IntVar* const var, IntVar* const * vars, int size);

  // This constraint maps the domain of 'var' onto the array of
  // variables 'vars'. That is
  // for all i in [0 .. size - 1]: vars[i] == 1 <=> var->Contains(i);
  Constraint* MakeMapDomain(IntVar* const var, const vector<IntVar*>& vars);

  // This method creates a constraint where the graph of the relation
  // between the variables is given in extension. There are 'arity'
  // variables involved in the relation and the graph is given by a
  // matrix of size 'tuple_count' x 'arity'.
  Constraint* MakeAllowedAssignments(const IntVar* const * vars,
                                     const int64* const * tuples,
                                     int tuple_count,
                                     int arity);

  Constraint* MakeAllowedAssignments(const vector<IntVar*>& vars,
                                     const vector<vector<int64> >& tuples);

  // This constraint create a finite automaton that will check the
  // sequence of variables vars. It uses a transition table called
  // 'transitions'. Each transition is a triple
  //    (current_state, variable_value, new_state).
  // The initial state is given, and the set of accepted states is decribed
  // by 'final_states'. These states are hidden inside the constraint.
  // Only the transitions (i.e. the variables) are visible.
  Constraint* MakeTransitionConstraint(
      const vector<IntVar*>& vars,
      const vector<vector<int64> >& transitions,
      int64 initial_state,
      const vector<int64>& final_states);

  // ----- Packing constraint -----

  // This constraint packs all variables onto 'number_of_bins'
  // variables.  For any given variable, a value of 'number_of_bins'
  // indicates that the variable is not assigned to any bin.
  // Dimensions, i.e. cumulative constraints on this packing can be
  // added directly from the pack class.
  Pack* MakePack(const vector<IntVar*>& vars, int number_of_bins);

  // ----- scheduling objects -----

  // Create an interval var with a fixed duration. The duration must
  // be greater than 0. If optional is true, then the interval can be
  // performed or unperformed. If optional is false, then the interval
  // is always performed.
  IntervalVar* MakeFixedDurationIntervalVar(int64 start_min,
                                            int64 start_max,
                                            int64 duration,
                                            bool optional,
                                            const string& name);

  // This method fills the vector with 'count' interval var built with
  // the corresponding parameters.
  void MakeFixedDurationIntervalVarArray(int count,
                                         int64 start_min,
                                         int64 start_max,
                                         int64 duration,
                                         bool optional,
                                         const string& name,
                                         vector<IntervalVar*>* array);


  // Create an fixed and performed interval.
  IntervalVar* MakeFixedInterval(int64 start,
                                 int64 duration,
                                 const string& name);

  // Create an interval var that is the mirror image of the given one, that is,
  // the interval var obtained by reversing the axis.
  IntervalVar* MakeMirrorInterval(IntervalVar* const interval_var);

  // Creates and returns an interval variable that wraps around the given one,
  // relaxing the min start and end. Relaxing means making unbounded when
  // optional.
  //
  // More precisely, such an interval variable behaves as follows:
  // * When the underlying must be performed, the returned interval variable
  //     behaves exactly as the underlying;
  // * When the underlying may or may not be performed, the returned interval
  //     variable behaves like the underlying, except that it is unbounded on
  //     the min side;
  // * When the underlying cannot be performed, the returned interval variable
  //     is of duration 0 and must be performed in an interval unbounded on both
  //     sides.
  //
  // This is very useful to implement propagators that may only modify
  // the start max or end max.
  IntervalVar* MakeIntervalRelaxedMin(IntervalVar* const interval_var);

  // Creates and returns an interval variable that wraps around the given one,
  // relaxing the max start and end. Relaxing means making unbounded when
  // optional.
  //
  // More precisely, such an interval variable behaves as follows:
  // * When the underlying must be performed, the returned interval variable
  //     behaves exactly as the underlying;
  // * When the underlying may or may not be performed, the returned interval
  //     variable behaves like the underlying, except that it is unbounded on
  //     the max side;
  // * When the underlying cannot be performed, the returned interval variable
  //     is of duration 0 and must be performed in an interval unbounded on both
  //     sides.
  //
  // This is very useful to implement propagators that may only modify
  // the start min or end min.
  IntervalVar* MakeIntervalRelaxedMax(IntervalVar* const interval_var);

  // ----- scheduling constraints -----

  // This method creates a relation between an interval var and a
  // date.
  Constraint* MakeIntervalVarRelation(IntervalVar* const t,
                                      UnaryIntervalRelation r,
                                      int64 d);

  // This method creates a relation between two an interval vars.
  Constraint* MakeIntervalVarRelation(IntervalVar* const t1,
                                      BinaryIntervalRelation r,
                                      IntervalVar* const t2);

  // This constraint implements a temporal disjunction between two
  // interval vars t1 and t2. 'alt' indicates which alternative was
  // chosen (alt == 0 is equivalent to t1 before t2).
  Constraint* MakeTemporalDisjunction(IntervalVar* const t1,
                                      IntervalVar* const t2,
                                      IntVar* const alt);

  // This constraint implements a temporal disjunction between two
  // interval vars.
  Constraint* MakeTemporalDisjunction(IntervalVar* const t1,
                                      IntervalVar* const t2);

  // This constraint forces all interval vars into an non overlapping
  // sequence.
  Sequence* MakeSequence(const vector<IntervalVar*>& intervals,
                         const string& name);

  // This constraint forces all interval vars into an non overlapping
  // sequence.
  Sequence* MakeSequence(const IntervalVar* const * intervals, int size,
                         const string& name);

  // This constraint forces that, for any integer t, the sum of the demands
  // corresponding to an interval containing t does not exceed the given
  // capacity.
  //
  // Intervals and demands are arrays that should both be of the given size.
  //
  // Demands should only contain non-negative values. Zero values are supported,
  // and the corresponding intervals are filtered out, as they neither impact
  // nor are impacted by this constraint.
  Constraint* MakeCumulative(IntervalVar* const * intervals,
                             const int64 * demands,
                             int size,
                             int64 capacity,
                             const string& name);

  // This constraint forces that, for any integer t, the sum of the demands
  // corresponding to an interval containing t does not exceed the given
  // capacity.
  //
  // Intervals and demands should be vectors of equal size.
  //
  // Demands should only contain non-negative values. Zero values are supported,
  // and the corresponding intervals are filtered out, as they neither impact
  // nor are impacted by this constraint.
  Constraint* MakeCumulative(const vector<IntervalVar*>& intervals,
                             const vector<int64>& demands,
                             int64 capacity,
                             const string& name);

  // ----- Assignments -----

  // This method creates an empty assignment.
  Assignment* MakeAssignment();

  // This method creates an assignnment which is a copy of 'a'.
  Assignment* MakeAssignment(const Assignment* const a);

  // ----- Solution Collectors -----

  // Collect the first solution of the search.
  SolutionCollector* MakeFirstSolutionCollector(
      const Assignment* const assignment);
  // Collect the first solution of the search. The variables will need to
  // be added later.
  SolutionCollector* MakeFirstSolutionCollector();

  // Collect the last solution of the search.
  SolutionCollector* MakeLastSolutionCollector(
      const Assignment* const assignment);
  // Collect the last solution of the search. The variables will need to
  // be added later.
  SolutionCollector* MakeLastSolutionCollector();

  // Collect the solution corresponding to the optimal value of the objective
  // of 'a'; if 'a' does not have an objective no solution is collected. This
  // collector only collects one solution corresponding to the best objective
  // value (the first one found).
  SolutionCollector* MakeBestValueSolutionCollector(
      const Assignment* const assignment, bool maximize);
  // Collect the solution corresponding to the optimal value of the
  // objective of 'a'; if 'a' does not have an objective no solution
  // is collected. This collector only collects one solution
  // corresponding to the best objective value (the first one
  // found). The variables will need to be added later.
  SolutionCollector* MakeBestValueSolutionCollector(bool maximize);

  // Collect all solutions of the search.
  SolutionCollector* MakeAllSolutionCollector(
      const Assignment* const assignment);
  // Collect all solutions of the search. The variables will need to
  // be added later.
  SolutionCollector* MakeAllSolutionCollector();

  // ----- Objective -----

  // Create a minimization objective.
  OptimizeVar* MakeMinimize(IntVar* const v, int64 step);

  // Create a maximization objective.
  OptimizeVar* MakeMaximize(IntVar* const v, int64 step);

  // Create a objective with a given sense (true = maximization).
  OptimizeVar* MakeOptimize(bool maximize, IntVar* const v, int64 step);

  // Create a minimization weighted objective. The actual objective is
  // scalar_prod(vars, weights).
  OptimizeVar* MakeWeightedMinimize(const vector<IntVar*>& vars,
                                    const vector<int64>& weights,
                                    int64 step);

  // Create a maximization weigthed objective.
  OptimizeVar* MakeWeightedMaximize(const vector<IntVar*>& vars,
                                    const vector<int64>& weights,
                                    int64 step);

  // Create a weighted objective with a given sense (true = maximization).
  OptimizeVar* MakeWeightedOptimize(bool maximize,
                                    const vector<IntVar*>& vars,
                                    const vector<int64>& weights,
                                    int64 step);

  // ----- Meta-heuristics -----
  // Search monitors which try to get the search out of local optima.

  // Create a Tabu Search monitor.
  // In the context of local search the behavior is similar to MakeOptimize(),
  // creating an objective in a given sense. The behavior differs once a local
  // optimum is reached: thereafter solutions which degrade the value of the
  // objective are allowed if they are not "tabu". A solution is "tabu" if it
  // doesn't respect the following rules:
  // - improving the best solution found so far
  // - variables in the "keep" list must keep their value, variables in the
  // "forbid" list must not take the value they have in the list.
  // Variables with new values enter the tabu lists after each new solution
  // found and leave the lists after a given number of iterations (called
  // tenure). Only the variables passed to the method can enter the lists.
  // The tabu criterion is softened by the tabu factor which gives the number
  // of "tabu" violations which is tolerated; a factor of 1 means no violations
  // allowed, a factor of 0 means all violations allowed.

  SearchMonitor* MakeTabuSearch(bool maximize,
                                IntVar* const v,
                                int64 step,
                                const vector<IntVar*>& vars,
                                int64 keep_tenure,
                                int64 forbid_tenure,
                                double tabu_factor);
  SearchMonitor* MakeTabuSearch(bool maximize,
                                IntVar* const v,
                                int64 step,
                                const IntVar* const* vars,
                                int size,
                                int64 keep_tenure,
                                int64 forbid_tenure,
                                double tabu_factor);

  // Create a Simulated Annealing monitor.
  // TODO(user): document behavior
  SearchMonitor* MakeSimulatedAnnealing(bool maximize,
                                        IntVar* const v,
                                        int64 step,
                                        int64 initial_temperature);

  // Create a Guided Local Search monitor.
  // Description here: http://en.wikipedia.org/wiki/Guided_Local_Search
  SearchMonitor* MakeGuidedLocalSearch(bool maximize,
                                       IntVar* const objective,
                                       IndexEvaluator2* objective_function,
                                       int64 step,
                                       const vector<IntVar*>& vars,
                                       double penalty_factor);
  SearchMonitor* MakeGuidedLocalSearch(bool maximize,
                                       IntVar* const objective,
                                       IndexEvaluator2* objective_function,
                                       int64 step,
                                       const IntVar* const* vars,
                                       int size,
                                       double penalty_factor);
  SearchMonitor* MakeGuidedLocalSearch(bool maximize,
                                       IntVar* const objective,
                                       IndexEvaluator3* objective_function,
                                       int64 step,
                                       const vector<IntVar*>& vars,
                                       const vector<IntVar*> secondary_vars,
                                       double penalty_factor);
  SearchMonitor* MakeGuidedLocalSearch(bool maximize,
                                       IntVar* const objective,
                                       IndexEvaluator3* objective_function,
                                       int64 step,
                                       const IntVar* const* vars,
                                       const IntVar* const* secondary_vars,
                                       int size,
                                       double penalty_factor);

  // ----- Restart Search -----

  // This search monitor will restart the search periodically.
  // At the iteration n, it will restart after scale_factor * Luby(n) failures
  // where Luby is the Luby Strategy (i.e. 1 1 2 1 1 2 4 1 1 2 1 1 2 4 8...).
  SearchMonitor* MakeLubyRestart(int scale_factor);

  // This search monitor will restart the search periodically after 'frequency'
  // failures.
  SearchMonitor* MakeConstantRestart(int frequency);


  // ----- Search Limit -----

  // Limit the search with the 'time', 'branches', 'failures' and
  // 'solutions' limits.
  SearchLimit* MakeLimit(int64 time,
                         int64 branches,
                         int64 failures,
                         int64 solutions);
  // Version reducing calls to wall timer by estimating number of remaining
  // calls.
  SearchLimit* MakeLimit(int64 time,
                         int64 branches,
                         int64 failures,
                         int64 solutions,
                         bool smart_time_check);
  void UpdateLimits(int64 time,
                    int64 branches,
                    int64 failures,
                    int64 solutions,
                    SearchLimit* limit);
  // Returns 'time' limit of search limit
  int64 GetTime(SearchLimit* limit);

  // Callback-based search limit. Search stops when limiter returns true; if
  // this happens at a leaf the corresponding solution will be rejected.
  SearchLimit* MakeCustomLimit(ResultCallback<bool>* limiter);

  // ----- Tree Monitor -----
  // Creates a tree monitor that outputs a detailed overview of the
  // decision phase in cpviz format. The XML data is written to files
  // file_tree and file_visualization as the search finishes.
  SearchMonitor* MakeTreeMonitor(const IntVar* const* vars, int size,
                                 const string& file_tree,
                                 const string& file_visualization);

  // Creates a tree monitor that outputs a detailed overview of the
  // decision phase in cpviz format. The XML data is written to files
  // file_tree and file_visualization as the search finishes.
  SearchMonitor* MakeTreeMonitor(const vector<IntVar*>& vars,
                                 const string& file_tree,
                                 const string& file_visualization);

  // Creates a tree monitor that outputs a detailed overview of the
  // decision phase in cpviz format. The XML data is written to files
  // file_config, file_tree and file_visualization as the search
  // finishes.
  SearchMonitor* MakeTreeMonitor(const IntVar* const* vars, int size,
                                 const string& file_config,
                                 const string& file_tree,
                                 const string& file_visualization);

  // Creates a tree monitor that outputs a detailed overview of the
  // decision phase in cpviz format. The XML data is written to files
  // file_config, file_tree and file_visualization as the search
  // finishes.
  SearchMonitor* MakeTreeMonitor(const vector<IntVar*>& vars,
                                 const string& file_config,
                                 const string& file_tree,
                                 const string& file_visualization);

#if !defined(SWIG)
  // Creates a tree monitor that outputs a detailed overview of the
  // decision phase in cpviz format. The XML data is copied to tree_xml
  // and visualization_xml as the search finishes. The tree monitor does
  // not take ownership of either string.
  SearchMonitor* MakeTreeMonitor(const IntVar* const* vars, int size,
                                 string* const tree_xml,
                                 string* const visualization_xml);

  // Creates a tree monitor that outputs a detailed overview of the
  // decision phase in cpviz format. The XML data is copied to tree_xml
  // and visualization_xml as the search finishes. The tree monitor does
  // not take ownership of either string.
  SearchMonitor* MakeTreeMonitor(const vector<IntVar*>& vars,
                                 string* const tree_xml,
                                 string* const visualization_xml);

  // Creates a tree monitor that outputs a detailed overview of the
  // decision phase in cpviz format. The XML data is copied to config_xml,
  // tree_xml and visualization_xml as the search finishes. The tree monitor
  // does not take ownership of these strings.
  SearchMonitor* MakeTreeMonitor(const IntVar* const* vars, int size,
                                 string* const config_xml,
                                 string* const tree_xml,
                                 string* const visualization_xml);

  // Creates a tree monitor that outputs a detailed overview of the
  // decision phase in cpviz format. The XML data is copied to config_xml,
  // tree_xml and visualization_xml as the search finishes. The tree monitor
  // does not take ownership of these strings.
  SearchMonitor* MakeTreeMonitor(const vector<IntVar*>& vars,
                                 string* const config_xml,
                                 string* const tree_xml,
                                 string* const visualization_xml);

#endif

  // TODO(user): DEPRECATE API of MakeSearchLog(.., IntVar* var,..).
  // ----- Search Log -----
  // The SearchMonitors below will display a periodic search log
  // on LOG(INFO) every branch_count branches explored.

  SearchMonitor* MakeSearchLog(int branch_count);

  // At each solution, this monitor also display the objective value.
  SearchMonitor* MakeSearchLog(int branch_count, IntVar* const objective);

  // At each solution, this monitor will also display result of @p
  // display_callback.
  SearchMonitor* MakeSearchLog(int branch_count,
                               ResultCallback<string>* display_callback);

  // At each solution, this monitor will display the objective value and the
  // result of @p display_callback.
  SearchMonitor* MakeSearchLog(int branch_count,
                               IntVar* objective,
                               ResultCallback<string>* display_callback);

  // OptimizeVar Search Logs
  // At each solution, this monitor will also display the objective->Print().

  SearchMonitor* MakeSearchLog(int branch_count, OptimizeVar* const objective);

  // Create a search monitor that will also print the result of the
  // display callback.
  SearchMonitor* MakeSearchLog(int branch_count,
                               OptimizeVar* const objective,
                               ResultCallback<string>* display_callback);


  // ----- Search Trace ------

  // Create a search monitor that will trace precisely the behavior of the
  // search. Use this only for low level debugging.
  SearchMonitor* MakeSearchTrace(const string& prefix);

  // ----- Symmetry Breaking -----

  SearchMonitor* MakeSymmetryManager(const vector<SymmetryBreaker*>& visitors);
  SearchMonitor* MakeSymmetryManager(SymmetryBreaker* const * visitors,
                                     int size);
  SearchMonitor* MakeSymmetryManager(SymmetryBreaker* const v1);
  SearchMonitor* MakeSymmetryManager(SymmetryBreaker* const v1,
                                     SymmetryBreaker* const v2);
  SearchMonitor* MakeSymmetryManager(SymmetryBreaker* const v1,
                                     SymmetryBreaker* const v2,
                                     SymmetryBreaker* const v3);
  SearchMonitor* MakeSymmetryManager(SymmetryBreaker* const v1,
                                     SymmetryBreaker* const v2,
                                     SymmetryBreaker* const v3,
                                     SymmetryBreaker* const v4);


  // ----- Search Decicions and Decision Builders -----

  // ----- Decisions -----
  Decision* MakeAssignVariableValue(IntVar* const var, int64 value);
  Decision* MakeSplitVariableDomain(IntVar* const var,
                                    int64 value,
                                    bool start_with_lower_half);
  Decision* MakeAssignVariableValueOrFail(IntVar* const var, int64 value);
  Decision* MakeAssignVariablesValues(const IntVar* const* vars, int size,
                                      const int64* const values);
  Decision* MakeAssignVariablesValues(const vector<IntVar*>& vars,
                                      const vector<int64>& values);
  Decision* MakeFailDecision();

  // Sequential composition of Decision Builders
  DecisionBuilder* Compose(DecisionBuilder* const db1,
                           DecisionBuilder* const db2);
  DecisionBuilder* Compose(DecisionBuilder* const db1,
                           DecisionBuilder* const db2,
                           DecisionBuilder* const db3);
  DecisionBuilder* Compose(DecisionBuilder* const db1,
                           DecisionBuilder* const db2,
                           DecisionBuilder* const db3,
                           DecisionBuilder* const db4);
  DecisionBuilder* Compose(const vector<DecisionBuilder*>& dbs);

  // Phases on IntVar arrays.
  DecisionBuilder* MakePhase(const vector<IntVar*>& vars,
                             IntVarStrategy var_str,
                             IntValueStrategy val_str);
  DecisionBuilder* MakePhase(const IntVar* const* vars,
                             int size,
                             IntVarStrategy var_str,
                             IntValueStrategy val_str);

  DecisionBuilder* MakePhase(const vector<IntVar*>& vars,
                             IndexEvaluator1* var_evaluator,
                             IntValueStrategy val_str);
  DecisionBuilder* MakePhase(const IntVar* const* vars,
                             int size,
                             IndexEvaluator1* var_evaluator,
                             IntValueStrategy val_str);

  DecisionBuilder* MakePhase(const vector<IntVar*>& vars,
                             IntVarStrategy var_str,
                             IndexEvaluator2* val_eval);
  DecisionBuilder* MakePhase(const IntVar* const* vars,
                             int size,
                             IntVarStrategy var_str,
                             IndexEvaluator2* val_eval);

  DecisionBuilder* MakePhase(const vector<IntVar*>& vars,
                             IndexEvaluator1* var_evaluator,
                             IndexEvaluator2* val_eval);
  DecisionBuilder* MakePhase(const IntVar* const* vars,
                             int size,
                             IndexEvaluator1* var_evaluator,
                             IndexEvaluator2* val_eval);

  DecisionBuilder* MakePhase(const vector<IntVar*>& vars,
                             IntVarStrategy var_str,
                             IndexEvaluator2* val_eval,
                             IndexEvaluator1* tie_breaker);
  DecisionBuilder* MakePhase(const IntVar* const* vars,
                             int size,
                             IntVarStrategy var_str,
                             IndexEvaluator2* val_eval,
                             IndexEvaluator1* tie_breaker);

  DecisionBuilder* MakePhase(const vector<IntVar*>& vars,
                             IndexEvaluator1* var_evaluator,
                             IndexEvaluator2* val_eval,
                             IndexEvaluator1* tie_breaker);
  DecisionBuilder* MakePhase(const IntVar* const* vars,
                             int size,
                             IndexEvaluator1* var_evaluator,
                             IndexEvaluator2* val_eval,
                             IndexEvaluator1* tie_breaker);

  DecisionBuilder* MakeDefaultPhase(const IntVar* const* vars, int size);
  DecisionBuilder* MakeDefaultPhase(const vector<IntVar*>& vars);
  DecisionBuilder* MakeDefaultPhase(const IntVar* const* vars,
                                    int size,
                                    const DefaultPhaseParameters& parameters);
  DecisionBuilder* MakeDefaultPhase(const vector<IntVar*>& vars,
                                    const DefaultPhaseParameters& parameters);

  // shortcuts for small arrays.
  DecisionBuilder* MakePhase(IntVar* const v0,
                             IntVarStrategy var_str,
                             IntValueStrategy val_str);
  DecisionBuilder* MakePhase(IntVar* const v0,
                             IntVar* const v1,
                             IntVarStrategy var_str,
                             IntValueStrategy val_str);
  DecisionBuilder* MakePhase(IntVar* const v0,
                             IntVar* const v1,
                             IntVar* const v2,
                             IntVarStrategy var_str,
                             IntValueStrategy val_str);
  DecisionBuilder* MakePhase(IntVar* const v0,
                             IntVar* const v1,
                             IntVar* const v2,
                             IntVar* const v3,
                             IntVarStrategy var_str,
                             IntValueStrategy val_str);

  // ----- Scheduling Decisions -----

  // Returns a decision that tries to schedule a task at a given time.
  // On the Apply branch, it will set that interval var as performed and set
  // its start to 'est'. On the Refute branch, it will just update the
  // 'marker' to 'est' + 1. This decision is used in the
  // INTERVAL_SET_TIMES_FORWARD strategy.
  Decision* MakeScheduleOrPostpone(IntervalVar* const var,
                                   int64 est,
                                   int64* const marker);

  // Returns a decision that tries to rank first the ith interval var
  // in the sequence variable.
  Decision* MakeTryRankFirst(Sequence* const sequence, int index);

  // Returns a decision builder which assigns values to variables which
  // minimize the values returned by the evaluator. The arguments passed to the
  // evaluator callback are the indices of the variables in vars and the values
  // of these variables. Ownership of the callback is passed to the decision
  // builder.
  DecisionBuilder* MakePhase(const vector<IntVar*>& vars,
                             IndexEvaluator2* evaluator,
                             EvaluatorStrategy str);
  DecisionBuilder* MakePhase(const IntVar* const* vars,
                             int size,
                             IndexEvaluator2* evaluator,
                             EvaluatorStrategy str);

  // Returns a decision builder which assigns values to variables
  // which minimize the values returned by the evaluator. In case of
  // tie breaks, the second callback is used to choose the best index
  // in the array of equivalent pairs with equivalent evaluations. The
  // arguments passed to the evaluator callback are the indices of the
  // variables in vars and the values of these variables. Ownership of
  // the callback is passed to the decision builder.
  DecisionBuilder* MakePhase(const vector<IntVar*>& vars,
                             IndexEvaluator2* evaluator,
                             IndexEvaluator1* tie_breaker,
                             EvaluatorStrategy str);
  DecisionBuilder* MakePhase(const IntVar* const* vars,
                             int size,
                             IndexEvaluator2* evaluator,
                             IndexEvaluator1* tie_breaker,
                             EvaluatorStrategy str);

  // Scheduling phases.

  DecisionBuilder* MakePhase(const vector<IntervalVar*>& intervals,
                             IntervalStrategy str);

  DecisionBuilder* MakePhase(const vector<Sequence*>& sequences,
                             SequenceStrategy str);

  // Returns a decision builder for which the left-most leaf corresponds
  // to assignment, the rest of the tree being explored using 'db'.
  DecisionBuilder* MakeDecisionBuilderFromAssignment(
      Assignment* const assignment,
      DecisionBuilder* const db,
      const IntVar* const* vars,
      int size);

  // SolveOnce will collapse a search tree described by a 'db' decision
  // builder, and a set of monitors and wrap it into a single point.
  // If there are no solutions to this nested tree, then SolveOnce will
  // fail. If there is a solution, it will find it and returns NULL.
  DecisionBuilder* MakeSolveOnce(DecisionBuilder* const db);
  DecisionBuilder* MakeSolveOnce(DecisionBuilder* const db,
                                 SearchMonitor* const monitor1);
  DecisionBuilder* MakeSolveOnce(DecisionBuilder* const db,
                                 SearchMonitor* const monitor1,
                                 SearchMonitor* const monitor2);
  DecisionBuilder* MakeSolveOnce(DecisionBuilder* const db,
                                 SearchMonitor* const monitor1,
                                 SearchMonitor* const monitor2,
                                 SearchMonitor* const monitor3);
  DecisionBuilder* MakeSolveOnce(DecisionBuilder* const db,
                                 SearchMonitor* const monitor1,
                                 SearchMonitor* const monitor2,
                                 SearchMonitor* const monitor3,
                                 SearchMonitor* const monitor4);
  DecisionBuilder* MakeSolveOnce(DecisionBuilder* const db,
                                 const vector<SearchMonitor*>& monitors);

  // Returns a DecisionBuilder which restores an Assignment
  // (calls void Assignment::Restore())
  DecisionBuilder* MakeRestoreAssignment(Assignment* assignment);

  // Returns a DecisionBuilder which stores an Assignment
  // (calls void Assignment::Store())
  DecisionBuilder* MakeStoreAssignment(Assignment* assignment);

  // ----- Local Search Operators -----

  LocalSearchOperator* MakeOperator(const vector<IntVar*>& vars,
                                    LocalSearchOperators op);
  LocalSearchOperator* MakeOperator(const IntVar* const* vars,
                                    int size,
                                    LocalSearchOperators op);
  LocalSearchOperator* MakeOperator(const vector<IntVar*>& vars,
                                    const vector<IntVar*>& secondary_vars,
                                    LocalSearchOperators op);
  LocalSearchOperator* MakeOperator(const IntVar* const* vars,
                                    const IntVar* const* secondary_vars,
                                    int size,
                                    LocalSearchOperators op);
  // TODO(user): Make the callback a IndexEvaluator2 when there are no
  // secondary variables.
  LocalSearchOperator* MakeOperator(const vector<IntVar*>& vars,
                                    IndexEvaluator3* const evaluator,
                                    EvaluatorLocalSearchOperators op);
  LocalSearchOperator* MakeOperator(const IntVar* const* vars,
                                    int size,
                                    IndexEvaluator3* const evaluator,
                                    EvaluatorLocalSearchOperators op);
  LocalSearchOperator* MakeOperator(const vector<IntVar*>& vars,
                                    const vector<IntVar*>& secondary_vars,
                                    IndexEvaluator3* const evaluator,
                                    EvaluatorLocalSearchOperators op);
  LocalSearchOperator* MakeOperator(const IntVar* const* vars,
                                    const IntVar* const* secondary_vars,
                                    int size,
                                    IndexEvaluator3* const evaluator,
                                    EvaluatorLocalSearchOperators op);

  // Creates a large neighborhood search operator which creates fragments (set
  // of relaxed variables) with up to number_of_variables random variables
  // (sampling with replacement is performed meaning that at most
  // number_of_variables variables are selected). Warning: this operator will
  // always return neighbors; using it without a search limit will result in a
  // non-ending search.
  // Optionally a random seed can be specified.
  LocalSearchOperator* MakeRandomLNSOperator(const vector<IntVar*>& vars,
                                             int number_of_variables);
  LocalSearchOperator* MakeRandomLNSOperator(const vector<IntVar*>& vars,
                                             int number_of_variables,
                                             int32 seed);
  LocalSearchOperator* MakeRandomLNSOperator(const IntVar* const* vars,
                                             int size,
                                             int number_of_variables);
  LocalSearchOperator* MakeRandomLNSOperator(const IntVar* const* vars,
                                             int size,
                                             int number_of_variables,
                                             int32 seed);

  // Creates a local search operator which concatenates a vector of operators.
  // Each operator from the vector is called sequentially. By default, when a
  // neighbor is found the neighborhood exploration restarts from the last
  // active operator (the one which produced the neighbor).
  // This can be overriden by setting restart to true to force the exploration
  // to start from the first operator in the vector.
  // The default behavior can also be overriden using an evaluation callback to
  // set the order in which the operators are explored (the callback is called
  // in LocalSearchOperator::Start()). The first argument of the callback is
  // the index of the operator which produced the last move, the second
  // argument is the index of the operator to be evaluated.
  // Ownership of the callback is taken by ConcatenateOperators.
  //
  // Example:
  //
  //  const int kPriorities = {10, 100, 10, 0};
  //  int64 Evaluate(int active_operator, int current_operator) {
  //    return kPriorities[current_operator];
  //  }
  //
  //  LocalSearchOperator* concat =
  //    solver.ConcatenateOperators(operators,
  //                                NewPermanentCallback(&Evaluate));
  //
  // The elements of the vector operators will be sorted by increasing priority
  // and explored in that order (tie-breaks are handled by keeping the relative
  // operator order in the vector). This would result in the following order:
  // operators[3], operators[0], operators[2], operators[1].
  //
  LocalSearchOperator* ConcatenateOperators(
      const vector<LocalSearchOperator*>& ops);
  LocalSearchOperator* ConcatenateOperators(
      const vector<LocalSearchOperator*>& ops, bool restart);
  LocalSearchOperator* ConcatenateOperators(
      const vector<LocalSearchOperator*>& ops,
      ResultCallback2<int64, int, int>* const evaluator);
  // Randomized version of local search concatenator; calls a random operator at
  // each call to MakeNextNeighbor().
  LocalSearchOperator* RandomConcatenateOperators(
      const vector<LocalSearchOperator*>& ops);

  // Local Search decision builders factories.
  // Local search is used to improve a given solution. This initial solution
  // can be specified either by an Assignment or by a DecisionBulder, and the
  // corresponding variables, the initial solution being the first solution
  // found by the DecisionBuilder.
  // The LocalSearchPhaseParameters parameter holds the actual definition of
  // the local search phase:
  // - a local search operator used to explore the neighborhood of the current
  //   solution,
  // - a decision builder to instantiate unbound variables once a neighbor has
  //   been defined; in the case of LNS-based operators instantiates fragment
  //   variables; search monitors can be added to this sub-search by wrapping
  //   the decision builder with MakeSolveOnce.
  // - a search limit specifying how long local search looks for neighbors
  //   before accepting one; the last neighbor is always taken and in the case
  //   of a greedy search, this corresponds to the best local neighbor;
  //   first-accept (which is the default behavior) can be modeled using a
  //    solution found limit of 1,
  // - a vector of local search filters used to speed up the search by pruning
  //   unfeasible neighbors.
  // Metaheuristics can be added by defining specialized search monitors;
  // currently down/up-hill climbing is available through OptimizeVar, as well
  // as Guided Local Search, Tabu Search and Simulated Annealing.
  //
  // TODO(user): Make a variant which runs a local search after each
  //                solution found in a dfs

  DecisionBuilder* MakeLocalSearchPhase(
      Assignment* const assignment,
      LocalSearchPhaseParameters* const parameters);
  DecisionBuilder* MakeLocalSearchPhase(
      const vector<IntVar*>& vars,
      DecisionBuilder* const first_solution,
      LocalSearchPhaseParameters* const parameters);
  DecisionBuilder* MakeLocalSearchPhase(
      IntVar* const* vars, int size,
      DecisionBuilder* const first_solution,
      LocalSearchPhaseParameters* const parameters);


  // Solution Pool.
  SolutionPool* MakeDefaultSolutionPool();

  // Local Search Phase Parameters
  LocalSearchPhaseParameters* MakeLocalSearchPhaseParameters(
      LocalSearchOperator* const ls_operator,
      DecisionBuilder* const sub_decision_builder);
  LocalSearchPhaseParameters* MakeLocalSearchPhaseParameters(
      LocalSearchOperator* const ls_operator,
      DecisionBuilder* const sub_decision_builder,
      SearchLimit* const limit);
  LocalSearchPhaseParameters* MakeLocalSearchPhaseParameters(
      LocalSearchOperator* const ls_operator,
      DecisionBuilder* const sub_decision_builder,
      SearchLimit* const limit,
      const vector<LocalSearchFilter*>& filters);

  LocalSearchPhaseParameters* MakeLocalSearchPhaseParameters(
      SolutionPool* const pool,
      LocalSearchOperator* const ls_operator,
      DecisionBuilder* const sub_decision_builder);
  LocalSearchPhaseParameters* MakeLocalSearchPhaseParameters(
      SolutionPool* const pool,
      LocalSearchOperator* const ls_operator,
      DecisionBuilder* const sub_decision_builder,
      SearchLimit* const limit);
  LocalSearchPhaseParameters* MakeLocalSearchPhaseParameters(
      SolutionPool* const pool,
      LocalSearchOperator* const ls_operator,
      DecisionBuilder* const sub_decision_builder,
      SearchLimit* const limit,
      const vector<LocalSearchFilter*>& filters);

  // Local Search Filters
  LocalSearchFilter* MakeVariableDomainFilter();
  LocalSearchFilter* MakeLocalSearchObjectiveFilter(
      const IntVar* const* vars,
      int size,
      IndexEvaluator2* const values,
      const IntVar* const objective,
      Solver::LocalSearchFilterBound filter_enum,
      Solver::LocalSearchOperation op_enum);
  LocalSearchFilter* MakeLocalSearchObjectiveFilter(
      const vector<IntVar*>& vars,
      IndexEvaluator2* const values,
      const IntVar* const objective,
      Solver::LocalSearchFilterBound filter_enum,
      Solver::LocalSearchOperation op_enum);
  LocalSearchFilter* MakeLocalSearchObjectiveFilter(
      const IntVar* const* vars,
      const IntVar* const* secondary_vars,
      int size,
      ResultCallback3<int64, int64, int64, int64>* const values,
      const IntVar* const objective,
      Solver::LocalSearchFilterBound filter_enum,
      Solver::LocalSearchOperation op_enum);

  // Ensures communication of local optima between monitors and search
  bool LocalOptimum();
  // Checks with monitors if delta is acceptable
  bool AcceptDelta(Assignment* delta, Assignment* deltadelta);
  // Ensures communication of accepted neighbors between monitors and search
  void AcceptNeighbor();
  // Performs PeriodicCheck on the top-level search; can be called from a nested
  // solve to check top-level limits for instance.
  void TopPeriodicCheck();
  // The PushState and PopState methods manipulates the states
  // of the reversible objects. They are visible only because they
  // are useful to write unitary tests.
  void PushState();
  void PopState();

  // Gets the search depth of the current active search. Returns -1 in
  // case there are no active search opened.
  int SearchDepth() const;

  // Gets the search left depth of the current active search. Returns -1 in
  // case there are no active search opened.
  int SearchLeftDepth() const;

  // Gets the number of nested searches. It returns 0 outside search,
  // 1 during the top level search, 2 if one level of NestedSolve() is
  // used, and more if more solves are nested.
  int SolveDepth() const;

  // Sets the given branch selector on the current active search.
  void SetBranchSelector(
      ResultCallback1<Solver::DecisionModification, Solver*>* const bs);

  // Creates a decision builder that will set the branch selector.
  DecisionBuilder* MakeApplyBranchSelector(
      ResultCallback1<Solver::DecisionModification, Solver*>* const bs);

  // All-in-one SaveAndSetValue.
  template <class T> void SaveAndSetValue(T* adr, T val) {
    if (*adr != val) {
      InternalSaveValue(adr);
      *adr = val;
    }
  }

  // All-in-one SaveAndAdd_value.
  template <class T> void SaveAndAdd(T* adr, T val) {
    InternalSaveValue(adr);
    (*adr) += val;
  }

  // Returns a random value between 0 and 'size' - 1;
  int64 Rand64(int64 size) {
    return random_.Next64() % size;
  }

  // Returns a random value between 0 and 'size' - 1;
  int32 Rand32(int32 size) {
    return random_.Next() % size;
  }

  // Reseed the solver random generator.
  void ReSeed(int32 seed) {
    random_.Reset(seed);
  }

  // Add a fail hook, that is an action that will be called after each failure.
  void AddFailHook(Action* a);

  // This method exports the profiling information in a human readable overview.
  // The parameter profile_level used to create the solver must be
  // different from NO_PROFILING.
  void ExportProfilingOverview(const string& filename);

  // This functions returns true wether the current search has been
  // created using a Solve() call instead of a NewSearch 0ne. It
  // returns false if the solver is not is search at all.
  bool CurrentlyInSolve() const;

  // This method counts the number of constraints that have been added
  // to the solver before the search,
  int constraints() const { return constraints_; }

  Decision* balancing_decision() const { return balancing_decision_.get(); }

  // Internal
  // set_fail_intercept does not take ownership of the closure.
  void set_fail_intercept(Closure* const c) { fail_intercept_ = c; }
  void clear_fail_intercept() { fail_intercept_ = NULL; }
  // Access to demon monitor.
  DemonMonitor* demon_monitor() const { return demon_monitor_; }

  friend class BaseIntExpr;
  friend class BooleanVar;
  friend class DemonProfiler;
  friend class DomainIntVar;
  friend class FindOneNeighbor;
  friend class FixedDurationIntervalVar;
  friend class PropagationBaseObject;
  friend class Queue;
  friend class SearchMonitor;
  friend class UndoBranchSelector;
  friend class VarCstCache;
#ifndef SWIG
  template<class> friend class SimpleRevFIFO;
#endif

 private:
  void NotifyFailureToDemonMonitor();
  void PushState(MarkerType t, const StateInfo& info);
  MarkerType PopState(StateInfo* info);
  void PushSentinel(int magic_code);
  void BacktrackToSentinel(int magic_code);
  void CallFailHooks();
  void ProcessConstraints();
  bool BacktrackOneLevel(Decision** fd);
  void JumpToSentinelWhenNested();
  void JumpToSentinel();
  void check_alloc_state();
  void FreezeQueue();
  void Enqueue(Demon* d);
  void ProcessDemonsOnQueue();
  void UnfreezeQueue();
  void set_queue_action_on_fail(Action* a);
  void set_queue_cleaner_on_fail(DomainIntVar* const var);
  void clear_queue_action_on_fail();

  void InternalSaveValue(int* valptr);
  void InternalSaveValue(int64* valptr);
  void InternalSaveValue(uint64* valptr);
  void InternalSaveValue(bool* valptr);
  void InternalSaveValue(void** valptr);
  void InternalSaveValue(int64** valptr) {
    InternalSaveValue(reinterpret_cast<void**>(valptr));
  }
  void InternalSaveBooleanVarValue(BooleanVar* const var);

  // Adds a new demon and wraps it inside a DemonProfiler if necessary.
  Demon* RegisterDemon(Demon* const d);

  int* SafeRevAlloc(int* ptr);
  int64* SafeRevAlloc(int64* ptr);
  uint64* SafeRevAlloc(uint64* ptr);
  BaseObject* SafeRevAlloc(BaseObject* ptr);
  BaseObject** SafeRevAlloc(BaseObject** ptr);
  IntVar** SafeRevAlloc(IntVar** ptr);
  IntExpr** SafeRevAlloc(IntExpr** ptr);
  Constraint** SafeRevAlloc(Constraint** ptr);
  // UnsafeRevAlloc is used internally for cells in SimpleRevFIFO
  // and other structures like this.
  void* UnsafeRevAllocAux(void* ptr);
  template <class T> T* UnsafeRevAlloc(T* ptr) {
    return reinterpret_cast<T*>(
        UnsafeRevAllocAux(reinterpret_cast<void*>(ptr)));
  }
  void** UnsafeRevAllocArrayAux(void** ptr);
  template <class T> T** UnsafeRevAllocArray(T** ptr) {
    return reinterpret_cast<T**>(
        UnsafeRevAllocArrayAux(reinterpret_cast<void**>(ptr)));
  }

  void InitCachedIntConstants();
  void InitCachedConstraint();
  void InitBoolVarCaches();

  // Naming
  string GetName(const PropagationBaseObject* object) const;
  void SetName(const PropagationBaseObject* object, const string& name);

  const string name_;
  const SolverParameters parameters_;
  hash_map<const PropagationBaseObject*, string> propagation_object_names_;
  hash_map<const PropagationBaseObject*,
           pair<string, const PropagationBaseObject*> > delegate_objects_;
  const string empty_name_;
  scoped_ptr<Queue> queue_;
  scoped_ptr<Trail> trail_;
  vector<Constraint*> constraints_list_;
  SolverState state_;
  int64 branches_;
  int64 fails_;
  int64 decisions_;
  int64 demon_runs_[kNumPriorities];
  int64 neighbors_;
  int64 filtered_neighbors_;
  int64 accepted_neighbors_;
  scoped_ptr<VariableQueueCleaner> variable_cleaner_;
  scoped_ptr<ClockTimer> timer_;
  vector<Search*> searches_;
  ACMRandom random_;
  SimpleRevFIFO<Action*>* fail_hooks_;
  uint64 fail_stamp_;
  scoped_ptr<Decision> balancing_decision_;
  // intercept failures
  Closure* fail_intercept_;
  // Demon monitor
  DemonMonitor* const demon_monitor_;

  // interval of constants cached, inclusive:
  enum { MIN_CACHED_INT_CONST = -8, MAX_CACHED_INT_CONST = 8 };
  IntVar* cached_constants_[MAX_CACHED_INT_CONST + 1 - MIN_CACHED_INT_CONST];

  // Cached constraints.
  Constraint* true_constraint_;
  Constraint* false_constraint_;

  // status var caches:
  EqualityVarCstCache* equality_var_cst_cache_;
  UnequalityVarCstCache* unequality_var_cst_cache_;
  GreaterEqualCstCache* greater_equal_var_cst_cache_;
  LessEqualCstCache* less_equal_var_cst_cache_;
  scoped_ptr<Decision> fail_decision_;
  int constraints_;

  bool solution_found_;

  DISALLOW_COPY_AND_ASSIGN(Solver);
};

std::ostream& operator << (std::ostream& out, const Solver* const s);  // NOLINT

/////////////////////////////////////////////////////////////////////
//
// Useful Search and Modeling Objects
//
/////////////////////////////////////////////////////////////////////

// A BaseObject is the root of all reversibly allocated objects.
// A DebugString method and the associated << operator are implemented
// as a convenience.

class BaseObject {
 public:
  BaseObject() {}
  virtual ~BaseObject() {}
  virtual string DebugString() const { return "BaseObject"; }
 private:
  DISALLOW_COPY_AND_ASSIGN(BaseObject);
};

std::ostream& operator <<(std::ostream& out, const BaseObject* o);  // NOLINT

// The PropagationBaseObject is a subclass of BaseObject that is also
// friend to the Solver class. It allows accessing methods useful when
// writing new constraints or new expressions.
class PropagationBaseObject : public BaseObject {
 public:
  explicit PropagationBaseObject(Solver* const s) : solver_(s) {}
  virtual ~PropagationBaseObject() {}

  virtual string DebugString() const {
    if (name().empty()) {
      return "PropagationBaseObject";
    } else {
      return StringPrintf("PropagationBaseObject: %s", name().c_str());
    }
  }
  Solver* solver() const { return solver_; }
  // This method freezes the propagation queue. It is useful when you
  // need to apply multiple modifications at once.
  void FreezeQueue() { solver_->FreezeQueue(); }

  // This method unfreezes the propagation queue. All modifications
  // that happened when the queue was frozen will be processed.
  void UnfreezeQueue() { solver_->UnfreezeQueue(); }

  // This method pushes the demon onto the propagation queue. It will
  // be processed directly if the queue is empty. It will be enqueued
  // according to its priority otherwise.
  void Enqueue(Demon* d) { solver_->Enqueue(d); }

  // This methods process all demons with priority non IMMEDIATE on
  // the queue.
  void ProcessDemonsOnQueue() { solver_->ProcessDemonsOnQueue(); }

  // This method sets a callback that will be called if a failure
  // happens during the propagation of the queue.
  void set_queue_action_on_fail(Action* a) {
    solver_->set_queue_action_on_fail(a);
  }

  // This methods clears the failure callback.
  void clear_queue_action_on_fail() {
    solver_->clear_queue_action_on_fail();
  }

  // Naming
  virtual string name() const;
  void set_name(const string& name);

 private:
  Solver* const solver_;
  DISALLOW_COPY_AND_ASSIGN(PropagationBaseObject);
};

// A Decision represents a choice point in the search tree. The two main
// methods are Apply() to go left, or Refute() to go right.
class Decision : public BaseObject {
 public:
  Decision() {}
  virtual ~Decision() {}

  // Apply will be called first when the decision is executed.
  virtual void Apply(Solver* const s) = 0;

  // Refute will be called after a backtrack.
  virtual void Refute(Solver* const s) = 0;

  virtual string DebugString() const {
    return "Decision";
  }
  // Visits the decision.
  virtual void Accept(DecisionVisitor* const visitor) const;
 private:
  DISALLOW_COPY_AND_ASSIGN(Decision);
};

// A DecisionVisitor is used to inspect a decision.
// It contains virtual methods for all type of 'declared' decisions.
class DecisionVisitor : public BaseObject {
 public:
  DecisionVisitor() {}
  virtual ~DecisionVisitor() {}
  virtual void VisitSetVariableValue(IntVar* const var, int64 value);
  virtual void VisitSplitVariableDomain(IntVar* const var,
                                        int64 value,
                                        bool start_with_lower_half);
  virtual void VisitScheduleOrPostpone(IntervalVar* const var, int64 est);
  virtual void VisitTryRankFirst(Sequence* const sequence, int index);
  virtual void VisitUnknownDecision();
 private:
  DISALLOW_COPY_AND_ASSIGN(DecisionVisitor);
};

// A DecisionBuilder is responsible for creating the search tree. The
// important method is Next() that returns the next decision to execute.
class DecisionBuilder : public BaseObject {
 public:
  DecisionBuilder() {}
  virtual ~DecisionBuilder() {}
  // This is the main method of the decision builder class. It must
  // return a decision (an instance of the class Decision). If it
  // returns NULL, this means that the decision builder has finished
  // its work.
  virtual Decision* Next(Solver* const s) = 0;
  virtual string DebugString() const { return "DecisionBuilder"; }
 private:
  DISALLOW_COPY_AND_ASSIGN(DecisionBuilder);
};

// A Demon is the base element of a propagation queue. It is the main
//   object responsible for implementing the actual propagation
//   of the constraint and pruning the inconsistent values in the domains
//   of the variables. The main concept is that demons are listeners that are
//   attached to the variables and listen to their modifications.
// There are two methods:
//  - Run() is the actual methods that is called when the demon is processed
//  - priority() returns its priority. Standart priorities are slow, normal
//    or fast. immediate is reserved for variables and are treated separately.
class Demon : public BaseObject {
 public:
  // This indicates the priority of a demon. Immediate demons are treated
  // separately and corresponds to variables.
  Demon() : stamp_(GG_ULONGLONG(0)) {}
  virtual ~Demon() {}

  // This is the main callback of the demon.
  virtual void Run(Solver* const s) = 0;

  // This method returns the priority of the demon. Usually a demon is
  // fast, slow or normal. Immediate demons are reserved for internal
  // use to maintain variables.
  virtual Solver::DemonPriority priority() const;

  virtual string DebugString() const;

  // This method inhibits the demon in the search tree below the
  // current position.
  void inhibit(Solver* const s);

  // This method un-inhibit the demon that was inhibited.
  void desinhibit(Solver* const s);
 private:
  friend class Queue;
  void set_stamp(int64 stamp) { stamp_ = stamp; }
  uint64 stamp() const { return stamp_; }
  uint64 stamp_;
  DISALLOW_COPY_AND_ASSIGN(Demon);
};

// An action is the base callback method. It is separated from the standard
// google callback class because of its specific memory management.
class Action : public BaseObject {
 public:
  Action() {}
  virtual ~Action() {}

  // The main callback of the class.
  virtual void Run(Solver* const s) = 0;
  virtual string DebugString() const;
 private:
  DISALLOW_COPY_AND_ASSIGN(Action);
};

// Variable-based queue cleaner
class VariableQueueCleaner : public Action {
 public:
  VariableQueueCleaner() : var_(NULL) {}
  virtual ~VariableQueueCleaner() {}
  virtual void Run(Solver* const solver);
  void set_var(DomainIntVar* const var) { var_ = var; }
 private:
  DomainIntVar* var_;
};

// A constraint is the main modeling object. It proposes two methods:
//   - Post() is responsible for creating the demons and attaching them to
//     immediate demons()
//   - InitialPropagate() is called once just after the Post and performs
//     the initial propagation. The subsequent propagations will be performed
//     by the demons Posted during the post() method.
class Constraint : public PropagationBaseObject {
 public:
  explicit Constraint(Solver* const solver) : PropagationBaseObject(solver) {}
  virtual ~Constraint() {}

  // This method is called when the constraint is processed by the
  // solver. Its main usage is to attach demons to variables.
  virtual void Post() = 0;

  // This method performs the initial propagation of the
  // constraint. It is called just after the post.
  virtual void InitialPropagate() = 0;
  virtual string DebugString() const;

  void PostAndPropagate();
 private:
  DISALLOW_COPY_AND_ASSIGN(Constraint);
};

// A search monitor is a simple set of callbacks to monitor all search events
class SearchMonitor : public BaseObject {
 public:
  explicit SearchMonitor(Solver* const s) : solver_(s) {}
  virtual ~SearchMonitor() {}
  // Beginning of the search.
  virtual void EnterSearch();

  // Restart the search.
  virtual void RestartSearch();

  // End of the search.
  virtual void ExitSearch();

  // Before calling DecisionBuilder::Next
  virtual void BeginNextDecision(DecisionBuilder* const b);

  // After calling DecisionBuilder::Next, along with the returned decision.
  virtual void EndNextDecision(DecisionBuilder* const b, Decision* const d);

  // Before applying the decision
  virtual void ApplyDecision(Decision* const d);

  // Before refuting the Decision
  virtual void RefuteDecision(Decision* const d);

  // Just when the failure occurs.
  virtual void BeginFail();

  // After completing the backtrack.
  virtual void EndFail();

  // Before the initial propagation.
  virtual void BeginInitialPropagation();

  // After the initial propagation.
  virtual void EndInitialPropagation();

  // This method is called when a solution is found. It asserts of the
  // solution is valid. A value of false indicate that the solution
  // should be discarded.
  virtual bool AcceptSolution();

  // This method is called when a valid solution is found. If the
  // return value is true, then search will resume after. If the result
  // is false, then search will stop there..
  virtual bool AtSolution();

  // When the search tree is finished.
  virtual void NoMoreSolutions();

  // When a local optimum is reached. If 'true' is returned, the last solution
  // is discarded and the search proceeds with the next one.
  virtual bool LocalOptimum();

  //
  virtual bool AcceptDelta(Assignment* delta, Assignment* deltadelta);

  // After accepting a neighbor during local search.
  virtual void AcceptNeighbor();

  Solver* solver() const { return solver_; }

  // Tells the solver to kill the current search.
  void FinishCurrentSearch();

  // Tells the solver to restart the current search.
  void RestartCurrentSearch();

  // Periodic call to check limits in long running methods.
  virtual void PeriodicCheck();
 private:
  Solver* const solver_;
  DISALLOW_COPY_AND_ASSIGN(SearchMonitor);
};

// These class represent reversible POD types.
// It Contains the stamp optimization. i.e. the SaveValue call is done only
// once per node of the search tree.
// Please note that actual stamps always starts at 1, thus an initial value of
// 0 will always trigger the first SaveValue.
template <class T> class Rev {
 public:
  explicit Rev(const T& val) : stamp_(GG_ULONGLONG(0)), value_(val) {}

  const T& Value() const { return value_; }

  void SetValue(Solver* const s, const T& val) {
    if (val != value_) {
      if (stamp_ < s->stamp()) {
        s->SaveValue(&value_);
        stamp_ = s->stamp();
      }
      value_ = val;
    }
  }
 private:
  uint64 stamp_;
  T value_;
};

// The class IntExpr is the base of all integer expressions in
// constraint programming.
// It Contains the basic protocol for an expression:
//   - setting and modifying its bound
//   - querying if it is bound
//   - listening to events modifying its bounds
//   - casting it into a variable (instance of IntVar)
class IntExpr : public PropagationBaseObject {
 public:
  explicit IntExpr(Solver* const s) : PropagationBaseObject(s) {}
  virtual ~IntExpr() {}

  virtual int64 Min() const = 0;
  virtual void SetMin(int64 m) = 0;
  virtual int64 Max() const = 0;
  virtual void SetMax(int64 m) = 0;

  // By default calls Min() and Max(), but can be redefined when Min and Max
  // code can be factorized.
  virtual void Range(int64* l, int64* u) {
    *l = Min();
    *u = Max();
  }
  // This method sets both the min and the max of the expression.
  virtual void SetRange(int64 l, int64 u) {
    SetMin(l);
    SetMax(u);
  }

  // This method sets the value of the expression.
  virtual void SetValue(int64 v) { SetRange(v, v); }

  // Returns true if the min and the max of the expression are equal.
  virtual bool Bound() const { return (Min() == Max()); }

  // Returns true if the expression is indeed a variable.
  virtual bool IsVar() const { return false; }

  // Creates a variable from the expression.
  virtual IntVar* Var() = 0;

  // Attach a demon that will watch the min or the max of the expression.
  virtual void WhenRange(Demon* d) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(IntExpr);
};

// The class Iterator has two direct subclasses. HoleIterators
// iterates over all holes, that is value removed between the
// current min and max of the variable since the last time the
// variable was processed in the queue. DomainIterators iterates
// over all elements of the variable domain. Both iterators are not
// robust to domain changes. Hole iterators can also report values outside
// the current min and max of the variable.

// HoleIterators should only be called from a demon attached to the
// variable that has created this iterator.

// IntVar* current_var;
// scoped_ptr<IntVarIterator> it(current_var->MakeHoleIterator(false));
// for (it->Init(); it->Ok(); it->Next()) {
//   const int64 hole = it->Value();
//   // use the hole
// }

class IntVarIterator : public BaseObject {
 public:
  virtual ~IntVarIterator() {}

  // This method must be called before each loop.
  virtual void Init() = 0;

  // This method indicates if we can call Value() or not.
  virtual bool Ok() const = 0;

  // This method returns the value of the hole.
  virtual int64 Value() const = 0;

  // This method moves the iterator to the next value.
  virtual void Next() = 0;

  // Pretty Print.
  virtual string DebugString() const {
    return "IntVar::Iterator";
  }
};

// The class IntVar is a subset of IntExpr. In addition to the
// IntExpr protocol, it offers persistance,
// removing values from the domains and a finer model for events
class IntVar : public IntExpr {
 public:
  explicit IntVar(Solver* const s) : IntExpr(s) {}
  IntVar(Solver* const s, const string& name) : IntExpr(s) { set_name(name); }
  virtual ~IntVar() {}

  virtual bool IsVar() const { return true; }
  virtual IntVar* Var() { return this; }

  // This method returns the value of the variable. This method checks
  // before that the variable is bound.
  virtual int64 Value() const = 0;

  // This method removes the value 'v' from the domain of the variable.
  virtual void RemoveValue(int64 v) = 0;

  // This method removes the interval 'l' .. 'u' from the domain of
  // the variable. It assumes that 'l' <= 'u'.
  virtual void RemoveInterval(int64 l, int64 u) = 0;

  // This method remove the values from the domain of the variable.
  virtual void RemoveValues(const int64* const values, int size);

  // This method remove the values from the domain of the variable.
  void RemoveValues(const vector<int64>& values) {
    RemoveValues(values.data(), values.size());
  }

  // This method intersects the current domain with the values in the array.
  virtual void SetValues(const int64* const values, int size);

  // This method intersects the current domain with the values in the array.
  void SetValues(const vector<int64>& values) {
    SetValues(values.data(), values.size());
  }

  // This method attaches a demon that will be awakened when the
  // variable is bound.
  virtual void WhenBound(Demon* d) = 0;

  // This method attaches a demon that will watch any domain
  // modification of the domain of the variable.
  virtual void WhenDomain(Demon* d) = 0;

  // This method returns the number of values in the domain of the variable.
  virtual uint64 Size() const = 0;

  // This method returns wether the value 'v' is in the domain of the variable.
  virtual bool Contains(int64 v) const = 0;

  // Creates a hole iterator. The object is created on the normal C++
  // heap and the solver does NOT take ownership of the object.
  virtual IntVarIterator* MakeHoleIterator(bool reversible) const = 0;

  // Creates a domain iterator. The object is created on the normal C++
  // heap and the solver does NOT take ownership of the object.
  virtual IntVarIterator* MakeDomainIterator(bool reversible) const = 0;

  // Returns the previous min.
  virtual int64 OldMin() const = 0;

  // Returns the previous max.
  virtual int64 OldMax() const = 0;

  virtual int VarType() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(IntVar);
};

// ---------- Solution Collectors ----------

// This class is the root class of all solution collectors
// It implements a basic query API to be used independently
// from the collector used.
class SolutionCollector : public SearchMonitor {
 public:
  SolutionCollector(Solver* const s, const Assignment* assignment);
  explicit SolutionCollector(Solver* const s);
  virtual ~SolutionCollector();

  // Add API
  void Add(IntVar* const var);
  void Add(const vector<IntVar*>& vars);
  void Add(IntervalVar* const var);
  void Add(const vector<IntervalVar*>& vars);
  void AddObjective(IntVar* const objective);

  // Beginning of the search.
  virtual void EnterSearch();

  // Returns how many solutions were stored during the search.
  int solution_count() const;

  // Returns the nth solution.
  Assignment* solution(int n) const;

  // Returns the wall time in ms for the nth solution.
  int64 wall_time(int n) const;

  // Returns the number of branches when the nth solution was found.
  int64 branches(int n) const;

  // Returns the number of failures encountered at the time of the nth
  // solution.
  int64 failures(int n) const;

  // Returns the objective value of the nth solution.
  int64 objective_value(int n) const;

  // This is a short-cut to get the Value of 'var' in the nth solution.
  int64 Value(int n, IntVar* const var) const;

  // This is a short-cut to get the StartValue of 'var' in the nth solution.
  int64 StartValue(int n, IntervalVar* const var) const;

  // This is a short-cut to get the EndValue of 'var' in the nth solution.
  int64 EndValue(int n, IntervalVar* const var) const;

  // This is a short-cut to get the DurationValue of 'var' in the nth solution.
  int64 DurationValue(int n, IntervalVar* const var) const;

  // This is a short-cut to get the PerformedValue of 'var' in the nth solution.
  int64 PerformedValue(int n, IntervalVar* const var) const;

 protected:
  // Push the current state as a new solution.
  void PushSolution();
  // Remove and delete the last popped solution.
  void PopSolution();

  void check_index(int n) const;
  scoped_ptr<Assignment> prototype_;
  vector<Assignment*> solutions_;
  vector<Assignment*> recycle_solutions_;
  vector<int64> times_;
  vector<int64> branches_;
  vector<int64> failures_;
  vector<int64> objective_values_;
  DISALLOW_COPY_AND_ASSIGN(SolutionCollector);
};

// TODO(user): Refactor this into an Objective class:
//   - print methods for AtNode and AtSolution.
//   - support for weighted objective and lexicographical objective.

// ---------- Objective Management ----------

// This class encapsulate an objective. It requires the direction
// (minimize or maximize), the variable to optimize and the
// improvement step.
class OptimizeVar : public SearchMonitor {
 public:
  OptimizeVar(Solver* const s, bool maximize, IntVar* const a, int64 step);
  virtual ~OptimizeVar();

  // Returns the best value found during search.
  int64 best() const { return best_; }

  // Returns the variable that is optimized.
  IntVar* Var() const { return var_; }
  // Internal methods
  virtual void EnterSearch();
  virtual void RestartSearch();
  virtual void RefuteDecision(Decision* d);
  virtual bool AtSolution();
  virtual bool AcceptSolution();
  virtual string Print() const;
  virtual string DebugString() const;

  void ApplyBound();
 private:
  IntVar* const var_;
  int64 step_;
  int64 best_;
  bool maximize_;
  DISALLOW_COPY_AND_ASSIGN(OptimizeVar);
};

// ---------- Search Limits ----------

// base class of all search limits
class SearchLimit : public SearchMonitor {
 public:
  explicit SearchLimit(Solver* const s) : SearchMonitor(s), crossed_(false) { }
  virtual ~SearchLimit();

  // Returns true if the limit has been crossed.
  bool crossed() const { return crossed_; }

  // This method is called to check the status of the limit. A return
  // value of true indicates that we have indeed crossed the limit. In
  // that case, this method will not be called again and the remaining
  // search will be discarded.
  virtual bool Check() = 0;

  // This method is called when the search limit is initialized.
  virtual void Init() = 0;

  // Copy a limit. Warning: leads to a direct (no check) downcasting of 'limit'
  // so one needs to be sure both SearchLimits are of the same type.
  virtual void Copy(const SearchLimit* const limit) = 0;

  // Allocates a clone of the limit
  virtual SearchLimit* MakeClone() const = 0;

  // Internal methods
  virtual void EnterSearch();
  virtual void BeginNextDecision(DecisionBuilder* const b);
  virtual void PeriodicCheck();
  virtual void RefuteDecision(Decision* const d);
  virtual string DebugString() const {
    return StringPrintf("SearchLimit(crossed = %i)", crossed_);
  }
 private:
  bool crossed_;
  DISALLOW_COPY_AND_ASSIGN(SearchLimit);
};

// ---------- Interval Var ----------

// An interval var is often used in scheduling. Its main
// characteristics are its start position, its duration and its end
// date. All these characteristics can be queried, set and demons can
// be posted on their modifications.  An important aspect is
// optionality. An interval var can be performed or not. If
// unperformed, then it simply does not exist. Its characteristics
// cannot be accessed anymore. An interval var is automatically marked
// as unperformed when it is not consistent anymore (start greater
// than end, duration < 0...)
class IntervalVar : public PropagationBaseObject {
 public:
  // The smallest acceptable value to be returned by StartMin()
  static const int64 kMinValidValue;
  // The largest acceptable value to be returned by EndMax()
  static const int64 kMaxValidValue;
  IntervalVar(Solver* const solver, const string& name)
      : PropagationBaseObject(solver),
        start_expr_(NULL), duration_expr_(NULL), end_expr_(NULL),
        performed_expr_(NULL) {
    set_name(name);
  }
  virtual ~IntervalVar() {}

  // These methods query, set and watch the start position of the
  // interval var.
  virtual int64 StartMin() const = 0;
  virtual int64 StartMax() const = 0;
  virtual void SetStartMin(int64 m) = 0;
  virtual void SetStartMax(int64 m) = 0;
  virtual void SetStartRange(int64 mi, int64 ma) = 0;
  virtual void WhenStartRange(Demon* const d) = 0;
  virtual void WhenStartBound(Demon* const d) = 0;

  // These methods query, set and watch the duration of the interval var.
  virtual int64 DurationMin() const = 0;
  virtual int64 DurationMax() const = 0;
  virtual void SetDurationMin(int64 m) = 0;
  virtual void SetDurationMax(int64 m) = 0;
  virtual void SetDurationRange(int64 mi, int64 ma) = 0;
  virtual void WhenDurationRange(Demon* const d) = 0;
  virtual void WhenDurationBound(Demon* const d) = 0;

  // These methods query, set and watch the end position of the interval var.
  virtual int64 EndMin() const = 0;
  virtual int64 EndMax() const = 0;
  virtual void SetEndMin(int64 m) = 0;
  virtual void SetEndMax(int64 m) = 0;
  virtual void SetEndRange(int64 mi, int64 ma) = 0;
  virtual void WhenEndRange(Demon* const d) = 0;
  virtual void WhenEndBound(Demon* const d) = 0;

  // These methods query, set and watches the performed status of the
  // interval var.
  virtual bool MustBePerformed() const = 0;
  virtual bool MayBePerformed() const = 0;
  bool CannotBePerformed() const { return !MayBePerformed(); }
  bool IsPerformedBound() {
    return MustBePerformed() == MayBePerformed();
  }
  virtual void SetPerformed(bool val) = 0;
  virtual void WhenPerformedBound(Demon* const d) = 0;

  // Attaches a demon awakened when anything about this interval changes.
  void WhenAnything(Demon* const d);

  // These methods create expressions encapsulating the start, end
  // and duration of the interval var. Please note that these must not
  // be used if the interval var is unperformed.
  IntExpr* StartExpr();
  IntExpr* DurationExpr();
  IntExpr* EndExpr();
  IntExpr* PerformedExpr();

 private:
  IntExpr* start_expr_;
  IntExpr* duration_expr_;
  IntExpr* end_expr_;
  IntExpr* performed_expr_;
  DISALLOW_COPY_AND_ASSIGN(IntervalVar);
};

// ----- Sequence -----

// A sequence is groups an array of interval vars and force them into
// a sequence. It exports statistics about its interval states such
// that it can be used in a decision builder. The most important ones
// are PossibleFirst() which tells if an interval var can be ranked
// first and RankFirst/RankNotFirst which can be used to create the
// search decision.
class Sequence : public Constraint {
 public:

  enum State { ONE_BEFORE_TWO, TWO_BEFORE_ONE, UNDECIDED };

  Sequence(Solver* const s,
           const IntervalVar* const * intervals,
           int size,
           const string& name);
  virtual ~Sequence();

  virtual string DebugString() const;

  // Constraint protocol: post demons.
  virtual void Post();

  // Constraint protocol: Initial propagation
  virtual void InitialPropagate();

  // Returns the minimum and maximum duration of combined interval
  // vars in the sequence.
  void DurationRange(int64* dmin, int64* dmax) const;

  // Returns the minimum start min and the maximum end max of all
  // interval vars in the sequence.
  void HorizonRange(int64* hmin, int64* hmax) const;

  // Returns the minimum start min and the maximum end max of all
  // unranked interval vars in the sequence.
  void ActiveHorizonRange(int64* hmin, int64* hmax) const;

  // Returns the number of interval vars already ranked.
  int Ranked() const;

  // Returns the number of interval vars not yet ranked.
  int NotRanked() const;

  // Returns the number of interval vars not unperformed.
  int Active() const;

  // Returns the number of interval vars fixed and performed.
  int Fixed() const;

  // TODO(user) : hide ComputePossibleRanks() method.
  void ComputePossibleRanks();

  // Returns whether or not the index_th interval var in the sequence
  // can be ranked first of all unranked interval vars.
  bool PossibleFirst(int index);

  // Rank the index_th interval var first of all unranked interval
  // vars. After that, it will no longer be considered ranked.
  void RankFirst(int index);

  // Indicates that the index_th interval var will not be ranked first
  // of all currently unranked interval vars.
  void RankNotFirst(int index);

  // Returns the index_th interval of the sequence.
  IntervalVar* Interval(int index) const;

  // Returns the number of interval vars in the sequence.
  int size() const { return size_; }

  // Internal method called by demons
  void RangeChanged(int index);

 private:
  void TryToDecide(int i, int j);
  void Decide(State s, int i, int j);
  void Apply(int i, int j);

  scoped_array<IntervalVar*> intervals_;
  const int size_;
  scoped_array<int> ranks_;
  int current_rank_;
  vector<vector<State> > states_;
};

// --------- Assignments ----------------------------

// ---------- Assignment Elements ----------

// ----- AssignmentElement -----

class AssignmentElement {
 public:
  AssignmentElement() : activated_(true) {}

  void Activate() { activated_ = true; }
  void Deactivate() { activated_ = false; }
  bool Activated() const { return activated_; }
 private:
  bool activated_;
};

// ----- IntVarElement -----

class IntVarElement : public AssignmentElement {
 public:
  IntVarElement();
  explicit IntVarElement(IntVar* const var);
  void Reset(IntVar* const var);
  IntVarElement* Clone();
  void Copy(const IntVarElement& element);
  IntVar* Var() const { return var_; }
  void Store() {
    min_ = var_->Min();
    max_ = var_->Max();
  }
  void Restore() { var_->SetRange(min_, max_); }
  void StoreFromProto(const IntVarAssignmentProto& int_var_assignment_proto);
  void RestoreToProto(IntVarAssignmentProto* int_var_assignment_proto) const;

  int64 Min() const { return min_; }
  void SetMin(int64 m) { min_ = m; }
  int64 Max() const { return max_; }
  void SetMax(int64 m) { max_ = m; }
  int64 Value() const {
    DCHECK(min_ == max_);
    // Getting the value from an unbound int var assignment element.
    return min_;
  }
  bool Bound() const { return (max_ == min_); }
  void SetRange(int64 l, int64 u) {
    min_ = l;
    max_ = u;
  }
  void SetValue(int64 v) {
    min_ = v;
    max_ = v;
  }
  string DebugString() const;
 public:
  IntVar* var_;
  int64 min_;
  int64 max_;
};

// ----- IntervalVarElement -----

class IntervalVarElement : public AssignmentElement {
 public:
  IntervalVarElement();
  explicit IntervalVarElement(IntervalVar* const var);
  void Reset(IntervalVar* const var);
  IntervalVarElement* Clone();
  void Copy(const IntervalVarElement& element);
  IntervalVar* Var() const { return var_; }
  void Store();
  void Restore();
  void StoreFromProto(
      const IntervalVarAssignmentProto& interval_var_assignment_proto);
  void RestoreToProto(
      IntervalVarAssignmentProto* interval_var_assignment_proto) const;

  int64 StartMin() const { return start_min_; }
  int64 StartMax() const { return start_max_; }
  int64 StartValue() const {
    CHECK_EQ(start_max_, start_min_);
    return start_max_;
  }
  int64 DurationMin() const { return duration_min_; }
  int64 DurationMax() const { return duration_max_; }
  int64 DurationValue() const {
    CHECK_EQ(duration_max_, duration_min_);
    return duration_max_;
  }
  int64 EndMin() const { return end_min_; }
  int64 EndMax() const { return end_max_; }
  int64 EndValue() const {
    CHECK_EQ(end_max_, end_min_);
    return end_max_;
  }
  int64 PerformedMin() const { return performed_min_; }
  int64 PerformedMax() const { return performed_max_; }
  int64 PerformedValue() const {
    CHECK_EQ(performed_max_, performed_min_);
    return performed_max_;
  }
  void SetStartMin(int64 m) { start_min_ = m; }
  void SetStartMax(int64 m) { start_max_ = m; }
  void SetStartRange(int64 mi, int64 ma) {
    start_min_ = mi;
    start_max_ = ma;
  }
  void SetStartValue(int64 v) {
    start_min_ = v;
    start_max_ = v;
  }
  void SetDurationMin(int64 m) { duration_min_ = m; }
  void SetDurationMax(int64 m) { duration_max_ = m; }
  void SetDurationRange(int64 mi, int64 ma) {
    duration_min_ = mi;
    duration_max_ = ma;
  }
  void SetDurationValue(int64 v) {
    duration_min_ = v;
    duration_max_ = v;
  }
  void SetEndMin(int64 m) { end_min_ = m; }
  void SetEndMax(int64 m) { end_max_ = m; }
  void SetEndRange(int64 mi, int64 ma) {
    end_min_ = mi;
    end_max_ = ma;
  }
  void SetEndValue(int64 v) {
    end_min_ = v;
    end_max_ = v;
  }
  void SetPerformedMin(int64 m) { performed_min_ = m; }
  void SetPerformedMax(int64 m) { performed_max_ = m; }
  void SetPerformedRange(int64 mi, int64 ma) {
    performed_min_ = mi;
    performed_max_ = ma;
  }
  void SetPerformedValue(int64 v) {
    performed_min_ = v;
    performed_max_ = v;
  }
  string DebugString() const;
 private:
  int64 start_min_;
  int64 start_max_;
  int64 duration_min_;
  int64 duration_max_;
  int64 end_min_;
  int64 end_max_;
  int64 performed_min_;
  int64 performed_max_;
  IntervalVar* var_;
};

// ----- Assignment element container -----

template <class V, class E> class AssignmentContainer {
 public:
  AssignmentContainer() {}
  E& Add(V* const var) {
    CHECK_NOTNULL(var);
    int index = -1;
    if (!Find(var, &index)) {
      return FastAdd(var);
    } else {
      return elements_[index];
    }
  }
  // Adds element without checking its presence in the container.
  E& FastAdd(V* const var) {
    DCHECK(var != NULL);
    E e(var);
    elements_.push_back(e);
    if (elements_map_.size() != 0) {
      CopyToMap();
    }
    return elements_.back();
  }
  void Clear() {
    elements_.clear();
    elements_map_.clear();
  }
  bool Empty() const {
    return elements_.empty();
  }
  // Copies intersection of containers.
  void Copy(const AssignmentContainer<V, E>& container) {
    for (int i = 0; i < container.elements_.size(); ++i) {
      const E& element = container.elements_[i];
      const V* const var = element.Var();
      int index = -1;
      if (i < elements_.size() && elements_[i].Var() == var) {
        index = i;
      } else if (!Find(var, &index)) {
        continue;
      }
      DCHECK_GE(index, 0);
      E& local_element(elements_[index]);
      local_element.Copy(element);
      if (element.Activated()) {
        local_element.Activate();
      } else {
        local_element.Deactivate();
      }
    }
  }
  bool Contains(const V* const var) const {
    int index;
    return Find(var, &index);
  }
  E& MutableElement(const V* const var) {
    int index = -1;
    const bool found = Find(var, &index);
    DCHECK(found);
    return MutableElement(index);
  }
  const E& Element(const V* const var) const {
    int index = -1;
    const bool found = Find(var, &index);
    DCHECK(found);
    return Element(index);
  }
  E& MutableElement(int index) { return elements_[index]; }
  const E& Element(int index) const { return elements_[index]; }
  int Size() const { return elements_.size(); }
  void Store() {
    for (int i = 0; i < elements_.size(); ++i) {
      elements_[i].Store();
    }
  }
  void Restore() {
    for (int i = 0; i < elements_.size(); ++i) {
      E& element = elements_[i];
      if (element.Activated()) {
        element.Restore();
      }
    }
  }
 private:
  void CopyToMap() {
    for (int i = elements_map_.size(); i < elements_.size(); ++i) {
      elements_map_[elements_[i].Var()] = i;
    }
  }
  bool Find(const V* const var, int* index) const {
    if (elements_map_.size() == 0) {
      const_cast<AssignmentContainer<V, E>*>(this)->CopyToMap();
    }
    DCHECK_EQ(elements_map_.size(), elements_.size());
    return FindCopy(elements_map_, var, index);
  }

  vector<E> elements_;
  hash_map<const V*, int> elements_map_;
};

// ----- Assignment -----

// An Assignment is a variable -> domains mapping
// It is used to report solutions to the user
class Assignment : public PropagationBaseObject {
 public:
  typedef AssignmentContainer<IntVar, IntVarElement> IntContainer;
  typedef AssignmentContainer<IntervalVar, IntervalVarElement>
  IntervalContainer;

  explicit Assignment(Solver* const s);
  explicit Assignment(const Assignment* const copy);
  virtual ~Assignment();

  void Clear();
  bool Empty() const {
    return int_var_container_.Empty() && interval_var_container_.Empty();
  }
  int Size() const {
    return int_var_container_.Size() + interval_var_container_.Size();
  }
  void Store();
  void Restore();

  void Load(const AssignmentProto& proto);
  void Save(AssignmentProto* const proto);

  void AddObjective(IntVar* const v);
  IntVar* Objective() const;
  bool HasObjective() const;
  int64 ObjectiveMin() const;
  int64 ObjectiveMax() const;
  int64 ObjectiveValue() const;
  bool ObjectiveBound() const;
  void SetObjectiveMin(int64 m);
  void SetObjectiveMax(int64 m);
  void SetObjectiveValue(int64 value);
  void SetObjectiveRange(int64 l, int64 u);

  IntVarElement& Add(IntVar* const v);
  void Add(IntVar* const* vars, int size);
  void Add(const vector<IntVar*>& v);
  // Adds without checking if variable has been previously added.
  IntVarElement& FastAdd(IntVar* const v);
  int64 Min(const IntVar* const v) const;
  int64 Max(const IntVar* const v) const;
  int64 Value(const IntVar* const v) const;
  bool Bound(const IntVar* const v) const;
  void SetMin(const IntVar* const v, int64 m);
  void SetMax(const IntVar* const v, int64 m);
  void SetRange(const IntVar* const v, int64 l, int64 u);
  void SetValue(const IntVar* const v, int64 value);

  IntervalVarElement& Add(IntervalVar* const v);
  void Add(IntervalVar* const * vars, int size);
  void Add(const vector<IntervalVar*>& vars);
  // Adds without checking if variable has been previously added.
  IntervalVarElement& FastAdd(IntervalVar* const v);
  int64 StartMin(const IntervalVar* const v) const;
  int64 StartMax(const IntervalVar* const v) const;
  int64 StartValue(const IntervalVar* const v) const;
  int64 DurationMin(const IntervalVar* const v) const;
  int64 DurationMax(const IntervalVar* const v) const;
  int64 DurationValue(const IntervalVar* const c) const;
  int64 EndMin(const IntervalVar* const v) const;
  int64 EndMax(const IntervalVar* const v) const;
  int64 EndValue(const IntervalVar* const v) const;
  int64 PerformedMin(const IntervalVar* const v) const;
  int64 PerformedMax(const IntervalVar* const v) const;
  int64 PerformedValue(const IntervalVar* const v) const;
  void SetStartMin(const IntervalVar* const v, int64 m);
  void SetStartMax(const IntervalVar* const v, int64 m);
  void SetStartRange(const IntervalVar* const v, int64 mi, int64 ma);
  void SetStartValue(const IntervalVar* const v, int64 value);
  void SetDurationMin(const IntervalVar* const v, int64 m);
  void SetDurationMax(const IntervalVar* const v, int64 m);
  void SetDurationRange(const IntervalVar* const v, int64 mi, int64 ma);
  void SetDurationValue(const IntervalVar* const v, int64 value);
  void SetEndMin(const IntervalVar* const v, int64 m);
  void SetEndMax(const IntervalVar* const v, int64 m);
  void SetEndRange(const IntervalVar* const v, int64 mi, int64 ma);
  void SetEndValue(const IntervalVar* const v, int64 value);
  void SetPerformedMin(const IntervalVar* const v, int64 m);
  void SetPerformedMax(const IntervalVar* const v, int64 m);
  void SetPerformedRange(const IntervalVar* const v, int64 mi, int64 ma);
  void SetPerformedValue(const IntervalVar* const v, int64 value);

  void Activate(const IntVar* const v);
  void Deactivate(const IntVar* const v);
  bool Activated(const IntVar* const v) const;

  void Activate(const IntervalVar* const v);
  void Deactivate(const IntervalVar* const v);
  bool Activated(const IntervalVar* const v) const;

  void ActivateObjective();
  void DeactivateObjective();
  bool ActivatedObjective() const;

  virtual string DebugString() const;

  bool Contains(const IntVar* const var) const;
  bool Contains(const IntervalVar* const var) const;
  // Copies the intersection of the 2 assignments to the current assignment.
  void Copy(const Assignment* assignment);

  // TODO(user): Add iterators on elements to avoid exposing container class.
  const IntContainer& IntVarContainer() const {
    return int_var_container_;
  }
  IntContainer& MutableIntVarContainer() {
    return int_var_container_;
  }
  const IntervalContainer& IntervalVarContainer() const {
    return interval_var_container_;
  }
  IntervalContainer& MutableIntervalVarContainer() {
    return interval_var_container_;
  }
 private:
  IntContainer int_var_container_;
  IntervalContainer interval_var_container_;
  IntVarElement* obj_element_;
  IntVar* objective_;
  DISALLOW_COPY_AND_ASSIGN(Assignment);
};

// ---------- Misc ----------

// This method returns 0. It is useful when 0 can be cast either as
// a pointer or as a integer value and thus lead to an ambiguous
// function call.
inline int64 Zero() {
  return 0LL;
}

// This class represents a small reversible bitset (size <= 64).
// It supports the following operations:
// SetToOne(pos) (reversible)
// SetToZero(solver, pos) (reversible, with stamp)
// Cardinality()
// IsCardZero() // fast test
// IsCardOne() // fast test
// GetFirstOne()
// This class is useful to maintain supports.
class SmallRevBitSet {
 public:
  explicit SmallRevBitSet(int64 size);
  void SetToOne(Solver* const solver, int64 pos);
  void SetToZero(Solver* const solver, int64 pos);
  int64 Cardinality() const;
  bool IsCardinalityZero() const { return bits_ == GG_ULONGLONG(0); }
  bool IsCardinalityOne() const {
    return (bits_ != 0) && !(bits_ & (bits_ - 1));
  }
  int64 GetFirstOne() const;
 private:
  uint64 bits_;
  uint64 stamp_;
};

// This class represents a reversible bitset.
// It supports the following operations:
// SetToOne(pos) (reversible)
// SetToZero(solver, pos) (reversible, with stamp)
// Cardinality()
// IsCardZero() // fast test
// IsCardOne() // fast test
// GetFirstBit(int start)
// IsSet(pos)
// This class is useful to maintain supports.
// It also provides a matrix like API that is directly flattened into an array
// one.
class RevBitSet {
 public:
  explicit RevBitSet(int64 size);
  RevBitSet(int64 rows, int64 columns);
  ~RevBitSet();

  // Array API
  void SetToOne(Solver* const solver, int64 pos);
  void SetToZero(Solver* const solver, int64 pos);
  bool IsSet(int64 pos) const;
  int64 Cardinality() const;
  bool IsCardinalityZero() const;
  bool IsCardinalityOne() const;
  int64 GetFirstBit(int start) const;

  // Matrix API
  void SetToOne(Solver* const solver, int64 row, int64 column);
  void SetToZero(Solver* const solver, int64 row, int64 column);
  bool IsSet(int64 row, int64 column) const {
    DCHECK_GE(row, 0);
    DCHECK_LT(row, rows_);
    DCHECK_GE(column, 0);
    DCHECK_LT(column, columns_);
    return IsSet(row * columns_ + column);
  }
  int64 Cardinality(int row) const;
  bool IsCardinalityZero(int row) const;
  bool IsCardinalityOne(int row) const;
  int64 GetFirstBit(int row, int start) const;

  // Works in matrix and array mode.
  void RevClearAll(Solver* const solver);
 private:
  const int64 rows_;
  const int64 columns_;
  const int64 length_;
  uint64* bits_;
  uint64* stamps_;
};

// ---------- Pack Constraint ----------

class Pack : public Constraint {
 public:
  Pack(Solver* const s,
       const IntVar* const * vars,
       int vsize,
       int64 number_of_bins);

  virtual ~Pack();

  // ----- Public API -----

  // Dimensions are additional constraints than can restrict what is
  // possible with the pack constraint. It can be used to set capacity
  // limits, to count objects per bin, to compute unassigned
  // penalties...


  // This dimension imposes that for all bins b, the weighted sum
  // (weights[i]) of all objects i assigned to 'b' is less or equal
  // 'bounds[b]'.
  void AddWeightedSumLessOrEqualConstantDimension(const vector<int64>& weights,
                                                  const vector<int64>& bounds);

  // This dimension imposes that for all bins b, the weighted sum
  // (weights[i]) of all objects i assigned to 'b' is equal to loads[b].
  void AddWeightedSumEqualVarDimension(const vector<int64>& weights,
                                       const vector<IntVar*>& loads);

  // This dimension enforces that cost_var == sum of weights[i] for
  // all objects 'i' assigned to a bin.
  void AddWeightedSumOfAssignedDimension(const vector<int64>& weights,
                                         IntVar* const cost_var);

  // This dimension links 'count_var' to the actual number of bins used in the
  // pack.
  void AddCountUsedBinDimension(IntVar* const count_var);

  // This dimension links 'count_var' to the actual number of items
  // assigned to a bin in the pack.
  void AddCountAssignedItemsDimension(IntVar* const count_var);
  // ----- Internal API -----

  virtual void Post();
  void ClearAll();
  void PropagateDelayed();
  virtual void InitialPropagate();
  void Propagate();
  void OneDomain(int var_index);
  virtual string DebugString() const;
  bool IsUndecided(int64 var_index, int64 bin_index) const {
    return unprocessed_.IsSet(bin_index, var_index);
  }
  void SetImpossible(int64 var_index, int64 bin_index);
  void Assign(int64 var_index, int64 bin_index);
  bool IsAssignedStatusKnown(int64 var_index) const;
  void SetAssigned(int64 var_index);
  void SetUnassigned(int64 var_index);
  void RemoveAllPossibleFromBin(int64 bin_index);
  void AssignAllPossibleToBin(int64 bin_index);
  void AssignFirstPossibleToBin(int64 bin_index);
  void AssignAllRemainingItems();
  void UnassignAllRemainingItems();
 private:
  bool IsInProcess() const;
  scoped_array<IntVar*> vars_;
  const int vsize_;
  const int64 bins_;
  vector<Dimension*> dims_;
  RevBitSet unprocessed_;
  vector<vector<int64> > forced_;
  vector<vector<int64> > removed_;
  scoped_array<IntVarIterator*> holes_;
  uint64 stamp_;
  Demon* demon_;
  vector<pair<int64, int64> > to_set_;
  vector<pair<int64, int64> > to_unset_;
  bool in_process_;
};

// ----- SolutionPool -----

// This class is used to manage a pool of solutions. It can transform
// a single point local search into a multi point local search.
class SolutionPool : public BaseObject {
 public:
  SolutionPool() {}
  ~SolutionPool() {}

  // This method is called to initialize the solution pool with the assignment
  // from the local search.
  virtual void Initialize(Assignment* const assignment) = 0;

  // This method is called when a new solution has been accepted by the local
  // search.
  virtual void RegisterNewSolution(Assignment* const assignment) = 0;

  // This method is called when the local search starts a new neighborhood to
  // initialize the default assignment.
  virtual void GetNextSolution(Assignment* const assignment) = 0;

  // This method checks if the local solution needs to be updated with
  // an external one.
  virtual bool SyncNeeded(Assignment* const local_assignment) = 0;
};


}  // namespace operations_research
#endif  // CONSTRAINT_SOLVER_CONSTRAINT_SOLVER_H_

// Copyright 2010-2022 Google LLC
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

/// Declaration of the core objects for the constraint solver.
///
/// The literature around constraint programming is extremely dense but one
/// can find some basic introductions in the following links:
///   - http://en.wikipedia.org/wiki/Constraint_programming
///   - http://kti.mff.cuni.cz/~bartak/constraints/index.html
///
/// Here is a very simple Constraint Programming problem:
///
///   If we see 56 legs and 20 heads, how many two-legged pheasants
///   and four-legged rabbits are we looking at?
///
/// Here is some simple Constraint Programming code to find out:
///
///   void pheasant() {
///     Solver s("pheasant");
///     // Create integer variables to represent the number of pheasants and
///     // rabbits, with a minimum of 0 and a maximum of 20.
///     IntVar* const p = s.MakeIntVar(0, 20, "pheasant"));
///     IntVar* const r = s.MakeIntVar(0, 20, "rabbit"));
///     // The number of heads is the sum of pheasants and rabbits.
///     IntExpr* const heads = s.MakeSum(p, r);
///     // The number of legs is the sum of pheasants * 2 and rabbits * 4.
///     IntExpr* const legs = s.MakeSum(s.MakeProd(p, 2), s.MakeProd(r, 4));
///     // Constraints: the number of legs is 56 and heads is 20.
///     Constraint* const ct_legs = s.MakeEquality(legs, 56);
///     Constraint* const ct_heads = s.MakeEquality(heads, 20);
///     s.AddConstraint(ct_legs);
///     s.AddConstraint(ct_heads);
///     DecisionBuilder* const db = s.MakePhase(p, r,
///                                             Solver::CHOOSE_FIRST_UNBOUND,
///                                             Solver::ASSIGN_MIN_VALUE);
///     s.NewSearch(db);
///     CHECK(s.NextSolution());
///     LOG(INFO) << "rabbits -> " << r->Value() << ", pheasants -> "
///               << p->Value();
///     LOG(INFO) << s.DebugString();
///     s.EndSearch();
///   }
///
/// which outputs:
///
///   rabbits -> 8, pheasants -> 12
///   Solver(name = "pheasant",
///          state = OUTSIDE_SEARCH,
///          branches = 0,
///          fails = 0,
///          decisions = 0
///          propagation loops = 11,
///          demons Run = 25,
///          Run time = 0 ms)
///
///

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_CONSTRAINT_SOLVER_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_CONSTRAINT_SOLVER_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/log_severity.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/random/random.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/map_util.h"
#include "ortools/base/timer.h"
#include "ortools/constraint_solver/search_stats.pb.h"
#include "ortools/constraint_solver/solver_parameters.pb.h"
#include "ortools/util/piecewise_linear_function.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/tuple_set.h"

#if !defined(SWIG)
ABSL_DECLARE_FLAG(int64_t, cp_random_seed);
#endif  // !defined(SWIG)

class File;

namespace operations_research {

class Assignment;
class AssignmentProto;
class BaseObject;
class CastConstraint;
class Constraint;
class Decision;
class DecisionBuilder;
class DecisionVisitor;
class Demon;
class DemonProfiler;
class Dimension;
class DisjunctiveConstraint;
class ImprovementSearchLimit;
class IntExpr;
class IntVar;
class IntVarAssignment;
class IntVarLocalSearchFilter;
class IntervalVar;
class IntervalVarAssignment;
class LocalSearchFilter;
class LocalSearchFilterManager;
class LocalSearchMonitor;
class LocalSearchOperator;
class LocalSearchPhaseParameters;
class LocalSearchProfiler;
class ModelCache;
class ModelVisitor;
class OptimizeVar;
class Pack;
class ProfiledDecisionBuilder;
class PropagationBaseObject;
class PropagationMonitor;
class Queue;
class RegularLimit;
class RegularLimitParameters;
class RevBitMatrix;
class Search;
class SearchLimit;
class SearchMonitor;
class SequenceVar;
class SequenceVarAssignment;
class SolutionCollector;
class SolutionPool;
class SymmetryBreaker;
struct StateInfo;
struct Trail;
template <class T>
class SimpleRevFIFO;
template <typename F>
class LightIntFunctionElementCt;
template <typename F>
class LightIntIntFunctionElementCt;

inline int64_t CpRandomSeed() {
  return absl::GetFlag(FLAGS_cp_random_seed) == -1
             ? absl::Uniform<int64_t>(absl::BitGen(), 0, kint64max)
             : absl::GetFlag(FLAGS_cp_random_seed);
}

/// This struct holds all parameters for the default search.
/// DefaultPhaseParameters is only used by Solver::MakeDefaultPhase methods.
/// Note this is for advanced users only.
struct DefaultPhaseParameters {
 public:
  enum VariableSelection {
    CHOOSE_MAX_SUM_IMPACT = 0,
    CHOOSE_MAX_AVERAGE_IMPACT = 1,
    CHOOSE_MAX_VALUE_IMPACT = 2,
  };

  enum ValueSelection {
    SELECT_MIN_IMPACT = 0,
    SELECT_MAX_IMPACT = 1,
  };

  enum DisplayLevel { NONE = 0, NORMAL = 1, VERBOSE = 2 };

  /// This parameter describes how the next variable to instantiate
  /// will be chosen.
  VariableSelection var_selection_schema;

  /// This parameter describes which value to select for a given var.
  ValueSelection value_selection_schema;

  /// Maximum number of intervals that the initialization of impacts will scan
  /// per variable.
  int initialization_splits;

  /// The default phase will run heuristics periodically. This parameter
  /// indicates if we should run all heuristics, or a randomly selected
  /// one.
  bool run_all_heuristics;

  /// The distance in nodes between each run of the heuristics. A
  /// negative or null value will mean that we will not run heuristics
  /// at all.
  int heuristic_period;

  /// The failure limit for each heuristic that we run.
  int heuristic_num_failures_limit;

  /// Whether to keep the impact from the first search for other searches,
  /// or to recompute the impact for each new search.
  bool persistent_impact;

  /// Seed used to initialize the random part in some heuristics.
  int random_seed;

  /// This represents the amount of information displayed by the default search.
  /// NONE means no display, VERBOSE means extra information.
  DisplayLevel display_level;

  /// Should we use last conflict method. The default is false.
  bool use_last_conflict;

  /// When defined, this overrides the default impact based decision builder.
  DecisionBuilder* decision_builder;

  DefaultPhaseParameters();
};

/// Solver Class
///
/// A solver represents the main computation engine. It implements the entire
/// range of Constraint Programming protocols:
///   - Reversibility
///   - Propagation
///   - Search
///
/// Usually, Constraint Programming code consists of
///   - the creation of the Solver,
///   - the creation of the decision variables of the model,
///   - the creation of the constraints of the model and their addition to the
///     solver() through the AddConstraint() method,
///   - the creation of the main DecisionBuilder class,
///   - the launch of the solve() method with the decision builder.
///
/// For the time being, Solver is neither MT_SAFE nor MT_HOT.
class Solver {
 public:
  /// Holds semantic information stating that the 'expression' has been
  /// cast into 'variable' using the Var() method, and that
  /// 'maintainer' is responsible for maintaining the equality between
  /// 'variable' and 'expression'.
  struct IntegerCastInfo {
    IntegerCastInfo()
        : variable(nullptr), expression(nullptr), maintainer(nullptr) {}
    IntegerCastInfo(IntVar* const v, IntExpr* const e, Constraint* const c)
        : variable(v), expression(e), maintainer(c) {}
    IntVar* variable;
    IntExpr* expression;
    Constraint* maintainer;
  };

  /// Number of priorities for demons.
  static constexpr int kNumPriorities = 3;

  /// This enum describes the strategy used to select the next branching
  /// variable at each node during the search.
  enum IntVarStrategy {
    /// The default behavior is CHOOSE_FIRST_UNBOUND.
    INT_VAR_DEFAULT,

    /// The simple selection is CHOOSE_FIRST_UNBOUND.
    INT_VAR_SIMPLE,

    /// Select the first unbound variable.
    /// Variables are considered in the order of the vector of IntVars used
    /// to create the selector.
    CHOOSE_FIRST_UNBOUND,

    /// Randomly select one of the remaining unbound variables.
    CHOOSE_RANDOM,

    /// Among unbound variables, select the variable with the smallest size,
    /// i.e., the smallest number of possible values.
    /// In case of a tie, the selected variables is the one with the lowest min
    /// value.
    /// In case of a tie, the first one is selected, first being defined by the
    /// order in the vector of IntVars used to create the selector.
    CHOOSE_MIN_SIZE_LOWEST_MIN,

    /// Among unbound variables, select the variable with the smallest size,
    /// i.e., the smallest number of possible values.
    /// In case of a tie, the selected variable is the one with the highest min
    /// value.
    /// In case of a tie, the first one is selected, first being defined by the
    /// order in the vector of IntVars used to create the selector.
    CHOOSE_MIN_SIZE_HIGHEST_MIN,

    /// Among unbound variables, select the variable with the smallest size,
    /// i.e., the smallest number of possible values.
    /// In case of a tie, the selected variables is the one with the lowest max
    /// value.
    /// In case of a tie, the first one is selected, first being defined by the
    /// order in the vector of IntVars used to create the selector.
    CHOOSE_MIN_SIZE_LOWEST_MAX,

    /// Among unbound variables, select the variable with the smallest size,
    /// i.e., the smallest number of possible values.
    /// In case of a tie, the selected variable is the one with the highest max
    /// value.
    /// In case of a tie, the first one is selected, first being defined by the
    /// order in the vector of IntVars used to create the selector.
    CHOOSE_MIN_SIZE_HIGHEST_MAX,

    /// Among unbound variables, select the variable with the smallest minimal
    /// value.
    /// In case of a tie, the first one is selected, "first" defined by the
    /// order in the vector of IntVars used to create the selector.
    CHOOSE_LOWEST_MIN,

    /// Among unbound variables, select the variable with the highest maximal
    /// value.
    /// In case of a tie, the first one is selected, first being defined by the
    /// order in the vector of IntVars used to create the selector.
    CHOOSE_HIGHEST_MAX,

    /// Among unbound variables, select the variable with the smallest size.
    /// In case of a tie, the first one is selected, first being defined by the
    /// order in the vector of IntVars used to create the selector.
    CHOOSE_MIN_SIZE,

    /// Among unbound variables, select the variable with the highest size.
    /// In case of a tie, the first one is selected, first being defined by the
    /// order in the vector of IntVars used to create the selector.
    CHOOSE_MAX_SIZE,

    /// Among unbound variables, select the variable with the largest
    /// gap between the first and the second values of the domain.
    CHOOSE_MAX_REGRET_ON_MIN,

    /// Selects the next unbound variable on a path, the path being defined by
    /// the variables: var[i] corresponds to the index of the next of i.
    CHOOSE_PATH,
  };
  // TODO(user): add HIGHEST_MIN and LOWEST_MAX.

  /// This enum describes the strategy used to select the next variable value to
  /// set.
  enum IntValueStrategy {
    /// The default behavior is ASSIGN_MIN_VALUE.
    INT_VALUE_DEFAULT,

    /// The simple selection is ASSIGN_MIN_VALUE.
    INT_VALUE_SIMPLE,

    /// Selects the min value of the selected variable.
    ASSIGN_MIN_VALUE,

    /// Selects the max value of the selected variable.
    ASSIGN_MAX_VALUE,

    /// Selects randomly one of the possible values of the selected variable.
    ASSIGN_RANDOM_VALUE,

    /// Selects the first possible value which is the closest to the center
    /// of the domain of the selected variable.
    /// The center is defined as (min + max) / 2.
    ASSIGN_CENTER_VALUE,

    /// Split the domain in two around the center, and choose the lower
    /// part first.
    SPLIT_LOWER_HALF,

    /// Split the domain in two around the center, and choose the lower
    /// part first.
    SPLIT_UPPER_HALF,
  };

  /// This enum is used by Solver::MakePhase to specify how to select variables
  /// and values during the search.
  /// In Solver::MakePhase(const std::vector<IntVar*>&, IntVarStrategy,
  /// IntValueStrategy), variables are selected first, and then the associated
  /// value.
  /// In Solver::MakePhase(const std::vector<IntVar*>& vars, IndexEvaluator2,
  /// EvaluatorStrategy), the selection is done scanning every pair
  /// <variable, possible value>. The next selected pair is then the best among
  /// all possibilities, i.e. the pair with the smallest evaluation.
  /// As this is costly, two options are offered: static or dynamic evaluation.
  enum EvaluatorStrategy {
    /// Pairs are compared at the first call of the selector, and results are
    /// cached. Next calls to the selector use the previous computation, and so
    /// are not up-to-date, e.g. some <variable, value> pairs may not be
    /// possible anymore due to propagation since the first to call.
    CHOOSE_STATIC_GLOBAL_BEST,

    /// Pairs are compared each time a variable is selected. That way all pairs
    /// are relevant and evaluation is accurate.
    /// This strategy runs in O(number-of-pairs) at each variable selection,
    /// versus O(1) in the static version.
    CHOOSE_DYNAMIC_GLOBAL_BEST,
  };

  /// Used for scheduling. Not yet implemented.
  enum SequenceStrategy {
    SEQUENCE_DEFAULT,
    SEQUENCE_SIMPLE,
    CHOOSE_MIN_SLACK_RANK_FORWARD,
    CHOOSE_RANDOM_RANK_FORWARD,
  };

  /// This enum describes the straregy used to select the next interval variable
  /// and its value to be fixed.
  enum IntervalStrategy {
    /// The default is INTERVAL_SET_TIMES_FORWARD.
    INTERVAL_DEFAULT,
    /// The simple is INTERVAL_SET_TIMES_FORWARD.
    INTERVAL_SIMPLE,
    /// Selects the variable with the lowest starting time of all variables,
    /// and fixes its starting time to this lowest value.
    INTERVAL_SET_TIMES_FORWARD,
    /// Selects the variable with the highest ending time of all variables,
    /// and fixes the ending time to this highest values.
    INTERVAL_SET_TIMES_BACKWARD
  };

  /// This enum is used in Solver::MakeOperator to specify the neighborhood to
  /// create.
  enum LocalSearchOperators {
    /// Operator which reverses a sub-chain of a path. It is called TwoOpt
    /// because it breaks two arcs on the path; resulting paths are called
    /// two-optimal.
    /// Possible neighbors for the path 1 -> 2 -> 3 -> 4 -> 5
    /// (where (1, 5) are first and last nodes of the path and can therefore not
    /// be moved):
    ///   1 -> [3 -> 2] -> 4  -> 5
    ///   1 -> [4 -> 3  -> 2] -> 5
    ///   1 ->  2 -> [4 -> 3] -> 5
    TWOOPT,

    /// Relocate: OROPT and RELOCATE.
    /// Operator which moves a sub-chain of a path to another position; the
    /// specified chain length is the fixed length of the chains being moved.
    /// When this length is 1, the operator simply moves a node to another
    /// position.
    /// Possible neighbors for the path 1 -> 2 -> 3 -> 4 -> 5, for a chain
    /// length of 2 (where (1, 5) are first and last nodes of the path and can
    /// therefore not be moved):
    ///   1 ->  4 -> [2 -> 3] -> 5
    ///   1 -> [3 -> 4] -> 2  -> 5
    ///
    /// Using Relocate with chain lengths of 1, 2 and 3 together is equivalent
    /// to the OrOpt operator on a path. The OrOpt operator is a limited
    ///  version of 3Opt (breaks 3 arcs on a path).
    OROPT,

    /// Relocate neighborhood with length of 1 (see OROPT comment).
    RELOCATE,

    /// Operator which exchanges the positions of two nodes.
    /// Possible neighbors for the path 1 -> 2 -> 3 -> 4 -> 5
    /// (where (1, 5) are first and last nodes of the path and can therefore not
    /// be moved):
    ///   1 -> [3] -> [2] ->  4  -> 5
    ///   1 -> [4] ->  3  -> [2] -> 5
    ///   1 ->  2  -> [4] -> [3] -> 5
    EXCHANGE,

    /// Operator which cross exchanges the starting chains of 2 paths, including
    /// exchanging the whole paths.
    /// First and last nodes are not moved.
    /// Possible neighbors for the paths 1 -> 2 -> 3 -> 4 -> 5 and 6 -> 7 -> 8
    /// (where (1, 5) and (6, 8) are first and last nodes of the paths and can
    /// therefore not be moved):
    ///   1 -> [7] -> 3 -> 4 -> 5  6 -> [2] -> 8
    ///   1 -> [7] -> 4 -> 5       6 -> [2 -> 3] -> 8
    ///   1 -> [7] -> 5            6 -> [2 -> 3 -> 4] -> 8
    CROSS,

    /// Operator which inserts an inactive node into a path.
    /// Possible neighbors for the path 1 -> 2 -> 3 -> 4 with 5 inactive
    /// (where 1 and 4 are first and last nodes of the path) are:
    ///   1 -> [5] ->  2  ->  3  -> 4
    ///   1 ->  2  -> [5] ->  3  -> 4
    ///   1 ->  2  ->  3  -> [5] -> 4
    MAKEACTIVE,

    /// Operator which makes path nodes inactive.
    /// Possible neighbors for the path 1 -> 2 -> 3 -> 4 (where 1 and 4 are
    /// first and last nodes of the path) are:
    ///   1 -> 3 -> 4 with 2 inactive
    ///   1 -> 2 -> 4 with 3 inactive
    MAKEINACTIVE,

    /// Operator which makes a "chain" of path nodes inactive.
    /// Possible neighbors for the path 1 -> 2 -> 3 -> 4 (where 1 and 4 are
    /// first and last nodes of the path) are:
    ///   1 -> 3 -> 4 with 2 inactive
    ///   1 -> 2 -> 4 with 3 inactive
    ///   1 -> 4 with 2 and 3 inactive
    MAKECHAININACTIVE,

    /// Operator which replaces an active node by an inactive one.
    /// Possible neighbors for the path 1 -> 2 -> 3 -> 4 with 5 inactive
    /// (where 1 and 4 are first and last nodes of the path) are:
    ///   1 -> [5] ->  3  -> 4 with 2 inactive
    ///   1 ->  2  -> [5] -> 4 with 3 inactive
    SWAPACTIVE,

    /// Operator which makes an inactive node active and an active one inactive.
    /// It is similar to SwapActiveOperator except that it tries to insert the
    /// inactive node in all possible positions instead of just the position of
    /// the node made inactive.
    /// Possible neighbors for the path 1 -> 2 -> 3 -> 4 with 5 inactive
    /// (where 1 and 4 are first and last nodes of the path) are:
    ///   1 -> [5] ->  3  -> 4 with 2 inactive
    ///   1 ->  3  -> [5] -> 4 with 2 inactive
    ///   1 -> [5] ->  2  -> 4 with 3 inactive
    ///   1 ->  2  -> [5] -> 4 with 3 inactive
    EXTENDEDSWAPACTIVE,

    /// Operator which relaxes two sub-chains of three consecutive arcs each.
    /// Each sub-chain is defined by a start node and the next three arcs. Those
    /// six arcs are relaxed to build a new neighbor.
    /// PATHLNS explores all possible pairs of starting nodes and so defines
    /// n^2 neighbors, n being the number of nodes.
    /// Note that the two sub-chains can be part of the same path; they even may
    /// overlap.
    PATHLNS,

    /// Operator which relaxes one entire path and all inactive nodes, thus
    /// defining num_paths neighbors.
    FULLPATHLNS,

    /// Operator which relaxes all inactive nodes and one sub-chain of six
    /// consecutive arcs. That way the path can be improved by inserting
    /// inactive nodes or swapping arcs.
    UNACTIVELNS,

    /// Operator which defines one neighbor per variable. Each neighbor tries to
    /// increment by one the value of the corresponding variable. When a new
    /// solution is found the neighborhood is rebuilt from scratch, i.e., tries
    /// to increment values in the variable order.
    /// Consider for instance variables x and y. x is incremented one by one to
    /// its max, and when it is not possible to increment x anymore, y is
    /// incremented once. If this is a solution, then next neighbor tries to
    /// increment x.
    INCREMENT,

    /// Operator which defines a neighborhood to decrement values.
    /// The behavior is the same as INCREMENT, except values are decremented
    /// instead of incremented.
    DECREMENT,

    /// Operator which defines one neighbor per variable. Each neighbor relaxes
    /// one variable.
    /// When a new solution is found the neighborhood is rebuilt from scratch.
    /// Consider for instance variables x and y. First x is relaxed and the
    /// solver is looking for the best possible solution (with only x relaxed).
    /// Then y is relaxed, and the solver is looking for a new solution.
    /// If a new solution is found, then the next variable to be relaxed is x.
    SIMPLELNS
  };

  /// This enum is used in Solver::MakeOperator associated with an evaluator
  /// to specify the neighborhood to create.
  enum EvaluatorLocalSearchOperators {
    /// Lin-Kernighan local search.
    /// While the accumulated local gain is positive, perform a 2opt or a 3opt
    /// move followed by a series of 2opt moves. Return a neighbor for which the
    /// global gain is positive.
    LK,

    /// Sliding TSP operator.
    /// Uses an exact dynamic programming algorithm to solve the TSP
    /// corresponding to path sub-chains.
    /// For a subchain 1 -> 2 -> 3 -> 4 -> 5 -> 6, solves the TSP on
    /// nodes A, 2, 3, 4, 5, where A is a merger of nodes 1 and 6 such that
    /// cost(A,i) = cost(1,i) and cost(i,A) = cost(i,6).
    TSPOPT,

    /// TSP-base LNS.
    /// Randomly merge consecutive nodes until n "meta"-nodes remain and solve
    /// the corresponding TSP.
    /// This is an "unlimited" neighborhood which must be stopped by search
    /// limits. To force diversification, the operator iteratively forces each
    /// node to serve as base of a meta-node.
    TSPLNS
  };

  /// This enum is used in Solver::MakeLocalSearchObjectiveFilter. It specifies
  /// the behavior of the objective filter to create. The goal is to define
  /// under which condition a move is accepted based on the current objective
  /// value.
  enum LocalSearchFilterBound {
    /// Move is accepted when the current objective value >= objective.Min.
    GE,
    /// Move is accepted when the current objective value <= objective.Max.
    LE,
    /// Move is accepted when the current objective value is in the interval
    /// objective.Min .. objective.Max.
    EQ
  };

  /// This enum represents the three possible priorities for a demon in the
  /// Solver queue.
  /// Note: this is for advanced users only.
  enum DemonPriority {
    /// DELAYED_PRIORITY is the lowest priority: Demons will be processed after
    /// VAR_PRIORITY and NORMAL_PRIORITY demons.
    DELAYED_PRIORITY = 0,

    /// VAR_PRIORITY is between DELAYED_PRIORITY and NORMAL_PRIORITY.
    VAR_PRIORITY = 1,

    /// NORMAL_PRIORITY is the highest priority: Demons will be processed first.
    NORMAL_PRIORITY = 2,
  };

  /// This enum is used in Solver::MakeIntervalVarRelation to specify the
  /// temporal relation between the two intervals t1 and t2.
  enum BinaryIntervalRelation {
    /// t1 ends after t2 end, i.e. End(t1) >= End(t2) + delay.
    ENDS_AFTER_END,

    /// t1 ends after t2 start, i.e. End(t1) >= Start(t2) + delay.
    ENDS_AFTER_START,

    /// t1 ends at t2 end, i.e. End(t1) == End(t2) + delay.
    ENDS_AT_END,

    /// t1 ends at t2 start, i.e. End(t1) == Start(t2) + delay.
    ENDS_AT_START,

    /// t1 starts after t2 end, i.e. Start(t1) >= End(t2) + delay.
    STARTS_AFTER_END,

    /// t1 starts after t2 start, i.e. Start(t1) >= Start(t2) + delay.
    STARTS_AFTER_START,

    /// t1 starts at t2 end, i.e. Start(t1) == End(t2) + delay.
    STARTS_AT_END,

    /// t1 starts at t2 start, i.e. Start(t1) == Start(t2) + delay.
    STARTS_AT_START,

    /// STARTS_AT_START and ENDS_AT_END at the same time.
    /// t1 starts at t2 start, i.e. Start(t1) == Start(t2) + delay.
    /// t1 ends at t2 end, i.e. End(t1) == End(t2).
    STAYS_IN_SYNC
  };

  /// This enum is used in Solver::MakeIntervalVarRelation to specify the
  /// temporal relation between an interval t and an integer d.
  enum UnaryIntervalRelation {
    /// t ends after d, i.e. End(t) >= d.
    ENDS_AFTER,

    /// t ends at d, i.e. End(t) == d.
    ENDS_AT,

    /// t ends before d, i.e. End(t) <= d.
    ENDS_BEFORE,

    /// t starts after d, i.e. Start(t) >= d.
    STARTS_AFTER,

    /// t starts at d, i.e. Start(t) == d.
    STARTS_AT,

    /// t starts before d, i.e. Start(t) <= d.
    STARTS_BEFORE,

    /// STARTS_BEFORE and ENDS_AFTER at the same time, i.e. d is in t.
    /// t starts before d, i.e. Start(t) <= d.
    /// t ends after d, i.e. End(t) >= d.
    CROSS_DATE,

    /// STARTS_AFTER or ENDS_BEFORE, i.e. d is not in t.
    /// t starts after d, i.e. Start(t) >= d.
    /// t ends before d, i.e. End(t) <= d.
    AVOID_DATE
  };

  /// The Solver is responsible for creating the search tree. Thanks to the
  /// DecisionBuilder, it creates a new decision with two branches at each node:
  /// left and right.
  /// The DecisionModification enum is used to specify how the branch selector
  /// should behave.
  enum DecisionModification {
    /// Keeps the default behavior, i.e. apply left branch first, and then right
    /// branch in case of backtracking.
    NO_CHANGE,

    /// Right branches are ignored. This is used to make the code faster when
    /// backtrack makes no sense or is not useful.
    /// This is faster as there is no need to create one new node per decision.
    KEEP_LEFT,

    /// Left branches are ignored. This is used to make the code faster when
    /// backtrack makes no sense or is not useful.
    /// This is faster as there is no need to create one new node per decision.
    KEEP_RIGHT,

    /// Backtracks to the previous decisions, i.e. left and right branches are
    /// not applied.
    KILL_BOTH,

    /// Applies right branch first. Left branch will be applied in case of
    /// backtracking.
    SWITCH_BRANCHES
  };

  /// This enum is used internally in private methods Solver::PushState and
  /// Solver::PopState to tag states in the search tree.
  enum MarkerType { SENTINEL, SIMPLE_MARKER, CHOICE_POINT, REVERSIBLE_ACTION };

  /// This enum represents the state of the solver w.r.t. the search.
  enum SolverState {
    /// Before search, after search.
    OUTSIDE_SEARCH,
    /// Executing the root node.
    IN_ROOT_NODE,
    /// Executing the search code.
    IN_SEARCH,
    /// After successful NextSolution and before EndSearch.
    AT_SOLUTION,
    /// After failed NextSolution and before EndSearch.
    NO_MORE_SOLUTIONS,
    /// After search, the model is infeasible.
    PROBLEM_INFEASIBLE
  };

  /// Optimization directions.
  enum OptimizationDirection { NOT_SET, MAXIMIZATION, MINIMIZATION };

#ifndef SWIG
  /// Search monitor events.
  enum class MonitorEvent : int {
    kEnterSearch = 0,
    kRestartSearch,
    kExitSearch,
    kBeginNextDecision,
    kEndNextDecision,
    kApplyDecision,
    kRefuteDecision,
    kAfterDecision,
    kBeginFail,
    kEndFail,
    kBeginInitialPropagation,
    kEndInitialPropagation,
    kAcceptSolution,
    kAtSolution,
    kNoMoreSolutions,
    kLocalOptimum,
    kAcceptDelta,
    kAcceptNeighbor,
    kAcceptUncheckedNeighbor,
    kIsUncheckedSolutionLimitReached,
    kPeriodicCheck,
    kProgressPercent,
    kAccept,
    // Dummy event whose underlying int is the number of MonitorEvent enums.
    kLast,
  };
#endif  // SWIG

  /// Callback typedefs
  typedef std::function<int64_t(int64_t)> IndexEvaluator1;
  typedef std::function<int64_t(int64_t, int64_t)> IndexEvaluator2;
  typedef std::function<int64_t(int64_t, int64_t, int64_t)> IndexEvaluator3;

  typedef std::function<bool(int64_t)> IndexFilter1;

  typedef std::function<IntVar*(int64_t)> Int64ToIntVar;

  typedef std::function<int64_t(Solver* solver,
                                const std::vector<IntVar*>& vars,
                                int64_t first_unbound, int64_t last_unbound)>
      VariableIndexSelector;

  typedef std::function<int64_t(const IntVar* v, int64_t id)>
      VariableValueSelector;
  typedef std::function<bool(int64_t, int64_t, int64_t)>
      VariableValueComparator;
  typedef std::function<DecisionModification()> BranchSelector;
  // TODO(user): wrap in swig.
  typedef std::function<void(Solver*)> Action;
  typedef std::function<void()> Closure;

  /// Solver API
  explicit Solver(const std::string& name);
  Solver(const std::string& name, const ConstraintSolverParameters& parameters);
  ~Solver();

  /// Stored Parameters.
  ConstraintSolverParameters parameters() const { return parameters_; }
  // Read-only.
  const ConstraintSolverParameters& const_parameters() const {
    return parameters_;
  }
  /// Create a ConstraintSolverParameters proto with all the default values.
  // TODO(user): Move to constraint_solver_parameters.h.
  static ConstraintSolverParameters DefaultSolverParameters();

  /// reversibility

  /// SaveValue() saves the value of the corresponding object. It must be
  /// called before modifying the object. The value will be restored upon
  /// backtrack.
  template <class T>
  void SaveValue(T* o) {
    InternalSaveValue(o);
  }

  /// Registers the given object as being reversible. By calling this method,
  /// the caller gives ownership of the object to the solver, which will
  /// delete it when there is a backtrack out of the current state.
  ///
  /// Returns the argument for convenience: this way, the caller may directly
  /// invoke a constructor in the argument, without having to store the pointer
  /// first.
  ///
  /// This function is only for users that define their own subclasses of
  /// BaseObject: for all subclasses predefined in the library, the
  /// corresponding factory methods (e.g., MakeIntVar(...),
  /// MakeAllDifferent(...) already take care of the registration.
  template <typename T>
  T* RevAlloc(T* object) {
    return reinterpret_cast<T*>(SafeRevAlloc(object));
  }

  /// Like RevAlloc() above, but for an array of objects: the array
  /// must have been allocated with the new[] operator. The entire array
  /// will be deleted when backtracking out of the current state.
  ///
  /// This method is valid for arrays of int, int64_t, uint64_t, bool,
  /// BaseObject*, IntVar*, IntExpr*, and Constraint*.
  template <typename T>
  T* RevAllocArray(T* object) {
    return reinterpret_cast<T*>(SafeRevAllocArray(object));
  }

  /// Adds the constraint 'c' to the model.
  ///
  /// After calling this method, and until there is a backtrack that undoes the
  /// addition, any assignment of variables to values must satisfy the given
  /// constraint in order to be considered feasible. There are two fairly
  /// different use cases:
  ///
  /// - the most common use case is modeling: the given constraint is really
  /// part of the problem that the user is trying to solve. In this use case,
  /// AddConstraint is called outside of search (i.e., with <tt>state() ==
  /// OUTSIDE_SEARCH</tt>). Most users should only use AddConstraint in this
  /// way. In this case, the constraint will belong to the model forever: it
  /// cannot not be removed by backtracking.
  ///
  /// - a rarer use case is that 'c' is not a real constraint of the model. It
  /// may be a constraint generated by a branching decision (a constraint whose
  /// goal is to restrict the search space), a symmetry breaking constraint (a
  /// constraint that does restrict the search space, but in a way that cannot
  /// have an impact on the quality of the solutions in the subtree), or an
  /// inferred constraint that, while having no semantic value to the model (it
  /// does not restrict the set of solutions), is worth having because we
  /// believe it may strengthen the propagation. In these cases, it happens
  /// that the constraint is added during the search (i.e., with state() ==
  /// IN_SEARCH or state() == IN_ROOT_NODE). When a constraint is
  /// added during a search, it applies only to the subtree of the search tree
  /// rooted at the current node, and will be automatically removed by
  /// backtracking.
  ///
  /// This method does not take ownership of the constraint. If the constraint
  /// has been created by any factory method (Solver::MakeXXX), it will
  /// automatically be deleted. However, power users who implement their own
  /// constraints should do: solver.AddConstraint(solver.RevAlloc(new
  /// MyConstraint(...));
  void AddConstraint(Constraint* const c);
  /// Adds 'constraint' to the solver and marks it as a cast constraint, that
  /// is, a constraint created calling Var() on an expression. This is used
  /// internally.
  void AddCastConstraint(CastConstraint* const constraint,
                         IntVar* const target_var, IntExpr* const expr);

  /// @{
  /// Solves the problem using the given DecisionBuilder and returns true if a
  /// solution was found and accepted.
  ///
  /// These methods are the ones most users should use to search for a solution.
  /// Note that the definition of 'solution' is subtle. A solution here is
  /// defined as a leaf of the search tree with respect to the given decision
  /// builder for which there is no failure. What this means is that, contrary
  /// to intuition, a solution may not have all variables of the model bound.
  /// It is the responsibility of the decision builder to keep returning
  /// decisions until all variables are indeed bound. The most extreme
  /// counterexample is calling Solve with a trivial decision builder whose
  /// Next() method always returns nullptr. In this case, Solve immediately
  /// returns 'true', since not assigning any variable to any value is a
  /// solution, unless the root node propagation discovers that the model is
  /// infeasible.
  ///
  /// This function must be called either from outside of search,
  /// or from within the Next() method of a decision builder.
  ///
  /// Solve will terminate whenever any of the following event arise:
  /// * A search monitor asks the solver to terminate the search by calling
  ///   solver()->FinishCurrentSearch().
  /// * A solution is found that is accepted by all search monitors, and none of
  ///   the search monitors decides to search for another one.
  ///
  /// Upon search termination, there will be a series of backtracks all the way
  /// to the top level. This means that a user cannot expect to inspect the
  /// solution by querying variables after a call to Solve(): all the
  /// information will be lost. In order to do something with the solution, the
  /// user must either:
  ///
  /// * Use a search monitor that can process such a leaf. See, in particular,
  ///     the SolutionCollector class.
  /// * Do not use Solve. Instead, use the more fine-grained approach using
  ///     methods NewSearch(...), NextSolution(), and EndSearch().
  ///
  /// @param db The decision builder that will generate the search tree.
  /// @param monitors A vector of search monitors that will be notified of
  /// various events during the search. In their reaction to these events, such
  /// monitors may influence the search.
  bool Solve(DecisionBuilder* const db,
             const std::vector<SearchMonitor*>& monitors);
  bool Solve(DecisionBuilder* const db);
  bool Solve(DecisionBuilder* const db, SearchMonitor* const m1);
  bool Solve(DecisionBuilder* const db, SearchMonitor* const m1,
             SearchMonitor* const m2);
  bool Solve(DecisionBuilder* const db, SearchMonitor* const m1,
             SearchMonitor* const m2, SearchMonitor* const m3);
  bool Solve(DecisionBuilder* const db, SearchMonitor* const m1,
             SearchMonitor* const m2, SearchMonitor* const m3,
             SearchMonitor* const m4);
  /// @}

  /// @{
  /// Decomposed search.
  /// The code for a top level search should look like
  /// solver->NewSearch(db);
  /// while (solver->NextSolution()) {
  ///   //.. use the current solution
  /// }
  /// solver()->EndSearch();

  void NewSearch(DecisionBuilder* const db,
                 const std::vector<SearchMonitor*>& monitors);
  void NewSearch(DecisionBuilder* const db);
  void NewSearch(DecisionBuilder* const db, SearchMonitor* const m1);
  void NewSearch(DecisionBuilder* const db, SearchMonitor* const m1,
                 SearchMonitor* const m2);
  void NewSearch(DecisionBuilder* const db, SearchMonitor* const m1,
                 SearchMonitor* const m2, SearchMonitor* const m3);
  void NewSearch(DecisionBuilder* const db, SearchMonitor* const m1,
                 SearchMonitor* const m2, SearchMonitor* const m3,
                 SearchMonitor* const m4);

  bool NextSolution();
  void RestartSearch();
  void EndSearch();
  /// @}

  /// SolveAndCommit using a decision builder and up to three
  ///   search monitors, usually one for the objective, one for the limits
  ///   and one to collect solutions.
  ///
  /// The difference between a SolveAndCommit() and a Solve() method
  /// call is the fact that SolveAndCommit will not backtrack all
  /// modifications at the end of the search. This method is only
  /// usable during the Next() method of a decision builder.
  bool SolveAndCommit(DecisionBuilder* const db,
                      const std::vector<SearchMonitor*>& monitors);
  bool SolveAndCommit(DecisionBuilder* const db);
  bool SolveAndCommit(DecisionBuilder* const db, SearchMonitor* const m1);
  bool SolveAndCommit(DecisionBuilder* const db, SearchMonitor* const m1,
                      SearchMonitor* const m2);
  bool SolveAndCommit(DecisionBuilder* const db, SearchMonitor* const m1,
                      SearchMonitor* const m2, SearchMonitor* const m3);

  /// Checks whether the given assignment satisfies all relevant constraints.
  bool CheckAssignment(Assignment* const solution);

  /// Checks whether adding this constraint will lead to an immediate
  /// failure. It will return false if the model is already inconsistent, or if
  /// adding the constraint makes it inconsistent.
  bool CheckConstraint(Constraint* const ct);

  /// State of the solver.
  SolverState state() const { return state_; }

  /// Abandon the current branch in the search tree. A backtrack will follow.
  void Fail();

#if !defined(SWIG)
  /// When SaveValue() is not the best way to go, one can create a reversible
  /// action that will be called upon backtrack. The "fast" parameter
  /// indicates whether we need restore all values saved through SaveValue()
  /// before calling this method.
  void AddBacktrackAction(Action a, bool fast);
#endif  /// !defined(SWIG)

  /// misc debug string.
  std::string DebugString() const;

  /// Current memory usage in bytes
  static int64_t MemoryUsage();

  /// The 'absolute time' as seen by the solver. Unless a user-provided clock
  /// was injected via SetClock() (eg. for unit tests), this is a real walltime,
  /// shifted so that it was 0 at construction. All so-called "walltime" limits
  /// are relative to this time.
  absl::Time Now() const;

  /// DEPRECATED: Use Now() instead.
  /// Time elapsed, in ms since the creation of the solver.
  int64_t wall_time() const;

  /// The number of branches explored since the creation of the solver.
  int64_t branches() const { return branches_; }

  /// The number of solutions found since the start of the search.
  int64_t solutions() const;

  /// The number of unchecked solutions found by local search.
  int64_t unchecked_solutions() const;

  /// The number of demons executed during search for a given priority.
  int64_t demon_runs(DemonPriority p) const { return demon_runs_[p]; }

  /// The number of failures encountered since the creation of the solver.
  int64_t failures() const { return fails_; }

  /// The number of neighbors created.
  int64_t neighbors() const { return neighbors_; }

  /// The number of filtered neighbors (neighbors accepted by filters).
  int64_t filtered_neighbors() const { return filtered_neighbors_; }

  /// The number of accepted neighbors.
  int64_t accepted_neighbors() const { return accepted_neighbors_; }

  /// The stamp indicates how many moves in the search tree we have performed.
  /// It is useful to detect if we need to update same lazy structures.
  uint64_t stamp() const;

  /// The fail_stamp() is incremented after each backtrack.
  uint64_t fail_stamp() const;

  /// Sets the current context of the search.
  void set_context(const std::string& context) { context_ = context; }

  /// Gets the current context of the search.
  const std::string& context() const { return context_; }

  /// The direction of optimization, getter and setter.
  OptimizationDirection optimization_direction() const {
    return optimization_direction_;
  }
  void set_optimization_direction(OptimizationDirection direction) {
    optimization_direction_ = direction;
  }

  // All factories (MakeXXX methods) encapsulate creation of objects
  // through RevAlloc(). Hence, the Solver used for allocating the
  // returned object will retain ownership of the allocated memory.
  // Destructors are called upon backtrack, or when the Solver is
  // itself destructed.

  // ----- Int Variables and Constants -----

  /// MakeIntVar will create the best range based int var for the bounds given.
  IntVar* MakeIntVar(int64_t min, int64_t max, const std::string& name);

  /// MakeIntVar will create a variable with the given sparse domain.
  IntVar* MakeIntVar(const std::vector<int64_t>& values,
                     const std::string& name);

  /// MakeIntVar will create a variable with the given sparse domain.
  IntVar* MakeIntVar(const std::vector<int>& values, const std::string& name);

  /// MakeIntVar will create the best range based int var for the bounds given.
  IntVar* MakeIntVar(int64_t min, int64_t max);

  /// MakeIntVar will create a variable with the given sparse domain.
  IntVar* MakeIntVar(const std::vector<int64_t>& values);

  /// MakeIntVar will create a variable with the given sparse domain.
  IntVar* MakeIntVar(const std::vector<int>& values);

  /// MakeBoolVar will create a variable with a {0, 1} domain.
  IntVar* MakeBoolVar(const std::string& name);

  /// MakeBoolVar will create a variable with a {0, 1} domain.
  IntVar* MakeBoolVar();

  /// IntConst will create a constant expression.
  IntVar* MakeIntConst(int64_t val, const std::string& name);

  /// IntConst will create a constant expression.
  IntVar* MakeIntConst(int64_t val);

  /// This method will append the vector vars with 'var_count' variables
  /// having bounds vmin and vmax and having name "name<i>" where <i> is
  /// the index of the variable.
  void MakeIntVarArray(int var_count, int64_t vmin, int64_t vmax,
                       const std::string& name, std::vector<IntVar*>* vars);
  /// This method will append the vector vars with 'var_count' variables
  /// having bounds vmin and vmax and having no names.
  void MakeIntVarArray(int var_count, int64_t vmin, int64_t vmax,
                       std::vector<IntVar*>* vars);
  /// Same but allocates an array and returns it.
  IntVar** MakeIntVarArray(int var_count, int64_t vmin, int64_t vmax,
                           const std::string& name);

  /// This method will append the vector vars with 'var_count' boolean
  /// variables having name "name<i>" where <i> is the index of the
  /// variable.
  void MakeBoolVarArray(int var_count, const std::string& name,
                        std::vector<IntVar*>* vars);
  /// This method will append the vector vars with 'var_count' boolean
  /// variables having no names.
  void MakeBoolVarArray(int var_count, std::vector<IntVar*>* vars);
  /// Same but allocates an array and returns it.
  IntVar** MakeBoolVarArray(int var_count, const std::string& name);

  // ----- Integer Expressions -----

  /// left + right.
  IntExpr* MakeSum(IntExpr* const left, IntExpr* const right);
  /// expr + value.
  IntExpr* MakeSum(IntExpr* const expr, int64_t value);
  /// sum of all vars.
  IntExpr* MakeSum(const std::vector<IntVar*>& vars);

  /// scalar product
  IntExpr* MakeScalProd(const std::vector<IntVar*>& vars,
                        const std::vector<int64_t>& coefs);
  /// scalar product
  IntExpr* MakeScalProd(const std::vector<IntVar*>& vars,
                        const std::vector<int>& coefs);

  /// left - right
  IntExpr* MakeDifference(IntExpr* const left, IntExpr* const right);
  /// value - expr
  IntExpr* MakeDifference(int64_t value, IntExpr* const expr);
  /// -expr
  IntExpr* MakeOpposite(IntExpr* const expr);

  /// left * right
  IntExpr* MakeProd(IntExpr* const left, IntExpr* const right);
  /// expr * value
  IntExpr* MakeProd(IntExpr* const expr, int64_t value);

  /// expr / value (integer division)
  IntExpr* MakeDiv(IntExpr* const expr, int64_t value);
  /// numerator / denominator (integer division). Terms need to be positive.
  IntExpr* MakeDiv(IntExpr* const numerator, IntExpr* const denominator);

  /// |expr|
  IntExpr* MakeAbs(IntExpr* const expr);
  /// expr * expr
  IntExpr* MakeSquare(IntExpr* const expr);
  /// expr ^ n (n > 0)
  IntExpr* MakePower(IntExpr* const expr, int64_t n);

  /// values[index]
  IntExpr* MakeElement(const std::vector<int64_t>& values, IntVar* const index);
  /// values[index]
  IntExpr* MakeElement(const std::vector<int>& values, IntVar* const index);

  /// Function-based element. The constraint takes ownership of the
  /// callback. The callback must be able to cope with any possible
  /// value in the domain of 'index' (potentially negative ones too).
  IntExpr* MakeElement(IndexEvaluator1 values, IntVar* const index);
  /// Function based element. The constraint takes ownership of the
  /// callback.  The callback must be monotonic. It must be able to
  /// cope with any possible value in the domain of 'index'
  /// (potentially negative ones too). Furtermore, monotonicity is not
  /// checked. Thus giving a non-monotonic function, or specifying an
  /// incorrect increasing parameter will result in undefined behavior.
  IntExpr* MakeMonotonicElement(IndexEvaluator1 values, bool increasing,
                                IntVar* const index);
  /// 2D version of function-based element expression, values(expr1, expr2).
  IntExpr* MakeElement(IndexEvaluator2 values, IntVar* const index1,
                       IntVar* const index2);

  /// vars[expr]
  IntExpr* MakeElement(const std::vector<IntVar*>& vars, IntVar* const index);

#if !defined(SWIG)
  /// vars(argument)
  IntExpr* MakeElement(Int64ToIntVar vars, int64_t range_start,
                       int64_t range_end, IntVar* argument);
#endif  // SWIG

  /// Light versions of function-based elements, in constraint version only,
  /// well-suited for use within Local Search.
  /// These constraints are "checking" constraints, only triggered on WhenBound
  /// events. They provide very little (or no) domain filtering.

  /// Returns a light one-dimension function-based element constraint ensuring
  /// var == values(index).
  /// The constraint does not perform bound reduction of the resulting variable
  /// until the index variable is bound.
  /// If deep_serialize returns false, the model visitor will not extract all
  /// possible values from the values function.
  template <typename F>
  Constraint* MakeLightElement(F values, IntVar* const var, IntVar* const index,
                               std::function<bool()> deep_serialize = nullptr) {
    return RevAlloc(new LightIntFunctionElementCt<F>(
        this, var, index, std::move(values), std::move(deep_serialize)));
  }

  /// Light two-dimension function-based element constraint ensuring
  /// var == values(index1, index2).
  /// The constraint does not perform bound reduction of the resulting variable
  /// until the index variables are bound.
  /// If deep_serialize returns false, the model visitor will not extract all
  /// possible values from the values function.
  template <typename F>
  Constraint* MakeLightElement(F values, IntVar* const var,
                               IntVar* const index1, IntVar* const index2,
                               std::function<bool()> deep_serialize = nullptr) {
    return RevAlloc(new LightIntIntFunctionElementCt<F>(
        this, var, index1, index2, std::move(values),
        std::move(deep_serialize)));
  }

  /// Returns the expression expr such that vars[expr] == value.
  /// It assumes that vars are all different.
  IntExpr* MakeIndexExpression(const std::vector<IntVar*>& vars, int64_t value);

  /// Special cases with arrays of size two.
  Constraint* MakeIfThenElseCt(IntVar* const condition,
                               IntExpr* const then_expr,
                               IntExpr* const else_expr,
                               IntVar* const target_var);

  /// std::min(vars)
  IntExpr* MakeMin(const std::vector<IntVar*>& vars);
  /// std::min (left, right)
  IntExpr* MakeMin(IntExpr* const left, IntExpr* const right);
  /// std::min(expr, value)
  IntExpr* MakeMin(IntExpr* const expr, int64_t value);
  /// std::min(expr, value)
  IntExpr* MakeMin(IntExpr* const expr, int value);

  /// std::max(vars)
  IntExpr* MakeMax(const std::vector<IntVar*>& vars);
  /// std::max(left, right)
  IntExpr* MakeMax(IntExpr* const left, IntExpr* const right);
  /// std::max(expr, value)
  IntExpr* MakeMax(IntExpr* const expr, int64_t value);
  /// std::max(expr, value)
  IntExpr* MakeMax(IntExpr* const expr, int value);

  /// Convex piecewise function.
  IntExpr* MakeConvexPiecewiseExpr(IntExpr* expr, int64_t early_cost,
                                   int64_t early_date, int64_t late_date,
                                   int64_t late_cost);

  /// Semi continuous Expression (x <= 0 -> f(x) = 0; x > 0 -> f(x) = ax + b)
  /// a >= 0 and b >= 0
  IntExpr* MakeSemiContinuousExpr(IntExpr* const expr, int64_t fixed_charge,
                                  int64_t step);

  /// General piecewise-linear function expression, built from f(x) where f is
  /// piecewise-linear. The resulting expression is f(expr).
  // TODO(user): Investigate if we can merge all three piecewise linear
  /// expressions.
#ifndef SWIG
  IntExpr* MakePiecewiseLinearExpr(IntExpr* expr,
                                   const PiecewiseLinearFunction& f);
#endif

  /// Modulo expression x % mod (with the python convention for modulo).
  IntExpr* MakeModulo(IntExpr* const x, int64_t mod);

  /// Modulo expression x % mod (with the python convention for modulo).
  IntExpr* MakeModulo(IntExpr* const x, IntExpr* const mod);

  /// Conditional Expr condition ? expr : unperformed_value
  IntExpr* MakeConditionalExpression(IntVar* const condition,
                                     IntExpr* const expr,
                                     int64_t unperformed_value);

  /// This constraint always succeeds.
  Constraint* MakeTrueConstraint();
  /// This constraint always fails.
  Constraint* MakeFalseConstraint();
  Constraint* MakeFalseConstraint(const std::string& explanation);

  /// boolvar == (var == value)
  Constraint* MakeIsEqualCstCt(IntExpr* const var, int64_t value,
                               IntVar* const boolvar);
  /// status var of (var == value)
  IntVar* MakeIsEqualCstVar(IntExpr* const var, int64_t value);
  /// b == (v1 == v2)
  Constraint* MakeIsEqualCt(IntExpr* const v1, IntExpr* v2, IntVar* const b);
  /// status var of (v1 == v2)
  IntVar* MakeIsEqualVar(IntExpr* const v1, IntExpr* v2);
  /// left == right
  Constraint* MakeEquality(IntExpr* const left, IntExpr* const right);
  /// expr == value
  Constraint* MakeEquality(IntExpr* const expr, int64_t value);
  /// expr == value
  Constraint* MakeEquality(IntExpr* const expr, int value);

  /// boolvar == (var != value)
  Constraint* MakeIsDifferentCstCt(IntExpr* const var, int64_t value,
                                   IntVar* const boolvar);
  /// status var of (var != value)
  IntVar* MakeIsDifferentCstVar(IntExpr* const var, int64_t value);
  /// status var of (v1 != v2)
  IntVar* MakeIsDifferentVar(IntExpr* const v1, IntExpr* const v2);
  /// b == (v1 != v2)
  Constraint* MakeIsDifferentCt(IntExpr* const v1, IntExpr* const v2,
                                IntVar* const b);
  /// left != right
  Constraint* MakeNonEquality(IntExpr* const left, IntExpr* const right);
  /// expr != value
  Constraint* MakeNonEquality(IntExpr* const expr, int64_t value);
  /// expr != value
  Constraint* MakeNonEquality(IntExpr* const expr, int value);

  /// boolvar == (var <= value)
  Constraint* MakeIsLessOrEqualCstCt(IntExpr* const var, int64_t value,
                                     IntVar* const boolvar);
  /// status var of (var <= value)
  IntVar* MakeIsLessOrEqualCstVar(IntExpr* const var, int64_t value);
  /// status var of (left <= right)
  IntVar* MakeIsLessOrEqualVar(IntExpr* const left, IntExpr* const right);
  /// b == (left <= right)
  Constraint* MakeIsLessOrEqualCt(IntExpr* const left, IntExpr* const right,
                                  IntVar* const b);
  /// left <= right
  Constraint* MakeLessOrEqual(IntExpr* const left, IntExpr* const right);
  /// expr <= value
  Constraint* MakeLessOrEqual(IntExpr* const expr, int64_t value);
  /// expr <= value
  Constraint* MakeLessOrEqual(IntExpr* const expr, int value);

  /// boolvar == (var >= value)
  Constraint* MakeIsGreaterOrEqualCstCt(IntExpr* const var, int64_t value,
                                        IntVar* const boolvar);
  /// status var of (var >= value)
  IntVar* MakeIsGreaterOrEqualCstVar(IntExpr* const var, int64_t value);
  /// status var of (left >= right)
  IntVar* MakeIsGreaterOrEqualVar(IntExpr* const left, IntExpr* const right);
  /// b == (left >= right)
  Constraint* MakeIsGreaterOrEqualCt(IntExpr* const left, IntExpr* const right,
                                     IntVar* const b);
  /// left >= right
  Constraint* MakeGreaterOrEqual(IntExpr* const left, IntExpr* const right);
  /// expr >= value
  Constraint* MakeGreaterOrEqual(IntExpr* const expr, int64_t value);
  /// expr >= value
  Constraint* MakeGreaterOrEqual(IntExpr* const expr, int value);

  /// b == (v > c)
  Constraint* MakeIsGreaterCstCt(IntExpr* const v, int64_t c, IntVar* const b);
  /// status var of (var > value)
  IntVar* MakeIsGreaterCstVar(IntExpr* const var, int64_t value);
  /// status var of (left > right)
  IntVar* MakeIsGreaterVar(IntExpr* const left, IntExpr* const right);
  /// b == (left > right)
  Constraint* MakeIsGreaterCt(IntExpr* const left, IntExpr* const right,
                              IntVar* const b);
  /// left > right
  Constraint* MakeGreater(IntExpr* const left, IntExpr* const right);
  /// expr > value
  Constraint* MakeGreater(IntExpr* const expr, int64_t value);
  /// expr > value
  Constraint* MakeGreater(IntExpr* const expr, int value);

  /// b == (v < c)
  Constraint* MakeIsLessCstCt(IntExpr* const v, int64_t c, IntVar* const b);
  /// status var of (var < value)
  IntVar* MakeIsLessCstVar(IntExpr* const var, int64_t value);
  /// status var of (left < right)
  IntVar* MakeIsLessVar(IntExpr* const left, IntExpr* const right);
  /// b == (left < right)
  Constraint* MakeIsLessCt(IntExpr* const left, IntExpr* const right,
                           IntVar* const b);
  /// left < right
  Constraint* MakeLess(IntExpr* const left, IntExpr* const right);
  /// expr < value
  Constraint* MakeLess(IntExpr* const expr, int64_t value);
  /// expr < value
  Constraint* MakeLess(IntExpr* const expr, int value);

  /// Variation on arrays.
  Constraint* MakeSumLessOrEqual(const std::vector<IntVar*>& vars, int64_t cst);
  Constraint* MakeSumGreaterOrEqual(const std::vector<IntVar*>& vars,
                                    int64_t cst);
  Constraint* MakeSumEquality(const std::vector<IntVar*>& vars, int64_t cst);
  Constraint* MakeSumEquality(const std::vector<IntVar*>& vars,
                              IntVar* const var);
  Constraint* MakeScalProdEquality(const std::vector<IntVar*>& vars,
                                   const std::vector<int64_t>& coefficients,
                                   int64_t cst);
  Constraint* MakeScalProdEquality(const std::vector<IntVar*>& vars,
                                   const std::vector<int>& coefficients,
                                   int64_t cst);
  Constraint* MakeScalProdEquality(const std::vector<IntVar*>& vars,
                                   const std::vector<int64_t>& coefficients,
                                   IntVar* const target);
  Constraint* MakeScalProdEquality(const std::vector<IntVar*>& vars,
                                   const std::vector<int>& coefficients,
                                   IntVar* const target);
  Constraint* MakeScalProdGreaterOrEqual(const std::vector<IntVar*>& vars,
                                         const std::vector<int64_t>& coeffs,
                                         int64_t cst);
  Constraint* MakeScalProdGreaterOrEqual(const std::vector<IntVar*>& vars,
                                         const std::vector<int>& coeffs,
                                         int64_t cst);
  Constraint* MakeScalProdLessOrEqual(const std::vector<IntVar*>& vars,
                                      const std::vector<int64_t>& coefficients,
                                      int64_t cst);
  Constraint* MakeScalProdLessOrEqual(const std::vector<IntVar*>& vars,
                                      const std::vector<int>& coefficients,
                                      int64_t cst);

  Constraint* MakeMinEquality(const std::vector<IntVar*>& vars,
                              IntVar* const min_var);
  Constraint* MakeMaxEquality(const std::vector<IntVar*>& vars,
                              IntVar* const max_var);

  Constraint* MakeElementEquality(const std::vector<int64_t>& vals,
                                  IntVar* const index, IntVar* const target);
  Constraint* MakeElementEquality(const std::vector<int>& vals,
                                  IntVar* const index, IntVar* const target);
  Constraint* MakeElementEquality(const std::vector<IntVar*>& vars,
                                  IntVar* const index, IntVar* const target);
  Constraint* MakeElementEquality(const std::vector<IntVar*>& vars,
                                  IntVar* const index, int64_t target);
  /// Creates the constraint abs(var) == abs_var.
  Constraint* MakeAbsEquality(IntVar* const var, IntVar* const abs_var);
  /// This constraint is a special case of the element constraint with
  /// an array of integer variables, where the variables are all
  /// different and the index variable is constrained such that
  /// vars[index] == target.
  Constraint* MakeIndexOfConstraint(const std::vector<IntVar*>& vars,
                                    IntVar* const index, int64_t target);

  /// This method is a specialized case of the MakeConstraintDemon
  /// method to call the InitiatePropagate of the constraint 'ct'.
  Demon* MakeConstraintInitialPropagateCallback(Constraint* const ct);
  /// This method is a specialized case of the MakeConstraintDemon
  /// method to call the InitiatePropagate of the constraint 'ct' with
  /// low priority.
  Demon* MakeDelayedConstraintInitialPropagateCallback(Constraint* const ct);
#if !defined(SWIG)
  /// Creates a demon from a callback.
  Demon* MakeActionDemon(Action action);
#endif  /// !defined(SWIG)
  /// Creates a demon from a closure.
  Demon* MakeClosureDemon(Closure closure);

  // ----- Between and related constraints -----

  /// (l <= expr <= u)
  Constraint* MakeBetweenCt(IntExpr* const expr, int64_t l, int64_t u);

  /// (expr < l || expr > u)
  /// This constraint is lazy as it will not make holes in the domain of
  /// variables. It will propagate only when expr->Min() >= l
  /// or expr->Max() <= u.
  Constraint* MakeNotBetweenCt(IntExpr* const expr, int64_t l, int64_t u);

  /// b == (l <= expr <= u)
  Constraint* MakeIsBetweenCt(IntExpr* const expr, int64_t l, int64_t u,
                              IntVar* const b);
  IntVar* MakeIsBetweenVar(IntExpr* const v, int64_t l, int64_t u);

  // ----- Member and related constraints -----

  /// expr in set. Propagation is lazy, i.e. this constraint does not
  /// creates holes in the domain of the variable.
  Constraint* MakeMemberCt(IntExpr* const expr,
                           const std::vector<int64_t>& values);
  Constraint* MakeMemberCt(IntExpr* const expr, const std::vector<int>& values);

  /// expr not in set.
  Constraint* MakeNotMemberCt(IntExpr* const expr,
                              const std::vector<int64_t>& values);
  Constraint* MakeNotMemberCt(IntExpr* const expr,
                              const std::vector<int>& values);

  /// expr should not be in the list of forbidden intervals [start[i]..end[i]].
  Constraint* MakeNotMemberCt(IntExpr* const expr, std::vector<int64_t> starts,
                              std::vector<int64_t> ends);
  /// expr should not be in the list of forbidden intervals [start[i]..end[i]].
  Constraint* MakeNotMemberCt(IntExpr* const expr, std::vector<int> starts,
                              std::vector<int> ends);
#if !defined(SWIG)
  /// expr should not be in the list of forbidden intervals.
  Constraint* MakeNotMemberCt(IntExpr* expr,
                              SortedDisjointIntervalList intervals);
#endif  // !defined(SWIG)

  /// boolvar == (expr in set)
  Constraint* MakeIsMemberCt(IntExpr* const expr,
                             const std::vector<int64_t>& values,
                             IntVar* const boolvar);
  Constraint* MakeIsMemberCt(IntExpr* const expr,
                             const std::vector<int>& values,
                             IntVar* const boolvar);
  IntVar* MakeIsMemberVar(IntExpr* const expr,
                          const std::vector<int64_t>& values);
  IntVar* MakeIsMemberVar(IntExpr* const expr, const std::vector<int>& values);

  /// |{i | vars[i] == value}| <= max_count
  Constraint* MakeAtMost(std::vector<IntVar*> vars, int64_t value,
                         int64_t max_count);
  /// |{i | vars[i] == value}| == max_count
  Constraint* MakeCount(const std::vector<IntVar*>& vars, int64_t value,
                        int64_t max_count);
  /// |{i | vars[i] == value}| == max_count
  Constraint* MakeCount(const std::vector<IntVar*>& vars, int64_t value,
                        IntVar* const max_count);

  /// Aggregated version of count:  |{i | v[i] == values[j]}| == cards[j]
  Constraint* MakeDistribute(const std::vector<IntVar*>& vars,
                             const std::vector<int64_t>& values,
                             const std::vector<IntVar*>& cards);
  /// Aggregated version of count:  |{i | v[i] == values[j]}| == cards[j]
  Constraint* MakeDistribute(const std::vector<IntVar*>& vars,
                             const std::vector<int>& values,
                             const std::vector<IntVar*>& cards);
  /// Aggregated version of count:  |{i | v[i] == j}| == cards[j]
  Constraint* MakeDistribute(const std::vector<IntVar*>& vars,
                             const std::vector<IntVar*>& cards);
  /// Aggregated version of count with bounded cardinalities:
  /// forall j in 0 .. card_size - 1: card_min <= |{i | v[i] == j}| <= card_max
  Constraint* MakeDistribute(const std::vector<IntVar*>& vars, int64_t card_min,
                             int64_t card_max, int64_t card_size);
  /// Aggregated version of count with bounded cardinalities:
  /// forall j in 0 .. card_size - 1:
  ///    card_min[j] <= |{i | v[i] == j}| <= card_max[j]
  Constraint* MakeDistribute(const std::vector<IntVar*>& vars,
                             const std::vector<int64_t>& card_min,
                             const std::vector<int64_t>& card_max);
  /// Aggregated version of count with bounded cardinalities:
  /// forall j in 0 .. card_size - 1:
  ///    card_min[j] <= |{i | v[i] == j}| <= card_max[j]
  Constraint* MakeDistribute(const std::vector<IntVar*>& vars,
                             const std::vector<int>& card_min,
                             const std::vector<int>& card_max);
  /// Aggregated version of count with bounded cardinalities:
  /// forall j in 0 .. card_size - 1:
  ///    card_min[j] <= |{i | v[i] == values[j]}| <= card_max[j]
  Constraint* MakeDistribute(const std::vector<IntVar*>& vars,
                             const std::vector<int64_t>& values,
                             const std::vector<int64_t>& card_min,
                             const std::vector<int64_t>& card_max);
  /// Aggregated version of count with bounded cardinalities:
  /// forall j in 0 .. card_size - 1:
  ///    card_min[j] <= |{i | v[i] == values[j]}| <= card_max[j]
  Constraint* MakeDistribute(const std::vector<IntVar*>& vars,
                             const std::vector<int>& values,
                             const std::vector<int>& card_min,
                             const std::vector<int>& card_max);

  /// Deviation constraint:
  /// sum_i |n * vars[i] - total_sum| <= deviation_var and
  /// sum_i vars[i] == total_sum
  /// n = #vars
  Constraint* MakeDeviation(const std::vector<IntVar*>& vars,
                            IntVar* const deviation_var, int64_t total_sum);

  /// All variables are pairwise different. This corresponds to the
  /// stronger version of the propagation algorithm.
  Constraint* MakeAllDifferent(const std::vector<IntVar*>& vars);

  /// All variables are pairwise different.  If 'stronger_propagation'
  /// is true, stronger, and potentially slower propagation will
  /// occur. This API will be deprecated in the future.
  Constraint* MakeAllDifferent(const std::vector<IntVar*>& vars,
                               bool stronger_propagation);

  /// All variables are pairwise different, unless they are assigned to
  /// the escape value.
  Constraint* MakeAllDifferentExcept(const std::vector<IntVar*>& vars,
                                     int64_t escape_value);
  // TODO(user): Do we need a version with an array of escape values.

  /// Creates a constraint binding the arrays of variables "vars" and
  /// "sorted_vars": sorted_vars[0] must be equal to the minimum of all
  /// variables in vars, and so on: the value of sorted_vars[i] must be
  /// equal to the i-th value of variables invars.
  ///
  /// This constraint propagates in both directions: from "vars" to
  /// "sorted_vars" and vice-versa.
  ///
  /// Behind the scenes, this constraint maintains that:
  ///   - sorted is always increasing.
  ///   - whatever the values of vars, there exists a permutation that
  ///     injects its values into the sorted variables.
  ///
  /// For more info, please have a look at:
  ///   https://mpi-inf.mpg.de/~mehlhorn/ftp/Mehlhorn-Thiel.pdf
  Constraint* MakeSortingConstraint(const std::vector<IntVar*>& vars,
                                    const std::vector<IntVar*>& sorted);
  // TODO(user): Add void MakeSortedArray(
  //                             const std::vector<IntVar*>& vars,
  //                             std::vector<IntVar*>* const sorted);

  /// Creates a constraint that enforces that left is lexicographically less
  /// than right.
  Constraint* MakeLexicalLess(const std::vector<IntVar*>& left,
                              const std::vector<IntVar*>& right);

  /// Creates a constraint that enforces that left is lexicographically less
  /// than or equal to right.
  Constraint* MakeLexicalLessOrEqual(const std::vector<IntVar*>& left,
                                     const std::vector<IntVar*>& right);

  /// Creates a constraint that enforces that 'left' and 'right' both
  /// represent permutations of [0..left.size()-1], and that 'right' is
  /// the inverse permutation of 'left', i.e. for all i in
  /// [0..left.size()-1], right[left[i]] = i.
  Constraint* MakeInversePermutationConstraint(
      const std::vector<IntVar*>& left, const std::vector<IntVar*>& right);

  /// Creates a constraint that binds the index variable to the index of the
  /// first variable with the maximum value.
  Constraint* MakeIndexOfFirstMaxValueConstraint(
      IntVar* index, const std::vector<IntVar*>& vars);

  /// Creates a constraint that binds the index variable to the index of the
  /// first variable with the minimum value.
  Constraint* MakeIndexOfFirstMinValueConstraint(
      IntVar* index, const std::vector<IntVar*>& vars);

  /// Creates a constraint that states that all variables in the first
  /// vector are different from all variables in the second
  /// group. Thus the set of values in the first vector does not
  /// intersect with the set of values in the second vector.
  Constraint* MakeNullIntersect(const std::vector<IntVar*>& first_vars,
                                const std::vector<IntVar*>& second_vars);

  /// Creates a constraint that states that all variables in the first
  /// vector are different from all variables from the second group,
  /// unless they are assigned to the escape value. Thus the set of
  /// values in the first vector minus the escape value does not
  /// intersect with the set of values in the second vector.
  Constraint* MakeNullIntersectExcept(const std::vector<IntVar*>& first_vars,
                                      const std::vector<IntVar*>& second_vars,
                                      int64_t escape_value);

  // TODO(user): Implement MakeAllNullIntersect taking an array of
  // variable vectors.

  /// Prevent cycles. The "nexts" variables represent the next in the chain.
  /// "active" variables indicate if the corresponding next variable is active;
  /// this could be useful to model unperformed nodes in a routing problem.
  /// A callback can be added to specify sink values (by default sink values
  /// are values >= vars.size()). Ownership of the callback is passed to the
  /// constraint.
  /// If assume_paths is either not specified or true, the constraint assumes
  /// the "nexts" variables represent paths (and performs a faster propagation);
  /// otherwise the constraint assumes they represent a forest.
  Constraint* MakeNoCycle(const std::vector<IntVar*>& nexts,
                          const std::vector<IntVar*>& active,
                          IndexFilter1 sink_handler = nullptr);
  Constraint* MakeNoCycle(const std::vector<IntVar*>& nexts,
                          const std::vector<IntVar*>& active,
                          IndexFilter1 sink_handler, bool assume_paths);

  /// Force the "nexts" variable to create a complete Hamiltonian path.
  Constraint* MakeCircuit(const std::vector<IntVar*>& nexts);

  /// Force the "nexts" variable to create a complete Hamiltonian path
  /// for those that do not loop upon themselves.
  Constraint* MakeSubCircuit(const std::vector<IntVar*>& nexts);

  /// Creates a constraint which accumulates values along a path such that:
  /// cumuls[next[i]] = cumuls[i] + transits[i].
  /// Active variables indicate if the corresponding next variable is active;
  /// this could be useful to model unperformed nodes in a routing problem.
  Constraint* MakePathCumul(const std::vector<IntVar*>& nexts,
                            const std::vector<IntVar*>& active,
                            const std::vector<IntVar*>& cumuls,
                            const std::vector<IntVar*>& transits);
  /// Delayed version of the same constraint: propagation on the nexts variables
  /// is delayed until all constraints have propagated.
  // TODO(user): Merge with other path-cumuls constraints.
  Constraint* MakeDelayedPathCumul(const std::vector<IntVar*>& nexts,
                                   const std::vector<IntVar*>& active,
                                   const std::vector<IntVar*>& cumuls,
                                   const std::vector<IntVar*>& transits);
  /// Creates a constraint which accumulates values along a path such that:
  /// cumuls[next[i]] = cumuls[i] + transit_evaluator(i, next[i]).
  /// Active variables indicate if the corresponding next variable is active;
  /// this could be useful to model unperformed nodes in a routing problem.
  /// Ownership of transit_evaluator is taken and it must be a repeatable
  /// callback.
  Constraint* MakePathCumul(const std::vector<IntVar*>& nexts,
                            const std::vector<IntVar*>& active,
                            const std::vector<IntVar*>& cumuls,
                            IndexEvaluator2 transit_evaluator);

  /// Creates a constraint which accumulates values along a path such that:
  /// cumuls[next[i]] = cumuls[i] + transit_evaluator(i, next[i]) + slacks[i].
  /// Active variables indicate if the corresponding next variable is active;
  /// this could be useful to model unperformed nodes in a routing problem.
  /// Ownership of transit_evaluator is taken and it must be a repeatable
  /// callback.
  Constraint* MakePathCumul(const std::vector<IntVar*>& nexts,
                            const std::vector<IntVar*>& active,
                            const std::vector<IntVar*>& cumuls,
                            const std::vector<IntVar*>& slacks,
                            IndexEvaluator2 transit_evaluator);
  /// Constraint enforcing that status[i] is true iff there's a path defined on
  /// next variables from sources[i] to sinks[i].
  // TODO(user): Only does checking on WhenBound events on next variables.
  /// Check whether more propagation is needed.
  Constraint* MakePathConnected(std::vector<IntVar*> nexts,
                                std::vector<int64_t> sources,
                                std::vector<int64_t> sinks,
                                std::vector<IntVar*> status);
#ifndef SWIG
  /// Constraint enforcing, for each pair (i,j) in precedences, i to be before j
  /// in paths defined by next variables.
  // TODO(user): This constraint does not make holes in variable domains;
  /// the implementation can easily be modified to do that; evaluate the impact
  /// on models solved with local search.
  Constraint* MakePathPrecedenceConstraint(
      std::vector<IntVar*> nexts,
      const std::vector<std::pair<int, int>>& precedences);
  /// Same as MakePathPrecedenceConstraint but ensures precedence pairs on some
  /// paths follow a LIFO or FIFO order.
  /// LIFO order: given 2 pairs (a,b) and (c,d), if a is before c on the path
  /// then d must be before b or b must be before c.
  /// FIFO order: given 2 pairs (a,b) and (c,d), if a is before c on the path
  /// then b must be before d.
  /// LIFO (resp. FIFO) orders are enforced only on paths starting by indices in
  /// lifo_path_starts (resp. fifo_path_start).
  Constraint* MakePathPrecedenceConstraint(
      std::vector<IntVar*> nexts,
      const std::vector<std::pair<int, int>>& precedences,
      const std::vector<int>& lifo_path_starts,
      const std::vector<int>& fifo_path_starts);
  /// Same as MakePathPrecedenceConstraint but will force i to be before j if
  /// the sum of transits on the path from i to j is strictly positive.
  Constraint* MakePathTransitPrecedenceConstraint(
      std::vector<IntVar*> nexts, std::vector<IntVar*> transits,
      const std::vector<std::pair<int, int>>& precedences);
#endif  // !SWIG
  /// This constraint maps the domain of 'var' onto the array of
  /// variables 'actives'. That is
  /// for all i in [0 .. size - 1]: actives[i] == 1 <=> var->Contains(i);
  Constraint* MakeMapDomain(IntVar* const var,
                            const std::vector<IntVar*>& actives);

  /// This method creates a constraint where the graph of the relation
  /// between the variables is given in extension. There are 'arity'
  /// variables involved in the relation and the graph is given by a
  /// integer tuple set.
  Constraint* MakeAllowedAssignments(const std::vector<IntVar*>& vars,
                                     const IntTupleSet& tuples);

  /// This constraint create a finite automaton that will check the
  /// sequence of variables vars. It uses a transition table called
  /// 'transition_table'. Each transition is a triple
  ///    (current_state, variable_value, new_state).
  /// The initial state is given, and the set of accepted states is decribed
  /// by 'final_states'. These states are hidden inside the constraint.
  /// Only the transitions (i.e. the variables) are visible.
  Constraint* MakeTransitionConstraint(
      const std::vector<IntVar*>& vars, const IntTupleSet& transition_table,
      int64_t initial_state, const std::vector<int64_t>& final_states);

  /// This constraint create a finite automaton that will check the
  /// sequence of variables vars. It uses a transition table called
  /// 'transition_table'. Each transition is a triple
  ///    (current_state, variable_value, new_state).
  /// The initial state is given, and the set of accepted states is decribed
  /// by 'final_states'. These states are hidden inside the constraint.
  /// Only the transitions (i.e. the variables) are visible.
  Constraint* MakeTransitionConstraint(const std::vector<IntVar*>& vars,
                                       const IntTupleSet& transition_table,
                                       int64_t initial_state,
                                       const std::vector<int>& final_states);

#if defined(SWIGPYTHON)
  /// Compatibility layer for Python API.
  Constraint* MakeAllowedAssignments(
      const std::vector<IntVar*>& vars,
      const std::vector<std::vector<int64_t> /*keep for swig*/>& raw_tuples) {
    IntTupleSet tuples(vars.size());
    tuples.InsertAll(raw_tuples);
    return MakeAllowedAssignments(vars, tuples);
  }

  Constraint* MakeTransitionConstraint(
      const std::vector<IntVar*>& vars,
      const std::vector<std::vector<int64_t> /*keep for swig*/>&
          raw_transitions,
      int64_t initial_state, const std::vector<int>& final_states) {
    IntTupleSet transitions(3);
    transitions.InsertAll(raw_transitions);
    return MakeTransitionConstraint(vars, transitions, initial_state,
                                    final_states);
  }
#endif

  /// This constraint states that all the boxes must not overlap.
  /// The coordinates of box i are:
  ///   (x_vars[i], y_vars[i]),
  ///   (x_vars[i], y_vars[i] + y_size[i]),
  ///   (x_vars[i] + x_size[i], y_vars[i]),
  ///   (x_vars[i] + x_size[i], y_vars[i] + y_size[i]).
  /// The sizes must be non-negative. Boxes with a zero dimension can be
  /// pushed like any box.
  Constraint* MakeNonOverlappingBoxesConstraint(
      const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
      const std::vector<IntVar*>& x_size, const std::vector<IntVar*>& y_size);
  Constraint* MakeNonOverlappingBoxesConstraint(
      const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
      const std::vector<int64_t>& x_size, const std::vector<int64_t>& y_size);
  Constraint* MakeNonOverlappingBoxesConstraint(
      const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
      const std::vector<int>& x_size, const std::vector<int>& y_size);

  /// This constraint states that all the boxes must not overlap.
  /// The coordinates of box i are:
  ///   (x_vars[i], y_vars[i]),
  ///   (x_vars[i], y_vars[i] + y_size[i]),
  ///   (x_vars[i] + x_size[i], y_vars[i]),
  ///   (x_vars[i] + x_size[i], y_vars[i] + y_size[i]).
  /// The sizes must be positive.
  /// Boxes with a zero dimension can be placed anywhere.
  Constraint* MakeNonOverlappingNonStrictBoxesConstraint(
      const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
      const std::vector<IntVar*>& x_size, const std::vector<IntVar*>& y_size);
  Constraint* MakeNonOverlappingNonStrictBoxesConstraint(
      const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
      const std::vector<int64_t>& x_size, const std::vector<int64_t>& y_size);
  Constraint* MakeNonOverlappingNonStrictBoxesConstraint(
      const std::vector<IntVar*>& x_vars, const std::vector<IntVar*>& y_vars,
      const std::vector<int>& x_size, const std::vector<int>& y_size);

  /// This constraint packs all variables onto 'number_of_bins'
  /// variables.  For any given variable, a value of 'number_of_bins'
  /// indicates that the variable is not assigned to any bin.
  /// Dimensions, i.e., cumulative constraints on this packing, can be
  /// added directly from the pack class.
  Pack* MakePack(const std::vector<IntVar*>& vars, int number_of_bins);

  /// Creates an interval var with a fixed duration. The duration must
  /// be greater than 0. If optional is true, then the interval can be
  /// performed or unperformed. If optional is false, then the interval
  /// is always performed.
  IntervalVar* MakeFixedDurationIntervalVar(int64_t start_min,
                                            int64_t start_max, int64_t duration,
                                            bool optional,
                                            const std::string& name);

  /// This method fills the vector with 'count' interval variables built with
  /// the corresponding parameters.
  void MakeFixedDurationIntervalVarArray(
      int count, int64_t start_min, int64_t start_max, int64_t duration,
      bool optional, const std::string& name,
      std::vector<IntervalVar*>* const array);

  /// Creates a performed interval var with a fixed duration. The duration must
  /// be greater than 0.
  IntervalVar* MakeFixedDurationIntervalVar(IntVar* const start_variable,
                                            int64_t duration,
                                            const std::string& name);

  /// Creates an interval var with a fixed duration, and performed_variable.
  /// The duration must be greater than 0.
  IntervalVar* MakeFixedDurationIntervalVar(IntVar* const start_variable,
                                            int64_t duration,
                                            IntVar* const performed_variable,
                                            const std::string& name);

  /// This method fills the vector with 'count' interval var built with
  /// the corresponding start variables.
  void MakeFixedDurationIntervalVarArray(
      const std::vector<IntVar*>& start_variables, int64_t duration,
      const std::string& name, std::vector<IntervalVar*>* const array);

  /// This method fills the vector with interval variables built with
  /// the corresponding start variables.
  void MakeFixedDurationIntervalVarArray(
      const std::vector<IntVar*>& start_variables,
      const std::vector<int64_t>& durations, const std::string& name,
      std::vector<IntervalVar*>* const array);
  /// This method fills the vector with interval variables built with
  /// the corresponding start variables.
  void MakeFixedDurationIntervalVarArray(
      const std::vector<IntVar*>& start_variables,
      const std::vector<int>& durations, const std::string& name,
      std::vector<IntervalVar*>* const array);

  /// This method fills the vector with interval variables built with
  /// the corresponding start and performed variables.
  void MakeFixedDurationIntervalVarArray(
      const std::vector<IntVar*>& start_variables,
      const std::vector<int64_t>& durations,
      const std::vector<IntVar*>& performed_variables, const std::string& name,
      std::vector<IntervalVar*>* const array);

  /// This method fills the vector with interval variables built with
  /// the corresponding start and performed variables.
  void MakeFixedDurationIntervalVarArray(
      const std::vector<IntVar*>& start_variables,
      const std::vector<int>& durations,
      const std::vector<IntVar*>& performed_variables, const std::string& name,
      std::vector<IntervalVar*>* const array);

  /// Creates a fixed and performed interval.
  IntervalVar* MakeFixedInterval(int64_t start, int64_t duration,
                                 const std::string& name);

  /// Creates an interval var by specifying the bounds on start,
  /// duration, and end.
  IntervalVar* MakeIntervalVar(int64_t start_min, int64_t start_max,
                               int64_t duration_min, int64_t duration_max,
                               int64_t end_min, int64_t end_max, bool optional,
                               const std::string& name);

  /// This method fills the vector with 'count' interval var built with
  /// the corresponding parameters.
  void MakeIntervalVarArray(int count, int64_t start_min, int64_t start_max,
                            int64_t duration_min, int64_t duration_max,
                            int64_t end_min, int64_t end_max, bool optional,
                            const std::string& name,
                            std::vector<IntervalVar*>* const array);

  /// Creates an interval var that is the mirror image of the given one, that
  /// is, the interval var obtained by reversing the axis.
  IntervalVar* MakeMirrorInterval(IntervalVar* const interval_var);

  /// Creates an interval var with a fixed duration whose start is
  /// synchronized with the start of another interval, with a given
  /// offset. The performed status is also in sync with the performed
  /// status of the given interval variable.
  IntervalVar* MakeFixedDurationStartSyncedOnStartIntervalVar(
      IntervalVar* const interval_var, int64_t duration, int64_t offset);

  /// Creates an interval var with a fixed duration whose start is
  /// synchronized with the end of another interval, with a given
  /// offset. The performed status is also in sync with the performed
  /// status of the given interval variable.
  IntervalVar* MakeFixedDurationStartSyncedOnEndIntervalVar(
      IntervalVar* const interval_var, int64_t duration, int64_t offset);

  /// Creates an interval var with a fixed duration whose end is
  /// synchronized with the start of another interval, with a given
  /// offset. The performed status is also in sync with the performed
  /// status of the given interval variable.
  IntervalVar* MakeFixedDurationEndSyncedOnStartIntervalVar(
      IntervalVar* const interval_var, int64_t duration, int64_t offset);

  /// Creates an interval var with a fixed duration whose end is
  /// synchronized with the end of another interval, with a given
  /// offset. The performed status is also in sync with the performed
  /// status of the given interval variable.
  IntervalVar* MakeFixedDurationEndSyncedOnEndIntervalVar(
      IntervalVar* const interval_var, int64_t duration, int64_t offset);

  /// Creates and returns an interval variable that wraps around the given one,
  /// relaxing the min start and end. Relaxing means making unbounded when
  /// optional. If the variable is non-optional, this method returns
  /// interval_var.
  ///
  /// More precisely, such an interval variable behaves as follows:
  /// * When the underlying must be performed, the returned interval variable
  ///     behaves exactly as the underlying;
  /// * When the underlying may or may not be performed, the returned interval
  ///     variable behaves like the underlying, except that it is unbounded on
  ///     the min side;
  /// * When the underlying cannot be performed, the returned interval variable
  ///     is of duration 0 and must be performed in an interval unbounded on
  ///     both sides.
  ///
  /// This is very useful to implement propagators that may only modify
  /// the start max or end max.
  IntervalVar* MakeIntervalRelaxedMin(IntervalVar* const interval_var);

  /// Creates and returns an interval variable that wraps around the given one,
  /// relaxing the max start and end. Relaxing means making unbounded when
  /// optional. If the variable is non optional, this method returns
  /// interval_var.
  ///
  /// More precisely, such an interval variable behaves as follows:
  /// * When the underlying must be performed, the returned interval variable
  ///     behaves exactly as the underlying;
  /// * When the underlying may or may not be performed, the returned interval
  ///     variable behaves like the underlying, except that it is unbounded on
  ///     the max side;
  /// * When the underlying cannot be performed, the returned interval variable
  ///     is of duration 0 and must be performed in an interval unbounded on
  ///     both sides.
  ///
  /// This is very useful for implementing propagators that may only modify
  /// the start min or end min.
  IntervalVar* MakeIntervalRelaxedMax(IntervalVar* const interval_var);

  /// This method creates a relation between an interval var and a
  /// date.
  Constraint* MakeIntervalVarRelation(IntervalVar* const t,
                                      UnaryIntervalRelation r, int64_t d);

  /// This method creates a relation between two interval vars.
  Constraint* MakeIntervalVarRelation(IntervalVar* const t1,
                                      BinaryIntervalRelation r,
                                      IntervalVar* const t2);

  /// This method creates a relation between two interval vars.
  /// The given delay is added to the second interval.
  /// i.e.: t1 STARTS_AFTER_END of t2 with a delay of 2
  /// means t1 will start at least two units of time after the end of t2.
  Constraint* MakeIntervalVarRelationWithDelay(IntervalVar* const t1,
                                               BinaryIntervalRelation r,
                                               IntervalVar* const t2,
                                               int64_t delay);

  /// This constraint implements a temporal disjunction between two
  /// interval vars t1 and t2. 'alt' indicates which alternative was
  /// chosen (alt == 0 is equivalent to t1 before t2).
  Constraint* MakeTemporalDisjunction(IntervalVar* const t1,
                                      IntervalVar* const t2, IntVar* const alt);

  /// This constraint implements a temporal disjunction between two
  /// interval vars.
  Constraint* MakeTemporalDisjunction(IntervalVar* const t1,
                                      IntervalVar* const t2);

  /// This constraint forces all interval vars into an non-overlapping
  /// sequence. Intervals with zero duration can be scheduled anywhere.
  DisjunctiveConstraint* MakeDisjunctiveConstraint(
      const std::vector<IntervalVar*>& intervals, const std::string& name);

  /// This constraint forces all interval vars into an non-overlapping
  /// sequence. Intervals with zero durations cannot overlap with over
  /// intervals.
  DisjunctiveConstraint* MakeStrictDisjunctiveConstraint(
      const std::vector<IntervalVar*>& intervals, const std::string& name);

  /// This constraint forces that, for any integer t, the sum of the demands
  /// corresponding to an interval containing t does not exceed the given
  /// capacity.
  ///
  /// Intervals and demands should be vectors of equal size.
  ///
  /// Demands should only contain non-negative values. Zero values are
  /// supported, and the corresponding intervals are filtered out, as they
  /// neither impact nor are impacted by this constraint.
  Constraint* MakeCumulative(const std::vector<IntervalVar*>& intervals,
                             const std::vector<int64_t>& demands,
                             int64_t capacity, const std::string& name);

  /// This constraint forces that, for any integer t, the sum of the demands
  /// corresponding to an interval containing t does not exceed the given
  /// capacity.
  ///
  /// Intervals and demands should be vectors of equal size.
  ///
  /// Demands should only contain non-negative values. Zero values are
  /// supported, and the corresponding intervals are filtered out, as they
  /// neither impact nor are impacted by this constraint.
  Constraint* MakeCumulative(const std::vector<IntervalVar*>& intervals,
                             const std::vector<int>& demands, int64_t capacity,
                             const std::string& name);

  /// This constraint forces that, for any integer t, the sum of the demands
  /// corresponding to an interval containing t does not exceed the given
  /// capacity.
  ///
  /// Intervals and demands should be vectors of equal size.
  ///
  /// Demands should only contain non-negative values. Zero values are
  /// supported, and the corresponding intervals are filtered out, as they
  /// neither impact nor are impacted by this constraint.
  Constraint* MakeCumulative(const std::vector<IntervalVar*>& intervals,
                             const std::vector<int64_t>& demands,
                             IntVar* const capacity, const std::string& name);

  /// This constraint enforces that, for any integer t, the sum of the demands
  /// corresponding to an interval containing t does not exceed the given
  /// capacity.
  ///
  /// Intervals and demands should be vectors of equal size.
  ///
  /// Demands should only contain non-negative values. Zero values are
  /// supported, and the corresponding intervals are filtered out, as they
  /// neither impact nor are impacted by this constraint.
  Constraint* MakeCumulative(const std::vector<IntervalVar*>& intervals,
                             const std::vector<int>& demands,
                             IntVar* const capacity, const std::string& name);

  /// This constraint enforces that, for any integer t, the sum of demands
  /// corresponding to an interval containing t does not exceed the given
  /// capacity.
  ///
  /// Intervals and demands should be vectors of equal size.
  ///
  /// Demands should be positive.
  Constraint* MakeCumulative(const std::vector<IntervalVar*>& intervals,
                             const std::vector<IntVar*>& demands,
                             int64_t capacity, const std::string& name);

  /// This constraint enforces that, for any integer t, the sum of demands
  /// corresponding to an interval containing t does not exceed the given
  /// capacity.
  ///
  /// Intervals and demands should be vectors of equal size.
  ///
  /// Demands should be positive.
  Constraint* MakeCumulative(const std::vector<IntervalVar*>& intervals,
                             const std::vector<IntVar*>& demands,
                             IntVar* const capacity, const std::string& name);

  /// This constraint states that the target_var is the convex hull of
  /// the intervals. If none of the interval variables is performed,
  /// then the target var is unperformed too. Also, if the target
  /// variable is unperformed, then all the intervals variables are
  /// unperformed too.
  Constraint* MakeCover(const std::vector<IntervalVar*>& vars,
                        IntervalVar* const target_var);

  /// This constraints states that the two interval variables are equal.
  Constraint* MakeEquality(IntervalVar* const var1, IntervalVar* const var2);

  /// This method creates an empty assignment.
  Assignment* MakeAssignment();

  /// This method creates an assignment which is a copy of 'a'.
  Assignment* MakeAssignment(const Assignment* const a);

  /// Collect the first solution of the search.
  SolutionCollector* MakeFirstSolutionCollector(
      const Assignment* const assignment);
  /// Collect the first solution of the search. The variables will need to
  /// be added later.
  SolutionCollector* MakeFirstSolutionCollector();

  /// Collect the last solution of the search.
  SolutionCollector* MakeLastSolutionCollector(
      const Assignment* const assignment);
  /// Collect the last solution of the search. The variables will need to
  /// be added later.
  SolutionCollector* MakeLastSolutionCollector();

  /// Collect the solution corresponding to the optimal value of the objective
  /// of 'assignment'; if 'assignment' does not have an objective no solution is
  /// collected. This collector only collects one solution corresponding to the
  /// best objective value (the first one found).
  SolutionCollector* MakeBestValueSolutionCollector(
      const Assignment* const assignment, bool maximize);
  /// Collect the solution corresponding to the optimal value of the
  /// objective of 'assignment'; if 'assignment' does not have an objective no
  /// solution is collected. This collector only collects one solution
  /// corresponding to the best objective value (the first one
  /// found). The variables will need to be added later.
  SolutionCollector* MakeBestValueSolutionCollector(bool maximize);

  /// Same as MakeBestValueSolutionCollector but collects the best
  /// solution_count solutions. Collected solutions are sorted in increasing
  /// optimality order (the best solution is the last one).
  SolutionCollector* MakeNBestValueSolutionCollector(
      const Assignment* const assignment, int solution_count, bool maximize);
  SolutionCollector* MakeNBestValueSolutionCollector(int solution_count,
                                                     bool maximize);

  /// Collect all solutions of the search.
  SolutionCollector* MakeAllSolutionCollector(
      const Assignment* const assignment);
  /// Collect all solutions of the search. The variables will need to
  /// be added later.
  SolutionCollector* MakeAllSolutionCollector();

  /// Creates a minimization objective.
  OptimizeVar* MakeMinimize(IntVar* const v, int64_t step);

  /// Creates a maximization objective.
  OptimizeVar* MakeMaximize(IntVar* const v, int64_t step);

  /// Creates a objective with a given sense (true = maximization).
  OptimizeVar* MakeOptimize(bool maximize, IntVar* const v, int64_t step);

  /// Creates a minimization weighted objective. The actual objective is
  /// scalar_prod(sub_objectives, weights).
  OptimizeVar* MakeWeightedMinimize(const std::vector<IntVar*>& sub_objectives,
                                    const std::vector<int64_t>& weights,
                                    int64_t step);

  /// Creates a minimization weighted objective. The actual objective is
  /// scalar_prod(sub_objectives, weights).
  OptimizeVar* MakeWeightedMinimize(const std::vector<IntVar*>& sub_objectives,
                                    const std::vector<int>& weights,
                                    int64_t step);

  /// Creates a maximization weigthed objective.
  OptimizeVar* MakeWeightedMaximize(const std::vector<IntVar*>& sub_objectives,
                                    const std::vector<int64_t>& weights,
                                    int64_t step);

  /// Creates a maximization weigthed objective.
  OptimizeVar* MakeWeightedMaximize(const std::vector<IntVar*>& sub_objectives,
                                    const std::vector<int>& weights,
                                    int64_t step);

  /// Creates a weighted objective with a given sense (true = maximization).
  OptimizeVar* MakeWeightedOptimize(bool maximize,
                                    const std::vector<IntVar*>& sub_objectives,
                                    const std::vector<int64_t>& weights,
                                    int64_t step);

  /// Creates a weighted objective with a given sense (true = maximization).
  OptimizeVar* MakeWeightedOptimize(bool maximize,
                                    const std::vector<IntVar*>& sub_objectives,
                                    const std::vector<int>& weights,
                                    int64_t step);

  /// MetaHeuristics which try to get the search out of local optima.

  /// Creates a Tabu Search monitor.
  /// In the context of local search the behavior is similar to MakeOptimize(),
  /// creating an objective in a given sense. The behavior differs once a local
  /// optimum is reached: thereafter solutions which degrade the value of the
  /// objective are allowed if they are not "tabu". A solution is "tabu" if it
  /// doesn't respect the following rules:
  /// - improving the best solution found so far
  /// - variables in the "keep" list must keep their value, variables in the
  /// "forbid" list must not take the value they have in the list.
  /// Variables with new values enter the tabu lists after each new solution
  /// found and leave the lists after a given number of iterations (called
  /// tenure). Only the variables passed to the method can enter the lists.
  /// The tabu criterion is softened by the tabu factor which gives the number
  /// of "tabu" violations which is tolerated; a factor of 1 means no violations
  /// allowed; a factor of 0 means all violations are allowed.

  SearchMonitor* MakeTabuSearch(bool maximize, IntVar* const v, int64_t step,
                                const std::vector<IntVar*>& vars,
                                int64_t keep_tenure, int64_t forbid_tenure,
                                double tabu_factor);

  /// Creates a Tabu Search based on the vars |vars|.
  /// A solution is "tabu" if all the vars in |vars| keep their value.
  SearchMonitor* MakeGenericTabuSearch(bool maximize, IntVar* const v,
                                       int64_t step,
                                       const std::vector<IntVar*>& tabu_vars,
                                       int64_t forbid_tenure);

  /// Creates a Simulated Annealing monitor.
  // TODO(user): document behavior
  SearchMonitor* MakeSimulatedAnnealing(bool maximize, IntVar* const v,
                                        int64_t step,
                                        int64_t initial_temperature);

  /// Creates a Guided Local Search monitor.
  /// Description here: http://en.wikipedia.org/wiki/Guided_Local_Search
  SearchMonitor* MakeGuidedLocalSearch(
      bool maximize, IntVar* objective, IndexEvaluator2 objective_function,
      int64_t step, const std::vector<IntVar*>& vars, double penalty_factor,
      bool reset_penalties_on_new_best_solution = false);
  SearchMonitor* MakeGuidedLocalSearch(
      bool maximize, IntVar* objective, IndexEvaluator3 objective_function,
      int64_t step, const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars, double penalty_factor,
      bool reset_penalties_on_new_best_solution = false);

  /// This search monitor will restart the search periodically.
  /// At the iteration n, it will restart after scale_factor * Luby(n) failures
  /// where Luby is the Luby Strategy (i.e. 1 1 2 1 1 2 4 1 1 2 1 1 2 4 8...).
  SearchMonitor* MakeLubyRestart(int scale_factor);

  /// This search monitor will restart the search periodically after 'frequency'
  /// failures.
  SearchMonitor* MakeConstantRestart(int frequency);

  /// Creates a search limit that constrains the running time.
  ABSL_MUST_USE_RESULT RegularLimit* MakeTimeLimit(absl::Duration time);
#if !defined(SWIG)
  ABSL_DEPRECATED("Use the version taking absl::Duration() as argument")
#endif  // !defined(SWIG)
  ABSL_MUST_USE_RESULT RegularLimit* MakeTimeLimit(int64_t time_in_ms) {
    return MakeTimeLimit(time_in_ms == kint64max
                             ? absl::InfiniteDuration()
                             : absl::Milliseconds(time_in_ms));
  }

  /// Creates a search limit that constrains the number of branches
  /// explored in the search tree.
  ABSL_MUST_USE_RESULT RegularLimit* MakeBranchesLimit(int64_t branches);

  /// Creates a search limit that constrains the number of failures
  /// that can happen when exploring the search tree.
  ABSL_MUST_USE_RESULT RegularLimit* MakeFailuresLimit(int64_t failures);

  /// Creates a search limit that constrains the number of solutions found
  /// during the search.
  ABSL_MUST_USE_RESULT RegularLimit* MakeSolutionsLimit(int64_t solutions);

  /// Limits the search with the 'time', 'branches', 'failures' and
  /// 'solutions' limits. 'smart_time_check' reduces the calls to the wall
  // timer by estimating the number of remaining calls, and 'cumulative' means
  // that the limit applies cumulatively, instead of search-by-search.
  ABSL_MUST_USE_RESULT RegularLimit* MakeLimit(absl::Duration time,
                                               int64_t branches,
                                               int64_t failures,
                                               int64_t solutions,
                                               bool smart_time_check = false,
                                               bool cumulative = false);
  /// Creates a search limit from its protobuf description
  ABSL_MUST_USE_RESULT RegularLimit* MakeLimit(
      const RegularLimitParameters& proto);

#if !defined(SWIG)
  ABSL_DEPRECATED("Use other MakeLimit() versions")
#endif  // !defined(SWIG)
  ABSL_MUST_USE_RESULT RegularLimit* MakeLimit(int64_t time, int64_t branches,
                                               int64_t failures,
                                               int64_t solutions,
                                               bool smart_time_check = false,
                                               bool cumulative = false);

  /// Creates a regular limit proto containing default values.
  RegularLimitParameters MakeDefaultRegularLimitParameters() const;

  /// Creates a search limit that is reached when either of the underlying limit
  /// is reached. That is, the returned limit is more stringent than both
  /// argument limits.
  ABSL_MUST_USE_RESULT SearchLimit* MakeLimit(SearchLimit* const limit_1,
                                              SearchLimit* const limit_2);

  /// Limits the search based on the improvements of 'objective_var'. Stops the
  /// search when the improvement rate gets lower than a threshold value. This
  /// threshold value is computed based on the improvement rate during the first
  /// phase of the search.
  ABSL_MUST_USE_RESULT ImprovementSearchLimit* MakeImprovementLimit(
      IntVar* objective_var, bool maximize, double objective_scaling_factor,
      double objective_offset, double improvement_rate_coefficient,
      int improvement_rate_solutions_distance);

  /// Callback-based search limit. Search stops when limiter returns true; if
  /// this happens at a leaf the corresponding solution will be rejected.
  ABSL_MUST_USE_RESULT SearchLimit* MakeCustomLimit(
      std::function<bool()> limiter);

  // TODO(user): DEPRECATE API of MakeSearchLog(.., IntVar* var,..).

  /// The SearchMonitors below will display a periodic search log
  /// on LOG(INFO) every branch_period branches explored.
  SearchMonitor* MakeSearchLog(int branch_period);

  /// At each solution, this monitor also display the var value.
  SearchMonitor* MakeSearchLog(int branch_period, IntVar* const var);

  /// At each solution, this monitor will also display result of @p
  /// display_callback.
  SearchMonitor* MakeSearchLog(int branch_period,
                               std::function<std::string()> display_callback);

  /// At each solution, this monitor will display the 'var' value and the
  /// result of @p display_callback.
  SearchMonitor* MakeSearchLog(int branch_period, IntVar* var,
                               std::function<std::string()> display_callback);

  /// OptimizeVar Search Logs
  /// At each solution, this monitor will also display the 'opt_var' value.
  SearchMonitor* MakeSearchLog(int branch_period, OptimizeVar* const opt_var);

  /// Creates a search monitor that will also print the result of the
  /// display callback.
  SearchMonitor* MakeSearchLog(int branch_period, OptimizeVar* const opt_var,
                               std::function<std::string()> display_callback);

  /// Creates a search monitor from logging parameters.
  struct SearchLogParameters {
    /// SearchMonitors will display a periodic search log every branch_period
    /// branches explored.
    int branch_period = 1;
    /// SearchMonitors will display values of objective or variable (both cannot
    /// be used together).
    OptimizeVar* objective = nullptr;
    IntVar* variable = nullptr;
    /// When displayed, objective or var values will be scaled and offset by
    /// the given values in the following way:
    /// scaling_factor * (value + offset).
    double scaling_factor = 1.0;
    double offset = 0;
    /// SearchMonitors will display the result of display_callback at each new
    /// solution found and when the search finishes if
    /// display_on_new_solutions_only is false.
    std::function<std::string()> display_callback;
    /// To be used to protect from cases where display_callback assumes
    /// variables are instantiated, which only happens in AtSolution().
    bool display_on_new_solutions_only = true;
  };
  SearchMonitor* MakeSearchLog(SearchLogParameters parameters);

  /// Creates a search monitor that will trace precisely the behavior of the
  /// search. Use this only for low level debugging.
  SearchMonitor* MakeSearchTrace(const std::string& prefix);

  /// ----- Callback-based search monitors -----
  SearchMonitor* MakeEnterSearchCallback(std::function<void()> callback);
  SearchMonitor* MakeExitSearchCallback(std::function<void()> callback);
  SearchMonitor* MakeAtSolutionCallback(std::function<void()> callback);

  /// Prints the model.
  ModelVisitor* MakePrintModelVisitor();
  /// Displays some nice statistics on the model.
  ModelVisitor* MakeStatisticsModelVisitor();
#if !defined(SWIG)
  /// Compute the number of constraints a variable is attached to.
  ModelVisitor* MakeVariableDegreeVisitor(
      absl::flat_hash_map<const IntVar*, int>* const map);
#endif  // !defined(SWIG)

  /// Symmetry Breaking.
  SearchMonitor* MakeSymmetryManager(
      const std::vector<SymmetryBreaker*>& visitors);
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

  /// Decisions.
  Decision* MakeAssignVariableValue(IntVar* const var, int64_t val);
  Decision* MakeVariableLessOrEqualValue(IntVar* const var, int64_t value);
  Decision* MakeVariableGreaterOrEqualValue(IntVar* const var, int64_t value);
  Decision* MakeSplitVariableDomain(IntVar* const var, int64_t val,
                                    bool start_with_lower_half);
  Decision* MakeAssignVariableValueOrFail(IntVar* const var, int64_t value);
  Decision* MakeAssignVariableValueOrDoNothing(IntVar* const var,
                                               int64_t value);
  Decision* MakeAssignVariablesValues(const std::vector<IntVar*>& vars,
                                      const std::vector<int64_t>& values);
  Decision* MakeAssignVariablesValuesOrDoNothing(
      const std::vector<IntVar*>& vars, const std::vector<int64_t>& values);
  Decision* MakeAssignVariablesValuesOrFail(const std::vector<IntVar*>& vars,
                                            const std::vector<int64_t>& values);
  Decision* MakeFailDecision();
  Decision* MakeDecision(Action apply, Action refute);

  /// Creates a decision builder which sequentially composes decision builders.
  /// At each leaf of a decision builder, the next decision builder is therefore
  /// called. For instance, Compose(db1, db2) will result in the following tree:
  ///          d1 tree              |
  ///         /   |   \             |
  ///         db1 leaves            |
  ///       /     |     \           |
  ///  db2 tree db2 tree db2 tree   |
  DecisionBuilder* Compose(DecisionBuilder* const db1,
                           DecisionBuilder* const db2);
  DecisionBuilder* Compose(DecisionBuilder* const db1,
                           DecisionBuilder* const db2,
                           DecisionBuilder* const db3);
  DecisionBuilder* Compose(DecisionBuilder* const db1,
                           DecisionBuilder* const db2,
                           DecisionBuilder* const db3,
                           DecisionBuilder* const db4);
  DecisionBuilder* Compose(const std::vector<DecisionBuilder*>& dbs);

  /// Creates a decision builder which will create a search tree where each
  /// decision builder is called from the top of the search tree. For instance
  /// the decision builder Try(db1, db2) will entirely explore the search tree
  /// of db1 then the one of db2, resulting in the following search tree:
  ///        Tree root            |
  ///        /       \            |
  ///  db1 tree     db2 tree      |
  ///
  /// This is very handy to try a decision builder which partially explores the
  /// search space and if it fails to try another decision builder.
  ///
  // TODO(user): The search tree can be balanced by using binary
  /// "Try"-builders "recursively". For instance, Try(a,b,c,d) will give a tree
  /// unbalanced to the right, whereas Try(Try(a,b), Try(b,c)) will give a
  /// balanced tree. Investigate if we should only provide the binary version
  /// and/or if we should balance automatically.
  DecisionBuilder* Try(DecisionBuilder* const db1, DecisionBuilder* const db2);
  DecisionBuilder* Try(DecisionBuilder* const db1, DecisionBuilder* const db2,
                       DecisionBuilder* const db3);
  DecisionBuilder* Try(DecisionBuilder* const db1, DecisionBuilder* const db2,
                       DecisionBuilder* const db3, DecisionBuilder* const db4);
  DecisionBuilder* Try(const std::vector<DecisionBuilder*>& dbs);

  /// Phases on IntVar arrays.
  // TODO(user): name each of them differently, and document them (and do that
  /// for all other functions that have several homonyms in this .h).
  DecisionBuilder* MakePhase(const std::vector<IntVar*>& vars,
                             IntVarStrategy var_str, IntValueStrategy val_str);
  DecisionBuilder* MakePhase(const std::vector<IntVar*>& vars,
                             IndexEvaluator1 var_evaluator,
                             IntValueStrategy val_str);

  DecisionBuilder* MakePhase(const std::vector<IntVar*>& vars,
                             IntVarStrategy var_str,
                             IndexEvaluator2 value_evaluator);

  /// var_val1_val2_comparator(var, val1, val2) is true iff assigning value
  /// "val1" to variable "var" is better than assigning value "val2".
  DecisionBuilder* MakePhase(const std::vector<IntVar*>& vars,
                             IntVarStrategy var_str,
                             VariableValueComparator var_val1_val2_comparator);

  DecisionBuilder* MakePhase(const std::vector<IntVar*>& vars,
                             IndexEvaluator1 var_evaluator,
                             IndexEvaluator2 value_evaluator);

  DecisionBuilder* MakePhase(const std::vector<IntVar*>& vars,
                             IntVarStrategy var_str,
                             IndexEvaluator2 value_evaluator,
                             IndexEvaluator1 tie_breaker);

  DecisionBuilder* MakePhase(const std::vector<IntVar*>& vars,
                             IndexEvaluator1 var_evaluator,
                             IndexEvaluator2 value_evaluator,
                             IndexEvaluator1 tie_breaker);

  DecisionBuilder* MakeDefaultPhase(const std::vector<IntVar*>& vars);
  DecisionBuilder* MakeDefaultPhase(const std::vector<IntVar*>& vars,
                                    const DefaultPhaseParameters& parameters);

  /// Shortcuts for small arrays.
  DecisionBuilder* MakePhase(IntVar* const v0, IntVarStrategy var_str,
                             IntValueStrategy val_str);
  DecisionBuilder* MakePhase(IntVar* const v0, IntVar* const v1,
                             IntVarStrategy var_str, IntValueStrategy val_str);
  DecisionBuilder* MakePhase(IntVar* const v0, IntVar* const v1,
                             IntVar* const v2, IntVarStrategy var_str,
                             IntValueStrategy val_str);
  DecisionBuilder* MakePhase(IntVar* const v0, IntVar* const v1,
                             IntVar* const v2, IntVar* const v3,
                             IntVarStrategy var_str, IntValueStrategy val_str);

  /// Returns a decision that tries to schedule a task at a given time.
  /// On the Apply branch, it will set that interval var as performed and set
  /// its start to 'est'. On the Refute branch, it will just update the
  /// 'marker' to 'est' + 1. This decision is used in the
  /// INTERVAL_SET_TIMES_FORWARD strategy.
  Decision* MakeScheduleOrPostpone(IntervalVar* const var, int64_t est,
                                   int64_t* const marker);

  /// Returns a decision that tries to schedule a task at a given time.
  /// On the Apply branch, it will set that interval var as performed and set
  /// its end to 'est'. On the Refute branch, it will just update the
  /// 'marker' to 'est' - 1. This decision is used in the
  /// INTERVAL_SET_TIMES_BACKWARD strategy.
  Decision* MakeScheduleOrExpedite(IntervalVar* const var, int64_t est,
                                   int64_t* const marker);

  /// Returns a decision that tries to rank first the ith interval var
  /// in the sequence variable.
  Decision* MakeRankFirstInterval(SequenceVar* const sequence, int index);

  /// Returns a decision that tries to rank last the ith interval var
  /// in the sequence variable.
  Decision* MakeRankLastInterval(SequenceVar* const sequence, int index);

  /// Returns a decision builder which assigns values to variables which
  /// minimize the values returned by the evaluator. The arguments passed to the
  /// evaluator callback are the indices of the variables in vars and the values
  /// of these variables. Ownership of the callback is passed to the decision
  /// builder.
  DecisionBuilder* MakePhase(const std::vector<IntVar*>& vars,
                             IndexEvaluator2 eval, EvaluatorStrategy str);

  /// Returns a decision builder which assigns values to variables
  /// which minimize the values returned by the evaluator. In case of
  /// tie breaks, the second callback is used to choose the best index
  /// in the array of equivalent pairs with equivalent evaluations. The
  /// arguments passed to the evaluator callback are the indices of the
  /// variables in vars and the values of these variables. Ownership of
  /// the callback is passed to the decision builder.
  DecisionBuilder* MakePhase(const std::vector<IntVar*>& vars,
                             IndexEvaluator2 eval, IndexEvaluator1 tie_breaker,
                             EvaluatorStrategy str);

  /// Scheduling phases.
  DecisionBuilder* MakePhase(const std::vector<IntervalVar*>& intervals,
                             IntervalStrategy str);

  DecisionBuilder* MakePhase(const std::vector<SequenceVar*>& sequences,
                             SequenceStrategy str);

  /// Returns a decision builder for which the left-most leaf corresponds
  /// to assignment, the rest of the tree being explored using 'db'.
  DecisionBuilder* MakeDecisionBuilderFromAssignment(
      Assignment* const assignment, DecisionBuilder* const db,
      const std::vector<IntVar*>& vars);

  /// Returns a decision builder that will add the given constraint to
  /// the model.
  DecisionBuilder* MakeConstraintAdder(Constraint* const ct);

  /// SolveOnce will collapse a search tree described by a decision
  /// builder 'db' and a set of monitors and wrap it into a single point.
  /// If there are no solutions to this nested tree, then SolveOnce will
  /// fail. If there is a solution, it will find it and returns nullptr.
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
                                 const std::vector<SearchMonitor*>& monitors);

  /// NestedOptimize will collapse a search tree described by a
  /// decision builder 'db' and a set of monitors and wrap it into a
  /// single point. If there are no solutions to this nested tree, then
  /// NestedOptimize will fail. If there are solutions, it will find
  /// the best as described by the mandatory objective in the solution
  /// as well as the optimization direction, instantiate all variables
  /// to this solution, and return nullptr.
  DecisionBuilder* MakeNestedOptimize(DecisionBuilder* const db,
                                      Assignment* const solution, bool maximize,
                                      int64_t step);
  DecisionBuilder* MakeNestedOptimize(DecisionBuilder* const db,
                                      Assignment* const solution, bool maximize,
                                      int64_t step,
                                      SearchMonitor* const monitor1);
  DecisionBuilder* MakeNestedOptimize(DecisionBuilder* const db,
                                      Assignment* const solution, bool maximize,
                                      int64_t step,
                                      SearchMonitor* const monitor1,
                                      SearchMonitor* const monitor2);
  DecisionBuilder* MakeNestedOptimize(DecisionBuilder* const db,
                                      Assignment* const solution, bool maximize,
                                      int64_t step,
                                      SearchMonitor* const monitor1,
                                      SearchMonitor* const monitor2,
                                      SearchMonitor* const monitor3);
  DecisionBuilder* MakeNestedOptimize(DecisionBuilder* const db,
                                      Assignment* const solution, bool maximize,
                                      int64_t step,
                                      SearchMonitor* const monitor1,
                                      SearchMonitor* const monitor2,
                                      SearchMonitor* const monitor3,
                                      SearchMonitor* const monitor4);
  DecisionBuilder* MakeNestedOptimize(
      DecisionBuilder* const db, Assignment* const solution, bool maximize,
      int64_t step, const std::vector<SearchMonitor*>& monitors);

  /// Returns a DecisionBuilder which restores an Assignment
  /// (calls void Assignment::Restore())
  DecisionBuilder* MakeRestoreAssignment(Assignment* assignment);

  /// Returns a DecisionBuilder which stores an Assignment
  /// (calls void Assignment::Store())
  DecisionBuilder* MakeStoreAssignment(Assignment* assignment);

  /// Local Search Operators.
  LocalSearchOperator* MakeOperator(const std::vector<IntVar*>& vars,
                                    LocalSearchOperators op);
  LocalSearchOperator* MakeOperator(const std::vector<IntVar*>& vars,
                                    const std::vector<IntVar*>& secondary_vars,
                                    LocalSearchOperators op);
  // TODO(user): Make the callback an IndexEvaluator2 when there are no
  // secondary variables.
  LocalSearchOperator* MakeOperator(const std::vector<IntVar*>& vars,
                                    IndexEvaluator3 evaluator,
                                    EvaluatorLocalSearchOperators op);
  LocalSearchOperator* MakeOperator(const std::vector<IntVar*>& vars,
                                    const std::vector<IntVar*>& secondary_vars,
                                    IndexEvaluator3 evaluator,
                                    EvaluatorLocalSearchOperators op);

  /// Creates a large neighborhood search operator which creates fragments (set
  /// of relaxed variables) with up to number_of_variables random variables
  /// (sampling with replacement is performed meaning that at most
  /// number_of_variables variables are selected). Warning: this operator will
  /// always return neighbors; using it without a search limit will result in a
  /// non-ending search.
  /// Optionally a random seed can be specified.
  LocalSearchOperator* MakeRandomLnsOperator(const std::vector<IntVar*>& vars,
                                             int number_of_variables);
  LocalSearchOperator* MakeRandomLnsOperator(const std::vector<IntVar*>& vars,
                                             int number_of_variables,
                                             int32_t seed);

  /// Creates a local search operator that tries to move the assignment of some
  /// variables toward a target. The target is given as an Assignment. This
  /// operator generates neighbors in which the only difference compared to the
  /// current state is that one variable that belongs to the target assignment
  /// is set to its target value.
  LocalSearchOperator* MakeMoveTowardTargetOperator(const Assignment& target);

  /// Creates a local search operator that tries to move the assignment of some
  /// variables toward a target. The target is given either as two vectors: a
  /// vector of variables and a vector of associated target values. The two
  /// vectors should be of the same length. This operator generates neighbors in
  /// which the only difference compared to the current state is that one
  /// variable that belongs to the given vector is set to its target value.
  LocalSearchOperator* MakeMoveTowardTargetOperator(
      const std::vector<IntVar*>& variables,
      const std::vector<int64_t>& target_values);

  /// Creates a local search operator which concatenates a vector of operators.
  /// Each operator from the vector is called sequentially. By default, when a
  /// neighbor is found the neighborhood exploration restarts from the last
  /// active operator (the one which produced the neighbor).
  /// This can be overridden by setting restart to true to force the exploration
  /// to start from the first operator in the vector.
  ///
  /// The default behavior can also be overridden using an evaluation callback
  /// to set the order in which the operators are explored (the callback is
  /// called in LocalSearchOperator::Start()). The first argument of the
  /// callback is the index of the operator which produced the last move, the
  /// second argument is the index of the operator to be evaluated. Ownership of
  /// the callback is taken by ConcatenateOperators.
  ///
  /// Example:
  ///
  ///  const int kPriorities = {10, 100, 10, 0};
  ///  int64_t Evaluate(int active_operator, int current_operator) {
  ///    return kPriorities[current_operator];
  ///  }
  ///
  ///  LocalSearchOperator* concat =
  ///    solver.ConcatenateOperators(operators,
  ///                                NewPermanentCallback(&Evaluate));
  ///
  /// The elements of the vector operators will be sorted by increasing priority
  /// and explored in that order (tie-breaks are handled by keeping the relative
  /// operator order in the vector). This would result in the following order:
  /// operators[3], operators[0], operators[2], operators[1].
  ///
  LocalSearchOperator* ConcatenateOperators(
      const std::vector<LocalSearchOperator*>& ops);
  LocalSearchOperator* ConcatenateOperators(
      const std::vector<LocalSearchOperator*>& ops, bool restart);
  LocalSearchOperator* ConcatenateOperators(
      const std::vector<LocalSearchOperator*>& ops,
      std::function<int64_t(int, int)> evaluator);
  /// Randomized version of local search concatenator; calls a random operator
  /// at each call to MakeNextNeighbor().
  LocalSearchOperator* RandomConcatenateOperators(
      const std::vector<LocalSearchOperator*>& ops);

  /// Randomized version of local search concatenator; calls a random operator
  /// at each call to MakeNextNeighbor(). The provided seed is used to
  /// initialize the random number generator.
  LocalSearchOperator* RandomConcatenateOperators(
      const std::vector<LocalSearchOperator*>& ops, int32_t seed);

  /// Creates a local search operator which concatenates a vector of operators.
  /// Uses Multi-Armed Bandit approach for choosing the next operator to use.
  /// Sorts operators based on Upper Confidence Bound Algorithm which evaluates
  /// each operator as sum of average improvement and exploration function.
  ///
  /// Updates the order of operators when accepts a neighbor with objective
  /// improvement.
  LocalSearchOperator* MultiArmedBanditConcatenateOperators(
      const std::vector<LocalSearchOperator*>& ops, double memory_coefficient,
      double exploration_coefficient, bool maximize);

  /// Creates a local search operator that wraps another local search
  /// operator and limits the number of neighbors explored (i.e., calls
  /// to MakeNextNeighbor from the current solution (between two calls
  /// to Start()). When this limit is reached, MakeNextNeighbor()
  /// returns false. The counter is cleared when Start() is called.
  LocalSearchOperator* MakeNeighborhoodLimit(LocalSearchOperator* const op,
                                             int64_t limit);

  /// Local Search decision builders factories.
  /// Local search is used to improve a given solution. This initial solution
  /// can be specified either by an Assignment or by a DecisionBulder, and the
  /// corresponding variables, the initial solution being the first solution
  /// found by the DecisionBuilder.
  /// The LocalSearchPhaseParameters parameter holds the actual definition of
  /// the local search phase:
  /// - a local search operator used to explore the neighborhood of the current
  ///   solution,
  /// - a decision builder to instantiate unbound variables once a neighbor has
  ///   been defined; in the case of LNS-based operators instantiates fragment
  ///   variables; search monitors can be added to this sub-search by wrapping
  ///   the decision builder with MakeSolveOnce.
  /// - a search limit specifying how long local search looks for neighbors
  ///   before accepting one; the last neighbor is always taken and in the case
  ///   of a greedy search, this corresponds to the best local neighbor;
  ///   first-accept (which is the default behavior) can be modeled using a
  ///   solution found limit of 1,
  /// - a vector of local search filters used to speed up the search by pruning
  ///   unfeasible neighbors.
  /// Metaheuristics can be added by defining specialized search monitors;
  /// currently down/up-hill climbing is available through OptimizeVar, as well
  /// as Guided Local Search, Tabu Search and Simulated Annealing.
  ///
  // TODO(user): Make a variant which runs a local search after each
  //                solution found in a DFS.

  DecisionBuilder* MakeLocalSearchPhase(
      Assignment* const assignment,
      LocalSearchPhaseParameters* const parameters);
  DecisionBuilder* MakeLocalSearchPhase(
      const std::vector<IntVar*>& vars, DecisionBuilder* const first_solution,
      LocalSearchPhaseParameters* const parameters);
  /// Variant with a sub_decison_builder specific to the first solution.
  DecisionBuilder* MakeLocalSearchPhase(
      const std::vector<IntVar*>& vars, DecisionBuilder* const first_solution,
      DecisionBuilder* const first_solution_sub_decision_builder,
      LocalSearchPhaseParameters* const parameters);
  DecisionBuilder* MakeLocalSearchPhase(
      const std::vector<SequenceVar*>& vars,
      DecisionBuilder* const first_solution,
      LocalSearchPhaseParameters* const parameters);

  /// Solution Pool.
  SolutionPool* MakeDefaultSolutionPool();

  /// Local Search Phase Parameters
  LocalSearchPhaseParameters* MakeLocalSearchPhaseParameters(
      IntVar* objective, LocalSearchOperator* const ls_operator,
      DecisionBuilder* const sub_decision_builder);
  LocalSearchPhaseParameters* MakeLocalSearchPhaseParameters(
      IntVar* objective, LocalSearchOperator* const ls_operator,
      DecisionBuilder* const sub_decision_builder, RegularLimit* const limit);
  LocalSearchPhaseParameters* MakeLocalSearchPhaseParameters(
      IntVar* objective, LocalSearchOperator* const ls_operator,
      DecisionBuilder* const sub_decision_builder, RegularLimit* const limit,
      LocalSearchFilterManager* filter_manager);

  LocalSearchPhaseParameters* MakeLocalSearchPhaseParameters(
      IntVar* objective, SolutionPool* const pool,
      LocalSearchOperator* const ls_operator,
      DecisionBuilder* const sub_decision_builder);
  LocalSearchPhaseParameters* MakeLocalSearchPhaseParameters(
      IntVar* objective, SolutionPool* const pool,
      LocalSearchOperator* const ls_operator,
      DecisionBuilder* const sub_decision_builder, RegularLimit* const limit);
  LocalSearchPhaseParameters* MakeLocalSearchPhaseParameters(
      IntVar* objective, SolutionPool* const pool,
      LocalSearchOperator* const ls_operator,
      DecisionBuilder* const sub_decision_builder, RegularLimit* const limit,
      LocalSearchFilterManager* filter_manager);

  /// Local Search Filters
  LocalSearchFilter* MakeAcceptFilter();
  LocalSearchFilter* MakeRejectFilter();
  LocalSearchFilter* MakeVariableDomainFilter();
  IntVarLocalSearchFilter* MakeSumObjectiveFilter(
      const std::vector<IntVar*>& vars, IndexEvaluator2 values,
      Solver::LocalSearchFilterBound filter_enum);
  IntVarLocalSearchFilter* MakeSumObjectiveFilter(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars, IndexEvaluator3 values,
      Solver::LocalSearchFilterBound filter_enum);

  /// Performs PeriodicCheck on the top-level search; for instance, can be
  /// called from a nested solve to check top-level limits.
  void TopPeriodicCheck();
  /// Returns a percentage representing the propress of the search before
  /// reaching the limits of the top-level search (can be called from a nested
  /// solve).
  int TopProgressPercent();

  /// The PushState and PopState methods manipulates the states
  /// of the reversible objects. They are visible only because they
  /// are useful to write unitary tests.
  void PushState();
  void PopState();

  /// Gets the search depth of the current active search. Returns -1 if
  /// there is no active search opened.
  int SearchDepth() const;

  /// Gets the search left depth of the current active search. Returns -1 if
  /// there is no active search opened.
  int SearchLeftDepth() const;

  /// Gets the number of nested searches. It returns 0 outside search,
  /// 1 during the top level search, 2 or more in case of nested searches.
  int SolveDepth() const;

  /// Sets the given branch selector on the current active search.
  void SetBranchSelector(BranchSelector bs);

  /// Creates a decision builder that will set the branch selector.
  DecisionBuilder* MakeApplyBranchSelector(BranchSelector bs);

  /// All-in-one SaveAndSetValue.
  template <class T>
  void SaveAndSetValue(T* adr, T val) {
    if (*adr != val) {
      InternalSaveValue(adr);
      *adr = val;
    }
  }

  /// All-in-one SaveAndAdd_value.
  template <class T>
  void SaveAndAdd(T* adr, T val) {
    if (val != 0) {
      InternalSaveValue(adr);
      (*adr) += val;
    }
  }

  /// Returns a random value between 0 and 'size' - 1;
  int64_t Rand64(int64_t size) {
    DCHECK_GT(size, 0);
    return absl::Uniform<int64_t>(random_, 0, size);
  }

  /// Returns a random value between 0 and 'size' - 1;
  int32_t Rand32(int32_t size) {
    DCHECK_GT(size, 0);
    return absl::Uniform<int32_t>(random_, 0, size);
  }

  /// Reseed the solver random generator.
  void ReSeed(int32_t seed) { random_.seed(seed); }

  /// Exports the profiling information in a human readable overview.
  /// The parameter profile_level used to create the solver must be
  /// set to true.
  void ExportProfilingOverview(const std::string& filename);

  /// Returns local search profiling information in a human readable format.
  // TODO(user): Merge demon and local search profiles.
  std::string LocalSearchProfile() const;

#if !defined(SWIG)
  /// Returns detailed cp search statistics.
  ConstraintSolverStatistics GetConstraintSolverStatistics() const;
  /// Returns detailed local search statistics.
  LocalSearchStatistics GetLocalSearchStatistics() const;
#endif  // !defined(SWIG)

  /// Returns true whether the current search has been
  /// created using a Solve() call instead of a NewSearch one. It
  /// returns false if the solver is not in search at all.
  bool CurrentlyInSolve() const;

  /// Counts the number of constraints that have been added
  /// to the solver before the search.
  int constraints() const { return constraints_list_.size(); }

  /// Accepts the given model visitor.
  void Accept(ModelVisitor* const visitor) const;

  Decision* balancing_decision() const { return balancing_decision_.get(); }

  /// Internal
#if !defined(SWIG)
  void set_fail_intercept(std::function<void()> fail_intercept) {
    fail_intercept_ = std::move(fail_intercept);
  }
#endif  // !defined(SWIG)
  void clear_fail_intercept() { fail_intercept_ = nullptr; }
  /// Access to demon profiler.
  DemonProfiler* demon_profiler() const { return demon_profiler_; }
  // TODO(user): Get rid of the following methods once fast local search is
  /// enabled for metaheuristics.
  /// Disables/enables fast local search.
  void SetUseFastLocalSearch(bool use_fast_local_search) {
    use_fast_local_search_ = use_fast_local_search;
  }
  /// Returns true if fast local search is enabled.
  bool UseFastLocalSearch() const { return use_fast_local_search_; }
  /// Returns whether the object has been named or not.
  bool HasName(const PropagationBaseObject* object) const;
  /// Adds a new demon and wraps it inside a DemonProfiler if necessary.
  Demon* RegisterDemon(Demon* const demon);
  /// Registers a new IntExpr and wraps it inside a TraceIntExpr if necessary.
  IntExpr* RegisterIntExpr(IntExpr* const expr);
  /// Registers a new IntVar and wraps it inside a TraceIntVar if necessary.
  IntVar* RegisterIntVar(IntVar* const var);
  /// Registers a new IntervalVar and wraps it inside a TraceIntervalVar
  /// if necessary.
  IntervalVar* RegisterIntervalVar(IntervalVar* const var);

  /// Returns the active search, nullptr outside search.
  Search* ActiveSearch() const;
  /// Returns the cache of the model.
  ModelCache* Cache() const;
  /// Returns whether we are instrumenting demons.
  bool InstrumentsDemons() const;
  /// Returns whether we are profiling the solver.
  bool IsProfilingEnabled() const;
  /// Returns whether we are profiling local search.
  bool IsLocalSearchProfilingEnabled() const;
  /// Returns whether we are tracing variables.
  bool InstrumentsVariables() const;
  /// Returns whether all variables should be named.
  bool NameAllVariables() const;
  /// Returns the name of the model.
  std::string model_name() const;
  /// Returns the propagation monitor.
  PropagationMonitor* GetPropagationMonitor() const;
  /// Adds the propagation monitor to the solver. This is called internally when
  /// a propagation monitor is passed to the Solve() or NewSearch() method.
  void AddPropagationMonitor(PropagationMonitor* const monitor);
  /// Returns the local search monitor.
  LocalSearchMonitor* GetLocalSearchMonitor() const;
  /// Adds the local search monitor to the solver. This is called internally
  /// when a propagation monitor is passed to the Solve() or NewSearch() method.
  void AddLocalSearchMonitor(LocalSearchMonitor* monitor);
  void SetSearchContext(Search* search, const std::string& search_context);
  std::string SearchContext() const;
  std::string SearchContext(const Search* search) const;
  /// Returns (or creates) an assignment representing the state of local search.
  // TODO(user): Investigate if this should be moved to Search.
  Assignment* GetOrCreateLocalSearchState();
  /// Clears the local search state.
  void ClearLocalSearchState() { local_search_state_.reset(nullptr); }

  /// Unsafe temporary vector. It is used to avoid leaks in operations
  /// that need storage and that may fail. See IntVar::SetValues() for
  /// instance. It is not locked; do not use in a multi-threaded or reentrant
  /// setup.
  std::vector<int64_t> tmp_vector_;

  friend class BaseIntExpr;
  friend class Constraint;
  friend class DemonProfiler;
  friend class FindOneNeighbor;
  friend class IntVar;
  friend class PropagationBaseObject;
  friend class Queue;
  friend class SearchMonitor;
  friend class SearchLimit;
  friend class RoutingModel;
  friend class LocalSearchProfiler;

#if !defined(SWIG)
  friend void InternalSaveBooleanVarValue(Solver* const, IntVar* const);
  template <class>
  friend class SimpleRevFIFO;
  template <class K, class V>
  friend class RevImmutableMultiMap;

  /// Returns true if expr represents either boolean_var or 1 -
  /// boolean_var.  In that case, it fills inner_var and is_negated to be
  /// true if the expression is 1 - boolean_var -- equivalent to
  /// not(boolean_var).
  bool IsBooleanVar(IntExpr* const expr, IntVar** inner_var,
                    bool* is_negated) const;

  /// Returns true if expr represents a product of a expr and a
  /// constant.  In that case, it fills inner_expr and coefficient with
  /// these, and returns true. In the other case, it fills inner_expr
  /// with expr, coefficient with 1, and returns false.
  bool IsProduct(IntExpr* const expr, IntExpr** inner_expr,
                 int64_t* coefficient);
#endif  /// !defined(SWIG)

  /// Internal. If the variables is the result of expr->Var(), this
  /// method returns expr, nullptr otherwise.
  IntExpr* CastExpression(const IntVar* const var) const;

  /// Tells the solver to kill or restart the current search.
  void FinishCurrentSearch();
  void RestartCurrentSearch();

  /// These methods are only useful for the SWIG wrappers, which need a way
  /// to externally cause the Solver to fail.
  void ShouldFail() { should_fail_ = true; }
  void CheckFail() {
    if (!should_fail_) return;
    should_fail_ = false;
    Fail();
  }

  /// Activates profiling on a decision builder.
  DecisionBuilder* MakeProfiledDecisionBuilderWrapper(DecisionBuilder* db);

 private:
  void Init();  /// Initialization. To be called by the constructors only.
  void PushState(MarkerType t, const StateInfo& info);
  MarkerType PopState(StateInfo* info);
  void PushSentinel(int magic_code);
  void BacktrackToSentinel(int magic_code);
  void ProcessConstraints();
  bool BacktrackOneLevel(Decision** fail_decision);
  void JumpToSentinelWhenNested();
  void JumpToSentinel();
  void check_alloc_state();
  void FreezeQueue();
  void EnqueueVar(Demon* const d);
  void EnqueueDelayedDemon(Demon* const d);
  void ExecuteAll(const SimpleRevFIFO<Demon*>& demons);
  void EnqueueAll(const SimpleRevFIFO<Demon*>& demons);
  void UnfreezeQueue();
  void reset_action_on_fail();
  void set_action_on_fail(Action a);
  void set_variable_to_clean_on_fail(IntVar* v);
  void IncrementUncheckedSolutionCounter();
  bool IsUncheckedSolutionLimitReached();

  void InternalSaveValue(int* valptr);
  void InternalSaveValue(int64_t* valptr);
  void InternalSaveValue(uint64_t* valptr);
  void InternalSaveValue(double* valptr);
  void InternalSaveValue(bool* valptr);
  void InternalSaveValue(void** valptr);
  void InternalSaveValue(int64_t** valptr) {
    InternalSaveValue(reinterpret_cast<void**>(valptr));
  }

  BaseObject* SafeRevAlloc(BaseObject* ptr);

  int* SafeRevAllocArray(int* ptr);
  int64_t* SafeRevAllocArray(int64_t* ptr);
  uint64_t* SafeRevAllocArray(uint64_t* ptr);
  double* SafeRevAllocArray(double* ptr);
  BaseObject** SafeRevAllocArray(BaseObject** ptr);
  IntVar** SafeRevAllocArray(IntVar** ptr);
  IntExpr** SafeRevAllocArray(IntExpr** ptr);
  Constraint** SafeRevAllocArray(Constraint** ptr);
  /// UnsafeRevAlloc is used internally for cells in SimpleRevFIFO
  /// and other structures like this.
  void* UnsafeRevAllocAux(void* ptr);
  template <class T>
  T* UnsafeRevAlloc(T* ptr) {
    return reinterpret_cast<T*>(
        UnsafeRevAllocAux(reinterpret_cast<void*>(ptr)));
  }
  void** UnsafeRevAllocArrayAux(void** ptr);
  template <class T>
  T** UnsafeRevAllocArray(T** ptr) {
    return reinterpret_cast<T**>(
        UnsafeRevAllocArrayAux(reinterpret_cast<void**>(ptr)));
  }

  void InitCachedIntConstants();
  void InitCachedConstraint();

  /// Returns the Search object that is at the bottom of the search stack.
  /// Contrast with ActiveSearch(), which returns the search at the
  /// top of the stack.
  Search* TopLevelSearch() const { return searches_.at(1); }
  /// Returns the Search object which is the parent of the active search, i.e.,
  /// the search below the top of the stack. If the active search is at the
  /// bottom of the stack, returns the active search.
  Search* ParentSearch() const {
    const size_t search_size = searches_.size();
    DCHECK_GT(search_size, 1);
    return searches_[search_size - 2];
  }

  /// Naming
  std::string GetName(const PropagationBaseObject* object);
  void SetName(const PropagationBaseObject* object, const std::string& name);

  /// Variable indexing (note that indexing is not reversible).
  /// Returns a new index for an IntVar.
  int GetNewIntVarIndex() { return num_int_vars_++; }

  /// Internal.
  bool IsADifference(IntExpr* expr, IntExpr** const left,
                     IntExpr** const right);

  const std::string name_;
  const ConstraintSolverParameters parameters_;
  absl::flat_hash_map<const PropagationBaseObject*, std::string>
      propagation_object_names_;
  absl::flat_hash_map<const PropagationBaseObject*, IntegerCastInfo>
      cast_information_;
  absl::flat_hash_set<const Constraint*> cast_constraints_;
  const std::string empty_name_;
  std::unique_ptr<Queue> queue_;
  std::unique_ptr<Trail> trail_;
  std::vector<Constraint*> constraints_list_;
  std::vector<Constraint*> additional_constraints_list_;
  std::vector<int> additional_constraints_parent_list_;
  SolverState state_;
  int64_t branches_;
  int64_t fails_;
  int64_t decisions_;
  int64_t demon_runs_[kNumPriorities];
  int64_t neighbors_;
  int64_t filtered_neighbors_;
  int64_t accepted_neighbors_;
  std::string context_;
  OptimizationDirection optimization_direction_;
  std::unique_ptr<ClockTimer> timer_;
  std::vector<Search*> searches_;
  std::mt19937 random_;
  uint64_t fail_stamp_;
  std::unique_ptr<Decision> balancing_decision_;
  /// intercept failures
  std::function<void()> fail_intercept_;
  /// Demon monitor
  DemonProfiler* const demon_profiler_;
  /// Local search mode
  bool use_fast_local_search_;
  /// Local search profiler monitor
  LocalSearchProfiler* const local_search_profiler_;
  /// Local search state.
  std::unique_ptr<Assignment> local_search_state_;

  /// interval of constants cached, inclusive:
  enum { MIN_CACHED_INT_CONST = -8, MAX_CACHED_INT_CONST = 8 };
  IntVar* cached_constants_[MAX_CACHED_INT_CONST + 1 - MIN_CACHED_INT_CONST];

  /// Cached constraints.
  Constraint* true_constraint_;
  Constraint* false_constraint_;

  std::unique_ptr<Decision> fail_decision_;
  int constraint_index_;
  int additional_constraint_index_;
  int num_int_vars_;

  std::unique_ptr<ModelCache> model_cache_;
  std::unique_ptr<PropagationMonitor> propagation_monitor_;
  PropagationMonitor* print_trace_;
  std::unique_ptr<LocalSearchMonitor> local_search_monitor_;
  int anonymous_variable_index_;
  bool should_fail_;

  DISALLOW_COPY_AND_ASSIGN(Solver);
};

std::ostream& operator<<(std::ostream& out, const Solver* const s);  /// NOLINT

/// This method returns 0. It is useful when 0 can be cast either as
/// a pointer or as an integer value and thus lead to an ambiguous
/// function call.
inline int64_t Zero() { return 0; }

/// This method returns 1
inline int64_t One() { return 1; }

/// A BaseObject is the root of all reversibly allocated objects.
/// A DebugString method and the associated << operator are implemented
/// as a convenience.
class BaseObject {
 public:
  BaseObject() {}
  virtual ~BaseObject() {}
  virtual std::string DebugString() const { return "BaseObject"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(BaseObject);
};

std::ostream& operator<<(std::ostream& out, const BaseObject* o);  /// NOLINT

/// The PropagationBaseObject is a subclass of BaseObject that is also
/// friend to the Solver class. It allows accessing methods useful when
/// writing new constraints or new expressions.
class PropagationBaseObject : public BaseObject {
 public:
  explicit PropagationBaseObject(Solver* const s) : solver_(s) {}
  ~PropagationBaseObject() override {}

  std::string DebugString() const override {
    if (name().empty()) {
      return "PropagationBaseObject";
    } else {
      return absl::StrFormat("PropagationBaseObject: %s", name());
    }
  }
  Solver* solver() const { return solver_; }

  /// This method freezes the propagation queue. It is useful when you
  /// need to apply multiple modifications at once.
  void FreezeQueue() { solver_->FreezeQueue(); }

  /// This method unfreezes the propagation queue. All modifications
  /// that happened when the queue was frozen will be processed.
  void UnfreezeQueue() { solver_->UnfreezeQueue(); }

  /// This method pushes the demon onto the propagation queue. It will
  /// be processed directly if the queue is empty. It will be enqueued
  /// according to its priority otherwise.
  void EnqueueDelayedDemon(Demon* const d) { solver_->EnqueueDelayedDemon(d); }
  void EnqueueVar(Demon* const d) { solver_->EnqueueVar(d); }
  void ExecuteAll(const SimpleRevFIFO<Demon*>& demons);
  void EnqueueAll(const SimpleRevFIFO<Demon*>& demons);

#if !defined(SWIG)
  // This method sets a callback that will be called if a failure
  // happens during the propagation of the queue.
  void set_action_on_fail(Solver::Action a) {
    solver_->set_action_on_fail(std::move(a));
  }
#endif  // !defined(SWIG)

  /// This method clears the failure callback.
  void reset_action_on_fail() { solver_->reset_action_on_fail(); }

  /// Shortcut for variable cleaner.
  void set_variable_to_clean_on_fail(IntVar* v) {
    solver_->set_variable_to_clean_on_fail(v);
  }

  /// Object naming.
  virtual std::string name() const;
  void set_name(const std::string& name);
  /// Returns whether the object has been named or not.
  bool HasName() const;
  /// Returns a base name for automatic naming.
  virtual std::string BaseName() const;

 private:
  Solver* const solver_;
  DISALLOW_COPY_AND_ASSIGN(PropagationBaseObject);
};

/// A Decision represents a choice point in the search tree. The two main
/// methods are Apply() to go left, or Refute() to go right.
class Decision : public BaseObject {
 public:
  Decision() {}
  ~Decision() override {}

  /// Apply will be called first when the decision is executed.
  virtual void Apply(Solver* const s) = 0;

  /// Refute will be called after a backtrack.
  virtual void Refute(Solver* const s) = 0;

  std::string DebugString() const override { return "Decision"; }
  /// Accepts the given visitor.
  virtual void Accept(DecisionVisitor* const visitor) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(Decision);
};

/// A DecisionVisitor is used to inspect a decision.
/// It contains virtual methods for all type of 'declared' decisions.
class DecisionVisitor : public BaseObject {
 public:
  DecisionVisitor() {}
  ~DecisionVisitor() override {}
  virtual void VisitSetVariableValue(IntVar* const var, int64_t value);
  virtual void VisitSplitVariableDomain(IntVar* const var, int64_t value,
                                        bool start_with_lower_half);
  virtual void VisitScheduleOrPostpone(IntervalVar* const var, int64_t est);
  virtual void VisitScheduleOrExpedite(IntervalVar* const var, int64_t est);
  virtual void VisitRankFirstInterval(SequenceVar* const sequence, int index);
  virtual void VisitRankLastInterval(SequenceVar* const sequence, int index);
  virtual void VisitUnknownDecision();

 private:
  DISALLOW_COPY_AND_ASSIGN(DecisionVisitor);
};

/// A DecisionBuilder is responsible for creating the search tree. The
/// important method is Next(), which returns the next decision to execute.
class DecisionBuilder : public BaseObject {
 public:
  DecisionBuilder() {}
  ~DecisionBuilder() override {}
  /// This is the main method of the decision builder class. It must
  /// return a decision (an instance of the class Decision). If it
  /// returns nullptr, this means that the decision builder has finished
  /// its work.
  virtual Decision* Next(Solver* const s) = 0;
  std::string DebugString() const override;
#if !defined(SWIG)
  /// This method will be called at the start of the search.  It asks
  /// the decision builder if it wants to append search monitors to the
  /// list of active monitors for this search. Please note there are no
  /// checks at this point for duplication.
  virtual void AppendMonitors(Solver* const solver,
                              std::vector<SearchMonitor*>* const extras);
  virtual void Accept(ModelVisitor* const visitor) const;
#endif
  void set_name(const std::string& name) { name_ = name; }
  std::string GetName() const;

 private:
  std::string name_;
  DISALLOW_COPY_AND_ASSIGN(DecisionBuilder);
};

#if !defined(SWIG)
class ProfiledDecisionBuilder : public DecisionBuilder {
 public:
  explicit ProfiledDecisionBuilder(DecisionBuilder* db);
  ~ProfiledDecisionBuilder() override {}
  const std::string& name() const { return name_; }
  double seconds() const { return seconds_; }
  Decision* Next(Solver* const solver) override;
  std::string DebugString() const override;
  void AppendMonitors(Solver* const solver,
                      std::vector<SearchMonitor*>* const extras) override;
  void Accept(ModelVisitor* const visitor) const override;

 private:
  DecisionBuilder* const db_;
  const std::string name_;
  SimpleCycleTimer timer_;
  double seconds_;
};
#endif

/// A Demon is the base element of a propagation queue. It is the main
///   object responsible for implementing the actual propagation
///   of the constraint and pruning the inconsistent values in the domains
///   of the variables. The main concept is that demons are listeners that are
///   attached to the variables and listen to their modifications.
/// There are two methods:
///  - Run() is the actual method called when the demon is processed.
///  - priority() returns its priority. Standard priorities are slow, normal
///    or fast. "immediate" is reserved for variables and is treated separately.
class Demon : public BaseObject {
 public:
  /// This indicates the priority of a demon. Immediate demons are treated
  /// separately and corresponds to variables.
  Demon() : stamp_(uint64_t{0}) {}
  ~Demon() override {}

  /// This is the main callback of the demon.
  virtual void Run(Solver* const s) = 0;

  /// This method returns the priority of the demon. Usually a demon is
  /// fast, slow or normal. Immediate demons are reserved for internal
  /// use to maintain variables.
  virtual Solver::DemonPriority priority() const;

  std::string DebugString() const override;

  /// This method inhibits the demon in the search tree below the
  /// current position.
  void inhibit(Solver* const s);

  /// This method un-inhibits the demon that was previously inhibited.
  void desinhibit(Solver* const s);

 private:
  friend class Queue;
  void set_stamp(int64_t stamp) { stamp_ = stamp; }
  uint64_t stamp() const { return stamp_; }
  uint64_t stamp_;
  DISALLOW_COPY_AND_ASSIGN(Demon);
};

/// Model visitor.
class ModelVisitor : public BaseObject {
 public:
  /// Constraint and Expression types.
  static const char kAbs[];
  static const char kAbsEqual[];
  static const char kAllDifferent[];
  static const char kAllowedAssignments[];
  static const char kAtMost[];
  static const char kIndexOf[];
  static const char kBetween[];
  static const char kConditionalExpr[];
  static const char kCircuit[];
  static const char kConvexPiecewise[];
  static const char kCountEqual[];
  static const char kCover[];
  static const char kCumulative[];
  static const char kDeviation[];
  static const char kDifference[];
  static const char kDisjunctive[];
  static const char kDistribute[];
  static const char kDivide[];
  static const char kDurationExpr[];
  static const char kElement[];
  static const char kLightElementEqual[];
  static const char kElementEqual[];
  static const char kEndExpr[];
  static const char kEquality[];
  static const char kFalseConstraint[];
  static const char kGlobalCardinality[];
  static const char kGreater[];
  static const char kGreaterOrEqual[];
  static const char kIntegerVariable[];
  static const char kIntervalBinaryRelation[];
  static const char kIntervalDisjunction[];
  static const char kIntervalUnaryRelation[];
  static const char kIntervalVariable[];
  static const char kInversePermutation[];
  static const char kIsBetween[];
  static const char kIsDifferent[];
  static const char kIsEqual[];
  static const char kIsGreater[];
  static const char kIsGreaterOrEqual[];
  static const char kIsLess[];
  static const char kIsLessOrEqual[];
  static const char kIsMember[];
  static const char kLess[];
  static const char kLessOrEqual[];
  static const char kLexLess[];
  static const char kLinkExprVar[];
  static const char kMapDomain[];
  static const char kMax[];
  static const char kMaxEqual[];
  static const char kMember[];
  static const char kMin[];
  static const char kMinEqual[];
  static const char kModulo[];
  static const char kNoCycle[];
  static const char kNonEqual[];
  static const char kNotBetween[];
  static const char kNotMember[];
  static const char kNullIntersect[];
  static const char kOpposite[];
  static const char kPack[];
  static const char kPathCumul[];
  static const char kDelayedPathCumul[];
  static const char kPerformedExpr[];
  static const char kPower[];
  static const char kProduct[];
  static const char kScalProd[];
  static const char kScalProdEqual[];
  static const char kScalProdGreaterOrEqual[];
  static const char kScalProdLessOrEqual[];
  static const char kSemiContinuous[];
  static const char kSequenceVariable[];
  static const char kSortingConstraint[];
  static const char kSquare[];
  static const char kStartExpr[];
  static const char kSum[];
  static const char kSumEqual[];
  static const char kSumGreaterOrEqual[];
  static const char kSumLessOrEqual[];
  static const char kTrace[];
  static const char kTransition[];
  static const char kTrueConstraint[];
  static const char kVarBoundWatcher[];
  static const char kVarValueWatcher[];

  /// Extension names:
  static const char kCountAssignedItemsExtension[];
  static const char kCountUsedBinsExtension[];
  static const char kInt64ToBoolExtension[];
  static const char kInt64ToInt64Extension[];
  static const char kObjectiveExtension[];
  static const char kSearchLimitExtension[];
  static const char kUsageEqualVariableExtension[];

  static const char kUsageLessConstantExtension[];
  static const char kVariableGroupExtension[];
  static const char kVariableUsageLessConstantExtension[];
  static const char kWeightedSumOfAssignedEqualVariableExtension[];

  /// argument names:
  static const char kActiveArgument[];
  static const char kAssumePathsArgument[];
  static const char kBranchesLimitArgument[];
  static const char kCapacityArgument[];
  static const char kCardsArgument[];
  static const char kCoefficientsArgument[];
  static const char kCountArgument[];
  static const char kCumulativeArgument[];
  static const char kCumulsArgument[];
  static const char kDemandsArgument[];
  static const char kDurationMaxArgument[];
  static const char kDurationMinArgument[];
  static const char kEarlyCostArgument[];
  static const char kEarlyDateArgument[];
  static const char kEndMaxArgument[];
  static const char kEndMinArgument[];
  static const char kEndsArgument[];
  static const char kExpressionArgument[];
  static const char kFailuresLimitArgument[];
  static const char kFinalStatesArgument[];
  static const char kFixedChargeArgument[];
  static const char kIndex2Argument[];
  static const char kIndexArgument[];
  static const char kInitialState[];
  static const char kIntervalArgument[];
  static const char kIntervalsArgument[];
  static const char kLateCostArgument[];
  static const char kLateDateArgument[];
  static const char kLeftArgument[];
  static const char kMaxArgument[];
  static const char kMaximizeArgument[];
  static const char kMinArgument[];
  static const char kModuloArgument[];
  static const char kNextsArgument[];
  static const char kOptionalArgument[];
  static const char kPartialArgument[];
  static const char kPositionXArgument[];
  static const char kPositionYArgument[];
  static const char kRangeArgument[];
  static const char kRelationArgument[];
  static const char kRightArgument[];
  static const char kSequenceArgument[];
  static const char kSequencesArgument[];
  static const char kSizeArgument[];
  static const char kSizeXArgument[];
  static const char kSizeYArgument[];
  static const char kSmartTimeCheckArgument[];
  static const char kSolutionLimitArgument[];
  static const char kStartMaxArgument[];
  static const char kStartMinArgument[];
  static const char kStartsArgument[];
  static const char kStepArgument[];
  static const char kTargetArgument[];
  static const char kTimeLimitArgument[];
  static const char kTransitsArgument[];
  static const char kTuplesArgument[];
  static const char kValueArgument[];
  static const char kValuesArgument[];
  static const char kVariableArgument[];
  static const char kVarsArgument[];
  static const char kEvaluatorArgument[];

  /// Operations.
  static const char kMirrorOperation[];
  static const char kRelaxedMaxOperation[];
  static const char kRelaxedMinOperation[];
  static const char kSumOperation[];
  static const char kDifferenceOperation[];
  static const char kProductOperation[];
  static const char kStartSyncOnStartOperation[];
  static const char kStartSyncOnEndOperation[];
  static const char kTraceOperation[];

  ~ModelVisitor() override;

  /// ----- Virtual methods for visitors -----

  /// Begin/End visit element.
  virtual void BeginVisitModel(const std::string& type_name);
  virtual void EndVisitModel(const std::string& type_name);
  virtual void BeginVisitConstraint(const std::string& type_name,
                                    const Constraint* const constraint);
  virtual void EndVisitConstraint(const std::string& type_name,
                                  const Constraint* const constraint);
  virtual void BeginVisitExtension(const std::string& type);
  virtual void EndVisitExtension(const std::string& type);
  virtual void BeginVisitIntegerExpression(const std::string& type_name,
                                           const IntExpr* const expr);
  virtual void EndVisitIntegerExpression(const std::string& type_name,
                                         const IntExpr* const expr);
  virtual void VisitIntegerVariable(const IntVar* const variable,
                                    IntExpr* const delegate);
  virtual void VisitIntegerVariable(const IntVar* const variable,
                                    const std::string& operation, int64_t value,
                                    IntVar* const delegate);
  virtual void VisitIntervalVariable(const IntervalVar* const variable,
                                     const std::string& operation,
                                     int64_t value,
                                     IntervalVar* const delegate);
  virtual void VisitSequenceVariable(const SequenceVar* const variable);

  /// Visit integer arguments.
  virtual void VisitIntegerArgument(const std::string& arg_name, int64_t value);
  virtual void VisitIntegerArrayArgument(const std::string& arg_name,
                                         const std::vector<int64_t>& values);
  virtual void VisitIntegerMatrixArgument(const std::string& arg_name,
                                          const IntTupleSet& tuples);

  /// Visit integer expression argument.
  virtual void VisitIntegerExpressionArgument(const std::string& arg_name,
                                              IntExpr* const argument);

  virtual void VisitIntegerVariableArrayArgument(
      const std::string& arg_name, const std::vector<IntVar*>& arguments);

  /// Visit interval argument.
  virtual void VisitIntervalArgument(const std::string& arg_name,
                                     IntervalVar* const argument);

  virtual void VisitIntervalArrayArgument(
      const std::string& arg_name, const std::vector<IntervalVar*>& arguments);
  /// Visit sequence argument.
  virtual void VisitSequenceArgument(const std::string& arg_name,
                                     SequenceVar* const argument);

  virtual void VisitSequenceArrayArgument(
      const std::string& arg_name, const std::vector<SequenceVar*>& arguments);
#if !defined(SWIG)
  /// Helpers.
  virtual void VisitIntegerVariableEvaluatorArgument(
      const std::string& arg_name, const Solver::Int64ToIntVar& arguments);

  /// Using SWIG on callbacks is troublesome, so we hide these methods during
  /// the wrapping.
  void VisitInt64ToBoolExtension(Solver::IndexFilter1 filter, int64_t index_min,
                                 int64_t index_max);
  void VisitInt64ToInt64Extension(const Solver::IndexEvaluator1& eval,
                                  int64_t index_min, int64_t index_max);
  /// Expands function as array when index min is 0.
  void VisitInt64ToInt64AsArray(const Solver::IndexEvaluator1& eval,
                                const std::string& arg_name, int64_t index_max);
#endif  // #if !defined(SWIG)
};

/// A constraint is the main modeling object. It provides two methods:
///   - Post() is responsible for creating the demons and attaching them to
///     immediate demons().
///   - InitialPropagate() is called once just after Post and performs
///     the initial propagation. The subsequent propagations will be performed
///     by the demons Posted during the post() method.
class Constraint : public PropagationBaseObject {
 public:
  explicit Constraint(Solver* const solver) : PropagationBaseObject(solver) {}
  ~Constraint() override {}

  /// This method is called when the constraint is processed by the
  /// solver. Its main usage is to attach demons to variables.
  virtual void Post() = 0;

  /// This method performs the initial propagation of the
  /// constraint. It is called just after the post.
  virtual void InitialPropagate() = 0;
  std::string DebugString() const override;

  /// Calls Post and then Propagate to initialize the constraints. This
  /// is usually done in the root node.
  void PostAndPropagate();

  /// Accepts the given visitor.
  virtual void Accept(ModelVisitor* const visitor) const;

  /// Is the constraint created by a cast from expression to integer variable?
  bool IsCastConstraint() const;

  /// Creates a Boolean variable representing the status of the constraint
  /// (false = constraint is violated, true = constraint is satisfied). It
  /// returns nullptr if the constraint does not support this API.
  virtual IntVar* Var();

 private:
  DISALLOW_COPY_AND_ASSIGN(Constraint);
};

/// Cast constraints are special channeling constraints designed
/// to keep a variable in sync with an expression.  They are
/// created internally when Var() is called on a subclass of IntExpr.
class CastConstraint : public Constraint {
 public:
  CastConstraint(Solver* const solver, IntVar* const target_var)
      : Constraint(solver), target_var_(target_var) {
    CHECK(target_var != nullptr);
  }
  ~CastConstraint() override {}

  IntVar* target_var() const { return target_var_; }

 protected:
  IntVar* const target_var_;
};

/// A search monitor is a simple set of callbacks to monitor all search events
class SearchMonitor : public BaseObject {
 public:
  static constexpr int kNoProgress = -1;

  explicit SearchMonitor(Solver* const s) : solver_(s) {}
  ~SearchMonitor() override {}
  /// Beginning of the search.
  virtual void EnterSearch();

  /// Restart the search.
  virtual void RestartSearch();

  /// End of the search.
  virtual void ExitSearch();

  /// Before calling DecisionBuilder::Next.
  virtual void BeginNextDecision(DecisionBuilder* const b);

  /// After calling DecisionBuilder::Next, along with the returned decision.
  virtual void EndNextDecision(DecisionBuilder* const b, Decision* const d);

  /// Before applying the decision.
  virtual void ApplyDecision(Decision* const d);

  /// Before refuting the decision.
  virtual void RefuteDecision(Decision* const d);

  /// Just after refuting or applying the decision, apply is true after Apply.
  /// This is called only if the Apply() or Refute() methods have not failed.
  virtual void AfterDecision(Decision* const d, bool apply);

  /// Just when the failure occurs.
  virtual void BeginFail();

  /// After completing the backtrack.
  virtual void EndFail();

  /// Before the initial propagation.
  virtual void BeginInitialPropagation();

  /// After the initial propagation.
  virtual void EndInitialPropagation();

  /// This method is called when a solution is found. It asserts whether the
  /// solution is valid. A value of false indicates that the solution
  /// should be discarded.
  virtual bool AcceptSolution();

  /// This method is called when a valid solution is found. If the
  /// return value is true, then search will resume after. If the result
  /// is false, then search will stop there.
  virtual bool AtSolution();

  /// When the search tree is finished.
  virtual void NoMoreSolutions();

  /// When a local optimum is reached. If 'true' is returned, the last solution
  /// is discarded and the search proceeds with the next one.
  virtual bool LocalOptimum();

  ///
  virtual bool AcceptDelta(Assignment* delta, Assignment* deltadelta);

  /// After accepting a neighbor during local search.
  virtual void AcceptNeighbor();

  /// After accepting an unchecked neighbor during local search.
  virtual void AcceptUncheckedNeighbor();

  /// Returns true if the limit of solutions has been reached including
  /// unchecked solutions.
  virtual bool IsUncheckedSolutionLimitReached() { return false; }

  /// Periodic call to check limits in long running methods.
  virtual void PeriodicCheck();

  /// Returns a percentage representing the propress of the search before
  /// reaching limits.
  virtual int ProgressPercent() { return kNoProgress; }

  /// Accepts the given model visitor.
  virtual void Accept(ModelVisitor* const visitor) const;

  /// Registers itself on the solver such that it gets notified of the search
  /// and propagation events. Override to incrementally install listeners for
  /// specific events.
  virtual void Install();

  Solver* solver() const { return solver_; }

 protected:
  void ListenToEvent(Solver::MonitorEvent event);

 private:
  Solver* const solver_;
  DISALLOW_COPY_AND_ASSIGN(SearchMonitor);
};

/// This class adds reversibility to a POD type.
/// It contains the stamp optimization. i.e. the SaveValue call is done
/// only once per node of the search tree.  Please note that actual
/// stamps always starts at 1, thus an initial value of 0 will always
/// trigger the first SaveValue.
template <class T>
class Rev {
 public:
  explicit Rev(const T& val) : stamp_(0), value_(val) {}

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
  uint64_t stamp_;
  T value_;
};

/// Subclass of Rev<T> which adds numerical operations.
template <class T>
class NumericalRev : public Rev<T> {
 public:
  explicit NumericalRev(const T& val) : Rev<T>(val) {}

  void Add(Solver* const s, const T& to_add) {
    this->SetValue(s, this->Value() + to_add);
  }

  void Incr(Solver* const s) { Add(s, 1); }

  void Decr(Solver* const s) { Add(s, -1); }
};

/// Reversible array of POD types.
/// It contains the stamp optimization. I.e., the SaveValue call is done only
/// once per node of the search tree.
/// Please note that actual stamp always starts at 1, thus an initial value of
/// 0 always triggers the first SaveValue.
template <class T>
class RevArray {
 public:
  RevArray(int size, const T& val)
      : stamps_(new uint64_t[size]), values_(new T[size]), size_(size) {
    for (int i = 0; i < size; ++i) {
      stamps_[i] = 0;
      values_[i] = val;
    }
  }

  ~RevArray() {}

  int64_t size() const { return size_; }

  const T& Value(int index) const { return values_[index]; }

#if !defined(SWIG)
  const T& operator[](int index) const { return values_[index]; }
#endif

  void SetValue(Solver* const s, int index, const T& val) {
    DCHECK_LT(index, size_);
    if (val != values_[index]) {
      if (stamps_[index] < s->stamp()) {
        s->SaveValue(&values_[index]);
        stamps_[index] = s->stamp();
      }
      values_[index] = val;
    }
  }

 private:
  std::unique_ptr<uint64_t[]> stamps_;
  std::unique_ptr<T[]> values_;
  const int size_;
};

/// Subclass of RevArray<T> which adds numerical operations.
template <class T>
class NumericalRevArray : public RevArray<T> {
 public:
  NumericalRevArray(int size, const T& val) : RevArray<T>(size, val) {}

  void Add(Solver* const s, int index, const T& to_add) {
    this->SetValue(s, index, this->Value(index) + to_add);
  }

  void Incr(Solver* const s, int index) { Add(s, index, 1); }

  void Decr(Solver* const s, int index) { Add(s, index, -1); }
};

/// The class IntExpr is the base of all integer expressions in
/// constraint programming.
/// It contains the basic protocol for an expression:
///   - setting and modifying its bound
///   - querying if it is bound
///   - listening to events modifying its bounds
///   - casting it into a variable (instance of IntVar)
class IntExpr : public PropagationBaseObject {
 public:
  explicit IntExpr(Solver* const s) : PropagationBaseObject(s) {}
  ~IntExpr() override {}

  virtual int64_t Min() const = 0;
  virtual void SetMin(int64_t m) = 0;
  virtual int64_t Max() const = 0;
  virtual void SetMax(int64_t m) = 0;

  /// By default calls Min() and Max(), but can be redefined when Min and Max
  /// code can be factorized.
  virtual void Range(int64_t* l, int64_t* u) {
    *l = Min();
    *u = Max();
  }
  /// This method sets both the min and the max of the expression.
  virtual void SetRange(int64_t l, int64_t u) {
    SetMin(l);
    SetMax(u);
  }

  /// This method sets the value of the expression.
  virtual void SetValue(int64_t v) { SetRange(v, v); }

  /// Returns true if the min and the max of the expression are equal.
  virtual bool Bound() const { return (Min() == Max()); }

  /// Returns true if the expression is indeed a variable.
  virtual bool IsVar() const { return false; }

  /// Creates a variable from the expression.
  virtual IntVar* Var() = 0;

  /// Creates a variable from the expression and set the name of the
  /// resulting var. If the expression is already a variable, then it
  /// will set the name of the expression, possibly overwriting it.
  /// This is just a shortcut to Var() followed by set_name().
  IntVar* VarWithName(const std::string& name);

  /// Attach a demon that will watch the min or the max of the expression.
  virtual void WhenRange(Demon* d) = 0;
  /// Attach a demon that will watch the min or the max of the expression.
  void WhenRange(Solver::Closure closure) {
    WhenRange(solver()->MakeClosureDemon(std::move(closure)));
  }

#if !defined(SWIG)
  /// Attach a demon that will watch the min or the max of the expression.
  void WhenRange(Solver::Action action) {
    WhenRange(solver()->MakeActionDemon(std::move(action)));
  }
#endif  // SWIG

  /// Accepts the given visitor.
  virtual void Accept(ModelVisitor* const visitor) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(IntExpr);
};

/// The class Iterator has two direct subclasses. HoleIterators
/// iterates over all holes, that is value removed between the
/// current min and max of the variable since the last time the
/// variable was processed in the queue. DomainIterators iterates
/// over all elements of the variable domain. Both iterators are not
/// robust to domain changes. Hole iterators can also report values outside
/// the current min and max of the variable.

/// HoleIterators should only be called from a demon attached to the
/// variable that has created this iterator.

/// IntVar* current_var;
/// std::unique_ptr<IntVarIterator> it(current_var->MakeHoleIterator(false));
/// for (const int64_t hole : InitAndGetValues(it)) {
///   /// use the hole
/// }

class IntVarIterator : public BaseObject {
 public:
  ~IntVarIterator() override {}

  /// This method must be called before each loop.
  virtual void Init() = 0;

  /// This method indicates if we can call Value() or not.
  virtual bool Ok() const = 0;

  /// This method returns the current value of the iterator.
  virtual int64_t Value() const = 0;

  /// This method moves the iterator to the next value.
  virtual void Next() = 0;

  /// Pretty Print.
  std::string DebugString() const override { return "IntVar::Iterator"; }
};

#ifndef SWIG
/// Utility class to encapsulate an IntVarIterator and use it in a range-based
/// loop. See the code snippet above IntVarIterator.
///
/// It contains DEBUG_MODE-enabled code that DCHECKs that the
/// same iterator instance isn't being iterated on in multiple places
/// simultaneously.
class InitAndGetValues {
 public:
  explicit InitAndGetValues(IntVarIterator* it)
      : it_(it), begin_was_called_(false) {
    it_->Init();
  }
  struct Iterator;

  Iterator begin() {
    if (DEBUG_MODE) {
      DCHECK(!begin_was_called_);
      begin_was_called_ = true;
    }
    return Iterator::Begin(it_);
  }
  Iterator end() { return Iterator::End(it_); }

  struct Iterator {
    /// These are the only way to construct an Iterator.
    static Iterator Begin(IntVarIterator* it) {
      return Iterator(it, /*is_end=*/false);
    }
    static Iterator End(IntVarIterator* it) {
      return Iterator(it, /*is_end=*/true);
    }

    int64_t operator*() const {
      DCHECK(it_->Ok());
      return it_->Value();
    }
    Iterator& operator++() {
      DCHECK(it_->Ok());
      it_->Next();
      return *this;
    }
    bool operator!=(const Iterator& other) const {
      DCHECK(other.it_ == it_);
      DCHECK(other.is_end_);
      return it_->Ok();
    }

   private:
    Iterator(IntVarIterator* it, bool is_end) : it_(it), is_end_(is_end) {}

    IntVarIterator* const it_;
    const bool is_end_;
  };

 private:
  IntVarIterator* const it_;
  bool begin_was_called_;
};
#endif  // SWIG

/// The class IntVar is a subset of IntExpr. In addition to the
/// IntExpr protocol, it offers persistence, removing values from the domains,
/// and a finer model for events.
class IntVar : public IntExpr {
 public:
  explicit IntVar(Solver* const s);
  IntVar(Solver* const s, const std::string& name);
  ~IntVar() override {}

  bool IsVar() const override { return true; }
  IntVar* Var() override { return this; }

  /// This method returns the value of the variable. This method checks
  /// before that the variable is bound.
  virtual int64_t Value() const = 0;

  /// This method removes the value 'v' from the domain of the variable.
  virtual void RemoveValue(int64_t v) = 0;

  /// This method removes the interval 'l' .. 'u' from the domain of
  /// the variable. It assumes that 'l' <= 'u'.
  virtual void RemoveInterval(int64_t l, int64_t u) = 0;

  /// This method remove the values from the domain of the variable.
  virtual void RemoveValues(const std::vector<int64_t>& values);

  /// This method intersects the current domain with the values in the array.
  virtual void SetValues(const std::vector<int64_t>& values);

  /// This method attaches a demon that will be awakened when the
  /// variable is bound.
  virtual void WhenBound(Demon* d) = 0;
  /// This method attaches a closure that will be awakened when the
  /// variable is bound.
  void WhenBound(Solver::Closure closure) {
    WhenBound(solver()->MakeClosureDemon(std::move(closure)));
  }

#if !defined(SWIG)
  /// This method attaches an action that will be awakened when the
  /// variable is bound.
  void WhenBound(Solver::Action action) {
    WhenBound(solver()->MakeActionDemon(std::move(action)));
  }
#endif  // SWIG

  /// This method attaches a demon that will watch any domain
  /// modification of the domain of the variable.
  virtual void WhenDomain(Demon* d) = 0;
  /// This method attaches a closure that will watch any domain
  /// modification of the domain of the variable.
  void WhenDomain(Solver::Closure closure) {
    WhenDomain(solver()->MakeClosureDemon(std::move(closure)));
  }
#if !defined(SWIG)
  /// This method attaches an action that will watch any domain
  /// modification of the domain of the variable.
  void WhenDomain(Solver::Action action) {
    WhenDomain(solver()->MakeActionDemon(std::move(action)));
  }
#endif  // SWIG

  /// This method returns the number of values in the domain of the variable.
  virtual uint64_t Size() const = 0;

  /// This method returns whether the value 'v' is in the domain of the
  /// variable.
  virtual bool Contains(int64_t v) const = 0;

  /// Creates a hole iterator. When 'reversible' is false, the returned
  /// object is created on the normal C++ heap and the solver does NOT
  /// take ownership of the object.
  virtual IntVarIterator* MakeHoleIterator(bool reversible) const = 0;

  /// Creates a domain iterator. When 'reversible' is false, the
  /// returned object is created on the normal C++ heap and the solver
  /// does NOT take ownership of the object.
  virtual IntVarIterator* MakeDomainIterator(bool reversible) const = 0;

  /// Returns the previous min.
  virtual int64_t OldMin() const = 0;

  /// Returns the previous max.
  virtual int64_t OldMax() const = 0;

  virtual int VarType() const;

  /// Accepts the given visitor.
  void Accept(ModelVisitor* const visitor) const override;

  /// IsEqual
  virtual IntVar* IsEqual(int64_t constant) = 0;
  virtual IntVar* IsDifferent(int64_t constant) = 0;
  virtual IntVar* IsGreaterOrEqual(int64_t constant) = 0;
  virtual IntVar* IsLessOrEqual(int64_t constant) = 0;

  /// Returns the index of the variable.
  int index() const { return index_; }

 private:
  const int index_;
  DISALLOW_COPY_AND_ASSIGN(IntVar);
};

/// This class is the root class of all solution collectors.
/// It implements a basic query API to be used independently
/// of the collector used.
class SolutionCollector : public SearchMonitor {
 public:
  SolutionCollector(Solver* const solver, const Assignment* assignment);
  explicit SolutionCollector(Solver* const solver);
  ~SolutionCollector() override;
  void Install() override;
  std::string DebugString() const override { return "SolutionCollector"; }

  /// Add API.
  void Add(IntVar* const var);
  void Add(const std::vector<IntVar*>& vars);
  void Add(IntervalVar* const var);
  void Add(const std::vector<IntervalVar*>& vars);
  void Add(SequenceVar* const var);
  void Add(const std::vector<SequenceVar*>& vars);
  void AddObjective(IntVar* const objective);

  /// Beginning of the search.
  void EnterSearch() override;

  /// Returns how many solutions were stored during the search.
  int solution_count() const;

  /// Returns the nth solution.
  Assignment* solution(int n) const;

  /// Returns the wall time in ms for the nth solution.
  int64_t wall_time(int n) const;

  /// Returns the number of branches when the nth solution was found.
  int64_t branches(int n) const;

  /// Returns the number of failures encountered at the time of the nth
  /// solution.
  int64_t failures(int n) const;

  /// Returns the objective value of the nth solution.
  int64_t objective_value(int n) const;

  /// This is a shortcut to get the Value of 'var' in the nth solution.
  int64_t Value(int n, IntVar* const var) const;

  /// This is a shortcut to get the StartValue of 'var' in the nth solution.
  int64_t StartValue(int n, IntervalVar* const var) const;

  /// This is a shortcut to get the EndValue of 'var' in the nth solution.
  int64_t EndValue(int n, IntervalVar* const var) const;

  /// This is a shortcut to get the DurationValue of 'var' in the nth solution.
  int64_t DurationValue(int n, IntervalVar* const var) const;

  /// This is a shortcut to get the PerformedValue of 'var' in the nth solution.
  int64_t PerformedValue(int n, IntervalVar* const var) const;

  /// This is a shortcut to get the ForwardSequence of 'var' in the
  /// nth solution. The forward sequence is the list of ranked interval
  /// variables starting from the start of the sequence.
  const std::vector<int>& ForwardSequence(int n, SequenceVar* const var) const;
  /// This is a shortcut to get the BackwardSequence of 'var' in the
  /// nth solution. The backward sequence is the list of ranked interval
  /// variables starting from the end of the sequence.
  const std::vector<int>& BackwardSequence(int n, SequenceVar* const var) const;
  /// This is a shortcut to get the list of unperformed of 'var' in the
  /// nth solution.
  const std::vector<int>& Unperformed(int n, SequenceVar* const var) const;

 protected:
  struct SolutionData {
    Assignment* solution;
    int64_t time;
    int64_t branches;
    int64_t failures;
    int64_t objective_value;
    bool operator<(const SolutionData& other) const {
      return std::tie(solution, time, branches, failures, objective_value) <
             std::tie(other.solution, other.time, other.branches,
                      other.failures, other.objective_value);
    }
  };

  /// Push the current state as a new solution.
  void PushSolution();
  void Push(const SolutionData& data) { solution_data_.push_back(data); }
  /// Remove and delete the last popped solution.
  void PopSolution();
  SolutionData BuildSolutionDataForCurrentState();
  void FreeSolution(Assignment* solution);
  void check_index(int n) const;

  std::unique_ptr<Assignment> prototype_;
  std::vector<SolutionData> solution_data_;
  std::vector<Assignment*> recycle_solutions_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SolutionCollector);
};

// TODO(user): Refactor this into an Objective class:
//   - print methods for AtNode and AtSolution.
//   - support for weighted objective and lexicographical objective.

/// This class encapsulates an objective. It requires the direction
/// (minimize or maximize), the variable to optimize, and the
/// improvement step.
class OptimizeVar : public SearchMonitor {
 public:
  OptimizeVar(Solver* const s, bool maximize, IntVar* const a, int64_t step);
  ~OptimizeVar() override;

  /// Returns the best value found during search.
  int64_t best() const { return best_; }

  /// Returns the variable that is optimized.
  IntVar* Var() const { return var_; }
  /// Internal methods.
  bool AcceptDelta(Assignment* delta, Assignment* deltadelta) override;
  void EnterSearch() override;
  void BeginNextDecision(DecisionBuilder* const db) override;
  void RefuteDecision(Decision* const d) override;
  bool AtSolution() override;
  bool AcceptSolution() override;
  virtual std::string Print() const;
  std::string DebugString() const override;
  void Accept(ModelVisitor* const visitor) const override;

  void ApplyBound();

 protected:
  IntVar* const var_;
  int64_t step_;
  int64_t best_;
  bool maximize_;
  bool found_initial_solution_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OptimizeVar);
};

/// Base class of all search limits.
class SearchLimit : public SearchMonitor {
 public:
  explicit SearchLimit(Solver* const s) : SearchMonitor(s), crossed_(false) {}
  ~SearchLimit() override;

  /// Returns true if the limit has been crossed.
  bool crossed() const { return crossed_; }

  /// This method is called to check the status of the limit. A return
  /// value of true indicates that we have indeed crossed the limit. In
  /// that case, this method will not be called again and the remaining
  /// search will be discarded.
  bool Check() { return CheckWithOffset(absl::ZeroDuration()); }
  /// Same as Check() but adds the 'offset' value to the current time when time
  /// is considered in the limit.
  virtual bool CheckWithOffset(absl::Duration offset) = 0;

  /// This method is called when the search limit is initialized.
  virtual void Init() = 0;

  /// Copy a limit. Warning: leads to a direct (no check) downcasting of 'limit'
  /// so one needs to be sure both SearchLimits are of the same type.
  virtual void Copy(const SearchLimit* const limit) = 0;

  /// Allocates a clone of the limit.
  virtual SearchLimit* MakeClone() const = 0;

  /// Internal methods.
  void EnterSearch() override;
  void BeginNextDecision(DecisionBuilder* const b) override;
  void PeriodicCheck() override;
  void RefuteDecision(Decision* const d) override;
  std::string DebugString() const override {
    return absl::StrFormat("SearchLimit(crossed = %i)", crossed_);
  }
  void Install() override;

 private:
  void TopPeriodicCheck();

  bool crossed_;
  DISALLOW_COPY_AND_ASSIGN(SearchLimit);
};

/// Usual limit based on wall_time, number of explored branches and
/// number of failures in the search tree
class RegularLimit : public SearchLimit {
 public:
  RegularLimit(Solver* const s, absl::Duration time, int64_t branches,
               int64_t failures, int64_t solutions, bool smart_time_check,
               bool cumulative);
  ~RegularLimit() override;
  void Copy(const SearchLimit* const limit) override;
  SearchLimit* MakeClone() const override;
  RegularLimit* MakeIdenticalClone() const;
  bool CheckWithOffset(absl::Duration offset) override;
  void Init() override;
  void ExitSearch() override;
  void UpdateLimits(absl::Duration time, int64_t branches, int64_t failures,
                    int64_t solutions);
  absl::Duration duration_limit() const { return duration_limit_; }
  int64_t wall_time() const {
    return duration_limit_ == absl::InfiniteDuration()
               ? kint64max
               : absl::ToInt64Milliseconds(duration_limit());
  }
  int64_t branches() const { return branches_; }
  int64_t failures() const { return failures_; }
  int64_t solutions() const { return solutions_; }
  bool IsUncheckedSolutionLimitReached() override;
  int ProgressPercent() override;
  std::string DebugString() const override;
  void Install() override;

  absl::Time AbsoluteSolverDeadline() const {
    return solver_time_at_limit_start_ + duration_limit_;
  }

  void Accept(ModelVisitor* const visitor) const override;

 private:
  bool CheckTime(absl::Duration offset);
  absl::Duration TimeElapsed();
  static int64_t GetPercent(int64_t value, int64_t offset, int64_t total) {
    return (total > 0 && total < kint64max) ? 100 * (value - offset) / total
                                            : -1;
  }

  absl::Duration duration_limit_;
  absl::Time solver_time_at_limit_start_;
  absl::Duration last_time_elapsed_;
  int64_t check_count_;
  int64_t next_check_;
  bool smart_time_check_;
  int64_t branches_;
  int64_t branches_offset_;
  int64_t failures_;
  int64_t failures_offset_;
  int64_t solutions_;
  int64_t solutions_offset_;
  /// If cumulative if false, then the limit applies to each search
  /// independently. If it's true, the limit applies globally to all search for
  /// which this monitor is used.
  /// When cumulative is true, the offset fields have two different meanings
  /// depending on context:
  /// - within a search, it's an offset to be subtracted from the current value
  /// - outside of search, it's the amount consumed in previous searches
  bool cumulative_;
};

// Limit based on the improvement rate of 'objective_var'.
// This limit proceeds in two stages:
// 1) During the phase of the search in which the objective_var is strictly
// improving, a threshold value is computed as the minimum improvement rate of
// the objective, based on the 'improvement_rate_coefficient' and
// 'improvement_rate_solutions_distance' parameters.
// 2) Then, if the search continues beyond this phase of strict improvement, the
// limit stops the search when the improvement rate of the objective gets below
// this threshold value.
class ImprovementSearchLimit : public SearchLimit {
 public:
  ImprovementSearchLimit(Solver* const s, IntVar* objective_var, bool maximize,
                         double objective_scaling_factor,
                         double objective_offset,
                         double improvement_rate_coefficient,
                         int improvement_rate_solutions_distance);
  ~ImprovementSearchLimit() override;
  void Copy(const SearchLimit* const limit) override;
  SearchLimit* MakeClone() const override;
  bool CheckWithOffset(absl::Duration offset) override;
  bool AtSolution() override;
  void Init() override;
  void Install() override;

 private:
  IntVar* objective_var_;
  bool maximize_;
  double objective_scaling_factor_;
  double objective_offset_;
  double improvement_rate_coefficient_;
  int improvement_rate_solutions_distance_;

  double best_objective_;
  // clang-format off
  std::deque<std::pair<double, int64_t> > improvements_;
  // clang-format on
  double threshold_;
  bool objective_updated_;
  bool gradient_stage_;
};

/// Interval variables are often used in scheduling. The main characteristics
/// of an IntervalVar are the start position, duration, and end
/// date. All these characteristics can be queried and set, and demons can
/// be posted on their modifications.
///
/// An important aspect is optionality: an IntervalVar can be performed or not.
/// If unperformed, then it simply does not exist, and its characteristics
/// cannot be accessed any more. An interval var is automatically marked
/// as unperformed when it is not consistent anymore (start greater
/// than end, duration < 0...)
class IntervalVar : public PropagationBaseObject {
 public:
  /// The smallest acceptable value to be returned by StartMin()
  static const int64_t kMinValidValue;
  /// The largest acceptable value to be returned by EndMax()
  static const int64_t kMaxValidValue;
  IntervalVar(Solver* const solver, const std::string& name)
      : PropagationBaseObject(solver) {
    set_name(name);
  }
  ~IntervalVar() override {}

  /// These methods query, set, and watch the start position of the
  /// interval var.
  virtual int64_t StartMin() const = 0;
  virtual int64_t StartMax() const = 0;
  virtual void SetStartMin(int64_t m) = 0;
  virtual void SetStartMax(int64_t m) = 0;
  virtual void SetStartRange(int64_t mi, int64_t ma) = 0;
  virtual int64_t OldStartMin() const = 0;
  virtual int64_t OldStartMax() const = 0;
  virtual void WhenStartRange(Demon* const d) = 0;
  void WhenStartRange(Solver::Closure closure) {
    WhenStartRange(solver()->MakeClosureDemon(std::move(closure)));
  }
#if !defined(SWIG)
  void WhenStartRange(Solver::Action action) {
    WhenStartRange(solver()->MakeActionDemon(std::move(action)));
  }
#endif  // SWIG
  virtual void WhenStartBound(Demon* const d) = 0;
  void WhenStartBound(Solver::Closure closure) {
    WhenStartBound(solver()->MakeClosureDemon(std::move(closure)));
  }
#if !defined(SWIG)
  void WhenStartBound(Solver::Action action) {
    WhenStartBound(solver()->MakeActionDemon(std::move(action)));
  }
#endif  // SWIG

  /// These methods query, set, and watch the duration of the interval var.
  virtual int64_t DurationMin() const = 0;
  virtual int64_t DurationMax() const = 0;
  virtual void SetDurationMin(int64_t m) = 0;
  virtual void SetDurationMax(int64_t m) = 0;
  virtual void SetDurationRange(int64_t mi, int64_t ma) = 0;
  virtual int64_t OldDurationMin() const = 0;
  virtual int64_t OldDurationMax() const = 0;
  virtual void WhenDurationRange(Demon* const d) = 0;
  void WhenDurationRange(Solver::Closure closure) {
    WhenDurationRange(solver()->MakeClosureDemon(std::move(closure)));
  }
#if !defined(SWIG)
  void WhenDurationRange(Solver::Action action) {
    WhenDurationRange(solver()->MakeActionDemon(std::move(action)));
  }
#endif  // SWIG
  virtual void WhenDurationBound(Demon* const d) = 0;
  void WhenDurationBound(Solver::Closure closure) {
    WhenDurationBound(solver()->MakeClosureDemon(std::move(closure)));
  }
#if !defined(SWIG)
  void WhenDurationBound(Solver::Action action) {
    WhenDurationBound(solver()->MakeActionDemon(std::move(action)));
  }
#endif  // SWIG

  /// These methods query, set, and watch the end position of the interval var.
  virtual int64_t EndMin() const = 0;
  virtual int64_t EndMax() const = 0;
  virtual void SetEndMin(int64_t m) = 0;
  virtual void SetEndMax(int64_t m) = 0;
  virtual void SetEndRange(int64_t mi, int64_t ma) = 0;
  virtual int64_t OldEndMin() const = 0;
  virtual int64_t OldEndMax() const = 0;
  virtual void WhenEndRange(Demon* const d) = 0;
  void WhenEndRange(Solver::Closure closure) {
    WhenEndRange(solver()->MakeClosureDemon(std::move(closure)));
  }
#if !defined(SWIG)
  void WhenEndRange(Solver::Action action) {
    WhenEndRange(solver()->MakeActionDemon(std::move(action)));
  }
#endif  // SWIG
  virtual void WhenEndBound(Demon* const d) = 0;
  void WhenEndBound(Solver::Closure closure) {
    WhenEndBound(solver()->MakeClosureDemon(std::move(closure)));
  }
#if !defined(SWIG)
  void WhenEndBound(Solver::Action action) {
    WhenEndBound(solver()->MakeActionDemon(std::move(action)));
  }
#endif  // SWIG

  /// These methods query, set, and watch the performed status of the
  /// interval var.
  virtual bool MustBePerformed() const = 0;
  virtual bool MayBePerformed() const = 0;
  bool CannotBePerformed() const { return !MayBePerformed(); }
  bool IsPerformedBound() const {
    return MustBePerformed() || !MayBePerformed();
  }
  virtual void SetPerformed(bool val) = 0;
  virtual bool WasPerformedBound() const = 0;
  virtual void WhenPerformedBound(Demon* const d) = 0;
  void WhenPerformedBound(Solver::Closure closure) {
    WhenPerformedBound(solver()->MakeClosureDemon(std::move(closure)));
  }
#if !defined(SWIG)
  void WhenPerformedBound(Solver::Action action) {
    WhenPerformedBound(solver()->MakeActionDemon(std::move(action)));
  }
#endif  // SWIG

  /// Attaches a demon awakened when anything about this interval changes.
  void WhenAnything(Demon* const d);
  /// Attaches a closure awakened when anything about this interval changes.
  void WhenAnything(Solver::Closure closure) {
    WhenAnything(solver()->MakeClosureDemon(std::move(closure)));
  }
#if !defined(SWIG)
  /// Attaches an action awakened when anything about this interval changes.
  void WhenAnything(Solver::Action action) {
    WhenAnything(solver()->MakeActionDemon(std::move(action)));
  }
#endif  // SWIG

  /// These methods create expressions encapsulating the start, end
  /// and duration of the interval var. Please note that these must not
  /// be used if the interval var is unperformed.
  virtual IntExpr* StartExpr() = 0;
  virtual IntExpr* DurationExpr() = 0;
  virtual IntExpr* EndExpr() = 0;
  virtual IntExpr* PerformedExpr() = 0;
  /// These methods create expressions encapsulating the start, end
  /// and duration of the interval var. If the interval var is
  /// unperformed, they will return the unperformed_value.
  virtual IntExpr* SafeStartExpr(int64_t unperformed_value) = 0;
  virtual IntExpr* SafeDurationExpr(int64_t unperformed_value) = 0;
  virtual IntExpr* SafeEndExpr(int64_t unperformed_value) = 0;

  /// Accepts the given visitor.
  virtual void Accept(ModelVisitor* const visitor) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(IntervalVar);
};

/// A sequence variable is a variable whose domain is a set of possible
/// orderings of the interval variables. It allows ordering of tasks. It
/// has two sets of methods: ComputePossibleFirstsAndLasts(), which
/// returns the list of interval variables that can be ranked first or
/// last; and RankFirst/RankNotFirst/RankLast/RankNotLast, which can be
/// used to create the search decision.
class SequenceVar : public PropagationBaseObject {
 public:
  SequenceVar(Solver* const s, const std::vector<IntervalVar*>& intervals,
              const std::vector<IntVar*>& nexts, const std::string& name);

  ~SequenceVar() override;

  std::string DebugString() const override;

#if !defined(SWIG)
  /// Returns the minimum and maximum duration of combined interval
  /// vars in the sequence.
  void DurationRange(int64_t* const dmin, int64_t* const dmax) const;

  /// Returns the minimum start min and the maximum end max of all
  /// interval vars in the sequence.
  void HorizonRange(int64_t* const hmin, int64_t* const hmax) const;

  /// Returns the minimum start min and the maximum end max of all
  /// unranked interval vars in the sequence.
  void ActiveHorizonRange(int64_t* const hmin, int64_t* const hmax) const;

  /// Compute statistics on the sequence.
  void ComputeStatistics(int* const ranked, int* const not_ranked,
                         int* const unperformed) const;
#endif  // !defined(SWIG)

  /// Ranks the index_th interval var first of all unranked interval
  /// vars. After that, it will no longer be considered ranked.
  void RankFirst(int index);

  /// Indicates that the index_th interval var will not be ranked first
  /// of all currently unranked interval vars.
  void RankNotFirst(int index);

  /// Ranks the index_th interval var first of all unranked interval
  /// vars. After that, it will no longer be considered ranked.
  void RankLast(int index);

  /// Indicates that the index_th interval var will not be ranked first
  /// of all currently unranked interval vars.
  void RankNotLast(int index);

  /// Computes the set of indices of interval variables that can be
  /// ranked first in the set of unranked activities.
  void ComputePossibleFirstsAndLasts(std::vector<int>* const possible_firsts,
                                     std::vector<int>* const possible_lasts);

  /// Applies the following sequence of ranks, ranks first, then rank
  /// last.  rank_first and rank_last represents different directions.
  /// rank_first[0] corresponds to the first interval of the sequence.
  /// rank_last[0] corresponds to the last interval of the sequence.
  /// All intervals in the unperformed vector will be marked as such.
  void RankSequence(const std::vector<int>& rank_first,
                    const std::vector<int>& rank_last,
                    const std::vector<int>& unperformed);

  /// Clears 'rank_first' and 'rank_last', and fills them with the
  /// intervals in the order of the ranks. If all variables are ranked,
  /// 'rank_first' will contain all variables, and 'rank_last' will
  /// contain none.
  /// 'unperformed' will contains all such interval variables.
  /// rank_first and rank_last represents different directions.
  /// rank_first[0] corresponds to the first interval of the sequence.
  /// rank_last[0] corresponds to the last interval of the sequence.
  void FillSequence(std::vector<int>* const rank_first,
                    std::vector<int>* const rank_last,
                    std::vector<int>* const unperformed) const;

  /// Returns the index_th interval of the sequence.
  IntervalVar* Interval(int index) const;

  /// Returns the next of the index_th interval of the sequence.
  IntVar* Next(int index) const;

  /// Returns the number of interval vars in the sequence.
  int64_t size() const { return intervals_.size(); }

  /// Accepts the given visitor.
  virtual void Accept(ModelVisitor* const visitor) const;

 private:
  int ComputeForwardFrontier();
  int ComputeBackwardFrontier();
  void UpdatePrevious() const;

  const std::vector<IntervalVar*> intervals_;
  const std::vector<IntVar*> nexts_;
  mutable std::vector<int> previous_;
};

class AssignmentElement {
 public:
  AssignmentElement() : activated_(true) {}

  void Activate() { activated_ = true; }
  void Deactivate() { activated_ = false; }
  bool Activated() const { return activated_; }

 private:
  bool activated_;
};

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
  void Restore() {
    if (var_ != nullptr) {
      var_->SetRange(min_, max_);
    }
  }
  void LoadFromProto(const IntVarAssignment& int_var_assignment_proto);
  void WriteToProto(IntVarAssignment* int_var_assignment_proto) const;

  int64_t Min() const { return min_; }
  void SetMin(int64_t m) { min_ = m; }
  int64_t Max() const { return max_; }
  void SetMax(int64_t m) { max_ = m; }
  int64_t Value() const {
    DCHECK_EQ(min_, max_);
    // Get the value from an unbound int var assignment element.
    return min_;
  }
  bool Bound() const { return (max_ == min_); }
  void SetRange(int64_t l, int64_t u) {
    min_ = l;
    max_ = u;
  }
  void SetValue(int64_t v) {
    min_ = v;
    max_ = v;
  }
  std::string DebugString() const;

  bool operator==(const IntVarElement& element) const;
  bool operator!=(const IntVarElement& element) const {
    return !(*this == element);
  }

 private:
  IntVar* var_;
  int64_t min_;
  int64_t max_;
};

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
  void LoadFromProto(
      const IntervalVarAssignment& interval_var_assignment_proto);
  void WriteToProto(IntervalVarAssignment* interval_var_assignment_proto) const;

  int64_t StartMin() const { return start_min_; }
  int64_t StartMax() const { return start_max_; }
  int64_t StartValue() const {
    CHECK_EQ(start_max_, start_min_);
    return start_max_;
  }
  int64_t DurationMin() const { return duration_min_; }
  int64_t DurationMax() const { return duration_max_; }
  int64_t DurationValue() const {
    CHECK_EQ(duration_max_, duration_min_);
    return duration_max_;
  }
  int64_t EndMin() const { return end_min_; }
  int64_t EndMax() const { return end_max_; }
  int64_t EndValue() const {
    CHECK_EQ(end_max_, end_min_);
    return end_max_;
  }
  int64_t PerformedMin() const { return performed_min_; }
  int64_t PerformedMax() const { return performed_max_; }
  int64_t PerformedValue() const {
    CHECK_EQ(performed_max_, performed_min_);
    return performed_max_;
  }
  void SetStartMin(int64_t m) { start_min_ = m; }
  void SetStartMax(int64_t m) { start_max_ = m; }
  void SetStartRange(int64_t mi, int64_t ma) {
    start_min_ = mi;
    start_max_ = ma;
  }
  void SetStartValue(int64_t v) {
    start_min_ = v;
    start_max_ = v;
  }
  void SetDurationMin(int64_t m) { duration_min_ = m; }
  void SetDurationMax(int64_t m) { duration_max_ = m; }
  void SetDurationRange(int64_t mi, int64_t ma) {
    duration_min_ = mi;
    duration_max_ = ma;
  }
  void SetDurationValue(int64_t v) {
    duration_min_ = v;
    duration_max_ = v;
  }
  void SetEndMin(int64_t m) { end_min_ = m; }
  void SetEndMax(int64_t m) { end_max_ = m; }
  void SetEndRange(int64_t mi, int64_t ma) {
    end_min_ = mi;
    end_max_ = ma;
  }
  void SetEndValue(int64_t v) {
    end_min_ = v;
    end_max_ = v;
  }
  void SetPerformedMin(int64_t m) { performed_min_ = m; }
  void SetPerformedMax(int64_t m) { performed_max_ = m; }
  void SetPerformedRange(int64_t mi, int64_t ma) {
    performed_min_ = mi;
    performed_max_ = ma;
  }
  void SetPerformedValue(int64_t v) {
    performed_min_ = v;
    performed_max_ = v;
  }
  bool Bound() const {
    return (start_min_ == start_max_ && duration_min_ == duration_max_ &&
            end_min_ == end_max_ && performed_min_ == performed_max_);
  }
  std::string DebugString() const;
  bool operator==(const IntervalVarElement& element) const;
  bool operator!=(const IntervalVarElement& element) const {
    return !(*this == element);
  }

 private:
  int64_t start_min_;
  int64_t start_max_;
  int64_t duration_min_;
  int64_t duration_max_;
  int64_t end_min_;
  int64_t end_max_;
  int64_t performed_min_;
  int64_t performed_max_;
  IntervalVar* var_;
};

/// The SequenceVarElement stores a partial representation of ranked
/// interval variables in the underlying sequence variable.
/// This representation consists of three vectors:
///   - the forward sequence. That is the list of interval variables
///     ranked first in the sequence.  The first element of the backward
///     sequence is the first interval in the sequence variable.
///   - the backward sequence. That is the list of interval variables
///     ranked last in the sequence. The first element of the backward
///     sequence is the last interval in the sequence variable.
///   - The list of unperformed interval variables.
///  Furthermore, if all performed variables are ranked, then by
///  convention, the forward_sequence will contain all such variables
///  and the backward_sequence will be empty.
class SequenceVarElement : public AssignmentElement {
 public:
  SequenceVarElement();
  explicit SequenceVarElement(SequenceVar* const var);
  void Reset(SequenceVar* const var);
  SequenceVarElement* Clone();
  void Copy(const SequenceVarElement& element);
  SequenceVar* Var() const { return var_; }
  void Store();
  void Restore();
  void LoadFromProto(
      const SequenceVarAssignment& sequence_var_assignment_proto);
  void WriteToProto(SequenceVarAssignment* sequence_var_assignment_proto) const;

  const std::vector<int>& ForwardSequence() const;
  const std::vector<int>& BackwardSequence() const;
  const std::vector<int>& Unperformed() const;
  void SetSequence(const std::vector<int>& forward_sequence,
                   const std::vector<int>& backward_sequence,
                   const std::vector<int>& unperformed);
  void SetForwardSequence(const std::vector<int>& forward_sequence);
  void SetBackwardSequence(const std::vector<int>& backward_sequence);
  void SetUnperformed(const std::vector<int>& unperformed);
  bool Bound() const {
    return forward_sequence_.size() + unperformed_.size() == var_->size();
  }

  std::string DebugString() const;

  bool operator==(const SequenceVarElement& element) const;
  bool operator!=(const SequenceVarElement& element) const {
    return !(*this == element);
  }

 private:
  bool CheckClassInvariants();

  SequenceVar* var_;
  std::vector<int> forward_sequence_;
  std::vector<int> backward_sequence_;
  std::vector<int> unperformed_;
};

template <class V, class E>
class AssignmentContainer {
 public:
  AssignmentContainer() {}
  E* Add(V* var) {
    CHECK(var != nullptr);
    int index = -1;
    if (!Find(var, &index)) {
      return FastAdd(var);
    } else {
      return &elements_[index];
    }
  }
  /// Adds element without checking its presence in the container.
  E* FastAdd(V* var) {
    DCHECK(var != nullptr);
    elements_.emplace_back(var);
    return &elements_.back();
  }
  /// Advanced usage: Adds element at a given position; position has to have
  /// been allocated with AssignmentContainer::Resize() beforehand.
  E* AddAtPosition(V* var, int position) {
    elements_[position].Reset(var);
    return &elements_[position];
  }
  void Clear() {
    elements_.clear();
    if (!elements_map_.empty()) {  /// 2x speedup on OR-Tools.
      elements_map_.clear();
    }
  }
  /// Advanced usage: Resizes the container, potentially adding elements with
  /// null variables.
  void Resize(size_t size) { elements_.resize(size); }
  bool Empty() const { return elements_.empty(); }
  /// Copies the elements of 'container' which are already in the calling
  /// container.
  void CopyIntersection(const AssignmentContainer<V, E>& container) {
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
      E* const local_element = &elements_[index];
      local_element->Copy(element);
      if (element.Activated()) {
        local_element->Activate();
      } else {
        local_element->Deactivate();
      }
    }
  }
  /// Copies all the elements of 'container' to this container, clearing its
  /// previous content.
  void Copy(const AssignmentContainer<V, E>& container) {
    Clear();
    for (int i = 0; i < container.elements_.size(); ++i) {
      const E& element = container.elements_[i];
      FastAdd(element.Var())->Copy(element);
    }
  }
  bool Contains(const V* const var) const {
    int index;
    return Find(var, &index);
  }
  E* MutableElement(const V* const var) {
    E* const element = MutableElementOrNull(var);
    DCHECK(element != nullptr)
        << "Unknown variable " << var->DebugString() << " in solution";
    return element;
  }
  E* MutableElementOrNull(const V* const var) {
    int index = -1;
    if (Find(var, &index)) {
      return MutableElement(index);
    }
    return nullptr;
  }
  const E& Element(const V* const var) const {
    const E* const element = ElementPtrOrNull(var);
    DCHECK(element != nullptr)
        << "Unknown variable " << var->DebugString() << " in solution";
    return *element;
  }
  const E* ElementPtrOrNull(const V* const var) const {
    int index = -1;
    if (Find(var, &index)) {
      return &Element(index);
    }
    return nullptr;
  }
  const std::vector<E>& elements() const { return elements_; }
  E* MutableElement(int index) { return &elements_[index]; }
  const E& Element(int index) const { return elements_[index]; }
  int Size() const { return elements_.size(); }
  void Store() {
    for (E& element : elements_) {
      element.Store();
    }
  }
  void Restore() {
    for (E& element : elements_) {
      if (element.Activated()) {
        element.Restore();
      }
    }
  }
  bool AreAllElementsBound() const {
    for (const E& element : elements_) {
      if (!element.Bound()) return false;
    }
    return true;
  }

  /// Returns true if this and 'container' both represent the same V* -> E map.
  /// Runs in linear time; requires that the == operator on the type E is well
  /// defined.
  bool operator==(const AssignmentContainer<V, E>& container) const {
    /// We may not have any work to do
    if (Size() != container.Size()) {
      return false;
    }
    /// The == should be order-independent
    EnsureMapIsUpToDate();
    /// Do not use the hash_map::== operator! It
    /// compares both content and how the map is hashed (e.g., number of
    /// buckets). This is not what we want.
    for (const E& element : container.elements_) {
      const int position =
          gtl::FindWithDefault(elements_map_, element.Var(), -1);
      if (position < 0 || elements_[position] != element) {
        return false;
      }
    }
    return true;
  }
  bool operator!=(const AssignmentContainer<V, E>& container) const {
    return !(*this == container);
  }

 private:
  void EnsureMapIsUpToDate() const {
    absl::flat_hash_map<const V*, int>* map =
        const_cast<absl::flat_hash_map<const V*, int>*>(&elements_map_);
    for (int i = map->size(); i < elements_.size(); ++i) {
      (*map)[elements_[i].Var()] = i;
    }
  }
  bool Find(const V* const var, int* index) const {
    /// This threshold was determined from microbenchmarks on Nehalem platform.
    const size_t kMaxSizeForLinearAccess = 11;
    if (Size() <= kMaxSizeForLinearAccess) {
      /// Look for 'var' in the container by performing a linear
      /// search, avoiding the access to (and creation of) the elements
      /// hash table.
      for (int i = 0; i < elements_.size(); ++i) {
        if (var == elements_[i].Var()) {
          *index = i;
          return true;
        }
      }
      return false;
    } else {
      EnsureMapIsUpToDate();
      DCHECK_EQ(elements_map_.size(), elements_.size());
      return gtl::FindCopy(elements_map_, var, index);
    }
  }

  std::vector<E> elements_;
  absl::flat_hash_map<const V*, int> elements_map_;
};

/// An Assignment is a variable -> domains mapping, used
/// to report solutions to the user.
class Assignment : public PropagationBaseObject {
 public:
  typedef AssignmentContainer<IntVar, IntVarElement> IntContainer;
  typedef AssignmentContainer<IntervalVar, IntervalVarElement>
      IntervalContainer;
  typedef AssignmentContainer<SequenceVar, SequenceVarElement>
      SequenceContainer;

  explicit Assignment(Solver* const s);
  explicit Assignment(const Assignment* const copy);
  ~Assignment() override;

  void Clear();
  bool Empty() const {
    return int_var_container_.Empty() && interval_var_container_.Empty() &&
           sequence_var_container_.Empty();
  }
  int Size() const {
    return NumIntVars() + NumIntervalVars() + NumSequenceVars();
  }
  int NumIntVars() const { return int_var_container_.Size(); }
  int NumIntervalVars() const { return interval_var_container_.Size(); }
  int NumSequenceVars() const { return sequence_var_container_.Size(); }
  void Store();
  void Restore();

  /// Loads an assignment from a file; does not add variables to the
  /// assignment (only the variables contained in the assignment are modified).
  bool Load(const std::string& filename);
#if !defined(SWIG)
  bool Load(File* file);
#endif  /// #if !defined(SWIG)
  void Load(const AssignmentProto& assignment_proto);
  /// Saves the assignment to a file.
  bool Save(const std::string& filename) const;
#if !defined(SWIG)
  bool Save(File* file) const;
#endif  // #if !defined(SWIG)
  void Save(AssignmentProto* const assignment_proto) const;

  void AddObjective(IntVar* const v) {
    // Objective can only set once.
    DCHECK(!HasObjective());
    objective_element_.Reset(v);
  }
  void ClearObjective() { objective_element_.Reset(nullptr); }
  IntVar* Objective() const { return objective_element_.Var(); }
  bool HasObjective() const { return (Objective() != nullptr); }
  int64_t ObjectiveMin() const;
  int64_t ObjectiveMax() const;
  int64_t ObjectiveValue() const;
  bool ObjectiveBound() const;
  void SetObjectiveMin(int64_t m);
  void SetObjectiveMax(int64_t m);
  void SetObjectiveValue(int64_t value);
  void SetObjectiveRange(int64_t l, int64_t u);

  IntVarElement* Add(IntVar* const var);
  void Add(const std::vector<IntVar*>& vars);
  /// Adds without checking if variable has been previously added.
  IntVarElement* FastAdd(IntVar* const var);
  int64_t Min(const IntVar* const var) const;
  int64_t Max(const IntVar* const var) const;
  int64_t Value(const IntVar* const var) const;
  bool Bound(const IntVar* const var) const;
  void SetMin(const IntVar* const var, int64_t m);
  void SetMax(const IntVar* const var, int64_t m);
  void SetRange(const IntVar* const var, int64_t l, int64_t u);
  void SetValue(const IntVar* const var, int64_t value);

  IntervalVarElement* Add(IntervalVar* const var);
  void Add(const std::vector<IntervalVar*>& vars);
  /// Adds without checking if variable has been previously added.
  IntervalVarElement* FastAdd(IntervalVar* const var);
  int64_t StartMin(const IntervalVar* const var) const;
  int64_t StartMax(const IntervalVar* const var) const;
  int64_t StartValue(const IntervalVar* const var) const;
  int64_t DurationMin(const IntervalVar* const var) const;
  int64_t DurationMax(const IntervalVar* const var) const;
  int64_t DurationValue(const IntervalVar* const var) const;
  int64_t EndMin(const IntervalVar* const var) const;
  int64_t EndMax(const IntervalVar* const var) const;
  int64_t EndValue(const IntervalVar* const var) const;
  int64_t PerformedMin(const IntervalVar* const var) const;
  int64_t PerformedMax(const IntervalVar* const var) const;
  int64_t PerformedValue(const IntervalVar* const var) const;
  void SetStartMin(const IntervalVar* const var, int64_t m);
  void SetStartMax(const IntervalVar* const var, int64_t m);
  void SetStartRange(const IntervalVar* const var, int64_t mi, int64_t ma);
  void SetStartValue(const IntervalVar* const var, int64_t value);
  void SetDurationMin(const IntervalVar* const var, int64_t m);
  void SetDurationMax(const IntervalVar* const var, int64_t m);
  void SetDurationRange(const IntervalVar* const var, int64_t mi, int64_t ma);
  void SetDurationValue(const IntervalVar* const var, int64_t value);
  void SetEndMin(const IntervalVar* const var, int64_t m);
  void SetEndMax(const IntervalVar* const var, int64_t m);
  void SetEndRange(const IntervalVar* const var, int64_t mi, int64_t ma);
  void SetEndValue(const IntervalVar* const var, int64_t value);
  void SetPerformedMin(const IntervalVar* const var, int64_t m);
  void SetPerformedMax(const IntervalVar* const var, int64_t m);
  void SetPerformedRange(const IntervalVar* const var, int64_t mi, int64_t ma);
  void SetPerformedValue(const IntervalVar* const var, int64_t value);

  SequenceVarElement* Add(SequenceVar* const var);
  void Add(const std::vector<SequenceVar*>& vars);
  /// Adds without checking if the variable had been previously added.
  SequenceVarElement* FastAdd(SequenceVar* const var);
  const std::vector<int>& ForwardSequence(const SequenceVar* const var) const;
  const std::vector<int>& BackwardSequence(const SequenceVar* const var) const;
  const std::vector<int>& Unperformed(const SequenceVar* const var) const;
  void SetSequence(const SequenceVar* const var,
                   const std::vector<int>& forward_sequence,
                   const std::vector<int>& backward_sequence,
                   const std::vector<int>& unperformed);
  void SetForwardSequence(const SequenceVar* const var,
                          const std::vector<int>& forward_sequence);
  void SetBackwardSequence(const SequenceVar* const var,
                           const std::vector<int>& backward_sequence);
  void SetUnperformed(const SequenceVar* const var,
                      const std::vector<int>& unperformed);

  void Activate(const IntVar* const var);
  void Deactivate(const IntVar* const var);
  bool Activated(const IntVar* const var) const;

  void Activate(const IntervalVar* const var);
  void Deactivate(const IntervalVar* const var);
  bool Activated(const IntervalVar* const var) const;

  void Activate(const SequenceVar* const var);
  void Deactivate(const SequenceVar* const var);
  bool Activated(const SequenceVar* const var) const;

  void ActivateObjective();
  void DeactivateObjective();
  bool ActivatedObjective() const;

  std::string DebugString() const override;

  bool AreAllElementsBound() const {
    return int_var_container_.AreAllElementsBound() &&
           interval_var_container_.AreAllElementsBound() &&
           sequence_var_container_.AreAllElementsBound();
  }

  bool Contains(const IntVar* const var) const;
  bool Contains(const IntervalVar* const var) const;
  bool Contains(const SequenceVar* const var) const;
  /// Copies the intersection of the two assignments to the current assignment.
  void CopyIntersection(const Assignment* assignment);
  /// Copies 'assignment' to the current assignment, clearing its previous
  /// content.
  void Copy(const Assignment* assignment);

  // TODO(user): Add element iterators to avoid exposing container class.
  const IntContainer& IntVarContainer() const { return int_var_container_; }
  IntContainer* MutableIntVarContainer() { return &int_var_container_; }
  const IntervalContainer& IntervalVarContainer() const {
    return interval_var_container_;
  }
  IntervalContainer* MutableIntervalVarContainer() {
    return &interval_var_container_;
  }
  const SequenceContainer& SequenceVarContainer() const {
    return sequence_var_container_;
  }
  SequenceContainer* MutableSequenceVarContainer() {
    return &sequence_var_container_;
  }
  bool operator==(const Assignment& assignment) const {
    return int_var_container_ == assignment.int_var_container_ &&
           interval_var_container_ == assignment.interval_var_container_ &&
           sequence_var_container_ == assignment.sequence_var_container_ &&
           objective_element_ == assignment.objective_element_;
  }
  bool operator!=(const Assignment& assignment) const {
    return !(*this == assignment);
  }

 private:
  IntContainer int_var_container_;
  IntervalContainer interval_var_container_;
  SequenceContainer sequence_var_container_;
  IntVarElement objective_element_;
  DISALLOW_COPY_AND_ASSIGN(Assignment);
};

std::ostream& operator<<(std::ostream& out,
                         const Assignment& assignment);  /// NOLINT

/// Given a "source_assignment", clears the "target_assignment" and adds
/// all IntVars in "target_vars", with the values of the variables set according
/// to the corresponding values of "source_vars" in "source_assignment".
/// source_vars and target_vars must have the same number of elements.
/// The source and target assignments can belong to different Solvers.
void SetAssignmentFromAssignment(Assignment* target_assignment,
                                 const std::vector<IntVar*>& target_vars,
                                 const Assignment* source_assignment,
                                 const std::vector<IntVar*>& source_vars);

class Pack : public Constraint {
 public:
  Pack(Solver* const s, const std::vector<IntVar*>& vars, int number_of_bins);

  ~Pack() override;

  /// Dimensions are additional constraints than can restrict what is
  /// possible with the pack constraint. It can be used to set capacity
  /// limits, to count objects per bin, to compute unassigned
  /// penalties...

  /// This dimension imposes that for all bins b, the weighted sum
  /// (weights[i]) of all objects i assigned to 'b' is less or equal
  /// 'bounds[b]'.
  void AddWeightedSumLessOrEqualConstantDimension(
      const std::vector<int64_t>& weights, const std::vector<int64_t>& bounds);

  /// This dimension imposes that for all bins b, the weighted sum
  /// (weights->Run(i)) of all objects i assigned to 'b' is less or
  /// equal to 'bounds[b]'. Ownership of the callback is transferred to
  /// the pack constraint.
  void AddWeightedSumLessOrEqualConstantDimension(
      Solver::IndexEvaluator1 weights, const std::vector<int64_t>& bounds);

  /// This dimension imposes that for all bins b, the weighted sum
  /// (weights->Run(i, b) of all objects i assigned to 'b' is less or
  /// equal to 'bounds[b]'. Ownership of the callback is transferred to
  /// the pack constraint.
  void AddWeightedSumLessOrEqualConstantDimension(
      Solver::IndexEvaluator2 weights, const std::vector<int64_t>& bounds);

  /// This dimension imposes that for all bins b, the weighted sum
  /// (weights[i]) of all objects i assigned to 'b' is equal to loads[b].
  void AddWeightedSumEqualVarDimension(const std::vector<int64_t>& weights,
                                       const std::vector<IntVar*>& loads);

  /// This dimension imposes that for all bins b, the weighted sum
  /// (weights->Run(i, b)) of all objects i assigned to 'b' is equal to
  /// loads[b].
  void AddWeightedSumEqualVarDimension(Solver::IndexEvaluator2 weights,
                                       const std::vector<IntVar*>& loads);

  /// This dimension imposes:
  /// forall b in bins,
  ///    sum (i in items: usage[i] * is_assigned(i, b)) <= capacity[b]
  /// where is_assigned(i, b) is true if and only if item i is assigned
  /// to the bin b.
  ///
  /// This can be used to model shapes of items by linking variables of
  /// the same item on parallel dimensions with an allowed assignment
  /// constraint.
  void AddSumVariableWeightsLessOrEqualConstantDimension(
      const std::vector<IntVar*>& usage, const std::vector<int64_t>& capacity);

  /// This dimension enforces that cost_var == sum of weights[i] for
  /// all objects 'i' assigned to a bin.
  void AddWeightedSumOfAssignedDimension(const std::vector<int64_t>& weights,
                                         IntVar* const cost_var);

  /// This dimension links 'count_var' to the actual number of bins used in the
  /// pack.
  void AddCountUsedBinDimension(IntVar* const count_var);

  /// This dimension links 'count_var' to the actual number of items
  /// assigned to a bin in the pack.
  void AddCountAssignedItemsDimension(IntVar* const count_var);

  void Post() override;
  void ClearAll();
  void PropagateDelayed();
  void InitialPropagate() override;
  void Propagate();
  void OneDomain(int var_index);
  std::string DebugString() const override;
  bool IsUndecided(int var_index, int bin_index) const;
  void SetImpossible(int var_index, int bin_index);
  void Assign(int var_index, int bin_index);
  bool IsAssignedStatusKnown(int var_index) const;
  bool IsPossible(int var_index, int bin_index) const;
  IntVar* AssignVar(int var_index, int bin_index) const;
  void SetAssigned(int var_index);
  void SetUnassigned(int var_index);
  void RemoveAllPossibleFromBin(int bin_index);
  void AssignAllPossibleToBin(int bin_index);
  void AssignFirstPossibleToBin(int bin_index);
  void AssignAllRemainingItems();
  void UnassignAllRemainingItems();
  void Accept(ModelVisitor* const visitor) const override;

 private:
  bool IsInProcess() const;
  const std::vector<IntVar*> vars_;
  const int bins_;
  std::vector<Dimension*> dims_;
  std::unique_ptr<RevBitMatrix> unprocessed_;
  std::vector<std::vector<int>> forced_;
  std::vector<std::vector<int>> removed_;
  std::vector<IntVarIterator*> holes_;
  uint64_t stamp_;
  Demon* demon_;
  std::vector<std::pair<int, int>> to_set_;
  std::vector<std::pair<int, int>> to_unset_;
  bool in_process_;
};

class DisjunctiveConstraint : public Constraint {
 public:
  DisjunctiveConstraint(Solver* const s,
                        const std::vector<IntervalVar*>& intervals,
                        const std::string& name);
  ~DisjunctiveConstraint() override;

  /// Creates a sequence variable from the constraint.
  virtual SequenceVar* MakeSequenceVar() = 0;

  /// Add a transition time between intervals.  It forces the distance between
  /// the end of interval a and start of interval b that follows it to be at
  /// least transition_time(a, b). This function must always return
  /// a positive or null value.
  void SetTransitionTime(Solver::IndexEvaluator2 transition_time);

  int64_t TransitionTime(int before_index, int after_index) {
    DCHECK(transition_time_);
    return transition_time_(before_index, after_index);
  }

#if !defined(SWIG)
  virtual const std::vector<IntVar*>& nexts() const = 0;
  virtual const std::vector<IntVar*>& actives() const = 0;
  virtual const std::vector<IntVar*>& time_cumuls() const = 0;
  virtual const std::vector<IntVar*>& time_slacks() const = 0;
#endif  // !defined(SWIG)

 protected:
  const std::vector<IntervalVar*> intervals_;
  Solver::IndexEvaluator2 transition_time_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisjunctiveConstraint);
};

/// This class is used to manage a pool of solutions. It can transform
/// a single point local search into a multipoint local search.
class SolutionPool : public BaseObject {
 public:
  SolutionPool() {}
  ~SolutionPool() override {}

  /// This method is called to initialize the solution pool with the assignment
  /// from the local search.
  virtual void Initialize(Assignment* const assignment) = 0;

  /// This method is called when a new solution has been accepted by the local
  /// search.
  virtual void RegisterNewSolution(Assignment* const assignment) = 0;

  /// This method is called when the local search starts a new neighborhood to
  /// initialize the default assignment.
  virtual void GetNextSolution(Assignment* const assignment) = 0;

  /// This method checks if the local solution needs to be updated with
  /// an external one.
  virtual bool SyncNeeded(Assignment* const local_assignment) = 0;
};
}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_CONSTRAINT_SOLVER_H_

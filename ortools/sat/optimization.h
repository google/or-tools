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

#ifndef OR_TOOLS_SAT_OPTIMIZATION_H_
#define OR_TOOLS_SAT_OPTIMIZATION_H_

#include <functional>
#include <vector>

#include "ortools/sat/clause.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// Like MinimizeCore() with a slower but strictly better heuristic. This
// algorithm should produce a minimal core with respect to propagation. We put
// each literal of the initial core "last" at least once, so if such literal can
// be inferred by propagation by any subset of the other literal, it will be
// removed.
//
// Note that the literal of the minimized core will stay in the same order.
//
// TODO(user): Avoid spending too much time trying to minimize a core.
void MinimizeCoreWithPropagation(TimeLimit* limit, SatSolver* solver,
                                 std::vector<Literal>* core);

void MinimizeCoreWithSearch(TimeLimit* limit, SatSolver* solver,
                            std::vector<Literal>* core);

bool ProbeLiteral(Literal assumption, SatSolver* solver);

// Remove fixed literals from the core.
void FilterAssignedLiteral(const VariablesAssignment& assignment,
                           std::vector<Literal>* core);

// Model-based API to minimize a given IntegerVariable by solving a sequence of
// decision problem. Each problem is solved using SolveIntegerProblem(). Returns
// the status of the last solved decision problem.
//
// The feasible_solution_observer function will be called each time a new
// feasible solution is found.
//
// Note that this function will resume the search from the current state of the
// solver, and it is up to the client to backtrack to the root node if needed.
SatSolver::Status MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
    IntegerVariable objective_var,
    const std::function<void()>& feasible_solution_observer, Model* model);

// Use a low conflict limit and performs a binary search to try to restrict the
// domain of objective_var.
void RestrictObjectiveDomainWithBinarySearch(
    IntegerVariable objective_var,
    const std::function<void()>& feasible_solution_observer, Model* model);

// Transforms the given linear expression so that:
//  - duplicate terms are merged.
//  - terms with a literal and its negation are merged.
//  - all weight are positive.
//
// TODO(user): Merge this with similar code like
// ComputeBooleanLinearExpressionCanonicalForm().
void PresolveBooleanLinearExpression(std::vector<Literal>* literals,
                                     std::vector<Coefficient>* coefficients,
                                     Coefficient* offset);

// Same as MinimizeIntegerVariableWithLinearScanAndLazyEncoding() but use
// a core-based approach instead. Note that the given objective_var is just used
// for reporting the lower-bound/upper-bound and do not need to be linked with
// its linear representation.
//
// Unlike MinimizeIntegerVariableWithLinearScanAndLazyEncoding() this function
// just return the last solver status. In particular if it is INFEASIBLE but
// feasible_solution_observer() was called, it means we are at OPTIMAL.
class CoreBasedOptimizer {
 public:
  CoreBasedOptimizer(IntegerVariable objective_var,
                     const std::vector<IntegerVariable>& variables,
                     const std::vector<IntegerValue>& coefficients,
                     std::function<void()> feasible_solution_observer,
                     Model* model);

  // TODO(user): Change the algo slighlty to allow resuming from the last
  // aborted position. Currently, the search is "resumable", but it will restart
  // some of the work already done, so it might just never find anything.
  SatSolver::Status Optimize();

  // A different way to encode the objective as core are found.
  //
  // If the vector if literals is passed it will use that, otherwise it will
  // encode the passed integer variables. In both cases, the vector used should
  // be of the same size as the coefficients vector.
  //
  // It seems to be more powerful, but it isn't completely implemented yet.
  // TODO(user):
  // - Support resuming for interleaved search.
  // - Implement all core heurisitics.
  SatSolver::Status OptimizeWithSatEncoding(
      const std::vector<Literal>& literals,
      const std::vector<IntegerVariable>& vars,
      const std::vector<Coefficient>& coefficients, Coefficient offset);

 private:
  CoreBasedOptimizer(const CoreBasedOptimizer&) = delete;
  CoreBasedOptimizer& operator=(const CoreBasedOptimizer&) = delete;

  struct ObjectiveTerm {
    IntegerVariable var;
    IntegerValue weight;
    int depth;  // Only for logging/debugging.
    IntegerValue old_var_lb;

    // An upper bound on the optimal solution if we were to optimize only this
    // term. This is used by the cover optimization code.
    IntegerValue cover_ub;
  };

  // This will be called each time a feasible solution is found. Returns false
  // if a conflict was detected while trying to constrain the objective to a
  // smaller value.
  bool ProcessSolution();

  // Use the gap an implied bounds to propagated the bounds of the objective
  // variables and of its terms.
  bool PropagateObjectiveBounds();

  // Heuristic that aim to find the "real" lower bound of the objective on each
  // core by using a linear scan optimization approach.
  bool CoverOptimization();

  // Computes the next stratification threshold.
  // Sets it to zero if all the assumptions where already considered.
  void ComputeNextStratificationThreshold();

  // If we have an "at most one can be false" between literals with a positive
  // cost, you then know that at least n - 1 will contribute to the cost, and
  // you can increase the objective lower bound. This is the same as having
  // a real "at most one" constraint on the negation of such literals.
  //
  // This detects such "at most ones" and rewrite the objective accordingly.
  // For each at most one, the rewrite create a new Boolean variable and update
  // the cost so that the trivial objective lower bound reflect the increase.
  //
  // TODO(user) : Code that as a general presolve rule? I am not sure adding
  // the extra Booleans is always a good idea though. Especially since the LP
  // will see the same lower bound that what is computed by this.
  void PresolveObjectiveWithAtMostOne(std::vector<Literal>* literals,
                                      std::vector<Coefficient>* coefficients,
                                      Coefficient* offset);

  SatParameters* parameters_;
  SatSolver* sat_solver_;
  TimeLimit* time_limit_;
  BinaryImplicationGraph* implications_;
  IntegerTrail* integer_trail_;
  IntegerEncoder* integer_encoder_;
  Model* model_;

  IntegerVariable objective_var_;
  std::vector<ObjectiveTerm> terms_;
  IntegerValue stratification_threshold_;
  std::function<void()> feasible_solution_observer_;

  // This is used to not add the objective equation more than once if we
  // solve in "chunk".
  bool already_switched_to_linear_scan_ = false;

  // Set to true when we need to abort early.
  //
  // TODO(user): This is only used for the stop after first solution parameter
  // which should likely be handled differently by simply using the normal way
  // to stop a solver from the feasible solution callback.
  bool stop_ = false;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_OPTIMIZATION_H_

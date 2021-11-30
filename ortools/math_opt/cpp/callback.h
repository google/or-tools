// Copyright 2010-2021 Google LLC
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

// Data types for using callbacks with MathOpt.
//
// Callbacks allow to user to observe the progress of a solver and modify its
// behavior mid solve. This is supported by allowing the user to a function of
// type MathOpt::Callback as an optional argument to MathOpt::Solve(). This
// function is called periodically throughout the solve process. This file
// defines the data types needed to use this callback.
//
// The example below registers a callback that listens for feasible solutions
// the solvers finds along the way and accumulates them in a list for analysis
// after the solve.
//
//   using ::operations_research::math_opt::CallbackData;
//   using ::operations_research::math_opt::CallbackRegistration;
//   using ::operations_research::math_opt::CallbackResult;
//   using ::operations_research::math_opt::MathOpt;
//   using ::operations_research::math_opt::Result;
//   using ::operations_research::math_opt::Variable;
//   using ::operations_research::math_opt::VariableMap;
//
//   MathOpt model(operations_research::math_opt::SOLVER_TYPE_GUROBI);
//   Variable x = model.AddBinaryVariable();
//   model.objective().Maximize(x);
//   CallbackRegistration cb_reg;
//   cb_reg.events = {
//       operations_research::math_opt::CALLBACK_EVENT_MIP_SOLUTION};
//   std::vector<VariableMap<double>> solutions;
//   auto cb = [&solutions](const CallbackData& cb_data) {
//     // NOTE: this assumes the callback is always called from the same thread.
//     // Gurobi always does this, multi-threaded SCIP does not.
//     solutions.push_back(*cb_data.solution);
//     return CallbackResult();
//   };
//   absl::StatusOr<Result> result = opt.Solve({}, {}, cb_reb, cb);
//
// At the termination of the example, solutions will have {{x, 1.0}}, and
// possibly {{x, 0.0}} as well.
//
// If the callback argument to MathOpt::Solve() is not null, it will be invoked
// on the events specified by the callback_registration argument (and when the
// callback is null, callback_registration must not request any events or will
// CHECK fail). Some solvers do not support callbacks or certain events, in this
// case the callback is ignored. TODO(b/180617976): change this behavior.
//
// Some solvers may call callback from multiple threads (SCIP will, Gurobi
// will not). You should either solve with one thread (see
// solver_parameters.common_parameters.threads), write a threadsafe callback,
// or consult the documentation of your underlying solver.
#ifndef OR_TOOLS_MATH_OPT_CPP_CALLBACK_H_
#define OR_TOOLS_MATH_OPT_CPP_CALLBACK_H_

#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/indexed_model.h"
#include "ortools/math_opt/cpp/map_filter.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"

namespace operations_research {
namespace math_opt {

// The input to the MathOpt::Callback function.
//
// The information available depends on the current event.
struct CallbackData {
  // Users will typically not need this function.
  // Will CHECK fail if proto is not valid.
  CallbackData(IndexedModel* model, const CallbackDataProto& proto);
  CallbackData() = default;

  // The current state of the underlying solver.
  CallbackEventProto event = CALLBACK_EVENT_UNSPECIFIED;

  // If event == CALLBACK_EVENT_MIP_NODE, the primal_solution contains the
  //   primal solution to the current LP-node relaxation. In some cases, no
  //   solution will be available (e.g. because LP was infeasible or the solve
  //   was imprecise).
  // If event == CALLBACK_EVENT_MIP_SOLUTION, the primal_solution contains the
  //   newly found primal (integer) feasible solution. The solution is always
  //   present.
  // Otherwise, the primal_solution is not available.
  absl::optional<VariableMap<double>> solution;

  // If event == CALLBACK_EVENT_MESSAGE, contains the messages from the solver.
  //   Each message represents a single output line from the solver, and each
  //   message does not contain any '\n' characters.
  // Otherwise, messages is empty.
  std::vector<std::string> messages;

  // Time since `Solve()` was called. Available for all events except
  // CALLBACK_EVENT_POLLING.
  absl::Duration runtime;

  // Only available for event == CALLBACK_EVENT_PRESOLVE.
  CallbackDataProto::PresolveStats presolve_stats;

  // Only available for event == CALLBACK_EVENT_SIMPLEX.
  CallbackDataProto::SimplexStats simplex_stats;

  // Only available for event == CALLBACK_EVENT_BARRIER.
  CallbackDataProto::BarrierStats barrier_stats;

  // Only available for event of CALLBACK_EVENT_MIP, CALLBACK_EVENT_MIP_NODE, or
  // CALLBACK_EVENT_MIP_SOLUTION.
  CallbackDataProto::MipStats mip_stats;
};

// Provided with a callback at the start of a Solve() to inform the solver:
//   * what information the callback needs,
//   * how the callback might alter the solve process.
struct CallbackRegistration {
  // Will CHECK fail if referenced variables are not from the same model.
  CallbackRegistrationProto Proto() const;

  // Returns the model referenced variables, or null if no variables are
  // referenced. Will CHECK fail if variables are not from the same model.
  IndexedModel* model() const;

  // The events the solver should invoke the callback at.
  absl::flat_hash_set<CallbackEventProto> events;

  // Restricts the variable returned in CallbackData.solution for event
  // CALLBACK_EVENT_MIP_SOLUTION. This can improve performance.
  MapFilter<Variable> mip_solution_filter;

  // Restricts the variable returned in CallbackData.solution for event
  // CALLBACK_EVENT_MIP_NODE. This can improve performance.
  MapFilter<Variable> mip_node_filter;

  // If the callback will ever add "user cuts" at event CALLBACK_EVENT_MIP_NODE
  // during the solve process (a linear constraint that excludes the current LP
  // solution but does not cut off any integer points).
  bool add_cuts = false;

  // If the callback will ever add "lazy constraints" at event
  // CALLBACK_EVENT_MIP_NODE or CALLBACK_EVENT_MIP_SOLUTION during the solve
  // process (a linear constraint that excludes integer points).
  bool add_lazy_constraints = false;
};

// The value returned by the MathOpt::Callback function.
struct CallbackResult {
  // Prefer AddUserCut and AddLazyConstraint below instead of using this
  // directly.
  struct GeneratedLinearConstraint {
    BoundedLinearExpression linear_constraint;
    bool is_lazy = false;

    IndexedModel* model() const { return linear_constraint.expression.model(); }
  };

  // Adds a "user cut," a linear constraint that excludes the current LP
  // solution but does not cut off any integer points. Use only for
  // CALLBACK_EVENT_MIP_NODE.
  void AddUserCut(BoundedLinearExpression linear_constraint) {
    new_constraints.push_back({std::move(linear_constraint), false});
  }

  // Adds a "lazy constraint," a linear constraint that excludes integer points.
  // Use only for CALLBACK_EVENT_MIP_NODE and CALLBACK_EVENT_MIP_SOLUTION.
  void AddLazyConstraint(BoundedLinearExpression linear_constraint) {
    new_constraints.push_back({std::move(linear_constraint), true});
  }

  // Will CHECK fail if referenced variables are not from the same model.
  CallbackResultProto Proto() const;

  // Returns the model referenced variables, or null if no variables are
  // referenced. Will CHECK fail if variables are not from the same model.
  //
  // Runs in O(num constraints + num suggested solutions).
  IndexedModel* model() const;

  // Stop the solve process and return early. Can be called from any event.
  bool terminate = false;

  // The user cuts and lazy constraints added. Prefer AddUserCut() and
  // AddLazyConstraint() to modifying this directly.
  std::vector<GeneratedLinearConstraint> new_constraints;

  // A solution or partially defined solution to give to the solver.
  std::vector<VariableMap<double>> suggested_solutions;
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_CALLBACK_H_

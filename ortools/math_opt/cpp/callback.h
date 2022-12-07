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

// IWYU pragma: private, include "ortools/math_opt/cpp/math_opt.h"
// IWYU pragma: friend "ortools/math_opt/cpp/.*"

// Data types for using callbacks with Solve() and IncrementalSolver.
//
// Callbacks allow to user to observe the progress of a solver and modify its
// behavior mid solve. This is supported by allowing the user to a function of
// type Callback as an optional argument to Solve() and
// IncrementalSolver::Solve(). This function is called periodically throughout
// the solve process. This file defines the data types needed to use this
// callback.
//
// The example below registers a callback that listens for feasible solutions
// the solvers finds along the way and accumulates them in a list for analysis
// after the solve.
//
//   using ::operations_research::math_opt::CallbackData;
//   using ::operations_research::math_opt::CallbackRegistration;
//   using ::operations_research::math_opt::CallbackResult;
//   using ::operations_research::math_opt::Model;
//   using ::operations_research::math_opt::SolveResult;
//   using ::operations_research::math_opt::Solve;
//   using ::operations_research::math_opt::Variable;
//   using ::operations_research::math_opt::VariableMap;
//
//   Model model;
//   Variable x = model.AddBinaryVariable();
//   model.Maximize(x);
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
//   absl::StatusOr<SolveResult> result = Solve(
//     model, operations_research::math_opt::SOLVER_TYPE_GUROBI,
//     /*parameters=*/{}, /*model_parameters=*/{}, cb_reb, cb);
//
// At the termination of the example, solutions will have {{x, 1.0}}, and
// possibly {{x, 0.0}} as well.
//
// If the callback argument to Solve() is not null, it will be invoked on the
// events specified by the callback_registration argument (and when the
// callback is null, callback_registration must not request any events or will
// CHECK fail). Some solvers do not support callbacks or certain events, in this
// case the callback is ignored. TODO(b/180617976): change this behavior.
//
// Some solvers may call callback from multiple threads (SCIP will, Gurobi will
// not). You should either solve with one thread (see
// solver_parameters.threads), write a threadsafe callback, or consult
// the documentation of your underlying solver.
#ifndef OR_TOOLS_MATH_OPT_CPP_CALLBACK_H_
#define OR_TOOLS_MATH_OPT_CPP_CALLBACK_H_

#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/cpp/enums.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/map_filter.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research {
namespace math_opt {

struct CallbackData;
struct CallbackResult;

using Callback = std::function<CallbackResult(const CallbackData&)>;

// The supported events for LP/MIP callbacks.
enum class CallbackEvent {
  // The solver is currently running presolve.
  //
  // This event is supported for MIP & LP models by SolverType::kGurobi. Other
  // solvers don't support this event.
  kPresolve = CALLBACK_EVENT_PRESOLVE,

  // The solver is currently running the simplex method.
  //
  // This event is supported for MIP & LP models by SolverType::kGurobi. Other
  // solvers don't support this event.
  kSimplex = CALLBACK_EVENT_SIMPLEX,

  // The solver is in the MIP loop (called periodically before starting a new
  // node). Useful for early termination. Note that this event does not provide
  // information on LP relaxations nor about new incumbent solutions.
  //
  // This event is supported for MIP models only by SolverType::kGurobi. Other
  // solvers don't support this event.
  kMip = CALLBACK_EVENT_MIP,

  // Called every time a new MIP incumbent is found.
  //
  // This event is fully supported for MIP models by SolverType::kGurobi. CP-SAT
  // has partial support: you can view the solutions and request termination,
  // but you cannot add lazy constraints. Other solvers don't support this
  // event.
  kMipSolution = CALLBACK_EVENT_MIP_SOLUTION,

  // Called inside a MIP node. Note that there is no guarantee that the
  // callback function will be called on every node. That behavior is
  // solver-dependent.
  //
  // Disabling cuts using CommonSolveParameters may interfere with this event
  // being called and/or adding cuts at this event, the behavior is solver
  // specific.
  //
  // This event is supported for MIP models only by SolverType::kGurobi. Other
  // solvers don't support this event.
  kMipNode = CALLBACK_EVENT_MIP_NODE,

  // Called in each iterate of an interior point/barrier method.
  //
  // This event is supported for LP models only by SolverType::kGurobi. Other
  // solvers don't support this event.
  kBarrier = CALLBACK_EVENT_BARRIER,
};

MATH_OPT_DEFINE_ENUM(CallbackEvent, CALLBACK_EVENT_UNSPECIFIED);

// Provided with a callback at the start of a Solve() to inform the solver:
//   * what information the callback needs,
//   * how the callback might alter the solve process.
struct CallbackRegistration {
  // Returns a failure if the referenced variables don't belong to the input
  // expected_storage (which must not be nullptr).
  absl::Status CheckModelStorage(const ModelStorage* expected_storage) const;

  // Returns the proto equivalent of this object.
  //
  // The caller should use CheckModelStorage() as this function does not check
  // internal consistency of the referenced variables.
  CallbackRegistrationProto Proto() const;

  // The events the solver should invoke the callback at.
  //
  // A solver will return an InvalidArgument status when called with registered
  // events that are not supported for the selected solver and the type of
  // model. For example registring for CallbackEvent::kMip with a model that
  // only contains continuous variables will fail for most solvers (see the
  // documentation of each event to see which solvers support them and in which
  // case).
  absl::flat_hash_set<CallbackEvent> events;

  // Restricts the variable returned in CallbackData.solution for event
  // CallbackEvent::kMipSolution. This can improve performance.
  MapFilter<Variable> mip_solution_filter;

  // Restricts the variable returned in CallbackData.solution for event
  // CallbackEvent::kMipNode. This can improve performance.
  MapFilter<Variable> mip_node_filter;

  // If the callback will ever add "user cuts" at event CallbackEvent::kMipNode
  // during the solve process (a linear constraint that excludes the current LP
  // solution but does not cut off any integer points).
  bool add_cuts = false;

  // If the callback will ever add "lazy constraints" at event
  // CallbackEvent::kMipNode or CallbackEvent::kMipSolution during the solve
  // process (a linear constraint that excludes integer points).
  bool add_lazy_constraints = false;
};

// The input to the Callback function.
//
// The information available depends on the current event.
struct CallbackData {
  // Users will typically not need this function other than for testing.
  CallbackData(CallbackEvent event, absl::Duration runtime);

  // Users will typically not need this function.
  // Will CHECK fail if proto is not valid.
  CallbackData(const ModelStorage* storage, const CallbackDataProto& proto);

  // The current state of the underlying solver.
  CallbackEvent event;

  // If event == CallbackEvent::kMipNode, the primal_solution contains the
  //   primal solution to the current LP-node relaxation. In some cases, no
  //   solution will be available (e.g. because LP was infeasible or the solve
  //   was imprecise).
  // If event == CallbackEvent::kMipSolution, the primal_solution contains the
  //   newly found primal (integer) feasible solution. The solution is always
  //   present.
  // Otherwise, the primal_solution is not available.
  std::optional<VariableMap<double>> solution;

  // Time since `Solve()` was called. Available for all events except
  // CallbackEvent::kPolling.
  absl::Duration runtime;

  // Only available for event == CallbackEvent::kPresolve.
  CallbackDataProto::PresolveStats presolve_stats;

  // Only available for event == CallbackEvent::kSimplex.
  CallbackDataProto::SimplexStats simplex_stats;

  // Only available for event == CallbackEvent::kBarrier.
  CallbackDataProto::BarrierStats barrier_stats;

  // Only available for event of CallbackEvent::kMip, CallbackEvent::kMipNode,
  // or CallbackEvent::kMipSolution.
  CallbackDataProto::MipStats mip_stats;
};

// The value returned by the Callback function.
struct CallbackResult {
  // Prefer AddUserCut and AddLazyConstraint below instead of using this
  // directly.
  struct GeneratedLinearConstraint {
    BoundedLinearExpression linear_constraint;
    bool is_lazy = false;

    const ModelStorage* storage() const {
      return linear_constraint.expression.storage();
    }
  };

  // Adds a "user cut," a linear constraint that excludes the current LP
  // solution but does not cut off any integer points. Use only for
  // CallbackEvent::kMipNode.
  void AddUserCut(BoundedLinearExpression linear_constraint) {
    new_constraints.push_back({std::move(linear_constraint), false});
  }

  // Adds a "lazy constraint," a linear constraint that excludes integer points.
  // Use only for CallbackEvent::kMipNode and CallbackEvent::kMipSolution.
  void AddLazyConstraint(BoundedLinearExpression linear_constraint) {
    new_constraints.push_back({std::move(linear_constraint), true});
  }

  // Returns a failure if the referenced variables don't belong to the input
  // expected_storage (which must not be nullptr).
  absl::Status CheckModelStorage(const ModelStorage* expected_storage) const;

  // Returns the proto equivalent of this object.
  //
  // The caller should use CheckModelStorage() as this function does not check
  // internal consistency of the referenced variables.
  CallbackResultProto Proto() const;

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

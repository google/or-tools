// Copyright 2010-2025 Google LLC
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

#include <iostream>
#include <ostream>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "ortools/base/init_google.h"
#include "ortools/math_opt/cpp/math_opt.h"

ABSL_FLAG(operations_research::math_opt::SolverType, solver,
          operations_research::math_opt::SolverType::kMinCostFlow,
          "What underlying LP or min cost flow solver to use.");
ABSL_FLAG(bool, gurobi_network, false,
          "Used only when solver is Gurobi, request network simplex (may be "
          "ignored).");

namespace {

namespace math_opt = ::operations_research::math_opt;

// We build and solve the min cost flow problem with nodes:
//
// Node | Net Supply
// s    | 5
// a    | 0
// b    | 0
// t    | -5
//
// (above, negative supply is demand, so we are routing 5 units of flow from s
// to t), and with arcs:
//
// Source | Dest | Cost | Capacity
// s      | a    | 3    | 5
// s      | b    | 5    | 5
// a      | t    | 0    | 2
// b      | t    | 0    | 5
//
// This graph has two s-t paths, s -> a -> t and s -> b -> t. The first path is
// cheaper, but we can only send 2 units of flow on it, so we send the other
// three units on the second path.
//
// In mathopt, we can detect min-cost-flow and run a faster algorithm when:
//  * There are only linear constraints
//  * Each variable appears in two constraints, with constraint coefficient of 1
//    once and -1 once.
//  * The variable lower bounds are zero, and the variable upper bounds are
//    nonnegative integers, and the variables are continuous.
//  * The objective coefficients are nonnegative integers.
//  * All linear constraints are equality constraints with integer RHS.
//
// See go/mathopt-min-cost-flow for more details.
//
// In such a formulation, the variables are the flow along an arc, and the
// linear constraints are flow conservation at each node. The LP is of the form:
//
// Data:
//  * G = (N, A): a directed graph
//  * s_n: the net supply of node n in N
//  * c_a: the cost of arc a
//  * u_a: the capacity of arc a
//
// Decision variables:
//  * x_a: flow on arc a.
//
// min     sum_{a in A} c_a x_a
// s.t.    sum_{a out of n} x_a - sum_{a into n} x_a = s_n  for all n in N
//         0 <= x_a  <= u_a
//
// So for this particular min cost flow problem, our model (on decision
// variables sa, sb, at, bt) is:
//
// min   3sa + 5sb
// s.t.  sa + sb =  5   (flow conservation s)
//       at - sa =  0   (flow conservation a)
//       bt - sb =  0   (flow conservation b)
//      -at - bt = -5   (flow conservation t)
//      0 <= sa <= 5
//      0 <= sb <= 5
//      0 <= at <= 2
//      0 <= bt <= 5
//
// We expect an optimal solution of: sa = at = 2, sb = bt = 3, cost 21.
absl::Status Main() {
  math_opt::Model model;
  const math_opt::Variable sa = model.AddContinuousVariable(0.0, 5.0);
  const math_opt::Variable sb = model.AddContinuousVariable(0.0, 5.0);
  const math_opt::Variable at = model.AddContinuousVariable(0.0, 2.0);
  const math_opt::Variable bt = model.AddContinuousVariable(0.0, 5.0);
  model.Minimize(3.0 * sa + 5.0 * sb);
  model.AddLinearConstraint(sa + sb == 5.0);
  model.AddLinearConstraint(at - sa == 0.0);
  model.AddLinearConstraint(bt - sb == 0.0);
  model.AddLinearConstraint(-at - bt == -5.0);

  const math_opt::SolverType solver_type = absl::GetFlag(FLAGS_solver);
  math_opt::SolveParameters params;
  params.enable_output = true;
  if (solver_type == math_opt::SolverType::kGurobi &&
      absl::GetFlag(FLAGS_gurobi_network)) {
    // This problem is so small that Gurobi can solve it in presolve, which
    // prevents network simplex from running. If we disable presolve, we can
    // see in the logs "Starting network simplex...", indicating that network
    // simplex actually ran.
    //
    // On a real problem, you should typically not disable presolve when you
    // have network structure as we do below.
    params.presolve = math_opt::Emphasis::kOff;
    params.gurobi.param_values["NetworkAlg"] = "1";
  }
  ABSL_ASSIGN_OR_RETURN(
      const math_opt::SolveResult result,
      Solve(model, solver_type, {.parameters = std::move(params)}));
  ABSL_RETURN_IF_ERROR(result.termination.EnsureIsOptimal());
  std::cout << "Objective value: " << result.objective_value() << std::endl;
  std::cout << "sa: " << result.variable_values().at(sa) << std::endl;
  std::cout << "sb: " << result.variable_values().at(sb) << std::endl;
  std::cout << "at: " << result.variable_values().at(at) << std::endl;
  std::cout << "bt: " << result.variable_values().at(bt) << std::endl;
  return absl::OkStatus();
}

}  // namespace

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  const absl::Status status = Main();
  if (!status.ok()) {
    LOG(QFATAL) << status;
  }
  return 0;
}

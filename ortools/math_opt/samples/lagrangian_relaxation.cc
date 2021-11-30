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

// Solves a constrained shortest path problem via Lagrangian Relaxation. The
// Lagrangian dual is solved with subgradient ascent.
//
// Problem data:
// * N: set of nodes.
// * A: set of arcs.
// * R: set of resources.
// * c_(i,j): cost of traversing arc (i,j) in A.
// * r_(i,j,k): resource k spent by traversing arc (i,j) in A, for all k in R.
// * b_i: flow balance at node i in N (+1 at the source, -1 at the sink, and 0
//        otherwise).
// * r_max_k: availability of resource k for a path, for all k in R.
//
// Decision variables:
// * x_(i,j): flow through arc (i,j) in A.
//
// Formulation:
// Z = min  sum(c_(i,j) * x_(i,j): (i,j) in A)
//     s.t.
//     sum(x_(i,j): (i,j) in A) - sum(x_(j,i): (j,i) in A) = b_i for all i in N,
//     sum(r_(i,j,k) * x_(i,j): (i,j) in A) <= r_max_k for all k in R,
//     x_(i,j) in {0,1} for all (i,j) in A.
//
// Upon dualizing a subset of the constraints (here we chose to relax some or
// all of the knapsack constraints), we obtaing a subproblem parameterized by
// dual variables mu (one per dualized constraint). We refer to this as the
// Lagrangian subproblem. Let R+ be the set of knapsack constraints that we
// keep, and R- the set of knapsack constraints that get dualized. The
// Lagrangian subproblem follows:
//
// z(mu) = min  sum(
//              (c_(i,j) - sum(mu_k * r_(i,j,k): k in R)) * x_(i,j): (i,j) in A)
//              + sum(mu_k * r_max_k: k in R-)
// s.t.
//   sum(x_(i,j): (i,j) in A) - sum(x_(j,i): (j,i) in A) = b_i for all i in N,
// sum(r_(i,j,k) * x_(i,j): (i,j) in A) <= r_max_k for all k in R+,
//   x_(i,j) in {0,1} for all (i,j) in A.
//
// We seek to solve the Lagrangian dual, which is of the form:
// Z_D = max{ z(mu) : mu <=0 }. Concavity of z(mu) allows us to solve the
// Lagrangian dual with the iterates:
// mu_(t+1) = mu_t + step_size_t * grad_mu_t, where
// grad_mu_t = r_max - sum(t_(i,j) * x_(i,j)^t: (i,j) in A) is a subgradient of
// z(mu_t) and x^t is an optimal solution to the problem induced by z(mu_t).
//
// In general we have that Z_D <= Z. For convex problems, Z_D = Z. For MIPs,
// Z_LP <= Z_D <= Z, where Z_LP is the linear relaxation of the original
// problem.
//
// In this particular example, we use two resource constraints. Either
// constraint or both can be dualized via the flags `dualize_resource_1` and
// `dualize_resource_2`. If both constraints are dualized, we have that Z_LP =
// Z_D because the resulting Lagrangian subproblem can be solved as a linear
// program (i.e., the problem becomes a pure shortest path problem upon
// dualizing all the side constraints). When only one of the side constraints is
// dualized, we can have Z_LP <= Z_D because the resulting Lagrangian subproblem
// needs to be solved as an MIP. For the particular data used in this example,
// dualizing only the first resource constraint leads to Z_LP < Z_D, while
// dualizing only the second resource constraint leads to Z_LP = Z_D. In either
// case, solving the Lagrandual dual also provides an upper bound to Z.
//
// Usage: blaze build -c opt
// ortools/math_opt/examples:lagrangian_relaxation
// blaze-bin/ortools/math_opt/examples/lagrangian_relaxation

#include <math.h>

#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ortools/base/container_logging.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"
#include "ortools/math_opt/cpp/math_opt.h"

ABSL_FLAG(double, step_size, 0.95,
          "Stepsize for gradient ascent, determined as step_size^t.");
ABSL_FLAG(int, max_iterations, 1000,
          "Max number of iterations for gradient ascent.");
ABSL_FLAG(bool, dualize_resource_1, true,
          "If true, the side constraint associated to resource 1 is dualized.");
ABSL_FLAG(bool, dualize_resource_2, false,
          "If true, the side constraint associated to resource 2 is dualized.");

ABSL_FLAG(bool, lagrangian_output, false,
          "If true, shows the iteration log of the subgradient ascent "
          "procedure use to solve the Lagrangian problem");

constexpr double kZeroTol = 1.0e-8;

namespace {
using ::operations_research::MathUtil;
using ::operations_research::math_opt::LinearExpression;
using ::operations_research::math_opt::MathOpt;
using ::operations_research::math_opt::Result;
using ::operations_research::math_opt::SolveParametersProto;
using ::operations_research::math_opt::SolverType;
using ::operations_research::math_opt::Variable;
using ::operations_research::math_opt::VariableMap;

struct Arc {
  int i;
  int j;
  double cost;
  double resource_1;
  double resource_2;
};

struct Graph {
  int num_nodes;
  std::vector<Arc> arcs;
  int source;
  int sink;
};

struct FlowModel {
  explicit FlowModel(SolverType solver_type) {
    model = std::make_unique<MathOpt>(solver_type, "LagrangianProblem");
  }
  std::unique_ptr<MathOpt> model;
  LinearExpression cost;
  LinearExpression resource_1;
  LinearExpression resource_2;
  std::vector<Variable> flow_vars;
};

// Populates `model` with variables and constraints of a shortest path problem.
FlowModel CreateShortestPathModel(const Graph graph) {
  FlowModel flow_model(operations_research::math_opt::SOLVER_TYPE_GSCIP);
  MathOpt& model = *flow_model.model;
  for (const Arc& arc : graph.arcs) {
    Variable var = model.AddContinuousVariable(
        /*lower_bound=*/0, /*upper_bound=*/1,
        /*name=*/absl::StrFormat("x_%d_%d", arc.i, arc.j));
    flow_model.cost += arc.cost * var;
    flow_model.resource_1 += arc.resource_1 * var;
    flow_model.resource_2 += arc.resource_2 * var;
    flow_model.flow_vars.push_back(var);
  }

  // Flow balance constraints
  std::vector<LinearExpression> out_flow(graph.num_nodes);
  std::vector<LinearExpression> in_flow(graph.num_nodes);
  for (int arc_index = 0; arc_index < graph.arcs.size(); ++arc_index) {
    out_flow[graph.arcs[arc_index].i] += flow_model.flow_vars[arc_index];
    in_flow[graph.arcs[arc_index].j] += flow_model.flow_vars[arc_index];
  }
  for (int node_index = 0; node_index < graph.num_nodes; ++node_index) {
    int rhs = node_index == graph.source ? 1
              : node_index == graph.sink ? -1
                                         : 0;
    model.AddLinearConstraint(out_flow[node_index] - in_flow[node_index] ==
                              rhs);
  }

  return flow_model;
}

Graph CreateSampleNetwork() {
  std::vector<Arc> arcs;
  arcs.push_back(
      {.i = 0, .j = 1, .cost = 12, .resource_1 = 1, .resource_2 = 1});
  arcs.push_back(
      {.i = 0, .j = 2, .cost = 3, .resource_1 = 2.5, .resource_2 = 1});
  arcs.push_back(
      {.i = 1, .j = 3, .cost = 5, .resource_1 = 1, .resource_2 = 1.5});
  arcs.push_back(
      {.i = 1, .j = 4, .cost = 5, .resource_1 = 2.5, .resource_2 = 1});
  arcs.push_back(
      {.i = 2, .j = 1, .cost = 7, .resource_1 = 2.5, .resource_2 = 1});
  arcs.push_back(
      {.i = 2, .j = 3, .cost = 5, .resource_1 = 7, .resource_2 = 2.5});
  arcs.push_back(
      {.i = 2, .j = 4, .cost = 1, .resource_1 = 6.5, .resource_2 = 1});
  arcs.push_back(
      {.i = 3, .j = 5, .cost = 6, .resource_1 = 1, .resource_2 = 2.0});
  arcs.push_back(
      {.i = 4, .j = 3, .cost = 3, .resource_1 = 1, .resource_2 = 0.5});
  arcs.push_back(
      {.i = 4, .j = 5, .cost = 5, .resource_1 = 2.5, .resource_2 = 1});
  const Graph graph = {.num_nodes = 6, .arcs = arcs, .source = 0, .sink = 5};

  return graph;
}

// Solves the constrained shortest path as an MIP.
FlowModel SolveMip(const Graph graph, const double max_resource_1,
                   const double max_resource_2) {
  FlowModel flow_model(operations_research::math_opt::SOLVER_TYPE_GSCIP);
  MathOpt& model = *flow_model.model;
  for (const Arc& arc : graph.arcs) {
    Variable var = model.AddBinaryVariable(
        /*name=*/absl::StrFormat("x_%d_%d", arc.i, arc.j));
    flow_model.cost += arc.cost * var;
    flow_model.resource_1 += +arc.resource_1 * var;
    flow_model.resource_2 += arc.resource_2 * var;
    flow_model.flow_vars.push_back(var);
  }

  // Flow balance constraints
  std::vector<LinearExpression> out_flow(graph.num_nodes);
  std::vector<LinearExpression> in_flow(graph.num_nodes);
  for (int arc_index = 0; arc_index < graph.arcs.size(); ++arc_index) {
    out_flow[graph.arcs[arc_index].i] += flow_model.flow_vars[arc_index];
    in_flow[graph.arcs[arc_index].j] += flow_model.flow_vars[arc_index];
  }
  for (int node_index = 0; node_index < graph.num_nodes; ++node_index) {
    int rhs = node_index == graph.source ? 1
              : node_index == graph.sink ? -1
                                         : 0;
    model.AddLinearConstraint(out_flow[node_index] - in_flow[node_index] ==
                              rhs);
  }

  model.AddLinearConstraint(flow_model.resource_1 <= max_resource_1,
                            "resource_ctr_1");
  model.AddLinearConstraint(flow_model.resource_2 <= max_resource_2,
                            "resource_ctr_2");
  model.objective().Minimize(flow_model.cost);
  SolveParametersProto params;
  params.mutable_common_parameters()->set_enable_output(false);
  const Result result = model.Solve(params).value();
  const VariableMap<double>& variable_values = result.variable_values();
  std::cout << "MIP Solution with 2 side constraints" << std::endl;
  std::cout << absl::StrFormat("MIP objective value: %6.3f",
                               result.objective_value())
            << std::endl;
  std::cout << "Resource 1: " << flow_model.resource_1.Evaluate(variable_values)
            << std::endl;
  std::cout << "Resource 2: " << flow_model.resource_2.Evaluate(variable_values)
            << std::endl;
  std::cout << "========================================" << std::endl;
  return flow_model;
}

// Solves the linear relaxation of a constrained shortest path problem
// formulated as an MIP.
void SolveLinearRelaxation(FlowModel& flow_model, const Graph& graph,
                           const double max_resource_1,
                           const double max_resource_2) {
  MathOpt& model = *flow_model.model;
  SolveParametersProto params;
  params.mutable_common_parameters()->set_enable_output(false);
  const Result result = model.Solve(params).value();
  const VariableMap<double>& variable_values = result.variable_values();
  std::cout << "LP relaxation with 2 side constraints" << std::endl;
  std::cout << absl::StrFormat("LP objective value: %6.3f",
                               result.objective_value())
            << std::endl;
  std::cout << "Resource 1: " << flow_model.resource_1.Evaluate(variable_values)
            << std::endl;
  std::cout << "Resource 2: " << flow_model.resource_2.Evaluate(variable_values)
            << std::endl;
  std::cout << "========================================" << std::endl;
}

void SolveLagrangianRelaxation(const Graph graph, const double max_resource_1,
                               const double max_resource_2) {
  // Model, variables, and linear expressions.
  FlowModel flow_model = CreateShortestPathModel(graph);
  MathOpt& model = *flow_model.model;
  LinearExpression& cost = flow_model.cost;
  LinearExpression& resource_1 = flow_model.resource_1;
  LinearExpression& resource_2 = flow_model.resource_2;
  SolveParametersProto params;
  params.mutable_common_parameters()->set_enable_output(false);

  // Dualized constraints and dual variable iterates.
  std::vector<double> mu;
  std::vector<LinearExpression> grad_mu;
  const bool dualized_resource_1 = absl::GetFlag(FLAGS_dualize_resource_1);
  const bool dualized_resource_2 = absl::GetFlag(FLAGS_dualize_resource_2);
  const bool lagrangian_output = absl::GetFlag(FLAGS_lagrangian_output);
  CHECK(dualized_resource_1 || dualized_resource_2)
      << "At least one of the side constraints should be dualized.";

  // Modify the lagrangian problem according to the constraints that are
  // dualized. We use a initial dual value different from zero to prioritize
  // finding a feasible solution.
  const double initial_dual_value = -10;
  if (dualized_resource_1 && !dualized_resource_2) {
    mu.push_back(initial_dual_value);
    grad_mu.push_back(max_resource_1 - resource_1);
    model.AddLinearConstraint(resource_2 <= max_resource_2);
    for (Variable& var : flow_model.flow_vars) {
      var.set_integer();
    }
  } else if (!dualized_resource_1 && dualized_resource_2) {
    mu.push_back(initial_dual_value);
    grad_mu.push_back(max_resource_2 - resource_2);
    model.AddLinearConstraint(resource_1 <= max_resource_1);
    for (Variable& var : flow_model.flow_vars) {
      var.set_integer();
    }
  } else {
    mu.push_back(initial_dual_value);
    mu.push_back(initial_dual_value);
    grad_mu.push_back(max_resource_1 - resource_1);
    grad_mu.push_back(max_resource_2 - resource_2);
  }

  // Gradient ascent setup
  bool termination = false;
  int iterations = 1;
  const double step_size = absl::GetFlag(FLAGS_step_size);
  CHECK_GT(step_size, 0) << "step_size must be strictly positive";
  CHECK_LT(step_size, 1) << "step_size must be strictly less than 1";
  const int max_iterations = absl::GetFlag(FLAGS_max_iterations);
  CHECK_GT(max_iterations, 0)
      << "Number of iterations must be strictly positive.";

  // Upper and lower bounds on the full problem.
  double upper_bound = std::numeric_limits<double>().infinity();
  double lower_bound = -std::numeric_limits<double>().infinity();
  double best_solution_resource_1 = 0;
  double best_solution_resource_2 = 0;

  if (lagrangian_output) {
    std::cout << "Starting gradient ascent..." << std::endl;
    std::cout << absl::StrFormat("%4s %6s %6s %9s %10s %10s", "Iter", "LB",
                                 "UB", "Step size", "mu_t", "grad_mu_t")
              << std::endl;
  }

  while (!termination) {
    LinearExpression lagrangian_function;
    lagrangian_function += cost;
    for (int k = 0; k < mu.size(); ++k) {
      lagrangian_function += mu[k] * grad_mu[k];
    }
    model.objective().Minimize(lagrangian_function);
    Result result = model.Solve(params).value();
    const VariableMap<double>& vars_val = result.variable_values();
    bool feasible = true;

    // Iterate update. Takes a step in the direction of the gradient (since the
    // Lagrangian dual is a max problem), and projects onto {mu: mu <=0} to
    // satisfy the sign of the dual variable. In general, convergence to an
    // optimal solution requires diminishing step sizes satisfying:
    //       * sum(step_size_t: t=1...) = infinity and,
    //       * sum((step_size_t)^2: t=1...) < infinity
    // See details in Prop 3.2.6 Bertsekas 2015, Convex Optimization Algorithms.
    // Here we use step_size_t = step_size^t which does NOT satisfy the
    // first condition, but is good enough for the purpose of this example.
    std::vector<double> grad_mu_vals;
    const double step_size_t = MathUtil::IPow(step_size, iterations);
    for (int k = 0; k < mu.size(); ++k) {
      // Evaluate resource k and evaluate the gradient of z(mu).
      const double grad_mu_val = grad_mu[k].Evaluate(vars_val);
      grad_mu_vals.push_back(grad_mu_val);
      mu[k] = std::min(0.0, mu[k] + step_size_t * grad_mu_val);
      if (grad_mu_val < 0) {
        feasible = false;
      }
    }
    // Bounds update
    const double path_cost = cost.Evaluate(vars_val);
    if (feasible && path_cost < upper_bound) {
      best_solution_resource_1 = resource_1.Evaluate(vars_val);
      best_solution_resource_2 = resource_2.Evaluate(vars_val);
      if (lagrangian_output) {
        std::cout << "Feasible solution with"
                  << absl::StrFormat(
                         "cost=%4.2f, resource_1=%4.2f, and resource_2=%4.2f. ",
                         path_cost, best_solution_resource_1,
                         best_solution_resource_2)
                  << std::endl;
      }
      upper_bound = path_cost;
    }
    if (lower_bound < result.objective_value()) {
      lower_bound = result.objective_value();
    }

    if (lagrangian_output) {
      std::cout << absl::StrFormat("%4d %6.3f %6.3f %9.3f", iterations,
                                   lower_bound, upper_bound, step_size_t)
                << " " << gtl::LogContainer(mu) << " "
                << gtl::LogContainer(grad_mu_vals) << std::endl;
    }

    // Termination criteria
    double norm = 0;
    for (double value : grad_mu_vals) {
      norm += (value * value);
    }
    norm = sqrt(norm);
    if (iterations == max_iterations || lower_bound == upper_bound ||
        step_size_t * norm < kZeroTol) {
      termination = true;
    }
    iterations++;
  }

  std::cout << "Lagrangian relaxation with 2 side constraints" << std::endl;
  std::cout << "Constraint for resource 1 dualized: "
            << (dualized_resource_1 ? "true" : "false") << std::endl;
  std::cout << "Constraint for resource 2 dualized: "
            << (dualized_resource_2 ? "true" : "false") << std::endl;
  std::cout << absl::StrFormat("Lower bound: %6.3f", lower_bound) << std::endl;
  std::cout << absl::StrFormat("Upper bound: %6.3f (Integer solution)",
                               upper_bound)
            << std::endl;
  std::cout << "========================================" << std::endl;
}

void RelaxModel(FlowModel& flow_model) {
  for (Variable& var : flow_model.flow_vars) {
    var.set_continuous();
    var.set_lower_bound(0.0);
    var.set_upper_bound(1.0);
  }
}

void SolveFullModel(const Graph& graph, double max_resource_1,
                    double max_resource_2) {
  FlowModel flow_model = SolveMip(graph, max_resource_1, max_resource_2);
  RelaxModel(flow_model);
  SolveLinearRelaxation(flow_model, graph, max_resource_1, max_resource_2);
}

}  // namespace

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  absl::ParseCommandLine(argc, argv);

  // Problem data
  const Graph graph = CreateSampleNetwork();
  const double max_resource_1 = 10;
  const double max_resource_2 = 4;

  SolveFullModel(graph, max_resource_1, max_resource_2);

  SolveLagrangianRelaxation(graph, max_resource_1, max_resource_2);
  return 0;
}

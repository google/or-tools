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
#include <random>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "ortools/base/init_google.h"
#include "ortools/math_opt/cpp/math_opt.h"

ABSL_FLAG(operations_research::math_opt::SolverType, solver,
          operations_research::math_opt::SolverType::kMinCostFlow,
          "What underlying LP or min cost flow solver to use.");
ABSL_FLAG(bool, gurobi_network, false,
          "Used only when solver is Gurobi, request network simplex (may be "
          "ignored).");

ABSL_FLAG(int, nodes, 10'000,
          "The number of nodes for each side of random bipartite input graph.");

namespace {

namespace math_opt = ::operations_research::math_opt;

// We encode a bipartite matching problem as a min cost flow problem.
//
// Problem data for bipartite matching on a (multi-)graph:
//
// * B = (L, R, E): a bipartite (multi-)graph, where |L| = |R| and which is
//                  REQUIRED to have a perfect matching.
// * c_e: the cost of using edge e.
//
// The goal is to find the perfect matching (a set of arcs that touch each node
// exactly once) at minimum cost. We allow for B to be a multigraph (you can
// have multip edges connecting any (l, r) pair) as this does not change the
// difficulty of the problem.
//
// We let adj(n) for n in L cup R be the arcs adjacent to node n (with one
// endpoint equal to n).
//
// The typical way to encode this in LP (on decision variable x_e) is:
//
// min sum_{e in E} c_e x_e
// s.t. sum_{e adj(l)} x_e = 1   for all l in L
//      sum_{e adj(r)} x_e = 1   for all r in R
//      0 <= x_e <= 1.
//
// (Note that the LP is integral, so any optimal basic solution will have x_e
// in {0, 1} and thus be a matching.)
//
// However, for min-cost-flow detection to work, we must have each variable
// appear in the constraint matrix once with coefficient +1 and once with
// coefficient -1. So instead, we encode the problem such that every L node has
// a net supply of +1, every node in R has a net supply of -1, and the edges are
// directed arcs from L to R. Effectively, we multiply the constraints for r in
// R by -1, arriving at the formulation:
//
// min sum_{e in E} c_e x_e
// s.t. sum_{e adj(l)}  x_e =  1   for all l in L
//      sum_{e adj(r)} -x_e = -1   for all r in R
//      0 <= x_e <= 1.
//
// which is now in min-cost-flow form (and this problem is still integral).
//
// In the example below, we generate our bipartite multi-graph as a function of
// a single parameter n (from FLAGS_nodes) as follows:
//   * n nodes in L
//   * n nodes in R
//   * 5*n edges with endpoints uniform on each side (repeat endpoints OK), each
//     with cost Uniform(0,100) and integer.
//   * An edge from i in L to i in R wth cost 100 (to ensure a perfect matching
//     exists).
//
// So a solution with cost 100 * num_nodes is always possible, and typically
// we can find a much better solution.
absl::Status Main() {
  absl::Time start = absl::Now();
  std::cout << "Generating random matching problem." << std::endl;
  const int num_nodes_per_side = absl::GetFlag(FLAGS_nodes);
  std::mt19937 bitgen;
  std::vector<std::tuple<int, int, int>> edges;
  const int max_cost = 100;
  for (int i = 0; i < 5 * num_nodes_per_side; ++i) {
    const int left = absl::Uniform(bitgen, 0, num_nodes_per_side);
    const int right = absl::Uniform(bitgen, 0, num_nodes_per_side);
    const int cost = absl::Uniform(bitgen, 0, max_cost);
    edges.push_back({left, right, cost});
  }
  // ensure always feasible
  for (int i = 0; i < num_nodes_per_side; ++i) {
    const int left = i;
    const int right = i;
    const int cost = max_cost;
    edges.push_back({left, right, cost});
  }
  std::vector<math_opt::LinearConstraint> left_cons;
  std::vector<math_opt::LinearConstraint> right_cons;
  std::cout << "Done generating problem (" << absl::Now() - start << ")"
            << std::endl;
  start = absl::Now();
  std::cout << "Building MathOpt model." << std::endl;
  math_opt::Model model;
  for (int i = 0; i < num_nodes_per_side; ++i) {
    math_opt::LinearConstraint left_c = model.AddLinearConstraint();
    model.set_lower_bound(left_c, 1.0);
    model.set_upper_bound(left_c, 1.0);
    left_cons.push_back(left_c);

    math_opt::LinearConstraint right_c = model.AddLinearConstraint();
    model.set_lower_bound(right_c, -1.0);
    model.set_upper_bound(right_c, -1.0);
    right_cons.push_back(right_c);
  }

  math_opt::LinearExpression obj;
  for (const auto [left, right, cost] : edges) {
    math_opt::Variable edge_var = model.AddContinuousVariable(0.0, 1.0);
    model.set_coefficient(left_cons[left], edge_var, 1.0);
    model.set_coefficient(right_cons[right], edge_var, -1.0);
    obj += cost * edge_var;
  }
  model.Minimize(obj);
  std::cout << "Done building model (" << absl::Now() - start << ")"
            << std::endl;
  start = absl::Now();
  std::cout << "Solving matching problem." << std::endl;

  const math_opt::SolverType solver_type = absl::GetFlag(FLAGS_solver);
  math_opt::SolveParameters params;
  params.enable_output = true;
  if (solver_type == math_opt::SolverType::kGurobi &&
      absl::GetFlag(FLAGS_gurobi_network)) {
    params.gurobi.param_values["NetworkAlg"] = "1";
  }
  ABSL_ASSIGN_OR_RETURN(
      const math_opt::SolveResult result,
      Solve(model, solver_type, {.parameters = std::move(params)}));
  std::cout << "Done solving (" << absl::Now() - start << ")" << std::endl;
  ABSL_RETURN_IF_ERROR(result.termination.EnsureIsOptimal());
  std::cout << "Cost: " << result.objective_value() << std::endl;
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

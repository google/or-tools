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

// A minimal TSP solver using MathOpt.
//
// In the Euclidean Traveling Salesperson Problem (TSP), you are given a list of
// n cities, each with an (x, y) coordinate, and you must find an order to visit
// the cities which minimizes the (Euclidean) travel distance.
//
// The MIP "cutset" formulation for the problem is as follows:
//   * Data:
//       n: An integer, the number of cities
//       (x_i, y_i): a pair of floats for each i in N={0..n-1}, the location of
//           each city
//       d_ij for all (i, j) pairs of cities, the distance between city i and j
//           (derived from the cities coordinates (x_i, y_i); this function is
//           symmetric, i.e. d_ij = d_ji).
//   * Decision variables:
//       x_ij: A binary variable, indicates if the edge connecting i and j is
//           used. Note that x_ij == x_ji, because the problem is symmetric. We
//           only create variables for i < j, and have x_ji as an alias for
//           x_ij.
//   * MIP model:
//       minimize sum_{i in N} sum_{j in N, j < i} d_ij * x_ij
//       s.t. sum_{j in N, j != i} x_ij = 2 for all i in N
//            sum_{i in S} sum_{j not in S} x_ij >= 2 for all S subset N
//                                                    |S| >= 3, |S| <= n - 3
//            x_ij in {0, 1}
// The first set of constraints are called the degree constraints, and the
// second set of constraints are called the cutset constraints. There are
// exponentially many cutset, so we cannot add them all at the start of the
// solve. Instead, we will use a solver callback to view each integer solution
// and add any violated cutset constraints that exist.
//
// Note that, while there are exponentially many cutset constraints, we can
// quickly identify violated ones by exploiting that the solution is integer
// and the degree constraints are all already in the model and satisfied. As a
// result, the graph n nodes and edges when x_ij = 1 will be a degree two graph,
// so it will be a collection of cycles. If it is a single large cycle, then the
// solution is feasible, and if there multiple cycles, then taking the nodes of
// any cycle as S produces a violated cutset constraint.
//
// Note that this is a minimal TSP solution, more sophisticated MIP methods are
// possible.

#include <cmath>
#include <iostream>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "ortools/base/helpers.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/base/options.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/math_opt.h"

ABSL_FLAG(int, num_cities, 50, "Number of cities in random TSP.");
ABSL_FLAG(std::string, output, "",
          "Write a svg of the solution here, or to standard out if empty.");
ABSL_FLAG(bool, test_instance, false,
          "Solve the test TSP instead of a random instance.");
ABSL_FLAG(int, threads, 0,
          "How many threads to solve with, or solver default if <= 0.");
ABSL_FLAG(bool, solve_logs, false,
          "Have the solver print logs to standard out.");
ABSL_FLAG(operations_research::math_opt::SolverType, solver,
          operations_research::math_opt::SolverType::kGscip,
          "What underlying MIP solver to use (must support callbacks).");

namespace {

namespace math_opt = operations_research::math_opt;
using Cycle = std::vector<int>;

// Creates variables modeling the undirected edges for the TSP. For every (i, j)
// pair in [0,n) * [0, n), a variable is created only for j < i, but querying
// for the variable x_ij with j > i returns x_ji. Querying for x_ii (which does
// not exist) gives a CHECK failure.
//
// The Model object passed in to create EdgeVariables must outlive this.
class EdgeVariables {
 public:
  EdgeVariables(math_opt::Model& model, const int n) {
    variables_.resize(n);
    for (int i = 0; i < n; ++i) {
      variables_[i].reserve(i);
      for (int j = 0; j < i; ++j) {
        variables_[i].push_back(
            model.AddBinaryVariable(absl::StrCat("e_", i, "_", j)));
      }
    }
  }

  math_opt::Variable get(const int i, const int j) const {
    CHECK_NE(i, j);
    return i > j ? variables_[i][j] : variables_[j][i];
  }

  int num_cities() const { return static_cast<int>(variables_.size()); }

 private:
  std::vector<std::vector<math_opt::Variable>> variables_;
};

// Produces a random TSP problem where cities have random locations that are
// I.I.D Uniform [0, 1].
std::vector<std::pair<double, double>> RandomCities(int num_cities) {
  absl::BitGen rand;
  std::vector<std::pair<double, double>> cities;
  for (int i = 0; i < num_cities; ++i) {
    cities.push_back({absl::Uniform<double>(rand, 0.0, 1.0),
                      absl::Uniform<double>(rand, 0.0, 1.0)});
  }
  return cities;
}

std::vector<std::pair<double, double>> TestCities() {
  return {{0, 0}, {0, 0.1}, {0.1, 0}, {0.1, 0.1},
          {1, 0}, {1, 0.1}, {0.9, 0}, {0.9, 0.1}};
}

// Given an n city TSP instance, computes the n by n distance matrix using the
// Euclidean distance.
std::vector<std::vector<double>> DistanceMatrix(
    absl::Span<const std::pair<double, double>> cities) {
  const int num_cities = static_cast<int>(cities.size());
  std::vector<std::vector<double>> distance_matrix(
      num_cities, std::vector<double>(num_cities, 0.0));
  for (int i = 0; i < num_cities; ++i) {
    for (int j = 0; j < num_cities; ++j) {
      if (i != j) {
        const double dx = cities[i].first - cities[j].first;
        const double dy = cities[i].second - cities[j].second;
        distance_matrix[i][j] = std::sqrt(dx * dx + dy * dy);
      }
    }
  }
  return distance_matrix;
}

// Given the EdgeVariables and a var_values containing the value of each edge in
// a solution, returns an n by n boolean matrix of which edges are used (with
// false diagonal elements). It is assumed that var_values are approximately 0-1
// integer.
std::vector<std::vector<bool>> EdgeValues(
    const EdgeVariables& edge_vars,
    const math_opt::VariableMap<double>& var_values) {
  const int n = edge_vars.num_cities();
  std::vector<std::vector<bool>> edge_values(n, std::vector<bool>(n, false));
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      if (i != j) {
        edge_values[i][j] = var_values.at(edge_vars.get(i, j)) > 0.5;
      }
    }
  }
  return edge_values;
}

// Given an n by n boolean matrix of edge values, returns a cycle decomposition.
// it is assumed that edge values respects the degree constraints (each row has
// only two true entries). Each cycle is represented as a list of cities with
// no repeats.
std::vector<Cycle> FindCycles(absl::Span<const std::vector<bool>> edge_values) {
  // Algorithm: maintain a "visited" bit for each city indicating if we have
  // formed a cycle containing this city. Consider the cities in order. When you
  // find an unvisited city, start a new cycle beginning at this city. Then,
  // build the cycle by finding an unvisited neighbor until no such neighbor
  // exists (every city will have two neighbors, but eventually both will be
  // visited). To find the "unvisited neighbor", we simply do a linear scan
  // over the cities, checking both the adjacency matrix and the visited bit.
  //
  // Note that for this algorithm, in each cycle, the city with lowest index
  // will be first, and the cycles will be sorted by their city of lowest index.
  // This is an implementation detail and should not be relied upon.
  const int n = static_cast<int>(edge_values.size());
  std::vector<Cycle> result;
  std::vector<bool> visited(n, false);
  for (int i = 0; i < n; ++i) {
    if (visited[i]) {
      continue;
    }
    std::vector<int> cycle;
    std::optional<int> next = i;
    while (next.has_value()) {
      cycle.push_back(*next);
      visited[*next] = true;
      int current = *next;
      next = std::nullopt;
      // Scan for an unvisited neighbor. We can start at i+1 since we know that
      // everything from i back is visited.
      for (int j = i + 1; j < n; ++j) {
        if (!visited[j] && edge_values[current][j]) {
          next = j;
          break;
        }
      }
    }
    result.push_back(cycle);
  }
  return result;
}

// Returns the cutset constraint for the given set of nodes.
math_opt::BoundedLinearExpression CutsetConstraint(
    absl::Span<const int> nodes, const EdgeVariables& edge_vars) {
  const int n = edge_vars.num_cities();
  const absl::flat_hash_set<int> node_set(nodes.begin(), nodes.end());
  std::vector<int> not_in_set;
  for (int i = 0; i < n; ++i) {
    if (!node_set.contains(i)) {
      not_in_set.push_back(i);
    }
  }
  math_opt::LinearExpression cutset_edges;
  for (const int in_set : nodes) {
    for (const int out_of_set : not_in_set) {
      cutset_edges += edge_vars.get(in_set, out_of_set);
    }
  }
  return cutset_edges >= 2;
}

// Solves the TSP by returning the ordering of the cities that minimizes travel
// distance.
absl::StatusOr<Cycle> SolveTsp(
    absl::Span<const std::pair<double, double>> cities,
    const math_opt::SolverType solver) {
  const int n = static_cast<int>(cities.size());
  const std::vector<std::vector<double>> distance_matrix =
      DistanceMatrix(cities);
  CHECK_GE(n, 3);
  math_opt::Model model("tsp");
  const EdgeVariables edge_vars(model, n);
  math_opt::LinearExpression edge_cost;
  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      edge_cost += edge_vars.get(i, j) * distance_matrix[i][j];
    }
  }
  model.Minimize(edge_cost);

  // Add the degree constraints
  for (int i = 0; i < n; ++i) {
    math_opt::LinearExpression neighbors;
    for (int j = 0; j < n; ++j) {
      if (i != j) {
        neighbors += edge_vars.get(i, j);
      }
    }
    model.AddLinearConstraint(neighbors == 2, absl::StrCat("n_", i));
  }
  math_opt::SolveArguments args;
  args.parameters.enable_output = absl::GetFlag(FLAGS_solve_logs);
  const int threads = absl::GetFlag(FLAGS_threads);
  if (threads > 0) {
    args.parameters.threads = threads;
  }
  args.callback_registration.events.insert(
      math_opt::CallbackEvent::kMipSolution);
  args.callback_registration.add_lazy_constraints = true;
  args.callback = [&edge_vars](const math_opt::CallbackData& cb_data) {
    // At event CallbackEvent::kMipSolution, a solution is always present.
    CHECK(cb_data.solution.has_value());
    const std::vector<Cycle> cycles =
        FindCycles(EdgeValues(edge_vars, *cb_data.solution));
    math_opt::CallbackResult result;
    if (cycles.size() > 1) {
      for (const Cycle& cycle : cycles) {
        result.AddLazyConstraint(CutsetConstraint(cycle, edge_vars));
      }
    }
    return result;
  };
  ASSIGN_OR_RETURN(const math_opt::SolveResult result,
                   math_opt::Solve(model, solver, args));
  RETURN_IF_ERROR(result.termination.EnsureIsOptimal());
  std::cout << "Route length: " << result.objective_value() << std::endl;
  const std::vector<Cycle> cycles =
      FindCycles(EdgeValues(edge_vars, result.variable_values()));
  CHECK_EQ(cycles.size(), 1);
  CHECK_EQ(cycles[0].size(), n);
  return cycles[0];
}

// Produces an SVG to draw a route for a TSP.
std::string RouteSvg(absl::Span<const std::pair<double, double>> cities,
                     const Cycle& cycle) {
  constexpr int image_px = 1000;
  constexpr int r = 5;
  constexpr int image_plus_border = image_px + 2 * r;
  std::vector<std::string> svg_lines;
  svg_lines.push_back(absl::StrCat("<svg width=\"", image_plus_border,
                                   "\" height=\"", image_plus_border, "\">"));
  std::vector<std::string> polygon_coords;
  for (const int city : cycle) {
    const int x =
        static_cast<int>(std::round(cities[city].first * image_px)) + r;
    const int y =
        static_cast<int>(std::round(cities[city].second * image_px)) + r;
    svg_lines.push_back(absl::StrCat("<circle cx=\"", x, "\" cy=\"", y,
                                     "\" r=\"", r, "\" fill=\"blue\" />"));
    polygon_coords.push_back(absl::StrCat(x, ",", y));
  }
  std::string polygon_coords_string = absl::StrJoin(polygon_coords, " ");
  svg_lines.push_back(
      absl::StrCat("<polygon fill=\"none\" stroke=\"blue\" points=\"",
                   polygon_coords_string, "\" />"));
  svg_lines.push_back("</svg>");
  return absl::StrJoin(svg_lines, "\n");
}

absl::Status RealMain() {
  std::vector<std::pair<double, double>> cities;
  if (absl::GetFlag(FLAGS_test_instance)) {
    cities = TestCities();
  } else {
    cities = RandomCities(absl::GetFlag(FLAGS_num_cities));
  }
  ASSIGN_OR_RETURN(const Cycle solution,
                   SolveTsp(cities, absl::GetFlag(FLAGS_solver)));
  const std::string svg = RouteSvg(cities, solution);
  if (absl::GetFlag(FLAGS_output).empty()) {
    std::cout << svg << std::endl;
  } else {
    RETURN_IF_ERROR(
        file::SetContents(absl::GetFlag(FLAGS_output), svg, file::Defaults()));
  }
  return absl::OkStatus();
}

}  // namespace

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  const absl::Status status = RealMain();
  if (!status.ok()) {
    LOG(QFATAL) << status;
  }
  return 0;
}

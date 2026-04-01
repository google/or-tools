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

// In the context of the graph coloring problem, we say that a proper coloring
// is an assignment of colors (labels) to the vertices such that adjacent
// vertices have different colors. Usually one is interested in finding the
// chromatic number of a graph, that is, the minimum number of colors that a
// proper coloring should have. However, in this example, we are only interested
// in the feasibility problem: given the graph G = (V, E) and a number k, is
// there a proper coloring which uses at most k colors? In this model, for each
// vertex i and color c, we have a binary variable x_i,c which indicates if
// vertex i is colored with color c. Then, enforcing the constraint
// x_i,c + x_j,c <= 1,
// for each edge (i, j) and color c, is equivalent to saying that
// adjacent vertices should have different colors. Hence, the model is as
// follows:
//    min     0 * x
//    s.t.    x_i,c + x_j,c <= 1,          for all edges (i, j) and color c
//            sum(x_(i,c) : color c) == 1, for all vertex i
//            x_i,c binary,                for all vertex i and color c
// This example uses a graph based on the bordering adjacencies of south
// american countries.
#include <cmath>
#include <iostream>
#include <limits>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "ortools/base/init_google.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/math_opt.h"

ABSL_FLAG(int, num_colors, 4, "Maximum number of colors.");
ABSL_FLAG(bool, solver_output, false, "Enable solver output.");

namespace {

namespace math_opt = operations_research::math_opt;

// a graph coloring solution is simply an assignment of colors
// to the vertices
struct GraphColoringSolution {
  std::vector<int> vertex_color;
};

// An instance of the graph coloring problem, to color the nodes of a graph
// using at most max_num_colors such that no neighboring nodes use the same
// color.
struct GraphColoringInstance {
  // The number of nodes in the graph
  int num_nodes = 0;

  // The number of colors allowed
  int num_colors = 0;

  // The undirected edges of the graph
  std::vector<std::pair<int, int>> edges;
};

// Solves the graph coloring problem.
absl::StatusOr<GraphColoringSolution> SolveGraphColoring(
    const GraphColoringInstance& instance) {
  // Create the model. Since we are just checking feasibility,
  // the objective function is empty, i.e., zero
  math_opt::Model model("graph_coloring");

  // declare variables x_{i, c} for each vertex i and color c
  std::vector<std::vector<math_opt::Variable>> x(instance.num_nodes);
  for (int i = 0; i < instance.num_nodes; ++i) {
    x[i].reserve(instance.num_colors);
    for (int c = 0; c < instance.num_colors; ++c) {
      x[i].push_back(model.AddBinaryVariable(absl::StrCat("x_", i, "_", c)));
    }
  }

  // add color conflict constraints
  for (const auto [i, j] : instance.edges) {
    for (int c = 0; c < instance.num_colors; ++c) {
      model.AddLinearConstraint(x[i][c] + x[j][c] <= 1.0,
                                absl::StrCat("edge_", i, ",", j, "_color_", c));
    }
  }

  // add requirement that each vertex should have a color
  for (int i = 0; i < instance.num_nodes; ++i) {
    model.AddLinearConstraint(math_opt::Sum(x[i]) == 1.0,
                              absl::StrCat("vertex_", i));
  }

  // Set parameters, e.g. turn on logging.
  math_opt::SolveArguments args;
  args.parameters.enable_output = absl::GetFlag(FLAGS_solver_output);

  // solve the model and check the result
  ASSIGN_OR_RETURN(const math_opt::SolveResult result,
                   Solve(model, math_opt::SolverType::kCpSat));
  RETURN_IF_ERROR(result.termination.EnsureIsOptimalOrFeasible());

  // build solution from solver output
  GraphColoringSolution solution;
  solution.vertex_color.resize(instance.num_nodes);
  for (int i = 0; i < instance.num_nodes; ++i) {
    for (int c = 0; c < instance.num_colors; ++c) {
      if (std::round(result.variable_values().at(x[i][c])) == 1.0) {
        solution.vertex_color[i] = c;
      }
    }
  }

  return solution;
}

absl::Status RealMain() {
  // ids for south america countries
  constexpr int kColombia = 0;
  constexpr int kEcuador = 1;
  constexpr int kVenezuela = 2;
  constexpr int kGuyana = 3;
  constexpr int kSuriname = 4;
  constexpr int kFrenchGuyana = 5;
  constexpr int kBrazil = 6;
  constexpr int kPeru = 7;
  constexpr int kBolivia = 8;
  constexpr int kChile = 9;
  constexpr int kArgentina = 10;
  constexpr int kUruguay = 11;
  constexpr int kParaguay = 12;

  GraphColoringInstance instance;
  instance.num_nodes = 13;
  instance.num_colors = absl::GetFlag(FLAGS_num_colors);
  instance.edges = {{kBrazil, kFrenchGuyana},  {kBrazil, kSuriname},
                    {kBrazil, kGuyana},        {kBrazil, kVenezuela},
                    {kBrazil, kColombia},      {kBrazil, kPeru},
                    {kBrazil, kBolivia},       {kBrazil, kParaguay},
                    {kBrazil, kUruguay},       {kBrazil, kArgentina},
                    {kArgentina, kUruguay},    {kArgentina, kParaguay},
                    {kArgentina, kBolivia},    {kArgentina, kChile},
                    {kPeru, kEcuador},         {kPeru, kColombia},
                    {kPeru, kBolivia},         {kPeru, kChile},
                    {kBolivia, kChile},        {kBolivia, kParaguay},
                    {kColombia, kEcuador},     {kColombia, kVenezuela},
                    {kGuyana, kSuriname},      {kGuyana, kVenezuela},
                    {kSuriname, kFrenchGuyana}};

  // The chromatic number of this graph is 4. The graph is planar
  // and it has a 4-clique (Brazil, Bolivia, Paraguay, Argentina)
  // https://en.wikipedia.org/wiki/Four_color_theorem
  ASSIGN_OR_RETURN(GraphColoringSolution solution,
                   SolveGraphColoring(instance));

  const std::vector<std::string> vertex_names = {
      "Colombia",      "Ecuador", "Venezuela", "Guyana",  "Suriname",
      "French Guyana", "Brazil",  "Peru",      "Bolivia", "Chile",
      "Argentina",     "Uruguay", "Paraguay"};
  const std::vector<std::string> color_names = {"Red", "Green", "Blue",
                                                "Yellow"};

  std::cout << "Graph can be colored with " << absl::GetFlag(FLAGS_num_colors)
            << " colors as follows:" << std::endl;
  for (int i = 0; i < instance.num_nodes; ++i) {
    std::cout << "country: " << vertex_names[i]
              << " color: " << color_names[solution.vertex_color[i]]
              << std::endl;
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

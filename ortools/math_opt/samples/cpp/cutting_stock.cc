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

// The Cutting Stock problem is as follows. You begin with unlimited boards, all
// of the same length. You are also given a list of smaller pieces to cut out,
// each with a length and a demanded quantity. You want to cut out all these
// pieces using as few of your starting boards as possible.
//
// E.g. you begin with boards that are 20 feet long, and you must cut out 3
// pieces that are 6 feet long and 5 pieces that are 8 feet long. An optimal
// solution is:
//   [(6,), (8, 8) (8, 8), (6, 6, 8)]
// (We cut a 6 foot piece from the first board, two 8 foot pieces from
// the second board, and so on.)
//
// This example approximately solves the problem with a column generation
// heuristic. The leader problem is a set cover problem, and the worker is an
// unbounded knapsack problem. We alternate between solving the LP relaxation of
// the leader incrementally, and solving the worker to generate new a
// configuration (a column) for the leader. When the worker can no longer find a
// column improving the LP cost, we convert the leader problem to a MIP and
// solve again. We now give precise statements of the leader and worker.
//
// Problem data:
//  * P: the set of pieces
//  * l_i: the length of each piece we need to cut out, for all i in P.
//  * d_i: how many copies of each piece we need, for all i in P.
//  * L: the length of our initial boards.
//  * C: the set of configurations. A configuration specifies a feasible set of
//        pieces to cut from a board (see q_ci below). Note that there are
//        exponentially many configurations.
//  * q_ci: for configuration c in C, the quantity of piece i in P to cut from a
//        board (a nonnegative integer).
//
// Leader problem variables:
//  * x_c: how many copies of configuration c in C to produce.
//
// Leader problem formulation:
//   min  sum_{c in C} x_c
//   s.t. sum_{c in C} q_ci * x_c = d_i, for all i in P
//        x_c >= 0, integer for all c in C.
//
// The worker problem is to generate new configurations for the leader problem
// based on the dual variables of the demand constraints in the LP relaxation.
// Worker problem data:
//   * p_i: The "price" of piece i in P (dual value from leader's demand
//         constraint)
//
// Worker decision variables:
//  * y_i: How many copies of piece i in P should be in the configuration.
//
// Worker formulation
//   max   sum_{i in P} p_i * y_i
//   s.t.  sum_{i in P} l_i * y_i <= L
//         y_i >= 0, integer for all i in P
//
// An optimal solution y* defines a new configuration c with q_ci = y_i* for all
// i in P. If the solution has objective value <= 1, no further improvement on
// the LP is possible. For additional background and proofs see:
//   https://people.orie.cornell.edu/shmoys/or630/notes-06/lec16.pdf
// or any other reference on the "Cutting Stock Problem".
//
// Note: this problem is equivalent to symmetric bin packing:
//   https://en.wikipedia.org/wiki/Bin_packing_problem#Formal_statement
// but typically in bin packing it is not assumed that you should exploit having
// multiple items of the same size.
#include <cmath>
#include <iostream>
#include <limits>
#include <ostream>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "ortools/base/init_google.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace {

namespace math_opt = operations_research::math_opt;
constexpr double kInf = std::numeric_limits<double>::infinity();

// piece_lengths and piece_demands must have equal length.
// every piece must have 0 < length <= board_length.
// every piece must have demand > 0.
struct CuttingStockInstance {
  std::vector<int> piece_lengths;
  std::vector<int> piece_demands;
  int board_length;
};

// pieces and quantity must have equal size.
// Defined for a related CuttingStockInstance, the total length all pieces
// weighted by their quantity must not exceed board_length.
struct Configuration {
  std::vector<int> pieces;
  std::vector<int> quantity;
};

// configurations and quantity must have equal size.
// objective_value is the sum of the vales in quantity (how many total boards
// are used).
// To be feasible, the demand for each piece type must be met by the produced
// configurations.
struct CuttingStockSolution {
  std::vector<Configuration> configurations;
  std::vector<int> quantity;
  int objective_value = 0;
};

// Solves the worker problem.
//
// Solves the problem on finding the configuration (with its objective value) to
// add to the leader model that will give the greatest improvement in the LP
// relaxation. This is equivalent to a knapsack problem.
absl::StatusOr<std::pair<Configuration, double>> BestConfiguration(
    const std::vector<double>& piece_prices,
    const std::vector<int>& piece_lengths, const int board_length) {
  int num_pieces = piece_prices.size();
  CHECK_EQ(piece_lengths.size(), num_pieces);
  math_opt::Model model("knapsack");
  std::vector<math_opt::Variable> pieces;
  for (int i = 0; i < num_pieces; ++i) {
    pieces.push_back(
        model.AddIntegerVariable(0, kInf, absl::StrCat("piece_", i)));
  }
  model.Maximize(math_opt::InnerProduct(pieces, piece_prices));
  model.AddLinearConstraint(math_opt::InnerProduct(pieces, piece_lengths) <=
                            board_length);
  ASSIGN_OR_RETURN(const math_opt::SolveResult solve_result,
                   math_opt::Solve(model, math_opt::SolverType::kCpSat));
  RETURN_IF_ERROR(solve_result.termination.EnsureIsOptimal());
  Configuration config;
  for (int i = 0; i < num_pieces; ++i) {
    const int use = static_cast<int>(
        std::round(solve_result.variable_values().at(pieces[i])));
    if (use > 0) {
      config.pieces.push_back(i);
      config.quantity.push_back(use);
    }
  }
  return std::make_pair(config, solve_result.objective_value());
}

// Solves the full cutting stock problem by decomposition.
absl::StatusOr<CuttingStockSolution> SolveCuttingStock(
    const CuttingStockInstance& instance) {
  math_opt::Model model("cutting_stock");
  model.set_minimize();
  const int n = static_cast<int>(instance.piece_lengths.size());
  std::vector<math_opt::LinearConstraint> demand_met;
  for (int i = 0; i < n; ++i) {
    const int d = instance.piece_demands[i];
    demand_met.push_back(model.AddLinearConstraint(d, d));
  }
  std::vector<std::pair<Configuration, math_opt::Variable>> configs;
  auto add_config = [&](const Configuration& config) {
    const math_opt::Variable v = model.AddContinuousVariable(0.0, kInf);
    model.set_objective_coefficient(v, 1);
    for (int i = 0; i < config.pieces.size(); ++i) {
      const int piece = config.pieces[i];
      const int use = config.quantity[i];
      if (use >= 1) {
        model.set_coefficient(demand_met[piece], v, use);
      }
    }
    configs.push_back({config, v});
  };

  // To ensure the leader problem is always feasible, begin with one
  // configuration per piece, each having a single copy of the piece.
  for (int i = 0; i < n; ++i) {
    add_config(Configuration{.pieces = {i}, .quantity = {1}});
  }

  ASSIGN_OR_RETURN(auto solver, math_opt::NewIncrementalSolver(
                                    &model, math_opt::SolverType::kGlop));
  int pricing_round = 0;
  while (true) {
    ASSIGN_OR_RETURN(math_opt::SolveResult solve_result, solver->Solve());
    RETURN_IF_ERROR(solve_result.termination.EnsureIsOptimal())
        << " at iteration " << pricing_round;
    if (!solve_result.has_dual_feasible_solution()) {
      // MathOpt does not require solvers to return a dual solution on optimal,
      // but most LP solvers always will.
      return util::InternalErrorBuilder()
             << "no dual solution was returned with optimal solution at "
                "iteration "
             << pricing_round;
    }
    std::vector<double> prices;
    for (const math_opt::LinearConstraint d : demand_met) {
      prices.push_back(solve_result.dual_values().at(d));
    }
    ASSIGN_OR_RETURN((const auto [config, value]),
                     BestConfiguration(prices, instance.piece_lengths,
                                       instance.board_length));
    if (value <= 1 + 1e-3) {
      // The LP relaxation is solved, we can stop adding columns.
      break;
    }
    add_config(config);
    LOG(INFO) << "round: " << pricing_round
              << " lp objective: " << solve_result.objective_value();
    pricing_round++;
  }
  LOG(INFO) << "Done adding columns, switching to MIP";
  for (const auto& [config, var] : configs) {
    model.set_integer(var);
  }
  ASSIGN_OR_RETURN(const math_opt::SolveResult solve_result,
                   math_opt::Solve(model, math_opt::SolverType::kCpSat));
  RETURN_IF_ERROR(solve_result.termination.EnsureIsOptimalOrFeasible())
      << " in final cutting stock MIP";
  CuttingStockSolution solution;
  for (const auto& [config, var] : configs) {
    int use =
        static_cast<int>(std::round(solve_result.variable_values().at(var)));
    if (use > 0) {
      solution.configurations.push_back(config);
      solution.quantity.push_back(use);
      solution.objective_value += use;
    }
  }
  return solution;
}

absl::Status RealMain() {
  // Data from https://en.wikipedia.org/wiki/Cutting_stock_problem
  CuttingStockInstance instance;
  instance.board_length = 5600;
  instance.piece_lengths = {1380, 1520, 1560, 1710, 1820, 1880, 1930,
                            2000, 2050, 2100, 2140, 2150, 2200};
  instance.piece_demands = {22, 25, 12, 14, 18, 18, 20, 10, 12, 14, 16, 18, 20};
  ASSIGN_OR_RETURN(CuttingStockSolution solution, SolveCuttingStock(instance));
  std::cout << "Best known solution uses 73 boards." << std::endl;
  std::cout << "Total boards used in actual solution found: "
            << solution.objective_value << std::endl;
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

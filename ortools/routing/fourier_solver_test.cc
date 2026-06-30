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

#include "ortools/routing/fourier_solver.h"

#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "google/protobuf/duration.pb.h"
#include "gtest/gtest.h"
#include "ortools/util/optional_boolean.pb.h"

namespace operations_research::routing {

// Implement equality, comparison and hashability for local Arc classes.

// All Arc local classes in tests below are `TupleConvertible`.
template <typename T>
concept TupleConvertible = requires(const T& a) {
  { a.AsTuple() };
};

// Implements equality (and inequality) of `TupleConvertible` types.
template <typename T>
bool operator==(const T& a, const T& b)
  requires TupleConvertible<T>
{
  return a.AsTuple() == b.AsTuple();
}

// Implements comparison of `TupleConvertible` types.
template <typename T>
auto operator<=>(const T& a, const T& b)
  requires TupleConvertible<T>
{
  return a.AsTuple() <=> b.AsTuple();
}

// Implements hashing of `TupleConvertible` types.
template <typename H, typename T>
H AbslHashValue(H h, const T& value)
  requires TupleConvertible<T>
{
  return H::combine(std::move(h), value.AsTuple());
}

TEST(MaxLinearExpressionEvaluatorTest, NoExpressions) {
  MaxLinearExpressionEvaluator evaluator({});
  EXPECT_EQ(evaluator.Evaluate({}), -std::numeric_limits<double>::infinity());
}

TEST(MaxLinearExpressionEvaluatorTest, NoVariables) {
  MaxLinearExpressionEvaluator evaluator({{}, {}, {}});
  EXPECT_EQ(evaluator.Evaluate({}), 0.0);
}

TEST(MaxLinearExpressionEvaluatorTest, OneVariable) {
  MaxLinearExpressionEvaluator evaluator({{1.0}, {2.0}, {3.0}});
  EXPECT_EQ(evaluator.Evaluate({1.0}), 3.0);
  EXPECT_EQ(evaluator.Evaluate({-1.0}), -1.0);
}

TEST(MaxLinearExpressionEvaluatorTest, TwoVariables) {
  MaxLinearExpressionEvaluator evaluator({{1.0, -1.0}, {5.0, 1.0}, {3.0, 3.0}});
  EXPECT_EQ(evaluator.Evaluate({0.0, -2.0}), 2.0);  // Row 0 is the max.
  EXPECT_EQ(evaluator.Evaluate({1.0, -1.0}), 4.0);  // Row 1 is the max.
  EXPECT_EQ(evaluator.Evaluate({1.0, 2.0}), 9.0);   // Row 2 is the max.

  EXPECT_EQ(evaluator.Evaluate({1.0, -2.0}), 3.0);  // Rows 0 and 1 are the max.
  EXPECT_EQ(evaluator.Evaluate({1.0, 1.0}), 6.0);   // Rows 1 and 2 are the max.
}

// For many number of variables and expressions, set only one nonzero term,
// make sure that activating it with a particular value vector works.
TEST(MaxLinearExpressionEvaluatorTest, ManyVariablesManyExpressions) {
  const int max_vars = 16;
  const int max_rows = 32;
  for (int num_vars = 1; num_vars <= max_vars; ++num_vars) {
    for (int num_rows = 1; num_rows <= max_rows; ++num_rows) {
      for (int argmax = 0; argmax < num_rows; ++argmax) {
        std::vector<std::vector<double>> rows;
        for (int r = 0; r < num_rows; ++r) {
          std::vector<double> row(num_vars, 0.0);
          if (r == argmax) row[r % num_vars] = 5.0;
          rows.push_back(std::move(row));
        }
        MaxLinearExpressionEvaluator evaluator(rows);
        std::vector<double> values(num_vars, 0.0);
        EXPECT_EQ(evaluator.Evaluate(values), 0.0);
        values[argmax % num_vars] = 1.0;
        EXPECT_EQ(evaluator.Evaluate(values), 5.0);
      }
    }
  }
}

void BM_MaxLinearExpressionEvaluator_Evaluate(benchmark::State& bench_state) {
  const int num_variables = bench_state.range(0);
  const int num_constraints = bench_state.range(1);
  // Make constraints.
  std::vector<std::vector<double>> rows;
  for (int i = 0; i < num_constraints; ++i) {
    std::vector<double> row;
    row.reserve(num_variables);
    for (int j = 0; j < num_variables; ++j) {
      row.push_back(i + 1);
    }
    rows.push_back(std::move(row));
  }
  MaxLinearExpressionEvaluator evaluator(rows);
  std::vector<double> values(num_variables, 0.0);
  absl::c_iota(values, 1);
  int64_t num_items = 0;
  for (auto _ : bench_state) {
    benchmark::DoNotOptimize(evaluator.Evaluate(values));
    ++num_items;
  }
  bench_state.SetItemsProcessed(num_items);
  bench_state.SetBytesProcessed(num_items * num_constraints * num_variables *
                                sizeof(double));
}

BENCHMARK(BM_MaxLinearExpressionEvaluator_Evaluate)
    ->RangePair(1, 1 << 8, 1, 1 << 8);

using FourierSolverTest = ::testing::TestWithParam<int>;

TEST_P(FourierSolverTest, NCubeTest) {
  // With variables x_i in [0, 1], minimize objective -sum_i x_i.
  const int num_variables = GetParam();
  FourierSolver solver;
  for (int i = 0; i < num_variables; ++i) {
    const FourierSolver::ColIndex xi = solver.AddVariable(0, 1);
    solver.SetObjectiveCoefficient(xi, -1);
  }
  ASSERT_TRUE(solver.Solve());
  EXPECT_EQ(solver.EvaluateObjective(), -num_variables);
}

INSTANTIATE_TEST_SUITE_P(FMESolverTest, FourierSolverTest,
                         ::testing::Values(1, 2, 3, 4, 5, 6, 7));

// FMESolver Tests.
TEST(FMESolverTest, ShortestPathFlowEncoding) {
  // Shortest path on a graph. We set one {0, 1} variable per arc, there must be
  // a flow of 1 from origin to destination, the cost is the sum of lengths of
  // arcs set to 1.
  struct Scenario {
    int num_nodes;
    struct Arc {
      int tail;
      int head;
      double length;

      auto AsTuple() const { return std::make_tuple(tail, head, length); }
    };
    std::vector<Arc> arcs;
    int source;
    int destination;
    bool feasible;
    double objective;
  };
  static_assert(TupleConvertible<Scenario::Arc>);
  // Some scenarios are from "Network Flows", Ahuja, Magnanti, Orlin.
  std::vector<Scenario> scenarios = {
      {// Simple route 0 -> 1 -> 2 -> 3.
       .num_nodes = 4,
       .arcs =
           {
               {.tail = 0, .head = 1, .length = 4.0},
               {.tail = 1, .head = 2, .length = 3.0},
               {.tail = 2, .head = 3, .length = 5.0},
           },
       .source = 0,
       .destination = 3,
       .feasible = true,
       .objective = 12.0},
      {// Infeasible route 0 -> 1  3 -> 2.
       .num_nodes = 4,
       .arcs =
           {
               {.tail = 0, .head = 1, .length = 4.0},
               {.tail = 3, .head = 2, .length = 5.0},
           },
       .source = 0,
       .destination = 3,
       .feasible = false,
       .objective = 0.0},
      {
          // Network Flows, figure 4.7.
          .num_nodes = 7,  // Node 0 is not used.
          .arcs =
              {
                  {.tail = 1, .head = 2, .length = 6.0},
                  {.tail = 1, .head = 3, .length = 4.0},
                  {.tail = 2, .head = 3, .length = 2.0},
                  {.tail = 2, .head = 4, .length = 2.0},
                  {.tail = 3, .head = 4, .length = 1.0},
                  {.tail = 3, .head = 5, .length = 2.0},
                  {.tail = 4, .head = 6, .length = 7.0},
                  {.tail = 5, .head = 4, .length = 1.0},
                  {.tail = 5, .head = 6, .length = 3.0},
              },
          .source = 1,
          .destination = 6,
          .feasible = true,
          .objective = 9.0  // 1 -> 3 -> 5 -> 6.
      },
      {
          // Network Flows, figure 4.15 left.
          .num_nodes = 7,  // Node 0 is not used.
          .arcs =
              {
                  {.tail = 1, .head = 2, .length = 2.0},
                  {.tail = 1, .head = 3, .length = 8.0},
                  {.tail = 2, .head = 3, .length = 5.0},
                  {.tail = 2, .head = 4, .length = 3.0},
                  {.tail = 3, .head = 2, .length = 6.0},
                  {.tail = 3, .head = 5, .length = 0.0},
                  {.tail = 4, .head = 3, .length = 1.0},
                  {.tail = 4, .head = 5, .length = 7.0},
                  {.tail = 4, .head = 6, .length = 6.0},
                  {.tail = 5, .head = 4, .length = 4.0},
                  {.tail = 6, .head = 5, .length = 2.0},
              },
          .source = 1,
          .destination = 6,
          .feasible = true,
          .objective = 11.0  // 1 -> 2 -> 4 -> 6.
      },
      {
          // Network Flows, figure 4.15 right.
          .num_nodes = 13,  // Node 0 is not used.
          .arcs =
              {
                  {.tail = 1, .head = 2, .length = 5.0},
                  {.tail = 1, .head = 4, .length = 10.0},
                  {.tail = 2, .head = 3, .length = 7.0},
                  {.tail = 2, .head = 5, .length = 1.0},
                  {.tail = 3, .head = 6, .length = 4.0},
                  {.tail = 4, .head = 5, .length = 3.0},
                  {.tail = 4, .head = 11, .length = 11.0},
                  {.tail = 5, .head = 6, .length = 3.0},
                  {.tail = 5, .head = 8, .length = 7.0},
                  {.tail = 6, .head = 9, .length = 5.0},
                  {.tail = 7, .head = 8, .length = 2.0},
                  {.tail = 7, .head = 10, .length = 9.0},
                  {.tail = 8, .head = 9, .length = 0.0},
                  {.tail = 8, .head = 11, .length = 1.0},
                  {.tail = 9, .head = 12, .length = 12.0},
                  {.tail = 10, .head = 11, .length = 2.0},
                  {.tail = 11, .head = 12, .length = 4.0},
              },
          .source = 1,
          .destination = 12,
          .feasible = true,
          .objective = 18.0  // 1 -> 2 -> 5 -> 8 -> 11.
      },
  };

  for (int s = 0; s < 4; ++s) {
    SCOPED_TRACE(absl::StrCat("Scenario ", s));
    const Scenario& scenario = scenarios[s];
    FourierSolver solver;
    // Node linear expressions: at each node, outflow - inflow = supply.
    // Foreach node, we keep outflow - inflow as a vector of CoefVars.
    using CoefVar = FourierSolver::CoefficientVariable;
    std::vector<std::vector<CoefVar>> linexpr_of_node(scenario.num_nodes);
    using ColIndex = FourierSolver::ColIndex;
    for (const Scenario::Arc& arc : scenario.arcs) {
      const ColIndex var = solver.AddVariable(0, 1);
      linexpr_of_node[arc.tail].push_back({1.0, var});
      linexpr_of_node[arc.head].push_back({-1.0, var});
      solver.SetObjectiveCoefficient(var, arc.length);
    }
    for (int n = 0; n < scenario.num_nodes; ++n) {
      double supply = 0.0;
      if (n == scenario.source) supply = 1.0;
      if (n == scenario.destination) supply = -1.0;
      solver.AddConstraint(supply, supply, linexpr_of_node[n]);
    }
    EXPECT_EQ(solver.Solve(), scenario.feasible);
    if (scenario.feasible) {
      EXPECT_EQ(solver.EvaluateObjective(), scenario.objective);
    }
  }
}

TEST(FMESolverTest, ShortestPaths) {
  // Shortest path on a graph. We set one variable per node, the distance from
  // the origin, and look for the shortest path to destination.
  struct Scenario {
    int num_nodes;
    struct Arc {
      int tail;
      int head;
      double length;

      auto AsTuple() const { return std::make_tuple(tail, head, length); }
    };
    std::vector<Arc> arcs;
    int source;
    int destination;
    bool feasible;
    double objective;
  };
  static_assert(TupleConvertible<Scenario::Arc>);
  std::vector<Scenario> scenarios = {
      {// Simple route 0 -> 1 -> 2 -> 3.
       .num_nodes = 4,
       .arcs =
           {
               {.tail = 0, .head = 1, .length = 4.0},
               {.tail = 1, .head = 2, .length = 3.0},
               {.tail = 2, .head = 3, .length = 5.0},
           },
       .source = 0,
       .destination = 3,
       .feasible = true,
       .objective = 12.0},
  };

  constexpr double kInfinity = std::numeric_limits<double>::infinity();

  for (int s = 0; s < scenarios.size(); ++s) {
    SCOPED_TRACE(absl::StrCat("Scenario ", s));
    const Scenario& scenario = scenarios[s];
    FourierSolver solver;
    // Node variables.
    using ColIndex = FourierSolver::ColIndex;
    std::vector<ColIndex> distance;
    for (int n = 0; n < scenario.num_nodes; ++n) {
      distance.push_back(solver.AddVariable(0.0, 1e6));
    }
    // Arc constraints: distance[head] <= distance[tail] + length(tail -> head).
    for (const Scenario::Arc& arc : scenario.arcs) {
      solver.AddConstraint(
          -kInfinity, arc.length,
          {{1.0, distance[arc.head]}, {-1.0, distance[arc.tail]}});
    }
    // Maximize distance(destination) - distance(source), so minimize the
    // opposite.
    solver.SetObjectiveCoefficient(distance[scenario.destination], -1.0);
    solver.SetObjectiveCoefficient(distance[scenario.source], 1.0);
    EXPECT_EQ(solver.Solve(), scenario.feasible);
    if (scenario.feasible) {
      EXPECT_EQ(solver.EvaluateObjective(), -scenario.objective);
    }
  }
}

// TODO(b/492476073): Add more tests. For instance Shortest path on DAG,
// Shortest path on general graph, Shortest path with negative loop, Min flow,
// Knapsack, min cost assignment, general LP.

// Shortest span on route.
TEST(FMESolverTest, RoutingTest) {
  FourierSolver solver;
  using ColIndex = FourierSolver::ColIndex;
  // Start + duration == end.
  const ColIndex start = solver.AddVariable(0, 3);
  const ColIndex duration = solver.AddVariable(3, 10);
  const ColIndex end = solver.AddVariable(7, 10);
  solver.AddConstraint(0, 0, {{1, start}, {1, duration}, {-1, end}});
  // Duration cost.
  solver.SetObjectiveCoefficient(duration, 3);

  auto add_soft_max = [&](double coef, ColIndex var, double soft_max,
                          double cost) {
    constexpr double kInfinity = std::numeric_limits<double>::infinity();
    ColIndex violation = solver.AddVariable(0, kInfinity);
    solver.AddConstraint(-kInfinity, soft_max, {{coef, var}, {-1, violation}});
    solver.SetObjectiveCoefficient(violation, cost);
  };
  add_soft_max(1, duration, 5, 7);  // duration soft max.
  add_soft_max(-1, start, -1, 11);  // start soft min is 1.
  add_soft_max(1, start, 2, 13);    // start soft max is 2.
  add_soft_max(-1, end, -8, 17);    // end soft min is 8.
  add_soft_max(1, end, 9, 19);      // end soft max is 9.

  EXPECT_TRUE(solver.Solve());
  EXPECT_EQ(solver.EvaluateObjective(), (8 - 2) * 3 + (8 - 2 - 5) * 7);
}

// Shortest span on route.
TEST(FMESolverTest, RoutingTest2) {
  FourierSolver solver;
  using ColIndex = FourierSolver::ColIndex;
  // Start + duration == end.
  const ColIndex start = solver.AddVariable(0, 3);
  const ColIndex end = solver.AddVariable(7, 10);
  solver.AddConstraint(3, 10, {{1, end}, {-1, start}});
  // Duration cost.
  solver.SetObjectiveCoefficient(end, 3);
  solver.SetObjectiveCoefficient(start, -3);

  constexpr double kInfinity = std::numeric_limits<double>::infinity();
  {
    // Soft duration max.
    ColIndex violation = solver.AddVariable(0, kInfinity);
    solver.AddConstraint(-kInfinity, 5,
                         {{1, end}, {-1, start}, {-1, violation}});
    solver.SetObjectiveCoefficient(violation, 7);
  }

  auto add_soft_max = [&](double coef, ColIndex var, double soft_max,
                          double cost) {
    ColIndex violation = solver.AddVariable(0, kInfinity);
    solver.AddConstraint(-kInfinity, soft_max, {{coef, var}, {-1, violation}});
    solver.SetObjectiveCoefficient(violation, cost);
  };
  add_soft_max(-1, start, -1, 11);  // start soft min is 1.
  add_soft_max(1, start, 2, 13);    // start soft max is 2.
  add_soft_max(-1, end, -8, 17);    // end soft min is 8.
  add_soft_max(1, end, 9, 19);      // end soft max is 9.

  EXPECT_TRUE(solver.Solve());
  EXPECT_EQ(solver.EvaluateObjective(), (8 - 2) * 3 + (8 - 2 - 5) * 7);
}

TEST(FMESolverTest, RoutingTestSymbolic) {
  FourierSolver solver;
  using ColIndex = FourierSolver::ColIndex;
  constexpr double kInfinity = FourierSolver::kInfinity;
  // Start + duration == end.
  const ColIndex start = solver.AddVariable(0, 3);
  const ColIndex end = solver.AddVariable(7, 10);
  const ColIndex duration = solver.AddVariable(0, 10);
  solver.AddConstraint(0, 0, {{1, start}, {1, duration}, {-1, end}});

  // Duration cost.
  solver.SetObjectiveCoefficient(duration, 3);

  const ColIndex duration_lb = solver.AddVariable(0, 10, /*is_symbolic=*/true);
  solver.AddConstraint(0, kInfinity, {{1, duration}, {-1, duration_lb}});
  {
    ColIndex violation = solver.AddVariable(0, kInfinity);
    solver.AddConstraint(-kInfinity, 5, {{1, duration}, {-1, violation}});
    solver.SetObjectiveCoefficient(violation, 7);
  };

  EXPECT_TRUE(solver.Solve());
  {
    solver.SetSymbolicVariableValue(duration_lb, 3);
    EXPECT_EQ(solver.EvaluateObjective(), 12);
  }
  {
    solver.SetSymbolicVariableValue(duration_lb, 4);
    EXPECT_EQ(solver.EvaluateObjective(), 12);
  }
  {
    solver.SetSymbolicVariableValue(duration_lb, 5);
    EXPECT_EQ(solver.EvaluateObjective(), 15);
  }
  {
    solver.SetSymbolicVariableValue(duration_lb, 6);
    EXPECT_EQ(solver.EvaluateObjective(), 18 + 7);
  }
}

TEST(FMESolverTest, RoutingTestSymbolic2) {
  FourierSolver solver;
  using ColIndex = FourierSolver::ColIndex;
  constexpr double kInfinity = std::numeric_limits<double>::infinity();
  // Start + duration == end.
  // const ColIndex start = solver.AddVariable(0, 3);
  // const ColIndex end = solver.AddVariable(7, 10);
  // const ColIndex duration = solver.AddVariable(-kInfinity, 10);
  const ColIndex start = solver.AddVariable(-kInfinity, kInfinity);
  const ColIndex end = solver.AddVariable(-kInfinity, kInfinity);
  const ColIndex duration = solver.AddVariable(-kInfinity, kInfinity);
  solver.AddConstraint(0, 0, {{1, start}, {1, duration}, {-1, end}});
  // Duration cost.
  solver.SetObjectiveCoefficient(duration, 3);

  auto add_soft_max = [&](double coef, ColIndex var, double soft_max,
                          double cost) {
    ColIndex violation = solver.AddVariable(0, kInfinity);
    solver.AddConstraint(-kInfinity, soft_max, {{coef, var}, {-1, violation}});
    solver.SetObjectiveCoefficient(violation, cost);
  };
  add_soft_max(1, duration, 5, 7);  // duration soft max.
  add_soft_max(-1, start, -1, 9);   // start soft min is 1.
  add_soft_max(1, start, 2, 11);    // start soft max is 2.
  add_soft_max(-1, end, -8, 13);    // end soft min is 8.
  add_soft_max(1, end, 9, 17);      // end soft max is 9.

  const ColIndex duration_min = solver.AddVariable(0, 10, true);
  solver.AddConstraint(0, kInfinity, {{1, duration}, {-1, duration_min}});
  const ColIndex start_min = solver.AddVariable(0, 3, true);
  solver.AddConstraint(0, kInfinity, {{1, start}, {-1, start_min}});
  const ColIndex end_min = solver.AddVariable(7, 10, true);
  solver.AddConstraint(0, kInfinity, {{1, end}, {-1, end_min}});
  const ColIndex start_max = solver.AddVariable(0, 3, true);
  solver.AddConstraint(0, kInfinity, {{1, start_max}, {-1, start}});
  const ColIndex end_max = solver.AddVariable(7, 10, true);
  solver.AddConstraint(0, kInfinity, {{1, end_max}, {-1, end}});

  EXPECT_TRUE(solver.Solve());
  {
    solver.SetSymbolicVariableValue(duration_min, 3);
    solver.SetSymbolicVariableValue(start_min, 0);
    solver.SetSymbolicVariableValue(start_max, 3);
    solver.SetSymbolicVariableValue(end_min, 7);
    solver.SetSymbolicVariableValue(end_max, 10);
    EXPECT_EQ(solver.EvaluateObjective(), (8 - 2) * 3 + (8 - 2 - 5) * 7);
  }
}

// Assignment
TEST(FMESolverTest, AssignmentTests) {
  // Encode assignment problems:
  // - sum_w x_jw = 1  for all j (all jobs must have one worker)
  // - sum_j x_jw <= 1  for all w (a worker is assigned at most one job)
  // - x_jw in {0, 1}
  struct Scenario {
    int num_jobs;
    int num_workers;
    struct Arc {
      int job;
      int worker;
      double cost;

      auto AsTuple() const { return std::make_tuple(job, worker, cost); }
    };
    std::vector<Arc> arcs;
    bool feasible;
    double objective;
  };
  static_assert(TupleConvertible<Scenario::Arc>);
  std::vector<Scenario> scenarios = {
      {.num_jobs = 3,
       .num_workers = 4,
       .arcs = {{.job = 0, .worker = 0, .cost = 1.0},
                {.job = 0, .worker = 1, .cost = 1.0},
                {.job = 0, .worker = 2, .cost = 1.0},
                {.job = 1, .worker = 1, .cost = 1.0},
                {.job = 1, .worker = 2, .cost = 1.0},
                {.job = 2, .worker = 1, .cost = 1.0},
                {.job = 2, .worker = 2, .cost = 1.0},
                {.job = 2, .worker = 3, .cost = 1.0}},
       .feasible = true,
       .objective = 3.0},
      {.num_jobs = 3,
       .num_workers = 4,
       .arcs = {{.job = 0, .worker = 1, .cost = 1.0},
                {.job = 0, .worker = 2, .cost = 1.0},
                {.job = 1, .worker = 1, .cost = 1.0},
                {.job = 1, .worker = 2, .cost = 1.0},
                {.job = 2, .worker = 1, .cost = 1.0},
                {.job = 2, .worker = 2, .cost = 1.0}},
       .feasible = false,
       .objective = -1.0},
  };

  for (int s = 0; s < scenarios.size(); ++s) {
    SCOPED_TRACE(absl::StrCat("Scenario ", s));
    const Scenario& scenario = scenarios[s];
    FourierSolver solver;
    using ColIndex = FourierSolver::ColIndex;
    absl::flat_hash_map<Scenario::Arc, ColIndex> variable_of_arc;
    using CoefVar = FourierSolver::CoefficientVariable;
    absl::flat_hash_map<int, std::vector<CoefVar>> linexpr_of_worker;
    absl::flat_hash_map<int, std::vector<CoefVar>> linexpr_of_job;
    for (const Scenario::Arc& arc : scenario.arcs) {
      const ColIndex var = solver.AddVariable(0, 1);
      solver.SetObjectiveCoefficient(var, arc.cost);
      variable_of_arc[arc] = var;
      linexpr_of_job[arc.job].push_back({1, var});
      linexpr_of_worker[arc.worker].push_back({1, var});
    }
    for (int j = 0; j < scenario.num_jobs; ++j) {
      solver.AddConstraint(1, 1, linexpr_of_job[j]);
    }
    for (int w = 0; w < scenario.num_workers; ++w) {
      solver.AddConstraint(0, 1, linexpr_of_worker[w]);
    }
    EXPECT_EQ(solver.Solve(), scenario.feasible);
    if (scenario.feasible) {
      EXPECT_EQ(solver.EvaluateObjective(), scenario.objective);
    }
  }
}

}  // namespace operations_research::routing

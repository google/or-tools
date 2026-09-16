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

#include "ortools/sat/circuit.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <numeric>
#include <utility>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/graph_base/strongly_connected_components.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {
namespace {

std::function<void(Model*)> DenseCircuitConstraint(
    int num_nodes, bool allow_subcircuit,
    bool allow_multiple_subcircuit_through_zero) {
  return [=](Model* model) {
    std::vector<int> tails;
    std::vector<int> heads;
    std::vector<Literal> literals;
    for (int tail = 0; tail < num_nodes; ++tail) {
      for (int head = 0; head < num_nodes; ++head) {
        if (!allow_subcircuit && tail == head) continue;
        tails.push_back(tail);
        heads.push_back(head);
        literals.push_back(Literal(model->Add(NewBooleanVariable()), true));
      }
    }
    LoadSubcircuitConstraint(num_nodes, tails, heads,
                             /*enforcement_literals=*/{}, literals, model,
                             allow_multiple_subcircuit_through_zero);
  };
}

int CountSolutions(Model* model) {
  int num_solutions = 0;
  while (true) {
    const SatSolver::Status status = SolveIntegerProblemWithLazyEncoding(model);
    if (status != SatSolver::Status::FEASIBLE) break;

    // Add the solution.
    ++num_solutions;

    // Loop to the next solution.
    model->Add(ExcludeCurrentSolutionAndBacktrack());
  }
  return num_solutions;
}

int Factorial(int n) { return n ? n * Factorial(n - 1) : 1; }

TEST(ReindexArcTest, BasicCase) {
  const int num_nodes = 1000;
  std::vector<int> tails(num_nodes);
  std::vector<int> heads(num_nodes);
  for (int i = 0; i < num_nodes; ++i) {
    tails[i] = 100 * i;
    heads[i] = 100 * i;
  }
  ReindexArcs(&tails, &heads);
  for (int i = 0; i < num_nodes; ++i) {
    EXPECT_EQ(i, tails[i]);
    EXPECT_EQ(i, heads[i]);
  }
}

TEST(ReindexArcTest, NegativeNumbering) {
  const int num_nodes = 1000;
  std::vector<int> tails(num_nodes);
  std::vector<int> heads(num_nodes);
  for (int i = 0; i < num_nodes; ++i) {
    tails[i] = -100 * i;
    heads[i] = -100 * i;
  }
  ReindexArcs(&tails, &heads);
  for (int i = 0; i < num_nodes; ++i) {
    EXPECT_EQ(i, tails[num_nodes - 1 - i]);
    EXPECT_EQ(i, heads[num_nodes - 1 - i]);
  }
}

TEST(CircuitConstraintTest, NodeWithNoArcsIsUnsat) {
  static const int kNumNodes = 2;
  Model model;
  std::vector<int> tails;
  std::vector<int> heads;
  std::vector<Literal> literals;
  tails.push_back(0);
  heads.push_back(1);
  literals.push_back(Literal(model.Add(NewBooleanVariable()), true));
  LoadSubcircuitConstraint(kNumNodes, tails, heads, /*enforcement_literals=*/{},
                           literals, &model);
  EXPECT_TRUE(model.GetOrCreate<SatSolver>()->ModelIsUnsat());
}

TEST(CircuitConstraintTest, AllCircuits) {
  static const int kNumNodes = 4;
  Model model;
  model.Add(
      DenseCircuitConstraint(kNumNodes, /*allow_subcircuit=*/false,
                             /*allow_multiple_subcircuit_through_zero=*/false));

  const int num_solutions = CountSolutions(&model);
  EXPECT_EQ(num_solutions, Factorial(kNumNodes - 1));
}

TEST(CircuitConstraintTest, AllSubCircuits) {
  static const int kNumNodes = 4;

  Model model;
  model.Add(
      DenseCircuitConstraint(kNumNodes, /*allow_subcircuit=*/true,
                             /*allow_multiple_subcircuit_through_zero=*/false));

  const int num_solutions = CountSolutions(&model);
  int expected = 1;  // No circuit at all.
  for (int circuit_size = 2; circuit_size <= kNumNodes; ++circuit_size) {
    // The number of circuits of a given size is:
    //   - n for the first element
    //   - times (n-1) for the second
    //   - ...
    //   - times (n - (circuit_size - 1)) for the last.
    // That is n! / (n - circuit_size)!, and like this we count circuit_size
    // times the same circuit, so we have to divide by circuit_size in the end.
    expected += Factorial(kNumNodes) /
                (circuit_size * Factorial(kNumNodes - circuit_size));
  }
  EXPECT_EQ(num_solutions, expected);
}

TEST(CircuitConstraintTest, AllVehiculeRoutes) {
  static const int kNumNodes = 4;
  Model model;

  model.Add(
      DenseCircuitConstraint(kNumNodes, /*allow_subcircuit=*/false,
                             /*allow_multiple_subcircuit_through_zero=*/true));

  const int num_solutions = CountSolutions(&model);
  int expected = 1;   // 3 outgoing arcs from zero.
  expected += 2 * 3;  // 2 outgoing arcs from zero. 3 pairs, 2 directions.
  expected += 6;      // full circuit.
  EXPECT_EQ(num_solutions, expected);
}

TEST(CircuitConstraintTest, AllCircuitCoverings) {
  // This test counts the number of circuit coverings of the clique on
  // num_nodes with num_distinguished distinguished nodes, i.e. graphs that are
  // vertex-disjoint circuits where every circuit must contain exactly one
  // distinguished node.
  //
  // When writing n the number of nodes and k the number of distinguished nodes,
  // and the number of such coverings T(n, k), we have:
  // T(n,1) = (n-1)!, T(k,k) = 1, T(n,k) = (n-1)!/(k-1)! for n >= k >= 1.
  // Indeed, we can enumerate canonical representations, e.g. [1]64[2]35,
  // by starting with [1][2]...[k], and place every node in turn at its final
  // place w.r.t. existing neighbours. To generate the above example, we go
  // through [1][2], [1][2]3, [1]4[2]3, [1]4[2]35, [1]64[2]35.
  // At the first iteration, there are k choices, then k+1 ... n-1.
  for (int num_nodes = 1; num_nodes <= 6; num_nodes++) {
    for (int num_distinguished = 1; num_distinguished <= num_nodes;
         num_distinguished++) {
      Model model;
      std::vector<int> distinguished(num_distinguished);
      std::iota(distinguished.begin(), distinguished.end(), 0);
      std::vector<std::vector<Literal>> graph(num_nodes);
      std::vector<Literal> arcs;
      for (int i = 0; i < num_nodes; i++) {
        graph[i].resize(num_nodes);
        for (int j = 0; j < num_nodes; j++) {
          const auto var = model.Add(NewBooleanVariable());
          graph[i][j] = Literal(var, true);
          arcs.emplace_back(graph[i][j]);
        }
        if (i >= num_distinguished) {
          AddClauseConstraint({graph[i][i].Negated()}, &model);
        }
      }
      AddExactlyOnePerRowAndPerColumn(graph, &model);
      AddCircuitCovering(graph, distinguished, &model);
      const int64_t num_solutions = CountSolutions(&model);
      EXPECT_EQ(num_solutions * Factorial(num_distinguished - 1),
                Factorial(num_nodes - 1));
    }
  }
}

TEST(CircuitConstraintTest, InfeasibleBecauseOfMissingArcs) {
  Model model;
  std::vector<int> tails;
  std::vector<int> heads;
  std::vector<Literal> literals;
  for (const auto arcs :
       std::vector<std::pair<int, int>>{{0, 1}, {1, 1}, {0, 2}, {2, 2}}) {
    tails.push_back(arcs.first);
    heads.push_back(arcs.second);
    literals.push_back(Literal(model.Add(NewBooleanVariable()), true));
  }
  LoadSubcircuitConstraint(3, tails, heads, /*enforcement_literals=*/{},
                           literals, &model, false);
  const SatSolver::Status status = SolveIntegerProblemWithLazyEncoding(&model);
  EXPECT_EQ(status, SatSolver::Status::INFEASIBLE);
}

// The graph looks like this with a self-loop at 2. If 2 is not selected
// (self-loop) then there is one solution (0,1,3,0) and (0,3,5,0). Otherwise,
// there are 2 more solutions with 2 inserted in one of the two routes.
//
//   0  ---> 1 ---> 4 -------------
//   |       |      ^             |
//   |       -----> 2* --> 5 ---> 0
//   |              ^      ^
//   |              |      |
//   -------------> 3 ------
//
TEST(CircuitConstraintTest, RouteConstraint) {
  Model model;
  std::vector<int> tails;
  std::vector<int> heads;
  std::vector<Literal> literals;
  for (const auto arcs : std::vector<std::pair<int, int>>{{0, 1},
                                                          {0, 3},
                                                          {1, 2},
                                                          {1, 4},
                                                          {2, 2},
                                                          {2, 4},
                                                          {2, 5},
                                                          {3, 2},
                                                          {3, 5},
                                                          {4, 0},
                                                          {5, 0}}) {
    tails.push_back(arcs.first);
    heads.push_back(arcs.second);
    literals.push_back(Literal(model.Add(NewBooleanVariable()), true));
  }
  LoadSubcircuitConstraint(6, tails, heads, /*enforcement_literals=*/{},
                           literals, &model, true);
  const int64_t num_solutions = CountSolutions(&model);
  EXPECT_EQ(num_solutions, 3);
}

TEST(NoCyclePropagatorTest, CountAllSolutions) {
  // We create a 2 * 2 grid with diagonal arcs.
  Model model;
  int num_nodes = 0;
  const int num_x = 2;
  const int num_y = 2;
  const auto get_index = [&num_nodes](int x, int y) {
    const int index = x * num_y + y;
    num_nodes = std::max(num_nodes, index + 1);
    return index;
  };

  std::vector<int> tails;
  std::vector<int> heads;
  std::vector<Literal> literals;
  for (int x = 0; x < num_x; ++x) {
    for (int y = 0; y < num_y; ++y) {
      for (const int x_dir : {-1, 0, 1}) {
        for (const int y_dir : {-1, 0, 1}) {
          const int head_x = x + x_dir;
          const int head_y = y + y_dir;
          if (x_dir == 0 && y_dir == 0) continue;
          if (head_x < 0 || head_x >= num_x) continue;
          if (head_y < 0 || head_y >= num_y) continue;
          tails.push_back(get_index(x, y));
          heads.push_back(get_index(head_x, head_y));
          literals.push_back(Literal(model.Add(NewBooleanVariable()), true));
        }
      }
    }
  }
  model.TakeOwnership(
      new NoCyclePropagator(num_nodes, tails, heads, literals, &model));

  // Graph is small enough.
  CHECK_EQ(num_nodes, 4);
  CHECK_EQ(tails.size(), 12);

  // Counts solutions with brute-force algo.
  int num_expected_solutions = 0;
  std::vector<std::vector<int>> subgraph(num_nodes);
  std::vector<std::vector<int>> components;
  const int num_cases = 1 << tails.size();
  for (int mask = 0; mask < num_cases; ++mask) {
    for (int n = 0; n < num_nodes; ++n) {
      subgraph[n].clear();
    }
    for (int a = 0; a < tails.size(); ++a) {
      if ((1 << a) & mask) {
        subgraph[tails[a]].push_back(heads[a]);
      }
    }
    components.clear();
    FindStronglyConnectedComponents(num_nodes, subgraph, &components);
    bool has_cycle = false;
    for (const std::vector<int> compo : components) {
      if (compo.size() > 1) {
        has_cycle = true;
        break;
      }
    }
    if (!has_cycle) ++num_expected_solutions;
  }
  EXPECT_EQ(num_expected_solutions, 543);

  // There are 12 arcs.
  // So out of 2^12 solutions, we have to exclude all the ones with cycles.
  EXPECT_EQ(CountSolutions(&model), 543);
}

CpSolverResponse SolveAndCheck(
    const CpModelProto& initial_model,
    absl::btree_set<std::vector<int>>* solutions = nullptr) {
  SatParameters params;
  params.set_use_sat_inprocessing(false);
  params.set_cp_model_presolve(false);
  params.set_enumerate_all_solutions(true);
  auto observer = [&](const CpSolverResponse& response) {
    if (solutions != nullptr) {
      solutions->insert(std::vector<int>(response.solution().begin(),
                                         response.solution().end()));
    }
  };
  Model model;
  model.Add(NewSatParameters(params));
  model.Add(NewFeasibleSolutionObserver(observer));
  return SolveCpModel(initial_model, &model);
}

TEST(CircuitConstraintTest, RandomCircuitConstraint) {
  absl::BitGen random;
  for (int iter = 0; iter < 5; ++iter) {
    CpModelProto model;
    for (int i = 0; i < 16; ++i) {
      IntegerVariableProto* var = model.add_variables();
      var->add_domain(0);
      var->add_domain(1);
    }
    const auto random_literal = [&]() {
      const int var = absl::Uniform(random, 0, 16);
      return absl::Bernoulli(random, 0.5) ? var : NegatedRef(var);
    };

    ConstraintProto* ct = model.add_constraints();
    ct->add_enforcement_literal(random_literal());
    ct->add_enforcement_literal(random_literal());

    CircuitConstraintProto* circuit = ct->mutable_circuit();
    for (int tail = 0; tail < 4; ++tail) {
      for (int head = 0; head < 4; ++head) {
        if (tail == head && absl::Bernoulli(random, 0.5)) {
          continue;
        }
        circuit->add_tails(tail);
        circuit->add_heads(head);
        circuit->add_literals(random_literal());
      }
    }

    absl::btree_set<std::vector<int>> solutions;
    SolveAndCheck(model, &solutions);

    absl::btree_set<std::vector<int>> expected_solutions;
    std::vector<int64_t> assignment(16);
    for (int mask = 0; mask < (1 << 16); ++mask) {
      for (int i = 0; i < 16; ++i) {
        assignment[i] = (mask >> i) & 1;
      }
      if (SolutionIsFeasible(model, assignment)) {
        expected_solutions.insert(
            std::vector<int>(assignment.begin(), assignment.end()));
      }
    }
    EXPECT_EQ(solutions, expected_solutions) << model.DebugString();
  }
}

}  // namespace
}  // namespace sat
}  // namespace operations_research

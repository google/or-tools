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

#include "ortools/routing/insertion_lns.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/routing.h"
#include "ortools/routing/search.h"
#include "ortools/routing/types.h"

namespace operations_research::routing {
namespace {

using VisitTypePolicy = Model::VisitTypePolicy;

TEST(HeuristicPathLNSOperatorTest, TestName) {
  const IndexManager manager(5, 2, NodeIndex(0));
  Model model(manager);
  model.CloseModel();
  LocalCheapestInsertionParameters lci_params;
  lci_params.set_pickup_delivery_strategy(
      LocalCheapestInsertionParameters::BEST_PICKUP_THEN_BEST_DELIVERY);
  FilteredHeuristicPathLNSOperator ls_operator(
      std::make_unique<LocalCheapestInsertionFilteredHeuristic>(
          &model, []() { return false; },
          [](int64_t, int64_t, int64_t) { return 1; }, lci_params, nullptr,
          /*use_first_solution_hint=*/false));
  EXPECT_EQ(ls_operator.DebugString(),
            "HeuristicPathLNS(LocalCheapestInsertion)");
}

std::function<int64_t(int64_t, int64_t)> ArcCost2(const Model* model) {
  return [model](int64_t from, int64_t to) {
    if (model->vehicles() > 1 && from == model->Start(1) &&
        to != model->End(1)) {
      // Second vehicle is more expensive when used.
      return 1000;
    }
    return 1;
  };
}

std::vector<std::unique_ptr<RoutingFilteredHeuristic>> GetRoutingHeuristics(
    Model* model, double gci_seeds_ratio = 0.5, bool insertion_only = false) {
  std::vector<std::unique_ptr<RoutingFilteredHeuristic>> heuristics;

  const auto arc_cost2 = ArcCost2(model);
  model->SetArcCostEvaluatorOfAllVehicles(
      model->RegisterTransitCallback(arc_cost2));
  if (model->vehicles() > 1) {
    model->SetFixedCostOfVehicle(1000, 1);
  }
  model->CloseModel();
  const auto arc_cost3 = [arc_cost2](int64_t from, int64_t to,
                                     int64_t /*vehicle*/) {
    return arc_cost2(from, to);
  };
  for (const LocalCheapestInsertionParameters::PairInsertionStrategy strategy :
       {LocalCheapestInsertionParameters::AUTOMATIC,
        LocalCheapestInsertionParameters::BEST_PICKUP_THEN_BEST_DELIVERY,
        LocalCheapestInsertionParameters::BEST_PICKUP_DELIVERY_PAIR,
        LocalCheapestInsertionParameters::
            BEST_PICKUP_DELIVERY_PAIR_MULTITOUR}) {
    LocalCheapestInsertionParameters lci_params;
    lci_params.set_pickup_delivery_strategy(strategy);
    heuristics.push_back(
        {std::make_unique<LocalCheapestInsertionFilteredHeuristic>(
            model, []() { return false; }, arc_cost3, lci_params, nullptr,
            /*use_first_solution_hint=*/false)});
  }

  GlobalCheapestInsertionParameters gci_params;
  gci_params.set_farthest_seeds_ratio(0.0);
  gci_params.set_neighbors_ratio(1.0);
  gci_params.set_min_neighbors(1);
  gci_params.set_use_neighbors_ratio_for_initialization(true);

  // Parallel GCI no seed.
  heuristics.push_back(
      std::make_unique<GlobalCheapestInsertionFilteredHeuristic>(
          model, []() { return false; }, arc_cost3, nullptr, nullptr,
          gci_params, /*is_sequential=*/false));
  // Parallel GCI with seed.
  gci_params.set_farthest_seeds_ratio(gci_seeds_ratio);
  heuristics.push_back(
      std::make_unique<GlobalCheapestInsertionFilteredHeuristic>(
          model, []() { return false; }, arc_cost3, nullptr, nullptr,
          gci_params, /*is_sequential=*/false));
  // Sequential GCI.
  heuristics.push_back(
      std::make_unique<GlobalCheapestInsertionFilteredHeuristic>(
          model, []() { return false; }, arc_cost3, nullptr, nullptr,
          gci_params, /*is_sequential=*/true));

  if (insertion_only) return heuristics;

  const SavingsParameters savings_params =
      DefaultRoutingSearchParameters().savings_parameters();
  heuristics.push_back(std::make_unique<ParallelSavingsFilteredHeuristic>(
      model, []() { return false; }, savings_params, nullptr));
  heuristics.push_back(std::make_unique<SequentialSavingsFilteredHeuristic>(
      model, []() { return false; }, savings_params, nullptr));

  return heuristics;
}

bool MakesSingleNodePerformedInEmptyPath(absl::string_view heuristic_lns_name) {
  for (const std::string& heuristic_sub_name : {"Savings"}) {
    if (absl::StrContains(heuristic_lns_name, heuristic_sub_name)) {
      return true;
    }
  }
  return false;
}

const Assignment* CreateAssignment(absl::Span<const int> solution,
                                   Model* model) {
  EXPECT_EQ(solution.size(), model->Size());
  Assignment* assignment = model->solver()->MakeAssignment();
  for (int i = 0; i < solution.size(); ++i) {
    assignment->Add(model->NextVar(i))->SetValue(solution[i]);
  }
  return assignment;
}

void OneEmptyPathTestForAllHeuristics(
    std::function<LocalSearchOperator*(
        Model* model, std::unique_ptr<RoutingFilteredHeuristic>)>
        create_heuristic_operator,
    bool expect_node_inserted) {
  const int kNumNodes = 2;
  const int kNumVehicles = 1;

  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  // Route empty, node unperformed.
  const Assignment* const assignment = CreateAssignment({2, 1}, &model);

  Solver* const solver = model.solver();
  for (std::unique_ptr<RoutingFilteredHeuristic>& heuristic :
       GetRoutingHeuristics(&model)) {
    LocalSearchOperator* const heuristic_lns =
        create_heuristic_operator(&model, std::move(heuristic));

    heuristic_lns->Start(assignment);
    Assignment delta(solver);
    Assignment deltadelta(solver);
    // Some operators will insert the unperformed node on the route.
    ASSERT_EQ(heuristic_lns->MakeNextNeighbor(&delta, &deltadelta),
              expect_node_inserted)
        << heuristic_lns->DebugString();
    if (expect_node_inserted) {
      EXPECT_EQ(delta.Value(model.NextVar(0)), 1);
      EXPECT_FALSE(heuristic_lns->MakeNextNeighbor(&delta, &deltadelta));
    }
  }
}

TEST(HeuristicOperatorTest, OneEmptyPath_PathLNS) {
  OneEmptyPathTestForAllHeuristics(
      [](Model* model, std::unique_ptr<RoutingFilteredHeuristic> heuristic) {
        return model->solver()->RevAlloc(
            new FilteredHeuristicPathLNSOperator(std::move(heuristic)));
      },
      /*expect_node_inserted=*/false);
}

TEST(HeuristicOperatorTest, OneEmptyPath_RelocatePathAndInsertUnperformed) {
  OneEmptyPathTestForAllHeuristics(
      [](Model* model, std::unique_ptr<RoutingFilteredHeuristic> heuristic) {
        return model->solver()->RevAlloc(
            new RelocatePathAndHeuristicInsertUnperformedOperator(
                std::move(heuristic)));
      },
      /*expect_node_inserted=*/false);
}

TEST(HeuristicOperatorTest, OneEmptyPath_ExpensiveChainLNS) {
  OneEmptyPathTestForAllHeuristics(
      [](Model* model, std::unique_ptr<RoutingFilteredHeuristic> heuristic) {
        return model->solver()->RevAlloc(
            new FilteredHeuristicExpensiveChainLNSOperator(
                std::move(heuristic), 2,
                [](int64_t, int64_t, int64_t) { return 10; }));
      },
      /*expect_node_inserted=*/false);
}

TEST(HeuristicOperatorTest, OneEmptyPath_CloseNodesLNS) {
  const int kNumCloseNodes = 50;
  OneEmptyPathTestForAllHeuristics(
      [](Model* model, std::unique_ptr<RoutingFilteredHeuristic> heuristic) {
        return model->solver()->RevAlloc(
            new FilteredHeuristicCloseNodesLNSOperator(std::move(heuristic),
                                                       kNumCloseNodes));
      },
      /*expect_node_inserted=*/true);
}

void TwoEmptyPathsTestForAllHeuristics(
    std::function<LocalSearchOperator*(
        Model* model, std::unique_ptr<RoutingFilteredHeuristic>)>
        create_heuristic_operator,
    bool expect_node_inserted) {
  const int kNumNodes = 2;
  const int kNumVehicles = 2;

  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  // Routes empty, node unperformed.
  const Assignment* const assignment = CreateAssignment({3, 1, 4}, &model);

  Solver* const solver = model.solver();
  for (std::unique_ptr<RoutingFilteredHeuristic>& heuristic :
       GetRoutingHeuristics(&model)) {
    LocalSearchOperator* const heuristic_lns =
        create_heuristic_operator(&model, std::move(heuristic));

    heuristic_lns->Start(assignment);
    Assignment delta(solver);
    Assignment deltadelta(solver);
    // Some operators will insert the unperformed node on the first route
    // (less expensive than the second, see ArcCost2()).
    ASSERT_EQ(heuristic_lns->MakeNextNeighbor(&delta, &deltadelta),
              expect_node_inserted)
        << heuristic_lns->DebugString();
    if (expect_node_inserted) {
      EXPECT_EQ(delta.Value(model.NextVar(0)), 1);
      EXPECT_FALSE(heuristic_lns->MakeNextNeighbor(&delta, &deltadelta));
    }
  }
}

TEST(HeuristicOperatorTest, TwoEmptyPaths_PathLNS) {
  TwoEmptyPathsTestForAllHeuristics(
      [](Model* model, std::unique_ptr<RoutingFilteredHeuristic> heuristic) {
        return model->solver()->RevAlloc(
            new FilteredHeuristicPathLNSOperator(std::move(heuristic)));
      },
      /*expect_node_inserted=*/false);
}

TEST(HeuristicOperatorTest, TwoEmptyPaths_RelocatePathAndInsertUnperformed) {
  TwoEmptyPathsTestForAllHeuristics(
      [](Model* model, std::unique_ptr<RoutingFilteredHeuristic> heuristic) {
        return model->solver()->RevAlloc(
            new RelocatePathAndHeuristicInsertUnperformedOperator(
                std::move(heuristic)));
      },
      /*expect_node_inserted=*/false);
}

TEST(HeuristicOperatorTest, TwoEmptyPaths_ExpensiveChainLNS) {
  TwoEmptyPathsTestForAllHeuristics(
      [](Model* model, std::unique_ptr<RoutingFilteredHeuristic> heuristic) {
        return model->solver()->RevAlloc(
            new FilteredHeuristicExpensiveChainLNSOperator(
                std::move(heuristic), 2,
                [](int64_t, int64_t, int64_t) { return 10; }));
      },
      /*expect_node_inserted=*/false);
}

TEST(HeuristicOperatorTest, TwoEmptyPaths_CloseNodesLNS) {
  const int kNumCloseNodes = 50;
  TwoEmptyPathsTestForAllHeuristics(
      [](Model* model, std::unique_ptr<RoutingFilteredHeuristic> heuristic) {
        return model->solver()->RevAlloc(
            new FilteredHeuristicCloseNodesLNSOperator(std::move(heuristic),
                                                       kNumCloseNodes));
      },
      /*expect_node_inserted=*/true);
}

void FirstPathEmptyTestForAllHeuristics(
    std::function<LocalSearchOperator*(
        Model* model, std::unique_ptr<RoutingFilteredHeuristic>)>
        create_heuristic_operator,
    bool expect_neighbor) {
  const int kNumNodes = 2;
  const int kNumVehicles = 2;

  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  // First route empty, node performed by second vehicle.
  const Assignment* const assignment = CreateAssignment({3, 4, 1}, &model);

  Solver* const solver = model.solver();
  for (std::unique_ptr<RoutingFilteredHeuristic>& heuristic :
       GetRoutingHeuristics(&model)) {
    LocalSearchOperator* const heuristic_lns =
        create_heuristic_operator(&model, std::move(heuristic));

    heuristic_lns->Start(assignment);
    Assignment delta(solver);
    Assignment deltadelta(solver);
    ASSERT_EQ(heuristic_lns->MakeNextNeighbor(&delta, &deltadelta),
              expect_neighbor)
        << heuristic_lns->DebugString();
    if (expect_neighbor) {
      // The node should be moved to the first route.
      EXPECT_EQ(delta.Value(model.NextVar(0)), 1);
      EXPECT_FALSE(heuristic_lns->MakeNextNeighbor(&delta, &deltadelta))
          << heuristic_lns->DebugString()
          << ", delta = " << delta.DebugString();
    }
  }
}

TEST(HeuristicOperatorTest, FirstPathEmpty_PathLNS) {
  // Since there's a single non-empty route, the operator will not create a
  // neighbor.
  FirstPathEmptyTestForAllHeuristics(
      [](Model* model, std::unique_ptr<RoutingFilteredHeuristic> heuristic) {
        return model->solver()->RevAlloc(
            new FilteredHeuristicPathLNSOperator(std::move(heuristic)));
      },
      /*expect_neighbor=*/false);
}

TEST(HeuristicOperatorTest, FirstPathEmpty_ExpensiveChainLNS) {
  FirstPathEmptyTestForAllHeuristics(
      [](Model* model, std::unique_ptr<RoutingFilteredHeuristic> heuristic) {
        const auto arc_cost = ArcCost2(model);
        return model->solver()->RevAlloc(
            new FilteredHeuristicExpensiveChainLNSOperator(
                std::move(heuristic), 2,
                [arc_cost](int64_t i, int64_t j, int64_t /*vehicle*/) {
                  return arc_cost(i, j);
                }));
      },
      /*expect_neighbor=*/true);
}

TEST(HeuristicOperatorTest, FirstPathEmpty_CloseNodesLNS) {
  const int kNumCloseNodes = 50;
  FirstPathEmptyTestForAllHeuristics(
      [](Model* model, std::unique_ptr<RoutingFilteredHeuristic> heuristic) {
        return model->solver()->RevAlloc(
            new FilteredHeuristicCloseNodesLNSOperator(std::move(heuristic),
                                                       kNumCloseNodes));
      },
      /*expect_neighbor=*/true);
}

void SecondPathEmptyTestForAllHeuristics(
    std::function<LocalSearchOperator*(
        Model* model, std::unique_ptr<RoutingFilteredHeuristic>)>
        create_heuristic_operator) {
  const int kNumNodes = 2;
  const int kNumVehicles = 2;

  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  // Second route empty, node performed by first vehicle.
  const Assignment* const assignment = CreateAssignment({1, 3, 4}, &model);

  Solver* const solver = model.solver();
  for (std::unique_ptr<RoutingFilteredHeuristic>& heuristic :
       GetRoutingHeuristics(&model)) {
    LocalSearchOperator* const heuristic_lns =
        create_heuristic_operator(&model, std::move(heuristic));

    heuristic_lns->Start(assignment);
    Assignment delta(solver);
    Assignment deltadelta(solver);
    // None of the first solution heuristics will leave the node unperformed,
    // so we never make a neighbor.
    ASSERT_FALSE(heuristic_lns->MakeNextNeighbor(&delta, &deltadelta));
  }
}

TEST(HeuristicOperatorTest, SecondPathEmpty_PathLNS) {
  SecondPathEmptyTestForAllHeuristics(
      [](Model* model, std::unique_ptr<RoutingFilteredHeuristic> heuristic) {
        return model->solver()->RevAlloc(
            new FilteredHeuristicPathLNSOperator(std::move(heuristic)));
      });
}

TEST(HeuristicOperatorTest, SecondPathEmpty_RelocatePathAndInsertUnperformed) {
  SecondPathEmptyTestForAllHeuristics(
      [](Model* model, std::unique_ptr<RoutingFilteredHeuristic> heuristic) {
        return model->solver()->RevAlloc(
            new RelocatePathAndHeuristicInsertUnperformedOperator(
                std::move(heuristic)));
      });
}

TEST(HeuristicOperatorTest, SecondPathEmpty_ExpensiveChainLNS) {
  SecondPathEmptyTestForAllHeuristics(
      [](Model* model, std::unique_ptr<RoutingFilteredHeuristic> heuristic) {
        const auto arc_cost = ArcCost2(model);
        return model->solver()->RevAlloc(
            new FilteredHeuristicExpensiveChainLNSOperator(
                std::move(heuristic), 2,
                [arc_cost](int64_t i, int64_t j, int64_t /*vehicle*/) {
                  return arc_cost(i, j);
                }));
      });
}

TEST(HeuristicOperatorTest, SecondPathEmpty_CloseNodesLNS) {
  const int kNumCloseNodes = 50;
  SecondPathEmptyTestForAllHeuristics(
      [](Model* model, std::unique_ptr<RoutingFilteredHeuristic> heuristic) {
        return model->solver()->RevAlloc(
            new FilteredHeuristicCloseNodesLNSOperator(std::move(heuristic),
                                                       kNumCloseNodes));
      });
}

void FirstPathEmptyAndOneUnperformedTestForAllHeuristics(
    std::function<LocalSearchOperator*(
        Model* model, std::unique_ptr<RoutingFilteredHeuristic>)>
        create_heuristic_operator,
    bool expect_neighbor) {
  const int kNumNodes = 3;
  const int kNumVehicles = 2;

  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  // First route empty, node 1 performed by second vehicle, node 2 unperfomed.
  const Assignment* const assignment = CreateAssignment({4, 5, 2, 1}, &model);

  Solver* const solver = model.solver();
  for (std::unique_ptr<RoutingFilteredHeuristic>& heuristic :
       GetRoutingHeuristics(&model, /*gci_seeds_ratio*/ 0.0)) {
    LocalSearchOperator* const heuristic_lns =
        create_heuristic_operator(&model, std::move(heuristic));

    heuristic_lns->Start(assignment);
    Assignment delta(solver);
    Assignment deltadelta(solver);
    ASSERT_EQ(heuristic_lns->MakeNextNeighbor(&delta, &deltadelta),
              expect_neighbor)
        << heuristic_lns->DebugString();
    if (expect_neighbor) {
      if (!MakesSingleNodePerformedInEmptyPath(heuristic_lns->DebugString())) {
        // Only the first route should be used.
        EXPECT_EQ(delta.Value(model.NextVar(3)), 5);
      }
      EXPECT_NE(delta.Value(model.NextVar(1)), 1);
      EXPECT_NE(delta.Value(model.NextVar(2)), 2);
    }
  }
}

TEST(HeuristicOperatorTest, FirstPathEmptyOneUnperformed_PathLNS) {
  // Since there's a single non-empty route, the operator will not create a
  // neighbor.
  FirstPathEmptyAndOneUnperformedTestForAllHeuristics(
      [](Model* model, std::unique_ptr<RoutingFilteredHeuristic> heuristic) {
        return model->solver()->RevAlloc(
            new FilteredHeuristicPathLNSOperator(std::move(heuristic)));
      },
      /*expect_neighbor=*/false);
}

TEST(HeuristicOperatorTest,
     FirstPathEmptyOneUnperformed_RelocatePathAndInsertUnperformed) {
  FirstPathEmptyAndOneUnperformedTestForAllHeuristics(
      [](Model* model, std::unique_ptr<RoutingFilteredHeuristic> heuristic) {
        return model->solver()->RevAlloc(
            new RelocatePathAndHeuristicInsertUnperformedOperator(
                std::move(heuristic)));
      },
      /*expect_neighbor=*/true);
}

TEST(HeuristicOperatorTest, FirstPathEmptyOneUnperformed_ExpensiveChainLNS) {
  FirstPathEmptyAndOneUnperformedTestForAllHeuristics(
      [](Model* model, std::unique_ptr<RoutingFilteredHeuristic> heuristic) {
        const auto arc_cost = ArcCost2(model);
        return model->solver()->RevAlloc(
            new FilteredHeuristicExpensiveChainLNSOperator(
                std::move(heuristic), 2,
                [arc_cost](int64_t i, int64_t j, int64_t /*vehicle*/) {
                  return arc_cost(i, j);
                }));
      },
      /*expect_neighbor=*/true);
}

TEST(HeuristicOperatorTest, FirstPathEmptyOneUnperformed_CloseNodesLNS) {
  const int kNumCloseNodes = 50;
  FirstPathEmptyAndOneUnperformedTestForAllHeuristics(
      [](Model* model, std::unique_ptr<RoutingFilteredHeuristic> heuristic) {
        return model->solver()->RevAlloc(
            new FilteredHeuristicCloseNodesLNSOperator(std::move(heuristic),
                                                       kNumCloseNodes));
      },
      /*expect_neighbor=*/true);
}

void FirstPathEmptyTwoNodesOnDifferentPathsTestForAllHeuristics(
    std::function<LocalSearchOperator*(
        Model* model, std::unique_ptr<RoutingFilteredHeuristic>)>
        create_heuristic_operator) {
  const int kNumNodes = 3;
  const int kNumVehicles = 3;

  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  // First route empty, node 1 performed by second vehicle, node 2 by the third.
  const Assignment* const assignment =
      CreateAssignment({5, 6, 7, 1, 2}, &model);

  Solver* const solver = model.solver();
  for (std::unique_ptr<RoutingFilteredHeuristic>& heuristic :
       GetRoutingHeuristics(&model, /*gci_seeds_ratio*/ 0.0)) {
    LocalSearchOperator* const heuristic_lns =
        create_heuristic_operator(&model, std::move(heuristic));

    heuristic_lns->Start(assignment);
    Assignment delta(solver);
    Assignment deltadelta(solver);
    ASSERT_TRUE(heuristic_lns->MakeNextNeighbor(&delta, &deltadelta));
    ASSERT_TRUE(delta.Contains(model.NextVar(1)));
    EXPECT_NE(delta.Value(model.NextVar(1)), 1);
    EXPECT_TRUE(!delta.Contains(model.NextVar(2)) ||
                delta.Value(model.NextVar(2)) != 2);
  }
}

TEST(HeuristicOperatorTest, FirstPathEmptyTwoNodesOnDifferentPaths_PathLNS) {
  FirstPathEmptyTwoNodesOnDifferentPathsTestForAllHeuristics(
      [](Model* model, std::unique_ptr<RoutingFilteredHeuristic> heuristic) {
        return model->solver()->RevAlloc(
            new FilteredHeuristicPathLNSOperator(std::move(heuristic)));
      });
}

TEST(HeuristicOperatorTest,
     FirstPathEmptyTwoNodesOnDifferentPaths_ExpensiveChainLNS) {
  FirstPathEmptyTwoNodesOnDifferentPathsTestForAllHeuristics(
      [](Model* model, std::unique_ptr<RoutingFilteredHeuristic> heuristic) {
        const auto arc_cost = ArcCost2(model);
        return model->solver()->RevAlloc(
            new FilteredHeuristicExpensiveChainLNSOperator(
                std::move(heuristic), 2,
                [arc_cost](int64_t i, int64_t j, int64_t /*vehicle*/) {
                  return arc_cost(i, j);
                }));
      });
}

TEST(HeuristicOperatorTest,
     FirstPathEmptyTwoNodesOnDifferentPaths_CloseNodesLNS) {
  const int kNumCloseNodes = 50;
  FirstPathEmptyTwoNodesOnDifferentPathsTestForAllHeuristics(
      [](Model* model, std::unique_ptr<RoutingFilteredHeuristic> heuristic) {
        return model->solver()->RevAlloc(
            new FilteredHeuristicCloseNodesLNSOperator(std::move(heuristic),
                                                       kNumCloseNodes));
      });
}

// Tests that in the LocalCheapestInsertionFilteredHeuristic, a node is
// considered for insertion at each call to MakeNextNeighbor independently from
// the number of vehicles in the model.
TEST(LocalCheapestInsertionFilteredHeuristicTest,
     NodeRepeatedlyConsideredForInsertion) {
  const int kNumNodes = 2;
  const int kNumVehicles = 1;

  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  model.CloseModel();

  // Route empty, node unperformed.
  const Assignment* const assignment = CreateAssignment({2, 1}, &model);

  Solver* const solver = model.solver();
  LocalCheapestInsertionParameters lci_params;
  lci_params.set_pickup_delivery_strategy(
      LocalCheapestInsertionParameters::AUTOMATIC);
  auto heuristic = std::make_unique<LocalCheapestInsertionFilteredHeuristic>(
      &model, []() { return false; },
      [](int64_t, int64_t, int64_t) { return 1; }, lci_params, nullptr,
      /*use_first_solution_hint=*/false);
  LocalSearchOperator* const heuristic_lns =
      model.solver()->RevAlloc(new FilteredHeuristicCloseNodesLNSOperator(
          std::move(heuristic), /*num_close_nodes=*/2));

  for (int i = 0; i < 10; ++i) {
    heuristic_lns->Start(assignment);
    Assignment delta(solver);
    Assignment deltadelta(solver);

    ASSERT_TRUE(heuristic_lns->MakeNextNeighbor(&delta, &deltadelta));
    EXPECT_EQ(delta.Value(model.NextVar(0)), 1);
    EXPECT_FALSE(heuristic_lns->MakeNextNeighbor(&delta, &deltadelta));
  }
}

TEST(RelocateVisitTypeOperatorTest, NoVisitTypes) {
  const int kNumNodes = 2;
  const int kNumVehicles = 2;

  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  // Route empty, node unperformed.
  const Assignment* const assignment = CreateAssignment({1, 3, 4}, &model);

  Solver* const solver = model.solver();
  for (std::unique_ptr<RoutingFilteredHeuristic>& heuristic :
       GetRoutingHeuristics(&model, /*gci_seeds_ratio*/ 0.0)) {
    LocalSearchOperator* const heuristic_lns = model.solver()->RevAlloc(
        new RelocateVisitTypeOperator(std::move(heuristic)));

    heuristic_lns->Start(assignment);
    Assignment delta(solver);
    Assignment deltadelta(solver);
    ASSERT_FALSE(heuristic_lns->MakeNextNeighbor(&delta, &deltadelta));
  }
}

TEST(RelocateVisitTypeOperatorTest, WithVisitTypes) {
  const int kNumNodes = 5;
  const int kNumVehicles = 2;

  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  model.SetVisitType(3, /*type*/ 0,
                     VisitTypePolicy::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED);
  model.SetVisitType(4, /*type*/ 1,
                     VisitTypePolicy::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED);
  model.SetVisitType(2, /*type*/ 2,
                     VisitTypePolicy::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED);
  model.SetVisitType(1, /*type*/ 3,
                     VisitTypePolicy::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED);
  model.AddSameVehicleRequiredTypeAlternatives(1, {0});
  model.AddSameVehicleRequiredTypeAlternatives(3, {1});
  model.AddPickupAndDelivery(1, 3);

  // Route 1 empty, node 1 unperformed, the rest on route 0.
  const Assignment* const assignment =
      CreateAssignment({2, 1, 3, 4, 6, 7}, &model);

  Solver* const solver = model.solver();
  for (std::unique_ptr<RoutingFilteredHeuristic>& heuristic :
       GetRoutingHeuristics(&model, /*gci_seeds_ratio*/ 0.0,
                            /*insertion_only=*/true)) {
    SCOPED_TRACE(heuristic->DebugString());
    LocalSearchOperator* const heuristic_lns = model.solver()->RevAlloc(
        new RelocateVisitTypeOperator(std::move(heuristic)));

    heuristic_lns->Start(assignment);
    Assignment delta(solver);
    Assignment deltadelta(solver);
    // Relocate component 0,1,3, corresponding to nodes 3,4,1.
    EXPECT_TRUE(heuristic_lns->MakeNextNeighbor(&delta, &deltadelta));
    EXPECT_TRUE(delta.Contains(model.NextVar(1)));
    EXPECT_NE(delta.Value(model.NextVar(1)), 1);
    EXPECT_TRUE(!delta.Contains(model.NextVar(2)) ||
                delta.Value(model.NextVar(2)) != 2);
    // Not relocating component 2 which has no dependencies.
    EXPECT_FALSE(heuristic_lns->MakeNextNeighbor(&delta, &deltadelta));
  }
}

// TODO(user): Add unit tests for all other operators.
// TODO(user): Add unit tests with vehicle variables in neighbors.

}  // namespace
}  // namespace operations_research::routing

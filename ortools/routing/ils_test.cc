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

#include "ortools/routing/ils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <numeric>
#include <random>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/log/check.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "google/protobuf/duration.pb.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/mathutil.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/routing/enums.pb.h"
#include "ortools/routing/ils.pb.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/routing.h"
#include "ortools/routing/types.h"

namespace operations_research::routing {
namespace {

TEST(IteratedLocalSearchEventSubscriberManagerTest, AddAndRemoveSubscriber) {
  IteratedLocalSearchEventManager manager;
  IteratedLocalSearchEventSubscriber subscriber;

  EXPECT_TRUE(manager.AddSubscriber(&subscriber));
  // Adding a subscriber twice fails.
  EXPECT_FALSE(manager.AddSubscriber(&subscriber));

  EXPECT_TRUE(manager.RemoveSubscriber(&subscriber));
  // Removing a subscriber twice fails.
  EXPECT_FALSE(manager.RemoveSubscriber(&subscriber));
}

class MockIteratedLocalSearchEventSubscriber
    : public IteratedLocalSearchEventSubscriber {
 public:
  MOCK_METHOD(void, OnLocalOptimumReached,
              (const Assignment* absl_nonnull assignment), (override));
  MOCK_METHOD(void, OnReferenceSolutionUpdated,
              (const Assignment* absl_nonnull assignment), (override));
};

TEST(IteratedLocalSearchEventSubscriberManagerTest, NotifySubscribers) {
  IteratedLocalSearchEventManager manager;
  MockIteratedLocalSearchEventSubscriber subscriber;
  Solver solver("dummy");
  Assignment dummy_assignment(&solver);

  EXPECT_TRUE(manager.AddSubscriber(&subscriber));
  EXPECT_CALL(subscriber, OnLocalOptimumReached(&dummy_assignment)).Times(1);
  manager.OnLocalOptimumReached(&dummy_assignment);
  EXPECT_CALL(subscriber, OnReferenceSolutionUpdated(&dummy_assignment))
      .Times(1);
  manager.OnReferenceSolutionUpdated(&dummy_assignment);

  EXPECT_TRUE(manager.RemoveSubscriber(&subscriber));
  EXPECT_CALL(subscriber, OnLocalOptimumReached(&dummy_assignment)).Times(0);
  manager.OnLocalOptimumReached(&dummy_assignment);
  EXPECT_CALL(subscriber, OnReferenceSolutionUpdated(&dummy_assignment))
      .Times(0);
  manager.OnReferenceSolutionUpdated(&dummy_assignment);
}

// Creates an assignment with next and vehicle variables set as provided.
Assignment* CreateAssignment(Model* model, absl::Span<const int64_t> nexts,
                             absl::Span<const int> vehicles) {
  EXPECT_EQ(nexts.size(), model->Size());
  Assignment* assignment = model->solver()->MakeAssignment();
  for (int i = 0; i < nexts.size(); ++i) {
    assignment->Add(model->NextVar(i))->SetValue(nexts[i]);
  }
  EXPECT_EQ(vehicles.size(), model->Size() + model->vehicles());
  for (int i = 0; i < vehicles.size(); ++i) {
    assignment->Add(model->VehicleVar(i))->SetValue(vehicles[i]);
  }
  return assignment;
}

// Stores next and vehicle variables representing a routing assignment.
struct NextsAndVehicles {
  std::vector<int64_t> nexts;
  std::vector<int> vehicles;
};

// Generates nexts and vehicles vectors describing a solution with customers
// sequentially allocated to routes of the given input sizes.
NextsAndVehicles GenerateSequentialNextsAndVehicles(
    const Model& model, absl::Span<const int> route_sizes) {
  EXPECT_EQ(route_sizes.size(), model.vehicles());
  const int num_customers =
      std::accumulate(route_sizes.begin(), route_sizes.end(), 0L);
  EXPECT_LE(num_customers, model.Size() - model.vehicles());

  NextsAndVehicles vars;
  auto& [nexts, vehicles] = vars;
  nexts.resize(model.Size());
  std::iota(nexts.begin(), nexts.end(), 0L);
  vehicles.resize(model.Size() + model.vehicles(), -1);

  int next_customer = 0;
  while (model.IsStart(next_customer)) ++next_customer;

  for (int vehicle = 0; vehicle < model.vehicles(); ++vehicle) {
    int curr_node = model.Start(vehicle);
    int size = route_sizes[vehicle];

    while (size-- > 0) {
      EXPECT_FALSE(model.IsEnd(next_customer));
      nexts[curr_node] = next_customer;
      vehicles[curr_node] = vehicle;
      curr_node = next_customer;
      ++next_customer;
      while (model.IsStart(next_customer)) ++next_customer;
    }

    nexts[curr_node] = model.End(vehicle);
    vehicles[curr_node] = vehicle;
    vehicles[model.End(vehicle)] = vehicle;
  }

  return vars;
}

class RoutingSolutionTest : public testing::Test {
 protected:
  void SetUp() override {
    const int kNumNodes = 9;
    const int kNumVehicles = 2;
    IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
    model_ = std::make_unique<Model>(manager);
    model_->AddPickupAndDelivery(2, 7);
    model_->CloseModel();

    solution_ = std::make_unique<Solution>(*model_);

    // Route 0: [0] 1 2 3 4 [10]
    // Route 1: [9] 5 6 7 8 [11]
    vars_ = GenerateSequentialNextsAndVehicles(*model_, /*route_sizes=*/{4, 4});
  }

  std::unique_ptr<Model> model_;
  std::unique_ptr<Solution> solution_;

  NextsAndVehicles vars_;
};

TEST_F(RoutingSolutionTest, Initialization) {
  solution_->Reset(CreateAssignment(model_.get(), vars_.nexts, vars_.vehicles));
  for (int64_t node = 0; node < model_->nodes() + model_->vehicles(); ++node) {
    EXPECT_FALSE(solution_->BelongsToInitializedRoute(node));
  }
}

TEST_F(RoutingSolutionTest, RouteInfoInitialization) {
  solution_->Reset(CreateAssignment(model_.get(), vars_.nexts, vars_.vehicles));
  solution_->InitializeRouteInfoIfNeeded(0);
  EXPECT_EQ(solution_->GetRouteSize(0), 4);

  for (int64_t node = 0; node < model_->nodes() + model_->vehicles(); ++node) {
    EXPECT_EQ(solution_->BelongsToInitializedRoute(node),
              vars_.vehicles[node] == 0);
  }

  solution_->Reset(CreateAssignment(model_.get(), vars_.nexts, vars_.vehicles));
  for (int64_t node = 0; node < model_->nodes() + model_->vehicles(); ++node) {
    EXPECT_FALSE(solution_->BelongsToInitializedRoute(node));
  }
}

TEST_F(RoutingSolutionTest, GetNextAndPrev) {
  solution_->Reset(CreateAssignment(model_.get(), vars_.nexts, vars_.vehicles));
  solution_->InitializeRouteInfoIfNeeded(0);
  solution_->InitializeRouteInfoIfNeeded(1);
  EXPECT_EQ(solution_->GetRouteSize(0), 4);
  EXPECT_EQ(solution_->GetRouteSize(1), 4);

  std::vector<int64_t> nexts = vars_.nexts;
  // Add the next values for the end nodes.
  nexts.insert(nexts.end(), {0, 9});
  const std::vector<int64_t> prevs = {10, 0, 1, 2, 3, 9, 5, 6, 7, 11, 4, 8};

  for (int64_t node = 0; node < model_->nodes() + model_->vehicles(); ++node) {
    EXPECT_EQ(solution_->GetNextNodeIndex(node), nexts[node]);
    EXPECT_EQ(solution_->GetInitializedPrevNodeIndex(node), prevs[node]);
  }
}

TEST_F(RoutingSolutionTest, NodeRemoval) {
  solution_->Reset(CreateAssignment(model_.get(), vars_.nexts, vars_.vehicles));
  solution_->InitializeRouteInfoIfNeeded(0);

  int route_size = 4;
  EXPECT_EQ(solution_->GetRouteSize(0), route_size);
  for (const int64_t node : {1, 2, 4}) {
    EXPECT_TRUE(solution_->CanBeRemoved(node));
    solution_->RemoveNode(node);
    --route_size;
    EXPECT_EQ(solution_->GetRouteSize(0), route_size);
  }

  EXPECT_EQ(solution_->GetRouteSize(0), 1);
  for (const int64_t node : {1, 2, 4}) {
    EXPECT_FALSE(solution_->CanBeRemoved(node));
  }
}

TEST_F(RoutingSolutionTest, PickupAndDeliveryRemoval) {
  solution_->Reset(CreateAssignment(model_.get(), vars_.nexts, vars_.vehicles));

  EXPECT_TRUE(solution_->CanBeRemoved(7));
  solution_->RemovePerformedPickupDeliverySibling(2);
  EXPECT_FALSE(solution_->CanBeRemoved(7));

  EXPECT_TRUE(solution_->CanBeRemoved(2));
  solution_->RemovePerformedPickupDeliverySibling(7);
  EXPECT_FALSE(solution_->CanBeRemoved(2));
}

TEST_F(RoutingSolutionTest, GetRandomAdjacentVisit) {
  solution_->Reset(CreateAssignment(model_.get(), vars_.nexts, vars_.vehicles));
  // Route 0: [0] 1 2 3 4 [10]
  solution_->InitializeRouteInfoIfNeeded(0);

  std::mt19937_64 rnd(/*seed=*/0);
  std::bernoulli_distribution boolean_dist;

  for (int attempt = 0; attempt < 10; ++attempt) {
    const int64_t adjOfVisit2 =
        solution_->GetRandomAdjacentVisit(2, rnd, boolean_dist);
    EXPECT_THAT(std::vector<int64_t>{adjOfVisit2},
                testing::IsSubsetOf(std::vector<int64_t>{1, 3}));

    const int64_t adjOfVisit3 =
        solution_->GetRandomAdjacentVisit(3, rnd, boolean_dist);
    EXPECT_THAT(std::vector<int64_t>{adjOfVisit3},
                testing::IsSubsetOf(std::vector<int64_t>{2, 4}));

    EXPECT_EQ(solution_->GetRandomAdjacentVisit(1, rnd, boolean_dist), 2);
    EXPECT_EQ(solution_->GetRandomAdjacentVisit(4, rnd, boolean_dist), 3);
  }
}

TEST_F(RoutingSolutionTest, GetDeterministicAdjacentVisit) {
  solution_->Reset(CreateAssignment(model_.get(), vars_.nexts, vars_.vehicles));
  // Route 0: [0] 1 2 3 4 [10]
  solution_->InitializeRouteInfoIfNeeded(0);

  std::mt19937_64 rnd(/*seed=*/0);
  // The distribution will always return true, thus the returned adjacent
  // visit is deterministically selected.
  std::bernoulli_distribution boolean_dist(1);

  for (int attempt = 0; attempt < 10; ++attempt) {
    EXPECT_EQ(solution_->GetRandomAdjacentVisit(2, rnd, boolean_dist), 3);
    EXPECT_EQ(solution_->GetRandomAdjacentVisit(3, rnd, boolean_dist), 4);
    EXPECT_EQ(solution_->GetRandomAdjacentVisit(1, rnd, boolean_dist), 2);
    EXPECT_EQ(solution_->GetRandomAdjacentVisit(4, rnd, boolean_dist), 3);
  }
}

TEST_F(RoutingSolutionTest, GetRandomSequenceOfVisits) {
  solution_->Reset(CreateAssignment(model_.get(), vars_.nexts, vars_.vehicles));
  // Route 0: [0] 1 2 3 4 [10]
  solution_->InitializeRouteInfoIfNeeded(0);

  std::mt19937_64 rnd(/*seed=*/0);
  std::bernoulli_distribution boolean_dist;

  for (int attempt = 0; attempt < 10; ++attempt) {
    const std::vector<int64_t> sequence1 =
        solution_->GetRandomSequenceOfVisits(2, rnd, boolean_dist, 1);
    EXPECT_EQ(sequence1, std::vector<int64_t>{2});

    const std::vector<int64_t> sequence2 =
        solution_->GetRandomSequenceOfVisits(2, rnd, boolean_dist, 2);
    EXPECT_THAT(
        std::vector<std::vector<int64_t>>{sequence2},
        testing::IsSubsetOf(std::vector<std::vector<int64_t>>{{1, 2}, {2, 3}}));

    const std::vector<int64_t> sequence3 =
        solution_->GetRandomSequenceOfVisits(2, rnd, boolean_dist, 3);
    EXPECT_THAT(std::vector<std::vector<int64_t>>{sequence3},
                testing::IsSubsetOf(
                    std::vector<std::vector<int64_t>>{{1, 2, 3}, {2, 3, 4}}));

    const std::vector<int64_t> sequence4 =
        solution_->GetRandomSequenceOfVisits(2, rnd, boolean_dist, 4);
    EXPECT_EQ(sequence4, (std::vector<int64_t>{1, 2, 3, 4}));
  }
}

TEST_F(RoutingSolutionTest, GetDeterministicSequenceOfVisits) {
  solution_->Reset(CreateAssignment(model_.get(), vars_.nexts, vars_.vehicles));
  // Route 0: [0] 1 2 3 4 [10]
  solution_->InitializeRouteInfoIfNeeded(0);

  std::mt19937_64 rnd(/*seed=*/0);
  // The distribution will always return true, thus the returned sequence of
  // visits is deterministically selected.
  std::bernoulli_distribution boolean_dist(1);

  for (int attempt = 0; attempt < 10; ++attempt) {
    const std::vector<int64_t> sequence1 =
        solution_->GetRandomSequenceOfVisits(2, rnd, boolean_dist, 1);
    EXPECT_EQ(sequence1, std::vector<int64_t>{2});

    const std::vector<int64_t> sequence2 =
        solution_->GetRandomSequenceOfVisits(2, rnd, boolean_dist, 2);
    EXPECT_EQ(sequence2, (std::vector<int64_t>{2, 3}));

    const std::vector<int64_t> sequence3 =
        solution_->GetRandomSequenceOfVisits(2, rnd, boolean_dist, 3);
    EXPECT_EQ(sequence3, (std::vector<int64_t>{2, 3, 4}));

    const std::vector<int64_t> sequence4 =
        solution_->GetRandomSequenceOfVisits(2, rnd, boolean_dist, 4);
    EXPECT_EQ(sequence4, (std::vector<int64_t>{1, 2, 3, 4}));
  }
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

// Returns the number of performed customers for the given next accessor.
int CountPerformedCustomersFromNextAccessor(
    const Model& model, const std::function<int64_t(int64_t)>& next_accessor) {
  int performed_customers = 0;

  for (int route = 0; route < model.vehicles(); ++route) {
    const int start = model.Start(route);
    int64_t node = next_accessor(start);
    while (!model.IsEnd(node)) {
      const int64_t next_node = next_accessor(node);
      ++performed_customers;
      node = next_node;
    }
  }

  return performed_customers;
}

// Returns unperformed nodes for the given next accessors.
std::vector<int64_t> GetUnperformedNodes(
    const Model& model, const std::function<int64_t(int64_t)>& next_accessor) {
  std::vector<int64_t> unperformed_nodes;
  for (int64_t node = 0; node < model.nodes(); ++node) {
    if (next_accessor(node) == node) {
      unperformed_nodes.push_back(node);
    }
  }
  return unperformed_nodes;
}

// Returns a vector containing some relevant first solution strategies.
std::vector<FirstSolutionStrategy::Value> GetFirstSolutionStrategies() {
  return {FirstSolutionStrategy::ALL_UNPERFORMED,
          FirstSolutionStrategy::SAVINGS,
          FirstSolutionStrategy::PARALLEL_SAVINGS,
          FirstSolutionStrategy::SWEEP,
          FirstSolutionStrategy::CHRISTOFIDES,
          FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION,
          FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION,
          FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
          FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION,
          FirstSolutionStrategy::GLOBAL_CHEAPEST_ARC,
          FirstSolutionStrategy::LOCAL_CHEAPEST_ARC};
}

// Returns the available perturbation strategies.
std::vector<PerturbationStrategy::Value> GetPerturbationStrategies() {
  return {PerturbationStrategy::RUIN_AND_RECREATE};
}

// Returns proto message setters for the given acceptance strategy.
std::vector<std::function<void(AcceptanceStrategy*)>>
GetCostBasedAcceptanceStrategySetters() {
  std::vector<std::function<void(AcceptanceStrategy*)>> setters;
  setters.push_back([](AcceptanceStrategy* acceptance_strategy) {
    acceptance_strategy->mutable_greedy_descent();
  });
  setters.push_back([](AcceptanceStrategy* acceptance_strategy) {
    SimulatedAnnealingAcceptanceStrategy* sa =
        acceptance_strategy->mutable_simulated_annealing();
    sa->set_cooling_schedule_strategy(CoolingScheduleStrategy::LINEAR);
    sa->set_automatic_temperatures(true);
  });
  return setters;
}

// Returns the supported recreate strategies.
std::vector<FirstSolutionStrategy::Value> GetRecreateStrategies() {
  return {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
          FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION,
          FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION,
          FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION,
          FirstSolutionStrategy::SAVINGS,
          FirstSolutionStrategy::PARALLEL_SAVINGS};
}

// Returns a list of functions that can be used to setup recreate strategy
// parameters.
std::vector<std::function<void(RecreateStrategy&)>>
GetRecreateParametersSetupFunctions(FirstSolutionStrategy::Value heuristic) {
  std::vector<std::function<void(RecreateStrategy&)>> setup_functions = {
      [](RecreateStrategy&) {
        // Do nothing to use default parameters inherited from
        // RoutingSearchParameters.
      }};
  switch (heuristic) {
    case FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION:
    case FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION:
      setup_functions.push_back([](RecreateStrategy& strategy) {
        strategy.mutable_parameters()
            ->mutable_local_cheapest_insertion()
            ->add_insertion_sorting_properties(
                LocalCheapestInsertionParameters::SORTING_PROPERTY_RANDOM);
      });
      break;
    default:
      break;
  }
  return setup_functions;
}

// Simple ruin procedure removing the given node.
class RemoveNodeRuinProcedure : public RuinProcedure {
 public:
  RemoveNodeRuinProcedure(const Model& model, int node)
      : model_(model), removed_node_(node) {
    EXPECT_FALSE(model.IsStart(node));
    EXPECT_FALSE(model.IsEnd(node));
    EXPECT_TRUE(model.GetPickupAndDeliveryPairs().empty());
  }

  std::function<int64_t(int64_t)> Ruin(const Assignment* assignment) override {
    return [this, assignment](int64_t node) {
      if (node == removed_node_) {
        return node;
      }
      const int64_t next = assignment->Value(model_.NextVar(node));
      return next == removed_node_ ? assignment->Value(model_.NextVar(next))
                                   : next;
    };
  }

 private:
  const Model& model_;
  int removed_node_;
};

class CompositeRuinProcedureTest : public testing::Test {
 protected:
  void SetUp() override {
    const int kNumNodes = 9;
    const int kNumVehicles = 2;
    IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
    model_ = std::make_unique<Model>(manager);
    model_->SetArcCostEvaluatorOfAllVehicles(
        model_->RegisterTransitCallback(ArcCost2(model_.get())));

    model_->CloseModel();
  }

  std::unique_ptr<Model> model_;
};

TEST_F(CompositeRuinProcedureTest, RunAllRuinsCompositionStrategy) {
  const std::vector<std::vector<int64_t>> removed_nodes_sets = {
      {1}, {1, 3, 5, 7}, {1, 2, 3, 4}, {1, 2, 3, 4, 5, 6, 7, 8}};

  for (RuinCompositionStrategy::Value composition_strategy :
       {RuinCompositionStrategy::RUN_ALL_SEQUENTIALLY,
        RuinCompositionStrategy::RUN_ALL_RANDOMLY}) {
    for (const auto& nodes : removed_nodes_sets) {
      // Route 0: [0] 1 2 3 4 [10]
      // Route 1: [9] 5 6 7 8 [11]
      const auto [nexts, vehicles] =
          GenerateSequentialNextsAndVehicles(*model_, /*route_sizes=*/{4, 4});

      const Assignment* assignment =
          CreateAssignment(model_.get(), nexts, vehicles);

      std::vector<std::unique_ptr<RuinProcedure>> ruins;
      for (int64_t node : nodes) {
        ruins.emplace_back(
            std::make_unique<RemoveNodeRuinProcedure>(*model_, node));
      }

      std::mt19937_64 rnd(/*seed=*/0);
      CompositeRuinProcedure ruin(model_.get(), std::move(ruins),
                                  composition_strategy, &rnd);

      for (int iter = 0; iter < 10; ++iter) {
        EXPECT_THAT(GetUnperformedNodes(*model_.get(), ruin.Ruin(assignment)),
                    testing::UnorderedElementsAreArray(nodes));
      }
    }
  }
}

TEST_F(CompositeRuinProcedureTest, RunSingleRuinCompositionStrategy) {
  RuinCompositionStrategy::Value composition_strategy =
      RuinCompositionStrategy::RUN_ONE_RANDOMLY;

  const std::vector<int64_t> nodes = {1, 2, 3, 4, 5, 6, 7, 8};

  // Route 0: [0] 1 2 3 4 [10]
  // Route 1: [9] 5 6 7 8 [11]
  const auto [nexts, vehicles] =
      GenerateSequentialNextsAndVehicles(*model_, /*route_sizes=*/{4, 4});
  const Assignment* assignment =
      CreateAssignment(model_.get(), nexts, vehicles);

  std::vector<std::unique_ptr<RuinProcedure>> ruins;
  for (int64_t node : nodes) {
    ruins.emplace_back(
        std::make_unique<RemoveNodeRuinProcedure>(*model_, node));
  }

  std::mt19937_64 rnd(/*seed=*/0);
  CompositeRuinProcedure ruin(model_.get(), std::move(ruins),
                              composition_strategy, &rnd);

  const int num_removed_customers = 1;
  const int num_depots = 1;

  for (int iter = 0; iter < 10; ++iter) {
    EXPECT_EQ(CountPerformedCustomersFromNextAccessor(*model_.get(),
                                                      ruin.Ruin(assignment)),
              model_->nodes() - num_depots - num_removed_customers);
  }
}

// Test parameters:
// - int -- number of ruined routes.
class CloseRouteRemovalRuinProcedureTest
    : public ::testing::TestWithParam<int> {
 protected:
  void SetUp() override {
    const int kNumNodes = 5;
    const int kNumVehicles = 2;
    IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
    model_ = std::make_unique<Model>(manager);
    model_->SetArcCostEvaluatorOfAllVehicles(
        model_->RegisterTransitCallback(ArcCost2(model_.get())));
    model_->SetFixedCostOfVehicle(1000, 1);

    model_->CloseModel();
  }

  std::unique_ptr<Model> model_;
};

TEST_P(CloseRouteRemovalRuinProcedureTest, RemoveRoutes) {
  const int expected_removed_routes = GetParam();

  // Route 0: [0] 1 2 [6]
  // Route 1: [5] 3 4 [7]
  const auto [nexts, vehicles] =
      GenerateSequentialNextsAndVehicles(*model_, /*route_sizes=*/{2, 2});
  const Assignment* assignment =
      CreateAssignment(model_.get(), nexts, vehicles);

  std::mt19937_64 rnd(/*seed=*/0);
  const int kNumNeighborsForRouteSelection = 4;
  CloseRoutesRemovalRuinProcedure ruin(model_.get(), &rnd,
                                       expected_removed_routes,
                                       kNumNeighborsForRouteSelection);
  const std::function<int64_t(int64_t)> next_accessor = ruin.Ruin(assignment);

  int removed_routes = 0;
  for (int route = 0; route < model_->vehicles(); ++route) {
    const int start = model_->Start(route);
    if (next_accessor(start) == model_->End(route)) {
      removed_routes++;
      int64_t node = assignment->Value(model_->NextVar(start));
      while (!model_->IsEnd(node)) {
        EXPECT_EQ(next_accessor(node), node);
        node = assignment->Value(model_->NextVar(node));
      }
    } else {
      int64_t node = start;
      while (!model_->IsEnd(node)) {
        const int64_t next_node = assignment->Value(model_->NextVar(node));
        EXPECT_EQ(next_node, next_accessor(node));
        node = next_node;
      }
    }
  }

  EXPECT_EQ(removed_routes,
            std::min(expected_removed_routes, model_->vehicles()));
}

TEST_P(CloseRouteRemovalRuinProcedureTest, RemoveRoutesFromEmptySolution) {
  const int expected_removed_routes = GetParam();

  // Route 0: [0] [6]
  // Route 1: [5] [7]
  // All customers are unperformed.
  const auto [nexts, vehicles] =
      GenerateSequentialNextsAndVehicles(*model_, /*route_sizes=*/{0, 0});
  const Assignment* assignment =
      CreateAssignment(model_.get(), nexts, vehicles);

  std::mt19937_64 rnd(/*seed=*/0);
  const int kNumNeighborsForRouteSelection = 4;
  CloseRoutesRemovalRuinProcedure ruin(model_.get(), &rnd,
                                       expected_removed_routes,
                                       kNumNeighborsForRouteSelection);
  const std::function<int64_t(int64_t)> next_accessor = ruin.Ruin(assignment);

  // If all customers are unperformed, the ruin should do nothing.
  for (int i = 0; i < nexts.size(); ++i) {
    EXPECT_EQ(next_accessor(i), nexts[i]);
  }
}

TEST_P(CloseRouteRemovalRuinProcedureTest, RemoveRoutesWithUnperformedNodes) {
  // Route 0: [0] 1 [6]
  // Route 1: [5] [7]
  // Customers 2, 3, 4 are unperformed.
  const auto [nexts, vehicles] =
      GenerateSequentialNextsAndVehicles(*model_, /*route_sizes=*/{1, 0});
  const Assignment* assignment =
      CreateAssignment(model_.get(), nexts, vehicles);

  std::mt19937_64 rnd(/*seed=*/0);
  const int kRemovedRoutes = 1;
  const int kNumNeighborsForRouteSelection = 4;
  CloseRoutesRemovalRuinProcedure ruin(model_.get(), &rnd, kRemovedRoutes,
                                       kNumNeighborsForRouteSelection);
  const std::function<int64_t(int64_t)> next_accessor = ruin.Ruin(assignment);

  EXPECT_EQ(next_accessor(0), 6);
}

TEST_P(CloseRouteRemovalRuinProcedureTest, RemoveOnlySeedRouteZeroNeighbors) {
  // Route 0: [0] 1 2 [6]
  // Route 1: [5] 3 4 [7]
  const auto [nexts, vehicles] =
      GenerateSequentialNextsAndVehicles(*model_, /*route_sizes=*/{2, 2});
  const Assignment* assignment =
      CreateAssignment(model_.get(), nexts, vehicles);

  // Whatever it is the positive requested number of removed routes, if the
  // specified number of neighbors do not allow to identify neighboring nodes,
  // only the seed route should be removed.
  std::mt19937_64 rnd(/*seed=*/0);
  const int kRemovedRoutes = GetParam();
  const int kNumNeighborsForRouteSelection = 0;
  CloseRoutesRemovalRuinProcedure ruin(model_.get(), &rnd, kRemovedRoutes,
                                       kNumNeighborsForRouteSelection);
  const std::function<int64_t(int64_t)> next_accessor = ruin.Ruin(assignment);

  int removed_routes = 0;
  for (int route = 0; route < model_->vehicles(); ++route) {
    const int start = model_->Start(route);
    if (next_accessor(start) == model_->End(route)) {
      removed_routes++;
    } else {
      int64_t node = start;
      while (!model_->IsEnd(node)) {
        const int64_t next_node = assignment->Value(model_->NextVar(node));
        EXPECT_EQ(next_node, next_accessor(node));
        node = next_node;
      }
    }
  }

  // We do not expect any removed route when kRemovedRoutes is 0.
  EXPECT_EQ(removed_routes, std::min(kRemovedRoutes, 1));
}

INSTANTIATE_TEST_SUITE_P(DifferentNumberOfRemovedRoutes,
                         CloseRouteRemovalRuinProcedureTest,
                         ::testing::Values(0, 1, 2, 3));

// Test parameters:
// - int -- number of ruined routes.
class RandomWalkRemovalRuinProcedureTest
    : public ::testing::TestWithParam<int> {
 protected:
  void SetUp() override {
    const int kNumNodes = 9;
    const int kNumVehicles = 2;
    IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
    model_ = std::make_unique<Model>(manager);
    model_->SetArcCostEvaluatorOfAllVehicles(
        model_->RegisterTransitCallback(ArcCost2(model_.get())));

    model_->AddPickupAndDelivery(2, 4);

    model_->CloseModel();
  }

  std::unique_ptr<Model> model_;
};

TEST_P(RandomWalkRemovalRuinProcedureTest, RemoveCustomers) {
  const int expected_removed_customers = GetParam();

  // Route 0: [0] 1 2 3 4 [10]
  // Route 1: [9] 5 6 7 8 [11]
  const auto [nexts, vehicles] =
      GenerateSequentialNextsAndVehicles(*model_, /*route_sizes=*/{4, 4});
  const Assignment* assignment =
      CreateAssignment(model_.get(), nexts, vehicles);

  std::mt19937_64 rnd(/*seed=*/0);
  const int kNumNeighborsForRouteSelection = 8;
  RandomWalkRemovalRuinProcedure ruin(model_.get(), &rnd,
                                      expected_removed_customers,
                                      kNumNeighborsForRouteSelection);
  const std::function<int64_t(int64_t)> next_accessor = ruin.Ruin(assignment);

  int performed_customers =
      CountPerformedCustomersFromNextAccessor(*model_, next_accessor);

  const int num_customers = model_->Size() - model_->vehicles();
  const int removed_customers = num_customers - performed_customers;

  // We consider the removal of a pickup and delivery as a single customer
  // removal.
  const bool has_pickup_delivery_pair_been_removed =
      next_accessor(2) == 2 && next_accessor(4) == 4;

  EXPECT_EQ(removed_customers,
            std::min(expected_removed_customers +
                         (has_pickup_delivery_pair_been_removed ? 1 : 0),
                     num_customers));
}

INSTANTIATE_TEST_SUITE_P(DifferentRandomWalkLengths,
                         RandomWalkRemovalRuinProcedureTest,
                         ::testing::Values(0, 2, 4, 6, 8));

// Returns the number of visits per vehicle that have been removed from the
// assignment, according to the next accessor function.
std::vector<int> CountNumRemovedVisitsPerVehicle(
    const Model& model, const Assignment& assignment,
    const std::function<int64_t(int64_t)>& next_accessor) {
  std::vector<int> num_removed_visits(model.vehicles(), 0);
  for (int vehicle = 0; vehicle < model.vehicles(); ++vehicle) {
    int64_t curr_node = model.Start(vehicle);
    while (curr_node != model.End(vehicle)) {
      num_removed_visits[vehicle] += next_accessor(curr_node) == curr_node;
      curr_node = assignment.Value(model.NextVar(curr_node));
    }
  }
  return num_removed_visits;
}

// Returns the number of visits that have been removed from the assignment,
// according to the next accessor function.
int CountNumRemovedVisits(
    const Model& model, const Assignment& assignment,
    const std::function<int64_t(int64_t)>& next_accessor) {
  std::vector<int> num_removed_visits =
      CountNumRemovedVisitsPerVehicle(model, assignment, next_accessor);
  return std::accumulate(num_removed_visits.begin(), num_removed_visits.end(),
                         0);
}

// Solves the model with the given assignment. Returns an assignment with a
// valid objective function if successful. Returns nullptr otherwise.
const Assignment* FinalizeAssignment(Model& model, Assignment* assignment) {
  RoutingSearchParameters search_parameters = DefaultRoutingSearchParameters();
  search_parameters.set_solution_limit(1);
  return model.SolveFromAssignmentWithParameters(assignment, search_parameters);
}

TEST(AdaptiveRandomWalkRemovalRuinProcedureTest, RuinApplication) {
  const int kNumNodes = 35;
  const int kNumVehicles = 7;

  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  model.SetArcCostEvaluatorOfAllVehicles(
      model.RegisterTransitCallback([](int64_t from, int64_t to) {
        if (from == 40 && to == 32) {
          return 1000;
        }
        if (from == 24 && to == 25) {
          return 1;
        }
        return 10;
      }));
  model.CloseModel();

  // Route 0: [0] 1 2 3 4 5 6 [41]
  // Route 1: [35] 7 8 9 10 [42]
  // Route 2: [36] 11 12 13 14 15 16 17 [43]
  // Route 3: [37] 18 19 20 [44]
  // Route 4: [38] 21 22 23 24 [45]
  // Route 5: [39] 25 26 27 28 29 30 31 32 [46]
  // Route 6: [40] 33 34 [47]
  const auto [nexts, vehicles] = GenerateSequentialNextsAndVehicles(
      model, /*route_sizes=*/{6, 4, 7, 3, 4, 8, 2});
  const Assignment* reference_solution =
      FinalizeAssignment(model, CreateAssignment(&model, nexts, vehicles));
  ASSERT_TRUE(reference_solution);
  EXPECT_EQ(reference_solution->ObjectiveValue(), 410);

  const int rnd_seed = 0;
  std::mt19937_64 rnd(/*seed=*/rnd_seed);
  const int kNumNeighborsForRouteSelection = kNumNodes;
  const double kStrengtheningFactor = 0.375;
  const double kWeakeningFactor = 0.85;

  AdaptiveRandomWalkRemovalRuinProcedure ruin(
      &model, reference_solution, &rnd, kNumNeighborsForRouteSelection,
      kStrengtheningFactor, kWeakeningFactor);

  const int num_visits = model.Size() - model.vehicles();
  const int base_walk_length =
      num_visits > 0 ? std::ceil(std::log(num_visits)) : 0;

  const int num_iterations = 5;
  for (int iter = 0; iter < num_iterations; ++iter) {
    const Assignment* solution = FinalizeAssignment(
        model, model.solver()->MakeAssignment(reference_solution));
    ASSERT_TRUE(solution);
    EXPECT_EQ(solution->ObjectiveValue(), 410);

    const auto next_accessor = ruin.Ruin(solution);
    EXPECT_EQ(CountNumRemovedVisits(model, *solution, next_accessor),
              base_walk_length + iter);

    // The recreate and local search steps are skipped in this test, and we
    // suppose that the local optimum reached is the same as the reference
    // solution.
    const Assignment* local_optimum = FinalizeAssignment(
        model, model.solver()->MakeAssignment(reference_solution));

    // When OnLocalOptimumReached is called with a solution with a cost too
    // close (based on the strengthening factor) to that of the reference one,
    // the walk lengths of the removed nodes are increased.
    ruin.OnLocalOptimumReached(local_optimum);

    // Reset the random number generator to get the same ruin during the next
    // iteration.
    rnd.seed(rnd_seed);
  }

  // Build a very expensive solution to simulate a local optimum far from the
  // reference solution.
  const Assignment* very_expensive_solution = [&]() {
    Assignment* solution = model.solver()->MakeAssignment(reference_solution);
    solution->SetValue(model.NextVar(40), 32);
    solution->SetValue(model.NextVar(32), 33);
    solution->SetValue(model.NextVar(31), 46);
    return FinalizeAssignment(model, solution);
  }();
  ASSERT_TRUE(very_expensive_solution);
  ASSERT_EQ(very_expensive_solution->ObjectiveValue(), 1400);

  // After 5 iterations generating with a cost too close to the reference one,
  // the walk length is increased by 5 compared to the base walk length.
  const int current_walk_length = base_walk_length + num_iterations;

  for (int iter = 0; iter < num_iterations; ++iter) {
    const Assignment* solution = FinalizeAssignment(
        model, model.solver()->MakeAssignment(reference_solution));
    ASSERT_TRUE(solution);
    EXPECT_EQ(solution->ObjectiveValue(), 410);

    const auto next_accessor = ruin.Ruin(solution);
    EXPECT_EQ(CountNumRemovedVisits(model, *solution, next_accessor),
              current_walk_length - iter);

    // The recreate and local search steps are skipped in this test, and we
    // suppose that the local optimum reached is a very expensive solution.
    const Assignment* local_optimum = FinalizeAssignment(
        model, model.solver()->MakeAssignment(very_expensive_solution));

    // When OnLocalOptimumReached is called with a solution with a cost too
    // far away (based on the weakening factor) from that of the reference one,
    // the walk lengths of the removed nodes are decreased.
    ruin.OnLocalOptimumReached(local_optimum);

    // Reset the random number generator to get the same ruin during the next
    // iteration.
    rnd.seed(rnd_seed);
  }

  // After 5 more iterations generating expensive solutions, the walk length is
  // back to the base walk length.
  const int final_walk_length = base_walk_length;

  // Build a solution which is improving compared to the reference solution.
  const Assignment* improving_solution = [&]() {
    Assignment* solution = model.solver()->MakeAssignment(reference_solution);
    solution->SetValue(model.NextVar(24), 25);
    solution->SetValue(model.NextVar(25), 45);
    solution->SetValue(model.NextVar(39), 26);
    return FinalizeAssignment(model, solution);
  }();
  ASSERT_TRUE(improving_solution);
  EXPECT_EQ(improving_solution->ObjectiveValue(), 401);

  // When OnLocalOptimumReached is called with a solution with a cost better
  // than that of the reference one or with a cost that is not too close neither
  // too far away from the reference one, the walk lengths of the removed nodes
  // are randomly increased or decreased.
  ruin.OnLocalOptimumReached(improving_solution);

  const Assignment* solution = FinalizeAssignment(
      model, model.solver()->MakeAssignment(reference_solution));
  ASSERT_TRUE(solution);
  EXPECT_EQ(solution->ObjectiveValue(), 410);

  const auto next_accessor = ruin.Ruin(solution);
  const int num_removed_visits =
      CountNumRemovedVisits(model, *solution, next_accessor);
  EXPECT_GE(num_removed_visits, final_walk_length - 1);
  EXPECT_LE(num_removed_visits, final_walk_length + 1);
}

// Returns the number of used vehicles.
int CountUsedVehicles(const Model& model, const Assignment& assignment) {
  int count = 0;
  for (int vehicle = 0; vehicle < model.vehicles(); ++vehicle) {
    count += model.Next(assignment, model.Start(vehicle)) != model.End(vehicle);
  }
  return count;
}

// Returns the average route size of non empty routes.
double ComputeAverageNonEmptyRouteSize(const Model& model,
                                       const Assignment& assignment) {
  const int num_used_vehicles = CountUsedVehicles(model, assignment);
  if (num_used_vehicles == 0) return 0;

  const double num_visits = model.Size() - model.vehicles();
  return num_visits / num_used_vehicles;
}

// Returns the number of non start/end nodes served by the given vehicle.
int CountNonStartEndNodes(const Model& model, const Assignment& assignment,
                          int vehicle) {
  const int64_t start = model.Start(vehicle);
  const int64_t end = model.End(vehicle);
  if (assignment.Value(model.NextVar(start)) == end) return 0;

  int size = -1;
  int64_t curr = start;
  while (curr != end) {
    const int64_t next = assignment.Value(model.NextVar(curr));
    ++size;
    curr = next;
  }
  return size;
}

// Returns the maximum route size for the given assignment.
int ComputeMaxRouteSize(const Model& model, const Assignment& assignment) {
  int max_size = 0;
  for (int vehicle = 0; vehicle < model.vehicles(); ++vehicle) {
    max_size =
        std::max(max_size, CountNonStartEndNodes(model, assignment, vehicle));
  }
  return max_size;
}

// Info related to ruined routes and nodes.
struct ExpectedSISRInfo {
  int min_num_ruined_routes;
  int max_num_ruined_routes;
  int min_num_removed_visits_per_route;
  int max_num_removed_visits_per_route;
};

// Computes the expected number of ruined routes and nodes based on the
// given input parameters and on the equations defining the behavior of the SISR
// ruin described in ils.proto.
ExpectedSISRInfo ComputeExpectedSISRInfo(const Model& model,
                                         const Assignment& assignment,
                                         int max_removed_sequence_size,
                                         int avg_num_removed_visits) {
  ExpectedSISRInfo info;

  const double kAvgRouteSize =
      ComputeAverageNonEmptyRouteSize(model, assignment);
  const double kMaxSequenceSize =
      std::min<double>(max_removed_sequence_size, kAvgRouteSize);
  const double kMaxRuinedRoutes =
      (4.0 * avg_num_removed_visits) / (1.0 + kMaxSequenceSize) + 1;
  std::uniform_real_distribution<double> num_ruined_routes_distribution(
      1, kMaxRuinedRoutes + 1);

  info.min_num_ruined_routes = std::floor(num_ruined_routes_distribution.min());
  info.max_num_ruined_routes = std::floor(num_ruined_routes_distribution.max());

  const int kMaxRouteSize = ComputeMaxRouteSize(model, assignment);
  const double kRouteMaxSequenceSize =
      std::min<double>(kMaxRouteSize, kMaxSequenceSize);
  std::uniform_real_distribution<double> num_removed_visits_distribution(
      1, kRouteMaxSequenceSize + 1);

  info.min_num_removed_visits_per_route =
      std::floor(num_removed_visits_distribution.min());
  info.max_num_removed_visits_per_route =
      std::floor(num_removed_visits_distribution.max());

  return info;
}

TEST(SISRRuinProcedureTest, RuinApplication) {
  const int kNumNodes = 35;
  const int kNumVehicles = 7;

  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  model.CloseModel();

  // Route 0: [0] 1 2 3 4 5 6 [41]
  // Route 1: [35] 7 8 9 10 [42]
  // Route 2: [36] 11 12 13 14 15 16 17 [43]
  // Route 3: [37] 18 19 20 [44]
  // Route 4: [38] 21 22 23 24 [45]
  // Route 5: [39] 25 26 27 28 29 30 31 32 [46]
  // Route 6: [40] 33 34 [47]
  const auto [nexts, vehicles] = GenerateSequentialNextsAndVehicles(
      model, /*route_sizes=*/{6, 4, 7, 3, 4, 8, 2});
  const Assignment* assignment = CreateAssignment(&model, nexts, vehicles);

  std::mt19937_64 rnd(/*seed=*/0);
  const int kMaxRemovedSequenceSize = 10;
  const int kAvgNumRemovedVisits = 10;
  const double kBypassFactor = 0.01;

  SISRRuinProcedure ruin(&model, &rnd, kMaxRemovedSequenceSize,
                         kAvgNumRemovedVisits, kBypassFactor,
                         /*num_neighbors=*/kNumNodes);

  ExpectedSISRInfo info = ComputeExpectedSISRInfo(
      model, *assignment, kMaxRemovedSequenceSize, kAvgNumRemovedVisits);

  for (int iter = 0; iter < 10; ++iter) {
    const auto next_accessor = ruin.Ruin(assignment);
    int num_ruined_routes = 0;
    for (const int num_disrupted_visits :
         CountNumRemovedVisitsPerVehicle(model, *assignment, next_accessor)) {
      if (num_disrupted_visits > 0) {
        EXPECT_GE(num_disrupted_visits, info.min_num_removed_visits_per_route);
        EXPECT_LE(num_disrupted_visits, info.max_num_removed_visits_per_route);
        ++num_ruined_routes;
      }
    }
    EXPECT_GE(num_ruined_routes, info.min_num_ruined_routes);
    EXPECT_LE(num_ruined_routes, info.max_num_ruined_routes);
  }
}

TEST(SISRRuinProcedureTest, RuinApplicationWithPickupsAndDeliveries) {
  const int kNumNodes = 9;
  const int kNumVehicles = 2;

  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  model.AddPickupAndDelivery(2, 7);
  model.CloseModel();

  // Route 0: [0] 1 2 3 4 [10]
  // Route 1: [9] 5 6 7 8 [11]
  const auto [nexts, vehicles] =
      GenerateSequentialNextsAndVehicles(model, /*route_sizes=*/{4, 4});
  const Assignment* assignment = CreateAssignment(&model, nexts, vehicles);

  std::mt19937_64 rnd(/*seed=*/0);
  const int kNumNeighborsForRouteSelection = kNumNodes;
  const int kMaxRemovedSequenceSize = 10;
  const int kAvgNumRemovedVisits = 10;
  const double kBypassFactor = 0.01;
  SISRRuinProcedure ruin(&model, &rnd, kMaxRemovedSequenceSize,
                         kAvgNumRemovedVisits, kBypassFactor,
                         kNumNeighborsForRouteSelection);

  for (int iter = 0; iter < 10; ++iter) {
    const auto next_accessor = ruin.Ruin(assignment);
    if (next_accessor(2) == 2 || next_accessor(7) == 7) {
      EXPECT_EQ(next_accessor(7), 7);
      EXPECT_EQ(next_accessor(2), 2);
    }
  }
}

TEST(RuinProcedures, SolutionWithoutPerformedNodes) {
  const int kNumNodes = 9;
  const int kNumVehicles = 2;
  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  std::mt19937_64 rnd(/*seed=*/0);
  const int kRemovedEntities = kNumNodes;
  const int kNumNeighbors = kNumNodes;

  std::vector<std::unique_ptr<RuinProcedure>> ruin_procedures;
  ruin_procedures.push_back(std::make_unique<CloseRoutesRemovalRuinProcedure>(
      &model, &rnd, kRemovedEntities, kNumNeighbors));
  ruin_procedures.push_back(std::make_unique<RandomWalkRemovalRuinProcedure>(
      &model, &rnd, kRemovedEntities, kNumNeighbors));
  const int kMaxRemovedSequenceSize = 10;
  const int kAvgNumRemovedVisits = 10;
  const double kBypassFactor = 0.01;
  ruin_procedures.push_back(std::make_unique<SISRRuinProcedure>(
      &model, &rnd, kMaxRemovedSequenceSize, kAvgNumRemovedVisits,
      kBypassFactor, kNumNeighbors));

  for (const auto& ruin_procedure : ruin_procedures) {
    // Route 0: [0] [10]
    // Route 1: [9] [11]
    const auto [nexts, vehicles] =
        GenerateSequentialNextsAndVehicles(model, /*route_sizes=*/{0, 0});
    const Assignment* assignment = CreateAssignment(&model, nexts, vehicles);

    const auto next_accessor = ruin_procedure->Ruin(assignment);
    ASSERT_EQ(next_accessor(0), 10);
    ASSERT_EQ(next_accessor(9), 11);
  }
}

// Test parameters:
// - bool -- whether to improve the perturbed solution.
class IteratedLocalSearchTest : public ::testing::TestWithParam<bool> {};

TEST_P(IteratedLocalSearchTest, DefaultParameters) {
  const bool improve_perturbed_solution = GetParam();

  const int kNumNodes = 5;
  const int kNumVehicles = 2;

  for (FirstSolutionStrategy::Value first_solution_strategy :
       GetFirstSolutionStrategies()) {
    for (PerturbationStrategy::Value perturbation_strategy :
         GetPerturbationStrategies()) {
      for (const auto& set_reference_solution_acceptance_strategy :
           GetCostBasedAcceptanceStrategySetters()) {
        for (const auto& set_best_solution_acceptance_strategy :
             GetCostBasedAcceptanceStrategySetters()) {
          IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
          Model model(manager);
          model.SetFixedCostOfVehicle(1000, 1);
          model.CloseModel();

          RoutingSearchParameters search_parameters =
              DefaultRoutingSearchParameters();
          *search_parameters.mutable_iterated_local_search_parameters() =
              RecommendedIteratedLocalSearchParameters();

          search_parameters.set_first_solution_strategy(
              first_solution_strategy);
          IteratedLocalSearchParameters* ils =
              search_parameters.mutable_iterated_local_search_parameters();
          ils->set_perturbation_strategy(perturbation_strategy);
          // Remove any default reference solution acceptance strategies.
          ils->mutable_reference_solution_acceptance_policy()
              ->clear_strategies();
          set_reference_solution_acceptance_strategy(
              ils->mutable_reference_solution_acceptance_policy()
                  ->add_strategies());
          // Remove any default best solution acceptance strategies.
          ils->mutable_best_solution_acceptance_policy()->clear_strategies();
          set_best_solution_acceptance_strategy(
              ils->mutable_best_solution_acceptance_policy()->add_strategies());
          ils->set_improve_perturbed_solution(improve_perturbed_solution);

          search_parameters.set_solution_limit(10);

          const Assignment* assignment =
              model.SolveWithIteratedLocalSearch(search_parameters);
          if (first_solution_strategy ==
              FirstSolutionStrategy::ALL_UNPERFORMED) {
            // Nodes are mandatory so ALL_UNPERFORMED will fail to find a
            // solution. This proves the proper strategy was actually called.
            EXPECT_EQ(assignment, nullptr);
            continue;
          }
          if (first_solution_strategy == FirstSolutionStrategy::SWEEP) {
            // SWEEP requires a SweepArranger. The model will be considered as
            // invalid. This proves the proper strategy was actually called.
            EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_INVALID);
            continue;
          }
          ASSERT_NE(assignment, nullptr);
          // We expect all customers to be assigned to route 0 as route 1 has a
          // huge fixed cost.
          for (int64_t route = 0; route < model.vehicles(); ++route) {
            for (int64_t node = model.Start(route); node != model.End(route);
                 node = assignment->Value(model.NextVar(node))) {
              if (!model.IsStart(node)) {
                EXPECT_EQ(assignment->Value(model.VehicleVar(node)), 0);
              }
            }
          }
        }
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(ImprovePerturbedSolutionOnOff, IteratedLocalSearchTest,
                         ::testing::Values(true, false));

class RuinRecreatePerturbation : public ::testing::Test {
 protected:
  static DecisionBuilder* MakeRuinRecreateDecisionBuilder(
      Model& model, IteratedLocalSearchEventManager* event_manager,
      const Assignment* assignment, std::mt19937_64& rnd,
      RuinStrategy ruin_strategy,
      FirstSolutionStrategy::Value recreate_heuristic,
      const std::function<void(RecreateStrategy&)>& setup_recreate_params) {
    return MakeRuinRecreateDecisionBuilder(
        model, event_manager, assignment, rnd, {ruin_strategy},
        RuinCompositionStrategy::UNSET, recreate_heuristic,
        setup_recreate_params);
  }

  static DecisionBuilder* MakeRuinRecreateDecisionBuilder(
      Model& model, IteratedLocalSearchEventManager* event_manager,
      const Assignment* assignment, std::mt19937_64& rnd,
      absl::Span<const RuinStrategy> ruin_strategies,
      RuinCompositionStrategy::Value composition_strategy,
      FirstSolutionStrategy::Value recreate_heuristic,
      const std::function<void(RecreateStrategy&)>& setup_recreate_params) {
    RoutingSearchParameters p = DefaultRoutingSearchParameters();
    *p.mutable_iterated_local_search_parameters() =
        RecommendedIteratedLocalSearchParameters();
    IteratedLocalSearchParameters* ils =
        p.mutable_iterated_local_search_parameters();
    RuinRecreateParameters* rr = ils->mutable_ruin_recreate_parameters();
    EXPECT_EQ(rr->ruin_strategies_size(), 1);
    rr->clear_ruin_strategies();
    for (int ruin_index = 0; ruin_index < ruin_strategies.size();
         ++ruin_index) {
      *rr->add_ruin_strategies() = ruin_strategies[ruin_index];
    }
    if (ruin_strategies.size() > 1) {
      rr->set_ruin_composition_strategy(composition_strategy);
    }
    rr->mutable_recreate_strategy()->set_heuristic(recreate_heuristic);
    DCHECK(setup_recreate_params);
    setup_recreate_params(*rr->mutable_recreate_strategy());

    return MakePerturbationDecisionBuilder(
        p, &model, event_manager, &rnd, assignment,
        /*stop_search=*/[]() { return false; },
        /*filter_manager=*/nullptr);
  }
};

// Test parameters:
// - int -- number of ruined routes or nodes according to the ruin strategy.
class RuinRecreatePerturbation_RemoveRoutes
    : public RuinRecreatePerturbation,
      public ::testing::WithParamInterface<int> {};

TEST_P(RuinRecreatePerturbation_RemoveRoutes, Solve) {
  const int num_ruined_routes = GetParam();

  const int kNumNodes = 5;
  const int kNumVehicles = 2;

  RuinStrategy ruin_strategy;
  ruin_strategy.mutable_spatially_close_routes()->set_num_ruined_routes(
      num_ruined_routes);

  for (FirstSolutionStrategy::Value recreate_strategy :
       GetRecreateStrategies()) {
    for (const auto& recreate_params_setup :
         GetRecreateParametersSetupFunctions(recreate_strategy)) {
      IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
      Model model(manager);
      model.SetFixedCostOfVehicle(1000, 1);
      model.CloseModel();

      IteratedLocalSearchEventManager event_manager;

      // Route 0: [0] 1 2 [6]
      // Route 1: [5] 3 4 [7]
      const auto [nexts, vehicles] =
          GenerateSequentialNextsAndVehicles(model, /*route_sizes=*/{2, 2});

      const Assignment* assignment = CreateAssignment(&model, nexts, vehicles);

      std::mt19937_64 rnd(/*seed=*/0);
      DecisionBuilder* ruin_and_recreate_db = MakeRuinRecreateDecisionBuilder(
          model, &event_manager, assignment, rnd, ruin_strategy,
          recreate_strategy, recreate_params_setup);

      std::vector<SearchMonitor*> monitors;
      SolutionCollector* collect_assignments =
          model.solver()->MakeLastSolutionCollector(assignment);
      monitors.push_back(collect_assignments);

      model.solver()->Solve(ruin_and_recreate_db, monitors);
      EXPECT_EQ(collect_assignments->solution_count(), 1);

      const Assignment* result_assignment = collect_assignments->solution(0);

      if (num_ruined_routes == 0) {
        // No ruined routes, resulting assignment should be the same as
        // the input one.
        for (const auto& next : model.Nexts()) {
          EXPECT_EQ(assignment->Value(next), result_assignment->Value(next));
        }
        for (const auto& vehicle : model.VehicleVars()) {
          EXPECT_EQ(assignment->Value(vehicle),
                    result_assignment->Value(vehicle));
        }
      } else if (num_ruined_routes > 1) {
        // Since the solution has two routes, and the second vehicle
        // has a huge fixed cost, when we are removing more than a
        // route, all customers should end up in the first vehicle.
        for (int node = 0; node < model.Size(); ++node) {
          if (model.IsStart(node)) continue;
          EXPECT_EQ(result_assignment->Value(model.VehicleVar(node)), 0);
        }
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(NumberOfRemovedRoutes,
                         RuinRecreatePerturbation_RemoveRoutes,
                         ::testing::Values(0, 1, 2, 3, 4));

// Test parameters:
// - int -- number of removed nodes according to the ruin strategy.
class RuinRecreatePerturbation_RemoveNodes
    : public RuinRecreatePerturbation,
      public ::testing::WithParamInterface<int> {};

TEST_P(RuinRecreatePerturbation_RemoveNodes, Solve) {
  const int num_removed_nodes = GetParam();

  const int kNumNodes = 5;
  const int kNumVehicles = 2;

  RuinStrategy ruin_strategy;
  ruin_strategy.mutable_random_walk()->set_num_removed_visits(
      num_removed_nodes);

  for (FirstSolutionStrategy::Value recreate_strategy :
       GetRecreateStrategies()) {
    for (const auto& recreate_params_setup :
         GetRecreateParametersSetupFunctions(recreate_strategy)) {
      IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
      Model model(manager);
      model.SetFixedCostOfVehicle(1000, 1);
      model.CloseModel();

      IteratedLocalSearchEventManager event_manager;

      // Route 0: [0] 1 2 [6]
      // Route 1: [5] 3 4 [7]
      const auto [nexts, vehicles] =
          GenerateSequentialNextsAndVehicles(model, /*route_sizes=*/{2, 2});

      const Assignment* assignment = CreateAssignment(&model, nexts, vehicles);

      std::mt19937_64 rnd(/*seed=*/0);
      DecisionBuilder* ruin_and_recreate_db = MakeRuinRecreateDecisionBuilder(
          model, &event_manager, assignment, rnd, ruin_strategy,
          recreate_strategy, recreate_params_setup);

      std::vector<SearchMonitor*> monitors;
      SolutionCollector* collect_assignments =
          model.solver()->MakeLastSolutionCollector(assignment);
      monitors.push_back(collect_assignments);

      model.solver()->Solve(ruin_and_recreate_db, monitors);
      if ((recreate_strategy == FirstSolutionStrategy::SAVINGS ||
           recreate_strategy == FirstSolutionStrategy::PARALLEL_SAVINGS) &&
          collect_assignments->solution_count() == 0) {
        // Currently savings-based recreate may fail to create a feasible
        // solution in certain scenarios. This is a temporary workaround
        // for this issue.
        // TODO(user): investigate this issue.
        continue;
      }
      ASSERT_EQ(collect_assignments->solution_count(), 1);

      const Assignment* result_assignment = collect_assignments->solution(0);

      if (num_removed_nodes == 0) {
        // No removed nodes, resulting assignment should be the same as
        // the input one.
        for (const auto& next : model.Nexts()) {
          EXPECT_EQ(assignment->Value(next), result_assignment->Value(next));
        }
        for (const auto& vehicle : model.VehicleVars()) {
          EXPECT_EQ(assignment->Value(vehicle),
                    result_assignment->Value(vehicle));
        }
      } else if (num_removed_nodes == 4) {
        // All nodes should be served by the first vehicle as the second one has
        // a huge fixed cost.
        for (int node = 0; node < model.Size(); ++node) {
          if (model.IsStart(node)) continue;
          EXPECT_EQ(result_assignment->Value(model.VehicleVar(node)), 0);
        }
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(NumberOfRemovedNodes,
                         RuinRecreatePerturbation_RemoveNodes,
                         ::testing::Values(0, 1, 2, 3, 4));

// Test parameters:
// - int -- max number of removed sequence size.
// - int -- average number of removed visits.
// - double -- bypass factor.
class RuinRecreatePerturbation_SISR
    : public RuinRecreatePerturbation,
      public ::testing::WithParamInterface<std::tuple<int, int, double>> {};

TEST_P(RuinRecreatePerturbation_SISR, Solve) {
  const auto [max_removed_sequence_size, avg_num_removed_visits,
              bypass_factor] = GetParam();

  const int kNumNodes = 5;
  const int kNumVehicles = 2;

  RuinStrategy ruin_strategy;
  ruin_strategy.mutable_sisr()->set_max_removed_sequence_size(
      max_removed_sequence_size);
  // Set avg_num_removed_visits to be the same to avoid complexity in this test.
  ruin_strategy.mutable_sisr()->set_avg_num_removed_visits(
      avg_num_removed_visits);
  ruin_strategy.mutable_sisr()->set_bypass_factor(bypass_factor);

  for (FirstSolutionStrategy::Value recreate_strategy :
       GetRecreateStrategies()) {
    for (const auto& recreate_params_setup :
         GetRecreateParametersSetupFunctions(recreate_strategy)) {
      IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
      Model model(manager);
      model.CloseModel();

      IteratedLocalSearchEventManager event_manager;

      // Route 0: [0] 1 2 [6]
      // Route 1: [5] 3 4 [7]
      const auto [nexts, vehicles] =
          GenerateSequentialNextsAndVehicles(model, /*route_sizes=*/{2, 2});

      const Assignment* assignment = CreateAssignment(&model, nexts, vehicles);

      std::mt19937_64 rnd(/*seed=*/0);
      DecisionBuilder* ruin_and_recreate_db = MakeRuinRecreateDecisionBuilder(
          model, &event_manager, assignment, rnd, ruin_strategy,
          recreate_strategy, recreate_params_setup);

      std::vector<SearchMonitor*> monitors;
      SolutionCollector* collect_assignments =
          model.solver()->MakeLastSolutionCollector(assignment);
      monitors.push_back(collect_assignments);

      model.solver()->Solve(ruin_and_recreate_db, monitors);
      if ((recreate_strategy == FirstSolutionStrategy::SAVINGS ||
           recreate_strategy == FirstSolutionStrategy::PARALLEL_SAVINGS) &&
          collect_assignments->solution_count() == 0) {
        // Currently savings-based recreate may fail to create a feasible
        // solution in certain scenarios. This is a temporary workaround
        // for this issue.
        // TODO(user): investigate this issue.
        continue;
      }
      ASSERT_EQ(collect_assignments->solution_count(), 1);

      const Assignment* result_assignment = collect_assignments->solution(0);
      EXPECT_TRUE(result_assignment);

      // All nodes should be served by some vehicle.
      for (int node = 0; node < model.Size(); ++node) {
        if (model.IsStart(node)) continue;
        EXPECT_NE(result_assignment->Value(model.VehicleVar(node)), -1);
        EXPECT_NE(result_assignment->Value(model.NextVar(node)), node);
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(SISRParameters, RuinRecreatePerturbation_SISR,
                         ::testing::Combine(::testing::Values(2, 3, 4),
                                            ::testing::Values(2, 3, 4),
                                            ::testing::Values(0.0, 0.1)));

// Test parameters:
// - double -- strengthening factor.
// - double -- weakening factor.
class RuinRecreatePerturbation_AdaptiveRandomWalk
    : public RuinRecreatePerturbation,
      public ::testing::WithParamInterface<std::tuple<double, double>> {};

TEST_P(RuinRecreatePerturbation_AdaptiveRandomWalk, Solve) {
  const auto [strengthening_factor, weakening_factor] = GetParam();

  const int kNumNodes = 5;
  const int kNumVehicles = 2;

  RuinStrategy ruin_strategy;
  ruin_strategy.mutable_adaptive_random_walk()->set_strengthening_factor(
      strengthening_factor);
  ruin_strategy.mutable_adaptive_random_walk()->set_weakening_factor(
      weakening_factor);

  for (FirstSolutionStrategy::Value recreate_strategy :
       GetRecreateStrategies()) {
    for (const auto& recreate_params_setup :
         GetRecreateParametersSetupFunctions(recreate_strategy)) {
      IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
      Model model(manager);
      model.SetFixedCostOfVehicle(1000, 1);
      model.CloseModel();

      IteratedLocalSearchEventManager event_manager;

      // Route 0: [0] 1 2 [6]
      // Route 1: [5] 3 4 [7]
      const auto [nexts, vehicles] =
          GenerateSequentialNextsAndVehicles(model, /*route_sizes=*/{2, 2});

      const Assignment* assignment = CreateAssignment(&model, nexts, vehicles);

      std::mt19937_64 rnd(/*seed=*/0);
      DecisionBuilder* ruin_and_recreate_db = MakeRuinRecreateDecisionBuilder(
          model, &event_manager, assignment, rnd, ruin_strategy,
          recreate_strategy, recreate_params_setup);

      std::vector<SearchMonitor*> monitors;
      SolutionCollector* collect_assignments =
          model.solver()->MakeLastSolutionCollector(assignment);
      monitors.push_back(collect_assignments);

      model.solver()->Solve(ruin_and_recreate_db, monitors);
      if ((recreate_strategy == FirstSolutionStrategy::SAVINGS ||
           recreate_strategy == FirstSolutionStrategy::PARALLEL_SAVINGS) &&
          collect_assignments->solution_count() == 0) {
        // Currently savings-based recreate may fail to create a feasible
        // solution in certain scenarios. This is a temporary workaround
        // for this issue.
        // TODO(user): investigate this issue.
        continue;
      }
      ASSERT_EQ(collect_assignments->solution_count(), 1);

      const Assignment* result_assignment = collect_assignments->solution(0);

      // We don't verify node assignment strictly because the walk length is
      // adaptive and randomized, but we do ensure the decision builder
      // successfully recreates a valid solution.
      EXPECT_TRUE(result_assignment);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(AdaptiveRandomWalkParameters,
                         RuinRecreatePerturbation_AdaptiveRandomWalk,
                         ::testing::Values(std::make_tuple(0.5, 1.0),
                                           std::make_tuple(0.7, 1.4)));

// Returns the supported ruin composition strategies.
std::vector<RuinCompositionStrategy::Value> GetRuinCompositionStrategies() {
  return {RuinCompositionStrategy::RUN_ALL_SEQUENTIALLY,
          RuinCompositionStrategy::RUN_ALL_RANDOMLY,
          RuinCompositionStrategy::RUN_ONE_RANDOMLY};
}

// Test parameters:
// - int -- number of removed entities.
class RuinRecreatePerturbation_CompositeRuin
    : public RuinRecreatePerturbation,
      public ::testing::WithParamInterface<int> {};

TEST_P(RuinRecreatePerturbation_CompositeRuin, Solve) {
  const int num_removed_entities = GetParam();

  const int kNumNodes = 5;
  const int kNumVehicles = 2;

  RuinStrategy ruin_strategy1;
  ruin_strategy1.mutable_random_walk()->set_num_removed_visits(
      num_removed_entities);

  RuinStrategy ruin_strategy2;
  ruin_strategy2.mutable_spatially_close_routes()->set_num_ruined_routes(
      num_removed_entities);

  for (FirstSolutionStrategy::Value recreate_strategy :
       GetRecreateStrategies()) {
    for (const auto& recreate_params_setup :
         GetRecreateParametersSetupFunctions(recreate_strategy)) {
      for (RuinCompositionStrategy::Value composition_strategy :
           GetRuinCompositionStrategies()) {
        for (const std::vector<RuinStrategy>& ruin_strategies :
             std::vector<std::vector<RuinStrategy>>{
                 {ruin_strategy1, ruin_strategy2},
                 {ruin_strategy2, ruin_strategy1}}) {
          IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
          Model model(manager);
          model.SetFixedCostOfVehicle(1000, 1);
          model.CloseModel();

          IteratedLocalSearchEventManager event_manager;

          // Route 0: [0] 1 2 [6]
          // Route 1: [5] 3 4 [7]
          const auto [nexts, vehicles] =
              GenerateSequentialNextsAndVehicles(model, /*route_sizes=*/{2, 2});

          const Assignment* assignment =
              CreateAssignment(&model, nexts, vehicles);

          std::mt19937_64 rnd(/*seed=*/0);
          DecisionBuilder* ruin_and_recreate_db =
              MakeRuinRecreateDecisionBuilder(
                  model, &event_manager, assignment, rnd, ruin_strategies,
                  composition_strategy, recreate_strategy,
                  recreate_params_setup);

          std::vector<SearchMonitor*> monitors;
          SolutionCollector* collect_assignments =
              model.solver()->MakeLastSolutionCollector(assignment);
          monitors.push_back(collect_assignments);

          model.solver()->Solve(ruin_and_recreate_db, monitors);
          if ((recreate_strategy == FirstSolutionStrategy::SAVINGS ||
               recreate_strategy == FirstSolutionStrategy::PARALLEL_SAVINGS) &&
              collect_assignments->solution_count() == 0) {
            // Currently savings-based recreate may fail to create a feasible
            // solution in certain scenarios. This is a temporary workaround
            // for this issue.
            // TODO(user): investigate this issue.
            continue;
          }
          ASSERT_EQ(collect_assignments->solution_count(), 1);

          const Assignment* result_assignment =
              collect_assignments->solution(0);

          if (num_removed_entities == 0) {
            // No removed nodes, resulting assignment should be the same as
            // the input one.
            for (const auto& next : model.Nexts()) {
              EXPECT_EQ(assignment->Value(next),
                        result_assignment->Value(next));
            }
            for (const auto& vehicle : model.VehicleVars()) {
              EXPECT_EQ(assignment->Value(vehicle),
                        result_assignment->Value(vehicle));
            }
          } else if (num_removed_entities >= 2 &&
                     composition_strategy !=
                         RuinCompositionStrategy::RUN_ONE_RANDOMLY) {
            // All nodes should be served by the first vehicle as the second one
            // has a huge fixed cost.
            for (int node = 0; node < model.Size(); ++node) {
              if (model.IsStart(node)) continue;
              EXPECT_EQ(result_assignment->Value(model.VehicleVar(node)), 0);
            }
          }
        }
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(NumberOfRemovedEntities,
                         RuinRecreatePerturbation_CompositeRuin,
                         ::testing::Values(0, 1, 2, 3, 4));

TEST(InvalidInstanceTest, NoNodesOrVehicles) {
  const int kNumNodes = 0;
  const int kNumVehicles = 0;

  IndexManager manager(kNumNodes, kNumVehicles,
                       /*starts=*/{}, /*ends=*/{});
  Model model(manager);

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  *parameters.mutable_iterated_local_search_parameters() =
      RecommendedIteratedLocalSearchParameters();

  const Assignment* assignment = model.SolveWithIteratedLocalSearch(parameters);
  ASSERT_EQ(assignment, nullptr);
}

// Returns the available cooling schedule strategies.
std::vector<CoolingScheduleStrategy::Value> GetCoolingScheduleStrategies() {
  return {CoolingScheduleStrategy::EXPONENTIAL,
          CoolingScheduleStrategy::LINEAR};
}

TEST(NeighborAcceptanceCriteria, GreedyDescent) {
  const int kNumNodes = 5;
  const int kNumVehicles = 2;

  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  model.SetArcCostEvaluatorOfAllVehicles(
      model.RegisterTransitCallback([](int64_t from, int64_t to) {
        if (from == 2 && to == 3) {
          return 1;
        }
        return 2;
      }));
  model.CloseModel();

  IteratedLocalSearchEventManager event_manager;

  // Route 0: [0] 1 2 [6]
  // Route 1: [5] 3 4 [7]
  const auto [nexts, vehicles] =
      GenerateSequentialNextsAndVehicles(model, /*route_sizes=*/{2, 2});

  const Assignment* reference =
      FinalizeAssignment(model, CreateAssignment(&model, nexts, vehicles));
  ASSERT_TRUE(reference);

  Assignment* candidate = model.solver()->MakeAssignment(reference);
  // Use arc (2, 3) that is cheaper.
  candidate->SetValue(model.NextVar(0), 6);
  candidate->SetValue(model.NextVar(5), 1);
  candidate->SetValue(model.NextVar(2), 3);
  // Route 0: [0] [6]
  // Route 1: [5] 1 2 3 4 [7]
  const Assignment* finalized_candidate = FinalizeAssignment(model, candidate);
  ASSERT_TRUE(finalized_candidate);

  AcceptancePolicy acceptance_policy;
  acceptance_policy.add_strategies()->mutable_greedy_descent();

  NeighborAcceptanceCriterion::SearchState final_search_state = {
      /*duration=*/absl::Milliseconds(10), /*solutions=*/10};
  std::mt19937_64 rnd(/*seed=*/0);

  std::unique_ptr<NeighborAcceptanceCriterion> acceptance_criterion =
      MakeNeighborAcceptanceCriterion(
          model, &event_manager, acceptance_policy, final_search_state, &rnd,
          /*initial_best_solution_cost=*/std::numeric_limits<int64_t>::max());

  NeighborAcceptanceCriterion::SearchState current_search_state = {
      /*duration=*/absl::Milliseconds(5), /*solutions=*/5};

  EXPECT_TRUE(acceptance_criterion->Accept(current_search_state,
                                           finalized_candidate, reference));
  EXPECT_FALSE(acceptance_criterion->Accept(current_search_state, reference,
                                            finalized_candidate));
}

TEST(NeighborAcceptanceCriteria, GreedyDescentWithLateAcceptance) {
  const int kNumNodes = 5;
  const int kNumVehicles = 2;

  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  model.SetArcCostEvaluatorOfAllVehicles(
      model.RegisterTransitCallback([](int64_t from, int64_t to) {
        if (from == 2 && to == 3) {
          return 1;
        }
        if (from == 4 && to == 1) {
          return 2;
        }
        if (from == 1 && to == 3) {
          return 7;
        }
        return 3;
      }));
  model.CloseModel();

  IteratedLocalSearchEventManager event_manager;

  // Build a solution with cost 18.
  const Assignment* solutionWithCost18 = [&]() {
    // Route 0: [0] 1 2 [6]
    // Route 1: [5] 3 4 [7]
    const auto [nexts, vehicles] =
        GenerateSequentialNextsAndVehicles(model, /*route_sizes=*/{2, 2});
    return FinalizeAssignment(model, CreateAssignment(&model, nexts, vehicles));
  }();
  ASSERT_TRUE(solutionWithCost18);
  ASSERT_EQ(solutionWithCost18->ObjectiveValue(), 18);

  // Build a solution with cost 14.
  const Assignment* solutionWithCost14 = [&]() {
    Assignment* solution = model.solver()->MakeAssignment(solutionWithCost18);
    // Use arc (4, 1) that is cheaper.
    solution->SetValue(model.NextVar(0), 6);
    solution->SetValue(model.NextVar(4), 1);
    solution->SetValue(model.NextVar(2), 7);
    // Route 0: [0] [6]
    // Route 1: [5] 3 4 1 2 [7]
    return FinalizeAssignment(model, solution);
  }();
  ASSERT_TRUE(solutionWithCost14);
  ASSERT_EQ(solutionWithCost14->ObjectiveValue(), 14);

  // Build a solution with cost 13.
  const Assignment* solutionWithCost13 = [&]() {
    Assignment* solution = model.solver()->MakeAssignment(solutionWithCost18);
    // Use arc (2, 3) that is cheaper.
    solution->SetValue(model.NextVar(0), 6);
    solution->SetValue(model.NextVar(5), 1);
    solution->SetValue(model.NextVar(2), 3);
    // Route 0: [0] [6]
    // Route 1: [5] 1 2 3 4 [7]
    return FinalizeAssignment(model, solution);
  }();
  ASSERT_TRUE(solutionWithCost13);
  ASSERT_EQ(solutionWithCost13->ObjectiveValue(), 13);

  // Build a solution with cost 19.
  const Assignment* solutionWithCost19 = [&]() {
    Assignment* solution = model.solver()->MakeAssignment(solutionWithCost18);
    // Use arc (1, 3) that is more expensive.
    solution->SetValue(model.NextVar(1), 3);
    solution->SetValue(model.NextVar(4), 2);
    solution->SetValue(model.NextVar(5), 7);
    // Route 0: [0] 1 3 4 2 [6]
    // Route 1: [5] [7]
    return FinalizeAssignment(model, solution);
  }();
  ASSERT_TRUE(solutionWithCost19);
  ASSERT_EQ(solutionWithCost19->ObjectiveValue(), 19);

  for (int late_acceptance_window : {0, 1, 2, 3, 10, 100}) {
    AcceptancePolicy acceptance_policy;
    acceptance_policy.add_strategies()
        ->mutable_greedy_descent()
        ->set_late_acceptance_window(late_acceptance_window);

    NeighborAcceptanceCriterion::SearchState final_search_state = {
        /*duration=*/absl::Milliseconds(10), /*solutions=*/10};
    std::mt19937_64 rnd(/*seed=*/0);

    std::unique_ptr<NeighborAcceptanceCriterion> acceptance_criterion =
        MakeNeighborAcceptanceCriterion(
            model, &event_manager, acceptance_policy, final_search_state, &rnd,
            /*initial_best_solution_cost=*/std::numeric_limits<int64_t>::max());

    NeighborAcceptanceCriterion::SearchState current_search_state = {
        /*duration=*/absl::Milliseconds(5), /*solutions=*/5};

    // Fill the history with cost 18 solutions.
    for (int i = 0; i < late_acceptance_window; ++i) {
      EXPECT_TRUE(acceptance_criterion->Accept(
          current_search_state, /*candidate=*/solutionWithCost18,
          /*reference=*/solutionWithCost18));
    }

    // Fill the history except the last element with cost 13 solutions.
    for (int i = 0; i < late_acceptance_window - 1; ++i) {
      EXPECT_TRUE(acceptance_criterion->Accept(
          current_search_state, /*candidate=*/solutionWithCost13,
          /*reference=*/solutionWithCost13));
    }

    if (late_acceptance_window > 0) {
      // Try to insert a cost 14 solution by using solutionWithCost13 as
      // reference. This should be accepted as the associated history cost
      // is 18.
      EXPECT_TRUE(
          acceptance_criterion->Accept(current_search_state,
                                       /*candidate=*/solutionWithCost14,
                                       /*reference=*/solutionWithCost13));
    } else {
      // Without late acceptance, the solution with cost 14 will not be
      // accepted.
      EXPECT_FALSE(
          acceptance_criterion->Accept(current_search_state,
                                       /*candidate=*/solutionWithCost14,
                                       /*reference=*/solutionWithCost13));
    }

    // Solutions with a cost of 18 will no longer be accepted if not improving
    // the reference one.
    for (int i = 0; i < late_acceptance_window; ++i) {
      EXPECT_FALSE(acceptance_criterion->Accept(
          current_search_state, /*candidate=*/solutionWithCost18,
          /*reference=*/solutionWithCost18));
    }

    // But a solution with cost 18 will still be accepted if improving the
    // reference one.
    EXPECT_TRUE(acceptance_criterion->Accept(current_search_state,
                                             /*candidate=*/solutionWithCost18,
                                             /*reference=*/solutionWithCost19));
  }
}

TEST(NeighborAcceptanceCriteria, SimulatedAnnealing) {
  const int kNumNodes = 5;
  const int kNumVehicles = 2;

  for (CoolingScheduleStrategy::Value cooling_schedule :
       GetCoolingScheduleStrategies()) {
    IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
    Model model(manager);
    model.SetArcCostEvaluatorOfAllVehicles(
        model.RegisterTransitCallback([](int64_t from, int64_t to) {
          if (from == 2 && to == 3) {
            return 10;
          }
          return 1;
        }));
    model.CloseModel();

    IteratedLocalSearchEventManager event_manager;

    // Route 0: [0] 1 2 [6]
    // Route 1: [5] 3 4 [7]
    const auto [nexts, vehicles] =
        GenerateSequentialNextsAndVehicles(model, /*route_sizes=*/{2, 2});

    const Assignment* reference =
        FinalizeAssignment(model, CreateAssignment(&model, nexts, vehicles));
    ASSERT_TRUE(reference);

    Assignment* candidate = model.solver()->MakeAssignment(reference);
    // Move all nodes to Route 1.
    candidate->SetValue(model.NextVar(0), 6);
    candidate->SetValue(model.NextVar(5), 1);
    candidate->SetValue(model.NextVar(2), 3);
    // Route 0: [0] [6]
    // Route 1: [5] 1 2 3 4 [7]
    const Assignment* finalized_candidate =
        FinalizeAssignment(model, candidate);
    ASSERT_TRUE(finalized_candidate);

    AcceptancePolicy acceptance_policy;
    SimulatedAnnealingAcceptanceStrategy* sa =
        acceptance_policy.add_strategies()->mutable_simulated_annealing();
    sa->set_initial_temperature(100);
    sa->set_final_temperature(0.01);
    sa->set_cooling_schedule_strategy(cooling_schedule);

    NeighborAcceptanceCriterion::SearchState final_search_state = {
        /*duration=*/absl::Milliseconds(10), /*solutions=*/10};
    std::mt19937_64 rnd(/*seed=*/0);

    std::unique_ptr<NeighborAcceptanceCriterion> acceptance_criterion =
        MakeNeighborAcceptanceCriterion(
            model, &event_manager, acceptance_policy, final_search_state, &rnd,
            /*initial_best_solution_cost=*/std::numeric_limits<int64_t>::max());

    EXPECT_GT(finalized_candidate->ObjectiveValue(),
              reference->ObjectiveValue());

    // During early search stages, worsening neighbors are likely accepted.
    NeighborAcceptanceCriterion::SearchState early_search_state = {
        /*duration=*/absl::Milliseconds(1), /*solutions=*/1};
    EXPECT_TRUE(acceptance_criterion->Accept(early_search_state,
                                             finalized_candidate, reference));

    // During late search stages, worsening neighbors are unlikely accepted.
    NeighborAcceptanceCriterion::SearchState late_search_state = {
        /*duration=*/absl::Milliseconds(9), /*solutions=*/9};
    EXPECT_FALSE(acceptance_criterion->Accept(late_search_state,
                                              finalized_candidate, reference));
  }
}

TEST(NeighborAcceptanceCriteria, AllNodesPerformed) {
  const int kNumNodes = 7;
  const int kNumVehicles = 2;
  const int kDisjunctionPenalty = 1000;

  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  // Both 2 and 5 should be performed to satisfy this disjunction.
  model.AddDisjunction({2, 5}, kDisjunctionPenalty, /*max_cardinality=*/2);
  // One between 3 and 6 should be performed to satisfy this disjunction.
  model.AddDisjunction({3, 6}, kDisjunctionPenalty, /*max_cardinality=*/1);
  model.CloseModel();

  IteratedLocalSearchEventManager event_manager;

  // Route 0: [0] 1 2 3 [8]
  // Route 1: [7] 4 5 6 [9]
  const auto [nexts, vehicles] =
      GenerateSequentialNextsAndVehicles(model, /*route_sizes=*/{3, 3});
  // Route 0: [0] 1 2 3 [8]
  // Route 1: [7] 4 5 [9]
  Assignment* assignment = CreateAssignment(&model, nexts, vehicles);
  assignment->SetValue(model.NextVar(6), 6);
  assignment->SetValue(model.NextVar(5), 9);

  AcceptancePolicy acceptance_policy;
  acceptance_policy.add_strategies()->mutable_all_nodes_performed();

  NeighborAcceptanceCriterion::SearchState final_search_state = {
      /*duration=*/absl::Milliseconds(10), /*solutions=*/10};
  std::mt19937_64 rnd(/*seed=*/0);

  std::unique_ptr<NeighborAcceptanceCriterion> acceptance_criterion =
      MakeNeighborAcceptanceCriterion(
          model, &event_manager, acceptance_policy, final_search_state, &rnd,
          /*initial_best_solution_cost=*/std::numeric_limits<int64_t>::max());

  NeighborAcceptanceCriterion::SearchState any_search_state = {
      /*duration=*/absl::Milliseconds(5), /*solutions=*/5};

  // All customers are performed.
  EXPECT_TRUE(acceptance_criterion->Accept(any_search_state,
                                           /*candidate=*/assignment,
                                           /*reference=*/assignment));
  {
    Assignment* candidate = model.solver()->MakeAssignment(assignment);
    // Make node 1 unperformed.
    candidate->SetValue(model.NextVar(0), 2);
    candidate->SetValue(model.NextVar(1), 1);
    // The assignment is not accepted because node 1 is not part of any
    // disjunction.
    EXPECT_FALSE(acceptance_criterion->Accept(any_search_state,
                                              /*candidate=*/candidate,
                                              /*reference=*/candidate));
  }

  {
    Assignment* candidate = model.solver()->MakeAssignment(assignment);
    // Make node 4 unperformed.
    candidate->SetValue(model.NextVar(7), 5);
    candidate->SetValue(model.NextVar(4), 4);
    // The assignment is not accepted because node 4 is not part of any
    // disjunction.
    EXPECT_FALSE(acceptance_criterion->Accept(any_search_state,
                                              /*candidate=*/candidate,
                                              /*reference=*/candidate));
  }

  {
    Assignment* candidate = model.solver()->MakeAssignment(assignment);
    // Make node 2 unperformed.
    candidate->SetValue(model.NextVar(1), 3);
    candidate->SetValue(model.NextVar(2), 2);
    // The assignment is not accepted as node 2 is part of a disjunction with
    // a max cardinality of 2.
    EXPECT_FALSE(acceptance_criterion->Accept(any_search_state,
                                              /*candidate=*/candidate,
                                              /*reference=*/candidate));
  }

  {
    Assignment* candidate = model.solver()->MakeAssignment(assignment);
    // Make node 5 unperformed.
    candidate->SetValue(model.NextVar(4), 6);
    candidate->SetValue(model.NextVar(5), 5);
    // The assignment is not accepted as node 5 is part of a disjunction with
    // a max cardinality of 2.
    EXPECT_FALSE(acceptance_criterion->Accept(any_search_state,
                                              /*candidate=*/candidate,
                                              /*reference=*/candidate));
  }

  {
    Assignment* candidate = model.solver()->MakeAssignment(assignment);
    // Make node 3 unperformed.
    candidate->SetValue(model.NextVar(2), 4);
    candidate->SetValue(model.NextVar(3), 3);
    // The assignment is not accepted because with both nodes 3 and 6
    // unperformed, the disjunction on {3, 6} with max_cardinality=1 is
    // violated.
    EXPECT_FALSE(acceptance_criterion->Accept(any_search_state,
                                              /*candidate=*/candidate,
                                              /*reference=*/candidate));
    // Insert 6 where 3 was.
    // The assignment is as node 6 is part of a disjunction with a
    // max cardinality of 1.
    candidate->SetValue(model.NextVar(2), 6);
    candidate->SetValue(model.NextVar(6), 4);
    EXPECT_TRUE(acceptance_criterion->Accept(any_search_state,
                                             /*candidate=*/candidate,
                                             /*reference=*/candidate));
  }
}

TEST(NeighborAcceptanceCriteria, MoreNodesPerformed) {
  const int kNumNodes = 7;
  const int kNumVehicles = 2;
  const int kDisjunctionPenalty = 1000;

  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  // One between 3 and 6 should be performed to satisfy this disjunction.
  model.AddDisjunction({3, 6}, kDisjunctionPenalty, /*max_cardinality=*/1);
  model.CloseModel();

  IteratedLocalSearchEventManager event_manager;

  // Route 0: [0] 1 2 3 [8]
  // Route 1: [7] 4 5 6 [9]
  const auto [nexts, vehicles] =
      GenerateSequentialNextsAndVehicles(model, /*route_sizes=*/{3, 3});

  Assignment* assignment = CreateAssignment(&model, nexts, vehicles);

  AcceptancePolicy acceptance_policy;
  acceptance_policy.add_strategies()->mutable_more_nodes_performed();

  NeighborAcceptanceCriterion::SearchState final_search_state = {
      /*duration=*/absl::Milliseconds(10), /*solutions=*/10};
  std::mt19937_64 rnd(/*seed=*/0);

  std::unique_ptr<NeighborAcceptanceCriterion> acceptance_criterion =
      MakeNeighborAcceptanceCriterion(
          model, &event_manager, acceptance_policy, final_search_state, &rnd,
          /*initial_best_solution_cost=*/std::numeric_limits<int64_t>::max());

  NeighborAcceptanceCriterion::SearchState any_search_state = {
      /*duration=*/absl::Milliseconds(5), /*solutions=*/5};

  {
    Assignment* reference = model.solver()->MakeAssignment(assignment);
    // Make node 3 unperformed.
    reference->SetValue(model.NextVar(2), 8);
    reference->SetValue(model.NextVar(3), 3);

    Assignment* candidate = model.solver()->MakeAssignment(assignment);
    // Make node 6 unperformed.
    candidate->SetValue(model.NextVar(5), 9);
    candidate->SetValue(model.NextVar(6), 6);

    // The candidate assignment is not accepted because it performs the same
    // number of nodes as the reference assignment.
    EXPECT_FALSE(
        acceptance_criterion->Accept(any_search_state, candidate, reference));
  }

  {
    Assignment* reference = model.solver()->MakeAssignment(assignment);

    Assignment* candidate = model.solver()->MakeAssignment(assignment);
    // Make node 6 unperformed.
    candidate->SetValue(model.NextVar(5), 9);
    candidate->SetValue(model.NextVar(6), 6);

    // The candidate assignment is not accepted because it performs fewer
    // nodes than the reference assignment.
    EXPECT_FALSE(
        acceptance_criterion->Accept(any_search_state, candidate, reference));
  }

  {
    Assignment* reference = model.solver()->MakeAssignment(assignment);
    // Make node 3 unperformed.
    reference->SetValue(model.NextVar(2), 8);
    reference->SetValue(model.NextVar(3), 3);

    Assignment* candidate = model.solver()->MakeAssignment(assignment);

    // The candidate assignment is accepted because it performs more nodes
    // than the reference assignment.
    EXPECT_TRUE(
        acceptance_criterion->Accept(any_search_state, candidate, reference));
  }
}

void TestAbsencesBasedAcceptanceCriterion(
    const bool remove_route_with_lowest_absences) {
  const int kNumNodes = 7;
  const int kNumVehicles = 2;
  const int kDisjunctionPenalty = 1000;

  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  // One between 3 and 6 should be performed to satisfy this disjunction.
  model.AddDisjunction({3, 6}, kDisjunctionPenalty, /*max_cardinality=*/1);
  model.CloseModel();

  IteratedLocalSearchEventManager event_manager;

  // Route 0: [0] 1 2 3 [8]
  // Route 1: [7] 4 5 6 [9]
  const auto [nexts, vehicles] =
      GenerateSequentialNextsAndVehicles(model, /*route_sizes=*/{3, 3});

  const Assignment* assignment = CreateAssignment(&model, nexts, vehicles);

  AcceptancePolicy acceptance_policy;
  acceptance_policy.add_strategies()
      ->mutable_absences_based()
      ->set_remove_route_with_lowest_absences(
          remove_route_with_lowest_absences);

  NeighborAcceptanceCriterion::SearchState final_search_state = {
      /*duration=*/absl::Milliseconds(10), /*solutions=*/10};
  std::mt19937_64 rnd(/*seed=*/0);

  std::unique_ptr<NeighborAcceptanceCriterion> acceptance_criterion =
      MakeNeighborAcceptanceCriterion(
          model, &event_manager, acceptance_policy, final_search_state, &rnd,
          /*initial_best_solution_cost=*/std::numeric_limits<int64_t>::max());

  NeighborAcceptanceCriterion::SearchState any_search_state = {
      /*duration=*/absl::Milliseconds(5), /*solutions=*/5};

  {
    Assignment* reference = model.solver()->MakeAssignment(assignment);

    // Trigger the OnIterationEnd callback.
    // No absences counter is increased as all reference solution nodes are
    // performed.
    acceptance_criterion->OnIterationEnd(reference);

    Assignment* candidate = model.solver()->MakeAssignment(assignment);

    // Current state:
    // - All absences counters are 0.
    //
    // The candidate assignment is not accepted because sum(absences of
    // candidate) and sum(absences of reference) both are 0.
    EXPECT_FALSE(
        acceptance_criterion->Accept(any_search_state, candidate, reference));

    // Trigger the OnBestSolutionFound callback
    // - no-op when remove_route_with_lowest_absences is false
    // - the reference assignment is updated when
    //   remove_route_with_lowest_absences is true.
    Assignment* expected_assignment = model.solver()->MakeAssignment(reference);
    if (remove_route_with_lowest_absences) {
      // Route 0 is removed from the reference assignment
      // - sum(absences of route 0) = 0
      // - sum(absences of route 1) = 0
      // We take the first route with the lowest absences counter, which is
      // route 0.
      for (int64_t node : {1, 2, 3}) {
        expected_assignment->SetValue(model.VehicleVar(node), -1);
        expected_assignment->SetValue(model.NextVar(node), node);
      }
      expected_assignment->SetValue(model.NextVar(0), 8);
    }
    acceptance_criterion->OnBestSolutionFound(reference);
    EXPECT_EQ(*reference, *expected_assignment);
  }

  {
    Assignment* reference = model.solver()->MakeAssignment(assignment);
    // Make node 3 unperformed.
    reference->SetValue(model.NextVar(2), 8);
    reference->SetValue(model.NextVar(3), 3);

    // Trigger the OnIterationEnd callback.
    // Absences counter for node 3 is increased.
    acceptance_criterion->OnIterationEnd(reference);

    Assignment* candidate = model.solver()->MakeAssignment(assignment);

    // Current state:
    // - Absences counter for node 3 is 1.
    // - All other absences counters are 0.
    //
    // The candidate assignment is accepted because sum(absences of
    // candidate) is 0 and sum(absences of reference) is 1.
    EXPECT_TRUE(
        acceptance_criterion->Accept(any_search_state, candidate, reference));

    // Make node 3 performed again before testing the OnBestSolutionFound
    // callback.
    reference->SetValue(model.NextVar(2), 3);
    reference->SetValue(model.NextVar(3), 8);

    // Trigger the OnBestSolutionFound callback
    // - no-op when remove_route_with_lowest_absences is false
    // - the reference assignment is updated when
    //   remove_route_with_lowest_absences is true.
    Assignment* expected_assignment = model.solver()->MakeAssignment(reference);
    if (remove_route_with_lowest_absences) {
      // Route 1 is removed from the reference assignment
      // - sum(absences of route 0) = 1
      // - sum(absences of route 1) = 0
      // We take the first route with the lowest absences counter, which is
      // route 1.
      for (int64_t node : {4, 5, 6}) {
        expected_assignment->SetValue(model.VehicleVar(node), -1);
        expected_assignment->SetValue(model.NextVar(node), node);
      }
      expected_assignment->SetValue(model.NextVar(7), 9);
    }
    acceptance_criterion->OnBestSolutionFound(reference);
    EXPECT_EQ(*reference, *expected_assignment);
  }

  {
    Assignment* reference = model.solver()->MakeAssignment(assignment);

    // Trigger the OnIterationEnd callback.
    // No absences counter is increased as all reference solution nodes are
    // performed.
    acceptance_criterion->OnIterationEnd(reference);

    Assignment* candidate = model.solver()->MakeAssignment(assignment);
    // Make node 3 unperformed.
    candidate->SetValue(model.NextVar(2), 8);
    candidate->SetValue(model.NextVar(3), 3);

    // Current state:
    // - Absences counter for node 3 is 1.
    // - All other absences counters are 0.
    //
    // The candidate assignment is not accepted because sum(absences of
    // candidate) is 1 and sum(absences of reference) is 0.
    EXPECT_FALSE(
        acceptance_criterion->Accept(any_search_state, candidate, reference));

    // Trigger the OnBestSolutionFound callback
    // - no-op when remove_route_with_lowest_absences is false
    // - the reference assignment is updated when
    //   remove_route_with_lowest_absences is true.
    Assignment* expected_assignment = model.solver()->MakeAssignment(reference);
    if (remove_route_with_lowest_absences) {
      // Route 1 is removed from the reference assignment
      // - sum(absences of route 0) is 1
      // - sum(absences of route 1) is 0
      // We take the first route with the lowest absences counter, which is
      // route 1.
      for (int64_t node : {4, 5, 6}) {
        expected_assignment->SetValue(model.VehicleVar(node), -1);
        expected_assignment->SetValue(model.NextVar(node), node);
      }
      expected_assignment->SetValue(model.NextVar(7), 9);
    }
    acceptance_criterion->OnBestSolutionFound(reference);
    EXPECT_EQ(*reference, *expected_assignment);
  }
}

TEST(NeighborAcceptanceCriteria,
     AbsencesBasedAcceptanceCriterion_DoNotRemoveRouteWithLowestAbsences) {
  TestAbsencesBasedAcceptanceCriterion(
      /*remove_route_with_lowest_absences=*/false);
}

TEST(NeighborAcceptanceCriteria,
     AbsencesBasedAcceptanceCriterion_RemoveRouteWithLowestAbsences) {
  TestAbsencesBasedAcceptanceCriterion(
      /*remove_route_with_lowest_absences=*/true);
}

TEST(NeighborAcceptanceCriteria, BooleanOrAcceptanceCriterion) {
  const int kNumNodes = 7;
  const int kNumVehicles = 2;
  const int kDisjunctionPenalty = 1000;

  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  // One between 3 and 6 should be performed to satisfy this disjunction.
  model.AddDisjunction({3, 6}, kDisjunctionPenalty, /*max_cardinality=*/1);
  model.SetArcCostEvaluatorOfAllVehicles(
      model.RegisterTransitCallback([](int64_t from, int64_t to) {
        if (from == 2 && to == 3) {
          return 10;
        }
        return 1;
      }));
  model.CloseModel();

  IteratedLocalSearchEventManager event_manager;

  // Route 0: [0] 1 2 3 [8]
  // Route 1: [7] 4 5 6 [9]
  const auto [nexts, vehicles] =
      GenerateSequentialNextsAndVehicles(model, /*route_sizes=*/{3, 3});

  Assignment* reference = CreateAssignment(&model, nexts, vehicles);

  // Route 0: [0] 1 2 [8]
  // Route 1: [7] 4 5 3 [9]
  Assignment* candidate = CreateAssignment(&model, nexts, vehicles);
  // Node 3 is moved to route 1.
  candidate->SetValue(model.NextVar(2), 8);
  // Node 3 is performed in route 1 after node 5.
  candidate->SetValue(model.NextVar(5), 3);
  candidate->SetValue(model.NextVar(3), 9);
  candidate->SetValue(model.VehicleVar(3), 1);
  // Node 6 is unperformed.
  candidate->SetValue(model.NextVar(6), 6);
  candidate->SetValue(model.VehicleVar(6), -1);

  const std::vector<std::function<void(AcceptanceStrategy&)>>
      acceptance_strategy_setter_list = {
          [](AcceptanceStrategy& acceptance_strategy) {
            acceptance_strategy.mutable_greedy_descent();
          },
          [](AcceptanceStrategy& acceptance_strategy) {
            SimulatedAnnealingAcceptanceStrategy* sa =
                acceptance_strategy.mutable_simulated_annealing();
            sa->set_cooling_schedule_strategy(CoolingScheduleStrategy::LINEAR);
            sa->set_automatic_temperatures(true);
          },
          [](AcceptanceStrategy& acceptance_strategy) {
            acceptance_strategy.mutable_all_nodes_performed();
          },
          [](AcceptanceStrategy& acceptance_strategy) {
            acceptance_strategy.mutable_more_nodes_performed();
          },
          [](AcceptanceStrategy& acceptance_strategy) {
            ThresholdAcceptingDeterministicAnnealingAcceptanceStrategy* ta =
                acceptance_strategy
                    .mutable_threshold_accepting_deterministic_annealing();
            ta->set_cooling_schedule_strategy(CoolingScheduleStrategy::LINEAR);
          },
          [](AcceptanceStrategy& acceptance_strategy) {
            acceptance_strategy
                .mutable_record_to_record_travel_deterministic_annealing();
          },
          [](AcceptanceStrategy& acceptance_strategy) {
            acceptance_strategy.mutable_absences_based();
          }};

  NeighborAcceptanceCriterion::SearchState final_search_state = {
      /*duration=*/absl::Milliseconds(10), /*solutions=*/10};
  NeighborAcceptanceCriterion::SearchState any_search_state = {
      /*duration=*/absl::Milliseconds(5), /*solutions=*/5};
  std::mt19937_64 rnd(/*seed=*/0);

  for (const auto& setter1 : acceptance_strategy_setter_list) {
    for (const auto& setter2 : acceptance_strategy_setter_list) {
      AcceptancePolicy acceptance_policy1;
      setter1(*acceptance_policy1.add_strategies());
      AcceptancePolicy acceptance_policy2;
      setter2(*acceptance_policy2.add_strategies());

      std::unique_ptr<NeighborAcceptanceCriterion> criterion1 =
          MakeNeighborAcceptanceCriterion(
              model, &event_manager, acceptance_policy1, final_search_state,
              &rnd, /*initial_best_solution_cost=*/
              std::numeric_limits<int64_t>::max());
      std::unique_ptr<NeighborAcceptanceCriterion> criterion2 =
          MakeNeighborAcceptanceCriterion(
              model, &event_manager, acceptance_policy2, final_search_state,
              &rnd, /*initial_best_solution_cost=*/
              std::numeric_limits<int64_t>::max());

      // Trigger callbacks for updating any internal state.
      criterion1->OnIterationEnd(reference);
      criterion2->OnIterationEnd(reference);
      criterion1->OnBestSolutionFound(reference);
      criterion2->OnBestSolutionFound(reference);

      AcceptancePolicy combined_acceptance_policy;
      combined_acceptance_policy.set_composition(AcceptancePolicy::OR);
      setter1(*combined_acceptance_policy.add_strategies());
      setter2(*combined_acceptance_policy.add_strategies());

      std::unique_ptr<NeighborAcceptanceCriterion> boolean_or_criterion =
          MakeNeighborAcceptanceCriterion(
              model, &event_manager, combined_acceptance_policy,
              final_search_state, &rnd, /*initial_best_solution_cost=*/
              std::numeric_limits<int64_t>::max());

      // Trigger callbacks for updating any internal state.
      boolean_or_criterion->OnIterationEnd(reference);
      boolean_or_criterion->OnBestSolutionFound(reference);

      EXPECT_EQ(
          boolean_or_criterion->Accept(any_search_state, candidate, reference),
          criterion1->Accept(any_search_state, candidate, reference) ||
              criterion2->Accept(any_search_state, candidate, reference));
    }
  }
}

TEST(CoolingScheduleTest, CoolingScheduleTest) {
  const int kNumNodes = 5;
  const int kNumVehicles = 2;

  for (CoolingScheduleStrategy::Value cooling_schedule :
       GetCoolingScheduleStrategies()) {
    for (const bool automatic_temperatures : {true, false}) {
      IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
      Model model(manager);
      model.SetFixedCostOfVehicle(1000, 1);
      model.CloseModel();

      RoutingSearchParameters search_parameters =
          DefaultRoutingSearchParameters();
      *search_parameters.mutable_iterated_local_search_parameters() =
          RecommendedIteratedLocalSearchParameters();
      search_parameters.set_solution_limit(10);
      IteratedLocalSearchParameters* ils =
          search_parameters.mutable_iterated_local_search_parameters();
      // Remove any default acceptance strategies as we are testing this
      // aspect.
      ils->mutable_reference_solution_acceptance_policy()->clear_strategies();
      SimulatedAnnealingAcceptanceStrategy* sa_acceptance_strategy =
          ils->mutable_reference_solution_acceptance_policy()
              ->add_strategies()
              ->mutable_simulated_annealing();
      sa_acceptance_strategy->set_cooling_schedule_strategy(cooling_schedule);
      sa_acceptance_strategy->set_automatic_temperatures(
          automatic_temperatures);
      if (!automatic_temperatures) {
        sa_acceptance_strategy->set_initial_temperature(100.0);
        sa_acceptance_strategy->set_final_temperature(0.01);
      }

      const Assignment* assignment =
          model.SolveWithIteratedLocalSearch(search_parameters);
      ASSERT_NE(assignment, nullptr);
      // We expect all customers to be assigned to route 0 as route 1 has a
      // huge fixed cost.
      for (int64_t route = 0; route < model.vehicles(); ++route) {
        for (int64_t node = model.Start(route); node != model.End(route);
             node = assignment->Value(model.NextVar(node))) {
          if (!model.IsStart(node)) {
            EXPECT_EQ(assignment->Value(model.VehicleVar(node)), 0);
          }
        }
      }
    }
  }
}

TEST(ThresholdAcceptingCoolingScheduleTest, CoolingScheduleTest) {
  const int kNumNodes = 5;
  const int kNumVehicles = 2;

  for (CoolingScheduleStrategy::Value cooling_schedule :
       GetCoolingScheduleStrategies()) {
    IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
    Model model(manager);
    model.SetFixedCostOfVehicle(1000, 1);
    model.CloseModel();

    RoutingSearchParameters search_parameters =
        DefaultRoutingSearchParameters();
    *search_parameters.mutable_iterated_local_search_parameters() =
        RecommendedIteratedLocalSearchParameters();
    search_parameters.set_solution_limit(10);
    IteratedLocalSearchParameters* ils =
        search_parameters.mutable_iterated_local_search_parameters();
    ils->mutable_reference_solution_acceptance_policy()->clear_strategies();
    ThresholdAcceptingDeterministicAnnealingAcceptanceStrategy*
        ta_acceptance_strategy =
            ils->mutable_reference_solution_acceptance_policy()
                ->add_strategies()
                ->mutable_threshold_accepting_deterministic_annealing();
    ta_acceptance_strategy->set_cooling_schedule_strategy(cooling_schedule);
    ta_acceptance_strategy->set_initial_threshold(100.0);
    ta_acceptance_strategy->set_final_threshold(0.01);

    const Assignment* assignment =
        model.SolveWithIteratedLocalSearch(search_parameters);
    ASSERT_NE(assignment, nullptr);
    for (int64_t route = 0; route < model.vehicles(); ++route) {
      for (int64_t node = model.Start(route); node != model.End(route);
           node = assignment->Value(model.NextVar(node))) {
        if (!model.IsStart(node)) {
          EXPECT_EQ(assignment->Value(model.VehicleVar(node)), 0);
        }
      }
    }
  }
}

TEST(SimulatedAnnealingTemperatures, GetSimulatedAnnealingTemperatures) {
  const int kNumNodes = 5;
  const int kNumVehicles = 2;

  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  model.SetArcCostEvaluatorOfAllVehicles(
      model.RegisterTransitCallback(ArcCost2(&model)));
  model.CloseModel();

  RoutingSearchParameters search_parameters = DefaultRoutingSearchParameters();
  *search_parameters.mutable_iterated_local_search_parameters() =
      RecommendedIteratedLocalSearchParameters();
  IteratedLocalSearchParameters* ils_parameters =
      search_parameters.mutable_iterated_local_search_parameters();
  // Remove any default acceptance strategies as we are testing this aspect.
  ils_parameters->mutable_reference_solution_acceptance_policy()
      ->clear_strategies();
  SimulatedAnnealingAcceptanceStrategy* sa_acceptance_strategy =
      ils_parameters->mutable_reference_solution_acceptance_policy()
          ->add_strategies()
          ->mutable_simulated_annealing();

  std::mt19937_64 rnd(/*seed=*/0);

  const double kInitialTemperature = 100.0;
  const double kFinalTemperature = 0.01;
  sa_acceptance_strategy->set_initial_temperature(kInitialTemperature);
  sa_acceptance_strategy->set_final_temperature(kFinalTemperature);

  sa_acceptance_strategy->set_automatic_temperatures(false);
  const std::pair<double, double> user_defined_temperatures =
      GetSimulatedAnnealingTemperatures(model, *sa_acceptance_strategy, &rnd);
  ASSERT_EQ(std::get<0>(user_defined_temperatures), kInitialTemperature);
  ASSERT_EQ(std::get<1>(user_defined_temperatures), kFinalTemperature);

  sa_acceptance_strategy->set_automatic_temperatures(true);
  const std::pair<double, double> automatic_temperatures =
      GetSimulatedAnnealingTemperatures(model, *sa_acceptance_strategy, &rnd);
  ASSERT_TRUE(
      MathUtil::AlmostEquals(std::get<0>(automatic_temperatures), 0.08));
  ASSERT_TRUE(
      MathUtil::AlmostEquals(std::get<1>(automatic_temperatures), 0.0008));
}

TEST(SimulatedAnnealingTemperatures,
     GetAutomaticSimulatedAnnealingTemperaturesZeroVehicles) {
  const int kNumNodes = 5;
  const int kNumVehicles = 0;

  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  model.CloseModel();

  RoutingSearchParameters search_parameters = DefaultRoutingSearchParameters();
  *search_parameters.mutable_iterated_local_search_parameters() =
      RecommendedIteratedLocalSearchParameters();
  IteratedLocalSearchParameters* ils_parameters =
      search_parameters.mutable_iterated_local_search_parameters();
  // Remove any default acceptance strategies as we are testing this aspect.
  ils_parameters->mutable_reference_solution_acceptance_policy()
      ->clear_strategies();
  SimulatedAnnealingAcceptanceStrategy* sa_acceptance_strategy =
      ils_parameters->mutable_reference_solution_acceptance_policy()
          ->add_strategies()
          ->mutable_simulated_annealing();

  std::mt19937_64 rnd(/*seed=*/0);

  sa_acceptance_strategy->set_automatic_temperatures(true);
  const std::pair<double, double> automatic_temperatures =
      GetSimulatedAnnealingTemperatures(model, *sa_acceptance_strategy, &rnd);
  ASSERT_EQ(std::get<0>(automatic_temperatures), 0.0);
  ASSERT_EQ(std::get<1>(automatic_temperatures), 0.0);
}

TEST(IteratedLocalSearchTest, HintSupport) {
  const int kNumNodes = 5;
  const int kNumVehicles = 2;

  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  RoutingSearchParameters search_parameters = DefaultRoutingSearchParameters();
  *search_parameters.mutable_iterated_local_search_parameters() =
      RecommendedIteratedLocalSearchParameters();
  // Use a strategy that fails to find a solution from scratch.
  search_parameters.set_first_solution_strategy(
      FirstSolutionStrategy::ALL_UNPERFORMED);
  // Limit to 1 solution so ILS returns the hint without modifying it.
  search_parameters.set_solution_limit(1);

  // Without hint, ALL_UNPERFORMED fails to find any solution.
  {
    Model model_no_hint(manager);
    model_no_hint.CloseModel();
    const Assignment* assignment =
        model_no_hint.SolveWithIteratedLocalSearch(search_parameters);
    EXPECT_EQ(assignment, nullptr);
  }

  // With hint, the solver should succeed and return a solution where all
  // nodes are performed. This would fail if CopyIntersection were skipped,
  // since ALL_UNPERFORMED would leave all nodes unperformed.
  {
    Model model_with_hint(manager);
    model_with_hint.CloseModel();
    const auto vars = GenerateSequentialNextsAndVehicles(
        model_with_hint, /*route_sizes=*/{2, 2});
    const Assignment* hint =
        CreateAssignment(&model_with_hint, vars.nexts, vars.vehicles);
    ASSERT_NE(hint, nullptr);

    model_with_hint.SetFirstSolutionHint(hint);
    const Assignment* assignment =
        model_with_hint.SolveWithIteratedLocalSearch(search_parameters);
    ASSERT_NE(assignment, nullptr);
    // Verify all nodes are performed (not self-loops).
    for (int i = 0; i < model_with_hint.Size(); ++i) {
      if (model_with_hint.IsStart(i) || model_with_hint.IsEnd(i)) continue;
      EXPECT_NE(assignment->Value(model_with_hint.NextVar(i)), i)
          << "Node " << i << " is unperformed";
    }
  }
}

TEST(IteratedLocalSearchTest, InfeasibleHintSupportFallback) {
  const int kNumNodes = 5;
  const int kNumVehicles = 2;

  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  RoutingSearchParameters search_parameters = DefaultRoutingSearchParameters();
  *search_parameters.mutable_iterated_local_search_parameters() =
      RecommendedIteratedLocalSearchParameters();
  // Use a strategy that succeeds to find a solution from scratch.
  search_parameters.set_first_solution_strategy(
      FirstSolutionStrategy::PATH_CHEAPEST_ARC);
  search_parameters.set_solution_limit(1);

  // With infeasible hint, it should fallback to solve_db_ and succeed.
  {
    Model model_with_hint(manager);
    model_with_hint.CloseModel();
    // Create an infeasible hint (node 4 is unperformed and mandatory).
    // Route 0: [0] 1 [6]
    // Route 1: [5] 2 3 [7]
    // Node 4 is unperformed.
    const auto vars = GenerateSequentialNextsAndVehicles(
        model_with_hint, /*route_sizes=*/{1, 2});
    const Assignment* hint =
        CreateAssignment(&model_with_hint, vars.nexts, vars.vehicles);
    ASSERT_NE(hint, nullptr);

    model_with_hint.SetFirstSolutionHint(hint);
    const Assignment* assignment =
        model_with_hint.SolveWithIteratedLocalSearch(search_parameters);
    ASSERT_NE(assignment, nullptr);
    // Verify all nodes are performed (since fallback succeeded and nodes are
    // mandatory).
    for (int i = 0; i < model_with_hint.Size(); ++i) {
      if (model_with_hint.IsStart(i) || model_with_hint.IsEnd(i)) continue;
      EXPECT_NE(assignment->Value(model_with_hint.NextVar(i)), i)
          << "Node " << i << " is unperformed";
    }
  }
}

TEST(IteratedLocalSearchTest, ZeroVisitsModel) {
  // A model with exactly as many vehicles as nodes, implying 0 visits.
  const int kNumNodes = 2;
  const int kNumVehicles = 2;
  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  model.CloseModel();

  RoutingSearchParameters search_parameters = DefaultRoutingSearchParameters();
  *search_parameters.mutable_iterated_local_search_parameters() =
      RecommendedIteratedLocalSearchParameters();
  search_parameters.set_solution_limit(10);

  // This should not crash (previously crashed with SIGILL due to std::log(0)).
  const Assignment* assignment =
      model.SolveWithIteratedLocalSearch(search_parameters);
  EXPECT_NE(assignment, nullptr);
}

TEST(IteratedLocalSearchTest, UnconstrainedObjectiveModel) {
  const int kNumNodes = 5;
  const int kNumVehicles = 2;
  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  const int transit_callback_index = model.RegisterTransitCallback(
      [](int64_t /*from_index*/, int64_t /*to_index*/) { return 1; });
  model.AddDimension(transit_callback_index, 100, 100, false, "Span");
  model.GetMutableDimension("Span")->SetGlobalSpanCostCoefficient(10);

  model.CloseModel();

  RoutingSearchParameters search_parameters = DefaultRoutingSearchParameters();
  *search_parameters.mutable_iterated_local_search_parameters() =
      RecommendedIteratedLocalSearchParameters();
  // Disable improve_perturbed_solution to force acceptance criteria
  // to evaluate the perturbed solution directly. This requires the finalizer
  // to run on the perturbed solution to bind the objective.
  search_parameters.mutable_iterated_local_search_parameters()
      ->set_improve_perturbed_solution(false);
  search_parameters.set_solution_limit(10);

  // This should not crash (previously crashed with Check failed: min_ == max_).
  const Assignment* assignment =
      model.SolveWithIteratedLocalSearch(search_parameters);
  EXPECT_NE(assignment, nullptr);
}

}  // namespace
}  // namespace operations_research::routing

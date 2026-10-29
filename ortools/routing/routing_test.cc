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

#include "ortools/routing/routing.h"

#include <algorithm>
#include <bitset>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "google/protobuf/duration.pb.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/path.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/timer.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/assignment.pb.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/interval.h"
#include "ortools/constraint_solver/search_stats.pb.h"
#include "ortools/graph/bounded_dijkstra.h"
#include "ortools/routing/enums.pb.h"
#include "ortools/routing/heuristic_parameters.pb.h"
#include "ortools/routing/ils.pb.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/lp_scheduling_test_utils.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/parameters_utils.h"
#include "ortools/routing/routing_test_utils.h"
#include "ortools/routing/search.h"
#include "ortools/routing/types.h"
#include "ortools/routing/utils.h"
#include "ortools/util/optional_boolean.pb.h"
#include "ortools/util/piecewise_linear_function.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/testing_utils.h"

ABSL_DECLARE_FLAG(bool, cp_name_cast_variables);
ABSL_FLAG(int64_t, routing_test_num_neighbors, 1000,
          "The maximal number of neighbors tested in the local search "
          "operator test.");

namespace operations_research::routing {
namespace {

using ::google::protobuf::EnumerateEnumValues;
using ::testing::ContainerEq;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::EqualsProto;
using ::testing::FieldsAre;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using Attributes = Model::ResourceGroup::Attributes;
using ResourceGroup = Model::ResourceGroup;
using VisitTypePolicy = Model::VisitTypePolicy;

absl::flat_hash_set<int> GetNodesInDisjunctionWithMaxCardinality(
    const Model& model, int64_t node_index, int64_t max_cardinality) {
  absl::flat_hash_set<int> nodes;
  for (const DisjunctionIndex disjunction :
       model.GetDisjunctionIndices(node_index)) {
    if (model.GetDisjunctionMaxCardinality(disjunction) != max_cardinality) {
      continue;
    }
    for (const int64_t node : model.GetDisjunctionNodeIndices(disjunction)) {
      nodes.insert(node);
    }
  }
  return nodes;
}

TEST(ModelTest, Model) {
  const int kNodes = 10;
  const int kVehicles = 5;
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  ASSERT_NE(model.solver(), nullptr);
  EXPECT_EQ(kNodes + kVehicles - 1, model.Size());
  EXPECT_EQ(kNodes, model.nodes());
  EXPECT_EQ(kVehicles, model.vehicles());
  IntVar* const* nexts = model.Nexts().data();
  IntVar* const* vehicle_vars = model.VehicleVars().data();
  for (int i = 0; i < model.Size(); ++i) {
    EXPECT_EQ(nexts[i], model.NextVar(i));
    EXPECT_EQ(vehicle_vars[i], model.VehicleVar(i));
    EXPECT_EQ(0, model.ActiveVar(i)->Min());
    EXPECT_EQ(1, model.ActiveVar(i)->Max());
  }
}

int64_t ValueIfDifferent(const IndexManager& manager, int64_t value, int64_t i,
                         int64_t j) {
  return manager.IndexToNode(i) == manager.IndexToNode(j) ? 0 : value;
}

TEST(ModelTest, DimensionVariableAccessors) {
  const int kNodes = 10;
  const int kVehicles = 5;
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  const int callback = model.RegisterTransitCallback(
      absl::bind_front(ValueIfDifferent, manager, 1));
  model.AddDimension(callback, kint64max, kint64max, true, "time");
  Dimension* const dimension = model.GetMutableDimension("time");
  // Set Cumul variables.
  dimension->SetCumulVarRange(2, 3, 7);
  dimension->SetCumulVarRange(4, 5, 5);
  dimension->SetCumulVarRange(6, 6, 4);
  // Test that we are getting what we set.
  EXPECT_EQ(dimension->GetCumulVarMin(2), 3);
  EXPECT_EQ(dimension->GetCumulVarMax(2), 7);
  EXPECT_EQ(dimension->GetCumulVarMin(4), 5);
  EXPECT_EQ(dimension->GetCumulVarMax(4), 5);
  EXPECT_EQ(dimension->GetCumulVarMin(6), 6);
  EXPECT_EQ(dimension->GetCumulVarMax(6), 4);
}

// Model with 2 nodes to visit (1 and 2), with cumul(1) <= cumul(2).
// Forcing 2 to be before 1 causes a slow propagation loop between the dimension
// (path) constraint and the <= constraint.
TEST(RoutingRegressionTest, SlowPropagationLoop) {
  IndexManager manager(3, 1, NodeIndex(0));
  Model model(manager);
  const int callback = model.RegisterTransitCallback(
      absl::bind_front(ValueIfDifferent, manager, 1));
  model.AddDimension(callback, kint64max, kint64max, true, "stop_count");
  Dimension* const dimension = model.GetMutableDimension("stop_count");
  Solver* const solver = model.solver();
  solver->AddConstraint(
      solver->MakeLessOrEqual(dimension->CumulVar(1), dimension->CumulVar(2)));
  Assignment* const assignment = solver->MakeAssignment();
  assignment->Add(model.NextVar(0))->SetValue(2);
  assignment->Add(model.NextVar(2))->SetValue(1);
  assignment->Add(model.NextVar(1))->SetValue(3);
  model.CloseModel();
  EXPECT_FALSE(solver->Solve(solver->MakeRestoreAssignment(assignment)));
}

TEST(ModelTest, LocalOptimumNotReached) {
  IndexManager manager(2, 2, NodeIndex(0));
  Model model(manager);
  model.AddDisjunction({1}, /*penalty=*/2000);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::ALL_UNPERFORMED);
  parameters.set_solution_limit(1);
  EXPECT_NE(model.SolveWithParameters(parameters), nullptr);
  EXPECT_EQ(
      model.status(),
      RoutingSearchStatus::ROUTING_PARTIAL_SUCCESS_LOCAL_OPTIMUM_NOT_REACHED);
}

TEST(ModelTest, NoFirstSolutionLocalOptimumNotReached) {
  const int kNumNonStartEndNodes = 250;
  const int kPenalty = 2000;
  const IndexManager manager(kNumNonStartEndNodes + 1, 1, NodeIndex(0));

  Model model(manager);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();

  for (int node = 0; node < model.Size(); ++node) {
    model.AddDisjunction({node}, kPenalty);
  }
  parameters.set_first_solution_strategy(FirstSolutionStrategy::SAVINGS);
  parameters.set_solution_limit(1);

  // Compute the average time it takes to find a first solution.
  const int kSampleCount = 5;
  CycleTimer timer;
  for (int i = 0; i < kSampleCount; ++i) {
    timer.Start();
    const Assignment* solution = model.SolveWithParameters(parameters);
    timer.Stop();
    ASSERT_NE(solution, nullptr) << i;
  }
  // Set a very small time limit (1 tenth of the average time to find a first
  // solution) to try and prevent the first solution heuristic from finding a
  // solution.
  int32_t time_limit_nanos =
      absl::ToInt64Nanoseconds(timer.GetDuration()) / kSampleCount / 10;
  parameters.mutable_time_limit()->set_nanos(time_limit_nanos);

  const int kNumAttempts = 50;
  int num_attempts_no_solution;
  auto test_has_all_unperformed_solution = [&]() {
    int num_attempts_all_unperformed = 0;
    num_attempts_no_solution = 0;
    for (int i = 0; i < kNumAttempts; ++i) {
      const Assignment* solution = model.SolveWithParameters(parameters);
      if (solution == nullptr) {
        ++num_attempts_no_solution;
        continue;
      }
      if (solution->ObjectiveValue() == kPenalty * kNumNonStartEndNodes) {
        ++num_attempts_all_unperformed;
        EXPECT_EQ(model.status(),
                  RoutingSearchStatus::
                      ROUTING_PARTIAL_SUCCESS_LOCAL_OPTIMUM_NOT_REACHED);
      }
    }
    return num_attempts_all_unperformed > 0;
  };

  bool success = false;
  for (int retry = 0; retry < 10; ++retry) {
    if (test_has_all_unperformed_solution()) {
      success = true;
      break;
    }
    // We adapt the time limit before trying again, to make sure that the test
    // actually exercises the case where the first solution doesn't have time to
    // come up with a solution.
    if (num_attempts_no_solution > 3 * kNumAttempts / 4) {
      time_limit_nanos *= 2;
    } else {
      time_limit_nanos /= 2;
    }
    parameters.mutable_time_limit()->set_nanos(time_limit_nanos);
  }
  EXPECT_TRUE(success)
      << "Failed to find an all-unperformed solution, increase the number of "
         "time limit adjustment retries.";
}

TEST(ModelTest, LocalOptimumReached) {
  IndexManager manager(2, 2, NodeIndex(0));
  Model model(manager);
  model.AddDisjunction({1}, /*penalty=*/2000);
  model.SetFixedCostOfAllVehicles(1);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::ALL_UNPERFORMED);
  EXPECT_NE(model.SolveWithParameters(parameters), nullptr);
  EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_SUCCESS);
}

TEST(ModelTest, LocalOptimumReachedWithSolutions) {
  IndexManager manager(2, 2, NodeIndex(0));
  Model model(manager);
  model.AddDisjunction({1}, /*penalty=*/2000);
  model.SetFixedCostOfAllVehicles(1);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::ALL_UNPERFORMED);
  parameters.set_number_of_solutions_to_collect(2);
  std::vector<const Assignment*> solutions;
  EXPECT_NE(model.SolveWithParameters(parameters, &solutions), nullptr);
  EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_SUCCESS);
}

TEST(ModelTest, LocalOptimiumReachedNoLSOperatorReset) {
  for (bool use_metaheuristic : {true, false}) {
    SCOPED_TRACE(absl::StrCat("use_metaheuristic: ", use_metaheuristic));
    IndexManager manager(2, 2, NodeIndex(0));
    Model model(manager);
    model.AddDisjunction({1}, /*penalty=*/2000);
    const int callback1 =
        model.RegisterTransitCallback([](int64_t, int64_t) { return 1; });
    const int callback2 =
        model.RegisterTransitCallback([](int64_t, int64_t) { return 2; });
    model.SetArcCostEvaluatorOfVehicle(callback1, 0);
    model.SetArcCostEvaluatorOfVehicle(callback2, 1);
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(
        FirstSolutionStrategy::ALL_UNPERFORMED);
    if (use_metaheuristic) {
      parameters.set_local_search_metaheuristic(
          LocalSearchMetaheuristic::GUIDED_LOCAL_SEARCH);
      // Any limit large enough is ok.
      parameters.set_solution_limit(50);
    }
    parameters.set_log_search(true);
    // As of 2023/11, the following operators are not reset. They are disabled
    // to make sure all operators are locally optimal after the first search.
    RoutingSearchParameters::LocalSearchNeighborhoodOperators* ops =
        parameters.mutable_local_search_operators();
    ops->set_use_relocate_expensive_chain(BOOL_FALSE);
    ops->set_use_make_inactive(BOOL_FALSE);
    ops->set_use_make_chain_inactive(BOOL_FALSE);
    ops->set_use_shortest_path_two_opt(BOOL_FALSE);
    const Assignment* solution = model.SolveWithParameters(parameters);
    ASSERT_NE(solution, nullptr);
    ASSERT_EQ(model.status(), RoutingSearchStatus::ROUTING_SUCCESS);
    const int num_neighbors = model.solver()->neighbors();
    const Assignment assignment(solution);
    solution = model.SolveFromAssignmentWithParameters(&assignment, parameters);
    ASSERT_NE(solution, nullptr);
    ASSERT_EQ(model.status(), RoutingSearchStatus::ROUTING_SUCCESS);
    if (use_metaheuristic) {
      // Operators were reset due to the metaheuristic.
      EXPECT_LT(num_neighbors, model.solver()->neighbors());
    } else {
      // Check that no neighbor was explored since all moves are optimal.
      EXPECT_EQ(num_neighbors, model.solver()->neighbors());
    }
  }
}

TEST(ModelTest, GlobalOptimumReached) {
  IndexManager manager(2, 2, NodeIndex(0));
  Model model(manager);
  model.AddDisjunction({1}, /*penalty=*/2000);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::ALL_UNPERFORMED);
  EXPECT_NE(model.SolveWithParameters(parameters), nullptr);
  EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
}

TEST(ModelTest, InfeasibleModel) {
  IndexManager manager(3, 1, NodeIndex(0));
  Model model(manager);
  Solver* const solver = model.solver();
  solver->AddConstraint(solver->MakeFalseConstraint());
  EXPECT_EQ(model.Solve(), nullptr);
  EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_INFEASIBLE);
}

TEST(ModelTest, InfeasibleModelWithSpanConstraint) {
  IndexManager manager(3, 1, NodeIndex(0));
  Model model(manager);
  model.AddConstantDimension(1, 10, false, "dim");
  Dimension* const dim = model.GetMutableDimension("dim");
  // Constraining span >= 8 through start/end time windows.
  dim->CumulVar(model.Start(0))->SetMax(1);
  dim->CumulVar(model.End(0))->SetMin(9);
  // Constraining span <= 5 directly.
  dim->SetSpanUpperBoundForVehicle(5, 0);
  EXPECT_EQ(model.Solve(), nullptr);
  EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_INFEASIBLE);
}

// Tests that clear cases of model infeasibility due to the vehicle resource
// requirements are properly detected by the ResourceAssignmentConstraint
// (instance taken from b/219043402).
TEST(ModelTest, InfeasibleModelWithVehicleResources) {
  const IndexManager manager(1, 1, NodeIndex(0));
  Model model(manager);

  // Add a dimension to the model and add a resource to it, required by the
  // vehicle.
  const std::string kDimName = "time";
  model.AddConstantDimension(/*value=*/0, /*capacity=*/1000,
                             /*fix_start_cumul_to_zero=*/false, kDimName);
  Dimension& dim = *model.GetMutableDimension(kDimName);

  // Add 2 resources to the dimension, both with an end_domain.max of 1.
  ResourceGroup& resource_group = *model.AddResourceGroup();
  resource_group.AddResource(Attributes(Domain(0, 100), Domain(0, 1)), &dim);
  resource_group.AddResource(Attributes(Domain(0, 100), Domain(0, 1)), &dim);
  resource_group.NotifyVehicleRequiresAResource(0);

  // The vehicle is used even when empty, and must end its route after 2.
  dim.CumulVar(model.End(0))->SetMin(2);
  model.SetVehicleUsedWhenEmpty(true, 0);

  EXPECT_EQ(model.Solve(), nullptr);
  EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_INFEASIBLE);
}

// Same as above but what makes the model infeasible is that 2 vehicles have
// the same only allowed resource.
TEST(ModelTest, InfeasibleModelWithAllowedVehicleResources) {
  const int kNumVehicles = 2;
  const IndexManager manager(1, kNumVehicles, NodeIndex(0));
  Model model(manager);

  // Add a dimension to the model and add a resource group to it, required by
  // the 2 vehicles.
  const std::string kDimName = "time";
  model.AddConstantDimension(/*value=*/0, /*capacity=*/1000,
                             /*fix_start_cumul_to_zero=*/false, kDimName);
  Dimension& dim = *model.GetMutableDimension(kDimName);

  // Add 2 resources to the dimension, both with an end_domain.max of 1.
  ResourceGroup& resource_group = *model.AddResourceGroup();
  resource_group.AddResource(Attributes(Domain(0, 100), Domain(0, 1)), &dim);
  resource_group.AddResource(Attributes(Domain(0, 100), Domain(0, 1)), &dim);
  // The vehicles are used even when empty, and both only have resource #0 as
  // allowed.
  for (int v = 0; v < kNumVehicles; ++v) {
    model.SetVehicleUsedWhenEmpty(true, v);
    resource_group.NotifyVehicleRequiresAResource(v);
    resource_group.SetAllowedResourcesForVehicle(v, {0});
  }

  EXPECT_EQ(model.Solve(), nullptr);
  EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_INFEASIBLE);
}

TEST(ModelTest, InfeasibleModelDueToSpanUBWithVehicleResources) {
  const IndexManager manager(/*num_nodes=*/2, /*num_vehicles=*/1, NodeIndex(0));
  Model model(manager);

  // Add a dimension to the model and add a resource to it, required by the
  // vehicle.
  const std::string kDimName = "time";
  model.AddConstantDimension(/*value=*/1, /*capacity=*/1000,
                             /*fix_start_cumul_to_zero=*/false, kDimName);
  Dimension& dim = *model.GetMutableDimension(kDimName);

  // Add 2 resources to the dimension, both with a span ub of 1.
  ResourceGroup& resource_group = *model.AddResourceGroup();
  resource_group.AddResource(
      Attributes(Domain(0, 100), Domain(0, 100), /*span_upper_bound=*/1), &dim);
  resource_group.AddResource(
      Attributes(Domain(0, 100), Domain(0, 100), /*span_upper_bound=*/1), &dim);
  resource_group.NotifyVehicleRequiresAResource(0);

  EXPECT_EQ(model.Solve(), nullptr);
  EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_INFEASIBLE);
}

// Tests that the ResourceAssignmentConstraint doesn't have false positives,
// i.e. doesn't detect as infeasible a model in which the start->end trip is
// infeasible but start->node->end makes the resource assignment possible.
TEST(ModelTest, FeasibleModelWithVehicleResourcesAndNoTriangularInequality) {
  const IndexManager manager(2, 1, NodeIndex(0));
  Model model(manager);

  // Add a dimension where the start->end arc costs 25, but start->node and
  // node->end only cost 1.
  const int callback =
      model.RegisterTransitCallback([&model](int64_t from, int64_t to) {
        if (model.IsStart(from) && model.IsEnd(to)) {
          return 25;
        }
        return 1;
      });
  const std::string kDimName = "time";
  model.AddDimension(callback, /*slack_max=*/1000, /*capacity=*/1000,
                     /*fix_start_cumul_to_zero=*/false, kDimName);
  Dimension& dim = *model.GetMutableDimension(kDimName);

  // Add 2 resources to the dimension, with an end_domain.max of 2.
  ResourceGroup& resource_group = *model.AddResourceGroup();
  resource_group.AddResource(Attributes(Domain(0, 100), Domain(0, 2)), &dim);
  resource_group.AddResource(Attributes(Domain(0, 100), Domain(0, 2)), &dim);
  resource_group.NotifyVehicleRequiresAResource(0);

  model.SetVehicleUsedWhenEmpty(true, 0);

  // Make the node optional.
  model.AddDisjunction({1}, /*penalty=*/2000);

  const Assignment* const assignment = model.Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(assignment->Value(model.VehicleVar(1)), 0);
}

TEST(ModelTest, VeryConstrainedModel) {
  const int kNumNodes = 10;
  const IndexManager manager(kNumNodes, 1, NodeIndex(0));
  for (int threshold = 0; threshold < 20; ++threshold) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    // Using ALL_UNPERFORMED to make the CP/LS phase fail since all nodes are
    // mandatory.
    parameters.set_first_solution_strategy(
        FirstSolutionStrategy::ALL_UNPERFORMED);
    parameters.set_fallback_to_cp_sat_size_threshold(threshold);
    for (double secondary_ls_ratio : {0.0, 0.5, 0.9}) {
      parameters.set_secondary_ls_time_limit_ratio(secondary_ls_ratio);
      Model model(manager);
      const Assignment* assignment = model.SolveWithParameters(parameters);
      if (threshold >= model.Size()) {
        ASSERT_NE(assignment, nullptr);
      } else {
        ASSERT_EQ(assignment, nullptr);
      }
    }
  }
}

class BaseRoutingTest : public testing::Test {
 protected:
  enum SolveMode { STANDARD, FAST, ILS };

  BaseRoutingTest(bool use_unary_callback, bool use_light_propagation,
                  bool force_non_homogeneous_costs,
                  bool use_non_filtered_first_solution)
      : use_unary_callback_(use_unary_callback),
        use_light_propagation_(use_light_propagation),
        force_non_homogeneous_costs_(force_non_homogeneous_costs),
        use_non_filtered_first_solution_(use_non_filtered_first_solution) {}
  ~BaseRoutingTest() override = default;
  void SetUpModel() {
    RoutingModelParameters parameters = DefaultRoutingModelParameters();
    parameters.set_reduce_vehicle_cost_model(!force_non_homogeneous_costs_);
    model_.reset(CreateModel(parameters));
    ASSERT_NE(model_, nullptr);
  }
  const Assignment* SetUpModelAndSolve() {
    SetUpModel();
    return Solve();
  }
  const Assignment* Solve(SolveMode solve_mode = STANDARD) {
    std::unique_ptr<Assignment> assignment(CreateFirstSolutionAssignment());
    RoutingSearchParameters parameters = GetSearchParameters();
    parameters.set_use_unfiltered_first_solution_strategy(
        use_non_filtered_first_solution_);
    parameters.set_use_full_propagation(!use_light_propagation_);
    model_->CloseModelWithParameters(parameters);
    PostCloseModel();
    if (force_non_homogeneous_costs_) {
      EXPECT_FALSE(model_->CostsAreHomogeneousAcrossVehicles());
    } else {
      EXPECT_EQ(HasHomogeneousCosts(),
                model_->CostsAreHomogeneousAcrossVehicles());
    }
    EXPECT_EQ(VehicleCostCount(), model_->GetNonZeroCostClassesCount());
    switch (solve_mode) {
      case ILS:
        *parameters.mutable_iterated_local_search_parameters() =
            RecommendedIteratedLocalSearchParameters();
        return model_->SolveWithIteratedLocalSearch(parameters);
      case FAST: {
        RoutingSearchParameters one_solution = parameters;
        one_solution.set_solution_limit(1);
        const Assignment* solution = model_->SolveFromAssignmentWithParameters(
            assignment.get(), one_solution);
        return model_->FastSolveFromAssignmentWithParameters(
            solution, parameters, /*check_solution_in_cp=*/true);
      }
      default:
        return model_->SolveFromAssignmentWithParameters(assignment.get(),
                                                         parameters);
    }
  }
  virtual void PostCloseModel() {}
  virtual Model* CreateModel(const RoutingModelParameters& parameters) = 0;
  virtual Assignment* CreateFirstSolutionAssignment() { return nullptr; }
  virtual bool HasHomogeneousCosts() const { return true; }
  virtual int VehicleCostCount() const { return 0; }
  virtual RoutingSearchParameters GetSearchParameters() {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(
        FirstSolutionStrategy::FIRST_UNBOUND_MIN_VALUE);
    parameters.mutable_local_search_operators()->set_use_path_lns(BOOL_TRUE);
    parameters.mutable_local_search_operators()->set_use_inactive_lns(
        BOOL_TRUE);
    parameters.set_log_search(true);
    return parameters;
  }

  void ExpectRoutes(const Assignment* solution,
                    absl::Span<const std::vector<int64_t>> expected_routes) {
    ASSERT_EQ(model_->vehicles(), expected_routes.size());
    ASSERT_FALSE(solution == nullptr);
    std::vector<std::vector<int64_t>> actual_routes;
    model_->AssignmentToRoutes(*solution, &actual_routes);

    for (int vehicle = 0; vehicle < expected_routes.size(); ++vehicle) {
      std::vector<int64_t> actual_route;
      for (const int64_t index : actual_routes[vehicle]) {
        actual_route.push_back(index);
      }
      EXPECT_EQ(absl::StrJoin(expected_routes[vehicle], "->"),
                absl::StrJoin(actual_route, "->"))
          << "In route #" << vehicle;
    }
  }

  std::unique_ptr<IndexManager> manager_;
  std::unique_ptr<Model> model_;
  const bool use_unary_callback_;
  const bool use_light_propagation_;
  const bool force_non_homogeneous_costs_;
  const bool use_non_filtered_first_solution_;
};

class RoutingTest : public BaseRoutingTest,
                    public ::testing::WithParamInterface<
                        ::testing::tuple<bool, bool, bool, bool>> {
 protected:
  static const int64_t kSize;

  RoutingTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/::testing::get<0>(GetParam()),
            /*use_light_propagation=*/::testing::get<1>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<2>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<3>(GetParam())) {
  }
  ~RoutingTest() override = default;
  void SetUp() override { absl::SetFlag(&FLAGS_cp_name_cast_variables, true); }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    manager_ = std::make_unique<IndexManager>(kSize, 10, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    EXPECT_EQ(0, model->GetNonZeroCostClassesCount());
    const int vehicle_cost =
        use_unary_callback_
            ? model->RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model->RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    return model;
  }
  int VehicleCostCount() const override { return 1; }
  RoutingSearchParameters GetSearchParameters() override {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    if (use_light_propagation_) {
      parameters.set_first_solution_strategy(
          FirstSolutionStrategy::FIRST_UNBOUND_MIN_VALUE);
    } else {
      parameters.set_first_solution_strategy(
          FirstSolutionStrategy::GLOBAL_CHEAPEST_ARC);
    }
    return parameters;
  }
};

const int64_t RoutingTest::kSize = 20;

TEST_P(RoutingTest, PlanInspection) {
  const Assignment* const assignment = SetUpModelAndSolve();
  ASSERT_NE(assignment, nullptr);
  for (int i = 0; i < model_->Size(); ++i) {
    EXPECT_EQ(1, model_->ActiveVar(i)->Min());
  }
  const int64_t start = model_->Start(0);
  EXPECT_EQ(0, start);
  EXPECT_FALSE(model_->IsEnd(start));
  int node = start;
  int count = 0;
  while (!model_->IsEnd(node)) {
    ++count;
    node = assignment->Value(model_->NextVar(node));
  }
  EXPECT_EQ(count, kSize);
}

TEST_P(RoutingTest, AddToAssignment) {
  SetUpModel();
  Solver* const solver = model_->solver();
  IntVar* const var = solver->MakeIntVar(0, 1, "var");
  IntervalVar* const interval =
      solver->MakeFixedDurationIntervalVar(var, 1, "interval");
  model_->AddToAssignment(var);
  model_->AddIntervalToAssignment(interval);
  const Assignment* const assignment = Solve();
  EXPECT_EQ(0, assignment->Min(var));
  EXPECT_EQ(1, assignment->Max(var));
  EXPECT_EQ(0, assignment->StartMin(interval));
  EXPECT_EQ(1, assignment->StartMax(interval));
}

TEST_P(RoutingTest, DebugOutputAssignment) {
  const char* kExpectedOutput =
      "Vehicle 0:0 Vehicle(0) 1 Vehicle(0) 2 Vehicle(0) 3 Vehicle(0) 4 "
      "Vehicle(0) 5 Vehicle(0) 6 Vehicle(0) 7 Vehicle(0) 8 Vehicle(0) 9 "
      "Vehicle(0) 10 Vehicle(0) 11 Vehicle(0) 12 Vehicle(0) 13 Vehicle(0) 14 "
      "Vehicle(0) 15 Vehicle(0) 16 Vehicle(0) 17 Vehicle(0) 18 Vehicle(0) 19 "
      "Vehicle(0) Route end 29 Vehicle(0) \nVehicles 1-9: empty\n"
      "Unperformed nodes: None\n";
  const char* kExpectedOutputWithDimension =
      "Vehicle 0:0 Vehicle(0) Hop(0..0) 1 Vehicle(0) Hop(123..123) 2 "
      "Vehicle(0) Hop(246..246) 3 Vehicle(0) Hop(369..369) 4 Vehicle(0) "
      "Hop(492..492) 5 Vehicle(0) Hop(615..615) 6 Vehicle(0) Hop(738..738) 7 "
      "Vehicle(0) Hop(861..861) 8 Vehicle(0) Hop(984..984) 9 Vehicle(0) "
      "Hop(1107..1107) 10 Vehicle(0) Hop(1230..1230) 11 Vehicle(0) "
      "Hop(1353..1353) 12 Vehicle(0) Hop(1476..1476) 13 Vehicle(0) "
      "Hop(1599..1599) 14 Vehicle(0) Hop(1722..1722) 15 Vehicle(0) "
      "Hop(1845..1845) 16 Vehicle(0) Hop(1968..1968) 17 Vehicle(0) "
      "Hop(2091..2091) 18 Vehicle(0) Hop(2214..2214) 19 Vehicle(0) "
      "Hop(2337..2337) Route end 29 Vehicle(0) Hop(2460..2460) \n"
      "Vehicles 1-9: empty\nUnperformed nodes: None\n";
  const Assignment* assignment = SetUpModelAndSolve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(model_->DebugOutputAssignment(*assignment, ""), kExpectedOutput);

  // Add a dimension and get the debug output for that dimension.
  model_.reset(nullptr);
  SetUpModel();
  model_->AddConstantDimension(
      /*value = */ 123, /*capacity = */ 4567,
      /*fix_start_cumul_to_zero=*/true, "Hop");
  RoutingSearchParameters parameters = GetSearchParameters();
  assignment = model_->SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(model_->DebugOutputAssignment(*assignment, ""),
            kExpectedOutputWithDimension);
  EXPECT_EQ(model_->DebugOutputAssignment(*assignment, "Hip"), kExpectedOutput);
}

INSTANTIATE_TEST_SUITE_P(WithParameters, RoutingTest,
                         testing::Combine(::testing::Bool(), ::testing::Bool(),
                                          ::testing::Bool(),
                                          ::testing::Bool()));

class NoVehicleRoutingTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<::testing::tuple<bool, bool, bool>> {
 protected:
  NoVehicleRoutingTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/false,
            /*use_light_propagation=*/::testing::get<0>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<1>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<2>(GetParam())) {
  }
  ~NoVehicleRoutingTest() override = default;
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    const int64_t kSize = 20;
    const int64_t kPenalty = 1;
    manager_ = std::make_unique<IndexManager>(kSize, 0, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    EXPECT_EQ(0, model->GetNonZeroCostClassesCount());
    for (NodeIndex i(0); i < NodeIndex(kSize); ++i) {
      model->AddDisjunction(manager_->NodesToIndices({i}), kPenalty);
    }
    return model;
  }
};

TEST_P(NoVehicleRoutingTest, PlanInspection) {
  const Assignment* const assignment = SetUpModelAndSolve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(RoutingSearchStatus::ROUTING_OPTIMAL, model_->status());
  for (int i = 0; i < model_->Size(); ++i) {
    EXPECT_EQ(0, assignment->Value(model_->ActiveVar(i)));
  }
  EXPECT_EQ(0, model_->GetVehicleClassesCount());
}

INSTANTIATE_TEST_SUITE_P(WithParameters, NoVehicleRoutingTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

class OptionalGroupTest : public BaseRoutingTest,
                          public ::testing::WithParamInterface<
                              ::testing::tuple<bool, bool, bool, bool>> {
 protected:
  OptionalGroupTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/::testing::get<0>(GetParam()),
            /*use_light_propagation=*/::testing::get<1>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<2>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<3>(GetParam())),
        size_(0) {}
  static const int64_t kPenalty;
  ~OptionalGroupTest() override = default;
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    manager_ = std::make_unique<IndexManager>(size_ + 1, 1, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    std::vector<int64_t> optional_indices(size_);
    for (int i = 0; i < size_; ++i) {
      optional_indices[i] = manager_->NodeToIndex(NodeIndex(i + 1));
    }
    model->AddDisjunction(optional_indices, kPenalty);
    if (Active()) {
      model->ActiveVar(1)->SetMin(1);
    } else {
      for (int i = 0; i < size_; ++i) {
        model->ActiveVar(i + 1)->SetMax(0);
      }
    }
    const int vehicle_cost =
        use_unary_callback_
            ? model->RegisterUnaryTransitCallback([](int64_t) { return 0LL; })
            : model->RegisterTransitCallback(
                  [](int64_t, int64_t) { return 0LL; });
    model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    return model;
  }
  virtual bool Active() const = 0;
  int VehicleCostCount() const override { return 1; }

  int size_;
};

const int64_t OptionalGroupTest::kPenalty = 10;

class UnactiveGroupTest : public OptionalGroupTest {
 protected:
  bool Active() const override { return false; }
};

TEST_P(UnactiveGroupTest, GroupCost) {
  size_ = 20;
  EXPECT_EQ(kPenalty, SetUpModelAndSolve()->ObjectiveValue());
  EXPECT_EQ(1, model_->GetVehicleClassesCount());
}

TEST_P(UnactiveGroupTest, NodeCost) {
  size_ = 1;
  EXPECT_EQ(kPenalty, SetUpModelAndSolve()->ObjectiveValue());
  EXPECT_EQ(1, model_->GetVehicleClassesCount());
}

INSTANTIATE_TEST_SUITE_P(WithParameters, UnactiveGroupTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

class ActiveGroupTest : public OptionalGroupTest {
 protected:
  bool Active() const override { return true; }
};

TEST_P(ActiveGroupTest, GroupCost) {
  size_ = 20;
  EXPECT_EQ(0, SetUpModelAndSolve()->ObjectiveValue());
  EXPECT_EQ(1, model_->GetVehicleClassesCount());
}

TEST_P(ActiveGroupTest, NodeCost) {
  size_ = 1;
  EXPECT_EQ(0, SetUpModelAndSolve()->ObjectiveValue());
  EXPECT_EQ(1, model_->GetVehicleClassesCount());
}

INSTANTIATE_TEST_SUITE_P(WithParameters, ActiveGroupTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

TEST(RouteConstraintTest, AllConstrainedOnOneRoute) {
  for (bool costs_are_homogeneous_across_vehicles : {true, false}) {
    IndexManager manager(/*num_nodes=*/4, /*num_vehicles=*/2, NodeIndex(0));
    Model model(manager);
    std::vector<int64_t> constrained_nexts = {1, 2, 3, 5, 6};
    model.AddRouteConstraint(
        [&constrained_nexts](
            absl::Span<const int64_t> route) -> std::optional<int64_t> {
          for (int i = 0; i < route.size() - 1; ++i) {
            if (constrained_nexts[route[i]] != route[i + 1])
              return std::nullopt;
          }
          return route.front() == 0 ? 1 : 0;
        },
        costs_are_homogeneous_across_vehicles);
    model.AddRouteConstraint(
        [](absl::Span<const int64_t> /*route*/) -> std::optional<int64_t> {
          return 2;
        });
    const Assignment* assignment = model.Solve();
    ASSERT_NE(assignment, nullptr);
    for (int i = 0; i < model.Nexts().size(); ++i) {
      EXPECT_EQ(constrained_nexts[i], assignment->Value(model.Nexts()[i]));
    }
    EXPECT_EQ(1 + 2 * 2, assignment->ObjectiveValue());
  }
}

TEST(RouteConstraintTest, AllConstrainedOnTwoRoutes) {
  for (bool costs_are_homogeneous_across_vehicles : {true, false}) {
    IndexManager manager(/*num_nodes=*/4, /*num_vehicles=*/2, NodeIndex(0));
    Model model(manager);
    std::vector<int64_t> constrained_nexts = {1, 5, 3, 6, 2};
    model.AddRouteConstraint(
        [&constrained_nexts](
            absl::Span<const int64_t> route) -> std::optional<int64_t> {
          for (int i = 0; i < route.size() - 1; ++i) {
            if (constrained_nexts[route[i]] != route[i + 1])
              return std::nullopt;
          }
          return 1;
        },
        costs_are_homogeneous_across_vehicles);
    const Assignment* assignment = model.Solve();
    ASSERT_NE(assignment, nullptr);
    for (int i = 0; i < model.Nexts().size(); ++i) {
      EXPECT_EQ(constrained_nexts[i], assignment->Value(model.Nexts()[i]));
    }
    EXPECT_EQ(2, assignment->ObjectiveValue());
  }
}

// Finds the value associated with the given key and copies it to *value (if
// non-null). Returns false if the key was not found, true otherwise.
template <typename M, typename Key, typename Value>
bool FindCopy(const M& m, const Key& key, Value& value) {
  auto it = m.find(key);
  if (it == m.end()) return false;
  value = it->second;
  return true;
}

TEST(RouteConstraintTest, PartiallyConstrained) {
  for (bool costs_are_homogeneous_across_vehicles : {true, false}) {
    IndexManager manager(/*num_nodes=*/4, /*num_vehicles=*/2, NodeIndex(0));
    Model model(manager);
    absl::flat_hash_map<int64_t, int64_t> constrained_nexts = {{0, 1}, {3, 2}};
    model.AddRouteConstraint(
        [&constrained_nexts](
            absl::Span<const int64_t> route) -> std::optional<int64_t> {
          for (int i = 0; i < route.size() - 1; ++i) {
            int64_t next = -1;
            if (FindCopy(constrained_nexts, route[i], next) &&
                next != route[i + 1]) {
              return std::nullopt;
            }
          }
          return 1;
        },
        costs_are_homogeneous_across_vehicles);
    const Assignment* assignment = model.Solve();
    ASSERT_NE(assignment, nullptr);
    for (int i = 0; i < model.Nexts().size(); ++i) {
      if (constrained_nexts.contains(i)) {
        EXPECT_EQ(constrained_nexts[i], assignment->Value(model.Nexts()[i]));
      }
    }
    EXPECT_EQ(2, assignment->ObjectiveValue());
  }
}

TEST(RouteConstraintTest, PartiallyPrecedenceConstrained) {
  for (bool costs_are_homogeneous_across_vehicles : {true, false}) {
    IndexManager manager(/*num_nodes=*/4, /*num_vehicles=*/2, NodeIndex(0));
    RoutingModelParameters parameters = DefaultRoutingModelParameters();
    parameters.set_reduce_vehicle_cost_model(false);
    Model model(manager, parameters);
    absl::flat_hash_map<int64_t, int64_t> constrained_precedences = {{0, 1},
                                                                     {3, 2}};
    model.AddRouteConstraint(
        [&constrained_precedences](
            const std::vector<int64_t>& route) -> std::optional<int64_t> {
          absl::flat_hash_set<int64_t> seen;
          for (int i = 0; i < route.size(); ++i) {
            int64_t sibling = -1;
            if (FindCopy(constrained_precedences, route[i], sibling) &&
                seen.contains(sibling)) {
              return std::nullopt;
            }
            seen.insert(route[i]);
          }
          for (int64_t node : seen) {
            int64_t sibling = -1;
            if (FindCopy(constrained_precedences, node, sibling) &&
                !seen.contains(sibling)) {
              return std::nullopt;
            }
          }
          return 1;
        },
        costs_are_homogeneous_across_vehicles);
    model.AddConstantDimension(/*value=*/1, /*capacity=*/10000,
                               /*fix_start_cumul_to_zero=*/true, "rank");
    const Dimension& ranks = model.GetDimensionOrDie("rank");
    const Assignment* assignment = model.Solve();
    ASSERT_NE(assignment, nullptr);
    for (auto [first, second] : constrained_precedences) {
      EXPECT_EQ(assignment->Value(model.VehicleVars()[first]),
                assignment->Value(model.VehicleVars()[second]));
      EXPECT_LT(assignment->Value(ranks.CumulVar(first)),
                assignment->Value(ranks.CumulVar(second)));
    }
    EXPECT_EQ(2, assignment->ObjectiveValue());
  }
}

// TODO(user): Extend the following tests to test all LS operators.

void ExpectContainsAllSimpleDisjunctionOperators(
    const absl::flat_hash_set<std::string>& operators) {
  EXPECT_TRUE(operators.contains("MakeInactiveOperator"));
  EXPECT_TRUE(operators.contains("MakeChainInactiveOperator"));
  EXPECT_TRUE(operators.contains("MakeActiveOperator"));
  // Disabled in default search parameters:
  // EXPECT_TRUE(operators.contains("RelocateAndMakeActiveOperator"));
  // EXPECT_TRUE(operators.contains("MakeActiveAndRelocateOperator"));
  EXPECT_TRUE(operators.contains("SwapActiveOperator"));
  EXPECT_TRUE(operators.contains("SwapActiveChainOperator"));
}

TEST(RoutingNeighborhoodTest, ModelWithDisjunctions) {
  IndexManager manager(4, 1, NodeIndex(0));
  RoutingModelParameters parameters = DefaultRoutingModelParameters();
  parameters.mutable_solver_parameters()->set_profile_local_search(true);
  Model model(manager, parameters);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(1)}), 5);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(3)}), 10);
  model.SetArcCostEvaluatorOfAllVehicles(
      model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; }));
  EXPECT_NE(nullptr, model.Solve());
  const LocalSearchStatistics stats =
      model.solver()->GetLocalSearchStatistics();
  absl::flat_hash_set<std::string> operators;
  for (const auto& ls_stats : stats.local_search_operator_statistics()) {
    operators.insert(ls_stats.local_search_operator());
  }
  EXPECT_FALSE(operators.contains("TwoOptWithShortestPath"));
  EXPECT_FALSE(operators.contains("SwapActiveToShortestPath"));
  ExpectContainsAllSimpleDisjunctionOperators(operators);
}

TEST(RoutingNeighborhoodTest, ModelWithNonUnaryDisjunctions) {
  IndexManager manager(4, 1, NodeIndex(0));
  RoutingModelParameters parameters = DefaultRoutingModelParameters();
  parameters.mutable_solver_parameters()->set_profile_local_search(true);
  Model model(manager, parameters);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(1), NodeIndex(2)}), 5);
  model.SetArcCostEvaluatorOfAllVehicles(
      model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; }));
  EXPECT_NE(nullptr, model.Solve());
  const LocalSearchStatistics stats =
      model.solver()->GetLocalSearchStatistics();
  absl::flat_hash_set<std::string> operators;
  for (const auto& ls_stats : stats.local_search_operator_statistics()) {
    operators.insert(ls_stats.local_search_operator());
  }
  EXPECT_TRUE(operators.contains("TwoOptWithShortestPath"));
  EXPECT_TRUE(operators.contains("SwapActiveToShortestPath"));
  ExpectContainsAllSimpleDisjunctionOperators(operators);
}

TEST(DisjunctionTest, MandatoryDisjunctions) {
  IndexManager manager(4, 1, NodeIndex(0));
  Model model(manager);
  // Default mandatory API.
  EXPECT_EQ(0, model.AddDisjunction(manager.NodesToIndices({NodeIndex(1)})));
  // Default penalty value for mandatory disjunctions.
  EXPECT_EQ(1,
            model.AddDisjunction(manager.NodesToIndices({NodeIndex(2)}), -1));
  // Arbitrary negative penalty value for mandatory disjunction.
  EXPECT_EQ(
      2, model.AddDisjunction(manager.NodesToIndices({NodeIndex(3)}), -1000));

  const Assignment* assignment = model.Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(0, assignment->ObjectiveValue());
  for (int i = 0; i < model.Nexts().size(); ++i) {
    EXPECT_NE(i, assignment->Value(model.Nexts()[i]));
  }
}

TEST(DisjunctionsTest, SingleNodeInMultipleDisjunctions) {
  IndexManager manager(4, 1, NodeIndex(0));
  Model model(manager);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(1), NodeIndex(2)}),
                       10);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(1), NodeIndex(3)}),
                       10);
  const Assignment* assignment = model.Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(0, assignment->ObjectiveValue());
  std::vector<DisjunctionIndex> indices =
      model.GetDisjunctionIndices(manager.NodeToIndex(NodeIndex(1)));
  EXPECT_EQ(2, indices.size());
  EXPECT_EQ(0, indices[0].value());
  EXPECT_EQ(1, indices[1].value());
  indices = model.GetDisjunctionIndices(1);
  EXPECT_EQ(2, indices.size());
  EXPECT_EQ(0, indices[0].value());
  EXPECT_EQ(1, indices[1].value());
  absl::flat_hash_set<int> nodes =
      GetNodesInDisjunctionWithMaxCardinality(model, 1, 1);
  EXPECT_EQ(3, nodes.size());
  EXPECT_EQ(1, nodes.count(1));
  EXPECT_EQ(1, nodes.count(2));
  EXPECT_EQ(1, nodes.count(3));
  indices = model.GetDisjunctionIndices(manager.NodeToIndex(NodeIndex(2)));
  EXPECT_EQ(1, indices.size());
  EXPECT_EQ(0, indices[0].value());
  indices = model.GetDisjunctionIndices(2);
  EXPECT_EQ(1, indices.size());
  EXPECT_EQ(0, indices[0].value());
  nodes = GetNodesInDisjunctionWithMaxCardinality(model, 2, 1);
  EXPECT_EQ(2, nodes.size());
  EXPECT_EQ(1, nodes.count(1));
  EXPECT_EQ(1, nodes.count(2));
  indices = model.GetDisjunctionIndices(manager.NodeToIndex(NodeIndex(3)));
  EXPECT_EQ(1, indices.size());
  EXPECT_EQ(1, indices[0].value());
  indices = model.GetDisjunctionIndices(3);
  EXPECT_EQ(1, indices.size());
  EXPECT_EQ(1, indices[0].value());
  nodes = GetNodesInDisjunctionWithMaxCardinality(model, 3, 1);
  EXPECT_EQ(2, nodes.size());
  EXPECT_EQ(1, nodes.count(1));
  EXPECT_EQ(1, nodes.count(3));
}

TEST(DisjunctionsTest, SingleNodeInSingleDisjunction) {
  IndexManager manager(3, 1, NodeIndex(0));
  Model model(manager);
  model.AddDisjunction(
      {manager.NodeToIndex(NodeIndex(1)), manager.NodeToIndex(NodeIndex(2))},
      10);
  const Assignment* assignment = model.Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(0, assignment->ObjectiveValue());
  std::vector<DisjunctionIndex> indices =
      model.GetDisjunctionIndices(manager.NodeToIndex(NodeIndex(1)));
  EXPECT_EQ(1, indices.size());
  EXPECT_EQ(0, indices[0].value());
  indices = model.GetDisjunctionIndices(1);
  EXPECT_EQ(1, indices.size());
  EXPECT_EQ(0, indices[0].value());
  absl::flat_hash_set<int> nodes =
      GetNodesInDisjunctionWithMaxCardinality(model, 1, 1);
  EXPECT_EQ(2, nodes.size());
  EXPECT_EQ(1, nodes.count(1));
  EXPECT_EQ(1, nodes.count(2));
}

TEST(DisjunctionsTest, MultipleNodesInSingleDisjunctionWithMaxCardinality) {
  IndexManager manager(3, 1, NodeIndex(0));
  Model model(manager);
  const DisjunctionIndex disj_index = model.AddDisjunction(
      {manager.NodeToIndex(NodeIndex(1)), manager.NodeToIndex(NodeIndex(2))},
      10, 2);
  EXPECT_EQ(10, model.GetDisjunctionSoftMinPenalty(disj_index));
  EXPECT_EQ(2, model.GetDisjunctionMaxCardinality(disj_index));
  EXPECT_EQ(Model::PenaltyCostBehavior::PENALIZE_ONCE,
            model.GetDisjunctionSoftMinPenaltyCostBehavior(disj_index));
  const Assignment* assignment = model.Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(0, assignment->ObjectiveValue());
  std::vector<DisjunctionIndex> indices =
      model.GetDisjunctionIndices(manager.NodeToIndex(NodeIndex(1)));
  EXPECT_EQ(1, indices.size());
  EXPECT_EQ(0, indices[0].value());
  indices = model.GetDisjunctionIndices(1);
  EXPECT_EQ(1, indices.size());
  EXPECT_EQ(0, indices[0].value());
  absl::flat_hash_set<int> nodes =
      GetNodesInDisjunctionWithMaxCardinality(model, 1, 1);
  EXPECT_EQ(0, nodes.size());
  nodes = GetNodesInDisjunctionWithMaxCardinality(model, 1, 2);
  EXPECT_EQ(2, nodes.size());
  EXPECT_EQ(1, nodes.count(1));
  EXPECT_EQ(1, nodes.count(2));
}

TEST(DisjunctionsTest,
     MultipleNodesInSingleDisjunctionWithMaxCardinalityAndPenalty) {
  IndexManager manager(7, 1, NodeIndex(0));
  Model model(manager);
  const DisjunctionIndex disj_index = model.AddDisjunction(
      {
          manager.NodeToIndex(NodeIndex(1)),
          manager.NodeToIndex(NodeIndex(2)),
          manager.NodeToIndex(NodeIndex(3)),
          manager.NodeToIndex(NodeIndex(4)),
          manager.NodeToIndex(NodeIndex(5)),
          manager.NodeToIndex(NodeIndex(6)),
      },
      10, 6, Model::PenaltyCostBehavior::PENALIZE_ONCE);
  EXPECT_EQ(10, model.GetDisjunctionSoftMinPenalty(disj_index));
  EXPECT_EQ(6, model.GetDisjunctionMaxCardinality(disj_index));
  EXPECT_EQ(Model::PenaltyCostBehavior::PENALIZE_ONCE,
            model.GetDisjunctionSoftMinPenaltyCostBehavior(disj_index));

  // Only 3 nodes can be visited, i.e. 3 nodes must be dropped.
  model.AddConstantDimension(1, 4, /*fix_start_cumul_to_zero=*/true, "dim");
  const Assignment* const assignment = model.Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(10, assignment->ObjectiveValue());
}

TEST(DisjunctionsTest,
     MultipleNodesInSingleDisjunctionWithMaxCardinalityAndPenaltyPerInactive) {
  IndexManager manager(7, 1, NodeIndex(0));
  Model model(manager);
  const DisjunctionIndex disj_index = model.AddDisjunction(
      {
          manager.NodeToIndex(NodeIndex(1)),
          manager.NodeToIndex(NodeIndex(2)),
          manager.NodeToIndex(NodeIndex(3)),
          manager.NodeToIndex(NodeIndex(4)),
          manager.NodeToIndex(NodeIndex(5)),
          manager.NodeToIndex(NodeIndex(6)),
      },
      10, 6, Model::PenaltyCostBehavior::PENALIZE_PER_INACTIVE);
  EXPECT_EQ(10, model.GetDisjunctionSoftMinPenalty(disj_index));
  EXPECT_EQ(6, model.GetDisjunctionMaxCardinality(disj_index));
  EXPECT_EQ(Model::PenaltyCostBehavior::PENALIZE_PER_INACTIVE,
            model.GetDisjunctionSoftMinPenaltyCostBehavior(disj_index));

  // Only 3 nodes can be visited, i.e. 3 nodes must be dropped.
  model.AddConstantDimension(1, 4, /*fix_start_cumul_to_zero=*/true, "dim");
  const Assignment* const assignment = model.Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(30, assignment->ObjectiveValue());
}

TEST(DisjunctionsTest, DisjunctionSettersGetters) {
  IndexManager manager(4, 1, NodeIndex(0));
  Model model(manager);

  const DisjunctionIndex disj_index = model.MakeDisjunction({1, 2, 3});
  EXPECT_EQ(0, model.GetDisjunctionMinCardinality(disj_index));
  EXPECT_EQ(3, model.GetDisjunctionMaxCardinality(disj_index));

  model.SetDisjunctionHardMinimum(disj_index, 1);
  model.SetDisjunctionHardMaximum(disj_index, 2);
  EXPECT_EQ(1, model.GetDisjunctionMinCardinality(disj_index));
  EXPECT_EQ(2, model.GetDisjunctionMaxCardinality(disj_index));

  model.SetDisjunctionSoftMinimum(
      disj_index, 2, 5, Model::PenaltyCostBehavior::PENALIZE_PER_INACTIVE);
  model.SetDisjunctionSoftMaximum(disj_index, 1, 10,
                                  Model::PenaltyCostBehavior::PENALIZE_ONCE);

  EXPECT_EQ(2, model.GetDisjunctionSoftMinCardinality(disj_index));
  EXPECT_EQ(5, model.GetDisjunctionSoftMinPenalty(disj_index));
  EXPECT_EQ(Model::PenaltyCostBehavior::PENALIZE_PER_INACTIVE,
            model.GetDisjunctionSoftMinPenaltyCostBehavior(disj_index));

  EXPECT_EQ(1, model.GetDisjunctionSoftMaxCardinality(disj_index));
  EXPECT_EQ(10, model.GetDisjunctionSoftMaxPenalty(disj_index));
  EXPECT_EQ(Model::PenaltyCostBehavior::PENALIZE_ONCE,
            model.GetDisjunctionSoftMaxPenaltyCostBehavior(disj_index));

  EXPECT_EQ(Model::kNoPenalty, model.GetDisjunctionPenalty(disj_index));

  model.SetDisjunctionHardMinimum(disj_index, 0);
  EXPECT_EQ(5, model.GetDisjunctionPenalty(disj_index));
  // Reset for the struct check below.
  model.SetDisjunctionHardMinimum(disj_index, 1);

  const std::vector<int64_t>& node_indices =
      model.GetDisjunctionNodeIndices(disj_index);
  EXPECT_EQ(3, node_indices.size());
  EXPECT_EQ(1, node_indices[0]);
  EXPECT_EQ(2, node_indices[1]);
  EXPECT_EQ(3, node_indices[2]);

  const RoutingModel::Disjunction& disjunction =
      model.GetDisjunction(disj_index);
  EXPECT_EQ(1, disjunction.min_cardinality);
  EXPECT_EQ(2, disjunction.max_cardinality);
  EXPECT_EQ(2, disjunction.soft_min_cardinality);
  EXPECT_EQ(5, disjunction.soft_min_penalty);
  EXPECT_EQ(Model::PenaltyCostBehavior::PENALIZE_PER_INACTIVE,
            disjunction.soft_min_penalty_type);
  EXPECT_EQ(1, disjunction.soft_max_cardinality);
  EXPECT_EQ(10, disjunction.soft_max_penalty);
  EXPECT_EQ(Model::PenaltyCostBehavior::PENALIZE_ONCE,
            disjunction.soft_max_penalty_type);

  // Getting disjunction indices of nodes in a disjunction.
  EXPECT_THAT(model.GetDisjunctionIndices(1), ElementsAre(disj_index));
  EXPECT_THAT(model.GetDisjunctionIndices(2), ElementsAre(disj_index));
  EXPECT_THAT(model.GetDisjunctionIndices(3), ElementsAre(disj_index));
}

struct DisjunctionTestScenario {
  int num_nodes;
  int num_vehicles;
  std::vector<Model::Disjunction> disjunctions;
  int64_t expected_objective_value;
};

// Test class can make a model from a DisjunctionTestScenario.
class DisjunctionTestWithScenario : public ::testing::Test {
 protected:
  using Disjunction = Model::Disjunction;
  using DisjunctionIndex = Model::DisjunctionIndex;
  using PenaltyCostBehavior = Model::PenaltyCostBehavior;

  void MakeModel(const DisjunctionTestScenario& scenario) {
    manager_ = std::make_unique<IndexManager>(
        scenario.num_nodes, scenario.num_vehicles, NodeIndex(0));
    model_ = std::make_unique<Model>(*manager_);
    for (const Disjunction& disj : scenario.disjunctions) {
      const DisjunctionIndex disj_index = model_->MakeDisjunction(disj.indices);
      model_->SetDisjunctionHardMinimum(
          disj_index,
          std::clamp<int64_t>(disj.min_cardinality, 0, disj.indices.size()));
      model_->SetDisjunctionSoftMinimum(
          disj_index,
          std::clamp<int64_t>(disj.soft_min_cardinality, 0,
                              disj.indices.size()),
          disj.soft_min_penalty, disj.soft_min_penalty_type);
      model_->SetDisjunctionHardMaximum(
          disj_index,
          std::clamp<int64_t>(disj.max_cardinality, 0, disj.indices.size()));
      model_->SetDisjunctionSoftMaximum(
          disj_index,
          std::clamp<int64_t>(disj.soft_max_cardinality, 0,
                              disj.indices.size()),
          disj.soft_max_penalty, disj.soft_max_penalty_type);
    }
  }

  std::unique_ptr<IndexManager> manager_;
  std::unique_ptr<Model> model_;
};

TEST_F(DisjunctionTestWithScenario, HardMinHardMax) {
  DisjunctionTestScenario scenario = {
      .num_nodes = 8,
      .num_vehicles = 1,
      .disjunctions =
          {
              Disjunction{
                  .indices = {1, 2, 3, 4, 5, 6, 7},
                  .min_cardinality = 2,
                  .max_cardinality = 5,
              },
          },
      .expected_objective_value = 200,  // 2 inactive nodes.
  };
  // Each node in [1, 7] may be inactive, with a penalty of 100.
  for (int64_t node = 1; node < 8; ++node) {
    scenario.disjunctions.push_back(Disjunction{
        .indices = {node},
        .min_cardinality = 0,
        .soft_min_cardinality = 1,
        .soft_min_penalty = 100,
        .soft_min_penalty_type = PenaltyCostBehavior::PENALIZE_ONCE,
        .max_cardinality = 1,
    });
  }
  MakeModel(scenario);
  const Assignment* const assignment = model_->Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(scenario.expected_objective_value, assignment->ObjectiveValue());
}

TEST_F(DisjunctionTestWithScenario, SoftMinHardMax) {
  DisjunctionTestScenario scenario = {
      .num_nodes = 8,
      .num_vehicles = 1,
      .disjunctions =
          {
              Disjunction{
                  .indices = {1, 2, 3, 4, 5, 6, 7},
                  .soft_min_cardinality = 6,
                  .soft_min_penalty = 100,
                  .soft_min_penalty_type =
                      PenaltyCostBehavior::PENALIZE_PER_INACTIVE,
                  .max_cardinality = 3,
              },
          },
      .expected_objective_value = 300,  // 4 inactive nodes, 3 penalized.
  };
  MakeModel(scenario);
  const Assignment* const assignment = model_->Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(scenario.expected_objective_value, assignment->ObjectiveValue());
}

TEST_F(DisjunctionTestWithScenario, HardMinSoftMax) {
  DisjunctionTestScenario scenario = {
      .num_nodes = 8,
      .num_vehicles = 1,
      .disjunctions =
          {
              Disjunction{
                  .indices = {1, 2, 3, 4, 5, 6, 7},
                  .min_cardinality = 5,
                  .soft_max_cardinality = 2,
                  .soft_max_penalty = 100,
                  .soft_max_penalty_type =
                      PenaltyCostBehavior::PENALIZE_PER_INACTIVE,
              },
          },
      .expected_objective_value = 300,  // 5 active nodes, 3 penalized.
  };
  MakeModel(scenario);
  EXPECT_EQ(model_->GetDisjunctionMaxCardinality(DisjunctionIndex(0)), 7);
  const Assignment* const assignment = model_->Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(scenario.expected_objective_value, assignment->ObjectiveValue());
}

TEST(GetPerfectBinaryDisjunctionsTest, DiverseTestCase) {
  // We try to cover everything in one example:
  // - Some nodes are in a perfect binary disjunction.
  // - Some nodes are only in a binary disjunction, but the other node is
  //   in some other disjunction, so they aren't mapped.
  // - Some nodes are in singleton disjunctions.
  // - Some nodes aren't in any disjunction.
  // Disjunction have various penalties (zero, >0, infinite aka no penalty).
  IndexManager manager(/*num_nodes=*/20, /*num_vehicles=*/1,
                       /*depot*/ NodeIndex(0));
  Model model(manager);
  // In this simple model, the node<->index conversion is trivial. We check that
  // quickly, and then we leverage this in our main EXPECT_THAT(), below.
  for (int i = 0; i < model.nodes(); ++i) {
    ASSERT_EQ(manager.IndexToNode(i).value(), i);
  }

  // Add the disjunctions.
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(1), NodeIndex(3)}), 0);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(5), NodeIndex(0)}), 1);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(6)}), 0);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(8), NodeIndex(2)}));
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(2), NodeIndex(7)}),
                       1000);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(9)}));
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(10), NodeIndex(11)}),
                       3);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(10), NodeIndex(12)}),
                       4);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(13), NodeIndex(9)}));
  model.AddDisjunction(
      manager.NodesToIndices({NodeIndex(14), NodeIndex(15), NodeIndex(16)}));
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(19), NodeIndex(17)}));

  // Remember that in this simple model, NodeIndex == var index.
  EXPECT_THAT(model.GetPerfectBinaryDisjunctions(),
              ElementsAre(Pair(0, 5), Pair(1, 3), Pair(17, 19)));
}

TEST(IgnoreTriviallyInfeasibleDisjunctionsTest, DiverseTestCase) {
  IndexManager manager(/*num_nodes=*/3, /*num_vehicles=*/1,
                       /*depot*/ NodeIndex(0));
  Model model(manager);
  for (int i = 0; i < model.nodes(); ++i) {
    ASSERT_EQ(manager.IndexToNode(i).value(), i);
  }
  // "Disable" var #0 and #2 (eg. NodeIndex #0 and #2).
  model.ActiveVar(0)->SetMax(0);
  model.ActiveVar(2)->SetMax(0);
  // Add a few disjunctions of varying cardinalities, all with the same dummy
  // penalty.
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(0), NodeIndex(1)}),
                       1234,
                       /*max_cardinality=*/2);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(0), NodeIndex(2)}),
                       1234,
                       /*max_cardinality=*/1);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(0)}), 1234,
                       /*max_cardinality=*/1);
  model.AddDisjunction(
      manager.NodesToIndices({NodeIndex(0), NodeIndex(1), NodeIndex(2)}), 1234,
      /*max_cardinality=*/2);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(2), NodeIndex(0)}),
                       1234,
                       /*max_cardinality=*/2);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(1)}), 1234,
                       /*max_cardinality=*/1);

  model.IgnoreDisjunctionsAlreadyForcedToZero();
  const std::set<int> cleared_disjunctions = {1, 2, 4};
  for (int d = 0; d < model.GetNumberOfDisjunctions(); ++d) {
    const DisjunctionIndex index(d);
    SCOPED_TRACE(absl::StrCat(
        "On disjunction #", d, " on vars [",
        absl::StrJoin(model.GetDisjunctionNodeIndices(index), ", "),
        "], penalty=", model.GetDisjunctionSoftMinPenalty(index)));
    const int max_cardinality = model.GetDisjunctionMaxCardinality(index);
    if (cleared_disjunctions.count(d) > 0) {
      EXPECT_EQ(max_cardinality, 0);
    } else {
      EXPECT_NE(max_cardinality, 0);
    }
  }
}

class SameVehicleCostTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<::testing::tuple<bool, bool, bool>> {
 protected:
  SameVehicleCostTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/false,
            /*use_light_propagation=*/::testing::get<0>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<1>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<2>(GetParam())),
        size_(0) {}
  static const int64_t kSameVehicleCost;
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    manager_ = std::make_unique<IndexManager>(size_ + 1, 2, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    std::vector<NodeIndex> group1(size_ / 3);
    std::iota(group1.begin(), group1.end(), 1);
    model->AddSoftSameVehicleConstraint(manager_->NodesToIndices(group1),
                                        kSameVehicleCost);
    std::vector<NodeIndex> group2(size_ / 3);
    std::iota(group2.begin(), group2.end(), 1 + size_ / 3);
    model->AddSoftSameVehicleConstraint(manager_->NodesToIndices(group2),
                                        kSameVehicleCost / 2);
    for (int i = 0; i < size_; ++i) {
      model->VehicleVar(i + 1)->SetValue(GetVehicle(i));
    }
    return model;
  }
  virtual int GetVehicle(int i) const = 0;

  int size_;
};

const int64_t SameVehicleCostTest::kSameVehicleCost = 10;

class AlternateVehicleSameVehicleCostTest : public SameVehicleCostTest {
 protected:
  int GetVehicle(int i) const override { return i % 2; }
};

TEST_P(AlternateVehicleSameVehicleCostTest, SameVehicleCost) {
  size_ = 20;
  EXPECT_EQ(15, SetUpModelAndSolve()->ObjectiveValue());
}

INSTANTIATE_TEST_SUITE_P(WithParameters, AlternateVehicleSameVehicleCostTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

TEST(AllowedVehiclesTest, MandatoryNodeNoVehicle) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model model(manager);
  model.SetAllowedVehiclesForIndex({}, 1);
  const Assignment* assignment = model.Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(1, assignment->Value(model.NextVar(model.Start(0))));
}

TEST(AllowedVehiclesTest, MandatoryNodeNoVehicleMultiCalls) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model model(manager);
  model.SetAllowedVehiclesForIndex({0}, 1);
  model.SetAllowedVehiclesForIndex({}, 1);
  const Assignment* assignment = model.Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(1, assignment->Value(model.NextVar(model.Start(0))));
}

TEST(AllowedVehiclesTest, MandatoryNodeOutOfRangeVehicle) {
  IndexManager manager(2, 2, NodeIndex(0));
  Model model(manager);
  model.SetAllowedVehiclesForIndex({2}, 1);
  const Assignment* assignment = model.Solve();
  EXPECT_EQ(model.GetVehicleClassesCount(), 1);
  EXPECT_EQ(nullptr, assignment);
}

TEST(AllowedVehiclesTest, MandatoryNodeOutOfAndInRangeVehicle) {
  IndexManager manager(2, 2, NodeIndex(0));
  Model model(manager);
  model.SetAllowedVehiclesForIndex({0, 2}, 1);
  const Assignment* assignment = model.Solve();
  EXPECT_EQ(model.GetVehicleClassesCount(), 2);
  ASSERT_NE(assignment, nullptr);
}

TEST(AllowedVehiclesTest, OptionalNodeOutOfRangeVehicle) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model model(manager);
  model.AddDisjunction({1}, 10);
  model.SetAllowedVehiclesForIndex({2}, 1);
  const Assignment* assignment = model.Solve();
  EXPECT_EQ(model.GetVehicleClassesCount(), 1);
  ASSERT_NE(assignment, nullptr);
}

TEST(AllowedVehiclesTest, MandatoryNodeOneVehicle) {
  IndexManager manager(2, 2, NodeIndex(0));
  Model model(manager);
  model.SetAllowedVehiclesForIndex({0}, 1);
  const Assignment* assignment = model.Solve();
  EXPECT_EQ(model.GetVehicleClassesCount(), 2);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(1, assignment->Value(model.NextVar(model.Start(0))));
}

TEST(AllowedVehiclesTest, MandatoryNodeOneImpossibleVehicle) {
  IndexManager manager(2, 2, NodeIndex(0));
  Model model(manager);
  model.SetAllowedVehiclesForIndex({0}, 1);
  model.VehicleVar(1)->SetValue(1);
  const Assignment* assignment = model.Solve();
  // NOTE: Neither vehicles can serve node 1, so there's a single vehicle class.
  EXPECT_EQ(model.GetVehicleClassesCount(), 1);
  EXPECT_EQ(nullptr, assignment);
}

TEST(AllowedVehiclesTest, OptionalNodeOneImpossibleVehicle) {
  IndexManager manager(2, 2, NodeIndex(0));
  Model model(manager);
  model.AddDisjunction({1}, 10);
  model.SetAllowedVehiclesForIndex({0}, 1);
  model.VehicleVar(1)->RemoveValue(0);
  const Assignment* assignment = model.Solve();
  // NOTE: Neither vehicles can serve node 1, so there's a single vehicle class.
  EXPECT_EQ(model.GetVehicleClassesCount(), 1);
  ASSERT_NE(assignment, nullptr);
}

TEST(AllowedVehiclesTest, MandatoryNodeVaryingOneVehicle) {
  IndexManager manager(2, 3, NodeIndex(0));
  for (int i = 0; i < 3; ++i) {
    Model model(manager);
    model.SetAllowedVehiclesForIndex({i}, 1);
    const Assignment* assignment = model.Solve();
    EXPECT_EQ(model.GetVehicleClassesCount(), 2);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(1, assignment->Value(model.NextVar(model.Start(i))));
  }
}

TEST(AllowedVehiclesTest, MandatoryNodeVaryingManyVehicle) {
  IndexManager manager(2, 3, NodeIndex(0));
  for (int i = 0; i < 3; ++i) {
    std::vector<int> allowed_vehicles;
    for (int j = 0; j < 3; ++j) {
      if (i != j) allowed_vehicles.push_back(j);
    }
    Model model(manager);
    model.SetAllowedVehiclesForIndex(allowed_vehicles, 1);
    const Assignment* assignment = model.Solve();
    EXPECT_EQ(model.GetVehicleClassesCount(), 2);
    ASSERT_NE(assignment, nullptr);
    EXPECT_NE(1, assignment->Value(model.NextVar(model.Start(i))));
  }
}

TEST(AllowedVehiclesTest, AllowedVehicleRemovedBasedOnDimensionCapacity) {
  const int kNumVehicles = 2;
  const int kNumNodes = 3;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  for (bool node1_optional : {true, false}) {
    Model model(manager);
    // Node1 is only allowed on vehicle 0.
    model.SetAllowedVehiclesForIndex({0}, 1);
    ASSERT_TRUE(model.IsVehicleAllowedForIndex(0, 1));
    ASSERT_FALSE(model.IsVehicleAllowedForIndex(1, 1));
    // Node2 is allowed on both vehicles.
    ASSERT_TRUE(model.IsVehicleAllowedForIndex(0, 2));
    ASSERT_TRUE(model.IsVehicleAllowedForIndex(1, 2));

    // Add a unary dimension for which both nodes have a higher demand than
    // vehicle 0's capacity.
    const int evaluator_index =
        model.RegisterUnaryTransitCallback([](int64_t node) {
          if (node == 1 || node == 2) return 100;
          return 1;
        });
    model.AddDimensionWithVehicleCapacity(
        evaluator_index, 0, /*vehicle_capacities=*/{10, 1000}, false, "dim");

    if (node1_optional) {
      model.AddDisjunction({1}, 100);
    }

    model.CloseModel();

    // Node1 is no longer allowed on either vehicle.
    EXPECT_FALSE(model.IsVehicleAllowedForIndex(0, 1));
    EXPECT_FALSE(model.IsVehicleAllowedForIndex(1, 1));
    // Node2 is no longer allowed on vehicle 0.
    EXPECT_FALSE(model.IsVehicleAllowedForIndex(0, 2));
    EXPECT_TRUE(model.IsVehicleAllowedForIndex(1, 2));

    // Test that the Solve() works as intended.
    if (node1_optional) {
      // If Node1 is optional, it should be unperformed.
      const Assignment* const assignment = model.Solve();
      ASSERT_NE(assignment, nullptr);
      EXPECT_EQ(assignment->Value(model.NextVar(1)), 1);
    } else {
      // Node1 cannot be unperformed so there's no solution.
      EXPECT_EQ(model.Solve(), nullptr);
    }
  }
}

TEST(AllowedVehiclesTest, TransitPlusSlack) {
  IndexManager manager(5, 1, NodeIndex(0));
  Model model(manager);
  // Vehicle capacity is 10.
  // Node 1 has slack [0, 5], transit -15, total transit may reach -10.
  // Node 2 has slack [0, 4], transit -15, total transit at most -11.
  // Node 3 has slack [0, 5], transit 10, total transit may reach 10.
  // Node 4 has slack [1, 5], transit 10, total transit at least 11.
  const int evaluator_index = model.RegisterUnaryTransitCallback(
      [](int64_t index) { return index < 3 ? -15 : 10; });

  model.AddDimensionWithVehicleCapacity(evaluator_index, /*slack_max=*/5,
                                        /*vehicle_capacities=*/{10}, false,
                                        "dim");
  Dimension* const dimension = model.GetMutableDimension("dim");
  dimension->SlackVar(2)->SetMax(4);
  dimension->SlackVar(4)->SetMin(1);

  model.CloseModel();

  EXPECT_TRUE(model.IsVehicleAllowedForIndex(0, 1));
  EXPECT_FALSE(model.IsVehicleAllowedForIndex(0, 2));
  EXPECT_TRUE(model.IsVehicleAllowedForIndex(0, 3));
  EXPECT_FALSE(model.IsVehicleAllowedForIndex(0, 4));
}

TEST(AllowedVehiclesTest, AllowedVehicleRemovedBasedOnPositiveDimensionCumuls) {
  IndexManager manager(6, 2, NodeIndex(0));
  Model model(manager);
  const int evaluator_index = model.RegisterTransitCallback(
      [](int64_t /*tail*/, int64_t /*head*/) { return 1; },
      Model::kTransitEvaluatorSignPositiveOrZero);
  model.AddDimension(evaluator_index, 0, 100, /*fix_start_cumul_to_zero=*/false,
                     "dim");
  Dimension* const dimension = model.GetMutableDimension("dim");
  // Vehicle 0 starts at 10, ends at 50.
  // Vehicle 1 starts at 30, ends at 70.
  // Node 1's window starts at 0, ends at 5.
  // Node 2's window starts at 0, ends at 10.
  // Node 3's window starts at 20, ends at 35.
  // Node 4's window starts at 70, ends at 85.
  // Node 5's window starts at 80, ends at 100.
  dimension->CumulVar(model.Start(0))->SetRange(10, 50);
  dimension->CumulVar(model.End(0))->SetRange(10, 50);
  dimension->CumulVar(model.Start(1))->SetRange(30, 70);
  dimension->CumulVar(model.End(1))->SetRange(30, 70);
  dimension->CumulVar(1)->SetRange(0, 5);
  dimension->CumulVar(2)->SetRange(0, 10);
  dimension->CumulVar(3)->SetRange(20, 35);
  dimension->CumulVar(4)->SetRange(70, 85);
  dimension->CumulVar(5)->SetRange(80, 100);
  model.CloseModel();
  EXPECT_FALSE(model.IsVehicleAllowedForIndex(0, 1));
  EXPECT_FALSE(model.IsVehicleAllowedForIndex(1, 1));
  EXPECT_TRUE(model.IsVehicleAllowedForIndex(0, 2));
  EXPECT_FALSE(model.IsVehicleAllowedForIndex(1, 2));
  EXPECT_TRUE(model.IsVehicleAllowedForIndex(0, 3));
  EXPECT_TRUE(model.IsVehicleAllowedForIndex(1, 3));
  EXPECT_FALSE(model.IsVehicleAllowedForIndex(0, 4));
  EXPECT_TRUE(model.IsVehicleAllowedForIndex(1, 4));
  EXPECT_FALSE(model.IsVehicleAllowedForIndex(0, 5));
  EXPECT_FALSE(model.IsVehicleAllowedForIndex(1, 5));
}

TEST(AllowedVehiclesTest,
     AllowedVehicleRemovedBasedOnNonPositiveDimensionCumuls) {
  IndexManager manager(3, 2, NodeIndex(0));
  Model model(manager);
  // Make transits not all positive.
  const int evaluator_index = model.RegisterTransitCallback(
      [](int64_t /*tail*/, int64_t /*head*/) { return 1; });
  model.AddDimension(evaluator_index, 0, 100, /*fix_start_cumul_to_zero=*/false,
                     "dim");
  Dimension* const dimension = model.GetMutableDimension("dim");
  // Vehicle 0 starts at 10, ends at 50.
  // Vehicle 1 starts at 30, ends at 70.
  // Node 1's window starts at 0, ends at 5.
  // Node 2's window starts at 0, ends at 10.
  dimension->CumulVar(model.Start(0))->SetRange(10, 50);
  dimension->CumulVar(model.End(0))->SetRange(10, 50);
  dimension->CumulVar(model.Start(1))->SetRange(30, 70);
  dimension->CumulVar(model.End(1))->SetRange(30, 70);
  dimension->CumulVar(1)->SetRange(0, 5);
  dimension->CumulVar(2)->SetRange(0, 10);
  model.CloseModel();
  EXPECT_TRUE(model.IsVehicleAllowedForIndex(0, 1));
  EXPECT_TRUE(model.IsVehicleAllowedForIndex(1, 1));
  EXPECT_TRUE(model.IsVehicleAllowedForIndex(0, 2));
  EXPECT_TRUE(model.IsVehicleAllowedForIndex(1, 2));
}

TEST(AllowedVehiclesTest, AllowedVehicleRemovedBasedOnMixedDimensionCumuls) {
  IndexManager manager(6, 2, NodeIndex(0));
  Model model(manager);
  // Make transits not all positive.
  const int positive_evaluator_index = model.RegisterTransitCallback(
      [](int64_t /*tail*/, int64_t /*head*/) { return 1; },
      Model::kTransitEvaluatorSignPositiveOrZero);
  const int evaluator_index = model.RegisterTransitCallback(
      [](int64_t /*tail*/, int64_t /*head*/) { return 1; });
  model.AddDimensionWithVehicleTransits(
      {positive_evaluator_index, evaluator_index}, 0, 100,
      /*fix_start_cumul_to_zero=*/false, "dim");
  Dimension* const dimension = model.GetMutableDimension("dim");
  // Vehicle 0 starts at 10, ends at 50.
  // Vehicle 1 starts at 30, ends at 70.
  // Node 1's window starts at 0, ends at 5.
  // Node 2's window starts at 0, ends at 10.
  // Node 3's window starts at 20, ends at 35.
  // Node 4's window starts at 70, ends at 85.
  // Node 5's window starts at 80, ends at 100.
  dimension->CumulVar(model.Start(0))->SetRange(10, 50);
  dimension->CumulVar(model.End(0))->SetRange(10, 50);
  dimension->CumulVar(model.Start(1))->SetRange(30, 70);
  dimension->CumulVar(model.End(1))->SetRange(30, 70);
  dimension->CumulVar(1)->SetRange(0, 5);
  dimension->CumulVar(2)->SetRange(0, 10);
  dimension->CumulVar(3)->SetRange(20, 35);
  dimension->CumulVar(4)->SetRange(70, 85);
  dimension->CumulVar(5)->SetRange(80, 100);
  model.CloseModel();
  EXPECT_FALSE(model.IsVehicleAllowedForIndex(0, 1));
  EXPECT_TRUE(model.IsVehicleAllowedForIndex(1, 1));
  EXPECT_TRUE(model.IsVehicleAllowedForIndex(0, 2));
  EXPECT_TRUE(model.IsVehicleAllowedForIndex(1, 2));
  EXPECT_TRUE(model.IsVehicleAllowedForIndex(0, 3));
  EXPECT_TRUE(model.IsVehicleAllowedForIndex(1, 3));
  EXPECT_FALSE(model.IsVehicleAllowedForIndex(0, 4));
  EXPECT_TRUE(model.IsVehicleAllowedForIndex(1, 4));
  EXPECT_FALSE(model.IsVehicleAllowedForIndex(0, 5));
  EXPECT_TRUE(model.IsVehicleAllowedForIndex(1, 5));
}

class DimensionVehicleSpanUpperBoundTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<bool, bool, bool, bool>> {
 protected:
  DimensionVehicleSpanUpperBoundTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/::testing::get<0>(GetParam()),
            /*use_light_propagation=*/::testing::get<1>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<2>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<3>(GetParam())) {
  }
  // Model with 2 vehicles, one dimension on which vehicle 0 has a span
  // upper bound.
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    const char* kDimension = "dimension";
    const int64_t kSpanUpperBound = 1;
    manager_ = std::make_unique<IndexManager>(4, 2, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    const int callback =
        use_unary_callback_
            ? model->RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model->RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    model->AddDimension(callback, 0, 10,
                        /*fix_start_cumul_to_zero=*/false, kDimension);
    Dimension* const dimension = model->GetMutableDimension(kDimension);
    dimension->SetSpanUpperBoundForVehicle(kSpanUpperBound, 0);
    EXPECT_EQ(kSpanUpperBound, dimension->GetSpanUpperBoundForVehicle(0));
    EXPECT_EQ(kint64max, dimension->GetSpanUpperBoundForVehicle(1));
    return model;
  }
};

TEST_P(DimensionVehicleSpanUpperBoundTest, InspectSolution) {
  const Assignment* const solution = SetUpModelAndSolve();
  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(0, solution->ObjectiveValue());
  // First route is empty, second is not.
  EXPECT_EQ(5, model_->Next(*solution, 0));
  EXPECT_NE(6, model_->Next(*solution, 4));
}

INSTANTIATE_TEST_SUITE_P(WithParameters, DimensionVehicleSpanUpperBoundTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

// Testing the finalizer with disjoint time windows and soft lower and upper
// bounds which impact the optimal objective value (including vehicle offset).
// With the LP solver the closest feasible value does not lead to an optimal
// schedule; the MILP solver finds the optimal.
TEST(RoutingFinalizerTest, DimensionLowerUpperBoundWithDisjointWindows) {
  const int64_t kOffset = 10;
  const std::vector<int64_t> starts = {21 + kOffset};
  const std::vector<int64_t> ends = {29 + kOffset};
  for (RoutingSearchParameters::SchedulingSolver solver_type :
       {RoutingSearchParameters::SCHEDULING_GLOP,
        RoutingSearchParameters::SCHEDULING_CP_SAT}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_mixed_integer_scheduling_solver(solver_type);
    parameters.set_first_solution_strategy(
        FirstSolutionStrategy::ALL_UNPERFORMED);
    parameters.mutable_local_search_operators()->set_use_shortest_path_two_opt(
        BOOL_FALSE);
    const IndexManager manager(2, 1, NodeIndex(0));
    Model model(manager);
    const int callback =
        model.RegisterUnaryTransitCallback([](int64_t) { return 10LL; });
    model.AddDimension(callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/false, "dim");
    Dimension* const dimension = model.GetMutableDimension("dim");
    dimension->CumulVar(model.Start(0))->SetMin(kOffset);
    // Skewed towards the upper bound to make the LP schedule sub-optimal.
    dimension->SetCumulVarSoftLowerBound(1, 28 + kOffset, 1);
    dimension->SetCumulVarSoftUpperBound(1, 28 + kOffset, 100);
    dimension->CumulVar(1)->SetRange(15 + kOffset, 35 + kOffset);
    // Making the node optional to allow using ALL_UNPERFORMED which then can be
    // improved by local search.
    model.AddDisjunction({1}, 1000);
    Solver* const solver = model.solver();
    solver->AddConstraint(
        solver->MakeNotMemberCt(dimension->CumulVar(1), starts, ends));
    const Assignment* const solution = model.SolveWithParameters(parameters);
    ASSERT_TRUE(solution != nullptr);
    if (solver_type == RoutingSearchParameters::SCHEDULING_GLOP) {
      // Node scheduled at 30.
      EXPECT_EQ(solution->ObjectiveValue(), 200);
    } else {
      // Node scheduled at 20.
      EXPECT_EQ(solution->ObjectiveValue(), 8);
    }
    EXPECT_TRUE(model.GetMutableLocalCumulMPOptimizer(*dimension) != nullptr);
  }
}

// Testing span cost, with 4 vehicles and 3 nodes (+ 1 depot), each node allows
// a different vehicle, one node with a small cumul max, one node with a
// large cumul min and the last node with a cumul whose bounds are in between.
// The resulting cost should reflect the range between the smallest cumul max
// and the largest cumul min, one vehicle remaining unused.
class DimensionGlobalSpanCostTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<bool, bool, bool, bool>> {
 protected:
  DimensionGlobalSpanCostTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/::testing::get<0>(GetParam()),
            /*use_light_propagation=*/::testing::get<1>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<2>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<3>(GetParam())) {
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    const int kNumTestDimensions = 3;
    const char* kDimension = "test_dimension";
    const char* kDummyDimension = "dummy_dimension";
    const int64_t kGlobalSpanCostCoef = 1;
    manager_ = std::make_unique<IndexManager>(4, 4, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    const int callback =
        use_unary_callback_
            ? model->RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model->RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    for (int i = 0; i < kNumTestDimensions; ++i) {
      const std::string name = absl::StrCat(kDimension, i);
      model->AddDimension(callback, 0, kint64max,
                          /*fix_start_cumul_to_zero=*/false, name);
      Dimension* const dimension = model->GetMutableDimension(name);
      dimension->SetGlobalSpanCostCoefficient(kGlobalSpanCostCoef);
      dimension->CumulVar(manager_->NodeToIndex(NodeIndex(1)))->SetMax(3);
      dimension->CumulVar(manager_->NodeToIndex(NodeIndex(2)))->SetMin(30);
      dimension->CumulVar(manager_->NodeToIndex(NodeIndex(3)))->SetRange(5, 10);
    }
    model->AddDimension(callback, 0, kint64max,
                        /*fix_start_cumul_to_zero=*/false, kDummyDimension);
    model->GetMutableDimension(kDummyDimension)
        ->SetGlobalSpanCostCoefficient(0);
    model->VehicleVar(manager_->NodeToIndex(NodeIndex(1)))->SetValue(0);
    model->VehicleVar(manager_->NodeToIndex(NodeIndex(2)))->SetValue(1);
    model->VehicleVar(manager_->NodeToIndex(NodeIndex(3)))->SetValue(2);
    return model;
  }
};

TEST_P(DimensionGlobalSpanCostTest, GetObjectiveValue) {
  const int64_t kExpectedCost = 3 * ((30 + 1) - (3 - 1));
  const Assignment* const solution = SetUpModelAndSolve();
  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(kExpectedCost, solution->ObjectiveValue());
  EXPECT_EQ(4, model_->GetVehicleClassesCount());
}

INSTANTIATE_TEST_SUITE_P(WithParameters, DimensionGlobalSpanCostTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

TEST(GlobalSpanCostTest, UnboundDimension) {
  IndexManager manager(1, 1, NodeIndex(0));
  Model model(manager);
  model.AddDimension(
      model.RegisterUnaryTransitCallback([](int64_t) { return 0; }), kint64max,
      kint64max, false, "dimension");
  Dimension* const dimension = model.GetMutableDimension("dimension");
  // NOTE: The cost needs to be > 1 to deactivate some expression optimizations.
  dimension->SetGlobalSpanCostCoefficient(2);
  SimpleCycleTimer timer;
  timer.Start();
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  const int64_t kMaxTimeSeconds = 10;
  parameters.mutable_time_limit()->set_seconds(kMaxTimeSeconds);
  const Assignment* const solution = model.SolveWithParameters(parameters);
  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(0, solution->ObjectiveValue());
  timer.Stop();
  // The solver should return very quickly, never getting close to the time
  // limit.
  EXPECT_LT(timer.Get(), 0.1 * kMaxTimeSeconds);
}

TEST(GlobalSpanCostTest, SlowPropagation) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model model(manager);
  model.AddDimension(model.RegisterTransitCallback([](int64_t from, int64_t) {
    if (from == 1) return 111;
    return 0;
  }),
                     kint64max, kint64max, false, "dimension");
  model.SetFixedCostOfAllVehicles(1);
  Dimension* const dimension = model.GetMutableDimension("dimension");
  dimension->SetGlobalSpanCostCoefficient(1);
  SimpleCycleTimer timer;
  timer.Start();
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  const int64_t kMaxTimeSeconds = 10;
  parameters.mutable_time_limit()->set_seconds(kMaxTimeSeconds);
  const Assignment* const solution = model.SolveWithParameters(parameters);
  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(112, solution->ObjectiveValue());
  timer.Stop();
  // The solver should return very quickly, never getting close to the time
  // limit.
  EXPECT_LT(timer.Get(), 0.1 * kMaxTimeSeconds);
}

// Unit test for b/214560991.
TEST(GlobalSpanCostTest, ProperStartEndValuesWithoutLP) {
  const int kNumVehicles = 2;
  const int kNumNodes = 3;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  // Node 1 can only be served by vehicle 0, Node2 only by vehicle 1.
  model.SetAllowedVehiclesForIndex({0}, 1);
  model.SetAllowedVehiclesForIndex({1}, 2);

  const std::string kDimName = "time";
  model.AddConstantDimensionWithSlack(0, 1000, 1000, false, kDimName);
  Dimension* const dimension = model.GetMutableDimension(kDimName);
  dimension->SetGlobalSpanCostCoefficient(1);
  // Node 1 can only be served after 100, Node 2 only after 200.
  dimension->CumulVar(1)->SetMin(100);
  dimension->CumulVar(2)->SetMin(200);

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_disable_scheduling_beware_this_may_degrade_performance(true);

  const Assignment* const solution = model.SolveWithParameters(parameters);
  ASSERT_NE(solution, nullptr);
  // All nodes should be served at 200, for a total cost of 0.
  EXPECT_EQ(solution->ObjectiveValue(), 0);
  for (int i = 0; i < dimension->cumuls().size(); i++) {
    EXPECT_EQ(solution->Value(dimension->CumulVar(i)), 200);
  }
}

class GlobalSpanCostCumulOptimizerTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<bool, bool, bool, bool>> {
 protected:
  GlobalSpanCostCumulOptimizerTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/false,
            /*use_light_propagation=*/::testing::get<1>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<2>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<3>(GetParam())) {
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    const int kNumNodes = 4;
    const int kNumVehicles = 2;
    manager_ =
        std::make_unique<IndexManager>(kNumNodes, kNumVehicles, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);

    const int callback = model->RegisterTransitCallback(
        [this, model](int64_t from, int64_t to) {
          if (model->IsStart(from) &&
              (to == manager_->NodeToIndex(NodeIndex(1)) ||
               to == manager_->NodeToIndex(NodeIndex(2)))) {
            // Going from a vehicle start to node 1 or 2.
            return 0;
          }
          if (from == manager_->NodeToIndex(NodeIndex(2)) &&
              to == manager_->NodeToIndex(NodeIndex(3))) {
            // 2 to 3 is more expensive.
            return 2;
          }
          return 1;
        },
        Model::kTransitEvaluatorSignPositiveOrZero);
    model->AddDimension(callback, 0, kint64max,
                        /*fix_start_cumul_to_zero=*/false, dimension_name_);
    Dimension* const dimension = model->GetMutableDimension(dimension_name_);
    dimension->SetGlobalSpanCostCoefficient(global_span_cost_coefficient_);

    // First vehicle starts after 1000, second vehicle after 1500.
    dimension->CumulVar(model->Start(0))->SetMin(1000);
    dimension->CumulVar(model->Start(1))->SetMin(1500);

    return model;
  }

  RoutingSearchParameters GetSearchParameters() override {
    RoutingSearchParameters parameters = BaseRoutingTest::GetSearchParameters();
    parameters.set_disable_scheduling_beware_this_may_degrade_performance(
        LPSchedulingDisabled());
    return parameters;
  }

  bool LPSchedulingDisabled() const { return ::testing::get<0>(GetParam()); }

  const std::string dimension_name_ = "time";
  const int global_span_cost_coefficient_ = 3;
};

TEST_P(GlobalSpanCostCumulOptimizerTest, OptimizeWithOffset) {
  const bool kLPSchedulingDisabled = LPSchedulingDisabled();
  // Node 2 should be done before 500 (impossible), node 1 before 1000 (so it
  // must be visited by the first vehicle), and node 3 before 3000.
  const int kNodeMaxCumuls[] = {1000, 500, 3000};
  const int kNodeSoftUpperBounds[] = {995, 200, 2000};
  const int kNodeSoftUpperBoundCostCoefficient = 7;
  const int kVehicleStartEndSoftUpperBounds[][2] = {{500, 1000}, {1000, 1500}};
  const int kVehicleSoftUpperBoundCostCoefficient = 11;
  const int kPenaltyCost = 10000;

  const int kExpectedVehicleStartEndTimes[][2] = {{1000, 1002}, {1500, 1501}};

  const bool kConsiderEmptyVehicleCosts[] = {true, false};
  for (int c = 0; c < std::size(kConsiderEmptyVehicleCosts); c++) {
    SetUpModel();
    Dimension* const dimension = model_->GetMutableDimension(dimension_name_);

    for (int v = 0; v < model_->vehicles(); v++) {
      model_->SetVehicleUsedWhenEmpty(kConsiderEmptyVehicleCosts[c], v);
      dimension->SetCumulVarSoftUpperBound(
          model_->Start(v), kVehicleStartEndSoftUpperBounds[v][0],
          kVehicleSoftUpperBoundCostCoefficient);
      dimension->SetCumulVarSoftUpperBound(
          model_->End(v), kVehicleStartEndSoftUpperBounds[v][1],
          kVehicleSoftUpperBoundCostCoefficient);
    }

    for (int i = 0; i < std::size(kNodeMaxCumuls); i++) {
      const int index = manager_->NodeToIndex(NodeIndex(i + 1));
      // Make node optional, with a penalty of kPenaltyCost.
      model_->AddDisjunction({index}, kPenaltyCost, /*max_cardinality=*/1);

      dimension->CumulVar(index)->SetMax(kNodeMaxCumuls[i]);
      dimension->SetCumulVarSoftUpperBound(index, kNodeSoftUpperBounds[i],
                                           kNodeSoftUpperBoundCostCoefficient);
    }

    const Assignment* const solution = Solve();
    ASSERT_NE(solution, nullptr);
    // Verify that the offset was correctly computed.
    EXPECT_EQ(dimension->GetGlobalOptimizerOffset(),
              kLPSchedulingDisabled ? 0 : 1000 - 1);

    if (kLPSchedulingDisabled) {
      // We don't check the solution when LP scheduling is disabled, the Solve()
      // above ensures the parameter is properly handled by the solver.
      continue;
    }

    // Node 2 can't be served, and Node 1 is late. The second vehicle is unused
    // (we need the first vehicle to serve node 1, and using both vehicles would
    // entail in a higher global span cost), but still goes from its start to
    // end.
    int vehicle_costs = 0;
    int max_end_time = 0;
    int min_start_time = 1e6;
    for (int v = 0; v < model_->vehicles(); v++) {
      const int* vehicle_start_end_time = kExpectedVehicleStartEndTimes[v];
      const int64_t start = model_->Start(v);
      const int64_t end = model_->End(v);
      EXPECT_EQ(solution->Value(dimension->CumulVar(start)),
                vehicle_start_end_time[0]);
      EXPECT_EQ(solution->Value(dimension->CumulVar(end)),
                vehicle_start_end_time[1]);
      if (model_->Next(*solution, start) != end ||
          kConsiderEmptyVehicleCosts[c]) {
        max_end_time = std::max(max_end_time, vehicle_start_end_time[1]);
        min_start_time = std::min(min_start_time, vehicle_start_end_time[0]);
        for (int i = 0; i < 2; i++) {
          vehicle_costs += kVehicleSoftUpperBoundCostCoefficient *
                           (vehicle_start_end_time[i] -
                            kVehicleStartEndSoftUpperBounds[v][i]);
        }
      }
    }

    const int node_costs =
        kPenaltyCost + kNodeSoftUpperBoundCostCoefficient * (1000 - 995);

    ASSERT_GE(max_end_time, min_start_time);
    EXPECT_EQ(
        node_costs + vehicle_costs +
            global_span_cost_coefficient_ * (max_end_time - min_start_time),
        solution->ObjectiveValue());
  }
}

constexpr absl::Duration kPackCumulsDurationLimit =
    kAnyXsanEnabled ? absl::Seconds(30) : absl::Seconds(3);

TEST_P(GlobalSpanCostCumulOptimizerTest, OptimizeAndPackWithOffset) {
  SetUpModel();

  Dimension* const dimension = model_->GetMutableDimension(dimension_name_);
  const int64_t kMaxCumul = 1000000001000001000;
  for (int index = 0; index < dimension->cumuls().size(); index++) {
    dimension->CumulVar(index)->SetMax(kMaxCumul);
  }
  const int64_t kVehicleCumulMin = kMaxCumul - 1000;
  for (int v = 0; v < model_->vehicles(); v++) {
    dimension->CumulVar(model_->Start(v))->SetMin(kVehicleCumulMin);
  }

  // Add a dimension without global span cost that will not be optimized by the
  // global LP. Packing the solution should therefore have no effect on it.
  const std::string kDummyDimensionName = "constant_dimension";
  model_->AddConstantDimension(
      /*value*/ 0, /*capacity*/ kint64max,
      /*fix_start_cumul_to_zero=*/false, kDummyDimensionName);
  Dimension* dummy_dimension = model_->GetMutableDimension(kDummyDimensionName);
  const int kDummyDimensionValue = 15;
  for (int i = 0; i < dummy_dimension->cumuls().size(); i++) {
    dummy_dimension->CumulVar(i)->SetValue(kDummyDimensionValue);
  }

  const Assignment* const solution =
      model_->PackCumulsOfOptimizerDimensionsFromAssignment(
          Solve(), kPackCumulsDurationLimit);
  const bool kLPSchedulingDisabled = LPSchedulingDisabled();
  EXPECT_EQ(dimension->GetGlobalOptimizerOffset(),
            kLPSchedulingDisabled ? 0 : kVehicleCumulMin - 1);
  ASSERT_NE(solution, nullptr);

  if (kLPSchedulingDisabled) {
    // We don't check the solution when LP scheduling is disabled, the Solve()
    // above ensures the parameter is properly handled by the solver.
    return;
  }
  const int kExpectedCost = global_span_cost_coefficient_ * 2;
  EXPECT_EQ(kExpectedCost, solution->ObjectiveValue());

  EXPECT_EQ(solution->Value(dimension->CumulVar(model_->Start(0))),
            kVehicleCumulMin);

  // Nodes 1 and 2 should be the first on their routes, and Node 3 comes after
  // Node 1.
  const int64_t index1 = manager_->NodeToIndex(NodeIndex(1));
  const int64_t index2 = manager_->NodeToIndex(NodeIndex(2));
  const int64_t index3 = manager_->NodeToIndex(NodeIndex(3));
  for (int v = 0; v < model_->vehicles(); v++) {
    EXPECT_EQ(solution->Value(dimension->CumulVar(model_->Start(v))),
              kVehicleCumulMin);
    const int64_t next_after_start =
        solution->Value(model_->NextVar(model_->Start(v)));
    EXPECT_TRUE(next_after_start == index1 || next_after_start == index2);
    if (next_after_start == index1) {
      // index1 --> index3 --> End(v)
      EXPECT_EQ(solution->Value(model_->NextVar(next_after_start)), index3);
      EXPECT_EQ(solution->Value(model_->NextVar(index3)), model_->End(v));
    } else {
      // index2 --> End(v)
      EXPECT_EQ(solution->Value(model_->NextVar(next_after_start)),
                model_->End(v));
    }
  }

  // Verify that the dummy dimension's cumuls haven't been affected by packing.
  for (int i = 0; i < dummy_dimension->cumuls().size(); i++) {
    EXPECT_EQ(solution->Value(dummy_dimension->CumulVar(i)),
              kDummyDimensionValue);
  }
}

INSTANTIATE_TEST_SUITE_P(WithParameters, GlobalSpanCostCumulOptimizerTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

// Testing the scheduler in a setup where LP only will fail to pack the
// solution.
TEST(GlobalSpanCostCumulOptimizerPackingTest, DisjointTimeWindows) {
  for (RoutingSearchParameters::SchedulingSolver solver_type :
       {RoutingSearchParameters::SCHEDULING_GLOP,
        RoutingSearchParameters::SCHEDULING_CP_SAT}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_mixed_integer_scheduling_solver(solver_type);
    IndexManager manager(2, 2, NodeIndex(0));
    Model model(manager);
    const int callback =
        model.RegisterTransitCallback([&model](int64_t from, int64_t) {
          if (model.IsStart(from)) return 0;
          return 999;
        });
    model.AddDimension(callback, 31536000, 31536000,
                       /*fix_start_cumul_to_zero*/ false, "dim");
    Dimension* const dimension = model.GetMutableDimension("dim");
    dimension->SetGlobalSpanCostCoefficient(263224087809000.0 / 3600.0);
    Solver* const solver = model.solver();
    const std::vector<int64_t> starts = {1};
    const std::vector<int64_t> ends = {999};
    solver->AddConstraint(
        solver->MakeNotMemberCt(dimension->CumulVar(1), starts, ends));
    dimension->CumulVar(model.End(0))->SetMin(1000);
    model.SetVehicleUsedWhenEmpty(true, 0);

    model.CloseModelWithParameters(parameters);
    // Original solution: put node1 on the second route: 0->3, and 2->1->4
    Assignment* original_solution =
        BuildAssignmentFromSolution(model, {3, 4, 1});
    for (int i = 0; i < model.Size() + model.vehicles(); ++i) {
      original_solution->Add(dimension->CumulVar(i));
    }
    const Assignment* solution =
        model.PackCumulsOfOptimizerDimensionsFromAssignment(
            original_solution, kPackCumulsDurationLimit);
    ASSERT_NE(solution, nullptr);
    const std::vector<int64_t> kExpectedCumuls =
        solver_type == RoutingSearchParameters::SCHEDULING_CP_SAT
            ? std::vector<int64_t>{1000, 1000, 1000, 1000, 1999}
            // sub-optimal global span due to ignoring node1's disjoint time
            // window with GLOP when minimizing vehicle end times in packing.
            : std::vector<int64_t>{1000, 0, 0, 1000, 1000};
    for (int i = 0; i < kExpectedCumuls.size(); ++i) {
      EXPECT_EQ(solution->Value(dimension->CumulVar(i)), kExpectedCumuls[i]);
    }
  }
}

using PackCumulsOfOptimizerDimensionsFromAssignmentTest =
    ::testing::TestWithParam<RoutingSearchParameters::SchedulingSolver>;

// Unit test for PackCumulsOfOptimizerDimensionsFromAssignment() to
// verify it returns an assignment compatible with the model's preassignment.
TEST_P(PackCumulsOfOptimizerDimensionsFromAssignmentTest,
       CumulsConstrainedByPreAssignment) {
  const int kNumNodes = 1;
  const int kNumVehicles = 1;
  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_continuous_scheduling_solver(GetParam());

  const std::string kDimensionName = "time";
  model.AddConstantDimensionWithSlack(1, kint64max, kint64max,
                                      /*fix_start_cumul_to_zero*/ false,
                                      kDimensionName);
  Dimension* const time_dimension = model.GetMutableDimension(kDimensionName);
  time_dimension->SetGlobalSpanCostCoefficient(1);
  // Set the vehicle min/max start time to 123/456.
  const int start_index = model.Start(0);
  const int kMinStartTime = 123;
  const int kMaxStartTime = 456;
  time_dimension->CumulVar(start_index)->SetRange(kMinStartTime, kMaxStartTime);

  // Set the vehicle end time to 123456 in the preassignment.
  const int end_index = model.End(0);
  const int kEndTime = 123456;
  model.MutablePreAssignment()
      ->Add(time_dimension->CumulVar(end_index))
      ->SetValue(kEndTime);

  const Assignment* assignment = model.SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  assignment = model.PackCumulsOfOptimizerDimensionsFromAssignment(
      assignment, kPackCumulsDurationLimit);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(assignment->Value(time_dimension->CumulVar(start_index)),
            kMaxStartTime);
  EXPECT_EQ(assignment->Value(time_dimension->CumulVar(end_index)), kEndTime);
}

// Simple unit test for PackCumulsOfOptimizerDimensionsFromAssignment() with
// one vehicle and one node to serve, with a time window with soft start and
// soft end times, with a higher cost on the soft end.
// This test verifies that the resulting route is properly packed (see
// b/154381899).
TEST_P(PackCumulsOfOptimizerDimensionsFromAssignmentTest,
       RouteEndCumulProperlyMinimized) {
  const int kNumNodes = 2;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_continuous_scheduling_solver(GetParam());

  const std::string kDimensionName = "time";
  model.AddConstantDimensionWithSlack(0, kint64max, kint64max,
                                      /*fix_start_cumul_to_zero*/ false,
                                      kDimensionName);
  Dimension* const time_dimension = model.GetMutableDimension(kDimensionName);
  // Set the vehicle min/max start time to 123/456.
  const int node_index = manager.NodeToIndex(NodeIndex(1));
  const int kSoftLowerBound = 20;
  const int kSoftUpperBound = 10;
  time_dimension->SetCumulVarSoftLowerBound(node_index, kSoftLowerBound, 1);
  time_dimension->SetCumulVarSoftUpperBound(node_index, kSoftUpperBound, 2);

  const Assignment* assignment = model.SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  assignment = model.PackCumulsOfOptimizerDimensionsFromAssignment(
      assignment, kPackCumulsDurationLimit);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(assignment->Value(time_dimension->CumulVar(model.Start(0))),
            kSoftUpperBound);
  EXPECT_EQ(assignment->Value(time_dimension->CumulVar(model.End(0))),
            kSoftUpperBound);
}

// Unit test for PackCumulsOfOptimizerDimensionsFromAssignment() that verifies
// that packing with vehicle breaks properly propagates the new break start
// and end times.
// The example has a single vehicle with different start/end nodes, with an
// empty route (but 'used_if_route_is_empty' set to true, so the vehicle is
// considered as used). In the original solution passed to packing, the route
// starts at 200, with one break scheduled from 290 to 295. With packing, the
// route start time gets shifted to 0, so the break must occur from 0 to 5.
TEST_P(PackCumulsOfOptimizerDimensionsFromAssignmentTest,
       PackingWithVehicleBreak) {
  const int kNumNodes = 2;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNodes, kNumVehicles,
                             {{NodeIndex(0), NodeIndex(1)}});
  Model model(manager);
  model.SetVehicleUsedWhenEmpty(true, 0);

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_continuous_scheduling_solver(GetParam());

  const std::string kDimensionName = "time";
  model.AddConstantDimensionWithSlack(100, kint64max, kint64max,
                                      /*fix_start_cumul_to_zero*/ false,
                                      kDimensionName);
  Dimension* const time_dimension = model.GetMutableDimension(kDimensionName);

  // Add two 5-second breaks to the vehicle, one that must start at 0 and
  // another that must start at 300.
  IntervalVar* const break_interval1 =
      model.solver()->MakeFixedDurationIntervalVar(0, 0, 5, false, "break");
  IntervalVar* const break_interval2 =
      model.solver()->MakeFixedDurationIntervalVar(290, 295, 5, false, "break");
  time_dimension->SetBreakIntervalsOfVehicle({break_interval1, break_interval2},
                                             0, -1, -1);
  model.CloseModelWithParameters(parameters);

  // Make the vehicle start either at 0 or after 200.
  time_dimension->CumulVar(model.Start(0))->SetRange(0, 500);
  model.solver()->AddConstraint(model.solver()->MakeNotMemberCt(
      time_dimension->CumulVar(model.Start(0)), std::vector<int64_t>{1},
      std::vector<int64_t>{199}));

  // Original solution: Start(t:200)->End(t:305), break happens from 290 to 295.
  Assignment* original_solution = BuildAssignmentFromSolution(model, {1});
  original_solution->Add(time_dimension->CumulVar(model.Start(0)))
      ->SetValue(200);
  original_solution->Add(time_dimension->CumulVar(model.End(0)))->SetValue(305);
  original_solution->Add(break_interval1)->SetStartValue(0);
  original_solution->Add(break_interval1)->SetEndValue(0);
  original_solution->Add(break_interval2)->SetStartValue(290);
  original_solution->Add(break_interval2)->SetEndValue(295);
  const Assignment* solution =
      model.PackCumulsOfOptimizerDimensionsFromAssignment(
          original_solution, kPackCumulsDurationLimit);

  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(solution->Value(time_dimension->CumulVar(model.Start(0))), 0);
  EXPECT_EQ(solution->Value(time_dimension->CumulVar(model.End(0))), 105);
  EXPECT_EQ(solution->StartValue(break_interval1), 0);
  EXPECT_EQ(solution->EndValue(break_interval1), 5);
}

INSTANTIATE_TEST_SUITE_P(
    WithSolverType, PackCumulsOfOptimizerDimensionsFromAssignmentTest,
    ::testing::Values(RoutingSearchParameters::SCHEDULING_GLOP,
                      RoutingSearchParameters::SCHEDULING_CP_SAT));

class DimensionNodePrecedenceTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<::testing::tuple<bool, bool, bool>> {
 protected:
  DimensionNodePrecedenceTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/false,
            /*use_light_propagation=*/::testing::get<0>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<1>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<2>(GetParam())) {
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    const int kNumNodes = 5;
    const int kNumVehicles = 1;
    manager_ =
        std::make_unique<IndexManager>(kNumNodes, kNumVehicles, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);

    feasible_arcs_ = {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}};
    const int callback = model->RegisterTransitCallback(
        [this](int64_t from, int64_t to) {
          if (accept_all_arcs_ ||
              feasible_arcs_.contains(std::make_pair(from, to))) {
            return int64_t{1};
          }
          // All other arcs are infeasible.
          return kint64max;
        },
        Model::kTransitEvaluatorSignPositiveOrZero);

    model->AddDimension(callback,
                        /*slack_max*/ kint64max, kint64max,
                        /*fix_start_cumul_to_zero=*/false, dimension_name_);
    Dimension* const dimension = model->GetMutableDimension(dimension_name_);

    dimension->AddNodePrecedence({/*first_node*/ 1, /*second_node*/ 3,
                                  /*offset*/ 4, performed_constraint_});
    dimension->AddNodePrecedence({/*first_node*/ 3, /*second_node*/ 2,
                                  /*offset*/ -1, performed_constraint_});
    dimension->AddNodePrecedence({/*first_node*/ 2, /*second_node*/ 4,
                                  /*offset*/ 6, performed_constraint_});

    return model;
  }

  const char* dimension_name_ = "test_dimension";
  bool accept_all_arcs_ = false;
  Dimension::NodePrecedence::PerformedConstraint performed_constraint_ =
      Dimension::NodePrecedence::PerformedConstraint::
          kFirstAndSecondIndependent;

 private:
  absl::flat_hash_set<std::pair<int64_t, int64_t>> feasible_arcs_;
};

TEST_P(DimensionNodePrecedenceTest, NodePrecedencesAreActive) {
  using PerformedStatus = Dimension::NodePrecedence::PerformedConstraint;
  EXPECT_EQ(Dimension::GetPrecedenceStatus(
                true, true, PerformedStatus::kFirstAndSecondIndependent),
            Dimension::PrecedenceStatus::kInactive);
  EXPECT_EQ(Dimension::GetPrecedenceStatus(
                true, false, PerformedStatus::kFirstAndSecondIndependent),
            Dimension::PrecedenceStatus::kInactive);
  EXPECT_EQ(Dimension::GetPrecedenceStatus(
                false, true, PerformedStatus::kFirstAndSecondIndependent),
            Dimension::PrecedenceStatus::kInactive);
  EXPECT_EQ(Dimension::GetPrecedenceStatus(
                false, false, PerformedStatus::kFirstAndSecondIndependent),
            Dimension::PrecedenceStatus::kActive);
  EXPECT_EQ(Dimension::GetPrecedenceStatus(
                true, true, PerformedStatus::kSecondImpliesFirst),
            Dimension::PrecedenceStatus::kInactive);
  EXPECT_EQ(Dimension::GetPrecedenceStatus(
                true, false, PerformedStatus::kSecondImpliesFirst),
            Dimension::PrecedenceStatus::kInvalid);
  EXPECT_EQ(Dimension::GetPrecedenceStatus(
                false, true, PerformedStatus::kSecondImpliesFirst),
            Dimension::PrecedenceStatus::kInactive);
  EXPECT_EQ(Dimension::GetPrecedenceStatus(
                false, false, PerformedStatus::kSecondImpliesFirst),
            Dimension::PrecedenceStatus::kActive);
  EXPECT_EQ(Dimension::GetPrecedenceStatus(
                true, true, PerformedStatus::kFirstImpliesSecond),
            Dimension::PrecedenceStatus::kInactive);
  EXPECT_EQ(Dimension::GetPrecedenceStatus(
                true, false, PerformedStatus::kFirstImpliesSecond),
            Dimension::PrecedenceStatus::kInactive);
  EXPECT_EQ(Dimension::GetPrecedenceStatus(
                false, true, PerformedStatus::kFirstImpliesSecond),
            Dimension::PrecedenceStatus::kInvalid);
  EXPECT_EQ(Dimension::GetPrecedenceStatus(
                false, false, PerformedStatus::kFirstImpliesSecond),
            Dimension::PrecedenceStatus::kActive);
  EXPECT_EQ(Dimension::GetPrecedenceStatus(
                true, true, PerformedStatus::kFirstAndSecondEqual),
            Dimension::PrecedenceStatus::kInactive);
  EXPECT_EQ(Dimension::GetPrecedenceStatus(
                true, false, PerformedStatus::kFirstAndSecondEqual),
            Dimension::PrecedenceStatus::kInvalid);
  EXPECT_EQ(Dimension::GetPrecedenceStatus(
                false, true, PerformedStatus::kFirstAndSecondEqual),
            Dimension::PrecedenceStatus::kInvalid);
  EXPECT_EQ(Dimension::GetPrecedenceStatus(
                false, false, PerformedStatus::kFirstAndSecondEqual),
            Dimension::PrecedenceStatus::kActive);
}

TEST_P(DimensionNodePrecedenceTest,
       ValidateNodePrecedencesFirstAndSecondIndependent) {
  const std::vector<std::pair<bool, std::vector<int64_t>>> test_routes = {
      {true, {1, 2, 3, 4}}, {false, {3, 2, 1, 4}}, {true, {2, 3, 4}}};
  accept_all_arcs_ = true;
  for (const auto& test_route : test_routes) {
    std::unique_ptr<Model> model(CreateModel(DefaultRoutingModelParameters()));
    const Dimension* const dimension =
        model->GetMutableDimension(dimension_name_);
    dimension->CumulVar(model->End(0))->SetMax(20);
    model->AddDisjunction({int64_t{1}}, 100);
    model->CloseModelWithParameters(DefaultRoutingSearchParameters());
    Assignment* assignment = model->solver()->MakeAssignment();
    ASSERT_TRUE(model->RoutesToAssignment({test_route.second},
                                          /*ignore_inactive_indices=*/false,
                                          /*close_routes=*/true, assignment));

    Solver* solver = model->solver();
    EXPECT_EQ(solver->Solve(solver->MakeRestoreAssignment(assignment)),
              test_route.first);
  }
}

TEST_P(DimensionNodePrecedenceTest, ValidateNodePrecedencesSecondImpliesFirst) {
  const std::vector<std::pair<bool, std::vector<int64_t>>> test_routes = {
      {true, {1, 2, 3, 4}}, {false, {3, 2, 1, 4}}, {false, {2, 3, 4}}};
  accept_all_arcs_ = true;
  performed_constraint_ =
      Dimension::NodePrecedence::PerformedConstraint::kSecondImpliesFirst;
  for (const auto& test_route : test_routes) {
    std::unique_ptr<Model> model(CreateModel(DefaultRoutingModelParameters()));
    const Dimension* const dimension =
        model->GetMutableDimension(dimension_name_);
    dimension->CumulVar(model->End(0))->SetMax(20);
    model->AddDisjunction({int64_t{1}}, 100);
    model->CloseModelWithParameters(DefaultRoutingSearchParameters());
    Assignment* assignment = model->solver()->MakeAssignment();
    ASSERT_TRUE(model->RoutesToAssignment({test_route.second},
                                          /*ignore_inactive_indices=*/false,
                                          /*close_routes=*/true, assignment));
    Solver* solver = model->solver();
    EXPECT_EQ(solver->Solve(solver->MakeRestoreAssignment(assignment)),
              test_route.first);
  }
}

TEST_P(DimensionNodePrecedenceTest, ValidateNodePrecedencesFirstImpliesSecond) {
  const std::vector<std::pair<bool, std::vector<int64_t>>> test_routes = {
      {true, {1, 2, 3, 4}}, {false, {3, 2, 1, 4}}, {false, {2, 1, 4}}};
  accept_all_arcs_ = true;
  performed_constraint_ =
      Dimension::NodePrecedence::PerformedConstraint::kFirstImpliesSecond;
  for (const auto& test_route : test_routes) {
    std::unique_ptr<Model> model(CreateModel(DefaultRoutingModelParameters()));
    const Dimension* const dimension =
        model->GetMutableDimension(dimension_name_);
    dimension->CumulVar(model->End(0))->SetMax(20);
    model->AddDisjunction({int64_t{1}}, 100);
    model->CloseModelWithParameters(DefaultRoutingSearchParameters());
    Assignment* assignment = model->solver()->MakeAssignment();
    ASSERT_TRUE(model->RoutesToAssignment({test_route.second},
                                          /*ignore_inactive_indices=*/false,
                                          /*close_routes=*/true, assignment));
    Solver* solver = model->solver();
    EXPECT_EQ(solver->Solve(solver->MakeRestoreAssignment(assignment)),
              test_route.first);
  }
}

TEST_P(DimensionNodePrecedenceTest,
       ValidateNodePrecedencesFirstAndSecondEqual) {
  const std::vector<std::pair<bool, std::vector<int64_t>>> test_routes = {
      {true, {1, 2, 3, 4}}, {false, {3, 2, 1, 4}}, {false, {2, 3, 4}}};
  accept_all_arcs_ = true;
  performed_constraint_ =
      Dimension::NodePrecedence::PerformedConstraint::kFirstAndSecondEqual;
  for (const auto& test_route : test_routes) {
    std::unique_ptr<Model> model(CreateModel(DefaultRoutingModelParameters()));
    const Dimension* const dimension =
        model->GetMutableDimension(dimension_name_);
    dimension->CumulVar(model->End(0))->SetMax(20);
    model->AddDisjunction({int64_t{1}}, 100);
    model->CloseModelWithParameters(DefaultRoutingSearchParameters());
    Assignment* assignment = model->solver()->MakeAssignment();
    ASSERT_TRUE(model->RoutesToAssignment({test_route.second},
                                          /*ignore_inactive_indices=*/false,
                                          /*close_routes=*/true, assignment));
    Solver* solver = model->solver();
    EXPECT_EQ(solver->Solve(solver->MakeRestoreAssignment(assignment)),
              test_route.first);
  }
}

TEST_P(DimensionNodePrecedenceTest, CumulVarsSetCorrectlyWithPrecedences) {
  const Assignment* const assignment = SetUpModelAndSolve();
  ASSERT_NE(assignment, nullptr);

  // Verify the route is 0-->1-->2-->3-->4-->5
  for (int i = 0; i < model_->Size(); i++) {
    ASSERT_EQ(assignment->Value(model_->NextVar(i)), i + 1);
  }

  const int kExpectedCumulValues[] = {0, 1, 4, 5, 10, 11};
  const Dimension* const dimension =
      model_->GetMutableDimension(dimension_name_);
  ASSERT_EQ(dimension->cumuls().size(), std::size(kExpectedCumulValues));

  const Assignment* const packed_assignment =
      model_->PackCumulsOfOptimizerDimensionsFromAssignment(
          assignment, kPackCumulsDurationLimit);
  ASSERT_NE(packed_assignment, nullptr);
  for (int i = 0; i < dimension->cumuls().size(); i++) {
    EXPECT_EQ(packed_assignment->Value(dimension->CumulVar(i)),
              kExpectedCumulValues[i]);
  }
}

INSTANTIATE_TEST_SUITE_P(WithAndWithoutUnaryCallbacks,
                         DimensionNodePrecedenceTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

class DimensionVehicleSpanCostTest
    : public BaseRoutingTest,
      public testing::WithParamInterface<::std::tuple<bool, bool, bool, bool>> {
 protected:
  DimensionVehicleSpanCostTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/::testing::get<0>(GetParam()),
            /*use_light_propagation=*/::testing::get<1>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<2>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<3>(GetParam())) {
  }
  void SetUp() override {
    BaseRoutingTest::SetUp();
    dimension_transit_spans_ = {7, 0, 1};
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    // We'll set up a few dimensions, all identical except in the value of their
    // span across transit arcs.
    manager_ = std::make_unique<IndexManager>(
        /*num_nodes=*/7, /*num_vehicles=*/4, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    // One node on the route of vehicle #0 and #3, and two nodes on the routes
    // of vehicle #1 and #2.
    const int64_t v0_node0 = manager_->NodeToIndex(NodeIndex(1));
    const int64_t v1_node0 = manager_->NodeToIndex(NodeIndex(2));
    const int64_t v1_node1 = manager_->NodeToIndex(NodeIndex(3));
    const int64_t v2_node0 = manager_->NodeToIndex(NodeIndex(4));
    const int64_t v2_node1 = manager_->NodeToIndex(NodeIndex(5));
    const int64_t v3_node0 = manager_->NodeToIndex(NodeIndex(6));
    model->VehicleVar(v0_node0)->SetValue(0);
    model->VehicleVar(v1_node0)->SetValue(1);
    model->VehicleVar(v1_node1)->SetValue(1);
    model->VehicleVar(v2_node0)->SetValue(2);
    model->VehicleVar(v2_node1)->SetValue(2);
    model->VehicleVar(v3_node0)->SetValue(3);
    for (int i = 0; i < dimension_transit_spans_.size(); ++i) {
      const int transit_value_on_all_arcs = dimension_transit_spans_[i];
      const std::string dimension_name = absl::StrFormat(
          "Dimension%dWithTransit%d_", i, transit_value_on_all_arcs);
      const int callback =
          use_unary_callback_
              ? model->RegisterUnaryTransitCallback(
                    [transit_value_on_all_arcs](int64_t) {
                      return transit_value_on_all_arcs;
                    })
              : model->RegisterTransitCallback(
                    [transit_value_on_all_arcs](int64_t, int64_t) {
                      return transit_value_on_all_arcs;
                    });
      model->AddDimension(
          /*evaluator_index=*/callback,
          /*slack_max=*/kint64max,
          /*capacity=*/kint64max,
          /*fix_start_cumul_to_zero=*/false,
          /*name=*/dimension_name);
      Dimension* dimension = model->GetMutableDimension(dimension_name);

      dimension->SetSpanCostCoefficientForVehicle(
          /*coefficient=*/10, /*vehicle=*/2);
      dimension->SetSpanCostCoefficientForVehicle(
          /*coefficient=*/10, /*vehicle=*/0);
      dimension->SetSpanCostCoefficientForVehicle(
          /*coefficient=*/20, /*vehicle=*/2);  // Should override 10.
      dimension->SetSpanCostCoefficientForVehicle(
          /*coefficient=*/0, /*vehicle=*/3);  // Same as no span cost.

      // The node on route #0 only has a Max; the overall route span should be
      // 2 * transit_value_on_all_arcs.
      dimension->CumulVar(v0_node0)->SetMax(333);

      // The nodes on route #1 span more, but it won't affect the global cost
      // since vehicle #1 doesn't have a span cost.
      dimension->CumulVar(v1_node0)->SetMin(300);
      dimension->CumulVar(v1_node1)->SetMax(100);

      // Route #2 should span 20 + 2 * transit_value_on_all_arcs.
      dimension->CumulVar(v2_node0)->SetMin(300);
      dimension->CumulVar(v2_node1)->SetMax(280);

      // Route #3's span, like route #1's, doesn't matter.
    }
    return model;
  }

  // Vehicle #1 and #3 don't have a (non-zero) dimension span cost.
  int VehicleCostCount() const override { return 2; }

  bool HasHomogeneousCosts() const override { return false; }

  std::vector<int> dimension_transit_spans_;
};

TEST_P(DimensionVehicleSpanCostTest, GetObjectiveValue) {
  const int64_t num_dimensions = dimension_transit_spans_.size();
  const int64_t sum_of_dimension_transit_span = std::accumulate(
      dimension_transit_spans_.begin(), dimension_transit_spans_.end(), 0);
  const int64_t kExpectedCost =
      /*route 0*/ 10 * (2 * sum_of_dimension_transit_span) +
      /*route 2*/ 20 *
          (20 * num_dimensions + 2 * sum_of_dimension_transit_span);
  const Assignment* const solution = SetUpModelAndSolve();
  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(kExpectedCost, solution->ObjectiveValue());
  EXPECT_EQ(4, model_->GetVehicleClassesCount());
}

INSTANTIATE_TEST_SUITE_P(WithParameters, DimensionVehicleSpanCostTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

class DimensionVehicleSlackCostTest
    : public BaseRoutingTest,
      public testing::WithParamInterface<
          ::testing::tuple<bool, bool, bool, bool>> {
 protected:
  DimensionVehicleSlackCostTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/::testing::get<0>(GetParam()),
            /*use_light_propagation=*/::testing::get<1>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<2>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<3>(GetParam())) {
  }
  void SetUp() override {
    BaseRoutingTest::SetUp();
    dimension_transit_spans_ = {7, 0, 1};
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    // We'll set up a few dimensions, all identical except in the value of their
    // span across transit arcs.
    manager_ = std::make_unique<IndexManager>(
        /*num_nodes=*/7, /*num_vehicles=*/4, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    // One node on the route of vehicle #0 and #3, and two nodes on the routes
    // of vehicle #1 and #2.
    const int64_t v0_node0 = manager_->NodeToIndex(NodeIndex(1));
    const int64_t v1_node0 = manager_->NodeToIndex(NodeIndex(2));
    const int64_t v1_node1 = manager_->NodeToIndex(NodeIndex(3));
    const int64_t v2_node0 = manager_->NodeToIndex(NodeIndex(4));
    const int64_t v2_node1 = manager_->NodeToIndex(NodeIndex(5));
    const int64_t v3_node0 = manager_->NodeToIndex(NodeIndex(6));
    model->VehicleVar(v0_node0)->SetValue(0);
    model->VehicleVar(v1_node0)->SetValue(1);
    model->VehicleVar(v1_node1)->SetValue(1);
    model->VehicleVar(v2_node0)->SetValue(2);
    model->VehicleVar(v2_node1)->SetValue(2);
    model->VehicleVar(v3_node0)->SetValue(3);
    for (int i = 0; i < dimension_transit_spans_.size(); ++i) {
      const int transit_value_on_all_arcs = dimension_transit_spans_[i];
      const std::string dimension_name = absl::StrFormat(
          "Dimension%dWithTransit%d_", i, transit_value_on_all_arcs);
      const int callback =
          use_unary_callback_
              ? model->RegisterUnaryTransitCallback(
                    [transit_value_on_all_arcs](int64_t) {
                      return transit_value_on_all_arcs;
                    })
              : model->RegisterTransitCallback(
                    [transit_value_on_all_arcs](int64_t, int64_t) {
                      return transit_value_on_all_arcs;
                    });
      model->AddDimension(
          /*evaluator_index=*/callback,
          /*slack_max=*/kint64max,
          /*capacity=*/kint64max,
          /*fix_start_cumul_to_zero=*/false,
          /*name=*/dimension_name);
      Dimension* dimension = model->GetMutableDimension(dimension_name);

      dimension->SetSlackCostCoefficientForVehicle(
          /*coefficient=*/10, /*vehicle=*/2);
      dimension->SetSlackCostCoefficientForVehicle(
          /*coefficient=*/10, /*vehicle=*/0);
      dimension->SetSlackCostCoefficientForVehicle(
          /*coefficient=*/20, /*vehicle=*/2);  // Should override 10.
      dimension->SetSlackCostCoefficientForVehicle(
          /*coefficient=*/0, /*vehicle=*/3);  // Same as no span cost.

      // The node on route #0 only has a Max, leading to a min slack of 0.
      dimension->CumulVar(v0_node0)->SetMax(333);

      // Route #1 has a slack of 200 - transit, but it won't affect the global
      // cost since vehicle #1 doesn't have a slack cost.
      dimension->CumulVar(v1_node0)->SetMin(300);
      dimension->CumulVar(v1_node1)->SetMax(100);

      // Route #2 will have 20 - transit of slack.
      dimension->CumulVar(v2_node0)->SetMin(300);
      dimension->CumulVar(v2_node1)->SetMax(280);

      // Route #3's slack, like route #1's, doesn't matter.
    }
    return model;
  }

  // Vehicle #1 and #3 don't have a (non-zero) dimension slack cost.
  int VehicleCostCount() const override { return 2; }

  bool HasHomogeneousCosts() const override { return false; }

  std::vector<int> dimension_transit_spans_;
};

TEST_P(DimensionVehicleSlackCostTest, GetObjectiveValue) {
  const int64_t num_dimensions = dimension_transit_spans_.size();
  const int64_t sum_of_dimension_transits = std::accumulate(
      dimension_transit_spans_.begin(), dimension_transit_spans_.end(), 0);
  const int64_t kExpectedCost =
      /*route 2*/ 20 * (20 * num_dimensions - sum_of_dimension_transits);
  const Assignment* const solution = SetUpModelAndSolve();
  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(kExpectedCost, solution->ObjectiveValue());
  EXPECT_EQ(4, model_->GetVehicleClassesCount());
}

INSTANTIATE_TEST_SUITE_P(WithParameters, DimensionVehicleSlackCostTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

class DimensionCostTest : public BaseRoutingTest,
                          public testing::WithParamInterface<
                              ::testing::tuple<bool, bool, bool, bool>> {
 protected:
  DimensionCostTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/::testing::get<0>(GetParam()),
            /*use_light_propagation=*/::testing::get<1>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<2>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<3>(GetParam())) {
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    const char* kDimension = "test_dimension";
    const char* kDummyDimension = "dummy_dimension";
    const int64_t kCostCoef = 2;
    manager_ = std::make_unique<IndexManager>(5, 2, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    const int callback =
        use_unary_callback_
            ? model->RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model->RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    model->AddDimension(callback, kint64max, kint64max,
                        /*fix_start_cumul_to_zero=*/false, kDimension);
    Dimension* const dimension = model->GetMutableDimension(kDimension);
    dimension->SetSpanCostCoefficientForAllVehicles(kCostCoef);
    dimension->CumulVar(manager_->NodeToIndex(NodeIndex(1)))->SetMax(3);
    dimension->CumulVar(manager_->NodeToIndex(NodeIndex(2)))->SetRange(30, 60);
    dimension->CumulVar(manager_->NodeToIndex(NodeIndex(3)))
        ->SetRange(105, 115);
    dimension->CumulVar(manager_->NodeToIndex(NodeIndex(4)))
        ->SetRange(110, 130);
    model->AddDimension(callback, 0, kint64max,
                        /*fix_start_cumul_to_zero=*/false, kDummyDimension);
    model->GetMutableDimension(kDummyDimension)
        ->SetSpanCostCoefficientForAllVehicles(0);
    return model;
  }

  int VehicleCostCount() const override { return 1; }
};

TEST_P(DimensionCostTest, GetObjectiveValue) {
  const int64_t kExpectedCost = 2 * (4 + (30 - 3) + 1);
  const Assignment* const assignment = SetUpModelAndSolve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(kExpectedCost, assignment->ObjectiveValue());
  EXPECT_EQ(1, model_->GetVehicleClassesCount());
}

TEST_P(DimensionCostTest, NonExistingDimension) {
  SetUpModel();
  EXPECT_EQ(model_->GetMutableDimension("fake"), nullptr);
}

INSTANTIATE_TEST_SUITE_P(WithParameters, DimensionCostTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

typedef DimensionCostTest DimensionCostDeathTest;
TEST_P(DimensionCostDeathTest, NonExistingDimension) {
  SetUpModel();
  EXPECT_THROW(model_->GetDimensionOrDie("fake"), std::out_of_range);
}

INSTANTIATE_TEST_SUITE_P(WithParameters, DimensionCostDeathTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

TEST(CumulVarSoftBoundTest, SetAndGet) {
  const char* kDimension = "test_dimension";
  const IndexManager manager(5, 1, NodeIndex(0));
  Model model(manager);
  const int callback =
      model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  model.AddDimension(callback, 0, kint64max,
                     /*fix_start_cumul_to_zero=*/false, kDimension);
  Dimension* const dimension = model.GetMutableDimension(kDimension);
  dimension->CumulVar(1)->SetRange(10, 20);
  EXPECT_EQ(dimension->GetCumulVarSoftLowerBoundCoefficient(1), 0);
  EXPECT_EQ(dimension->GetCumulVarSoftLowerBound(1), 10);
  EXPECT_EQ(dimension->GetCumulVarSoftUpperBoundCoefficient(1), 0);
  EXPECT_EQ(dimension->GetCumulVarSoftUpperBound(1), 20);

  // Set a soft lower bound lower than the hard one, hence ineffective.
  dimension->SetCumulVarSoftLowerBound(1, 3, 4);
  EXPECT_EQ(dimension->GetCumulVarSoftLowerBoundCoefficient(1), 0);
  EXPECT_EQ(dimension->GetCumulVarSoftLowerBound(1), 10);  // hard lower bound
  EXPECT_FALSE(dimension->HasCumulVarSoftLowerBounds());

  // Set a soft upper bound higher than the hard one, hence ineffective.
  dimension->SetCumulVarSoftUpperBound(1, 25, 6);
  EXPECT_EQ(dimension->GetCumulVarSoftUpperBoundCoefficient(1), 0);
  EXPECT_EQ(dimension->GetCumulVarSoftUpperBound(1), 20);  // hard upper bound
  EXPECT_FALSE(dimension->HasCumulVarSoftUpperBounds());

  for (int it = 0; it < 2; ++it) {
    dimension->SetCumulVarSoftLowerBound(1, 12, 4);
    EXPECT_EQ(dimension->GetCumulVarSoftLowerBoundCoefficient(1), 4);
    EXPECT_EQ(dimension->GetCumulVarSoftLowerBound(1), 12);
    dimension->SetCumulVarSoftUpperBound(1, 5, 6);
    EXPECT_EQ(dimension->GetCumulVarSoftUpperBoundCoefficient(1), 6);
    EXPECT_EQ(dimension->GetCumulVarSoftUpperBound(1), 5);

    // Set a cost of 0: The soft bound is inactive, ignore the bound value.
    dimension->SetCumulVarSoftLowerBound(1, 7, 0);
    EXPECT_EQ(dimension->GetCumulVarSoftLowerBoundCoefficient(1), 0);
    dimension->SetCumulVarSoftUpperBound(1, 9, 0);
    EXPECT_EQ(dimension->GetCumulVarSoftUpperBoundCoefficient(1), 0);

    EXPECT_TRUE(dimension->HasCumulVarSoftLowerBounds());
    EXPECT_TRUE(dimension->HasCumulVarSoftUpperBounds());
  }
}

// Testing on a model with one depot, 3 nodes with soft deadlines, 1 node with
// a hard deadline.
class CumulVarSoftUpperBoundTest
    : public BaseRoutingTest,
      public testing::WithParamInterface<
          ::testing::tuple<bool, bool, bool, bool>> {
 protected:
  CumulVarSoftUpperBoundTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/::testing::get<0>(GetParam()),
            /*use_light_propagation=*/::testing::get<1>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<2>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<3>(GetParam())) {
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    const char* kDimension = "test_dimension";
    const int64_t kCumulUpperBounds[] = {0, 5, 10};
    const int64_t kCumulCosts[] = {5, 1, 2};
    manager_ = std::make_unique<IndexManager>(5, 1, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    const int callback =
        use_unary_callback_
            ? model->RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model->RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    model->AddDimension(callback, 0, kint64max,
                        /*fix_start_cumul_to_zero=*/false, kDimension);
    Dimension* const dimension = model->GetMutableDimension(kDimension);
    for (NodeIndex node(1); node < NodeIndex(4); ++node) {
      dimension->SetCumulVarSoftUpperBound(manager_->NodeToIndex(node),
                                           kCumulUpperBounds[node.value() - 1],
                                           kCumulCosts[node.value() - 1]);
    }
    dimension->SetCumulVarSoftUpperBound(
        model->Start(0), kCumulUpperBounds[0] - 1, kCumulCosts[0]);
    dimension->SetCumulVarSoftUpperBound(model->End(0), kCumulUpperBounds[0],
                                         kCumulCosts[0]);
    dimension->CumulVar(4)->SetMax(100);
    return model;
  }
};

TEST_P(CumulVarSoftUpperBoundTest, GetObjectiveValue) {
  const int64_t kExpectedCost = 5 * 1 + 1 * 0 + 2 * 0 + 5 * 1 + 5 * 5;
  const Assignment* const assignment = SetUpModelAndSolve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(kExpectedCost, assignment->ObjectiveValue());
  EXPECT_EQ(1, model_->GetVehicleClassesCount());
}

TEST_P(CumulVarSoftUpperBoundTest, SoftUpperBoundAccessors) {
  SetUpModelAndSolve();
  Dimension* const dimension = model_->GetMutableDimension("test_dimension");
  const int64_t kExpectedCumulUpperBounds[] = {-1, 0, 5, 10, 100, 0};
  const int64_t kExpectedCumulCosts[] = {5, 5, 1, 2, 0, 5};
  for (int64_t index = 0; index < model_->Size(); ++index) {
    EXPECT_EQ(kExpectedCumulCosts[index],
              dimension->GetCumulVarSoftUpperBoundCoefficient(index));
    if (kExpectedCumulCosts[index] != 0) {
      EXPECT_EQ(kExpectedCumulUpperBounds[index],
                dimension->GetCumulVarSoftUpperBound(index));
    }
  }
}

INSTANTIATE_TEST_SUITE_P(WithParameters, CumulVarSoftUpperBoundTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

using UnactiveCumulVarSoftUpperBoundTest = ::testing::TestWithParam<bool>;
TEST_P(UnactiveCumulVarSoftUpperBoundTest, GetObjectiveValue) {
  const char* kDimension = "test_dimension";
  const int64_t kSoftBounds[] = {-10, 0, 10, 100};
  for (int i = 0; i < std::size(kSoftBounds); ++i) {
    IndexManager manager(2, 1, NodeIndex(0));
    Model model(manager);
    const int callback =
        GetParam()
            ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model.RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    model.AddDimension(callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/true, kDimension);
    NodeIndex node(1);
    int64_t index = manager.NodeToIndex(node);
    model.AddDisjunction(std::vector<int64_t>({index}), 0);
    model.ActiveVar(index)->SetValue(0);
    Dimension* const dimension = model.GetMutableDimension(kDimension);
    dimension->CumulVar(index)->SetRange(0, 10);
    dimension->SetCumulVarSoftUpperBound(index, kSoftBounds[i], 1);
    const Assignment* const assignment = model.Solve();
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(0, assignment->ObjectiveValue());
  }
}

INSTANTIATE_TEST_SUITE_P(WithOrWithoutUnaryCallback,
                         UnactiveCumulVarSoftUpperBoundTest, ::testing::Bool());

// Testing on a model found by pilot user: one depot, 2 nodes with soft
// deadlines, and vehicle with span cost (to exercise the lp-based finalizer).
// Without the proper finalizer finds a solution with a cost of 129376580.
class CumulVarSoftUpperBoundWithSpanCostTest
    : public BaseRoutingTest,
      public testing::WithParamInterface<::testing::tuple<bool, bool, bool>> {
 protected:
  CumulVarSoftUpperBoundWithSpanCostTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/false,
            /*use_light_propagation=*/::testing::get<0>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<1>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<2>(GetParam())) {
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    const char* kDimension = "test_dimension";
    const int64_t kCumulLowerBounds[] = {1529758800, 1529740800, 1529741700};
    const int64_t kCumulUpperBounds[] = {1529759700, 1529755200, 1529772300};
    const int64_t kCumulSoftUpperBounds[] = {1529759220, 1529740800};
    const int64_t kCumulCosts[] = {10000, 10000};
    manager_ = std::make_unique<IndexManager>(3, 1, NodeIndex(2));
    Model* const model = new Model(*manager_, parameters);
    const int callback =
        model->RegisterTransitCallback([this](int64_t i, int64_t j) {
          const auto from = manager_->IndexToNode(i);
          const auto to = manager_->IndexToNode(j);
          if (from == NodeIndex(2) && to == NodeIndex(1)) return 974;
          if (from == NodeIndex(0) && to == NodeIndex(2)) return 1812 + 1800;
          if (from == NodeIndex(1) && to == NodeIndex(0)) return 1472 + 3600;
          return 100000;
        });
    model->AddDimension(callback, 7258114800, kint64max,
                        /*fix_start_cumul_to_zero=*/false, kDimension);
    Dimension* const dimension = model->GetMutableDimension(kDimension);
    dimension->SetSpanCostCoefficientForAllVehicles(10);
    for (NodeIndex node(0); node < NodeIndex(2); ++node) {
      dimension->CumulVar(manager_->NodeToIndex(node))
          ->SetRange(kCumulLowerBounds[node.value()],
                     kCumulUpperBounds[node.value()]);
      dimension->SetCumulVarSoftUpperBound(manager_->NodeToIndex(node),
                                           kCumulSoftUpperBounds[node.value()],
                                           kCumulCosts[node.value()]);
    }
    dimension->CumulVar(model->Start(0))
        ->SetRange(kCumulLowerBounds[2], kCumulUpperBounds[2]);
    dimension->CumulVar(model->End(0))
        ->SetRange(kCumulLowerBounds[2], kCumulUpperBounds[2]);
    return model;
  }
  int VehicleCostCount() const override { return 1; }
  RoutingSearchParameters GetSearchParameters() override {
    return DefaultRoutingSearchParameters();
  }
};

TEST_P(CumulVarSoftUpperBoundWithSpanCostTest, GetObjectiveValue) {
  const int64_t kExpectedCost =
      /* node 1 late */ (1529742674 - 1529740800) * 10000 +
      /* span cost */ (1529762412 - 1529741700) * 10;
  const Assignment* const assignment = SetUpModelAndSolve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(kExpectedCost, assignment->ObjectiveValue());
  EXPECT_EQ(1, model_->GetVehicleClassesCount());
}

INSTANTIATE_TEST_SUITE_P(WithParameters, CumulVarSoftUpperBoundWithSpanCostTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

// Model with one depot, 1 node and 2 vehicles, used to test combinations of
// lower/upper cumul costs on vehicle start/ends without span cost.
class CumulVarSoftBoundWithoutSpanCostTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<::testing::tuple<bool, bool, bool>> {
 protected:
  CumulVarSoftBoundWithoutSpanCostTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/false,
            /*use_light_propagation=*/::testing::get<0>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<1>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<2>(GetParam())) {
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    const int kNumNodes = 2;
    const int kNumVehicles = 2;
    manager_ =
        std::make_unique<IndexManager>(kNumNodes, kNumVehicles, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    const int callback =
        model->RegisterTransitCallback([](int64_t, int64_t) { return 0; });
    model->AddDimension(callback, kint64max, kint64max,
                        /*fix_start_cumul_to_zero=*/false, kDimension);
    for (int vehicle = 0; vehicle < kNumVehicles; vehicle++) {
      model->SetVehicleUsedWhenEmpty(true, vehicle);
    }
    return model;
  }
  int VehicleCostCount() const override { return 0; }
  RoutingSearchParameters GetSearchParameters() override {
    return DefaultRoutingSearchParameters();
  }

  static const char kDimension[];
};

constexpr char CumulVarSoftBoundWithoutSpanCostTest::kDimension[] =
    "test_dimension";

// With a soft cumul lower bound at the vehicle end.
TEST_P(CumulVarSoftBoundWithoutSpanCostTest, EndSoftLowerBoundCumulValue) {
  SetUpModel();
  Dimension* const dimension = model_->GetMutableDimension(kDimension);
  dimension->SetCumulVarSoftLowerBound(model_->End(0), 1000, 1);
  const Assignment* const assignment = Solve();
  ASSERT_NE(assignment, nullptr);

  EXPECT_EQ(1000, assignment->Min(dimension->CumulVar(model_->End(0))))
      << assignment->DebugString();
  EXPECT_EQ(1, model_->GetVehicleClassesCount());
}

// With a soft cumul upper bound at the vehicle start.
TEST_P(CumulVarSoftBoundWithoutSpanCostTest, StartSoftUpperBoundCumulValue) {
  SetUpModel();
  Dimension* const dimension = model_->GetMutableDimension(kDimension);
  dimension->SetCumulVarSoftUpperBound(model_->Start(0), 500, 1);
  const Assignment* const assignment = Solve();
  ASSERT_NE(assignment, nullptr);

  EXPECT_EQ(500, assignment->Max(dimension->CumulVar(model_->Start(0))))
      << assignment->DebugString();
  EXPECT_EQ(1, model_->GetVehicleClassesCount());
}

// With a soft cumul lower bound at the vehicle end, and soft upper bound at the
// vehicle start, to exercise the LP-based finalizer. We also set a different
// non-zero minimum to the start of the two vehicles to test vehicle offsets.
// Without packing, the LP finalizer sets the start of the vehicle to 0, and
// calling the packing pushes it to 500 as expected.
TEST_P(CumulVarSoftBoundWithoutSpanCostTest,
       PackingWithEndSoftLowerAndStartSoftUpperBoundAndOffsets) {
  SetUpModel();
  Dimension* const dimension = model_->GetMutableDimension(kDimension);
  dimension->CumulVar(model_->Start(0))->SetMin(250);
  dimension->SetCumulVarSoftUpperBound(model_->Start(0), 500, 1);
  dimension->SetCumulVarSoftLowerBound(model_->End(0), 1000, 1);

  dimension->CumulVar(model_->Start(1))->SetMin(5000);

  const Assignment* assignment = Solve();
  ASSERT_NE(assignment, nullptr);

  const IntVar* const start_cumul = dimension->CumulVar(model_->Start(0));
  const IntVar* const end_cumul = dimension->CumulVar(model_->End(0));
  // NOTE: If the route cumuls are packed already, then the packing function
  // below is not being tested properly.
  EXPECT_TRUE((assignment->Value(start_cumul) != 500) ||
              (assignment->Value(end_cumul) != 1000))
      << "Result is the same with or without packing, test should be adapted.";

  assignment = model_->PackCumulsOfOptimizerDimensionsFromAssignment(
      assignment, kPackCumulsDurationLimit);
  ASSERT_NE(assignment, nullptr);

  EXPECT_EQ(500, assignment->Value(start_cumul)) << assignment->DebugString();
  EXPECT_EQ(1000, assignment->Value(end_cumul)) << assignment->DebugString();
}

INSTANTIATE_TEST_SUITE_P(WithParameters, CumulVarSoftBoundWithoutSpanCostTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

// Tests a model with disjoint windows and soft dealines which cannot be solved
// and packed optimally by GLOP alone. Making sure the cost value is in sync
// with the (suboptimal) value found by GLOP.
// The data was taken from a tour_optimization stress test.
TEST(SoftBoundsAndDisjointWindowsTest, RelaxedPackingReportingProperCost) {
  const IndexManager manager(6, 1, NodeIndex(0));
  const bool kDisableLPScheduling[] = {true, false};
  // NOTE: Since the model is small, the solver actually manages to find the
  // more optimal solution without the relaxed LP scheduling.
  const int64_t kExpectedObjectiveValue[] = {
      200000 + (27068679 - 27068442) * 559,
      200000 + (27068679 - 24649276) * 559};
  for (int s = 0; s < std::size(kDisableLPScheduling); s++) {
    Model model(manager);
    const int callback = model.RegisterTransitCallback([](int64_t i, int64_t) {
      // Forced route:   2 3 1 4
      if (i == 1) return int64_t{52};
      if (i == 2) return int64_t{49};
      if (i == 3) return int64_t{136};
      return int64_t{0};
    });
    model.AddDimension(callback, 31500000, 31500000,
                       /*fix_start_cumul_to_zero=*/false, "time");
    Dimension* const dimension = model.GetMutableDimension("time");
    dimension->CumulVar(1)->SetMin(24649461);
    model.solver()->AddConstraint(model.solver()->MakeNotMemberCt(
        dimension->CumulVar(2), std::vector<int64_t>{21497334},
        std::vector<int64_t>{27068441}));
    model.solver()->AddConstraint(model.solver()->MakeNotMemberCt(
        dimension->CumulVar(3), std::vector<int64_t>{22080463},
        std::vector<int64_t>{23657379}));
    dimension->SetCumulVarSoftUpperBound(5, 8419894, 5584);
    dimension->SetSpanCostCoefficientForAllVehicles(559);
    model.AddDisjunction({5}, 200000, /*max_cardinality=*/1);
    model.NextVar(0)->SetValue(2);
    model.NextVar(2)->SetValue(3);
    model.NextVar(3)->SetValue(1);
    model.NextVar(1)->SetValue(4);
    model.NextVar(4)->SetValue(6);
    model.NextVar(5)->SetValue(5);
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_mixed_integer_scheduling_solver(
        RoutingSearchParameters::SCHEDULING_GLOP);
    parameters.set_disable_scheduling_beware_this_may_degrade_performance(
        kDisableLPScheduling[s]);
    model.CloseModelWithParameters(parameters);
    const Assignment* assignment = model.Solve();
    ASSERT_NE(assignment, nullptr);
    assignment = model.PackCumulsOfOptimizerDimensionsFromAssignment(
        assignment, kPackCumulsDurationLimit);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(assignment->ObjectiveValue(), kExpectedObjectiveValue[s]);
  }
}

// Tests a model with a large soft lower bound to make sure the underlying
// CP-SAT model doesn't truncate the corresponding cumul before the lower bound.
// The data was taken from a tour_optimization stress test.
TEST(SoftBoundsAndDisjointWindowsTest, PackWithLargeHorizon) {
  const IndexManager manager(3, 1, NodeIndex(0));
  for (const bool disable_lp_scheduling : {true, false}) {
    Model model(manager);
    const int callback =
        model.RegisterTransitCallback([](int64_t i, int64_t j) {
          if (i == 1 && j == 2) return int64_t{337};
          return int64_t{0};
        });
    model.AddDimension(callback, 31500000, 31500000,
                       /*fix_start_cumul_to_zero=*/false, "time");
    Dimension* const dimension = model.GetMutableDimension("time");
    model.solver()->AddConstraint(model.solver()->MakeNotMemberCt(
        dimension->CumulVar(1), std::vector<int64_t>{8617379},
        std::vector<int64_t>{28655562}));
    dimension->SetCumulVarSoftLowerBound(2, 13394136, 192);
    model.AddPickupAndDelivery(1, 2);
    dimension->SetSpanCostCoefficientForAllVehicles(512);

    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_disable_scheduling_beware_this_may_degrade_performance(
        disable_lp_scheduling);
    const Assignment* assignment = model.SolveWithParameters(parameters);
    ASSERT_NE(assignment, nullptr);
    assignment = model.PackCumulsOfOptimizerDimensionsFromAssignment(
        assignment, kPackCumulsDurationLimit);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(28655563, assignment->Value(dimension->CumulVar(model.Start(0))));
    EXPECT_EQ(28655563, assignment->Value(dimension->CumulVar(1)));
    EXPECT_EQ(28655900, assignment->Value(dimension->CumulVar(2)));
    EXPECT_EQ(28655900, assignment->Value(dimension->CumulVar(model.End(0))));
  }
}

Model::RouteDimensionTravelInfo CreateDimensionTravelInfo() {
  constexpr int kNumTransitions = 2;
  Model::RouteDimensionTravelInfo travel_info;
  travel_info.travel_cost_coefficient = 0;  // No cost
  travel_info.transition_info.resize(kNumTransitions);

  const std::vector<absl::InlinedVector<int64_t, 8>> x_anchors = {{0, 50, 100},
                                                                  {0, 80, 120}};
  const std::vector<absl::InlinedVector<int64_t, 8>> y_anchors = {
      {70, 50, 30}, {80, 120, 80}};
  for (int k = 0; k < kNumTransitions; ++k) {
    auto& transition = travel_info.transition_info[k];
    transition.pre_travel_transit_value = 0;
    transition.post_travel_transit_value = 0;
    transition.travel_start_dependent_travel =
        FloatSlopePiecewiseLinearFunction(x_anchors[k], y_anchors[k]);
  }

  PopulateTravelInfoCompressionCost(
      travel_info,
      /*lower_bound_compressed_travel_factor=*/0.7,
      /*upper_bound_travel_factor=*/3.0,
      /*potential_cost=*/1 << 12,
      /*static_travel_durations=*/{40, 60},
      /*num_points_lin_cost=*/30);

  return travel_info;
}

// Testing on a model found by pilot user: one depot, 2 nodes with soft
// deadlines and availabilities, and vehicle with no span cost (to exercise the
// lp-based finalizer).
class CumulVarSoftLowerUpperBoundWithoutSpanCostTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<bool, bool, bool, bool>> {
 protected:
  CumulVarSoftLowerUpperBoundWithoutSpanCostTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/false,
            /*use_light_propagation=*/::testing::get<1>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<2>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<3>(GetParam())) {
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    const char* kDimension = "test_dimension";
    const int64_t kCumulLowerBounds[] = {1529758800, 1529740800, 1529741700};
    const int64_t kCumulUpperBounds[] = {1529759700, 1529755200, 1529772300};
    const int64_t kCumulSoftLowerBounds[] = {1529759220, 1529740800};
    const int64_t kCumulSoftUpperBounds[] = {1529759220, 1529740800};
    const int64_t kCumulCosts[] = {10000, 10000};
    manager_ = std::make_unique<IndexManager>(3, 1, NodeIndex(2));
    Model* const model = new Model(*manager_, parameters);
    const int callback =
        model->RegisterTransitCallback([this](int64_t i, int64_t j) {
          const auto from = manager_->IndexToNode(i);
          const auto to = manager_->IndexToNode(j);
          if (from == NodeIndex(2) && to == NodeIndex(1)) return 974;
          if (from == NodeIndex(0) && to == NodeIndex(2)) return 1812 + 1800;
          if (from == NodeIndex(1) && to == NodeIndex(0)) return 1472 + 3600;
          return 100000;
        });
    model->AddDimension(callback, 7258114800, kint64max,
                        /*fix_start_cumul_to_zero=*/false, kDimension);
    Dimension* const dimension = model->GetMutableDimension(kDimension);
    for (NodeIndex node(0); node < NodeIndex(2); ++node) {
      dimension->CumulVar(manager_->NodeToIndex(node))
          ->SetRange(kCumulLowerBounds[node.value()],
                     kCumulUpperBounds[node.value()]);
      dimension->SetCumulVarSoftLowerBound(manager_->NodeToIndex(node),
                                           kCumulSoftLowerBounds[node.value()],
                                           kCumulCosts[node.value()]);
      dimension->SetCumulVarSoftUpperBound(manager_->NodeToIndex(node),
                                           kCumulSoftUpperBounds[node.value()],
                                           kCumulCosts[node.value()]);
    }
    dimension->CumulVar(model->Start(0))
        ->SetRange(kCumulLowerBounds[2], kCumulUpperBounds[2]);
    dimension->CumulVar(model->End(0))
        ->SetRange(kCumulLowerBounds[2], kCumulUpperBounds[2]);
    return model;
  }
  int VehicleCostCount() const override { return 0; }
  RoutingSearchParameters GetSearchParameters() override {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_disable_scheduling_beware_this_may_degrade_performance(
        ::testing::get<0>(GetParam()));
    return parameters;
  }
};

TEST_P(CumulVarSoftLowerUpperBoundWithoutSpanCostTest, GetObjectiveValue) {
  const int64_t kExpectedCost =
      /* node 1 late */ (1529742674 - 1529740800) * 10000;
  const Assignment* const original_assignment = SetUpModelAndSolve();
  const Assignment* const packed_assignment =
      model_->PackCumulsOfOptimizerDimensionsFromAssignment(
          original_assignment, kPackCumulsDurationLimit);
  for (const Assignment* assignment :
       {original_assignment, packed_assignment}) {
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(kExpectedCost, assignment->ObjectiveValue())
        << assignment->DebugString();
  }
  EXPECT_EQ(1, model_->GetVehicleClassesCount());
}

INSTANTIATE_TEST_SUITE_P(
    WithParameters, CumulVarSoftLowerUpperBoundWithoutSpanCostTest,
    ::testing::Combine(::testing::Bool(), ::testing::Bool(), ::testing::Bool(),
                       ::testing::Bool()));

// Testing on a model found by pilot user: one depot, 2 nodes with soft
// deadlines and availabilities, and vehicle with span cost (to exercise the
// lp-based finalizer).
class CumulVarSoftLowerUpperBoundWithSpanCostTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<bool, bool, bool, bool>> {
 protected:
  CumulVarSoftLowerUpperBoundWithSpanCostTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/false,
            /*use_light_propagation=*/::testing::get<1>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<2>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<3>(GetParam())) {
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    const char* kDimension = "test_dimension";
    const int64_t kCumulLowerBounds[] = {1529758800, 1529740800, 1529741700};
    const int64_t kCumulUpperBounds[] = {1529759700, 1529755200, 1529772300};
    const int64_t kCumulSoftLowerBounds[] = {1529759220, 1529740800};
    const int64_t kCumulSoftUpperBounds[] = {1529759220, 1529740800};
    const int64_t kCumulCosts[] = {10000, 10000};
    manager_ = std::make_unique<IndexManager>(3, 1, NodeIndex(2));
    Model* const model = new Model(*manager_, parameters);
    const int callback =
        model->RegisterTransitCallback([this](int64_t i, int64_t j) {
          const auto from = manager_->IndexToNode(i);
          const auto to = manager_->IndexToNode(j);
          if (from == NodeIndex(2) && to == NodeIndex(1)) return 974;
          if (from == NodeIndex(0) && to == NodeIndex(2)) return 1812 + 1800;
          if (from == NodeIndex(1) && to == NodeIndex(0)) return 1472 + 3600;
          return 100000;
        });
    model->AddDimension(callback, 7258114800, kint64max,
                        /*fix_start_cumul_to_zero=*/false, kDimension);
    Dimension* const dimension = model->GetMutableDimension(kDimension);
    dimension->SetSpanCostCoefficientForAllVehicles(10);
    for (NodeIndex node(0); node < NodeIndex(2); ++node) {
      dimension->CumulVar(manager_->NodeToIndex(node))
          ->SetRange(kCumulLowerBounds[node.value()],
                     kCumulUpperBounds[node.value()]);
      dimension->SetCumulVarSoftLowerBound(manager_->NodeToIndex(node),
                                           kCumulSoftLowerBounds[node.value()],
                                           kCumulCosts[node.value()]);
      dimension->SetCumulVarSoftUpperBound(manager_->NodeToIndex(node),
                                           kCumulSoftUpperBounds[node.value()],
                                           kCumulCosts[node.value()]);
    }
    dimension->CumulVar(model->Start(0))
        ->SetRange(kCumulLowerBounds[2], kCumulUpperBounds[2]);
    dimension->CumulVar(model->End(0))
        ->SetRange(kCumulLowerBounds[2], kCumulUpperBounds[2]);
    return model;
  }
  int VehicleCostCount() const override { return 1; }
  RoutingSearchParameters GetSearchParameters() override {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_disable_scheduling_beware_this_may_degrade_performance(
        ::testing::get<0>(GetParam()));
    return parameters;
  }
};

TEST_P(CumulVarSoftLowerUpperBoundWithSpanCostTest, GetObjectiveValue) {
  const int64_t kExpectedCost =
      /* node 1 late */ (1529742674 - 1529740800) * 10000 +
      /* span cost */ (1529762832 - 1529741700) * 10;
  const Assignment* const original_assignment = SetUpModelAndSolve();
  const Assignment* const packed_assignment =
      model_->PackCumulsOfOptimizerDimensionsFromAssignment(
          original_assignment, kPackCumulsDurationLimit);
  for (const Assignment* assignment :
       {original_assignment, packed_assignment}) {
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(kExpectedCost, assignment->ObjectiveValue())
        << assignment->DebugString();
  }
  EXPECT_EQ(1, model_->GetVehicleClassesCount());
}

INSTANTIATE_TEST_SUITE_P(
    WithParameters, CumulVarSoftLowerUpperBoundWithSpanCostTest,
    ::testing::Combine(::testing::Bool(), ::testing::Bool(), ::testing::Bool(),
                       ::testing::Bool()));

// The "VehicleResourceAssignmentTest"s verify that resource indices are
// correctly set by the finalizer during solve and solution packing, and stored
// in the solution assignment.
//
// The following test has a model with 2 vehicles and 2 non start/end nodes.
// Every node has a soft upper bound on its "time" dimension cumul, and the
// corresponding cost coefficients increase with the index.
// Each vehicle has a capacity of 1 non start/end node, and we have 3 resources:
// - Resource #0 must start in [10, 20] and end in [20, 30].
// - Resource #1 must start in [5, 100] and end in [5, +inf].
// - Resource #2 must start in [0, 10] and end in [6, 12].
// Since it costs more for vehicle #1 and the second node to be late (higher
// indices), we expect the schedule to be as follows:
// - V0: [5] Start -> [6] Node1 -> [7] End - Resource #1.
// - V1: [0] Start -> [1] Node2 -> [6] End - Resource #2.
// Adding a span upper bound of 5 on Resource #2 doesn't change the routes nor
// the resource assignments but forces V1 to start later, to be closer to the
// earliest end time.
TEST(VehicleResourceAssignmentTest, SimpleResourcesTest) {
  const int kNodes = 3;
  const int kVehicles = 2;
  const IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  RoutingSearchParameters params = DefaultRoutingSearchParameters();

  for (int64_t resource_cost : {0, 1}) {
    for (int64_t span_upper_bound : {int64_t{5}, kint64max}) {
      for (auto fs : EnumerateEnumValues<FirstSolutionStrategy::Value>()) {
        FirstSolutionStrategy::Value strategy;
        ASSERT_TRUE(FirstSolutionStrategy_Value_Parse(
            FirstSolutionStrategy_Value_Name(fs), &strategy));
        if (strategy == FirstSolutionStrategy::EVALUATOR_STRATEGY ||
            strategy == FirstSolutionStrategy::SWEEP) {
          continue;
        }
        params.set_first_solution_strategy(strategy);
        Model model(manager);
        // Max 1 non start/end node per vehicle.
        model.AddConstantDimension(1, /*capacity*/ 2, true, "dummy_dim");
        // Add time dimension.
        const std::string kDimName = "time";
        model.AddConstantDimensionWithSlack(
            1, /*capacity*/ 1000, /*slack_max*/ 1000,
            /*fix_start_cumul_to_zero*/ false, kDimName);
        Dimension* const dimension = model.GetMutableDimension(kDimName);

        // Add a soft upper bound on all start times.
        for (int index = 0; index < dimension->cumuls().size(); index++) {
          dimension->SetCumulVarSoftUpperBound(index, /*upper_bound*/ 0,
                                               /*coefficient*/ index + 1);
        }
        // Add resources with start/end domains for the dimension.
        ResourceGroup& resource_group = *model.AddResourceGroup();
        resource_group.AddResource(Attributes(Domain(10, 20), Domain(20, 30),
                                              kint64max, resource_cost),
                                   dimension);
        resource_group.AddResource(Attributes(Domain(10, 20), Domain(20, 30),
                                              kint64max, resource_cost),
                                   dimension);
        resource_group.AddResource(
            Attributes(Domain(5, 100), Domain(5, kint64max), kint64max,
                       resource_cost),
            dimension);
        resource_group.AddResource(
            Attributes(Domain(5, 100), Domain(5, kint64max), kint64max,
                       resource_cost),
            dimension);
        resource_group.AddResource(Attributes(Domain(-1, 10), Domain(6, 12),
                                              span_upper_bound, resource_cost),
                                   dimension);
        // Both vehicles require a resource from the group.
        for (int v = 0; v < model.vehicles(); v++) {
          resource_group.NotifyVehicleRequiresAResource(v);
        }
        model.CloseModelWithParameters(params);

        // There shouldn't be a global optimizer for this dimension as there's a
        // single resource group for it.
        ASSERT_FALSE(model.HasGlobalCumulOptimizer(*dimension));

        const Assignment* const original_assignment =
            model.SolveWithParameters(params);
        ASSERT_NE(original_assignment, nullptr) << strategy;
        const Assignment* const packed_assignment =
            model.PackCumulsOfOptimizerDimensionsFromAssignment(
                original_assignment, kPackCumulsDurationLimit);
        ASSERT_NE(packed_assignment, nullptr);

        std::vector<int64_t> expected_cumuls;
        if (span_upper_bound == 5) {
          expected_cumuls = {5, 6, 2, 1, 7, 6};
        } else {
          expected_cumuls = {5, 6, 1, 0, 7, 6};
        }
        ASSERT_EQ(expected_cumuls.size(), dimension->cumuls().size());

        const std::vector<int> kExpectedResources = {2, 4};
        ASSERT_EQ(model.GetResourceGroups().size(), 1);
        ASSERT_EQ(model.ResourceVars(0).size(), model.vehicles());

        for (const Assignment* const assignment :
             {original_assignment, packed_assignment}) {
          for (int i = 0; i < dimension->cumuls().size(); ++i) {
            EXPECT_EQ(assignment->Value(dimension->CumulVar(i)),
                      expected_cumuls[i]);
          }
          for (int v = 0; v < model.vehicles(); v++) {
            EXPECT_EQ(assignment->Value(model.ResourceVar(v, 0)),
                      kExpectedResources[v]);
          }
          EXPECT_EQ(
              assignment->ObjectiveValue(),
              (span_upper_bound == 5 ? 98 : 91) + kVehicles * resource_cost);
        }
      }
    }
  }
}

// In the following test, we have 2 vehicles, a single non start/end node, and
// a single resource. Vehicles are marked used even when empty.
// The test verifies that the resource assignment is correctly done with
// different settings of vehicle resource requirements and node allowed vehicle
// index.
TEST(VehicleResourceAssignmentTest, NotifyVehicleRequiresAResourceTest) {
  const int kNodes = 2;
  const int kVehicles = 2;
  const IndexManager manager(kNodes, kVehicles, NodeIndex(0));

  const bool kVehicleRequiresResource[][kVehicles] = {
      {true, true}, {false, true}, {true, false}, {false, false}};

  for (int i = 0; i < std::size(kVehicleRequiresResource); i++) {
    for (int allowed_v = 0; allowed_v < kVehicles; ++allowed_v) {
      for (bool use_global_optimizer : {true, false}) {
        Model model(manager);
        const std::string kDimName = "time";
        model.AddConstantDimensionWithSlack(
            1, /*capacity*/ 1000, /*slack_max*/ 1000,
            /*fix_start_cumul_to_zero*/ false, kDimName);
        Dimension* const dimension = model.GetMutableDimension(kDimName);

        if (use_global_optimizer) {
          dimension->SetGlobalSpanCostCoefficient(1);
        }

        // Add a single resource.
        Model::ResourceGroup& resource_group = *model.AddResourceGroup();
        resource_group.AddResource(Attributes(), dimension);

        int num_vehicles_requiring_resource = 0;
        for (int v = 0; v < model.vehicles(); v++) {
          // Make vehicles used even when empty.
          model.SetVehicleUsedWhenEmpty(true, v);
          if (kVehicleRequiresResource[i][v]) {
            num_vehicles_requiring_resource++;
            resource_group.NotifyVehicleRequiresAResource(v);
          }
        }
        // Mark the allowed vehicle for the non start/end node.
        model.SetAllowedVehiclesForIndex({allowed_v}, 1);

        const Assignment* const original_assignment = model.Solve();
        EXPECT_EQ(model.HasGlobalCumulOptimizer(*dimension),
                  use_global_optimizer);
        if (num_vehicles_requiring_resource > 1) {
          EXPECT_EQ(original_assignment, nullptr);
          EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_INFEASIBLE);
          continue;
        }

        ASSERT_NE(original_assignment, nullptr);
        const Assignment* const packed_assignment =
            model.PackCumulsOfOptimizerDimensionsFromAssignment(
                original_assignment, kPackCumulsDurationLimit);
        for (const Assignment* const assignment :
             {original_assignment, packed_assignment}) {
          // Regardless of what vehicle the node is inserted on, the resource
          // should be assigned to the vehicle requiring it (because it's used
          // even if empty).
          for (int v = 0; v < model.vehicles(); v++) {
            EXPECT_EQ(assignment->Value(model.ResourceVar(v, 0)),
                      kVehicleRequiresResource[i][v] ? 0 : -1);
          }
        }
      }
    }
  }
}

// The following test verifies that the LP-based finalizer properly handles
// resource assignment with multiple resource groups on the same dimension.
TEST(VehicleResourceAssignmentTest, TwoResourceGroupsForSameDimensionTest) {
  const int kNodes = 3;
  const int kVehicles = 2;
  const IndexManager manager(kNodes, kVehicles, NodeIndex(0));

  for (const bool disable_lp_scheduling : {true, false}) {
    Model model(manager);
    const std::string kDimName = "time";
    model.AddConstantDimension(1, /*capacity*/ 1000,
                               /*fix_start_cumul_to_zero*/ false, kDimName);
    Dimension* const dimension = model.GetMutableDimension(kDimName);

    // First node can only be served by the first vehicle, second node only by
    // the second vehicle.
    model.SetAllowedVehiclesForIndex({0}, 1);
    model.SetAllowedVehiclesForIndex({1}, 2);

    // First vehicle can start between 8 and 10, second vehicle from 4 to 5.
    const std::vector<std::pair<int, int>> start_cumul_bounds = {{8, 10},
                                                                 {4, 5}};
    for (int v = 0; v < kVehicles; v++) {
      dimension->CumulVar(model.Start(v))
          ->SetRange(start_cumul_bounds[v].first, start_cumul_bounds[v].second);
    }

    // First ResourceGroup.
    // The first 2 resources can only be assigned to the first vehicle.
    Model::ResourceGroup* resource_group = model.AddResourceGroup();
    resource_group->AddResource(Attributes(Domain(10, 13), Domain::AllValues()),
                                dimension);
    resource_group->AddResource(Attributes(Domain(10, 13), Domain::AllValues()),
                                dimension);
    resource_group->AddResource(Attributes(), dimension);
    for (int v = 0; v < model.vehicles(); v++) {
      resource_group->NotifyVehicleRequiresAResource(v);
    }

    // Second ResourceGroup.
    // The first 2 resources can only be assigned to the second vehicle.
    resource_group = model.AddResourceGroup();
    resource_group->AddResource(Attributes(Domain::AllValues(), Domain(7, 11)),
                                dimension);
    resource_group->AddResource(Attributes(Domain::AllValues(), Domain(7, 11)),
                                dimension);
    resource_group->AddResource(Attributes(), dimension);
    for (int v = 0; v < model.vehicles(); v++) {
      resource_group->NotifyVehicleRequiresAResource(v);
    }

    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_disable_scheduling_beware_this_may_degrade_performance(
        disable_lp_scheduling);
    model.CloseModelWithParameters(parameters);

    // There should be a global cumul optimizer for this dimension iff
    // disable_lp_scheduling is false.
    ASSERT_NE(disable_lp_scheduling, model.HasGlobalCumulOptimizer(*dimension));

    const Assignment* const original_assignment = model.Solve();
    ASSERT_NE(original_assignment, nullptr);
    const Assignment* const packed_assignment =
        model.PackCumulsOfOptimizerDimensionsFromAssignment(
            original_assignment, kPackCumulsDurationLimit);
    ASSERT_NE(packed_assignment, nullptr);
    for (const Assignment* const assignment :
         {original_assignment, packed_assignment}) {
      // Expected times:
      // Start(v0): 10 -> Node1: 11 -> End(v0): 12
      // Start(v1):  5 -> Node2:  6 -> End(v1):  7
      const std::vector<int64_t> kExpectedCumuls = {10, 11, 6, 5, 12, 7};
      ASSERT_EQ(kExpectedCumuls.size(), dimension->cumuls().size());
      for (int i = 0; i < dimension->cumuls().size(); i++) {
        EXPECT_EQ(assignment->Value(dimension->CumulVar(i)),
                  kExpectedCumuls[i]);
      }
      const std::vector<std::vector<int>> kExpectedResources = {{0, 2}, {2, 0}};
      ASSERT_EQ(kExpectedResources.size(), model.GetResourceGroups().size());
      for (int rg = 0; rg < model.GetResourceGroups().size(); rg++) {
        for (int v = 0; v < model.vehicles(); v++) {
          EXPECT_EQ(assignment->Value(model.ResourceVar(v, rg)),
                    kExpectedResources[rg][v]);
        }
      }
    }
  }
}

// The following test verifies that the SetCumulsFromResourceAssignmentCosts
// finalizer properly recomputes the route cumuls using CP-SAT when using
// GLOP leads to a relaxed solution with multiple time windows.
TEST(VehicleResourceAssignmentTest, ResourcesAndMultipleTimeWindows) {
  const int kNodes = 2;
  const int kVehicles = 1;
  const IndexManager manager(kNodes, kVehicles, NodeIndex(0));

  for (bool disable_lp_scheduling : {true, false}) {
    Model model(manager);
    const std::string kDimName = "time";
    model.AddConstantDimensionWithSlack(
        1, /*capacity*/ 1000, /*slack_max*/ 1000,
        /*fix_start_cumul_to_zero*/ false, kDimName);
    Dimension* const dimension = model.GetMutableDimension(kDimName);

    // The node cannot be served between 10 and 15.
    dimension->CumulVar(1)->SetRange(5, 20);
    model.solver()->AddConstraint(model.solver()->MakeNotMemberCt(
        dimension->CumulVar(1), std::vector<int64_t>{10},
        std::vector<int64_t>{15}));

    // Add 2 resources:
    // - One which should start before 5 and end after 50. This resource
    //   assignment leads to a feasible optimal result with GLOP, but is more
    //   expensive (cost of 10*(50-5)).
    // - The second resource should start in [12, 13], leading to a relaxed
    //   optimal only result from GLOP. Using this resource is less expensive
    //   (cost of 10*(17-13)).
    Model::ResourceGroup& resource_group = *model.AddResourceGroup();
    resource_group.AddResource(Attributes(Domain(0, 5), Domain(50, 200)),
                               dimension);
    resource_group.AddResource(Attributes(Domain(12, 13), Domain::AllValues()),
                               dimension);
    resource_group.NotifyVehicleRequiresAResource(0);

    // Add a span cost to the vehicle.
    dimension->SetSpanCostCoefficientForAllVehicles(10);

    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_disable_scheduling_beware_this_may_degrade_performance(
        disable_lp_scheduling);
    const Assignment* const original_assignment =
        model.SolveWithParameters(parameters);
    ASSERT_NE(original_assignment, nullptr);
    const Assignment* const packed_assignment =
        model.PackCumulsOfOptimizerDimensionsFromAssignment(
            original_assignment, kPackCumulsDurationLimit);
    ASSERT_NE(packed_assignment, nullptr);
    for (const Assignment* const assignment :
         {original_assignment, packed_assignment}) {
      // Expected times:
      // Start(v0): 13 -> Node1: 16 -> End(v0): 17
      const std::vector<int64_t> kExpectedCumuls = {13, 16, 17};
      ASSERT_EQ(kExpectedCumuls.size(), dimension->cumuls().size());
      for (int i = 0; i < dimension->cumuls().size(); i++) {
        EXPECT_EQ(assignment->Value(dimension->CumulVar(i)),
                  kExpectedCumuls[i]);
      }
      EXPECT_EQ(assignment->Value(model.ResourceVar(0, 0)), 1);
      EXPECT_EQ(assignment->ObjectiveValue(), 10 * (17 - 13));
    }
  }
}

// In the following test, we have 1 vehicle and two resources. The first
// resource allows the vehicle to have a smaller span on its route and is
// therefore more optimal.
// This test verifies "allowed" resources are properly taken into account by the
// solver.
TEST(VehicleResourceAssignmentTest, VehicleAllowedResourceTest) {
  const int kNodes = 1;
  const int kVehicles = 1;
  const IndexManager manager(kNodes, kVehicles, NodeIndex(0));

  for (const bool first_resource_allowed : {true, false}) {
    for (bool use_global_optimizer : {true, false}) {
      Model model(manager);
      const std::string kDimName = "time";
      model.AddConstantDimensionWithSlack(
          1, /*capacity*/ 1000, /*slack_max*/ 1000,
          /*fix_start_cumul_to_zero*/ false, kDimName);
      Dimension* const dimension = model.GetMutableDimension(kDimName);

      // Add a span cost on the dimension.
      if (use_global_optimizer) {
        dimension->SetGlobalSpanCostCoefficient(10);
      } else {
        dimension->SetSpanCostCoefficientForAllVehicles(10);
      }

      // Add two resources.
      Model::ResourceGroup& resource_group = *model.AddResourceGroup();
      // First resource doesn't constrain the span, the second makes the vehicle
      // have a span of (at least) 50.
      resource_group.AddResource(Attributes(), dimension);
      resource_group.AddResource(Attributes(Domain(0, 1), Domain(51, 200)),
                                 dimension);

      const int vehicle = 0;
      // Make vehicle used even when empty.
      model.SetVehicleUsedWhenEmpty(true, vehicle);
      resource_group.NotifyVehicleRequiresAResource(vehicle);

      // Setup allowed resources.
      std::vector<int> allowed_resources = {1};
      if (first_resource_allowed) {
        allowed_resources.push_back(0);
      }
      resource_group.SetAllowedResourcesForVehicle(vehicle, allowed_resources);

      const Assignment* const original_assignment = model.Solve();
      EXPECT_EQ(model.HasGlobalCumulOptimizer(*dimension),
                use_global_optimizer);
      const Assignment* const packed_assignment =
          model.PackCumulsOfOptimizerDimensionsFromAssignment(
              original_assignment, kPackCumulsDurationLimit);
      for (const Assignment* const assignment :
           {original_assignment, packed_assignment}) {
        ASSERT_NE(assignment, nullptr);
        if (first_resource_allowed) {
          EXPECT_EQ(assignment->Value(model.ResourceVar(vehicle, 0)), 0);
          EXPECT_EQ(assignment->ObjectiveValue(), 10 * 1);
        } else {
          EXPECT_EQ(assignment->Value(model.ResourceVar(vehicle, 0)), 1);
          EXPECT_EQ(assignment->ObjectiveValue(), 10 * (51 - 1));
        }
      }
    }
  }
}

// The following test checks that the node is correctly inserted on the second
// vehicle by the MakeActiveOperator after insertion on the first vehicle
// has failed because of the incompatibility of the node time window with the
// end time window of the resource, required by the first vehicle.
TEST(OperatorVehicleClassesTest, DifferentResourceRequirements) {
  const int kNumNodes = 2;
  const int kNumVehicles = 2;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  const std::string kDimName = "time";
  model.AddConstantDimension(1, /*capacity*/ 1000,
                             /*fix_start_cumul_to_zero*/ false, kDimName);
  Dimension* const dimension = model.GetMutableDimension(kDimName);

  // The node time window is [10, 15].
  dimension->CumulVar(1)->SetRange(10, 15);
  model.AddDisjunction({1}, 10000);

  // The resource must end before 5, and vehicle 0 requires it.
  ResourceGroup* resource_group = model.AddResourceGroup();
  resource_group->AddResource(Attributes(Domain::AllValues(), Domain(0, 5)),
                              dimension);
  resource_group->NotifyVehicleRequiresAResource(0);

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  DisableAllLocalSearchOperators(parameters.mutable_local_search_operators());
  parameters.mutable_local_search_operators()->set_use_make_active(BOOL_TRUE);
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::ALL_UNPERFORMED);

  const Assignment* const assignment = model.SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  // The node should be served by the second vehicle.
  EXPECT_EQ(assignment->Value(model.NextVar(2)), 1);
  EXPECT_EQ(assignment->Value(model.NextVar(1)), 4);
  EXPECT_EQ(assignment->ObjectiveValue(), 0);
}

// The following test checks that the node is correctly inserted on the second
// vehicle by the MakeActiveOperator after insertion on the first vehicle
// has failed because of the incompatibility of the node time window with the
// end time window of the only allowed resource of the first vehicle.
// The node can however be inserted on the second vehicle since it's compatible
// with the second resource (allowed for the second vehicle).
TEST(OperatorVehicleClassesTest, DifferentAllowedResourceIndices) {
  const int kNumNodes = 2;
  const int kNumVehicles = 2;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  const std::string kDimName = "time";
  model.AddConstantDimension(1, /*capacity*/ 1000,
                             /*fix_start_cumul_to_zero*/ false, kDimName);
  Dimension* const dimension = model.GetMutableDimension(kDimName);

  // The node time window is [10, 15].
  dimension->CumulVar(1)->SetRange(10, 15);
  model.AddDisjunction({1}, 10000);

  ResourceGroup* resource_group = model.AddResourceGroup();
  for (int v = 0; v < model.vehicles(); v++) {
    resource_group->NotifyVehicleRequiresAResource(v);
  }
  // The first resource must end before 5, and it's the only allowed resource
  // for vehicle 0.
  resource_group->AddResource(Attributes(Domain::AllValues(), Domain(0, 5)),
                              dimension);
  resource_group->SetAllowedResourcesForVehicle(0, {0});
  // Second resource is compatible with the node's end time window.
  resource_group->AddResource(Attributes(), dimension);

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  DisableAllLocalSearchOperators(parameters.mutable_local_search_operators());
  parameters.mutable_local_search_operators()->set_use_make_active(BOOL_TRUE);
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::ALL_UNPERFORMED);

  const Assignment* const assignment = model.SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  // The node should be served by the second vehicle.
  EXPECT_EQ(assignment->Value(model.NextVar(2)), 1);
  EXPECT_EQ(assignment->Value(model.NextVar(1)), 4);
  EXPECT_EQ(assignment->ObjectiveValue(), 0);
}

// The following test checks that the MakeActiveOperator considers insertion on
// both empty vehicles, which differ only in their "unused if route is empty"
// status.
// In this test, both vehicles require a resource, but only the first vehicle is
// unused when empty, so the node can only be inserted on the second vehicle
// since there's only a single resource available.
TEST(OperatorVehicleClassesTest, DifferentUnusedIfRouteIsEmpty) {
  const int kNumNodes = 2;
  const int kNumVehicles = 2;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  const std::string kDimName = "time";
  model.AddConstantDimension(1, /*capacity*/ 1000,
                             /*fix_start_cumul_to_zero*/ false, kDimName);
  Dimension* const dimension = model.GetMutableDimension(kDimName);

  // Make the node non-mandatory.
  model.AddDisjunction({1}, 10000);

  // Add a resource required by both vehicles.
  ResourceGroup* resource_group = model.AddResourceGroup();
  resource_group->AddResource(
      Attributes(Domain::AllValues(), Domain::AllValues()), dimension);
  // Make both vehicles require a resource from the group, and only the second
  // one used when empty.
  const std::vector<bool> used_when_empty = {false, true};
  for (int v = 0; v < model.vehicles(); v++) {
    resource_group->NotifyVehicleRequiresAResource(v);
    model.SetVehicleUsedWhenEmpty(used_when_empty[v], v);
  }

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  DisableAllLocalSearchOperators(parameters.mutable_local_search_operators());
  parameters.mutable_local_search_operators()->set_use_make_active(BOOL_TRUE);
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::ALL_UNPERFORMED);

  const Assignment* const assignment = model.SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  // The node should be served by the second vehicle.
  EXPECT_EQ(assignment->Value(model.NextVar(2)), 1);
  EXPECT_EQ(assignment->Value(model.NextVar(1)), 4);
  EXPECT_EQ(assignment->ObjectiveValue(), 0);
}

TEST(CumulVarCostTest, VehicleWithAndWithoutSpanCosts) {
  IndexManager manager(
      4, 2, {{NodeIndex(0), NodeIndex(0)}, {NodeIndex(1), NodeIndex(2)}});
  const NodeIndex node(3);

  const bool kConsiderEmptyVehicleCosts[] = {true, false};
  const int kExpectedCost[] = {(2000 - 500) * 1 + 1000 * 10, (2000 - 500) * 1};

  for (int c = 0; c < std::size(kConsiderEmptyVehicleCosts); c++) {
    Model model(manager);
    const int callback =
        model.RegisterTransitCallback([&manager](int64_t i, int64_t j) {
          if (manager.IndexToNode(i) == 0 || manager.IndexToNode(j) == 0) {
            return 0;
          }
          return 10;
        });
    SetConsiderEmptyRouteCostsForAllVehicles(&model,
                                             kConsiderEmptyVehicleCosts[c]);
    model.AddDimension(callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/false, "dim");
    Dimension* const dimension = model.GetMutableDimension("dim");
    dimension->CumulVar(manager.NodeToIndex(node))->SetRange(1000, 2000);
    dimension->SetCumulVarSoftUpperBound(manager.NodeToIndex(node), 500, 1);
    dimension->CumulVar(model.Start(0))->SetMin(2000);
    dimension->SetSpanUpperBoundForVehicle(1000000000000, 0);
    dimension->SetSpanCostCoefficientForVehicle(1000, 1);
    dimension->SetSpanUpperBoundForVehicle(1000000000000, 1);
    const Assignment* const unpacked_assignment = model.Solve();
    ASSERT_NE(unpacked_assignment, nullptr);
    const Assignment* const assignment =
        model.PackCumulsOfOptimizerDimensionsFromAssignment(
            unpacked_assignment, kPackCumulsDurationLimit);
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(kExpectedCost[c], assignment->ObjectiveValue());
    EXPECT_TRUE(model.IsVehicleUsed(*assignment, 0));
    EXPECT_EQ(2000, assignment->Value(dimension->CumulVar(
                        manager.NodeToIndex(NodeIndex(3)))));
    EXPECT_EQ(2000, assignment->Value(dimension->CumulVar(model.Start(0))));
    EXPECT_EQ(2000, assignment->Value(dimension->CumulVar(model.End(0))));
    EXPECT_EQ(0, assignment->Value(dimension->CumulVar(model.Start(1))));
    EXPECT_EQ(10, assignment->Value(dimension->CumulVar(model.End(1))));
  }
}

TEST(CumulVarCostTest, GlobalSpanCostAndVehicleWithAndWithoutSpanCosts) {
  IndexManager manager(
      3, 2, {{NodeIndex(0), NodeIndex(1)}, {NodeIndex(0), NodeIndex(0)}});
  Model model(manager);
  const int callback =
      model.RegisterTransitCallback([&manager](int64_t i, int64_t j) {
        int64_t offset = 0;
        if (manager.IndexToNode(i) == 2) {
          offset = 1001;
        }
        if (manager.IndexToNode(i) == 0 || manager.IndexToNode(j) == 0) {
          return offset;
        }
        return 10 + offset;
      });
  model.AddDimension(callback, 1000000100000, 1000000100000,
                     /*fix_start_cumul_to_zero=*/false, "dim");
  Dimension* const dimension = model.GetMutableDimension("dim");
  const NodeIndex node(2);
  dimension->CumulVar(manager.NodeToIndex(node))->SetMin(1000);
  dimension->SetCumulVarSoftUpperBound(manager.NodeToIndex(node), 1000000000001,
                                       1);
  dimension->CumulVar(model.Start(0))->SetMin(1000);
  dimension->SetSpanCostCoefficientForVehicle(1000, 0);
  dimension->SetSpanCostCoefficientForVehicle(100, 1);
  model.SetFixedCostOfVehicle(1, 1);
  dimension->SetGlobalSpanCostCoefficient(1000);
  const Assignment* const assignment = model.Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(1 + 1001 * (1000 + 100), assignment->ObjectiveValue());

  const Assignment* packed_assignment =
      model.PackCumulsOfOptimizerDimensionsFromAssignment(
          assignment, kPackCumulsDurationLimit);
  ASSERT_NE(packed_assignment, nullptr);
  EXPECT_EQ(1000, packed_assignment->Value(
                      dimension->CumulVar(manager.NodeToIndex(NodeIndex(2)))));
  EXPECT_EQ(1000,
            packed_assignment->Value(dimension->CumulVar(model.Start(1))));
  EXPECT_EQ(2001, packed_assignment->Value(dimension->CumulVar(model.End(1))));
  EXPECT_EQ(1000,
            packed_assignment->Value(dimension->CumulVar(model.Start(0))));
  EXPECT_EQ(1000, packed_assignment->Value(dimension->CumulVar(model.End(0))));
}

TEST(CumulVarCostTest, GlobalSpanCostAndVehicleLimitsWithAndWithoutSpanCosts) {
  IndexManager manager(
      3, 2, {{NodeIndex(0), NodeIndex(1)}, {NodeIndex(0), NodeIndex(0)}});
  Model model(manager);
  const int callback =
      model.RegisterTransitCallback([&manager](int64_t i, int64_t j) {
        int64_t offset = 0;
        if (manager.IndexToNode(i) == 2) {
          offset = 1001;
        }
        if (manager.IndexToNode(i) == 0 || manager.IndexToNode(j) == 0) {
          return offset;
        }
        return 10 + offset;
      });
  const int travel_callback =
      model.RegisterTransitCallback([&manager](int64_t i, int64_t j) {
        if (manager.IndexToNode(i) == 0 || manager.IndexToNode(j) == 0) {
          return 0;
        }
        return 1000;
      });
  model.AddDimension(callback, 1000000100000, 1000000100000,
                     /*fix_start_cumul_to_zero=*/false, "dim");
  Dimension* const dimension = model.GetMutableDimension("dim");
  model.AddDimension(travel_callback, kint64max, kint64max,
                     /*fix_start_cumul_to_zero=*/false, "travel");
  Dimension* const travel = model.GetMutableDimension("travel");
  const NodeIndex node(2);
  dimension->CumulVar(manager.NodeToIndex(node))->SetMin(1000);
  dimension->SetCumulVarSoftUpperBound(manager.NodeToIndex(node), 1000000000001,
                                       1);
  dimension->CumulVar(model.Start(0))->SetMin(1000);
  travel->SetSpanUpperBoundForVehicle(0, 0);
  dimension->SetSpanCostCoefficientForVehicle(1000, 0);
  travel->SetSpanCostCoefficientForVehicle(1000, 0);
  dimension->SetSpanUpperBoundForVehicle(1000000000000, 1);
  travel->SetSpanUpperBoundForVehicle(1000, 1);
  model.SetFixedCostOfVehicle(1, 1);
  dimension->SetGlobalSpanCostCoefficient(1000);
  const Assignment* const assignment = model.Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(1 + 1001 * 1000, assignment->ObjectiveValue());

  const Assignment* packed_assignment =
      model.PackCumulsOfOptimizerDimensionsFromAssignment(
          assignment, kPackCumulsDurationLimit);
  ASSERT_NE(packed_assignment, nullptr);
  EXPECT_EQ(1000, packed_assignment->Value(
                      dimension->CumulVar(manager.NodeToIndex(NodeIndex(2)))));
  EXPECT_EQ(1000,
            packed_assignment->Value(dimension->CumulVar(model.Start(1))));
  EXPECT_EQ(2001, packed_assignment->Value(dimension->CumulVar(model.End(1))));
  EXPECT_EQ(1000,
            packed_assignment->Value(dimension->CumulVar(model.Start(0))));
  EXPECT_EQ(1000, packed_assignment->Value(dimension->CumulVar(model.End(0))));
}

// Simple unit test with 3 vehicles and no nodes besides the depot, to make sure
// that vehicle times are optimized to minimize the global_span_cost even when
// they're empty, iff their cost should be considered when they're empty.
TEST(CumulVarCostTest, EmptyVehicleTimesOptimized) {
  const int kNumVehicles = 3;
  IndexManager manager(1, kNumVehicles, NodeIndex(0));
  const bool kConsiderEmptyVehicleCosts[] = {true, false};
  const int kStartTime = 1000;
  const int kExpectedVehicleStartEndTimes[][kNumVehicles] = {
      {kStartTime, kStartTime, kStartTime}, {kStartTime, 0, 0}};
  const int kExpectedCost = 0;

  for (int c = 0; c < std::size(kConsiderEmptyVehicleCosts); c++) {
    SCOPED_TRACE(absl::StrCat("c: ", c));
    Model model(manager);

    const std::string kDimensionName = "time";
    model.AddDimension(
        model.RegisterTransitCallback([](int64_t, int64_t) { return 0; }),
        /*slack_max*/ kint64max,
        /*capacity*/ kint64max,
        /*fix_start_cumul_to_zero*/ false, kDimensionName);
    Dimension* dimension = model.GetMutableDimension(kDimensionName);

    // First vehicle must start after 1000.
    dimension->CumulVar(model.Start(0))->SetRange(kStartTime, kint64max);
    dimension->SetSpanCostCoefficientForVehicle(3600, 0);
    dimension->CumulVar(model.Start(1))->SetRange(0, kint64max);
    dimension->CumulVar(model.Start(2))->SetRange(0, kint64max);
    dimension->SetGlobalSpanCostCoefficient(1000);

    SetConsiderEmptyRouteCostsForAllVehicles(&model,
                                             kConsiderEmptyVehicleCosts[c]);

    const Assignment* assignment = model.Solve();
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(assignment->ObjectiveValue(), kExpectedCost);
    for (int v = 0; v < kNumVehicles; v++) {
      EXPECT_EQ(kExpectedVehicleStartEndTimes[c][v],
                assignment->Value(dimension->CumulVar(model.Start(v))));
      EXPECT_EQ(kExpectedVehicleStartEndTimes[c][v],
                assignment->Value(dimension->CumulVar(model.End(v))));
    }
  }
}

// Testing on a model with one depot, 3 nodes with earliness costs, 1 node with
// a hard window.
class CumulVarSoftLowerBoundTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<bool, bool, bool, bool>> {
 protected:
  CumulVarSoftLowerBoundTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/::testing::get<0>(GetParam()),
            /*use_light_propagation=*/::testing::get<1>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<2>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<3>(GetParam())) {
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    const char* kDimension = "test_dimension";
    const int64_t kCumulLowerBounds[] = {2, 500, 10};
    const int64_t kCumulCosts[] = {5, 1, 2};
    manager_ = std::make_unique<IndexManager>(5, 1, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    const int callback =
        use_unary_callback_
            ? model->RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model->RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    model->AddDimension(callback, kint64max, kint64max,
                        /*fix_start_cumul_to_zero=*/true, kDimension);
    Dimension* const dimension = model->GetMutableDimension(kDimension);
    for (NodeIndex node(1); node < NodeIndex(4); ++node) {
      dimension->SetCumulVarSoftLowerBound(manager_->NodeToIndex(node),
                                           kCumulLowerBounds[node.value() - 1],
                                           kCumulCosts[node.value() - 1]);
    }
    dimension->SetCumulVarSoftLowerBound(
        model->Start(0), kCumulLowerBounds[0] - 1, kCumulCosts[0]);
    dimension->SetCumulVarSoftLowerBound(model->End(0), kCumulLowerBounds[0],
                                         kCumulCosts[0]);
    dimension->CumulVar(model->End(0))->SetMax(200);
    dimension->CumulVar(4)->SetMin(100);
    return model;
  }
};

TEST_P(CumulVarSoftLowerBoundTest, GetObjectiveValue) {
  const int64_t kExpectedCost = 5 * 1 + 5 * 0 + 1 * 301 + 2 * 0 + 5 * 0 + 5 * 0;
  const Assignment* const assignment = SetUpModelAndSolve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(kExpectedCost, assignment->ObjectiveValue());
  EXPECT_EQ(1, model_->GetVehicleClassesCount());
}

TEST_P(CumulVarSoftLowerBoundTest, SoftLowerBoundAccessors) {
  SetUpModelAndSolve();
  Dimension* const dimension = model_->GetMutableDimension("test_dimension");
  const int64_t kExpectedCumulLowerBounds[] = {1, 2, 500, 10, 100, 2};
  const int64_t kExpectedCumulCosts[] = {5, 5, 1, 2, 0, 5};
  for (int64_t index = 0; index < model_->Size(); ++index) {
    EXPECT_EQ(kExpectedCumulCosts[index],
              dimension->GetCumulVarSoftLowerBoundCoefficient(index));
    if (kExpectedCumulCosts[index] != 0) {
      EXPECT_EQ(kExpectedCumulLowerBounds[index],
                dimension->GetCumulVarSoftLowerBound(index));
    }
  }
}

INSTANTIATE_TEST_SUITE_P(WithParameters, CumulVarSoftLowerBoundTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

using UnactiveCumulVarSoftLowerBoundTest = ::testing::TestWithParam<bool>;
TEST_P(UnactiveCumulVarSoftLowerBoundTest, GetObjectiveValue) {
  const char* kDimension = "test_dimension";
  const int64_t kSoftBounds[] = {-10, 0, 10, 100};
  for (int i = 0; i < std::size(kSoftBounds); ++i) {
    IndexManager manager(2, 1, NodeIndex(0));
    Model model(manager);
    const int callback =
        GetParam()
            ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model.RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    model.AddDimension(callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/true, kDimension);
    NodeIndex node(1);
    int64_t index = manager.NodeToIndex(node);
    model.AddDisjunction(std::vector<int64_t>({index}), 0);
    model.ActiveVar(index)->SetValue(0);
    Dimension* const dimension = model.GetMutableDimension(kDimension);
    dimension->CumulVar(index)->SetRange(0, 10);
    dimension->SetCumulVarSoftLowerBound(index, kSoftBounds[i], 1);
    const Assignment* const assignment = model.Solve();
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(0, assignment->ObjectiveValue());
  }
}

INSTANTIATE_TEST_SUITE_P(WithOrWithoutUnaryCallback,
                         UnactiveCumulVarSoftLowerBoundTest, ::testing::Bool());

// Testing on a model with one depot, 3 nodes with piecewise linear costs.
class CumulVarPiecewiseLinearCostTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<bool, bool, bool, bool>> {
 protected:
  CumulVarPiecewiseLinearCostTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/::testing::get<0>(GetParam()),
            /*use_light_propagation=*/::testing::get<1>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<2>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<3>(GetParam())) {
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    const char* kDimension = "test_dimension";
    const int64_t kCumulUpperBounds[] = {0, 5, 10};
    const int64_t kCumulCosts[] = {5, 1, 2};
    manager_ = std::make_unique<IndexManager>(5, 1, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    const int callback =
        use_unary_callback_
            ? model->RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model->RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    model->AddDimension(callback, 0, kint64max,
                        /*fix_start_cumul_to_zero=*/false, kDimension);
    Dimension* const dimension = model->GetMutableDimension(kDimension);
    for (NodeIndex node(1); node < NodeIndex(4); ++node) {
      std::unique_ptr<PiecewiseLinearFunction> f(
          PiecewiseLinearFunction::CreateFullDomainFunction(
              0, {kCumulUpperBounds[node.value() - 1]},
              {0, kCumulCosts[node.value() - 1]}));
      dimension->SetCumulVarPiecewiseLinearCost(manager_->NodeToIndex(node),
                                                *f);
    }
    std::unique_ptr<PiecewiseLinearFunction> f(
        PiecewiseLinearFunction::CreateFullDomainFunction(
            0, {kCumulUpperBounds[0] - 1}, {0, kCumulCosts[0]}));
    dimension->SetCumulVarPiecewiseLinearCost(model->Start(0), *f);
    f.reset(PiecewiseLinearFunction::CreateFullDomainFunction(
        0, {kCumulUpperBounds[0]}, {0, kCumulCosts[0]}));
    dimension->SetCumulVarPiecewiseLinearCost(model->End(0), *f);
    return model;
  }
};

TEST_P(CumulVarPiecewiseLinearCostTest, GetObjectiveValue) {
  const int64_t kExpectedCost = 5 * 1 + 1 * 0 + 2 * 0 + 5 * 1 + 5 * 5;
  const Assignment* const assignment = SetUpModelAndSolve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(kExpectedCost, assignment->ObjectiveValue());
  EXPECT_EQ(1, model_->GetVehicleClassesCount());
}

TEST_P(CumulVarPiecewiseLinearCostTest, PiecewiseLinearCostAccessors) {
  SetUpModelAndSolve();
  Dimension* const dimension = model_->GetMutableDimension("test_dimension");
  const int64_t kExpectedCumulUpperBounds[] = {0, 5, 10};
  const int64_t kExpectedCumulCosts[] = {5, 1, 2};
  for (NodeIndex node(1); node < NodeIndex(4); ++node) {
    std::unique_ptr<PiecewiseLinearFunction> f(
        PiecewiseLinearFunction::CreateFullDomainFunction(
            0, {kExpectedCumulUpperBounds[node.value() - 1]},
            {0, kExpectedCumulCosts[node.value() - 1]}));
    EXPECT_EQ(
        f->DebugString(),
        dimension->GetCumulVarPiecewiseLinearCost(manager_->NodeToIndex(node))
            ->DebugString());
  }
  EXPECT_EQ(nullptr, dimension->GetCumulVarPiecewiseLinearCost(
                         manager_->NodeToIndex(NodeIndex(4))));
  std::unique_ptr<PiecewiseLinearFunction> f(
      PiecewiseLinearFunction::CreateFullDomainFunction(
          0, {kExpectedCumulUpperBounds[0] - 1}, {0, kExpectedCumulCosts[0]}));
  EXPECT_EQ(f->DebugString(),
            dimension->GetCumulVarPiecewiseLinearCost(model_->Start(0))
                ->DebugString());
  f.reset(PiecewiseLinearFunction::CreateFullDomainFunction(
      0, {kExpectedCumulUpperBounds[0]}, {0, kExpectedCumulCosts[0]}));
  EXPECT_EQ(
      f->DebugString(),
      dimension->GetCumulVarPiecewiseLinearCost(model_->End(0))->DebugString());
}

using UnactiveCumulVarPiecewiseLinearCostTest = ::testing::TestWithParam<bool>;
TEST_P(UnactiveCumulVarPiecewiseLinearCostTest, GetObjectiveValue) {
  const char* kDimension = "test_dimension";
  const int64_t kSoftBounds[] = {-10, 0, 10, 100};
  for (int i = 0; i < std::size(kSoftBounds); ++i) {
    IndexManager manager(2, 1, NodeIndex(0));
    Model model(manager);
    const int callback =
        GetParam()
            ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model.RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    model.AddDimension(callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/true, kDimension);
    NodeIndex node(1);
    int64_t index = manager.NodeToIndex(node);
    model.AddDisjunction(std::vector<int64_t>({index}), 0);
    model.ActiveVar(index)->SetValue(0);
    Dimension* const dimension = model.GetMutableDimension(kDimension);
    dimension->CumulVar(index)->SetRange(0, 10);
    std::unique_ptr<PiecewiseLinearFunction> f(
        PiecewiseLinearFunction::CreateFullDomainFunction(0, {kSoftBounds[i]},
                                                          {0, 1}));
    dimension->SetCumulVarPiecewiseLinearCost(index, *f);
    const Assignment* const assignment = model.Solve();
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(0, assignment->ObjectiveValue());
  }
}

INSTANTIATE_TEST_SUITE_P(WithOrWithoutUnaryCallback,
                         UnactiveCumulVarPiecewiseLinearCostTest,
                         ::testing::Bool());

TEST_P(CumulVarPiecewiseLinearCostTest, NoMemoryLeak) {
  SetUpModelAndSolve();
  Dimension* const dimension = model_->GetMutableDimension("test_dimension");
  // Setting the same cost twice creates a memory leak if the first pointer is
  // not disposed of.
  const PiecewiseLinearFunction* cost =
      dimension->GetCumulVarPiecewiseLinearCost(model_->End(0));
  dimension->SetCumulVarPiecewiseLinearCost(model_->End(0), *cost);
}

INSTANTIATE_TEST_SUITE_P(WithParameters, CumulVarPiecewiseLinearCostTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

using CumulVarPiecewiseLinearCostFailTest = ::testing::TestWithParam<bool>;
TEST_P(CumulVarPiecewiseLinearCostFailTest, DecreasingFunction) {
  const char* kDimension = "test_dimension";
  IndexManager manager(2, 1, NodeIndex(0));
  Model model(manager);
  const int callback =
      GetParam()
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  model.AddDimension(callback, kint64max, kint64max,
                     /*fix_start_cumul_to_zero=*/true, kDimension);
  Dimension* const dimension = model.GetMutableDimension(kDimension);
  std::unique_ptr<PiecewiseLinearFunction> f(
      PiecewiseLinearFunction::CreateFullDomainFunction(0, {0}, {-1, 0}));
  const int64_t index = manager.NodeToIndex(NodeIndex(1));
  dimension->SetCumulVarPiecewiseLinearCost(index, *f);
  EXPECT_EQ(nullptr, dimension->GetCumulVarPiecewiseLinearCost(index));
}

TEST_P(CumulVarPiecewiseLinearCostFailTest, NonMonotonicFunction) {
  const char* kDimension = "test_dimension";
  IndexManager manager(2, 1, NodeIndex(0));
  Model model(manager);
  const int callback =
      GetParam()
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  model.AddDimension(callback, kint64max, kint64max,
                     /*fix_start_cumul_to_zero=*/true, kDimension);
  Dimension* const dimension = model.GetMutableDimension(kDimension);
  std::unique_ptr<PiecewiseLinearFunction> f(
      PiecewiseLinearFunction::CreateFullDomainFunction(0, {0}, {-1, 1}));
  const int64_t index = manager.NodeToIndex(NodeIndex(1));
  dimension->SetCumulVarPiecewiseLinearCost(index, *f);
  EXPECT_EQ(nullptr, dimension->GetCumulVarPiecewiseLinearCost(index));
}

TEST_P(CumulVarPiecewiseLinearCostFailTest, NegativeFunction) {
  const char* kDimension = "test_dimension";
  IndexManager manager(2, 1, NodeIndex(0));
  Model model(manager);
  const int callback =
      GetParam()
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  model.AddDimension(callback, kint64max, kint64max,
                     /*fix_start_cumul_to_zero=*/true, kDimension);
  Dimension* const dimension = model.GetMutableDimension(kDimension);
  std::unique_ptr<PiecewiseLinearFunction> f(
      PiecewiseLinearFunction::CreateFullDomainFunction(-10, {0}, {0, 1}));
  const int64_t index = manager.NodeToIndex(NodeIndex(1));
  dimension->SetCumulVarPiecewiseLinearCost(index, *f);
  EXPECT_EQ(nullptr, dimension->GetCumulVarPiecewiseLinearCost(index));
}

INSTANTIATE_TEST_SUITE_P(WithOrWithoutUnaryCallback,
                         CumulVarPiecewiseLinearCostFailTest,
                         ::testing::Bool());

class ArcIsMoreConstrainedThanArcNoCostsTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<bool, bool, bool, bool>> {
 protected:
  ArcIsMoreConstrainedThanArcNoCostsTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/::testing::get<0>(GetParam()),
            /*use_light_propagation=*/::testing::get<1>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<2>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<3>(GetParam())) {
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    // Model:
    // - One primary dimension.
    // - 3 vehicles.
    // - Nodes:
    //   node #0 is the depot node.
    //   node #1: optional, bound to vehicle #1
    //   node #2: mandatory, not bound, cumul var in the primary dimension <= 10
    //   node #3: mandatory, restricted to vehicle #0 or #2, cumul var in the
    //            primary dimension <= 10.
    //   node #4: mandatory, restricted to vehicle #0 or #2.
    //   node #5: optional, not bound; cumul var in the primary dimension <= 10
    //   node #6: optional, not bound; not restricted.
    //   node #7: mandatory, bound to vehicle #1
    const char kPrimaryDimension[] = "test_primary_dimension";
    manager_ = std::make_unique<IndexManager>(
        /*num_nodes=*/8, /*num_vehicles=*/3, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    const int callback =
        use_unary_callback_
            ? model->RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model->RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    model->AddDimension(callback, 0, kint64max,
                        /*fix_start_cumul_to_zero=*/false, kPrimaryDimension);
    Dimension* const primary_dimension =
        model->GetMutableDimension(kPrimaryDimension);
    EXPECT_EQ("", model->GetPrimaryConstrainedDimension());
    model->SetPrimaryConstrainedDimension(kPrimaryDimension);
    EXPECT_EQ(kPrimaryDimension, model->GetPrimaryConstrainedDimension());
    // We just set the same unperformed penalty to all nodes, by default.
    // Making a node mandatory is done below by forcing its active var to 1.
    const int kUnperformedPenalty = 1;
    for (NodeIndex node(1); node < model->nodes(); ++node) {
      model->AddDisjunction(manager_->NodesToIndices({node}),
                            kUnperformedPenalty);
    }
    // Handy shortcut:
    // var_index[i] == manager_->NodeToIndex(NodeIndex(i))
    std::vector<int64_t> var_index(model->nodes(), -1);
    for (int i = 0; i < var_index.size(); ++i) {
      var_index[i] = manager_->NodeToIndex(NodeIndex(i));
    }
    // Set up the nodes as described above. Note that removing the value '-1'
    // from the VehicleVar domain makes the node mandatory.
    model->VehicleVar(var_index[1])->SetValues({-1, 1});  // Keep it optional.
    model->ActiveVar(var_index[2])->SetMin(1);
    primary_dimension->CumulVar(var_index[2])->SetMax(10);
    model->VehicleVar(var_index[3])->SetValues({0, 2});  // Mandatory.
    primary_dimension->CumulVar(var_index[3])->SetMax(10);
    model->VehicleVar(var_index[4])->SetValues({0, 2});  // Mandatory.
    primary_dimension->CumulVar(var_index[5])->SetMax(10);
    model->VehicleVar(var_index[7])->SetValue(1);  // Mandatory.

    return model;
  }

  bool HasHomogeneousCosts() const override { return true; }
  RoutingSearchParameters GetSearchParameters() override {
    // IMPORTANT: we only care about the first solution. Therefore, we set the
    // solution limit to 1.
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_solution_limit(1);
    parameters.set_first_solution_strategy(
        FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC);
    return parameters;
  }
};

TEST_P(ArcIsMoreConstrainedThanArcNoCostsTest, FirstSolution) {
  // Routes are built from last to first: vehicle #2 gets all the nodes it can,
  // then the others get theirs.
  ExpectRoutes(SetUpModelAndSolve(), {{}, {7, 1}, {3, 2, 4, 5, 6}});
  EXPECT_EQ(2, model_->GetVehicleClassesCount());
}

INSTANTIATE_TEST_SUITE_P(WithParameters, ArcIsMoreConstrainedThanArcNoCostsTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

int64_t LookupCostMatrix(const IndexManager& manager,
                         absl::Span<const std::vector<int64_t>> cost_matrix,
                         int64_t from, int64_t to) {
  return cost_matrix[manager.IndexToNode(from).value()]
                    [manager.IndexToNode(to).value()];
}

class ArcIsMoreConstrainedThanArcCostsTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<::testing::tuple<bool, bool, bool>> {
 protected:
  ArcIsMoreConstrainedThanArcCostsTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/false,
            /*use_light_propagation=*/::testing::get<0>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<1>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<2>(GetParam())) {
  }
  bool HasHomogeneousCosts() const override { return false; }
  int VehicleCostCount() const override { return 2; }
  RoutingSearchParameters GetSearchParameters() override {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_solution_limit(1);
    parameters.set_first_solution_strategy(
        FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC);
    return parameters;
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    // Model:
    // - No primary dimension.
    // - 3 vehicles.
    manager_ = std::make_unique<IndexManager>(
        /*num_nodes=*/6, /*num_vehicles=*/3, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    model->SetPrimaryConstrainedDimension("");
    EXPECT_EQ(std::string(""), model->GetPrimaryConstrainedDimension());

    // Set up the nodes: all optional, all bound to vehicle #1.
    const int64_t kUnperformedPenalty[] = {/*node #1*/ kint64max,
                                           /*2*/ kint64max,
                                           /*3*/ 8, /*4*/ 10, /*5*/ 0};
    for (NodeIndex node(1); node < model->nodes(); ++node) {
      const int64_t index = manager_->NodeToIndex(node);
      model->VehicleVar(index)->SetValues({-1, 1});
      model->AddDisjunction(std::vector<int64_t>({index}),
                            kUnperformedPenalty[node.value() - 1]);
    }

    // Transit costs:
    cost_on_vehicle_from_to_.resize(2);
    cost_on_vehicle_from_to_[0] =
        std::vector<std::vector<int64_t>>(6, std::vector<int64_t>(6, 0));
    cost_on_vehicle_from_to_[1] = {
        // to:         0,   #1  #2  #3  #4  #5
        /*from 0*/ {100, 99, 9, 2, 4, 0},
        /*    #1*/ {200, 0, 0, 99, 20, 9},
        /*    #2*/ {300, 99, 0, 0, 0, 0},
        /*    #3*/ {400, 20, 0, 0, 0, 10},
        /*    #4*/ {500, 30, 0, 10, 0, 0},
        /*    #5*/ {600, 40, 0, 2, 4, 0},
    };
    int first_vehicle_cost = 0;
    for (int vehicle = 0; vehicle < model->vehicles() - 1; ++vehicle) {
      const int vehicle_cost = model->RegisterTransitCallback(absl::bind_front(
          LookupCostMatrix, *manager_, cost_on_vehicle_from_to_[vehicle]));
      if (vehicle == 0) first_vehicle_cost = vehicle_cost;
      model->SetArcCostEvaluatorOfVehicle(vehicle_cost, vehicle);
    }
    model->SetArcCostEvaluatorOfVehicle(first_vehicle_cost,
                                        model->vehicles() - 1);

    return model;
  }

  // Per-vehicle cost matrices, indexed by vehicle, then source NodeIndex, then
  // destination NodeIndex.
  // We use vector<> and not StrictITIVector for the convenient inline array
  // initialization.
  std::vector<std::vector<std::vector<int64_t>>> cost_on_vehicle_from_to_;
};

TEST_P(ArcIsMoreConstrainedThanArcCostsTest, FirstSolution) {
  // We expect the following first solution:
  // vehicle 0 and 2: <none>
  // vehicle 1: 2->1->5->3->4
  //
  // Justification:
  // - vehicle 0 and 2 can't get any nodes, because all nodes are bound to
  //   vehicle 1.
  // - The unperformed penalty of node #1 and #2 take precedence over
  //   everything else. 0->#2 is slightly cheaper thanks to the lower transit
  //   cost. So we have 0->#2->#1 so far.
  // - Then #1->#5 is the cheapest: cost(#1->#5) - unperformed_penalty(#5) is
  //   9-10 = -1, lower than the rest.
  // - Then #5->#3 wins over #5->#4 with the same total, because we break ties
  //   by node var index.
  ExpectRoutes(SetUpModelAndSolve(), {{}, {2, 1, 5, 3, 4}, {}});
  EXPECT_EQ(2, model_->GetVehicleClassesCount());
}

INSTANTIATE_TEST_SUITE_P(WithParameters, ArcIsMoreConstrainedThanArcCostsTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

class GetArcCostForFirstSolutionTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<::testing::tuple<bool, bool, bool>> {
 protected:
  GetArcCostForFirstSolutionTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/false,
            /*use_light_propagation=*/::testing::get<0>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<1>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<2>(GetParam())) {
  }
  int VehicleCostCount() const override { return 1; }
  RoutingSearchParameters GetSearchParameters() override {
    // We're only interested in the first solution.
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_solution_limit(1);
    parameters.set_first_solution_strategy(
        FirstSolutionStrategy::PATH_CHEAPEST_ARC);
    return parameters;
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    // Model: 1 vehicle, 6 non-depot nodes #1, #2, #3, #4, #5, #6. All nodes
    // are optional with a high penalty.
    // Node #2 is bound to be the last node of the route, and node #4 is bound
    // to be the penultimate. So the route must end with ...->#4->#2->END.
    //
    // Here's how the first solution is expected to be evaluated:
    // - From the depot (0):
    //   The arcs from the depot, from cheapest to most expensive, are:
    //     0->#2 (not selected because #2 is directly bound to the end).
    //     0->#4 (not selected because #4 is indirectly bound to the end).
    //     0->#3 (selected)
    //     then 0->#1, 0->#5 and 0->#6 (not selected)
    // - Then from #3:
    //   Among the arcs from #3, ditto: #3->#2 amd #3->#4 are cheapest, but
    //   they aren't selected because they're bound to the end. #3->#1 is
    //   selected because it's cheaper than #3->#5 and #3->#6.
    // - Then from #1:
    //   Since #1->#5, #1->#6 and #1->#4 being forbidden, the route cannot be
    //   continued.. So we backtrack to #3->#6 (cheaper than #3->#5).
    // - Then from #6:
    //   Since #6->#5 is forbidden, we have to close the route (#6->#1 will fail
    //   for the same reason as above): 0->#3->#6->#4->#2->END.
    //
    // This first solution is clearly suboptimal, a better solution is to
    // perform all the nodes with 0->#1->#3->#5->#6->#4->#2->END.
    // This helps us be sure that we're indeed just testing the first solution
    // strategy here; and not the full optimization.
    manager_ = std::make_unique<IndexManager>(
        /*num_nodes=*/7, /*num_vehicles=*/1, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);

    for (NodeIndex node(1); node < model->nodes(); ++node) {
      model->AddDisjunction(manager_->NodesToIndices({node}),
                            /*unperformed penalty*/ 1000000);
    }

    // Transit costs:
    cost_from_to_ = {
        // to:       0,   #1    #2    #3    #4    #5    #6
        /*from 0*/ {9999, 1001, 0, 1000, 1, 1002, 1003},
        /*    #1*/ {9999, 0, 0, 0, 0, 0, 0},
        /*    #2*/ {9999, 0, 0, 0, 0, 0, 0},
        /*    #3*/ {9999, 1000, 0, 0, 1, 1002, 1001},
        /*    #4*/ {9999, 0, 0, 0, 0, 0, 0},
        /*    #5*/ {9999, 0, 0, 0, 0, 0, 0},
        /*    #6*/ {9999, 0, 0, 0, 1000, 0, 0},
    };
    const int vehicle_cost = model->RegisterTransitCallback(
        absl::bind_front(LookupCostMatrix, *manager_, cost_from_to_));
    model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    // Restrictions.
    const int64_t end = model->End(0);
    const int64_t var1 = manager_->NodeToIndex(NodeIndex(1));
    const int64_t var2 = manager_->NodeToIndex(NodeIndex(2));
    const int64_t var4 = manager_->NodeToIndex(NodeIndex(4));
    const int64_t var5 = manager_->NodeToIndex(NodeIndex(5));
    const int64_t var6 = manager_->NodeToIndex(NodeIndex(6));
    model->NextVar(var2)->SetValue(end);
    model->NextVar(var4)->SetValue(var2);
    model->NextVar(var1)->RemoveValues({var4, var5, var6});
    model->NextVar(var6)->RemoveValue(var5);

    return model;
  }

  // Cost matrix, indexed by source NodeIndex, then destination NodeIndex.
  // We use vector<> and not StrictITIVector for the convenient inline array
  // initialization.
  std::vector<std::vector<int64_t>> cost_from_to_;
};

TEST_P(GetArcCostForFirstSolutionTest, FirstSolution) {
  ExpectRoutes(SetUpModelAndSolve(), {{3, 6, 4, 2}});
}

INSTANTIATE_TEST_SUITE_P(WithParameters, GetArcCostForFirstSolutionTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

// Testing that fixed costs are taken into account: building a model with
// vehicle fixed costs but no arc costs, forcing each node to be on a different
// vehicle.
TEST(VehicleFixedCostTest, OnlyFixedVehicleCosts) {
  const int vehicles = 10;
  IndexManager manager(vehicles + 1, vehicles, NodeIndex(0));
  Model model(manager);
  for (int i = 0; i < vehicles; ++i) {
    model.SetFixedCostOfVehicle(i, i);
    model.VehicleVar(manager.NodeToIndex(NodeIndex(i + 1)))->SetValue(i);
  }
  const Assignment* assignment = model.Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(vehicles * (vehicles - 1) / 2, assignment->ObjectiveValue());
}

class BaseAllCostsTest : public BaseRoutingTest,
                         public ::testing::WithParamInterface<
                             ::testing::tuple<bool, bool, bool, bool>> {
 protected:
  static const int64_t kPenalty;
  static const int64_t kVehicles;
  BaseAllCostsTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/::testing::get<0>(GetParam()),
            /*use_light_propagation=*/::testing::get<1>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<2>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<3>(GetParam())) {
  }
  ~BaseAllCostsTest() override = default;
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    manager_ = std::make_unique<IndexManager>(4, kVehicles, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    SetFixedCosts(model);
    std::vector<int64_t> index_disjunction(2);
    for (int i = 0; i < index_disjunction.size(); ++i) {
      index_disjunction[i] = manager_->NodeToIndex(NodeIndex(i + 1));
    }
    model->AddDisjunction(index_disjunction, kPenalty);
    std::vector<int64_t> unactive_index(1, manager_->NodeToIndex(NodeIndex(1)));
    model->AddDisjunction(unactive_index, kPenalty);
    const int vehicle_cost =
        use_unary_callback_
            ? model->RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model->RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    model->ActiveVar(1)->SetMax(0);
    model->ActiveVar(2)->SetMax(0);
    model->ActiveVar(3)->SetMin(1);
    AddExtraConstraint(model);
    return model;
  }
  virtual void SetFixedCosts(Model* model) = 0;
  virtual void AddExtraConstraint(Model* /*model*/) {}
  int VehicleCostCount() const override { return 1; }
};

const int64_t BaseAllCostsTest::kPenalty = 10;
const int64_t BaseAllCostsTest::kVehicles = 2;

class AllCostsTest : public BaseAllCostsTest {
 protected:
  static const int64_t kFixedCost;
  void SetFixedCosts(Model* model) override {
    model->SetFixedCostOfAllVehicles(kFixedCost);
    EXPECT_EQ(kFixedCost, model->GetFixedCostOfVehicle(0));
  }
};

const int64_t AllCostsTest::kFixedCost = 1000;

TEST_P(AllCostsTest, Cost) {
  const Assignment* solution = SetUpModelAndSolve();
  EXPECT_EQ(2 * kPenalty + 2 + kFixedCost, solution->ObjectiveValue())
      << model_->DebugOutputAssignment(*solution, "");
  EXPECT_EQ(1, model_->GetVehicleClassesCount());
}

TEST_P(AllCostsTest, WriteRead) {
  {
    const std::string filename =
        file::JoinPath(::testing::TempDir(), "tmp.txt");
    const int64_t cost = SetUpModelAndSolve()->ObjectiveValue();
    EXPECT_TRUE(model_->WriteAssignment(filename));
    const Assignment* read_assignment = model_->ReadAssignment(filename);
    ASSERT_NE(read_assignment, nullptr);
    EXPECT_EQ(cost, read_assignment->ObjectiveValue());
  }
  // Restart from scratch, reading the assignment that we just wrote out.
  {
    SetUpModel();
    EXPECT_EQ(RoutingSearchStatus::ROUTING_NOT_SOLVED, model_->status());
    const std::string filename =
        file::JoinPath(::testing::TempDir(), "tmp.txt");
    const Assignment* read_assignment = model_->ReadAssignment(filename);
    EXPECT_EQ(RoutingSearchStatus::ROUTING_SUCCESS, model_->status());
    ASSERT_NE(read_assignment, nullptr);
    EXPECT_EQ(2 * kPenalty + 2 + kFixedCost, read_assignment->ObjectiveValue());
  }
}

INSTANTIATE_TEST_SUITE_P(WithParameters, AllCostsTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

using SimpleHeterogeneousCostsTest = ::testing::TestWithParam<bool>;
TEST_P(SimpleHeterogeneousCostsTest, Cost) {
  const int kNodes = 4;
  const int64_t kVehicleCosts[] = {1LL, 2LL};
  const int kTestNodePair[] = {0, 1};
  IndexManager manager(kNodes, std::size(kVehicleCosts), NodeIndex(0));
  Model model(manager);
  // Checking support for vehicle cost modification.
  for (int vehicle = 0; vehicle < std::size(kVehicleCosts); ++vehicle) {
    const int vehicle_cost =
        GetParam() ? model.RegisterUnaryTransitCallback(
                         [&kVehicleCosts, vehicle](int64_t) {
                           return 2 * kVehicleCosts[vehicle];
                         })
                   : model.RegisterTransitCallback(
                         [&kVehicleCosts, vehicle](int64_t, int64_t) {
                           return 2 * kVehicleCosts[vehicle];
                         });
    model.SetArcCostEvaluatorOfVehicle(vehicle_cost, vehicle);
  }
  for (int vehicle = 0; vehicle < std::size(kVehicleCosts); ++vehicle) {
    const int vehicle_cost =
        GetParam() ? model.RegisterUnaryTransitCallback(
                         [&kVehicleCosts, vehicle](int64_t) {
                           return kVehicleCosts[vehicle];
                         })
                   : model.RegisterTransitCallback(
                         [&kVehicleCosts, vehicle](int64_t, int64_t) {
                           return kVehicleCosts[vehicle];
                         });
    model.SetArcCostEvaluatorOfVehicle(vehicle_cost, vehicle);
  }
  model.CloseModel();
  EXPECT_EQ(std::size(kVehicleCosts), model.GetVehicleClassesCount());
  EXPECT_EQ(std::size(kVehicleCosts), model.GetNonZeroCostClassesCount());
  for (int vehicle = 0; vehicle < std::size(kVehicleCosts); ++vehicle) {
    EXPECT_EQ(kVehicleCosts[vehicle],
              model.GetArcCostForVehicle(kTestNodePair[0], kTestNodePair[1],
                                         vehicle));
  }
}

INSTANTIATE_TEST_SUITE_P(WithOrWithoutUnaryCallback,
                         SimpleHeterogeneousCostsTest, ::testing::Bool());

class HeterogeneousCostsRoutingTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<bool, bool, bool, bool>> {
 protected:
  HeterogeneousCostsRoutingTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/::testing::get<0>(GetParam()),
            /*use_light_propagation=*/::testing::get<1>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<2>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<3>(GetParam())),
        vehicle_(-1) {}
  ~HeterogeneousCostsRoutingTest() override = default;
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    const int kNodes = 2;
    const int kVehicles = 3;
    const int kVehicleCosts[] = {0, 1, 0};
    const std::vector<int64_t> costs = {1, 2};
    manager_ = std::make_unique<IndexManager>(kNodes, kVehicles, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    EXPECT_EQ(0, model->GetNonZeroCostClassesCount());
    std::vector<int> cost_to_cost_index(costs.size());
    for (int i = 0; i < costs.size(); ++i) {
      // 'costs' goes out of scope before the model is solved, so we must pass
      // it by copy to the cost function.
      cost_to_cost_index[i] =
          ::testing::get<0>(GetParam())
              ? model->RegisterUnaryTransitCallback(
                    [costs, i](int64_t) { return costs[i]; })
              : model->RegisterTransitCallback(
                    [costs, i](int64_t, int64_t) { return costs[i]; });
    }
    for (int i = 0; i < kVehicles; ++i) {
      model->SetArcCostEvaluatorOfVehicle(cost_to_cost_index[kVehicleCosts[i]],
                                          i);
    }
    ConstrainToVehicle(model);
    return model;
  }
  bool HasHomogeneousCosts() const override { return false; }
  int VehicleCostCount() const override { return 2; }

  void ConstrainToVehicle(Model* model) {
    model->VehicleVar(1)->SetValue(vehicle_);
  }
  void SetVehicle(int vehicle) { vehicle_ = vehicle; }

 private:
  int vehicle_;
};

TEST_P(HeterogeneousCostsRoutingTest, PlanCosts) {
  const int64_t kExpectedCosts[] = {2, 4, 2};
  for (int i = 0; i < std::size(kExpectedCosts); ++i) {
    SetVehicle(i);
    const Assignment* const assignment = SetUpModelAndSolve();
    ASSERT_NE(assignment, nullptr);
    EXPECT_EQ(kExpectedCosts[i], assignment->ObjectiveValue());
  }
  EXPECT_EQ(3, model_->GetVehicleClassesCount());
}

INSTANTIATE_TEST_SUITE_P(WithParameters, HeterogeneousCostsRoutingTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

const int64_t kFixedCosts[] = {1000, 10000};

class FixedCostOfVehicleTest : public BaseAllCostsTest {
 protected:
  void SetFixedCosts(Model* model) override {
    for (int i = 0; i < kVehicles; ++i) {
      model->SetFixedCostOfVehicle(kFixedCosts[i], i);
      EXPECT_EQ(kFixedCosts[i], model->GetFixedCostOfVehicle(i));
    }
  }
};

class FixedCostOfVehicleTest1 : public FixedCostOfVehicleTest {
 protected:
  static constexpr int kVehicle = 0;
  void AddExtraConstraint(Model* model) override {
    model->VehicleVar(3)->SetValue(kVehicle);
  }
};

TEST_P(FixedCostOfVehicleTest1, Cost) {
  EXPECT_EQ(2 * kPenalty + 2 + kFixedCosts[kVehicle],
            SetUpModelAndSolve()->ObjectiveValue());
  EXPECT_EQ(2, model_->GetVehicleClassesCount());
}

INSTANTIATE_TEST_SUITE_P(WithParameters, FixedCostOfVehicleTest1,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

class FixedCostOfVehicleTest2 : public FixedCostOfVehicleTest {
 protected:
  static constexpr int kVehicle = 1;
  void AddExtraConstraint(Model* model) override {
    model->VehicleVar(3)->SetValue(kVehicle);
  }
};

TEST_P(FixedCostOfVehicleTest2, Cost) {
  EXPECT_EQ(2 * kPenalty + 2 + kFixedCosts[kVehicle],
            SetUpModelAndSolve()->ObjectiveValue());
  EXPECT_EQ(2, model_->GetVehicleClassesCount());
}

INSTANTIATE_TEST_SUITE_P(WithParameters, FixedCostOfVehicleTest2,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

// Allows to enumerate all solutions of a constraint model.
class EnumerationTest : public ::testing::Test {
 protected:
  // Returns the number of solutions of the model,
  // or max_solutions if it has more.
  int64_t EnumerateAllSolutions(Solver* solver,
                                const std::vector<IntVar*>& int_vars,
                                const std::vector<IntervalVar*>& interval_vars,
                                int64_t max_solutions);
};

int64_t EnumerationTest::EnumerateAllSolutions(
    Solver* solver, const std::vector<IntVar*>& int_vars,
    const std::vector<IntervalVar*>& interval_vars, int64_t max_solutions) {
  std::vector<IntVar*> vars = int_vars;
  for (auto* interval : interval_vars) {
    vars.push_back(interval->StartExpr()->Var());
    vars.push_back(interval->DurationExpr()->Var());
  }
  DecisionBuilder* db =
      solver->MakePhase(vars, Solver::CHOOSE_RANDOM, Solver::SPLIT_UPPER_HALF);
  solver->NewSearch(db);
  int64_t num_solutions = 0;
  while (max_solutions-- > 0 && solver->NextSolution()) {
    ++num_solutions;
  }
  solver->EndSearch();
  return num_solutions;
}

// TODO(user): The following test cases do not examine many situations,
// we need a test that enumerates all solutions to detect bugs more reliably.
// One vehicle, one node to visit.
// One break between 10 and 12, duration 5.
// One node to visit starting between 12 and 13, with a transit of 1.
// Expected result: break is scheduled after node pushing end of route to
// 12 (node visit time) + 5 (break duration) + 1 (transit) = 18.
using RoutingWithBreaksTest = ::testing::TestWithParam<bool>;
TEST_P(RoutingWithBreaksTest, OneBreak) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model routing(manager);
  const int callback =
      GetParam()
          ? routing.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : routing.RegisterTransitCallback(
                [](int64_t, int64_t) { return 1LL; });
  routing.AddDimension(callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/false, "time");
  Dimension* const time_dimension = routing.GetMutableDimension("time");
  for (int i = 0; i < routing.Size(); ++i) {
    routing.AddToAssignment(time_dimension->SlackVar(i));
  }
  time_dimension->CumulVar(1)->SetRange(12, 13);
  IntervalVar* const break_interval =
      routing.solver()->MakeFixedDurationIntervalVar(10, 12, 5, false, "break");
  time_dimension->SetBreakIntervalsOfVehicle({break_interval}, 0, {0, 0});
  const Assignment* solution = routing.Solve();
  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(18, solution->Value(time_dimension->CumulVar(routing.End(0))));
  EXPECT_EQ(5, solution->Value(time_dimension->SlackVar(1)));
}

// One vehicle, one visit. A break must be done at the second step because of
// time constraints.
TEST_P(RoutingWithBreaksTest, BreakAfterVisit) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model routing(manager);
  const int callback =
      GetParam()
          ? routing.RegisterUnaryTransitCallback([](int64_t) { return 15LL; })
          : routing.RegisterTransitCallback(
                [](int64_t, int64_t) { return 15LL; });
  routing.AddDimension(callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/false, "time");
  Dimension* const time_dimension = routing.GetMutableDimension("time");
  time_dimension->CumulVar(0)->SetRange(0, 0);
  time_dimension->CumulVar(2)->SetRange(40, 40);

  for (int i = 0; i < routing.Size(); ++i) {
    routing.AddToAssignment(time_dimension->SlackVar(i));
  }
  IntervalVar* const break_interval =
      routing.solver()->MakeFixedDurationIntervalVar(25, 30, 10, false, "b");
  time_dimension->SetBreakIntervalsOfVehicle({break_interval}, 0, {0, 0});
  const Assignment* solution = routing.Solve();
  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(15, solution->Value(time_dimension->CumulVar(1)));
}

// One vehicle, one node to visit.
// One break starting between 10 and 13, duration 5.
// One node to visit between 12 and 13, with a transit of 2 (1 service, 1
// travel). Expected result: break is scheduled after node pushing end of route
// to 12 (node visit time) + 1 (service) + 5 (break duration) + 1 (travel) = 19.
TEST_P(RoutingWithBreaksTest, OneBreak2) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model routing(manager);
  const int callback =
      GetParam()
          ? routing.RegisterUnaryTransitCallback([](int64_t) { return 2LL; })
          : routing.RegisterTransitCallback(
                [](int64_t, int64_t) { return 2LL; });
  routing.AddDimension(callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/false, "time");
  Dimension* const time_dimension = routing.GetMutableDimension("time");
  for (int i = 0; i < routing.Size(); ++i) {
    routing.AddToAssignment(time_dimension->SlackVar(i));
  }
  time_dimension->CumulVar(1)->SetRange(12, 13);
  IntervalVar* const break_interval =
      routing.solver()->MakeFixedDurationIntervalVar(10, 13, 5, false, "break");
  time_dimension->SetBreakIntervalsOfVehicle({break_interval}, 0, {1, 1});
  const Assignment* solution = routing.Solve();
  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(19, solution->Value(time_dimension->CumulVar(routing.End(0))));
  EXPECT_EQ(5, solution->Value(time_dimension->SlackVar(1)));
}

// One vehicle, one node to visit.
// A break of duration 10 must be inserted in [10, 40),
// but cannot start at 10 because the post-travel to the visit and pre-travel
// from the visit push it back to at least 25.
TEST_P(RoutingWithBreaksTest, PrePostTravelMustBeTakenIntoAccount) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model routing(manager);
  const int transit_callback =
      GetParam() ? routing.RegisterUnaryTransitCallback(
                       [](int64_t) { return int64_t{10}; })
                 : routing.RegisterTransitCallback(
                       [](int64_t, int64_t) { return int64_t{10}; });
  const int pre_travel_callback = routing.RegisterTransitCallback(
      [](int64_t, int64_t) { return int64_t{5}; });
  const int post_travel_callback = routing.RegisterTransitCallback(
      [](int64_t, int64_t) { return int64_t{5}; });
  routing.AddDimension(transit_callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/false, "time");
  Dimension* const time_dimension = routing.GetMutableDimension("time");
  time_dimension->CumulVar(0)->SetRange(0, 0);
  time_dimension->CumulVar(1)->SetRange(20, 20);
  time_dimension->CumulVar(2)->SetRange(40, 40);
  IntervalVar* const break_interval =
      routing.solver()->MakeFixedDurationIntervalVar(10, 25, 10, false, "br");
  time_dimension->SetBreakIntervalsOfVehicle(
      {break_interval}, 0, pre_travel_callback, post_travel_callback);
  routing.AddToAssignment(break_interval->StartExpr()->Var());
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_use_depth_first_search(true);
  const Assignment* solution = routing.SolveWithParameters(parameters);
  ASSERT_NE(solution, nullptr);
  EXPECT_GE(25, solution->Value(break_interval->StartExpr()->Var()));
}

// One vehicle, one node to visit.
// A break of duration 10 must be inserted in [10, 40),
// but cannot happen during the first step, because the travel is too large.
TEST_P(RoutingWithBreaksTest, TransitMustBeTakenIntoAccount) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model routing(manager);
  const int transit_callback =
      GetParam() ? routing.RegisterUnaryTransitCallback(
                       [](int64_t) { return int64_t{15}; })
                 : routing.RegisterTransitCallback(
                       [](int64_t, int64_t) { return int64_t{15}; });
  routing.AddDimension(transit_callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/false, "time");
  Dimension* const time_dimension = routing.GetMutableDimension("time");
  time_dimension->CumulVar(0)->SetRange(0, 0);
  time_dimension->CumulVar(1)->SetRange(20, 20);
  time_dimension->CumulVar(2)->SetRange(50, 50);
  IntervalVar* const break_interval =
      routing.solver()->MakeFixedDurationIntervalVar(0, 25, 10, false, "br");
  time_dimension->SetBreakIntervalsOfVehicle({break_interval}, 0, {0, 0});
  routing.AddToAssignment(break_interval->StartExpr()->Var());
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_use_depth_first_search(true);
  const Assignment* solution = routing.SolveWithParameters(parameters);
  ASSERT_NE(solution, nullptr);
  EXPECT_GE(20, solution->Value(break_interval->StartExpr()->Var()));
}

// One vehicle, one node to visit.
// A test where the distance does not respect triangular inequality.
// This should still be feasible.
TEST_P(RoutingWithBreaksTest, NonEuclidianDistanceTest) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model routing(manager);
  const int transit_callback =
      GetParam() ? routing.RegisterUnaryTransitCallback(
                       [](int64_t) { return int64_t{10}; })
                 : routing.RegisterTransitCallback([](int64_t i, int64_t j) {
                     return (i == 0 && j == 2) ? 50 : 10;
                   });
  routing.AddDimension(transit_callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/false, "time");
  Dimension* const time_dimension = routing.GetMutableDimension("time");
  time_dimension->CumulVar(0)->SetRange(0, 0);
  time_dimension->CumulVar(1)->SetRange(20, 20);
  time_dimension->CumulVar(2)->SetRange(40, 40);
  IntervalVar* const break_interval =
      routing.solver()->MakeFixedDurationIntervalVar(0, 40, 10, false, "br");
  time_dimension->SetBreakIntervalsOfVehicle({break_interval}, 0, {0, 0});
  routing.AddToAssignment(break_interval->StartExpr()->Var());
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_use_depth_first_search(true);
  const Assignment* solution = routing.SolveWithParameters(parameters);
  ASSERT_NE(solution, nullptr);
}

// Same as OneBreak but break happens after the end of the route (node to visit
// does not have a time window).
TEST_P(RoutingWithBreaksTest, OneBreakAfterEnd) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model routing(manager);
  const int callback =
      GetParam()
          ? routing.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : routing.RegisterTransitCallback(
                [](int64_t, int64_t) { return 1LL; });
  routing.AddDimension(callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/false, "time");
  Dimension* const time_dimension = routing.GetMutableDimension("time");
  time_dimension->CumulVar(routing.End(0))->SetMax(11);
  for (int i = 0; i < routing.Size(); ++i) {
    routing.AddToAssignment(time_dimension->SlackVar(i));
  }
  IntervalVar* const break_interval =
      routing.solver()->MakeFixedDurationIntervalVar(10, 12, 5, false, "break");
  time_dimension->SetBreakIntervalsOfVehicle({break_interval}, 0, {0, 0});
  const Assignment* solution = routing.Solve();
  ASSERT_NE(solution, nullptr);
  EXPECT_GE(10, solution->Value(time_dimension->CumulVar(routing.End(0))));
  EXPECT_EQ(0, solution->Value(time_dimension->SlackVar(0)));
  EXPECT_EQ(0, solution->Value(time_dimension->SlackVar(1)));
}

// Same as OneBreak except break is optional and will skipped. Therefore the
// route ends at 13.
TEST_P(RoutingWithBreaksTest, OneOptionalBreak) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model routing(manager);
  const int callback =
      GetParam()
          ? routing.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : routing.RegisterTransitCallback(
                [](int64_t, int64_t) { return 1LL; });
  routing.AddDimension(callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/false, "time");
  Dimension* const time_dimension = routing.GetMutableDimension("time");
  for (int i = 0; i < routing.Size(); ++i) {
    routing.AddToAssignment(time_dimension->SlackVar(i));
  }
  time_dimension->CumulVar(1)->SetRange(12, 13);
  IntervalVar* const break_interval =
      routing.solver()->MakeFixedDurationIntervalVar(10, 12, 5, true, "break");
  time_dimension->SetBreakIntervalsOfVehicle({break_interval}, 0, {0, 0});
  const Assignment* const solution = routing.Solve();
  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(13, solution->Value(time_dimension->CumulVar(routing.End(0))));
  EXPECT_EQ(0, solution->Value(time_dimension->SlackVar(1)));
}

// Same as OneOptionalBreak, where break is skipped because it conflicts with
// service time.
TEST_P(RoutingWithBreaksTest, OneOptionalBreak2) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model routing(manager);
  const int callback =
      GetParam()
          ? routing.RegisterUnaryTransitCallback([](int64_t) { return 2LL; })
          : routing.RegisterTransitCallback(
                [](int64_t, int64_t) { return 2LL; });
  routing.AddDimension(callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/false, "time");
  Dimension* const time_dimension = routing.GetMutableDimension("time");
  for (int i = 0; i < routing.Size(); ++i) {
    routing.AddToAssignment(time_dimension->SlackVar(i));
  }
  time_dimension->CumulVar(1)->SetRange(12, 13);
  IntervalVar* const break_interval =
      routing.solver()->MakeFixedDurationIntervalVar(10, 13, 5, true, "break");
  time_dimension->SetBreakIntervalsOfVehicle({break_interval}, 0, {1, 1});
  const Assignment* solution = routing.Solve();
  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(14, solution->Value(time_dimension->CumulVar(routing.End(0))));
  EXPECT_EQ(0, solution->Value(time_dimension->SlackVar(1)));
}

// Same as OneBreak except route must end after 20, which implies a slack of
// 6 or 7, which is greater than the duration of the break. The break will
// happen during the slack time.
TEST_P(RoutingWithBreaksTest, OneBreakWithSlack) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model routing(manager);
  const int callback =
      GetParam()
          ? routing.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : routing.RegisterTransitCallback(
                [](int64_t, int64_t) { return 1LL; });
  routing.AddDimension(callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/false, "time");
  Dimension* const time_dimension = routing.GetMutableDimension("time");
  for (int i = 0; i < routing.Size(); ++i) {
    routing.AddToAssignment(time_dimension->SlackVar(i));
  }
  time_dimension->CumulVar(1)->SetRange(12, 13);
  time_dimension->CumulVar(routing.End(0))->SetMin(20);
  IntervalVar* const break_interval =
      routing.solver()->MakeFixedDurationIntervalVar(10, 12, 5, false, "break");
  time_dimension->SetBreakIntervalsOfVehicle({break_interval}, 0, {0, 0});
  const Assignment* const solution = routing.Solve();
  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(20, solution->Value(time_dimension->CumulVar(routing.End(0))));
  EXPECT_EQ(7, solution->Value(time_dimension->SlackVar(1)));
}

// Same as OneBreak expect two breaks must happen, the second being between
// 15 and 17 with a duration of 1. The route then ends at 19.
TEST_P(RoutingWithBreaksTest, TwoBreaks) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model routing(manager);
  const int callback =
      GetParam()
          ? routing.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : routing.RegisterTransitCallback(
                [](int64_t, int64_t) { return 1LL; });
  routing.AddDimension(callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/false, "time");
  Dimension* const time_dimension = routing.GetMutableDimension("time");
  for (int i = 0; i < routing.Size(); ++i) {
    routing.AddToAssignment(time_dimension->SlackVar(i));
  }
  time_dimension->CumulVar(1)->SetRange(12, 13);
  IntervalVar* const break_interval1 =
      routing.solver()->MakeFixedDurationIntervalVar(10, 12, 5, false, "break");
  IntervalVar* const break_interval2 =
      routing.solver()->MakeFixedDurationIntervalVar(15, 17, 1, false, "break");
  time_dimension->SetBreakIntervalsOfVehicle({break_interval1, break_interval2},
                                             0, {0, 0});
  const Assignment* const solution = routing.Solve();
  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(19, solution->Value(time_dimension->CumulVar(routing.End(0))));
  EXPECT_EQ(6, solution->Value(time_dimension->SlackVar(1)));
}

// Same as TwoBreaks (with a slight modification to break dates) except only one
// of the two breaks must be performed.
TEST_P(RoutingWithBreaksTest, TwoBreakAlternatives) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model routing(manager);
  const int callback =
      GetParam()
          ? routing.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : routing.RegisterTransitCallback(
                [](int64_t, int64_t) { return 1LL; });
  routing.AddDimension(callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/false, "time");
  Dimension* const time_dimension = routing.GetMutableDimension("time");
  for (int i = 0; i < routing.Size(); ++i) {
    routing.AddToAssignment(time_dimension->SlackVar(i));
  }
  time_dimension->CumulVar(1)->SetValue(12);
  IntervalVar* const break_interval1 =
      routing.solver()->MakeFixedDurationIntervalVar(10, 12, 5, true, "break");
  IntervalVar* const break_interval2 =
      routing.solver()->MakeFixedDurationIntervalVar(10, 12, 3, true, "break");
  Solver* const solver = routing.solver();
  solver->AddConstraint(solver->MakeNonEquality(
      break_interval1->PerformedExpr(), break_interval2->PerformedExpr()));
  time_dimension->SetBreakIntervalsOfVehicle({break_interval1, break_interval2},
                                             0, {0, 0});
  const Assignment* const solution = routing.Solve();
  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(16, solution->Value(time_dimension->CumulVar(routing.End(0))));
  EXPECT_EQ(3, solution->Value(time_dimension->SlackVar(1)));
}

// This test checks that a break leaves enough time for transits to happen.
// All transits are equal to 1, only node 1 has 1 of possible slack into which
// the break can go.
TEST_P(RoutingWithBreaksTest, OneBreakOnePosition) {
  IndexManager manager(3, 1, NodeIndex(0));
  Model routing(manager);
  const int callback =
      GetParam()
          ? routing.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : routing.RegisterTransitCallback(
                [](int64_t, int64_t) { return 1LL; });
  routing.AddDimension(callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/true, "time");
  Dimension* const time_dimension = routing.GetMutableDimension("time");
  for (int i = 0; i < routing.Size(); ++i) {
    routing.AddToAssignment(time_dimension->SlackVar(i));
  }
  routing.NextVar(0)->SetValue(1);
  routing.NextVar(1)->SetValue(2);
  routing.NextVar(2)->SetValue(3);
  time_dimension->CumulVar(1)->SetValue(1);
  time_dimension->CumulVar(2)->SetValue(3);
  time_dimension->CumulVar(routing.End(0))->SetValue(4);
  IntervalVar* const break_interval =
      routing.solver()->MakeFixedDurationIntervalVar(0, 3, 1, false, "break");
  time_dimension->SetBreakIntervalsOfVehicle({break_interval}, 0, {0, 0, 0});
  const Assignment* const solution = routing.Solve();
  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(4, solution->Value(time_dimension->CumulVar(routing.End(0))));
  EXPECT_EQ(0, solution->Value(time_dimension->SlackVar(0)));
  EXPECT_EQ(1, solution->Value(time_dimension->SlackVar(1)));
  EXPECT_EQ(0, solution->Value(time_dimension->SlackVar(2)));
}

// Because of breaks, one vehicle cannot do both visits.
// This tests vehicle triggers.
//
// V0 -[1]-> [Cumul=1,Service=1] A -[1]-> End0
// V1 -[1]-> [Cumul=1,Service=1] B -[1]-> End1
TEST_P(RoutingWithBreaksTest, TwoVehiclesMustBeUsed) {
  IndexManager manager(4, 2, NodeIndex(0));
  Model routing(manager);
  const int callback =
      GetParam()
          ? routing.RegisterUnaryTransitCallback([](int64_t) { return 2LL; })
          : routing.RegisterTransitCallback(
                [](int64_t, int64_t) { return 2LL; });
  routing.AddDimension(callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/true, "time");
  Dimension* const time_dimension = routing.GetMutableDimension("time");
  for (int i = 0; i < routing.Size(); ++i) {
    routing.AddToAssignment(time_dimension->SlackVar(i));
  }
  time_dimension->CumulVar(2)->SetRange(2, 5);
  time_dimension->CumulVar(3)->SetRange(2, 5);
  std::vector<IntervalVar*> const break_intervals_0{
      routing.solver()->MakeFixedDurationIntervalVar(2, 5, 1, false, "break"),
      routing.solver()->MakeFixedDurationIntervalVar(2, 5, 1, false, "break")};
  routing.solver()->AddConstraint(routing.solver()->MakeIntervalVarRelation(
      break_intervals_0[0], Solver::STARTS_AFTER_END, break_intervals_0[1]));
  std::vector<IntervalVar*> const break_intervals_1{
      routing.solver()->MakeFixedDurationIntervalVar(2, 5, 1, false, "break"),
      routing.solver()->MakeFixedDurationIntervalVar(2, 5, 1, false, "break")};
  routing.solver()->AddConstraint(routing.solver()->MakeIntervalVarRelation(
      break_intervals_1[0], Solver::STARTS_AFTER_END, break_intervals_1[1]));
  time_dimension->SetBreakIntervalsOfVehicle(break_intervals_0, 0,
                                             {1, 1, 1, 1, 1, 1});
  time_dimension->SetBreakIntervalsOfVehicle(break_intervals_1, 1,
                                             {1, 1, 1, 1, 1, 1});
  const Assignment* const solution = routing.Solve();
  ASSERT_NE(solution, nullptr);
  EXPECT_NE(3, solution->Value(routing.NextVar(2)));
  EXPECT_NE(2, solution->Value(routing.NextVar(3)));
}

TEST_P(RoutingWithBreaksTest, BreaksDistanceWithLargeDomain) {
  const int kNumNodes = 6;
  const int kNumBreaks = 5;
  RoutingIndexManager manager(1 + kNumNodes, 1, NodeIndex(0));
  RoutingModel routing(manager);
  const int callback = GetParam()
                           ? routing.RegisterUnaryTransitCallback(
                                 [](int64_t i) { return i * 10000; })
                           : routing.RegisterTransitCallback(
                                 [](int64_t i, int64_t) { return i * 10000; });
  const int64_t kHorizon = 1000000;
  routing.AddDimension(callback, kHorizon, kHorizon,
                       /*fix_start_cumul_to_zero=*/false, "time");
  RoutingDimension* const time_dimension = routing.GetMutableDimension("time");

  std::vector<IntervalVar*> break_intervals;
  for (int i = 0; i < kNumBreaks; ++i) {
    break_intervals.push_back(routing.solver()->MakeFixedDurationIntervalVar(
        0, kHorizon, 30000, false, "break"));
    if (i == 0) continue;
    routing.solver()->AddConstraint(routing.solver()->MakeIntervalVarRelation(
        break_intervals[i], Solver::STARTS_AFTER_END, break_intervals[i - 1]));
  }
  time_dimension->SetBreakIntervalsOfVehicle(
      break_intervals, 0, std::vector<int64_t>(1 + kNumNodes, 0));
  time_dimension->SetBreakDistanceDurationOfVehicle(50000, 30000, 0);

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.mutable_time_limit()->set_seconds(10);
  const Assignment* const solution = routing.SolveWithParameters(parameters);
  ASSERT_NE(solution, nullptr);
  int nodes_visited = 0;
  int64_t current = routing.Start(0);
  while (!routing.IsEnd(current)) {
    current = solution->Value(routing.NextVar(current));
    if (!routing.IsEnd(current)) {
      nodes_visited++;
    }
  }
  EXPECT_EQ(nodes_visited, kNumNodes);
}

INSTANTIATE_TEST_SUITE_P(WithOrWithoutUnaryCallback, RoutingWithBreaksTest,
                         ::testing::Bool());

// Only one solution for cumuls, breaks are free:
// 01234567890123456789012345678901
// s..bb.vv..bbvv.bb.e            // All nodes are in the same group, no delay.
// s...   bbddvv.bb.ddvv.. bbdde  // All nodes have delay.
TEST(RoutingWithTransitionDelaysAndBreaksTest, VariousTimingProblems) {
  IndexManager manager(8, 2,
                       /*starts*/ {NodeIndex(0), NodeIndex(1)},
                       /*ends*/ {NodeIndex(2), NodeIndex(3)});
  Model routing(manager);
  ASSERT_EQ(routing.Start(0), 0);
  ASSERT_EQ(routing.Start(1), 1);
  ASSERT_EQ(routing.End(0), 6);
  ASSERT_EQ(routing.End(1), 7);
  // Routes will be {0, 2, 4, 6} and {1, 3, 5, 7}.
  // Group delays: first route {0, 2, 4, 6} has no group delays,
  // second route {1, 3, 5, 7} has delay 2 between every node.
  // Fix routes, we are only interested in the timing problem.
  routing.NextVar(0)->SetValue(2);
  routing.NextVar(2)->SetValue(4);
  routing.NextVar(4)->SetValue(6);
  routing.NextVar(1)->SetValue(3);
  routing.NextVar(3)->SetValue(5);
  routing.NextVar(5)->SetValue(7);
  auto transition_delay = [](int from, int to) -> int64_t {
    const bool has_delay = (from % 2 == 1) || (to % 2 == 1);
    return has_delay ? 2LL : 0LL;
  };

  const int callback =
      routing.RegisterTransitCallback([&](int64_t from, int64_t to) {
        return 4LL + transition_delay(from, to);
      });
  routing.AddDimension(callback, kint64max,
                       /*capacity*/ 100,
                       /*fix_start_cumul_to_zero=*/true, "time");
  Dimension* const time_dimension = routing.GetMutableDimension("time");
  time_dimension->CumulVar(6)->SetMax(18);
  time_dimension->CumulVar(7)->SetMax(28);

  std::vector<IntervalVar*> const break_intervals_0{
      routing.solver()->MakeFixedDurationIntervalVar(0, 5, 2, false, "break"),
      routing.solver()->MakeFixedDurationIntervalVar(7, 11, 2, false, "break"),
      routing.solver()->MakeFixedDurationIntervalVar(15, 15, 2, false,
                                                     "break")};
  std::vector<IntervalVar*> const break_intervals_1{
      routing.solver()->MakeFixedDurationIntervalVar(7, 7, 2, false, "break"),
      routing.solver()->MakeFixedDurationIntervalVar(14, 14, 2, false, "break"),
      routing.solver()->MakeFixedDurationIntervalVar(24, 24, 2, false,
                                                     "break")};
  time_dimension->SetBreakIntervalsOfVehicle(
      break_intervals_0, 0, {0, 0, 2, 2, 2, 2, 0, 0}, transition_delay);
  time_dimension->SetBreakIntervalsOfVehicle(
      break_intervals_1, 1, {0, 0, 2, 2, 2, 2, 0, 0}, transition_delay);

  const Assignment* const solution = routing.Solve();
  ASSERT_NE(solution, nullptr);

  EXPECT_EQ(6, solution->Value(time_dimension->CumulVar(2)));
  EXPECT_EQ(12, solution->Value(time_dimension->CumulVar(4)));
  EXPECT_EQ(18, solution->Value(time_dimension->CumulVar(6)));

  EXPECT_EQ(11, solution->Value(time_dimension->CumulVar(3)));
  EXPECT_EQ(19, solution->Value(time_dimension->CumulVar(5)));
  EXPECT_EQ(28, solution->Value(time_dimension->CumulVar(7)));
}

// Test routing -> breaks distance communication.
TEST_F(EnumerationTest, BreaksDistance) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model model(manager);
  Solver* solver = model.solver();

  const int transit = model.RegisterTransitCallback(
      absl::bind_front(ValueIfDifferent, manager, 1));
  model.AddDimension(transit, kint64max, kint64max, false, "typicallytime");
  Dimension* dimension = model.GetMutableDimension("typicallytime");
  IntVar* start_var = dimension->CumulVar(model.Start(0));
  IntVar* end_var = dimension->CumulVar(model.End(0));
  const int64_t begin = 0;
  const int64_t end = 24;
  start_var->SetRange(begin, begin);
  end_var->SetRange(end, end);

  std::vector<IntervalVar*> interval_vars;
  const int num_breaks = 5;
  for (int i = 0; i < num_breaks; ++i) {
    IntervalVar* interval =
        solver->MakeIntervalVar(begin, end, 1, 2, begin, end, false, "");
    interval_vars.push_back(interval);
  }
  dimension->SetBreakIntervalsOfVehicle(interval_vars, 0, {0, 0});
  dimension->SetBreakDistanceDurationOfVehicle(3, 2, 0);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  model.CloseModelWithParameters(parameters);

  // Let us count the number of solutions to this problem.
  // There are 6 interbreaks constrained to have duration in [0,3],
  // and 5 breaks of duration 2, all in [0, 24). So the total interbreak is 14.
  // The number of solutions is the number of assignment of interbreak durations
  // so that the sum of the 6 values in [0, 3] is 14.
  // In a solution, all values are 3, except:
  // - one is 0 and one is 2 (6 * 5 = 30 cases)
  // - two are 2 (6 choose 2 = 15 cases)
  // - four are 1 (6 choose 4 = 15 cases)
  // - two are 2, one is 1 (6 * 5 choose 2 = 6 * 10 = 60)
  // The total number of scenarios is 120.
  EXPECT_EQ(EnumerateAllSolutions(solver, {}, interval_vars, 100000), 120);
}

TEST_F(EnumerationTest, BreaksDistanceEmptyBreaks) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model model(manager);

  const int transit = model.RegisterTransitCallback(
      absl::bind_front(ValueIfDifferent, manager, 1));
  model.AddDimension(transit, kint64max, kint64max, false, "typicallytime");
  Dimension* dimension = model.GetMutableDimension("typicallytime");
  IntVar* start_var = dimension->CumulVar(model.Start(0));
  IntVar* end_var = dimension->CumulVar(model.End(0));
  const int64_t begin = 0;
  const int64_t end = 24;
  start_var->SetRange(begin, end);
  end_var->SetRange(begin, end);

  const int64_t max_distance = 3;
  dimension->SetBreakDistanceDurationOfVehicle(max_distance, 2, 0);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  model.CloseModelWithParameters(parameters);

  const Assignment* const solution = model.Solve();
  ASSERT_NE(solution, nullptr);

  // Start and end cumuls should be bound and respect max distance.
  EXPECT_TRUE(solution->Bound(start_var));
  EXPECT_TRUE(solution->Bound(end_var));
  EXPECT_LE(solution->Bound(end_var) - solution->Bound(start_var),
            max_distance);
}

TEST_F(EnumerationTest, BreaksDistanceEmptyBreaksInfeasible) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model model(manager);

  const int transit = model.RegisterTransitCallback(
      absl::bind_front(ValueIfDifferent, manager, 1));
  model.AddDimension(transit, kint64max, kint64max, false, "typicallytime");
  Dimension* dimension = model.GetMutableDimension("typicallytime");
  IntVar* start_var = dimension->CumulVar(model.Start(0));
  IntVar* end_var = dimension->CumulVar(model.End(0));
  const int64_t begin = 0;
  const int64_t end = 24;
  start_var->SetRange(begin, end);
  end_var->SetRange(begin, end);

  const int64_t max_distance = 0;
  dimension->SetBreakDistanceDurationOfVehicle(max_distance, 2, 0);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  model.CloseModelWithParameters(parameters);

  const Assignment* const solution = model.Solve();
  // Infeasible since transit is 1 and break distance constrains it to 0.
  ASSERT_EQ(solution, nullptr);
}

class CountOperator : public LocalSearchOperator {
 public:
  CountOperator() : count_(0) {}
  ~CountOperator() override = default;
  bool MakeNextNeighbor(Assignment* /*delta*/,
                        Assignment* /*deltadelta*/) override {
    if (count_ == 0) {
      ++count_;
    }
    return false;
  }
  void Start(const Assignment* /*assignment*/) override {}
  int count() const { return count_; }

 private:
  int count_;
};

int64_t ModuloCost(const IndexManager& manager, int64_t from, int64_t to) {
  return manager.IndexToNode(from).value() % 2 !=
         manager.IndexToNode(to).value() % 2;
}

class ExtraOperatorTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<::testing::tuple<bool, bool, bool>> {
 protected:
  ExtraOperatorTest()
      : BaseRoutingTest(
            /*use_unary_callback*/ false,
            /*use_light_propagation=*/::testing::get<0>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<1>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<2>(GetParam())) {
  }
  ~ExtraOperatorTest() override = default;
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    manager_ = std::make_unique<IndexManager>(5, 1, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    const int vehicle_cost =
        model->RegisterTransitCallback(absl::bind_front(ModuloCost, *manager_));
    model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    op_ = std::make_unique<CountOperator>();
    EXPECT_EQ(op_->count(), 0);
    model->AddLocalSearchOperator(op_.get());
    return model;
  }
  int VehicleCostCount() const override { return 1; }

  std::unique_ptr<CountOperator> op_;
};

TEST_P(ExtraOperatorTest, ExtraOperator) {
  SetUpModelAndSolve();
  EXPECT_EQ(op_->count(), 1);
}

INSTANTIATE_TEST_SUITE_P(WithParameters, ExtraOperatorTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

class TestSearchMonitor : public SearchMonitor {
 public:
  explicit TestSearchMonitor(Solver* solver)
      : SearchMonitor(solver), called_(false) {}
  bool AtSolution() override {
    called_ = true;
    return false;
  }
  bool called() const { return called_; }
  void Clear() { called_ = false; }

 private:
  bool called_;
};

TestSearchMonitor* CreateTestSearchMonitor(Model* model) {
  return model->solver()->RevAlloc(new TestSearchMonitor(model->solver()));
}

void CheckMonitorsAndClear(std::vector<TestSearchMonitor*> monitors) {
  for (TestSearchMonitor* monitor : monitors) {
    EXPECT_TRUE(monitor->called());
    monitor->Clear();
  }
}

TEST_P(RoutingTest, AddSearchMonitor) {
  SetUpModel();
  TestSearchMonitor* test_monitor_before_close =
      CreateTestSearchMonitor(model_.get());
  model_->AddSearchMonitor(test_monitor_before_close);
  model_->CloseModel();
  TestSearchMonitor* test_monitor_after_close =
      CreateTestSearchMonitor(model_.get());
  model_->AddSearchMonitor(test_monitor_after_close);
  Solve();
  CheckMonitorsAndClear({test_monitor_before_close, test_monitor_after_close});
  Solve();
  CheckMonitorsAndClear({test_monitor_before_close, test_monitor_after_close});
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(FirstSolutionStrategy::SAVINGS);
  ASSERT_THAT(parameters,
              ::testing::Not(::testing::EquivToProto(GetSearchParameters())));
  model_->SolveFromAssignmentWithParameters(/*assignment=*/nullptr, parameters);
  CheckMonitorsAndClear({test_monitor_before_close, test_monitor_after_close});
}

TEST_P(RoutingTest, AddAtSolutionCallback) {
  SetUpModel();
  bool called = false;
  model_->AddAtSolutionCallback([&called]() { called = true; });
  Solve();
  EXPECT_TRUE(called);
}

TEST_P(RoutingTest, EnterSearchCallback) {
  SetUpModel();
  int calls = 0;
  model_->AddEnterSearchCallback([&calls]() { ++calls; });
  Solve();
  EXPECT_EQ(calls, 1);
}

class RoutingMultiRunTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<::testing::tuple<bool, bool, bool>> {
 protected:
  RoutingMultiRunTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/false,
            /*use_light_propagation=*/::testing::get<0>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<1>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<2>(GetParam())) {
  }
  ~RoutingMultiRunTest() override = default;
  void SetUp() override { absl::SetFlag(&FLAGS_cp_name_cast_variables, true); }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    manager_ = std::make_unique<IndexManager>(30, 10, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    const int vehicle_cost = model->RegisterTransitCallback(
        [](int64_t i, int64_t j) { return std::abs(i - j); });
    model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    return model;
  }
  int VehicleCostCount() const override { return 1; }
  RoutingSearchParameters GetSearchParameters() override {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    if (use_light_propagation_) {
      parameters.set_first_solution_strategy(
          FirstSolutionStrategy::FIRST_UNBOUND_MIN_VALUE);
    } else {
      parameters.set_first_solution_strategy(
          FirstSolutionStrategy::GLOBAL_CHEAPEST_ARC);
    }
    parameters.set_solution_limit(50);
    return parameters;
  }
};

TEST_P(RoutingMultiRunTest, MultiRunsWithMetaheuristic) {
  SetUpModel();
  Solve();
  const int solution_count = model_->solver()->solutions();
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(FirstSolutionStrategy::SAVINGS);
  parameters.set_local_search_metaheuristic(
      LocalSearchMetaheuristic::GUIDED_LOCAL_SEARCH);
  parameters.set_solution_limit(10 + solution_count);
  model_->SolveFromAssignmentWithParameters(/*assignment=*/nullptr, parameters);
  EXPECT_EQ(model_->solver()->solutions(), 10 + solution_count);
  // Back to defaults
  RoutingSearchParameters defaults = DefaultRoutingSearchParameters();
  model_->SolveFromAssignmentWithParameters(/*assignment=*/nullptr, defaults);
  EXPECT_LT(model_->solver()->solutions(), 10 + solution_count);
}

TEST_P(RoutingMultiRunTest, FastMultiRunsWithMetaheuristic) {
  SetUpModel();
  Solve(FAST);
  const int solution_count = model_->solver()->solutions();
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(FirstSolutionStrategy::SAVINGS);
  parameters.set_local_search_metaheuristic(
      LocalSearchMetaheuristic::GUIDED_LOCAL_SEARCH);
  parameters.set_solution_limit(10 + solution_count);
  model_->SolveFromAssignmentWithParameters(/*assignment=*/nullptr, parameters);
  EXPECT_EQ(model_->solver()->solutions(), 10 + solution_count);
  // Back to defaults
  Solve(FAST);
  EXPECT_EQ(model_->solver()->solutions(), solution_count);
}

TEST_P(RoutingMultiRunTest, ILSMultiRunsWithMetaheuristic) {
  SetUpModel();
  int num_solutions = 0;
  model_->AddAtSolutionCallback([&num_solutions]() { ++num_solutions; });
  Solve(ILS);
  const int solution_count = num_solutions;
  num_solutions = 0;
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.mutable_iterated_local_search_parameters()
      ->set_improve_perturbed_solution(false);
  IteratedLocalSearchParameters* ils =
      parameters.mutable_iterated_local_search_parameters();
  // NOTE: As of 07/2024, we no longer add any default ruin strategies to the
  // default RuinRecreateParameters, because since ruin_strategies is a repeated
  // field, it will only be appended to when merging with a proto containing
  // this field.
  ils->mutable_ruin_recreate_parameters()
      ->add_ruin_strategies()
      ->mutable_spatially_close_routes()
      ->set_num_ruined_routes(2);
  // NOTE: As of 12/2025, we no longer add any default acceptance policies to
  // the default IteratedLocalSearchParameters. Since strategies is a
  // repeated field, it will only be appended to when merging with a proto
  // containing this field.
  // Acceptance policies can be added as follows.
  ils->mutable_best_solution_acceptance_policy()
      ->add_strategies()
      ->mutable_greedy_descent();
  SimulatedAnnealingAcceptanceStrategy* sa =
      ils->mutable_reference_solution_acceptance_policy()
          ->add_strategies()
          ->mutable_simulated_annealing();
  sa->set_cooling_schedule_strategy(CoolingScheduleStrategy::EXPONENTIAL);
  sa->set_initial_temperature(100.0);
  sa->set_final_temperature(0.01);
  sa->set_automatic_temperatures(false);
  parameters.set_solution_limit(10 + solution_count);
  model_->SolveWithIteratedLocalSearch(parameters);
  EXPECT_EQ(num_solutions, 10 + solution_count);
  // Back to defaults
  num_solutions = 0;
  Solve(ILS);
  EXPECT_EQ(num_solutions, solution_count);
}

INSTANTIATE_TEST_SUITE_P(WithParameters, RoutingMultiRunTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

TEST(ApplyLocksTest, ApplyLocks) {
  const int kUnactive = 4;
  const int locks[] = {2, 4, 1};
  const int expected_lock_next[] = {1, 4};
  const int size = std::size(locks);
  IndexManager manager(5, 1, NodeIndex(0));
  Model model(manager);
  model.ActiveVar(kUnactive)->SetValue(0);
  std::vector<int64_t> lock_vector(locks, locks + size);
  model.ApplyLocks(lock_vector);
  const Assignment* const preassignment = model.PreAssignment();
  for (int i = 0; i < size - 1; ++i) {
    if (expected_lock_next[i] == locks[i]) {
      EXPECT_FALSE(preassignment->Contains(model.NextVar(locks[i])));
    } else {
      EXPECT_EQ(preassignment->Value(model.NextVar(locks[i])),
                expected_lock_next[i]);
    }
  }
}

TEST(FirstSolutionStrategyTest, InvalidStrategy) {
  if (!DEBUG_MODE) {
    static const FirstSolutionStrategy::Value kStrategies[] = {
        FirstSolutionStrategy::SWEEP,  // missing arranger
        static_cast<FirstSolutionStrategy::Value>(
            FirstSolutionStrategy::Value_MAX + 1)};
    for (int i = 0; i < std::size(kStrategies); ++i) {
      IndexManager manager(5, 1, NodeIndex(0));
      Model model(manager);
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      parameters.set_first_solution_strategy(kStrategies[i]);
      model.CloseModelWithParameters(parameters);
      EXPECT_EQ(RoutingSearchStatus::ROUTING_INVALID, model.status());
      EXPECT_EQ(nullptr, model.SolveWithParameters(parameters));
      EXPECT_EQ(RoutingSearchStatus::ROUTING_INVALID, model.status());
    }
  }
}

TEST(FirstSolutionStrategyTest, InvalidThenValidStrategy) {
  if (!DEBUG_MODE) {
    static const FirstSolutionStrategy::Value kStrategies[] = {
        FirstSolutionStrategy::SWEEP,  // missing arranger
        static_cast<FirstSolutionStrategy::Value>(
            FirstSolutionStrategy::Value_MAX + 1)};
    for (int i = 0; i < std::size(kStrategies); ++i) {
      IndexManager manager(5, 1, NodeIndex(0));
      Model model(manager);
      RoutingSearchParameters invalid_parameters =
          DefaultRoutingSearchParameters();
      invalid_parameters.set_first_solution_strategy(kStrategies[i]);
      model.CloseModelWithParameters(invalid_parameters);
      EXPECT_EQ(RoutingSearchStatus::ROUTING_INVALID, model.status());
      EXPECT_EQ(nullptr, model.SolveWithParameters(invalid_parameters));
      EXPECT_EQ(RoutingSearchStatus::ROUTING_INVALID, model.status());
      RoutingSearchParameters valid_parameters =
          DefaultRoutingSearchParameters();
      valid_parameters.set_log_search(true);
      EXPECT_NE(nullptr, model.SolveWithParameters(valid_parameters));
      EXPECT_NE(RoutingSearchStatus::ROUTING_INVALID, model.status());
    }
  }
}

TEST(FirstSolutionStrategyTest, ValidThenInvalidStrategy) {
  if (!DEBUG_MODE) {
    static const FirstSolutionStrategy::Value kStrategies[] = {
        FirstSolutionStrategy::SWEEP,  // missing arranger
        static_cast<FirstSolutionStrategy::Value>(
            FirstSolutionStrategy::Value_MAX + 1)};
    for (int i = 0; i < std::size(kStrategies); ++i) {
      IndexManager manager(5, 1, NodeIndex(0));
      Model model(manager);
      RoutingSearchParameters valid_parameters =
          DefaultRoutingSearchParameters();
      model.CloseModelWithParameters(valid_parameters);
      EXPECT_NE(RoutingSearchStatus::ROUTING_INVALID, model.status());
      RoutingSearchParameters invalid_parameters =
          DefaultRoutingSearchParameters();
      invalid_parameters.set_first_solution_strategy(kStrategies[i]);
      EXPECT_EQ(nullptr, model.SolveWithParameters(invalid_parameters));
      EXPECT_EQ(RoutingSearchStatus::ROUTING_INVALID, model.status());
    }
  }
}

TEST(FirstSolutionStrategyTest, FirstSolutionStrategyFromParameters) {
  for (auto i : EnumerateEnumValues<FirstSolutionStrategy::Value>()) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    const std::string& fs_name = FirstSolutionStrategy_Value_Name(i);
    FirstSolutionStrategy::Value fs;
    ASSERT_TRUE(FirstSolutionStrategy_Value_Parse(fs_name, &fs));
    parameters.set_first_solution_strategy(fs);
    SCOPED_TRACE(fs);
    IndexManager manager(5, 1, NodeIndex(0));
    Model model(manager);
    model.SetFirstSolutionEvaluator([](int64_t i, int64_t) { return i; });
    std::vector<std::pair<int64_t, int64_t>> points(manager.num_indices());
    model.SetSweepArranger(new SweepArranger(points));
    model.SolveWithParameters(parameters);
  }
}

TEST(FirstSolutionStrategyTest, EvaluatorStrategy) {
  IndexManager manager(5, 1, NodeIndex(0));
  Model model(manager);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::EVALUATOR_STRATEGY);
  model.CloseModelWithParameters(parameters);
  model.SetFirstSolutionEvaluator(
      [](int64_t i, int64_t j) { return i * (j + 42); });
  ASSERT_NE(model.first_solution_evaluator(), nullptr);
  EXPECT_EQ(model.first_solution_evaluator()(3, 5), 3 * (5 + 42));
}

int64_t TestCallback(int nodes, NodeIndex depot, NodeIndex i, NodeIndex j) {
  const NodeIndex ii = (i < nodes) ? i : depot;
  const NodeIndex jj = (j < nodes) ? j : depot;
  return ((ii < jj) ? jj - ii : ii - jj).value();
}

TEST(CachedCallbackTest, CachedCost) {
  const int kNodes = 5;
  const int kRoutes = 1;
  const NodeIndex kDepot(0);
  RoutingModelParameters parameters = DefaultRoutingModelParameters();
  parameters.set_max_callback_cache_size(kNodes * 2);
  IndexManager manager(kNodes, kRoutes, kDepot);
  Model model(manager, parameters);
  const int cost =
      model.RegisterTransitCallback([&manager, kDepot](int64_t i, int64_t j) {
        return TestCallback(kNodes, kDepot, manager.IndexToNode(i),
                            manager.IndexToNode(j));
      });
  model.SetArcCostEvaluatorOfAllVehicles(cost);
  model.CloseModel();
  EXPECT_EQ(1, model.GetNonZeroCostClassesCount());
  const int rows = model.Size();
  const int columns = model.Size() + model.vehicles();
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      NodeIndex node_i = manager.IndexToNode(i);
      NodeIndex node_j = manager.IndexToNode(j);
      EXPECT_EQ(TestCallback(kNodes, kDepot, node_i, node_j),
                model.GetHomogeneousCost(i, j))
          << i << " " << j;
    }
  }
}

int64_t CountingTestCallback(
    const IndexManager& manager,
    std::map<std::pair<int64_t, int64_t>, int>* num_calls, int64_t i,
    int64_t j) {
  CHECK(num_calls != nullptr);
  ++(*num_calls)[std::make_pair(i, j)];
  return manager.IndexToNode(i) == manager.IndexToNode(j) ? 0 : 1;
}

TEST(CachedCallbackTest, CachedCallbackReuse) {
  static const char* const kDimensionNames[] = {"Dimension1", "Dimension2",
                                                "Dimension3"};
  const int kNodes = 5;
  const int kRoutes = 1;
  const NodeIndex kDepot(0);
  RoutingModelParameters parameters = DefaultRoutingModelParameters();
  parameters.set_max_callback_cache_size(kNodes * 2);
  IndexManager manager(kNodes, kRoutes, kDepot);
  Model model(manager, parameters);
  std::map<std::pair<int64_t, int64_t>, int> num_callback_calls;
  const int cost = model.RegisterTransitCallback(
      [&num_callback_calls, &manager](int64_t i, int64_t j) {
        return CountingTestCallback(manager, &num_callback_calls, i, j);
      });
  model.SetArcCostEvaluatorOfAllVehicles(cost);
  for (int dimension = 0; dimension < std::size(kDimensionNames); ++dimension) {
    model.AddDimension(cost, kint64max, kint64max, true,
                       kDimensionNames[dimension]);
  }
  RoutingSearchParameters search_parameters = DefaultRoutingSearchParameters();
  search_parameters.set_first_solution_strategy(
      FirstSolutionStrategy::FIRST_UNBOUND_MIN_VALUE);
  search_parameters.mutable_local_search_operators()->set_use_path_lns(
      BOOL_TRUE);
  search_parameters.mutable_local_search_operators()->set_use_inactive_lns(
      BOOL_TRUE);
  model.CloseModelWithParameters(search_parameters);
  model.SolveWithParameters(search_parameters);
  // Verify that the callback was called once for each index pair. In this test,
  // the callback is actually called for every pair of indices, because we run
  // the solver with full propagation and in that case it needs to see the
  // values of the callback to determine the bounds of all variables to run the
  // propagation correctly.
  // If we used the light propagation, the solver might call the callback only
  // for node pairs it actually considers during the search.
  const int num_indices = manager.num_indices();
  EXPECT_EQ(num_indices * num_indices, num_callback_calls.size());
  for (int i = 0; i < num_indices; ++i) {
    for (int j = 0; j < num_indices; ++j) {
      int num_calls_for_pair = 0;
      EXPECT_TRUE(FindCopy(num_callback_calls, std::make_pair(i, j),
                           num_calls_for_pair));
      EXPECT_EQ(num_calls_for_pair, 1);
    }
  }
}

using LowerBoundTest =
    ::testing::TestWithParam<::testing::tuple<bool, bool, bool>>;
TEST_P(LowerBoundTest, LowerBoundCases) {
  const int kNodes = 10;
  const int kVehicles = 2;
  const int64_t kNoLowerBound = 0;
  const auto [use_unary_callback, use_light_propagation,
              force_non_homogeneous_costs] = GetParam();
  RoutingModelParameters parameters = DefaultRoutingModelParameters();
  if (force_non_homogeneous_costs) {
    parameters.set_reduce_vehicle_cost_model(false);
  }
  const int64_t kExpectedLowerBound =
      force_non_homogeneous_costs ? 0 : kNodes - 1;
  RoutingSearchParameters search_parameters = DefaultRoutingSearchParameters();
  search_parameters.set_use_full_propagation(!use_light_propagation);

  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager, parameters);
  const int vehicle_cost =
      use_unary_callback
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1; });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  EXPECT_EQ(kNoLowerBound, model.ComputeLowerBound());
  model.CloseModelWithParameters(search_parameters);
  EXPECT_EQ(kExpectedLowerBound, model.ComputeLowerBound());
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(1)}));
  EXPECT_EQ(kNoLowerBound, model.ComputeLowerBound());
}

INSTANTIATE_TEST_SUITE_P(WithParameters, LowerBoundTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

// Solves the vehicle routing problem on arcs of a complete graph (modeled as a
// node routing problem) with multiple vehicles and disjunctions, where each
// arc is in disjunction with the arc in the opposite direction.
class MultipleVehiclesAndDisjunctionsTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<::testing::tuple<bool, bool, bool>> {
 public:
  MultipleVehiclesAndDisjunctionsTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/false,
            /*use_light_propagation=*/::testing::get<0>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<1>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<2>(GetParam())) {
  }
  int64_t Distance(const IndexManager& manager, int64_t i, int64_t j) {
    const int node_i = arcs_[manager.IndexToNode(i).value()].second;
    const int node_j = arcs_[manager.IndexToNode(j).value()].first;
    return shortest_paths_[node_i][node_j];
  }

 protected:
  static const int kNumNodes;
  static const int kNumVehicles;
  static const int kArcLength;
  static const int kVehicleFixedCost;
  static const int kExpectedInactiveArcs;
  static const NodeIndex kDepots[];

  Model* CreateModel(const RoutingModelParameters& parameters) override {
    util::ListGraph<int, int> graph(kNumNodes, kNumNodes * (kNumNodes - 1));

    std::vector<int> arc_lengths;
    for (int i = 0; i < kNumNodes - 1; ++i) {
      for (int j = i + 1; j < kNumNodes; ++j) {
        graph.AddArc(i, j);
        graph.AddArc(j, i);
        arcs_.push_back({i, j});
        arcs_.push_back({j, i});
        arc_lengths.push_back(kArcLength);
        arc_lengths.push_back(kArcLength);
      }
    }
    shortest_paths_.resize(kNumNodes);
    std::fill_n(shortest_paths_.begin(), kNumNodes,
                std::vector<int>(kNumNodes));
    for (int source = 0; source < kNumNodes; ++source) {
      BoundedDijkstraWrapper<util::ListGraph<int, int>, int> dijkstra(
          &graph, &arc_lengths);
      dijkstra.RunBoundedDijkstra(source, /*distance_limit=*/kint32max);
      shortest_paths_[source] = dijkstra.distances();
    }

    std::vector<NodeIndex> depots(kDepots, kDepots + kNumVehicles);
    manager_ = std::make_unique<IndexManager>(graph.num_arcs(), kNumVehicles,
                                              depots, depots);
    Model* const model = new Model(*manager_, parameters);
    model->SetFixedCostOfAllVehicles(kVehicleFixedCost);
    const int vehicle_cost = model->RegisterTransitCallback(absl::bind_front(
        &MultipleVehiclesAndDisjunctionsTest::Distance, this, *manager_));
    model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    for (int i = 0; i < graph.num_arcs() / 2; ++i) {
      std::vector<int64_t> disjunctions;
      disjunctions.push_back(manager_->NodeToIndex(NodeIndex(i * 2)));
      disjunctions.push_back(manager_->NodeToIndex(NodeIndex(i * 2 + 1)));
      model->AddDisjunction(disjunctions);
    }

    return model;
  }
  int VehicleCostCount() const override { return 1; }

  std::vector<std::pair<int, int>> arcs_;
  std::vector<std::vector<int>> shortest_paths_;
};

const int MultipleVehiclesAndDisjunctionsTest::kNumNodes = 7;
const int MultipleVehiclesAndDisjunctionsTest::kNumVehicles = 2;
const int MultipleVehiclesAndDisjunctionsTest::kArcLength = 100;
const int MultipleVehiclesAndDisjunctionsTest::kVehicleFixedCost = 0;
const int MultipleVehiclesAndDisjunctionsTest::kExpectedInactiveArcs = 21;
const NodeIndex MultipleVehiclesAndDisjunctionsTest::kDepots[] = {NodeIndex(0),
                                                                  NodeIndex(2)};

TEST_P(MultipleVehiclesAndDisjunctionsTest, Solve) {
  const Assignment* assignment = SetUpModelAndSolve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(2, model_->GetVehicleClassesCount());

  int inactive_arcs = 0;
  for (int i = 0; i < model_->Size(); ++i) {
    IntVar* const active_var = model_->ActiveVar(i);
    IntVar* const vehicle_var = model_->VehicleVar(i);

    bool is_active = assignment->Value(active_var);
    int64_t vehicle_id = assignment->Value(vehicle_var);

    EXPECT_TRUE(is_active || vehicle_id == -1);
    if (!is_active) {
      ++inactive_arcs;
    }
  }

  EXPECT_EQ(kExpectedInactiveArcs, inactive_arcs);
}

INSTANTIATE_TEST_SUITE_P(WithParameters, MultipleVehiclesAndDisjunctionsTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

class RoutesToAssignmentTest : public MultipleVehiclesAndDisjunctionsTest {};

TEST_P(RoutesToAssignmentTest, ReadWriteAssignment) {
  SetUpModel();

  // Create the routes. The non-start/end nodes are assigned to veh
  std::vector<std::vector<int64_t>> routes(kNumVehicles);
  absl::flat_hash_set<int> active_indices;
  int current_vehicle = 0;
  for (NodeIndex node(0); node < manager_->num_nodes(); node += 2) {
    const int node_index = manager_->NodeToIndex(node);
    if (node_index < 0 || model_->IsStart(node_index) ||
        model_->IsEnd(node_index)) {
      // The node is a start/end node for some vehicle, do not assign it.
      continue;
    }
    routes[current_vehicle].push_back(manager_->NodeToIndex(node));
    active_indices.insert(node_index);
    current_vehicle = (current_vehicle + 1) % kNumVehicles;
  }

  const Assignment* loaded_assignment =
      model_->ReadAssignmentFromRoutes(routes, false);
  ASSERT_NE(loaded_assignment, nullptr);

  const int kExpectedCost = 2000;
  EXPECT_EQ(kExpectedCost, loaded_assignment->ObjectiveValue());

  for (int index = 0; index < model_->Size(); ++index) {
    IntVar* const next_var = model_->NextVar(index);
    EXPECT_TRUE(loaded_assignment->Contains(next_var));
    EXPECT_TRUE(loaded_assignment->Bound(next_var));

    IntVar* const active_var = model_->ActiveVar(index);
    EXPECT_TRUE(loaded_assignment->Contains(active_var));
    EXPECT_TRUE(loaded_assignment->Bound(active_var));
    const bool expected_active = model_->IsStart(index) ||
                                 model_->IsEnd(index) ||
                                 active_indices.contains(index);
    EXPECT_EQ(expected_active, loaded_assignment->Value(active_var));

    IntVar* const vehicle_var = model_->VehicleVar(index);
    EXPECT_TRUE(loaded_assignment->Contains(vehicle_var));
    EXPECT_TRUE(loaded_assignment->Bound(vehicle_var));
    if (!expected_active) {
      EXPECT_EQ(-1, loaded_assignment->Value(vehicle_var));
      const int next_index = loaded_assignment->Value(next_var);
      EXPECT_EQ(index, next_index);
    }
  }

  for (int vehicle = 0; vehicle < kNumVehicles; ++vehicle) {
    const int size = routes[vehicle].size();
    int current_index = model_->Start(vehicle);
    for (int i = 0; i < size; ++i) {
      const int64_t expected_next_index = routes[vehicle][i];
      IntVar* const next_var = model_->NextVar(current_index);
      IntVar* const vehicle_var = model_->VehicleVar(current_index);
      const int64_t vehicle_var_value = loaded_assignment->Value(vehicle_var);
      const int64_t next_index = loaded_assignment->Value(next_var);
      EXPECT_EQ(expected_next_index, next_index);
      EXPECT_EQ(vehicle, vehicle_var_value);
      current_index = next_index;
    }
  }

  std::vector<std::vector<int64_t>> extracted_routes;
  model_->AssignmentToRoutes(*loaded_assignment, &extracted_routes);

  EXPECT_EQ(routes.size(), extracted_routes.size());
  for (int vehicle = 0; vehicle < routes.size(); ++vehicle) {
    const std::vector<int64_t>& route = routes[vehicle];
    const std::vector<int64_t>& extracted_route = extracted_routes[vehicle];
    EXPECT_EQ(route.size(), extracted_route.size());
    for (int i = 0; i < route.size(); ++i) {
      EXPECT_EQ(route[i], extracted_route[i]);
    }
  }
}

TEST_P(RoutesToAssignmentTest, GetRoutesFromAssignment) {
  // Compare routes returned by GetRoutesFromAssignment
  // with routes returned by AssignmentToRoutes.
  const Assignment* assignment = SetUpModelAndSolve();
  ASSERT_NE(assignment, nullptr);
  std::vector<std::vector<int64_t>> expected_routes;
  model_->AssignmentToRoutes(*assignment, &expected_routes);
  std::vector<std::vector<int64_t>> routes =
      model_->GetRoutesFromAssignment(*assignment);
  for (int vehicle = 0; vehicle < routes.size(); ++vehicle) {
    const std::vector<int64_t>& route = routes[vehicle];
    const std::vector<int64_t>& expected_route = expected_routes[vehicle];
    int64_t index = model_->Start(vehicle);
    EXPECT_EQ(route[0], index);
    for (int i = 0; i < expected_route.size(); ++i) {
      EXPECT_EQ(route[i + 1], expected_route[i]);
    }
    EXPECT_EQ(route.back(), model_->End(vehicle));
  }
}

TEST_P(RoutesToAssignmentTest, WriteReadAssignment) {
  const Assignment* const first_solution = SetUpModelAndSolve();
  ASSERT_NE(first_solution, nullptr);
  AssignmentProto first_solution_proto;
  first_solution->Save(&first_solution_proto);

  std::vector<std::vector<int64_t>> routes;
  model_->AssignmentToRoutes(*first_solution, &routes);

  // Reset the model and restore the solution from routes.
  SetUpModel();
  Assignment* const restored_solution =
      model_->ReadAssignmentFromRoutes(routes, false);
  ASSERT_NE(restored_solution, nullptr);
  EXPECT_EQ(RoutingSearchStatus::ROUTING_SUCCESS, model_->status());
  AssignmentProto restored_solution_proto;
  restored_solution->Save(&restored_solution_proto);

  EXPECT_THAT(first_solution_proto, EqualsProto(restored_solution_proto));

  // Restore the solution using RestoreAssignment.
  Assignment* const restored_to_model =
      model_->RestoreAssignment(*restored_solution);
  ASSERT_NE(restored_to_model, nullptr);
  EXPECT_EQ(RoutingSearchStatus::ROUTING_SUCCESS, model_->status());
  AssignmentProto restored_to_model_proto;
  restored_to_model->Save(&restored_to_model_proto);

  EXPECT_THAT(first_solution_proto, EqualsProto(restored_to_model_proto));
}

TEST_P(RoutesToAssignmentTest, ReadEmptyRoutes) {
  SetUpModel();
  model_->CloseModel();

  Assignment assignment(model_->solver());
  std::vector<std::vector<int64_t>> routes;

  EXPECT_TRUE(model_->RoutesToAssignment(routes, false, true, &assignment));
  for (int vehicle = 0; vehicle < kNumVehicles; ++vehicle) {
    IntVar* const start_next_var = model_->NextVar(model_->Start(vehicle));
    EXPECT_TRUE(assignment.Contains(start_next_var));
    EXPECT_EQ(model_->End(vehicle), assignment.Value(start_next_var));
  }
}

void AddIndicesToRoute(const int64_t* const indices, int num_indices,
                       std::vector<int64_t>* const route) {
  CHECK(route != nullptr);
  route->resize(num_indices);
  CHECK(num_indices == 0 || indices != nullptr);
  for (int i = 0; i < num_indices; ++i) {
    route->at(i) = indices[i];
  }
}

TEST_P(RoutesToAssignmentTest, RoutestToAssignmentInvalidRoutes) {
  std::vector<std::vector<int64_t>> routes(2);

  SetUpModel();
  model_->CloseModel();
  Assignment temp(model_->solver());

  // Create two routes that share a node.
  const int64_t shared_route[] = {manager_->NodeToIndex(NodeIndex(5))};
  AddIndicesToRoute(shared_route, std::size(shared_route), &routes[0]);
  AddIndicesToRoute(shared_route, std::size(shared_route), &routes[1]);
  EXPECT_FALSE(model_->RoutesToAssignment(routes, false, false, &temp));

  // Create a route that sends the vehicle through a depot.
  const int64_t route_with_a_depot_in_it[] = {
      manager_->NodeToIndex(NodeIndex(4)), manager_->NodeToIndex(NodeIndex(2)),
      manager_->NodeToIndex(NodeIndex(0)), manager_->NodeToIndex(NodeIndex(8))};
  AddIndicesToRoute(route_with_a_depot_in_it,
                    std::size(route_with_a_depot_in_it), &routes[0]);
  EXPECT_FALSE(model_->RoutesToAssignment(routes, false, false, &temp));

  // Create a route with an invalid index.
  const int64_t route_with_invalid_index[] = {
      manager_->NodeToIndex(NodeIndex(4)), -1};
  AddIndicesToRoute(route_with_invalid_index,
                    std::size(route_with_invalid_index), &routes[0]);
  EXPECT_FALSE(model_->RoutesToAssignment(routes, false, false, &temp));

  // Create a route with an inactive node. Node 6 is marked as inactive.
  const int64_t route_with_inactive_node[] = {
      manager_->NodeToIndex(NodeIndex(4)), manager_->NodeToIndex(NodeIndex(1)),
      manager_->NodeToIndex(NodeIndex(8)),
      manager_->NodeToIndex(NodeIndex(10))};
  model_->ActiveVar(8)->SetValue(0);
  AddIndicesToRoute(route_with_inactive_node,
                    std::size(route_with_inactive_node), &routes[0]);
  EXPECT_FALSE(model_->RoutesToAssignment(routes, false, false, &temp));
  // Check that the same route is OK, when ignoring inactive nodes.
  EXPECT_TRUE(model_->RoutesToAssignment(routes, true, false, &temp));

  // Create a route with that goes over a node, where the vehicle is not
  // allowed. Vehicle 0 is disallowed at node 6.
  const int64_t route_with_disallowed_node[] = {
      manager_->NodeToIndex(NodeIndex(6)),
      manager_->NodeToIndex(NodeIndex(10))};
  model_->VehicleVar(6)->RemoveValue(0);
  AddIndicesToRoute(route_with_disallowed_node,
                    std::size(route_with_disallowed_node), &routes[0]);
  EXPECT_FALSE(model_->RoutesToAssignment(routes, false, false, &temp));

  // Use more routes than vehicles.
  routes.push_back(std::vector<int64_t>());
  AddIndicesToRoute(nullptr, 0, &routes[0]);
  AddIndicesToRoute(nullptr, 0, &routes[1]);
  EXPECT_FALSE(model_->RoutesToAssignment(routes, false, false, &temp));
}

INSTANTIATE_TEST_SUITE_P(WithParameters, RoutesToAssignmentTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

class CompactAssignmentTest : public BaseRoutingTest,
                              public ::testing::WithParamInterface<
                                  ::testing::tuple<bool, bool, bool, bool>> {
 protected:
  static const int kNumNodes;
  static const int kNumVehicles;
  static const int kDisjunctionPenalty;

  CompactAssignmentTest()
      : BaseRoutingTest(
            /*use_unary_callback*/ ::testing::get<0>(GetParam()),
            /*use_light_propagation=*/::testing::get<1>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<2>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<3>(GetParam())) {
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    manager_ =
        std::make_unique<IndexManager>(kNumNodes, kNumVehicles, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    const int vehicle_cost =
        use_unary_callback_
            ? model->RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model->RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    return model;
  }
  int VehicleCostCount() const override { return 1; }
  RoutingSearchParameters GetSearchParameters() override {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.mutable_local_search_operators()->set_use_path_lns(BOOL_TRUE);
    parameters.mutable_local_search_operators()->set_use_inactive_lns(
        BOOL_TRUE);
    parameters.set_first_solution_strategy(FirstSolutionStrategy::AUTOMATIC);
    return parameters;
  }

  // Make all nodes optional.
  void SetUpDisjunctions() {
    for (NodeIndex node(0); node < kNumNodes; ++node) {
      model_->AddDisjunction(manager_->NodesToIndices({node}),
                             kDisjunctionPenalty);
    }
  }
};

const int CompactAssignmentTest::kNumNodes = 30;
const int CompactAssignmentTest::kNumVehicles = 10;
const int CompactAssignmentTest::kDisjunctionPenalty = 1000;

void DisplayPlan(const Model& routing, const Assignment& plan) {
  // Display plan cost.
  std::string plan_output = absl::StrFormat("Cost %d\n", plan.ObjectiveValue());

  // Display dropped orders.
  std::string dropped;
  for (int order = 1; order < routing.nodes(); ++order) {
    if (plan.Value(routing.NextVar(order)) == order) {
      if (dropped.empty()) {
        absl::StrAppendFormat(&dropped, " %d", order);
      } else {
        absl::StrAppendFormat(&dropped, ", %d", order);
      }
    }
  }
  if (!dropped.empty()) {
    plan_output += "Dropped orders:" + dropped + "\n";
  }

  // Display actual output for each vehicle.
  for (int route_number = 0; route_number < routing.vehicles();
       ++route_number) {
    int64_t order = routing.Start(route_number);
    absl::StrAppendFormat(&plan_output, "Route %d: ", route_number);
    if (routing.IsEnd(plan.Value(routing.NextVar(order)))) {
      plan_output += "Empty\n";
    } else {
      while (!routing.IsEnd(order)) {
        absl::StrAppendFormat(&plan_output, "%d -> ", order);
        order = plan.Value(routing.NextVar(order));
      }
      absl::StrAppendFormat(&plan_output, "%d\n", order);
    }
  }
}

void TestCompactAssignment(const Model& model, const Assignment& solution,
                           bool force_non_homogeneous_costs) {
  int num_used_vehicles_solution = 0;
  for (int vehicle = 0; vehicle < model.vehicles(); ++vehicle) {
    if (model.IsVehicleUsed(solution, vehicle)) {
      ++num_used_vehicles_solution;
    }
  }

  std::unique_ptr<Assignment> compact_assignment(
      model.CompactAssignment(solution));
  std::unique_ptr<Assignment> checked_compact_assignment(
      model.CompactAndCheckAssignment(solution));
  if (force_non_homogeneous_costs) {
    EXPECT_EQ(compact_assignment, nullptr);
    EXPECT_EQ(checked_compact_assignment, nullptr);
  } else {
    ASSERT_NE(compact_assignment, nullptr);
    EXPECT_TRUE(model.solver()->CheckAssignment(compact_assignment.get()));
    ASSERT_NE(checked_compact_assignment, nullptr);
    EXPECT_TRUE(
        model.solver()->CheckAssignment(checked_compact_assignment.get()));
    DisplayPlan(model, *compact_assignment);

    int num_used_vehicles_compact = 0;
    bool found_empty_plan = false;
    for (int vehicle = 0; vehicle < model.vehicles(); ++vehicle) {
      if (model.IsVehicleUsed(*compact_assignment, vehicle)) {
        ++num_used_vehicles_compact;
        EXPECT_FALSE(found_empty_plan);
      } else {
        found_empty_plan = true;
      }
    }
    EXPECT_EQ(num_used_vehicles_solution, num_used_vehicles_compact);
  }
}

void TestCompactAssignmentFails(const Model& model,
                                const Assignment& solution) {
  std::unique_ptr<Assignment> compact_assignment(
      model.CompactAssignment(solution));
  EXPECT_EQ(compact_assignment, nullptr);
  std::unique_ptr<Assignment> checked_compact_assignment(
      model.CompactAssignment(solution));
  EXPECT_EQ(checked_compact_assignment, nullptr);
  if (compact_assignment != nullptr) {
    DisplayPlan(model, *compact_assignment);
  }
}

TEST_P(CompactAssignmentTest, CompactAssignment) {
  const int64_t kExpectedCost = 30;
  const Assignment* const solution = SetUpModelAndSolve();
  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(kExpectedCost, solution->ObjectiveValue());

  TestCompactAssignment(*model_, *solution, force_non_homogeneous_costs_);
}

TEST_P(CompactAssignmentTest, CompactAssignmentWithDimension) {
  const int64_t kExpectedCost = 33;
  const int64_t kVisitedNodesDimensionStep = 1;
  const int64_t kMaxVisitedNodes = 10;
  const char kVisitedNodesDimensionName[] = "visited_nodes";
  SetUpModel();
  model_->AddConstantDimension(kVisitedNodesDimensionStep, kMaxVisitedNodes,
                               /*fix_start_cumul_to_zero=*/true,
                               kVisitedNodesDimensionName);
  RoutingSearchParameters parameters = GetSearchParameters();
  parameters.set_use_unfiltered_first_solution_strategy(
      use_non_filtered_first_solution_);
  parameters.set_use_full_propagation(!use_light_propagation_);
  const Assignment* const solution = model_->SolveWithParameters(parameters);

  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(kExpectedCost, solution->ObjectiveValue());

  TestCompactAssignment(*model_, *solution, force_non_homogeneous_costs_);
}

// Adds a "number of visited nodes" dimension with a capacity that only allows
// transitions from the start node to end node (to produce an empty assignment).
TEST_P(CompactAssignmentTest, EmptyAssignment) {
  const int64_t kExpectedCost = 29000;
  const int kVisitedNodesDimensionStep = 1;
  const int64_t kMaxVisitedNodes = 1;
  const char kVisitedNodesDimensionName[] = "visited_nodes";
  SetUpModel();
  SetUpDisjunctions();
  model_->AddConstantDimension(kVisitedNodesDimensionStep, kMaxVisitedNodes,
                               /*fix_start_cumul_to_zero=*/true,
                               kVisitedNodesDimensionName);
  RoutingSearchParameters parameters = GetSearchParameters();
  parameters.set_use_unfiltered_first_solution_strategy(
      use_non_filtered_first_solution_);
  parameters.set_use_full_propagation(!use_light_propagation_);
  const Assignment* const solution = model_->SolveWithParameters(parameters);

  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(kExpectedCost, solution->ObjectiveValue());

  for (int vehicle = 0; vehicle < kNumVehicles; ++vehicle) {
    EXPECT_FALSE(model_->IsVehicleUsed(*solution, vehicle));
  }

  TestCompactAssignment(*model_, *solution, force_non_homogeneous_costs_);
}

TEST_P(CompactAssignmentTest, IncompactableAssignment) {
  const int64_t kExpectedCost = 31;
  SetUpModel();
  // Force vehicles 7 and 9 to be used (in the optimal plan, these will be the
  // only vehicles used, creating a gap that cannot be compacted).
  model_->VehicleVar(1)->SetValue(9);
  model_->VehicleVar(2)->SetValue(7);
  RoutingSearchParameters parameters = GetSearchParameters();
  parameters.set_use_unfiltered_first_solution_strategy(
      use_non_filtered_first_solution_);
  parameters.set_use_full_propagation(!use_light_propagation_);
  const Assignment* const solution = model_->SolveWithParameters(parameters);

  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(kExpectedCost, solution->ObjectiveValue());

  TestCompactAssignmentFails(*model_, *solution);
}

TEST_P(CompactAssignmentTest, AssignmentWithVehicleConstraints) {
  const int64_t kExpectedCost = 31;

  // Create a pre-assignment that uses vehicle 1 and 9.
  SetUpModel();
  std::vector<std::vector<int64_t>> routes(kNumVehicles);
  routes[1].push_back(manager_->NodeToIndex(NodeIndex(1)));
  routes[9].push_back(manager_->NodeToIndex(NodeIndex(9)));
  RoutingSearchParameters parameters = GetSearchParameters();
  parameters.set_use_unfiltered_first_solution_strategy(
      use_non_filtered_first_solution_);
  parameters.set_use_full_propagation(!use_light_propagation_);
  model_->CloseModelWithParameters(parameters);
  EXPECT_TRUE(model_->ApplyLocksToAllVehicles(routes, false));
  // Do not allow vehicle 0 at node 9. This effectively disables swapping
  // vehicles 0 and 9.
  IntVar* const vehicle_var_9 =
      model_->VehicleVar(manager_->NodeToIndex(NodeIndex(9)));
  vehicle_var_9->RemoveValue(0);

  const Assignment* const solution = model_->SolveWithParameters(parameters);
  DisplayPlan(*model_, *solution);

  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(kExpectedCost, solution->ObjectiveValue());

  TestCompactAssignment(*model_, *solution, force_non_homogeneous_costs_);
}

INSTANTIATE_TEST_SUITE_P(WithParameters, CompactAssignmentTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

class CompactAssignmentWithMultipleDepotsTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<bool, bool, bool, bool>> {
 protected:
  static const int kNumNodes;
  static const int kNumVehicles;

  static const NodeIndex kStartNodes[];
  static const NodeIndex kEndNodes[];

  CompactAssignmentWithMultipleDepotsTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/::testing::get<0>(GetParam()),
            /*use_light_propagation=*/::testing::get<1>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<2>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<3>(GetParam())) {
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    std::vector<NodeIndex> starts(kStartNodes, kStartNodes + kNumVehicles);
    std::vector<NodeIndex> ends(kEndNodes, kEndNodes + kNumVehicles);
    manager_ =
        std::make_unique<IndexManager>(kNumNodes, kNumVehicles, starts, ends);
    Model* const model = new Model(*manager_, parameters);
    const int vehicle_cost =
        use_unary_callback_
            ? model->RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model->RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    return model;
  }
  int VehicleCostCount() const override { return 1; }
  RoutingSearchParameters GetSearchParameters() override {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.mutable_local_search_operators()->set_use_path_lns(BOOL_TRUE);
    parameters.mutable_local_search_operators()->set_use_inactive_lns(
        BOOL_TRUE);
    parameters.set_first_solution_strategy(
        FirstSolutionStrategy::PATH_CHEAPEST_ARC);
    return parameters;
  }
};

const int CompactAssignmentWithMultipleDepotsTest::kNumNodes = 30;
const int CompactAssignmentWithMultipleDepotsTest::kNumVehicles = 10;

const NodeIndex CompactAssignmentWithMultipleDepotsTest::kStartNodes[] = {
    NodeIndex(0), NodeIndex(0), NodeIndex(0), NodeIndex(0), NodeIndex(1),
    NodeIndex(1), NodeIndex(1), NodeIndex(1), NodeIndex(0), NodeIndex(1)};

const NodeIndex CompactAssignmentWithMultipleDepotsTest::kEndNodes[] = {
    NodeIndex(0), NodeIndex(2), NodeIndex(0), NodeIndex(2), NodeIndex(1),
    NodeIndex(2), NodeIndex(1), NodeIndex(1), NodeIndex(0), NodeIndex(1)};

TEST_P(CompactAssignmentWithMultipleDepotsTest, CompactibleAssignment) {
  const int64_t kExpectedCost = 29;

  SetUpModel();
  std::vector<std::vector<int64_t>> routes(kNumVehicles);
  routes[2].push_back(manager_->NodeToIndex(NodeIndex(10)));
  routes[3].push_back(manager_->NodeToIndex(NodeIndex(11)));

  RoutingSearchParameters parameters = GetSearchParameters();
  parameters.set_use_unfiltered_first_solution_strategy(
      use_non_filtered_first_solution_);
  parameters.set_use_unfiltered_first_solution_strategy(false);
  parameters.set_use_full_propagation(!use_light_propagation_);
  model_->CloseModelWithParameters(parameters);
  EXPECT_TRUE(model_->ApplyLocksToAllVehicles(routes, false));

  const Assignment* const solution = model_->SolveWithParameters(parameters);
  DisplayPlan(*model_, *solution);
  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(kExpectedCost, solution->ObjectiveValue());

  TestCompactAssignment(*model_, *solution, force_non_homogeneous_costs_);
}

TEST_P(CompactAssignmentWithMultipleDepotsTest, AlreadyCompactAssignment) {
  const int64_t kExpectedCost = 29;

  SetUpModel();
  std::vector<std::vector<int64_t>> routes(kNumVehicles);
  routes[0].push_back(manager_->NodeToIndex(NodeIndex(10)));
  routes[1].push_back(manager_->NodeToIndex(NodeIndex(11)));

  RoutingSearchParameters parameters = GetSearchParameters();
  parameters.set_use_unfiltered_first_solution_strategy(
      use_non_filtered_first_solution_);
  parameters.set_use_full_propagation(!use_light_propagation_);
  model_->CloseModelWithParameters(parameters);
  EXPECT_TRUE(model_->ApplyLocksToAllVehicles(routes, false));

  const Assignment* const solution = model_->SolveWithParameters(parameters);
  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(kExpectedCost, solution->ObjectiveValue());

  TestCompactAssignment(*model_, *solution, force_non_homogeneous_costs_);
}

TEST_P(CompactAssignmentWithMultipleDepotsTest, IncompactibleAssignments) {
  const int64_t kExpectedCost = 29;

  // Set up a pre-assignment with different end nodes.
  SetUpModel();
  std::vector<std::vector<int64_t>> routes(kNumVehicles);
  routes[0].push_back(manager_->NodeToIndex(NodeIndex(10)));
  routes[2].push_back(manager_->NodeToIndex(NodeIndex(11)));

  RoutingSearchParameters parameters = GetSearchParameters();
  parameters.set_use_unfiltered_first_solution_strategy(
      use_non_filtered_first_solution_);
  parameters.set_use_full_propagation(!use_light_propagation_);
  model_->CloseModelWithParameters(parameters);
  EXPECT_TRUE(model_->ApplyLocksToAllVehicles(routes, false));

  const Assignment* solution = model_->SolveWithParameters(parameters);
  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(kExpectedCost, solution->ObjectiveValue());

  TestCompactAssignmentFails(*model_, *solution);

  // Set up a pre-assignment with different start nodes.
  SetUpModel();
  routes[2].clear();
  routes[5].push_back(manager_->NodeToIndex(NodeIndex(11)));

  model_->CloseModelWithParameters(parameters);
  EXPECT_TRUE(model_->ApplyLocksToAllVehicles(routes, false));

  solution = model_->SolveWithParameters(parameters);
  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(kExpectedCost, solution->ObjectiveValue());

  TestCompactAssignmentFails(*model_, *solution);
}

INSTANTIATE_TEST_SUITE_P(
    WithParameters, CompactAssignmentWithMultipleDepotsTest,
    ::testing::Combine(::testing::Bool(), ::testing::Bool(), ::testing::Bool(),
                       ::testing::Bool()));

TEST(VisitTypesTest, SingleNodesOfTypeTest) {
  const int kNumNodes = 3;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  model.SetVisitType(1, 5, VisitTypePolicy::ADDED_TYPE_REMOVED_FROM_VEHICLE);
  model.SetVisitType(2, 0, VisitTypePolicy::TYPE_ADDED_TO_VEHICLE);
  model.CloseModel();

  EXPECT_EQ(model.GetNumberOfVisitTypes(), 6);
  EXPECT_THAT(model.GetSingleNodesOfType(0), testing::ElementsAre(2));
  EXPECT_THAT(model.GetSingleNodesOfType(5), testing::ElementsAre(1));
  for (int inexisting_type = 1; inexisting_type < 5; inexisting_type++) {
    EXPECT_TRUE(model.GetSingleNodesOfType(inexisting_type).empty());
  }
}

TEST(VisitTypesTest, PairIndicesOfTypeTest) {
  const int kNumNodes = 11;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  const std::vector<PickupDeliveryPair> pickup_delivery_nodes = {
      {{1, 2}, {3}}, {{4, 5}, {6, 7}}, {{8}, {9, 10}}};

  for (const auto& [pickups, deliveries] : pickup_delivery_nodes) {
    const DisjunctionIndex pickup_disjunction(
        model.AddDisjunction(pickups, Model::kNoPenalty,
                             /*max_cardinality=*/1));
    const DisjunctionIndex delivery_disjunction(
        model.AddDisjunction(deliveries, Model::kNoPenalty,
                             /*max_cardinality=*/1));
    model.AddPickupAndDeliverySets(pickup_disjunction, delivery_disjunction);
  }

  for (int index = 1; index < kNumNodes; index++) {
    model.SetVisitType(index, /*type*/ (index + 1) / 2,
                       VisitTypePolicy::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED);
  }
  model.CloseModel();

  // Types go from 0 to 5.
  const int kExpectedNumTypes = 6;
  ASSERT_EQ(model.GetNumberOfVisitTypes(), kExpectedNumTypes);
  const std::vector<std::vector<int>> kExpectedPairIndicesOfType = {
      {}, {0}, {0, 1}, {1}, {1, 2}, {2}};
  for (int type = 0; type < kExpectedPairIndicesOfType.size(); type++) {
    EXPECT_THAT(
        model.GetPairIndicesOfType(type),
        testing::UnorderedElementsAreArray(kExpectedPairIndicesOfType[type]));
  }
}

TEST(VisitTypesConnectComponentTest, NoVisitTypes) {
  const IndexManager manager(/*num_nodes=*/5, /*num_vehicles=*/3, NodeIndex(0));
  Model model(manager);
  model.AddConstantDimensionWithSlack(
      /*value=*/1, /*capacity=*/10, /*slack_max=*/10,
      /*fix_start_cumul_to_zero=*/false, /*name=*/"dim");
  model.CloseModel();
  EXPECT_EQ(model.GetVisitTypeComponents().size(), 0);
  for (const std::vector<int>& component : model.GetVisitTypeComponents()) {
    EXPECT_TRUE(component.empty());
  }
}

TEST(VisitTypesConnectComponentTest, WithVisitTypes) {
  const IndexManager manager(/*num_nodes=*/5, /*num_vehicles=*/3, NodeIndex(0));
  Model model(manager);
  model.AddConstantDimensionWithSlack(
      /*value=*/1, /*capacity=*/10, /*slack_max=*/10,
      /*fix_start_cumul_to_zero=*/false, /*name=*/"dim");
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
  model.CloseModel();
  EXPECT_EQ(model.GetVisitTypeComponents().size(), 4);
  EXPECT_THAT(model.GetVisitTypeComponents(),
              ElementsAre(ElementsAre(0, 1, 3), ElementsAre(2), ElementsAre(),
                          ElementsAre()));
}

enum RegulationMode { HARD, WHEN_ADDING, WHEN_REMOVING };

using ParametrizedVisitTypesTest = ::testing::TestWithParam<RegulationMode>;

TEST_P(ParametrizedVisitTypesTest,
       NewVisitTypeAddedAfterAddingIncompatibility) {
  const int kNumNodes = 11;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  const RegulationMode incompatibility_mode = GetParam();
  if (incompatibility_mode == WHEN_REMOVING) {
    GTEST_SKIP();
  }
  Model model(manager);
  const auto add_incompatibility = [&model, incompatibility_mode](int type1,
                                                                  int type2) {
    if (incompatibility_mode == HARD) {
      model.AddHardTypeIncompatibility(type1, type2);
    } else {
      model.AddTemporalTypeIncompatibility(type1, type2);
    }
  };
  model.SetVisitType(1, 5, VisitTypePolicy::ADDED_TYPE_REMOVED_FROM_VEHICLE);
  model.SetVisitType(2, 0, VisitTypePolicy::TYPE_ADDED_TO_VEHICLE);
  EXPECT_EQ(model.GetNumberOfVisitTypes(), 6);
  add_incompatibility(1, 2);
  // Verify we cannot add an incompatibility for a type that does not exist.
  ASSERT_DEATH(add_incompatibility(1, 7), "");
  ASSERT_DEATH(add_incompatibility(7, 1), "");

  model.SetVisitType(3, 10, VisitTypePolicy::TYPE_ADDED_TO_VEHICLE);
  EXPECT_EQ(model.GetNumberOfVisitTypes(), 11);
  // We should now be able to add an incompatibility for type 7.
  add_incompatibility(1, 7);
  add_incompatibility(7, 1);
}

TEST_P(ParametrizedVisitTypesTest, NewVisitTypeAddedAfterAddingRequirement) {
  const int kNumNodes = 11;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  const RegulationMode requirement_mode = GetParam();
  Model model(manager);
  const auto add_requirement_for_type = [&model, requirement_mode](int type) {
    switch (requirement_mode) {
      case WHEN_ADDING:
        model.AddRequiredTypeAlternativesWhenAddingType(type, {0});
        return;
      case WHEN_REMOVING:
        model.AddRequiredTypeAlternativesWhenRemovingType(type, {0});
        return;
      case HARD:
        model.AddSameVehicleRequiredTypeAlternatives(type, {0});
        return;
    };
  };
  model.SetVisitType(1, 5, VisitTypePolicy::ADDED_TYPE_REMOVED_FROM_VEHICLE);
  model.SetVisitType(2, 0, VisitTypePolicy::TYPE_ADDED_TO_VEHICLE);
  EXPECT_EQ(model.GetNumberOfVisitTypes(), 6);
  add_requirement_for_type(2);
  // Verify we cannot add a requirement for a type that does not exist.
  ASSERT_DEATH(add_requirement_for_type(7), "");

  model.SetVisitType(3, 10, VisitTypePolicy::TYPE_ADDED_TO_VEHICLE);
  EXPECT_EQ(model.GetNumberOfVisitTypes(), 11);
  // We should now be able to add a requirement for type 7.
  add_requirement_for_type(7);
}

INSTANTIATE_TEST_SUITE_P(ParametrizedVisitTypesTest, ParametrizedVisitTypesTest,
                         testing::Values(HARD, WHEN_ADDING, WHEN_REMOVING));

TEST(VisitTypesTest, CyclicPrecedenceNoTopologicalOrder) {
  const int kNumNodes = 4;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  model.AddConstantDimension(
      /*value=*/1, /*capacity=*/100, /*fix_start_cumul_to_zero=*/false,
      /*name=*/"count");
  Dimension* dimension = model.GetMutableDimension("count");
  dimension->AddNodePrecedence(1, 2, /*offset=*/1);
  dimension->AddNodePrecedence(2, 3, /*offset=*/2);
  dimension->AddNodePrecedence(3, 1, /*offset=*/0);

  model.CloseModel();
  EXPECT_EQ(model.GetTopologicallySortedNodePrecedences().size(), 1);
  EXPECT_TRUE(model.GetTopologicallySortedNodePrecedences().back().empty());
}

TEST(VisitTypesTest, AcyclicPrecedenceNoTopologicalOrder) {
  const int kNumNodes = 10;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  model.AddConstantDimension(
      /*value=*/1, /*capacity=*/100, /*fix_start_cumul_to_zero=*/false,
      /*name=*/"count");
  Dimension* dimension = model.GetMutableDimension("count");
  dimension->AddNodePrecedence(1, 2, /*offset=*/1);
  dimension->AddNodePrecedence(1, 7, /*offset=*/45);
  dimension->AddNodePrecedence(2, 3, /*offset=*/2);
  dimension->AddNodePrecedence(2, 4, /*offset=*/50);

  model.CloseModel();
  EXPECT_TRUE(!model.GetTopologicallySortedNodePrecedences().empty());
  const std::vector<std::vector<int>> expected_topological_order = {
      {1}, {2, 7}, {4, 3}};
  const std::vector<std::vector<int>>& model_topological_order =
      model.GetTopologicallySortedNodePrecedences().back();
  ASSERT_EQ(model_topological_order.size(), expected_topological_order.size());
  for (int level = 0; level < model_topological_order.size(); level++) {
    EXPECT_THAT(model_topological_order[level],
                ElementsAreArray(expected_topological_order[level]));
  }
}

TEST(VisitTypesTest, CyclicTypeRequirementNoTopologicalOrder) {
  const int kNumNodes = 4;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  for (int index = 1; index < kNumNodes; index++) {
    model.SetVisitType(index, index, VisitTypePolicy::TYPE_ADDED_TO_VEHICLE);
  }
  ASSERT_EQ(model.GetNumberOfVisitTypes(), kNumNodes);

  model.AddRequiredTypeAlternativesWhenAddingType(1, {2});
  model.AddRequiredTypeAlternativesWhenRemovingType(2, {3});
  model.AddSameVehicleRequiredTypeAlternatives(3, {1});

  model.CloseModel();
  EXPECT_TRUE(model.GetTopologicallySortedVisitTypes().empty());
}

TEST(VisitTypesTest, AcyclicTypeRequirementTopologicalOrder) {
  const int kNumNodes = 10;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  for (int index = 1; index < kNumNodes; index++) {
    model.SetVisitType(index, index, VisitTypePolicy::TYPE_ADDED_TO_VEHICLE);
  }
  ASSERT_EQ(model.GetNumberOfVisitTypes(), kNumNodes);

  model.AddSameVehicleRequiredTypeAlternatives(0, {1, 2, 3, 4});
  model.AddRequiredTypeAlternativesWhenAddingType(1, {2, 3});
  model.AddRequiredTypeAlternativesWhenRemovingType(2, {3, 4});
  model.AddSameVehicleRequiredTypeAlternatives(3, {4});
  model.CloseModel();

  // Only nodes in the requirement graph appear in the topological order, so
  // types 5 to 9 don't figure in the list.
  const std::vector<std::vector<int>> expected_topological_order = {
      {4}, {3}, {2}, {1}, {0}};
  const std::vector<std::vector<int>>& model_topological_order =
      model.GetTopologicallySortedVisitTypes();
  ASSERT_EQ(model_topological_order.size(), expected_topological_order.size());
  for (int level = 0; level < model_topological_order.size(); level++) {
    EXPECT_THAT(model_topological_order[level],
                ElementsAreArray(expected_topological_order[level]));
  }
}

// Test to verify that the requirement "tightness" is correctly considered
// when ordering the types in the same topological level.
// The requirement graph is as follows:
//
//            type4 ---- OR ---- type5 ---- AND ----  type6
//              |        |                   |          |
//              |        |                   |          |
//              v        v                   v          v
//            type3     type1              type2       type0
//
// There are two topological levels in this graph:
// - Types 4, 5 and 6 are in the first level, type 6 having a higher requirement
//   "tightness" (in two singleton required sets) than types 4 and 5 (in one
//   singleton and one 2-element set).
// - In the second level, type 2 has the highest requirement tightness (has
//   2 singleton required sets), followed by types 0 and 3 (each having one
//   singleton required set), and finally type 1 (one two-element required set).
TEST(VisitTypesTest, TypeRequirementSameTopologicalOrderRequirementTightness) {
  const int kNumNodes = 7;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  for (int index = 1; index < kNumNodes; index++) {
    model.SetVisitType(index, index, VisitTypePolicy::TYPE_ADDED_TO_VEHICLE);
  }
  ASSERT_EQ(model.GetNumberOfVisitTypes(), kNumNodes);

  // Type 0 requires 6.
  model.AddRequiredTypeAlternativesWhenAddingType(0, {6});
  // Type 1 requires 4 or 5.
  model.AddRequiredTypeAlternativesWhenAddingType(1, {4, 5});
  // Type 2 requires 5 and 6.
  model.AddRequiredTypeAlternativesWhenRemovingType(2, {5});
  model.AddRequiredTypeAlternativesWhenAddingType(2, {6});
  // Type 3 requires 4.
  model.AddRequiredTypeAlternativesWhenAddingType(3, {4});
  model.CloseModel();

  const std::vector<std::vector<int>> expected_topological_order = {
      {6, 4, 5}, {2, 0, 3, 1}};
  const std::vector<std::vector<int>>& model_topological_order =
      model.GetTopologicallySortedVisitTypes();
  ASSERT_EQ(model_topological_order.size(), expected_topological_order.size());
  for (int level = 0; level < model_topological_order.size(); level++) {
    EXPECT_THAT(model_topological_order[level],
                ElementsAreArray(expected_topological_order[level]));
  }
}

// Tests that the local search operators either do not use
// Model::VehicleVars(), or they set it to -1 if they deactivate a node.
class RoutingLocalSearchOperatorsTest
    : public MultipleVehiclesAndDisjunctionsTest {
 protected:
  void TestLocalSearchOperator(Solver::LocalSearchOperators op_type) {
    const Assignment* assignment = SetUpModelAndSolve();

    Solver* solver = model_->solver();
    ASSERT_NE(solver, nullptr);
    const std::vector<IntVar*>& nexts = model_->Nexts();
    EXPECT_GT(nexts.size(), 0);
    const std::vector<IntVar*>& vehicle_vars = model_->VehicleVars();
    EXPECT_GT(vehicle_vars.size(), 0);

    LocalSearchOperator* op =
        force_non_homogeneous_costs_
            ? solver->MakeOperator(nexts, vehicle_vars, op_type)
            : solver->MakeOperator(nexts, op_type);

    TestLocalSearchOperatorInternal(assignment, op);
  }

  void TestEvaluatorLocalSearchOperator(
      Solver::EvaluatorLocalSearchOperators op_type) {
    const Assignment* assignment = SetUpModelAndSolve();

    Solver* solver = model_->solver();
    ASSERT_NE(solver, nullptr);
    const std::vector<IntVar*>& nexts = model_->Nexts();
    EXPECT_GT(nexts.size(), 0);
    const std::vector<IntVar*>& vehicle_vars = model_->VehicleVars();
    EXPECT_GT(vehicle_vars.size(), 0);

    Solver::IndexEvaluator3 evaluator = [this](int64_t i, int64_t j, int64_t) {
      CHECK(manager_ != nullptr);
      return Distance(*manager_, i, j);
    };

    LocalSearchOperator* op =
        force_non_homogeneous_costs_
            ? solver->MakeOperator(nexts, vehicle_vars, evaluator, op_type)
            : solver->MakeOperator(nexts, evaluator, op_type);

    TestLocalSearchOperatorInternal(assignment, op);
  }

 private:
  void TestLocalSearchOperatorInternal(const Assignment* const assignment,
                                       LocalSearchOperator* const op) {
    const int kNoPath = -1;

    Solver* solver = model_->solver();
    ASSERT_NE(solver, nullptr);
    IntVar* const* const nexts = model_->Nexts().data();
    ASSERT_NE(nexts, nullptr);
    IntVar* const* const vehicle_vars = model_->VehicleVars().data();
    ASSERT_NE(vehicle_vars, nullptr);

    Assignment delta(solver);
    Assignment deltadelta(solver);

    const int size = model_->Size();
    op->Start(assignment);
    int64_t remaining_neighbors =
        absl::GetFlag(FLAGS_routing_test_num_neighbors);
    while (op->MakeNextNeighbor(&delta, &deltadelta) &&
           remaining_neighbors > 0) {
      for (int i = 0; i < size; ++i) {
        if (delta.Contains(vehicle_vars[i])) {
          EXPECT_TRUE(delta.Contains(nexts[i]));
          if (delta.Activated(nexts[i])) {
            if (delta.Value(nexts[i]) == i) {
              EXPECT_EQ(kNoPath, delta.Value(vehicle_vars[i]));
            }
          } else {
            EXPECT_FALSE(delta.Activated(vehicle_vars[i]));
          }
        }
      }
      --remaining_neighbors;
      if (!op->HoldsDelta()) delta.Clear();
    }
  }
};

TEST_P(RoutingLocalSearchOperatorsTest, TWOOPT) {
  TestLocalSearchOperator(Solver::TWOOPT);
}

TEST_P(RoutingLocalSearchOperatorsTest, OROPT) {
  TestLocalSearchOperator(Solver::OROPT);
}

TEST_P(RoutingLocalSearchOperatorsTest, RELOCATE) {
  TestLocalSearchOperator(Solver::RELOCATE);
}

TEST_P(RoutingLocalSearchOperatorsTest, EXCHANGE) {
  TestLocalSearchOperator(Solver::EXCHANGE);
}

TEST_P(RoutingLocalSearchOperatorsTest, CROSS) {
  TestLocalSearchOperator(Solver::CROSS);
}

TEST_P(RoutingLocalSearchOperatorsTest, MAKEACTIVE) {
  TestLocalSearchOperator(Solver::MAKEACTIVE);
}

TEST_P(RoutingLocalSearchOperatorsTest, MAKEINACTIVE) {
  TestLocalSearchOperator(Solver::MAKEINACTIVE);
}

TEST_P(RoutingLocalSearchOperatorsTest, MAKECHAININACTIVE) {
  TestLocalSearchOperator(Solver::MAKECHAININACTIVE);
}

TEST_P(RoutingLocalSearchOperatorsTest, SWAPACTIVE) {
  TestLocalSearchOperator(Solver::SWAPACTIVE);
}

TEST_P(RoutingLocalSearchOperatorsTest, SWAPACTIVECHAIN) {
  TestLocalSearchOperator(Solver::SWAPACTIVECHAIN);
}

TEST_P(RoutingLocalSearchOperatorsTest, EXTENDEDSWAPACTIVE) {
  TestLocalSearchOperator(Solver::EXTENDEDSWAPACTIVE);
}

TEST_P(RoutingLocalSearchOperatorsTest, PATHLNS) {
  TestLocalSearchOperator(Solver::PATHLNS);
}

TEST_P(RoutingLocalSearchOperatorsTest, UNACTIVELNS) {
  TestLocalSearchOperator(Solver::UNACTIVELNS);
}

TEST_P(RoutingLocalSearchOperatorsTest, LK) {
  TestEvaluatorLocalSearchOperator(Solver::LK);
}

TEST_P(RoutingLocalSearchOperatorsTest, TSPOPT) {
  TestEvaluatorLocalSearchOperator(Solver::TSPOPT);
}

TEST_P(RoutingLocalSearchOperatorsTest, TSPLNS) {
  TestEvaluatorLocalSearchOperator(Solver::TSPLNS);
}

INSTANTIATE_TEST_SUITE_P(WithParameters, RoutingLocalSearchOperatorsTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

class PenaltyTest : public BaseRoutingTest,
                    public ::testing::WithParamInterface<
                        ::testing::tuple<bool, bool, bool, bool>> {
 protected:
  PenaltyTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/::testing::get<0>(GetParam()),
            /*use_light_propagation=*/::testing::get<1>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<2>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<3>(GetParam())) {
  }
  ~PenaltyTest() override = default;
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    manager_ = std::make_unique<IndexManager>(2, 1, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    const int64_t kPenalty = 1;
    const int vehicle_cost =
        use_unary_callback_ ? model->RegisterUnaryTransitCallback(
                                  [](int64_t) { return kPenalty; })
                            : model->RegisterTransitCallback(
                                  [](int64_t, int64_t) { return kPenalty; });
    model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    model->AddDisjunction(manager_->NodesToIndices({NodeIndex(1)}), kPenalty);
    return model;
  }
  Assignment* CreateFirstSolutionAssignment() override {
    const int64_t kFirstSolution[] = {1, 2};
    Assignment* assignment = new Assignment(model_->solver());
    for (int i = 0; i < std::size(kFirstSolution); ++i) {
      assignment->Add(model_->Nexts()[i])->SetValue(kFirstSolution[i]);
    }
    return assignment;
  }
  int VehicleCostCount() const override { return 1; }
  RoutingSearchParameters GetSearchParameters() override {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    return parameters;
  }
};

TEST_P(PenaltyTest, Penalty) {
  const int64_t kExpectedCost = 1;
  const int64_t kExpectedSolution[] = {2, 1};
  const Assignment* const assignment = SetUpModelAndSolve();
  EXPECT_EQ(kExpectedCost, assignment->ObjectiveValue());
  for (int i = 0; i < std::size(kExpectedSolution); ++i) {
    EXPECT_EQ(kExpectedSolution[i], assignment->Value(model_->Nexts()[i]));
  }
}

INSTANTIATE_TEST_SUITE_P(WithParameters, PenaltyTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

void CheckStartCumulStatus(const Model& model, int num_vehicles,
                           const std::string& dimension, bool expect_bounded) {
  for (int i = 0; i < num_vehicles; ++i) {
    IntVar* const cumulVar =
        model.GetDimensionOrDie(dimension).CumulVar(model.Start(i));
    if (expect_bounded) {
      EXPECT_TRUE(cumulVar->Bound());
      EXPECT_EQ(0, cumulVar->Value());
    } else {
      EXPECT_FALSE(cumulVar->Bound());
      EXPECT_EQ(kint64max, cumulVar->Max());
      EXPECT_EQ(0, cumulVar->Min());
    }
  }
}

using DimensionTest = ::testing::TestWithParam<bool>;
TEST_P(DimensionTest, Model) {
  IndexManager manager(1, 1, NodeIndex(0));
  Model model(manager);
  const int callback =
      GetParam()
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  model.AddDimension(callback,
                     /* slack_max */ 0,
                     /* capacity */ kint64max,
                     /* fix_start_cumul_to_zero */ true, /* name */ "dim");
  const Dimension& dimension = model.GetDimensionOrDie("dim");
  EXPECT_EQ(dimension.model(), &model);
}

// TODO(user): Decompose this test in multiple smaller and easier-to-read
// tests.
TEST_P(DimensionTest, Accessors) {
  static const char* const kDimensions[] = {"dim1", "dim2", "dim3", "dim4",
                                            "dim5", "dim6", "dim7", "dim8"};
  static const bool kHasSoftUpperBound[] = {false, false, false, false,
                                            true,  true,  true,  true};
  static const bool kHasSoftLowerBound[] = {false, false, true, true,
                                            false, false, true, true};
  static const bool kHasSpanCost[] = {false, true, false, true,
                                      false, true, false, true};
  static const bool kHasGlobalSpanCost[] = {true, true, false, false,
                                            true, true, false, false};
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  for (bool disable_lp_scheduling : {true, false}) {
    parameters.set_disable_scheduling_beware_this_may_degrade_performance(
        disable_lp_scheduling);
    for (bool has_forbidden_intervals : {true, false}) {
      IndexManager manager(5, 2, NodeIndex(0));
      Model model(manager);
      Solver* const solver = model.solver();
      std::vector<Dimension*> dimensions;
      for (int i = 0; i < std::size(kDimensions); ++i) {
        if (GetParam()) {
          model.AddDimension(
              model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; }),
              0, kint64max, false, kDimensions[i]);
        } else {
          model.AddDimension(model.RegisterTransitCallback(
                                 [](int64_t, int64_t) { return 1LL; }),
                             0, kint64max, false, kDimensions[i]);
        }
        Dimension* dimension = model.GetMutableDimension(kDimensions[i]);
        dimensions.push_back(dimension);
        if (kHasSoftUpperBound[i]) {
          dimension->SetCumulVarSoftUpperBound(1, 1, 1);
        }
        if (kHasSoftLowerBound[i]) {
          dimension->SetCumulVarSoftLowerBound(1, 1, 1);
        }
        if (kHasSpanCost[i]) {
          dimension->SetSpanCostCoefficientForAllVehicles(1);
        }
        if (kHasGlobalSpanCost[i]) {
          dimension->SetGlobalSpanCostCoefficient(1);
        }
        dimension->CumulVar(1)->SetRange(0, 10);
        if (has_forbidden_intervals) {
          const std::vector<int64_t> starts = {5};
          const std::vector<int64_t> ends = {7};
          solver->AddConstraint(
              solver->MakeNotMemberCt(dimension->CumulVar(1), starts, ends));
        }
      }
      EXPECT_THAT(
          model.GetAllDimensionNames(),
          testing::UnorderedElementsAreArray({"dim1", "dim2", "dim3", "dim4",
                                              "dim5", "dim6", "dim7", "dim8"}));
      EXPECT_THAT(model.GetDimensions(),
                  testing::UnorderedElementsAreArray(dimensions));
      EXPECT_THAT(
          model.GetDimensionsWithSoftOrSpanCosts(),
          testing::UnorderedElementsAreArray(
              {dimensions[1], dimensions[2], dimensions[3], dimensions[4],
               dimensions[5], dimensions[6], dimensions[7]}));

      model.CloseModelWithParameters(parameters);

      if (disable_lp_scheduling) {
        EXPECT_TRUE(model.GetDimensionsWithGlobalCumulOptimizers().empty());
        EXPECT_TRUE(model.GetDimensionsWithLocalCumulOptimizers().empty());
        continue;
      }
      EXPECT_THAT(
          model.GetDimensionsWithGlobalCumulOptimizers(),
          testing::UnorderedElementsAreArray(
              {dimensions[0], dimensions[1], dimensions[4], dimensions[5]}));
      EXPECT_THAT(
          model.GetDimensionsWithLocalCumulOptimizers(),
          testing::UnorderedElementsAreArray(
              {dimensions[3], dimensions[5], dimensions[6], dimensions[7]}));
    }
  }
}

TEST_P(DimensionTest, UnaryDimensionTest) {
  const int kNumNodes = 5;
  const int kNumVehicles = 2;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  Model model(manager);
  const int callback1 =
      model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; });
  const int callback2 =
      model.RegisterTransitCallback([](int64_t, int64_t) { return 2LL; });

  std::vector<Dimension*> dimensions;
  std::vector<bool> expected_unary;

  // dim1 has a unary callback for both vehicles, so it's unary.
  model.AddDimension(callback1, 0, 10, true, "dim1");
  dimensions.push_back(model.GetMutableDimension("dim1"));
  expected_unary.push_back(true);

  // dim2 has one unary and one non-unary callback, so it's not unary.
  model.AddDimensionWithVehicleTransits({callback1, callback2}, 0, 10, true,
                                        "dim2");
  dimensions.push_back(model.GetMutableDimension("dim2"));
  expected_unary.push_back(false);

  // dim3 is a constant dimension, so also unary.
  model.AddConstantDimension(3, 10, true, "dim3");
  dimensions.push_back(model.GetMutableDimension("dim3"));
  expected_unary.push_back(true);

  model.CloseModel();
  for (int i = 0; i < dimensions.size(); ++i) {
    EXPECT_EQ(dimensions[i]->IsUnary(), expected_unary[i]);
  }
  EXPECT_THAT(model.GetUnaryDimensions(), testing::UnorderedElementsAreArray(
                                              {dimensions[0], dimensions[2]}));
}

TEST_P(DimensionTest, StartCumulStatus) {
  const char* kDimension = "test_dimension";
  const int kNumVehicles = 5;
  const bool kExpectBounded[] = {true, false};
  for (int i = 0; i < std::size(kExpectBounded); ++i) {
    {
      IndexManager manager(1, kNumVehicles, NodeIndex(0));
      Model model(manager);
      const int callback =
          GetParam()
              ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
              : model.RegisterTransitCallback(
                    [](int64_t, int64_t) { return 1LL; });
      model.AddDimension(callback, 0, kint64max, kExpectBounded[i], kDimension);
      CheckStartCumulStatus(model, kNumVehicles, kDimension, kExpectBounded[i]);
    }
    {
      IndexManager manager(1, kNumVehicles, NodeIndex(0));
      Model model(manager);
      const int callback =
          GetParam()
              ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
              : model.RegisterTransitCallback(
                    [](int64_t, int64_t) { return 1LL; });
      model.AddDimensionWithVehicleCapacity(
          callback, 0, std::vector<int64_t>(kNumVehicles, kint64max),
          kExpectBounded[i], kDimension);
      CheckStartCumulStatus(model, kNumVehicles, kDimension, kExpectBounded[i]);
    }
    {
      IndexManager manager(1, kNumVehicles, NodeIndex(0));
      Model model(manager);
      model.AddConstantDimension(1, kint64max, kExpectBounded[i], kDimension);
      CheckStartCumulStatus(model, kNumVehicles, kDimension, kExpectBounded[i]);
    }
    std::vector<int64_t> values(1 + kNumVehicles, 1);
    {
      IndexManager manager(1, kNumVehicles, NodeIndex(0));
      Model model(manager);
      model.AddVectorDimension(values, kint64max, kExpectBounded[i],
                               kDimension);
      CheckStartCumulStatus(model, kNumVehicles, kDimension, kExpectBounded[i]);
    }
    std::vector<std::vector<int64_t>> matrix(1 + kNumVehicles, values);
    {
      IndexManager manager(1, kNumVehicles, NodeIndex(0));
      Model model(manager);
      model.AddMatrixDimension(matrix, kint64max, kExpectBounded[i],
                               kDimension);
      CheckStartCumulStatus(model, kNumVehicles, kDimension, kExpectBounded[i]);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(WithOrWithoutUnaryCallback, DimensionTest,
                         ::testing::Bool());

std::bitset<Dimension::kNumLinearConstraintBits> MakeLinearConstraintsBitset(
    absl::Span<const Dimension::LinearConstraintBit> bits) {
  std::bitset<Dimension::kNumLinearConstraintBits> bitset;
  for (const Dimension::LinearConstraintBit bit : bits) {
    bitset.set(bit);
  }
  return bitset;
}

TEST(GetLinearConstraintsInfoForVehicleTest, UnconstrainedVehicle) {
  const IndexManager manager(/*num_nodes=*/3, /*num_vehicles=*/2, NodeIndex(0));
  Model model(manager);
  const std::string kDimName = "dim";
  model.AddConstantDimensionWithSlack(1, /*capacity=*/100, /*slack_max=*/10,
                                      /*fix_start_cumul_to_zero=*/true,
                                      kDimName);
  Dimension* const dimension = model.GetMutableDimension(kDimName);

  // Add break constraints only on vehicle 0; vehicle 1 remains unconstrained.
  dimension->SetBreakDistanceDurationOfVehicle(/*distance=*/20, /*duration=*/5,
                                               0);
  EXPECT_THAT(dimension->GetLinearConstraintsInfoForVehicle(1),
              FieldsAre(MakeLinearConstraintsBitset({}),
                        /*has_break_constraints=*/false));
}

TEST(GetLinearConstraintsInfoForVehicleTest, WithSpanCostAndBreakInterval) {
  const IndexManager manager(/*num_nodes=*/3, /*num_vehicles=*/1, NodeIndex(0));
  Model model(manager);
  const std::string kDimName = "dim";
  model.AddConstantDimensionWithSlack(1, /*capacity=*/100, /*slack_max=*/10,
                                      /*fix_start_cumul_to_zero=*/true,
                                      kDimName);
  Dimension* const dimension = model.GetMutableDimension(kDimName);

  dimension->SetSpanCostCoefficientForVehicle(10, 0);
  dimension->SetSoftSpanUpperBoundForVehicle({/*bound=*/50, /*cost=*/5}, 0);
  // Setting a quadratic soft span upper bound with cost == 0 should not set
  // kHasQuadraticCostSoftSpanUpperBound.
  dimension->SetQuadraticCostSoftSpanUpperBoundForVehicle(
      {/*bound=*/50, /*cost=*/0}, 0);
  IntervalVar* const break_interval =
      model.solver()->MakeFixedDurationIntervalVar(10, 20, 5, false, "break");
  dimension->SetBreakIntervalsOfVehicle({break_interval}, 0, {});

  EXPECT_THAT(dimension->GetLinearConstraintsInfoForVehicle(0),
              FieldsAre(MakeLinearConstraintsBitset(
                            {Dimension::kHasSpanOrSlackCost,
                             Dimension::kHasSoftSpanUpperBound}),
                        /*has_break_constraints=*/true));
}

TEST(GetLinearConstraintsInfoForVehicleTest, WithSlackCostAndBreakDistance) {
  const IndexManager manager(/*num_nodes=*/3, /*num_vehicles=*/1, NodeIndex(0));
  Model model(manager);
  const std::string kDimName = "dim";
  model.AddConstantDimensionWithSlack(1, /*capacity=*/100, /*slack_max=*/10,
                                      /*fix_start_cumul_to_zero=*/true,
                                      kDimName);
  Dimension* const dimension = model.GetMutableDimension(kDimName);

  dimension->SetSlackCostCoefficientForVehicle(5, 0);
  dimension->SetSpanUpperBoundForVehicle(80, 0);
  // Setting a soft span upper bound with cost == 0 should not set
  // kHasSoftSpanUpperBound.
  dimension->SetSoftSpanUpperBoundForVehicle({/*bound=*/50, /*cost=*/0}, 0);
  dimension->SetQuadraticCostSoftSpanUpperBoundForVehicle(
      {/*bound=*/60, /*cost=*/4}, 0);
  dimension->SetBreakDistanceDurationOfVehicle(/*distance=*/20, /*duration=*/5,
                                               0);
  EXPECT_THAT(dimension->GetLinearConstraintsInfoForVehicle(0),
              FieldsAre(MakeLinearConstraintsBitset(
                            {Dimension::kHasSpanOrSlackCost,
                             Dimension::kHasSpanUpperBound,
                             Dimension::kHasQuadraticCostSoftSpanUpperBound}),
                        /*has_break_constraints=*/true));
}

TEST(GetLinearConstraintsInfoForVehicleTest, CumulSoftLowerAndUpperBounds) {
  const IndexManager manager(/*num_nodes=*/3, /*num_vehicles=*/9, NodeIndex(0));
  Model model(manager);
  const std::string kDimName = "dim";
  model.AddConstantDimensionWithSlack(1, /*capacity=*/100, /*slack_max=*/10,
                                      /*fix_start_cumul_to_zero=*/true,
                                      kDimName);
  Dimension* const dimension = model.GetMutableDimension(kDimName);

  // Dimension-wide cumul soft lower/upper bounds apply to all vehicles.
  dimension->SetCumulVarSoftLowerBound(1, /*lower_bound=*/10, /*cost=*/2);
  dimension->SetCumulVarSoftUpperBound(1, /*upper_bound=*/90, /*cost=*/3);
  for (int v = 0; v < model.vehicles(); ++v) {
    EXPECT_THAT(dimension->GetLinearConstraintsInfoForVehicle(v),
                FieldsAre(MakeLinearConstraintsBitset(
                              {Dimension::kHasCumulVarSoftLowerBounds,
                               Dimension::kHasCumulVarSoftUpperBounds}),
                          /*has_break_constraints=*/false));
  }
}

TEST(RequiresLocalCumulOptimizerTest, UnconstrainedAndSingleConstraint) {
  const IndexManager manager(/*num_nodes=*/3, /*num_vehicles=*/1, NodeIndex(0));
  Model model(manager);
  const std::string kDimName = "dim";
  model.AddConstantDimensionWithSlack(1, /*capacity=*/100, /*slack_max=*/10,
                                      /*fix_start_cumul_to_zero=*/true,
                                      kDimName);
  Dimension* const dimension = model.GetMutableDimension(kDimName);
  model.CloseModel();

  EXPECT_FALSE(dimension->RequiresLocalCumulOptimizer());

  // Span cost and slack cost on vehicle 0 both map to the same single
  // constraint bit (kHasSpanOrSlackCost), so the total count remains 1.
  dimension->SetSpanCostCoefficientForVehicle(10, 0);
  dimension->SetSlackCostCoefficientForVehicle(5, 0);
  EXPECT_FALSE(dimension->RequiresLocalCumulOptimizer());
}

TEST(RequiresLocalCumulOptimizerTest, WithResourceGroup) {
  const IndexManager manager(/*num_nodes=*/3, /*num_vehicles=*/1, NodeIndex(0));
  Model model(manager);
  const std::string kDimName = "dim";
  model.AddConstantDimensionWithSlack(1, /*capacity=*/100, /*slack_max=*/10,
                                      /*fix_start_cumul_to_zero=*/true,
                                      kDimName);
  Dimension* const dimension = model.GetMutableDimension(kDimName);

  ResourceGroup& resource_group = *model.AddResourceGroup();
  resource_group.AddResource(Attributes(Domain(0, 50), Domain(0, 100)),
                             dimension);
  resource_group.NotifyVehicleRequiresAResource(0);
  model.CloseModel();

  EXPECT_TRUE(dimension->RequiresLocalCumulOptimizer());
}

TEST(RequiresLocalCumulOptimizerTest, WithBreakConstraints) {
  const IndexManager manager(/*num_nodes=*/3, /*num_vehicles=*/2, NodeIndex(0));
  Model model(manager);
  const std::string kDimName = "dim";
  model.AddConstantDimensionWithSlack(1, /*capacity=*/100, /*slack_max=*/10,
                                      /*fix_start_cumul_to_zero=*/true,
                                      kDimName);
  Dimension* const dimension = model.GetMutableDimension(kDimName);
  model.CloseModel();
  EXPECT_FALSE(dimension->RequiresLocalCumulOptimizer());

  dimension->SetBreakDistanceDurationOfVehicle(/*distance=*/20, /*duration=*/5,
                                               1);
  EXPECT_TRUE(dimension->RequiresLocalCumulOptimizer());
}

TEST(RequiresLocalCumulOptimizerTest, TwoConstraintsOnSameVehicle) {
  const IndexManager manager(/*num_nodes=*/3, /*num_vehicles=*/1, NodeIndex(0));
  Model model(manager);
  const std::string kDimName = "dim";
  model.AddConstantDimensionWithSlack(1, /*capacity=*/100, /*slack_max=*/10,
                                      /*fix_start_cumul_to_zero=*/true,
                                      kDimName);
  Dimension* const dimension = model.GetMutableDimension(kDimName);
  model.CloseModel();

  dimension->SetSpanCostCoefficientForVehicle(10, 0);
  EXPECT_FALSE(dimension->RequiresLocalCumulOptimizer());

  dimension->SetSoftSpanUpperBoundForVehicle({/*bound=*/50, /*cost=*/5}, 0);
  EXPECT_TRUE(dimension->RequiresLocalCumulOptimizer());
}

TEST(RequiresLocalCumulOptimizerTest, TwoConstraintsAcrossDifferentVehicles) {
  const IndexManager manager(/*num_nodes=*/3, /*num_vehicles=*/2, NodeIndex(0));
  Model model(manager);
  const std::string kDimName = "dim";
  model.AddConstantDimensionWithSlack(1, /*capacity=*/100, /*slack_max=*/10,
                                      /*fix_start_cumul_to_zero=*/true,
                                      kDimName);
  Dimension* const dimension = model.GetMutableDimension(kDimName);
  model.CloseModel();

  // Vehicle 0 has only 1 linear constraint (kHasSpanOrSlackCost).
  dimension->SetSpanCostCoefficientForVehicle(10, 0);
  EXPECT_FALSE(dimension->RequiresLocalCumulOptimizer());

  // Vehicle 1 has a different single linear constraint (kHasSpanUpperBound).
  dimension->SetSpanUpperBoundForVehicle(80, 1);
  EXPECT_FALSE(dimension->RequiresLocalCumulOptimizer());
}

TEST(RequiresLocalCumulOptimizerTest, CumulSoftLowerAndUpperBounds) {
  const IndexManager manager(/*num_nodes=*/3, /*num_vehicles=*/1, NodeIndex(0));
  Model model(manager);
  const std::string kDimName = "dim";
  model.AddConstantDimensionWithSlack(1, /*capacity=*/100, /*slack_max=*/10,
                                      /*fix_start_cumul_to_zero=*/true,
                                      kDimName);
  Dimension* const dimension = model.GetMutableDimension(kDimName);
  model.CloseModel();

  dimension->SetCumulVarSoftLowerBound(1, /*lower_bound=*/10, /*cost=*/2);
  EXPECT_FALSE(dimension->RequiresLocalCumulOptimizer());

  dimension->SetCumulVarSoftUpperBound(1, /*upper_bound=*/90, /*cost=*/3);
  EXPECT_TRUE(dimension->RequiresLocalCumulOptimizer());
}

// Check bug with infeasible first solutions and metaheuristics.
// The bug is actually in the constraint solver library but adding
// a test here as a non-regression test.
// Unfeasible model used: not enough vehicle capacity for nodes.
// TODO(user): add test in local_search_test.cc.

TEST(InfeasibilityTest, InfeasibilityWithMetaheuristics) {
  const LocalSearchMetaheuristic::Value kMetaheuristics[] = {
      LocalSearchMetaheuristic::GUIDED_LOCAL_SEARCH,
      LocalSearchMetaheuristic::SIMULATED_ANNEALING,
      LocalSearchMetaheuristic::TABU_SEARCH};
  for (int i = 0; i < std::size(kMetaheuristics); ++i) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_local_search_metaheuristic(kMetaheuristics[i]);
    IndexManager manager(4, 2, NodeIndex(0));
    Model model(manager);
    model.AddDimension(model.RegisterTransitCallback(
                           absl::bind_front(GetFixedDemand, &model, 100)),
                       0, 100,
                       /*fix_start_cumul_to_zero=*/true, "Demand");
    const Assignment* const solution = model.SolveWithParameters(parameters);
    EXPECT_EQ(solution, nullptr);
  }
}

TEST(InfeasibilityTest, InfeasibilityWithAllMetaheuristics) {
  // This unit test sometimes "loops forever" in zapfhahn (coverage) mode, so
  // we disable it in that mode: fusion2/50769406-5a1d-4791-99ff-a5a30526a216.
  // TODO(user): Investigate.
  if (HACK_CodeCoverageIsEnabled()) {
    GTEST_SKIP() << "Can get stuck in coverage mode";
  }
  const LocalSearchMetaheuristic::Value kMetaheuristics[] = {
      LocalSearchMetaheuristic::GUIDED_LOCAL_SEARCH,
      LocalSearchMetaheuristic::SIMULATED_ANNEALING,
      LocalSearchMetaheuristic::TABU_SEARCH};
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  for (int i = 0; i < std::size(kMetaheuristics); ++i) {
    parameters.add_local_search_metaheuristics(kMetaheuristics[i]);
  }
  IndexManager manager(4, 2, NodeIndex(0));
  Model model(manager);
  model.AddDimension(model.RegisterTransitCallback(
                         absl::bind_front(GetFixedDemand, &model, 100)),
                     0, 100,
                     /*fix_start_cumul_to_zero=*/true, "Demand");
  const Assignment* const solution = model.SolveWithParameters(parameters);
  EXPECT_EQ(solution, nullptr);
}

// Variable vehicle capacity tests.

int64_t GetNodeDemand(IndexManager* manager, Model* model, int64_t from,
                      int64_t /*to*/) {
  const int64_t kSmallNode = 1;
  const int64_t kLargeDemand = 11;
  const int64_t kNoDemand = 0;
  if (manager->IndexToNode(from) != kSmallNode && !model->IsEnd(from) &&
      !model->IsStart(from)) {
    return kLargeDemand;
  } else {
    return kNoDemand;
  }
}

TEST(BaseVariableCapacityDeathTest, EmptyDimensionCapacity) {
  const int kNumNodes = 30;
  const int kNumVehicles = 10;
  const char* kDimensionName = "CapacityDimension";
  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  const int callback = model.RegisterTransitCallback(
      absl::bind_front(GetNodeDemand, &manager, &model));
  std::vector<int64_t> capacities;
  EXPECT_DEATH(model.AddDimensionWithVehicleCapacity(
                   callback, 0, capacities,
                   /*fix_start_cumul_to_zero=*/true, kDimensionName),
               "");
}

Model::StateDependentTransit NullStateDependentTransit(int64_t, int64_t) {
  return Model::MakeStateDependentTransit([](int64_t) { return 0; }, 0,
                                          kint64max);
}

TEST(BaseVariableCapacityDeathTest, EmptyDimensionDependentDimensionCapacity) {
  const int kNumNodes = 30;
  const int kNumVehicles = 10;
  const char* kDimensionName = "CapacityDimension";
  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  model.AddConstantDimensionWithSlack(1, 100, 100, false, "time");
  const Dimension& time_dimension = model.GetDimensionOrDie("time");
  const int callback = model.RegisterTransitCallback(
      absl::bind_front(GetNodeDemand, &manager, &model));
  std::vector<int> pure_transits(kNumVehicles, callback);
  const int state_dependent_callback =
      model.RegisterStateDependentTransitCallback(NullStateDependentTransit);
  std::vector<int> dependent_transits(kNumVehicles, state_dependent_callback);
  std::vector<int64_t> capacities;
  EXPECT_DEATH(
      model.AddDimensionDependentDimensionWithVehicleCapacity(
          pure_transits, dependent_transits, &time_dimension, 0, capacities,
          /*fix_start_cumul_to_zero=*/true, kDimensionName),
      "");
}

// Problem with cheap vehicles with low capacity and one single expensive and
// large capacity vehicle; many high demand nodes and a single low demand node.
// Resulting solution should put all high demand nodes on the large vehicle
// and the low demand on a small vehicle.
class VariableCapacityTest : public BaseRoutingTest,
                             public ::testing::WithParamInterface<
                                 ::testing::tuple<bool, bool, bool, bool>> {
 protected:
  static constexpr int kBigVehicle = 5;

  VariableCapacityTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/::testing::get<0>(GetParam()),
            /*use_light_propagation=*/::testing::get<1>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<2>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<3>(GetParam())) {
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    const char* kDimensionName = "CapacityDimension";
    const int kNumNodes = 30;
    const int kNumVehicles = 10;
    manager_ =
        std::make_unique<IndexManager>(kNumNodes, kNumVehicles, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    const int callback = model->RegisterTransitCallback(
        absl::bind_front(GetNodeDemand, manager_.get(), model));
    model->AddDimensionWithVehicleCapacity(
        callback, 0, GetVehicleCapacities(kNumVehicles, kBigVehicle),
        /*fix_start_cumul_to_zero=*/true, kDimensionName);

    const int vehicle_cost =
        use_unary_callback_
            ? model->RegisterUnaryTransitCallback([](int64_t) { return 1; })
            : model->RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1; });
    const int big_vehicle_cost =
        use_unary_callback_
            ? model->RegisterUnaryTransitCallback([](int64_t) { return 10; })
            : model->RegisterTransitCallback(
                  [](int64_t, int64_t) { return 10; });
    for (int vehicle = 0; vehicle < kNumVehicles; ++vehicle) {
      const int cost =
          (vehicle == kBigVehicle) ? big_vehicle_cost : vehicle_cost;
      model->SetArcCostEvaluatorOfVehicle(cost, vehicle);
    }
    return model;
  }
  RoutingSearchParameters GetSearchParameters() override {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.mutable_local_search_operators()->set_use_path_lns(BOOL_TRUE);
    parameters.mutable_local_search_operators()->set_use_inactive_lns(
        BOOL_TRUE);
    parameters.set_first_solution_strategy(
        FirstSolutionStrategy::PATH_CHEAPEST_ARC);
    return parameters;
  }
};

TEST_P(VariableCapacityTest, AssignmentWithVehicleCapacity) {
  const int64_t kExpectedCost = 292;
  SetUpModel();
  RoutingSearchParameters parameters = GetSearchParameters();
  parameters.set_use_unfiltered_first_solution_strategy(
      use_non_filtered_first_solution_);
  parameters.set_use_full_propagation(!use_light_propagation_);
  const Assignment* const solution = model_->SolveWithParameters(parameters);
  ASSERT_NE(solution, nullptr);
  EXPECT_EQ(kExpectedCost, solution->ObjectiveValue());
  EXPECT_TRUE(
      model_->solver()->CheckAssignment(const_cast<Assignment*>(solution)));
  EXPECT_EQ(2, model_->GetVehicleClassesCount());
  for (int vehicle = 0; vehicle < model_->vehicles(); ++vehicle) {
    if (vehicle != kBigVehicle && model_->IsVehicleUsed(*solution, vehicle)) {
      // There's a single node visited with a small demand.
      const int64_t vehicle_first = model_->Start(vehicle);
      const int64_t next_first =
          solution->Value(model_->NextVar(vehicle_first));
      const int64_t vehicle_last = solution->Value(model_->NextVar(next_first));
      EXPECT_GE(GetVehicleCapacity(kBigVehicle, vehicle),
                GetNodeDemand(manager_.get(), model_.get(), next_first,
                              vehicle_last));
      EXPECT_TRUE(model_->IsEnd(vehicle_last));
    }
  }
  DisplayPlan(*model_, *solution);
}

INSTANTIATE_TEST_SUITE_P(WithParameters, VariableCapacityTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

// Inspecting model with NotMember constraints creating disjoint windows in a
// cumul variable. We check that overlapping intervals are merged.
using ModelInspection = ::testing::TestWithParam<bool>;
TEST_P(ModelInspection, CumulNotMemberCt) {
  const char* kDimension = "dimension";
  const int64_t kExpectedIntervals[][2] = {{10, 20}, {30, 40}};
  const std::vector<int64_t> starts1 = {10, 30};
  const std::vector<int64_t> ends1 = {20, 32};
  const std::vector<int64_t> starts2 = {31};
  const std::vector<int64_t> ends2 = {40};
  IndexManager manager(2, 1, NodeIndex(0));
  Model model(manager);
  const int callback =
      GetParam()
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  model.AddDimension(callback, kint64max, kint64max,
                     /*fix_start_cumul_to_zero=*/true, kDimension);
  Dimension* const dimension = model.GetMutableDimension(kDimension);
  Solver* solver = model.solver();
  solver->AddConstraint(
      solver->MakeNotMemberCt(dimension->CumulVar(1), starts1, ends1));
  dimension->CumulVar(model.Start(0))->SetMin(0);
  solver->AddConstraint(
      solver->MakeNotMemberCt(dimension->CumulVar(1), starts2, ends2));
  model.CloseModel();
  EXPECT_EQ(3, dimension->forbidden_intervals().size());
  EXPECT_EQ(std::size(kExpectedIntervals),
            dimension->forbidden_intervals()[1].NumIntervals());
  int index = 0;
  for (const auto& interval : dimension->forbidden_intervals()[1]) {
    EXPECT_EQ(kExpectedIntervals[index][0], interval.start);
    EXPECT_EQ(kExpectedIntervals[index][1], interval.end);
    ++index;
  }
}

TEST_P(ModelInspection, NotMemberCtOnNonCumulVar) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model model(manager);
  Solver* const solver = model.solver();
  IntVar* var = solver->MakeIntVar(0, 10);
  const std::vector<int64_t> starts = {2};
  const std::vector<int64_t> ends = {5};
  solver->AddConstraint(solver->MakeNotMemberCt(var, starts, ends));
  model.CloseModel();
}

INSTANTIATE_TEST_SUITE_P(WithOrWithoutUnaryCallback, ModelInspection,
                         ::testing::Bool());

// Testing transit values

int64_t GetVariableDemand(const IndexManager& manager, int64_t from,
                          int64_t to) {
  return manager.IndexToNode(from).value() + manager.IndexToNode(to).value();
}

using TransitValueTest = ::testing::TestWithParam<bool>;
TEST_P(TransitValueTest, TransitValuesOnOpenModel) {
  const int kNumNodes = 10;
  const int kCapacity = 100;
  IndexManager manager(kNumNodes, 1, NodeIndex(0));
  Model model(manager);
  const int fixed_demand =
      GetParam() ? model.RegisterUnaryTransitCallback(
                       absl::bind_front(GetUnaryFixedDemand, &model, 100))
                 : model.RegisterTransitCallback(
                       absl::bind_front(GetFixedDemand, &model, 100));
  model.AddDimension(fixed_demand, 0, kCapacity,
                     /*fix_start_cumul_to_zero=*/true, "dim1");
  model.AddDimensionWithVehicleCapacity(
      fixed_demand, 0, GetVehicleCapacities(1, 0),
      /*fix_start_cumul_to_zero=*/true, "dim2");
  const int variable_demand = model.RegisterTransitCallback(
      absl::bind_front(GetVariableDemand, manager));
  model.AddDimension(variable_demand, 0, kCapacity,
                     /*fix_start_cumul_to_zero=*/true, "dim3");
  model.AddDimensionWithVehicleCapacity(
      variable_demand, 0, GetVehicleCapacities(1, 0),
      /*fix_start_cumul_to_zero=*/true, "dim4");
  const Dimension& dim1 = model.GetDimensionOrDie("dim1");
  const Dimension& dim2 = model.GetDimensionOrDie("dim2");
  const Dimension& dim3 = model.GetDimensionOrDie("dim3");
  const Dimension& dim4 = model.GetDimensionOrDie("dim4");
  for (int i = 0; i < model.Size(); ++i) {
    for (int j = 0; j < model.Size(); ++j) {
      EXPECT_EQ(GetUnaryFixedDemand(&model, kCapacity, i),
                dim1.GetTransitValue(i, j, 0));
      EXPECT_EQ(GetUnaryFixedDemand(&model, kCapacity, i),
                dim2.GetTransitValue(i, j, 0));
      EXPECT_EQ(GetVariableDemand(manager, i, j),
                dim3.GetTransitValue(i, j, 0));
      EXPECT_EQ(GetVariableDemand(manager, i, j),
                dim4.GetTransitValue(i, j, 0));
    }
  }
}

INSTANTIATE_TEST_SUITE_P(WithOrWithoutUnaryCallback, TransitValueTest,
                         ::testing::Bool());

TEST(DimensionBreaksTest, SetAndGetBreakIntervals) {
  const int kNumNodes = 100;
  const int kNumVehicles = 50;
  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  const int transit = model.RegisterTransitCallback(
      absl::bind_front(ValueIfDifferent, manager, 5));
  model.AddDimension(transit, kint64max, kint64max, false, "typicallytime");
  // Set randomish parameters, this is probably overkill.
  std::vector<int> vehicle_breaks_number(kNumVehicles);
  for (int vehicle = 0; vehicle < kNumVehicles; ++vehicle) {
    vehicle_breaks_number[vehicle] = (vehicle * 19) % 11;
  }
  std::vector<int64_t> visit_durations(manager.num_indices());
  for (int index = 0; index < manager.num_indices(); ++index) {
    visit_durations[index] = (index * 65537) % 97;
  }
  // Set breaks from parameters.
  {
    Dimension* dimension = model.GetMutableDimension("typicallytime");
    for (int vehicle = 0; vehicle < kNumVehicles; ++vehicle) {
      std::vector<IntervalVar*> intervals;
      const int64_t begin = -5000;
      const int64_t end = -begin;
      for (int b = 0; b < vehicle_breaks_number[vehicle]; ++b) {
        intervals.push_back(model.solver()->MakeIntervalVar(
            begin, end, 0, end - begin, begin, end, true, ""));
      }
      dimension->SetBreakIntervalsOfVehicle(intervals, vehicle,
                                            visit_durations);
    }
  }
  // Get breaks back from dimension, check that we have the right amount.
  {
    const Dimension& dimension = model.GetDimensionOrDie("typicallytime");
    for (int vehicle = 0; vehicle < kNumVehicles; ++vehicle) {
      EXPECT_EQ(dimension.GetBreakIntervalsOfVehicle(vehicle).size(),
                vehicle_breaks_number[vehicle]);
    }
  }
}

TEST(TimeLimitRoutingTest, CheckTimeBuffer) {
  RoutingSearchParameters params = DefaultRoutingSearchParameters();
  params.set_fallback_to_cp_sat_size_threshold(0);
  for (const int64_t limit_milliseconds : {0, 1, 100, 1000, 10000}) {
    SCOPED_TRACE(absl::StrCat(limit_milliseconds, " milliseconds"));
    *params.mutable_time_limit() = *util_time::EncodeGoogleApiProto(
        absl::Milliseconds(limit_milliseconds));
    IndexManager manager(50, 10, NodeIndex(0));
    Model model(manager);
    model.SolveWithParameters(params);
    EXPECT_GE(absl::Milliseconds(limit_milliseconds / 20), model.TimeBuffer());
    EXPECT_LE(absl::Milliseconds(limit_milliseconds / 20 -
                                 (kDebugMode || kAnyXsanEnabled ? 10 : 1)),
              model.TimeBuffer());
  }
}

TEST(CancelRoutingTest, InterruptCP) {
  for (bool interrupt : {false, true}) {
    IndexManager manager(/*num_nodes=*/10, /*num_vehicles=*/3, NodeIndex(0));
    Model model(manager);
    for (int i = 1; i < 10; ++i) {
      model.AddDisjunction(std::vector<int64_t>{i}, 1000);
    }
    model.SetArcCostEvaluatorOfAllVehicles(
        model.RegisterTransitCallback([](int64_t, int64_t) { return 1; }));
    if (interrupt) {
      // Interrupt after the first solution where all nodes are unperformed.
      model.AddAtSolutionCallback([&model]() { model.CancelSearch(); });
    }
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(
        FirstSolutionStrategy::ALL_UNPERFORMED);
    const Assignment* solution = model.SolveWithParameters(parameters);
    ASSERT_TRUE(solution != nullptr);
    if (interrupt) {
      EXPECT_EQ(1000 * 9, solution->ObjectiveValue());
    } else {
      EXPECT_EQ(10 * 1, solution->ObjectiveValue());
    }
    EXPECT_EQ(interrupt, model.GetMutableCPInterrupt()->load());

    // TODO(user): CP-SAT set the boolean to true internally when the search
    // is done, fix this.
    if (interrupt) {
      EXPECT_EQ(interrupt, model.GetMutableCPSatInterrupt()->load());
    }
  }
}

TEST(CancelRoutingTest, InterruptCPWithAtomic) {
  for (bool interrupt : {false, true}) {
    IndexManager manager(/*num_nodes=*/10, /*num_vehicles=*/3, NodeIndex(0));
    Model model(manager);
    for (int i = 1; i < 10; ++i) {
      model.AddDisjunction(std::vector<int64_t>{i}, 1000);
    }
    model.SetArcCostEvaluatorOfAllVehicles(
        model.RegisterTransitCallback([](int64_t, int64_t) { return 1; }));
    if (interrupt) {
      // Interrupt after the first solution where all nodes are unperformed.
      model.AddAtSolutionCallback(
          [&model]() { *model.GetMutableCPInterrupt() = true; });
    }
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(
        FirstSolutionStrategy::ALL_UNPERFORMED);
    const Assignment* solution = model.SolveWithParameters(parameters);
    ASSERT_TRUE(solution != nullptr);
    if (interrupt) {
      EXPECT_EQ(1000 * 9, solution->ObjectiveValue());
    } else {
      EXPECT_EQ(10 * 1, solution->ObjectiveValue());
    }
    EXPECT_EQ(interrupt, model.GetMutableCPInterrupt()->load());

    if (interrupt) {
      // CP-SAT was not even called in this case. Otherwise, without interrupt
      // it currently set the boolean to true internally at the end of the
      // search.
      EXPECT_FALSE(model.GetMutableCPSatInterrupt()->load());
    }
  }
}

TEST(CancelRoutingTest, InterruptCPSat) {
  for (bool interrupt : {false, true}) {
    IndexManager manager(/*num_nodes=*/10, /*num_vehicles=*/3, NodeIndex(0));
    Model model(manager);
    for (int i = 1; i < 10; ++i) {
      model.AddDisjunction(std::vector<int64_t>{i}, 1000);
    }
    model.SetArcCostEvaluatorOfAllVehicles(
        model.RegisterTransitCallback([](int64_t, int64_t) { return 1; }));
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_use_cp(BOOL_FALSE);
    parameters.set_use_cp_sat(BOOL_TRUE);
    model.CloseModelWithParameters(parameters);
    if (interrupt) {
      // It's good enough to check that the CP-SAT solver will bail out right
      // away.
      model.CancelSearch();
    }
    const Assignment* solution = model.SolveWithParameters(parameters);
    if (interrupt) {
      EXPECT_EQ(solution, nullptr);
    } else {
      EXPECT_EQ(10 * 1, solution->ObjectiveValue());
    }
    if (interrupt) {
      EXPECT_EQ(interrupt, model.GetMutableCPSatInterrupt()->load());
    }
    EXPECT_EQ(interrupt, model.GetMutableCPInterrupt()->load());
  }
}

TEST(CancelRoutingTest, InterruptCPSatWithAtomic) {
  for (bool interrupt : {false, true}) {
    IndexManager manager(/*num_nodes=*/10, /*num_vehicles=*/3, NodeIndex(0));
    Model model(manager);
    for (int i = 1; i < 10; ++i) {
      model.AddDisjunction(std::vector<int64_t>{i}, 1000);
    }
    model.SetArcCostEvaluatorOfAllVehicles(
        model.RegisterTransitCallback([](int64_t, int64_t) { return 1; }));
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_use_cp(BOOL_FALSE);
    parameters.set_use_cp_sat(BOOL_TRUE);
    model.CloseModelWithParameters(parameters);
    if (interrupt) {
      // It's good enough to check that the CP-SAT solver will bail out right
      // away.
      *model.GetMutableCPSatInterrupt() = true;
    }
    const Assignment* solution = model.SolveWithParameters(parameters);
    if (interrupt) {
      EXPECT_EQ(solution, nullptr);
    } else {
      EXPECT_EQ(10 * 1, solution->ObjectiveValue());
    }
    if (interrupt) {
      EXPECT_EQ(interrupt, model.GetMutableCPSatInterrupt()->load());
    }
    EXPECT_FALSE(model.GetMutableCPInterrupt()->load());
  }
}

class CVRPTWInstanceTest : public BaseRoutingTest,
                           public ::testing::WithParamInterface<
                               ::testing::tuple<bool, bool, bool, bool>> {
 protected:
  static const int kNumNodes;
  static const int kNumVehicles;
  static const int64_t kCapacity;
  static const int64_t kDemand[];
  static const int64_t kReadyTime[];
  static const int64_t kDueTime[];
  static const int64_t kServiceTime[];
  static const int64_t kXCoord[];
  static const int64_t kYCoord[];

  CVRPTWInstanceTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/::testing::get<0>(GetParam()),
            /*use_light_propagation=*/::testing::get<1>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<2>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<3>(GetParam())) {
  }
  int64_t Distance(int64_t i, int64_t j) const {
    const int64_t x_i = kXCoord[i];
    const int64_t y_i = kYCoord[i];
    const int64_t x_j = kXCoord[j];
    const int64_t y_j = kYCoord[j];
    const double xd = x_i - x_j;
    const double yd = y_i - y_j;
    return MathUtil::Round<int64_t>(sqrt(xd * xd + yd * yd));
  }

  int64_t ServeTravelTime(int64_t i, int64_t j) const {
    return kServiceTime[i] + Distance(i, j);
  }
  int64_t UnaryDemand(int64_t i) const { return kDemand[i]; }
  int64_t Demand(int64_t i, int64_t /*j*/) const { return kDemand[i]; }
};

class SweepTest : public CVRPTWInstanceTest {
 protected:
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    manager_ =
        std::make_unique<IndexManager>(kNumNodes, kNumVehicles, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    if (use_unary_callback_) {
      model->AddDimension(model->RegisterUnaryTransitCallback(
                              absl::bind_front(&SweepTest::UnaryDemand, this)),
                          0, kCapacity,
                          /*fix_start_cumul_to_zero=*/true, "demand");
    } else {
      model->AddDimension(model->RegisterTransitCallback(
                              absl::bind_front(&SweepTest::Demand, this)),
                          0, kCapacity,
                          /*fix_start_cumul_to_zero=*/true, "demand");
    }
    model->AddDimension(model->RegisterTransitCallback(absl::bind_front(
                            &SweepTest::ServeTravelTime, this)),
                        kDueTime[0], kDueTime[0],
                        /*fix_start_cumul_to_zero=*/true, "time");
    Dimension* const time_dimension = model->GetMutableDimension("time");
    for (NodeIndex node(0); node < kNumNodes; ++node) {
      const int64_t index = manager_->NodeToIndex(node);
      IntVar* const cumul = time_dimension->CumulVar(index);
      cumul->SetMin(kReadyTime[index]);
      cumul->SetMax(kDueTime[index]);
    }
    const int vehicle_cost = model->RegisterTransitCallback(
        [this](int64_t i, int64_t j) { return Distance(i, j); });
    model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    std::vector<std::pair<int64_t, int64_t>> points(manager_->num_indices());
    for (int64_t i = 0; i < manager_->num_indices(); ++i) {
      const NodeIndex node = manager_->IndexToNode(i);
      points[i] = std::make_pair(kXCoord[node.value()], kYCoord[node.value()]);
    }
    model->SetSweepArranger(new SweepArranger(points));
    return model;
  }
  int VehicleCostCount() const override { return 1; }
  RoutingSearchParameters GetSearchParameters() override {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(FirstSolutionStrategy::SWEEP);
    parameters.set_solution_limit(1);
    return parameters;
  }
};

//
// VEHICLE
// NUMBER     CAPACITY
//   6          90
//
// CUSTOMER
// CUST NO.  XCOORD.    YCOORD.    DEMAND   READY TIME  DUE DATE   SERVICE TIME
//
//     0      70         70          0          0       1351          0
//     1      33         78         20        750        809         90
//     2      59         52         20        553        602         90
//     3      10        137         30        147        219         90
//     4       4         28         10        616        661         90
//     5      25         26         20        128        179         90
//     6      86         37         10        478        531         90
//     7       1        109         10        616        680         90
//     8       6        135         40        351        386         90
//     9      32         79         20        655        721         90
//    10      24         26         20        219        271         90
//

const int CVRPTWInstanceTest::kNumNodes = 11;
const int CVRPTWInstanceTest::kNumVehicles = 6;
const int64_t CVRPTWInstanceTest::kCapacity = 90;
const int64_t CVRPTWInstanceTest::kDemand[] = {0,  20, 20, 30, 10, 20, 10, 10,
                                               40, 20, 20, 0,  0,  0,  0,  0};
const int64_t CVRPTWInstanceTest::kReadyTime[] = {0,   750, 553, 147, 616, 128,
                                                  478, 616, 351, 655, 219};
const int64_t CVRPTWInstanceTest::kDueTime[] = {1351, 809, 602, 219, 661, 179,
                                                531,  680, 386, 721, 271};
const int64_t CVRPTWInstanceTest::kServiceTime[] = {
    0, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 0, 0, 0, 0, 0};
const int64_t CVRPTWInstanceTest::kXCoord[] = {70, 33, 59, 10, 4,  25, 86, 1,
                                               6,  32, 24, 70, 70, 70, 70, 70,
                                               70, 70, 70, 70, 70, 70};
const int64_t CVRPTWInstanceTest::kYCoord[] = {
    70, 78, 52, 137, 28, 26, 37, 109, 135, 79, 26,
    70, 70, 70, 70,  70, 70, 70, 70,  70,  70, 70};

// Solves a CVRPTW instance with few orders using the Wren & Holliday Sweep
// algorithm as a first solution heuristic and verifies that the cost of the
// given solution is the expected.
TEST_P(SweepTest, Sweep) {
  const int64_t kExpectedCost = 762;
  const Assignment* const assignment = SetUpModelAndSolve();

  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(kExpectedCost, assignment->ObjectiveValue());

  TestCompactAssignment(*model_, *assignment, force_non_homogeneous_costs_);
}

INSTANTIATE_TEST_SUITE_P(WithParameters, SweepTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

TEST(DepthFirstSearchTest, ModelWithSubproblem) {
  // Ensuring that when dfs is activated, "non-next" decision variables are
  // actually instantiated.
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_use_depth_first_search(true);
  const int kNumNonVehicleNodes = 0;
  const int kNumVehicles = 1;
  IndexManager manager(kNumNonVehicleNodes + 1, kNumVehicles, NodeIndex(0));
  Model model(manager);
  IntVar* min_var = model.solver()->MakeIntVar(0, 10);
  model.AddVariableTargetToFinalizer(min_var, kint64min);
  model.AddToAssignment(min_var);
  IntVar* max_var = model.solver()->MakeIntVar(0, 10);
  model.AddVariableTargetToFinalizer(max_var, kint64max);
  model.AddToAssignment(max_var);
  const Assignment* const assignment = model.SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(0, assignment->ObjectiveValue());
  EXPECT_EQ(assignment->Max(min_var), assignment->Min(min_var));
  EXPECT_EQ(0, assignment->Value(min_var));
  EXPECT_EQ(assignment->Max(max_var), assignment->Min(max_var));
  EXPECT_EQ(10, assignment->Value(max_var));
}

// Tests the type incompatibilities are correctly taken into account, by solving
// the following model with the Savings heuristic:
//
//               Node2 ----------- 20 ------------ Node3
//                 |   \                             |
//                 8     \                           8
//                 |       15                        |
//               Node1 ----  \ ------- 20 -------- Node4
//                 |            \                    |
//                 10------------ NODE0 ------------10
//
// This model has two vehicles with a depot at NODE0, and 4 nodes to serve.
// The highest Saving is for arcs 1-->2 and 3-->4, with a value of 25-8=17.
// Without the incompatibilities, a single route is created, serving all nodes
// from 1 to 4 and back to the depot.
// We make nodes 1 and 2 have incompatible types, as well as nodes 3 and 4. In
// this case, the result should have two routes, one serving nodes 2 and 3, the
// other 1 and 4.
//
// The first test below checks the result obtained with the feasibility filters,
// and the second without them.
TEST(NodeTypeIncompatibilitiesTest, FilteredSolution) {
  RoutingSearchParameters params = DefaultRoutingSearchParameters();
  params.set_solution_limit(1);
  params.set_first_solution_strategy(FirstSolutionStrategy::SAVINGS);
  params.set_use_unfiltered_first_solution_strategy(false);

  const int kNumNodes = 5;
  const int kNumVehicles = 2;

  std::vector<std::vector<int64_t>> costs = {{0, 10, 15, 15, 10},
                                             {10, 0, 8, 22, 20},
                                             {15, 8, 0, 20, 22},
                                             {15, 22, 20, 0, 8},
                                             {10, 20, 22, 8, 0}};
  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  for (bool hard_incompatibilities : {true, false}) {
    Model model(manager);

    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    for (int i = 1; i < kNumNodes; i++) {
      model.SetVisitType(manager.NodeToIndex(NodeIndex(i)), i,
                         Model::TYPE_ADDED_TO_VEHICLE);
    }
    if (hard_incompatibilities) {
      model.AddHardTypeIncompatibility(1, 2);
      model.AddHardTypeIncompatibility(3, 4);
    } else {
      model.AddTemporalTypeIncompatibility(1, 2);
      model.AddTemporalTypeIncompatibility(3, 4);
    }
    const Assignment* const assignment = model.SolveWithParameters(params);
    ASSERT_NE(assignment, nullptr);
    EXPECT_TRUE(model.IsVehicleUsed(*assignment, 0));
    EXPECT_TRUE(model.IsVehicleUsed(*assignment, 1));
    EXPECT_EQ(assignment->ObjectiveValue(), (10 + 20 + 10) + (15 + 20 + 15));
  }
}

// Same example as above but without using the filter on type incompatibilities.
// Checks that the validity of the solution regarding type incompatibilities is
// ensured by the CPFeasibilityFilter.
TEST(NodeTypeIncompatibilitiesTest, NonFilteredSolution) {
  RoutingSearchParameters params = DefaultRoutingSearchParameters();
  params.set_solution_limit(1);
  params.set_first_solution_strategy(FirstSolutionStrategy::SAVINGS);
  params.set_use_unfiltered_first_solution_strategy(true);

  const int kNumNodes = 5;
  const int kNumVehicles = 2;

  std::vector<std::vector<int64_t>> costs = {{0, 10, 15, 15, 10},
                                             {10, 0, 8, 22, 20},
                                             {15, 8, 0, 20, 22},
                                             {15, 22, 20, 0, 8},
                                             {10, 20, 22, 8, 0}};
  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  for (bool hard_incompatibilities : {true, false}) {
    Model model(manager);

    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    for (int i = 1; i < kNumNodes; i++) {
      model.SetVisitType(manager.NodeToIndex(NodeIndex(i)), i,
                         Model::TYPE_ADDED_TO_VEHICLE);
    }
    if (hard_incompatibilities) {
      model.AddHardTypeIncompatibility(1, 2);
      model.AddHardTypeIncompatibility(3, 4);
    } else {
      model.AddTemporalTypeIncompatibility(1, 2);
      model.AddTemporalTypeIncompatibility(3, 4);
    }

    const Assignment* const assignment = model.SolveWithParameters(params);
    ASSERT_NE(assignment, nullptr);
    EXPECT_TRUE(model.IsVehicleUsed(*assignment, 0));
    EXPECT_TRUE(model.IsVehicleUsed(*assignment, 1));
    EXPECT_EQ(assignment->ObjectiveValue(), (10 + 20 + 10) + (15 + 20 + 15));
  }
}

// Tests the type requirements are correctly taken into account, by solving a
// model similar to the one above:
//
//               Node2 ----------- 20 ------------ Node3
//                 |   \                             |
//                 8     \                           8
//                 |       15                        |
//               Node1 ----  \ ------- 20 -------- Node4
//                 |            \                    |
//                 10------------ NODE0 ------------10
//
// This model has two vehicles with a depot at NODE0 and a capacity of 3 (2
// extra nodes), and 4 nodes to serve.
// The optimal solution would be to serve nodes 1 and 2 on one vehicle, and
// node 3 and 4 on another, but when the type of Node2 is set as a temporal
// requirement for the type of Node3, Node3 has to be visited after Node2 on the
// same route.
// Since the vehicles have a capacity for serving only 2 extra nodes, the 2
// other nodes are served by another vehicle.
TEST(NodeTypeRequirementsTest, TemporalRequiredTypeAlternatives) {
  const int kNumNodes = 5;
  const int kNumVehicles = 2;
  const int kPenalty = 1000;

  const std::vector<absl::flat_hash_set<int>> kNode3RequirementAlternatives = {
      /*Node1 or Node2 required for Node3*/ {2, 1},
      /*Node2 or Node4 required for Node3*/ {2, 4}};
  const std::vector<int> kExpectedCost = {
      /*Node2-->Node3 on 1 vehicle, Node1 and Node4 on other*/ 50 + 40,
      /*Node4-->Node3 on 1 vehicle, Node1 and Node2 on other*/ 33 + 33};
  const std::vector<int> kExpectedNode3Predecessor = {2, 4};

  std::vector<std::vector<int64_t>> costs = {{0, 10, 15, 15, 10},
                                             {10, 0, 8, 22, 20},
                                             {15, 8, 0, 20, 22},
                                             {15, 22, 20, 0, 8},
                                             {10, 20, 22, 8, 0}};
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  for (bool temporal_requirements : {true, false}) {
    for (int r = 0; r < kNode3RequirementAlternatives.size(); r++) {
      const absl::flat_hash_set<int>& requirement_alternatives =
          kNode3RequirementAlternatives[r];
      Model model(manager);
      const int vehicle_cost = model.RegisterTransitCallback(
          absl::bind_front(GetCostFromMatrix, manager, costs));
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
      model.AddConstantDimension(1, 3, /*fix_start_cumul_to_zero*/ true, "dim");
      for (int i = 1; i < kNumNodes; i++) {
        const int64_t index = manager.NodeToIndex(NodeIndex(i));
        model.SetVisitType(index, i, Model::TYPE_ADDED_TO_VEHICLE);
        model.AddDisjunction({index}, kPenalty, 1);
      }

      if (temporal_requirements) {
        model.AddRequiredTypeAlternativesWhenAddingType(
            3, requirement_alternatives);
      } else {
        model.AddSameVehicleRequiredTypeAlternatives(3,
                                                     requirement_alternatives);
      }
      const Assignment* const assignment = model.Solve();
      ASSERT_NE(assignment, nullptr);
      EXPECT_TRUE(model.IsVehicleUsed(*assignment, 0));
      EXPECT_TRUE(model.IsVehicleUsed(*assignment, 1));
      EXPECT_EQ(assignment->ObjectiveValue(), kExpectedCost[r]);
      EXPECT_EQ(assignment->Value(model.NextVar(kExpectedNode3Predecessor[r])),
                3);
    }
  }
}

// Unit test to verify that when a requirement with an empty set of required
// type alternatives is added, the node with dependent type is unperformed iff
// its policy makes the requirement active.
TEST(NodeTypeRequirementsTest, EmptyRequirementSet) {
  const IndexManager manager(2, 1, NodeIndex(0));

  absl::flat_hash_map<std::string, absl::flat_hash_set<VisitTypePolicy>>
      requirement_mode_to_expected_performed_policies;
  requirement_mode_to_expected_performed_policies["same_vehicle"] = {
      Model::ADDED_TYPE_REMOVED_FROM_VEHICLE};
  requirement_mode_to_expected_performed_policies["when_added"] = {
      Model::ADDED_TYPE_REMOVED_FROM_VEHICLE,
      Model::TYPE_ON_VEHICLE_UP_TO_VISIT};
  requirement_mode_to_expected_performed_policies["when_removed"] = {
      Model::TYPE_ADDED_TO_VEHICLE};

  for (VisitTypePolicy type_policy :
       {Model::TYPE_ADDED_TO_VEHICLE, Model::ADDED_TYPE_REMOVED_FROM_VEHICLE,
        Model::TYPE_ON_VEHICLE_UP_TO_VISIT,
        Model::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED}) {
    for (const std::string& requirement_mode :
         {"same_vehicle", "when_added", "when_removed"}) {
      Model model(manager);

      const int kPenaltyCost = 1000;
      const int index = 1;
      model.AddDisjunction({index}, kPenaltyCost);
      const int dependent_type = 123;
      model.SetVisitType(index, dependent_type, type_policy);

      if (requirement_mode == "same_vehicle") {
        model.AddSameVehicleRequiredTypeAlternatives(dependent_type, {});
      } else if (requirement_mode == "when_added") {
        model.AddRequiredTypeAlternativesWhenAddingType(dependent_type, {});
      } else {
        model.AddRequiredTypeAlternativesWhenRemovingType(dependent_type, {});
      }
      const Assignment* const assignment = model.Solve();
      ASSERT_NE(assignment, nullptr);

      if (requirement_mode_to_expected_performed_policies[requirement_mode]
              .contains(type_policy)) {
        EXPECT_TRUE(model.IsVehicleUsed(*assignment, 0));
        EXPECT_EQ(assignment->ObjectiveValue(), 0);
      } else {
        EXPECT_FALSE(model.IsVehicleUsed(*assignment, 0));
        EXPECT_EQ(assignment->ObjectiveValue(), kPenaltyCost);
      }
    }
  }
}

// Tests that two nodes with incompatible types can both be unperformed (i.e.
// can have equal vehicle_vars_ when inactive).
TEST(NodeTypeIncompatibilitiesTest, UnperformedNodesWithIncompatibleTypes) {
  const int kNumNodes = 3;
  const int kNumVehicles = 1;
  const int kPenalty = 1000;

  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  // Neither nodes can be performed.
  model.AddConstantDimension(1, /*capacity*/ 1,
                             /*fix_start_cumul_to_zero*/ true, "volume");

  for (int i = 1; i < kNumNodes; i++) {
    const int64_t index = manager.NodeToIndex(NodeIndex(i));
    model.SetVisitType(index, i, Model::TYPE_ADDED_TO_VEHICLE);
    model.AddDisjunction({index}, kPenalty, 1);
  }
  model.AddHardTypeIncompatibility(1, 2);
  const Assignment* const assignment = model.Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(assignment->ObjectiveValue(), kPenalty * 2);
}

TEST(InsertNodesInAssignmentWithParametersTest, EmptySolution) {
  const IndexManager manager(/*num_nodes=*/2, /*num_vehicles=*/2,
                             /*depot=*/NodeIndex(0));
  Model model(manager);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  Assignment* assignment = model.solver()->MakeAssignment();
  const int vehicle_cost =
      model.RegisterTransitCallback([](int64_t, int64_t) { return 1; });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  const Assignment* solution =
      model.InsertNodesInAssignmentWithParameters(*assignment, parameters);
  EXPECT_NE(solution, nullptr);
  EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_SUCCESS);
  EXPECT_EQ(solution->ObjectiveValue(), 2);
}

TEST(InsertNodesInAssignmentWithParametersTest, WithUnperformedSolution) {
  const IndexManager manager(/*num_nodes=*/3, /*num_vehicles=*/2,
                             /*depot=*/NodeIndex(0));
  Model model(manager);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  Assignment* assignment = model.solver()->MakeAssignment();
  // 1 is unperformed.
  assignment->Add(model.NextVar(1))->SetValue(1);
  assignment->Add(model.NextVar(model.Start(0)))->SetValue(2);
  assignment->Add(model.NextVar(2))->SetValue(model.End(0));
  assignment->Add(model.NextVar(model.Start(1)))->SetValue(model.End(1));
  const int vehicle_cost =
      model.RegisterTransitCallback([](int64_t, int64_t) { return 1; });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  const Assignment* solution =
      model.InsertNodesInAssignmentWithParameters(*assignment, parameters);
  EXPECT_NE(solution, nullptr);
  EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_SUCCESS);
  EXPECT_EQ(solution->ObjectiveValue(), 3);
}

TEST(InsertNodesInAssignmentWithParametersTest, Timeout) {
  const IndexManager manager(/*num_nodes=*/2, /*num_vehicles=*/2,
                             /*depot=*/NodeIndex(0));
  Model model(manager);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.mutable_time_limit()->set_nanos(1);
  Assignment* assignment = model.solver()->MakeAssignment();
  const int vehicle_cost =
      model.RegisterTransitCallback([](int64_t, int64_t) { return 1; });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  const Assignment* solution =
      model.InsertNodesInAssignmentWithParameters(*assignment, parameters);
  EXPECT_EQ(solution, nullptr);
}

TEST(InsertNodesInAssignmentWithParametersTest, MalformedCycleSolution) {
  const IndexManager manager(/*num_nodes=*/4, /*num_vehicles=*/2,
                             /*depot=*/NodeIndex(0));
  Model model(manager);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  // Cycle: 0->1->2->3->1
  Assignment* assignment = model.solver()->MakeAssignment();
  assignment->Add(model.NextVar(model.Start(0)))->SetValue(1);
  assignment->Add(model.NextVar(1))->SetValue(2);
  assignment->Add(model.NextVar(2))->SetValue(3);
  assignment->Add(model.NextVar(3))->SetValue(1);
  const Assignment* solution =
      model.InsertNodesInAssignmentWithParameters(*assignment, parameters);
  EXPECT_EQ(solution, nullptr);
  EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_NOT_SOLVED);
}

TEST(InsertNodesInAssignmentWithParametersTest, MalformedDisconnectedSolution) {
  const IndexManager manager(/*num_nodes=*/4, /*num_vehicles=*/2,
                             /*depot=*/NodeIndex(0));
  Model model(manager);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  // Disconnected chain: 1->2->3
  Assignment* assignment = model.solver()->MakeAssignment();
  assignment->Add(model.NextVar(1))->SetValue(2);
  assignment->Add(model.NextVar(2))->SetValue(3);
  const Assignment* solution =
      model.InsertNodesInAssignmentWithParameters(*assignment, parameters);
  EXPECT_EQ(solution, nullptr);
  EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_NOT_SOLVED);
}

TEST(InsertNodesInAssignmentWithParametersTest, PartialSolution) {
  const IndexManager manager(/*num_nodes=*/4, /*num_vehicles=*/2,
                             /*depot=*/NodeIndex(0));
  for (bool closed : {true, false}) {
    Model model(manager);
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    // Insert 3 in 0->1->5 and 4->2->6
    Assignment* assignment = model.solver()->MakeAssignment();
    assignment->Add(model.NextVar(model.Start(0)))->SetValue(1);
    assignment->Add(model.NextVar(1))->SetValue(model.End(0));
    assignment->Add(model.NextVar(model.Start(1)))->SetValue(2);
    if (closed) {
      assignment->Add(model.NextVar(2))->SetValue(model.End(1));
    }
    const int vehicle_cost = model.RegisterTransitCallback(
        [](int64_t i, int64_t j) { return std::abs(i - j); });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    const Assignment* solution =
        model.InsertNodesInAssignmentWithParameters(*assignment, parameters);
    EXPECT_NE(solution, nullptr);
    EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_SUCCESS);
    EXPECT_TRUE(solution->Contains(model.NextVar(3)));
    EXPECT_NE(solution->Value(model.NextVar(3)), 3);
    EXPECT_EQ(solution->ObjectiveValue(), 1 + 4 + 1 + 1 + 4);
  }
}

TEST(InsertNodesInAssignmentWithParametersTest, FullSolution) {
  const IndexManager manager(/*num_nodes=*/3, /*num_vehicles=*/2,
                             /*depot=*/NodeIndex(0));
  Model model(manager);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  // 0->1->4 and 3->2->5
  Assignment* assignment = model.solver()->MakeAssignment();
  assignment->Add(model.NextVar(model.Start(0)))->SetValue(1);
  assignment->Add(model.NextVar(1))->SetValue(model.End(0));
  assignment->Add(model.NextVar(model.Start(1)))->SetValue(2);
  assignment->Add(model.NextVar(2))->SetValue(model.End(1));
  const int vehicle_cost = model.RegisterTransitCallback(
      [](int64_t i, int64_t j) { return std::abs(i - j); });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  const Assignment* solution =
      model.InsertNodesInAssignmentWithParameters(*assignment, parameters);
  EXPECT_NE(solution, nullptr);
  EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_SUCCESS);
  for (IntVar* next_var : model.Nexts()) {
    EXPECT_EQ(solution->Value(next_var), assignment->Value(next_var));
  }
  EXPECT_EQ(solution->ObjectiveValue(), 1 + 3 + 1 + 3);
}

TEST(InsertNodesInAssignmentWithParametersTest, PartiallyFixedSolution) {
  const IndexManager manager(/*num_nodes=*/4, /*num_vehicles=*/1,
                             /*depot=*/NodeIndex(0));
  const std::vector<int64_t> expected_objective_values = {
      /*fixed_chain_length=0*/ 4,
      /*fixed_chain_length=1*/ 6,
      /*fixed_chain_length=2*/ 8,
      /*fixed_chain_length=3*/ kint64max,
  };
  for (int fixed_chain_length = 0; fixed_chain_length < 4;
       ++fixed_chain_length) {
    Model model(manager);
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    // Insert 1 in 0->2->3->4
    Assignment* assignment = model.solver()->MakeAssignment();
    assignment->Add(model.NextVar(model.Start(0)))->SetValue(2);
    assignment->Add(model.NextVar(2))->SetValue(3);
    assignment->Add(model.NextVar(3))->SetValue(model.End(0));
    Assignment* pre_assignment = model.MutablePreAssignment();
    if (fixed_chain_length > 0) {
      pre_assignment->Add(model.NextVar(model.Start(0)))->SetValue(2);
    }
    if (fixed_chain_length > 1) {
      pre_assignment->Add(model.NextVar(2))->SetValue(3);
    }
    if (fixed_chain_length > 2) {
      pre_assignment->Add(model.NextVar(3))->SetValue(model.End(0));
    }
    const int vehicle_cost = model.RegisterTransitCallback(
        [](int64_t i, int64_t j) { return std::abs(i - j); });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    const Assignment* solution =
        model.InsertNodesInAssignmentWithParameters(*assignment, parameters);
    if (expected_objective_values[fixed_chain_length] != kint64max) {
      EXPECT_NE(solution, nullptr);
      EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_SUCCESS);
      EXPECT_TRUE(solution->Contains(model.NextVar(1)));
      EXPECT_EQ(solution->ObjectiveValue(),
                expected_objective_values[fixed_chain_length]);
    } else {
      EXPECT_EQ(solution, nullptr);
      EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_FAIL);
    }
  }
}

TEST(InsertNodesInAssignmentWithParametersTest, NonConsistentFixedSolution) {
  const IndexManager manager(/*num_nodes=*/5, /*num_vehicles=*/1,
                             /*depot=*/NodeIndex(0));
  Model model(manager);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  // Insert 1 in 0->2->3->5 with pre_assignment 0->4->5
  Assignment* assignment = model.solver()->MakeAssignment();
  assignment->Add(model.NextVar(model.Start(0)))->SetValue(2);
  assignment->Add(model.NextVar(2))->SetValue(3);
  assignment->Add(model.NextVar(3))->SetValue(model.End(0));
  Assignment* pre_assignment = model.MutablePreAssignment();
  pre_assignment->Add(model.NextVar(model.Start(0)))->SetValue(4);
  pre_assignment->Add(model.NextVar(4))->SetValue(model.End(0));
  const int vehicle_cost = model.RegisterTransitCallback(
      [](int64_t i, int64_t j) { return std::abs(i - j); });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  const Assignment* solution =
      model.InsertNodesInAssignmentWithParameters(*assignment, parameters);
  EXPECT_EQ(solution, nullptr);
  EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_FAIL);
}

TEST(InsertNodesInAssignmentWithParametersTest, ConsistentFixedSolution) {
  const IndexManager manager(/*num_nodes=*/5, /*num_vehicles=*/1,
                             /*depot=*/NodeIndex(0));
  Model model(manager);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  // Insert 1 in 0->2->3->5 with pre_assignment 0->4->2
  Assignment* assignment = model.solver()->MakeAssignment();
  assignment->Add(model.NextVar(model.Start(0)))->SetValue(2);
  assignment->Add(model.NextVar(2))->SetValue(3);
  assignment->Add(model.NextVar(3))->SetValue(model.End(0));
  Assignment* pre_assignment = model.MutablePreAssignment();
  pre_assignment->Add(model.NextVar(model.Start(0)))->SetValue(4);
  pre_assignment->Add(model.NextVar(4))->SetValue(2);
  const int vehicle_cost = model.RegisterTransitCallback(
      [](int64_t i, int64_t j) { return std::abs(i - j); });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  const Assignment* solution =
      model.InsertNodesInAssignmentWithParameters(*assignment, parameters);
  EXPECT_NE(solution, nullptr);
  EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_SUCCESS);
  EXPECT_TRUE(solution->Contains(model.NextVar(1)));
  EXPECT_EQ(solution->ObjectiveValue(), 4 + 2 + 1 + 2 + 2);
}

TEST(FastSolveFromAssignmentWithParameters, EmptySolution) {
  const IndexManager manager(/*num_nodes=*/2, /*num_vehicles=*/2,
                             /*depot=*/NodeIndex(0));
  Model model(manager);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  absl::flat_hash_set<IntVar*> touched;
  const Assignment* solution = model.FastSolveFromAssignmentWithParameters(
      /*assignment=*/nullptr, parameters, /*check_solution_in_cp=*/false,
      &touched);
  EXPECT_EQ(solution, nullptr);
  EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_NOT_SOLVED);
  EXPECT_TRUE(touched.empty());
}

TEST(FastSolveFromAssignmentWithParameters, ImproveSolution) {
  for (int64_t solution_limit :
       {int64_t{1}, int64_t{2}, int64_t{100}, kint64max}) {
    const IndexManager manager(/*num_nodes=*/4, /*num_vehicles=*/2,
                               /*depot=*/NodeIndex(0));
    Model model(manager);
    for (int i = 1; i < 4; ++i) {
      model.AddDisjunction(manager.NodesToIndices({NodeIndex(i)}), 10);
    }
    // Only 2 nodes per route
    model.AddConstantDimension(/*value=*/1, /*capacity=*/3,
                               /*fix_start_cumul_to_zero=*/true, "dim");
    // Vehicle 1 is cheaper than vehicle 0.
    model.GetMutableDimension("dim")->SetSpanCostCoefficientForVehicle(
        /*coefficient=*/2, /*vehicle=*/0);
    model.GetMutableDimension("dim")->SetSpanCostCoefficientForVehicle(
        /*coefficient=*/1, /*vehicle=*/1);
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_solution_limit(solution_limit);
    model.CloseModelWithParameters(parameters);

    Assignment* initial_solution = model.solver()->MakeAssignment();
    initial_solution->AddObjective(model.CostVar());
    initial_solution->SetObjectiveValue(10 + 4 + 2);
    // First solution routes:
    // 0 -> 1 -> 5
    // 4 -> 3 -> 6
    // 2 unperformed
    initial_solution->Add(model.NextVar(0))->SetValue(1);
    initial_solution->Add(model.NextVar(1))->SetValue(5);
    initial_solution->Add(model.NextVar(2))->SetValue(2);
    initial_solution->Add(model.NextVar(3))->SetValue(6);
    initial_solution->Add(model.NextVar(4))->SetValue(3);
    // Vehicles have to be specified since the vehicles are in different
    // classes.
    initial_solution->Add(model.VehicleVar(0))->SetValue(0);
    initial_solution->Add(model.VehicleVar(1))->SetValue(0);
    initial_solution->Add(model.VehicleVar(2))->SetValue(-1);
    initial_solution->Add(model.VehicleVar(3))->SetValue(1);
    initial_solution->Add(model.VehicleVar(4))->SetValue(1);
    initial_solution->Add(model.VehicleVar(5))->SetValue(1);
    initial_solution->Add(model.VehicleVar(6))->SetValue(1);
    absl::flat_hash_set<IntVar*> touched;
    const Assignment* solution = model.FastSolveFromAssignmentWithParameters(
        initial_solution, parameters, /*check_solution_in_cp=*/false, &touched);
    ASSERT_NE(solution, nullptr);
    switch (solution_limit) {
      case 1:
        // Initial solution.
        EXPECT_EQ(solution->ObjectiveValue(), 10 + 4 + 2)
            << solution->DebugString();
        EXPECT_EQ(0, touched.size());
        break;
      case 2:
        // Node on vehicle 0 moved to vehicle 1.
        EXPECT_EQ(solution->ObjectiveValue(), 10 + 3)
            << solution->DebugString();
        EXPECT_EQ(6, touched.size());
        break;
      case 100:
        [[fallthrough]];
      case kint64max:
        // One node on vehicle 0, the other two on vehicle 1.
        EXPECT_EQ(solution->ObjectiveValue(), 4 + 3) << solution->DebugString();
        EXPECT_EQ(8, touched.size());
        break;
      default:
        break;
    }
  }
}

// Builds a model with 2 vehicles and 1 non start-end node. The first vehicle
// has a fixed cost of 500, the second 100, and the node has a penalty cost of
// 2.
std::unique_ptr<Model> BuildModelForSecondaryLSPhaseTest() {
  const IndexManager manager(2, 2, NodeIndex(0));
  const int kVehicleFixedCosts[] = {500, 100};

  auto model = std::make_unique<Model>(manager);
  EXPECT_EQ(model->vehicles(), std::size(kVehicleFixedCosts));
  for (int v = 0; v < model->vehicles(); ++v) {
    model->SetFixedCostOfVehicle(kVehicleFixedCosts[v], v);
  }
  model->AddDisjunction(manager.NodesToIndices({NodeIndex(1)}), 2);

  return model;
}

// Builds parameters for the SecondaryLSPhaseTests below. The only local search
// operators active are relocate and make inactive, and the solution limit is
// set to 2.
RoutingSearchParameters BuildSearchParametersForSecondaryLSPhaseTest(
    double secondary_ls_time_limit_ratio, bool set_time_limit = true) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_solution_limit(2);
  parameters.set_secondary_ls_time_limit_ratio(secondary_ls_time_limit_ratio);
  if (set_time_limit) {
    // The secondary_ls_time_limit_ratio only applies when the time limit
    // is finite.
    parameters.mutable_time_limit()->set_seconds(1);
  }
  DisableAllLocalSearchOperators(parameters.mutable_local_search_operators());
  parameters.mutable_local_search_operators()->set_use_relocate(BOOL_TRUE);
  parameters.mutable_local_search_operators()->set_use_make_inactive(BOOL_TRUE);

  return parameters;
}

// The "SecondaryLSPhaseTest"s verify that the secondary LS phase with a
// restricted number of cleanup neighborhood operators runs correctly and works
// as intended.
//
// Tests that the secondary LS phase runs as expected. In the first solution
// given to the solver, the node is put on the first (more expensive vehicle).
// During the "primary" LS phase, the first move is to relocate the node to the
// second vehicle (less expensive), and the search stops there because the limit
// of 2 moves is reached. When the secondary LS phase is active, this solution
// is further improved by making the node inactive.
TEST(SecondaryLSPhaseTest, ProperlyRun) {
  for (double secondary_ls_time_limit_ratio : {0.0, 0.5}) {
    for (const bool set_time_limit : {true, false}) {
      const RoutingSearchParameters parameters =
          BuildSearchParametersForSecondaryLSPhaseTest(
              secondary_ls_time_limit_ratio, set_time_limit);
      std::unique_ptr<Model> model = BuildModelForSecondaryLSPhaseTest();

      // In the first solution the node is served by the first vehicle, with a
      // cost of 500.
      const Assignment* const solution =
          model->SolveFromAssignmentWithParameters(
              BuildAssignmentFromSolution(*model, {1, 3, 4}), parameters);
      if (secondary_ls_time_limit_ratio > 0 && set_time_limit) {
        // The node is made inactive during the secondary LS phase.
        EXPECT_EQ(solution->ObjectiveValue(), 2);
      } else {
        // The node is moved to the second (less expensive) vehicle.
        EXPECT_EQ(solution->ObjectiveValue(), 100);
      }
    }
  }
}

// The following test has the same settings as before, and tests that solutions
// are properly reported by the solver with number_of_solutions_to_collect = 4.
// As previously, the first solution puts the node on the first vehicle.
// With the solution_limit set to 2 in the parameters, in practice there are
// only 2 solutions when the secondary LS phase isn't active, and 3 with the
// secondary LS phase (as the first solution from the secondary LS is the last
// solution from the first phase).
TEST(SecondaryLSPhaseTest, MultipleSolutions) {
  const int kExpectedSolutionCosts[] = {500, 100, 2};
  for (double secondary_ls_time_limit_ratio : {0.0, 0.5}) {
    RoutingSearchParameters parameters =
        BuildSearchParametersForSecondaryLSPhaseTest(
            secondary_ls_time_limit_ratio);
    parameters.set_number_of_solutions_to_collect(4);
    std::unique_ptr<Model> model = BuildModelForSecondaryLSPhaseTest();

    std::vector<const Assignment*> solutions;
    model->SolveFromAssignmentWithParameters(
        BuildAssignmentFromSolution(*model, {1, 3, 4}), parameters, &solutions);
    const int kExpectedNumSolutions = secondary_ls_time_limit_ratio > 0 ? 3 : 2;
    ASSERT_EQ(solutions.size(), kExpectedNumSolutions);
    for (int i = 0; i < kExpectedNumSolutions; ++i) {
      EXPECT_EQ(solutions[i]->ObjectiveValue(), kExpectedSolutionCosts[i]);
    }
  }
}

using CostClassTest = ::testing::TestWithParam<bool>;
TEST_P(CostClassTest, NoCost) {
  const int kNodes = 3;
  const int kVehicles = 3;
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  model.CloseModel();
  EXPECT_EQ(1, model.GetCostClassesCount());
  EXPECT_EQ(0, model.GetNonZeroCostClassesCount());
  for (int i = 0; i < kNodes; ++i) {
    for (int j = 0; j < kNodes; ++j) {
      for (int k = 0; k < kVehicles; ++k) {
        EXPECT_EQ(0, model.GetArcCostForVehicle(i, j, k));
      }
    }
  }
}

TEST_P(CostClassTest, ArcCost) {
  const int kNodes = 3;
  const int kVehicles = 3;
  const int64_t kCost = 1LL;
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  const int vehicle_cost =
      GetParam()
          ? model.RegisterUnaryTransitCallback([](int64_t) { return kCost; })
          : model.RegisterTransitCallback(
                [](int64_t, int64_t) { return kCost; });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  model.CloseModel();
  EXPECT_EQ(2, model.GetCostClassesCount());
  EXPECT_EQ(1, model.GetNonZeroCostClassesCount());
  for (int i = 0; i < kNodes; ++i) {
    for (int j = 0; j < kNodes; ++j) {
      if (i != j) {
        for (int k = 0; k < kVehicles; ++k) {
          EXPECT_EQ(kCost, model.GetArcCostForVehicle(i, j, k));
        }
      }
    }
  }
}

// All vehicles except one have equivalent callbacks.
TEST_P(CostClassTest, EquivalentArcCost) {
  const int kNodes = 3;
  const int kVehicles = 4;
  const int64_t kCost = 1LL;
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  const int vehicle_cost =
      GetParam() ? model.RegisterUnaryTransitCallback(
                       [](int64_t) { return kCost + 1; })
                 : model.RegisterTransitCallback(
                       [](int64_t, int64_t) { return kCost + 1; });
  model.SetArcCostEvaluatorOfVehicle(vehicle_cost, 0);
  const int vehicle_cost_2 =
      GetParam()
          ? model.RegisterUnaryTransitCallback([](int64_t) { return kCost; })
          : model.RegisterTransitCallback(
                [](int64_t, int64_t) { return kCost; });
  for (int vehicle = 1; vehicle < kVehicles; ++vehicle) {
    model.SetArcCostEvaluatorOfVehicle(vehicle_cost_2, vehicle);
  }
  model.CloseModelWithParameters(parameters);
  EXPECT_EQ(3, model.GetCostClassesCount());
  EXPECT_EQ(2, model.GetNonZeroCostClassesCount());
  for (int i = 0; i < kNodes; ++i) {
    for (int j = 0; j < kNodes; ++j) {
      if (i != j) {
        EXPECT_EQ(kCost + 1, model.GetArcCostForVehicle(i, j, 0));
        for (int k = 1; k < kVehicles; ++k) {
          EXPECT_EQ(kCost, model.GetArcCostForVehicle(i, j, k));
        }
      }
    }
  }
}

TEST_P(CostClassTest, DimensionSpanCost) {
  const int kNodes = 3;
  const int kVehicles = 3;
  const int64_t kCost = 1LL;
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  const int callback =
      GetParam()
          ? model.RegisterUnaryTransitCallback([](int64_t) { return kCost; })
          : model.RegisterTransitCallback(
                [](int64_t, int64_t) { return kCost; });
  model.AddDimension(callback, kint64max, kint64max, true, "dimension");
  Dimension* dimension = model.GetMutableDimension("dimension");
  dimension->SetSpanCostCoefficientForVehicle(kCost, 1);
  model.CloseModel();
  EXPECT_EQ(2, model.GetCostClassesCount());
  EXPECT_EQ(1, model.GetNonZeroCostClassesCount());
}

TEST_P(CostClassTest, DimensionSlackCost) {
  const int kNodes = 3;
  const int kVehicles = 3;
  const int64_t kCost = 1LL;
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  const int callback =
      GetParam()
          ? model.RegisterUnaryTransitCallback([](int64_t) { return kCost; })
          : model.RegisterTransitCallback(
                [](int64_t, int64_t) { return kCost; });
  model.AddDimension(callback, kint64max, kint64max, true, "dimension");
  Dimension* dimension = model.GetMutableDimension("dimension");
  dimension->SetSlackCostCoefficientForVehicle(kCost, 1);
  model.CloseModel();
  EXPECT_EQ(2, model.GetCostClassesCount());
  EXPECT_EQ(1, model.GetNonZeroCostClassesCount());
}

TEST_P(CostClassTest, DimensionSpanAndSlackCost) {
  const int kNodes = 3;
  const int kVehicles = 3;
  const int64_t kCost = 1LL;
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  const int callback =
      GetParam()
          ? model.RegisterUnaryTransitCallback([](int64_t) { return kCost; })
          : model.RegisterTransitCallback(
                [](int64_t, int64_t) { return kCost; });
  model.AddDimension(callback, kint64max, kint64max, true, "dimension");
  Dimension* dimension = model.GetMutableDimension("dimension");
  dimension->SetSpanCostCoefficientForVehicle(kCost, 0);
  dimension->SetSlackCostCoefficientForVehicle(kCost, 1);
  model.CloseModel();
  EXPECT_EQ(3, model.GetCostClassesCount());
  EXPECT_EQ(2, model.GetNonZeroCostClassesCount());
}

TEST_P(CostClassTest, DimensionVehicleDependentSpanCost) {
  const int kNodes = 3;
  const int kVehicles = 3;
  const int64_t kCost = 1LL;
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  const int callback =
      GetParam()
          ? model.RegisterUnaryTransitCallback([](int64_t) { return kCost; })
          : model.RegisterTransitCallback(
                [](int64_t, int64_t) { return kCost; });
  model.AddDimension(callback, kint64max, kint64max, true, "dimension");
  Dimension* dimension = model.GetMutableDimension("dimension");
  for (int i = 0; i < kVehicles; ++i) {
    dimension->SetSpanCostCoefficientForVehicle(1 + i % 2, i);
  }
  model.CloseModel();
  EXPECT_EQ(3, model.GetCostClassesCount());
  EXPECT_EQ(2, model.GetNonZeroCostClassesCount());
}

TEST_P(CostClassTest, VehicleDependentDimensionSpanCost) {
  const int64_t kCost = 1LL;
  const int kNodes = 3;
  const int kVehicles = 3;
  const int kEvaluators = 2;
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  const int64_t v0_node0 = manager.NodeToIndex(NodeIndex(1));
  const int64_t v1_node0 = manager.NodeToIndex(NodeIndex(2));
  model.NextVar(model.Start(0))->SetValue(v0_node0);
  model.NextVar(v0_node0)->SetValue(model.End(0));
  model.NextVar(model.Start(1))->SetValue(v1_node0);
  model.NextVar(v1_node0)->SetValue(model.End(1));
  model.NextVar(model.Start(2))->SetValue(model.End(2));
  std::vector<int> evaluators(kEvaluators, -1);
  for (int i = 0; i < kEvaluators; ++i) {
    evaluators[i] = GetParam()
                        ? model.RegisterUnaryTransitCallback(
                              [i](int64_t) { return 1 + i % kEvaluators; })
                        : model.RegisterTransitCallback([i](int64_t, int64_t) {
                            return 1 + i % kEvaluators;
                          });
  }
  std::vector<int> transits(kVehicles, -1);
  for (int i = 0; i < kVehicles; ++i) {
    transits[i] = evaluators[i % kEvaluators];
  }
  model.AddDimensionWithVehicleTransits(transits, kint64max, kint64max, true,
                                        "dimension");
  Dimension* dimension = model.GetMutableDimension("dimension");
  dimension->SetSpanCostCoefficientForAllVehicles(kCost);
  model.AddToAssignment(dimension->TransitVar(model.Start(0)));
  model.AddToAssignment(dimension->TransitVar(model.Start(1)));
  model.AddToAssignment(dimension->TransitVar(model.Start(2)));
  model.AddToAssignment(dimension->TransitVar(v0_node0));
  model.AddToAssignment(dimension->TransitVar(v1_node0));
  const Assignment* assignment = model.Solve();
  EXPECT_EQ(kEvaluators + 1, model.GetCostClassesCount());
  EXPECT_EQ(kEvaluators, model.GetNonZeroCostClassesCount());
  EXPECT_EQ(1, assignment->Value(dimension->TransitVar(model.Start(0))));
  EXPECT_EQ(1, assignment->Value(dimension->TransitVar(v0_node0)));
  EXPECT_EQ(2, assignment->Value(dimension->TransitVar(model.Start(1))));
  EXPECT_EQ(2, assignment->Value(dimension->TransitVar(v1_node0)));
  EXPECT_EQ(1, assignment->Value(dimension->TransitVar(model.Start(2))));
}

INSTANTIATE_TEST_SUITE_P(WithOrWithoutUnaryCallback, CostClassTest,
                         ::testing::Bool());

using VehicleClassTest = ::testing::TestWithParam<bool>;
TEST_P(VehicleClassTest, NoVehicles) {
  IndexManager manager(1, 0, NodeIndex(0));
  Model model(manager);
  model.CloseModel();
  EXPECT_EQ(0, model.GetVehicleClassesCount());
}

TEST_P(VehicleClassTest, SingleVehicleClass) {
  IndexManager manager(4, 3, NodeIndex(0));
  Model model(manager);
  model.CloseModel();
  EXPECT_EQ(1, model.GetVehicleClassesCount());
  for (int vehicle = 0; vehicle < model.vehicles(); ++vehicle) {
    EXPECT_THAT(model.GetVehiclesOfSameClass(model.Start(vehicle)),
                UnorderedElementsAre(0, 1, 2));
    EXPECT_THAT(model.GetVehiclesOfSameClass(model.End(vehicle)),
                UnorderedElementsAre(0, 1, 2));
  }
  EXPECT_THAT(model.GetSameVehicleClassArcs(0, 6),
              UnorderedElementsAre(testing::Pair(0, 6), testing::Pair(4, 7),
                                   testing::Pair(5, 8)));
  EXPECT_THAT(model.GetSameVehicleClassArcs(4, 7),
              UnorderedElementsAre(testing::Pair(0, 6), testing::Pair(4, 7),
                                   testing::Pair(5, 8)));
  EXPECT_THAT(model.GetSameVehicleClassArcs(0, 1),
              UnorderedElementsAre(testing::Pair(0, 1), testing::Pair(4, 1),
                                   testing::Pair(5, 1)));
  EXPECT_THAT(model.GetSameVehicleClassArcs(4, 1),
              UnorderedElementsAre(testing::Pair(0, 1), testing::Pair(4, 1),
                                   testing::Pair(5, 1)));
  EXPECT_THAT(model.GetSameVehicleClassArcs(5, 1),
              UnorderedElementsAre(testing::Pair(0, 1), testing::Pair(4, 1),
                                   testing::Pair(5, 1)));
  EXPECT_THAT(model.GetSameVehicleClassArcs(1, 6),
              UnorderedElementsAre(testing::Pair(1, 6), testing::Pair(1, 7),
                                   testing::Pair(1, 8)));
  EXPECT_THAT(model.GetSameVehicleClassArcs(1, 7),
              UnorderedElementsAre(testing::Pair(1, 6), testing::Pair(1, 7),
                                   testing::Pair(1, 8)));
  EXPECT_THAT(model.GetSameVehicleClassArcs(1, 8),
              UnorderedElementsAre(testing::Pair(1, 6), testing::Pair(1, 7),
                                   testing::Pair(1, 8)));
}

TEST_P(VehicleClassTest, FixedCost) {
  IndexManager manager(4, 3, NodeIndex(0));
  Model model(manager);
  model.SetFixedCostOfVehicle(1, 0);
  model.SetFixedCostOfVehicle(2, 2);
  model.CloseModel();
  EXPECT_EQ(3, model.GetVehicleClassesCount());
  for (int vehicle = 0; vehicle < model.vehicles(); ++vehicle) {
    EXPECT_THAT(model.GetVehiclesOfSameClass(model.Start(vehicle)),
                UnorderedElementsAre(vehicle));
    EXPECT_THAT(model.GetVehiclesOfSameClass(model.End(vehicle)),
                UnorderedElementsAre(vehicle));
    for (int from = 0; from < model.Nexts().size(); ++from) {
      IntVar* const next = model.NextVar(from);
      for (int to = next->Min(); to != next->Max(); ++to) {
        if (model.VehicleIndex(from) != model.VehicleIndex(to) &&
            model.VehicleIndex(from) != -1 && model.VehicleIndex(to) != -1) {
          continue;
        }
        EXPECT_THAT(model.GetSameVehicleClassArcs(from, to),
                    UnorderedElementsAre(testing::Pair(from, to)));
      }
    }
  }
}

TEST_P(VehicleClassTest, UsedWhenEmpty) {
  IndexManager manager(4, 3, NodeIndex(0));
  Model model(manager);
  model.SetVehicleUsedWhenEmpty(true, 0);
  model.SetVehicleUsedWhenEmpty(false, 2);
  model.CloseModel();
  EXPECT_EQ(2, model.GetVehicleClassesCount());
  for (int vehicle : {0}) {
    EXPECT_THAT(model.GetVehiclesOfSameClass(model.Start(vehicle)),
                UnorderedElementsAre(vehicle));
    EXPECT_THAT(model.GetVehiclesOfSameClass(model.End(vehicle)),
                UnorderedElementsAre(vehicle));
    IntVar* const next = model.NextVar(model.Start(vehicle));
    for (int to = next->Min(); to != next->Max(); ++to) {
      if (vehicle != model.VehicleIndex(to) && model.VehicleIndex(to) != -1) {
        continue;
      }
      EXPECT_THAT(
          model.GetSameVehicleClassArcs(model.Start(vehicle), to),
          UnorderedElementsAre(testing::Pair(model.Start(vehicle), to)));
    }
  }
  for (int vehicle : {1, 2}) {
    EXPECT_THAT(model.GetVehiclesOfSameClass(model.Start(vehicle)),
                UnorderedElementsAre(1, 2));
    EXPECT_THAT(model.GetVehiclesOfSameClass(model.End(vehicle)),
                UnorderedElementsAre(1, 2));
  }
  EXPECT_THAT(model.GetSameVehicleClassArcs(4, 7),
              UnorderedElementsAre(testing::Pair(4, 7), testing::Pair(5, 8)));
  EXPECT_THAT(model.GetSameVehicleClassArcs(4, 1),
              UnorderedElementsAre(testing::Pair(4, 1), testing::Pair(5, 1)));
  EXPECT_THAT(model.GetSameVehicleClassArcs(5, 1),
              UnorderedElementsAre(testing::Pair(4, 1), testing::Pair(5, 1)));
  EXPECT_THAT(model.GetSameVehicleClassArcs(1, 7),
              UnorderedElementsAre(testing::Pair(1, 7), testing::Pair(1, 8)));
  EXPECT_THAT(model.GetSameVehicleClassArcs(1, 8),
              UnorderedElementsAre(testing::Pair(1, 7), testing::Pair(1, 8)));
}

TEST_P(VehicleClassTest, StartEnds) {
  const std::vector<NodeIndex> starts = {NodeIndex(0), NodeIndex(1),
                                         NodeIndex(1), NodeIndex(1)};
  const std::vector<NodeIndex> ends = {NodeIndex(0), NodeIndex(0), NodeIndex(2),
                                       NodeIndex(0)};
  IndexManager manager(5, 4, starts, ends);
  Model model(manager);
  model.CloseModel();
  EXPECT_EQ(3, model.GetVehicleClassesCount());
}

TEST_P(VehicleClassTest, StartEndCumuls) {
  const std::vector<int64_t> start_mins = {0, 0, 1, 1, 0, 0, 1, 1, 0, 1};
  const std::vector<int64_t> start_maxs = {0, 9, 0, 9, 0, 9, 0, 9, 0, 9};
  const std::vector<int64_t> end_mins = {7, 7, 7, 7, 8, 8, 8, 8, 7, 8};
  const std::vector<int64_t> end_maxs = {8, 8, 8, 8, 8, 8, 8, 9, 0, 9};
  IndexManager manager(15, 10, NodeIndex(0));
  Model model(manager);
  const int callback =
      GetParam()
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  model.AddDimension(callback, 0, kint64max,
                     /*fix_start_cumul_to_zero=*/false, "dimension");
  Dimension* const dimension = model.GetMutableDimension("dimension");
  for (int i = 0; i < model.vehicles(); ++i) {
    dimension->CumulVar(model.Start(i))->SetRange(start_mins[i], start_maxs[i]);
    dimension->CumulVar(model.End(i))->SetRange(end_mins[i], end_maxs[i]);
  }
  model.CloseModel();
  EXPECT_EQ(9, model.GetVehicleClassesCount());
}

// Vehicle 0 and 1 are equivalent, vehicle 2 is not equivalent to any other.
// This leads to 3 classes with the remaining unconstrained vehicles.
TEST_P(VehicleClassTest, IncompatibleNodes) {
  IndexManager manager(10, 7, NodeIndex(0));
  Model model(manager);
  // Removing from start node doesn't change equivalence.
  model.VehicleVar(model.Start(1))->RemoveValue(0);
  model.VehicleVar(9)->RemoveValue(0);
  model.VehicleVar(2)->RemoveValue(0);
  model.VehicleVar(9)->RemoveValue(1);
  model.VehicleVar(2)->RemoveValue(1);
  model.VehicleVar(2)->RemoveValue(2);
  model.CloseModel();
  EXPECT_EQ(3, model.GetVehicleClassesCount());
}

TEST_P(VehicleClassTest, DimensionCapacities) {
  const int64_t kCapacities[][3] = {{10, 10, 10}, {11, 10, 10}, {10, 11, 11}};
  IndexManager manager(5, 3, NodeIndex(0));
  Model model(manager);
  const int callback =
      GetParam()
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  for (int i = 0; i < std::size(kCapacities); ++i) {
    model.AddDimensionWithVehicleCapacity(
        callback, 0, std::vector<int64_t>(kCapacities[i], kCapacities[i] + 3),
        /*fix_start_cumul_to_zero=*/false, absl::StrCat("dimension", i));
  }
  model.CloseModel();
  EXPECT_EQ(2, model.GetVehicleClassesCount());
}

TEST_P(VehicleClassTest, DimensionEvaluators) {
  IndexManager manager(5, 3, NodeIndex(0));
  Model model(manager);
  const int callback1 =
      GetParam()
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  const int callback2 =
      GetParam()
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  std::vector<int> transit_callbacks = {callback1, callback2, callback1};
  model.AddDimensionWithVehicleTransits(transit_callbacks, 0, kint64max,
                                        /*fix_start_cumul_to_zero=*/false,
                                        "dimension1");
  model.AddDimension(callback1, 0, kint64max,
                     /*fix_start_cumul_to_zero=*/false, "dimension2");
  model.CloseModel();
  EXPECT_EQ(2, model.GetVehicleClassesCount());
}

TEST_P(VehicleClassTest, CumulDependentDimensionEvaluators) {
  const int kNumNodes = 5;
  const int kNumVehicles = 3;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  for (int i = 0; i < kNumVehicles; ++i) {
    for (int j = 0; j < kNumVehicles; ++j) {
      Model model(manager);
      const int fixed_callback1 =
          model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
      const int fixed_callback2 =
          model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });

      FloatSlopePiecewiseLinearFunction pwl({0, 1}, {0, 1});
      const int cd_callback1 = model.RegisterCumulDependentTransitCallback(
          [&pwl](int64_t, int64_t) { return &pwl; });
      const int cd_callback2 = model.RegisterCumulDependentTransitCallback(
          [&pwl](int64_t, int64_t) { return &pwl; });

      std::vector<int> fixed_transit_callbacks(kNumVehicles, fixed_callback1);
      fixed_transit_callbacks[i] = fixed_callback2;

      std::vector<int> cd_transit_callbacks(kNumVehicles, cd_callback1);
      cd_transit_callbacks[j] = cd_callback2;

      model.AddDimensionWithCumulDependentVehicleTransitAndCapacity(
          fixed_transit_callbacks, cd_transit_callbacks, 0,
          /*vehicle_capacities=*/std::vector<int64_t>(kNumVehicles, 1000),
          /*fix_start_cumul_to_zero=*/false, "dimension1");

      model.CloseModel();
      EXPECT_EQ(model.GetVehicleClassesCount(), i == j ? 2 : 3);
    }
  }
}

// Vehicles 0 and 2 require the first two resource groups and vehicle 3 requires
// all of them. The other vehicles don't require any resource.
TEST_P(VehicleClassTest, ResourceRequirements) {
  const IndexManager manager(10, 7, NodeIndex(0));
  Model model(manager);
  // Add 3 resource groups.
  model.AddResourceGroup();
  model.AddResourceGroup();
  model.AddResourceGroup();
  const std::vector<std::vector<int>> kVehiclesRequiringResource = {
      {0, 2, 3}, {0, 2, 3}, {3}};
  const auto& resource_groups = model.GetResourceGroups();
  ASSERT_EQ(kVehiclesRequiringResource.size(), resource_groups.size());
  for (int rg_index = 0; rg_index < resource_groups.size(); rg_index++) {
    Model::ResourceGroup* const resource_group =
        resource_groups[rg_index].get();
    for (int v : kVehiclesRequiringResource[rg_index]) {
      resource_group->NotifyVehicleRequiresAResource(v);
    }
  }
  model.CloseModel();
  EXPECT_EQ(model.GetVehicleClassesCount(), 3);
}

// 5 vehicles, 2 resource groups. All vehicles require both resource groups.
// - Vehicle #0 is only allowed resource #0 from the first resource group.
// - Vehicle #1 and #2 are both allowed resource #0 from the first resource
//   group, and resource #1 from the second resource group.
// - Vehicle #3 doesn't have any resources explicitly marked as allowed, and
//   Vehicle #4 explicitly marks them all as allowed.
TEST_P(VehicleClassTest, AllowedResources) {
  const int kNumNodes = 10;
  const int kNumVehicles = 5;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  const std::string kDimName = "dim";
  const int kNumResourceGroups = 3;
  const int kNumResourcesPerGroup = 2;
  const std::vector<std::vector<std::vector<int>>>
      kAllowedResourcesInGroupPerVehicle = {
          /*v0*/ {/*rg0*/ {0}, /*rg1*/ {}, /*rg2*/ {}},
          /*v1*/ {/*rg0*/ {0}, /*rg1*/ {1}, /*rg2*/ {}},
          /*v2*/ {/*rg0*/ {0}, /*rg1*/ {1}, /*rg2*/ {0, 1}},
          /*v3*/ {/*rg0*/ {}, /*rg1*/ {}, /*rg2*/ {}},
          /*v4*/ {/*rg0*/ {0, 1}, /*rg1*/ {0, 1}, /*rg2*/ {0, 1}}};
  ASSERT_EQ(kAllowedResourcesInGroupPerVehicle.size(), kNumVehicles);

  for (const bool set_values_on_resource_var : {true, false}) {
    Model model(manager);
    model.AddConstantDimension(1, 10, false, kDimName);
    const Dimension* const dim = model.GetMutableDimension(kDimName);

    for (int rg = 0; rg < kNumResourceGroups; rg++) {
      ResourceGroup* const resource_group = model.AddResourceGroup();
      for (int r = 0; r < kNumResourcesPerGroup; r++) {
        resource_group->AddResource(Attributes(), dim);
      }
    }

    const auto& resource_groups = model.GetResourceGroups();
    for (int v = 0; v < model.vehicles(); v++) {
      const std::vector<std::vector<int>>& allowed_resources_in_group =
          kAllowedResourcesInGroupPerVehicle[v];
      ASSERT_EQ(allowed_resources_in_group.size(), resource_groups.size());
      for (int rg = 0; rg < resource_groups.size(); rg++) {
        resource_groups[rg]->NotifyVehicleRequiresAResource(v);

        const std::vector<int>& resources = allowed_resources_in_group[rg];
        if (resources.empty()) continue;
        if (set_values_on_resource_var) {
          const std::vector<int64_t> values(resources.begin(), resources.end());
          model.ResourceVar(v, rg)->SetValues(values);
        } else {
          resource_groups[rg]->SetAllowedResourcesForVehicle(v, resources);
        }
      }
    }
    model.CloseModel();
    EXPECT_EQ(model.GetVehicleClassesCount(), 3);
  }
}

INSTANTIATE_TEST_SUITE_P(WithOrWithoutUnaryCallback, VehicleClassTest,
                         ::testing::Bool());

TEST(ResourceClassTest, DifferentDimensionAttributes) {
  const IndexManager manager(10, 7, NodeIndex(0));
  Model model(manager);
  const std::string kDimName = "dim";
  model.AddConstantDimensionWithSlack(1, 1000, 1000, false, kDimName);
  const Dimension* const dim = model.GetMutableDimension(kDimName);

  // Add a resource group.
  ResourceGroup* const resource_group = model.AddResourceGroup();
  for (int v = 0; v < model.vehicles(); v++) {
    resource_group->NotifyVehicleRequiresAResource(v);
  }
  const std::vector<ResourceGroup::Attributes> kResourceAttributes = {
      Attributes(Domain::AllValues(), Domain::AllValues()),  // Class 0
      Attributes(Domain(10, 15), Domain::AllValues()),       // Class 1
      Attributes(Domain::AllValues(), Domain(10, 15)),       // Class 2
      Attributes(Domain(10, 15), Domain(10, 15)),            // Class 3
      Attributes(Domain(10, 15), Domain::AllValues()),       // Class 1
      Attributes(Domain(10, 15), Domain(10, 15)),            // Class 3
      Attributes(Domain::AllValues(), Domain(10, 15)),       // Class 2
      Attributes()                                           // Class 0
  };
  const std::vector<int> kExpectedResourceClassIndex = {0, 1, 2, 3, 1, 3, 2, 0};
  ASSERT_EQ(kResourceAttributes.size(), kExpectedResourceClassIndex.size());
  for (int r = 0; r < kResourceAttributes.size(); r++) {
    resource_group->AddResource(kResourceAttributes[r], dim);
  }
  const int kExpectedNumResourceClasses =
      *absl::c_max_element(kExpectedResourceClassIndex) + 1;

  model.CloseModel();
  EXPECT_EQ(resource_group->GetResourceClassesCount(),
            kExpectedNumResourceClasses);
  for (int r = 0; r < kExpectedResourceClassIndex.size(); r++) {
    EXPECT_EQ(resource_group->GetResourceClassIndex(r).value(),
              kExpectedResourceClassIndex[r]);
  }
}

TEST(ResourceClassTest, DifferentVehicleRequiredResources) {
  const IndexManager manager(10, 7, NodeIndex(0));
  Model model(manager);
  const std::string kDimName = "dim";
  model.AddConstantDimensionWithSlack(1, 1000, 1000, false, kDimName);
  const Dimension* const dim = model.GetMutableDimension(kDimName);

  // Add a resource group.
  ResourceGroup* const resource_group = model.AddResourceGroup();
  for (int r = 0; r < model.vehicles(); r++) {
    resource_group->AddResource(Attributes(), dim);
  }
  // Half the vehicles require a resource from this group. This doesn't
  // impact the number of resource classes.
  for (int v = 0; v < model.vehicles(); v++) {
    if (v <= model.vehicles() / 2) {
      resource_group->NotifyVehicleRequiresAResource(v);
    }
  }

  model.CloseModel();
  EXPECT_EQ(resource_group->GetResourceClassesCount(), 1);
}

template <bool set_values_on_resource_var>
void TestResourceClassesWithDifferentVehicleAllowedResources() {
  const int kNumNodes = 10;
  const int kNumVehicles = 5;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  const std::string kDimName = "dim";
  model.AddConstantDimensionWithSlack(1, 1000, 1000, false, kDimName);
  const Dimension* const dim = model.GetMutableDimension(kDimName);

  // Add a resource group.
  ResourceGroup* const resource_group = model.AddResourceGroup();
  const int kNumResources = 3;
  for (int r = 0; r < kNumResources; r++) {
    resource_group->AddResource(Attributes(), dim);
  }
  // Resource #0 and resource #2 are allowed for all vehicles except vehicle #3.
  // Resource #1 is allowed for all vehicles other than vehicles #0 and #2.
  using ResourceList =
      std::conditional<set_values_on_resource_var, std::vector<int64_t>,
                       std::vector<int>>::type;
  const std::vector<ResourceList> kAllowedResourcesPerVehicle = {
      {2, 0}, /*all*/ {}, {0, 2}, {1}, {2, 0, 1}};
  ASSERT_EQ(kAllowedResourcesPerVehicle.size(), kNumVehicles);
  for (int v = 0; v < kNumVehicles; v++) {
    resource_group->NotifyVehicleRequiresAResource(v);
    if (!kAllowedResourcesPerVehicle[v].empty()) {
      if constexpr (set_values_on_resource_var) {
        model.ResourceVar(v, resource_group->Index())
            ->SetValues(kAllowedResourcesPerVehicle[v]);
      } else {
        resource_group->SetAllowedResourcesForVehicle(
            v, kAllowedResourcesPerVehicle[v]);
      }
    }
  }

  model.CloseModel();

  EXPECT_EQ(resource_group->GetResourceClassesCount(), 2);
  const std::vector<int> kExpectedResourceClassIndex = {0, 1, 0};
  ASSERT_EQ(kExpectedResourceClassIndex.size(), kNumResources);
  for (int r = 0; r < kNumResources; r++) {
    EXPECT_EQ(resource_group->GetResourceClassIndex(r).value(),
              kExpectedResourceClassIndex[r]);
  }
}

TEST(ResourceClassTest, DifferentVehicleAllowedResources_SetAllowedResources) {
  TestResourceClassesWithDifferentVehicleAllowedResources<false>();
}

// Same as above but the allowed values are set on the  resource var directly.
TEST(ResourceClassTest, DifferentVehicleAllowedResources_ValuesOnResourceVar) {
  TestResourceClassesWithDifferentVehicleAllowedResources<true>();
}

class TestFilter : public LocalSearchFilter {
 public:
  TestFilter(bool* accept_called, bool* sync_called)
      : accept_called_(accept_called), sync_called_(sync_called) {}
  bool Accept(const Assignment* /*delta*/, const Assignment* /*deltadelta*/,
              int64_t /*lower_bound*/, int64_t /*upper_bound*/) override {
    *accept_called_ = true;
    return true;
  }
  void Synchronize(const Assignment* /*assignment*/,
                   const Assignment* /*delta*/) override {
    *sync_called_ = true;
  }

 private:
  bool* const accept_called_;
  bool* const sync_called_;
};

using ModelTest = ::testing::TestWithParam<bool>;
TEST_P(ModelTest, ExtraLocalSearchFilters) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model model(manager);
  // Set up the model such that there are possible moves in local search.
  model.SetFixedCostOfAllVehicles(10);
  const int vehicle_cost =
      GetParam()
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(1)}), 1);

  // Adding one filter before closing the model.
  bool accept_called1 = false;
  bool sync_called1 = false;
  TestFilter filter1(&accept_called1, &sync_called1);
  model.AddLocalSearchFilter(&filter1);
  model.CloseModel();
  // ... and one after.
  bool accept_called2 = false;
  bool sync_called2 = false;
  TestFilter filter2(&accept_called2, &sync_called2);
  model.AddLocalSearchFilter(&filter2);
  ASSERT_NE(model.Solve(), nullptr);
  EXPECT_TRUE(accept_called1);
  EXPECT_TRUE(sync_called1);
  EXPECT_FALSE(accept_called2);
  EXPECT_FALSE(sync_called2);
}

INSTANTIATE_TEST_SUITE_P(WithOrWithoutUnaryCallback, ModelTest,
                         ::testing::Bool());

TEST(UnperformedPenaltyTest, AllMandatory) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model model(manager);
  model.CloseModel();
  EXPECT_EQ(kint64max, model.UnperformedPenalty(1));
  EXPECT_EQ(kint64max, model.UnperformedPenaltyOrValue(100, 1));
}

TEST(UnperformedPenaltyTest, AllOptional) {
  static const int64_t kPenalty = 10;
  IndexManager manager(3, 1, NodeIndex(0));
  Model model(manager);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(1)}), kPenalty);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(2)}), 0);
  model.CloseModel();
  EXPECT_EQ(kPenalty, model.UnperformedPenalty(1));
  EXPECT_EQ(kPenalty, model.UnperformedPenaltyOrValue(100, 1));
  EXPECT_EQ(0, model.UnperformedPenalty(2));
  EXPECT_EQ(0, model.UnperformedPenaltyOrValue(100, 2));
}

TEST(UnperformedPenaltyTest, MixOptionalMandatory) {
  static const int64_t kPenalty = 10;
  IndexManager manager(3, 1, NodeIndex(0));
  Model model(manager);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(1)}), kPenalty);
  model.CloseModel();
  EXPECT_EQ(kPenalty, model.UnperformedPenalty(1));
  EXPECT_EQ(kPenalty, model.UnperformedPenaltyOrValue(100, 1));
  EXPECT_EQ(kint64max, model.UnperformedPenalty(2));
  EXPECT_EQ(kint64max, model.UnperformedPenaltyOrValue(100, 2));
}

TEST(UnperformedPenaltyTest, NodeInMultipleDisjunction) {
  static const int64_t kPenalty = 10;
  IndexManager manager(3, 1, NodeIndex(0));
  Model model(manager);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(1)}), kPenalty);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(1), NodeIndex(2)}),
                       kPenalty);
  model.CloseModel();
  EXPECT_EQ(0, model.UnperformedPenalty(1));
  EXPECT_EQ(100, model.UnperformedPenaltyOrValue(100, 1));
}

class PickupAndDeliverySetsTest : public testing::Test {
 protected:
  ~PickupAndDeliverySetsTest() override = default;
  void SetUp() override { absl::SetFlag(&FLAGS_cp_name_cast_variables, true); }
  std::unique_ptr<Model> CreateModel(std::vector<int> nodes1,
                                     std::vector<int> nodes2) {
    IndexManager manager(7, 1, NodeIndex(0));
    auto model = std::make_unique<Model>(manager);
    Solver* const solver = model->solver();
    std::vector<int64_t> indices1;
    for (int node : nodes1) {
      indices1.push_back(manager.NodeToIndex(NodeIndex(node)));
    }
    std::vector<int64_t> indices2;
    for (int node : nodes2) {
      indices2.push_back(manager.NodeToIndex(NodeIndex(node)));
    }
    if (!indices1.empty() && !indices2.empty()) {
      model->AddPickupAndDeliverySets(model->AddDisjunction(indices1, 10),
                                      model->AddDisjunction(indices2, 10));
      const int callback = model->RegisterTransitCallback(
          absl::bind_front(ValueIfDifferent, manager, 1));
      model->AddDimension(callback, kint64max, kint64max, true, "count");
      const Dimension& dimension = model->GetDimensionOrDie("count");
      std::vector<IntVar*> vehicle1;
      std::vector<IntVar*> cumuls1;
      for (int index : indices1) {
        vehicle1.push_back(model->VehicleVar(index));
        cumuls1.push_back(
            solver->MakeProd(model->ActiveVar(index), dimension.CumulVar(index))
                ->Var());
      }
      std::vector<IntVar*> vehicle2;
      std::vector<IntVar*> cumuls2;
      for (int index : indices2) {
        vehicle2.push_back(model->VehicleVar(index));
        cumuls2.push_back(
            solver->MakeProd(model->ActiveVar(index), dimension.CumulVar(index))
                ->Var());
      }
      // Performed indices must be on the same vehicle.
      solver->AddConstraint(solver->MakeEquality(solver->MakeMax(vehicle1),
                                                 solver->MakeMax(vehicle2)));
      // Performed index of indices1 must happen before the one of indices2.
      solver->AddConstraint(solver->MakeLessOrEqual(solver->MakeSum(cumuls1),
                                                    solver->MakeSum(cumuls2)));
    }
    return model;
  }
};

TEST_F(PickupAndDeliverySetsTest, PickupAndDeliverySets) {
  std::unique_ptr<Model> model(CreateModel({1, 2}, {3, 4, 5, 6}));
  const Assignment* const assignment = model->Solve();
  EXPECT_EQ(0, assignment->ObjectiveValue());
  int non_skipped_count = 0;
  for (int i = 0; i < model->Nexts().size(); ++i) {
    if (assignment->Value(model->NextVar(i)) != i) ++non_skipped_count;
  }
  EXPECT_EQ(3, non_skipped_count);
}

TEST_F(PickupAndDeliverySetsTest, PickupAndDeliverySingletons) {
  std::unique_ptr<Model> model(CreateModel({1}, {2}));
  const Assignment* const assignment = model->Solve();
  EXPECT_EQ(0, assignment->ObjectiveValue());
}

TEST_F(PickupAndDeliverySetsTest, PickupAndDeliverySingleton1) {
  std::unique_ptr<Model> model(CreateModel({1}, {2, 3}));
  const Assignment* const assignment = model->Solve();
  EXPECT_EQ(0, assignment->ObjectiveValue());
}

TEST_F(PickupAndDeliverySetsTest, PickupAndDeliverySingleton2) {
  std::unique_ptr<Model> model(CreateModel({1, 2}, {3}));
  const Assignment* const assignment = model->Solve();
  EXPECT_EQ(0, assignment->ObjectiveValue());
}

TEST_F(PickupAndDeliverySetsTest, PickupAndDeliveryEmptySets) {
  std::unique_ptr<Model> model(CreateModel({}, {}));
  const Assignment* const assignment = model->Solve();
  EXPECT_EQ(0, assignment->ObjectiveValue());
}

TEST_F(PickupAndDeliverySetsTest, PickupAndDeliveryEmptySets1) {
  std::unique_ptr<Model> model(CreateModel({}, {3, 4, 5}));
  const Assignment* const assignment = model->Solve();
  EXPECT_EQ(0, assignment->ObjectiveValue());
}

TEST_F(PickupAndDeliverySetsTest, PickupAndDeliveryEmptySets2) {
  std::unique_ptr<Model> model(CreateModel({1, 2}, {}));
  const Assignment* const assignment = model->Solve();
  EXPECT_EQ(0, assignment->ObjectiveValue());
}

TEST(PickupAndDeliveryTest, PickupAndDelivery) {
  IndexManager manager(3, 1, NodeIndex(0));
  Model model(manager);
  model.AddPickupAndDelivery(manager.NodeToIndex(NodeIndex(1)),
                             manager.NodeToIndex(NodeIndex(2)));
  EXPECT_EQ(1, model.GetPickupAndDeliveryPairs().size());
  EXPECT_EQ(1, model.GetPickupAndDeliveryDisjunctions().size());
}

TEST(PickupAndDeliveryTest, PickupAndDeliveryIndexPairs) {
  const int kNumNodes = 16;
  IndexManager manager(kNumNodes, 1, NodeIndex(0));
  Model model(manager);

  // Node indices 1, 3, 4, 7, 9 and 10 are pickups.
  const std::vector<bool> is_pickup = {
      false, /*1*/ true, false, /*3*/ true, /*4*/ true,  false,
      false, /*7*/ true, false, /*9*/ true, /*10*/ true, false,
      false, false,      false, false};
  // Nodes 5, 6, 11, 13, 14 and 15 are deliveries.
  const std::vector<bool> is_delivery = {
      false,      false,       false,       false,      false, /*5*/ true,
      /*6*/ true, false,       false,       false,      false, /*11*/ true,
      false,      /*13*/ true, /*14*/ true, /*15*/ true};

  const std::vector<PickupDeliveryPair> pickup_delivery_pairs = {
      {{1, 3, 4}, {5, 6}}, {{7, 9}, {11}}, {{10}, {13, 14, 15}}};

  for (const auto& [pickups, deliveries] : pickup_delivery_pairs) {
    const DisjunctionIndex pickup_disjunction = model.AddDisjunction(pickups);
    const DisjunctionIndex delivery_disjunction =
        model.AddDisjunction(deliveries);
    model.AddPickupAndDeliverySets(pickup_disjunction, delivery_disjunction);
  }

  EXPECT_EQ(pickup_delivery_pairs.size(),
            model.GetPickupAndDeliveryPairs().size());
  EXPECT_EQ(pickup_delivery_pairs.size(),
            model.GetPickupAndDeliveryDisjunctions().size());
  for (int i = 0; i < pickup_delivery_pairs.size(); ++i) {
    EXPECT_EQ(model.GetPickupAndDeliveryDisjunctions()[i].first,
              DisjunctionIndex(2 * i));
    EXPECT_EQ(model.GetPickupAndDeliveryDisjunctions()[i].second,
              DisjunctionIndex(2 * i + 1));
  }

  for (int index = 0; index < kNumNodes; index++) {
    const auto pickup_position = model.GetPickupPosition(index);
    EXPECT_EQ(pickup_position.has_value(), is_pickup[index]);
    if (pickup_position.has_value()) {
      const auto [pair_index, alternative_index] = pickup_position.value();
      EXPECT_EQ((pickup_delivery_pairs[pair_index]
                     .pickup_alternatives)[alternative_index],
                index);
    }

    const auto delivery_position = model.GetDeliveryPosition(index);
    EXPECT_EQ(delivery_position.has_value(), is_delivery[index]);
    if (delivery_position.has_value()) {
      const auto [pair_index, alternative_index] = delivery_position.value();
      EXPECT_EQ((pickup_delivery_pairs[pair_index]
                     .delivery_alternatives)[alternative_index],
                index);
    }
  }
}

TEST(NumOfSingletonNodesTest, NoNonStartEndNodes) {
  IndexManager manager(1, 2, NodeIndex(0));
  Model model(manager);
  EXPECT_EQ(0, model.GetNumOfSingletonNodes());
}

TEST(NumOfSingletonNodesTest, SingletonsNoPairs) {
  IndexManager manager(3, 2, NodeIndex(0));
  Model model(manager);
  EXPECT_EQ(2, model.GetNumOfSingletonNodes());
}

TEST(NumOfSingletonNodesTest, PairsNoSingletons) {
  IndexManager manager(3, 2, NodeIndex(0));
  Model model(manager);
  model.AddPickupAndDelivery(manager.NodeToIndex(NodeIndex(1)),
                             manager.NodeToIndex(NodeIndex(2)));
  EXPECT_EQ(0, model.GetNumOfSingletonNodes());
}

TEST(NumOfSingletonNodesTest, PairsWithDisjunctionNoSingletons) {
  IndexManager manager(5, 2, NodeIndex(0));
  Model model(manager);
  model.AddPickupAndDeliverySets(
      model.AddDisjunction({manager.NodeToIndex(NodeIndex(1)),
                            manager.NodeToIndex(NodeIndex(2))}),
      model.AddDisjunction({manager.NodeToIndex(NodeIndex(3)),
                            manager.NodeToIndex(NodeIndex(4))}));
  EXPECT_EQ(0, model.GetNumOfSingletonNodes());
}

TEST(NumOfSingletonNodesTest, PairsAndSingletons) {
  IndexManager manager(4, 2, NodeIndex(0));
  Model model(manager);
  model.AddPickupAndDelivery(manager.NodeToIndex(NodeIndex(1)),
                             manager.NodeToIndex(NodeIndex(2)));
  EXPECT_EQ(1, model.GetNumOfSingletonNodes());
}

TEST(SameVehicleInspectionTest, SameVehicleIndicesOfIndex) {
  IndexManager manager(5, 10, NodeIndex(0));
  Model model(manager);
  Solver* const solver = model.solver();
  solver->AddConstraint(
      solver->MakeEquality(model.VehicleVar(1), model.VehicleVar(2)));
  solver->AddConstraint(
      solver->MakeEquality(model.VehicleVar(3), model.VehicleVar(1)));
  model.CloseModel();
  EXPECT_THAT(model.GetSameVehicleIndicesOfIndex(1),
              UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(model.GetSameVehicleIndicesOfIndex(2),
              UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(model.GetSameVehicleIndicesOfIndex(3),
              UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(model.GetSameVehicleIndicesOfIndex(4), UnorderedElementsAre(4));
}

TEST(SameVehicleInspectionTest, SameVehicleIndicesOfIndexNoConstraint) {
  IndexManager manager(5, 10, NodeIndex(0));
  Model model(manager);
  model.CloseModel();
  for (int i = 0; i < model.Size(); ++i) {
    EXPECT_THAT(model.GetSameVehicleIndicesOfIndex(i), UnorderedElementsAre(i));
  }
}

TEST(SameVehicleInspectionTest, SameActivityIndicesOfIndex) {
  IndexManager manager(5, 10, NodeIndex(0));
  Model model(manager);
  Solver* const solver = model.solver();
  solver->AddConstraint(
      solver->MakeEquality(model.ActiveVar(1), model.ActiveVar(2)));
  solver->AddConstraint(
      solver->MakeEquality(model.ActiveVar(3), model.ActiveVar(1)));
  model.CloseModel();
  EXPECT_THAT(model.GetSameActivityIndicesOfIndex(1),
              UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(model.GetSameActivityIndicesOfIndex(2),
              UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(model.GetSameActivityIndicesOfIndex(3),
              UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(model.GetSameActivityIndicesOfIndex(4), UnorderedElementsAre(4));
}

TEST(SameVehicleInspectionTest, SameActivityIndicesOfIndexWithBatchCall) {
  IndexManager manager(5, 10, NodeIndex(0));
  Model model(manager);
  model.AddSameActivityGroup({1, 2, 3});
  model.CloseModel();
  EXPECT_THAT(model.GetSameActivityIndicesOfIndex(1),
              UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(model.GetSameActivityIndicesOfIndex(2),
              UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(model.GetSameActivityIndicesOfIndex(3),
              UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(model.GetSameActivityIndicesOfIndex(4), UnorderedElementsAre(4));
}

TEST(SameVehicleInspectionTest, SameActivityIndicesOfIndexWithSparseBatchCall) {
  IndexManager manager(5, 10, NodeIndex(0));
  Model model(manager);
  model.AddSameActivityGroup({3, 1});
  model.CloseModel();
  EXPECT_THAT(model.GetSameActivityIndicesOfIndex(1),
              UnorderedElementsAre(1, 3));
  EXPECT_THAT(model.GetSameActivityIndicesOfIndex(2), UnorderedElementsAre(2));
  EXPECT_THAT(model.GetSameActivityIndicesOfIndex(3),
              UnorderedElementsAre(1, 3));
  EXPECT_THAT(model.GetSameActivityIndicesOfIndex(4), UnorderedElementsAre(4));
}

TEST(SameVehicleInspectionTest, SameActivityIndicesOfIndexWithMixedBatchCall) {
  IndexManager manager(5, 10, NodeIndex(0));
  Model model(manager);
  Solver* const solver = model.solver();
  solver->AddConstraint(
      solver->MakeEquality(model.ActiveVar(1), model.ActiveVar(2)));
  model.AddSameActivityGroup({2, 3});
  model.CloseModel();
  EXPECT_THAT(model.GetSameActivityIndicesOfIndex(1),
              UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(model.GetSameActivityIndicesOfIndex(2),
              UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(model.GetSameActivityIndicesOfIndex(3),
              UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(model.GetSameActivityIndicesOfIndex(4), UnorderedElementsAre(4));
}

TEST(SameVehicleInspectionTest, SameActivityIndicesOfIndexNoConstraint) {
  IndexManager manager(5, 10, NodeIndex(0));
  Model model(manager);
  model.CloseModel();
  for (int i = 0; i < model.Size(); ++i) {
    EXPECT_THAT(model.GetSameActivityIndicesOfIndex(i),
                UnorderedElementsAre(i));
  }
}

TEST(SameVehicleInspectionTest, SameActivityIndicesOfIndexEmptyConstraint) {
  IndexManager manager(5, 10, NodeIndex(0));
  Model model(manager);
  model.AddSameActivityGroup({});
  model.CloseModel();
  for (int i = 0; i < model.Size(); ++i) {
    EXPECT_THAT(model.GetSameActivityIndicesOfIndex(i),
                UnorderedElementsAre(i));
  }
}

TEST(SameVehicleInspectionTest, SameActivityIndicesOfIndexSingletonConstraint) {
  IndexManager manager(5, 10, NodeIndex(0));
  Model model(manager);
  model.AddSameActivityGroup({1});
  model.CloseModel();
  for (int i = 0; i < model.Size(); ++i) {
    EXPECT_THAT(model.GetSameActivityIndicesOfIndex(i),
                UnorderedElementsAre(i));
  }
}

TEST(OrderedActivityGroupTest, EmptyGroup) {
  IndexManager manager(5, 10, NodeIndex(0));
  Model model(manager);
  model.AddOrderedActivityGroup({});
  model.CloseModel();
  EXPECT_THAT(model.GetOrderedActivityGroups(), ::testing::IsEmpty());
}

class CallbackTestDB : public DecisionBuilder {
 public:
  explicit CallbackTestDB(std::function<void()> callback)
      : callback_(std::move(callback)) {}
  Decision* Next(Solver*) override {
    callback_();
    return nullptr;
  }

 private:
  std::function<void()> callback_;
};

TEST(OrderedActivityGroupTest, SimpleGroup) {
  IndexManager manager(5, 10, NodeIndex(0));
  Model model(manager);
  std::vector<DisjunctionIndex> disjunctions;
  for (int64_t node : {4, 3, 1}) {
    disjunctions.push_back(model.AddDisjunction({node}, 10));
  }
  model.AddOrderedActivityGroup(disjunctions);
  model.CloseModel();
  Solver* const solver = model.solver();
  EXPECT_TRUE(solver->Solve(solver->RevAlloc(new CallbackTestDB([&model]() {
    EXPECT_EQ(model.ActiveVar(1)->Max(), 1);
    model.ActiveVar(3)->SetValue(0);
    EXPECT_EQ(model.ActiveVar(1)->Value(), 0);
  }))));
  EXPECT_TRUE(solver->Solve(solver->RevAlloc(new CallbackTestDB([&model]() {
    EXPECT_EQ(model.ActiveVar(4)->Min(), 0);
    model.ActiveVar(3)->SetValue(1);
    EXPECT_EQ(model.ActiveVar(4)->Value(), 1);
  }))));
}

TEST(OrderedActivityGroupTest, SimpleGroupWithAlternatives) {
  IndexManager manager(6, 10, NodeIndex(0));
  Model model(manager);
  std::vector<DisjunctionIndex> disjunctions;
  disjunctions.push_back(model.AddDisjunction({1, 2}, 10));
  disjunctions.push_back(model.AddDisjunction({3, 4}, 10));
  model.AddOrderedActivityGroup(disjunctions);
  model.CloseModel();
  Solver* const solver = model.solver();
  EXPECT_TRUE(solver->Solve(solver->RevAlloc(new CallbackTestDB([&model]() {
    model.ActiveVar(1)->SetValue(0);
    EXPECT_EQ(model.ActiveVar(3)->Max(), 1);
    EXPECT_EQ(model.ActiveVar(4)->Max(), 1);
    model.ActiveVar(2)->SetValue(0);
    EXPECT_TRUE(model.ActiveVar(3)->Bound());
    EXPECT_TRUE(model.ActiveVar(4)->Bound());
    EXPECT_EQ(model.ActiveVar(3)->Value(), 0);
    EXPECT_EQ(model.ActiveVar(4)->Value(), 0);
  }))));
  EXPECT_TRUE(solver->Solve(solver->RevAlloc(new CallbackTestDB([&model]() {
    model.ActiveVar(4)->SetValue(1);
    EXPECT_EQ(model.ActiveVar(2)->Min(), 0);
    model.ActiveVar(1)->SetValue(0);
    EXPECT_TRUE(model.ActiveVar(2)->Bound());
    EXPECT_EQ(model.ActiveVar(2)->Value(), 1);
  }))));
}

TEST(OrderedActivityGroupTest, SimpleGroupWithAlternativesWithMaxCardinality) {
  IndexManager manager(6, 10, NodeIndex(0));
  Model model(manager);
  std::vector<DisjunctionIndex> disjunctions;
  disjunctions.push_back(
      model.AddDisjunction({1, 2}, 10, /*max_cardinality=*/2));
  disjunctions.push_back(
      model.AddDisjunction({3, 4}, 10, /*max_cardinality=*/2));
  model.AddOrderedActivityGroup(disjunctions);
  model.CloseModel();
  Solver* const solver = model.solver();
  EXPECT_TRUE(solver->Solve(solver->RevAlloc(new CallbackTestDB([&model]() {
    model.ActiveVar(1)->SetValue(0);
    EXPECT_EQ(model.ActiveVar(3)->Max(), 1);
    EXPECT_EQ(model.ActiveVar(4)->Max(), 1);
    model.ActiveVar(3)->SetValue(1);
    EXPECT_TRUE(model.ActiveVar(4)->Bound());
    EXPECT_EQ(model.ActiveVar(4)->Value(), 0);
  }))));
  EXPECT_TRUE(solver->Solve(solver->RevAlloc(new CallbackTestDB([&model]() {
    model.ActiveVar(4)->SetValue(1);
    model.ActiveVar(3)->SetValue(1);
    EXPECT_EQ(model.ActiveVar(1)->Value(), 1);
    EXPECT_EQ(model.ActiveVar(2)->Value(), 1);
  }))));
}

using PrecedenceGraphTest = ::testing::TestWithParam<bool>;
TEST_P(PrecedenceGraphTest, CumulVarConstraint) {
  const char* kDimension = "dimension";
  IndexManager manager(5, 10, NodeIndex(0));
  Model model(manager);
  const int callback =
      GetParam()
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  model.AddDimension(callback, 0, 10,
                     /*fix_start_cumul_to_zero=*/true, kDimension);
  Dimension* const dimension = model.GetMutableDimension(kDimension);
  Solver* const solver = model.solver();
  solver->AddConstraint(
      solver->MakeLessOrEqual(dimension->CumulVar(1), dimension->CumulVar(2)));
  solver->AddConstraint(solver->MakeGreaterOrEqual(dimension->CumulVar(3),
                                                   dimension->CumulVar(1)));
  model.CloseModel();
  const util::ReverseArcListGraph<int, int>& graph =
      dimension->GetPathPrecedenceGraph();
  EXPECT_EQ(2, graph.OutDegree(1));
  std::vector<int> neighbors;
  for (int neighbor : graph[1]) {
    neighbors.push_back(neighbor);
  }
  EXPECT_THAT(neighbors, UnorderedElementsAre(2, 3));
}

TEST_P(PrecedenceGraphTest, NoConstraints) {
  const char* kDimension = "dimension";
  IndexManager manager(5, 10, NodeIndex(0));
  Model model(manager);
  const int callback =
      GetParam()
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  model.AddDimension(callback, 0, 10,
                     /*fix_start_cumul_to_zero=*/true, kDimension);
  Dimension* const dimension = model.GetMutableDimension(kDimension);
  model.CloseModel();
  const util::ReverseArcListGraph<int, int>& graph =
      dimension->GetPathPrecedenceGraph();
  EXPECT_EQ(0, graph.num_nodes());
}

INSTANTIATE_TEST_SUITE_P(WithOrWithoutUnaryCallback, PrecedenceGraphTest,
                         ::testing::Bool());

// The next test checks time-dependent transitions. Each transition takes one
// time step. The test has the following features:
//   * There is only one vehicle.
//   * There is a unique path of minimum cost for that vehicle.
//   * Some transitions have both pure and dependent evaluators.
//   * The optimal path involves waiting at a node for a cheaper transition.
//   * There is a path of lower cost if one ignores the time dependent
//     transitions or takes the minimum value over the time interval.
//   * There is a limit for the total time spent. A path of lower cost exists if
//     one ignores the limit.
//
// The test consists of three nodes, 0, 1, and 2, where 0 is the start/end node.
// Each transition takes one unit of time. The time limit is 5. The costs are
// like this:
// 0->1  1+3/3/3/0/0/0/0
// 0->2  2+2/1/2/0/0/0/0
// 1->0  0+0/0/1/1/0/0/0
// 1->2  2+1/1/0/1/1/0/0
// 2->0  0+0/0/1/1/0/0/0
// 2->1  2+1/1/0/1/1/0/0
//
// The solution is: wait; go to 2; go to 1; wait; go to 0 with cost 3+2+0=5.
// The only possible routes are 0->1->2->0 and 0->2->1->0, but 1 and 2 are
// symmetric except for the transition from 0 at time 1, where 0->2->1->0 is
// strictly better, so there is no point in taking 0->1->2->0.

Model::StateDependentTransit Lookup3DMatrix(
    const IndexManager& manager,
    const std::vector<std::vector<std::vector<int64_t>>>* matrix, int64_t i,
    int64_t j) {
  const std::vector<int64_t>& slice =
      (*matrix)[manager.IndexToNode(i).value()][manager.IndexToNode(j).value()];
  return Model::MakeStateDependentTransit(
      [&slice](int64_t idx) { return slice[idx]; }, 0, slice.size());
}

class TimeDependentTransitionTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<::testing::tuple<bool, bool, bool>> {
 protected:
  static const int64_t kNumNodes;
  static const int64_t kNumVehicles;
  static const int64_t kMaxTime;
  static const int64_t kTimeLimit;
  static const NodeIndex kDepot;
  static const char kTime[];
  static const char kTransit[];
  std::vector<std::vector<int64_t>> pure_transits_;
  std::vector<std::vector<std::vector<int64_t>>> dependent_transits_;

  TimeDependentTransitionTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/false,
            /*use_light_propagation=*/::testing::get<0>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<1>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<2>(GetParam())) {
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    pure_transits_ = {{0, 1, 2}, {0, 0, 2}, {0, 2, 0}};
    dependent_transits_ = {
        {{0, 0, 0, 0, 0, 0, 0}, {3, 3, 3, 0, 0, 0, 0}, {2, 1, 2, 0, 0, 0, 0}},
        {{0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 1, 1, 0, 0}},
        {{0, 0, 1, 1, 0, 0, 0}, {1, 1, 0, 1, 1, 0, 0}, {0, 0, 0, 0, 0, 0, 0}},
    };

    manager_ = std::make_unique<IndexManager>(kNumNodes, kNumVehicles, kDepot);
    Model* const model = new Model(*manager_, parameters);
    EXPECT_TRUE(model
                    ->AddConstantDimensionWithSlack(1, kMaxTime - 1,
                                                    kMaxTime - 1, false, kTime)
                    .second);
    const Dimension& time_dimension = model->GetDimensionOrDie(kTime);
    time_dimension.CumulVar(model->End(0))->SetRange(0, kTimeLimit - 1);
    EXPECT_TRUE(model->AddDimensionDependentDimensionWithVehicleCapacity(
        model->RegisterTransitCallback(
            absl::bind_front(LookupCostMatrix, *manager_, pure_transits_)),
        model->RegisterStateDependentTransitCallback(
            absl::bind_front(&Lookup3DMatrix, *manager_, &dependent_transits_)),
        &time_dimension, kMaxTime, kint64max, true, kTransit));
    model->GetMutableDimension(kTransit)->SetSpanCostCoefficientForAllVehicles(
        1);
    return model;
  }

  int VehicleCostCount() const override { return 1; }
};

const int64_t TimeDependentTransitionTest::kNumNodes = 3;
const int64_t TimeDependentTransitionTest::kNumVehicles = 1;
const int64_t TimeDependentTransitionTest::kMaxTime = 7;
const int64_t TimeDependentTransitionTest::kTimeLimit = 6;

const NodeIndex TimeDependentTransitionTest::kDepot = NodeIndex(0);

const char TimeDependentTransitionTest::kTime[] = "Time";
const char TimeDependentTransitionTest::kTransit[] = "Transit";

TEST_P(TimeDependentTransitionTest, CorrectnessTest) {
  const Assignment* const assignment = SetUpModelAndSolve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(5, assignment->ObjectiveValue());
  ExpectRoutes(assignment, {{2, 1}});

  const Dimension& transit_dimension = model_->GetDimensionOrDie(kTransit);
  EXPECT_EQ(0, assignment->Value(transit_dimension.CumulVar(0)));
  EXPECT_EQ(3, assignment->Value(transit_dimension.CumulVar(2)));
  EXPECT_EQ(5, assignment->Value(transit_dimension.CumulVar(1)));
  EXPECT_EQ(5, assignment->Value(transit_dimension.CumulVar(3)));

  const Dimension& time_dimension = model_->GetDimensionOrDie(kTime);
  EXPECT_EQ(1, assignment->Value(time_dimension.CumulVar(0)));
  EXPECT_EQ(2, assignment->Value(time_dimension.CumulVar(2)));
  EXPECT_EQ(4, assignment->Value(time_dimension.CumulVar(1)));
  EXPECT_EQ(5, assignment->Value(time_dimension.CumulVar(3)));
}

INSTANTIATE_TEST_SUITE_P(WithParameters, TimeDependentTransitionTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

// The next test checks non-homogeneous time-dependent transitions with 3
// vehicles and 2 vehicles classes. Each transition takes one time step. The
// test has the following features:
//   * There is a unique solution for all vehicles with unique routes, departure
//     and arrival times for each vehicle.
//   * The vehicles have to wait, cooperate and compromise in order to achieve
//     the optimal path.
//   * The cost of each transit is at least one, and in the optimal solution
//     every transit costs precisely 1.
//
// The optimal routes are:
// Vehicle 0: 0 -> 3 -> 1 -> 0
// Vehicle 1: 0 -> 2 -> 0
// Vehicle 2: 0 -> 4 -> 5
// (Vehicle 0 and Vehicle 2 are the same class.)
//
//       1   +--2
//       |\ /  /
//       | 0--+
//       |/ \
//       3   4--5
//
// Note that the assignment above has cost 7. All arcs between nodes of
// different
// groups of the partition {{1, 3}, {2}, {4}} cost more than 7, so no optimal
// solution may contain a route spanning over more than one group. Fortunately
// for us, there are 3 groups and 3 vehicles. Vehicle 1 has expensive
// transitions to nodes 3 and 4, so it must take the route 0 -> 2 -> 0. Vehicle
// 2 has to end up at node5, and arcs from 1 and 3 to 5 are too expensive, so it
// must take the route 0 -> 4 -> 5. Once the routes are decided, the vehicles
// have to carefully adjust their time tables in order to construct an
// assignment of cost 7.

class NonHomogeneousTimeDependentTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 protected:
  static const int64_t kNumNodes;
  static const int64_t kNumVehicles;
  static const int64_t kMaxTime;
  static const int64_t kTimeLimit;
  static const int64_t kOptimalSolutionCost;
  static const int64_t kVehicleCapacity;
  static const std::vector<std::pair<NodeIndex, NodeIndex>>* const kStartEnds;
  static const std::vector<std::vector<std::vector<int64_t>>>* const
      kPureTransits;
  static const std::vector<
      std::vector<std::vector<std::vector<int64_t>>>>* const kDependentTransits;
  static const char kTime[];
  static const char kTransit[];

  NonHomogeneousTimeDependentTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/false,
            /*use_light_propagation=*/::testing::get<0>(GetParam()),
            /*force_non_homogeneous_costs=*/true,
            /*use_non_filtered_first_solution=*/::testing::get<1>(GetParam())) {
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    manager_ =
        std::make_unique<IndexManager>(kNumNodes, kNumVehicles, *kStartEnds);
    Model* const model = new Model(*manager_, parameters);
    EXPECT_TRUE(model
                    ->AddConstantDimensionWithSlack(1, kMaxTime - 1,
                                                    kMaxTime - 1, false, kTime)
                    .second);
    const Dimension& time_dimension = model->GetDimensionOrDie(kTime);

    for (int vehicle = 0; vehicle < kNumVehicles; ++vehicle) {
      time_dimension.CumulVar(model->End(vehicle))->SetRange(0, kTimeLimit - 1);
    }

    CHECK(kNumVehicles == kPureTransits->size());
    std::vector<int> pure_transits;
    for (const std::vector<std::vector<int64_t>>& transit : *kPureTransits) {
      pure_transits.push_back(model->RegisterTransitCallback(
          absl::bind_front(LookupCostMatrix, *manager_, transit)));
    }

    CHECK(kNumVehicles == kDependentTransits->size());
    std::vector<int> dependent_evaluators;
    for (const std::vector<std::vector<std::vector<int64_t>>>& transit :
         *kDependentTransits) {
      dependent_evaluators.push_back(
          model->RegisterStateDependentTransitCallback(
              absl::bind_front(&Lookup3DMatrix, *manager_, &transit)));
    }

    EXPECT_TRUE(model->AddDimensionDependentDimensionWithVehicleCapacity(
        pure_transits, dependent_evaluators, &time_dimension, kMaxTime,
        std::vector<int64_t>(kNumVehicles, kVehicleCapacity), true, kTransit));

    model->GetMutableDimension(kTransit)->SetSpanCostCoefficientForAllVehicles(
        1);
    return model;
  }

  int VehicleCostCount() const override { return 3; }
  bool HasHomogeneousCosts() const override { return false; }
  RoutingSearchParameters GetSearchParameters() override {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(
        FirstSolutionStrategy::FIRST_UNBOUND_MIN_VALUE);
    parameters.set_use_depth_first_search(true);
    return parameters;
  }

  static std::vector<int64_t> CreateEmptyTransit();
  static std::vector<std::vector<int64_t>> CreateEmptyTransitFromNode();
  static std::vector<std::vector<std::vector<int64_t>>>
  CreateEmptyTransitTable();
  static std::vector<std::vector<std::vector<int64_t>>>
  CreateTimeDepTransitsForVehicles02();
};

std::vector<int64_t> NonHomogeneousTimeDependentTest::CreateEmptyTransit() {
  return std::vector<int64_t>(kMaxTime, 0);
}

std::vector<std::vector<int64_t>>
NonHomogeneousTimeDependentTest::CreateEmptyTransitFromNode() {
  return std::vector<std::vector<int64_t>>(kNumNodes, CreateEmptyTransit());
}

std::vector<std::vector<std::vector<int64_t>>>
NonHomogeneousTimeDependentTest::CreateEmptyTransitTable() {
  return std::vector<std::vector<std::vector<int64_t>>>(
      kNumNodes, CreateEmptyTransitFromNode());
}

std::vector<std::vector<std::vector<int64_t>>>
NonHomogeneousTimeDependentTest::CreateTimeDepTransitsForVehicles02() {
  return {
      {
          CreateEmptyTransit(),            // 0->0
          {1, 0, 0, 0, 0, 0, 0, 0, 0, 0},  // 0->1
          CreateEmptyTransit(),            // 0->2
          {1, 0, 0, 0, 0, 0, 0, 0, 0, 0},  // 0->3
          CreateEmptyTransit(),            // 0->4
          CreateEmptyTransit(),            // 0->5
      },
      {
          {1, 2, 3, 2, 0, 0, 0, 0, 0, 0},  // 1->0
          CreateEmptyTransit(),            // 1->1
          CreateEmptyTransit(),            // 1->2
          {0, 0, 1, 0, 0, 0, 0, 0, 0, 0},  // 1->3
          CreateEmptyTransit(),            // 1->4
          CreateEmptyTransit(),            // 1->5
      },
      CreateEmptyTransitFromNode(),  // 2->
      {
          {0, 0, 0, 0, 1, 0, 0, 0, 0, 0},  // 3->0
          {3, 2, 1, 2, 3, 0, 0, 0, 0, 0},  // 3->1
          CreateEmptyTransit(),            // 3->2
          CreateEmptyTransit(),            // 3->3
          CreateEmptyTransit(),            // 3->4
          CreateEmptyTransit(),            // 3->5
      },
      {
          CreateEmptyTransit(),            // 4->0
          CreateEmptyTransit(),            // 4->1
          CreateEmptyTransit(),            // 4->2
          CreateEmptyTransit(),            // 4->3
          CreateEmptyTransit(),            // 4->4
          {0, 2, 2, 2, 1, 0, 0, 0, 0, 0},  // 4->5
      },
      {
          CreateEmptyTransit(),  // 5->0
          CreateEmptyTransit(),  // 5->1
          CreateEmptyTransit(),  // 5->2
          CreateEmptyTransit(),  // 5->3
          CreateEmptyTransit(),  // 5->4
          CreateEmptyTransit(),  // 5->5
      },
  };
}

const int64_t NonHomogeneousTimeDependentTest::kNumNodes = 6;
const int64_t NonHomogeneousTimeDependentTest::kNumVehicles = 3;
const int64_t NonHomogeneousTimeDependentTest::kMaxTime = 10;
const int64_t NonHomogeneousTimeDependentTest::kTimeLimit = 6;
const int64_t NonHomogeneousTimeDependentTest::kOptimalSolutionCost = 7;
const int64_t NonHomogeneousTimeDependentTest::kVehicleCapacity = 10;
const std::vector<std::pair<NodeIndex, NodeIndex>>* const
    NonHomogeneousTimeDependentTest::kStartEnds =
        new std::vector<std::pair<NodeIndex, NodeIndex>>{
            {NodeIndex(0), NodeIndex(0)},
            {NodeIndex(0), NodeIndex(0)},
            {NodeIndex(0), NodeIndex(5)}};

const std::vector<std::vector<std::vector<int64_t>>>* const
    NonHomogeneousTimeDependentTest::kPureTransits =
        new std::vector<std::vector<std::vector<int64_t>>>{
            {
                // Vehicle 0
                {0, 1, 1, 1, 1, 4},  // 0->
                {1, 0, 9, 1, 9, 4},  // 1->
                {1, 9, 0, 9, 9, 4},  // 2->
                {1, 0, 9, 0, 9, 4},  // 3->
                {0, 9, 9, 9, 0, 0},  // 4->
                {0, 0, 0, 0, 0, 0},  // 5->
            },
            {
                // Vehicle 1
                {0, 1, 1, 2, 9, 1},  // 0->
                {1, 0, 9, 2, 9, 1},  // 1->
                {1, 9, 0, 9, 9, 1},  // 2->
                {1, 1, 9, 0, 9, 1},  // 3->
                {1, 9, 9, 9, 0, 1},  // 4->
                {1, 1, 1, 1, 1, 0},  // 5->
            },
            {
                // Vehicle 2
                {0, 1, 1, 1, 1, 4},  // 0->
                {1, 0, 9, 1, 9, 4},  // 1->
                {1, 9, 0, 9, 9, 4},  // 2->
                {1, 0, 9, 0, 9, 4},  // 3->
                {0, 9, 9, 9, 0, 0},  // 4->
                {0, 0, 0, 0, 0, 0},  // 5->
            },
        };

const std::vector<std::vector<std::vector<std::vector<int64_t>>>>* const
    NonHomogeneousTimeDependentTest::kDependentTransits =
        new std::vector<std::vector<std::vector<std::vector<int64_t>>>>{
            CreateTimeDepTransitsForVehicles02(),
            CreateEmptyTransitTable(),
            CreateTimeDepTransitsForVehicles02(),
        };

const char NonHomogeneousTimeDependentTest::kTime[] = "Time";
const char NonHomogeneousTimeDependentTest::kTransit[] = "Transit";

TEST_P(NonHomogeneousTimeDependentTest, CorrectnessTest) {
  const Assignment* const assignment = SetUpModelAndSolve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(kOptimalSolutionCost, assignment->ObjectiveValue());
  ExpectRoutes(assignment, {{3, 1}, {2}, {4}});

  const Dimension& transit_dimension = model_->GetDimensionOrDie(kTransit);
  // Route for vehicle 0
  EXPECT_EQ(0, assignment->Min(transit_dimension.CumulVar(model_->Start(0))));
  EXPECT_EQ(0, assignment->Max(transit_dimension.CumulVar(model_->Start(0))));
  EXPECT_EQ(1, assignment->Min(transit_dimension.CumulVar(3)));
  EXPECT_EQ(1, assignment->Max(transit_dimension.CumulVar(3)));
  EXPECT_EQ(2, assignment->Min(transit_dimension.CumulVar(1)));
  EXPECT_EQ(2, assignment->Max(transit_dimension.CumulVar(1)));
  EXPECT_EQ(3, assignment->Min(transit_dimension.CumulVar(model_->End(0))));
  EXPECT_EQ(3, assignment->Max(transit_dimension.CumulVar(model_->End(0))));

  EXPECT_EQ(0, assignment->Min(transit_dimension.CumulVar(model_->Start(1))));
  EXPECT_EQ(0, assignment->Max(transit_dimension.CumulVar(model_->Start(1))));
  EXPECT_EQ(1, assignment->Min(transit_dimension.CumulVar(2)));
  EXPECT_EQ(1, assignment->Max(transit_dimension.CumulVar(2)));
  EXPECT_EQ(2, assignment->Min(transit_dimension.CumulVar(model_->End(1))));
  EXPECT_EQ(2, assignment->Max(transit_dimension.CumulVar(model_->End(1))));

  EXPECT_EQ(0, assignment->Min(transit_dimension.CumulVar(model_->Start(2))));
  EXPECT_EQ(0, assignment->Max(transit_dimension.CumulVar(model_->Start(2))));
  EXPECT_EQ(1, assignment->Min(transit_dimension.CumulVar(4)));
  EXPECT_EQ(1, assignment->Max(transit_dimension.CumulVar(4)));
  EXPECT_EQ(2, assignment->Min(transit_dimension.CumulVar(model_->End(2))));
  EXPECT_EQ(2, assignment->Max(transit_dimension.CumulVar(model_->End(2))));

  int64_t sum_of_cumul_differences = 0;
  for (int idx = 0; idx < kNumVehicles; ++idx) {
    sum_of_cumul_differences +=
        assignment->Value(transit_dimension.CumulVar(model_->End(idx))) -
        assignment->Value(transit_dimension.CumulVar(model_->Start(idx)));
  }
  EXPECT_EQ(kOptimalSolutionCost, sum_of_cumul_differences);

  // Only vehicle 0 has a unique optimal time schedule
  const Dimension& time_dimension = model_->GetDimensionOrDie(kTime);
  EXPECT_EQ(1, assignment->Value(time_dimension.CumulVar(model_->Start(0))));
  EXPECT_EQ(2, assignment->Value(time_dimension.CumulVar(3)));
  EXPECT_EQ(4, assignment->Value(time_dimension.CumulVar(1)));
  EXPECT_EQ(5, assignment->Value(time_dimension.CumulVar(model_->End(0))));
}

INSTANTIATE_TEST_SUITE_P(WithParameters, NonHomogeneousTimeDependentTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

// The next test checks self-based dimensions. It creates one dimension where
// each transit i->j depends on cumul[i]. To make things simple, there is a
// unique optimal route: 0->3->1->2, and all arcs not contained in the route
// have cost kint64max. The difficulty comes when deciding how long to wait on
// each node. The optimal waiting time is:
//   * on node 0 wait 2
//   * on node 3 wait 1
//   * on node 1 do not wait
//
// The transits are:
//   Transition table for arc 0->3:
//        [0, 1) -> 10
//        [1, 2) -> 20
//        [2, 3) -> 30
//
//   (The vehicle will arrive at node 3 at time 32 in the optimal path.)
//   Transition table for arc 3->1:
//        [0, 33) -> 0
//        [33, 1000) -> 100
//
//   (The vehicle will arrive at node 1 at time 133 in the optimal path.)
//   Transition table for arc 1->2:
//        [0, 133) -> 1000
//        [133, 140) -> 20
//        [140, 1000) -> 0
//   The vehicle cannot take the last transition because the slack is at most 2.
//
// The final arrival time is 153. The value of the objective is 151, since the
// initial waiting does not count.

class SelfBaseDimensionTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<::testing::tuple<bool, bool, bool>> {
 protected:
  static const int64_t kNumNodes;
  static const int64_t kNumVehicles;
  static const int64_t kOptimalSolutionCost;
  static const int64_t kMaxSlack;
  static const int64_t kMaxTime;
  static const char kTime[];
  std::vector<std::vector<int64_t>> pure_transits_;

  SelfBaseDimensionTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/false,
            /*use_light_propagation=*/::testing::get<0>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<1>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<2>(GetParam())) {
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    pure_transits_ = {
        {kMaxTime, kMaxTime, kMaxTime, 0},
        {kMaxTime, kMaxTime, 0, kMaxTime},
        {kMaxTime, kMaxTime, kMaxTime, kMaxTime},
        {kMaxTime, 0, kMaxTime, kMaxTime},
    };
    const std::vector<std::pair<NodeIndex, NodeIndex>> kStartEnds = {
        {NodeIndex(0), NodeIndex(2)}};
    manager_ =
        std::make_unique<IndexManager>(kNumNodes, kNumVehicles, kStartEnds);
    Model* const model = new Model(*manager_, parameters);
    EXPECT_TRUE(model->AddDimensionDependentDimensionWithVehicleCapacity(
        model->RegisterTransitCallback(
            absl::bind_front(LookupCostMatrix, *manager_, pure_transits_)),
        model->RegisterStateDependentTransitCallback(
            absl::bind_front(&DependentTransit, *manager_)),
        nullptr, kMaxSlack, kMaxTime, false, kTime));
    const Dimension& time = model->GetDimensionOrDie(kTime);
    time.CumulVar(0)->SetMax(kMaxSlack);
    model->GetMutableDimension(kTime)->SetSpanCostCoefficientForAllVehicles(1);
    return model;
  }

  static Model::StateDependentTransit DependentTransit(
      const IndexManager& manager, int64_t i_index, int64_t j_index) {
    const NodeIndex i = manager.IndexToNode(i_index);
    const NodeIndex j = manager.IndexToNode(j_index);
    if (i.value() == 0 && j.value() == 3) {
      return Model::MakeStateDependentTransit(
          [](int64_t argument) { return (argument + 1) * 10; }, 0,
          kMaxTime + 1);
    }
    if (i.value() == 3 && j.value() == 1) {
      return Model::MakeStateDependentTransit(
          [](int64_t argument) { return argument >= 33 ? 100 : 0; }, 0,
          kMaxTime + 1);
    }
    if (i.value() == 1 && j.value() == 2) {
      return Model::MakeStateDependentTransit(
          [](int64_t argument) {
            return (argument < 133) ? 10000 : (argument < 140 ? 20 : 0);
          },
          0, kMaxTime + 1);
    }
    return Model::MakeStateDependentTransit([](int64_t) { return 0; }, 0,
                                            kMaxTime + 1);
  }

  int VehicleCostCount() const override { return 1; }
};

TEST_P(SelfBaseDimensionTest, CorrectnessTest) {
  const Assignment* const assignment = SetUpModelAndSolve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(kOptimalSolutionCost, assignment->ObjectiveValue());
  std::vector<int64_t> optimal_route;
  for (const int n : {3, 1}) {
    optimal_route.push_back(manager_->NodeToIndex(NodeIndex(n)));
  }
  ExpectRoutes(assignment, {optimal_route});

  const int64_t first_step = optimal_route[0];
  const int64_t second_step = optimal_route[1];

  const Dimension& time = model_->GetDimensionOrDie(kTime);
  CHECK_EQ(2, time.SlackVar(model_->Start(0))->Max());
  EXPECT_EQ(2, assignment->Value(time.CumulVar(model_->Start(0))));
  EXPECT_EQ(33, assignment->Value(time.CumulVar(first_step)));
  EXPECT_EQ(133, assignment->Value(time.CumulVar(second_step)));
  EXPECT_EQ(153, assignment->Value(time.CumulVar(model_->End(0))));
}

const int64_t SelfBaseDimensionTest::kNumNodes = 4;
const int64_t SelfBaseDimensionTest::kNumVehicles = 1;
const int64_t SelfBaseDimensionTest::kOptimalSolutionCost = 151;
const int64_t SelfBaseDimensionTest::kMaxSlack = 2;
const int64_t SelfBaseDimensionTest::kMaxTime = (1 << 10);
const char SelfBaseDimensionTest::kTime[] = "Time";

INSTANTIATE_TEST_SUITE_P(WithParameters, SelfBaseDimensionTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

// This test creates a non-trivial forest of dimensions and checks if the
// routing library finds the optimal time schedule.
// The forest looks like this:
//
//           +---+       +---+
//            \ /         \ /
//            D 1         D 7
//             | \         |
//             |  D6      D 8
//            D 2
//           /   \
//         D 3   D 4
//
// Only D1 and D7 have slacks and are not forced to start at 0 (otherwise it
// takes ages). The cost function includes D3 and D8; there are time windows for
// D6 and D8. To make things simple, all arcs except 0->1->2->0 have very high
// cost, and therefore the only possible route is the one above. Furthermore,
// all time dependent transitions along the route are very simple: the
// transition from i to j is the cumul[i] of the base. All slacks, including the
// starting value of the cumuls, are 3 at most. In this setting we have:
//
//            cumul[0]    cumul[1]    cumul[2]    cumul[3]
// D1         i           2*i + s
// D2/D6      0           i           3*i + s
// D3         0           0           i           4*i + s
//
// where i is D1.cumul[0] and s is D1.slack[0].
// In the test cumul[2] is restricted to be 6, so either (i, s) = (1, 3) or
// (i, s) = (2, 0), but the former pair has lower final cost (7).
//
//            cumul[0]    cumul[1]    cumul[2]    cumul[3]
// D7         i           2*i + s1    4*i+2*s1+s2
// D8         0           i           3*i+s1      7*i+3*s1+s2
//
// D8.cumul[2] is restricted to be 6, so again (i, s1) = (1, 3) or (2, 0). The
// optimal schedule is i=2, s1=s2=0 with cost 14.

class DependencyTreeTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<::testing::tuple<bool, bool, bool>> {
 protected:
  static const int64_t kNumNodes;
  static const int64_t kNumVehicles;
  static const int64_t kOptimalSolutionCost;
  static const int64_t kMaxSlack;
  static const int64_t kCapacity;
  static const char kNames[][3];
  std::vector<int> optimal_route_;

  DependencyTreeTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/false,
            /*use_light_propagation=*/::testing::get<0>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<1>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<2>(GetParam())) {
  }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    optimal_route_ = {1, 2};
    manager_ =
        std::make_unique<IndexManager>(kNumNodes, kNumVehicles, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);

    const int64_t steps[] = {
        model->Start(0), manager_->NodeToIndex(NodeIndex(optimal_route_[0])),
        manager_->NodeToIndex(NodeIndex(optimal_route_[1])), model->End(0)};

    for (int i = 0; i < kNumNodes; ++i) {
      model->NextVar(steps[i])->SetValue(steps[i + 1]);
    }

    const int state_dependent_transit =
        model->RegisterStateDependentTransitCallback(DependentTransit);
    EXPECT_TRUE(model->AddDimensionDependentDimensionWithVehicleCapacity(
        state_dependent_transit, nullptr, kMaxSlack, kCapacity, false,
        kNames[0]));
    Dimension* const d1 = model->GetMutableDimension(kNames[0]);
    d1->CumulVar(0)->SetMax(kMaxSlack);
    d1->SlackVar(1)->SetMax(0);  // Speeds up the test
    d1->SlackVar(2)->SetMax(0);  // Speeds up the test

    EXPECT_TRUE(model->AddDimensionDependentDimensionWithVehicleCapacity(
        state_dependent_transit, d1, 0, kCapacity, true, kNames[1]));
    Dimension* const d2 = model->GetMutableDimension(kNames[1]);

    EXPECT_TRUE(model->AddDimensionDependentDimensionWithVehicleCapacity(
        state_dependent_transit, d2, 0, kCapacity, true, kNames[2]));
    Dimension* const d3 = model->GetMutableDimension(kNames[2]);
    d3->SetSpanCostCoefficientForAllVehicles(1);

    EXPECT_TRUE(model->AddDimensionDependentDimensionWithVehicleCapacity(
        state_dependent_transit, d2, 0, kCapacity, true, kNames[3]));

    EXPECT_TRUE(model->AddDimensionDependentDimensionWithVehicleCapacity(
        state_dependent_transit, d1, 0, kCapacity, true, kNames[4]));
    Dimension* const d6 = model->GetMutableDimension(kNames[4]);
    d6->CumulVar(2)->SetValue(6);

    EXPECT_TRUE(model->AddDimensionDependentDimensionWithVehicleCapacity(
        state_dependent_transit, nullptr, kMaxSlack, kCapacity, false,
        kNames[5]));
    Dimension* const d7 = model->GetMutableDimension(kNames[5]);
    d7->CumulVar(0)->SetMax(kMaxSlack);
    d7->SlackVar(2)->SetMax(0);  // Speeds up the test

    EXPECT_TRUE(model->AddDimensionDependentDimensionWithVehicleCapacity(
        state_dependent_transit, d7, 0, kCapacity, true, kNames[6]));
    Dimension* const d8 = model->GetMutableDimension(kNames[6]);
    d8->SetSpanCostCoefficientForAllVehicles(1);
    d8->CumulVar(2)->SetValue(6);

    return model;
  }

  void PostCloseModel() override {
    CHECK(model_->CostVar());
    model_->CostVar()->SetMax(10 * kOptimalSolutionCost);  // Speeds up the test
  }

  static Model::StateDependentTransit DependentTransit(int64_t, int64_t) {
    return Model::MakeStateDependentTransit([](int64_t x) { return x; }, 0,
                                            kCapacity + 1);
  }

  int VehicleCostCount() const override { return 1; }

  RoutingSearchParameters GetSearchParameters() override {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(
        FirstSolutionStrategy::FIRST_UNBOUND_MIN_VALUE);
    parameters.set_use_depth_first_search(true);
    return parameters;
  }
};

TEST_P(DependencyTreeTest, CorrectnessTest) {
  const Assignment* const assignment = SetUpModelAndSolve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(kOptimalSolutionCost, assignment->ObjectiveValue());
  std::vector<int64_t> optimal_route;
  for (const int n : optimal_route_) {
    optimal_route.push_back(manager_->NodeToIndex(NodeIndex(n)));
  }
  ExpectRoutes(assignment, {optimal_route});

  const int64_t steps[] = {model_->Start(0), optimal_route[0], optimal_route[1],
                           model_->End(0)};

  const Dimension* dimensions[7] = {&model_->GetDimensionOrDie(kNames[0]),
                                    &model_->GetDimensionOrDie(kNames[1]),
                                    &model_->GetDimensionOrDie(kNames[2]),
                                    &model_->GetDimensionOrDie(kNames[3]),
                                    &model_->GetDimensionOrDie(kNames[4]),
                                    &model_->GetDimensionOrDie(kNames[5]),
                                    &model_->GetDimensionOrDie(kNames[6])};

  // From above:
  //            cumul[0]    cumul[1]    cumul[2]    cumul[3]
  // D1         i           2*i + s
  // D2/D6      0           i           3*i + s
  // D3         0           0           i           4*i + s
  {
    static const int64_t i = 1;
    static const int64_t s = 3;

    EXPECT_EQ(i, assignment->Value(dimensions[0]->CumulVar(steps[0])));
    EXPECT_EQ(2 * i + s, assignment->Value(dimensions[0]->CumulVar(steps[1])));

    EXPECT_EQ(0, assignment->Value(dimensions[1]->CumulVar(steps[0])));
    EXPECT_EQ(i, assignment->Value(dimensions[1]->CumulVar(steps[1])));
    EXPECT_EQ(3 * i + s, assignment->Value(dimensions[1]->CumulVar(steps[2])));

    EXPECT_EQ(0, assignment->Value(dimensions[4]->CumulVar(steps[0])));
    EXPECT_EQ(i, assignment->Value(dimensions[4]->CumulVar(steps[1])));
    EXPECT_EQ(3 * i + s, assignment->Value(dimensions[4]->CumulVar(steps[2])));

    EXPECT_EQ(0, assignment->Value(dimensions[2]->CumulVar(steps[0])));
    EXPECT_EQ(0, assignment->Value(dimensions[2]->CumulVar(steps[1])));
    EXPECT_EQ(i, assignment->Value(dimensions[2]->CumulVar(steps[2])));
    EXPECT_EQ(4 * i + s, assignment->Value(dimensions[2]->CumulVar(steps[3])));
  }

  // D7         i           2*i + s1    4*i+2*s1+s2
  // D8         0           i           3*i+s1      7*i+3*s1+s2
  {
    static const int64_t i = 2;
    static const int64_t s1 = 0;
    static const int64_t s2 = 0;

    EXPECT_EQ(i, assignment->Value(dimensions[5]->CumulVar(steps[0])));
    EXPECT_EQ(2 * i + s1, assignment->Value(dimensions[5]->CumulVar(steps[1])));
    EXPECT_EQ(4 * i + 2 * s1 + s2,
              assignment->Value(dimensions[5]->CumulVar(steps[2])));

    EXPECT_EQ(0, assignment->Value(dimensions[6]->CumulVar(steps[0])));
    EXPECT_EQ(i, assignment->Value(dimensions[6]->CumulVar(steps[1])));
    EXPECT_EQ(3 * i + s1, assignment->Value(dimensions[6]->CumulVar(steps[2])));
    EXPECT_EQ(7 * i + 3 * s1 + s2,
              assignment->Value(dimensions[6]->CumulVar(steps[3])));
  }
}

const int64_t DependencyTreeTest::kNumNodes = 3;
const int64_t DependencyTreeTest::kNumVehicles = 1;
const int64_t DependencyTreeTest::kOptimalSolutionCost = 21;
const int64_t DependencyTreeTest::kMaxSlack = 3;
const int64_t DependencyTreeTest::kCapacity = 1 << 10;
const char DependencyTreeTest::kNames[][3] = {"D1", "D2", "D3", "D4",
                                              "D6", "D7", "D8"};

INSTANTIATE_TEST_SUITE_P(WithParameters, DependencyTreeTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

class GuidedFinalizerTest : public testing::Test {
 protected:
  static int64_t InitializerOrderTest(int64_t var_idx) {
    static const int64_t return_values[] = {1, 1, 2};
    return return_values[var_idx];
  }
  static int64_t InitializerBacktrackingTest(int64_t var_idx) {
    static const int64_t return_values[2][4] = {{1, 1, 1, 1}, {0, 0, 0, 1}};
    static int counter = 0;
    return return_values[++counter >= 4][var_idx];
  }
  int64_t InitializerGuidedFinalizerTest() {
    return absl::Uniform<int64_t>(random_, 0, 3);
  }
  static const char kDimensionName[];
  absl::BitGen random_;
};

const char GuidedFinalizerTest::kDimensionName[] = "Time";

// The following case tests GuidedFinalizer. There are three variables, x0, x1
// and x2, each with domain [0, 2]. There is an additional constraint
// AllDifferent({x0, x1, x2}). A GuidedFinalizer is constructed over x0, x1
// and x2 with guiding values {1, 1, 2}. There is a unique route in the routing
// model, fixed to be 1->0->2. The decision tree should look like this:
//
//                                  /|\
//              +------------------+ | +-----------------+
//             /                     |                    \
//         x1=1                    x1=2                    x1=0
//        /    \                  /    \                  /    \
//       /      \                /      \                /      \
//   x0=2        x0=0        x0=1        x0=0        x0=1        x0=2
//     |           |           |           |           |           |
//     |           |           |           |           |           |
//   x2=0        x2=2        x2=0        x2=1        x2=2        x2=1
//
// and hence the solutions are 210, 012, 120, 021, 102, 201 and they should
// appear in this order.
TEST_F(GuidedFinalizerTest, OrderTest) {
  static const int solution_count = 6;
  static const int solutions[solution_count][3] = {
      {2, 1, 0}, {0, 1, 2}, {1, 2, 0}, {0, 2, 1}, {1, 0, 2}, {2, 0, 1}};
  IndexManager manager(4, 1, {{NodeIndex(1), NodeIndex(3)}});
  Model model(manager);
  model.NextVar(1)->SetValue(0);
  model.NextVar(0)->SetValue(2);
  model.NextVar(2)->SetValue(3);
  model.AddConstantDimensionWithSlack(0, 10, 2, true, kDimensionName);
  const Dimension& dimension = model.GetDimensionOrDie(kDimensionName);
  model.solver()->AddConstraint(
      model.solver()->MakeAllDifferent(dimension.slacks()));
  model.CloseModel();
  DecisionBuilder* const db =
      model.MakeGuidedSlackFinalizer(&dimension, InitializerOrderTest);
  SolutionCollector* const collector =
      model.solver()->MakeAllSolutionCollector();
  collector->Add(dimension.slacks());

  model.solver()->Solve(db, collector);

  EXPECT_EQ(solution_count, collector->solution_count());
  for (int64_t solution_idx = 0; solution_idx < solution_count;
       ++solution_idx) {
    for (int64_t slack_idx = 0; slack_idx < 3; ++slack_idx) {
      EXPECT_EQ(collector->Value(solution_idx, dimension.SlackVar(slack_idx)),
                solutions[solution_idx][slack_idx]);
    }
  }
}

// GuidedFinalizer should be easily composable with other decision builders.
// This test creates a decision builder generating different permutations of the
// next variables and also changing the guiding values. The test checks if
// GuidedFinalizer is able to correctly start a new search after
// finishing one, and if it adequately handles the changes.
//
// The class CreatePathsAndCenterssDB is a decision builder over four boolean
// variables, var[0] ... var[3]. var[3] is bounded to 0 and there is a
// constraint that var[1] and var[2] are different.
//
// Decision 1
// nexts[0] = 1      guiding_values[0] = 1
// nexts[1] = 2      guiding_values[1] = 1
// nexts[2] = 3      guiding_values[2] = 1
// nexts[3] = -1     guiding_values[3] = 1
//
// GuidedFinalizer should generate solutions 1100, 1010, 0100, 0010 in
// this order.
//
// Decision 2
// nexts[0] = 2      guiding_values[0] = 0
// nexts[1] = 3      guiding_values[1] = 0
// nexts[2] = 1      guiding_values[2] = 0
// nexts[3] = -1     guiding_values[3] = 1
//
// For this decision GuidedFinalizer should generate solutions 0100,
// 0010, 1100, 1010 in  this order.

class CreatePathsDB : public DecisionBuilder {
 public:
  explicit CreatePathsDB(const std::vector<IntVar*>* nexts)
      : nexts_(nexts), decision_number_(0) {}
  Decision* Next(Solver* solver) override {
    switch (decision_number_++) {
      case 0:  // Level 0
        return solver->MakeAssignVariablesValues(*nexts_, {1, 2, 3, 4});
      case 1:  // Level 1
        return nullptr;
      case 2:  // Level 0
        return solver->MakeAssignVariablesValues(*nexts_, {2, 3, 1, 4});
      case 3:  // Level 1
        return nullptr;
      default:
        solver->Fail();
        return nullptr;
    }
  }

 protected:
  const std::vector<IntVar*>* const nexts_;
  int64_t decision_number_;
};

TEST_F(GuidedFinalizerTest, BacktrackingTest) {
  static const int solution_count = 8;
  static const int solutions[solution_count][4] = {
      {1, 1, 0, 0}, {1, 0, 1, 0}, {0, 1, 0, 0}, {0, 0, 1, 0},
      {0, 1, 0, 0}, {0, 0, 1, 0}, {1, 1, 0, 0}, {1, 0, 1, 0}};
  IndexManager manager(5, 1, {{NodeIndex(0), NodeIndex(4)}});
  Model model(manager);
  model.AddConstantDimensionWithSlack(0, 10, 1, true, kDimensionName);
  const Dimension& dimension = model.GetDimensionOrDie(kDimensionName);
  dimension.SlackVar(3)->SetValue(0);
  model.CloseModel();
  model.solver()->AddConstraint(model.solver()->MakeNonEquality(
      dimension.SlackVar(1), dimension.SlackVar(2)));

  DecisionBuilder* const db1 =
      model.solver()->RevAlloc(new CreatePathsDB(&model.Nexts()));
  DecisionBuilder* const db2 =
      model.MakeGuidedSlackFinalizer(&dimension, InitializerBacktrackingTest);
  DecisionBuilder* const db = model.solver()->Compose(db1, db2);
  SolutionCollector* const collector =
      model.solver()->MakeAllSolutionCollector();
  collector->Add(dimension.slacks());
  model.solver()->Solve(db, collector);
  EXPECT_EQ(solution_count, collector->solution_count());
  for (int64_t solution_idx = 0; solution_idx < solution_count;
       ++solution_idx) {
    for (int64_t var_idx = 0; var_idx < 4; ++var_idx) {
      EXPECT_EQ(collector->Value(solution_idx, dimension.SlackVar(var_idx)),
                solutions[solution_idx][var_idx]);
    }
  }
}

// The next test checks if GuidedFinalizer works with more than one routes. It
// creates the following routes:
// 0->3; 1->2->4
// and an IntVar* with domain [0, 2] for each node in the network. The test
// checks if GuidedFinalizer finds all 3^5 different solutions.
typedef std::tuple<int64_t, int64_t, int64_t, int64_t, int64_t>
    OrderedAssignment;
TEST_F(GuidedFinalizerTest, MultiplePathsTest) {
  static const int solution_count = 3 * 3 * 3 * 3 * 3;
  absl::flat_hash_set<OrderedAssignment> all_solutions;
  for (int64_t idx1 = 0; idx1 < 3; ++idx1) {
    for (int64_t idx2 = 0; idx2 < 3; ++idx2) {
      for (int64_t idx3 = 0; idx3 < 3; ++idx3) {
        for (int64_t idx4 = 0; idx4 < 3; ++idx4) {
          for (int64_t idx5 = 0; idx5 < 3; ++idx5) {
            all_solutions.insert(
                OrderedAssignment{idx1, idx2, idx3, idx4, idx5});
          }
        }
      }
    }
  }
  CHECK_EQ(solution_count, all_solutions.size());
  IndexManager manager(
      7, 2, {{NodeIndex(0), NodeIndex(5)}, {NodeIndex(2), NodeIndex(6)}});
  Model model(manager);
  model.NextVar(0)->SetValue(1);
  model.NextVar(1)->SetValue(5);
  model.NextVar(2)->SetValue(3);
  model.NextVar(3)->SetValue(4);
  model.NextVar(4)->SetValue(6);
  model.AddConstantDimensionWithSlack(0, 10, 2, true, kDimensionName);
  const Dimension& dimension = model.GetDimensionOrDie(kDimensionName);
  model.CloseModel();
  DecisionBuilder* const db = model.MakeGuidedSlackFinalizer(
      &dimension, [this](int64_t) { return InitializerGuidedFinalizerTest(); });
  SolutionCollector* const collector =
      model.solver()->MakeAllSolutionCollector();
  collector->Add(dimension.slacks());

  model.solver()->Solve(db, collector);

  EXPECT_EQ(solution_count, collector->solution_count());
  absl::flat_hash_set<OrderedAssignment> found_solutions;
  for (int64_t solution_idx = 0; solution_idx < solution_count;
       ++solution_idx) {
    found_solutions.insert(OrderedAssignment{
        collector->Value(solution_idx, dimension.SlackVar(0)),
        collector->Value(solution_idx, dimension.SlackVar(1)),
        collector->Value(solution_idx, dimension.SlackVar(2)),
        collector->Value(solution_idx, dimension.SlackVar(3)),
        collector->Value(solution_idx, dimension.SlackVar(4))});
  }
  EXPECT_EQ(all_solutions, found_solutions);
}

class ShortestTransitionSlackTest : public testing::Test {
 protected:
  // A uniformly zero transit for the interval [0, 20).
  static Model::StateDependentTransit ZeroTransit(int64_t, int64_t) {
    return Model::MakeStateDependentTransit([](int64_t) { return 0; }, 0, 20);
  }
  // At time t, the transit for the i-th node along a route is 0 if t == 5*i or
  // 6 otherwise. There are two routes of length 4 each, so we take i%4 instead.
  static Model::StateDependentTransit HiddenMinimumTransit(
      const IndexManager& manager, int64_t i_index, int64_t) {
    const NodeIndex i = manager.IndexToNode(i_index);
    return Model::MakeStateDependentTransit(
        [i](int64_t x) { return (x != (i.value() % 4) * 5) * 6; }, 0, 100);
  }
  static const char kDimensionName[];
};

const char ShortestTransitionSlackTest::kDimensionName[] = "Time";

// The test creates a simple routing network 0->1->2 with all transits 0.
// In this situation the optimal value for slack[node] is slack[node]->Min() and
// the next test checks this.
TEST_F(ShortestTransitionSlackTest, SingleTransition) {
  IndexManager manager(3, 1, {{NodeIndex(0), NodeIndex(2)}});
  Model model(manager);
  model.AddDimensionDependentDimensionWithVehicleCapacity(
      model.RegisterStateDependentTransitCallback(ZeroTransit), nullptr, 10, 10,
      true, kDimensionName);
  model.CloseModel();

  model.NextVar(0)->SetValue(1);
  model.NextVar(1)->SetValue(2);
  model.VehicleVar(0)->SetValue(0);
  model.VehicleVar(1)->SetValue(0);
  model.VehicleVar(2)->SetValue(0);

  const Dimension& time = model.GetDimensionOrDie(kDimensionName);
  EXPECT_EQ(0, time.ShortestTransitionSlack(0));
  time.SlackVar(0)->SetMin(5);
  time.CumulVar(1)->SetMin(5);
  EXPECT_EQ(5, time.ShortestTransitionSlack(0));
  time.SlackVar(0)->SetValue(6);
  time.CumulVar(1)->SetValue(6);
  time.CumulVar(2)->SetMin(6);
  EXPECT_EQ(0, time.ShortestTransitionSlack(1));
  time.SlackVar(1)->SetValue(2);
  time.CumulVar(2)->SetMin(8);
  EXPECT_EQ(2, time.ShortestTransitionSlack(1));
}

// This test creates two routes:
// 0 -> 1 -> 2 -> 3 -> 8
// 4 -> 5 -> 6 -> 7 -> 9
// With all transits 6, except
//   the transit costs for 0->1 and 4->5 at time 0 are 0
//   the transit costs for 1->2 and 5->6 at time 5 are 0
//   the transit costs for 2->3 and 6->7 at time 10 are 0
//   the transit costs for 4->8 and 7->9 at time 15 are 0.
// The optimal time schedule is:
//   cumul[0] = cumul[4] = 0;    slack[0] = slack[4] = 5
//   cumul[1] = cumul[5] = 5;    slack[1] = slack[5] = 5
//   cumul[2] = cumul[6] = 10;   slack[2] = slack[6] = 5
//   cumul[3] = cumul[7] = 15;   slack[3] = slack[7] = 0
//   cumul[8] = cumul[9] = 15.
//
// ShortestTransitionSlack should be able to find the schedule immediately, so
// the test creates a FirstSolutionCollector, which is supposed to contain the
// optimal solution described above.
TEST_F(ShortestTransitionSlackTest, HiddenGap) {
  static const int64_t kNumNodes = 10;
  static const int64_t kNumVehicles = 2;
  const std::vector<std::pair<NodeIndex, NodeIndex>> kStartEnds = {
      {NodeIndex(0), NodeIndex(8)}, {NodeIndex(4), NodeIndex(9)}};
  ASSERT_EQ(kNumVehicles, kStartEnds.size());
  static const int64_t kNexts[] = {1, 2, 3, 8, 5, 6, 7, 9};
  static_assert(
      kNumNodes - kNumVehicles == std::size(kNexts),
      "The number of nexts differs from the number of internal nodes.");
  static const int64_t kExpectedCumuls[] = {0, 5, 10, 15, 0, 5, 10, 15, 15, 15};
  static const int64_t kExpectedSlacks[] = {5, 5, 5, 0, 5, 5, 5, 0};

  IndexManager manager(kNumNodes, kNumVehicles, kStartEnds);
  Model model(manager);
  model.AddDimensionDependentDimensionWithVehicleCapacity(
      model.RegisterStateDependentTransitCallback(
          absl::bind_front(&HiddenMinimumTransit, manager)),
      nullptr, 99, 99, true, kDimensionName);
  Dimension* const time = model.GetMutableDimension(kDimensionName);
  time->SetSpanCostCoefficientForAllVehicles(1);
  model.CloseModel();

  for (int node = 0; node < std::size(kNexts); ++node) {
    model.NextVar(node)->SetValue(kNexts[node]);
  }
  std::function<int64_t(int64_t)> slack_guide = std::bind(
      &Dimension::ShortestTransitionSlack, time, std::placeholders::_1);
  DecisionBuilder* const guided_finalizer =
      model.MakeGuidedSlackFinalizer(time, slack_guide);
  SolutionCollector* const collector =
      model.solver()->MakeFirstSolutionCollector();
  collector->Add(time->cumuls());
  collector->Add(time->slacks());
  model.solver()->Solve(guided_finalizer, collector);
  EXPECT_EQ(1, collector->solution_count());
  if (collector->solution_count() > 0) {
    for (int node = 0; node < kNumNodes; ++node) {
      EXPECT_EQ(kExpectedCumuls[node],
                collector->Value(0, time->CumulVar(node)));
    }
    for (int node = 0; node < kNumNodes - kNumVehicles; ++node) {
      EXPECT_EQ(kExpectedSlacks[node],
                collector->Value(0, time->SlackVar(node)));
    }
  }
}

class GreedyDescentLSOperatorTest : public testing::Test {
 protected:
  // Unfortunately this private member of GreedyDescentLSOperator has been
  // copy-pasted here.
  // TODO(user): Make this a library function.
  int64_t FindMaxDistance(const std::vector<IntVar*>& variables,
                          const Assignment* assignment) {
    int64_t result = kint64min;
    for (const IntVar* const var : variables) {
      result = std::max(result, std::abs(var->Max() - assignment->Value(var)));
      result = std::max(result, std::abs(var->Min() - assignment->Value(var)));
    }
    return result;
  }

  // Verifies that the delta of the next neighborhood is a single variable, var,
  // with assigned value delta_value.
  void ExpectDelta(LocalSearchOperator* ls_operator, Solver* solver,
                   IntVar* var, int64_t delta_value) {
    ASSERT_NE(nullptr, ls_operator);
    ASSERT_NE(nullptr, solver);
    ASSERT_NE(nullptr, var);
    Assignment* delta = solver->MakeAssignment();
    EXPECT_TRUE(ls_operator->MakeNextNeighbor(delta, nullptr));
    EXPECT_EQ(1, delta->NumIntVars());
    EXPECT_TRUE(delta->Contains(var));
    EXPECT_EQ(0, delta->NumIntervalVars());
    EXPECT_EQ(0, delta->NumSequenceVars());
    EXPECT_EQ(delta_value, delta->Value(var));
  }

  // Verifies that the local search operator has reached the end of the
  // neighborhood.
  void ExpectEndOfNeighborhood(LocalSearchOperator* ls_operator,
                               Solver* solver) {
    EXPECT_FALSE(
        ls_operator->MakeNextNeighbor(solver->MakeAssignment(), nullptr));
  }

  // Verifies that GreedyDescentLSOperator generates the expected neighborhood
  // for the variables with the given initial assignment.
  void CheckGeneratedNeighborhood(const std::vector<IntVar*>& variables,
                                  const Assignment* assignment,
                                  Solver* solver) {
    static const int64_t kSigns[] = {1, -1};
    const std::unique_ptr<LocalSearchOperator> ls_operator =
        Model::MakeGreedyDescentLSOperator(variables);
    ls_operator->Start(assignment);

    for (int64_t step = FindMaxDistance(variables, assignment); step >= 1;
         step /= 2) {
      for (IntVar* const variable : variables) {
        for (int64_t sign_idx = 0; sign_idx < 2; ++sign_idx) {
          const int64_t delta_value = kSigns[sign_idx] * step;
          const int64_t new_value = assignment->Value(variable) + delta_value;
          if (variable->Contains(new_value)) {
            ExpectDelta(ls_operator.get(), solver, variable, new_value);
          }
        }
      }
    }
    ExpectEndOfNeighborhood(ls_operator.get(), solver);
  }
};

TEST_F(GreedyDescentLSOperatorTest, EmptyVector) {
  Solver solver("Empty vector test");
  const std::unique_ptr<LocalSearchOperator> ls_operator =
      Model::MakeGreedyDescentLSOperator({});
  ls_operator->Start(solver.MakeAssignment());
  ExpectEndOfNeighborhood(ls_operator.get(), &solver);
}

TEST_F(GreedyDescentLSOperatorTest, OneVariable) {
  static const int64_t kNumDeltas = 5;
  static const int64_t kDeltas[] = {16, 8, 4, 2, 1};
  ASSERT_EQ(kNumDeltas, std::size(kDeltas));

  Solver solver("One variable test");
  IntVar* const var = solver.MakeIntVar(0, 16);
  Assignment* const initial_assignment = solver.MakeAssignment();
  initial_assignment->Add(var);
  initial_assignment->SetValue(var, 0);
  const std::unique_ptr<LocalSearchOperator> ls_operator =
      Model::MakeGreedyDescentLSOperator({var});
  ls_operator->Start(initial_assignment);

  for (int64_t delta_idx = 0; delta_idx < kNumDeltas; ++delta_idx) {
    ExpectDelta(ls_operator.get(), &solver, var, kDeltas[delta_idx]);
  }
  ExpectEndOfNeighborhood(ls_operator.get(), &solver);
}

TEST_F(GreedyDescentLSOperatorTest, SequentialStarts) {
  Solver solver("Sequential starts");
  IntVar* const var = solver.MakeIntVar(0, 2);
  Assignment* const initial_assignment = solver.MakeAssignment();
  initial_assignment->Add(var);
  const std::unique_ptr<LocalSearchOperator> ls_operator =
      Model::MakeGreedyDescentLSOperator({var});

  // Initial value 0.
  initial_assignment->SetValue(var, 0);
  ls_operator->Start(initial_assignment);
  ExpectDelta(ls_operator.get(), &solver, var, 2);
  ExpectDelta(ls_operator.get(), &solver, var, 1);
  ExpectEndOfNeighborhood(ls_operator.get(), &solver);

  // Initial value 1.
  initial_assignment->SetValue(var, 1);
  ls_operator->Start(initial_assignment);
  ExpectDelta(ls_operator.get(), &solver, var, 2);
  ExpectDelta(ls_operator.get(), &solver, var, 0);
  ExpectEndOfNeighborhood(ls_operator.get(), &solver);

  // Initial value 2.
  initial_assignment->SetValue(var, 2);
  ls_operator->Start(initial_assignment);
  ExpectDelta(ls_operator.get(), &solver, var, 0);
  ExpectDelta(ls_operator.get(), &solver, var, 1);
  ExpectEndOfNeighborhood(ls_operator.get(), &solver);
}

TEST_F(GreedyDescentLSOperatorTest, HomogeneousVariables) {
  static const int64_t kNumberVars = 10;
  Solver solver("Homogeneous variables test");
  std::vector<IntVar*> vars(kNumberVars, nullptr);
  Assignment* const initial_assignment = solver.MakeAssignment();
  for (int64_t var_idx = 0; var_idx < kNumberVars; ++var_idx) {
    vars[var_idx] = solver.MakeIntVar(-10, 10);
    initial_assignment->Add(vars[var_idx]);
    initial_assignment->SetValue(vars[var_idx], 0);
  }
  CheckGeneratedNeighborhood(vars, initial_assignment, &solver);
}

TEST_F(GreedyDescentLSOperatorTest, NonHomogeneousVariables) {
  static const int64_t kNumVars = 4;
  static const int64_t kDomainStarts[] = {-20, 5, 10, -100};
  static const int64_t kDomainEnds[] = {-10, 5, 20, 100};
  static const int64_t kInitialValues[] = {30, 5, 15, 0};
  static_assert(kNumVars == std::size(kDomainStarts),
                "The number of variables differs from the number of domains.");
  static_assert(kNumVars == std::size(kDomainEnds),
                "The number of variables differs from the number of domains.");
  static_assert(
      kNumVars == std::size(kInitialValues),
      "The number of variables differs from the number of initial values.");

  Solver solver("Non-homogeneous variables test");
  std::vector<IntVar*> vars(kNumVars, nullptr);
  Assignment* const initial_assignment = solver.MakeAssignment();
  for (int64_t var_idx = 0; var_idx < kNumVars; ++var_idx) {
    vars[var_idx] =
        solver.MakeIntVar(kDomainStarts[var_idx], kDomainEnds[var_idx]);
    initial_assignment->Add(vars[var_idx]);
    initial_assignment->SetValue(vars[var_idx], kInitialValues[var_idx]);
  }
  CheckGeneratedNeighborhood(vars, initial_assignment, &solver);
}

// Solver parameter tests checking parameters from the routing parameter proto
// get passed properly to the solver.

TEST(SolverParameterTest, NoOverride) {
  const NodeIndex node(0);
  ConstraintSolverParameters parameters = Solver::DefaultSolverParameters();
  parameters.set_skip_locally_optimal_paths(true);
  IndexManager manager(1, 1, NodeIndex(0));
  EXPECT_THAT(Model(manager).solver()->parameters(), EqualsProto(parameters));

  IndexManager manager_2(1, 1, {{node, node}});
  EXPECT_THAT(Model(manager_2).solver()->parameters(), EqualsProto(parameters));

  IndexManager manager_3(1, 1, {node}, {node});
  EXPECT_THAT(Model(manager_3).solver()->parameters(), EqualsProto(parameters));
}

TEST(SolverParameterTest, Override) {
  const NodeIndex node(0);
  RoutingModelParameters routing_parameters = DefaultRoutingModelParameters();
  ConstraintSolverParameters* parameters =
      routing_parameters.mutable_solver_parameters();
  *parameters = Solver::DefaultSolverParameters();
  parameters->set_trace_search(true);
  IndexManager manager(1, 1, node);
  EXPECT_THAT(Model(manager, routing_parameters).solver()->parameters(),
              EqualsProto(*parameters));

  IndexManager manager_2(1, 1, {{node, node}});
  EXPECT_THAT(Model(manager_2, routing_parameters).solver()->parameters(),
              EqualsProto(*parameters));

  IndexManager manager_3(1, 1, {node}, {node});
  EXPECT_THAT(Model(manager_3, routing_parameters).solver()->parameters(),
              EqualsProto(*parameters));
}

// The Next test checks if the self dependent dimension finalizer works well.
// To this end it creates five identical routes:
// 0->1->10
// 2->3->11
// 4->5->12
// 6->7->13
// 8->9->14
// with maximum slack and starting cumul 5.
// We are going to describe only the transits 0->1->10:
//
//       from 0 to 1                   from 1 to 10
//    t in {0, 4}  -> 40               t >= 6  -> 0
//    t in {1, 3}  -> 20               o/w    -> 10
//    t = 2        ->  0
//
// A brief analysis reveals that the optimal time shedule is
// cumul[0] = 2     slack[0] = 4
// cumul[1] = 6     slack[1] = 0
// cumul[10] = 6.
// Moreover, the cost function is convex w.r.t. cumul[0], so the local search
// must find the global minimum. Therefore we expect the finalizer to recognize
// the shedule above as the unique optimum solution.
class MakeSelfDependentDimensionFinalizerTest : public testing::Test {
 protected:
  static Model::StateDependentTransit Transit(const IndexManager& manager,
                                              int64_t i_index, int64_t) {
    const NodeIndex i = manager.IndexToNode(i_index);
    const std::function<int64_t(int64_t)> transit = (i.value() % 2) == 0 ?
        [] (int64_t t) { return (t == 0 || t == 4)*40 + ( t== 1 || t == 3)*20; }
        : [] (int64_t t) { return (t < 6)*10; };
    return Model::MakeStateDependentTransit(transit, 0, kCapacity + 1);
  }
  static const int64_t kNumNodes;
  static const int64_t kNumVehicles;
  static const int64_t kMaxSlack;
  static const int64_t kCapacity;
  static const int64_t kNexts[];
  static const int64_t kExpectedCumuls[];
  static const int64_t kExpectedSlacks[];
  static const char kDimensionName[];
};

const int64_t MakeSelfDependentDimensionFinalizerTest::kNumNodes = 15;
const int64_t MakeSelfDependentDimensionFinalizerTest::kNumVehicles = 5;
const int64_t MakeSelfDependentDimensionFinalizerTest::kMaxSlack = 4;
const int64_t MakeSelfDependentDimensionFinalizerTest::kCapacity = 50;
const int64_t MakeSelfDependentDimensionFinalizerTest::kNexts[] = {
    1, 10, 3, 11, 5, 12, 7, 13, 9, 14};
const int64_t MakeSelfDependentDimensionFinalizerTest::kExpectedCumuls[] = {
    2, 6, 2, 6, 2, 6, 2, 6, 2, 6, 6, 6, 6, 6, 6};
const int64_t MakeSelfDependentDimensionFinalizerTest::kExpectedSlacks[] = {
    4, 0, 4, 0, 4, 0, 4, 0, 4, 0};
const char MakeSelfDependentDimensionFinalizerTest::kDimensionName[] = "Time";

TEST_F(MakeSelfDependentDimensionFinalizerTest, GeneralTest) {
  static_assert(
      kNumNodes - kNumVehicles == std::size(kNexts),
      "The number of internal nodes differs from the number of nexts.");
  static_assert(kNumNodes == std::size(kExpectedCumuls),
                "The number of nodes differs from the expected cumuls.");
  static_assert(kNumNodes - kNumVehicles == std::size(kExpectedSlacks),
                "The number of nodes differs from the expected slacks.");
  const std::vector<std::pair<NodeIndex, NodeIndex>> kStartEnds = {
      {NodeIndex(0), NodeIndex(10)},
      {NodeIndex(2), NodeIndex(11)},
      {NodeIndex(4), NodeIndex(12)},
      {NodeIndex(6), NodeIndex(13)},
      {NodeIndex(8), NodeIndex(14)}};
  ASSERT_EQ(kNumVehicles, kStartEnds.size());

  IndexManager manager(kNumNodes, kNumVehicles, kStartEnds);
  Model model(manager);
  model.AddDimensionDependentDimensionWithVehicleCapacity(
      model.RegisterStateDependentTransitCallback(
          absl::bind_front(&Transit, manager)),
      nullptr, kMaxSlack, kCapacity, false, kDimensionName);
  const Dimension& time = model.GetDimensionOrDie(kDimensionName);
  for (int64_t route_idx = 0; route_idx < kNumVehicles; ++route_idx) {
    time.CumulVar(model.Start(route_idx))->SetMax(kMaxSlack);
  }
  model.CloseModel();

  for (int node = 0; node < kNumNodes - kNumVehicles; ++node) {
    model.NextVar(node)->SetValue(kNexts[node]);
  }
  DecisionBuilder* const finalizer =
      model.MakeSelfDependentDimensionFinalizer(&time);
  std::vector<IntVar*> end_cumuls(kNumVehicles, nullptr);
  for (int64_t route_idx = 0; route_idx < kNumVehicles; ++route_idx) {
    end_cumuls[route_idx] = time.CumulVar(model.End(route_idx));
  }
  IntVar* const cost = model.solver()->MakeSum(end_cumuls)->Var();
  OptimizeVar* const min_cost_monitor = model.solver()->MakeMinimize(cost, 1);
  Assignment* const cost_assignment = model.solver()->MakeAssignment();
  cost_assignment->AddObjective(cost);
  SolutionCollector* const collector =
      model.solver()->MakeBestValueSolutionCollector(cost_assignment, false);
  collector->Add(time.cumuls());
  collector->Add(time.slacks());

  // TODO(user): Invoke Model::Solve instead.
  model.solver()->Solve(finalizer, collector, min_cost_monitor);
  EXPECT_GE(collector->solution_count(), 1);
  if (collector->solution_count() > 0) {
    for (int node = 0; node < kNumNodes; ++node) {
      EXPECT_EQ(kExpectedCumuls[node],
                collector->Value(0, time.CumulVar(node)));
    }
    for (int node = 0; node < std::size(kExpectedSlacks); ++node) {
      EXPECT_EQ(kExpectedSlacks[node],
                collector->Value(0, time.SlackVar(node)));
    }
  }
}

int64_t ReturnValueFromVector(
    const std::vector<std::vector<int64_t>>* distances, NodeIndex i,
    NodeIndex j) {
  return (*distances)[i.value()][j.value()];
}

TEST(ModelTest, ConstrainedTsp) {
  struct Edge {
    int from;
    int to;
    int64_t distance;
  };
  constexpr Edge kEdges[] = {
      {0, 1, 20},    {0, 2, 34},    {0, 3, 68},    {0, 4, 137},   {0, 5, 173},
      {0, 6, 170},   {0, 7, 192},   {0, 8, 197},   {0, 9, 241},   {0, 10, 246},
      {0, 11, 268},  {0, 12, 238},  {1, 2, 36},    {0, 13, 187},  {0, 14, 218},
      {0, 15, 292},  {0, 16, 267},  {0, 17, 284},  {0, 18, 210},  {0, 19, 231},
      {0, 20, 209},  {0, 21, 134},  {0, 22, 124},  {2, 3, 33},    {0, 23, 98},
      {0, 24, 121},  {0, 25, 177},  {0, 26, 186},  {0, 27, 257},  {0, 28, 243},
      {0, 29, 237},  {0, 30, 181},  {0, 31, 71},   {0, 32, 13},   {1, 22, 104},
      {0, 33, 15},   {3, 4, 83},    {0, 34, 158},  {0, 35, 267},  {0, 36, 293},
      {0, 37, 267},  {0, 38, 293},  {1, 32, 23},   {4, 2, 96},    {4, 5, 83},
      {5, 3, 121},   {5, 7, 52},    {6, 1, 151},   {3, 32, 80},   {3, 33, 89},
      {6, 5, 32},    {6, 8, 64},    {5, 22, 78},   {4, 33, 124},  {4, 34, 25},
      {7, 6, 40},    {6, 21, 121},  {6, 22, 73},   {7, 12, 65},   {8, 4, 84},
      {8, 5, 58},    {8, 7, 32},    {8, 9, 61},    {8, 10, 66},   {8, 12, 88},
      {6, 34, 101},  {9, 6, 92},    {9, 7, 52},    {9, 10, 31},   {7, 34, 112},
      {10, 7, 53},   {10, 11, 63},  {11, 7, 93},   {11, 9, 84},   {9, 37, 128},
      {12, 9, 58},   {12, 11, 27},  {10, 35, 99},  {10, 36, 82},  {10, 38, 119},
      {13, 16, 96},  {13, 20, 81},  {13, 21, 83},  {14, 13, 38},  {14, 17, 87},
      {14, 18, 54},  {15, 11, 177}, {14, 21, 97},  {15, 14, 99},  {15, 16, 69},
      {16, 14, 64},  {16, 17, 38},  {17, 15, 65},  {16, 26, 225}, {17, 19, 83},
      {17, 26, 223}, {18, 17, 66},  {18, 19, 38},  {18, 20, 51},  {18, 21, 97},
      {18, 26, 172}, {21, 1, 144},  {20, 14, 46},  {19, 25, 83},  {20, 16, 66},
      {20, 17, 83},  {19, 27, 214}, {20, 19, 68},  {20, 23, 107}, {22, 7, 112},
      {21, 20, 64},  {21, 24, 78},  {22, 20, 148}, {22, 21, 98},  {22, 23, 94},
      {23, 21, 57},  {24, 19, 145}, {24, 23, 49},  {24, 26, 122}, {25, 21, 79},
      {25, 23, 77},  {25, 24, 60},  {26, 15, 288}, {25, 26, 126}, {26, 19, 166},
      {26, 27, 91},  {26, 29, 111}, {27, 28, 68},  {28, 19, 259}, {28, 30, 85},
      {31, 1, 75},   {29, 27, 96},  {29, 28, 60},  {30, 19, 185}, {29, 30, 70},
      {29, 31, 162}, {32, 2, 47},   {30, 26, 58},  {30, 27, 86},  {33, 1, 34},
      {31, 21, 77},  {33, 2, 44},   {31, 23, 44},  {31, 24, 60},  {31, 25, 101},
      {31, 28, 192}, {31, 30, 130}, {31, 32, 59},  {32, 23, 85},  {34, 3, 111},
      {32, 24, 107}, {34, 5, 67},   {34, 8, 60},   {33, 23, 112}, {35, 8, 75},
      {35, 9, 116},  {33, 31, 84},  {33, 32, 27},  {36, 9, 108},  {36, 11, 44},
      {36, 15, 204}, {37, 11, 55},  {37, 12, 79},  {37, 15, 207}, {35, 36, 158},
      {38, 11, 77},  {38, 15, 201}, {36, 37, 50},  {37, 38, 26},  {38, 36, 73},
  };
  std::vector<std::vector<int64_t>> distances;
  int size = 0;
  for (const auto& [from, to, d] : kEdges) {
    size = std::max(size, std::max(to, from));
    if (from >= distances.size()) distances.resize(from + 1);
    if (to >= distances[from].size()) distances[from].resize(to + 1, kint64max);
    distances[from][to] = d;
    if (to >= distances.size()) distances.resize(to + 1);
    if (from >= distances[to].size()) distances[to].resize(from + 1, kint64max);
    distances[to][from] = d;
  }
  distances.resize(size);
  for (std::vector<int64_t>& distance : distances) {
    distance.resize(size, kint64max);
  }
  IndexManager manager(distances.size(), 1, NodeIndex(0));
  Model model(manager);
  const int vehicle_cost = model.RegisterTransitCallback(
      [&distances, &manager](int64_t i, int64_t j) {
        return ReturnValueFromVector(&distances, manager.IndexToNode(i),
                                     manager.IndexToNode(j));
      });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(FirstSolutionStrategy::SAVINGS);
  parameters.mutable_local_search_operators()->set_use_tsp_opt(BOOL_TRUE);
  parameters.mutable_local_search_operators()->set_use_tsp_lns(BOOL_TRUE);
  parameters.set_solution_limit(7);
  const Assignment* const solution = model.SolveWithParameters(parameters);
  EXPECT_GE(2472, solution->ObjectiveValue());
}

// Testing collecting the best 3 solutions. The model contains 10 optional nodes
// each with a penalty cost of 1. The initial solution makes all nodes
// unperformed. Local search will make each node performed one by one. Therefore
// the best 3 solutions will have 2, 1 and 0 unperformed nodes with respective
// solution costs of 2, 1, and 0.
TEST(ModelTest, MultipleSolutions) {
  const int kNumberofNodes = 10;
  const int kNumberOfSolutions = 3;
  IndexManager manager(kNumberofNodes, 1, NodeIndex(0));
  Model model(manager);
  for (int i = 1; i < kNumberofNodes; ++i) {
    model.AddDisjunction({i}, 1);
  }
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::ALL_UNPERFORMED);
  parameters.set_number_of_solutions_to_collect(kNumberOfSolutions);
  std::vector<const Assignment*> solutions;
  const Assignment* const solution =
      model.SolveWithParameters(parameters, &solutions);
  EXPECT_EQ(kNumberOfSolutions, solutions.size());
  EXPECT_EQ(solution, solutions.back());
  for (int i = 0; i < kNumberOfSolutions; ++i) {
    EXPECT_EQ(kNumberOfSolutions - i - 1, solutions[i]->ObjectiveValue());
  }
  // Testing that previous solutions don't get deleted after a second solve.
  std::vector<const Assignment*> new_solutions;
  const Assignment* const new_solution =
      model.SolveWithParameters(parameters, &new_solutions);
  EXPECT_EQ(new_solution, new_solutions.back());
  // Check that the old solutions are still accessible.
  CHECK(!solution->DebugString().empty());
  for (const Assignment* s : solutions) {
    CHECK(!s->DebugString().empty());
  }
}

TEST(ModelTest, PickupDeliverySiblings) {
  const int kNumNodes = 9;
  const int kNumVehicles = 2;
  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  model.AddPickupAndDelivery(2, 4);
  model.CloseModel();

  EXPECT_EQ(model.GetFirstMatchingPickupDeliverySibling(
                1, [](int64_t) { return true; }),
            (std::optional<int64_t>{}));

  EXPECT_EQ(model.GetFirstMatchingPickupDeliverySibling(
                2, [](int64_t) { return true; }),
            (std::optional<int64_t>{4}));
  EXPECT_EQ(model.GetFirstMatchingPickupDeliverySibling(
                4, [](int64_t) { return true; }),
            (std::optional<int64_t>{2}));

  EXPECT_EQ(model.GetFirstMatchingPickupDeliverySibling(
                2, [](int64_t) { return false; }),
            (std::optional<int64_t>{}));
  EXPECT_EQ(model.GetFirstMatchingPickupDeliverySibling(
                4, [](int64_t) { return false; }),
            (std::optional<int64_t>{}));
}

TEST(ModelTest, AlternativePickupDeliverySiblings) {
  const int kNumNodes = 9;
  const int kNumVehicles = 2;
  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  PickupDeliveryPair pickup_delivery_pair = {{2, 3, 4}, {6, 7, 8}};

  const DisjunctionIndex pickup_disjunction(
      model.AddDisjunction(pickup_delivery_pair.pickup_alternatives));
  const DisjunctionIndex delivery_disjunction(
      model.AddDisjunction(pickup_delivery_pair.delivery_alternatives));

  model.AddPickupAndDeliverySets(pickup_disjunction, delivery_disjunction);

  model.CloseModel();

  EXPECT_EQ(model.GetFirstMatchingPickupDeliverySibling(
                2, [](int64_t node) { return node >= 7; }),
            (std::optional<int64_t>{7}));
  EXPECT_EQ(model.GetFirstMatchingPickupDeliverySibling(
                6, [](int64_t node) { return node > 3; }),
            (std::optional<int64_t>{4}));
}

// Testing solver on "pure" assignment problems which will trigger the usage of
// the min-cost flow solver.

enum PenalizationMode { NONE_PENALIZED, SOME_PENALIZED, ALL_PENALIZED };

// Model with 2 nodes, 2 vehicles which can visit a single node. The first
// node route costs (0+2+0)*(vehicles-vehicle), the second node route costs
// (0+4+0)*(vehicles-vehicle). The optimal solution performs both nodes,
// first node on first vehicle, second node on second vehicle.
TEST(RoutingFlowAssignmentTest, AssignVisitsToVehicles) {
  const int vehicles = 2;
  IndexManager manager(3, vehicles, NodeIndex(0));
  int num_solutions = 0;

  auto create_model = [&manager, &num_solutions](PenalizationMode mode) {
    auto model = std::make_unique<Model>(manager);
    for (int vehicle = 0; vehicle < vehicles; ++vehicle) {
      const int vehicle_cost = model->RegisterTransitCallback(
          [&manager, vehicle](int64_t i, int64_t) {
            return (vehicles - vehicle) * manager.IndexToNode(i).value() * 2;
          });
      model->SetArcCostEvaluatorOfVehicle(vehicle_cost, vehicle);
    }
    model->AddDimension(
        model->RegisterUnaryTransitCallback([m = model.get()](int64_t i) {
          if (m->IsStart(i) || m->IsEnd(i)) {
            return 0;
          }
          return 1;
        }),
        1, 1, true, "count");
    for (int i = 0; i < model->Size(); ++i) {
      if (mode == NONE_PENALIZED || (mode == SOME_PENALIZED && i % 2 == 0))
        continue;
      if (!model->IsStart(i) && !model->IsEnd(i)) {
        model->AddDisjunction({i}, Model::kNoPenalty, 1);
      }
    }
    num_solutions = 0;
    model->AddAtSolutionCallback([&num_solutions]() { ++num_solutions; });
    return model;
  };

  for (PenalizationMode mode :
       {NONE_PENALIZED, SOME_PENALIZED, ALL_PENALIZED}) {
    // Setting parameters to make normal search fail as nodes are mandatory.
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(
        FirstSolutionStrategy::ALL_UNPERFORMED);
    const std::vector<int64_t> expected_solution = {1, 4, 5, 2};
    {
      std::unique_ptr<Model> model = create_model(mode);
      const Assignment* assignment = model->SolveWithParameters(parameters);
      ASSERT_NE(assignment, nullptr);
      EXPECT_EQ(1, num_solutions);
      EXPECT_EQ(2 * 2 + 1 * 4, assignment->ObjectiveValue());
      for (int i = 0; i < model->Nexts().size(); ++i) {
        EXPECT_EQ(expected_solution[i], assignment->Value(model->Nexts()[i]));
      }
      const SubSolverStatistics& stats = model->GetSubSolverStatistics();
      EXPECT_EQ(stats.num_glop_calls_in_lp_scheduling(), 0);
      EXPECT_EQ(stats.num_cp_sat_calls_in_lp_scheduling(), 0);
      EXPECT_EQ(stats.num_min_cost_flow_calls(), 1);
    }
    {
      // Forbid previous solution to find second best solution.
      // NOTE: As the model is already Closed(), we use another model to set the
      // allowed vehicles on nodes.
      std::unique_ptr<Model> model = create_model(mode);
      for (int i = 0; i < model->vehicles(); ++i) {
        std::vector<int> allowed;
        for (int j = 0; j < model->vehicles(); ++j) {
          if (i != j) allowed.push_back(j);
        }
        model->SetAllowedVehiclesForIndex(allowed,
                                          expected_solution[model->Start(i)]);
      }
      const Assignment* assignment = model->SolveWithParameters(parameters);
      ASSERT_NE(assignment, nullptr);
      EXPECT_EQ(1, num_solutions);
      EXPECT_EQ(2 * 4 + 1 * 2, assignment->ObjectiveValue());
      const std::vector<int64_t> new_expected_solution = {2, 5, 4, 1};
      for (int i = 0; i < model->Nexts().size(); ++i) {
        EXPECT_EQ(new_expected_solution[i],
                  assignment->Value(model->Nexts()[i]));
      }
      const SubSolverStatistics& stats = model->GetSubSolverStatistics();
      EXPECT_EQ(stats.num_glop_calls_in_lp_scheduling(), 0);
      EXPECT_EQ(stats.num_cp_sat_calls_in_lp_scheduling(), 0);
      EXPECT_EQ(stats.num_min_cost_flow_calls(), 1);
    }
  }
}

// Model with 2 nodes, 2 vehicles which can visit a single node. The first
// node route costs 1 + 1 (cost) + (10 + 10 - 0) * 10 (dimension soft cost), the
// second node route costs 1 + 1 (cost) + (1 + 10 - 6) * 30 (dimension soft
// cost). The optimal solution performs both nodes, first node on first vehicle,
// second node on second vehicle.
TEST(RoutingFlowAssignmentTest, AssignVisitsWithSoftCostsToVehicles) {
  const int vehicles = 2;
  IndexManager manager(3, vehicles, NodeIndex(0));
  Model model(manager);
  for (int vehicle = 0; vehicle < vehicles; ++vehicle) {
    const int vehicle_cost =
        model.RegisterTransitCallback([](int64_t, int64_t) { return 1; });
    model.SetArcCostEvaluatorOfVehicle(vehicle_cost, vehicle);
  }
  model.AddDimension(model.RegisterUnaryTransitCallback([&model](int64_t i) {
    if (model.IsStart(i) || model.IsEnd(i)) {
      return 0;
    }
    return 1;
  }),
                     1, 1, true, "count");
  model.AddDimension(
      model.RegisterTransitCallback([](int64_t, int64_t) { return 10; }), 100,
      100, false, "time");
  model.GetMutableDimension("time")->CumulVar(model.Start(0))->SetMin(10);
  model.GetMutableDimension("time")->CumulVar(model.Start(1))->SetMin(1);
  model.GetMutableDimension("time")->SetCumulVarSoftUpperBound(1, 0, 10);
  model.GetMutableDimension("time")->SetCumulVarSoftUpperBound(2, 6, 30);

  int num_solutions = 0;
  model.AddAtSolutionCallback([&num_solutions]() { ++num_solutions; });

  // Setting parameters to make normal search fail as nodes are mandatory.
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::ALL_UNPERFORMED);
  const Assignment* assignment = model.SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(1, num_solutions);
  EXPECT_EQ(2 + 2 + 10 * 20 + 30 * 5, assignment->ObjectiveValue());
  const std::vector<int64_t> expected_solution = {1, 4, 5, 2};
  for (int i = 0; i < model.Nexts().size(); ++i) {
    EXPECT_EQ(expected_solution[i], assignment->Value(model.Nexts()[i]));
  }
}

// Model with 2 nodes in a single disjunction, 2 vehicles which can visit a
// single node. The best solution performs the first node on the second vehicle,
// leaving the first vehicle and the second node unperformed.
TEST(RoutingFlowAssignmentTest, AssignVisitAlternatesToVehicles) {
  const int vehicles = 2;
  IndexManager manager(3, vehicles, NodeIndex(0));
  Model model(manager);
  for (int vehicle = 0; vehicle < vehicles; ++vehicle) {
    const int vehicle_cost =
        model.RegisterTransitCallback([&manager, vehicle](int64_t i, int64_t) {
          return (vehicles - vehicle) * manager.IndexToNode(i).value() * 2;
        });
    model.SetArcCostEvaluatorOfVehicle(vehicle_cost, vehicle);
  }
  model.AddDimension(model.RegisterUnaryTransitCallback([&model](int64_t i) {
    if (model.IsStart(i) || model.IsEnd(i)) {
      return 0;
    }
    return 1;
  }),
                     1, 1, true, "count");
  model.AddDisjunction({1, 2}, Model::kNoPenalty, 1);

  // Setting parameters to make normal search fail as nodes are mandatory.
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::ALL_UNPERFORMED);
  const Assignment* assignment = model.SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(2, assignment->ObjectiveValue());
  const std::vector<int64_t> expected_solution = {4, 5, 2, 1};
  for (int i = 0; i < model.Nexts().size(); ++i) {
    EXPECT_EQ(expected_solution[i], assignment->Value(model.Nexts()[i]));
  }
}

// Model with 2 pickup and delivery pairs, 2 vehicles which can visit a single
// pair. The first pair route costs (0+1+2+0)*(vehicles-vehicle), the second
// pair route costs (0+3+4+0)*(vehicles-vehicle). The optimal solution performs
// both pairs, first pair on first vehicle, second pair on second vehicle.
TEST(RoutingFlowAssignmentTest, AssignPickupAndDeliveriesToVehicles) {
  const int vehicles = 2;
  IndexManager manager(5, vehicles, NodeIndex(0));
  auto create_model = [&manager](PenalizationMode mode) {
    auto model = std::make_unique<Model>(manager);
    for (int vehicle = 0; vehicle < vehicles; ++vehicle) {
      const int vehicle_cost = model->RegisterTransitCallback(
          [&manager, vehicle](int64_t i, int64_t) {
            return (vehicles - vehicle) * manager.IndexToNode(i).value();
          });
      model->SetArcCostEvaluatorOfVehicle(vehicle_cost, vehicle);
    }
    model->AddDimension(model->RegisterUnaryTransitCallback(
                            [m = model.get(), &manager](int64_t i) {
                              if (m->IsStart(i) || m->IsEnd(i)) return 0;
                              if (manager.IndexToNode(i) % 2 == 1) return 1;
                              return -1;
                            }),
                        1, 1, true, "demand");
    Solver* const solver = model->solver();
    model->AddDimension(
        model->RegisterUnaryTransitCallback([](int64_t) { return 1; }), 10, 10,
        true, "rank");
    absl::flat_hash_set<int> pickups;
    const Dimension& rank = model->GetDimensionOrDie("rank");
    for (NodeIndex i(1); i < NodeIndex(5); i += 2) {
      const int pickup = manager.NodeToIndex(i);
      const int delivery = manager.NodeToIndex(i + 1);
      pickups.insert(pickup);
      solver->AddConstraint(solver->MakeEquality(model->VehicleVar(pickup),
                                                 model->VehicleVar(delivery)));
      solver->AddConstraint(solver->MakeLessOrEqual(rank.CumulVar(pickup),
                                                    rank.CumulVar(delivery)));
      model->AddPickupAndDelivery(pickup, delivery);
    }
    model->AddDimension(
        model->RegisterUnaryTransitCallback(
            [m = model.get(), pickups](int64_t i) {
              if (m->IsStart(i) || m->IsEnd(i) || pickups.count(i) == 0) {
                return 0;
              }
              return 1;
            }),
        1, 1, true, "count");
    for (int i = 0; i < model->Size(); ++i) {
      if (mode == NONE_PENALIZED || (mode == SOME_PENALIZED && i % 2 == 0))
        continue;
      if (!model->IsStart(i) && !model->IsEnd(i)) {
        model->AddDisjunction({i}, Model::kNoPenalty, 1);
      }
    }
    return model;
  };

  for (PenalizationMode mode :
       {NONE_PENALIZED, SOME_PENALIZED, ALL_PENALIZED}) {
    // Setting parameters to make normal search fail as nodes are mandatory.
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(
        FirstSolutionStrategy::ALL_UNPERFORMED);

    const std::vector<int64_t> expected_solution = {1, 2, 6, 4, 7, 3};
    {
      std::unique_ptr<Model> model = create_model(mode);
      const Assignment* assignment = model->SolveWithParameters(parameters);
      ASSERT_NE(assignment, nullptr);
      EXPECT_EQ(2 * 3 + 1 * 7, assignment->ObjectiveValue());
      for (int i = 0; i < model->Nexts().size(); ++i) {
        EXPECT_EQ(expected_solution[i], assignment->Value(model->Nexts()[i]));
      }
    }

    {
      // Forbid previous solution to find second best solution.
      // NOTE: As the model is already Closed(), we use another model to set the
      // allowed vehicles on nodes.
      std::unique_ptr<Model> model = create_model(mode);
      for (int i = 0; i < model->vehicles(); ++i) {
        std::vector<int> allowed;
        for (int j = 0; j < model->vehicles(); ++j) {
          if (i != j) allowed.push_back(j);
        }
        model->SetAllowedVehiclesForIndex(allowed,
                                          expected_solution[model->Start(i)]);
      }
      const Assignment* assignment = model->SolveWithParameters(parameters);
      ASSERT_NE(assignment, nullptr);
      EXPECT_EQ(2 * 7 + 1 * 3, assignment->ObjectiveValue());
      const std::vector<int64_t> new_expected_solution = {3, 2, 7, 4, 6, 1};
      for (int i = 0; i < model->Nexts().size(); ++i) {
        EXPECT_EQ(new_expected_solution[i],
                  assignment->Value(model->Nexts()[i]));
      }
    }
  }
}

// Model with 2 pickup and delivery pairs, 2 vehicles which can visit a single
// pair. The first pair route costs 3 + 20 * 10 + 24 * 30, the second pair route
// costs 3 + 11 * 10 + 15 * 30. The optimal solution performs both pairs, first
// pair on first vehicle, second pair on second vehicle.
TEST(RoutingFlowAssignmentTest,
     AssignPickupAndDeliveriesWithSoftCostsToVehicles) {
  const int vehicles = 2;
  IndexManager manager(5, vehicles, NodeIndex(0));
  Model model(manager);
  for (int vehicle = 0; vehicle < vehicles; ++vehicle) {
    const int vehicle_cost =
        model.RegisterTransitCallback([](int64_t, int64_t) { return 1; });
    model.SetArcCostEvaluatorOfVehicle(vehicle_cost, vehicle);
  }
  model.AddDimension(
      model.RegisterUnaryTransitCallback([&model, &manager](int64_t i) {
        if (model.IsStart(i) || model.IsEnd(i)) return 0;
        if (manager.IndexToNode(i) % 2 == 1) return 1;
        return -1;
      }),
      1, 1, true, "demand");
  Solver* const solver = model.solver();
  model.AddDimension(
      model.RegisterUnaryTransitCallback([](int64_t) { return 1; }), 10, 10,
      true, "rank");
  absl::flat_hash_set<int> pickups;
  const Dimension& rank = model.GetDimensionOrDie("rank");
  for (NodeIndex i(1); i < NodeIndex(5); i += 2) {
    const int pickup = manager.NodeToIndex(i);
    const int delivery = manager.NodeToIndex(i + 1);
    pickups.insert(pickup);
    solver->AddConstraint(solver->MakeEquality(model.VehicleVar(pickup),
                                               model.VehicleVar(delivery)));
    solver->AddConstraint(solver->MakeLessOrEqual(rank.CumulVar(pickup),
                                                  rank.CumulVar(delivery)));
    model.AddPickupAndDelivery(pickup, delivery);
  }
  model.AddDimension(
      model.RegisterUnaryTransitCallback([&model, &pickups](int64_t i) {
        if (model.IsStart(i) || model.IsEnd(i) || pickups.count(i) == 0) {
          return 0;
        }
        return 1;
      }),
      1, 1, true, "count");
  model.AddDimension(
      model.RegisterTransitCallback([](int64_t, int64_t) { return 10; }), 100,
      100, false, "time");
  model.GetMutableDimension("time")->CumulVar(model.Start(0))->SetMin(10);
  model.GetMutableDimension("time")->CumulVar(model.Start(1))->SetMin(1);
  model.GetMutableDimension("time")->SetCumulVarSoftUpperBound(1, 0, 10);
  model.GetMutableDimension("time")->SetCumulVarSoftUpperBound(2, 6, 30);
  model.GetMutableDimension("time")->SetCumulVarSoftUpperBound(3, 0, 10);
  model.GetMutableDimension("time")->SetCumulVarSoftUpperBound(4, 6, 30);

  // Setting parameters to make normal search fail as nodes are mandatory.
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::ALL_UNPERFORMED);
  const Assignment* assignment = model.SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(3 + 3 + 20 * 10 + 24 * 30 + 11 * 10 + 15 * 30,
            assignment->ObjectiveValue());
  const std::vector<int64_t> expected_solution = {1, 2, 6, 4, 7, 3};
  for (int i = 0; i < model.Nexts().size(); ++i) {
    EXPECT_EQ(expected_solution[i], assignment->Value(model.Nexts()[i]));
  }
  const SubSolverStatistics& stats = model.GetSubSolverStatistics();
  EXPECT_EQ(stats.num_glop_calls_in_lp_scheduling(), 4);
  EXPECT_EQ(stats.num_cp_sat_calls_in_lp_scheduling(), 0);
  EXPECT_EQ(stats.num_min_cost_flow_calls(), 1);
}

TEST(RoutingFlowAssignmentTest,
     AssignPickupAndDeliveriesAlternativesToVehicles) {
  const int vehicles = 2;
  IndexManager manager(5, vehicles, NodeIndex(0));
  Model model(manager);
  for (int vehicle = 0; vehicle < vehicles; ++vehicle) {
    const int vehicle_cost =
        model.RegisterTransitCallback([&manager, vehicle](int64_t i, int64_t) {
          return (vehicles - vehicle) * manager.IndexToNode(i).value();
        });
    model.SetArcCostEvaluatorOfVehicle(vehicle_cost, vehicle);
  }
  model.AddDimension(
      model.RegisterUnaryTransitCallback([&model, &manager](int64_t i) {
        if (model.IsStart(i) || model.IsEnd(i)) return 0;
        if (manager.IndexToNode(i) % 2 == 1) return 1;
        return -1;
      }),
      1, 1, true, "demand");
  Solver* const solver = model.solver();
  model.AddDimension(
      model.RegisterUnaryTransitCallback([](int64_t) { return 1; }), 10, 10,
      true, "rank");
  const Dimension& rank = model.GetDimensionOrDie("rank");
  std::vector<int64_t> pickups;
  std::vector<int64_t> deliveries;
  for (NodeIndex i(1); i < NodeIndex(5); i += 2) {
    const int pickup = manager.NodeToIndex(i);
    const int delivery = manager.NodeToIndex(i + 1);
    pickups.push_back(pickup);
    deliveries.push_back(delivery);
    solver->AddConstraint(solver->MakeEquality(model.VehicleVar(pickup),
                                               model.VehicleVar(delivery)));
    solver->AddConstraint(solver->MakeLessOrEqual(rank.CumulVar(pickup),
                                                  rank.CumulVar(delivery)));
  }
  model.AddPickupAndDeliverySets(
      model.AddDisjunction(pickups, Model::kNoPenalty, 1),
      model.AddDisjunction(deliveries, Model::kNoPenalty, 1));
  model.AddDimension(model.RegisterUnaryTransitCallback([&model](int64_t i) {
    if (model.IsStart(i) || model.IsEnd(i)) {
      return 0;
    }
    return 1;
  }),
                     2, 2, true, "count");

  // Setting parameters to make normal search fail as nodes are mandatory.
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::ALL_UNPERFORMED);
  const Assignment* assignment = model.SolveWithParameters(parameters);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(3, assignment->ObjectiveValue());
  const std::vector<int64_t> expected_solution = {6, 2, 7, 3, 4, 1};
  for (int i = 0; i < model.Nexts().size(); ++i) {
    EXPECT_EQ(expected_solution[i], assignment->Value(model.Nexts()[i]));
  }
}

// Model with 1 pickup and delivery pair and 1 node, 2 vehicles which can
// visit a single pair or node. The pair route costs
// (0+2+3+0)*(vehicles-vehicle), the node route costs
// (0+1+0)*(vehicles-vehicle). The optimal solution performs all nodes, the
// pair on the second vehicle, the single node on first vehicle.
TEST(RoutingFlowAssignmentTest, AssignNodesAndPickupAndDeliveriesToVehicles) {
  const int vehicles = 2;
  IndexManager manager(4, vehicles, NodeIndex(0));
  Model model(manager);
  for (int vehicle = 0; vehicle < vehicles; ++vehicle) {
    const int vehicle_cost =
        model.RegisterTransitCallback([&manager, vehicle](int64_t i, int64_t) {
          return (vehicles - vehicle) * manager.IndexToNode(i).value();
        });
    model.SetArcCostEvaluatorOfVehicle(vehicle_cost, vehicle);
  }
  Solver* const solver = model.solver();
  model.AddDimension(
      model.RegisterUnaryTransitCallback([](int64_t) { return 1; }), 10, 10,
      true, "rank");
  absl::flat_hash_set<int> pickups;
  pickups.insert(manager.NodeToIndex(NodeIndex(1)));  // single node
  const Dimension& rank = model.GetDimensionOrDie("rank");
  for (NodeIndex i(2); i < NodeIndex(4); i += 2) {
    const int pickup = manager.NodeToIndex(i);
    const int delivery = manager.NodeToIndex(i + 1);
    pickups.insert(pickup);
    solver->AddConstraint(solver->MakeEquality(model.VehicleVar(pickup),
                                               model.VehicleVar(delivery)));
    solver->AddConstraint(solver->MakeLessOrEqual(rank.CumulVar(pickup),
                                                  rank.CumulVar(delivery)));
    model.AddPickupAndDelivery(pickup, delivery);
  }
  model.AddDimension(
      model.RegisterUnaryTransitCallback([&model, &pickups](int64_t i) {
        if (model.IsStart(i) || model.IsEnd(i) || pickups.count(i) == 0) {
          return 0;
        }
        return 1;
      }),
      1, 1, true, "count");
  model.AddDimension(
      model.RegisterUnaryTransitCallback([&model, &pickups](int64_t i) {
        if (model.IsStart(i) || model.IsEnd(i)) {
          return 0;
        }
        if (pickups.count(i) == 1) return 1;
        return -1;
      }),
      1, 1, true, "demand");

  const Assignment* assignment = model.Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(2 * 1 + 1 * 5, assignment->ObjectiveValue());
  const std::vector<int64_t> expected_solution = {1, 5, 3, 6, 2};
  for (int i = 0; i < model.Nexts().size(); ++i) {
    EXPECT_EQ(expected_solution[i], assignment->Value(model.Nexts()[i]));
  }
}

// Model with 2 nodes, 2 vehicles which can visit a single node and which have
// a high fixed cost. The optimal solution must drop both nodes due to low
// penalty costs.
TEST(RoutingFlowAssignmentTest, MakeVisitsUnperformedDueToVehicleCosts) {
  const int vehicles = 2;
  const int nodes = 3;
  IndexManager manager(nodes, vehicles, NodeIndex(0));
  Model model(manager);
  for (int vehicle = 0; vehicle < vehicles; ++vehicle) {
    model.SetFixedCostOfVehicle(1000, vehicle);
  }
  for (NodeIndex i(1); i < NodeIndex(nodes); ++i) {
    model.AddDisjunction({manager.NodeToIndex(i)}, 10);
  }
  model.AddDimension(model.RegisterUnaryTransitCallback([&model](int64_t i) {
    if (model.IsStart(i) || model.IsEnd(i)) return 0;
    return 1;
  }),
                     1, 1, true, "count");
  const Assignment* assignment = model.Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ((nodes - 1) * 10, assignment->ObjectiveValue());
  const std::vector<int64_t> expected_solution = {4, 1, 2, 5};
  for (int i = 0; i < model.Nexts().size(); ++i) {
    EXPECT_EQ(expected_solution[i], assignment->Value(model.Nexts()[i]));
  }
}

// Same as MakeVisitsUnperformedDueToVehicleCosts with some penalties unset
// (should default to zero).
TEST(RoutingFlowAssignmentTest, MakeVisitsUnperformedDueToNullPenalties) {
  const int vehicles = 2;
  const int nodes = 3;
  IndexManager manager(nodes, vehicles, NodeIndex(0));
  Model model(manager);
  for (int vehicle = 0; vehicle < vehicles; ++vehicle) {
    model.SetFixedCostOfVehicle(1000, vehicle);
  }
  model.AddDisjunction({manager.NodeToIndex(NodeIndex(1))}, 0);
  model.AddDisjunction({manager.NodeToIndex(NodeIndex(2))}, 0);
  model.AddDimension(model.RegisterUnaryTransitCallback([&model](int64_t i) {
    if (model.IsStart(i) || model.IsEnd(i)) return 0;
    return 1;
  }),
                     1, 1, true, "count");
  const Assignment* assignment = model.Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(0, assignment->ObjectiveValue());
  const std::vector<int64_t> expected_solution = {4, 1, 2, 5};
  for (int i = 0; i < model.Nexts().size(); ++i) {
    EXPECT_EQ(expected_solution[i], assignment->Value(model.Nexts()[i]));
  }
}

// Model with 2 pickup and delivery pairs, 2 vehicles which can visit a single
// pair and which have a high fixed cost. The optimal solution must drop both
// pairs due to low penalty costs.
TEST(RoutingFlowAssignmentTest,
     MakePickupAndDeliveriesUnperformedDueToVehicleCosts) {
  const int vehicles = 2;
  const int nodes = 5;
  IndexManager manager(nodes, vehicles, NodeIndex(0));
  Model model(manager);
  for (int vehicle = 0; vehicle < vehicles; ++vehicle) {
    model.SetFixedCostOfVehicle(1000, vehicle);
  }
  Solver* const solver = model.solver();
  model.AddDimension(
      model.RegisterUnaryTransitCallback([](int64_t) { return 1; }), nodes * 2,
      nodes * 2, true, "rank");
  absl::flat_hash_set<int> pickups;
  const Dimension& rank = model.GetDimensionOrDie("rank");
  for (NodeIndex i(1); i < NodeIndex(nodes); i += 2) {
    const int pickup = manager.NodeToIndex(i);
    const int delivery = manager.NodeToIndex(i + 1);
    pickups.insert(pickup);
    solver->AddConstraint(solver->MakeEquality(model.VehicleVar(pickup),
                                               model.VehicleVar(delivery)));
    solver->AddConstraint(solver->MakeLessOrEqual(rank.CumulVar(pickup),
                                                  rank.CumulVar(delivery)));
    model.AddPickupAndDelivery(pickup, delivery);
    model.AddDisjunction({pickup}, 10);
    model.AddDisjunction({delivery}, 10);
  }
  model.AddDimension(
      model.RegisterUnaryTransitCallback([&model, &pickups](int64_t i) {
        if (model.IsStart(i) || model.IsEnd(i) || pickups.count(i) == 0) {
          return 0;
        }
        return 1;
      }),
      1, 1, true, "count");
  const Assignment* assignment = model.Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ((nodes - 1) * 10, assignment->ObjectiveValue());
  const std::vector<int64_t> expected_solution = {6, 1, 2, 3, 4, 7};
  for (int i = 0; i < model.Nexts().size(); ++i) {
    EXPECT_EQ(expected_solution[i], assignment->Value(model.Nexts()[i]));
  }
}

// Same as MakePickupAndDeliveriesUnperformedDueToVehicleCosts with some
// penalties unset (should default to zero).
TEST(RoutingFlowAssignmentTest,
     MakePickupAndDeliveriesUnperformedDueToNullPenalties) {
  const int vehicles = 2;
  const int nodes = 5;
  IndexManager manager(nodes, vehicles, NodeIndex(0));
  Model model(manager);
  for (int vehicle = 0; vehicle < vehicles; ++vehicle) {
    model.SetFixedCostOfVehicle(1000, vehicle);
  }
  Solver* const solver = model.solver();
  model.AddDimension(
      model.RegisterUnaryTransitCallback([](int64_t) { return 1; }), nodes * 2,
      nodes * 2, true, "rank");
  absl::flat_hash_set<int> pickups;
  const Dimension& rank = model.GetDimensionOrDie("rank");
  for (NodeIndex i(1); i < NodeIndex(nodes); i += 2) {
    const int pickup = manager.NodeToIndex(i);
    const int delivery = manager.NodeToIndex(i + 1);
    pickups.insert(pickup);
    solver->AddConstraint(solver->MakeEquality(model.VehicleVar(pickup),
                                               model.VehicleVar(delivery)));
    solver->AddConstraint(solver->MakeLessOrEqual(rank.CumulVar(pickup),
                                                  rank.CumulVar(delivery)));
    model.AddPickupAndDelivery(pickup, delivery);
    model.AddDisjunction({pickup}, 0);
    model.AddDisjunction({delivery}, 0);
  }
  model.AddDimension(
      model.RegisterUnaryTransitCallback([&model, &pickups](int64_t i) {
        if (model.IsStart(i) || model.IsEnd(i) || pickups.count(i) == 0) {
          return 0;
        }
        return 1;
      }),
      1, 1, true, "count");
  const Assignment* assignment = model.Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(0, assignment->ObjectiveValue());
  const std::vector<int64_t> expected_solution = {6, 1, 2, 3, 4, 7};
  for (int i = 0; i < model.Nexts().size(); ++i) {
    EXPECT_EQ(expected_solution[i], assignment->Value(model.Nexts()[i]));
  }
}

// Model with 4 no-vehicle nodes, 2 vehicles which can visit a single
// node. The optimal solution must drop 2 nodes (the ones with higher indices).
TEST(RoutingFlowAssignmentTest, MakeSomeVisitsUnperformedDueToLackOfVehicles) {
  const int vehicles = 2;
  const int nodes = 5;
  IndexManager manager(nodes, vehicles, NodeIndex(0));
  Model model(manager);
  for (int vehicle = 0; vehicle < vehicles; ++vehicle) {
    model.SetFixedCostOfVehicle(10, vehicle);
    // Prefer smaller indices in routes.
    model.SetArcCostEvaluatorOfVehicle(
        model.RegisterTransitCallback([&manager](int64_t, int64_t j) {
          return manager.IndexToNode(j).value();
        }),
        vehicle);
  }
  for (NodeIndex i(1); i < NodeIndex(nodes); ++i) {
    model.AddDisjunction({manager.NodeToIndex(i)}, 1000);
  }
  model.AddDimension(model.RegisterUnaryTransitCallback([&model](int64_t i) {
    if (model.IsStart(i) || model.IsEnd(i)) return 0;
    return 1;
  }),
                     1, 1, true, "count");
  const Assignment* assignment = model.Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ((nodes - 1 - vehicles) * 1000 + vehicles * 10 + 1 + 2,
            assignment->ObjectiveValue());
  const std::vector<int64_t> expected_unperformed = {3, 4};
  for (int64_t unperformed : expected_unperformed) {
    EXPECT_EQ(unperformed, assignment->Value(model.Nexts()[unperformed]));
  }
  for (int vehicle = 0; vehicle < vehicles; ++vehicle) {
    const int64_t next_start =
        assignment->Value(model.Nexts()[model.Start(vehicle)]);
    EXPECT_TRUE(next_start == 1 || next_start == 2);
  }
}

// Model with 4 pickup and delivery pairs, 2 vehicles which can visit a single
// pair. The optimal solution must drop 2 pairs (the ones with higher indices).
TEST(RoutingFlowAssignmentTest,
     MakeSomePickupAndDeliveriesUnperformedDueToLackOfVehicles) {
  const int vehicles = 2;
  const int nodes = 9;
  IndexManager manager(nodes, vehicles, NodeIndex(0));
  Model model(manager);
  for (int vehicle = 0; vehicle < vehicles; ++vehicle) {
    model.SetFixedCostOfVehicle(10, vehicle);
    // Prefer smaller indices in routes.
    model.SetArcCostEvaluatorOfVehicle(
        model.RegisterTransitCallback([&manager, vehicle](int64_t, int64_t j) {
          return manager.IndexToNode(j).value() * (vehicles - vehicle);
        }),
        vehicle);
  }
  Solver* const solver = model.solver();
  model.AddDimension(
      model.RegisterUnaryTransitCallback([](int64_t) { return 1; }), nodes * 2,
      nodes * 2, true, "rank");
  absl::flat_hash_set<int> pickups;
  const Dimension& rank = model.GetDimensionOrDie("rank");
  int pair_count = 0;
  for (NodeIndex i(1); i < NodeIndex(nodes); i += 2) {
    const int pickup = manager.NodeToIndex(i);
    const int delivery = manager.NodeToIndex(i + 1);
    pickups.insert(pickup);
    solver->AddConstraint(solver->MakeEquality(model.VehicleVar(pickup),
                                               model.VehicleVar(delivery)));
    solver->AddConstraint(solver->MakeLessOrEqual(rank.CumulVar(pickup),
                                                  rank.CumulVar(delivery)));
    model.AddPickupAndDelivery(pickup, delivery);
    if (pair_count % 2 == 0) {
      model.AddDisjunction({pickup}, 1000);
      model.AddDisjunction({delivery}, 0);
    } else {
      model.AddDisjunction({pickup}, 0);
      model.AddDisjunction({delivery}, 1000);
    }
    ++pair_count;
  }
  model.AddDimension(
      model.RegisterUnaryTransitCallback([&model, &pickups](int64_t i) {
        if (model.IsStart(i) || model.IsEnd(i) || pickups.count(i) == 0) {
          return 0;
        }
        return 1;
      }),
      1, 1, true, "count");
  const Assignment* assignment = model.Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(
      ((nodes - 1) / 2 - vehicles) * 1000 + vehicles * 10 + (1 + 2) * 2 + 3 + 4,
      assignment->ObjectiveValue());
  const std::vector<int64_t> expected_solution = {1, 2, 10, 4, 11,
                                                  5, 6, 7,  8, 3};
  std::vector<int64_t> found_solutions;
  for (int i = 0; i < model.Nexts().size(); ++i) {
    found_solutions.push_back(assignment->Value(model.Nexts()[i]));
  }
  EXPECT_THAT(found_solutions, ContainerEq(expected_solution));
}

// Same as AssignVisitsToVehicles with an extra constraint in the model
// not visible by the flow model which will invalidate the solution.
TEST(RoutingFlowAssignmentTest, AssignVisitsToVehiclesWithFailingFlowSolution) {
  const int vehicles = 2;
  const int nodes = 3;
  IndexManager manager(nodes, vehicles, NodeIndex(0));
  Model model(manager);
  for (int vehicle = 0; vehicle < vehicles; ++vehicle) {
    const int vehicle_cost =
        model.RegisterTransitCallback([&model](int64_t i, int64_t j) {
          if (model.IsStart(i) && model.IsEnd(j)) {
            return 0;
          }
          return 1;
        });
    model.SetArcCostEvaluatorOfVehicle(vehicle_cost, vehicle);
  }
  for (NodeIndex i(1); i < NodeIndex(nodes); ++i) {
    model.AddDisjunction({manager.NodeToIndex(i)}, 10);
  }
  model.AddDimension(model.RegisterUnaryTransitCallback([&model](int64_t i) {
    if (model.IsStart(i) || model.IsEnd(i)) {
      return 0;
    }
    return 1;
  }),
                     1, 1, true, "count");
  // Make flow solution unfeasible (to update when more constraints are taken
  // into account).
  for (NodeIndex i(1); i < NodeIndex(nodes); ++i) {
    model.VehicleVar(manager.NodeToIndex(i))->SetValue(-1);
  }

  const Assignment* assignment = model.Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ((nodes - 1) * 10, assignment->ObjectiveValue());
}

// Same as AssignVisitsToVehicles with high cost values which can potentially
// overflow. Best solution is to skip all nodes.
TEST(RoutingFlowAssignmentTest, AssignVisitsToVehiclesWithHighCosts) {
  const int vehicles = 2;
  const int nodes = 3;
  IndexManager manager(nodes, vehicles, NodeIndex(0));
  Model model(manager);
  for (int vehicle = 0; vehicle < vehicles; ++vehicle) {
    const int vehicle_cost =
        model.RegisterTransitCallback([&model](int64_t i, int64_t j) {
          if (model.IsStart(i) && model.IsEnd(j)) {
            return int64_t{0};
          }
          // To keep the cost of each potential route under kint64max.
          return kint64max / 2 - 1;
        });
    model.SetArcCostEvaluatorOfVehicle(vehicle_cost, vehicle);
  }
  for (NodeIndex i(1); i < NodeIndex(nodes); ++i) {
    model.AddDisjunction({manager.NodeToIndex(i)}, 10);
  }
  model.AddDimension(model.RegisterUnaryTransitCallback([&model](int64_t i) {
    if (model.IsStart(i) || model.IsEnd(i)) {
      return 0;
    }
    return 1;
  }),
                     1, 1, true, "count");
  const Assignment* assignment = model.Solve();
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ((nodes - 1) * 10, assignment->ObjectiveValue());
}

// TODO(user): Add more tests with potential cost overflows in min-cost flow.

// Trivial assignment (all unperformed) is not feasible given all nodes are
// mandatory.
TEST(RoutingTrivialAssignmentTest, TrivialAssignmentUnfeasible) {
  IndexManager manager(3, 2, NodeIndex(0));
  Model model(manager);
  const int vehicle_cost =
      model.RegisterTransitCallback([](int64_t, int64_t) { return 1; });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_solution_limit(1);
  std::vector<const Assignment*> solutions;
  const Assignment* assignment =
      model.SolveWithParameters(parameters, &solutions);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(3, assignment->ObjectiveValue());
  EXPECT_EQ(1, solutions.size());
}

// Trivial assignment (all unperformed) is feasible given all nodes are
// optional. Forcing the search to find a solution with all performed by
// limiting the search to one solution.
TEST(RoutingTrivialAssignmentTest, TrivialAssignmentFeasible) {
  const int nodes = 3;
  IndexManager manager(nodes, 2, NodeIndex(0));
  Model model(manager);
  const int vehicle_cost =
      model.RegisterTransitCallback([](int64_t, int64_t) { return 10; });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  for (NodeIndex i(1); i < NodeIndex(nodes); ++i) {
    model.AddDisjunction({manager.NodeToIndex(i)}, 1);
  }
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_solution_limit(1);
  std::vector<const Assignment*> solutions;
  const Assignment* assignment =
      model.SolveWithParameters(parameters, &solutions);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(2, assignment->ObjectiveValue());
  EXPECT_EQ(2, solutions.size());
  EXPECT_EQ(30, solutions[0]->ObjectiveValue());
}

TEST(SimpleBoundCostsTest, GeneralInterface) {
  SimpleBoundCosts bound_costs(5, BoundCost{-1, 1});
  EXPECT_EQ(bound_costs.Size(), 5);
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(bound_costs.bound_cost(i).bound, -1);
    EXPECT_EQ(bound_costs.bound_cost(i).cost, 1);
  }
  bound_costs.bound_cost(2) = {kint64min, kint64max};
  EXPECT_EQ(bound_costs.bound_cost(2).bound, kint64min);
  EXPECT_EQ(bound_costs.bound_cost(2).cost, kint64max);
}

TEST(DimensionSoftSpanUpperBounds, GeneralInterface) {
  const int num_vehicles = 3;
  IndexManager manager(10, num_vehicles, NodeIndex{0});
  Model model(manager);
  const int evaluator_index = model.RegisterTransitCallback(
      [](int64_t, int64_t) -> int64_t { return 1; });
  model.AddDimension(evaluator_index, kint64max, kint64max, false, "test");
  Dimension* dimension = model.GetMutableDimension("test");
  EXPECT_FALSE(dimension->HasSoftSpanUpperBounds());
  dimension->SetSoftSpanUpperBoundForVehicle({1, 2}, 1);
  EXPECT_TRUE(dimension->HasSoftSpanUpperBounds());
  EXPECT_EQ(dimension->GetSoftSpanUpperBoundForVehicle(0).bound, kint64max);
  EXPECT_EQ(dimension->GetSoftSpanUpperBoundForVehicle(0).cost, 0);
  EXPECT_EQ(dimension->GetSoftSpanUpperBoundForVehicle(1).bound, 1);
  EXPECT_EQ(dimension->GetSoftSpanUpperBoundForVehicle(1).cost, 2);
  EXPECT_EQ(dimension->GetSoftSpanUpperBoundForVehicle(2).bound, kint64max);
  EXPECT_EQ(dimension->GetSoftSpanUpperBoundForVehicle(2).cost, 0);
}

TEST(ModelSoftSpanUpperBound, SoftSpanUpperBoundCostIsCorrect) {
  const int num_vehicles = 3;
  IndexManager manager(2, num_vehicles, NodeIndex{0});
  Model model(manager);
  const int evaluator_index = model.RegisterTransitCallback(
      [](int64_t, int64_t) -> int64_t { return 20; });
  // Vehicle 0 has time span 40, soft span bound makes its cost 1000.
  {
    model.AddDimension(evaluator_index, 0, kint64max, false, "dim0");
    Dimension* dimension0 = model.GetMutableDimension("dim0");
    dimension0->SetSoftSpanUpperBoundForVehicle({30, 100}, 0);
    model.NextVar(model.Start(0))->SetValue(1);
    model.NextVar(1)->SetValue(model.End(0));
  }
  // Vehicle 1 has time span 20 with no deliveries, soft span bound could make
  // its cost 100, but its cost must not be counted when empty,
  // so the cost is actually 0.
  {
    model.AddDimension(evaluator_index, 0, kint64max, false, "dim1");
    Dimension* dimension1 = model.GetMutableDimension("dim1");
    dimension1->SetSoftSpanUpperBoundForVehicle({10, 10}, 1);
    model.SetVehicleUsedWhenEmpty(false, 1);
    model.NextVar(model.Start(1))->SetValue(model.End(1));
  }
  // Vehicle 2 has time span 20 with no deliveries, soft span bound could make
  // its cost 10 and it must be counted even when empty, so its cost is 10.
  {
    model.AddDimension(evaluator_index, 0, kint64max, false, "dim2");
    Dimension* dimension2 = model.GetMutableDimension("dim2");
    dimension2->SetSoftSpanUpperBoundForVehicle({10, 1}, 2);
    model.SetVehicleUsedWhenEmpty(true, 2);
    model.NextVar(model.Start(2))->SetValue(model.End(2));
  }
  // The solution must have cost 1000 + 10 = 1010.
  const Assignment* solution = model.Solve();
  ASSERT_TRUE(solution != nullptr);
  EXPECT_EQ(solution->ObjectiveValue(), 1010);
}

TEST(DimensionQuadraticCostSoftSpanUpperBounds, GeneralInterface) {
  const int num_vehicles = 3;
  IndexManager manager(10, num_vehicles, NodeIndex{0});
  Model model(manager);
  const int evaluator_index = model.RegisterTransitCallback(
      [](int64_t, int64_t) -> int64_t { return 1; });
  model.AddDimension(evaluator_index, kint64max, kint64max, false, "test");
  Dimension* dimension = model.GetMutableDimension("test");
  EXPECT_FALSE(dimension->HasQuadraticCostSoftSpanUpperBounds());
  dimension->SetQuadraticCostSoftSpanUpperBoundForVehicle({1, 2}, 1);
  EXPECT_TRUE(dimension->HasQuadraticCostSoftSpanUpperBounds());
  EXPECT_EQ(dimension->GetQuadraticCostSoftSpanUpperBoundForVehicle(0).bound,
            kint64max);
  EXPECT_EQ(dimension->GetQuadraticCostSoftSpanUpperBoundForVehicle(0).cost, 0);
  EXPECT_EQ(dimension->GetQuadraticCostSoftSpanUpperBoundForVehicle(1).bound,
            1);
  EXPECT_EQ(dimension->GetQuadraticCostSoftSpanUpperBoundForVehicle(1).cost, 2);
  EXPECT_EQ(dimension->GetQuadraticCostSoftSpanUpperBoundForVehicle(2).bound,
            kint64max);
  EXPECT_EQ(dimension->GetQuadraticCostSoftSpanUpperBoundForVehicle(2).cost, 0);
}

TEST(ModelQuadraticCostSoftSpanUpperBound,
     QuadraticCostSoftSpanUpperBoundCostIsCorrect) {
  const int num_vehicles = 3;
  IndexManager manager(2, num_vehicles, NodeIndex{0});
  Model model(manager);
  const int evaluator_index = model.RegisterTransitCallback(
      [](int64_t, int64_t) -> int64_t { return 20; });
  // Vehicle 0 has time span 40, soft span bound makes its cost 10000.
  {
    model.AddDimension(evaluator_index, 0, kint64max, false, "dim0");
    Dimension* dimension0 = model.GetMutableDimension("dim0");
    dimension0->SetQuadraticCostSoftSpanUpperBoundForVehicle({30, 100}, 0);
    model.NextVar(model.Start(0))->SetValue(1);
    model.NextVar(1)->SetValue(model.End(0));
  }
  // Vehicle 1 has time span 20 with no deliveries, soft span bound could make
  // its cost 100, but its cost must not be counted when empty,
  // so the cost is actually 0.
  {
    model.AddDimension(evaluator_index, 0, kint64max, false, "dim1");
    Dimension* dimension1 = model.GetMutableDimension("dim1");
    dimension1->SetQuadraticCostSoftSpanUpperBoundForVehicle({10, 10}, 1);
    model.SetVehicleUsedWhenEmpty(false, 1);
    model.NextVar(model.Start(1))->SetValue(model.End(1));
  }
  // Vehicle 2 has time span 20 with no deliveries, soft span bound could make
  // its cost 100 and it must be counted even when empty, so its cost is 100.
  {
    model.AddDimension(evaluator_index, 0, kint64max, false, "dim2");
    Dimension* dimension2 = model.GetMutableDimension("dim2");
    dimension2->SetQuadraticCostSoftSpanUpperBoundForVehicle({10, 1}, 2);
    model.SetVehicleUsedWhenEmpty(true, 2);
    model.NextVar(model.Start(2))->SetValue(model.End(2));
  }
  // The solution must have cost 10000 + 100 = 10100.
  const Assignment* solution = model.Solve();
  ASSERT_TRUE(solution != nullptr);
  EXPECT_EQ(solution->ObjectiveValue(), 10100);
}

class PathEnergyCostTest
    : public testing::TestWithParam<std::tuple<bool, bool>> {};

TEST_P(PathEnergyCostTest, SolverOptimizesEnergy) {
  const auto [cheap_vehicles_have_zero_cost, start_unperformed] = GetParam();
  // Four vehicles, 1 and 2 are still used when empty so it is better to use
  // one of those anyways. 1 is cheaper than 2.
  std::vector<bool> vehicle_used_when_empty{false, true, true, false};
  struct Cost {
    int64_t threshold;
    int64_t below;
    int64_t above;
  };
  const int num_vehicles = 4;
  std::vector<Cost> cost_per_vehicle{
      {1007, 3, 11},
      {1007, 3, 11},
      {1007, 5, 11},
      {1007, 5, 11},
  };
  if (cheap_vehicles_have_zero_cost) {
    cost_per_vehicle[0].below = 0;
    cost_per_vehicle[1].below = 0;
  }

  IndexManager manager(10, num_vehicles, NodeIndex{0});
  Model model(manager);

  // Force and distance dimensions.
  // Force starts at 1000 for all vehicles, then each visit adds 1.
  const int distance_index = model.RegisterTransitCallback(
      [](int64_t, int64_t) -> int64_t { return 1; });
  const int force_index =
      model.RegisterUnaryTransitCallback([&model](int64_t node) -> int64_t {
        return model.IsStart(node) ? 1000 : 1;
      });

  model.AddDimension(force_index, 0, kint64max, false, "force");
  model.AddDimension(distance_index, 0, kint64max, false, "distance");

  for (int v = 0; v < num_vehicles; ++v) {
    const auto [threshold, below, above] = cost_per_vehicle[v];
    model.SetPathEnergyCostsOfVehicle("force", "distance", threshold, below,
                                      above, v);
    model.SetVehicleUsedWhenEmpty(vehicle_used_when_empty[v], v);
  }

  const Assignment* solution = nullptr;
  if (start_unperformed) {
    RoutingSearchParameters search_parameters =
        DefaultRoutingSearchParameters();
    search_parameters.set_first_solution_strategy(
        FirstSolutionStrategy::PATH_CHEAPEST_ARC);
    solution = model.SolveWithParameters(search_parameters);
  } else {
    solution = model.Solve();
  }

  ASSERT_TRUE(solution != nullptr);
  // Vehicle 1 should do everything because it is used when empty and cheapest.
  // Vehicle 2 has some cost because used when empty.
  // Other vehicles are unused, cost 0.
  const int64_t cost_vehicle_1 =
      3 * 10 * 1000 +  // 10 arcs carrying the initial 1000 at unit cost 3.
      3 * 42 +         // 3 * (1 + 2 + ... + 7 + 7 + 7) energy cost below
      11 * 3;          // 11 * (8 - 7 + 9 - 7)  soft_energy cost.
  const int64_t cost_vehicle_2 = 5 * 1000;  // start -> end
  const int64_t total_cost =
      (cheap_vehicles_have_zero_cost ? 33 : cost_vehicle_1) + cost_vehicle_2;
  EXPECT_EQ(solution->ObjectiveValue(), total_cost);
}

INSTANTIATE_TEST_SUITE_P(PathEnergyCostTest, PathEnergyCostTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(PathEnergyCostTest, EnergyFiltersArePerfect) {
  std::vector<bool> vehicle_used_when_empty{true, true};
  struct Cost {
    int64_t threshold;
    int64_t below;
    int64_t above;
  };
  const int num_vehicles = 2;
  std::vector<Cost> cost_per_vehicle{
      {0, 0, 1},
      {0, 0, 1},
  };

  IndexManager manager(10, num_vehicles, NodeIndex{0});
  Model model(manager);

  // Force and distance dimensions.
  const int distance_index = model.RegisterTransitCallback(
      [](int64_t, int64_t) -> int64_t { return 1; });
  const int force_index =
      model.RegisterUnaryTransitCallback([](int64_t) -> int64_t { return 1; });

  model.AddDimension(force_index, 0, kint64max, false, "force");
  Dimension* force_dimension = model.GetMutableDimension("force");
  force_dimension->CumulVar(model.Start(0))->SetMin(10);
  force_dimension->CumulVar(model.End(0))->SetMin(2);
  force_dimension->CumulVar(model.Start(1))->SetMin(3);
  force_dimension->CumulVar(model.End(1))->SetMin(9);
  model.AddDimension(distance_index, 0, kint64max, false, "distance");

  for (int v = 0; v < num_vehicles; ++v) {
    const auto [threshold, below, above] = cost_per_vehicle[v];
    model.SetPathEnergyCostsOfVehicle("force", "distance", threshold, below,
                                      above, v);
    model.SetVehicleUsedWhenEmpty(vehicle_used_when_empty[v], v);
  }

  const Assignment* solution = model.Solve();
  ASSERT_TRUE(solution != nullptr);
  EXPECT_EQ(model.solver()->filtered_neighbors(),
            model.solver()->accepted_neighbors());
}

TEST(DimensionAllowedIntervals, EmptyForbidden) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model model(manager);
  const int callback =
      model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; });
  model.AddDimension(callback, kint64max, kint64max,
                     /*fix_start_cumul_to_zero=*/false, "dim");
  Dimension* const dimension = model.GetMutableDimension("dim");
  dimension->CumulVar(1)->SetRange(5, 15);
  model.CloseModel();
  SortedDisjointIntervalList allowed =
      dimension->GetAllowedIntervalsInRange(1, 0, 20);
  EXPECT_EQ(allowed.NumIntervals(), 1);
  for (const ClosedInterval interval : allowed) {
    EXPECT_EQ(interval.start, 5);
    EXPECT_EQ(interval.end, 15);
  }
  allowed = dimension->GetAllowedIntervalsInRange(1, 10, 20);
  EXPECT_EQ(allowed.NumIntervals(), 1);
  for (const ClosedInterval interval : allowed) {
    EXPECT_EQ(interval.start, 10);
    EXPECT_EQ(interval.end, 15);
  }
  allowed = dimension->GetAllowedIntervalsInRange(1, 10, 11);
  EXPECT_EQ(allowed.NumIntervals(), 1);
  for (const ClosedInterval interval : allowed) {
    EXPECT_EQ(interval.start, 10);
    EXPECT_EQ(interval.end, 11);
  }
  allowed = dimension->GetAllowedIntervalsInRange(1, 0, 11);
  EXPECT_EQ(allowed.NumIntervals(), 1);
  for (const ClosedInterval interval : allowed) {
    EXPECT_EQ(interval.start, 5);
    EXPECT_EQ(interval.end, 11);
  }
}

TEST(DimensionAllowedIntervals, OneForbidden) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model model(manager);
  const int callback =
      model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; });
  model.AddDimension(callback, kint64max, kint64max,
                     /*fix_start_cumul_to_zero=*/false, "dim");
  Dimension* const dimension = model.GetMutableDimension("dim");
  dimension->CumulVar(1)->SetRange(5, 15);
  const std::vector<int64_t> starts = {10};
  const std::vector<int64_t> ends = {12};
  Solver* const solver = model.solver();
  solver->AddConstraint(
      solver->MakeNotMemberCt(dimension->CumulVar(1), starts, ends));
  model.CloseModel();
  SortedDisjointIntervalList allowed =
      dimension->GetAllowedIntervalsInRange(1, 0, 20);
  EXPECT_EQ(allowed.NumIntervals(), 2);
  std::vector<int64_t> expected_starts = {5, 13};
  std::vector<int64_t> expected_ends = {9, 15};
  int count = 0;
  for (const ClosedInterval interval : allowed) {
    EXPECT_EQ(interval.start, expected_starts[count]);
    EXPECT_EQ(interval.end, expected_ends[count]);
    ++count;
  }
  allowed = dimension->GetAllowedIntervalsInRange(1, 10, 20);
  expected_starts = {13};
  expected_ends = {15};
  count = 0;
  EXPECT_EQ(allowed.NumIntervals(), 1);
  for (const ClosedInterval interval : allowed) {
    EXPECT_EQ(interval.start, expected_starts[count]);
    EXPECT_EQ(interval.end, expected_ends[count]);
    ++count;
  }
  allowed = dimension->GetAllowedIntervalsInRange(1, 10, 11);
  EXPECT_EQ(allowed.NumIntervals(), 0);
  allowed = dimension->GetAllowedIntervalsInRange(1, 0, 11);
  expected_starts = {5};
  expected_ends = {9};
  count = 0;
  EXPECT_EQ(allowed.NumIntervals(), 1);
  for (const ClosedInterval interval : allowed) {
    EXPECT_EQ(interval.start, expected_starts[count]);
    EXPECT_EQ(interval.end, expected_ends[count]);
    ++count;
  }
}

TEST(DimensionAllowedIntervals, TwoForbidden) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model model(manager);
  const int callback =
      model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; });
  model.AddDimension(callback, kint64max, kint64max,
                     /*fix_start_cumul_to_zero=*/false, "dim");
  Dimension* const dimension = model.GetMutableDimension("dim");
  dimension->CumulVar(1)->SetRange(5, 25);
  const std::vector<int64_t> starts = {10, 20};
  const std::vector<int64_t> ends = {12, 22};
  Solver* const solver = model.solver();
  solver->AddConstraint(
      solver->MakeNotMemberCt(dimension->CumulVar(1), starts, ends));
  model.CloseModel();
  SortedDisjointIntervalList allowed =
      dimension->GetAllowedIntervalsInRange(1, 6, 24);
  EXPECT_EQ(allowed.NumIntervals(), 3);
  std::vector<int64_t> expected_starts = {6, 13, 23};
  std::vector<int64_t> expected_ends = {9, 19, 24};
  int count = 0;
  for (const ClosedInterval interval : allowed) {
    EXPECT_EQ(interval.start, expected_starts[count]);
    EXPECT_EQ(interval.end, expected_ends[count]);
    ++count;
  }
  allowed = dimension->GetAllowedIntervalsInRange(1, 10, 30);
  expected_starts = {13, 23};
  expected_ends = {19, 25};
  count = 0;
  EXPECT_EQ(allowed.NumIntervals(), 2);
  for (const ClosedInterval interval : allowed) {
    EXPECT_EQ(interval.start, expected_starts[count]);
    EXPECT_EQ(interval.end, expected_ends[count]);
    ++count;
  }
  allowed = dimension->GetAllowedIntervalsInRange(1, 10, 11);
  EXPECT_EQ(allowed.NumIntervals(), 0);
  allowed = dimension->GetAllowedIntervalsInRange(1, 10, 23);
  expected_starts = {13, 23};
  expected_ends = {19, 23};
  count = 0;
  EXPECT_EQ(allowed.NumIntervals(), 2);
  for (const ClosedInterval interval : allowed) {
    EXPECT_EQ(interval.start, expected_starts[count]);
    EXPECT_EQ(interval.end, expected_ends[count]);
    ++count;
  }
  allowed = dimension->GetAllowedIntervalsInRange(1, 0, 23);
  expected_starts = {5, 13, 23};
  expected_ends = {9, 19, 23};
  count = 0;
  EXPECT_EQ(allowed.NumIntervals(), 3);
  for (const ClosedInterval interval : allowed) {
    EXPECT_EQ(interval.start, expected_starts[count]);
    EXPECT_EQ(interval.end, expected_ends[count]);
    ++count;
  }
}

TEST(NodeNeighborsTest, ZeroNeighborsZeroNonStartEndNodes) {
  // Create a model with only the depot.
  IndexManager manager(1, 1, NodeIndex(0));
  Model model(manager);
  model.SetArcCostEvaluatorOfAllVehicles(
      model.RegisterTransitCallback([](int i, int) { return i; }));
  model.CloseModel();
  for (const bool add_vehicle_starts_to_neighbors : {true, false}) {
    for (const bool add_vehicle_ends_to_neighbors : {true, false}) {
      for (bool only_sort_neighbors_for_partial_neighborhoods : {true, false}) {
        const Model::NodeNeighborsParameters params = {
            /*num_neighbors=*/0, add_vehicle_starts_to_neighbors,
            add_vehicle_ends_to_neighbors,
            only_sort_neighbors_for_partial_neighborhoods};
        const Model::NodeNeighborsByCostClass* const node_neighbors =
            model.GetOrCreateNodeNeighborsByCostClass(params);
        EXPECT_EQ(node_neighbors,
                  model.GetOrCreateNodeNeighborsByCostClass(params));
        EXPECT_EQ(
            node_neighbors
                ->GetIncomingNeighborsOfNodeForCostClass(1, /*node_index=*/1)
                .size(),
            add_vehicle_starts_to_neighbors ? 1 : 0);
        EXPECT_EQ(
            node_neighbors
                ->GetOutgoingNeighborsOfNodeForCostClass(1, /*node_index=*/0)
                .size(),
            add_vehicle_ends_to_neighbors ? 1 : 0);
      }
    }
  }
}

void TestZeroNeighborsWithSingleVehicle(
    const Model& model, const Model::NodeNeighborsByCostClass& node_neighbors,
    bool add_vehicle_starts_to_neighbors, bool add_vehicle_ends_to_neighbors) {
  ASSERT_EQ(model.vehicles(), 1);
  const int start = 0;
  ASSERT_TRUE(model.IsStart(start));
  const int num_nodes_with_starts_or_ends = model.Size();
  const int num_non_start_end_nodes = num_nodes_with_starts_or_ends - 1;

  // Start to all.
  EXPECT_EQ(node_neighbors
                .GetOutgoingNeighborsOfNodeForCostClass(
                    1, /*node_index=*/model.Start(0))
                .size(),
            add_vehicle_ends_to_neighbors ? num_nodes_with_starts_or_ends
                                          : num_non_start_end_nodes);
  // NOTE: start(v)->end(v) is always a neighborhood arc, even when
  // starts and ends aren't explicitly added to the neighbors list.
  ASSERT_TRUE(model.IsEnd(num_nodes_with_starts_or_ends));
  for (int node = 1; node <= num_nodes_with_starts_or_ends; node++) {
    EXPECT_TRUE(node_neighbors.IsNeighborhoodArcForCostClass(1, 0, node));
    EXPECT_FALSE(node_neighbors.IsNeighborhoodArcForCostClass(1, node, 0));
  }

  // All to end.
  const int end = model.End(0);
  EXPECT_EQ(node_neighbors
                .GetIncomingNeighborsOfNodeForCostClass(
                    1, /*node_index=*/model.End(0))
                .size(),
            add_vehicle_starts_to_neighbors ? num_nodes_with_starts_or_ends
                                            : num_non_start_end_nodes);
  for (int node = 1; node < num_nodes_with_starts_or_ends; node++) {
    EXPECT_TRUE(node_neighbors.IsNeighborhoodArcForCostClass(1, node, end));
    EXPECT_FALSE(node_neighbors.IsNeighborhoodArcForCostClass(1, end, node));
  }

  // Non start-end nodes.
  for (int node = 1; node < num_nodes_with_starts_or_ends; node++) {
    EXPECT_EQ(
        node_neighbors.GetOutgoingNeighborsOfNodeForCostClass(1, node).size(),
        add_vehicle_ends_to_neighbors ? 1 : 0);
    EXPECT_EQ(
        node_neighbors.GetIncomingNeighborsOfNodeForCostClass(1, node).size(),
        add_vehicle_starts_to_neighbors ? 1 : 0);
  }
}

TEST(NodeNeighborsTest, ZeroNeighbors) {
  const int kNumNodes = 20;
  IndexManager manager(kNumNodes, 1, NodeIndex(0));
  Model model(manager);
  model.SetArcCostEvaluatorOfAllVehicles(
      model.RegisterTransitCallback([](int i, int) { return i; }));
  model.CloseModel();
  for (const bool add_vehicle_starts_to_neighbors : {true, false}) {
    for (const bool add_vehicle_ends_to_neighbors : {true, false}) {
      for (bool only_sort_neighbors_for_partial_neighborhoods : {true, false}) {
        const Model::NodeNeighborsParameters params = {
            /*num_neighbors=*/0, add_vehicle_starts_to_neighbors,
            add_vehicle_ends_to_neighbors,
            only_sort_neighbors_for_partial_neighborhoods};
        const Model::NodeNeighborsByCostClass* const node_neighbors =
            model.GetOrCreateNodeNeighborsByCostClass(params);
        EXPECT_EQ(node_neighbors,
                  model.GetOrCreateNodeNeighborsByCostClass(params));
        TestZeroNeighborsWithSingleVehicle(model, *node_neighbors,
                                           add_vehicle_starts_to_neighbors,
                                           add_vehicle_ends_to_neighbors);
      }
    }
  }
}

TEST(NodeNeighborsTest, ZeroNeighborsRatio) {
  const int kNumNodes = 20;
  IndexManager manager(kNumNodes, 1, NodeIndex(0));
  Model model(manager);
  model.SetArcCostEvaluatorOfAllVehicles(
      model.RegisterTransitCallback([](int i, int) { return i; }));
  model.CloseModel();
  double ratio = -1;
  for (const bool add_vehicle_starts_to_neighbors : {true, false}) {
    for (const bool add_vehicle_ends_to_neighbors : {true, false}) {
      for (bool only_sort_neighbors_for_partial_neighborhoods : {true, false}) {
        const Model::NodeNeighborsByCostClass* const node_neighbors =
            model.GetOrCreateNodeNeighborsByCostClass(
                0, 0, ratio, add_vehicle_starts_to_neighbors,
                add_vehicle_ends_to_neighbors,
                only_sort_neighbors_for_partial_neighborhoods);
        EXPECT_EQ(ratio, 0);
        EXPECT_EQ(node_neighbors,
                  model.GetOrCreateNodeNeighborsByCostClass(
                      0, 0, ratio, add_vehicle_starts_to_neighbors,
                      add_vehicle_ends_to_neighbors,
                      only_sort_neighbors_for_partial_neighborhoods));
        TestZeroNeighborsWithSingleVehicle(model, *node_neighbors,
                                           add_vehicle_starts_to_neighbors,
                                           add_vehicle_ends_to_neighbors);
      }
    }
  }
}

int GetExpectedNumNeighborsForFullGraph(
    const Model& model, int node, bool is_outgoing,
    bool add_vehicle_starts_to_neighbors, bool add_vehicle_ends_to_neighbors,
    bool only_sort_neighbors_for_partial_neighborhoods) {
  if ((model.IsStart(node) && !is_outgoing) ||
      (model.IsEnd(node) && is_outgoing)) {
    return 0;
  }
  const int num_vehicles = model.vehicles();
  int num_neighbors = model.Size() - num_vehicles;
  int num_start_end_neighbors = num_vehicles;
  if (!only_sort_neighbors_for_partial_neighborhoods) {
    if (model.IsStart(node) || model.IsEnd(node)) {
      // A start (resp. end) node is only (possibly) connected to its own end
      // (resp. start).
      num_start_end_neighbors = 1;
    } else {
      // A non start/end node isn't connected to itself.
      num_neighbors--;
    }
  }
  if (!is_outgoing && add_vehicle_starts_to_neighbors) {
    num_neighbors += num_start_end_neighbors;
  }
  if (is_outgoing && add_vehicle_ends_to_neighbors) {
    num_neighbors += num_start_end_neighbors;
  }
  return num_neighbors;
}

void TestFullNeighborhood(const Model& model,
                          const Model::NodeNeighborsByCostClass& node_neighbors,
                          bool add_vehicle_starts_to_neighbors,
                          bool add_vehicle_ends_to_neighbors,
                          bool only_sort_neighbors_for_partial_neighborhoods) {
  for (int node = 0; node < model.Size() + model.vehicles(); ++node) {
    EXPECT_EQ(
        node_neighbors.GetOutgoingNeighborsOfNodeForCostClass(1, node).size(),
        GetExpectedNumNeighborsForFullGraph(
            model, node, /*is_outgoing=*/true, add_vehicle_starts_to_neighbors,
            add_vehicle_ends_to_neighbors,
            only_sort_neighbors_for_partial_neighborhoods));
    EXPECT_EQ(
        node_neighbors.GetIncomingNeighborsOfNodeForCostClass(1, node).size(),
        GetExpectedNumNeighborsForFullGraph(
            model, node, /*is_outgoing=*/false, add_vehicle_starts_to_neighbors,
            add_vehicle_ends_to_neighbors,
            only_sort_neighbors_for_partial_neighborhoods));
  }
}

// NodeNeighborsByCostClass::ComputeNeighbors, which is called by
// Model::GetOrCreateNodeNeighborsByCostClass, crashed before
// cl/613903024 when the number of required neighbors is larger than the number
// of actual existing neighbors (but less than the model size). This test makes
// sure this is no longer happening.
TEST(NodeNeighborsTest, NumRequiredNeighborsHigherThanExistingNeighbors) {
  const int64_t kNumNodes = 20;
  IndexManager manager(kNumNodes, 1, NodeIndex(0));
  Model model(manager);
  model.SetArcCostEvaluatorOfAllVehicles(
      model.RegisterTransitCallback([](int i, int) { return i; }));
  model.CloseModel();
  for (int num_neighbors :
       {kNumNodes - 1, kNumNodes, model.Size() - 1, 2 * model.Size()}) {
    for (bool only_sort_neighbors_for_partial_neighborhoods : {true, false}) {
      for (const bool add_vehicle_starts_to_neighbors : {true, false}) {
        for (const bool add_vehicle_ends_to_neighbors : {true, false}) {
          const Model::NodeNeighborsParameters params = {
              num_neighbors, add_vehicle_starts_to_neighbors,
              add_vehicle_ends_to_neighbors,
              only_sort_neighbors_for_partial_neighborhoods};
          const Model::NodeNeighborsByCostClass* const node_neighbors =
              model.GetOrCreateNodeNeighborsByCostClass(params);
          EXPECT_EQ(node_neighbors,
                    model.GetOrCreateNodeNeighborsByCostClass(params));
          TestFullNeighborhood(model, *node_neighbors,
                               add_vehicle_starts_to_neighbors,
                               add_vehicle_ends_to_neighbors,
                               only_sort_neighbors_for_partial_neighborhoods);
        }
      }
    }
  }
}

TEST(NodeNeighborsTest, TooManyNeighbors) {
  const int kNumNodes = 20;
  IndexManager manager(kNumNodes, 1, NodeIndex(0));
  Model model(manager);
  model.SetArcCostEvaluatorOfAllVehicles(
      model.RegisterTransitCallback([](int i, int) { return i; }));
  model.CloseModel();
  for (bool add_vehicle_starts_to_neighbors : {true, false}) {
    for (bool add_vehicle_ends_to_neighbors : {true, false}) {
      for (bool only_sort_neighbors_for_partial_neighborhoods : {true, false}) {
        const Model::NodeNeighborsParameters params = {
            100, add_vehicle_starts_to_neighbors, add_vehicle_ends_to_neighbors,
            only_sort_neighbors_for_partial_neighborhoods};
        const Model::NodeNeighborsByCostClass* const node_neighbors =
            model.GetOrCreateNodeNeighborsByCostClass(params);
        EXPECT_EQ(node_neighbors,
                  model.GetOrCreateNodeNeighborsByCostClass(params));
        TestFullNeighborhood(model, *node_neighbors,
                             add_vehicle_starts_to_neighbors,
                             add_vehicle_ends_to_neighbors,
                             only_sort_neighbors_for_partial_neighborhoods);
      }
    }
  }
}

TEST(NodeNeighborsTest, TooManyNeighborsRatio) {
  IndexManager manager(20, 1, NodeIndex(0));
  Model model(manager);
  model.SetArcCostEvaluatorOfAllVehicles(
      model.RegisterTransitCallback([](int i, int) { return i; }));
  model.CloseModel();
  double ratio = -1;
  for (bool add_vehicle_starts_to_neighbors : {true, false}) {
    for (bool add_vehicle_ends_to_neighbors : {true, false}) {
      for (bool only_sort_neighbors_for_partial_neighborhoods : {true, false}) {
        const Model::NodeNeighborsByCostClass* const node_neighbors =
            model.GetOrCreateNodeNeighborsByCostClass(
                0.5, 100, ratio, add_vehicle_starts_to_neighbors,
                add_vehicle_ends_to_neighbors,
                only_sort_neighbors_for_partial_neighborhoods);
        EXPECT_EQ(ratio, 1);
        EXPECT_EQ(node_neighbors,
                  model.GetOrCreateNodeNeighborsByCostClass(
                      0.5, 100, ratio, add_vehicle_starts_to_neighbors,
                      add_vehicle_ends_to_neighbors,
                      only_sort_neighbors_for_partial_neighborhoods));
        TestFullNeighborhood(model, *node_neighbors,
                             add_vehicle_starts_to_neighbors,
                             add_vehicle_ends_to_neighbors,
                             only_sort_neighbors_for_partial_neighborhoods);
      }
    }
  }
}

// Verifies the node neighborhoods are as expected when the distance between two
// nodes is the difference between their indices, i.e.
// cost(i->j) = i - j.
// In this case the outgoing neighbors of node i are the num_neighbors largest j
// indices.
void TestNodeNeighborhoodsForIndexDiffCostFunction(
    const int num_nodes, const int num_neighbors,
    const Model::NodeNeighborsByCostClass& node_neighbors) {
  const int all_nodes = num_nodes + 1;
  std::vector<absl::flat_hash_set<int>> incoming_neighbors_of_node(all_nodes);
  std::vector<std::vector<int>> outgoing_neighbors_of_node(all_nodes);
  // Start to all, none to start.
  const int start = 0;
  for (int neighbor = num_nodes - 1; neighbor > 0; --neighbor) {
    outgoing_neighbors_of_node[start].push_back(neighbor);
    incoming_neighbors_of_node[neighbor].insert(start);
    EXPECT_TRUE(
        node_neighbors.IsNeighborhoodArcForCostClass(1, start, neighbor));
    EXPECT_FALSE(
        node_neighbors.IsNeighborhoodArcForCostClass(1, neighbor, start));
  }
  // All to end, end to none.
  const int end = num_nodes;
  for (int neighbor = 0; neighbor < num_nodes; ++neighbor) {
    incoming_neighbors_of_node[end].insert(neighbor);
    // NOTE: End nodes aren't added to the outgoing neighbors list by default.
    // outgoing_neighbors_of_node[neighbor].push_back(end);
    EXPECT_TRUE(node_neighbors.IsNeighborhoodArcForCostClass(1, neighbor, end));
    EXPECT_FALSE(
        node_neighbors.IsNeighborhoodArcForCostClass(1, end, neighbor));
  }
  // Non-start/end to non-start/end neighbors + reverse neighborhoods.
  for (int node = 1; node < num_nodes; ++node) {
    int num_added_neighbors = 0;
    int neighbor = num_nodes - 1;
    while (num_added_neighbors++ < num_neighbors) {
      if (neighbor == node) {
        neighbor--;
      }
      DCHECK_GT(neighbor, 0);
      DCHECK_LT(neighbor, num_nodes);
      outgoing_neighbors_of_node[node].push_back(neighbor);
      incoming_neighbors_of_node[neighbor].insert(node);
      EXPECT_TRUE(
          node_neighbors.IsNeighborhoodArcForCostClass(1, node, neighbor));
      neighbor--;
    }
  }

  for (int node = 0; node <= num_nodes; ++node) {
    // Check incoming neighbors.
    std::vector<int> kExpectedInNeighbors(
        incoming_neighbors_of_node[node].begin(),
        incoming_neighbors_of_node[node].end());
    // The "incoming" cost(neighbor -> node) = neighbor - node, so the neighbors
    // list should be sorted in increasing order.
    absl::c_sort(kExpectedInNeighbors);
    EXPECT_THAT(node_neighbors.GetIncomingNeighborsOfNodeForCostClass(1, node),
                ElementsAreArray(kExpectedInNeighbors));

    // Check outgoing neighbors.
    // Note outgoing_neighbors_of_node is already sorted by construction.
    EXPECT_THAT(node_neighbors.GetOutgoingNeighborsOfNodeForCostClass(1, node),
                ElementsAreArray(outgoing_neighbors_of_node[node]));
  }
}

TEST(NodeNeighborsTest, SomeNeighbors) {
  const int kNumNodes = 20;
  IndexManager manager(20, 1, NodeIndex(0));
  Model model(manager);
  // NOTE: An offset is added to the cost function to make sure arc costs are
  // positive.
  model.SetArcCostEvaluatorOfAllVehicles(
      model.RegisterTransitCallback([](int i, int j) { return 100 + i - j; }));
  model.CloseModel();
  const Model::NodeNeighborsByCostClass* const node_neighbors =
      model.GetOrCreateNodeNeighborsByCostClass({.num_neighbors = 10});
  EXPECT_EQ(node_neighbors,
            model.GetOrCreateNodeNeighborsByCostClass({.num_neighbors = 10}));
  TestNodeNeighborhoodsForIndexDiffCostFunction(kNumNodes, 10, *node_neighbors);
}

TEST(NodeNeighborsTest, SomeNeighborsRatio) {
  const int kNumNodes = 20;
  IndexManager manager(kNumNodes, 1, NodeIndex(0));
  Model model(manager);
  // NOTE: An offset is added to the cost function to make sure arc costs are
  // positive.
  model.SetArcCostEvaluatorOfAllVehicles(
      model.RegisterTransitCallback([](int i, int j) { return 100 + i - j; }));
  model.CloseModel();
  double ratio = -1;
  const Model::NodeNeighborsByCostClass* const node_neighbors =
      model.GetOrCreateNodeNeighborsByCostClass(0.5, 3, ratio);
  EXPECT_EQ(ratio, 0.5);
  EXPECT_EQ(node_neighbors,
            model.GetOrCreateNodeNeighborsByCostClass(0.5, 3, ratio));

  TestNodeNeighborhoodsForIndexDiffCostFunction(kNumNodes, 10, *node_neighbors);
}

std::vector<int> AllIncreasingIndicesExcept(int first_index, int last_index,
                                            int index_to_skip) {
  std::vector<int> indices;
  for (int i = first_index; i <= last_index; ++i) {
    if (i != index_to_skip) indices.push_back(i);
  }
  return indices;
}

void TestFullIncreasingNeighborhood(
    const Model& model, const Model::NodeNeighborsByCostClass& node_neighbors,
    bool only_sort_neighbors_for_partial_neighborhoods) {
  ASSERT_EQ(model.vehicles(), 1);
  ASSERT_TRUE(model.IsStart(0));

  const int size = model.Size();
  for (int node = 0; node <= size; ++node) {
    for (bool incoming_neighbors : {true, false}) {
      const std::vector<int>& neighbors =
          incoming_neighbors
              ? node_neighbors.GetIncomingNeighborsOfNodeForCostClass(1, node)
              : node_neighbors.GetOutgoingNeighborsOfNodeForCostClass(1, node);
      if ((model.IsStart(node) && incoming_neighbors) ||
          (model.IsEnd(node) && !incoming_neighbors)) {
        EXPECT_THAT(neighbors, testing::IsEmpty());
      } else {
        const int node_to_skip =
            only_sort_neighbors_for_partial_neighborhoods ? -1 : node;
        EXPECT_THAT(neighbors,
                    ElementsAreArray(AllIncreasingIndicesExcept(
                        incoming_neighbors ? 0 : 1, size - 1, node_to_skip)));
      }
    }
  }
}

TEST(NodeNeighborsTest, AllNodes) {
  const int kNumNodes = 20;
  IndexManager manager(kNumNodes, 1, NodeIndex(0));
  Model model(manager);
  model.SetArcCostEvaluatorOfAllVehicles(
      model.RegisterTransitCallback([](int i, int j) {
        if (i == 0) return 0;
        return j;
      }));
  model.CloseModel();
  const int size = model.Size();
  for (bool only_sort_neighbors_for_partial_neighborhoods : {true, false}) {
    const Model::NodeNeighborsParameters params = {
        .num_neighbors = size,
        .only_sort_neighbors_for_partial_neighborhoods =
            only_sort_neighbors_for_partial_neighborhoods};
    const Model::NodeNeighborsByCostClass* const node_neighbors =
        model.GetOrCreateNodeNeighborsByCostClass(params);
    EXPECT_EQ(node_neighbors,
              model.GetOrCreateNodeNeighborsByCostClass(params));
    TestFullIncreasingNeighborhood(
        model, *node_neighbors, only_sort_neighbors_for_partial_neighborhoods);
  }
}

TEST(NodeNeighborsTest, AllNodesRatio) {
  const int kNumNodes = 20;
  IndexManager manager(kNumNodes, 1, NodeIndex(0));
  Model model(manager);
  model.SetArcCostEvaluatorOfAllVehicles(
      model.RegisterTransitCallback([](int i, int j) {
        if (i == 0) return 0;
        return j;
      }));
  model.CloseModel();
  for (bool only_sort_neighbors_for_partial_neighborhoods : {true, false}) {
    double ratio = -1;
    const Model::NodeNeighborsByCostClass* const node_neighbors =
        model.GetOrCreateNodeNeighborsByCostClass(
            1, 3, ratio, true, false,
            only_sort_neighbors_for_partial_neighborhoods);
    EXPECT_EQ(ratio, 1);
    EXPECT_EQ(node_neighbors,
              model.GetOrCreateNodeNeighborsByCostClass(
                  1, 3, ratio, true, false,
                  only_sort_neighbors_for_partial_neighborhoods));
    TestFullIncreasingNeighborhood(
        model, *node_neighbors, only_sort_neighbors_for_partial_neighborhoods);
  }
}

TEST(NodeNeighborsTest, AllNodesHighRatio) {
  const int kNumNodes = 10;
  IndexManager manager(kNumNodes, 1, NodeIndex(0));
  Model model(manager);
  model.SetArcCostEvaluatorOfAllVehicles(
      model.RegisterTransitCallback([](int i, int) { return i; }));
  model.CloseModel();
  for (bool only_sort_neighbors_for_partial_neighborhoods : {true, false}) {
    double ratio = -1;
    const Model::NodeNeighborsByCostClass* const node_neighbors =
        model.GetOrCreateNodeNeighborsByCostClass(
            0.9, 3, ratio, true, false,
            only_sort_neighbors_for_partial_neighborhoods);
    // 0.9 of 10 is 9 = num non-vehicle nodes: switching to all nodes.
    EXPECT_EQ(ratio, 1);
    EXPECT_EQ(node_neighbors,
              model.GetOrCreateNodeNeighborsByCostClass(
                  0.9, 3, ratio, true, false,
                  only_sort_neighbors_for_partial_neighborhoods));
    TestFullIncreasingNeighborhood(
        model, *node_neighbors, only_sort_neighbors_for_partial_neighborhoods);
  }
}

class MultiArmedBanditCompoundOperatorsTest
    : public BaseRoutingTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<bool, bool, bool, bool>> {
 protected:
  static const int64_t kSize;

  MultiArmedBanditCompoundOperatorsTest()
      : BaseRoutingTest(
            /*use_unary_callback=*/::testing::get<0>(GetParam()),
            /*use_light_propagation=*/::testing::get<1>(GetParam()),
            /*force_non_homogeneous_costs=*/::testing::get<2>(GetParam()),
            /*use_non_filtered_first_solution=*/::testing::get<3>(GetParam())) {
        };
  ~MultiArmedBanditCompoundOperatorsTest() override = default;
  void SetUp() override { absl::SetFlag(&FLAGS_cp_name_cast_variables, true); }
  Model* CreateModel(const RoutingModelParameters& parameters) override {
    manager_ = std::make_unique<IndexManager>(kSize, 5, NodeIndex(0));
    Model* const model = new Model(*manager_, parameters);
    EXPECT_EQ(0, model->GetNonZeroCostClassesCount());
    const int vehicle_cost =
        use_unary_callback_
            ? model->RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model->RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    return model;
  }
  int VehicleCostCount() const override { return 1; }
  RoutingSearchParameters GetSearchParameters() override {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_local_search_metaheuristic(
        LocalSearchMetaheuristic::GUIDED_LOCAL_SEARCH);
    const int64_t kMaxTimeSeconds = 10;
    parameters.mutable_time_limit()->set_seconds(kMaxTimeSeconds);
    parameters.set_use_multi_armed_bandit_concatenate_operators(true);
    return parameters;
  }
};

const int64_t MultiArmedBanditCompoundOperatorsTest::kSize = 10;

TEST_P(MultiArmedBanditCompoundOperatorsTest,
       TestMultiArmedBanditCompoundOperatorsFlagTest) {
  const Assignment* const assignment = SetUpModelAndSolve();
  ASSERT_NE(assignment, nullptr);
  ASSERT_EQ(assignment->ObjectiveValue(), 10);
}

INSTANTIATE_TEST_SUITE_P(WithParameters, MultiArmedBanditCompoundOperatorsTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

TEST(ImplicitPairTest, VehicleOnlyModel) {
  IndexManager manager(1, 10, NodeIndex(0));
  Model model(manager);
  model.CloseModel();
  EXPECT_TRUE(model.GetImplicitUniquePickupAndDeliveryPairs().empty());
}

TEST(ImplicitPairTest, NoDimensions) {
  IndexManager manager(10, 10, NodeIndex(0));
  Model model(manager);
  model.AddPickupAndDelivery(1, 2);
  model.CloseModel();
  EXPECT_TRUE(model.GetImplicitUniquePickupAndDeliveryPairs().empty());
}

TEST(ImplicitPairTest, PositiveDemandOnly) {
  IndexManager manager(10, 10, NodeIndex(0));
  Model model(manager);
  model.AddConstantDimension(1, 100, true, "count");
  model.AddPickupAndDelivery(1, 2);
  model.CloseModel();
  EXPECT_TRUE(model.GetImplicitUniquePickupAndDeliveryPairs().empty());
}

TEST(ImplicitPairTest, NegativeDemandOnly) {
  IndexManager manager(10, 10, NodeIndex(0));
  Model model(manager);
  model.AddConstantDimension(-1, 100, true, "count");
  model.AddPickupAndDelivery(1, 2);
  model.CloseModel();
  EXPECT_TRUE(model.GetImplicitUniquePickupAndDeliveryPairs().empty());
}

TEST(ImplicitPairTest, NonImplicitPairsOnly) {
  IndexManager manager(3, 10, NodeIndex(0));
  Model model(manager);
  model.AddDimension(model.RegisterUnaryTransitCallback([](int64_t index) {
    if (index == 0) return 0;
    if (index == 1) return 1;
    return -1;
  }),
                     100, 100, true, "dim");
  model.AddPickupAndDelivery(1, 2);
  model.CloseModel();
  EXPECT_TRUE(model.GetImplicitUniquePickupAndDeliveryPairs().empty());
}

TEST(ImplicitPairTest, NonUnaryDimension) {
  IndexManager manager(5, 10, NodeIndex(0));
  Model model(manager);
  model.AddDimension(model.RegisterTransitCallback([](int64_t index, int64_t) {
    if (index == 0) return 0;
    if (index % 2 == 1) return 1;
    return -1;
  }),
                     100, 100, true, "dim");
  model.CloseModel();
  EXPECT_TRUE(model.GetImplicitUniquePickupAndDeliveryPairs().empty());
}

TEST(ImplicitPairTest, ImplicitPairsOnly) {
  IndexManager manager(5, 10, NodeIndex(0));
  Model model(manager);
  model.AddDimension(model.RegisterUnaryTransitCallback([](int64_t index) {
    if (index == 0) return 0;
    if (index % 2 == 1) return 1;
    return -1;
  }),
                     100, 100, true, "unary_dim");
  model.AddDimension(model.RegisterTransitCallback([](int64_t index, int64_t) {
    if (index == 0) return 0;
    if (index == 2) return 2;
    if (index == 3) return -2;
    return 1;
  }),
                     100, 100, true, "dim");
  model.AddDimension(model.RegisterUnaryTransitCallback([](int64_t index) {
    if (index == 0) return 0;
    return 1;
  }),
                     100, 100, true, "non_implicit_unary_dim");
  model.CloseModel();
  const auto& pairs = model.GetImplicitUniquePickupAndDeliveryPairs();
  const std::vector<std::pair<int64_t, int64_t>> expected_pairs = {
      {1, 2}, {1, 4}, {3, 2}, {3, 4}};
  EXPECT_EQ(expected_pairs.size(), pairs.size());
  int count = 0;
  for (const auto [pickup, delivery] : expected_pairs) {
    EXPECT_THAT(pairs[count],
                FieldsAre(ElementsAre(pickup), ElementsAre(delivery)));
    ++count;
  }
}

TEST(ImplicitPairTest, MixedNodes) {
  IndexManager manager(6, 10, NodeIndex(0));
  Model model(manager);
  // Explicit pickup/delivery (1,2)
  // Implicit pickup/delivery (3,4)
  // Singleton (5)
  model.AddDimension(model.RegisterUnaryTransitCallback([](int64_t index) {
    if (index == 0) return 0;
    if (index == 1 || index == 3) return 1;
    if (index == 2 || index == 4) return -1;
    return 2;
  }),
                     100, 100, true, "dim");
  model.AddPickupAndDelivery(1, 2);
  model.CloseModel();
  EXPECT_THAT(model.GetImplicitUniquePickupAndDeliveryPairs(),
              ElementsAre(FieldsAre(ElementsAre(3), ElementsAre(4))));
}

TEST(ImplicitPairTest, MakePairActive) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_first_solution_strategy(
      FirstSolutionStrategy::ALL_UNPERFORMED);
  IndexManager manager(3, 1, NodeIndex(0));
  Model model(manager);
  model.AddDimension(model.RegisterUnaryTransitCallback([](int64_t index) {
    if (index == 0) return 0;
    if (index % 2 == 1) return 1;
    return -1;
  }),
                     0, 1, true, "dim");
  const Dimension& dimension = model.GetDimensionOrDie("dim");
  dimension.CumulVar(model.End(0))->SetValue(0);
  model.AddDisjunction({1}, 1000);
  model.AddDisjunction({2}, 1000);
  const Assignment* const solution = model.SolveWithParameters(parameters);
  ASSERT_TRUE(solution != nullptr);
  EXPECT_EQ(solution->ObjectiveValue(), 0);
}

TEST(SolveFromMultipleFirstSolutions, EmptyInitialAssignments) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  IndexManager manager(3, 2, NodeIndex(0));
  Model model(manager);
  const Assignment* solution =
      model.SolveFromAssignmentsWithParameters({}, parameters);
  // First solution strategy is used.
  EXPECT_TRUE(solution != nullptr);
}

TEST(SolveFromMultipleFirstSolutions, NullInitialAssignments) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  IndexManager manager(3, 2, NodeIndex(0));
  Model model(manager);
  const Assignment* solution =
      model.SolveFromAssignmentsWithParameters({nullptr, nullptr}, parameters);
  // First solution strategy is used.
  EXPECT_TRUE(solution != nullptr);
}

void SetupRoutes(const Model& model, Assignment* assignment) {
  assignment->Add(model.NextVar(0))->SetValue(2);
  assignment->Add(model.NextVar(1))->SetValue(5);
  assignment->Add(model.NextVar(2))->SetValue(4);
  assignment->Add(model.NextVar(3))->SetValue(1);
}

void SetupExtraVariables(const Model& model, Assignment* assignment) {
  assignment->Add(model.ActiveVar(0))->SetValue(1);
  assignment->Add(model.ActiveVar(1))->SetValue(1);
  assignment->Add(model.ActiveVar(2))->SetValue(1);
  assignment->Add(model.ActiveVar(3))->SetValue(1);
  assignment->Add(model.VehicleVar(0))->SetValue(0);
  assignment->Add(model.VehicleVar(1))->SetValue(1);
  assignment->Add(model.VehicleVar(2))->SetValue(0);
  assignment->Add(model.VehicleVar(3))->SetValue(1);
  assignment->Add(model.VehicleVar(4))->SetValue(0);
  assignment->Add(model.VehicleVar(5))->SetValue(1);
  assignment->AddObjective(model.CostVar());
  assignment->SetObjectiveValue(0);
}

TEST(SolveFromMultipleFirstSolutions, PartiallyNullInitialAssignments) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  IndexManager manager(3, 2, NodeIndex(0));
  Model model(manager);
  Assignment assignment(model.solver());
  SetupRoutes(model, &assignment);
  const Assignment* solution = model.SolveFromAssignmentsWithParameters(
      {nullptr, &assignment, nullptr}, parameters);
  ASSERT_TRUE(solution != nullptr);
  SetupExtraVariables(model, &assignment);
  EXPECT_EQ(*solution, assignment);
}

TEST(SolveFromMultipleFirstSolutions, InitialAssignmentsWithFallback) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  IndexManager manager(3, 2, NodeIndex(0));
  Model model(manager);
  Assignment assignment(model.solver());
  SetupRoutes(model, &assignment);
  // Break solution which will force fallback to be used.
  assignment.Add(model.NextVar(2))->SetValue(2);
  Assignment fallback(model.solver());
  SetupRoutes(model, &fallback);
  const Assignment* solution = model.SolveFromAssignmentsWithParameters(
      {&assignment, &fallback}, parameters);
  ASSERT_TRUE(solution != nullptr);
  SetupExtraVariables(model, &fallback);
  EXPECT_EQ(*solution, fallback);
}

TEST(SolveFromFirstSolutionHint, InitialAssignmentFallbacksToHint) {
  for (bool use_hint : {true, false}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_fallback_to_cp_sat_size_threshold(0);
    parameters.set_solution_limit(1);
    IndexManager manager(3, 2, NodeIndex(0));
    Model model(manager);
    Assignment assignment(model.solver());
    assignment.Add(model.NextVar(0))->SetValue(1);
    assignment.Add(model.NextVar(1))->SetValue(2);
    assignment.Add(model.NextVar(2))->SetValue(4);
    assignment.Add(model.NextVar(3))->SetValue(5);
    // Cannot have more than one non-start/endnode on a route.
    model.AddConstantDimension(/*value=*/1, /*capacity=*/2,
                               /*fix_start_cumul_to_zero=*/true, "count");
    const int callback =
        model.RegisterTransitCallback([](int64_t, int64_t) { return 1; });
    model.SetArcCostEvaluatorOfAllVehicles(callback);
    // Make the first vehicle (used in the hint) very expensive. If the hint is
    // followed, the first vehicle will still be used after solving.
    model.SetFixedCostOfVehicle(/*cost=*/1000000, /*vehicle=*/0);
    if (use_hint) {
      // Hinting with LCI to use the first solution as much as possible.
      model.SetFirstSolutionHint(&assignment);
      parameters.set_first_solution_strategy(
          FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION);
    }
    const Assignment* solution = model.SolveFromAssignmentsWithParameters(
        {&assignment, nullptr}, parameters);
    EXPECT_EQ(solution != nullptr, use_hint);
    if (use_hint) {
      EXPECT_EQ(solution->Value(model.NextVar(0)), 1);
      EXPECT_EQ(solution->Value(model.NextVar(1)), 4);
      EXPECT_EQ(solution->Value(model.NextVar(2)), 5);
      EXPECT_EQ(solution->Value(model.NextVar(3)), 2);
    }
  }
}

TEST(SolveWithFullPropagation, MultipleDimensionVehicleClasses) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_use_full_propagation(true);
  IndexManager manager(2, 3, NodeIndex(0));
  Model model(manager);
  const int one =
      model.RegisterTransitCallback([](int64_t, int64_t) { return 0; });
  const int two =
      model.RegisterTransitCallback([](int64_t, int64_t) { return 10; });
  model.AddDimensionWithVehicleTransits({one, one, two}, 100, 100, true, "dim");
  model.GetMutableDimension("dim")->SetSpanCostCoefficientForAllVehicles(1);
  // Forcing route on 3rd vehicle to visit node 1.
  model.NextVar(model.Start(2))->SetValue(1);
  model.NextVar(1)->SetValue(model.End(2));
  const Assignment* const solution = model.SolveWithParameters(parameters);
  EXPECT_EQ(20, solution->Value(
                    model.GetDimensionOrDie("dim").CumulVar(model.End(2))));
}

TEST(AreRoutesInterdependentTest, NotInterdependent) {
  IndexManager manager(4, 2, NodeIndex(0));
  Model model(manager);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  EXPECT_FALSE(model.AreRoutesInterdependent(parameters));
}

TEST(AreRoutesInterdependentTest, TabuSearch) {
  IndexManager manager(4, 2, NodeIndex(0));
  Model model(manager);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_local_search_metaheuristic(
      LocalSearchMetaheuristic::TABU_SEARCH);
  EXPECT_TRUE(model.AreRoutesInterdependent(parameters));
}

TEST(AreRoutesInterdependentTest, GenericTabuSearch) {
  IndexManager manager(4, 2, NodeIndex(0));
  Model model(manager);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_local_search_metaheuristic(
      LocalSearchMetaheuristic::GENERIC_TABU_SEARCH);
  EXPECT_TRUE(model.AreRoutesInterdependent(parameters));
}

TEST(AreRoutesInterdependentTest, GlobalSpanCost) {
  IndexManager manager(4, 2, NodeIndex(0));
  Model model(manager);
  model.AddConstantDimension(/*value=*/10, /*capacity=*/10000,
                             /*fix_start_cumul_to_zero=*/false,
                             /*name=*/"dim");
  model.GetMutableDimension("dim")->SetGlobalSpanCostCoefficient(10);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  model.CloseModelWithParameters(parameters);
  EXPECT_TRUE(model.AreRoutesInterdependent(parameters));
}

TEST(AreRoutesInterdependentTest, Precedences) {
  IndexManager manager(4, 2, NodeIndex(0));
  Model model(manager);
  model.AddConstantDimension(/*value=*/10, /*capacity=*/10000,
                             /*fix_start_cumul_to_zero=*/false,
                             /*name=*/"dim");
  model.GetMutableDimension("dim")->AddNodePrecedence(
      {/*first_node*/ 1, /*second_node*/ 2, /*offset*/ 4});
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  model.CloseModelWithParameters(parameters);
  EXPECT_TRUE(model.AreRoutesInterdependent(parameters));
}

TEST(AreRoutesInterdependentTest, Resources) {
  IndexManager manager(4, 2, NodeIndex(0));
  Model model(manager);
  model.AddConstantDimension(/*value=*/10, /*capacity=*/10000,
                             /*fix_start_cumul_to_zero=*/false,
                             /*name=*/"dim");
  ResourceGroup& resource_group = *model.AddResourceGroup();
  resource_group.AddResource(Attributes(Domain(0, 100), Domain(0, 1)),
                             model.GetMutableDimension("dim"));
  resource_group.NotifyVehicleRequiresAResource(0);
  resource_group.NotifyVehicleRequiresAResource(1);
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  model.CloseModelWithParameters(parameters);
  EXPECT_TRUE(model.AreRoutesInterdependent(parameters));
}

TEST(UnaryDimensionsTest, ManyTransitDimensionWithOptionalNodes) {
  const int num_vehicles = 2;
  const int num_nodes = 6;
  IndexManager manager(num_nodes, num_vehicles, NodeIndex(0));
  for (bool all_transits_identical : {true, false}) {
    Model model(manager);
    for (int i = 1; i < num_nodes; ++i) {
      model.AddDisjunction({manager.NodeToIndex(NodeIndex(i))}, 1000);
    }
    std::vector<int> transits;
    if (all_transits_identical) {
      const int transit =
          model.RegisterUnaryTransitCallback([](int64_t) { return 1; });
      transits.resize(num_vehicles, transit);
    } else {
      for (int i = 0; i < num_vehicles; ++i) {
        transits.push_back(
            model.RegisterUnaryTransitCallback([](int64_t) { return 1; }));
      }
    }
    model.AddDimensionWithVehicleTransits(
        transits, /*slack_max=*/0, /*capacity=*/3,
        /*fix_start_cumul_to_zero=*/true, "dim");
    const Assignment* solution = model.Solve();
    EXPECT_NE(solution, nullptr);
    int num_active_nodes = 0;
    for (int i = 0; i < num_nodes; ++i) {
      if (solution->Value(model.ActiveVar(i))) {
        ++num_active_nodes;
      }
    }
    EXPECT_EQ(num_active_nodes, 5);
  }
}

TEST(RoutingTest, PickupToDeliveryLimitEnforcedInSolve) {
  const IndexManager manager(/*num_nodes=*/5, /*num_vehicles=*/1, NodeIndex(0));
  for (const int64_t limit : {3, 1, 0}) {
    Model model(manager);
    // Encourage interleaving 0 -> 1 -> 3 -> 4 -> 2 -> 0 if allowed by the
    // pickup-to-delivery limit on pair (1, 2).
    const int cost_index =
        model.RegisterTransitCallback([](int64_t from_index, int64_t to_index) {
          if ((from_index == 1 && to_index == 3) ||
              (from_index == 4 && to_index == 2)) {
            return 0;
          }
          return 10;
        });
    model.SetArcCostEvaluatorOfAllVehicles(cost_index);
    model.AddConstantDimension(/*value=*/1, /*capacity=*/10,
                               /*fix_start_cumul_to_zero=*/true, "time");
    Dimension* const time_dimension = model.GetMutableDimension("time");
    model.AddPickupAndDelivery(1, 2);
    model.AddPickupAndDelivery(3, 4);
    time_dimension->SetPickupToDeliveryLimitFunctionForPair(
        [limit](int /*pickup*/, int /*delivery*/) { return limit; },
        /*pair_index=*/0);

    const Assignment* const solution = model.Solve();
    if (limit == 0) {
      EXPECT_EQ(solution, nullptr);
    } else {
      ASSERT_NE(solution, nullptr);
      const int64_t pickup_cumul = solution->Value(time_dimension->CumulVar(1));
      const int64_t delivery_cumul =
          solution->Value(time_dimension->CumulVar(2));
      EXPECT_LE(delivery_cumul - pickup_cumul, limit);
      if (limit == 3) {
        // Interleaved route 0 -> 1 -> 3 -> 4 -> 2 -> 0 is feasible and cheaper.
        EXPECT_EQ(delivery_cumul - pickup_cumul, 3);
      } else {
        // Limit of 1 forces 2 immediately after 1.
        EXPECT_EQ(delivery_cumul - pickup_cumul, 1);
      }
    }
  }
}

}  // namespace
}  // namespace operations_research::routing

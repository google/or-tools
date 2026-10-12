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

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/functional/bind_front.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "google/protobuf/duration.pb.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/path.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/routing.h"
#include "ortools/routing/types.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_parameters.pb.h"  // IWYU pragma: keep
#include "ortools/util/file_util.h"
#include "ortools/util/optional_boolean.pb.h"
#include "ortools/util/sorted_interval_list.h"

// Declaring flags locally to test effect of parameters on CP-SAT models.
ABSL_DECLARE_FLAG(std::string, cp_model_dump_prefix);
ABSL_DECLARE_FLAG(bool, cp_model_dump_models);

namespace operations_research::routing {
namespace {

#ifdef USE_UNARY
int64_t ReturnFirstNodeValueFromVariableIndex(const IndexManager& manager,
                                              int64_t i) {
  return manager.IndexToNode(i).value();
}
#else
int64_t ReturnFirstNodeValueFromVariableIndex(const IndexManager& manager,
                                              int64_t i, int64_t /*j*/) {
  return manager.IndexToNode(i).value();
}
#endif

TEST(CPSATSearchTest, SolveTSPModel) {
  for (bool use_scaling : {false, true}) {
    for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
      for (bool use_generalized_cp_sat : {false, true}) {
        RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
        if (use_generalized_cp_sat) {
          parameters.set_use_generalized_cp_sat(BOOL_TRUE);
        } else {
          parameters.set_use_cp_sat(BOOL_TRUE);
        }
        parameters.set_use_cp(use_cp);
        if (use_scaling) {
          parameters.set_log_cost_scaling_factor(2.3);
          parameters.set_log_cost_offset(1.1);
        }
        IndexManager manager(4, 1, NodeIndex(0));
        Model model(manager);
#ifdef USE_UNARY
        const int vehicle_cost = model.RegisterUnaryTransitCallback(
            absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
        const int vehicle_cost = model.RegisterTransitCallback(
            absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
        model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
        std::vector<const Assignment*> solutions;
        const Assignment* const assignment =
            model.SolveWithParameters(parameters, &solutions);
        EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
        // CP-SAT and CP find the same solution so we only collect one solution.
        EXPECT_EQ(1, solutions.size());
        EXPECT_EQ(1 + 2 + 3, assignment->ObjectiveValue());
      }
    }
  }
}

// CP-SAT models do not support disjoint time windows, makes proof of optimality
// impossible in general. Checking if the objective lower bound is correct.
TEST(CPSATSearchTest, SolveTSPModelWithDisjointTimeWindows) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    for (bool use_generalized_cp_sat : {false, true}) {
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      if (use_generalized_cp_sat) {
        parameters.set_use_generalized_cp_sat(BOOL_TRUE);
      } else {
        parameters.set_use_cp_sat(BOOL_TRUE);
      }
      parameters.set_use_cp(use_cp);
      IndexManager manager(4, 1, NodeIndex(0));
      Model model(manager);
      model.AddConstantDimensionWithSlack(/*value = */ 1, /*capacity = */ 1000,
                                          /* slack_max = */ 1000,
                                          /*fix_start_cumul_to_zero=*/true,
                                          "dim");
      Dimension* dim = model.GetMutableDimension("dim");
      dim->SetSpanCostCoefficientForAllVehicles(10);
      for (int i = 0; i < 3; ++i) {
        dim->CumulVar(i)->SetRange(0, 100);
        Solver* const solver = model.solver();
        // Nodes cannot be reached in [1,2]; since the route starts at 0, there
        // will be extra span cost.
        const std::vector<int64_t> starts = {1};
        const std::vector<int64_t> ends = {2};
        solver->AddConstraint(
            solver->MakeNotMemberCt(dim->CumulVar(i), starts, ends));
      }
      std::vector<const Assignment*> solutions;
      const Assignment* const assignment =
          model.SolveWithParameters(parameters, &solutions);
      EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_SUCCESS);
      EXPECT_EQ(1, solutions.size());
      EXPECT_LE(10 * (1 + 2 + 1 + 1), assignment->ObjectiveValue());
      EXPECT_GE(10 * (1 + 3 + 1 + 1), assignment->ObjectiveValue());
      EXPECT_EQ(10 * (1 + 1 + 1 + 1), model.objective_lower_bound());
    }
  }
}

TEST(CPSATSearchTest, SolveTSPModelWithSoftUB) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    for (bool use_generalized_cp_sat : {false, true}) {
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      if (use_generalized_cp_sat) {
        parameters.set_use_generalized_cp_sat(BOOL_TRUE);
      } else {
        parameters.set_use_cp_sat(BOOL_TRUE);
      }
      parameters.set_use_cp(use_cp);
      IndexManager manager(4, 1, NodeIndex(0));
      Model model(manager);
#ifdef USE_UNARY
      const int vehicle_cost = model.RegisterUnaryTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
      const int vehicle_cost = model.RegisterTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
      model.AddConstantDimensionWithSlack(/*value = */ 1, /*capacity = */ 1000,
                                          /* slack_max = */ 1000,
                                          /*fix_start_cumul_to_zero=*/false,
                                          "dim");
      Dimension* dim = model.GetMutableDimension("dim");
      // One of node 1 or 2 will violate the soft upper bound, ideally node 2 as
      // it is cheaper.
      dim->SetCumulVarSoftUpperBound(/*index=*/1, /*upper_bound=*/1,
                                     /*coefficient=*/20);
      dim->SetCumulVarSoftUpperBound(/*index=*/2, /*upper_bound=*/1,
                                     /*coefficient=*/10);
      std::vector<const Assignment*> solutions;
      const Assignment* const assignment =
          model.SolveWithParameters(parameters, &solutions);
      EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
      // CP-SAT and CP find the same solution so we only collect one solution.
      EXPECT_EQ(1, solutions.size());
      EXPECT_EQ(1 + 2 + 3 + 10 * 1, assignment->ObjectiveValue());
    }
  }
}

TEST(CPSATSearchTest, SolveTSPModelWithSoftLB) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    for (bool use_generalized_cp_sat : {false, true}) {
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      if (use_generalized_cp_sat) {
        parameters.set_use_generalized_cp_sat(BOOL_TRUE);
      } else {
        parameters.set_use_cp_sat(BOOL_TRUE);
      }
      parameters.set_use_cp(use_cp);
      IndexManager manager(4, 1, NodeIndex(0));
      Model model(manager);
#ifdef USE_UNARY
      const int vehicle_cost = model.RegisterUnaryTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
      const int vehicle_cost = model.RegisterTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
      model.AddConstantDimensionWithSlack(/*value = */ 1, /*capacity = */ 1000,
                                          /* slack_max = */ 1000,
                                          /*fix_start_cumul_to_zero=*/false,
                                          "dim");
      Dimension* dim = model.GetMutableDimension("dim");
      // One of node 1 or 2 will violate the soft lower bound, ideally node 2 as
      // it is cheaper.
      dim->SetCumulVarSoftLowerBound(/*index=*/1, /*lower_bound=*/999,
                                     /*coefficient=*/20);
      dim->SetCumulVarSoftLowerBound(/*index=*/2, /*lower_bound=*/999,
                                     /*coefficient=*/10);
      std::vector<const Assignment*> solutions;
      const Assignment* const assignment =
          model.SolveWithParameters(parameters, &solutions);
      EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
      // CP-SAT and CP find the same solution so we only collect one solution.
      EXPECT_EQ(1, solutions.size());
      EXPECT_EQ(1 + 2 + 3 + 10 * 1, assignment->ObjectiveValue());
    }
  }
}

TEST(CPSATSearchTest, SolveTSPModelWithSoftLBAndUB) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    for (bool use_generalized_cp_sat : {false, true}) {
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      if (use_generalized_cp_sat) {
        parameters.set_use_generalized_cp_sat(BOOL_TRUE);
      } else {
        parameters.set_use_cp_sat(BOOL_TRUE);
      }
      parameters.set_use_cp(use_cp);
      IndexManager manager(4, 1, NodeIndex(0));
      Model model(manager);
#ifdef USE_UNARY
      const int vehicle_cost = model.RegisterUnaryTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
      const int vehicle_cost = model.RegisterTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
      model.AddConstantDimensionWithSlack(/*value = */ 1, /*capacity = */ 1000,
                                          /* slack_max = */ 1000,
                                          /*fix_start_cumul_to_zero=*/false,
                                          "dim");
      Dimension* dim = model.GetMutableDimension("dim");
      // One of node 1 or 2 will violate soft bounds, ideally node 2 as it is
      // cheaper.
      dim->SetCumulVarSoftLowerBound(/*index=*/1, /*lower_bound=*/1,
                                     /*coefficient=*/20);
      dim->SetCumulVarSoftLowerBound(/*index=*/2, /*lower_bound=*/1,
                                     /*coefficient=*/10);
      dim->SetCumulVarSoftUpperBound(/*index=*/1, /*upper_bound=*/1,
                                     /*coefficient=*/20);
      dim->SetCumulVarSoftUpperBound(/*index=*/2, /*upper_bound=*/1,
                                     /*coefficient=*/10);
      std::vector<const Assignment*> solutions;
      const Assignment* const assignment =
          model.SolveWithParameters(parameters, &solutions);
      EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
      // CP-SAT and CP find the same solution so we only collect one solution.
      EXPECT_EQ(1, solutions.size());
      EXPECT_EQ(1 + 2 + 3 + 10 * 1, assignment->ObjectiveValue());
    }
  }
}

TEST(CPSATSearchTest, SolveTSPWithUnperformedModel) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    for (bool use_generalized_cp_sat : {false, true}) {
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      if (use_generalized_cp_sat) {
        parameters.set_use_generalized_cp_sat(BOOL_TRUE);
      } else {
        parameters.set_use_cp_sat(BOOL_TRUE);
      }
      parameters.set_use_cp(use_cp);
      parameters.set_log_search(true);
      IndexManager manager(4, 1, NodeIndex(0));
      Model model(manager);
#ifdef USE_UNARY
      const int vehicle_cost = model.RegisterUnaryTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
      const int vehicle_cost = model.RegisterTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
      // Can perform at most 2 non-start/end nodes.
      model.AddConstantDimensionWithSlack(/*value = */ 1, /*capacity = */ 3,
                                          /* slack_max = */ 3,
                                          /*fix_start_cumul_to_zero=*/false,
                                          "dim");
      for (NodeIndex i(1); i < NodeIndex(4); ++i) {
        model.AddDisjunction(manager.NodesToIndices({i}), 100);
      }
      std::vector<const Assignment*> solutions;
      const Assignment* const assignment =
          model.SolveWithParameters(parameters, &solutions);
      EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
      // CP-SAT and CP find the same solution so we only collect one solution.
      EXPECT_EQ(1, solutions.size());
      EXPECT_EQ(1 + 2 + 100, assignment->ObjectiveValue());
    }
  }
}

// Test that "delayed" active nodes (ones which are part of a disjunction
// singleton with kNoPenalty penalty) are properly handled in the cp-sat model.
TEST(CPSATSearchTest, SolveTSPWithDelayedActiveNodes) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    // generalized_cp_sat will detect penalties properly
    parameters.set_use_cp_sat(BOOL_TRUE);
    parameters.set_use_cp(use_cp);
    IndexManager manager(4, 1, NodeIndex(0));
    Model model(manager);
#ifdef USE_UNARY
    const int vehicle_cost = model.RegisterUnaryTransitCallback(
        absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    for (NodeIndex i(1); i < NodeIndex(4); ++i) {
      model.AddDisjunction(manager.NodesToIndices({i}), Model::kNoPenalty);
    }
    std::vector<const Assignment*> solutions;
    const Assignment* const assignment =
        model.SolveWithParameters(parameters, &solutions);
    EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
    EXPECT_EQ(1, solutions.size());
    EXPECT_EQ(1 + 2 + 3, assignment->ObjectiveValue());
  }
}

TEST(CPSATSearchTest, SolveVRPWithSoftLBAndUB) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    for (bool use_generalized_cp_sat : {false, true}) {
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      if (use_generalized_cp_sat) {
        parameters.set_use_generalized_cp_sat(BOOL_TRUE);
      } else {
        parameters.set_use_cp_sat(BOOL_TRUE);
      }
      parameters.set_use_cp(use_cp);
      parameters.set_log_search(true);
      IndexManager manager(3, 2, NodeIndex(0));
      Model model(manager);
      const int vehicle_cost = model.RegisterTransitCallback(
          [](int /*from*/, int /*to*/) { return 100; });
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
      model.SetFixedCostOfAllVehicles(0);
      model.AddConstantDimensionWithSlack(/*value = */ 1, /*capacity = */ 1000,
                                          /* slack_max = */ 1000,
                                          /*fix_start_cumul_to_zero=*/false,
                                          "dim");
      Dimension* dim = model.GetMutableDimension("dim");
      // Nodes will not end up on different vehicles as soft bounds cost are
      // small.
      dim->SetCumulVarSoftLowerBound(/*index=*/1, /*lower_bound=*/1,
                                     /*coefficient=*/20);
      dim->SetCumulVarSoftLowerBound(/*index=*/2, /*lower_bound=*/1,
                                     /*coefficient=*/10);
      dim->SetCumulVarSoftUpperBound(/*index=*/1, /*upper_bound=*/1,
                                     /*coefficient=*/20);
      dim->SetCumulVarSoftUpperBound(/*index=*/2, /*upper_bound=*/1,
                                     /*coefficient=*/10);
      std::vector<const Assignment*> solutions;
      const Assignment* const assignment =
          model.SolveWithParameters(parameters, &solutions);
      EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
      // CP-SAT and CP find the same solution so we only collect one solution.
      EXPECT_EQ(1, solutions.size());
      EXPECT_EQ(100 + 100 + 100 + 10, assignment->ObjectiveValue());
    }
  }
}

TEST(CPSATSearchTest, SolveVRPCallbacks) {
  for (bool report_intermediate_cp_sat_solutions : {false, true}) {
    for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
      for (bool use_generalized_cp_sat : {false, true}) {
        RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
        parameters.set_report_intermediate_cp_sat_solutions(
            report_intermediate_cp_sat_solutions);
        if (use_generalized_cp_sat) {
          parameters.set_use_generalized_cp_sat(BOOL_TRUE);
        } else {
          parameters.set_use_cp_sat(BOOL_TRUE);
        }
        parameters.set_use_cp(use_cp);
        parameters.set_log_search(true);
        IndexManager manager(3, 2, NodeIndex(0));
        Model model(manager);
        const int vehicle_cost = model.RegisterTransitCallback(
            [](int /*from*/, int /*to*/) { return 100; });
        model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
        model.SetFixedCostOfAllVehicles(0);
        model.AddConstantDimensionWithSlack(
            /*value = */ 1, /*capacity = */ 1000,
            /* slack_max = */ 1000,
            /*fix_start_cumul_to_zero=*/false, "dim");
        Dimension* dim = model.GetMutableDimension("dim");
        dim->CumulVar(1)->SetRange(1, 3);
        dim->CumulVar(2)->SetRange(2, 30);
        std::vector<const Assignment*> solutions;
        int count = 0;
        model.AddAtSolutionCallback([&count]() { ++count; });
        const Assignment* const assignment =
            model.SolveWithParameters(parameters, &solutions);
        EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
        // CP-SAT and CP find the same solution so we only collect one solution.
        EXPECT_EQ(1, solutions.size());
        EXPECT_EQ(100 + 100 + 100, assignment->ObjectiveValue());
        EXPECT_EQ(1 + (report_intermediate_cp_sat_solutions ? 1 : 0) +
                      (use_cp == BOOL_TRUE ? 1 : 0),
                  count);
      }
    }
  }
}

TEST(CPSATSearchTest, SolveVRPWithUnperformedModel) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    for (bool use_generalized_cp_sat : {false, true}) {
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      if (use_generalized_cp_sat) {
        parameters.set_use_generalized_cp_sat(BOOL_TRUE);
      } else {
        parameters.set_use_cp_sat(BOOL_TRUE);
      }
      parameters.set_use_cp(use_cp);
      IndexManager manager(3, 2, NodeIndex(0));
      Model model(manager);
      const int vehicle_cost = model.RegisterTransitCallback(
          [](int /*from*/, int /*to*/) { return 100; });
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
      model.SetFixedCostOfAllVehicles(0);
      model.AddDisjunction({1}, 500);
      model.AddDisjunction({2}, 10);
      std::vector<const Assignment*> solutions;
      const Assignment* const assignment =
          model.SolveWithParameters(parameters, &solutions);
      EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
      // CP-SAT and CP find the same solution so we only collect one solution.
      EXPECT_EQ(1, solutions.size());
      EXPECT_EQ(100 + 100 + 10, assignment->ObjectiveValue());
    }
  }
}

TEST(CPSATSearchTest, SolveAlternativesVRPModelWithForcedActive) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_use_generalized_cp_sat(BOOL_TRUE);
    parameters.set_use_cp(use_cp);
    parameters.set_log_search(true);
    IndexManager manager(5, 2, NodeIndex(0));
    Model model(manager);
    const int vehicle_cost = model.RegisterTransitCallback(
        [](int /*from*/, int /*to*/) { return 10; });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    model.SetFixedCostOfAllVehicles(0);
    model.AddDisjunction({1, 2, 3}, /*penalty=*/50);
    model.ActiveVar(3)->SetValues({1});
    model.AddDisjunction({4}, 5);
    std::vector<const Assignment*> solutions;
    const Assignment* const assignment =
        model.SolveWithParameters(parameters, &solutions);
    EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
    // CP-SAT and CP find the same solution so we only collect one solution.
    EXPECT_EQ(1, solutions.size());
    // 4 -> 4 (unperformed)
    // 0 -> 3 -> 5 (2 arcs)
    EXPECT_EQ(5 + 2 * 10, assignment->ObjectiveValue());
    EXPECT_EQ(assignment->Value(model.NextVar(4)), 4);
    EXPECT_EQ(assignment->Value(model.ActiveVar(3)), 1);
    EXPECT_EQ(assignment->Value(model.ActiveVar(5)), 1);
  }
}

TEST(CPSATSearchTest, SolveAlternativesVRPModelWithDoubleDisjunctions) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_use_generalized_cp_sat(BOOL_TRUE);
    parameters.set_use_cp(use_cp);
    parameters.set_log_search(true);
    IndexManager manager(5, 2, NodeIndex(0));
    Model model(manager);
#ifdef USE_UNARY
    const int vehicle_cost = model.RegisterUnaryTransitCallback(
        absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    model.SetFixedCostOfAllVehicles(0);
    model.AddDisjunction({1, 2, 3, 4}, /*penalty=*/0, /*max_cardinality=*/3);
    model.AddDisjunction({1}, 100);
    model.AddDisjunction({2}, -1);
    model.AddDisjunction({3}, kint64max);
    model.ActiveVar(4)->SetValues({1});
    std::vector<const Assignment*> solutions;
    const Assignment* const assignment =
        model.SolveWithParameters(parameters, &solutions);
    ASSERT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
    // CP-SAT and CP find the same solution so we only collect one solution.
    EXPECT_EQ(1, solutions.size());
    // 1 -> 1 (unperformed)
    // 0 -> 2 -> 3 -> 4 -> 5
    EXPECT_EQ(100 + 2 + 3 + 4, assignment->ObjectiveValue());
  }
}

TEST(CPSATSearchTest, SolveAlternativesVRPModelWithSoftMaxDisjunction) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_use_generalized_cp_sat(BOOL_TRUE);
    parameters.set_use_cp(use_cp);
    parameters.set_log_search(true);
    IndexManager manager(5, 2, NodeIndex(0));
    Model model(manager);
#ifdef USE_UNARY
    const int vehicle_cost = model.RegisterUnaryTransitCallback(
        absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    model.SetFixedCostOfAllVehicles(0);
    // Create soft max disjunction: soft max cardinality = 1, penalizing once
    // with penalty 10.
    const DisjunctionIndex disj_index = model.MakeDisjunction(
        {manager.NodeToIndex(NodeIndex(1)), manager.NodeToIndex(NodeIndex(2)),
         manager.NodeToIndex(NodeIndex(3))});
    model.SetDisjunctionHardMaximum(disj_index, 3);
    model.SetDisjunctionSoftMaximum(disj_index, /*soft_max_cardinality=*/1,
                                    /*penalty=*/10,
                                    Model::PenaltyCostBehavior::PENALIZE_ONCE);

    // Force nodes 1 and 2 to be active. This will violate the soft max
    // cardinality of 1.
    model.ActiveVar(1)->SetValues({1});
    model.ActiveVar(2)->SetValues({1});

    model.AddDisjunction({manager.NodeToIndex(NodeIndex(4))}, 0);

    std::vector<const Assignment*> solutions;
    const Assignment* const assignment =
        model.SolveWithParameters(parameters, &solutions);
    ASSERT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
    // CP-SAT and CP find the same solution so we only collect one solution.
    EXPECT_EQ(1, solutions.size());
    // 0 -> 1 -> 2 -> 5 (0 is node 0, 1 is node 1, 2 is node 2, 5 is end node 0)
    // 3 and 4 are unperformed
    // Transit cost: 0 (from 0) + 1 (from 1) + 2 (from 2) = 3
    // Objective is transit cost + soft max penalty (10) = 13
    EXPECT_EQ(10 + 1 + 2, assignment->ObjectiveValue());
  }
}

TEST(CPSATSearchTest, SolveAlternativesVRPModelWithSoftMinDisjunction) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_use_generalized_cp_sat(BOOL_TRUE);
    parameters.set_use_cp(use_cp);
    parameters.set_log_search(true);
    IndexManager manager(5, 2, NodeIndex(0));
    Model model(manager);
#ifdef USE_UNARY
    const int vehicle_cost = model.RegisterUnaryTransitCallback(
        absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    model.SetFixedCostOfAllVehicles(0);

    const DisjunctionIndex disj_index = model.MakeDisjunction(
        {manager.NodeToIndex(NodeIndex(1)), manager.NodeToIndex(NodeIndex(2)),
         manager.NodeToIndex(NodeIndex(3))});
    model.SetDisjunctionHardMaximum(disj_index, 3);
    model.SetDisjunctionSoftMinimum(
        disj_index, /*soft_min_cardinality=*/2, /*penalty=*/10,
        Model::PenaltyCostBehavior::PENALIZE_PER_INACTIVE);

    // Node 4 is optional but has high penalty so it should be visited.
    model.AddDisjunction({manager.NodeToIndex(NodeIndex(4))}, 100);

    std::vector<const Assignment*> solutions;
    const Assignment* const assignment =
        model.SolveWithParameters(parameters, &solutions);
    ASSERT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
    // CP-SAT should choose to visit 2 nodes from {1, 2, 3} to satisfy the soft
    // min constraint and avoid the penalty of 10 per unvisited node (cost 20).
    // The nodes 1 and 2 have smaller traverse costs (1 and 2), while 3 has
    // cost 3. So it visits 1 and 2. It also visits 4 (cost 4). Let's trace the
    // optimal path: 0 -> 1 -> 2 -> 4 -> end. Cost: 0 + 1 + 2 + 4 = 7. No soft
    // min penalty since we visited 2 nodes.
    EXPECT_EQ(7, assignment->ObjectiveValue());
  }
}

TEST(CPSATSearchTest, SolveCapacitatedAlternativesVRPModelWithForcedActive) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_use_generalized_cp_sat(BOOL_TRUE);
    parameters.set_use_cp(use_cp);
    parameters.set_log_search(true);
    IndexManager manager(6, 2, NodeIndex(0));
    Model model(manager);
#ifdef USE_UNARY
    const int vehicle_cost = model.RegisterUnaryTransitCallback(
        absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    model.SetFixedCostOfAllVehicles(100);
    model.AddConstantDimension(/*value = */ 1, /*capacity = */ 3,
                               /*fix_start_cumul_to_zero=*/true, "dim");
    model.AddDisjunction({1, 2, 3}, /*penalty=*/1000, /*max_cardinality=*/2);
    model.AddDisjunction({2}, -1);
    model.AddDisjunction({3}, kint64max);
    model.AddDisjunction({4, 5}, 1000);
    model.ActiveVar(5)->SetValues({1});
    std::vector<const Assignment*> solutions;
    const Assignment* const assignment =
        model.SolveWithParameters(parameters, &solutions);
    ASSERT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
    // CP-SAT and CP find the same solution so we only collect one solution.
    EXPECT_EQ(1, solutions.size());
    // vehicle #1: 0 -> 2 -> 3 -> 7
    // vehicle #2: 0 -> 5 -> 8
    EXPECT_EQ(200 + 2 + 3 + 5, assignment->ObjectiveValue());
  }
}

TEST(CPSATSearchTest, SolveAlternativesTSPWithPickupAndDelivery) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_use_generalized_cp_sat(BOOL_TRUE);
  parameters.set_use_cp(BOOL_FALSE);
  parameters.set_log_search(true);

  IndexManager manager(6, 1, NodeIndex(0));
  Model model(manager);
  // Forbid 1 -> 3 arc.
  const int vehicle_cost =
      model.RegisterTransitCallback([](int64_t from, int64_t to) {
        if (from == 1 && to == 3) {
          return kint64max;
        }
        return from;
      });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  const int kPenalty = 100;
  model.AddPickupAndDeliverySets(model.AddDisjunction({2, 1}, kPenalty),
                                 model.AddDisjunction({5, 3}, kPenalty));
  model.AddDisjunction({4}, /*penalty=*/1);
  std::vector<const Assignment*> solutions;
  const Assignment* const assignment =
      model.SolveWithParameters(parameters, &solutions);
  EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
  EXPECT_EQ(1, solutions.size());
  // cost += 1, 4 -> 4 (unperformed)
  // cost += 0 + 2 + 3, 0 -> 2 -> 3 -> 6
  EXPECT_EQ(1 + 0 + 2 + 3, assignment->ObjectiveValue());
  EXPECT_EQ(assignment->Value(model.NextVar(4)), 4);
  EXPECT_EQ(assignment->Value(model.NextVar(2)), 3);
}

TEST(CPSATSearchTest, SolveTSPWithWindowsModel) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    for (bool use_generalized_cp_sat : {false, true}) {
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      if (use_generalized_cp_sat) {
        parameters.set_use_generalized_cp_sat(BOOL_TRUE);
      } else {
        parameters.set_use_cp_sat(BOOL_TRUE);
      }
      parameters.set_use_cp(use_cp);
      parameters.set_log_search(true);
      IndexManager manager(4, 1, NodeIndex(0));
      Model model(manager);
#ifdef USE_UNARY
      const int vehicle_cost = model.RegisterUnaryTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
      const int vehicle_cost = model.RegisterTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
      model.AddConstantDimensionWithSlack(/*value = */ 1, /*capacity = */ 7,
                                          /* slack_max = */ 7,
                                          /*fix_start_cumul_to_zero=*/false,
                                          "dim");
      Dimension* dim = model.GetMutableDimension("dim");
      dim->CumulVar(0)->SetRange(0, 1);
      dim->CumulVar(1)->SetRange(5, 7);
      dim->CumulVar(2)->SetRange(3, 4);
      dim->CumulVar(3)->SetRange(4, 5);
      std::vector<const Assignment*> solutions;
      const Assignment* const assignment =
          model.SolveWithParameters(parameters, &solutions);
      EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
      // CP-SAT and CP find the same solution so we only collect one solution.
      EXPECT_EQ(1, solutions.size());
      EXPECT_EQ(1 + 2 + 3, assignment->ObjectiveValue());
      EXPECT_EQ(2, assignment->Value(model.NextVar(0)));
      EXPECT_EQ(3, assignment->Value(model.NextVar(2)));
      EXPECT_EQ(1, assignment->Value(model.NextVar(3)));
      EXPECT_EQ(4, assignment->Value(model.NextVar(1)));
    }
  }
}

TEST(CPSATSearchTest, SolveTSPWithWindowsModelDifferentSlacks) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_use_generalized_cp_sat(BOOL_TRUE);
  parameters.set_use_cp(BOOL_FALSE);
  parameters.set_log_search(true);
  IndexManager manager(3, 1, NodeIndex(0));
  Model model(manager);
#ifdef USE_UNARY
  const int vehicle_cost = model.RegisterUnaryTransitCallback(
      absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
  const int vehicle_cost = model.RegisterTransitCallback(
      absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  model.AddConstantDimensionWithSlack(/*value = */ 1, /*capacity = */ 4,
                                      /* slack_max = */ 1,
                                      /*fix_start_cumul_to_zero=*/false, "dim");
  Dimension* dim = model.GetMutableDimension("dim");
  dim->slacks()[0]->SetRange(0, 0);
  dim->slacks()[1]->SetRange(0, 1);
  dim->slacks()[2]->SetRange(0, 0);
  dim->CumulVar(0)->SetRange(0, 0);
  dim->CumulVar(1)->SetRange(1, 1);
  dim->CumulVar(2)->SetRange(3, 3);
  std::vector<const Assignment*> solutions;
  const Assignment* const assignment =
      model.SolveWithParameters(parameters, &solutions);
  EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
  EXPECT_EQ(1, solutions.size());
  EXPECT_EQ(1 + 2, assignment->ObjectiveValue());
}

TEST(CPSATSearchTest, SolveTSPWithPickupAndDelivery) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    for (bool use_generalized_cp_sat : {false, true}) {
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      if (use_generalized_cp_sat) {
        parameters.set_use_generalized_cp_sat(BOOL_TRUE);
      } else {
        parameters.set_use_cp_sat(BOOL_TRUE);
      }
      parameters.set_use_cp(use_cp);
      parameters.set_log_search(true);
      IndexManager manager(5, 1, NodeIndex(0));
      Model model(manager);
#ifdef USE_UNARY
      const int vehicle_cost = model.RegisterUnaryTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
      const int vehicle_cost = model.RegisterTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
      model.AddPickupAndDelivery(4, 1);
      model.AddPickupAndDelivery(2, 3);
      Solver* const solver = model.solver();
      solver->AddConstraint(
          solver->MakeEquality(model.VehicleVar(4), model.VehicleVar(1)));
      solver->AddConstraint(
          solver->MakeEquality(model.VehicleVar(2), model.VehicleVar(3)));
      model.AddConstantDimensionWithSlack(/*value = */ 1, /*capacity = */ 5,
                                          /* slack_max = */ 5,
                                          /*fix_start_cumul_to_zero=*/true,
                                          "dim");
      Dimension* dim = model.GetMutableDimension("dim");
      solver->AddConstraint(
          solver->MakeLessOrEqual(dim->CumulVar(4), dim->CumulVar(1)));
      solver->AddConstraint(
          solver->MakeLessOrEqual(dim->CumulVar(2), dim->CumulVar(3)));

      std::vector<const Assignment*> solutions;
      const Assignment* const assignment =
          model.SolveWithParameters(parameters, &solutions);
      EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
      // CP-SAT and CP find the same solution so we only collect one solution.
      EXPECT_EQ(1, solutions.size());
      EXPECT_EQ(1 + 2 + 3 + 4, assignment->ObjectiveValue());
      std::vector<int> ranks(5, 0);
      int rank = 0;
      int64_t node = 0;
      while (node != 5) {
        ranks[node] = rank;
        ++rank;
        node = assignment->Value(model.NextVar(node));
      }
      EXPECT_LT(ranks[4], ranks[1]);
      EXPECT_LT(ranks[2], ranks[3]);
    }
  }
}

// Exhibiting CP-SAT warm starting from a solution. CP limiting itself to a
// given initial solution with a solution limit.
TEST(CPSATSearchTest, SolveTSPModelWithSatImprovement) {
  for (bool use_generalized_cp_sat : {false, true}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    if (use_generalized_cp_sat) {
      parameters.set_use_generalized_cp_sat(BOOL_TRUE);
    } else {
      parameters.set_use_cp_sat(BOOL_TRUE);
    }
    parameters.set_use_cp(BOOL_TRUE);
    parameters.set_solution_limit(1);
    IndexManager manager(4, 1, NodeIndex(0));
    Model model(manager);
    const int vehicle_cost = model.RegisterTransitCallback(
        [](int from, int to) { return (from < to) ? 1 : 0; });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    Assignment initial_assignment(model.solver());
    for (int i = 0; i < 4; ++i) {
      initial_assignment.Add(model.NextVar(i))->SetValue(i + 1);
    }
    std::vector<const Assignment*> solutions;
    model.SolveFromAssignmentWithParameters(&initial_assignment, parameters,
                                            &solutions);
    if (use_generalized_cp_sat) {
      EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
    } else {
      EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_SUCCESS);
    }
    // CP-SAT and CP find different solutions.
    EXPECT_EQ(2, solutions.size());
    // First solution comes from the forced CP solution.
    EXPECT_EQ(4, solutions[0]->ObjectiveValue());
    // Second solutions is the optimal one found by CP-SAT.
    EXPECT_EQ(2, solutions[1]->ObjectiveValue());
  }
}

// Same as above with arcs with a value of kint64max to make sure hints given
// are properly setup on the partial model.
TEST(CPSATSearchTest, SolveTSPModelWithSatHintAndForbiddenCosts) {
  for (bool use_generalized_cp_sat : {false, true}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    if (use_generalized_cp_sat) {
      parameters.set_use_generalized_cp_sat(BOOL_TRUE);
    } else {
      parameters.set_use_cp_sat(BOOL_TRUE);
    }
    parameters.set_use_cp(BOOL_TRUE);
    // Setting collected solutions to a value greater than 1 to make sure the
    // solution is collected by CP (unnecessary after cl/284637113).
    parameters.set_number_of_solutions_to_collect(2);
    IndexManager manager(2, 1, NodeIndex(0));
    Model model(manager);
    const int vehicle_cost = model.RegisterTransitCallback(
        [](int from, int to) { return (from != to) ? kint64max : 0; });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    const Assignment* const solution = model.SolveWithParameters(parameters);
    EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
    EXPECT_FALSE(solution == nullptr);
    EXPECT_EQ(kint64max, solution->ObjectiveValue());
  }
}

// Solve a model with multiple unconstrained vehicles leading to actually
// solving a TSP.
TEST(CPSATSearchTest, SolveUnconstrainedVRPModel) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    for (bool use_generalized_cp_sat : {false, true}) {
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      if (use_generalized_cp_sat) {
        parameters.set_use_generalized_cp_sat(BOOL_TRUE);
      } else {
        parameters.set_use_cp_sat(BOOL_TRUE);
      }
      parameters.set_use_cp(use_cp);
      IndexManager manager(4, 2, NodeIndex(0));
      Model model(manager);
#ifdef USE_UNARY
      const int vehicle_cost = model.RegisterUnaryTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
      const int vehicle_cost = model.RegisterTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
      std::vector<const Assignment*> solutions;
      const Assignment* const assignment =
          model.SolveWithParameters(parameters, &solutions);
      EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
      // CP-SAT and CP find the same solution so we only collect one solution.
      EXPECT_EQ(1, solutions.size());
      EXPECT_EQ(1 + 2 + 3, assignment->ObjectiveValue());
    }
  }
}

// Solve a model with multiple mostly unconstrained vehicles but with
// constrained node vehicle variables. As of 2023-08 this is not supported in
// pure non-generalized cp-sat mode and should fail.
TEST(CPSATSearchTest, SolveConstrainedVehicleVRPModel) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    for (bool use_generalized_cp_sat : {false, true}) {
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      if (use_generalized_cp_sat) {
        parameters.set_use_generalized_cp_sat(BOOL_TRUE);
      } else {
        // Even if the non-generalized cp-sat solver is specified, we will
        // automatically fallback to the generalized model since vehicles
        // are heterogeneous.
        parameters.set_use_cp_sat(BOOL_TRUE);
      }
      parameters.set_use_cp(use_cp);
      IndexManager manager(4, 2, NodeIndex(0));
      Model model(manager);
#ifdef USE_UNARY
      const int vehicle_cost = model.RegisterUnaryTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
      const int vehicle_cost = model.RegisterTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
      for (int i = 0; i < 4; ++i) {
        if (model.IsStart(i) || model.IsEnd(i)) continue;
        model.VehicleVar(i)->RemoveValue(i % 2);
      }
      std::vector<const Assignment*> solutions;
      const Assignment* const assignment =
          model.SolveWithParameters(parameters, &solutions);
      EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
      EXPECT_EQ(1, solutions.size());
      EXPECT_EQ(1 + 2 + 3, assignment->ObjectiveValue());
      EXPECT_EQ(model.search_stats().num_cp_sat_calls_in_routing, 0);
      EXPECT_EQ(model.search_stats().num_generalized_cp_sat_calls_in_routing,
                1);
    }
  }
}

// Solve a model with a node capacity forcing the usage of 2 vehicles.
TEST(CPSATSearchTest, SolveCapacitatedVRPModel) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    for (bool use_generalized_cp_sat : {false, true}) {
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      if (use_generalized_cp_sat) {
        parameters.set_use_generalized_cp_sat(BOOL_TRUE);
      } else {
        parameters.set_use_cp_sat(BOOL_TRUE);
      }
      parameters.set_use_cp(use_cp);
      IndexManager manager(4, 2, NodeIndex(0));
      Model model(manager);
#ifdef USE_UNARY
      const int vehicle_cost = model.RegisterUnaryTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
      const int vehicle_cost = model.RegisterTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
      model.SetFixedCostOfAllVehicles(100);
      model.AddConstantDimensionWithSlack(/*value = */ 1, /*capacity = */ 3,
                                          /* slack_max = */ 3,
                                          /*fix_start_cumul_to_zero=*/true,
                                          "dim");
      std::vector<const Assignment*> solutions;
      const Assignment* const assignment =
          model.SolveWithParameters(parameters, &solutions);
      EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
      // CP-SAT and CP find the same solution so we only collect one solution.
      EXPECT_EQ(1, solutions.size());
      EXPECT_EQ(200 + 1 + 2 + 3, assignment->ObjectiveValue());
    }
  }
}

// Solve a model with time windows and vehicle span limits forcing the usage of
// 2 vehicles.
TEST(CPSATSearchTest, SolveLimitedVRPWithWindowsModel) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_use_generalized_cp_sat(BOOL_TRUE);
    parameters.set_use_cp(use_cp);
    IndexManager manager(4, 2, NodeIndex(0));
    Model model(manager);
#ifdef USE_UNARY
    const int vehicle_cost = model.RegisterUnaryTransitCallback(
        absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    model.SetFixedCostOfAllVehicles(100);
    model.AddConstantDimensionWithSlack(/*value = */ 1, /*capacity = */ 24,
                                        /* slack_max = */ 0,
                                        /*fix_start_cumul_to_zero=*/false,
                                        "dim");
    Dimension* dim = model.GetMutableDimension("dim");
    dim->SetSpanUpperBoundForVehicle(8, 0);
    dim->SetSpanUpperBoundForVehicle(8, 1);
    dim->CumulVar(0)->SetRange(6, 15);
    dim->CumulVar(1)->SetRange(7, 7);
    dim->CumulVar(2)->SetRange(8, 8);
    dim->CumulVar(3)->SetRange(23, 23);
    std::vector<const Assignment*> solutions;
    const Assignment* const assignment =
        model.SolveWithParameters(parameters, &solutions);
    EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
    // CP-SAT and CP find the same solution so we only collect one solution.
    EXPECT_EQ(1, solutions.size());
    EXPECT_EQ(200 + 1 + 2 + 3, assignment->ObjectiveValue());
  }
}

// Solve a model with a soft upper bound cost, choosing unperformed visit.
TEST(CPSATSearchTest, SolveUnperformingVRPModelWithSoftUpperBoundLimit) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_use_generalized_cp_sat(BOOL_TRUE);
    parameters.set_use_cp(use_cp);
    IndexManager manager(4, 1, NodeIndex(0));
    Model model(manager);
#ifdef USE_UNARY
    const int vehicle_cost = model.RegisterUnaryTransitCallback(
        absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    model.SetFixedCostOfAllVehicles(100);
    model.AddConstantDimensionWithSlack(/*value = */ 1, /*capacity = */ 24,
                                        /* slack_max = */ 24,
                                        /*fix_start_cumul_to_zero=*/false,
                                        "dim");
    Dimension* dim = model.GetMutableDimension("dim");
    dim->SetSoftSpanUpperBoundForVehicle(/* bound_cost = */ {8, 1},
                                         /* vehicle = */ 0);
    dim->CumulVar(0)->SetRange(6, 15);
    dim->CumulVar(1)->SetRange(7, 7);
    dim->CumulVar(2)->SetRange(8, 8);
    dim->CumulVar(3)->SetRange(23, 23);
    model.AddDisjunction({3}, 10);
    std::vector<const Assignment*> solutions;
    const Assignment* const assignment =
        model.SolveWithParameters(parameters, &solutions);
    EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
    // CP-SAT and CP find the same solution so we only collect one solution.
    EXPECT_EQ(1, solutions.size());
    EXPECT_EQ(100 + 1 + 2 + 10, assignment->ObjectiveValue());
  }
}

// Solve a model with a soft upper bound cost, choosing to pay soft span cost.
TEST(CPSATSearchTest, SolveVRPModelWithSoftUpperBoundLimit) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_use_generalized_cp_sat(BOOL_TRUE);
    parameters.set_use_cp(use_cp);
    IndexManager manager(4, 1, NodeIndex(0));
    Model model(manager);
#ifdef USE_UNARY
    const int vehicle_cost = model.RegisterUnaryTransitCallback(
        absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    model.SetFixedCostOfAllVehicles(100);
    model.AddConstantDimensionWithSlack(/*value = */ 1, /*capacity = */ 24,
                                        /* slack_max = */ 24,
                                        /*fix_start_cumul_to_zero=*/false,
                                        "dim");
    Dimension* dim = model.GetMutableDimension("dim");
    dim->SetSoftSpanUpperBoundForVehicle(/* bound_cost = */ {8, 2},
                                         /* vehicle = */ 0);
    dim->CumulVar(0)->SetRange(6, 15);
    dim->CumulVar(1)->SetRange(7, 7);
    dim->CumulVar(2)->SetRange(8, 8);
    dim->CumulVar(3)->SetRange(23, 23);
    model.AddDisjunction({3}, 100);
    std::vector<const Assignment*> solutions;
    const Assignment* const assignment =
        model.SolveWithParameters(parameters, &solutions);
    EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
    // CP-SAT and CP find the same solution so we only collect one solution.
    EXPECT_EQ(1, solutions.size());
    EXPECT_EQ(100 + 1 + 2 + 3 + 20, assignment->ObjectiveValue());
  }
}

// Solve a model with a node capacity and heterogeneous fleet.
TEST(CPSATSearchTest, SolveHeterogeneousCapacitatedVRPModel) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_use_generalized_cp_sat(BOOL_TRUE);
    parameters.set_use_cp(use_cp);
    const std::vector<int64_t> kVehicleCosts{1, 2};
    const std::vector<int64_t> kVehicleCapacities{2, 3};
    IndexManager manager(4, kVehicleCosts.size(), NodeIndex(0));
    Model model(manager);
    for (int vehicle = 0; vehicle < kVehicleCosts.size(); ++vehicle) {
#ifdef USE_UNARY
      const int vehicle_cost = model.RegisterUnaryTransitCallback(
          [&kVehicleCosts, vehicle](int64_t) {
            return kVehicleCosts[vehicle];
          });
#else
      const int vehicle_cost = model.RegisterTransitCallback(
          [&kVehicleCosts, vehicle](int64_t, int64_t) {
            return kVehicleCosts[vehicle];
          });
#endif
      model.SetArcCostEvaluatorOfVehicle(vehicle_cost, vehicle);
      model.SetFixedCostOfVehicle(kVehicleCosts[vehicle] * 100, vehicle);
    }
    model.AddDimensionWithVehicleCapacity(
        /*evaluator_index = */ 1, /* slack_max = */ 3,
        /*vehicle_capacities = */ kVehicleCapacities,
        /*fix_start_cumul_to_zero=*/true, "dim");
    std::vector<const Assignment*> solutions;
    const Assignment* const assignment =
        model.SolveWithParameters(parameters, &solutions);
    EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
    // CP-SAT and CP find the same solution so we only collect one solution.
    EXPECT_EQ(1, solutions.size());
    EXPECT_EQ(100 + 1 + 1 + 200 + 2 + 2 + 2, assignment->ObjectiveValue());
  }
}

TEST(CPSATSearchTest, SolveVRPModelWithForbiddenArc) {
  for (bool use_generalized_cp_sat : {false, true}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    if (use_generalized_cp_sat) {
      parameters.set_use_generalized_cp_sat(BOOL_TRUE);
    } else {
      parameters.set_use_cp_sat(BOOL_TRUE);
    }
    IndexManager manager(4, 2, NodeIndex(0));
    Model model(manager);
    // Forbid 1 -> 2
    const int vehicle_cost =
        model.RegisterTransitCallback([](int64_t from, int64_t to) {
          if (from == 1 && to == 2) return kint64max;
          return int64_t{1};
        });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    std::vector<const Assignment*> solutions;
    const Assignment* const assignment =
        model.SolveWithParameters(parameters, &solutions);
    EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
    EXPECT_EQ(1, solutions.size());
    EXPECT_EQ(4, assignment->ObjectiveValue());
  }
}

TEST(CPSATSearchTest, SolveVRPModelWithForbiddenArcFromDepot) {
  for (bool use_generalized_cp_sat : {false, true}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    if (use_generalized_cp_sat) {
      parameters.set_use_generalized_cp_sat(BOOL_TRUE);
    } else {
      parameters.set_use_cp_sat(BOOL_TRUE);
    }
    IndexManager manager(4, 2, NodeIndex(0));
    Model model(manager);
    // Forbid depot - > 1
    const int vehicle_cost =
        model.RegisterTransitCallback([](int64_t from, int64_t to) {
          if (from == 0 && to == 1) return kint64max;
          if (from == 4 && to == 1) return kint64max;
          return int64_t{1};
        });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    std::vector<const Assignment*> solutions;
    const Assignment* const assignment =
        model.SolveWithParameters(parameters, &solutions);
    EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
    EXPECT_EQ(1, solutions.size());
    EXPECT_EQ(4, assignment->ObjectiveValue());
  }
}

TEST(CPSATSearchTest, SolveVRPModelWithForbiddenArcToDepot) {
  for (bool use_generalized_cp_sat : {false, true}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    if (use_generalized_cp_sat) {
      parameters.set_use_generalized_cp_sat(BOOL_TRUE);
    } else {
      parameters.set_use_cp_sat(BOOL_TRUE);
    }
    IndexManager manager(4, 2, NodeIndex(0));
    Model model(manager);
    // Forbid 1 -> depot
    const int vehicle_cost =
        model.RegisterTransitCallback([](int64_t from, int64_t to) {
          if (from == 1 && to == 0) return kint64max;
          if (from == 1 && to > 4) return kint64max;
          return int64_t{1};
        });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    std::vector<const Assignment*> solutions;
    const Assignment* const assignment =
        model.SolveWithParameters(parameters, &solutions);
    EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
    EXPECT_EQ(1, solutions.size());
    EXPECT_EQ(4, assignment->ObjectiveValue());
  }
}

// Same as above but exercises the routes constraint with no capacities.
TEST(CPSATSearchTest, SolveNoUnaryCapacitiesVRPModel) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    for (bool use_generalized_cp_sat : {false, true}) {
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      if (use_generalized_cp_sat) {
        parameters.set_use_generalized_cp_sat(BOOL_TRUE);
      } else {
        parameters.set_use_cp_sat(BOOL_TRUE);
      }
      parameters.set_use_cp(use_cp);
      IndexManager manager(4, 2, NodeIndex(0));
      Model model(manager);
#ifdef USE_UNARY
      const int vehicle_cost = model.RegisterUnaryTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
      const int vehicle_cost = model.RegisterTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
      model.SetFixedCostOfAllVehicles(100);
      model.AddDimension(
          model.RegisterTransitCallback([](int, int) { return 1; }),
          /*slack_max = */ 3, /*capacity = */ 3,
          /*fix_start_cumul_to_zero = */ true, "dim");
      std::vector<const Assignment*> solutions;
      const Assignment* const assignment =
          model.SolveWithParameters(parameters, &solutions);
      EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
      // CP-SAT and CP find the same solution so we only collect one solution.
      EXPECT_EQ(1, solutions.size());
      EXPECT_EQ(200 + 1 + 2 + 3, assignment->ObjectiveValue());
    }
  }
}

// Same as SolveCapacitatedVRPModel but adding windows.
TEST(CPSATSearchTest, SolveCapacitatedVRPWithWindowsModel) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    for (bool use_generalized_cp_sat : {false, true}) {
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      if (use_generalized_cp_sat) {
        parameters.set_use_generalized_cp_sat(BOOL_TRUE);
      } else {
        parameters.set_use_cp_sat(BOOL_TRUE);
      }
      parameters.set_use_cp(use_cp);
      parameters.set_log_search(true);
      IndexManager manager(4, 2, NodeIndex(0));
      Model model(manager);
#ifdef USE_UNARY
      const int vehicle_cost = model.RegisterUnaryTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
      const int vehicle_cost = model.RegisterTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
      model.SetFixedCostOfAllVehicles(100);
      model.AddConstantDimensionWithSlack(/*value = */ 1, /*capacity = */ 3,
                                          /*slack_max = */ 3,
                                          /*fix_start_cumul_to_zero = */ true,
                                          "dim");
      model.AddDimension(
          model.RegisterTransitCallback([](int, int) { return 1; }),
          /*slack_max = */ 7, /*capacity = */ 7,
          /*fix_start_cumul_to_zero=*/false, "time");
      Dimension* time = model.GetMutableDimension("time");
      time->CumulVar(0)->SetRange(0, 1);
      time->CumulVar(1)->SetRange(5, 7);
      time->CumulVar(2)->SetRange(3, 4);
      time->CumulVar(3)->SetRange(4, 5);
      time->CumulVar(4)->SetRange(0, 1);
      std::vector<const Assignment*> solutions;
      const Assignment* const assignment =
          model.SolveWithParameters(parameters, &solutions);
      EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
      // CP-SAT and CP find the same solution so we only collect one solution.
      EXPECT_EQ(1, solutions.size());
      EXPECT_EQ(200 + 1 + 2 + 3, assignment->ObjectiveValue());
    }
  }
}

TEST(CPSATSearchTest, SolveCapacitatedTSPModelWithWindowsAndSlackCost) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_use_generalized_cp_sat(BOOL_TRUE);
    parameters.set_use_cp(use_cp);
    parameters.set_log_search(true);
    IndexManager manager(3, 1, NodeIndex(0));
    Model model(manager);
    const int vehicle_cost = model.RegisterTransitCallback(
        [](int from, int to) { return (from < to) ? 1 : 0; });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    model.SetFixedCostOfAllVehicles(0);
    model.AddDimension(
        model.RegisterTransitCallback([](int, int) { return 1; }),
        /*slack_max = */ 7, /*capacity = */ 7,
        /*fix_start_cumul_to_zero=*/true, "time");
    Dimension* time = model.GetMutableDimension("time");
    time->CumulVar(0)->SetRange(0, 0);
    time->CumulVar(1)->SetRange(1, 6);
    time->CumulVar(2)->SetRange(5, 5);
    time->SetSpanCostCoefficientForAllVehicles(100);
    std::vector<const Assignment*> solutions;
    const Assignment* const assignment =
        model.SolveWithParameters(parameters, &solutions);
    EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
    // CP-SAT and CP find the same solution so we only collect one solution.
    EXPECT_EQ(1, solutions.size());
    // 0 -> 1 -> 2 -> 3
    EXPECT_EQ(600 + 3, assignment->ObjectiveValue());
  }
}

TEST(CPSATSearchTest, SolveCapacitatedVRPModelWithWindowsAndSlackCost) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_use_generalized_cp_sat(BOOL_TRUE);
    parameters.set_use_cp(use_cp);
    parameters.set_log_search(true);
    IndexManager manager(3, 3, NodeIndex(0));
    Model model(manager);
    const int vehicle_cost = model.RegisterTransitCallback(
        [](int /*from*/, int /*to*/) { return 1; });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    model.AddDimension(
        model.RegisterTransitCallback([](int, int) { return 1; }),
        /*slack_max = */ 7, /*capacity = */ 7,
        /*fix_start_cumul_to_zero=*/true, "time");
    Dimension* time = model.GetMutableDimension("time");
    time->CumulVar(0)->SetRange(0, 0);
    time->CumulVar(1)->SetRange(1, 6);
    time->CumulVar(2)->SetRange(5, 5);
    time->SetSpanCostCoefficientForAllVehicles(100);
    std::vector<const Assignment*> solutions;
    const Assignment* const assignment =
        model.SolveWithParameters(parameters, &solutions);
    EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
    // CP-SAT and CP find the same solution so we only collect one solution.
    EXPECT_EQ(1, solutions.size());
    // 0 -> 1 -> 2 -> End
    EXPECT_EQ(600 + 1 + 1 + 1, assignment->ObjectiveValue());
  }
}

TEST(CPSATSearchTest,
     SolveCapacitatedVRPModelWithWindowsSlackCostsAndVehicleClasses) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_use_generalized_cp_sat(BOOL_TRUE);
    parameters.set_use_cp(use_cp);
    parameters.set_log_search(true);
    const int kNumVehicles = 3;
    IndexManager manager(3, kNumVehicles, NodeIndex(0));
    Model model(manager);
    const int vehicle_cost = model.RegisterTransitCallback(
        [](int /*from*/, int /*to*/) { return 1; });
    for (int i = 0; i < kNumVehicles; i++) {
      if (i == kNumVehicles - 1) {
        const int expensive_vehicle_cost = model.RegisterTransitCallback(
            [](int /*from*/, int /*to*/) { return 200; });
        model.SetArcCostEvaluatorOfVehicle(expensive_vehicle_cost, i);
        continue;
      }
      model.SetArcCostEvaluatorOfVehicle(vehicle_cost, i);
    }
    model.AddDimension(
        model.RegisterTransitCallback([](int, int) { return 1; }),
        /*slack_max = */ 7, /*capacity = */ 7,
        /*fix_start_cumul_to_zero=*/true, "time");
    Dimension* time = model.GetMutableDimension("time");
    time->CumulVar(0)->SetRange(0, 0);
    time->CumulVar(1)->SetRange(1, 6);
    time->CumulVar(2)->SetRange(5, 5);
    for (int vehicle = 0; vehicle < kNumVehicles; vehicle++) {
      time->SetSpanCostCoefficientForVehicle((kNumVehicles - vehicle) * 100,
                                             vehicle);
    }
    std::vector<const Assignment*> solutions;
    const Assignment* const assignment =
        model.SolveWithParameters(parameters, &solutions);
    EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
    // CP-SAT and CP find the same solution so we only collect one solution.
    EXPECT_EQ(1, solutions.size());
    // 0 -> 1 -> 2 -> End
    EXPECT_EQ(600 + 200 * 3, assignment->ObjectiveValue());
    EXPECT_EQ(3, model.GetVehicleClassesCount());
  }
}

TEST(CPSATSearchTest,
     SolveCapacitatedVRPModelWithWindowsDifferentTransitsAndVehicleClasses) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_use_generalized_cp_sat(BOOL_TRUE);
    parameters.set_use_cp(use_cp);
    parameters.set_log_search(true);
    const int kNumVehicles = 9;
    IndexManager manager(3, kNumVehicles, NodeIndex(0));
    Model model(manager);
    const int vehicle_cost = model.RegisterTransitCallback(
        [](int /*from*/, int /*to*/) { return 1; });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    std::vector<int> vehicle_transits;
    int vehicle_transit;
    for (int vehicle = 0; vehicle < kNumVehicles; vehicle++) {
      if (vehicle % 3 == 0) {
        vehicle_transit = model.RegisterTransitCallback([vehicle](int, int) {
          return 1 + (kNumVehicles - vehicle - 1) / 3;
        });
      }
      vehicle_transits.push_back(vehicle_transit);
    }
    model.SetFixedCostOfAllVehicles(100);
    model.AddDimensionWithVehicleTransits(vehicle_transits,
                                          /*slack_max = */ 0, /*capacity = */ 7,
                                          /*fix_start_cumul_to_zero=*/true,
                                          "time");
    Dimension* time = model.GetMutableDimension("time");
    time->CumulVar(0)->SetRange(0, 0);
    time->CumulVar(1)->SetRange(1, 1);
    time->CumulVar(2)->SetRange(2, 2);
    std::vector<const Assignment*> solutions;
    const Assignment* const assignment =
        model.SolveWithParameters(parameters, &solutions);
    EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
    // CP-SAT and CP find the same solution so we only collect one solution.
    EXPECT_EQ(1, solutions.size());
    // 0 -> 1 -> 2 -> End
    EXPECT_EQ(100 + 1 + 1 + 1, assignment->ObjectiveValue());
    EXPECT_EQ(3, model.GetVehicleClassesCount());
  }
}

TEST(CPSATSearchTest,
     SolveCapacitatedVRPModelWithWindowsSlackCostAndDisjunction) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_use_generalized_cp_sat(BOOL_TRUE);
    parameters.set_use_cp(use_cp);
    parameters.set_log_search(true);
    IndexManager manager(4, 2, NodeIndex(0));
    Model model(manager);
    const int vehicle_cost = model.RegisterTransitCallback(
        [](int from, int to) { return (from < to) ? 1 : 0; });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    model.SetFixedCostOfAllVehicles(1000);
    model.AddDimension(
        model.RegisterTransitCallback([](int, int) { return 1; }),
        /*slack_max = */ 7, /*capacity = */ 14,
        /*fix_start_cumul_to_zero=*/true, "time");
    Dimension* time = model.GetMutableDimension("time");
    time->CumulVar(0)->SetRange(0, 0);
    time->CumulVar(1)->SetRange(1, 6);
    time->CumulVar(2)->SetRange(5, 5);
    time->CumulVar(3)->SetRange(10, 10);
    model.AddDisjunction({3}, 2000);
    time->SetSpanCostCoefficientForVehicle(/* coefficient = */ 1000,
                                           /* vehicle = */ 0);
    time->SetSpanCostCoefficientForVehicle(/* coefficient = */ 100,
                                           /* vehicle = */ 1);
    std::vector<const Assignment*> solutions;
    const Assignment* const assignment =
        model.SolveWithParameters(parameters, &solutions);
    EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
    // CP-SAT and CP find the same solution so we only collect one solution.
    EXPECT_EQ(1, solutions.size());
    // 0 -> 1 -> 2 -> 3 -> 6
    EXPECT_EQ(1000 + 1100 + 2, assignment->ObjectiveValue());
  }
}

TEST(CPSATSearchTest, SolveCapacitatedVRPWithPickupAndDelivery) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    for (bool use_generalized_cp_sat : {false, true}) {
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      if (use_generalized_cp_sat) {
        parameters.set_use_generalized_cp_sat(BOOL_TRUE);
      } else {
        parameters.set_use_cp_sat(BOOL_TRUE);
      }
      parameters.set_use_cp(use_cp);
      parameters.set_log_search(true);
      IndexManager manager(5, 2, NodeIndex(0));
      Model model(manager);
#ifdef USE_UNARY
      const int vehicle_cost = model.RegisterUnaryTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
      const int vehicle_cost = model.RegisterTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
      model.SetFixedCostOfAllVehicles(100);
      model.AddConstantDimensionWithSlack(/*value = */ 1, /*capacity = */ 3,
                                          /* slack_max = */ 3,
                                          /*fix_start_cumul_to_zero=*/true,
                                          "capacity");
      model.AddPickupAndDelivery(4, 1);
      model.AddPickupAndDelivery(2, 3);
      Solver* const solver = model.solver();
      solver->AddConstraint(
          solver->MakeEquality(model.VehicleVar(4), model.VehicleVar(1)));
      solver->AddConstraint(
          solver->MakeEquality(model.VehicleVar(2), model.VehicleVar(3)));
      model.AddConstantDimensionWithSlack(/*value = */ 1, /*capacity = */ 5,
                                          /* slack_max = */ 5,
                                          /*fix_start_cumul_to_zero=*/true,
                                          "dim");
      Dimension* dim = model.GetMutableDimension("dim");
      solver->AddConstraint(
          solver->MakeLessOrEqual(dim->CumulVar(4), dim->CumulVar(1)));
      solver->AddConstraint(
          solver->MakeLessOrEqual(dim->CumulVar(2), dim->CumulVar(3)));

      std::vector<const Assignment*> solutions;
      const Assignment* const assignment =
          model.SolveWithParameters(parameters, &solutions);
      EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
      // CP-SAT and CP find the same solution so we only collect one solution.
      EXPECT_EQ(1, solutions.size());
      EXPECT_EQ(200 + 1 + 2 + 3 + 4, assignment->ObjectiveValue());
      std::vector<int> ranks(6, 0);
      for (int i = 0; i < 2; ++i) {
        int rank = 0;
        int64_t node = model.Start(i);
        while (node != model.End(i)) {
          ranks[node] = rank;
          ++rank;
          node = assignment->Value(model.NextVar(node));
        }
      }
      EXPECT_LT(ranks[4], ranks[1]);
      EXPECT_LT(ranks[2], ranks[3]);
    }
  }
}

TEST(CPSATSearchTest, SolveHeterogeneousCapacitatedVRPWithPickupAndDelivery) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_use_generalized_cp_sat(BOOL_TRUE);
    parameters.set_use_cp(use_cp);
    parameters.set_log_search(true);

    const std::vector<int64_t> kVehicleCosts{1, 100};
    const std::vector<int64_t> kVehicleCapacities{1, 2};
    IndexManager manager(5, 2, NodeIndex(0));
    Model model(manager);
    for (int vehicle = 0; vehicle < kVehicleCosts.size(); vehicle++) {
      model.SetFixedCostOfVehicle(kVehicleCosts[vehicle], vehicle);
    }
    // Forbid 1->2, 2->1, 3->4, 4->3
    const int vehicle_cost =
        model.RegisterTransitCallback([](int64_t from, int64_t to) {
          if (from == 1 && to == 2) {
            return kint64max;
          }
          if (from == 2 && to == 1) {
            return kint64max;
          }
          if (from == 3 && to == 4) {
            return kint64max;
          }
          if (from == 4 && to == 3) {
            return kint64max;
          }
          return int64_t{1};
        });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    model.AddDimensionWithVehicleCapacity(
        model.RegisterUnaryTransitCallback([](int node) {
          if (node == 2 || node == 4) return 1;
          if (node == 1 || node == 3) return -1;
          return 0;
        }),
        /* slack_max = */ 5,
        /* vehicle_capacities = */ kVehicleCapacities,
        /* fix_start_cumul_to_zero= */ true, "capacity");
    model.AddPickupAndDelivery(4, 1);
    model.AddPickupAndDelivery(2, 3);
    Solver* const solver = model.solver();
    solver->AddConstraint(
        solver->MakeEquality(model.VehicleVar(4), model.VehicleVar(1)));
    solver->AddConstraint(
        solver->MakeEquality(model.VehicleVar(2), model.VehicleVar(3)));

    std::vector<const Assignment*> solutions;
    const Assignment* const assignment =
        model.SolveWithParameters(parameters, &solutions);
    EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
    // CP-SAT and CP find the same solution so we only collect one solution.
    EXPECT_EQ(1, solutions.size());
    // The capacity constraints on visits prevents the optimal solution to use
    // the first vehicle only. Second vehicle of fixed cost 100 is used.
    EXPECT_EQ(100 + 5, assignment->ObjectiveValue());
    std::vector<int> ranks(6, 0);
    for (int i = 0; i < 2; ++i) {
      int rank = 0;
      int64_t node = model.Start(i);
      while (node != model.End(i)) {
        ranks[node] = rank;
        ++rank;
        node = assignment->Value(model.NextVar(node));
      }
    }
    EXPECT_LT(ranks[4], ranks[1]);
    EXPECT_LT(ranks[2], ranks[3]);
  }
}

// Solve a model with max active vehicles forcing to use fewer vehicles at a
// higher cost. The model has 3 nodes to visit and 3 vehicles. The cost is set
// up such that routes with a single node are free, so the optimal would be to
// have a node per vehicle. Tests with CP, CP-SAT and CP+CP-SAT.
TEST(RoutingSearchTest, SolveCapacitatedVRPModelWithMaxActiveVehicles) {
  for (int max_active_vehicles : {3, 2, 1}) {
    for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
      for (OptionalBoolean use_cp_sat : {BOOL_FALSE, BOOL_TRUE}) {
        for (OptionalBoolean use_generalized_cp_sat : {BOOL_FALSE, BOOL_TRUE}) {
          if (use_cp == BOOL_FALSE && use_cp_sat == BOOL_FALSE &&
              use_generalized_cp_sat == BOOL_FALSE) {
            continue;
          }
          RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
          parameters.set_use_cp_sat(use_cp_sat);
          parameters.set_use_generalized_cp_sat(use_generalized_cp_sat);
          parameters.set_use_cp(use_cp);
          IndexManager manager(4, 3, NodeIndex(0));
          Model model(manager);
          const int vehicle_cost =
              model.RegisterTransitCallback([&model](int64_t from, int64_t to) {
                if (model.IsStart(from) || model.IsEnd(to)) return 0;
                return 10;
              });
          model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
          model.SetMaximumNumberOfActiveVehicles(max_active_vehicles);
          std::vector<const Assignment*> solutions;
          const Assignment* const assignment =
              model.SolveWithParameters(parameters, &solutions);
          // If max_active_vehicles is 3, then all nodes can be on a separate
          // vehicle at no cost (aka optimal).
          if (use_cp_sat == BOOL_TRUE || use_generalized_cp_sat == BOOL_TRUE ||
              max_active_vehicles == 3) {
            EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
          } else {
            EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_SUCCESS);
          }
          // CP-SAT and CP find the same solution so we only collect one
          // solution.
          EXPECT_EQ(1, solutions.size());
          EXPECT_EQ((3 - max_active_vehicles) * 10,
                    assignment->ObjectiveValue());
        }
      }
    }
  }
}

// Solve a model with unbound cumul variables.
TEST(RoutingSearchTest, SolveVRPModelWithUnboundCumuls) {
  for (OptionalBoolean use_cp : {BOOL_FALSE, BOOL_TRUE}) {
    for (OptionalBoolean use_cp_sat : {BOOL_FALSE, BOOL_TRUE}) {
      for (OptionalBoolean use_generalized_cp_sat : {BOOL_FALSE, BOOL_TRUE}) {
        if (use_cp == BOOL_FALSE && use_cp_sat == BOOL_FALSE &&
            use_generalized_cp_sat == BOOL_FALSE) {
          continue;
        }
        RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
        parameters.set_use_cp_sat(use_cp_sat);
        parameters.set_use_generalized_cp_sat(use_generalized_cp_sat);
        parameters.set_use_cp(use_cp);
        IndexManager manager(/*num_nodes=*/4, /*num_vehicles=*/1, NodeIndex(0));
        Model model(manager);
        const int vehicle_cost =
            model.RegisterTransitCallback([&model](int64_t from, int64_t to) {
              if (model.IsStart(from) || model.IsEnd(to)) return 0;
              return 10;
            });
        model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
        model.AddConstantDimensionWithSlack(
            /*value = */ 1, /*capacity = */ kint64max,
            /*slack_max=*/kint64max,
            /*fix_start_cumul_to_zero=*/false, "capacity");
        std::vector<const Assignment*> solutions;
        const Assignment* const assignment =
            model.SolveWithParameters(parameters, &solutions);
        if (use_cp_sat == BOOL_TRUE || use_generalized_cp_sat == BOOL_TRUE) {
          EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_OPTIMAL);
        } else {
          EXPECT_EQ(model.status(), RoutingSearchStatus::ROUTING_SUCCESS);
        }
        EXPECT_EQ(1, solutions.size());
        EXPECT_EQ(20, assignment->ObjectiveValue())
            << assignment->DebugString();
      }
    }
  }
}

// Checking that the SAT solver is run with the SAT parameters passed through
// the routing search parameters, using presolve as a proxy to check changing
// parameters has an effect.
TEST(RoutingSearchTest, CheckSatParameters) {
  absl::SetFlag(&FLAGS_cp_model_dump_prefix, ::testing::TempDir() + "/");
  absl::SetFlag(&FLAGS_cp_model_dump_models, true);
  for (bool use_generalized_cp_sat : {false, true}) {
    const auto get_dumped_presolved_model =
        [](const RoutingSearchParameters& parameters) -> sat::CpModelProto {
      IndexManager manager(4, 1, NodeIndex(0));
      Model model(manager);
      const int vehicle_cost = model.RegisterTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
      std::vector<const Assignment*> solutions;
      model.SolveWithParameters(parameters, &solutions);
      return ReadFileToProto<sat::CpModelProto>(
                 file::JoinPath(::testing::TempDir(), "presolved_model.pb.txt"))
          .value();
    };
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    if (use_generalized_cp_sat) {
      parameters.set_use_generalized_cp_sat(BOOL_TRUE);
    } else {
      parameters.set_use_cp_sat(BOOL_TRUE);
    }
    parameters.set_use_cp(BOOL_FALSE);
    sat::CpModelProto cp_model_with_presolve =
        get_dumped_presolved_model(parameters);
    parameters.mutable_sat_parameters()->set_cp_model_presolve(false);
    sat::CpModelProto cp_model_no_presolve =
        get_dumped_presolved_model(parameters);
    EXPECT_THAT(cp_model_no_presolve,
                testing::Not(testing::EqualsProto(cp_model_with_presolve)));
  }
}

TEST(RoutingSearchTest, CheckInterruptSolve) {
  for (bool interrupt_cp_sat : {false, true}) {
    for (bool use_generalized_cp_sat : {false, true}) {
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      if (use_generalized_cp_sat) {
        parameters.set_use_generalized_cp_sat(BOOL_TRUE);
      } else {
        parameters.set_use_cp_sat(BOOL_TRUE);
      }
      parameters.set_use_cp(BOOL_FALSE);
      IndexManager manager(4, 1, NodeIndex(0));
      Model model(manager);
#ifdef USE_UNARY
      const int vehicle_cost = model.RegisterUnaryTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#else   // USE_UNARY
      const int vehicle_cost = model.RegisterTransitCallback(
          absl::bind_front(ReturnFirstNodeValueFromVariableIndex, manager));
#endif  // USE_UNARY
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
      model.CloseModelWithParameters(parameters);
      // Stop search right away.
      *model.GetMutableCPSatInterrupt() = interrupt_cp_sat;
      Assignment sat_solution(model.solver());
      const bool ok =
          SolveModelWithSat(&model, nullptr, parameters,
                            /*initial_solution=*/nullptr, &sat_solution);
      EXPECT_EQ(ok, !interrupt_cp_sat);
    }
  }
}

TEST(RoutingSearchTest, SlowRestoreAndTimeLimits) {
  const int64_t kTimeLimitSeconds = 5;
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_use_cp(BOOL_FALSE);
  parameters.set_use_generalized_cp_sat(BOOL_TRUE);
  parameters.set_disable_scheduling_beware_this_may_degrade_performance(true);
  parameters.set_report_intermediate_cp_sat_solutions(true);
  parameters.mutable_time_limit()->set_seconds(kTimeLimitSeconds);
  parameters.set_log_search(true);
  IndexManager manager(2, 2, NodeIndex(0));
  Model model(manager);
  const int time_callback =
      model.RegisterTransitCallback([](int64_t, int64_t) { return 0; });
  model.AddDimension(time_callback, /*slack_max=*/31611598,
                     /*capacity=*/31611598, /*fix_start_cumul_to_zero=*/false,
                     "time");
  model.GetMutableDimension("time")->SetSpanCostCoefficientForVehicle(
      /*coefficient=*/164095200, /*vehicle=*/0);
  // Adding resources to make the finalizer slow, especially since
  // disable_scheduling_beware_this_may_degrade_performance is true.
  Model::ResourceGroup& rg = *model.AddResourceGroup();
  rg.AddResource(Model::ResourceGroup::Attributes(Domain(31538974, 31611598),
                                                  Domain(0, 31611598)),
                 model.GetMutableDimension("time"));
  rg.AddResource(Model::ResourceGroup::Attributes(Domain(0, 31611598),
                                                  Domain(31549629, 31611598)),
                 model.GetMutableDimension("time"));
  rg.NotifyVehicleRequiresAResource(1);
  const int kNumRuns = 5;
  for (int i = 0; i < kNumRuns; ++i) {
    const absl::Time start_time = absl::Now();
    const Assignment* solution = model.SolveWithParameters(parameters);
    EXPECT_EQ(solution, nullptr);
    EXPECT_LT(absl::Now() - start_time,
              absl::Seconds(kTimeLimitSeconds) + /*slack*/ absl::Seconds(1));
  }
}

}  // namespace
}  // namespace operations_research::routing

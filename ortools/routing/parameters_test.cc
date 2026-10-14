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

#include "ortools/routing/parameters.h"

#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/duration.pb.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_text_proto.h"
#include "ortools/base/proto_enum_utils.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/solver_parameters.pb.h"
#include "ortools/routing/enums.pb.h"
#include "ortools/routing/heuristic_parameters.pb.h"
#include "ortools/routing/ils.pb.h"
#include "ortools/routing/ils_parameters_utils.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/optional_boolean.pb.h"

namespace operations_research::routing {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTextOrDie;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::EqualsProto;
using ::testing::EquivToProto;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAreArray;
using ::testing::proto::Partially;

TEST(DefaultRoutingModelParametersTest, GoldenTest) {
  auto parameters =
      ParseTextOrDie<RoutingModelParameters>("reduce_vehicle_cost_model: true");
  *parameters.mutable_solver_parameters() = Solver::DefaultSolverParameters();
  parameters.mutable_solver_parameters()->set_skip_locally_optimal_paths(true);
  EXPECT_THAT(DefaultRoutingModelParameters(), EqualsProto(parameters));
}

TEST(RoutingModelTest, DefaultRoutingSearchParameters) {
  EXPECT_THAT(
      DefaultRoutingSearchParameters(), EqualsProto(R"pb(
        first_solution_strategy: AUTOMATIC
        use_unfiltered_first_solution_strategy: false
        savings_parameters {
          neighbors_ratio: 1
          max_memory_usage_bytes: 6e9
          arc_coefficient: 1
        }
        global_cheapest_insertion_first_solution_parameters {
          farthest_seeds_ratio: 0
          neighbors_ratio: 1
          min_neighbors: 1
          use_neighbors_ratio_for_initialization: false
          add_unperformed_entries: false
        }
        global_cheapest_insertion_ls_operator_parameters {
          farthest_seeds_ratio: 0
          neighbors_ratio: 1
          min_neighbors: 1
          use_neighbors_ratio_for_initialization: true
          add_unperformed_entries: false
        }
        local_cheapest_insertion_parameters {
          pickup_delivery_strategy: BEST_PICKUP_THEN_BEST_DELIVERY
        }
        local_cheapest_cost_insertion_parameters {
          pickup_delivery_strategy: BEST_PICKUP_DELIVERY_PAIR
        }
        local_search_operators {
          use_relocate: BOOL_TRUE
          use_relocate_pair: BOOL_TRUE
          use_light_relocate_pair: BOOL_TRUE
          use_relocate_neighbors: BOOL_FALSE
          use_exchange: BOOL_TRUE
          use_exchange_pair: BOOL_TRUE
          use_cross: BOOL_TRUE
          use_cross_exchange: BOOL_FALSE
          use_relocate_expensive_chain: BOOL_TRUE
          use_two_opt: BOOL_TRUE
          use_or_opt: BOOL_TRUE
          use_lin_kernighan: BOOL_TRUE
          use_tsp_opt: BOOL_FALSE
          use_make_active: BOOL_TRUE
          use_exchange_and_make_active: BOOL_FALSE
          use_exchange_path_start_ends_and_make_active: BOOL_FALSE
          use_make_inactive: BOOL_TRUE
          use_make_chain_inactive: BOOL_TRUE
          use_swap_active: BOOL_TRUE
          use_swap_active_chain: BOOL_TRUE
          use_extended_swap_active: BOOL_FALSE
          use_shortest_path_swap_active: BOOL_TRUE
          use_shortest_path_two_opt: BOOL_TRUE
          use_node_pair_swap_active: BOOL_FALSE
          use_relocate_and_make_active: BOOL_FALSE
          use_path_lns: BOOL_FALSE
          use_full_path_lns: BOOL_FALSE
          use_tsp_lns: BOOL_FALSE
          use_inactive_lns: BOOL_FALSE
          use_relocate_subtrip: BOOL_TRUE
          use_exchange_subtrip: BOOL_TRUE
          use_global_cheapest_insertion_path_lns: BOOL_TRUE
          use_local_cheapest_insertion_path_lns: BOOL_TRUE
          use_relocate_path_global_cheapest_insertion_insert_unperformed:
              BOOL_TRUE
          use_global_cheapest_insertion_expensive_chain_lns: BOOL_FALSE
          use_local_cheapest_insertion_expensive_chain_lns: BOOL_FALSE
          use_global_cheapest_insertion_close_nodes_lns: BOOL_FALSE
          use_local_cheapest_insertion_close_nodes_lns: BOOL_FALSE
          use_global_cheapest_insertion_visit_types_lns: BOOL_TRUE
          use_local_cheapest_insertion_visit_types_lns: BOOL_TRUE
        }
        use_multi_armed_bandit_concatenate_operators: false
        multi_armed_bandit_compound_operator_memory_coefficient: 0.04
        multi_armed_bandit_compound_operator_exploration_coefficient: 1e12
        relocate_expensive_chain_num_arcs_to_consider: 4
        heuristic_expensive_chain_lns_num_arcs_to_consider: 4
        heuristic_close_nodes_lns_num_nodes: 5
        local_search_metaheuristic: AUTOMATIC
        guided_local_search_lambda_coefficient: 0.1
        guided_local_search_reset_penalties_on_new_best_solution: false
        use_depth_first_search: false
        use_cp: BOOL_TRUE
        use_cp_sat: BOOL_FALSE
        sat_parameters { linearization_level: 2 num_workers: 1 }
        use_generalized_cp_sat: BOOL_FALSE
        continuous_scheduling_solver: SCHEDULING_GLOP
        mixed_integer_scheduling_solver: SCHEDULING_CP_SAT
        fallback_to_cp_sat_size_threshold: 20
        ls_operator_neighbors_ratio: 1
        ls_operator_min_neighbors: 1
        disable_scheduling_beware_this_may_degrade_performance: false
        optimization_step: 0
        solution_limit: 0x7FFFFFFFFFFFFFFF
        # We're in a unit test, so lns_time_limit will get 1 second added to it.
        lns_time_limit: { seconds: 1 nanos: 100000000 }
        secondary_ls_time_limit_ratio: 0
        use_full_propagation: false
        number_of_solutions_to_collect: 1
        log_cost_scaling_factor: 1
        num_max_local_optima_before_metaheuristic_switch: 200
        max_swap_active_chain_size: 10
      )pb"));
}

TEST(RoutingModelTest, DefaultSecondaryRoutingSearchParameters) {
  EXPECT_THAT(
      DefaultSecondaryRoutingSearchParameters(), EqualsProto(R"pb(
        first_solution_strategy: AUTOMATIC
        use_unfiltered_first_solution_strategy: false
        savings_parameters {
          neighbors_ratio: 1
          max_memory_usage_bytes: 6e9
          arc_coefficient: 1
        }
        global_cheapest_insertion_first_solution_parameters {
          farthest_seeds_ratio: 0
          neighbors_ratio: 1
          min_neighbors: 1
          use_neighbors_ratio_for_initialization: false
          add_unperformed_entries: false
        }
        global_cheapest_insertion_ls_operator_parameters {
          farthest_seeds_ratio: 0
          neighbors_ratio: 1
          min_neighbors: 1
          use_neighbors_ratio_for_initialization: true
          add_unperformed_entries: false
        }
        local_cheapest_insertion_parameters {
          pickup_delivery_strategy: BEST_PICKUP_THEN_BEST_DELIVERY
        }
        local_cheapest_cost_insertion_parameters {
          pickup_delivery_strategy: BEST_PICKUP_DELIVERY_PAIR
        }
        local_search_operators {
          use_relocate: BOOL_TRUE
          use_relocate_pair: BOOL_FALSE
          use_light_relocate_pair: BOOL_TRUE
          use_relocate_neighbors: BOOL_FALSE
          use_exchange: BOOL_TRUE
          use_exchange_pair: BOOL_TRUE
          use_cross: BOOL_TRUE
          use_cross_exchange: BOOL_FALSE
          use_relocate_expensive_chain: BOOL_FALSE
          use_two_opt: BOOL_TRUE
          use_or_opt: BOOL_TRUE
          use_lin_kernighan: BOOL_TRUE
          use_tsp_opt: BOOL_FALSE
          use_make_active: BOOL_FALSE
          use_exchange_and_make_active: BOOL_FALSE
          use_exchange_path_start_ends_and_make_active: BOOL_FALSE
          use_make_inactive: BOOL_FALSE
          use_make_chain_inactive: BOOL_FALSE
          use_swap_active: BOOL_FALSE
          use_swap_active_chain: BOOL_FALSE
          use_extended_swap_active: BOOL_FALSE
          use_shortest_path_swap_active: BOOL_FALSE
          use_shortest_path_two_opt: BOOL_FALSE
          use_node_pair_swap_active: BOOL_FALSE
          use_relocate_and_make_active: BOOL_FALSE
          use_path_lns: BOOL_FALSE
          use_full_path_lns: BOOL_FALSE
          use_tsp_lns: BOOL_FALSE
          use_inactive_lns: BOOL_FALSE
          use_relocate_subtrip: BOOL_TRUE
          use_exchange_subtrip: BOOL_TRUE
          use_global_cheapest_insertion_path_lns: BOOL_FALSE
          use_local_cheapest_insertion_path_lns: BOOL_FALSE
          use_relocate_path_global_cheapest_insertion_insert_unperformed:
              BOOL_FALSE
          use_global_cheapest_insertion_expensive_chain_lns: BOOL_FALSE
          use_local_cheapest_insertion_expensive_chain_lns: BOOL_FALSE
          use_global_cheapest_insertion_close_nodes_lns: BOOL_FALSE
          use_local_cheapest_insertion_close_nodes_lns: BOOL_FALSE
          use_global_cheapest_insertion_visit_types_lns: BOOL_TRUE
          use_local_cheapest_insertion_visit_types_lns: BOOL_TRUE
        }
        use_multi_armed_bandit_concatenate_operators: false
        multi_armed_bandit_compound_operator_memory_coefficient: 0.04
        multi_armed_bandit_compound_operator_exploration_coefficient: 1e12
        relocate_expensive_chain_num_arcs_to_consider: 4
        heuristic_expensive_chain_lns_num_arcs_to_consider: 4
        heuristic_close_nodes_lns_num_nodes: 5
        local_search_metaheuristic: GREEDY_DESCENT
        guided_local_search_lambda_coefficient: 0.1
        guided_local_search_reset_penalties_on_new_best_solution: false
        use_depth_first_search: false
        use_cp: BOOL_TRUE
        use_cp_sat: BOOL_FALSE
        sat_parameters { linearization_level: 2 num_workers: 1 }
        use_generalized_cp_sat: BOOL_FALSE
        continuous_scheduling_solver: SCHEDULING_GLOP
        mixed_integer_scheduling_solver: SCHEDULING_CP_SAT
        fallback_to_cp_sat_size_threshold: 20
        ls_operator_neighbors_ratio: 1
        ls_operator_min_neighbors: 1
        disable_scheduling_beware_this_may_degrade_performance: false
        optimization_step: 0
        solution_limit: 0x7FFFFFFFFFFFFFFF
        # We're in a unit test, so lns_time_limit will get 1 second added to it.
        lns_time_limit: { seconds: 1 nanos: 100000000 }
        secondary_ls_time_limit_ratio: 0
        use_full_propagation: false
        number_of_solutions_to_collect: 1
        log_cost_scaling_factor: 1
        num_max_local_optima_before_metaheuristic_switch: 200
        max_swap_active_chain_size: 10
      )pb"));
}

TEST(RoutingModelTest, DefaultIteratedLocalSearchParameters) {
  EXPECT_THAT(RecommendedIteratedLocalSearchParameters(),
              Partially(EquivToProto(R"pb(
                ruin_recreate_parameters {
                  ruin_strategies {
                    spatially_close_routes { num_ruined_routes: 2 }
                  }
                }
                reference_solution_acceptance_policy {
                  strategies {
                    simulated_annealing {
                      cooling_schedule_strategy: EXPONENTIAL
                      initial_temperature: 100.0
                      final_temperature: 0.01
                      automatic_temperatures: false
                    }
                  }
                }
                best_solution_acceptance_policy {
                  strategies { greedy_descent {} }
                })pb")));
}

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kEpsilon = std::numeric_limits<double>::epsilon();

class RoutingSearchParametersValidationTest : public ::testing::Test {
 protected:
  void SetUp() override { parameters_ = DefaultRoutingSearchParameters(); }

  RoutingSearchParameters parameters_;

  static google::protobuf::Duration ParseDuration(absl::string_view str) {
    return google::protobuf::contrib::parse_proto::ParseTextOrDie<
        google::protobuf::Duration>(str);
  }

  std::vector<google::protobuf::Duration> InvalidDurations() {
    return {
        // First, some durations that aren't even valid Google API durations
        ParseDuration("seconds:-315576000001, nanos:0"),  // Too low
        ParseDuration("seconds:315576000001, nanos:0"),   // Too high
        ParseDuration("seconds:42, nanos:-1"),            // Sign discrepancy.
        ParseDuration("seconds:42 nanos:1000000000"),     // "nanos" too high.
        ParseDuration("seconds:-1 nanos:-1000000000"),    // "nanos" too low.
        // Then, some durations that are valid but negative.
        ParseDuration("seconds:-1 nanos:0"),
        ParseDuration("seconds:0 nanos:-1"),
    };
  }

  std::vector<google::protobuf::Duration> ValidDurations() {
    return {
        ParseDuration(""),
        ParseDuration("seconds:0 nanos:0"),
        ParseDuration("seconds:0"),
        ParseDuration("nanos:0"),
        ParseDuration("seconds:42 nanos:59"),
        ParseDuration("seconds:315576000000 nanos:999999999"),  // Max
    };
  }
};

TEST_F(RoutingSearchParametersValidationTest, BaselineIsValid) {
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_), IsEmpty());
}

TEST_F(RoutingSearchParametersValidationTest, TimeLimit) {
  for (const google::protobuf::Duration& d : ValidDurations()) {
    *parameters_.mutable_time_limit() = d;
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_), IsEmpty());
  }
  parameters_.clear_time_limit();
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_), IsEmpty());

  for (const google::protobuf::Duration& d : InvalidDurations()) {
    *parameters_.mutable_time_limit() = d;
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
                ElementsAre(HasSubstr("time_limit")));
  }
}

TEST_F(RoutingSearchParametersValidationTest, LnsTimeLimit) {
  for (const google::protobuf::Duration& d : ValidDurations()) {
    *parameters_.mutable_lns_time_limit() = d;
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_), IsEmpty());
  }
  parameters_.clear_lns_time_limit();
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_), IsEmpty());

  for (const google::protobuf::Duration& d : InvalidDurations()) {
    *parameters_.mutable_lns_time_limit() = d;
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
                ElementsAre(HasSubstr("lns_time_limit")));
  }
}

TEST_F(RoutingSearchParametersValidationTest, SecondaryLSTimeLimitRatio) {
  // Verify that a secondary_ls_time_limit_ratio of 0.0 is accepted.
  parameters_.set_secondary_ls_time_limit_ratio(0.0);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_), IsEmpty());

  for (double ratio : {kNaN, -kEpsilon, 1.0, 1 + kEpsilon, -1.0, kInfinity}) {
    parameters_.set_secondary_ls_time_limit_ratio(ratio);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
                ElementsAre(HasSubstr("secondary_ls_time_limit_ratio")))
        << ratio;
  }
}

TEST_F(RoutingSearchParametersValidationTest, LocalSearchMetaheuristic) {
  parameters_.set_local_search_metaheuristic(
      LocalSearchMetaheuristic::AUTOMATIC);
  parameters_.set_num_max_local_optima_before_metaheuristic_switch(10);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_), IsEmpty());
  parameters_.add_local_search_metaheuristics(
      LocalSearchMetaheuristic::TABU_SEARCH);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_), IsEmpty());
  parameters_.set_local_search_metaheuristic(
      LocalSearchMetaheuristic::GUIDED_LOCAL_SEARCH);
  EXPECT_THAT(
      FindErrorsInRoutingSearchParameters(parameters_),
      ElementsAre(HasSubstr("local_search_metaheuristics cannot be set if")));
  parameters_.set_local_search_metaheuristic(
      LocalSearchMetaheuristic::AUTOMATIC);
  parameters_.add_local_search_metaheuristics(LocalSearchMetaheuristic::UNSET);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
              ElementsAre(HasSubstr("Invalid metaheuristic")));
  parameters_.clear_local_search_metaheuristics();
  parameters_.add_local_search_metaheuristics(
      LocalSearchMetaheuristic::TABU_SEARCH);
  parameters_.set_num_max_local_optima_before_metaheuristic_switch(0);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
              ElementsAre(HasSubstr(
                  "Invalid num_max_local_optima_before_metaheuristic_switch")));
}

TEST_F(RoutingSearchParametersValidationTest, LocalSearchOperatorUnspecified) {
  parameters_.mutable_local_search_operators()->clear_use_make_inactive();
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
              ElementsAre(HasSubstr("use_make_inactive")));

  parameters_.mutable_local_search_operators()->set_use_make_inactive(
      BOOL_UNSPECIFIED);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
              ElementsAre(HasSubstr("use_make_inactive")));
}

TEST_F(RoutingSearchParametersValidationTest, LocalSearchOperatorInvalidBool) {
  parameters_.mutable_local_search_operators()->set_use_relocate(
      static_cast<OptionalBoolean>(42));
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
              ElementsAre(HasSubstr("use_relocate")));
}

void SetRecommendedIteratedLocalSearchParameters(
    RoutingSearchParameters& search_parameters) {
  *search_parameters.mutable_iterated_local_search_parameters() =
      RecommendedIteratedLocalSearchParameters();
}

constexpr double kInvalidSavingsNeighborsRatios[] = {
    kNaN, -kEpsilon, 0.0, 1 + kEpsilon, -1.0, kInfinity};

TEST_F(RoutingSearchParametersValidationTest,
       SavingsParameters_FirstSolutionHeuristic_NeighborsRatio) {
  for (double value : kInvalidSavingsNeighborsRatios) {
    parameters_.mutable_savings_parameters()->set_neighbors_ratio(value);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
                ElementsAre(AllOf(
                    HasSubstr("Savings"), HasSubstr("first solution heuristic"),
                    HasSubstr("savings_parameters.neighbors_ratio"))))
        << value;
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       SavingsParameters_RecreateHeuristic_NeighborsRatio) {
  SetRecommendedIteratedLocalSearchParameters(parameters_);
  RecreateStrategy* recreate_strategy =
      parameters_.mutable_iterated_local_search_parameters()
          ->mutable_ruin_recreate_parameters()
          ->mutable_recreate_strategy();
  for (const FirstSolutionStrategy::Value heuristic :
       {FirstSolutionStrategy::SAVINGS,
        FirstSolutionStrategy::PARALLEL_SAVINGS}) {
    recreate_strategy->set_heuristic(heuristic);
    SavingsParameters* savings_parameters =
        recreate_strategy->mutable_parameters()->mutable_savings();
    // Get default values from the RoutingSearchParameters proto.
    *savings_parameters = parameters_.savings_parameters();
    for (double value : kInvalidSavingsNeighborsRatios) {
      savings_parameters->set_neighbors_ratio(value);
      EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
                  ElementsAre(AllOf(
                      HasSubstr("Savings"), HasSubstr("recreate heuristic"),
                      HasSubstr("savings_parameters.neighbors_ratio"))))
          << value;
    }
  }
}

constexpr double kInvalidSavingsMaxMemoryUsageBytes[] = {
    kNaN, -kEpsilon, -1.0, 0.0, 1e10 + 0.01, kInfinity};

TEST_F(RoutingSearchParametersValidationTest,
       SavingsParameters_FirstSolutionHeuristic_MaxMemoryUsage) {
  for (double value : kInvalidSavingsMaxMemoryUsageBytes) {
    parameters_.mutable_savings_parameters()->set_max_memory_usage_bytes(value);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
                ElementsAre(AllOf(
                    HasSubstr("Savings"), HasSubstr("first solution heuristic"),
                    HasSubstr("savings_parameters.max_memory_usage_bytes"))))
        << value;
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       SavingsParameters_RecreateHeuristic_MaxMemoryUsage) {
  SetRecommendedIteratedLocalSearchParameters(parameters_);
  RecreateStrategy* recreate_strategy =
      parameters_.mutable_iterated_local_search_parameters()
          ->mutable_ruin_recreate_parameters()
          ->mutable_recreate_strategy();
  for (const FirstSolutionStrategy::Value heuristic :
       {FirstSolutionStrategy::SAVINGS,
        FirstSolutionStrategy::PARALLEL_SAVINGS}) {
    recreate_strategy->set_heuristic(heuristic);
    SavingsParameters* savings_parameters =
        recreate_strategy->mutable_parameters()->mutable_savings();
    // Get default values from the RoutingSearchParameters proto.
    *savings_parameters = parameters_.savings_parameters();
    for (double value : kInvalidSavingsMaxMemoryUsageBytes) {
      savings_parameters->set_max_memory_usage_bytes(value);
      EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
                  ElementsAre(AllOf(
                      HasSubstr("Savings"), HasSubstr("recreate heuristic"),
                      HasSubstr("savings_parameters.max_memory_usage_bytes"))))
          << value;
    }
  }
}

constexpr double kInvalidSavingsArcCoefficient[] = {kNaN, -kEpsilon, -1.0, 0.0,
                                                    kInfinity};

TEST_F(RoutingSearchParametersValidationTest,
       SavingsParameters_FirstSolutionHeuristic_ArcCoefficient) {
  for (double value : kInvalidSavingsArcCoefficient) {
    parameters_.mutable_savings_parameters()->set_arc_coefficient(value);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
                ElementsAre(AllOf(
                    HasSubstr("Savings"), HasSubstr("first solution heuristic"),
                    HasSubstr("savings_parameters.arc_coefficient"))))
        << value;
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       SavingsParameters_RecreateHeuristic_ArcCoefficient) {
  SetRecommendedIteratedLocalSearchParameters(parameters_);
  RecreateStrategy* recreate_strategy =
      parameters_.mutable_iterated_local_search_parameters()
          ->mutable_ruin_recreate_parameters()
          ->mutable_recreate_strategy();
  for (const FirstSolutionStrategy::Value heuristic :
       {FirstSolutionStrategy::SAVINGS,
        FirstSolutionStrategy::PARALLEL_SAVINGS}) {
    recreate_strategy->set_heuristic(heuristic);
    SavingsParameters* savings_parameters =
        recreate_strategy->mutable_parameters()->mutable_savings();
    // Get default values from the RoutingSearchParameters proto.
    *savings_parameters = parameters_.savings_parameters();
    for (double value : kInvalidSavingsArcCoefficient) {
      savings_parameters->set_arc_coefficient(value);
      EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
                  ElementsAre(AllOf(
                      HasSubstr("Savings"), HasSubstr("recreate heuristic"),
                      HasSubstr("savings_parameters.arc_coefficient"))))
          << value;
    }
  }
}

constexpr double kInvalidGlobalCheapestInsertionFarthestSeedsRatio[] = {
    kNaN, -kEpsilon, 1 + kEpsilon, -1.0, kInfinity};

TEST_F(RoutingSearchParametersValidationTest,
       GlobalCheapestInsertion_FirstSolutionHeuristic_FarthestSeedsRatio) {
  for (double value : kInvalidGlobalCheapestInsertionFarthestSeedsRatio) {
    parameters_.mutable_global_cheapest_insertion_first_solution_parameters()
        ->set_farthest_seeds_ratio(value);
    EXPECT_THAT(
        FindErrorsInRoutingSearchParameters(parameters_),
        ElementsAre(AllOf(
            HasSubstr("Global cheapest insertion"),
            HasSubstr("first solution heuristic"),
            HasSubstr(
                "global_cheapest_insertion_parameters.farthest_seeds_ratio"))))
        << value;
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       GlobalCheapestInsertion_LnsHeuristic_FarthestSeedsRatio) {
  for (double value : kInvalidGlobalCheapestInsertionFarthestSeedsRatio) {
    parameters_.mutable_global_cheapest_insertion_ls_operator_parameters()
        ->set_farthest_seeds_ratio(value);
    EXPECT_THAT(
        FindErrorsInRoutingSearchParameters(parameters_),
        ElementsAre(AllOf(
            HasSubstr("Global cheapest insertion"), HasSubstr("ls operator"),
            HasSubstr(
                "global_cheapest_insertion_parameters.farthest_seeds_ratio"))))
        << value;
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       GlobalCheapestInsertion_RecreateHeuristic_FarthestSeedsRatio) {
  SetRecommendedIteratedLocalSearchParameters(parameters_);
  RecreateStrategy* recreate_strategy =
      parameters_.mutable_iterated_local_search_parameters()
          ->mutable_ruin_recreate_parameters()
          ->mutable_recreate_strategy();
  for (const FirstSolutionStrategy::Value heuristic :
       {FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION,
        FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION}) {
    recreate_strategy->set_heuristic(heuristic);
    GlobalCheapestInsertionParameters* gci_parameters =
        recreate_strategy->mutable_parameters()
            ->mutable_global_cheapest_insertion();
    // Get default values from the RoutingSearchParameters proto.
    *gci_parameters =
        parameters_.global_cheapest_insertion_first_solution_parameters();
    for (double value : kInvalidGlobalCheapestInsertionFarthestSeedsRatio) {
      gci_parameters->set_farthest_seeds_ratio(value);
      EXPECT_THAT(
          FindErrorsInRoutingSearchParameters(parameters_),
          ElementsAre(AllOf(HasSubstr("Global cheapest insertion"),
                            HasSubstr("recreate heuristic"),
                            HasSubstr("global_cheapest_insertion_parameters."
                                      "farthest_seeds_ratio"))))
          << value;
    }
  }
}

constexpr double kInvalidGlobalCheapestInsertionNeighborsRatio[] = {
    kNaN, -kEpsilon, 0.0, 1 + kEpsilon, -1.0, kInfinity};

TEST_F(RoutingSearchParametersValidationTest,
       GlobalCheapestInsertion_FirstSolutionHeuristic_NeighborsRatio) {
  for (double value : kInvalidGlobalCheapestInsertionNeighborsRatio) {
    parameters_.mutable_global_cheapest_insertion_first_solution_parameters()
        ->set_neighbors_ratio(value);
    EXPECT_THAT(
        FindErrorsInRoutingSearchParameters(parameters_),
        ElementsAre(AllOf(
            HasSubstr("Global cheapest insertion"),
            HasSubstr("first solution heuristic"),
            HasSubstr("global_cheapest_insertion_parameters.neighbors_ratio"))))
        << value;
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       GlobalCheapestInsertion_LnsHeuristic_NeighborsRatio) {
  for (double value : kInvalidGlobalCheapestInsertionNeighborsRatio) {
    parameters_.mutable_global_cheapest_insertion_ls_operator_parameters()
        ->set_neighbors_ratio(value);
    EXPECT_THAT(
        FindErrorsInRoutingSearchParameters(parameters_),
        ElementsAre(AllOf(
            HasSubstr("Global cheapest insertion"), HasSubstr("ls operator"),
            HasSubstr("global_cheapest_insertion_parameters.neighbors_ratio"))))
        << value;
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       GlobalCheapestInsertion_RecreateHeuristic_NeighborsRatio) {
  SetRecommendedIteratedLocalSearchParameters(parameters_);
  RecreateStrategy* recreate_strategy =
      parameters_.mutable_iterated_local_search_parameters()
          ->mutable_ruin_recreate_parameters()
          ->mutable_recreate_strategy();
  for (const FirstSolutionStrategy::Value heuristic :
       {FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION,
        FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION}) {
    recreate_strategy->set_heuristic(heuristic);
    GlobalCheapestInsertionParameters* gci_parameters =
        recreate_strategy->mutable_parameters()
            ->mutable_global_cheapest_insertion();
    // Get default values from the RoutingSearchParameters proto.
    *gci_parameters =
        parameters_.global_cheapest_insertion_first_solution_parameters();
    for (double value : kInvalidGlobalCheapestInsertionNeighborsRatio) {
      gci_parameters->set_neighbors_ratio(value);
      EXPECT_THAT(
          FindErrorsInRoutingSearchParameters(parameters_),
          ElementsAre(AllOf(
              HasSubstr("Global cheapest insertion"),
              HasSubstr("recreate heuristic"),
              HasSubstr(
                  "global_cheapest_insertion_parameters.neighbors_ratio"))))
          << value;
    }
  }
}

constexpr int32_t kInvalidGlobalCheapestInsertionMinNeighbors[] = {kint32min,
                                                                   -1, 0};

TEST_F(RoutingSearchParametersValidationTest,
       GlobalCheapestInsertion_FirstSolutionHeuristic_MinNeighbors) {
  for (int value : kInvalidGlobalCheapestInsertionMinNeighbors) {
    parameters_.mutable_global_cheapest_insertion_first_solution_parameters()
        ->set_min_neighbors(value);
    EXPECT_THAT(
        FindErrorsInRoutingSearchParameters(parameters_),
        ElementsAre(HasSubstr(
            "Global cheapest insertion (first solution heuristic) - Invalid "
            "global_cheapest_insertion_parameters.min_neighbors")))
        << value;
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       GlobalCheapestInsertion_LnsHeuristic_MinNeighbors) {
  for (int value : kInvalidGlobalCheapestInsertionMinNeighbors) {
    parameters_.mutable_global_cheapest_insertion_ls_operator_parameters()
        ->set_min_neighbors(value);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
                ElementsAre(HasSubstr(
                    "Global cheapest insertion (ls operator) - Invalid "
                    "global_cheapest_insertion_parameters.min_neighbors")))
        << value;
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       GlobalCheapestInsertion_RecreateHeuristic_MinNeighbors) {
  SetRecommendedIteratedLocalSearchParameters(parameters_);
  RecreateStrategy* recreate_strategy =
      parameters_.mutable_iterated_local_search_parameters()
          ->mutable_ruin_recreate_parameters()
          ->mutable_recreate_strategy();
  for (const FirstSolutionStrategy::Value heuristic :
       {FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION,
        FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION}) {
    recreate_strategy->set_heuristic(heuristic);
    GlobalCheapestInsertionParameters* gci_parameters =
        recreate_strategy->mutable_parameters()
            ->mutable_global_cheapest_insertion();
    // Get default values from the RoutingSearchParameters proto.
    *gci_parameters =
        parameters_.global_cheapest_insertion_first_solution_parameters();
    for (int value : kInvalidGlobalCheapestInsertionMinNeighbors) {
      gci_parameters->set_min_neighbors(value);
      EXPECT_THAT(
          FindErrorsInRoutingSearchParameters(parameters_),
          ElementsAre(HasSubstr(
              "Global cheapest insertion (recreate heuristic) - Invalid "
              "global_cheapest_insertion_parameters.min_neighbors")))
          << value;
    }
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       RelocateExpensiveChainNumArcsToConsider) {
  for (int value : {kint32min, -1, 0, 1, 1000001, kint32max}) {
    parameters_.set_relocate_expensive_chain_num_arcs_to_consider(value);
    EXPECT_THAT(
        FindErrorsInRoutingSearchParameters(parameters_),
        ElementsAre(HasSubstr("relocate_expensive_chain_num_arcs_to_consider")))
        << value;
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       HeuristicExpensiveChainLNSNumArcsToConsider) {
  for (int value : {kint32min, -1, 0, 1, 1000001, kint32max}) {
    parameters_.set_heuristic_expensive_chain_lns_num_arcs_to_consider(value);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
                ElementsAre(HasSubstr(
                    "heuristic_expensive_chain_lns_num_arcs_to_consider")))
        << value;
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       HeuristicCloseNodesLNSNumClosestNodes) {
  for (int value : {kint32min, -1, 10001, kint32max}) {
    parameters_.set_heuristic_close_nodes_lns_num_nodes(value);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
                ElementsAre(HasSubstr("heuristic_close_nodes_lns_num_nodes")))
        << value;
  }
}

TEST_F(RoutingSearchParametersValidationTest, SwapActiveChain) {
  parameters_.mutable_local_search_operators()->set_use_swap_active_chain(
      OptionalBoolean::BOOL_FALSE);
  parameters_.set_max_swap_active_chain_size(0);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_), IsEmpty());
  parameters_.mutable_local_search_operators()->set_use_swap_active_chain(
      OptionalBoolean::BOOL_TRUE);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
              ElementsAre(HasSubstr("max_swap_active_chain_size")));
}

TEST_F(RoutingSearchParametersValidationTest,
       GuidedLocalSearchLambdaCoefficient) {
  for (double value : {kNaN, -kEpsilon, -1.0, -kInfinity, kInfinity}) {
    parameters_.set_guided_local_search_lambda_coefficient(value);
    EXPECT_THAT(
        FindErrorsInRoutingSearchParameters(parameters_),
        ElementsAre(HasSubstr("guided_local_search_lambda_coefficient")))
        << value;
  }
}

TEST_F(RoutingSearchParametersValidationTest, OptimizationStep) {
  for (double value : {kNaN, -kEpsilon, -1.0, -kInfinity}) {
    parameters_.set_optimization_step(value);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
                ElementsAre(HasSubstr("optimization_step")))
        << value;
  }
  for (double value : {0.0, 1.0, 1e6, kInfinity}) {
    parameters_.set_optimization_step(value);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_), IsEmpty())
        << value;
  }
}

TEST_F(RoutingSearchParametersValidationTest, NumberOfSolutionsToCollect) {
  for (int value : {0, -1, kint32min}) {
    parameters_.set_number_of_solutions_to_collect(value);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
                ElementsAre(HasSubstr("number_of_solutions_to_collect")))
        << value;
  }
}

TEST_F(RoutingSearchParametersValidationTest, SolutionLimit) {
  for (int64_t value : {int64_t{0}, int64_t{-1}, kint64min}) {
    parameters_.set_solution_limit(value);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
                ElementsAre(HasSubstr("solution_limit")))
        << value;
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       ImprovementSearchLimitRateCoefficient) {
  RoutingSearchParameters::ImprovementSearchLimitParameters*
      improvement_limit_parameters =
          parameters_.mutable_improvement_limit_parameters();
  improvement_limit_parameters->set_improvement_rate_solutions_distance(1);

  EXPECT_THAT(
      FindErrorsInRoutingSearchParameters(parameters_),
      ElementsAre(HasSubstr(
          "improvement_limit_parameters.improvement_rate_coefficient")));

  const std::vector<double> valid_parameters = {0.1, 10.0, kInfinity};
  for (double improvement_rate_coefficient : valid_parameters) {
    improvement_limit_parameters->set_improvement_rate_coefficient(
        improvement_rate_coefficient);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_), IsEmpty())
        << improvement_rate_coefficient;
  }

  const std::vector<double> invalid_parameters = {kNaN, -kInfinity, -kEpsilon,
                                                  0.0};
  for (double improvement_rate_coefficient : invalid_parameters) {
    improvement_limit_parameters->set_improvement_rate_coefficient(
        improvement_rate_coefficient);
    EXPECT_THAT(
        FindErrorsInRoutingSearchParameters(parameters_),
        ElementsAre(HasSubstr(
            "improvement_limit_parameters.improvement_rate_coefficient")))
        << improvement_rate_coefficient;
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       ImprovementSearchLimitSolutionsDistance) {
  RoutingSearchParameters::ImprovementSearchLimitParameters*
      improvement_limit_parameters =
          parameters_.mutable_improvement_limit_parameters();
  improvement_limit_parameters->set_improvement_rate_coefficient(1);

  EXPECT_THAT(
      FindErrorsInRoutingSearchParameters(parameters_),
      ElementsAre(HasSubstr(
          "improvement_limit_parameters.improvement_rate_solutions_distance")));

  std::vector<int> valid_parameters = {1, 10, kint32max};
  for (int improvement_rate_solutions_distance : valid_parameters) {
    improvement_limit_parameters->set_improvement_rate_solutions_distance(
        improvement_rate_solutions_distance);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_), IsEmpty())
        << improvement_rate_solutions_distance;
  }

  std::vector<int> invalid_parameters = {kint32min, -10, 0};
  for (int improvement_rate_solutions_distance : invalid_parameters) {
    improvement_limit_parameters->set_improvement_rate_solutions_distance(
        improvement_rate_solutions_distance);
    EXPECT_THAT(
        FindErrorsInRoutingSearchParameters(parameters_),
        ElementsAre(HasSubstr("improvement_limit_parameters.improvement_rate_"
                              "solutions_distance")))
        << improvement_rate_solutions_distance;
  }
}

TEST_F(RoutingSearchParametersValidationTest, CostScaling) {
  for (double value : {kNaN, 0.0, -kInfinity, kInfinity}) {
    parameters_.set_log_cost_scaling_factor(value);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
                ElementsAre(HasSubstr("log_cost_scaling_factor")))
        << value;
  }
}

TEST_F(RoutingSearchParametersValidationTest, CostOffset) {
  for (double value : {kNaN, -kInfinity, kInfinity}) {
    parameters_.set_log_cost_offset(value);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
                ElementsAre(HasSubstr("log_cost_offset")))
        << value;
  }
}

TEST_F(RoutingSearchParametersValidationTest, ContinuousSchedulingSolverTest) {
  parameters_.set_continuous_scheduling_solver(
      RoutingSearchParameters::SCHEDULING_UNSET);
  EXPECT_THAT(
      FindErrorsInRoutingSearchParameters(parameters_),
      ElementsAre(HasSubstr(
          "Invalid value for continuous_scheduling_solver: SCHEDULING_UNSET")));

  parameters_.set_continuous_scheduling_solver(
      RoutingSearchParameters::SCHEDULING_CP_SAT);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_), IsEmpty());
}

TEST_F(RoutingSearchParametersValidationTest,
       MixedIntegerSchedulingSolverTest) {
  parameters_.set_mixed_integer_scheduling_solver(
      RoutingSearchParameters::SCHEDULING_UNSET);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
              ElementsAre(HasSubstr(
                  "Invalid value for mixed_integer_scheduling_solver: "
                  "SCHEDULING_UNSET")));
}

TEST_F(RoutingSearchParametersValidationTest,
       MultiArmedBanditCompoundOperatorMemoryCoefficientTest) {
  for (double value : {kNaN, -kEpsilon, 1 + kEpsilon, -1.0, kInfinity}) {
    parameters_.set_multi_armed_bandit_compound_operator_memory_coefficient(
        value);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
                ElementsAre(HasSubstr(
                    "multi_armed_bandit_compound_operator_memory_coefficient")))
        << value;
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       MultiArmedBanditCompoundOperatorExplorationCoefficientTest) {
  for (double value : {kNaN, -kInfinity, -kEpsilon}) {
    parameters_
        .set_multi_armed_bandit_compound_operator_exploration_coefficient(
            value);
    EXPECT_THAT(
        FindErrorsInRoutingSearchParameters(parameters_),
        ElementsAre(HasSubstr(
            "multi_armed_bandit_compound_operator_exploration_coefficient")))
        << value;
  }
}

TEST_F(RoutingSearchParametersValidationTest, SatParametersTest) {
  parameters_.mutable_sat_parameters()->set_enumerate_all_solutions(true);

  parameters_.mutable_sat_parameters()->set_num_workers(5);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
              ElementsAre(HasSubstr(
                  "sat_parameters.enumerate_all_solutions cannot be true")));

  parameters_.mutable_sat_parameters()->set_num_workers(1);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_), IsEmpty());

  parameters_.mutable_sat_parameters()->set_interleave_search(true);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters_),
              ElementsAre(HasSubstr(
                  "sat_parameters.enumerate_all_solutions cannot be true")));
}

TEST_F(RoutingSearchParametersValidationTest, WrongestParameters) {
  // Create the set of parameters based on the default ones, to avoid triggering
  // more errors than expected (BOOL_UNSPECIFIED values).
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.mutable_local_search_operators()->clear_use_make_inactive();
  parameters.MergeFrom(ParseTextOrDie<RoutingSearchParameters>(R"pb(
    time_limit { seconds: -315576000001, nanos: 0 }
    lns_time_limit { seconds: -315576000001, nanos: 0 }
    local_search_operators {
      use_make_inactive: BOOL_UNSPECIFIED
      use_relocate: 42
    }
    savings_parameters {
      neighbors_ratio: nan
      max_memory_usage_bytes: nan
      arc_coefficient: nan
    }
    global_cheapest_insertion_first_solution_parameters {
      farthest_seeds_ratio: nan
      neighbors_ratio: nan
      min_neighbors: -1
    }
    global_cheapest_insertion_ls_operator_parameters {
      farthest_seeds_ratio: nan
      neighbors_ratio: nan
      min_neighbors: -1
    }
    relocate_expensive_chain_num_arcs_to_consider: -1
    heuristic_expensive_chain_lns_num_arcs_to_consider: -1
    heuristic_close_nodes_lns_num_nodes: -1
    guided_local_search_lambda_coefficient: -1.0
    optimization_step: -1.0
    number_of_solutions_to_collect: -1
    solution_limit: -1
    improvement_limit_parameters {
      improvement_rate_coefficient: nan,
      improvement_rate_solutions_distance: -10
    }
    log_cost_scaling_factor: nan
    log_cost_offset: nan
  )pb"));

  constexpr absl::string_view kExpectedErrors[] = {
      "time_limit", "lns_time_limit", "use_make_inactive", "use_relocate",
      "savings_parameters.neighbors_ratio",
      "savings_parameters.max_memory_usage_bytes",
      "savings_parameters.arc_coefficient",

      // First solution GCI parameters.
      "global_cheapest_insertion_parameters.farthest_seeds_ratio",
      "global_cheapest_insertion_parameters.neighbors_ratio",
      "global_cheapest_insertion_parameters.min_neighbors",
      // LNS GCI parameters.
      "global_cheapest_insertion_parameters.farthest_seeds_ratio",
      "global_cheapest_insertion_parameters.neighbors_ratio",
      "global_cheapest_insertion_parameters.min_neighbors",

      "relocate_expensive_chain_num_arcs_to_consider",
      "heuristic_expensive_chain_lns_num_arcs_to_consider",
      "heuristic_close_nodes_lns_num_nodes",
      "guided_local_search_lambda_coefficient", "optimization_step",
      "number_of_solutions_to_collect", "solution_limit",
      "improvement_limit_parameters.improvement_rate_coefficient",
      "improvement_limit_parameters.improvement_rate_solutions_distance",
      "log_cost_scaling_factor", "log_cost_offset"};
  std::vector<testing::Matcher<std::string>> expected_errors;
  for (const auto& error : kExpectedErrors) {
    expected_errors.push_back(HasSubstr(error));
  }
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
              UnorderedElementsAreArray(expected_errors));
}

void TestLocalCheapestInsertionSortingProperties(
    const RoutingSearchParameters& parameters,
    LocalCheapestInsertionParameters* lci_params,
    const std::string& error_prefix) {
  lci_params->add_insertion_sorting_properties(
      LocalCheapestInsertionParameters::SORTING_PROPERTY_ALLOWED_VEHICLES);
  lci_params->add_insertion_sorting_properties(
      LocalCheapestInsertionParameters::SORTING_PROPERTY_PENALTY);
  lci_params->add_insertion_sorting_properties(
      LocalCheapestInsertionParameters::
          SORTING_PROPERTY_PENALTY_OVER_ALLOWED_VEHICLES_RATIO);
  lci_params->add_insertion_sorting_properties(
      LocalCheapestInsertionParameters::
          SORTING_PROPERTY_HIGHEST_AVG_ARC_COST_TO_VEHICLE_START_ENDS);
  lci_params->add_insertion_sorting_properties(
      LocalCheapestInsertionParameters::
          SORTING_PROPERTY_LOWEST_AVG_ARC_COST_TO_VEHICLE_START_ENDS);
  lci_params->add_insertion_sorting_properties(
      LocalCheapestInsertionParameters::
          SORTING_PROPERTY_LOWEST_MIN_ARC_COST_TO_VEHICLE_START_ENDS);
  lci_params->add_insertion_sorting_properties(
      LocalCheapestInsertionParameters::
          SORTING_PROPERTY_HIGHEST_DIMENSION_USAGE);

  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());

  // The same property should not be defined more than once.
  lci_params->add_insertion_sorting_properties(
      LocalCheapestInsertionParameters::SORTING_PROPERTY_ALLOWED_VEHICLES);
  lci_params->add_insertion_sorting_properties(
      LocalCheapestInsertionParameters::SORTING_PROPERTY_ALLOWED_VEHICLES);
  const auto errors = FindErrorsInRoutingSearchParameters(parameters);
  EXPECT_THAT(errors,
              ElementsAre(HasSubstr(error_prefix +
                                    " - Duplicate insertion sorting property: "
                                    "SORTING_PROPERTY_ALLOWED_VEHICLES")));

  // The INVALID property should not be used.
  lci_params->add_insertion_sorting_properties(
      LocalCheapestInsertionParameters::SORTING_PROPERTY_UNSPECIFIED);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
              Contains(HasSubstr(error_prefix +
                                 " - Invalid insertion sorting property: "
                                 "SORTING_PROPERTY_UNSPECIFIED")));

  // The RANDOM property cannot be used in conjunction with other properties.
  lci_params->add_insertion_sorting_properties(
      LocalCheapestInsertionParameters::SORTING_PROPERTY_RANDOM);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
              Contains(HasSubstr(error_prefix +
                                 " - SORTING_PROPERTY_RANDOM cannot be used in "
                                 "conjunction with other properties")));

  lci_params->clear_insertion_sorting_properties();
  lci_params->add_insertion_sorting_properties(
      LocalCheapestInsertionParameters::SORTING_PROPERTY_RANDOM);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());
}

TEST_F(RoutingSearchParametersValidationTest,
       LocalCheapestInsertionSortingPropertiesTest_FirstSolutionHeuristic) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  TestLocalCheapestInsertionSortingProperties(
      parameters, parameters.mutable_local_cheapest_insertion_parameters(),
      "Local cheapest insertion (first solution heuristic)");
}

TEST_F(RoutingSearchParametersValidationTest,
       LocalCheapestCostInsertionSortingPropertiesTest_FirstSolutionHeuristic) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  TestLocalCheapestInsertionSortingProperties(
      parameters, parameters.mutable_local_cheapest_cost_insertion_parameters(),
      "Local cheapest cost insertion (first solution heuristic)");
}

TEST_F(RoutingSearchParametersValidationTest,
       LocalCheapestInsertionSortingPropertiesTest_RecreateHeuristic) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);
  RecreateStrategy* recreate_strategy =
      parameters.mutable_iterated_local_search_parameters()
          ->mutable_ruin_recreate_parameters()
          ->mutable_recreate_strategy();
  recreate_strategy->set_heuristic(
      FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION);
  TestLocalCheapestInsertionSortingProperties(
      parameters,
      recreate_strategy->mutable_parameters()
          ->mutable_local_cheapest_insertion(),
      "Local cheapest insertion (recreate heuristic)");
}

TEST_F(RoutingSearchParametersValidationTest,
       LocalCheapestCostInsertionSortingPropertiesTest_RecreateHeuristic) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);
  RecreateStrategy* recreate_strategy =
      parameters.mutable_iterated_local_search_parameters()
          ->mutable_ruin_recreate_parameters()
          ->mutable_recreate_strategy();
  recreate_strategy->set_heuristic(
      FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION);
  TestLocalCheapestInsertionSortingProperties(
      parameters,
      recreate_strategy->mutable_parameters()
          ->mutable_local_cheapest_insertion(),
      "Local cheapest cost insertion (recreate heuristic)");
}

TEST_F(RoutingSearchParametersValidationTest, DefaultIlsParametersTest) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());
}

TEST_F(RoutingSearchParametersValidationTest,
       IlsParametersUnsetPerturbationStrategyTest) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);

  IteratedLocalSearchParameters* ils =
      parameters.mutable_iterated_local_search_parameters();

  // PerturbationStrategy::UNSET is not supported.
  ils->set_perturbation_strategy(PerturbationStrategy::UNSET);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
              Contains(HasSubstr("Invalid `perturbation_strategy` = UNSET")));

  // But we should not raise any error after clearing the ILS parameters.
  parameters.clear_iterated_local_search_parameters();
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());
}

TEST_F(RoutingSearchParametersValidationTest,
       IlsParametersRuinRecreateWithUnsupportedPerturbationStrategyTest) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);

  IteratedLocalSearchParameters* ils =
      parameters.mutable_iterated_local_search_parameters();

  const auto* descriptor = PerturbationStrategy::Value_descriptor();
  for (int i = 0; i < descriptor->value_count(); ++i) {
    const PerturbationStrategy::Value strategy =
        static_cast<PerturbationStrategy::Value>(
            descriptor->value(i)->number());
    if (strategy == PerturbationStrategy::RUIN_AND_RECREATE) {
      continue;
    }

    ils->set_perturbation_strategy(strategy);
    EXPECT_THAT(
        FindErrorsInRoutingSearchParameters(parameters),
        Contains(HasSubstr(absl::StrCat(
            "`ruin_recreate_parameters` is set but `perturbation_strategy` is ",
            PerturbationStrategy::Value_Name(strategy)))));
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       IlsParametersUnsetRuinStrategyTest) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);

  RuinRecreateParameters* rr =
      parameters.mutable_iterated_local_search_parameters()
          ->mutable_ruin_recreate_parameters();

  rr->mutable_ruin_strategies(0)->clear_strategy();

  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
              Contains(HasSubstr("strategy` is not set")));
}

TEST_F(RoutingSearchParametersValidationTest,
       IlsParametersUnsetAcceptanceStrategiesTest) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);

  IteratedLocalSearchParameters* ils =
      parameters.mutable_iterated_local_search_parameters();

  ils->clear_best_solution_acceptance_policy();
  ils->clear_reference_solution_acceptance_policy();
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
              Contains(HasSubstr("best_solution_acceptance_policy")));
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
              Contains(HasSubstr("reference_solution_acceptance_policy")));

  // But we should not raise any error after clearing the ILS parameters.
  parameters.clear_iterated_local_search_parameters();
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());
}

TEST_F(RoutingSearchParametersValidationTest,
       IlsParametersUnsetAcceptanceStrategyTest) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);

  IteratedLocalSearchParameters* ils =
      parameters.mutable_iterated_local_search_parameters();

  ils->mutable_best_solution_acceptance_policy()
      ->mutable_strategies(0)
      ->clear_strategy();

  EXPECT_THAT(
      FindErrorsInRoutingSearchParameters(parameters),
      Contains(HasSubstr(
          "best_solution_acceptance_policy.strategies.strategy` is unset")));
}

TEST_F(RoutingSearchParametersValidationTest,
       IlsParametersSimulatedAnnealingParametersCoolingScheduleStrategyTest) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);

  IteratedLocalSearchParameters* ils =
      parameters.mutable_iterated_local_search_parameters();
  for (const auto& [name, acceptance_policy] :
       std::vector<std::pair<std::string, AcceptancePolicy*>>{
           {"best", ils->mutable_best_solution_acceptance_policy()},
           {"reference",
            ils->mutable_reference_solution_acceptance_policy()}}) {
    // Remove the default acceptance strategies as we will add a single one
    // below.
    acceptance_policy->clear_strategies();

    SimulatedAnnealingAcceptanceStrategy* sa =
        acceptance_policy->add_strategies()->mutable_simulated_annealing();

    sa->set_initial_temperature(100);
    sa->set_final_temperature(0.01);

    // Valid cooling strategies should not raise any error.
    for (CoolingScheduleStrategy::Value cooling_strategy :
         {CoolingScheduleStrategy::EXPONENTIAL,
          CoolingScheduleStrategy::LINEAR}) {
      sa->set_cooling_schedule_strategy(cooling_strategy);
      EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());
    }

    // CoolingScheduleStrategy::UNSET is not supported.
    sa->set_cooling_schedule_strategy(CoolingScheduleStrategy::UNSET);

    EXPECT_THAT(
        FindErrorsInRoutingSearchParameters(parameters),
        Contains(AllOf(
            HasSubstr(
                name +
                "_solution_acceptance_policy.strategies.simulated_annealing"),
            HasSubstr("cooling_schedule_strategy"))));

    // Reset to a valid value.
    sa->set_cooling_schedule_strategy(CoolingScheduleStrategy::EXPONENTIAL);
  }

  // But we should not raise errors for other valid acceptance strategies.
  // Before adding new strategies, remove the existing acceptance strategies to
  // ensure a single one is defined.
  ils->mutable_reference_solution_acceptance_policy()->clear_strategies();
  ils->mutable_best_solution_acceptance_policy()->clear_strategies();
  ils->mutable_best_solution_acceptance_policy()
      ->add_strategies()
      ->mutable_greedy_descent();
  ils->mutable_reference_solution_acceptance_policy()
      ->add_strategies()
      ->mutable_greedy_descent();
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());
}

TEST_F(RoutingSearchParametersValidationTest,
       IlsParametersSimulatedAnnealingParametersInvalidTemperatures) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);

  IteratedLocalSearchParameters* ils =
      parameters.mutable_iterated_local_search_parameters();

  for (const auto& [name, acceptance_policy] :
       std::vector<std::pair<std::string, AcceptancePolicy*>>{
           {"best", ils->mutable_best_solution_acceptance_policy()},
           {"reference",
            ils->mutable_reference_solution_acceptance_policy()}}) {
    // Remove the default acceptance strategies as we will add a single one
    // below.
    acceptance_policy->clear_strategies();

    SimulatedAnnealingAcceptanceStrategy* sa =
        acceptance_policy->add_strategies()->mutable_simulated_annealing();

    // Set a cooling schedule to have valid parameters where there are no
    // temperature issues.
    sa->set_cooling_schedule_strategy(CoolingScheduleStrategy::LINEAR);

    // The initial temperature has to be greater or equal to the final one.
    sa->set_initial_temperature(1.0);
    sa->set_final_temperature(100.0);

    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
                Contains(AllOf(
                    HasSubstr(name + "_solution_acceptance_policy.strategies."
                                     "simulated_annealing."),
                    HasSubstr("initial_temperature"))));

    // Test limit temperatures.
    sa->set_initial_temperature(1e-9);
    sa->set_final_temperature(1e-9);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());

    std::vector<double> invalid_temperatures = {-1.0, 1e-10, 0.0, -kInfinity,
                                                kNaN};

    for (const double temperature : invalid_temperatures) {
      sa->set_initial_temperature(temperature);
      EXPECT_THAT(
          FindErrorsInRoutingSearchParameters(parameters),
          Contains(AllOf(HasSubstr(name + "_solution_acceptance_policy."
                                          "strategies.simulated_annealing"),
                         HasSubstr("initial_temperature"))));
    }

    sa->set_initial_temperature(1e99);
    for (const double temperature : invalid_temperatures) {
      sa->set_final_temperature(temperature);
      EXPECT_THAT(
          FindErrorsInRoutingSearchParameters(parameters),
          Contains(AllOf(
              HasSubstr(
                  name +
                  "_solution_acceptance_policy.strategies.simulated_annealing"),
              HasSubstr("final_temperature"))));
    }

    // But we should not raise errors when using automatically defined
    // temperatures, as long as both initial and final temperatures are not set.
    sa->set_automatic_temperatures(true);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
                Contains(AllOf(HasSubstr("automatic_temperatures"),
                               HasSubstr("final_temperature"))));
    sa->clear_final_temperature();
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
                Contains(AllOf(HasSubstr("automatic_temperatures"),
                               HasSubstr("initial_temperature"))));
    sa->clear_initial_temperature();
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());
    sa->set_final_temperature(1e-9);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
                Contains(AllOf(HasSubstr("automatic_temperatures"),
                               HasSubstr("final_temperature"))));

    // Reset to valid temperatures.
    sa->set_automatic_temperatures(false);
    sa->set_initial_temperature(100);
    sa->set_final_temperature(0.01);
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       IlsParametersThresholdAcceptingParametersInvalidThresholds) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);

  IteratedLocalSearchParameters* ils =
      parameters.mutable_iterated_local_search_parameters();

  for (const auto& [name, acceptance_policy] :
       std::vector<std::pair<std::string, AcceptancePolicy*>>{
           {"best", ils->mutable_best_solution_acceptance_policy()},
           {"reference",
            ils->mutable_reference_solution_acceptance_policy()}}) {
    acceptance_policy->clear_strategies();

    ThresholdAcceptingDeterministicAnnealingAcceptanceStrategy* ta =
        acceptance_policy->add_strategies()
            ->mutable_threshold_accepting_deterministic_annealing();

    ta->set_cooling_schedule_strategy(CoolingScheduleStrategy::LINEAR);

    ta->set_initial_threshold(1.0);
    ta->set_final_threshold(100.0);

    EXPECT_THAT(
        FindErrorsInRoutingSearchParameters(parameters),
        Contains(AllOf(
            HasSubstr(name + "_solution_acceptance_policy.strategies."
                             "threshold_accepting_deterministic_annealing."),
            HasSubstr("initial_threshold"))));

    ta->set_initial_threshold(-1.0);
    ta->set_final_threshold(-1.0);

    EXPECT_THAT(
        FindErrorsInRoutingSearchParameters(parameters),
        Contains(AllOf(
            HasSubstr(name + "_solution_acceptance_policy.strategies."
                             "threshold_accepting_deterministic_annealing."),
            HasSubstr("final_threshold"))));
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       IlsParametersRecordToRecordTravelParametersInvalidDeviation) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);

  IteratedLocalSearchParameters* ils =
      parameters.mutable_iterated_local_search_parameters();

  for (const auto& [name, acceptance_policy] :
       std::vector<std::pair<std::string, AcceptancePolicy*>>{
           {"best", ils->mutable_best_solution_acceptance_policy()},
           {"reference",
            ils->mutable_reference_solution_acceptance_policy()}}) {
    acceptance_policy->clear_strategies();

    RecordToRecordTravelDeterministicAnnealingAcceptanceStrategy* rt =
        acceptance_policy->add_strategies()
            ->mutable_record_to_record_travel_deterministic_annealing();

    rt->set_deviation(-1);

    EXPECT_THAT(
        FindErrorsInRoutingSearchParameters(parameters),
        Contains(
            AllOf(HasSubstr(name +
                            "_solution_acceptance_policy.strategies."
                            "record_to_record_travel_deterministic_annealing."),
                  HasSubstr("deviation"))));
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       IlsParametersAcceptancePolicyCompositionRuleNoStrategiesTest) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);

  IteratedLocalSearchParameters* ils =
      parameters.mutable_iterated_local_search_parameters();

  for (const auto& [name, acceptance_policy] :
       std::vector<std::pair<std::string, AcceptancePolicy*>>{
           {"best", ils->mutable_best_solution_acceptance_policy()},
           {"reference",
            ils->mutable_reference_solution_acceptance_policy()}}) {
    acceptance_policy->clear_strategies();
    EXPECT_THAT(
        FindErrorsInRoutingSearchParameters(parameters),
        Contains(HasSubstr(name + "_solution_acceptance_policy.strategies")));

    // But we should not raise errors if there is at least one strategy.
    acceptance_policy->add_strategies()->mutable_greedy_descent();
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       IlsParametersAcceptancePolicyCompositionRuleMultipleStrategiesTest) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);

  IteratedLocalSearchParameters* ils =
      parameters.mutable_iterated_local_search_parameters();

  for (const auto& [name, acceptance_policy] :
       std::vector<std::pair<std::string, AcceptancePolicy*>>{
           {"best", ils->mutable_best_solution_acceptance_policy()},
           {"reference",
            ils->mutable_reference_solution_acceptance_policy()}}) {
    // Remove any default acceptance strategies.
    acceptance_policy->clear_strategies();

    acceptance_policy->add_strategies()->mutable_greedy_descent();
    acceptance_policy->add_strategies()->mutable_all_nodes_performed();
    acceptance_policy->set_composition(AcceptancePolicy::UNSET);
    EXPECT_THAT(
        FindErrorsInRoutingSearchParameters(parameters),
        Contains(HasSubstr(
            name +
            "_solution_acceptance_policy.composition` is unset but there "
            "are 2 `strategies`")));

    // But we should not raise errors if the composition rule is set.
    acceptance_policy->set_composition(AcceptancePolicy::OR);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       RuinRecreateParametersRuinStrategiesTest) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);

  IteratedLocalSearchParameters* ils =
      parameters.mutable_iterated_local_search_parameters();
  ils->set_perturbation_strategy(PerturbationStrategy::RUIN_AND_RECREATE);
  RuinRecreateParameters* rr = ils->mutable_ruin_recreate_parameters();

  // An empty list of ruin strategies is not supported.
  rr->clear_ruin_strategies();
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
              Contains(AllOf(HasSubstr("iterated_local_search_parameters"),
                             HasSubstr("ruin_recreate_parameters"),
                             HasSubstr("ruin_strategies"))));

  // A single ruin strategy with an unset composition strategy should not raise
  // any error.
  rr->add_ruin_strategies()
      ->mutable_spatially_close_routes()
      ->set_num_ruined_routes(2);
  rr->set_ruin_composition_strategy(RuinCompositionStrategy::UNSET);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());

  // When several ruin strategies are defined, the ruin composition strategy
  // must also be defined.
  rr->add_ruin_strategies()->mutable_random_walk()->set_num_removed_visits(10);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
              Contains(HasSubstr("ruin_composition_strategy")));

  // Setting the composition strategy should fix the issue.
  rr->set_ruin_composition_strategy(RuinCompositionStrategy::RUN_ONE_RANDOMLY);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());
}

TEST_F(RoutingSearchParametersValidationTest,
       RuinRecreateParametersRecreateStrategyTest_UnsetHeuristic) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);

  IteratedLocalSearchParameters* ils =
      parameters.mutable_iterated_local_search_parameters();
  ils->set_perturbation_strategy(PerturbationStrategy::RUIN_AND_RECREATE);
  RuinRecreateParameters* rr = ils->mutable_ruin_recreate_parameters();

  RecreateStrategy* recreate_strategy = rr->mutable_recreate_strategy();

  // RecreateStrategy::UNSET is not supported.
  recreate_strategy->set_heuristic(FirstSolutionStrategy::UNSET);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
              Contains(HasSubstr("recreate_strategy")));

  // But we should not raise any error after clearing the ILS parameters.
  parameters.clear_iterated_local_search_parameters();
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());
}

TEST_F(RoutingSearchParametersValidationTest,
       RuinRecreateParametersRecreateStrategyTest_UnsetParameters) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);

  IteratedLocalSearchParameters* ils =
      parameters.mutable_iterated_local_search_parameters();
  ils->set_perturbation_strategy(PerturbationStrategy::RUIN_AND_RECREATE);
  RuinRecreateParameters* rr = ils->mutable_ruin_recreate_parameters();

  RecreateStrategy* recreate_strategy = rr->mutable_recreate_strategy();

  // RecreateParameters::PARAMETERS_NOT_SET is not supported.
  recreate_strategy->mutable_parameters();
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
              Contains(AllOf(HasSubstr("recreate_strategy.parameters"),
                             HasSubstr("PARAMETERS_NOT_SET"))));

  // But we should not raise any error after clearing the ILS parameters.
  parameters.clear_iterated_local_search_parameters();
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());
}

void SetupDefaultRecreateParameters(
    RecreateStrategy* recreate_strategy,
    RecreateParameters::ParametersCase parameters_case) {
  switch (parameters_case) {
    case RecreateParameters::kLocalCheapestInsertion: {
      LocalCheapestInsertionParameters* lci_params =
          recreate_strategy->mutable_parameters()
              ->mutable_local_cheapest_insertion();
      lci_params->set_pickup_delivery_strategy(
          LocalCheapestInsertionParameters::BEST_PICKUP_DELIVERY_PAIR);
      lci_params->add_insertion_sorting_properties(
          LocalCheapestInsertionParameters::
              SORTING_PROPERTY_HIGHEST_DIMENSION_USAGE);
    } break;
    case RecreateParameters::kSavings: {
      SavingsParameters* savings_params =
          recreate_strategy->mutable_parameters()->mutable_savings();
      savings_params->set_arc_coefficient(1);
      savings_params->set_neighbors_ratio(1);
      savings_params->set_max_memory_usage_bytes(6e9);
      savings_params->set_add_reverse_arcs(true);
    } break;
    case RecreateParameters::kGlobalCheapestInsertion: {
      GlobalCheapestInsertionParameters* gci_params =
          recreate_strategy->mutable_parameters()
              ->mutable_global_cheapest_insertion();
      gci_params->set_farthest_seeds_ratio(0.1);
      gci_params->set_neighbors_ratio(1);
      gci_params->set_min_neighbors(1);
      gci_params->set_use_neighbors_ratio_for_initialization(true);
      gci_params->set_add_unperformed_entries(true);
    } break;
    default:
      DLOG(FATAL) << "Unsupported RecreateParameters::ParametersCase: "
                  << GetRecreateParametersName(parameters_case);
      break;
  }
}

TEST_F(
    RoutingSearchParametersValidationTest,
    RuinRecreateParametersRecreateStrategyTest_HeuristicMatchingAndNotMatchingParameters) {  // NOLINT
  for (const FirstSolutionStrategy::Value recreate_heuristic :
       google::protobuf::contrib::utils::EnumerateEnumValues<
           FirstSolutionStrategy::Value>()) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    SetRecommendedIteratedLocalSearchParameters(parameters);

    IteratedLocalSearchParameters* ils =
        parameters.mutable_iterated_local_search_parameters();
    ils->set_perturbation_strategy(PerturbationStrategy::RUIN_AND_RECREATE);
    RuinRecreateParameters* rr = ils->mutable_ruin_recreate_parameters();

    RecreateStrategy* recreate_strategy = rr->mutable_recreate_strategy();

    // UNSET is not a supported recreate heuristic.
    if (recreate_heuristic == FirstSolutionStrategy::UNSET) continue;
    recreate_strategy->set_heuristic(recreate_heuristic);

    for (const RecreateParameters::ParametersCase parameters_case :
         GetSupportedRecreateParametersCases()) {
      SetupDefaultRecreateParameters(recreate_strategy, parameters_case);

      const std::vector<std::string> errors =
          FindErrorsInRoutingSearchParameters(parameters);

      if (GetParameterCaseForRecreateHeuristic(recreate_heuristic) ==
          parameters_case) {
        EXPECT_THAT(errors, IsEmpty());
      } else {
        // Mismatches between the recreate strategy and the parameters should be
        // detected.
        EXPECT_THAT(errors,
                    ElementsAre(HasSubstr(absl::StrCat(
                        "`recreate_strategy.heuristic` is set to ",
                        FirstSolutionStrategy::Value_Name(recreate_heuristic),
                        " but `recreate_strategy.parameters` define ",
                        GetRecreateParametersName(parameters_case)))));
      }
    }
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       RuinRecreateParametersRecreateStrategyTest_DefaultParameters) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);

  IteratedLocalSearchParameters* ils =
      parameters.mutable_iterated_local_search_parameters();
  ils->set_perturbation_strategy(PerturbationStrategy::RUIN_AND_RECREATE);
  RuinRecreateParameters* rr = ils->mutable_ruin_recreate_parameters();

  RecreateStrategy* recreate_strategy = rr->mutable_recreate_strategy();

  const auto* descriptor = FirstSolutionStrategy::Value_descriptor();
  // No error if the heuristic is set but parameters are missing.
  for (int i = 0; i < descriptor->value_count(); ++i) {
    const FirstSolutionStrategy::Value heuristic =
        static_cast<FirstSolutionStrategy::Value>(
            descriptor->value(i)->number());
    if (heuristic == FirstSolutionStrategy::UNSET) continue;
    recreate_strategy->set_heuristic(heuristic);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());
  }
}

TEST_F(
    RoutingSearchParametersValidationTest,
    RuinRecreateParametersRecreateStrategyTest_LocalCheapestInsertionParameters) {  // NOLINT
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);

  IteratedLocalSearchParameters* ils =
      parameters.mutable_iterated_local_search_parameters();
  ils->set_perturbation_strategy(PerturbationStrategy::RUIN_AND_RECREATE);
  RuinRecreateParameters* rr = ils->mutable_ruin_recreate_parameters();

  RecreateStrategy* recreate_strategy = rr->mutable_recreate_strategy();

  LocalCheapestInsertionParameters* lci_parameters =
      recreate_strategy->mutable_parameters()
          ->mutable_local_cheapest_insertion();
  lci_parameters->set_pickup_delivery_strategy(
      LocalCheapestInsertionParameters::BEST_PICKUP_DELIVERY_PAIR);
  lci_parameters->add_insertion_sorting_properties(
      LocalCheapestInsertionParameters::SORTING_PROPERTY_RANDOM);

  for (const FirstSolutionStrategy::Value heuristic :
       {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
        FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION}) {
    recreate_strategy->set_heuristic(heuristic);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       RuinRecreateParametersRecreateStrategyTest_SavingsParameters) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);

  IteratedLocalSearchParameters* ils =
      parameters.mutable_iterated_local_search_parameters();
  ils->set_perturbation_strategy(PerturbationStrategy::RUIN_AND_RECREATE);
  RuinRecreateParameters* rr = ils->mutable_ruin_recreate_parameters();

  RecreateStrategy* recreate_strategy = rr->mutable_recreate_strategy();
  SetupDefaultRecreateParameters(recreate_strategy,
                                 RecreateParameters::kSavings);

  for (const FirstSolutionStrategy::Value heuristic :
       {FirstSolutionStrategy::SAVINGS,
        FirstSolutionStrategy::PARALLEL_SAVINGS}) {
    recreate_strategy->set_heuristic(heuristic);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       SpatiallyCloseRoutesRuinStrategyTest) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);

  IteratedLocalSearchParameters* ils =
      parameters.mutable_iterated_local_search_parameters();
  ils->set_perturbation_strategy(PerturbationStrategy::RUIN_AND_RECREATE);

  RuinRecreateParameters* rr = ils->mutable_ruin_recreate_parameters();
  // For the spatially close routes ruin strategy, we ask for a positive number
  // of ruined routes.
  rr->add_ruin_strategies()
      ->mutable_spatially_close_routes()
      ->set_num_ruined_routes(0);

  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
              Contains(HasSubstr("spatially_close_routes.num_ruined_routes")));

  // But we should not raise any error after clearing the ILS parameters.
  parameters.clear_iterated_local_search_parameters();
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());
}

TEST_F(RoutingSearchParametersValidationTest, RandomWalkRuinStrategyTest) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);

  IteratedLocalSearchParameters* ils =
      parameters.mutable_iterated_local_search_parameters();
  ils->set_perturbation_strategy(PerturbationStrategy::RUIN_AND_RECREATE);

  RuinRecreateParameters* rr = ils->mutable_ruin_recreate_parameters();
  // For the random walk ruin strategy, we ask for a positive number of
  // removed visits.
  rr->add_ruin_strategies()->mutable_random_walk()->set_num_removed_visits(0);

  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
              Contains(HasSubstr("random_walk.num_removed_visits")));

  // But we should not raise any error after clearing the ILS parameters.
  parameters.clear_iterated_local_search_parameters();
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());
}

TEST_F(RoutingSearchParametersValidationTest, SISRRuinStrategyTest) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);

  IteratedLocalSearchParameters* ils =
      parameters.mutable_iterated_local_search_parameters();
  ils->set_perturbation_strategy(PerturbationStrategy::RUIN_AND_RECREATE);

  RuinRecreateParameters* rr = ils->mutable_ruin_recreate_parameters();
  // Remove any default ruin strategies.
  rr->clear_ruin_strategies();
  SISRRuinStrategy* sisr = rr->add_ruin_strategies()->mutable_sisr();

  sisr->set_avg_num_removed_visits(0);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
              Contains(HasSubstr("sisr.avg_num_removed_visits")));
  // Set a valid value.
  sisr->set_avg_num_removed_visits(10);

  sisr->set_max_removed_sequence_size(0);
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
              Contains(HasSubstr("sisr.max_removed_sequence_size")));
  // Set a valid value.
  sisr->set_max_removed_sequence_size(10);

  for (const double invalid_value : {kNaN, -kEpsilon, 1 + kEpsilon}) {
    sisr->set_bypass_factor(invalid_value);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
                Contains(HasSubstr("sisr.bypass_factor")));
  }

  for (const double valid_value : {0.0, 0.5, 1.0}) {
    sisr->set_bypass_factor(valid_value);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());
  }
}

TEST_F(RoutingSearchParametersValidationTest, RouteSelectionNeighborsTest) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);

  IteratedLocalSearchParameters* ils =
      parameters.mutable_iterated_local_search_parameters();
  ils->set_perturbation_strategy(PerturbationStrategy::RUIN_AND_RECREATE);

  std::vector<std::function<void(RuinRecreateParameters*)>> ruin_setters;
  ruin_setters.emplace_back([](RuinRecreateParameters* rr) {
    ASSERT_EQ(rr->ruin_strategies_size(), 1);
    rr->mutable_ruin_strategies(0)
        ->mutable_spatially_close_routes()
        ->set_num_ruined_routes(2);
  });
  ruin_setters.emplace_back([](RuinRecreateParameters* rr) {
    ASSERT_EQ(rr->ruin_strategies_size(), 1);
    rr->mutable_ruin_strategies(0)
        ->mutable_random_walk()
        ->set_num_removed_visits(2);
  });

  // When using a ruin and recreate perturbation, we ask route selection
  // neighbors parameters to be properly defined.
  for (const auto& ruin_setter : ruin_setters) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    SetRecommendedIteratedLocalSearchParameters(parameters);

    IteratedLocalSearchParameters* ils =
        parameters.mutable_iterated_local_search_parameters();
    ils->set_perturbation_strategy(PerturbationStrategy::RUIN_AND_RECREATE);

    RuinRecreateParameters* rr = ils->mutable_ruin_recreate_parameters();

    ruin_setter(rr);

    // The min number of neighbors must be positive.
    rr->set_route_selection_min_neighbors(0);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
                Contains(HasSubstr("route_selection_min_neighbors")));

    // Reset a valid min number of neighbors.
    rr->set_route_selection_min_neighbors(1);

    // Invalid values for the ratio.
    for (const double ratio :
         {kNaN, -kEpsilon, -1.0, 1 + kEpsilon, kInfinity}) {
      rr->set_route_selection_neighbors_ratio(ratio);
      EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
                  Contains(HasSubstr("route_selection_neighbors_ratio")));
    }

    // Valid limit values for the ratio.
    for (const double ratio : {kEpsilon, 1.0}) {
      rr->set_route_selection_neighbors_ratio(ratio);
      EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());
    }

    // Inverted min and max values.
    rr->set_route_selection_min_neighbors(100);
    rr->set_route_selection_max_neighbors(99);
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters),
                Contains(AllOf(HasSubstr("iterated_local_search_parameters"),
                               HasSubstr("ruin_recreate_parameters"),
                               HasSubstr("route_selection_min_neighbors"),
                               HasSubstr("route_selection_max_neighbors"))));

    // But we should not raise any error after clearing the ILS parameters.
    parameters.clear_iterated_local_search_parameters();
    EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());
  }
}

TEST_F(RoutingSearchParametersValidationTest,
       MissingRuinRecreateParametersTest) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  SetRecommendedIteratedLocalSearchParameters(parameters);

  IteratedLocalSearchParameters* ils =
      parameters.mutable_iterated_local_search_parameters();
  ils->set_perturbation_strategy(PerturbationStrategy::RUIN_AND_RECREATE);
  ils->clear_ruin_recreate_parameters();

  // Empty RuinRecreateParameters should raise an error.
  ils->clear_ruin_recreate_parameters();
  EXPECT_THAT(FindErrorInRoutingSearchParameters(parameters),
              HasSubstr("`ruin_recreate_parameters` is missing"));

  // But we should not raise any error after clearing the ILS parameters.
  parameters.clear_iterated_local_search_parameters();
  EXPECT_THAT(FindErrorsInRoutingSearchParameters(parameters), IsEmpty());
}

}  // namespace
}  // namespace operations_research::routing

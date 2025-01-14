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

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;

CpModelProto CreateExactlyOneTrueBooleanCpModel(int size) {
  CpModelProto model_proto;
  auto* exactly_one_constraint =
      model_proto.add_constraints()->mutable_exactly_one();
  DecisionStrategyProto* const search_strategy =
      model_proto.add_search_strategy();

  for (int i = 0; i < size; ++i) {
    IntegerVariableProto* const var = model_proto.add_variables();
    var->add_domain(0);
    var->add_domain(1);
    exactly_one_constraint->add_literals(i);
    search_strategy->add_variables(i);
  }
  return model_proto;
}

TEST(RandomSearchTest, CheckDistribution) {
  const int kSize = 50;
  std::vector<int> winners(kSize, 0);
  const int kLoops = 100;
  for (int l = 0; l < kLoops; ++l) {
    const CpModelProto model_proto = CreateExactlyOneTrueBooleanCpModel(kSize);
    Model model;
    SatParameters parameters;
    parameters.set_search_random_variable_pool_size(10);
    parameters.set_cp_model_presolve(false);
    parameters.set_search_branching(SatParameters::FIXED_SEARCH);
    parameters.set_random_seed(l);
    parameters.set_num_workers(1);
    model.Add(NewSatParameters(parameters));
    const CpSolverResponse response = SolveCpModel(model_proto, &model);
    for (int i = 0; i < kSize; ++i) {
      if (response.solution(i)) {
        winners[i]++;
      }
    }
  }
  for (int i = 0; i < kSize; ++i) {
    EXPECT_LE(winners[i], kLoops / 10);
  }
}

TEST(RandomSearchTest, CheckSeed) {
  const int kSeeds = 10;
  for (int seed = 0; seed < kSeeds; ++seed) {
    const int kSize = 20;
    std::vector<int> winners(kSize, 0);
    const int kLoops = 50;
    for (int l = 0; l < kLoops; ++l) {
      const CpModelProto model_proto =
          CreateExactlyOneTrueBooleanCpModel(kSize);

      SatParameters params;
      params.set_randomize_search(true);
      params.set_cp_model_presolve(false);
      params.set_search_branching(SatParameters::FIXED_SEARCH);
      params.set_use_absl_random(false);  // Otherwise, each solve changes.
      params.set_random_seed(0);
      params.set_num_workers(1);
      const CpSolverResponse response =
          SolveWithParameters(model_proto, params);
      for (int i = 0; i < kSize; ++i) {
        if (response.solution(i)) {
          winners[i]++;
        }
      }
    }
    for (int i = 0; i < kSize; ++i) {
      EXPECT_TRUE(winners[i] == 0 || winners[i] == kLoops) << winners[i];
    }
  }
}

TEST(BasicFixedSearchBehaviorTest, Default) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 4, 50 ] }
    variables { domain: [ 3, 7 ] }
    variables { domain: [ 0, 7 ] }
    variables { domain: [ 4, 5 ] }
    variables { domain: [ 3, 9 ] }
    constraints {
      all_diff {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        exprs { vars: 3 coeffs: 1 }
        exprs { vars: 4 coeffs: 1 }
      }
    }
  )pb");
  Model model;
  model.Add(NewSatParameters(
      "cp_model_presolve:false,search_branching:FIXED_SEARCH,num_workers:1"));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_THAT(response.solution(), testing::ElementsAre(4, 3, 0, 5, 6));
}

TEST(BasicFixedSearchBehaviorTest, ReverseOrder) {
  // Note that SELECT_LOWER_HALF or SELECT_MIN_VALUE result in the same
  // solution.
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 4, 50 ] }
    variables { domain: [ 3, 7 ] }
    variables { domain: [ 0, 7 ] }
    variables { domain: [ 4, 5 ] }
    variables { domain: [ 3, 9 ] }
    constraints {
      all_diff {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        exprs { vars: 3 coeffs: 1 }
        exprs { vars: 4 coeffs: 1 }
      }
    }
    search_strategy {
      variables: [ 4, 3, 2, 1, 0 ]
      variable_selection_strategy: CHOOSE_FIRST
      domain_reduction_strategy: SELECT_LOWER_HALF
    }
  )pb");
  Model model;
  model.Add(NewSatParameters(
      "cp_model_presolve:false,search_branching:FIXED_SEARCH,num_workers:1"));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_THAT(response.solution(), testing::ElementsAre(6, 5, 0, 4, 3));
}

// The strategies that sort variables according to their domain do not have
// a fixed solution depending on the propagation strength...
TEST(BasicFixedSearchBehaviorTest, MinDomainSize) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 4, 10 ] }
    variables { domain: [ 3, 7 ] }
    variables { domain: [ 0, 7 ] }
    variables { domain: [ 4, 5 ] }
    variables { domain: [ 3, 9 ] }
    constraints {
      all_diff {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        exprs { vars: 3 coeffs: 1 }
        exprs { vars: 4 coeffs: 1 }
      }
    }
    search_strategy {
      variables: [ 0, 1, 2, 3, 4 ]
      variable_selection_strategy: CHOOSE_MIN_DOMAIN_SIZE
      domain_reduction_strategy: SELECT_MAX_VALUE
    }
  )pb");
  Model model;
  model.Add(NewSatParameters(
      "cp_model_presolve:false,search_branching:FIXED_SEARCH,num_workers:1"));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_THAT(response.solution(), testing::ElementsAre(10, 7, 6, 5, 9));
}

TEST(BasicFixedSearchBehaviorTest, WithTransformation1) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 3, 10 ] }
    variables { domain: [ 3, 7 ] }
    constraints {
      all_diff {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
    search_strategy {
      exprs { vars: 0 coeffs: 1 offset: 4 }
      exprs { vars: 1 coeffs: 4 }
      variable_selection_strategy: CHOOSE_LOWEST_MIN
      domain_reduction_strategy: SELECT_MIN_VALUE
    }
  )pb");
  Model model;
  model.Add(NewSatParameters(
      "cp_model_presolve:false,search_branching:FIXED_SEARCH,num_workers:1"));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_THAT(response.solution(), testing::ElementsAre(3, 4));
}

TEST(BasicFixedSearchBehaviorTest, WithTransformation2) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 3, 7 ] }
    variables { domain: [ 3, 7 ] }
    constraints {
      all_diff {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
    search_strategy {
      exprs { vars: 0 coeffs: -1 offset: 4 }
      exprs { vars: 1 coeffs: -4 }
      variable_selection_strategy: CHOOSE_LOWEST_MIN
      domain_reduction_strategy: SELECT_MIN_VALUE
    }
  )pb");
  Model model;
  model.Add(NewSatParameters(
      "cp_model_presolve:false,search_branching:FIXED_SEARCH,num_workers:1"));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_THAT(response.solution(), testing::ElementsAre(6, 7));
}

TEST(BasicFixedSearchBehaviorTest, MedianTest) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 0, 8 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 8, 100 ]
      }
    }
    search_strategy {
      variables: [ 0, 1 ]
      variable_selection_strategy: CHOOSE_FIRST
      domain_reduction_strategy: SELECT_MEDIAN_VALUE
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  params.set_search_branching(SatParameters::FIXED_SEARCH);
  params.set_num_workers(1);
  const CpSolverResponse response = SolveWithParameters(model_proto, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_THAT(response.solution(), testing::ElementsAre(4, 6));
}

TEST(BasicFixedSearchBehaviorTest, MedianTest2) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 6, 12 ] }
    constraints {
      all_diff {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
    search_strategy {
      variables: [ 0, 1 ]
      variable_selection_strategy: CHOOSE_MAX_DOMAIN_SIZE
      domain_reduction_strategy: SELECT_MEDIAN_VALUE
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  params.set_search_branching(SatParameters::FIXED_SEARCH);
  params.set_num_workers(1);
  const CpSolverResponse response = SolveWithParameters(model_proto, params);

  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_THAT(response.solution(), testing::ElementsAre(10, 8));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research

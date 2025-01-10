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

#include "ortools/sat/rins.h"

#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_loader.h"
#include "ortools/sat/model.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/random_engine.h"

namespace operations_research {
namespace sat {

namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;

TEST(GetRinsRensNeighborhoodTest, GetRENSNeighborhood) {
  CpModelProto proto = ParseTestProto(R"pb(
    variables { name: 'x' domain: 0 domain: 10 }
    variables { name: 'y' domain: 0 domain: 10 }
    objective {
      vars: [ 0, 1 ]
      coeffs: [ 1, 1 ]
    }
  )pb");

  Model model;
  LoadVariables(proto, /*view_all_booleans_as_integers=*/true, &model);

  SharedLPSolutionRepository lp_solutions(/*num_solutions_to_keep=*/1);
  SharedIncompleteSolutionManager incomplete_solutions;

  // No solutions are recorded.
  random_engine_t random;
  const ReducedDomainNeighborhood empty_rins_neighborhood =
      GetRinsRensNeighborhood(
          /*response_manager=*/nullptr, &lp_solutions, &incomplete_solutions,
          /*difficulty=*/1.0, random);

  EXPECT_EQ(empty_rins_neighborhood.reduced_domain_vars.size(), 0);
  EXPECT_EQ(empty_rins_neighborhood.fixed_vars.size(), 0);

  // Add a lp solution.
  lp_solutions.NewLPSolution({3.5, 5.0});
  lp_solutions.Synchronize();

  const ReducedDomainNeighborhood rins_neighborhood = GetRinsRensNeighborhood(
      /*response_manager=*/nullptr, &lp_solutions, &incomplete_solutions,
      /*difficulty=*/0.5, random);

  EXPECT_EQ(rins_neighborhood.reduced_domain_vars.size(), 1);
  EXPECT_EQ(rins_neighborhood.reduced_domain_vars[0].first, 0);
  EXPECT_EQ(rins_neighborhood.reduced_domain_vars[0].second.first, 3);
  EXPECT_EQ(rins_neighborhood.reduced_domain_vars[0].second.second, 4);

  EXPECT_EQ(rins_neighborhood.fixed_vars.size(), 1);
  EXPECT_EQ(rins_neighborhood.fixed_vars[0].first, 1);
  EXPECT_EQ(rins_neighborhood.fixed_vars[0].second, 5);
}

TEST(GetRinsRensNeighborhoodTest, GetRENSNeighborhoodIncomplete) {
  CpModelProto proto = ParseTestProto(R"pb(
    variables { name: 'x' domain: 0 domain: 10 }
    variables { name: 'y' domain: 0 domain: 10 }
    objective {
      vars: [ 0, 1 ]
      coeffs: [ 1, 1 ]
    }
  )pb");

  Model model;
  LoadVariables(proto, /*view_all_booleans_as_integers=*/true, &model);

  SharedLPSolutionRepository lp_solutions(/*num_solutions_to_keep=*/1);
  SharedIncompleteSolutionManager incomplete_solutions;

  // No solutions are recorded.
  random_engine_t random;
  const ReducedDomainNeighborhood empty_rins_neighborhood =
      GetRinsRensNeighborhood(
          /*response_manager=*/nullptr, &lp_solutions, &incomplete_solutions,
          /*difficulty=*/1.0, random);

  EXPECT_EQ(empty_rins_neighborhood.reduced_domain_vars.size(), 0);
  EXPECT_EQ(empty_rins_neighborhood.fixed_vars.size(), 0);

  // Add a incomplete solution.
  incomplete_solutions.AddSolution({4.0, 5.0});

  const ReducedDomainNeighborhood rins_neighborhood = GetRinsRensNeighborhood(
      /*response_manager=*/nullptr, &lp_solutions, &incomplete_solutions,
      /*difficulty=*/0.0, random);

  EXPECT_EQ(rins_neighborhood.fixed_vars.size(), 2);
  const int pos_0 = rins_neighborhood.fixed_vars[0].first == 0 ? 0 : 1;
  const int pos_1 = 1 - pos_0;
  EXPECT_EQ(rins_neighborhood.fixed_vars[pos_0].first, 0);
  EXPECT_EQ(rins_neighborhood.fixed_vars[pos_0].second, 4);

  EXPECT_EQ(rins_neighborhood.fixed_vars[pos_1].first, 1);
  EXPECT_EQ(rins_neighborhood.fixed_vars[pos_1].second, 5);
}

TEST(GetRinsRensNeighborhoodTest, GetRinsRensNeighborhoodLP) {
  const CpModelProto proto = ParseTestProto(R"pb(
    variables { name: 'x' domain: 0 domain: 10 }
    variables { name: 'y' domain: 0 domain: 10 }
    objective {
      vars: [ 0, 1 ]
      coeffs: [ 1, 1 ]
    }
  )pb");

  Model model;
  LoadVariables(proto, /*view_all_booleans_as_integers=*/true, &model);

  auto* shared_response_manager = model.GetOrCreate<SharedResponseManager>();
  shared_response_manager->InitializeObjective(proto);
  SharedLPSolutionRepository lp_solutions(/*num_solutions_to_keep=*/1);
  SharedIncompleteSolutionManager incomplete_solutions;

  // No solutions are recorded.
  random_engine_t random;
  const ReducedDomainNeighborhood empty_rins_neighborhood =
      GetRinsRensNeighborhood(shared_response_manager, &lp_solutions,
                              &incomplete_solutions,
                              /*difficulty=*/0.5, random);

  EXPECT_EQ(empty_rins_neighborhood.reduced_domain_vars.size(), 0);
  EXPECT_EQ(empty_rins_neighborhood.fixed_vars.size(), 0);

  // Add a lp solution.
  lp_solutions.NewLPSolution({3.5, 5});
  lp_solutions.Synchronize();
  // Add a solution.
  CpSolverResponse solution;
  solution.add_solution(4);
  solution.add_solution(5);
  shared_response_manager->NewSolution(solution.solution(),
                                       solution.solution_info(), &model);
  shared_response_manager->MutableSolutionsRepository()->Synchronize();

  const ReducedDomainNeighborhood rins_neighborhood = GetRinsRensNeighborhood(
      shared_response_manager, &lp_solutions, &incomplete_solutions,
      /*difficulty=*/0.5, random);

  EXPECT_EQ(rins_neighborhood.reduced_domain_vars.size(), 0);
  EXPECT_EQ(rins_neighborhood.fixed_vars.size(), 1);
  EXPECT_EQ(rins_neighborhood.fixed_vars[0].first, 1);
  EXPECT_EQ(rins_neighborhood.fixed_vars[0].second, 5);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research

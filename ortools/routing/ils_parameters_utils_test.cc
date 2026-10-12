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

#include "ortools/routing/ils_parameters_utils.h"

#include "absl/log/log.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/proto_enum_utils.h"
#include "ortools/routing/enums.pb.h"
#include "ortools/routing/ils.pb.h"

namespace operations_research::routing {

namespace {

TEST(GetParameterCaseForRecreateHeuristic, GetAllParameterCases) {
  for (const FirstSolutionStrategy::Value recreate_heuristic :
       google::protobuf::contrib::utils::EnumerateEnumValues<
           FirstSolutionStrategy::Value>()) {
    switch (recreate_heuristic) {
      case FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION:
      case FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION:
        EXPECT_EQ(GetParameterCaseForRecreateHeuristic(recreate_heuristic),
                  RecreateParameters::kLocalCheapestInsertion);
        break;
      case FirstSolutionStrategy::SAVINGS:
      case FirstSolutionStrategy::PARALLEL_SAVINGS:
        EXPECT_EQ(GetParameterCaseForRecreateHeuristic(recreate_heuristic),
                  RecreateParameters::kSavings);
        break;
      case FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION:
      case FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION:
        EXPECT_EQ(GetParameterCaseForRecreateHeuristic(recreate_heuristic),
                  RecreateParameters::kGlobalCheapestInsertion);
        break;
      default:
        // Unsupported recreate heuristics should return PARAMETERS_NOT_SET.
        EXPECT_EQ(GetParameterCaseForRecreateHeuristic(recreate_heuristic),
                  RecreateParameters::PARAMETERS_NOT_SET);
        break;
    }
  }
}

TEST(GetSupportedRecreateParametersCases, GetAllSupportedParameterCases) {
  EXPECT_THAT(
      GetSupportedRecreateParametersCases(),
      ::testing::ElementsAre(RecreateParameters::kLocalCheapestInsertion,
                             RecreateParameters::kSavings,
                             RecreateParameters::kGlobalCheapestInsertion));
}

TEST(GetRecreateParametersName, GetAllRecreateParametersNames) {
  for (const RecreateParameters::ParametersCase parameters_case :
       GetSupportedRecreateParametersCases()) {
    switch (parameters_case) {
      case RecreateParameters::kLocalCheapestInsertion:
        EXPECT_EQ(GetRecreateParametersName(parameters_case),
                  "local_cheapest_insertion");
        break;
      case RecreateParameters::kSavings:
        EXPECT_EQ(GetRecreateParametersName(parameters_case), "savings");
        break;
      case RecreateParameters::kGlobalCheapestInsertion:
        EXPECT_EQ(GetRecreateParametersName(parameters_case),
                  "global_cheapest_insertion");
        break;
      case RecreateParameters::PARAMETERS_NOT_SET:
        DLOG(FATAL) << "Unsupported recreate parameters case.";
        break;
    }
  }
}

}  // namespace

}  // namespace operations_research::routing

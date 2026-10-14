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

#include "ortools/routing/parameters_utils.h"

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/routing/heuristic_parameters.pb.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/util/optional_boolean.pb.h"

namespace operations_research::routing {

namespace {

using ::testing::ElementsAre;

TEST(GetLocalCheapestInsertionSortingProperties, EmptyInsertionProperties) {
  RoutingSearchParameters params = DefaultRoutingSearchParameters();
  LocalCheapestInsertionParameters* lci_params =
      params.mutable_local_cheapest_insertion_parameters();
  lci_params->clear_insertion_sorting_properties();

  const auto properties = GetLocalCheapestInsertionSortingProperties(
      lci_params->insertion_sorting_properties());
  EXPECT_THAT(
      properties,
      ElementsAre(
          LocalCheapestInsertionParameters::SORTING_PROPERTY_ALLOWED_VEHICLES,
          LocalCheapestInsertionParameters::SORTING_PROPERTY_PENALTY));
}

TEST(GetLocalCheapestInsertionSortingProperties, CustomInsertionProperties) {
  RoutingSearchParameters params = DefaultRoutingSearchParameters();
  LocalCheapestInsertionParameters* lci_params =
      params.mutable_local_cheapest_insertion_parameters();
  lci_params->clear_insertion_sorting_properties();
  lci_params->add_insertion_sorting_properties(
      LocalCheapestInsertionParameters::
          SORTING_PROPERTY_PENALTY_OVER_ALLOWED_VEHICLES_RATIO);

  const auto properties = GetLocalCheapestInsertionSortingProperties(
      lci_params->insertion_sorting_properties());
  EXPECT_THAT(
      properties,
      ElementsAre(LocalCheapestInsertionParameters::
                      SORTING_PROPERTY_PENALTY_OVER_ALLOWED_VEHICLES_RATIO));
}

TEST(DisableAllLocalSearchOperators, AllOperatorsDisabled) {
  RoutingSearchParameters params = DefaultRoutingSearchParameters();
  RoutingSearchParameters::LocalSearchNeighborhoodOperators* operators =
      params.mutable_local_search_operators();
  DisableAllLocalSearchOperators(operators);

  const auto* reflection = operators->GetReflection();
  const auto* descriptor = operators->GetDescriptor();

  for (int field_index = 0; field_index < descriptor->field_count();
       ++field_index) {
    const auto* field = descriptor->field(field_index);
    EXPECT_EQ(reflection->GetEnumValue(*operators, field),
              OptionalBoolean::BOOL_FALSE);
  }
}

}  // namespace

}  // namespace operations_research::routing

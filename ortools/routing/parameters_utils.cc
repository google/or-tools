// Copyright 2010-2024 Google LLC
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

#include <vector>

#include "absl/types/span.h"
#include "ortools/util/optional_boolean.pb.h"

namespace operations_research::routing {

std::vector<RoutingSearchParameters::InsertionSortingProperty>
GetLocalCheapestInsertionSortingProperties(
    absl::Span<const int> lci_insertion_sorting_properties) {
  std::vector<RoutingSearchParameters::InsertionSortingProperty>
      sorting_properties;

  for (const int property : lci_insertion_sorting_properties) {
    sorting_properties.push_back(
        static_cast<RoutingSearchParameters::InsertionSortingProperty>(
            property));
  }

  // For historical reasons if no insertion order is specified, we fallback to
  // selecting nodes with the least number of allowed vehicles first, then the
  // ones with the highest penalty.
  if (sorting_properties.empty()) {
    sorting_properties.push_back(
        RoutingSearchParameters::SORTING_PROPERTY_ALLOWED_VEHICLES);
    sorting_properties.push_back(
        RoutingSearchParameters::SORTING_PROPERTY_PENALTY);
  }
  return sorting_properties;
}

void DisableAllLocalSearchOperators(
    RoutingSearchParameters::LocalSearchNeighborhoodOperators* operators) {
  const auto* reflection = operators->GetReflection();
  const auto* descriptor = operators->GetDescriptor();
  const auto* false_enum =
      OptionalBoolean_descriptor()->FindValueByName("BOOL_FALSE");
  for (int /*this is NOT the field's tag number*/ field_index = 0;
       field_index < descriptor->field_count(); ++field_index) {
    const auto* field = descriptor->field(field_index);
    reflection->SetEnum(operators, field, false_enum);
  }
}

}  // namespace operations_research::routing

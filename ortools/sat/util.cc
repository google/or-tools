// Copyright 2010-2017 Google
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

#include "ortools/sat/util.h"

#include "google/protobuf/descriptor.h"

namespace operations_research {
namespace sat {

void RandomizeDecisionHeuristic(MTRandom* random, SatParameters* parameters) {
  // Random preferred variable order.
  const google::protobuf::EnumDescriptor* order_d =
      SatParameters::VariableOrder_descriptor();
  parameters->set_preferred_variable_order(
      static_cast<SatParameters::VariableOrder>(
          order_d->value(random->Uniform(order_d->value_count()))->number()));

  // Random polarity initial value.
  const google::protobuf::EnumDescriptor* polarity_d =
      SatParameters::Polarity_descriptor();
  parameters->set_initial_polarity(static_cast<SatParameters::Polarity>(
      polarity_d->value(random->Uniform(polarity_d->value_count()))->number()));

  // Other random parameters.
  parameters->set_use_phase_saving(random->OneIn(2));
  parameters->set_random_polarity_ratio(random->OneIn(2) ? 0.01 : 0.0);
  parameters->set_random_branches_ratio(random->OneIn(2) ? 0.01 : 0.0);
}

}  // namespace sat
}  // namespace operations_research

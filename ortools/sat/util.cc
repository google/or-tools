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

#if !defined(__PORTABLE_PLATFORM__)
#include "google/protobuf/descriptor.h"
#endif  // __PORTABLE_PLATFORM__

namespace operations_research {
namespace sat {

void RandomizeDecisionHeuristic(MTRandom* random, SatParameters* parameters) {
#if !defined(__PORTABLE_PLATFORM__)
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
#endif  // __PORTABLE_PLATFORM__
  // Other random parameters.
  parameters->set_use_phase_saving(random->OneIn(2));
  parameters->set_random_polarity_ratio(random->OneIn(2) ? 0.01 : 0.0);
  parameters->set_random_branches_ratio(random->OneIn(2) ? 0.01 : 0.0);
}

int MoveOneUnprocessedLiteralLast(const std::set<LiteralIndex>& processed,
                                  int relevant_prefix_size,
                                  std::vector<Literal>* literals) {
  if (literals->empty()) return -1;
  if (!ContainsKey(processed, literals->back().Index())) {
    return std::min<int>(relevant_prefix_size, literals->size());
  }

  // To get O(n log n) size of suffixes, we will first process the last n/2
  // literals, we then move all of them first and process the n/2 literals left.
  // We use the same algorithm recursively. The sum of the suffixes' size S(n)
  // is thus S(n/2) + n + S(n/2). That gives us the correct complexity. The code
  // below simulates one step of this algorithm and is made to be "robust" when
  // from one call to the next, some literals have been removed (but the order
  // of literals is preserved).
  int num_processed = 0;
  int num_not_processed = 0;
  int target_prefix_size = literals->size() - 1;
  for (int i = literals->size() - 1; i >= 0; i--) {
    if (ContainsKey(processed, (*literals)[i].Index())) {
      ++num_processed;
    } else {
      ++num_not_processed;
      target_prefix_size = i;
    }
    if (num_not_processed >= num_processed) break;
  }
  if (num_not_processed == 0) return -1;
  target_prefix_size = std::min(target_prefix_size, relevant_prefix_size);

  // Once a prefix size has been decided, it is always better to
  // enqueue the literal already processed first.
  std::stable_partition(
      literals->begin() + target_prefix_size, literals->end(),
      [&processed](Literal l) { return ContainsKey(processed, l.Index()); });
  return target_prefix_size;
}

}  // namespace sat
}  // namespace operations_research

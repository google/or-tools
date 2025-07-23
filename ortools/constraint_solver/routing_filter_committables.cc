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

#include "ortools/constraint_solver/routing_filter_committables.h"

#include "absl/log/check.h"
#include "absl/types/span.h"

namespace operations_research {

bool PropagateTransitAndSpan(int path, DimensionValues& dimension_values) {
  DCHECK_GT(dimension_values.NumNodes(path), 0);
  using Interval = DimensionValues::Interval;
  const absl::Span<Interval> cumuls = dimension_values.MutableCumuls(path);
  const absl::Span<const Interval> transits = dimension_values.Transits(path);
  const int num_nodes = dimension_values.NumNodes(path);
  const Interval span = dimension_values.Span(path);
  // Span -> cumul front/back.
  if (!cumuls.back().IntersectWith(cumuls[0] + span)) return false;
  if (!cumuls[0].IntersectWith(cumuls.back() - span)) return false;
  // Propagate from start to end.
  Interval cumul = cumuls[0];
  for (int t = 0; t < num_nodes - 1; ++t) {
    cumul.Add(transits[t]);
    if (!cumul.IntersectWith(cumuls[t + 1])) return false;
    cumuls[t + 1] = cumul;
  }
  // Propagate span to cumul front, then re-propagate from start to end
  // as long as there are changes.
  cumul = cumuls.back() - span;
  for (int t = 0; t < num_nodes; ++t) {
    if (!cumul.IntersectWith(cumuls[t])) return false;
    if (cumul == cumuls[t]) break;
    cumuls[t] = cumul;
    if (t == num_nodes - 1) break;
    cumul.Add(transits[t]);
  }
  // Propagate from end to start.
  cumul = cumuls.back();
  for (int t = num_nodes - 2; t >= 0; --t) {
    cumul.Subtract(transits[t]);
    if (!cumul.IntersectWith(cumuls[t])) return false;
    cumuls[t] = cumul;
  }
  // Propagate span to cumul back, then re-propagate from end to start
  // as long as there are changes.
  cumul = cumuls[0] + span;
  for (int t = num_nodes - 1; t >= 0; --t) {
    if (!cumul.IntersectWith(cumuls[t])) return false;
    if (cumul == cumuls[t]) break;
    cumuls[t] = cumul;
    if (t == 0) break;
    cumul.Subtract(transits[t - 1]);
  }
  // Cumul front/back -> span.
  if (!dimension_values.MutableSpan(path).IntersectWith(cumuls.back() -
                                                        cumuls[0])) {
    return false;
  }
  return true;
}

}  // namespace operations_research

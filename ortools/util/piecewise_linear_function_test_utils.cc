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

#include "ortools/util/piecewise_linear_function_test_utils.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "ortools/routing/lp_scheduling_test_utils.h"
#include "ortools/util/piecewise_linear_function.h"
#include "ortools/util/random_engine.h"

namespace operations_research {

FloatSlopePiecewiseLinearFunction RandomPiecewiseLinearFunctionFixedNbSegments(
    int64_t x_min, int64_t x_max, int64_t y_min, int64_t y_max,
    int num_segments, random_engine_t& random) {
  DCHECK_LE(num_segments, x_max - x_min);
  DCHECK_LT(y_min, y_max);

  // Special case: nb_segments = 0.
  if (num_segments == 0) {
    const int64_t y = absl::Uniform(random, y_min, y_max);
    return FloatSlopePiecewiseLinearFunction({x_min, x_max}, {y, y});
  }

  // Normal case: nb_segments > 0.
  absl::InlinedVector<int64_t, 8> x_anchors;
  absl::InlinedVector<int64_t, 8> y_anchors;
  x_anchors.reserve(num_segments + 1);
  y_anchors.reserve(num_segments + 1);
  const std::vector<int64_t> x_coordinates =
      routing::RandomDistribution(x_max - x_min, num_segments + 1, random);
  int64_t x_last = 0;
  int64_t y_last = absl::Uniform(random, y_min, y_max);
  x_anchors.push_back(x_min + x_last);
  y_anchors.push_back(y_last);
  for (int k = 1; k < num_segments + 1; ++k) {
    const int x = x_coordinates[k];
    const double dx = x - x_last;
    const double alpha_max = std::min(1.0, (y_max - y_last) / dx);
    const double alpha_min = std::max(-1.0, (y_min - y_last) / dx);
    const double alpha = absl::Uniform(random, alpha_min, alpha_max);
    x_last += dx;
    y_last += alpha * dx;
    x_anchors.push_back(x_min + x_last);
    y_anchors.push_back(y_last);
  }
  return FloatSlopePiecewiseLinearFunction(std::move(x_anchors),
                                           std::move(y_anchors));
}

}  // namespace operations_research

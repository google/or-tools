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

#include "ortools/math_opt/solvers/glpk/gap.h"

#include <cmath>
#include <limits>

namespace operations_research::math_opt {

constexpr double kInf = std::numeric_limits<double>::infinity();

double WorstGLPKDualBound(const bool is_maximize, const double objective_value,
                          const double relative_gap_limit) {
  // With:
  //          |best_objective_value − best_dual_bound|
  //   gap := ----------------------------------------
  //            |best_objective_value| + DBL_EPSILON
  //
  // When gap = relative_gap_limit:
  //
  //                        |best_objective_value − best_dual_bound|
  //   relative_gap_limit = ----------------------------------------
  //                          |best_objective_value| + DBL_EPSILON
  //
  // Thus if we define:
  //
  //   delta := relative_gap_limit * (|best_objective_value| + DBL_EPSILON)
  //
  // By definition:
  // - if we are maximizing: best_objective_value <= best_dual_bound
  // - if we are minimizing: best_dual_bound <= best_objective_value
  //
  // As a consequence:
  // - if we are maximizing:
  //
  //   best_dual_bound = best_objective_value + delta
  //
  // - if we are minimizing:
  //
  //   best_dual_bound = best_objective_value - delta
  //
  // Note that DBL_EPSILON is std::numeric_limits<double>::epsilon().
  if (std::isnan(objective_value)) {
    return objective_value;
  }
  // Note that -kInf and NaN are handled below with std::fmax().
  if (relative_gap_limit == kInf) {
    return is_maximize ? kInf : -kInf;
  }
  if (!std::isfinite(objective_value)) {
    return objective_value;
  }
  // Note that std::fmax() treats NaN as missing value.
  const double non_negative_relative_gap_limit =
      std::fmax(0.0, relative_gap_limit);
  const double delta =
      non_negative_relative_gap_limit *
      (std::abs(objective_value) + std::numeric_limits<double>::epsilon());
  // Note that delta can overflow, leading to infinite values. This is OK though
  // as objective_value is finite; hence objective_value +/- delta will return
  // the corresponding infinite.
  return objective_value + (is_maximize ? 1.0 : -1.0) * delta;
}

}  // namespace operations_research::math_opt

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

#ifndef ORTOOLS_UTIL_PIECEWISE_LINEAR_FUNCTION_TEST_UTILS_H_
#define ORTOOLS_UTIL_PIECEWISE_LINEAR_FUNCTION_TEST_UTILS_H_

#include <cstdint>

#include "ortools/util/piecewise_linear_function.h"
#include "ortools/util/random_engine.h"

namespace operations_research {

// Returns a random FloatSlopePiecewiseLinearFunction f satisfying:
// - y_min <= f(x) <= y_max
// - |f´(x)| < 1 for all x
// - f is defined on [0, x_max]
FloatSlopePiecewiseLinearFunction RandomPiecewiseLinearFunctionFixedNbSegments(
    int64_t x_min, int64_t x_max, int64_t y_min, int64_t y_max,
    int num_segments, random_engine_t& random);

}  // namespace operations_research

#endif  // ORTOOLS_UTIL_PIECEWISE_LINEAR_FUNCTION_TEST_UTILS_H_

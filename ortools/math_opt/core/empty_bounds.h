// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_MATH_OPT_CORE_EMPTY_BOUNDS_H_
#define OR_TOOLS_MATH_OPT_CORE_EMPTY_BOUNDS_H_

#include <cstdint>

#include "ortools/math_opt/result.pb.h"

namespace operations_research::math_opt {

// Returns an "infeasible" result for models where the infeasibility is caused
// by an integer variable whose bounds are nonempty but contain no integers.
//
// Callers should make sure to set the SolveResultProto.solve_stats.solve_time
// field before returning the result.
SolveResultProto ResultForIntegerInfeasible(bool is_maximize,
                                            int64_t bad_variable_id, double lb,
                                            double ub);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CORE_EMPTY_BOUNDS_H_

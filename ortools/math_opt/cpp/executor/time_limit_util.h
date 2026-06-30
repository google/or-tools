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

#ifndef ORTOOLS_MATH_OPT_CPP_EXECUTOR_TIME_LIMIT_UTIL_H_
#define ORTOOLS_MATH_OPT_CPP_EXECUTOR_TIME_LIMIT_UTIL_H_

#include <optional>

#include "absl/time/time.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {

void UpdateTimeLimit(absl::Time absolute_deadline,
                     math_opt::SolveParameters& params);

void UpdateTimeLimit(absl::Duration relative_deadline,
                     math_opt::SolveParameters& params);

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_CPP_EXECUTOR_TIME_LIMIT_UTIL_H_

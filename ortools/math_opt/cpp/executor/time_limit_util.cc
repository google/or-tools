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

#include "ortools/math_opt/cpp/executor/time_limit_util.h"

#include <algorithm>

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {

void UpdateTimeLimit(absl::Time absolute_deadline,
                     math_opt::SolveParameters& params) {
  UpdateTimeLimit(absolute_deadline - absl::Now(), params);
}

void UpdateTimeLimit(absl::Duration relative_deadline,
                     math_opt::SolveParameters& params) {
  params.time_limit = std::min(
      params.time_limit, std::max(absl::ZeroDuration(), relative_deadline));
}

}  // namespace operations_research::math_opt

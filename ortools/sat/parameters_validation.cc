// Copyright 2010-2021 Google LLC
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

#include "ortools/sat/parameters_validation.h"

#include <string>

#include "absl/strings/str_cat.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

#define TEST_IN_RANGE(name, min, max)                                       \
  if (params.name() < min || params.name() > max) {                         \
    return absl::StrCat("parameter '", #name, "' should be in [", min, ",", \
                        max, "]. Current value is ", params.name());        \
  }

std::string ValidateParameters(const SatParameters& params) {
  if (params.max_time_in_seconds() < 0) {
    return "Parameters max_time_in_seconds should be non-negative";
  }

  // TODO(user): Consider using annotations directly in the proto for these
  // validation. It is however not open sourced.
  TEST_IN_RANGE(mip_max_activity_exponent, 1, 62);
  TEST_IN_RANGE(mip_max_bound, 0, 1e17);
  TEST_IN_RANGE(solution_pool_size, 0, std::numeric_limits<int32_t>::max());

  return "";
}

#undef TEST_IN_RANGE

}  // namespace sat
}  // namespace operations_research

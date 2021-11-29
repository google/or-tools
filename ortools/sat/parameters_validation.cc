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

namespace operations_research {
namespace sat {

std::string ValidateParameters(const SatParameters& params) {
  if (params.max_time_in_seconds() < 0) {
    return "Field max_time_in_seconds should be non-negative";
  }
  return "";
}

}  // namespace sat
}  // namespace operations_research

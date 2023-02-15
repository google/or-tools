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

#ifndef OR_TOOLS_SAT_PARAMETERS_VALIDATION_H_
#define OR_TOOLS_SAT_PARAMETERS_VALIDATION_H_

#include <string>

#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

// Verifies that the given parameters are correct. Returns an empty string if it
// is the case, or an human-readable error message otherwise.
std::string ValidateParameters(const SatParameters& params);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_PARAMETERS_VALIDATION_H_

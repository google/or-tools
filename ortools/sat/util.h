// Copyright 2010-2017 Google
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

#ifndef OR_TOOLS_SAT_UTIL_H_
#define OR_TOOLS_SAT_UTIL_H_

#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/base/random.h"

namespace operations_research {
namespace sat {

// Randomizes the decision heuristic of the given SatParameters.
void RandomizeDecisionHeuristic(MTRandom* random, SatParameters* parameters);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_UTIL_H_

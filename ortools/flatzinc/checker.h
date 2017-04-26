// Copyright 2010-2014 Google
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

#ifndef OR_TOOLS_FLATZINC_CHECKER_H_
#define OR_TOOLS_FLATZINC_CHECKER_H_

#include <functional>

#include "ortools/flatzinc/model.h"

namespace operations_research {
namespace fz {

// Verifies that the solution specified by the given evaluator is a
// feasible solution of the given model. Returns true iff this is the
// case.
bool CheckSolution(const Model& model,
                   const std::function<int64(IntegerVariable*)>& evaluator);

}  // namespace fz
}  // namespace operations_research

#endif  // OR_TOOLS_FLATZINC_CHECKER_H_

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

#ifndef OR_TOOLS_FLATZINC_CP_MODEL_FZ_SOLVER_H_
#define OR_TOOLS_FLATZINC_CP_MODEL_FZ_SOLVER_H_

#include "ortools/flatzinc/model.h"
#include "ortools/flatzinc/solver.h"

namespace operations_research {
namespace sat {

void SolveFzWithCpModelProto(const fz::Model& model,
                             const fz::FlatzincParameters& p,
                             bool* interrupt_solve);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_FLATZINC_CP_MODEL_FZ_SOLVER_H_

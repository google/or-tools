// Copyright 2010-2018 Google LLC
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

#include "ortools/linear_solver/sat_solver_utils.h"

#include "absl/memory/memory.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/preprocessor.h"
#include "ortools/lp_data/proto_utils.h"

namespace operations_research {

MPSolverResponseStatus ApplyMipPresolveSteps(
    MPModelProto* model, std::unique_ptr<glop::ShiftVariableBoundsPreprocessor>*
                             shift_bounds_preprocessor) {
  return MPSolverResponseStatus::MPSOLVER_NOT_SOLVED;
}

}  // namespace operations_research

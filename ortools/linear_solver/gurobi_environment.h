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

#if defined(USE_GUROBI)
#ifndef OR_TOOLS_LINEAR_SOLVER_GUROBI_ENVIRONMENT_H_
#define OR_TOOLS_LINEAR_SOLVER_GUROBI_ENVIRONMENT_H_

#include "ortools/base/commandlineflags.h"
#include "ortools/base/status.h"

extern "C" {
#include "gurobi_c.h"
}

namespace operations_research {
util::Status LoadGurobiEnvironment(GRBenv** env);
}

#endif  // OR_TOOLS_LINEAR_SOLVER_GUROBI_ENVIRONMENT_H_
#endif  //  #if defined(USE_GUROBI)

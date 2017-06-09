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

#ifndef OR_TOOLS_SAT_CP_MODEL_CHECKER_H_
#define OR_TOOLS_SAT_CP_MODEL_CHECKER_H_

#include "ortools/base/hash.h"
#include "ortools/base/integral_types.h"
#include "ortools/sat/cp_model.pb.h"

namespace operations_research {
namespace sat {

// Verifies that the given model satisfies all the properties described in the
// proto comments. Returns an empty std::string if it is the case, otherwise fails at
// the first error and returns a human-readable description of the issue.
//
// TODO(user): Add any needed overflow validation.
std::string ValidateCpModel(const CpModelProto& model);

// Verifies that the given variable assignment is a feasible solution of the
// given model. The values vector should be in one to one correspondance with
// the model.variables() list of variables.
bool SolutionIsFeasible(const CpModelProto& model,
                        const std::vector<int64>& variable_values);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_CHECKER_H_

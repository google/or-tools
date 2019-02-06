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

#ifndef OR_TOOLS_SAT_CP_MODEL_EXPAND_H_
#define OR_TOOLS_SAT_CP_MODEL_EXPAND_H_

#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_presolve.h"

namespace operations_research {
namespace sat {

// Expands a given CpModelProto by rewriting complex constraints into
// simpler constraints.
// This is different from PresolveCpModel() as there are no reduction or
// simplification of the model. Furthermore, this expansion is mandatory.
void ExpandCpModel(CpModelProto* working_model, PresolveOptions options);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_EXPAND_H_

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

#ifndef OR_TOOLS_SAT_CP_MODEL_EXPAND_H_
#define OR_TOOLS_SAT_CP_MODEL_EXPAND_H_

#include <vector>

#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/presolve_context.h"

namespace operations_research {
namespace sat {

// Expands a given CpModelProto by rewriting complex constraints into
// simpler constraints.
// This is different from PresolveCpModel() as there are no reduction or
// simplification of the model. Furthermore, this expansion is mandatory.
void ExpandCpModel(PresolveContext* context);

// Linear constraint with a complex rhs need to be expanded at the end of the
// presolve. We do that at the end, because the presolve is allowed to simplify
// such constraints by updating the rhs. Also the extra variable we create are
// only linked by a few constraints to the rest of the model and should not be
// presolvable.
void FinalExpansionForLinearConstraint(PresolveContext* context);

// Fills and propagates the set of reachable states/labels.
void PropagateAutomaton(const AutomatonConstraintProto& proto,
                        const PresolveContext& context,
                        std::vector<absl::flat_hash_set<int64_t>>* states,
                        std::vector<absl::flat_hash_set<int64_t>>* labels);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_EXPAND_H_

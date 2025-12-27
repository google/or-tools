// Copyright 2010-2025 Google LLC
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

#ifndef ORTOOLS_SAT_PRESOLVE_ENCODING_H_
#define ORTOOLS_SAT_PRESOLVE_ENCODING_H_

#include <cstdint>
#include <vector>

#include "ortools/sat/presolve_context.h"

namespace operations_research {
namespace sat {

struct VariableEncodingLocalModel {
  // The integer variable that is encoded. Internally it can be replaced by
  // -1 if some presolve rule removed the variable.
  int var;

  // The linear1 constraint indexes that define conditional bounds on the
  // variable. Those linear1 should have exactly one enforcement literal and
  // satisfy `PositiveRef(enf) != var`. All linear1 restraining `var` and
  // fulfilling the conditions above will appear here.
  std::vector<int> linear1_constraints;

  // Zero if `var` doesn't appear in the objective.
  int64_t variable_coeff_in_objective = 0;

  // Note: the objective doesn't count as a constraint outside the local model.
  bool var_in_more_than_one_constraint_outside_the_local_model;

  // Set to -1 if there is none or if the variable appears in more than one
  // constraint outside the local model.
  int single_constraint_using_the_var_outside_the_local_model = -1;
};

// If we have a bunch of constraint of the form literal => Y \in domain and
// another constraint Y = f(X), we can remove Y, that constraint, and transform
// all linear1 from constraining Y to constraining X.
//
// We can for instance do it for Y = abs(X) or Y = X^2 easily. More complex
// function might be trickier.
//
// Note that we can't always do it in the reverse direction though!
// If we have l => X = -1, we can't transfer that to abs(X) for instance, since
// X=1 will also map to abs(-1). We can only do it if for all implied domain D
// we have f^-1(f(D)) = D, which is not easy to check.
// Returns false if we prove unsat.
bool MaybeTransferLinear1ToAnotherVariable(
    VariableEncodingLocalModel& local_model, PresolveContext* context);

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_PRESOLVE_ENCODING_H_

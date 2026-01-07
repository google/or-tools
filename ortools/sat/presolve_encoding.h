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

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/util/sorted_interval_list.h"

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

  // Constraints of the form bool_or/exactly_one/at_most_one that contains at
  // least two of the encoding booleans.
  std::vector<int> constraints_linking_two_encoding_booleans;

  // Booleans that do not appear on any constraints outside the local model.
  absl::flat_hash_set<int> bools_only_used_inside_the_local_model;

  // Zero if `var` doesn't appear in the objective.
  int64_t variable_coeff_in_objective = 0;

  // Note: the objective doesn't count as a constraint outside the local model.
  bool var_in_more_than_one_constraint_outside_the_local_model;

  // Set to -1 if there is none or if the variable appears in more than one
  // constraint outside the local model.
  int single_constraint_using_the_var_outside_the_local_model = -1;
};

// For performance, this skips variables that appears in a single linear1 and is
// used in more than another constraint, since there is no interesting presolve
// we can do in this case.
std::vector<VariableEncodingLocalModel> CreateVariableEncodingLocalModels(
    PresolveContext* context);

// Do a few simple presolve rules on the local model:
// - restrict the domain of the linear1 to the domain of the variable.
// - merge linear1 over the same enforcement,var pairs.
// - if we have a linear1 for a literal and another for its negation, do
//   not allow both to be true.
//
// Also returns a list of literals that fully encodes a domain for the variable.
// Returns false if we prove unsat.
bool BasicPresolveAndGetFullyEncodedDomains(
    PresolveContext* context, VariableEncodingLocalModel& local_model,
    absl::flat_hash_map<int, Domain>* result, bool* changed);

// If we have a model containing:
//    l1 => var in [0, 10]
//   ~l1 => var in [11, 20]
//    l2 => var in [50, 60]
//   ~l2 => var in [70, 80]
//   bool_or(l1, l2, ...)
//
// if moreover `l1` and `l2` are only used in the constraints above, we can
// replace them by:
//    l3 => var in [0, 10] U [50, 60]
//   ~l3 => var in [11, 20] U [70, 80]
//   bool_or(l3, ...)
//
// and remove the variables `l1` and `l2`. This also works if we replace the
// bool_or for an at_most_one or an exactly_one, but requires imposing
// (unconditionally) that the variable cannot be both in the domain encoded by
// `l1` and in the domain encoded by `l2`.
bool DetectAllEncodedComplexDomain(PresolveContext* context,
                                   VariableEncodingLocalModel& local_model);

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

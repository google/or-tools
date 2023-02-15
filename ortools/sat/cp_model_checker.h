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

#ifndef OR_TOOLS_SAT_CP_MODEL_CHECKER_H_
#define OR_TOOLS_SAT_CP_MODEL_CHECKER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/types/span.h"
#include "ortools/base/integral_types.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

// Verifies that the given model satisfies all the properties described in the
// proto comments. Returns an empty string if it is the case, otherwise fails at
// the first error and returns a human-readable description of the issue.
//
// The extra parameter is internal and mainly for debugging. After the problem
// has been presolved, we have a stricter set of properties we want to enforce.
//
// TODO(user): Add any needed overflow validation because we are far from
// exhaustive. We could also run a small presolve that tighten variable bounds
// before the overflow check to facilitate the lives of our users, but it is a
// some work to put in place.
std::string ValidateCpModel(const CpModelProto& model,
                            bool after_presolve = false);

// Some validation (in particular the floating point objective) requires to
// read parameters.
//
// TODO(user): Ideally we would have just one ValidateCpModel() function but
// this was introduced after many users already use ValidateCpModel() without
// parameters.
std::string ValidateInputCpModel(const SatParameters& params,
                                 const CpModelProto& model);

// Check if a given linear expression can create overflow. It is exposed to test
// new constraints created during the presolve.
bool PossibleIntegerOverflow(const CpModelProto& model,
                             absl::Span<const int> vars,
                             absl::Span<const int64_t> coeffs,
                             int64_t offset = 0);

// Verifies that the given variable assignment is a feasible solution of the
// given model. The values vector should be in one to one correspondence with
// the model.variables() list of variables.
//
// The last two arguments are optional and help debugging a failing constraint
// due to presolve.
bool SolutionIsFeasible(const CpModelProto& model,
                        absl::Span<const int64_t> variable_values,
                        const CpModelProto* mapping_proto = nullptr,
                        const std::vector<int>* postsolve_mapping = nullptr);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_CHECKER_H_

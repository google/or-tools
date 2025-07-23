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

#ifndef OR_TOOLS_LINEAR_SOLVER_GUROBI_UTIL_H_
#define OR_TOOLS_LINEAR_SOLVER_GUROBI_UTIL_H_

#include <string>

#include "absl/flags/declare.h"
#include "absl/status/statusor.h"
#include "ortools/third_party_solvers/gurobi_environment.h"

namespace operations_research {

absl::StatusOr<GRBenv*> GetGurobiEnv();

// This returns true if the Gurobi shared library is properly loaded (otherwise,
// tries to find it and load it) and if a Gurobi license can be obtained (it
// does that by trying to grab a license and then release it).
bool GurobiIsCorrectlyInstalled();

// Returns a human-readable listing of all gurobi parameters that are set to
// non-default values, and their current value in the given environment. If all
// parameters are at their default value, returns the empty string.
// To produce a one-liner string, use `one_liner_output=true`.
std::string GurobiParamInfoForLogging(GRBenv* grb,
                                      bool one_liner_output = false);

}  // namespace operations_research
#endif  // OR_TOOLS_LINEAR_SOLVER_GUROBI_UTIL_H_

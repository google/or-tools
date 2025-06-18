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

#ifndef OR_TOOLS_GUROBI_ISV_PUBLIC_GUROBI_ISV_H_
#define OR_TOOLS_GUROBI_ISV_PUBLIC_GUROBI_ISV_H_

#include <cstdint>
#include <string>

#include "absl/status/statusor.h"
#include "ortools/third_party_solvers/gurobi_environment.h"

namespace operations_research::math_opt {

// An ISV key for the Gurobi solver, an alternative to using a license file.
//
// See http://www.gurobi.com/products/licensing-pricing/isv-program.
struct GurobiIsvKey {
  std::string name;
  std::string application_name;
  // Zero means no expiration.
  int32_t expiration = 0;
  std::string key;
};

// Returns a new primary Gurobi environment initialized with an ISV key.
//
// See http://www.gurobi.com/products/licensing-pricing/isv-program.
absl::StatusOr<GRBenv*> NewPrimaryEnvFromISVKey(const GurobiIsvKey& isv_key);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_GUROBI_ISV_PUBLIC_GUROBI_ISV_H_

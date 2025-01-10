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

#include "ortools/math_opt/solvers/gurobi_init_arguments.h"

#include <optional>
#include <string>

#include "absl/status/statusor.h"
#include "ortools/gurobi/isv_public/gurobi_isv.h"
#include "ortools/math_opt/solvers/gurobi.pb.h"
#include "ortools/math_opt/solvers/gurobi/g_gurobi.h"

namespace operations_research {
namespace math_opt {

absl::StatusOr<GRBenvUniquePtr> NewPrimaryEnvironment(
    std::optional<GurobiInitializerProto::ISVKey> proto_isv_key) {
  std::optional<GurobiIsvKey> isv_key;
  if (proto_isv_key.has_value()) {
    GurobiIsvKey key;
    key.name = proto_isv_key->name();
    key.application_name = proto_isv_key->application_name();
    key.expiration = proto_isv_key->expiration();
    key.key = proto_isv_key->key();
    isv_key = key;
  }
  return GurobiNewPrimaryEnv(isv_key);
}

}  // namespace math_opt
}  // namespace operations_research

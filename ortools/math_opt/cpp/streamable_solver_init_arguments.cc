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

#include "ortools/math_opt/cpp/streamable_solver_init_arguments.h"

#include <optional>

#include "absl/status/statusor.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/solvers/gurobi.pb.h"

namespace operations_research {
namespace math_opt {

GurobiInitializerProto::ISVKey GurobiISVKey::Proto() const {
  GurobiInitializerProto::ISVKey isv_key_proto;
  isv_key_proto.set_name(name);
  isv_key_proto.set_application_name(application_name);
  isv_key_proto.set_expiration(expiration);
  isv_key_proto.set_key(key);
  return isv_key_proto;
}

GurobiISVKey GurobiISVKey::FromProto(
    const GurobiInitializerProto::ISVKey& key_proto) {
  return GurobiISVKey{
      .name = key_proto.name(),
      .application_name = key_proto.application_name(),
      .expiration = key_proto.expiration(),
      .key = key_proto.key(),
  };
}

GurobiInitializerProto StreamableGurobiInitArguments::Proto() const {
  GurobiInitializerProto params_proto;

  if (isv_key) {
    *params_proto.mutable_isv_key() = isv_key->Proto();
  }

  return params_proto;
}

StreamableGurobiInitArguments StreamableGurobiInitArguments::FromProto(
    const GurobiInitializerProto& args_proto) {
  StreamableGurobiInitArguments args;
  if (args_proto.has_isv_key()) {
    args.isv_key = GurobiISVKey::FromProto(args_proto.isv_key());
  }
  return args;
}

SolverInitializerProto StreamableSolverInitArguments::Proto() const {
  SolverInitializerProto params_proto;

  if (gurobi) {
    *params_proto.mutable_gurobi() = gurobi->Proto();
  }

  return params_proto;
}

absl::StatusOr<StreamableSolverInitArguments>
StreamableSolverInitArguments::FromProto(
    const SolverInitializerProto& args_proto) {
  StreamableSolverInitArguments args;
  if (args_proto.has_gurobi()) {
    args.gurobi = StreamableGurobiInitArguments::FromProto(args_proto.gurobi());
  }
  return args;
}

}  // namespace math_opt
}  // namespace operations_research

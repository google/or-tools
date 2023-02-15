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

// IWYU pragma: private, include "ortools/math_opt/cpp/math_opt.h"
// IWYU pragma: friend "ortools/math_opt/cpp/.*"

// This headers defines C++ wrappers of solver specific initialization
// parameters that can be streamed to be exchanged with another process.
//
// Parameters that can't be streamed (for example instances of C/C++ types that
// only exist in the process memory) are dealt with implementations of
// the NonStreamableSolverInitArguments.
#ifndef OR_TOOLS_MATH_OPT_CPP_STREAMABLE_SOLVER_INIT_ARGUMENTS_H_
#define OR_TOOLS_MATH_OPT_CPP_STREAMABLE_SOLVER_INIT_ARGUMENTS_H_

#include <cstdint>
#include <optional>
#include <string>

#include "absl/status/statusor.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/solvers/gurobi.pb.h"

namespace operations_research {
namespace math_opt {

// Streamable Pdlp specific parameters for solver instantiation.
struct StreamablePdlpInitArguments {};

// Streamable CpSat specific parameters for solver instantiation.
struct StreamableCpSatInitArguments {};

// Streamable GScip specific parameters for solver instantiation.
struct StreamableGScipInitArguments {};

// Streamable Glop specific parameters for solver instantiation.
struct StreamableGlopInitArguments {};

// Streamable Glpk specific parameters for solver instantiation.
struct StreamableGlpkInitArguments {};

// An ISV key for the Gurobi solver.
//
// See http://www.gurobi.com/products/licensing-pricing/isv-program.
struct GurobiISVKey {
  std::string name;
  std::string application_name;
  int32_t expiration = 0;
  std::string key;

  GurobiInitializerProto::ISVKey Proto() const;
  static GurobiISVKey FromProto(
      const GurobiInitializerProto::ISVKey& key_proto);
};

// Streamable Gurobi specific parameters for solver instantiation.
struct StreamableGurobiInitArguments {
  // An optional ISV key to use to instantiate the solver. This is ignored if a
  // `primary_env` is provided in `NonStreamableGurobiInitArguments`.
  std::optional<GurobiISVKey> isv_key;

  // Returns the proto corresponding to these parameters.
  GurobiInitializerProto Proto() const;

  // Parses the proto corresponding to these parameters.
  static StreamableGurobiInitArguments FromProto(
      const GurobiInitializerProto& args_proto);
};

// Solver initialization parameters that can be streamed to be exchanged with
// another process.
//
// Parameters that can't be streamed (for example instances of C/C++ types that
// only exist in the process memory) are dealt with implementations of
// the NonStreamableSolverInitArguments.
struct StreamableSolverInitArguments {
  std::optional<StreamableCpSatInitArguments> cp_sat;
  std::optional<StreamableGScipInitArguments> gscip;
  std::optional<StreamableGlopInitArguments> glop;
  std::optional<StreamableGlpkInitArguments> glpk;
  std::optional<StreamableGurobiInitArguments> gurobi;

  // Returns the proto corresponding to these parameters.
  SolverInitializerProto Proto() const;

  // Parses the proto corresponding to these parameters.
  static absl::StatusOr<StreamableSolverInitArguments> FromProto(
      const SolverInitializerProto& args_proto);
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_STREAMABLE_SOLVER_INIT_ARGUMENTS_H_

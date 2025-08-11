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

// Builds a gSCIP model from an MPModelProto and gives the mapping from proto
// variables to SCIP_VARs.
//
// Typically, prefer using scip_proto_solve.h. This class is useful if you need
// to set a callback or otherwise customize your model in a way not supported by
// the proto.
#ifndef OR_TOOLS_MATH_OPT_SOLVERS_GSCIP_GSCIP_FROM_MP_MODEL_PROTO_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_GSCIP_GSCIP_FROM_MP_MODEL_PROTO_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/math_opt/solvers/gscip/gscip.h"
#include "ortools/math_opt/solvers/gscip/gscip_ext.h"
#include "scip/type_var.h"

namespace operations_research {

struct GScipAndVariables {
  std::unique_ptr<GScip> gscip;
  std::vector<SCIP_VAR*> variables;

  // The model must be a valid MPModelProto and have no finite coefficients with
  // absolute value exceeding 1e20 (otherwise undefined behavior will occur).
  // See the linear_solver model validation methods to test this property.
  //
  // In the returned struct, variables will have one element for each of
  // model.variable() and be in the same order. Note that the underlying gSCIP
  // model may contain auxiliary variables not listed here when general
  // constraints are used.
  static absl::StatusOr<GScipAndVariables> FromMPModelProto(
      const MPModelProto& model);

  absl::Status AddHint(const PartialVariableAssignment& mp_hint);

 private:
  std::vector<SCIP_VAR*> TranslateMPVars(absl::Span<const int32_t> mp_vars);

  absl::Status AddLinearConstraint(const MPConstraintProto& lin_constraint);
  absl::Status AddGeneralConstraint(const MPGeneralConstraintProto& mp_gen);
  absl::Status AddIndicatorConstraint(absl::string_view name,
                                      const MPIndicatorConstraint& mp_ind);
  absl::Status AddSosConstraint(const std::string& name,
                                const MPSosConstraint& mp_sos);
  absl::Status AddQuadraticConstraint(const std::string& name,
                                      const MPQuadraticConstraint& mp_quad);
  absl::Status AddAbsConstraint(absl::string_view name,
                                const MPAbsConstraint& mp_abs);
  absl::Status AddAndConstraint(const std::string& name,
                                const MPArrayConstraint& mp_and);
  absl::Status AddOrConstraint(const std::string& name,
                               const MPArrayConstraint& mp_or);
  std::vector<GScipLinearExpr> MpArrayWithConstantToGScipLinearExprs(
      const MPArrayWithConstantConstraint& mp_array_with_constant);
  absl::Status AddMinConstraint(absl::string_view name,
                                const MPArrayWithConstantConstraint& mp_min);
  absl::Status AddMaxConstraint(absl::string_view name,
                                const MPArrayWithConstantConstraint& mp_max);

  // WARNING: YOU MUST SET THE OBJECTIVE DIRECTION BEFORE CALLING THIS, AND NOT
  // CHANGE IT AFTERWARDS!
  absl::Status AddQuadraticObjective(const MPQuadraticObjective& quad_obj);
};

}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_GSCIP_GSCIP_FROM_MP_MODEL_PROTO_H_

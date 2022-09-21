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

// Additional nonlinear constraints not supported directly by SCIP.
//
// The primary purpose of this file is to support the nonlinear constraints of
// MPSolver proto API.
//
// WARNING(rander): as these constraints are not natively supported in SCIP,
// they will generally not be a single SCIP_CONS* created, but will typically
// result in multiple SCIP_CONS* and SCIP_VAR* being created. Direct access to
// these intermediate variables and constraints is currently not provided.
//
// TODO(user): either implement with SCIP constraint handlers or use a solver
// independent implementation.
#ifndef OR_TOOLS_GSCIP_GSCIP_EXT_H_
#define OR_TOOLS_GSCIP_GSCIP_EXT_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "ortools/gscip/gscip.h"
#include "scip/scip.h"
#include "scip/scip_prob.h"
#include "scip/type_cons.h"
#include "scip/type_scip.h"
#include "scip/type_var.h"

namespace operations_research {

// Adds the constraint y = abs(x). May create auxiliary variables. Supports
// unbounded x.
absl::Status GScipCreateAbs(GScip* gscip, SCIP_Var* x, SCIP_Var* abs_x,
                            const std::string& name = "");

// TODO(user): delete this type and the methods below, use a generic version
// templated on the variable type that supports operator overloads.
struct GScipLinearExpr {
  absl::flat_hash_map<SCIP_VAR*, double> terms;
  double offset = 0.0;

  GScipLinearExpr() = default;
  explicit GScipLinearExpr(SCIP_VAR* variable);
  explicit GScipLinearExpr(double offset);
};

// Returns left - right.
GScipLinearExpr GScipDifference(GScipLinearExpr left,
                                const GScipLinearExpr& right);

// Returns -expr.
GScipLinearExpr GScipNegate(GScipLinearExpr expr);

// Returns the range -inf <= left.terms - right.terms <= right.offset -
// left.offset
GScipLinearRange GScipLe(const GScipLinearExpr left,
                         const GScipLinearExpr& right);

// Adds the constraint resultant = maximum(terms). Supports unbounded variables
// in terms.
absl::Status GScipCreateMaximum(GScip* gscip, const GScipLinearExpr& resultant,
                                const std::vector<GScipLinearExpr>& terms,
                                const std::string& name = "");

// Adds the constraint resultant = minimum(terms). Supports unbounded variables
// in terms.
absl::Status GScipCreateMinimum(GScip* gscip, const GScipLinearExpr& resultant,
                                const std::vector<GScipLinearExpr>& terms,
                                const std::string& name = "");

// Models the constraint z = 1 => lb <= ax <= ub
// If negate_indicator, then instead: z = 0 => lb <= ax <= ub
struct GScipIndicatorRangeConstraint {
  SCIP_VAR* indicator_variable = nullptr;
  bool negate_indicator = false;
  GScipLinearRange range;
};

// Supports unbounded variables in indicator_range.range.variables.
absl::Status GScipCreateIndicatorRange(
    GScip* gscip, const GScipIndicatorRangeConstraint& indicator_range,
    const std::string& name = "",
    const GScipConstraintOptions& options = GScipConstraintOptions());

// WARNING: DO NOT CHANGE THE OBJECTIVE DIRECTION AFTER CALLING THIS METHOD.
//
// This is implemented by modeling the quadratic term with an an inequality
// constraint and a single extra variable, which is then added to the objective.
// The inequality will be in the wrong direction if you change the objective
// direction after calling this method.
absl::Status GScipAddQuadraticObjectiveTerm(
    GScip* gscip, std::vector<SCIP_Var*> quadratic_variables1,
    std::vector<SCIP_Var*> quadratic_variables2,
    std::vector<double> quadratic_coefficients, const std::string& name = "");

}  // namespace operations_research

#endif  // OR_TOOLS_GSCIP_GSCIP_EXT_H_

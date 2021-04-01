// Copyright 2010-2021 Google LLC
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

#include "ortools/gscip/gscip_ext.h"

#include "ortools/base/logging.h"
#include "ortools/base/status_macros.h"

namespace operations_research {

namespace {

std::string MaybeExtendName(const std::string& base_name,
                            const std::string& extension) {
  if (base_name.empty()) {
    return "";
  }
  return absl::StrCat(base_name, "/", extension);
}

}  // namespace

GScipLinearExpr::GScipLinearExpr(SCIP_VAR* variable) { terms[variable] = 1.0; }

GScipLinearExpr::GScipLinearExpr(double offset) : offset(offset) {}

GScipLinearExpr GScipDifference(GScipLinearExpr left,
                                const GScipLinearExpr& right) {
  left.offset -= right.offset;
  for (const auto& term : right.terms) {
    left.terms[term.first] -= term.second;
  }
  return left;
}

GScipLinearExpr GScipNegate(GScipLinearExpr expr) {
  expr.offset = -expr.offset;
  for (auto& term : expr.terms) {
    term.second = -term.second;
  }
  return expr;
}

// Returns the range -inf <= left.terms - right.terms <= right.offset -
// left.offset
GScipLinearRange GScipLe(const GScipLinearExpr left,
                         const GScipLinearExpr& right) {
  GScipLinearExpr diff = GScipDifference(left, right);
  GScipLinearRange result;
  result.lower_bound = -std::numeric_limits<double>::infinity();
  result.upper_bound = -diff.offset;
  for (const auto& term : diff.terms) {
    result.variables.push_back(term.first);
    result.coefficients.push_back(term.second);
  }
  return result;
}

absl::Status GScipCreateAbs(GScip* gscip, SCIP_Var* x, SCIP_Var* abs_x,
                            const std::string& name) {
  return GScipCreateMaximum(
      gscip, GScipLinearExpr(abs_x),
      {GScipLinearExpr(x), GScipNegate(GScipLinearExpr(x))}, name);
}

absl::Status GScipCreateMaximum(GScip* gscip, const GScipLinearExpr& resultant,
                                const std::vector<GScipLinearExpr>& terms,
                                const std::string& name) {
  // TODO(user): it may be better to write this in terms of the disjuntive
  // constraint, we need to support disjunctions in gscip.h to do this.
  //
  // z_i in {0,1}, indicates if y = x_i
  //
  // x_i  <= y
  // z_i => y <= x_i
  //   \sum_i z_i == 1
  std::vector<SCIP_VAR*> indicators;
  for (int i = 0; i < terms.size(); ++i) {
    auto z = gscip->AddVariable(0.0, 1.0, 0.0, GScipVarType::kInteger,
                                MaybeExtendName(name, absl::StrCat("z_", i)));
    RETURN_IF_ERROR(z.status());
    indicators.push_back(*z);
  }

  for (int i = 0; i < terms.size(); ++i) {
    // x_i <= y
    RETURN_IF_ERROR(
        gscip
            ->AddLinearConstraint(
                GScipLe(terms.at(i), resultant),
                MaybeExtendName(name, absl::StrCat("x_", i, "_le_y")))
            .status());
    // z_i => y <= x_i
    {
      GScipLinearRange y_less_x = GScipLe(resultant, terms.at(i));
      CHECK_EQ(y_less_x.lower_bound, -std::numeric_limits<double>::infinity());
      GScipIndicatorConstraint ind;
      ind.indicator_variable = indicators.at(i);
      ind.variables = y_less_x.variables;
      ind.coefficients = y_less_x.coefficients;
      ind.upper_bound = y_less_x.upper_bound;
      RETURN_IF_ERROR(
          gscip
              ->AddIndicatorConstraint(
                  ind, MaybeExtendName(
                           name, absl::StrCat("y_le__x_", i, "_if_z_", i)))
              .status());
    }
  }

  // sum_i z_i = 1.
  GScipLinearRange z_use;
  z_use.upper_bound = 1.0;
  z_use.lower_bound = 1.0;
  z_use.variables = indicators;
  z_use.coefficients = std::vector<double>(indicators.size(), 1.0);

  return gscip->AddLinearConstraint(z_use, MaybeExtendName(name, "one_z"))
      .status();
}

absl::Status GScipCreateMinimum(GScip* gscip, const GScipLinearExpr& resultant,
                                const std::vector<GScipLinearExpr>& terms,
                                const std::string& name) {
  std::vector<GScipLinearExpr> negated_terms;
  negated_terms.reserve(terms.size());
  for (const GScipLinearExpr& e : terms) {
    negated_terms.push_back(GScipNegate(e));
  }
  return GScipCreateMaximum(gscip, GScipNegate(resultant), negated_terms, name);
}

absl::Status GScipAddQuadraticObjectiveTerm(
    GScip* gscip, std::vector<SCIP_Var*> quadratic_variables1,
    std::vector<SCIP_Var*> quadratic_variables2,
    std::vector<double> quadratic_coefficients, const std::string& name) {
  constexpr double kInf = std::numeric_limits<double>::infinity();
  auto obj_term =
      gscip->AddVariable(-kInf, kInf, 1.0, GScipVarType::kContinuous,
                         MaybeExtendName(name, "obj"));
  RETURN_IF_ERROR(obj_term.status());
  GScipQuadraticRange range;
  range.quadratic_variables1 = quadratic_variables1;
  range.quadratic_variables2 = quadratic_variables2;
  range.quadratic_coefficients = quadratic_coefficients;
  range.linear_coefficients = {-1.0};
  range.linear_variables = {*obj_term};
  if (gscip->ObjectiveIsMaximize()) {
    // maximize z
    // z <= Q(x, y)
    //   => 0 <= Q(x, y) - z <= inf
    range.lower_bound = 0.0;
  } else {
    // minimize z
    // z >= Q(x, y)
    //   => 0 >= Q(x, y) - z >= -inf
    range.upper_bound = 0.0;
  }
  return gscip->AddQuadraticConstraint(range, MaybeExtendName(name, "cons"))
      .status();
}

absl::Status GScipCreateIndicatorRange(
    GScip* gscip, const GScipIndicatorRangeConstraint& indicator_range,
    const std::string& name, const GScipConstraintOptions& options) {
  if (std::isfinite(indicator_range.range.upper_bound)) {
    GScipIndicatorConstraint ub_constraint;
    ub_constraint.upper_bound = indicator_range.range.upper_bound;
    ub_constraint.variables = indicator_range.range.variables;
    ub_constraint.coefficients = indicator_range.range.coefficients;
    ub_constraint.indicator_variable = indicator_range.indicator_variable;
    ub_constraint.negate_indicator = indicator_range.negate_indicator;
    RETURN_IF_ERROR(gscip
                        ->AddIndicatorConstraint(
                            ub_constraint, MaybeExtendName(name, "ub"), options)
                        .status());
  }
  if (std::isfinite(indicator_range.range.lower_bound)) {
    // want z -> lb <= a * x
    //   <=> z -> -lb >= -a * x
    GScipIndicatorConstraint lb_constraint;
    lb_constraint.upper_bound = -indicator_range.range.lower_bound;
    lb_constraint.variables = indicator_range.range.variables;
    for (const double c : indicator_range.range.coefficients) {
      lb_constraint.coefficients.push_back(-c);
    }
    lb_constraint.indicator_variable = indicator_range.indicator_variable;
    lb_constraint.negate_indicator = indicator_range.negate_indicator;
    RETURN_IF_ERROR(gscip
                        ->AddIndicatorConstraint(
                            lb_constraint, MaybeExtendName(name, "lb"), options)
                        .status());
  }
  return absl::OkStatus();
}

}  // namespace operations_research

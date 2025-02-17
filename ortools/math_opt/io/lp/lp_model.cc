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

#include "ortools/math_opt/io/lp/lp_model.h"

#include <cmath>
#include <cstdlib>
#include <ostream>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/io/lp/lp_name.h"
#include "ortools/util/fp_roundtrip_conv.h"

namespace operations_research::lp_format {
namespace {

absl::Status ValidateRelation(Relation relation) {
  switch (relation) {
    case Relation::kLessOrEqual:
    case Relation::kEqual:
    case Relation::kGreaterOrEqual:
      return absl::OkStatus();
  }
  return util::InvalidArgumentErrorBuilder()
         << "Invalid Relation: " << static_cast<int>(relation);
}

}  // namespace

std::ostream& operator<<(std::ostream& ostr, const Relation relation) {
  switch (relation) {
    case Relation::kGreaterOrEqual:
      ostr << ">=";
      return ostr;
    case Relation::kEqual:
      ostr << "=";
      return ostr;
    case Relation::kLessOrEqual:
      ostr << "<=";
      return ostr;
  }
  ostr << "__invalid_Relation_" << static_cast<int>(relation) << "__";
  return ostr;
}

std::ostream& operator<<(std::ostream& ostr, const Constraint& constraint) {
  auto term_format = [](std::string* out,
                        const std::pair<double, VariableIndex>& term) {
    absl::StrAppend(out, "{", RoundTripDoubleFormat(term.first), ", ",
                    term.second, "}");
  };
  ostr << "terms: {" << absl::StrJoin(constraint.terms, ", ", term_format)
       << "} relation: " << constraint.relation
       << " rhs: " << RoundTripDoubleFormat(constraint.rhs) << " name: \""
       << absl::CEscape(constraint.name) << "\"";
  return ostr;
}

absl::StatusOr<ConstraintIndex> LpModel::AddConstraint(Constraint constraint) {
  if (!constraint.name.empty()) {
    RETURN_IF_ERROR(ValidateName(constraint.name)) << "invalid constraint name";
  }
  if (constraint.terms.empty()) {
    return absl::InvalidArgumentError("constraint must have at least one term");
  }
  for (const auto [coef, var] : constraint.terms) {
    if (var < VariableIndex{0} || var >= variables_.end_index()) {
      return util::InvalidArgumentErrorBuilder()
             << "variable ids should be in [0," << variables_.end_index()
             << ") but found: " << var;
    }
    if (!std::isfinite(coef)) {
      return util::InvalidArgumentErrorBuilder()
             << "All coefficients in constraints must be finite and not NaN "
                "but found: "
             << coef;
    }
  }
  RETURN_IF_ERROR(ValidateRelation(constraint.relation));
  if (std::isnan(constraint.rhs)) {
    return absl::InvalidArgumentError("rhs of constraint was NaN");
  }
  ConstraintIndex result = constraints_.end_index();
  constraints_.push_back(std::move(constraint));
  return result;
}

absl::StatusOr<VariableIndex> LpModel::AddVariable(absl::string_view name) {
  if (variable_names_.contains(name)) {
    return util::InvalidArgumentErrorBuilder()
           << "duplicate variable name: " << name;
  }
  RETURN_IF_ERROR(ValidateName(name)) << "invalid variable name";
  const VariableIndex index = variables_.end_index();
  variables_.push_back(std::string(name));
  variable_names_[name] = index;
  return index;
}

std::ostream& operator<<(std::ostream& ostr, const LpModel& model) {
  ostr << "SUBJECT TO" << std::endl;
  for (const Constraint& constraint : model.constraints()) {
    ostr << "  ";
    if (!constraint.name.empty()) {
      ostr << constraint.name << ": ";
    }
    bool first = true;
    for (const auto [coef, var] : constraint.terms) {
      if (first) {
        if (coef != 1.0) {
          ostr << RoundTripDoubleFormat(coef) << " ";
        }
      } else {
        if (coef > 0) {
          ostr << " + ";
        } else {
          ostr << " - ";
        }
        ostr << RoundTripDoubleFormat(std::abs(coef)) << " ";
      }
      ostr << model.variables()[var];
      first = false;
    }
    ostr << " " << constraint.relation << " "
         << RoundTripDoubleFormat(constraint.rhs) << std::endl;
  }
  ostr << "END" << std::endl;
  return ostr;
}

}  // namespace operations_research::lp_format

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

#include "ortools/math_opt/cpp/objective.h"

#include <optional>
#include <ostream>
#include <sstream>
#include <string>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/model_storage_types.h"

namespace operations_research::math_opt {

LinearExpression Objective::AsLinearExpression() const {
  CHECK_EQ(storage()->num_quadratic_objective_terms(id_), 0)
      << "The objective function contains quadratic terms and cannot be "
         "represented as a LinearExpression";
  LinearExpression objective = offset();
  for (const auto [raw_var_id, coeff] : storage()->linear_objective(id_)) {
    objective += coeff * Variable(storage(), raw_var_id);
  }
  return objective;
}

QuadraticExpression Objective::AsQuadraticExpression() const {
  QuadraticExpression result = offset();
  for (const auto& [v, coef] : storage()->linear_objective(id_)) {
    result += coef * Variable(storage(), v);
  }
  for (const auto& [v1, v2, coef] : storage()->quadratic_objective_terms(id_)) {
    result +=
        QuadraticTerm(Variable(storage(), v1), Variable(storage(), v2), coef);
  }
  return result;
}

std::string Objective::ToString() const {
  if (!is_primary() && !storage()->has_auxiliary_objective(*id_)) {
    return std::string(kDeletedObjectiveDefaultDescription);
  }
  std::stringstream str;
  str << AsQuadraticExpression();
  return str.str();
}

std::ostream& operator<<(std::ostream& ostr, const Objective& objective) {
  // TODO(b/170992529): handle quoting of invalid characters in the name.
  const absl::string_view name = objective.name();
  if (name.empty()) {
    if (objective.is_primary()) {
      ostr << "__primary_obj__";
    } else {
      ostr << "__aux_obj#" << *objective.id() << "__";
    }
  } else {
    ostr << name;
  }
  return ostr;
}

}  // namespace operations_research::math_opt

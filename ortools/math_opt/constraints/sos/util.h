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

#ifndef OR_TOOLS_MATH_OPT_CONSTRAINTS_SOS_UTIL_H_
#define OR_TOOLS_MATH_OPT_CONSTRAINTS_SOS_UTIL_H_

#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/util/fp_roundtrip_conv.h"

namespace operations_research::math_opt::internal {

// This method can only be called with a parameter of either `Sos1Constraint`
// or `Sos2Constraint`.
//
// Tested in sos1_constraint_test and sos2_constraint_test, as the ToString()
// member functions are thin wrappers around this function.
template <typename SosConstraint>
std::string SosConstraintToString(SosConstraint constraint,
                                  absl::string_view sos_type_name);

////////////////////////////////////////////////////////////////////////////////
// Inline function implementations
////////////////////////////////////////////////////////////////////////////////

template <typename SosConstraint>
std::string SosConstraintToString(const SosConstraint constraint,
                                  const absl::string_view sos_type_name) {
  std::stringstream ostr;
  const int64_t num_expressions = constraint.num_expressions();
  std::vector<LinearExpression> expressions;
  expressions.reserve(num_expressions);
  for (int i = 0; i < num_expressions; ++i) {
    expressions.push_back(constraint.Expression(i));
  }
  ostr << "{" << absl::StrJoin(expressions, ", ", absl::StreamFormatter())
       << "} is " << sos_type_name;
  if (constraint.has_weights()) {
    std::vector<std::string> weights(num_expressions);
    for (int i = 0; i < num_expressions; ++i) {
      weights[i] = RoundTripDoubleFormat::ToString(constraint.weight(i));
    }
    ostr << " with weights {" << absl::StrJoin(weights, ", ") << "}";
  }
  return ostr.str();
}

}  // namespace operations_research::math_opt::internal

#endif  // OR_TOOLS_MATH_OPT_CONSTRAINTS_SOS_UTIL_H_

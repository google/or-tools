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

#ifndef OR_TOOLS_MATH_OPT_IO_LP_LP_MODEL_H_
#define OR_TOOLS_MATH_OPT_IO_LP_LP_MODEL_H_

#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ortools/base/strong_int.h"
#include "ortools/base/strong_vector.h"

namespace operations_research::lp_format {

DEFINE_STRONG_INT_TYPE(VariableIndex, int64_t);
DEFINE_STRONG_INT_TYPE(ConstraintIndex, int64_t);
using Term = std::pair<double, VariableIndex>;
enum class Relation { kLessOrEqual, kGreaterOrEqual, kEqual };

std::ostream& operator<<(std::ostream& ostr, Relation relation);

struct Constraint {
  std::vector<Term> terms;
  Relation relation = Relation::kLessOrEqual;
  double rhs = 0.0;
  std::string name;
};

// Note: this prints an exact representation of the data in Constraint, not
// the string form of the constraint in LP format.
std::ostream& operator<<(std::ostream& ostr, const Constraint& constraint);

// The contents of an optimization model in LP file format.
//
// You can convert this to a string in the LP file format using <<, and read
// from a string in the LP file format using ParseLp from parse_lp.h.
class LpModel {
 public:
  LpModel() = default;

  // Adds a new variable to the model and returns it. Errors if name:
  //  * is empty
  //  * is the same as any existing variable name
  //  * has invalid characters for the LP file format
  //
  // Variable names are case sensitive.
  absl::StatusOr<VariableIndex> AddVariable(absl::string_view name);

  // Adds a new constraint to the model and returns its index.
  //
  // Errors if:
  //  * a variable id from constraint.bounds is out of bounds
  //  * constraint.relation is an invalid enum
  //  * a coefficient on constraint.terms is Inf or NaN
  //  * the name has invalid characters
  //  * there are no terms in the constraint
  //
  // Constraint names can be repeated but this is not recommended.
  absl::StatusOr<ConstraintIndex> AddConstraint(Constraint constraint);

  const absl::flat_hash_map<std::string, VariableIndex>& variable_names()
      const {
    return variable_names_;
  }
  const util_intops::StrongVector<VariableIndex, std::string>& variables()
      const {
    return variables_;
  }
  const util_intops::StrongVector<ConstraintIndex, Constraint>& constraints()
      const {
    return constraints_;
  }

 private:
  absl::flat_hash_map<std::string, VariableIndex> variable_names_;
  util_intops::StrongVector<VariableIndex, std::string> variables_;
  util_intops::StrongVector<ConstraintIndex, Constraint> constraints_;
};

// Prints the model in LP format to ostr.
std::ostream& operator<<(std::ostream& ostr, const LpModel& model);

}  // namespace operations_research::lp_format

#endif  // OR_TOOLS_MATH_OPT_IO_LP_LP_MODEL_H_

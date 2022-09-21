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

// A simple parser of a linear program from string.
//
// We accept a format produced by LinearProgram::Dump(), which is similar to
// LP file used by lp_solve (see http://lpsolve.sourceforge.net/5.1/index.htm).
// Example:
//   1: min: 1 + x1 + 2 * x2;
//   2: 0 <= x1 <= 1;
//   3: x2 >= 2;
//   4: r1: 1 <= x1 - x2 <= 2;
//   5: 0 <= x1 + x2 <= inf;
//   6: int x1, x3;
//   7: bin x2;
//
//   Line 1 is the objective, line 2 and 3 define variable bounds, line 4 is a
//   named constraint, line 5 is an unnamed constraint. Line 6 is the list of
//   integer variables. Line 7 is the list of binary variables. The lines can be
//   in any order, the line numbers do _not_ belong to the string being parsed.
//
// Caveats:
//   1. Plus sign and multiplication sign are optional. Thus, "min: 1 x1 x2" is
//   the same as "min: 1*x1 + x2". All consecutive signs will be compacted into
//   one sign using mathematical rules (i.e., the parity of minus sign).
//   E.g., "min: ++---+ - +x1" is the same as "min: x1".
//   2. A constraint consists of two or three parts. A two part constraint has
//   a bound on the left (resp. right) side and variables on the right
//   (resp. left) side, with the two parts being separated by any of the
//   relation signs "<", "<=", "=", ">=", ">".
//   3. A three part constraint has the variables in the middle part, and two
//   bounds on the left and right side, with all three parts being separated by
//   any of "<", "<=", ">=", ">".
//   4. "<" means "<=", and ">" means ">=".
//   5. An unnamed constraint involving exactly one variable with coefficient
//   equal to 1, defines the variable bound(s). Otherwise, the constraint
//   defines a new constraint.
//   6. If there is no bound defined for a variable, it will be assumed to be
//   unbounded (i.e., from -inf to +inf).
//   7. A bound must be a number or "inf". A coefficient must be finite and
//   cannot overflow. A number can be represented in a scientific notation,
//   e.g., +1.2E-2. Consequently,
//   "min: 1e2" means minimization of 100,
//   "min: 1 e2" means minimization of 1*e2, where e2 is a variable,
//   "min: 1 + e2" means minimization of 1 + e2, where e2 is a variable,
//   "min: 1 1*e2" means minimization of 1 + e2, where e2 is a variable.
//   "min: 1 1e2" is invalid as it would mean minimization of 1 + 100.
//   8. In a constraint, in the part with variables, all elements must be
//   variables with optional coefficients and signs (i.e., no offset is
//   allowed).
//   9. Variables in the objective, and in each of the constraint cannot repeat.
//   E.g., this is invalid: "min: x + x".
//   10. The offset in the objective must be specified at the beginning, i.e.,
//   after min: or max: and before any variables.
//   11. The parsing will fail if due to bounding of a variable the lower bound
//   becomes strictly greater than the upper bound. E.g., these fail to
//   parse: "min x; 1 <= x <= 0;", "min x; 0 <= x <= 1; 2 <= x <= 3". On the
//   other hand the parser does _not_ attempt to "round" the bounds for integer
//   variables. E.g., "min x; 0.5 <= x <= 0.8; int x" results in bounding the x
//   variable between 0.5 and 0.8, despite there is no integer value it can
//   take. Similarly, "min x; bin x; x <= 0.5" results in bounding the x
//   variable between 0.0 and 0.5, despite the only value it can take is 0.

#ifndef OR_TOOLS_LP_DATA_LP_PARSER_H_
#define OR_TOOLS_LP_DATA_LP_PARSER_H_

#if defined(USE_LP_PARSER)

#include <string>
#include <vector>

#include "absl/base/port.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"

namespace operations_research {

// This calls ParseLp() under the hood. See below.
absl::StatusOr<MPModelProto> ModelProtoFromLpFormat(absl::string_view model);

namespace glop {

// Like ModelProtoFromLpFormat(), but outputs a glop::LinearProgram.
ABSL_MUST_USE_RESULT bool ParseLp(absl::string_view model, LinearProgram* lp);

// Represents a constraint parsed from the LP file format (used by
// LinearProgram::Dump()).
struct ParsedConstraint {
  // The name of the constraint. Empty if the constraint has no name.
  std::string name;
  // Contains the names of the variables used in the constraint, in the order in
  // which they appear in the string representation.
  std::vector<std::string> variable_names;
  // Contains the coefficients of the variables used in the constraint. Note
  // that variable_names and coefficients are parallel arrays, i.e.
  // coefficients[i] is the coefficient for variable_names[i].
  std::vector<Fractional> coefficients;
  // The lower bound of the constraint. Set to -infinity when the constraint has
  // no lower bound.
  Fractional lower_bound;
  // The upper bound of the constraint. Set to +infinity when the constraint has
  // no upper bound.
  Fractional upper_bound;
};

// Parses a constraint in the format used by LinearProgram::Dump(). Returns an
// InvalidArgumentError with an appropriate error message when the parsing
// fails.
absl::StatusOr<ParsedConstraint> ParseConstraint(absl::string_view constraint);

}  // namespace glop
}  // namespace operations_research

#endif  // defined(USE_LP_PARSER)

#endif  // OR_TOOLS_LP_DATA_LP_PARSER_H_

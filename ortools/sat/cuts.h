// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_SAT_CUTS_H_
#define OR_TOOLS_SAT_CUTS_H_

#include <utility>
#include <vector>

#include "ortools/base/int_type.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// A "cut" generator on a set of IntegerVariable.
//
// The generate_cuts() function will usually be called with the current LP
// optimal solution (but should work for any lp_values). Note that a
// CutGenerator should:
// - Only look at the lp_values positions that corresponds to its 'vars' or
//   their negation.
// - Only return cuts in term of the same variables or their negation.
struct CutGenerator {
  std::vector<IntegerVariable> vars;
  std::function<std::vector<LinearConstraint>(
      const gtl::ITIVector<IntegerVariable, double>& lp_values)>
      generate_cuts;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CUTS_H_

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

#ifndef ORTOOLS_MATH_OPT_IO_LP_MATCHERS_H_
#define ORTOOLS_MATH_OPT_IO_LP_MATCHERS_H_

#include "ortools/base/gmock.h"
#include "ortools/math_opt/io/lp/lp_model.h"

namespace operations_research::lp_format {

// Checks that all the values in a Constraint are identical to `expected` (and
// the order of terms matters).
testing::Matcher<Constraint> ConstraintEquals(const Constraint& expected);

// Checks that two models are the same (the list of variables is in the same
// order, the constraints are in the same order, and each constraint has the
// same terms in the same order).
testing::Matcher<LpModel> ModelEquals(const LpModel& expected);

// Similar to ModelEquals, but making the following relaxations:
//  * Variables can be in any order.
//  * If `remove_unused_variables` is true, variables appearing in no constraint
//    are deleted from both expected and actual before comparing (these
//    variables are not printed when the model is dumped to LP format anyway).
// Note that constraints still must be in the correct order and their terms
// must be in the correct order.
testing::Matcher<LpModel> ModelEquivTo(const LpModel& expected,
                                       bool remove_unused_variables = true);

}  // namespace operations_research::lp_format

#endif  // ORTOOLS_MATH_OPT_IO_LP_MATCHERS_H_

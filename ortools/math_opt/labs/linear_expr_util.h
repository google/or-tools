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

// Methods for manipulating LinearExpressions.
//
// Why in labs? Lots of users seem to need this (e.g. for big-M calculations),
// but there are several possible algorithms, and it is not clear what, if
// anything, would be used widely. The function also makes many assumptions on
// the input that are not easy to verify and can lead to confusing errors,
// it is worth seeing if the API can be hardened a bit.
#ifndef OR_TOOLS_MATH_OPT_LABS_LINEAR_EXPR_UTIL_H_
#define OR_TOOLS_MATH_OPT_LABS_LINEAR_EXPR_UTIL_H_

#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {

// Computes a lower bound on the value a linear expression can take based on
// the variable bounds.
//
// The user must ensure:
//  * Variable lower bounds are in [-inf, +inf) (required at solve time as well)
//  * Variable upper bounds are in (-inf, +inf] (required at solve time as well)
//  * Variables bounds are not NaN
//  * The expression has no NaNs and all finite coefficients
//  * The output computation does not overflow when summing finite terms (rarely
//    an issue, as then your problem is very poorly scaled).
// Under these assumptions, the returned value will be in [-inf, +inf). If an
// assumption is broken, it is possible to return NaN or +inf.
//
// This function is deterministic, but runs in O(n log n) and will allocate.
//
// Alternatives:
//  * If more precision is needed, see AccurateSum
//  * For a faster method that does not allocate, is less precise, and not
//    deterministic, simply add each term to the result in the hash map's
//    iteration order.
double LowerBound(const LinearExpression& linear_expression);

// Computes an upper bound on the value a linear expression can take based on
// the variable bounds.
//
// The returned value will be in (-inf, +inf] on valid input (see LowerBound()
// above, the requirements are the same).
//
// See LowerBound() above for more details.
double UpperBound(const LinearExpression& linear_expression);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_LABS_LINEAR_EXPR_UTIL_H_

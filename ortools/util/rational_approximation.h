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

#ifndef OR_TOOLS_UTIL_RATIONAL_APPROXIMATION_H_
#define OR_TOOLS_UTIL_RATIONAL_APPROXIMATION_H_

#include <utility>

#include "ortools/base/integral_types.h"

namespace operations_research {

// The type Fraction represents a number in the form of two integers: numerator
// and denominator. This type is used to display the rational approximation
// of a Fractional number.
typedef std::pair<int64, int64> Fraction;

// Computes a rational approximation numerator/denominator for value x
// using a continued fraction algorithm. The absolute difference between the
// output fraction and the input "x" will not exceed "precision".
// TODO(user): make a parameterized template with integer and floating-point
// type parameters.
Fraction RationalApproximation(const double x, const double precision);

}  // namespace operations_research
#endif  // OR_TOOLS_UTIL_RATIONAL_APPROXIMATION_H_

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

// Utilities to display linear expression in a human-readable way.

#ifndef OR_TOOLS_LP_DATA_LP_PRINT_UTILS_H_
#define OR_TOOLS_LP_DATA_LP_PRINT_UTILS_H_

#include <string>

#include "absl/strings/str_format.h"
#include "ortools/base/integral_types.h"
#include "ortools/lp_data/lp_types.h"

namespace operations_research {
namespace glop {

// Returns a string representing a floating-point number in decimal,
// with a precision corresponding to the type of the argument.
inline std::string Stringify(const float a) {
  return absl::StrFormat("%.7g", a);
}

inline std::string Stringify(const double a) {
  return absl::StrFormat("%.16g", a);
}

inline std::string Stringify(const long double a) {
  return absl::StrFormat("%.19g", a);
}

// Returns a string "num/den" representing the rational approximation of x.
// The absolute difference between the output fraction and the input "x" will
// not exceed "precision".
std::string StringifyRational(const double x, const double precision);

// If fraction is true, returns a string corresponding to the rational
// approximation or a decimal approximation otherwise. Note that the absolute
// difference between the output fraction and "x" will never exceed
// std::numeric_limits<T>::epsilon().
std::string Stringify(const Fractional x, bool fraction);

// Pretty prints a monomial a*x using Stringify(x, fraction) to display a,
// taking care of the sign of x, whether a is 0, 1, -1, integer. Note that the
// absolute difference between the output fraction and "x" will never exceed
// std::numeric_limits<T>::epsilon().
std::string StringifyMonomial(const Fractional a, const std::string& x,
                              bool fraction);

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_LP_DATA_LP_PRINT_UTILS_H_

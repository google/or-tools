// Copyright 2010-2021 Google LLC
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

#include "ortools/lp_data/lp_print_utils.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>

#include "absl/strings/str_cat.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/util/rational_approximation.h"

namespace operations_research {
namespace glop {

// Returns a string "num/den" representing the rational approximation of x.
// The absolute difference between the output fraction and the input "x" will
// not exceed "precision".
std::string StringifyRational(const double x, const double precision) {
  if (x == kInfinity) {
    return "inf";
  } else if (x == -kInfinity) {
    return "-inf";
  }
  Fraction fraction = RationalApproximation(x, precision);
  const int64_t numerator = fraction.first;
  const int64_t denominator = fraction.second;
  return denominator == 1 ? absl::StrCat(numerator)
                          : absl::StrCat(numerator, "/", denominator);
}

std::string Stringify(const Fractional x, bool fraction) {
  return fraction ? StringifyRational(ToDouble(x),
                                      std::numeric_limits<double>::epsilon())
                  : Stringify(x);
}

// Returns a string that pretty-prints a monomial ax with coefficient
// a and variable name x
std::string StringifyMonomial(const Fractional a, const std::string& x,
                              bool fraction) {
  if (a == 0.0) return "";
  return a > 0.0
             ? absl::StrCat(
                   " + ",
                   a == 1.0 ? x : absl::StrCat(Stringify(a, fraction), " ", x))
             : absl::StrCat(
                   " - ", a == -1.0
                              ? x
                              : absl::StrCat(Stringify(-a, fraction), " ", x));
}

}  // namespace glop
}  // namespace operations_research

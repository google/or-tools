// Copyright 2010-2017 Google
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

#include "ortools/util/rational_approximation.h"
#include <cmath>
#include <limits>

#include "ortools/base/logging.h"

namespace operations_research {

// Computes a rational approximation numerator/denominator for value x
// using a continued fraction algorithm. The absolute difference between the
// output fraction and the input "x" will not exceed "precision".
Fraction RationalApproximation(const double x, const double precision) {
  DCHECK_LT(x, std::numeric_limits<double>::infinity());
  DCHECK_GT(x, -std::numeric_limits<double>::infinity());
  // All computations are made on long doubles to guarantee the maximum
  // precision for the approximations. This way, the approximations when
  // Fractional is float or double are as accurate as possible.
  long double abs_x = std::abs(x);
  long double y = abs_x;
  int64 previous_numerator = 0;
  int64 previous_denominator = 1;
  int64 numerator = 1;
  int64 denominator = 0;
  while (true) {
    const int64 term = static_cast<int64>(std::floor(y));
    const int64 new_numerator = term * numerator + previous_numerator;
    const int64 new_denominator = term * denominator + previous_denominator;
    // If there was an overflow, we prefer returning a not-so-good approximation
    // rather than something that is completely wrong.
    if (new_numerator < 0 || new_denominator < 0) break;
    previous_numerator = numerator;
    previous_denominator = denominator;
    numerator = new_numerator;
    denominator = new_denominator;
    long double numerator_approximation = abs_x * denominator;
    if (std::abs(numerator_approximation - numerator) <=
        precision * numerator_approximation) {
      break;
    }
    y = 1 / (y - term);
  }
  return Fraction((x < 0) ? -numerator : numerator, denominator);
}
}  // namespace operations_research

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

#include "absl/log/check.h"
#if defined(_WIN32)
#define _USE_MATH_DEFINES
#include <cmath>
#endif

#include "ortools/base/mathutil.h"

namespace operations_research {

// The formula is extracted from the following page
// http://en.wikipedia.org/w/index.php?title=Stirling%27s_approximation
double MathUtil::Stirling(double n) {
  static const double kLog2Pi = log(2 * M_PI);
  const double logN = log(n);
  return (n * logN - n + 0.5 * (kLog2Pi + logN)  // 0.5 * log(2 * M_PI * n)
          + 1 / (12 * n) - 1 / (360 * n * n * n));
}

double MathUtil::LogCombinations(int n, int k) {
  CHECK_GE(n, k);
  CHECK_GE(n, 0);
  CHECK_GE(k, 0);

  // use symmetry to pick the shorter calculation
  if (k > n / 2) {
    k = n - k;
  }

  // If we have more than 30 logarithms to calculate, we'll use
  // Stirling's approximation for log(n!).
  if (k > 15) {
    return Stirling(n) - Stirling(k) - Stirling(n - k);
  } else {
    double result = 0;
    for (int i = 1; i <= k; i++) {
      result += log(n - k + i) - log(i);
    }
    return result;
  }
}
}  // namespace operations_research

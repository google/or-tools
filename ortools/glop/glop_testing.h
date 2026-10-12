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

// Types used in unit test of multiple components of Glop.
#ifndef ORTOOLS_GLOP_GLOP_TESTING_H_
#define ORTOOLS_GLOP_GLOP_TESTING_H_

#include <ostream>

namespace operations_research::glop {

// The tested algorithm for parametric tests.
enum TestAlgorithm { kPrimalSimplex, kDualSimplex };

// Prints a snake_case string.
std::ostream& operator<<(std::ostream& out, const TestAlgorithm algorithm);

// The tested optimization direction for parametrics test.
enum TestDirection { kMinimize, kMaximize };

// Prints a snake_case string.
std::ostream& operator<<(std::ostream& out, const TestDirection direction);

}  // namespace operations_research::glop

#endif  // ORTOOLS_GLOP_GLOP_TESTING_H_

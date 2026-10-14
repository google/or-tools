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

#include "ortools/glop/glop_testing.h"

#include <ostream>

namespace operations_research::glop {

std::ostream& operator<<(std::ostream& out, const TestAlgorithm algorithm) {
  switch (algorithm) {
    case TestAlgorithm::kPrimalSimplex:
      out << "primal_simplex";
      break;
    case TestAlgorithm::kDualSimplex:
      out << "dual_simplex";
      break;
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, const TestDirection direction) {
  switch (direction) {
    case TestDirection::kMinimize:
      out << "minimize";
      break;
    case TestDirection::kMaximize:
      out << "maximize";
      break;
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, const TestUseScaling use_scaling) {
  switch (use_scaling) {
    case TestUseScaling::kNo:
      out << "no_scaling";
      break;
    case TestUseScaling::kYes:
      out << "with_scaling";
      break;
  }
  return out;
}

}  // namespace operations_research::glop

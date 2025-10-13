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

#include "ortools/math_opt/elemental/elemental_matcher.h"

#include <ostream>

#include "ortools/math_opt/elemental/elemental.h"
#include "ortools/math_opt/elemental/elemental_differencer.h"

namespace operations_research::math_opt {

ElementalMatcher::ElementalMatcher(const Elemental& expected,
                                   const ElementalDifferenceOptions& options)
    : expected_(&expected), options_(options) {}

bool ElementalMatcher::MatchAndExplain(const Elemental& actual,
                                       std::ostream* os) const {
  const ElementalDifference diff =
      ElementalDifference::Create(actual, *expected_, options_);
  if (diff.Empty()) {
    return true;
  }
  if (os != nullptr) {
    *os << "\nWith differences:\n"
        << ElementalDifference::Describe(actual, *expected_, diff);
  }
  return false;
}

void ElementalMatcher::DescribeTo(std::ostream* os) const {
  *os << "Is equal to model: " << expected_->DebugString(/*print_diffs=*/false);
}

void ElementalMatcher::DescribeNegationTo(std::ostream* os) const {
  *os << "Is not equal to model: "
      << expected_->DebugString(/*print_diffs=*/false);
}

}  // namespace operations_research::math_opt

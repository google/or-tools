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

#ifndef ORTOOLS_MATH_OPT_ELEMENTAL_ELEMENTAL_MATCHER_H_
#define ORTOOLS_MATH_OPT_ELEMENTAL_ELEMENTAL_MATCHER_H_

#include <ostream>

#include "absl/base/attributes.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/elemental/elemental.h"
#include "ortools/math_opt/elemental/elemental_differencer.h"

namespace operations_research::math_opt {

class ElementalMatcher {
 public:
  using is_gtest_matcher = void;

  explicit ElementalMatcher(const Elemental& expected
                                ABSL_ATTRIBUTE_LIFETIME_BOUND,
                            const ElementalDifferenceOptions& options);

  bool MatchAndExplain(const Elemental& actual, std::ostream* os) const;

  void DescribeTo(std::ostream* os) const;

  void DescribeNegationTo(std::ostream* os) const;

 private:
  const Elemental* expected_;
  ElementalDifferenceOptions options_;
};

inline testing::Matcher<const Elemental&> EquivToElemental(
    const Elemental& expected ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const ElementalDifferenceOptions& options = {}) {
  return ElementalMatcher(expected, options);
}

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_ELEMENTAL_ELEMENTAL_MATCHER_H_

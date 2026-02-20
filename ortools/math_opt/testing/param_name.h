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

#ifndef ORTOOLS_MATH_OPT_TESTING_PARAM_NAME_H_
#define ORTOOLS_MATH_OPT_TESTING_PARAM_NAME_H_

#include <string>

#include "gtest/gtest.h"

namespace operations_research::math_opt {

// Generates the test name suffixes in a parameterized test suite when the
// parameter type has a field `name`.
struct ParamName {
  template <class ParamType>
  std::string operator()(const testing::TestParamInfo<ParamType>& info) const {
    return info.param.name;
  }
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_TESTING_PARAM_NAME_H_

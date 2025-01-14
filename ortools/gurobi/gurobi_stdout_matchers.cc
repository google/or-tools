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

#include "ortools/gurobi/gurobi_stdout_matchers.h"

#include <string>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research {

testing::Matcher<std::string> EmptyOrGurobiLicenseWarning() {
  return testing::AnyOf(
      "",
      testing::MatchesRegex(
          R"regexp(\n*-+\nWarning: your license will expire in [0-9]+ days\n-+\n+)regexp"));
}

testing::Matcher<std::string> EmptyOrGurobiLicenseWarningIfGurobi(
    const bool is_gurobi) {
  return is_gurobi ? EmptyOrGurobiLicenseWarning() : testing::Eq("");
}

}  // namespace operations_research

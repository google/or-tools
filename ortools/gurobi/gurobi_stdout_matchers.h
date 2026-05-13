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

#ifndef OR_TOOLS_GUROBI_GUROBI_STDOUT_MATCHERS_H_
#define OR_TOOLS_GUROBI_GUROBI_STDOUT_MATCHERS_H_

#include <string>

#include "ortools/base/gmock.h"

namespace operations_research {

// Returns a matcher that either matches an empty string or the logs printed to
// stdout by Gurobi when a license is about to expire.
testing::Matcher<std::string> EmptyOrGurobiLicenseWarning();

// Returns either a matcher for the empty string when is_gurobi is false or the
// result of EmptyOrGurobiLicenseWarning() when true.
//
// This is a convenience function for generic tests shared between Gurobi and
// other solvers.
testing::Matcher<std::string> EmptyOrGurobiLicenseWarningIfGurobi(
    bool is_gurobi);

}  // namespace operations_research

#endif  // OR_TOOLS_GUROBI_GUROBI_STDOUT_MATCHERS_H_

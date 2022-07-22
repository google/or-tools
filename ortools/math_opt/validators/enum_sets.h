// Copyright 2010-2022 Google LLC
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

// This header defines sets of enum values that can be used for parametrized
// tests.
#ifndef OR_TOOLS_MATH_OPT_VALIDATORS_ENUM_SETS_H_
#define OR_TOOLS_MATH_OPT_VALIDATORS_ENUM_SETS_H_

#include <ostream>
#include <string>
#include <tuple>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"

namespace operations_research {
namespace math_opt {

// Returns all valid feasibility statuses (i.e does not include UNSPECIFIED).
std::vector<FeasibilityStatusProto> AllFeasibilityStatuses();

// Returns all valid solution statuses (i.e does not include UNSPECIFIED).
std::vector<SolutionStatusProto> AllSolutionStatuses();

// Printing utilities for parametrized tests.
void PrintTo(const SolutionStatusProto& proto, std::ostream* os);
void PrintTo(const FeasibilityStatusProto& proto, std::ostream* os);
void PrintTo(const std::tuple<SolutionStatusProto, SolutionStatusProto>& proto,
             std::ostream* os);
void PrintTo(
    const std::tuple<FeasibilityStatusProto, FeasibilityStatusProto>& proto,
    std::ostream* os);
void PrintTo(
    const std::tuple<SolutionStatusProto, FeasibilityStatusProto>& proto,
    std::ostream* os);

}  // namespace math_opt
}  // namespace operations_research
#endif  // OR_TOOLS_MATH_OPT_VALIDATORS_ENUM_SETS_H_

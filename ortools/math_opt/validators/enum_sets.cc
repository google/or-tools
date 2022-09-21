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

#include "ortools/math_opt/validators/enum_sets.h"

#include <ostream>
#include <string>
#include <tuple>
#include <vector>

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/port/proto_utils.h"

namespace operations_research {
namespace math_opt {

// portable.
std::vector<FeasibilityStatusProto> AllFeasibilityStatuses() {
  std::vector<FeasibilityStatusProto> values;
  for (int f = FeasibilityStatusProto_MIN; f <= FeasibilityStatusProto_MAX;
       ++f) {
    if (FeasibilityStatusProto_IsValid(f) &&
        f != FEASIBILITY_STATUS_UNSPECIFIED) {
      values.push_back(static_cast<FeasibilityStatusProto>(f));
    }
  }
  return values;
}

std::vector<SolutionStatusProto> AllSolutionStatuses() {
  std::vector<SolutionStatusProto> values;
  for (int f = SolutionStatusProto_MIN; f <= SolutionStatusProto_MAX; ++f) {
    if (SolutionStatusProto_IsValid(f) && f != SOLUTION_STATUS_UNSPECIFIED) {
      values.push_back(static_cast<SolutionStatusProto>(f));
    }
  }
  return values;
}

void PrintTo(const SolutionStatusProto& proto, std::ostream* os) {
  *os << ProtoEnumToString(proto);
}

void PrintTo(const FeasibilityStatusProto& proto, std::ostream* os) {
  *os << ProtoEnumToString(proto);
}

void PrintTo(const std::tuple<SolutionStatusProto, SolutionStatusProto>& proto,
             std::ostream* os) {
  *os << ProtoEnumToString(std::get<0>(proto)) << "_"
      << ProtoEnumToString(std::get<1>(proto));
}

void PrintTo(
    const std::tuple<FeasibilityStatusProto, FeasibilityStatusProto>& proto,
    std::ostream* os) {
  *os << ProtoEnumToString(std::get<0>(proto)) << "_"
      << ProtoEnumToString(std::get<1>(proto));
}

void PrintTo(
    const std::tuple<SolutionStatusProto, FeasibilityStatusProto>& proto,
    std::ostream* os) {
  *os << ProtoEnumToString(std::get<0>(proto)) << "_"
      << ProtoEnumToString(std::get<1>(proto));
}

}  // namespace math_opt
}  // namespace operations_research

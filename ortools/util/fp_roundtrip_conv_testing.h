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

#ifndef OR_TOOLS_UTIL_FP_ROUNDTRIP_CONV_TESTING_H_
#define OR_TOOLS_UTIL_FP_ROUNDTRIP_CONV_TESTING_H_

#include "absl/strings/string_view.h"

namespace operations_research {

// Number that can be used in unit tests to validate that RoundTripDoubleFormat
// is actually used by the underlying code.
//
// This value has a unique shortest base-10 representation which is
// "0.10000000000000002": 0.10000000000000001 and 0.10000000000000003
// respectively round to the two adjacent doubles, which shortest
// representations are "0.1" and "0.10000000000000003".
//
// When printed with the default formatting of floating point numbers, this will
// be printed as "0.1".
//
// The kRoundTripTestNumberStr constant can be used to test the output.
inline constexpr double kRoundTripTestNumber = 0.10000000000000002;
inline constexpr absl::string_view kRoundTripTestNumberStr =
    "0.10000000000000002";

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_FP_ROUNDTRIP_CONV_TESTING_H_

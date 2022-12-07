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

// IWYU pragma: private, include "ortools/math_opt/cpp/math_opt.h"
// IWYU pragma: friend "ortools/math_opt/cpp/.*"

#ifndef OR_TOOLS_MATH_OPT_CPP_BASIS_STATUS_H_
#define OR_TOOLS_MATH_OPT_CPP_BASIS_STATUS_H_

#include <cstdint>
#include <optional>

#include "absl/types/span.h"
#include "ortools/math_opt/cpp/enums.h"  // IWYU pragma: export
#include "ortools/math_opt/solution.pb.h"

namespace operations_research::math_opt {

// Status of a variable/constraint in a LP basis.
enum class BasisStatus : int8_t {
  // The variable/constraint is free (it has no finite bounds).
  kFree = BASIS_STATUS_FREE,

  // The variable/constraint is at its lower bound (which must be finite).
  kAtLowerBound = BASIS_STATUS_AT_LOWER_BOUND,

  // The variable/constraint is at its upper bound (which must be finite).
  kAtUpperBound = BASIS_STATUS_AT_UPPER_BOUND,

  // The variable/constraint has identical finite lower and upper bounds.
  kFixedValue = BASIS_STATUS_FIXED_VALUE,

  // The variable/constraint is basic.
  kBasic = BASIS_STATUS_BASIC,
};

MATH_OPT_DEFINE_ENUM(BasisStatus, BASIS_STATUS_UNSPECIFIED);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CPP_BASIS_STATUS_H_

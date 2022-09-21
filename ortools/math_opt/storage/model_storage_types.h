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

#ifndef OR_TOOLS_MATH_OPT_STORAGE_MODEL_STORAGE_TYPES_H_
#define OR_TOOLS_MATH_OPT_STORAGE_MODEL_STORAGE_TYPES_H_

#include <cstdint>

#include "absl/strings/string_view.h"
#include "ortools/base/strong_int.h"

namespace operations_research::math_opt {

DEFINE_STRONG_INT_TYPE(VariableId, int64_t);
DEFINE_STRONG_INT_TYPE(LinearConstraintId, int64_t);
DEFINE_STRONG_INT_TYPE(QuadraticConstraintId, int64_t);
DEFINE_STRONG_INT_TYPE(Sos1ConstraintId, int64_t);
DEFINE_STRONG_INT_TYPE(Sos2ConstraintId, int64_t);
DEFINE_STRONG_INT_TYPE(IndicatorConstraintId, int64_t);
DEFINE_STRONG_INT_TYPE(UpdateTrackerId, int64_t);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_STORAGE_MODEL_STORAGE_TYPES_H_

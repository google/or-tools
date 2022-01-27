// Copyright 2010-2021 Google LLC
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

#ifndef OR_TOOLS_MATH_OPT_VALIDATORS_MODEL_VALIDATOR_H_
#define OR_TOOLS_MATH_OPT_VALIDATORS_MODEL_VALIDATOR_H_

#include "absl/status/status.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"

namespace operations_research {
namespace math_opt {

absl::Status ValidateModel(const ModelProto& model, bool check_names = true);

// Performance: runs in O(size of update).
absl::Status ValidateModelUpdate(const ModelUpdateProto& model_update,
                                 bool check_names = true);

// Performance: runs in O(size of update), allocates at most
// O(#new or deleted variables + #new or deleted linear constraints).
absl::Status ValidateModelUpdateAndSummary(const ModelUpdateProto& model_update,
                                           const ModelSummary& model_summary,
                                           bool check_names = true);

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_VALIDATORS_MODEL_VALIDATOR_H_

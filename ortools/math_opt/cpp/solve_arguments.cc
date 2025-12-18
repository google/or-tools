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

#include "ortools/math_opt/cpp/solve_arguments.h"

#include <functional>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/callback.h"
#include "ortools/math_opt/cpp/model_solve_parameters.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research::math_opt {

absl::Status SolveArguments::CheckModelStorage(
    const ModelStorageCPtr expected_storage) const {
  RETURN_IF_ERROR(model_parameters.CheckModelStorage(expected_storage))
      << "invalid model_parameters";
  RETURN_IF_ERROR(callback_registration.CheckModelStorage(expected_storage))
      << "invalid callback_registration";
  return absl::OkStatus();
}

}  // namespace operations_research::math_opt

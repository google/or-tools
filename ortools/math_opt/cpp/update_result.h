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

#ifndef ORTOOLS_MATH_OPT_CPP_UPDATE_RESULT_H_
#define ORTOOLS_MATH_OPT_CPP_UPDATE_RESULT_H_

#include "ortools/math_opt/model_update.pb.h"

namespace operations_research::math_opt {

// Result of the Update() on an incremental solver.
struct UpdateResult {
  explicit UpdateResult(const bool did_update) : did_update(did_update) {}

  // True if the solver has been successfully updated or if no update was
  // necessary (in which case `update` will be nullopt). False if the solver
  // had to be recreated.
  bool did_update;
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_CPP_UPDATE_RESULT_H_

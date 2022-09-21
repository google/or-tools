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

#include "ortools/graph/linear_assignment.h"

#include <cstdint>

#include "ortools/base/commandlineflags.h"

ABSL_FLAG(int64_t, assignment_alpha, 5,
          "Divisor for epsilon at each Refine "
          "step of LinearSumAssignment.");
ABSL_FLAG(int, assignment_progress_logging_period, 5000,
          "Number of relabelings to do between logging progress messages "
          "when verbose level is 4 or more.");
ABSL_FLAG(bool, assignment_stack_order, true,
          "Process active nodes in stack (as opposed to queue) order.");

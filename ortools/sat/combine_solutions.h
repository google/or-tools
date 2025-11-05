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

#ifndef ORTOOLS_SAT_COMBINE_SOLUTIONS_H_
#define ORTOOLS_SAT_COMBINE_SOLUTIONS_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/model.h"
#include "ortools/sat/synchronization.h"

namespace operations_research {
namespace sat {

// Given a `new_solution` that was created by changing a bit a `base_solution`,
// try to apply the same changes to the other solutions stored in the
// `response_manager` and return any such generated solution that is valid.
std::optional<std::vector<int64_t>> FindCombinedSolution(
    const CpModelProto& model, absl::Span<const int64_t> new_solution,
    absl::Span<const int64_t> base_solution,
    const SharedResponseManager* response_manager, std::string* solution_info);

// This is equivalent to calling SharedResponseManager::NewSolution() then, if
// `base_solution` is non-empty, trying to find a combined solution and calling
// SharedResponseManager::NewSolution() again if an improved solution is found.
struct PushedSolutionPointers {
  std::shared_ptr<const SharedSolutionRepository<int64_t>::Solution>
      pushed_solution;
  // nullptr if no improvement was found.
  std::shared_ptr<const SharedSolutionRepository<int64_t>::Solution>
      improved_solution;
};
PushedSolutionPointers PushAndMaybeCombineSolution(
    SharedResponseManager* response_manager, const CpModelProto& model_proto,
    absl::Span<const int64_t> new_solution, absl::string_view solution_info,
    std::shared_ptr<const SharedSolutionRepository<int64_t>::Solution>
        base_solution);

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_COMBINE_SOLUTIONS_H_

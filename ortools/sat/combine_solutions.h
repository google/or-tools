// Copyright 2010-2024 Google LLC
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

#ifndef OR_TOOLS_SAT_COMBINE_SOLUTIONS_H_
#define OR_TOOLS_SAT_COMBINE_SOLUTIONS_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/types/span.h"
#include "ortools/sat/cp_model.pb.h"
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

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_COMBINE_SOLUTIONS_H_

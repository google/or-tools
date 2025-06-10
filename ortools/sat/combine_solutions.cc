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

#include "ortools/sat/combine_solutions.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/model.h"
#include "ortools/sat/synchronization.h"

namespace operations_research {
namespace sat {

bool TrySolution(const CpModelProto& model, absl::Span<const int64_t> solution,
                 absl::Span<const int64_t> new_solution,
                 absl::Span<const int64_t> base_solution,
                 std::vector<int64_t>* new_combined_solution) {
  new_combined_solution->resize(new_solution.size());
  for (int i = 0; i < new_solution.size(); ++i) {
    if (new_solution[i] != base_solution[i]) {
      // A value that changed that we patch.
      (*new_combined_solution)[i] = new_solution[i];
    } else {
      (*new_combined_solution)[i] = solution[i];
    }
  }
  return SolutionIsFeasible(model, *new_combined_solution);
}

std::optional<std::vector<int64_t>> FindCombinedSolution(
    const CpModelProto& model, absl::Span<const int64_t> new_solution,
    absl::Span<const int64_t> base_solution,
    const SharedResponseManager* response_manager, std::string* solution_info) {
  CHECK_EQ(new_solution.size(), base_solution.size());
  const std::vector<
      std::shared_ptr<const SharedSolutionRepository<int64_t>::Solution>>
      solutions =
          response_manager->SolutionPool().BestSolutions().GetBestNSolutions(
              10);

  for (int sol_idx = 0; sol_idx < solutions.size(); ++sol_idx) {
    std::shared_ptr<const SharedSolutionRepository<int64_t>::Solution> s =
        solutions[sol_idx];
    DCHECK_EQ(s->variable_values.size(), new_solution.size());

    if (s->variable_values == new_solution ||
        s->variable_values == base_solution) {
      continue;
    }
    std::vector<int64_t> new_combined_solution;
    if (TrySolution(model, s->variable_values, new_solution, base_solution,
                    &new_combined_solution)) {
      absl::StrAppend(solution_info, " [combined with: ",
                      std::string_view(solutions[sol_idx]->info).substr(0, 20),
                      "...]");
      return new_combined_solution;
    }
  }
  return std::nullopt;
}

PushedSolutionPointers PushAndMaybeCombineSolution(
    SharedResponseManager* response_manager, const CpModelProto& model_proto,
    absl::Span<const int64_t> new_solution, const std::string& solution_info,
    std::shared_ptr<const SharedSolutionRepository<int64_t>::Solution>
        base_solution) {
  PushedSolutionPointers result = {nullptr, nullptr};
  result.pushed_solution = response_manager->NewSolution(
      new_solution, solution_info, nullptr,
      base_solution == nullptr ? -1 : base_solution->source_id);
  if (base_solution != nullptr) {
    std::string combined_solution_info = solution_info;
    std::optional<std::vector<int64_t>> combined_solution =
        FindCombinedSolution(model_proto, new_solution,
                             base_solution->variable_values, response_manager,
                             &combined_solution_info);
    if (combined_solution.has_value()) {
      result.improved_solution = response_manager->NewSolution(
          combined_solution.value(), combined_solution_info);
    }
  }
  return result;
}

}  // namespace sat
}  // namespace operations_research

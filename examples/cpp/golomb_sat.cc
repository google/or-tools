// Copyright 2010-2018 Google LLC
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

//
// Golomb ruler problem
//
//  find minimal ruler so that the differences between ticks are unique.
//
// First solutions:
//   0, 1
//   0, 1, 3
//   0, 1, 4,  6
//   0, 1, 4,  9, 11
//   0, 1, 4, 10, 12, 17
//   0, 1, 4, 10, 18, 23, 25

#include <cstdio>

#include "google/protobuf/text_format.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"

DEFINE_bool(print, false, "If true, print the minimal solution.");
DEFINE_int32(
    size, 0,
    "Size of the problem. If equal to 0, will test several increasing sizes.");
DEFINE_string(params, "", "Sat parameters.");

static const int kBestSolutions[] = {0, 1, 3, 6, 11, 17, 25, 34, 44, 55, 72, 85,
                                     // just for the optimistics ones, the rest:
                                     106, 127, 151, 177, 199, 216, 246};

static const int kKnownSolutions = 19;

namespace operations_research {
namespace sat {

void GolombRuler(int size) {
  CHECK_GE(size, 1);
  CpModelBuilder cp_model;

  std::vector<IntVar> ticks(size);
  ticks[0] = cp_model.NewConstant(0);
  const int64 max = size * size;
  Domain domain(1, max);
  for (int i = 1; i < size; ++i) {
    ticks[i] = cp_model.NewIntVar(domain);
  }
  std::vector<IntVar> diffs;
  for (int i = 0; i < size; ++i) {
    for (int j = i + 1; j < size; ++j) {
      const IntVar diff = cp_model.NewIntVar(domain);
      cp_model.AddEquality(LinearExpr::Sum({diff, ticks[i]}), ticks[j]);
      diffs.push_back(diff);
    }
  }

  cp_model.AddAllDifferent(diffs);

  // Symmetry breaking.
  if (size > 2) {
    cp_model.AddLessThan(diffs.front(), diffs.back());
  }

  // Objective.
  cp_model.Minimize(ticks.back());

  // Search strategy.
  cp_model.AddDecisionStrategy(ticks, DecisionStrategyProto::CHOOSE_FIRST,
                               DecisionStrategyProto::SELECT_MIN_VALUE);

  Model model;
  SatParameters parameters;
  parameters.set_search_branching(SatParameters::FIXED_SEARCH);
  // Parse the --params flag.
  if (!FLAGS_params.empty()) {
    CHECK(google::protobuf::TextFormat::MergeFromString(FLAGS_params,
                                                        &parameters))
        << FLAGS_params;
  }
  model.Add(NewSatParameters(parameters));
  const CpSolverResponse response = SolveWithModel(cp_model, &model);

  if (response.status() == CpSolverStatus::OPTIMAL) {
    const int64 result = SolutionIntegerValue(response, ticks.back());
    const int64 num_failures = response.num_conflicts();
    printf("N = %d, optimal length = %lld (conflicts:%lld, time=%f s)\n", size, result,
           num_failures, response.wall_time());
    if (size - 1 < kKnownSolutions) {
      CHECK_EQ(result, kBestSolutions[size - 1]);
    }
    if (FLAGS_print) {
      for (int i = 0; i < size; ++i) {
        const int64 tick = SolutionIntegerValue(response, ticks[i]);
        printf("%d ", static_cast<int>(tick));
      }
      printf("\n");
    }
  }
}

}  // namespace sat
}  // namespace operations_research

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_size != 0) {
    operations_research::sat::GolombRuler(FLAGS_size);
  } else {
    for (int n = 1; n < 11; ++n) {
      operations_research::sat::GolombRuler(n);
    }
  }
  return EXIT_SUCCESS;
}

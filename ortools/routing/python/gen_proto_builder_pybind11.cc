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

#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/die_if_null.h"
#include "absl/log/initialize.h"
#include "absl/strings/str_format.h"
#include "ortools/constraint_solver/search_stats.pb.h"
#include "ortools/constraint_solver/solver_parameters.pb.h"
#include "ortools/routing/enums.pb.h"
#include "ortools/routing/heuristic_parameters.pb.h"
#include "ortools/routing/ils.pb.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/util/python/wrappers.h"

namespace operations_research::routing::python {

void ParseAndGenerate() {
  absl::PrintF(
      R"(

// This is a generated file, do not edit.
#if defined(IMPORT_PROTO_WRAPPER_CODE)
%s
#endif  // defined(IMPORT_PROTO_WRAPPER_CODE)
)",
      operations_research::util::python::GeneratePybindCode(
          {ABSL_DIE_IF_NULL(RoutingModelParameters::descriptor()),
           ABSL_DIE_IF_NULL(RoutingSearchParameters::descriptor()),
           ABSL_DIE_IF_NULL(FirstSolutionStrategy::descriptor()),
           ABSL_DIE_IF_NULL(LocalSearchMetaheuristic::descriptor()),
           ABSL_DIE_IF_NULL(RoutingSearchStatus::descriptor()),
           ABSL_DIE_IF_NULL(PerturbationStrategy::descriptor()),
           ABSL_DIE_IF_NULL(CoolingScheduleStrategy::descriptor()),
           ABSL_DIE_IF_NULL(RuinCompositionStrategy::descriptor()),
           ABSL_DIE_IF_NULL(LocalCheapestInsertionParameters::descriptor()),
           ABSL_DIE_IF_NULL(SavingsParameters::descriptor()),
           ABSL_DIE_IF_NULL(GlobalCheapestInsertionParameters::descriptor()),
           ABSL_DIE_IF_NULL(SubSolverStatistics::descriptor())}));
}

}  // namespace operations_research::routing::python

int main(int argc, char* argv[]) {
  // We do not use InitGoogle() to avoid linking with or-tools as this would
  // create a circular dependency.
  absl::InitializeLog();
  absl::SetProgramUsageMessage(argv[0]);
  absl::ParseCommandLine(argc, argv);
  operations_research::routing::python::ParseAndGenerate();
  return 0;
}

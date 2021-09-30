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

#include <numeric>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/file.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#include "ortools/packing/arc_flow_builder.h"
#include "ortools/packing/arc_flow_solver.h"
#include "ortools/packing/vector_bin_packing.pb.h"
#include "ortools/packing/vector_bin_packing_parser.h"

ABSL_FLAG(std::string, input, "", "Vector Bin Packing (.vpb) data file name.");
ABSL_FLAG(std::string, params, "",
          "Parameters in solver specific text format.");
ABSL_FLAG(std::string, solver, "sat", "Solver to use: sat, scip");
ABSL_FLAG(double, time_limit, 900.0, "Time limit in seconds");
ABSL_FLAG(int, threads, 1, "Number of threads");
ABSL_FLAG(bool, display_proto, false, "Print the input protobuf");

namespace operations_research {
void ParseAndSolve(const std::string& filename, const std::string& solver,
                   const std::string& params) {
  std::string problem_name = filename;
  const size_t found = problem_name.find_last_of("/\\");
  if (found != std::string::npos) {
    problem_name = problem_name.substr(found + 1);
  }
  if (absl::EndsWith(problem_name, ".vbp")) {
    // TODO(user): Move naming code to parser.
    problem_name.resize(problem_name.size() - 4);
  }

  packing::vbp::VectorBinPackingProblem data;

  packing::vbp::VbpParser parser;
  if (!parser.ParseFile(filename)) {
    LOG(FATAL) << "Cannot read " << filename;
  }
  data = parser.problem();
  data.set_name(problem_name);

  if (data.max_bins() != 0) {
    LOG(WARNING)
        << "Ignoring max_bins value. The feasibility problem is not supported.";
  }

  LOG(INFO) << "Solving vector packing problem '" << data.name() << "' with "
            << data.item_size() << " item types, and "
            << data.resource_capacity_size() << " dimensions.";
  if (absl::GetFlag(FLAGS_display_proto)) {
    LOG(INFO) << data.DebugString();
  }

  // Build optimization model.
  MPSolver::OptimizationProblemType solver_type;
  MPSolver::ParseSolverType(solver, &solver_type);
  packing::vbp::VectorBinPackingSolution solution =
      packing::SolveVectorBinPackingWithArcFlow(data, solver_type, params,
                                                absl::GetFlag(FLAGS_time_limit),
                                                absl::GetFlag(FLAGS_threads));
  if (!solution.bins().empty()) {
    for (int b = 0; b < solution.bins_size(); ++b) {
      LOG(INFO) << "Bin " << b;
      const packing::vbp::VectorBinPackingOneBinInSolution& bin =
          solution.bins(b);
      for (int i = 0; i < bin.item_indices_size(); ++i) {
        LOG(INFO) << "  - item: " << bin.item_indices(i)
                  << ", copies: " << bin.item_copies(i);
      }
    }
  }
}

}  // namespace operations_research

int main(int argc, char** argv) {
  absl::SetFlag(&FLAGS_logtostderr, true);
  google::InitGoogleLogging(argv[0]);
  absl::ParseCommandLine(argc, argv);
  if (absl::GetFlag(FLAGS_input).empty()) {
    LOG(FATAL) << "Please supply a data file with --input=";
  }

  operations_research::ParseAndSolve(absl::GetFlag(FLAGS_input),
                                     absl::GetFlag(FLAGS_solver),
                                     absl::GetFlag(FLAGS_params));
  return EXIT_SUCCESS;
}

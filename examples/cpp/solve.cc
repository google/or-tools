// Copyright 2010-2014 Google
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

// Command line interface to the MPSolver class.
// See linear_solver.h and kUsageStr below.

#include <cstdio>
#include <string>

#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/file.h"
#include "base/timer.h"
#include "base/strutil.h"
#include "lp_data/mps_reader.h"
#include "glop/proto_utils.h"
#include "linear_solver/linear_solver2.pb.h"
#include "linear_solver/linear_solver.h"
#include "linear_solver/proto_tools.h"

DEFINE_string(input, "", "REQUIRED: Input file name.");
DEFINE_string(solver, "glop",
              "The solver to use: "
              "cbc, clp, glop, glpk, glpk_mip, gurobi, gurobi_mip, scip.");
DEFINE_string(params, "", "Solver specific parameters");
DEFINE_string(forced_mps_format, "",
              "Set to force the mps format to use: free, fixed");

static const char kUsageStr[] =
    "Run MPSolver on the given input file. Many formats are supported: \n"
    "  - a .mps or .mps.gz file,\n"
    "  - an MPModelProto (binary or text, possibly gzipped),\n"
    "  - an MPModelRequest (binary or text, possibly gzipped).\n"
    "MPModelProto and MPModelRequest files can comply with either the "
    "linear_solver.proto or the linear_solver2.proto format.";

namespace operations_research {
namespace {
void Run() {
  // Create the solver and set its parameters.
  MPSolver::OptimizationProblemType type;
  if (FLAGS_solver == "glop") {
    type = MPSolver::GLOP_LINEAR_PROGRAMMING;
#if defined(USE_GLPK)
  } else if (FLAGS_solver == "glpk") {
    type = MPSolver::GLPK_LINEAR_PROGRAMMING;
#endif
#if defined(USE_CLP)
  } else if (FLAGS_solver == "clp") {
    type = MPSolver::CLP_LINEAR_PROGRAMMING;
#endif
#if defined(USE_GUROBI)
  } else if (FLAGS_solver == "gurobi") {
    type = MPSolver::GUROBI_LINEAR_PROGRAMMING;
#endif
#if defined(USE_SCIP)
  } else if (FLAGS_solver == "scip") {
    type = MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING;
#endif
#if defined(USE_GUROBI)
  } else if (FLAGS_solver == "cbc") {
    type = MPSolver::CBC_MIXED_INTEGER_PROGRAMMING;
#endif
#if defined(USE_GLPK)
  } else if (FLAGS_solver == "glpk_mip") {
    type = MPSolver::GLPK_MIXED_INTEGER_PROGRAMMING;
#endif
#if defined(USE_GUROBI)
  } else if (FLAGS_solver == "gurobi_mip") {
    type = MPSolver::GUROBI_MIXED_INTEGER_PROGRAMMING;
#endif
  } else {
    LOG(FATAL) << "Unsupported --solver: " << FLAGS_solver;
  }
  MPSolver solver("command line solver", type);
  if (!FLAGS_params.empty()) {
    CHECK(solver.SetSolverSpecificParametersAsString(FLAGS_params))
        << "Wrong --params format.";
  }
  printf("%-12s: %s\n", "Solver",
         new_proto::MPModelRequest::SolverType_Name(
             static_cast<new_proto::MPModelRequest::SolverType>(
                 solver.ProblemType())).c_str());

  // Load the problem into an MPModelProto.
  new_proto::MPModelProto model_proto;
  new_proto::MPModelRequest request_proto;
  if (HasSuffixString(FLAGS_input, ".mps") ||
      HasSuffixString(FLAGS_input, ".mps.gz")) {
    glop::LinearProgram linear_program_fixed;
    glop::LinearProgram linear_program_free;
    glop::MPSReader mps_reader;
    const bool fixed_read = FLAGS_forced_mps_format != "free" &&
        mps_reader.LoadFileWithMode(FLAGS_input, false, &linear_program_fixed);
    const bool free_read = FLAGS_forced_mps_format != "fixed" &&
        mps_reader.LoadFileWithMode(FLAGS_input, true, &linear_program_free);
    CHECK(fixed_read || free_read)
        << "Error while parsing the mps file '" << FLAGS_input << "'.";
    if (!fixed_read) {
      LinearProgramToMPModelProto(linear_program_free, &model_proto);
    } else {
      LinearProgramToMPModelProto(linear_program_fixed, &model_proto);
    }
  } else if (!ReadFileToProto(FLAGS_input, &model_proto)) {
    LOG(WARNING) << "Failed to parse as an MPModelProto, trying other formats";
    if (ReadFileToProto(FLAGS_input, &request_proto)) {
      model_proto.Swap(request_proto.mutable_model());
    } else {
      LOG(FATAL) << "Failed to parse '" << FLAGS_input
                 << "' as an MPModelProto or an MPModelRequest.";
    }
  }
  printf("%-12s: '%s'\n", "File", FLAGS_input.c_str());

  // Load the proto into the solver.
  MPSolver::LoadStatus load_status = solver.LoadModelFromProto(model_proto);
  CHECK(load_status == MPSolver::NO_ERROR);
  printf("%-12s: %d x %d\n", "Dimension", solver.NumConstraints(),
         solver.NumVariables());

  // Solve.
  MPSolverParameters param;
  MPSolver::ResultStatus solve_status;
  double solving_time_in_sec = 0;
  {
    ScopedWallTime timer(&solving_time_in_sec);
    solve_status = solver.Solve(param);
  }

  printf("%-12s: %s\n", "Status",
         new_proto::MPSolutionResponse::Status_Name(
             static_cast<new_proto::MPSolutionResponse::Status>(solve_status))
             .c_str());
  printf("%-12s: %15.15e\n", "Objective", solver.Objective().Value());
  printf("%-12s: %-6.4g\n", "Time", solving_time_in_sec);
}
}  // namespace
}  // namespace operations_research

int main(int argc, char** argv) {
  google::ParseCommandLineFlags( &argc, &argv, /*remove_flags=*/true);
  CHECK(!FLAGS_input.empty()) << "--input is required";
  operations_research::Run();
}

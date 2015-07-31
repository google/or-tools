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
#include "base/timer.h"
#include "base/file.h"
#include "base/strutil.h"
#include "glop/proto_utils.h"
#include "linear_solver/linear_solver.h"
#include "linear_solver/linear_solver.pb.h"
#include "lp_data/mps_reader.h"
#include "util/proto_tools.h"

DEFINE_string(input, "", "REQUIRED: Input file name.");
DEFINE_string(solver, "glop",
              "The solver to use: "
              "cbc, clp, cplex, cplex_mip, glop, glpk_lp, glpk_mip,"
	          " gurobi_lp, gurobi_mip, scip.");
DEFINE_string(params, "", "Solver specific parameters");
DEFINE_int64(time_limit_ms, 0,
             "If stricitly positive, specifies a limit in ms on the solving"
             " time.");
DEFINE_string(forced_mps_format, "",
              "Set to force the mps format to use: free, fixed");

DEFINE_string(output, "",
              "If non-empty, write the MPSolverResponse there. "
              "The format will be binary except if the name end with '.txt'.");
DEFINE_string(output_csv, "",
              "If non-empty, write the returned solution in csv format with "
              "each line formed by a variable name and its value.");

static const char kUsageStr[] =
    "Run MPSolver on the given input file. Many formats are supported: \n"
    "  - a .mps or .mps.gz file,\n"
    "  - an MPModelProto (binary or text, possibly gzipped),\n"
    "  - an MPModelRequest (binary or text, possibly gzipped).\n"
    "MPModelProto and MPModelRequest files can comply with either the "
    "linear_solver.proto or the linear_solver.proto format.";

namespace operations_research {
namespace {


void Run() {
  // Create the solver and set its parameters.
  MPSolver::OptimizationProblemType type;
  if (FLAGS_solver == "glop") {
    type = MPSolver::GLOP_LINEAR_PROGRAMMING;
#if defined(USE_GLPK)
  } else if (FLAGS_solver == "glpk_lp") {
    type = MPSolver::GLPK_LINEAR_PROGRAMMING;
#endif
#if defined(USE_CLP)
  } else if (FLAGS_solver == "clp") {
    type = MPSolver::CLP_LINEAR_PROGRAMMING;
#endif
#if defined(USE_CPLEX)
  } else if (FLAGS_solver == "cplex") {
    type = MPSolver::CPLEX_LINEAR_PROGRAMMING;
#endif
#if defined(USE_GUROBI)
  } else if (FLAGS_solver == "gurobi_lp") {
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
#if defined(USE_CPLEX)
  } else if (FLAGS_solver == "cplex_mip") {
    type = MPSolver::CPLEX_MIXED_INTEGER_PROGRAMMING;
#endif
#if defined(USE_GUROBI)
  } else if (FLAGS_solver == "gurobi_mip") {
    type = MPSolver::GUROBI_MIXED_INTEGER_PROGRAMMING;
#endif
#if defined(USE_BOP)
  } else if (FLAGS_solver == "bop") {
    LOG(WARNING) << "This version of BOP needs a time limit to be set via "
                 << "the --params flag. Ex: "
                 << "--params=max_time_in_seconds:10,log_search_progress:true";
    type = MPSolver::BOP_INTEGER_PROGRAMMING;
#endif
  } else {
    LOG(FATAL) << "Unsupported --solver: " << FLAGS_solver;
  }
  MPSolver solver("command line solver", type);
  solver.EnableOutput();
  if (!FLAGS_params.empty()) {
    CHECK(solver.SetSolverSpecificParametersAsString(FLAGS_params))
        << "Wrong --params format.";
  }
  printf("%-12s: %s\n", "Solver", MPModelRequest::SolverType_Name(
                                      static_cast<MPModelRequest::SolverType>(
                                          solver.ProblemType())).c_str());

  // Load the problem into an MPModelProto.
  MPModelProto model_proto;
  MPModelRequest request_proto;
  if (HasSuffixString(FLAGS_input, ".mps") ||
      HasSuffixString(FLAGS_input, ".mps.gz")) {
    glop::LinearProgram linear_program_fixed;
    glop::LinearProgram linear_program_free;
    glop::MPSReader mps_reader;
    mps_reader.set_log_errors(FLAGS_forced_mps_format == "free" ||
                              FLAGS_forced_mps_format == "fixed");
    const bool fixed_read = FLAGS_forced_mps_format != "free" &&
        mps_reader.LoadFileWithMode(FLAGS_input, false, &linear_program_fixed);
    const bool free_read = FLAGS_forced_mps_format != "fixed" &&
        mps_reader.LoadFileWithMode(FLAGS_input, true, &linear_program_free);
    CHECK(fixed_read || free_read)
        << "Error while parsing the mps file '" << FLAGS_input << "' "
        << "Use the --forced_mps_format flags to see the errors.";
    if (!fixed_read) {
      LOG(INFO) << "Read file in free format.";
      LinearProgramToMPModelProto(linear_program_free, &model_proto);
    } else {
      LOG(INFO) << "Read file in fixed format.";
      LinearProgramToMPModelProto(linear_program_fixed, &model_proto);
    }
  } else {
    ReadFileToProto(FLAGS_input, &model_proto);
    ReadFileToProto(FLAGS_input, &request_proto);
    // If the input proto is in binary format, both ReadFileToProto could return
    // true. Instead use the actual number of variables found to test the
    // correct format of the input.
    const bool is_model_proto = model_proto.variable_size() > 0;
    const bool is_request_proto = request_proto.model().variable_size() > 0;
    if (!is_model_proto && !is_request_proto) {
      LOG(FATAL) << "Failed to parse '" << FLAGS_input
                 << "' as an MPModelProto or an MPModelRequest.";
    } else {
      CHECK(!(is_model_proto && is_request_proto));
      if (is_request_proto) {
        LOG(INFO) << "Read input proto as an MPModelRequest.";
        if (request_proto.has_solver_time_limit_seconds()) {
          solver.set_time_limit(static_cast<int64>(
              request_proto.solver_time_limit_seconds() * 1000));
        }
        model_proto.Swap(request_proto.mutable_model());
      } else {
        LOG(INFO) << "Read input proto as an MPModelProto.";
      }
    }
  }
  printf("%-12s: '%s'\n", "File", FLAGS_input.c_str());

  // Load the proto into the solver.
  std::string error_message;
  const MPSolverResponseStatus status =
      solver.LoadModelFromProto(model_proto, &error_message);
  if (FLAGS_time_limit_ms >= 0) {
    solver.set_time_limit(FLAGS_time_limit_ms);
  }
  CHECK_EQ(MPSOLVER_MODEL_IS_VALID, status)
      << MPSolverResponseStatus_Name(status) << ": " << error_message;
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

  // If requested, get the solver result and output it.
  if (!FLAGS_output.empty()) {
    operations_research::MPSolutionResponse result;
    solver.FillSolutionResponseProto(&result);
    if (HasSuffixString(FLAGS_output, ".txt")) {
      CHECK_OK(file::SetTextProto(FLAGS_output, result, file::Defaults()));
    } else {
      CHECK_OK(file::SetBinaryProto(FLAGS_output, result, file::Defaults()));
    }
  }
  if (!FLAGS_output_csv.empty()) {
    operations_research::MPSolutionResponse result;
    solver.FillSolutionResponseProto(&result);
    std::string csv_file;
    for (int i = 0; i < result.variable_value_size(); ++i) {
      csv_file +=
          StringPrintf("%s,%e\n", model_proto.variable(i).name().c_str(),
                       result.variable_value(i));
    }
    CHECK_OK(file::SetContents(FLAGS_output_csv, csv_file, file::Defaults()));
  }

  printf("%-12s: %s\n", "Status",
         MPSolverResponseStatus_Name(
             static_cast<MPSolverResponseStatus>(solve_status)).c_str());
  printf("%-12s: %15.15e\n", "Objective", solver.Objective().Value());
  printf("%-12s: %-6.4g\n", "Time", solving_time_in_sec);
}
}  // namespace
}  // namespace operations_research

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, /*remove_flags=*/true);
  CHECK(!FLAGS_input.empty()) << "--input is required";
  operations_research::Run();
}

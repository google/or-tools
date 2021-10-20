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

// Command line interface to the MPSolver class.
// See linear_solver.h and kUsageStr below.
//
// Examples.
//
// 1. To run SCIP for 90 seconds, dumping available information use:
//
// solve --solver=scip \
//       --time_limit=90s \
//       --logtostderr \
//       --linear_solver_enable_verbose_output \
//       --input=/tmp/foo.mps \
//       --dump_model=/tmp/foo.model \
//       --dump_request=/tmp/foo.request \
//       --dump_response=/tmp/foo.response \
//       >/tmp/foo.out 2>/tmp/foo.err
//
// 2. To run CP_SAT for 10 minutes with 8 workers, you can use
//    CP-SAT parameters:
//
// solve --solver=sat \
//       --params="max_time_in_seconds:600, num_search_workers:8"
//       --logtostderr \
//       --input=/tmp/foo.mps \
//       2>/tmp/foo.err
//
//     or use the solve binary flags:
//
// solve --solver=sat \
//       --time_limit=10m \
//       --num_threads=8 \
//       --logtostderr \
//       --input=/tmp/foo.mps \
//       --dump_model=/tmp/foo.model \
//       --dump_request=/tmp/foo.request \
//       --dump_response=/tmp/foo.response \
//       2>/tmp/foo.err

#include <cstdio>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/file.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/mps_reader.h"
#include "ortools/lp_data/proto_utils.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/util/file_util.h"
#include "ortools/util/sigint.h"

ABSL_FLAG(std::string, input, "", "REQUIRED: Input file name.");
ABSL_FLAG(std::string, solver, "glop",
          "The solver to use: bop, cbc, clp, glop, glpk_lp, glpk_mip, "
          "gurobi_lp, gurobi_mip, scip, knapsack, sat.");
ABSL_FLAG(int, num_threads, 1,
          "Number of threads to use by the underlying solver.");
ABSL_FLAG(std::string, params_file, "",
          "Solver specific parameters file. "
          "If this flag is set, the --params flag is ignored.");
ABSL_FLAG(std::string, params, "", "Solver specific parameters");
ABSL_FLAG(absl::Duration, time_limit, absl::InfiniteDuration(),
          "It specifies a limit on the solving time. The duration must be must "
          "be positive. It default to an infinite duration meaning that no "
          "time limit will be imposed.");
ABSL_FLAG(std::string, output_csv, "",
          "If non-empty, write the returned solution in csv format with "
          "each line formed by a variable name and its value.");

ABSL_FLAG(std::string, dump_format, "text",
          "Format in which to dump protos (if flags --dump_model, "
          "--dump_request, or --dump_response are used). Possible values: "
          "'text', 'binary', 'json' which correspond to text proto format "
          "binary proto format, and json. If 'binary' or 'json' are used, "
          "we append '.bin' and '.json' to file names.");
ABSL_FLAG(bool, dump_gzip, false,
          "Whether to gzip dumped protos. Appends .gz to their name.");
ABSL_FLAG(std::string, dump_model, "",
          "If non-empty, dumps MPModelProto there.");
ABSL_FLAG(std::string, dump_request, "",
          "If non-empty, dumps MPModelRequest there.");
ABSL_FLAG(std::string, dump_response, "",
          "If non-empty, dumps MPSolutionResponse there.");
ABSL_FLAG(std::string, sol_file, "",
          "If non-empty, output the best solution in Miplib .sol format.");

ABSL_DECLARE_FLAG(bool, verify_solution);  // Defined in ./linear_solver.cc
ABSL_DECLARE_FLAG(
    bool,
    linear_solver_enable_verbose_output);  // Defined in ./linear_solver.cc

static const char kUsageStr[] =
    "Run MPSolver on the given input file. Many formats are supported: \n"
    "  - a .mps or .mps.gz file,\n"
    "  - an MPModelProto (binary or text, possibly gzipped),\n"
    "  - an MPModelRequest (binary or text, possibly gzipped).";

namespace operations_research {
namespace {

MPModelRequest ReadMipModel(const std::string& input) {
  MPModelRequest request_proto;
  MPModelProto model_proto;
  if (absl::EndsWith(input, ".mps") || absl::EndsWith(input, ".mps.gz")) {
    QCHECK_OK(glop::MPSReader().ParseFile(input, &model_proto))
        << "Error while parsing the mps file '" << input << "'.";
  } else {
    ReadFileToProto(input, &model_proto);
    ReadFileToProto(input, &request_proto);
  }
  // If the input is a proto in binary format, both ReadFileToProto could
  // return true. Instead use the actual number of variables found to test the
  // correct format of the input.
  const bool is_model_proto = model_proto.variable_size() > 0;
  const bool is_request_proto = request_proto.model().variable_size() > 0;
  if (!is_model_proto && !is_request_proto) {
    LOG(FATAL) << "Failed to parse '" << input
               << "' as an MPModelProto or an MPModelRequest.";
  } else {
    CHECK(!(is_model_proto && is_request_proto));
  }
  if (is_request_proto) {
    LOG(INFO) << "Read input proto as an MPModelRequest.";
  } else {
    LOG(INFO) << "Read input proto as an MPModelProto.";
    model_proto.Swap(request_proto.mutable_model());
  }
  return request_proto;
}

// Returns false if an error was encountered.
// More details should be available in the logs.
bool Run(MPSolver::OptimizationProblemType type) {
  MPModelRequest request_proto = ReadMipModel(absl::GetFlag(FLAGS_input));

  printf("%-12s: '%s'\n", "File", absl::GetFlag(FLAGS_input).c_str());

  // Detect format to dump protos.
  operations_research::ProtoWriteFormat write_format;
  if (absl::GetFlag(FLAGS_dump_format) == "text") {
    write_format = ProtoWriteFormat::kProtoText;
  } else if (absl::GetFlag(FLAGS_dump_format) == "binary") {
    write_format = ProtoWriteFormat::kProtoBinary;
  } else if (absl::GetFlag(FLAGS_dump_format) == "json") {
    write_format = ProtoWriteFormat::kJson;
  } else {
    LOG(FATAL) << "Unsupported --dump_format: "
               << absl::GetFlag(FLAGS_dump_format);
  }

  // Create the solver, we use the name of the model as the solver name.
  MPSolver solver(request_proto.model().name(), type);
  const absl::Status set_num_threads_status =
      solver.SetNumThreads(absl::GetFlag(FLAGS_num_threads));
  if (set_num_threads_status.ok()) {
    LOG(INFO) << "Set number of threads to " << absl::GetFlag(FLAGS_num_threads)
              << ".";
  } else if (absl::GetFlag(FLAGS_num_threads) != 1) {
    LOG(ERROR) << "Failed to set number of threads due to: "
               << set_num_threads_status.message() << ". Using 1 as default.";
  }
  solver.EnableOutput();
  if (!absl::GetFlag(FLAGS_params_file).empty()) {
    std::string file_contents;
    CHECK_OK(file::GetContents(absl::GetFlag(FLAGS_params_file), &file_contents,
                               file::Defaults()))
        << "Could not read parameters file.";
    CHECK(solver.SetSolverSpecificParametersAsString(file_contents));
  } else if (!absl::GetFlag(FLAGS_params).empty()) {
    CHECK(
        solver.SetSolverSpecificParametersAsString(absl::GetFlag(FLAGS_params)))
        << "Wrong --params format.";
  }
  absl::PrintF(
      "%-12s: %s\n", "Solver",
      MPModelRequest::SolverType_Name(
          static_cast<MPModelRequest::SolverType>(solver.ProblemType()))
          .c_str());

  // Load the proto into the solver.
  std::string error_message;

  // If requested, save the model to file.
  if (!absl::GetFlag(FLAGS_dump_model).empty()) {
    CHECK(WriteProtoToFile(absl::GetFlag(FLAGS_dump_model),
                           request_proto.model(), write_format,
                           absl::GetFlag(FLAGS_dump_gzip)));
  }

  const MPSolverResponseStatus status =
      solver.LoadModelFromProtoWithUniqueNamesOrDie(request_proto.model(),
                                                    &error_message);
  // Note, the underlying MPSolver treats time limit equal to 0 as no limit.
  if (status != MPSOLVER_MODEL_IS_VALID) {
    LOG(ERROR) << MPSolverResponseStatus_Name(status) << ": " << error_message;
    return false;
  }

  // Time limits.
  if (absl::GetFlag(FLAGS_time_limit) != absl::InfiniteDuration()) {
    LOG(INFO) << "Setting a time limit of " << absl::GetFlag(FLAGS_time_limit);
    // Overwrite the request time limit.
    request_proto.set_solver_time_limit_seconds(
        absl::ToDoubleSeconds(absl::GetFlag(FLAGS_time_limit)));
  }
  if (request_proto.has_solver_time_limit_seconds()) {
    solver.SetTimeLimit(
        absl::Seconds(request_proto.solver_time_limit_seconds()));
  }

  absl::PrintF("%-12s: %d x %d\n", "Dimension", solver.NumConstraints(),
               solver.NumVariables());

  // Register a signal handler to interrupt the solve when the user presses ^C.
  // Note that we ignore all previously registered handler here. If SCIP is
  // used, this handler will be overridden by the one of SCIP that does the same
  // thing.
  SigintHandler handler;
  handler.Register([&solver] { solver.InterruptSolve(); });

  // Solve.
  MPSolverParameters param;
  MPSolver::ResultStatus solve_status = MPSolver::NOT_SOLVED;
  absl::Duration solving_time;
  const absl::Time time_before = absl::Now();
  solve_status = solver.Solve(param);
  solving_time = absl::Now() - time_before;

  // If requested, re-create a corresponding MPModelRequest and save it to file.
  if (!absl::GetFlag(FLAGS_dump_request).empty()) {
    request_proto.set_solver_type(
        static_cast<MPModelRequest::SolverType>(solver.ProblemType()));
    request_proto.set_solver_time_limit_seconds(solver.time_limit_in_secs());
    request_proto.set_solver_specific_parameters(
        solver.GetSolverSpecificParametersAsString());
    CHECK(WriteProtoToFile(absl::GetFlag(FLAGS_dump_request), request_proto,
                           write_format, absl::GetFlag(FLAGS_dump_gzip)));
  }

  const bool has_solution =
      solve_status == MPSolver::OPTIMAL || solve_status == MPSolver::FEASIBLE;

  if (!absl::GetFlag(FLAGS_sol_file).empty() && has_solution) {
    operations_research::MPSolutionResponse response;
    solver.FillSolutionResponseProto(&response);
    std::string sol_string;
    absl::StrAppend(&sol_string, "=obj= ", response.objective_value(), "\n");
    for (int i = 0; i < response.variable_value().size(); ++i) {
      absl::StrAppend(&sol_string, request_proto.model().variable(i).name(),
                      " ", response.variable_value(i), "\n");
    }
    LOG(INFO) << "Writing .sol solution to '" << absl::GetFlag(FLAGS_sol_file)
              << "'.\n";
    CHECK_OK(file::SetContents(absl::GetFlag(FLAGS_sol_file), sol_string,
                               file::Defaults()));
  }

  // If requested, get the MPSolutionResponse and save it to file.
  if (!absl::GetFlag(FLAGS_dump_response).empty() && has_solution) {
    operations_research::MPSolutionResponse response;
    solver.FillSolutionResponseProto(&response);
    CHECK(WriteProtoToFile(absl::GetFlag(FLAGS_dump_response), response,
                           write_format, absl::GetFlag(FLAGS_dump_gzip)));
  }
  if (!absl::GetFlag(FLAGS_output_csv).empty() && has_solution) {
    operations_research::MPSolutionResponse result;
    solver.FillSolutionResponseProto(&result);
    std::string csv_file;
    for (int i = 0; i < result.variable_value_size(); ++i) {
      csv_file +=
          absl::StrFormat("%s,%e\n", request_proto.model().variable(i).name(),
                          result.variable_value(i));
    }
    CHECK_OK(file::SetContents(absl::GetFlag(FLAGS_output_csv), csv_file,
                               file::Defaults()));
  }
  // If --verify_solution is true, we already verified it. If not, we add
  // a verification step here.
  if (has_solution && !absl::GetFlag(FLAGS_verify_solution)) {
    LOG(INFO) << "Verifying the solution";
    solver.VerifySolution(/*tolerance=*/param.GetDoubleParam(
                              MPSolverParameters::PRIMAL_TOLERANCE),
                          /*log_errors=*/true);
  }

  absl::PrintF("%-12s: %s\n", "Status",
               MPSolverResponseStatus_Name(
                   static_cast<MPSolverResponseStatus>(solve_status))
                   .c_str());
  absl::PrintF("%-12s: %15.15e\n", "Objective",
               has_solution ? solver.Objective().Value() : 0.0);
  absl::PrintF("%-12s: %15.15e\n", "BestBound",
               has_solution ? solver.Objective().BestBound() : 0.0);
  absl::PrintF("%-12s: %d\n", "Iterations", solver.iterations());
  // NOTE(user): nodes() for non-MIP solvers crashes in debug mode by design.
  if (solver.IsMIP()) {
    absl::PrintF("%-12s: %d\n", "Nodes", solver.nodes());
  }
  absl::PrintF("%-12s: %-6.4g\n", "Time", absl::ToDoubleSeconds(solving_time));
  return true;
}

}  // namespace
}  // namespace operations_research

int main(int argc, char** argv) {
  absl::SetFlag(&FLAGS_logtostderr, true);
  google::InitGoogleLogging(kUsageStr);
  absl::ParseCommandLine(argc, argv);
  QCHECK(!absl::GetFlag(FLAGS_input).empty()) << "--input is required";
  QCHECK_GE(absl::GetFlag(FLAGS_time_limit), absl::ZeroDuration())
      << "--time_limit must be given a positive duration";

  operations_research::MPSolver::OptimizationProblemType type;
  CHECK(operations_research::MPSolver::ParseSolverType(
      absl::GetFlag(FLAGS_solver), &type))
      << "Unsupported --solver: " << absl::GetFlag(FLAGS_solver);

  if (!operations_research::Run(type)) {
    // If the solver is SAT and we encountered an error, display it in a format
    // interpretable by our scripts.
    if (type == operations_research::MPSolver::SAT_INTEGER_PROGRAMMING) {
      operations_research::sat::CpSolverResponse response;
      response.set_status(
          operations_research::sat::CpSolverStatus::MODEL_INVALID);
      LOG(INFO) << operations_research::sat::CpSolverResponseStats(response);
    }
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

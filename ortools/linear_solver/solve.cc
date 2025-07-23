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

// Command line interface to the MPSolver class.
// See linear_solver.h and kUsageStr below.
//
// Examples.
//
// 1. To run SCIP for 90 seconds, dumping available information use:
//
// solve --solver=scip \
//       --time_limit=90s \
//       --stderrthreshold=0 \
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
//       --params="max_time_in_seconds:600, num_workers:8"
//       --stderrthreshold=0 \
//       --input=/tmp/foo.mps \
//       2>/tmp/foo.err
//
//     or use the solve binary flags:
//
// solve --solver=sat \
//       --time_limit=10m \
//       --num_threads=8 \
//       --stderrthreshold=0 \
//       --input=/tmp/foo.mps \
//       --dump_model=/tmp/foo.model \
//       --dump_request=/tmp/foo.request \
//       --dump_response=/tmp/foo.response \
//       2>/tmp/foo.err

#include <cstdio>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "ortools/base/file.h"
#include "ortools/base/helpers.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/base/options.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_exporter.h"
#include "ortools/lp_data/lp_parser.h"
#include "ortools/lp_data/mps_reader.h"
#include "ortools/lp_data/sol_reader.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/util/file_util.h"
#include "ortools/util/sigint.h"

ABSL_FLAG(std::string, input, "", "REQUIRED: Input file name.");
ABSL_FLAG(std::string, sol_hint, "",
          "Input file name with solution in .sol format.");
ABSL_FLAG(std::optional<std::string>, solver, std::nullopt,
          "The solver to use: bop, cbc, clp, glop, glpk_lp, glpk_mip, "
          "gurobi_lp, gurobi_mip, pdlp, scip, knapsack, sat. If unspecified "
          "either use MPModelRequest.solver_type if the --input is an "
          "MPModelRequest and the field is set or use glop.");
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
ABSL_FLAG(std::string, dump_mps, "",
          "If non-empty, dumps the model in mps format there.");

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
  if (absl::EndsWith(input, ".lp")) {
    std::string data;
    CHECK_OK(file::GetContents(input, &data, file::Defaults()));
    absl::StatusOr<MPModelProto> result = ModelProtoFromLpFormat(data);
    CHECK_OK(result);
    model_proto = std::move(result).value();
  } else if (absl::EndsWith(input, ".mps") ||
             absl::EndsWith(input, ".mps.gz")) {
    QCHECK_OK(glop::MPSReader().ParseFile(input, &model_proto))
        << "Error while parsing the mps file '" << input << "'.";
  } else {
    ReadFileToProto(input, &model_proto).IgnoreError();
    ReadFileToProto(input, &request_proto).IgnoreError();
  }
  // If the input is a proto in binary format, both ReadFileToProto could
  // return true. Instead use the actual number of variables found to test the
  // correct format of the input.
  const bool is_model_proto = model_proto.variable_size() > 0;
  const bool is_request_proto =
      request_proto.model().variable_size() > 0 ||
      !request_proto.model_delta().baseline_model_file_path().empty();
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

MPSolutionResponse LocalSolve(const MPModelRequest& request_proto) {
  // TODO(or-core-team): Why doesn't this use MPSolver::SolveWithProto() ?

  // Create the solver, we use the name of the model as the solver name.
  MPSolver solver(request_proto.model().name(),
                  static_cast<MPSolver::OptimizationProblemType>(
                      request_proto.solver_type()));
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

  if (request_proto.has_solver_specific_parameters()) {
    CHECK(solver.SetSolverSpecificParametersAsString(
        request_proto.solver_specific_parameters()))
        << "Wrong solver_specific_parameters (bad --params or --params_file ?)";
  }

  MPSolutionResponse response;

  // Load the model proto into the solver.
  {
    std::string error_message;
    const MPSolverResponseStatus status =
        solver.LoadModelFromProtoWithUniqueNamesOrDie(request_proto.model(),
                                                      &error_message);
    // Note, the underlying MPSolver treats time limit equal to 0 as no limit.
    if (status != MPSOLVER_MODEL_IS_VALID) {
      // HACK(user): For SAT solves, when the model is invalid we directly
      // exit here.
      if (request_proto.solver_type() ==
          MPModelRequest::SAT_INTEGER_PROGRAMMING) {
        sat::CpSolverResponse sat_response;
        sat_response.set_status(sat::CpSolverStatus::MODEL_INVALID);
        LOG(INFO) << sat::CpSolverResponseStats(sat_response);
        exit(1);
      }
      response.set_status(status);
      response.set_status_str(error_message);
      return response;
    }
  }
  if (request_proto.has_solver_time_limit_seconds()) {
    solver.SetTimeLimit(
        absl::Seconds(request_proto.solver_time_limit_seconds()));
  }

  // Register a signal handler to interrupt the solve when the user presses ^C.
  // Note that we ignore all previously registered handler here. If SCIP is
  // used, this handler will be overridden by the one of SCIP that does the same
  // thing.
  SigintHandler handler;
  handler.Register([&solver] { solver.InterruptSolve(); });

  // Solve.
  const MPSolver::ResultStatus status = solver.Solve();

  // If --verify_solution is true, we already verified it. If not, we add
  // a verification step here.
  if ((status == MPSolver::OPTIMAL || status == MPSolver::FEASIBLE) &&
      !absl::GetFlag(FLAGS_verify_solution)) {
    const bool verified =
        solver.VerifySolution(/*tolerance=*/MPSolverParameters().GetDoubleParam(
                                  MPSolverParameters::PRIMAL_TOLERANCE),
                              absl::GetFlag(FLAGS_log_verification_errors));
    LOG(INFO) << "The solution "
              << (verified ? "was verified." : "didn't pass verification.");
  }

  // If the solver is a MIP, print the number of nodes.
  // TODO(user): add the number of nodes to the response, and move this code
  // to the main Run().
  if (SolverTypeIsMip(request_proto.solver_type())) {
    absl::PrintF("%-12s: %d\n", "Nodes", solver.nodes());
  }

  // Fill and return the response proto.
  solver.FillSolutionResponseProto(&response);
  return response;
}

void Run() {
  QCHECK(!absl::GetFlag(FLAGS_input).empty()) << "--input is required";
  QCHECK_GE(absl::GetFlag(FLAGS_time_limit), absl::ZeroDuration())
      << "--time_limit must be given a positive duration";

  // Parses --solver if set.
  std::optional<MPSolver::OptimizationProblemType> type;
  if (const std::optional<std::string> type_flag = absl::GetFlag(FLAGS_solver);
      type_flag.has_value()) {
    MPSolver::OptimizationProblemType decoded_type;
    QCHECK(MPSolver::ParseSolverType(type_flag.value(), &decoded_type))
        << "Unsupported --solver: " << type_flag.value();
    type = decoded_type;
  }

  MPModelRequest request_proto = ReadMipModel(absl::GetFlag(FLAGS_input));

  if (!absl::GetFlag(FLAGS_sol_hint).empty()) {
    const auto read_sol =
        ParseSolFile(absl::GetFlag(FLAGS_sol_hint), request_proto.model());
    CHECK_OK(read_sol.status());
    const MPSolutionResponse& sol = read_sol.value();
    if (request_proto.model().has_solution_hint()) {
      LOG(WARNING) << "Overwriting solution hint found in the request with "
                   << "solution from " << absl::GetFlag(FLAGS_sol_hint);
    }
    request_proto.mutable_model()->clear_solution_hint();
    for (int i = 0; i < sol.variable_value_size(); ++i) {
      request_proto.mutable_model()->mutable_solution_hint()->add_var_index(i);
      request_proto.mutable_model()->mutable_solution_hint()->add_var_value(
          sol.variable_value(i));
    }
  }

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

  if (!absl::GetFlag(FLAGS_dump_mps).empty()) {
    CHECK_OK(WriteModelToMpsFile(absl::GetFlag(FLAGS_dump_mps),
                                 request_proto.model()));
  }

  // Set or override request proto options from the command line flags.
  if (type.has_value() || !request_proto.has_solver_type()) {
    request_proto.set_solver_type(static_cast<MPModelRequest::SolverType>(
        type.value_or(MPSolver::GLOP_LINEAR_PROGRAMMING)));
  }
  if (absl::GetFlag(FLAGS_time_limit) != absl::InfiniteDuration()) {
    LOG(INFO) << "Setting a time limit of " << absl::GetFlag(FLAGS_time_limit);
    request_proto.set_solver_time_limit_seconds(
        absl::ToDoubleSeconds(absl::GetFlag(FLAGS_time_limit)));
  }
  if (absl::GetFlag(FLAGS_linear_solver_enable_verbose_output)) {
    request_proto.set_enable_internal_solver_output(true);
  }
  if (!absl::GetFlag(FLAGS_params_file).empty()) {
    CHECK(absl::GetFlag(FLAGS_params).empty())
        << "--params and --params_file are incompatible";
    std::string file_contents;
    CHECK_OK(file::GetContents(absl::GetFlag(FLAGS_params_file), &file_contents,
                               file::Defaults()))
        << "Could not read parameters file.";
    request_proto.set_solver_specific_parameters(file_contents);
  }
  if (!absl::GetFlag(FLAGS_params).empty()) {
    request_proto.set_solver_specific_parameters(absl::GetFlag(FLAGS_params));
  }

  // If requested, save the model and/or request to file.
  if (!absl::GetFlag(FLAGS_dump_model).empty()) {
    CHECK_OK(WriteProtoToFile(absl::GetFlag(FLAGS_dump_model),
                              request_proto.model(), write_format,
                              absl::GetFlag(FLAGS_dump_gzip)));
  }
  if (!absl::GetFlag(FLAGS_dump_request).empty()) {
    CHECK_OK(WriteProtoToFile(absl::GetFlag(FLAGS_dump_request), request_proto,
                              write_format, absl::GetFlag(FLAGS_dump_gzip)));
  }

  absl::PrintF(
      "%-12s: %s\n", "Solver",
      MPModelRequest::SolverType_Name(request_proto.solver_type()).c_str());
  absl::PrintF("%-12s: %s\n", "Parameters", absl::GetFlag(FLAGS_params));
  absl::PrintF("%-12s: %d x %d\n", "Dimension",
               request_proto.model().constraint_size(),
               request_proto.model().variable_size());

  const absl::Time solve_start_time = absl::Now();

  const MPSolutionResponse response = LocalSolve(request_proto);

  const absl::Duration solving_time = absl::Now() - solve_start_time;
  const bool has_solution = response.status() == MPSOLVER_OPTIMAL ||
                            response.status() == MPSOLVER_FEASIBLE;
  absl::PrintF("%-12s: %s\n", "Status",
               MPSolverResponseStatus_Name(
                   static_cast<MPSolverResponseStatus>(response.status()))
                   .c_str());
  absl::PrintF("%-12s: %15.15e\n", "Objective",
               has_solution ? response.objective_value() : 0.0);
  absl::PrintF("%-12s: %15.15e\n", "BestBound",
               has_solution ? response.best_objective_bound() : 0.0);
  absl::PrintF("%-12s: %s\n", "StatusString", response.status_str());
  absl::PrintF("%-12s: %-6.4g s\n", "Time",
               absl::ToDoubleSeconds(solving_time));

  // If requested, write the solution, in .sol format (--sol_file), proto
  // format and/or csv format.
  if (!absl::GetFlag(FLAGS_sol_file).empty() && has_solution) {
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
  if (!absl::GetFlag(FLAGS_dump_response).empty() && has_solution) {
    CHECK_OK(WriteProtoToFile(absl::GetFlag(FLAGS_dump_response), response,
                              write_format, absl::GetFlag(FLAGS_dump_gzip)));
  }
  if (!absl::GetFlag(FLAGS_output_csv).empty() && has_solution) {
    std::string csv_file;
    for (int i = 0; i < response.variable_value_size(); ++i) {
      csv_file +=
          absl::StrFormat("%s,%e\n", request_proto.model().variable(i).name(),
                          response.variable_value(i));
    }
    CHECK_OK(file::SetContents(absl::GetFlag(FLAGS_output_csv), csv_file,
                               file::Defaults()));
  }
}

}  // namespace
}  // namespace operations_research

int main(int argc, char** argv) {
  InitGoogle(kUsageStr, &argc, &argv, /*remove_flags=*/true);
  operations_research::Run();
}

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

// Command-line interface to PDLP. The functionality is similar to solve.cc,
// but using PDLP's API directly. All integrality constraints are dropped from
// the input problem.

#include <atomic>
#include <string>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/file.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/pdlp/iteration_stats.h"
#include "ortools/pdlp/primal_dual_hybrid_gradient.h"
#include "ortools/pdlp/quadratic_program_io.h"
#include "ortools/pdlp/solve_log.pb.h"
#include "ortools/pdlp/solvers.pb.h"
#include "ortools/port/proto_utils.h"
#include "ortools/util/file_util.h"
#include "ortools/util/sigint.h"

// TODO: .mps.gz files aren't working. As a workaround, use .mps.

ABSL_FLAG(std::string, input, "", "REQUIRED: Input file name.");
ABSL_FLAG(std::string, params, "",
          "PrimalDualHybridGradientParams in text format");
ABSL_FLAG(std::string, solve_log_file, "",
          "If non-empty, writes PDLP's SolveLog here."
          "The extension must be .textproto (text), .pb (binary), or .json.");
ABSL_FLAG(
    std::string, sol_file, "",
    "If non-empty, output the final primal solution in Miplib .sol format.");

static const char kUsageStr[] =
    "Run PDLP on the given input file. The following formats are supported: \n"
    "  - a .mps, .mps.gz, .mps.bz2 file,\n"
    "  - an MPModelProto [.pb (binary), .textproto (text), *.json, *.json.gz]";

namespace operations_research::pdlp {

void WriteSolveLog(const std::string& solve_log_file, const SolveLog& log) {
  ProtoWriteFormat write_format;
  if (absl::EndsWith(solve_log_file, ".textproto")) {
    write_format = ProtoWriteFormat::kProtoText;
  } else if (absl::EndsWith(solve_log_file, ".pb")) {
    write_format = ProtoWriteFormat::kProtoBinary;
  } else if (absl::EndsWith(solve_log_file, ".json")) {
    write_format = ProtoWriteFormat::kJson;
  } else {
    LOG(FATAL) << "Unrecognized file extension for --solve_log_file: "
               << solve_log_file << ". Expected .textproto, .pb, or .json";
  }
  QCHECK(WriteProtoToFile(solve_log_file, log, write_format, /*gzipped=*/false,
                          /*append_extension_to_file_name=*/false).ok());
}

void Solve(const std::string& input, const std::string& params_str,
           const std::string& solve_log_file, const std::string& sol_file) {
  QCHECK(!input.empty()) << "--input is required";
  PrimalDualHybridGradientParams params;
  // Print iteration statistics by default. This can be overridden by
  // specifying verbosity_level in --params.
  params.set_verbosity_level(2);
  QCHECK(ProtobufTextFormatMergeFromString(params_str, &params))
      << "Error parsing --params";

  // Note: ReadQuadraticProgramOrDie drops integrality constraints.
  QuadraticProgram qp =
      ReadQuadraticProgramOrDie(input, /*include_names=*/true);

  // Register a signal handler to interrupt the solve when the user presses ^C.
  SigintHandler handler;
  std::atomic<bool> interrupted(false);
  handler.Register([&interrupted] { interrupted.store(true); });

  SolverResult result = PrimalDualHybridGradient(qp, params, &interrupted);

  if (!solve_log_file.empty()) {
    LOG(INFO) << "Writing SolveLog to '" << solve_log_file << "'.\n";
    WriteSolveLog(solve_log_file, result.solve_log);
  }

  const std::optional<ConvergenceInformation> convergence_information =
      pdlp::GetConvergenceInformation(result.solve_log.solution_stats(),
                                      result.solve_log.solution_type());
  // TODO: In what format should we write the dual solution?
  if (!sol_file.empty() && convergence_information.has_value()) {
    std::string sol_string;
    absl::StrAppend(&sol_string,
                    "=obj= ", convergence_information->primal_objective(),
                    "\n");
    for (int64_t i = 0; i < result.primal_solution.size(); ++i) {
      std::string name;
      if (qp.variable_names.has_value()) {
        name = (*qp.variable_names)[i];
      } else {
        name = absl::StrCat("var", i);
      }
      absl::StrAppend(&sol_string, name, " ", result.primal_solution(i), "\n");
    }
    LOG(INFO) << "Writing .sol solution to '" << sol_file << "'.\n";
    CHECK_OK(file::SetContents(sol_file, sol_string, file::Defaults()));
  }
}

}  // namespace operations_research::pdlp

int main(int argc, char** argv) {
  absl::SetFlag(&FLAGS_stderrthreshold, 0);
  google::InitGoogleLogging(kUsageStr);
  absl::ParseCommandLine(argc, argv);

  operations_research::pdlp::Solve(
      absl::GetFlag(FLAGS_input), absl::GetFlag(FLAGS_params),
      absl::GetFlag(FLAGS_solve_log_file), absl::GetFlag(FLAGS_sol_file));
  return EXIT_SUCCESS;
}

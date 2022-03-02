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

// Tool to run MathOpt on the given problems.
//
// Examples:
//
//   mathopt_solve --input_file model.pb
//
//   mathopt_solve --input_file model.mps.gz --solver_type=glop
//
//   mathopt_solve --input_file model --solver_logs --format=mathopt
//
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ortools/base/file.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/io/mps_converter.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/util/status_macros.h"

inline constexpr absl::string_view kMathOptBinaryFormat = "mathopt";
inline constexpr absl::string_view kMathOptTextFormat = "mathopt_txt";
inline constexpr absl::string_view kMPSFormat = "mps";
inline constexpr absl::string_view kAutoFormat = "auto";

inline constexpr absl::string_view kPbExt = ".pb";
inline constexpr absl::string_view kProtoExt = ".proto";
inline constexpr absl::string_view kPbTxtExt = ".pb.txt";
inline constexpr absl::string_view kTextProtoExt = ".textproto";
inline constexpr absl::string_view kMPSExt = ".mps";
inline constexpr absl::string_view kMPSGzipExt = ".mps.gz";

namespace {

struct SolverTypeProtoFormatter {
  void operator()(
      std::string* const out,
      const operations_research::math_opt::SolverTypeProto solver_type) {
    out->append(EnumToString(EnumFromProto(solver_type).value()));
  }
};

}  // namespace

ABSL_FLAG(std::string, input_file, "",
          "the file containing the model to solve; use --format to specify the "
          "file format");
ABSL_FLAG(std::string, format, "auto",
          absl::StrCat(
              "the format of the --input_file; possible values:\n", "* ",
              kMathOptBinaryFormat, ": for a MathOpt ModelProto in binary\n",
              "* ", kMathOptTextFormat, ": when the proto is in text\n", "* ",
              kMPSFormat, ": for MPS file (which can be GZiped)\n", "* ",
              kAutoFormat, ": to guess the format from the file extension:\n",
              "  - '", kPbExt, "', '", kProtoExt, "': ", kMathOptBinaryFormat,
              "\n", "  - '", kPbTxtExt, "', '", kTextProtoExt,
              "': ", kMathOptTextFormat, "\n", "  - '", kMPSExt, "', '",
              kMPSGzipExt, "': ", kMPSFormat));
ABSL_FLAG(
    std::vector<std::string>, update_files, {},
    absl::StrCat(
        "the file containing ModelUpdateProto to apply to the --input_file; "
        "when this flag is used, the --format must be either ",
        kMathOptBinaryFormat, " or  ", kMathOptTextFormat));
ABSL_FLAG(operations_research::math_opt::SolverType, solver_type,
          operations_research::math_opt::SolverType::kGscip,
          absl::StrCat(
              "the solver to use, possible values: ",
              absl::StrJoin(
                  operations_research::math_opt::AllSolversRegistry::Instance()
                      ->RegisteredSolvers(),
                  ", ", SolverTypeProtoFormatter())));
ABSL_FLAG(bool, solver_logs, false,
          "use a message callback to print the solver convergence logs");
ABSL_FLAG(absl::Duration, time_limit, absl::InfiniteDuration(),
          "the time limit to use for the solve");

namespace operations_research {
namespace math_opt {
namespace {

// Returned the guessed format (one of the kXxxFormat constant) from the file
// extension; or nullopt.
std::optional<absl::string_view> FormatFromFilePath(
    const absl::string_view file_path) {
  const std::vector<std::pair<absl::string_view, absl::string_view>>
      extension_to_format = {
          {kPbExt, kMathOptBinaryFormat},  {kProtoExt, kMathOptBinaryFormat},
          {kPbTxtExt, kMathOptTextFormat}, {kTextProtoExt, kMathOptTextFormat},
          {kMPSExt, kMPSFormat},           {kMPSGzipExt, kMPSFormat},
      };

  for (const auto& [ext, format] : extension_to_format) {
    if (absl::EndsWith(file_path, ext)) {
      return format;
    }
  }

  return std::nullopt;
}

// Returns the ModelProto read from the given file. The format must not be
// kAutoFormat; other invalid values will be reported as QFATAL log mentioning
// the --format flag.
absl::StatusOr<ModelProto> ReadModel(const absl::string_view file_path,
                                     const absl::string_view format) {
  if (format == kMathOptBinaryFormat) {
    return file::GetBinaryProto<ModelProto>(file_path, file::Defaults());
  }
  if (format == kMathOptTextFormat) {
    return file::GetTextProto<ModelProto>(file_path, file::Defaults());
  }
  if (format == kMPSFormat) {
    return ReadMpsFile(file_path);
  }
  LOG(QFATAL) << "Unsupported value of --format: " << format;
}

// Returns the ModelUpdateProto read from the given file. The format must be
// kMathOptBinaryFormat or kMathOptTextFormat; other values will generate an
// error.
absl::StatusOr<ModelUpdateProto> ReadModelUpdate(
    const absl::string_view file_path, const absl::string_view format) {
  if (format == kMathOptBinaryFormat) {
    return file::GetBinaryProto<ModelUpdateProto>(file_path, file::Defaults());
  }
  if (format == kMathOptTextFormat) {
    return file::GetTextProto<ModelUpdateProto>(file_path, file::Defaults());
  }
  return absl::InternalError(
      absl::StrCat("invalid format in ReadModelUpdate(): ", format));
}

// Prints the summary of the solve result.
absl::Status PrintSummary(const SolveResult& result) {
  std::cout << "Solve finished:\n"
            << "  termination: " << result.termination << "\n"
            << "  solve time: " << result.solve_stats.solve_time
            << "\n  best primal bound: " << result.solve_stats.best_primal_bound
            << "\n  best dual bound: " << result.solve_stats.best_dual_bound
            << std::endl;
  if (result.solutions.empty()) {
    std::cout << "  no solution" << std::endl;
  }
  for (int i = 0; i < result.solutions.size(); ++i) {
    const Solution& solution = result.solutions[i];
    std::cout << "  solution #" << (i + 1) << " objective: ";
    if (solution.primal_solution.has_value()) {
      std::cout << solution.primal_solution->objective_value;
    } else {
      std::cout << "n/a";
    }
    std::cout << std::endl;
  }

  return absl::OkStatus();
}

absl::Status RunSolver() {
  const std::string input_file_path = absl::GetFlag(FLAGS_input_file);
  if (input_file_path.empty()) {
    LOG(QFATAL) << "The flag --input_file is mandatory.";
  }

  // Parses --format.
  std::string format = absl::GetFlag(FLAGS_format);
  if (format == kAutoFormat) {
    const std::optional<absl::string_view> guessed_format =
        FormatFromFilePath(input_file_path);
    if (!guessed_format) {
      LOG(QFATAL) << "Can't guess the format from the file extension, please "
                     "use --format to specify the file format explicitly.";
    }
    format = *guessed_format;
  }
  // We deal with input validation in the ReadModel() function.

  // Read the model and the optional updates.
  const std::vector<std::string> update_file_paths =
      absl::GetFlag(FLAGS_update_files);
  if (!update_file_paths.empty() && format != kMathOptBinaryFormat &&
      format != kMathOptTextFormat) {
    LOG(QFATAL) << "Can't use --update_files with a input of format " << format
                << ".";
  }

  OR_ASSIGN_OR_RETURN3(const ModelProto model_proto,
                       ReadModel(input_file_path, format),
                       _ << "failed to read " << input_file_path);

  std::vector<ModelUpdateProto> model_updates;
  for (const std::string& update_file_path : update_file_paths) {
    ASSIGN_OR_RETURN(ModelUpdateProto update,
                     ReadModelUpdate(update_file_path, format));
    model_updates.emplace_back(std::move(update));
  }

  // Solve the problem.
  ASSIGN_OR_RETURN(const std::unique_ptr<Model> model,
                   Model::FromModelProto(model_proto));
  for (int u = 0; u < model_updates.size(); ++u) {
    const ModelUpdateProto& update = model_updates[u];
    RETURN_IF_ERROR(model->ApplyUpdateProto(update))
        << "failed to apply the update file: " << update_file_paths[u];
  }

  SolveArguments solve_args = {
      .parameters = {.time_limit = absl::GetFlag(FLAGS_time_limit)},
  };
  if (absl::GetFlag(FLAGS_solver_logs)) {
    solve_args.message_callback = PrinterMessageCallback(std::cout, "logs| ");
  }
  OR_ASSIGN_OR_RETURN3(
      const SolveResult result,
      Solve(*model, absl::GetFlag(FLAGS_solver_type), solve_args),
      _ << "the solver failed");

  RETURN_IF_ERROR(PrintSummary(result));

  return absl::OkStatus();
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research

int main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, /*remove_flags=*/true);

  const absl::Status status = operations_research::math_opt::RunSolver();
  // We don't use QCHECK_OK() here since the logged message contains more than
  // the failing status.
  if (!status.ok()) {
    LOG(QFATAL) << status;
  }

  return 0;
}

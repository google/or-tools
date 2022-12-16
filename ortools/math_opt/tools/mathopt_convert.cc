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

// Tool to run use MathOpt to convert MIP/LP models formats.
//
// Examples:
//  * Convert a text MPModelProto to a MPS file:
//      mathopt_convert \
//        --input_file model.textproto \
//        --input_format linear_solver_txt \
//        --output_file model.mps
//  * Convert a binary ModelProto to a binary MPModelProto:
//      mathopt_convert \
//        --input_file model.pb \
//        --output_file model_linear_solver.pb \
//        --output_format linear_solver
//  * Convert a binary ModelProto to a LP file:
//      mathopt_convert \
//        --input_file model.pb \
//        --output_file model.lp
//  * Anonymize a binary ModelProto:
//      mathopt_convert \
//        --input_file model.pb \
//        --nonames \
//        --output_file anonymous-model.pb
#include <optional>
#include <string>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ortools/base/init_google.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/io/names_removal.h"
#include "ortools/math_opt/tools/file_format_flags.h"
#include "ortools/util/status_macros.h"

ABSL_FLAG(
    std::string, input_file, "",
    "the file containing the model to solve; use --input_format to specify"
    " the file format");
ABSL_FLAG(
    std::optional<operations_research::math_opt::FileFormat>, input_format,
    std::nullopt,
    absl::StrCat(
        "the format of the --input_file; possible values:",
        operations_research::math_opt::OptionalFormatFlagPossibleValuesList()));

ABSL_FLAG(std::string, output_file, "",
          "the file to write to; use --output_format to specify"
          " the file format");
ABSL_FLAG(
    std::optional<operations_research::math_opt::FileFormat>, output_format,
    std::nullopt,
    absl::StrCat(
        "the format of the --output_file; possible values:",
        operations_research::math_opt::OptionalFormatFlagPossibleValuesList()));

ABSL_FLAG(bool, names, true,
          "use the names in the input models; ignoring names is useful when "
          "the input contains duplicates or if the model must be anonymized");

namespace operations_research::math_opt {
namespace {

// Returns the format to use for the file, or LOG(QFATAL) an error.
//
// Either use the format flag value is available, or guess the format based on
// the file_path's extension.
FileFormat ParseOptionalFormatFlag(
    const absl::string_view format_flag_name,
    const std::optional<FileFormat> format_flag_value,
    const absl::string_view file_path_flag_name,
    const absl::string_view file_path_flag_value) {
  const std::optional<FileFormat> format =
      FormatFromFlagOrFilePath(format_flag_value, file_path_flag_value);
  if (format.has_value()) {
    return *format;
  }
  LOG(QFATAL) << "Can't guess the format from the --" << file_path_flag_name
              << " extension, please use --" << format_flag_name
              << " to specify the file format explicitly.";
}

absl::Status Main() {
  const std::string input_file_path = absl::GetFlag(FLAGS_input_file);
  if (input_file_path.empty()) {
    LOG(QFATAL) << "The flag --input_file is mandatory.";
  }
  const std::string output_file_path = absl::GetFlag(FLAGS_output_file);
  if (output_file_path.empty()) {
    LOG(QFATAL) << "The flag --output_file is mandatory.";
  }

  const FileFormat input_format = ParseOptionalFormatFlag(
      /*format_flag_name=*/"input_format", absl::GetFlag(FLAGS_input_format),
      /*file_path_flag_name=*/"input_file", input_file_path);
  const FileFormat output_format = ParseOptionalFormatFlag(
      /*format_flag_name=*/"output_format", absl::GetFlag(FLAGS_output_format),
      /*file_path_flag_name=*/"output_file", output_file_path);

  // Read the model.
  OR_ASSIGN_OR_RETURN3((auto [model_proto, optional_hint]),
                       ReadModel(input_file_path, input_format),
                       _ << "failed to read " << input_file_path);

  if (!absl::GetFlag(FLAGS_names)) {
    RemoveNames(model_proto);
  }

  // Write the model.
  RETURN_IF_ERROR(
      WriteModel(output_file_path, model_proto, optional_hint, output_format))
      << "failed to write " << output_file_path;

  return absl::OkStatus();
}

}  // namespace
}  // namespace operations_research::math_opt

int main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, /*remove_flags=*/true);

  const absl::Status status = operations_research::math_opt::Main();
  // We don't use QCHECK_OK() here since the logged message contains more than
  // the failing status.
  if (!status.ok()) {
    LOG(QFATAL) << status;
  }

  return 0;
}

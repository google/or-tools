// Copyright 2010-2022 Google LLC
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

// Library to handles --input_file and --format flags of a binary that reads or
// writes MathOpt models in various binary or text formats.
//
// This library provides:
//
//   - a FileFormat enum type that can be used with ABSL_FLAG, either directly
//     or wrapped in a std::optional<FileFormat> to support guessing the file
//     format based on the file name's extension.
//
//   - functions that helps builds the help string of the flag using:
//     * FileFormat: FormatFlagPossibleValuesList()
//     * std::optional<FileFormat>: OptionalFormatFlagPossibleValuesList()
//
//   - a FormatFromFlagOrFilePath() function to handle the std::nullopt case
//     when using std::optional<FileFormat> for a flag.
//
//   - functions ReadModel() and WriteModel() that take the FileFormat and
//     read/write ModelProto.
//
#ifndef OR_TOOLS_MATH_OPT_TOOLS_FILE_FORMAT_FLAGS_H_
#define OR_TOOLS_MATH_OPT_TOOLS_FILE_FORMAT_FLAGS_H_

#include <optional>
#include <ostream>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"

namespace operations_research::math_opt {

// The supported file formats.
//
// The --format flag could be std::optional<FileFormat> to support automatic
// guessing of the format based on the input file extension.
enum class FileFormat {
  kMathOptBinary,
  kMathOptText,
  kLinearSolverBinary,
  kLinearSolverText,
  kMPS,
  kLP,
};

// Streams the enum value using AbslUnparseFlag().
std::ostream& operator<<(std::ostream& out, FileFormat f);

// Used to define flags with std::optional<FileFormat> or FileFormat.
//
// See OptionalFormatFlagPossibleValuesList() or FormatFlagPossibleValuesList()
// to build the related help strings.
//
// See FormatFromFlagOrFilePath() to handle the std::nullopt case when using an
// optional format.
bool AbslParseFlag(absl::string_view text, FileFormat* f, std::string* error);
std::string AbslUnparseFlag(FileFormat f);

// Returns a span of FileFormat enum values.
absl::Span<const FileFormat> AllFileFormats();

// Returns a multi-line list listing all possible formats that can be used with
// a --format flag of type std::optional<FileFormat>. Each entry is prefixed by
// a '\n'.
//
// The returned string contains a multiline list of bullet.
//
// See FormatFlagPossibleValuesList() for the alternative to use when the format
// value is not optional.
//
// Usage example:
//
//   ABSL_FLAG(
//       std::optional<operations_research::math_opt::FileFormat>, input_format,
//       std::nullopt,
//       absl::StrCat(
//           "the format of the --input_file; possible values:",
//           operations_research::math_opt::OptionalFormatFlagPossibleValuesList()));
//
std::string OptionalFormatFlagPossibleValuesList();

// Same as OptionalFormatFlagPossibleValuesList() but for a flag of type
// FileFormat (i.e. with a mandatory value).
//
// Usage example:
//
//   ABSL_FLAG(
//       operations_research::math_opt::FileFormat, output_format,
//       operations_research::math_opt::FileFormat::kMathOptBinary,
//       absl::StrCat(
//           "the format of the --output_file; possible values:",
//           operations_research::math_opt::FormatFlagPossibleValuesList()));
//
std::string FormatFlagPossibleValuesList();

// Returns a map from file extensions to their format.
absl::flat_hash_map<absl::string_view, FileFormat> ExtensionToFileFormat();

// Uses ExtensionToFileFormat() to returns the potential format from a given
// file path.
//
// Note that they may exists multiple formats for the same extension (like
// ".pb"). In that case an arbitrary choice is made (e.g. using MathOpt's
// ModelProto for ".pb").
std::optional<FileFormat> FormatFromFilePath(absl::string_view file_path);

// Returns either format_flag_value if not nullopt, else returns the result of
// FormatFromFilePath() on the input path.
//
// Note that in C++23 we could use std::optional<T>::or_else().
//
// Usage example:
//
//   const FileFormat input_format = [&](){
//     const std::optional<FileFormat> format =
//         FormatFromFlagOrFilePath(absl::GetFlag(FLAGS_input_format),
//                                  input_file_path);
//     if (format.has_value()) {
//       return *format;
//     }
//     LOG(QFATAL)
//       << "Can't guess the format from the file extension, please "
//          "use --input_format to specify the file format explicitly.";
//   }();
//
std::optional<FileFormat> FormatFromFlagOrFilePath(
    std::optional<FileFormat> format_flag_value, absl::string_view file_path);

// Returns the ModelProto read from the given file. Optionally returns a
// SolutionHintProto for kLinearSolverXxx format as it may contain one.
absl::StatusOr<std::pair<ModelProto, std::optional<SolutionHintProto>>>
ReadModel(absl::string_view file_path, FileFormat format);

// Writes the given model with the given format.
//
// The optional hint is used when the output format supports it
// (e.g. MPModelProto). **It is not yet implemented though**; if you need it,
// please contact us.
absl::Status WriteModel(absl::string_view file_path,
                        const ModelProto& model_proto,
                        const std::optional<SolutionHintProto>& hint_proto,
                        FileFormat format);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_TOOLS_FILE_FORMAT_FLAGS_H_

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

#include "ortools/math_opt/tools/file_format_flags.h"

#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/helpers.h"
#include "ortools/base/options.h"
#include "ortools/base/status_macros.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/math_opt/io/lp_converter.h"
#include "ortools/math_opt/io/lp_parser.h"
#include "ortools/math_opt/io/mps_converter.h"
#include "ortools/math_opt/io/proto_converter.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"

namespace operations_research::math_opt {

absl::Span<const FileFormat> AllFileFormats() {
  static constexpr FileFormat kValues[] = {
      FileFormat::kMathOptBinary,
      FileFormat::kMathOptText,
      FileFormat::kLinearSolverBinary,
      FileFormat::kLinearSolverText,
      FileFormat::kMPS,
      FileFormat::kLP,
  };
  return absl::MakeConstSpan(kValues);
}

std::string AbslUnparseFlag(const FileFormat f) {
  switch (f) {
    case FileFormat::kMathOptBinary:
      return "mathopt";
    case FileFormat::kMathOptText:
      return "mathopt_txt";
    case FileFormat::kLinearSolverBinary:
      return "linear_solver";
    case FileFormat::kLinearSolverText:
      return "linear_solver_txt";
    case FileFormat::kMPS:
      return "mps";
    case FileFormat::kLP:
      return "lp";
  }
}

std::ostream& operator<<(std::ostream& out, const FileFormat f) {
  out << AbslUnparseFlag(f);
  return out;
}

bool AbslParseFlag(const absl::string_view text, FileFormat* const f,
                   std::string* const error) {
  for (const FileFormat candidate : AllFileFormats()) {
    if (text == AbslUnparseFlag(candidate)) {
      *f = candidate;
      return true;
    }
  }

  absl::StrAppend(error, "not a known value");
  return false;
}

absl::flat_hash_map<absl::string_view, FileFormat> ExtensionToFileFormat() {
  return {
      {".pb", FileFormat::kMathOptBinary},
      {".proto", FileFormat::kMathOptBinary},
      {".pb.txt", FileFormat::kMathOptText},
      {".pbtxt", FileFormat::kMathOptText},
      {".textproto", FileFormat::kMathOptText},
      {".mps", FileFormat::kMPS},
      {".mps.gz", FileFormat::kMPS},
      {".lp", FileFormat::kLP},
  };
}

std::optional<FileFormat> FormatFromFilePath(absl::string_view file_path) {
  const absl::flat_hash_map<absl::string_view, FileFormat> extensions =
      ExtensionToFileFormat();

  // Sort extensions in reverse lexicographic order. The point here would be to
  // consider ".pb.txt" before we text for ".txt".
  using ExtensionPair = std::pair<absl::string_view, FileFormat>;
  std::vector<ExtensionPair> sorted_extensions(extensions.begin(),
                                               extensions.end());
  absl::c_sort(sorted_extensions,
               [](const ExtensionPair& lhs, const ExtensionPair& rhs) {
                 return lhs > rhs;
               });

  for (const auto& [extension, format] : sorted_extensions) {
    if (absl::EndsWith(file_path, extension)) {
      return format;
    }
  }
  return std::nullopt;
}

std::optional<FileFormat> FormatFromFlagOrFilePath(
    const std::optional<FileFormat> format_flag_value,
    const absl::string_view file_path) {
  if (format_flag_value.has_value()) {
    return *format_flag_value;
  }
  return FormatFromFilePath(file_path);
}

namespace {

constexpr absl::string_view kListLinePrefix = "* ";
constexpr absl::string_view kSubListLinePrefix = "  - ";

}  // namespace

std::string OptionalFormatFlagPossibleValuesList() {
  // Get the lines for each format and the introduction doc.
  std::string list = FormatFlagPossibleValuesList();

  // Add the doc of what happen when format is not specified.
  absl::StrAppend(&list, "\n", kListLinePrefix, "<unset>",
                  ": to guess the format from the file extension:");

  // Builds a map from formats to their extensions.
  absl::flat_hash_map<FileFormat, std::vector<absl::string_view>>
      format_extensions;
  for (const auto& [extension, format] : ExtensionToFileFormat()) {
    format_extensions[format].push_back(extension);
  }
  for (auto& [format, extensions] : format_extensions) {
    absl::c_sort(extensions);
  }

  // Here we iterate on all formats so that we list them in the same order as in
  // the enum.
  for (const FileFormat format : AllFileFormats()) {
    // Here we use operator[] as it is not an issue to create new empty entries
    // in the map.
    const std::vector<absl::string_view>& extensions =
        format_extensions[format];
    if (extensions.empty()) {
      continue;
    }

    absl::StrAppend(&list, "\n", kSubListLinePrefix,
                    absl::StrJoin(extensions, ", "), ": ",
                    AbslUnparseFlag(format));
  }
  return list;
}

std::string FormatFlagPossibleValuesList() {
  const absl::flat_hash_map<FileFormat, absl::string_view> format_help = {
      {FileFormat::kMathOptBinary, "for a MathOpt ModelProto in binary"},
      {FileFormat::kMathOptText, "when the proto is in text"},
      {FileFormat::kLinearSolverBinary,
       "for a LinearSolver MPModelProto in binary"},
      {FileFormat::kLinearSolverText, "when the proto is in text"},
      {FileFormat::kMPS, "for MPS file (which can be GZiped)"},
      {FileFormat::kLP, " for LP file"},
  };
  std::string list;
  for (const FileFormat format : AllFileFormats()) {
    absl::StrAppend(&list, "\n", kListLinePrefix, AbslUnparseFlag(format), ": ",
                    format_help.at(format));
  }
  return list;
}

absl::StatusOr<std::pair<ModelProto, std::optional<SolutionHintProto>>>
ReadModel(const absl::string_view file_path, const FileFormat format) {
  switch (format) {
    case FileFormat::kMathOptBinary: {
      ASSIGN_OR_RETURN(ModelProto model, file::GetBinaryProto<ModelProto>(
                                             file_path, file::Defaults()));
      return std::make_pair(std::move(model), std::nullopt);
    }
    case FileFormat::kMathOptText: {
      ASSIGN_OR_RETURN(ModelProto model, file::GetTextProto<ModelProto>(
                                             file_path, file::Defaults()));
      return std::make_pair(std::move(model), std::nullopt);
    }
    case FileFormat::kLinearSolverBinary:
    case FileFormat::kLinearSolverText: {
      ASSIGN_OR_RETURN(
          const MPModelProto linear_solver_model,
          format == FileFormat::kLinearSolverBinary
              ? file::GetBinaryProto<MPModelProto>(file_path, file::Defaults())
              : file::GetTextProto<MPModelProto>(file_path, file::Defaults()));
      ASSIGN_OR_RETURN(ModelProto model,
                       MPModelProtoToMathOptModel(linear_solver_model));
      ASSIGN_OR_RETURN(
          std::optional<SolutionHintProto> hint,
          MPModelProtoSolutionHintToMathOptHint(linear_solver_model));
      return std::make_pair(std::move(model), std::move(hint));
    }
    case FileFormat::kMPS: {
      ASSIGN_OR_RETURN(ModelProto model, ReadMpsFile(file_path));
      return std::make_pair(std::move(model), std::nullopt);
    }
    case FileFormat::kLP: {
      ASSIGN_OR_RETURN(const std::string lp_data,
                       file::GetContents(file_path, file::Defaults()));
      ASSIGN_OR_RETURN(ModelProto model, ModelProtoFromLp(lp_data));
      return std::make_pair(std::move(model), std::nullopt);
    }
  }
}

absl::Status WriteModel(const absl::string_view file_path,
                        const ModelProto& model_proto,
                        const std::optional<SolutionHintProto>& hint_proto,
                        const FileFormat format) {
  switch (format) {
    case FileFormat::kMathOptBinary:
      return file::SetBinaryProto(file_path, model_proto, file::Defaults());
    case FileFormat::kMathOptText:
      return file::SetTextProto(file_path, model_proto, file::Defaults());
    case FileFormat::kLinearSolverBinary:
    case FileFormat::kLinearSolverText: {
      ASSIGN_OR_RETURN(const MPModelProto linear_solver_model,
                       MathOptModelToMPModelProto(model_proto));
      if (hint_proto.has_value()) {
        LOG(WARNING) << "support for converting a MathOpt hint to MPModelProto "
                        "is not yet supported thus the hint has been lost";
      }
      return format == FileFormat::kLinearSolverBinary
                 ? file::SetBinaryProto(file_path, linear_solver_model,
                                        file::Defaults())
                 : file::SetTextProto(file_path, linear_solver_model,
                                      file::Defaults());
    }
    case FileFormat::kMPS: {
      ASSIGN_OR_RETURN(const std::string mps_data,
                       ModelProtoToMps(model_proto));
      return file::SetContents(file_path, mps_data, file::Defaults());
    }
    case FileFormat::kLP: {
      ASSIGN_OR_RETURN(const std::string lp_data, ModelProtoToLp(model_proto));
      return file::SetContents(file_path, lp_data, file::Defaults());
    }
  }
}

}  // namespace operations_research::math_opt

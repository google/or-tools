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

#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/strings/match.h"
#include "absl/strings/str_join.h"
#include "absl/time/time.h"
#include "ortools/algorithms/set_cover_heuristics.h"
#include "ortools/algorithms/set_cover_invariant.h"
#include "ortools/algorithms/set_cover_model.h"
#include "ortools/algorithms/set_cover_reader.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"

ABSL_FLAG(std::string, input, "", "REQUIRED: Input file name.");
ABSL_FLAG(std::string, input_fmt, "",
          "REQUIRED: Input file format. Either proto, proto_bin, OrlibRail, "
          "OrlibScp or FimiDat.");

ABSL_FLAG(std::string, hint_sol, "", "Input file name for solution.");
ABSL_FLAG(std::string, hint_fmt, "", "Input file format for solution.");

ABSL_FLAG(std::string, output, "",
          "If non-empty, write the returned solution to the given file.");
ABSL_FLAG(std::string, output_fmt, "",
          "If out is non-empty, use the given format for the output.");
ABSL_FLAG(std::string, output_model, "",
          "If non-empty, write the set cover model to the given file. ");
ABSL_FLAG(std::string, output_model_fmt, "",
          "If output_model is non-empty, use the given format for the output "
          "model file. Can be proto, proto_bin, OrlibRail, OrlibScp.");

ABSL_FLAG(bool, generate, false, "Generate a new model from the input model.");
ABSL_FLAG(int, num_elements_wanted, 0,
          "Number of elements wanted in the new generated model.");
ABSL_FLAG(int, num_subsets_wanted, 0,
          "Number of subsets wanted in the new generated model.");
ABSL_FLAG(float, row_scale, 1.0, "Row scale for the new generated model.");
ABSL_FLAG(float, column_scale, 1.0,
          "Column scale for the new generated model.");
ABSL_FLAG(float, cost_scale, 1.0, "Cost scale for the new generated model.");

ABSL_FLAG(std::string, generation, "", "First-solution generation method.");
ABSL_FLAG(std::string, improvement, "", "Solution improvement method.");
ABSL_FLAG(int, num_threads, 1,
          "Number of threads to use by the underlying solver.");

namespace operations_research {
using CL = SetCoverInvariant::ConsistencyLevel;

void LogStats(std::string name, SetCoverModel* model) {
  LOG(INFO) << ", " << name << ", num_elements, " << model->num_elements()
            << ", num_subsets, " << model->num_subsets();
  LOG(INFO) << ", " << name << ", num_nonzeros, " << model->num_nonzeros()
            << ", fill rate, " << model->FillRate();
  LOG(INFO) << ", " << name << ", cost, "
            << model->ComputeCostStats().DebugString();

  LOG(INFO) << ", " << name << ", num_rows, " << model->num_elements()
            << ", rows sizes, " << model->ComputeRowStats().DebugString();
  LOG(INFO) << ", " << name << ", row size deciles, "
            << absl::StrJoin(model->ComputeRowDeciles(), ", ");
  LOG(INFO) << ", " << name << ", row delta byte size stats, "
            << model->ComputeRowDeltaSizeStats().DebugString();
  LOG(INFO) << ", " << name << ", num_columns, " << model->num_subsets()
            << ", columns sizes, " << model->ComputeColumnStats().DebugString();
  LOG(INFO) << ", " << name << ", column size deciles, "
            << absl::StrJoin(model->ComputeColumnDeciles(), ", ");
  LOG(INFO) << ", " << name << ", column delta byte size stats, "
            << model->ComputeColumnDeltaSizeStats().DebugString();
  BaseInt num_singleton_rows = 0;
  for (const ElementIndex element : model->ElementRange()) {
    if (model->rows()[element].size() == 1) {
      ++num_singleton_rows;
    }
  }
  BaseInt num_singleton_columns = 0;
  for (const SubsetIndex subset : model->SubsetRange()) {
    if (model->columns()[subset].size() == 1) {
      ++num_singleton_columns;
    }
  }
  LOG(INFO) << ", " << name << ", num_singleton_rows, " << num_singleton_rows
            << ", num_singleton_columns, " << num_singleton_columns;
}

void LogCostAndTiming(std::string name, std::string algo, double cost,
                      BaseInt solution_cardinality, const WallTimer& timer) {
  LOG(INFO) << ", " << name << ", " << algo << ", cost, " << cost
            << ", solution_cardinality, " << solution_cardinality << ", "
            << absl::ToInt64Microseconds(timer.GetDuration()) << "e-6, s";
}

void LogCostAndTiming(std::string name, std::string algo,
                      const SetCoverInvariant& inv, const WallTimer& timer) {
  LogCostAndTiming(name, algo, inv.cost(), inv.trace().size(), timer);
}

enum class FileFormat {
  EMPTY,
  ORLIB_SCP,
  ORLIB_RAIL,
  FIMI_DAT,
  PROTO,
  PROTO_BIN,
  TXT,
};

FileFormat ParseFileFormat(const std::string& format_name) {
  if (format_name.empty()) {
    return FileFormat::EMPTY;
  } else if (absl::EqualsIgnoreCase(format_name, "OrlibScp")) {
    return FileFormat::ORLIB_SCP;
  } else if (absl::EqualsIgnoreCase(format_name, "OrlibRail")) {
    return FileFormat::ORLIB_RAIL;
  } else if (absl::EqualsIgnoreCase(format_name, "FimiDat")) {
    return FileFormat::FIMI_DAT;
  } else if (absl::EqualsIgnoreCase(format_name, "proto")) {
    return FileFormat::PROTO;
  } else if (absl::EqualsIgnoreCase(format_name, "proto_bin")) {
    return FileFormat::PROTO_BIN;
  } else if (absl::EqualsIgnoreCase(format_name, "txt")) {
    return FileFormat::TXT;
  } else {
    LOG(FATAL) << "Unsupported input format: " << format_name;
  }
}

SetCoverModel ReadModel(const std::string& input_file,
                        FileFormat input_format) {
  switch (input_format) {
    case FileFormat::ORLIB_SCP:
      return ReadOrlibScp(input_file);
    case FileFormat::ORLIB_RAIL:
      return ReadOrlibRail(input_file);
    case FileFormat::FIMI_DAT:
      return ReadFimiDat(input_file);
    case FileFormat::PROTO:
      return ReadSetCoverProto(input_file, /*binary=*/false);
    case FileFormat::PROTO_BIN:
      return ReadSetCoverProto(input_file, /*binary=*/true);
    default:
      LOG(FATAL) << "Unsupported input format: "
                 << static_cast<int>(input_format);
  }
}

SubsetBoolVector ReadSolution(const std::string& input_file,
                              FileFormat input_format) {
  switch (input_format) {
    case FileFormat::TXT:
      return ReadSetCoverSolutionText(input_file);
    case FileFormat::PROTO:
      return ReadSetCoverSolutionProto(input_file, /*binary=*/false);
    case FileFormat::PROTO_BIN:
      return ReadSetCoverSolutionProto(input_file, /*binary=*/true);
    default:
      LOG(FATAL) << "Unsupported input format: "
                 << static_cast<int>(input_format);
  }
}

void WriteModel(const SetCoverModel& model, const std::string& output_file,
                FileFormat output_format) {
  switch (output_format) {
    case FileFormat::ORLIB_SCP:
      WriteOrlibScp(model, output_file);
      break;
    case FileFormat::ORLIB_RAIL:
      WriteOrlibRail(model, output_file);
      break;
    case FileFormat::PROTO:
      WriteSetCoverProto(model, output_file, /*binary=*/false);
      break;
    case FileFormat::PROTO_BIN:
      WriteSetCoverProto(model, output_file, /*binary=*/true);
      break;
    default:
      LOG(FATAL) << "Unsupported output format: "
                 << static_cast<int>(output_format);
  }
}

void WriteSolution(const SetCoverModel& model, const SubsetBoolVector& solution,
                   const std::string& output_file, FileFormat output_format) {
  switch (output_format) {
    case FileFormat::TXT:
      WriteSetCoverSolutionText(model, solution, output_file);
      break;
    case FileFormat::PROTO:
      WriteSetCoverSolutionProto(model, solution, output_file,
                                 /*binary=*/false);
      break;
    case FileFormat::PROTO_BIN:
      WriteSetCoverSolutionProto(model, solution, output_file,
                                 /*binary=*/true);
      break;
    default:
      LOG(FATAL) << "Unsupported output format: "
                 << static_cast<int>(output_format);
  }
}

SetCoverInvariant RunLazyElementDegree(std::string name, SetCoverModel* model) {
  SetCoverInvariant inv(model);
  LazyElementDegreeSolutionGenerator element_degree(&inv);
  WallTimer timer;
  timer.Start();
  CHECK(element_degree.NextSolution());
  DCHECK(inv.CheckConsistency(CL::kCostAndCoverage));
  LogCostAndTiming(name, "LazyElementDegreeSolutionGenerator", inv.cost(),
                   inv.trace().size(), timer);
  return inv;
}

void Run() {
  const auto& input = absl::GetFlag(FLAGS_input);
  const auto& input_format = ParseFileFormat(absl::GetFlag(FLAGS_input_fmt));
  const auto& output = absl::GetFlag(FLAGS_output);
  const auto& output_format = ParseFileFormat(absl::GetFlag(FLAGS_output_fmt));
  SetCoverModel model = ReadModel(input, input_format);
  model.CreateSparseRowView();
  if (absl::GetFlag(FLAGS_generate)) {
    model = SetCoverModel::GenerateRandomModelFrom(
        model, absl::GetFlag(FLAGS_num_elements_wanted),
        absl::GetFlag(FLAGS_num_subsets_wanted), absl::GetFlag(FLAGS_row_scale),
        absl::GetFlag(FLAGS_column_scale), absl::GetFlag(FLAGS_cost_scale));
  }
  if (!output.empty()) {
    CHECK(output_format != FileFormat::EMPTY);
    WriteModel(model, output, output_format);
  }
  LogStats(input, &model);
  SetCoverInvariant inv = RunLazyElementDegree(input, &model);
}
}  // namespace operations_research

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  operations_research::Run();
  return 0;
}

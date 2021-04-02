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

// Driver for reading and solving files in the MPS format and in
// the linear_solver.proto format.

#include <stdio.h>

#include <string>

#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/file.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/lp_data/lp_print_utils.h"
#include "ortools/lp_data/mps_reader.h"
#include "ortools/lp_data/proto_utils.h"
#include "ortools/util/file_util.h"
#include "ortools/util/proto_tools.h"

ABSL_FLAG(bool, mps_dump_problem, false, "Dumps problem in readable form.");
ABSL_FLAG(bool, mps_solve, true, "Solves problem.");
ABSL_FLAG(bool, mps_terse_result, false,
          "Displays the result in form of a single CSV line.");
ABSL_FLAG(bool, mps_verbose_result, true,
          "Displays the result in verbose form.");
ABSL_FLAG(bool, mps_display_full_path, true,
          "Displays the full path of the input file in the result line.");
ABSL_FLAG(std::string, input, "", "File pattern for problems to be optimized.");
ABSL_FLAG(std::string, params_file, "",
          "Path to a GlopParameters file in text format.");
ABSL_FLAG(std::string, params, "",
          "GlopParameters in text format. If --params_file was "
          "also specified, the --params will be merged onto "
          "them (i.e. in case of conflicts, --params wins)");

using google::protobuf::TextFormat;
using operations_research::FullProtocolMessageAsString;
using operations_research::ReadFileToProto;
using operations_research::glop::GetProblemStatusString;
using operations_research::glop::GlopParameters;
using operations_research::glop::LinearProgram;
using operations_research::glop::LPSolver;
using operations_research::glop::MPModelProtoToLinearProgram;
using operations_research::glop::MPSReader;
using operations_research::glop::ProblemStatus;
using operations_research::glop::ToDouble;

// Parse glop parameters from the flags --params_file and --params.
void ReadGlopParameters(GlopParameters* parameters) {
  if (!absl::GetFlag(FLAGS_params_file).empty()) {
    std::string params;
    CHECK(TextFormat::ParseFromString(params, parameters)) << params;
  }
  if (!absl::GetFlag(FLAGS_params).empty()) {
    CHECK(TextFormat::MergeFromString(absl::GetFlag(FLAGS_params), parameters))
        << absl::GetFlag(FLAGS_params);
  }
  if (absl::GetFlag(FLAGS_mps_verbose_result)) {
    absl::PrintF("GlopParameters {\n%s}\n",
                 FullProtocolMessageAsString(*parameters, 1));
  }
}

int main(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);

  GlopParameters parameters;
  ReadGlopParameters(&parameters);

  LinearProgram linear_program;
  std::vector<std::string> file_list;
  // Replace this with your favorite match function.
  file_list.push_back(absl::GetFlag(FLAGS_input));
  for (int i = 0; i < file_list.size(); ++i) {
    const std::string& file_name = file_list[i];
    MPSReader mps_reader;
    operations_research::MPModelProto model_proto;
    if (absl::EndsWith(file_name, ".mps") ||
        absl::EndsWith(file_name, ".mps.gz")) {
      const absl::Status parse_status =
          mps_reader.ParseFile(file_name, &linear_program);
      if (!parse_status.ok()) {
        LOG(INFO) << "Parse error for " << file_name << ": " << parse_status;
        continue;
      }
    } else {
      ReadFileToProto(file_name, &model_proto);
      MPModelProtoToLinearProgram(model_proto, &linear_program);
    }
    if (absl::GetFlag(FLAGS_mps_dump_problem)) {
      absl::PrintF("%s", linear_program.Dump());
    }

    // Create the solver with the correct parameters.
    LPSolver solver;
    solver.SetParameters(parameters);
    ProblemStatus solve_status = ProblemStatus::INIT;

    std::string status_string;
    double objective_value;
    double solving_time_in_sec = 0;
    if (absl::GetFlag(FLAGS_mps_solve)) {
      ScopedWallTime timer(&solving_time_in_sec);
      solve_status = solver.Solve(linear_program);
      status_string = GetProblemStatusString(solve_status);
      objective_value = ToDouble(solver.GetObjectiveValue());
    }

    if (absl::GetFlag(FLAGS_mps_terse_result)) {
      if (absl::GetFlag(FLAGS_mps_display_full_path)) {
        absl::PrintF("%s,", file_name);
      }
      absl::PrintF("%s,", linear_program.name());
      if (absl::GetFlag(FLAGS_mps_solve)) {
        absl::PrintF("%15.15e,%s,%-6.4g,", objective_value, status_string,
                     solving_time_in_sec);
      }
      absl::PrintF("%s,%s\n", linear_program.GetProblemStats(),
                   linear_program.GetNonZeroStats());
    }

    if (absl::GetFlag(FLAGS_mps_verbose_result)) {
      if (absl::GetFlag(FLAGS_mps_display_full_path)) {
        absl::PrintF("%-45s: %s\n", "File path", file_name);
      }
      absl::PrintF("%-45s: %s\n", "Problem name", linear_program.name());
      if (absl::GetFlag(FLAGS_mps_solve)) {
        absl::PrintF("%-45s: %15.15e\n", "Objective value", objective_value);
        absl::PrintF("%-45s: %s\n", "Problem status", status_string);
        absl::PrintF("%-45s: %-6.4g\n", "Solving time", solving_time_in_sec);
      }
      absl::PrintF("%s%s", linear_program.GetPrettyProblemStats(),
                   linear_program.GetPrettyNonZeroStats());
    }
  }
  return EXIT_SUCCESS;
}

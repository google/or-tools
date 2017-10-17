// Copyright 2010-2017 Google
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

#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#include "ortools/base/file.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/stringpiece_utils.h"
#include "ortools/base/strutil.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/lp_data/lp_print_utils.h"
#include "ortools/lp_data/mps_reader.h"
#include "ortools/lp_data/proto_utils.h"
#include "ortools/util/proto_tools.h"
#include "ortools/base/status.h"

DEFINE_bool(mps_dump_problem, false, "Dumps problem in readable form.");
DEFINE_bool(mps_solve, true, "Solves problem.");
DEFINE_bool(mps_terse_result, false,
            "Displays the result in form of a single CSV line.");
DEFINE_bool(mps_verbose_result, true, "Displays the result in verbose form.");
DEFINE_bool(mps_display_full_path, true,
            "Displays the full path of the input file in the result line.");
DEFINE_string(input, "", "File pattern for problems to be optimized.");
DEFINE_string(params_file, "", "Path to a GlopParameters file in text format.");
DEFINE_string(params, "",
              "GlopParameters in text format. If --params_file was "
              "also specified, the --params will be merged onto "
              "them (i.e. in case of conflicts, --params wins)");

using operations_research::FullProtocolMessageAsString;
using operations_research::glop::GetProblemStatusString;
using operations_research::glop::GlopParameters;
using operations_research::glop::LinearProgram;
using operations_research::glop::LPSolver;
using operations_research::glop::MPSReader;
using operations_research::glop::MPModelProtoToLinearProgram;
using operations_research::glop::ProblemStatus;
using operations_research::glop::ToDouble;
using google::protobuf::TextFormat;


// Parse glop parameters from the flags --params_file and --params.
void ReadGlopParameters(GlopParameters* parameters) {
  if (!FLAGS_params_file.empty()) {
    std::string params;
    CHECK(TextFormat::ParseFromString(params, parameters)) << params;
  }
  if (!FLAGS_params.empty()) {
    CHECK(TextFormat::MergeFromString(FLAGS_params, parameters))
        << FLAGS_params;
  }
  if (FLAGS_mps_verbose_result) {
    printf("GlopParameters {\n%s}\n",
           FullProtocolMessageAsString(*parameters, 1).c_str());
  }
}


int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  GlopParameters parameters;
  ReadGlopParameters(&parameters);


  LinearProgram linear_program;
  std::vector<std::string> file_list;
  // Replace this with your favorite match function.
    file_list.push_back(FLAGS_input);
  for (int i = 0; i < file_list.size(); ++i) {
    const std::string& file_name = file_list[i];
    MPSReader mps_reader;
    operations_research::MPModelProto model_proto;
    if (strings::EndsWith(file_name, ".mps") ||
        strings::EndsWith(file_name, ".mps.gz")) {
      if (!mps_reader.LoadFileAndTryFreeFormOnFail(file_name,
                                                   &linear_program)) {
        LOG(INFO) << "Parse error for " << file_name;
        continue;
      }
    } else {
      file::ReadFileToProto(file_name, &model_proto);
      MPModelProtoToLinearProgram(model_proto, &linear_program);
    }
    if (FLAGS_mps_dump_problem) {
      printf("%s", linear_program.Dump().c_str());
    }

    // Create the solver with the correct parameters.
    LPSolver solver;
    solver.SetParameters(parameters);
    ProblemStatus solve_status = ProblemStatus::INIT;


    std::string status_string;
    double objective_value;
    double solving_time_in_sec = 0;
    if (FLAGS_mps_solve) {
      ScopedWallTime timer(&solving_time_in_sec);
      solve_status = solver.Solve(linear_program);
      status_string = GetProblemStatusString(solve_status);
      objective_value = ToDouble(solver.GetObjectiveValue());
    }

    if (FLAGS_mps_terse_result) {
      if (FLAGS_mps_display_full_path) {
        printf("%s,", file_name.c_str());
      }
      printf("%s,", mps_reader.GetProblemName().c_str());
      if (FLAGS_mps_solve) {
        printf("%15.15e,%s,%-6.4g,", objective_value, status_string.c_str(),
               solving_time_in_sec);
      }
      printf("%s,%s\n", linear_program.GetProblemStats().c_str(),
             linear_program.GetNonZeroStats().c_str());
    }

    if (FLAGS_mps_verbose_result) {
      if (FLAGS_mps_display_full_path) {
        printf("%-45s: %s\n", "File path", file_name.c_str());
      }
      printf("%-45s: %s\n", "Problem name",
             mps_reader.GetProblemName().c_str());
      if (FLAGS_mps_solve) {
        printf("%-45s: %15.15e\n", "Objective value", objective_value);
        printf("%-45s: %s\n", "Problem status", status_string.c_str());
        printf("%-45s: %-6.4g\n", "Solving time", solving_time_in_sec);
      }
      printf("%s%s", linear_program.GetPrettyProblemStats().c_str(),
             linear_program.GetPrettyNonZeroStats().c_str());
    }
  }
  return EXIT_SUCCESS;
}

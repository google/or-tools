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

// This is the skeleton for the official flatzinc interpreter.  Much
// of the functionalities are fixed (name of parameters, format of the
// input): see http://www.minizinc.org/downloads/doc-1.6/flatzinc-spec.pdf

#include <cstdlib>
#include <cstring>
#if defined(__GNUC__)  // Linux or Mac OS X.
#include <signal.h>
#endif  // __GNUC__

#include <iostream>
#include <ostream>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"

#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/base/path.h"
#include "ortools/base/timer.h"
#include "ortools/flatzinc/cp_model_fz_solver.h"
#include "ortools/flatzinc/model.h"
#include "ortools/flatzinc/parser.h"
#include "ortools/util/logging.h"

constexpr bool kOrToolsMode = true;

ABSL_FLAG(double, time_limit, 0, "time limit in seconds.");
ABSL_FLAG(bool, search_all_solutions, false, "Search for all solutions.");
ABSL_FLAG(bool, display_all_solutions, false,
          "Display all improving solutions.");
ABSL_FLAG(bool, free_search, false,
          "If false, the solver must follow the defined search."
          "If true, other search are allowed.");
ABSL_FLAG(int, threads, 0, "Number of threads the solver will use.");
ABSL_FLAG(bool, statistics, false, "Print solver statistics after search.");
ABSL_FLAG(bool, read_from_stdin, false,
          "Read the FlatZinc from stdin, not from a file.");
ABSL_FLAG(int, fz_seed, 0, "Random seed");
ABSL_FLAG(std::string, fz_model_name, "stdin",
          "Define problem name when reading from stdin.");
ABSL_FLAG(std::string, params, "", "SatParameters as a text proto.");
ABSL_FLAG(bool, fz_logging, false,
          "Print logging information from the flatzinc interpreter.");
ABSL_FLAG(bool, ortools_mode, kOrToolsMode,
          "Display solutions in the flatzinc format");

namespace operations_research {
namespace fz {

std::vector<char*> FixAndParseParameters(int* argc, char*** argv) {
  char all_param[] = "--search_all_solutions";
  char print_solutions[] = "--display_all_solutions";
  char free_param[] = "--free_search";
  char threads_param[] = "--threads";
  char logging_param[] = "--fz_logging";
  char statistics_param[] = "--statistics";
  char seed_param[] = "--fz_seed";
  char time_param[] = "--time_limit";
  bool use_time_param = false;
  bool set_free_search = false;
  for (int i = 1; i < *argc; ++i) {
    if (strcmp((*argv)[i], "-a") == 0) {
      (*argv)[i] = all_param;
    }
    if (strcmp((*argv)[i], "-i") == 0) {
      (*argv)[i] = print_solutions;
    }
    if (strcmp((*argv)[i], "-f") == 0) {
      (*argv)[i] = free_param;
      set_free_search = true;
    }
    if (strcmp((*argv)[i], "-p") == 0) {
      (*argv)[i] = threads_param;
    }
    if (strcmp((*argv)[i], "-l") == 0) {
      (*argv)[i] = logging_param;
    }
    if (strcmp((*argv)[i], "-s") == 0) {
      (*argv)[i] = statistics_param;
    }
    if (strcmp((*argv)[i], "-r") == 0) {
      (*argv)[i] = seed_param;
    }
    if (strcmp((*argv)[i], "-t") == 0) {
      (*argv)[i] = time_param;
      use_time_param = true;
    }
    if (kOrToolsMode) {
      if (strcmp((*argv)[i], "-v") == 0) {
        (*argv)[i] = logging_param;
      }
    }
  }
  const char kUsage[] =
      "Usage: see flags.\nThis program parses and solve a flatzinc problem.";

  absl::SetProgramUsageMessage(kUsage);
  const std::vector<char*> residual_flags =
      absl::ParseCommandLine(*argc, *argv);
  absl::InitializeLog();

  // Fix time limit if -t was used.
  if (use_time_param) {
    absl::SetFlag(&FLAGS_time_limit, absl::GetFlag(FLAGS_time_limit) / 1000.0);
  }

  // Define the default number of workers to 1 if -f was used.
  if (set_free_search && absl::GetFlag(FLAGS_threads) == 0) {
    absl::SetFlag(&FLAGS_threads, 1);
  }

  return residual_flags;
}

Model ParseFlatzincModel(const std::string& input, bool input_is_filename,
                         SolverLogger* logger, absl::Duration* parse_duration) {
  WallTimer timer;
  timer.Start();

  // Check the extension.
  if (input_is_filename && !absl::EndsWith(input, ".fzn")) {
    LOG(FATAL) << "Unrecognized flatzinc file: `" << input << "'";
  }

  // Read model.
  const std::string problem_name = input_is_filename
                                       ? std::string(file::Stem(input))
                                       : absl::GetFlag(FLAGS_fz_model_name);
  Model model(problem_name);
  if (input_is_filename) {
    CHECK(ParseFlatzincFile(input, &model));
  } else {
    CHECK(ParseFlatzincString(input, &model));
  }

  *parse_duration = timer.GetDuration();
  SOLVER_LOG(logger, "File ", (input_is_filename ? input : "stdin"),
             " parsed in ", absl::ToInt64Milliseconds(*parse_duration), " ms");
  SOLVER_LOG(logger, "");

  // Print statistics.
  ModelStatistics stats(model, logger);
  stats.BuildStatistics();
  stats.PrintStatistics();
  return model;
}

void LogInFlatzincFormat(const std::string& multi_line_input) {
  if (multi_line_input.empty()) {
    std::cout << std::endl;
    return;
  }
  const absl::string_view flatzinc_prefix =
      absl::GetFlag(FLAGS_ortools_mode) ? "%% " : "";
  const std::vector<absl::string_view> lines =
      absl::StrSplit(multi_line_input, '\n');
  for (const absl::string_view& line : lines) {
    std::cout << flatzinc_prefix << line << std::endl;
  }
}

}  // namespace fz
}  // namespace operations_research

int main(int argc, char** argv) {
  // Flatzinc specifications require single dash parameters (-a, -f, -p).
  // We need to fix parameters before parsing them.
  const std::vector<char*> residual_flags =
      operations_research::fz::FixAndParseParameters(&argc, &argv);
  // We allow piping model through stdin.
  std::string input;
  if (absl::GetFlag(FLAGS_read_from_stdin)) {
    std::string currentLine;
    while (std::getline(std::cin, currentLine)) {
      input.append(currentLine);
      input.append("\n");
    }
  } else {
    if (residual_flags.empty()) {
      LOG(ERROR) << "Usage: " << argv[0] << " <file>";
      return EXIT_FAILURE;
    }
    input = residual_flags.back();
  }

  operations_research::SolverLogger logger;
  if (absl::GetFlag(FLAGS_ortools_mode)) {
    logger.EnableLogging(absl::GetFlag(FLAGS_fz_logging));
    // log_to_stdout is disabled later.
    logger.AddInfoLoggingCallback(operations_research::fz::LogInFlatzincFormat);
  } else {
    logger.EnableLogging(true);
    logger.SetLogToStdOut(true);
  }

  absl::Duration parse_duration;
  operations_research::fz::Model model =
      operations_research::fz::ParseFlatzincModel(
          input, !absl::GetFlag(FLAGS_read_from_stdin), &logger,
          &parse_duration);
  operations_research::sat::ProcessFloatingPointOVariablesAndObjective(&model);

  operations_research::fz::FlatzincSatParameters parameters;
  parameters.display_all_solutions = absl::GetFlag(FLAGS_display_all_solutions);
  parameters.search_all_solutions = absl::GetFlag(FLAGS_search_all_solutions);
  parameters.use_free_search = absl::GetFlag(FLAGS_free_search);
  parameters.log_search_progress =
      absl::GetFlag(FLAGS_fz_logging) || !absl::GetFlag(FLAGS_ortools_mode);
  parameters.random_seed = absl::GetFlag(FLAGS_fz_seed);
  parameters.display_statistics = absl::GetFlag(FLAGS_statistics);
  parameters.number_of_threads = absl::GetFlag(FLAGS_threads);
  parameters.max_time_in_seconds =
      absl::GetFlag(FLAGS_time_limit) - absl::ToInt64Seconds(parse_duration);
  parameters.ortools_mode = absl::GetFlag(FLAGS_ortools_mode);

  operations_research::SolverLogger solution_logger;
  solution_logger.SetLogToStdOut(true);
  solution_logger.EnableLogging(parameters.ortools_mode);

  if (absl::GetFlag(FLAGS_time_limit) > 0 &&
      parse_duration > absl::Seconds(absl::GetFlag(FLAGS_time_limit))) {
    if (parameters.ortools_mode) {
      SOLVER_LOG(&solution_logger, "%% TIMEOUT");
    }
    if (parameters.log_search_progress) {
      SOLVER_LOG(&logger, "CpSolverResponse summary:");
      SOLVER_LOG(&logger, "status: UNKNOWN");
    }
    return EXIT_SUCCESS;
  }

  operations_research::sat::SolveFzWithCpModelProto(model, parameters,
                                                    absl::GetFlag(FLAGS_params),
                                                    &logger, &solution_logger);
  return EXIT_SUCCESS;
}

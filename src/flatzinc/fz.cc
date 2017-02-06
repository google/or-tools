// Copyright 2010-2014 Google
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
// of the funcionalities are fixed (name of parameters, format of the
// input): see http://www.minizinc.org/downloads/doc-1.6/flatzinc-spec.pdf

#if defined(__GNUC__)  // Linux or Mac OS X.
#include <signal.h>
#endif  // __GNUC__

#include <csignal>
#include <string>
#include <vector>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/timer.h"
#include "base/threadpool.h"
#include "base/commandlineflags.h"
#include "flatzinc/logging.h"
#include "flatzinc/model.h"
#include "flatzinc/parser.h"
#include "flatzinc/presolve.h"
#include "flatzinc/reporting.h"
#include "flatzinc/sat_fz_solver.h"
#include "flatzinc/solver.h"
#include "flatzinc/solver_util.h"

DEFINE_int32(time_limit, 0, "time limit in ms.");
DEFINE_bool(all_solutions, false, "Search for all solutions.");
DEFINE_int32(num_solutions, 0,
             "Maximum number of solution to search for, 0 means unspecified.");
DEFINE_bool(free_search, false,
            "Ignore search annotations in the flatzinc model.");
DEFINE_int32(threads, 0, "Number of threads the solver will use.");
DEFINE_bool(presolve, true, "Presolve the model to simplify it.");
DEFINE_bool(statistics, false, "Print solver statistics after search.");
DEFINE_bool(read_from_stdin, false,
            "Read the FlatZinc from stdin, not from a file.");
DEFINE_int32(fz_seed, 0, "Random seed");
DEFINE_int32(log_period, 10000000, "Search log period in the CP search.");
DEFINE_bool(use_impact, false, "Use impact based search in free search.");
DEFINE_double(restart_log_size, -1,
              "Restart log size parameter for impact search.");
DEFINE_bool(last_conflict, false,
            "Use last conflict search hints in free search.");
DEFINE_int32(luby_restart, -1,
             "Luby restart factor, <= 0 = no luby in free search.");
DEFINE_int32(heuristic_period, 100,
             "Period to call heuristics in free search.");
DEFINE_bool(
    verbose_impact, false,
    "Increase verbosity of the impact based search when used in free search.");
DEFINE_bool(verbose_mt, false, "Verbose Multi-Thread.");
DEFINE_bool(use_fz_sat, false, "Use the SAT/CP solver.");
DEFINE_string(fz_model_name, "stdin",
              "Define problem name when reading from stdin.");

DECLARE_bool(log_prefix);
DECLARE_bool(fz_use_sat);

using operations_research::ThreadPool;

namespace operations_research {
namespace fz {

int FixedNumberOfSolutions() {
  return FLAGS_num_solutions == 0 ?  // Not fixed.
             (FLAGS_num_solutions = FLAGS_all_solutions ? kint32max : 1)
                                  : FLAGS_num_solutions;
}

FlatzincParameters SingleThreadParameters() {
  FlatzincParameters parameters;
  parameters.all_solutions = FLAGS_all_solutions;
  parameters.free_search = FLAGS_free_search;
  parameters.heuristic_period = FLAGS_heuristic_period;
  parameters.ignore_unknown = false;
  parameters.last_conflict = FLAGS_last_conflict;
  parameters.logging = FLAGS_fz_logging;
  parameters.log_period = FLAGS_log_period;
  parameters.luby_restart = FLAGS_luby_restart;
  parameters.num_solutions = FixedNumberOfSolutions();
  parameters.random_seed = FLAGS_fz_seed;
  parameters.restart_log_size = FLAGS_restart_log_size;
  parameters.search_type =
      FLAGS_use_impact ? FlatzincParameters::IBS : FlatzincParameters::DEFAULT;
  parameters.statistics = FLAGS_statistics;
  parameters.threads = FLAGS_threads;
  parameters.thread_id = -1;
  parameters.time_limit_in_ms = FLAGS_time_limit;
  parameters.verbose_impact = FLAGS_verbose_impact;
  return parameters;
}

FlatzincParameters MultiThreadParameters(int thread_id) {
  FlatzincParameters parameters = SingleThreadParameters();
  parameters.logging = thread_id == 0;
  parameters.luby_restart = -1;
  parameters.random_seed = FLAGS_fz_seed + thread_id * 10;
  parameters.thread_id = thread_id;
  switch (thread_id) {
    case 0: {
      parameters.free_search = false;
      parameters.last_conflict = false;
      parameters.search_type =
          operations_research::fz::FlatzincParameters::DEFAULT;
      parameters.restart_log_size = -1.0;
      break;
    }
    case 1: {
      parameters.free_search = true;
      parameters.last_conflict = false;
      parameters.search_type =
          operations_research::fz::FlatzincParameters::MIN_SIZE;
      parameters.restart_log_size = -1.0;
      break;
    }
    case 2: {
      parameters.free_search = true;
      parameters.last_conflict = false;
      parameters.search_type = operations_research::fz::FlatzincParameters::IBS;
      parameters.restart_log_size = FLAGS_restart_log_size;
      break;
    }
    case 3: {
      parameters.free_search = true;
      parameters.last_conflict = false;
      parameters.search_type =
          operations_research::fz::FlatzincParameters::FIRST_UNBOUND;
      parameters.restart_log_size = -1.0;
      parameters.heuristic_period = 10000000;
      break;
    }
    case 4: {
      parameters.free_search = true;
      parameters.last_conflict = false;
      parameters.search_type =
          operations_research::fz::FlatzincParameters::DEFAULT;
      parameters.restart_log_size = -1.0;
      parameters.heuristic_period = 30;
      parameters.run_all_heuristics = true;
      break;
    }
    default: {
      parameters.free_search = true;
      parameters.last_conflict = false;
      parameters.search_type =
          thread_id % 2 == 0
              ? operations_research::fz::FlatzincParameters::RANDOM_MIN
              : operations_research::fz::FlatzincParameters::RANDOM_MAX;
      parameters.restart_log_size = -1.0;
      parameters.luby_restart = 250;
    }
  }
  return parameters;
}

void FixAndParseParameters(int* argc, char*** argv) {
  FLAGS_log_prefix = false;

  char all_param[] = "--all_solutions";
  char free_param[] = "--free_search";
  char threads_param[] = "--threads";
  char solutions_param[] = "--num_solutions";
  char logging_param[] = "--fz_logging";
  char statistics_param[] = "--statistics";
  char seed_param[] = "--fz_seed";
  char verbose_param[] = "--fz_verbose";
  char debug_param[] = "--fz_debug";
  for (int i = 1; i < *argc; ++i) {
    if (strcmp((*argv)[i], "-a") == 0) {
      (*argv)[i] = all_param;
    }
    if (strcmp((*argv)[i], "-f") == 0) {
      (*argv)[i] = free_param;
    }
    if (strcmp((*argv)[i], "-p") == 0) {
      (*argv)[i] = threads_param;
    }
    if (strcmp((*argv)[i], "-n") == 0) {
      (*argv)[i] = solutions_param;
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
    if (strcmp((*argv)[i], "-v") == 0) {
      (*argv)[i] = verbose_param;
    }
    if (strcmp((*argv)[i], "-d") == 0) {
      (*argv)[i] = debug_param;
    }
  }
  const char kUsage[] =
      "Usage: see flags.\nThis program parses and solve a flatzinc problem.";

  gflags::SetUsageMessage(kUsage);
  gflags::ParseCommandLineFlags(argc, argv, true);
}

Model ParseFlatzincModel(const std::string& input, bool input_is_filename) {
  WallTimer timer;
  timer.Start();
  // Read model.
  std::string problem_name = input_is_filename ? input : FLAGS_fz_model_name;
  if (input_is_filename || strings::EndsWith(problem_name, ".fzn")) {
    CHECK(strings::EndsWith(problem_name, ".fzn"));
    problem_name.resize(problem_name.size() - 4);
    const size_t found = problem_name.find_last_of("/\\");
    if (found != std::string::npos) {
      problem_name = problem_name.substr(found + 1);
    }
  }
  Model model(problem_name);
  if (input_is_filename) {
    CHECK(ParseFlatzincFile(input, &model));
  } else {
    CHECK(ParseFlatzincString(input, &model));
  }

  FZLOG << "File " << (input_is_filename ? input : "stdin") << " parsed in "
        << timer.GetInMs() << " ms" << FZENDL;

  // Presolve the model.
  Presolver presolve;
  presolve.CleanUpModelForTheCpSolver(&model, FLAGS_fz_use_sat);
  if (FLAGS_presolve) {
    FZLOG << "Presolve model" << FZENDL;
    timer.Reset();
    timer.Start();
    presolve.Run(&model);
    FZLOG << "  - done in " << timer.GetInMs() << " ms" << FZENDL;
  }

  // Print statistics.
  ModelStatistics stats(model);
  stats.BuildStatistics();
  stats.PrintStatistics();
  return model;
}

void Solve(const Model& model) {
#if defined(__GNUC__)
  signal(SIGINT, &operations_research::fz::Interrupt::ControlCHandler);
#endif

  if (model.IsInconsistent()) {
    MonoThreadReporting reporting(FLAGS_all_solutions,
                                  FixedNumberOfSolutions());
    Solver::ReportInconsistentModel(model, SingleThreadParameters(),
                                    &reporting);
    return;
  }

  if (FLAGS_threads == 0) {
    Solver solver(model);
    CHECK(solver.Extract());
    MonoThreadReporting reporting(FLAGS_all_solutions,
                                  FixedNumberOfSolutions());
    solver.Solve(SingleThreadParameters(), &reporting);
  } else {
    MultiThreadReporting reporting(FLAGS_all_solutions,
                                   FixedNumberOfSolutions(), FLAGS_verbose_mt);
    {
      ThreadPool pool("Parallel_FlatZinc", FLAGS_threads);
      pool.StartWorkers();
      for (int thread_id = 0; thread_id < FLAGS_threads; ++thread_id) {
        pool.Schedule([&model, thread_id, &reporting]() {
          Solver solver(model);
          CHECK(solver.Extract());
          solver.Solve(MultiThreadParameters(thread_id), &reporting);
        });
      }
    }
  }
}

}  // namespace fz
}  // namespace operations_research

int main(int argc, char** argv) {
  // Flatzinc specifications require single dash parameters (-a, -f, -p).
  // We need to fix parameters before parsing them.
  operations_research::fz::FixAndParseParameters(&argc, &argv);
  // We allow piping model through stdin.
  std::string input;
  if (FLAGS_read_from_stdin) {
    std::string currentLine;
    while (std::getline(std::cin, currentLine)) {
      input.append(currentLine);
    }
  } else {
    if (argc <= 1) {
      LOG(ERROR) << "Usage: " << argv[0] << " <file>";
      return EXIT_FAILURE;
    }
    input = argv[1];
  }
  operations_research::fz::Model model =
      operations_research::fz::ParseFlatzincModel(input,
                                                  !FLAGS_read_from_stdin);

  if (FLAGS_use_fz_sat) {
    bool interrupt_solve = false;
    operations_research::sat::SolveWithSat(
        model, operations_research::fz::SingleThreadParameters(),
        &interrupt_solve);
  } else {
    operations_research::fz::Solve(model);
  }
  return EXIT_SUCCESS;
}
